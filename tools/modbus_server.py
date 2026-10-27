#!/usr/bin/env python3
"""Pymodbus development server

Usage:
    python3 modbus_server.py
"""

import asyncio
import logging
from pymodbus import FramerType
from pymodbus.server import ModbusTcpServer
from pymodbus.server.requesthandler import ServerRequestHandler
from pymodbus.simulator import DataType, SimData, SimDevice

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


class PipelinedModbusTcpServer(ModbusTcpServer):
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


battery_charge_power = 20420
active_power = 500
plant_power = 1337
# SimData uses wire addresses directly.
registers = {
    32064: encode_32bit(plant_power)[0],
    32065: encode_32bit(plant_power)[1],
    37003: 440,
    37004: 220,
    37001: encode_32bit(battery_charge_power)[0],
    37002: encode_32bit(battery_charge_power)[1],
    37113: encode_32bit(active_power)[0],
    37114: encode_32bit(active_power)[1],
}

holding_registers = [
    SimData(address, values=value, datatype=DataType.REGISTERS)
    for address, value in sorted(registers.items())
]
# Device ID 0 handles all IDs, matching the previous single-device context.
context = SimDevice(
    id=0,
    simdata=(
        [SimData(1, values=False, datatype=DataType.BITS)],  # Coils
        [SimData(1, values=False, datatype=DataType.BITS)],  # Discrete inputs
        holding_registers,
        [SimData(1, values=0, datatype=DataType.REGISTERS)],  # Input registers
    ),
)


async def main():
    logging.basicConfig(level=logging.DEBUG)
    _logger.info(f"Starting Modbus TCP Server on port {PORT}")
    server = PipelinedModbusTcpServer(
        context=context, address=("0.0.0.0", PORT), framer=FramerType.SOCKET
    )
    try:
        await server.serve_forever()
    finally:
        await server.shutdown()


if __name__ == "__main__":
    asyncio.run(main())
