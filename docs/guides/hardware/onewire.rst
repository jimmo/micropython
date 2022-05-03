.. _guides_hardware_onewire:

OneWire
=======

The OneWire driver is implemented in software and works on all pins::

    from machine import Pin
    import onewire

    ow = onewire.OneWire(Pin(12)) # create a OneWire bus on GPIO12
    ow.scan()               # return a list of devices on the bus
    ow.reset()              # reset the bus
    ow.readbyte()           # read a byte
    ow.writebyte(0x12)      # write a byte on the bus
    ow.write('123')         # write bytes on the bus
    ow.select_rom(b'12345678') # select a specific device by its ROM code

There is a specific driver for DS18S20 and DS18B20 devices::

    import time, ds18x20
    ds = ds18x20.DS18X20(ow)
    roms = ds.scan()
    ds.convert_temp()
    time.sleep_ms(750)
    for rom in roms:
        print(ds.read_temp(rom))

Be sure to put a 4.7k pull-up resistor on the data line.  Note that
the ``convert_temp()`` method must be called each time you want to
sample the temperature.


The 1-wire bus is a serial bus that uses just a single wire for communication
(in addition to wires for ground and power).  The DS18B20 temperature sensor
is a very popular 1-wire device, and here we show how to use the onewire module
to read from such a device.

For the following code to work you need to have at least one DS18S20 or DS18B20 temperature
sensor with its data line connected to GPIO12.  You must also power the sensors
and connect a 4.7k Ohm resistor between the data pin and the power pin.  ::

    import time
    import machine
    import onewire, ds18x20

    # the device is on GPIO12
    dat = machine.Pin(12)

    # create the onewire object
    ds = ds18x20.DS18X20(onewire.OneWire(dat))

    # scan for devices on the bus
    roms = ds.scan()
    print('found devices:', roms)

    # loop 10 times and print all temperatures
    for i in range(10):
        print('temperatures:', end=' ')
        ds.convert_temp()
        time.sleep_ms(750)
        for rom in roms:
            print(ds.read_temp(rom), end=' ')
        print()

Note that you must execute the ``convert_temp()`` function to initiate a
temperature reading, then wait at least 750ms before reading the value.
