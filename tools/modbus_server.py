#!/usr/bin/env python3
"""Pymodbus development server

Usage:
    python3 modbus_server.py
"""

import logging
from pymodbus import FramerType
from pymodbus.server import StartTcpServer
from pymodbus.datastore import (
    ModbusSlaveContext,
    ModbusServerContext,
    ModbusSparseDataBlock,
)

PORT = 5020


def encode_32bit(value):
    """Encodes a 32-bit integer into two 16-bit words (big-endian)."""
    low_word = value & 0xFFFF
    high_word = (value >> 16) & 0xFFFF
    return high_word, low_word


logging.basicConfig(level=logging.DEBUG)
_logger = logging.getLogger(__name__)
_logger.info(f"Starting Modbus TCP Server on port {PORT}")


battery_charge_power = 20420
active_power = 500
plant_power = 1337
o = 1
registers = {
    32064 + o: encode_32bit(plant_power)[0],
    32065 + o: encode_32bit(plant_power)[1],
    37002 + o: 110,
    37003 + o: 440,
    37004 + o: 220,
    37001 + o: encode_32bit(battery_charge_power)[0],
    37002 + o: encode_32bit(battery_charge_power)[1],
    37113 + o: encode_32bit(active_power)[0],
    37114 + o: encode_32bit(active_power)[1],
}

sparse_block = ModbusSparseDataBlock(registers)
store = ModbusSlaveContext(hr=sparse_block)
context = ModbusServerContext(slaves=store, single=True)
StartTcpServer(context=context, address=("0.0.0.0", PORT), framer=FramerType.SOCKET)
