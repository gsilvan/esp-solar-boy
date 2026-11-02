import asyncio
import struct
import unittest

from pymodbus.constants import ExcCodes
from pymodbus.exceptions import NoSuchIdException
from tools.modbus_server import PipelinedModbusTcpServer, StrictSimCore, create_context, encode_32bit


class SimulatorTests(unittest.IsolatedAsyncioTestCase):
    async def test_sun2000_units_and_signed_values(self):
        core = StrictSimCore(create_context(grid_power=-500))
        self.assertEqual(await core.async_getValues(1, 3, 37113, 2), list(encode_32bit(-500)))
        self.assertEqual(await core.async_getValues(1, 3, 37004, 1), [220])
        self.assertEqual(await core.async_getValues(1, 3, 31657, 2), ExcCodes.ILLEGAL_ADDRESS)
        with self.assertRaises(NoSuchIdException):
            await core.async_getValues(0, 3, 37113, 2)

    async def test_emma_separates_inverter_and_meter(self):
        for power in (500, -500, 0):
            core = StrictSimCore(create_context("sun2000-emma", power))
            self.assertEqual(await core.async_getValues(0, 3, 31657, 2), list(encode_32bit(-power)))
            self.assertEqual(await core.async_getValues(2, 3, 37004, 1), [220])
            self.assertEqual(await core.async_getValues(2, 3, 31657, 2), ExcCodes.ILLEGAL_ADDRESS)
            self.assertEqual(await core.async_getValues(2, 3, 37113, 2), ExcCodes.ILLEGAL_ADDRESS)
            self.assertEqual(await core.async_getValues(0, 3, 37004, 1), ExcCodes.ILLEGAL_ADDRESS)
            with self.assertRaises(NoSuchIdException):
                await core.async_getValues(7, 3, 31657, 2)

    async def test_emma_units_and_generic_placeholders(self):
        emma = StrictSimCore(create_context("sun2000-emma"))
        self.assertEqual(await emma.async_getValues(2, 3, 37004, 1), [220])
        with self.assertRaises(NoSuchIdException):
            await emma.async_getValues(1, 3, 37004, 1)
        generic = StrictSimCore(create_context("generic"))
        for address, count in ((0, 1), (2, 2), (4, 2), (6, 2)):
            self.assertEqual(await generic.async_getValues(1, 3, address, count), [0] * count)

    async def test_pipelined_requests_across_emma_and_inverter(self):
        server = PipelinedModbusTcpServer(create_context("sun2000-emma", 500), address=("127.0.0.1", 0))
        writer = None
        try:
            await server.serve_forever(background=True)
            port = server.transport.sockets[0].getsockname()[1]
            reader, writer = await asyncio.open_connection("127.0.0.1", port)
            queries = ((2, 37004, 1), (2, 37001, 2), (2, 32064, 2), (0, 31657, 2), (2, 31657, 2))
            writer.write(b"".join(
                struct.pack(">HHHBBHH", transaction, 0, 6, unit, 3, address, count)
                for transaction, (unit, address, count) in enumerate(queries, 1)
            ))
            await writer.drain()
            for transaction, (unit, address, count) in enumerate(queries, 1):
                header = await asyncio.wait_for(reader.readexactly(7), timeout=2)
                response_id, protocol, length, response_unit = struct.unpack(">HHHB", header)
                data = await asyncio.wait_for(reader.readexactly(length - 1), timeout=2)
                self.assertEqual((response_id, protocol, response_unit), (transaction, 0, unit))
                if transaction == 5:
                    self.assertEqual(data, bytes((0x83, ExcCodes.ILLEGAL_ADDRESS)))
                else:
                    value = {37004: 220, 37001: 20420, 32064: 1337, 31657: -500}[address]
                    words = (value,) if count == 1 else encode_32bit(value)
                    self.assertEqual(data, bytes((3, count * 2)) + struct.pack(">" + "H" * count, *words))
        finally:
            if writer:
                writer.close()
                await writer.wait_closed()
            await server.shutdown()
