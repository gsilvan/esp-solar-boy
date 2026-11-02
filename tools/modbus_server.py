#!/usr/bin/env python3
"""Pymodbus development server

Usage:
    python3 modbus_server.py
"""

import asyncio
import argparse
import logging
import struct
import tomllib
from pathlib import Path
from pymodbus import FramerType
from pymodbus.server import ModbusTcpServer
from pymodbus.server.requesthandler import ServerRequestHandler
from pymodbus.simulator import DataType, SimData, SimDevice
from pymodbus.simulator.simcore import SimCore
from pymodbus.exceptions import NoSuchIdException

PORT = 5020


class PipelinedRequestHandler(ServerRequestHandler):
    """Drain buffered TCP requests without overwriting an active request.

    PyModbus 3.16.1 clears recv_buffer when sending a reply. Preserve and drain
    it because the ESP sends several requests before waiting for replies.
    """

    def __init__(self, *args):
        super().__init__(*args)
        self._handling_request = False

    def callback_data(self, data, addr=None):
        if self._handling_request:
            return 0
        used_len = super().callback_data(data, addr)
        self._handling_request = self.last_pdu is not None
        return used_len

    def send(self, data, addr=None):
        buffered_requests = self.recv_buffer
        try:
            super().send(data, addr)
        finally:
            self.recv_buffer = buffered_requests

    async def handle_request(self):
        try:
            await super().handle_request()
        finally:
            self._handling_request = False
            if self.recv_buffer:
                self.datagram_received(b"", self.last_addr)


class StrictSimCore(SimCore):
    """Do not let the simulator's unit-0 fallback mask an incorrect unit."""

    async def async_getValues(self, device_id, func_code, address, count=1):
        if device_id not in self.devices:
            raise NoSuchIdException(f"Unknown unit {device_id}")
        return await super().async_getValues(device_id, func_code, address, count)

    async def async_setValues(self, device_id, func_code, address, values):
        if device_id not in self.devices:
            raise NoSuchIdException(f"Unknown unit {device_id}")
        return await super().async_setValues(device_id, func_code, address, values)


class PipelinedModbusTcpServer(ModbusTcpServer):
    def __init__(self, context, **kwargs):
        super().__init__(context=context, **kwargs)
        self.context = StrictSimCore(context)

    def callback_new_connection(self):
        return PipelinedRequestHandler(
            self, self.trace_packet, self.trace_pdu, self.trace_connect
        )


def encode_32bit(value):
    """Encodes a 32-bit integer into two 16-bit words (big-endian)."""
    low_word = value & 0xFFFF
    high_word = (value >> 16) & 0xFFFF
    return high_word, low_word


_logger = logging.getLogger(__name__)


def make_device(unit, registers):
    # SimData uses wire addresses directly.
    holding_registers = [
        SimData(address, values=value, datatype=DataType.REGISTERS)
        for address, value in sorted(registers.items())
    ]
    return SimDevice(
        id=unit,
        simdata=(
            [SimData(1, values=False, datatype=DataType.BITS)],
            [SimData(1, values=False, datatype=DataType.BITS)],
            holding_registers or [SimData(1, values=0, datatype=DataType.REGISTERS)],
            [SimData(1, values=0, datatype=DataType.REGISTERS)],
        ),
    )


def load_presets():
    presets = {}
    for path in sorted((Path(__file__).resolve().parents[1] / "data" / "presets").glob("*.toml")):
        with path.open("rb") as source:
            config = tomllib.load(source)
        if config["id"] in presets:
            raise ValueError(f"Duplicate preset id: {config['id']}")
        presets[config["id"]] = config
    return presets


def create_context(preset="sun2000", grid_power=500):
    definition = load_presets()[preset]
    values = {"soc": 22, "battery": 20420, "plant": 1337, "grid": grid_power}
    devices = {}
    formats = {"u16": "H", "i16": "h", "u32": "I", "i32": "i", "float32": "f"}
    for key, sample in values.items():
        mapping = definition[key]
        unit = mapping["unit"]
        registers = devices.setdefault(unit, {})
        raw = 0 if definition["requires_overrides"] else sample / mapping["scale"]
        if mapping["invert"]:
            raw = -raw
        if mapping["type"] != "float32":
            raw = round(raw)
        packed = struct.pack(">" + formats[mapping["type"]], raw)
        words = list(struct.unpack(">" + "H" * (len(packed) // 2), packed))
        if mapping["word_order"] == "low":
            words.reverse()
        for offset, word in enumerate(words):
            address = mapping["register"] + offset
            if address in registers and registers[address] != word:
                raise ValueError(f"Conflicting sample registers for unit {unit} at {address}")
            registers[address] = word
    return [make_device(unit, data) for unit, data in devices.items()]


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preset", choices=sorted(load_presets()), default="sun2000")
    parser.add_argument("--grid-power", type=int, default=500, help="Watts: positive export, negative import")
    args = parser.parse_args()
    logging.basicConfig(level=logging.DEBUG)
    _logger.info(f"Starting Modbus TCP Server on port {PORT}")
    server = PipelinedModbusTcpServer(
        context=create_context(args.preset, args.grid_power),
        address=("0.0.0.0", PORT), framer=FramerType.SOCKET
    )
    try:
        await server.serve_forever()
    finally:
        await server.shutdown()


if __name__ == "__main__":
    asyncio.run(main())
