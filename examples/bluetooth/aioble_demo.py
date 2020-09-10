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


async def disconnect_in(d, t_ms):
    await asyncio.sleep_ms(t_ms)
    print("disconnecting after", t_ms)
    await d.disconnect()
    print("disconnected x")


async def central():
    d = None
    # async for result in aioble.scan(5000):
    #     print('Result:', result, result.name(), list(result.services()), list(result.manufacturer()))
    # return

    async with aioble.scan(5000, 30000, 30000, active=True) as scanner:
        async for result in scanner:
            if result.name() == "mpy-temp":
                if _ENV_SENSE_UUID in result.services():
                    d = result.device
                    break

    if d:
        print("connecting to", d.addr)
        # input('?')

        try:
            await d.connect()
        except asyncio.TimeoutError:
            print("connect timeout")
            return

        print("connected")

        # if not d.encrypted:
        #     print('pairing')
        #     await d.pair()

        temp_service = await d.service(_ENV_SENSE_UUID)

        if temp_service:
            temp_characteristic = await temp_service.characteristic(_ENV_SENSE_TEMP_UUID)

            if temp_characteristic:
                print(decode_temperature(await temp_characteristic.read()))

                await temp_characteristic.subscribe(notify=True)
                print("subscribed")

                # asyncio.create_task(disconnect_in(d, 3000))

                # file_transfer_mode = MODE_NONE
                # file_transfer = asycnio.Event()

                # async def l2cap_task():
                #     try:
                #         while True:
                #             channel = await d.l2cap_listen(mtu, psn)

                #             current_file = None
                #             await file_transfer.wait()
                #             while True:
                #                 if file_transfer_mode == MODE_SEND:
                #                     data = current_file.read(mtu)
                #                     await channel.write(data)
                #                 elif file_transfer_mode == MODE_RECV:
                #                     data = await channel.read()
                #                     current_file.write(data)
                #             file_transfer.set()
                #     except aioble.DeviceDisconnectedError:
                #         pass

                # async def ots_task():
                #     oacp_characteristic = await ots_service.characteristic(_OACP)

                #     while True:
                #         await oacp.written()
                #         data = oacp.read()
                #         if data == start writing:
                #             file_transfer_mode = MODE_SEND
                #             file_transfer.set()
                #         elif data == start reading:
                #             file_transfer_mode = MODE_RECV
                #             file_transfer.set()
                #         await file_transfer.wait()

                # try:
                #     while True:
                #         try:
                #             print(decode_temperature(await temp_characteristic.notified(6000)))
                #         except asyncio.TimeoutError:
                #             print('demo timeout')
                # except aioble.DeviceDisconnectedError:
                #     print('demo disconnected')

        await d.disconnect()


# asyncio.run(central())


async def peripheral_indicate_task(device, temp_characteristic):
    print("peripheral task", device)
    t = 22.5
    while device.is_connected():
        temp_characteristic.write(encode_temperature(t))
        try:
            await temp_characteristic.indicate(device)
        except asyncio.TimeoutError:
            print("indicate timeout")
            break
        except aioble.DeviceDisconnectedError:
            print("indicate disconnected")
            break
        t += random.uniform(-0.5, 0.5)
        await asyncio.sleep_ms(500)
    print("disconnected")


async def temperature_write_task(temp_characteristic):
    while True:
        device = await temp_characteristic.written()
        print("write from", device, device.mtu)
        print(decode_temperature(temp_characteristic.read()))


async def peripheral_listen(temp_characteristic):
    while True:
        device = await aioble.advertise(
            250000,
            name="mpy-temp",
            services=[_ENV_SENSE_UUID],
            appearance=_ADV_APPEARANCE_GENERIC_THERMOMETER,
            manufacturer=(
                0xFAFA,
                b"hello world",
            ),
        )
        print("mtu", await device.exchange_mtu(210))
        # if not device.encrypted:
        #     await device.pair()
        # asyncio.create_task(peripheral_indicate_task(device, temp_characteristic))


async def peripheral():
    temp_service = aioble.Service(_ENV_SENSE_UUID)
    temp_characteristic = aioble.Characteristic(
        temp_service, _ENV_SENSE_TEMP_UUID, read=True, write=True, notify=True, indicate=True
    )
    aioble.register_services(temp_service)
    temp_characteristic.write(encode_temperature(22.5))

    asyncio.create_task(temperature_write_task(temp_characteristic))

    await asyncio.create_task(peripheral_listen(temp_characteristic))

    # i = 0
    # t = 22.5
    # while True:
    #     temp_characteristic.write(encode_temperature(t))
    #     if i % 10 == 0 and remote_device and remote_device.is_connected():
    #         #temp_characteristic.notify(remote_device)
    #         print('indicate')
    #         await temp_characteristic.indicate(remote_device)
    #         print('~indicate')
    #     t += random.uniform(-0.5, 0.5)
    #     i += 1
    #     await asyncio.sleep_ms(500)


# asyncio.run(peripheral())

# async def x(i):
#     await aioble.advertise(250000, name='mpy-temp {}'.format(i))

# async def advertiser():
#     for i in range(10):
#         t = asyncio.create_task(x(i))
#         await asyncio.sleep_ms(3000)
#         t.cancel()


# async def advertiser():
#     for i in range(10):
#         await aioble.advertise(250000, name='mpy-temp {}'.format(i), timeout=3000)

# asyncio.run(advertiser())
