import sys

sys.path.append("")

import uasyncio as asyncio
import aioble

import bluetooth

from micropython import const

import random
import struct

# org.bluetooth.service.environmental_sensing
_ENV_SENSE_UUID = bluetooth.UUID(0x181A)
# org.bluetooth.characteristic.temperature
_ENV_SENSE_TEMP_UUID = bluetooth.UUID(0x2A6E)
# org.bluetooth.characteristic.gap.appearance.xml
_ADV_APPEARANCE_GENERIC_THERMOMETER = const(768)


def encode_temperature(temp_deg_c):
    return struct.pack("<h", int(temp_deg_c * 100))


def decode_temperature(data):
    return struct.unpack("<h", data)[0] / 100


def encode_txbytes(n):
    return struct.pack("<H", n)


def decode_txbytes(data):
    return struct.unpack("<H", data)[0]


async def disconnect_in(d, t_ms):
    await asyncio.sleep_ms(t_ms)
    print("disconnecting after", t_ms)
    await d.disconnect()
    print("disconnected x")


_L2CAP_PSN = const(22)
_L2CAP_MTU = const(128)


async def l2cap_peripheral():
    temp_service = aioble.Service(_ENV_SENSE_UUID)
    temp_characteristic = aioble.Characteristic(
        temp_service, _ENV_SENSE_TEMP_UUID, read=True, write=True, notify=True, indicate=True
    )
    aioble.register_services(temp_service)

    tx_bytes = 0
    temp_characteristic.write(encode_txbytes(tx_bytes))

    while True:
        device = await aioble.advertise(
            250000,
            name="mpy-l2cap",
            services=[_ENV_SENSE_UUID],
            appearance=_ADV_APPEARANCE_GENERIC_THERMOMETER,
        )

        async def l2cap_device_task():
            nonlocal tx_bytes
            channel = await device.l2cap_accept(_L2CAP_PSN, _L2CAP_MTU)

            buf = bytes(range(256))
            mv = memoryview(buf)
            rxbuf = bytearray(1024)

            while True:
                if tx_bytes:
                    l = min(tx_bytes, channel.peer_mtu)
                    tx_bytes -= l
                    temp_characteristic.write(encode_txbytes(tx_bytes))
                    print("chunk", l, "remaining", tx_bytes)
                    await channel.send(mv[0:l])

                if channel.available():
                    await channel.recvinto(rxbuf)

                await asyncio.sleep_ms(100)

        async with device:
            t = asyncio.create_task(l2cap_device_task())

            while True:
                await temp_characteristic.written()
                tx_bytes = decode_txbytes(temp_characteristic.read())


asyncio.run(l2cap_peripheral())
