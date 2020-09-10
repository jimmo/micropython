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


async def l2cap_central():
    async with aioble.scan(5000, 30000, 30000, active=True) as scanner:
        async for result in scanner:
            if result.name() == "mpy-l2cap" and _ENV_SENSE_UUID in result.services():
                d = result.device
                break
        else:
            return

    print("connecting to", d.addr)

    try:
        await d.connect()
    except asyncio.TimeoutError:
        print("connect timeout")
        return

    async with d:
        if temp_service := await d.service(_ENV_SENSE_UUID):
            if temp_characteristic := await temp_service.characteristic(_ENV_SENSE_TEMP_UUID):
                print(decode_txbytes(await temp_characteristic.read()))

                async with await d.l2cap_connect(_L2CAP_PSN, _L2CAP_MTU) as channel:
                    print("got channel", channel.peer_mtu, channel.our_mtu)

                    expect = 400
                    await temp_characteristic.write(encode_txbytes(expect))

                    total = 0
                    buf = bytearray(min(channel.peer_mtu, channel.our_mtu))
                    while total < expect:
                        n = await channel.recvinto(buf)
                        print("got", n)
                        total += n

                    print("done")

                    for i in range(20):
                        print("sending", len(buf))
                        await channel.send(buf, timeout_ms=1000)

                    await channel.disconnect()
                    print("channel disconnect")

        await d.disconnect()
        print("device disconnect")


asyncio.run(l2cap_central())
