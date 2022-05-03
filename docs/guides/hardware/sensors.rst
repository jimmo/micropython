.. _guides_hardware_sensors:

Sensors
=======

DHT
---

The DHT driver is implemented in software and works on all pins::

    import dht
    import machine

    d = dht.DHT11(machine.Pin(4))
    d.measure()
    d.temperature() # eg. 23 (°C)
    d.humidity()    # eg. 41 (% RH)

    d = dht.DHT22(machine.Pin(4))
    d.measure()
    d.temperature() # eg. 23.6 (°C)
    d.humidity()    # eg. 41.3 (% RH)


Sensor
------

Use the :ref:`zsensor.Sensor <zsensor.Sensor>` class to access sensor data::

    import zsensor
    from zsensor import Sensor

    accel = Sensor("FXOX8700")    # create sensor object for the accelerometer

    accel.measure()               # obtain a measurement reading from the accelerometer

    # each of these prints the value taken by measure()
    accel.float(zsensor.ACCEL_X)  # print measurement value for accelerometer X-axis sensor channel as float
    accel.millis(zsensor.ACCEL_Y) # print measurement value for accelerometer Y-axis sensor channel in millionths
    accel.micro(zsensor.ACCEL_Z)  # print measurement value for accelerometer Z-axis sensor channel in thousandths
    accel.int(zsensor.ACCEL_X)    # print measurement integer value only for accelerometer X-axis sensor channel


esp8266 tut
-----------

DHT (Digital Humidity & Temperature) sensors are low cost digital sensors with
capacitive humidity sensors and thermistors to measure the surrounding air.
They feature a chip that handles analog to digital conversion and provide a
1-wire interface. Newer sensors additionally provide an I2C interface.

The DHT11 (blue) and DHT22 (white) sensors provide the same 1-wire interface,
however, the DHT22 requires a separate object as it has more complex
calculation. DHT22 have 1 decimal place resolution for both humidity and
temperature readings. DHT11 have whole number for both.

A custom 1-wire protocol, which is different to Dallas 1-wire, is used to get
the measurements from the sensor. The payload consists of a humidity value,
a temperature value and a checksum.

To use the 1-wire interface, construct the objects referring to their data pin::

    >>> import dht
    >>> import machine
    >>> d = dht.DHT11(machine.Pin(4))

    >>> import dht
    >>> import machine
    >>> d = dht.DHT22(machine.Pin(4))

Then measure and read their values with::

    >>> d.measure()
    >>> d.temperature()
    >>> d.humidity()

Values returned from ``temperature()`` are in degrees Celsius and values
returned from ``humidity()`` are a percentage of relative humidity.

The DHT11 can be called no more than once per second and the DHT22 once every
two seconds for most accurate results. Sensor accuracy will degrade over time.
Each sensor supports a different operating range. Refer to the product
datasheets for specifics.

In 1-wire mode, only three of the four pins are used and in I2C mode, all four
pins are used. Older sensors may still have 4 pins even though they do not
support I2C. The 3rd pin is simply not connected.

Pin configurations:

Sensor without I2C in 1-wire mode (eg. DHT11, DHT22, AM2301, AM2302):

    1=VDD, 2=Data, 3=NC, 4=GND

Sensor with I2C in 1-wire mode (eg. DHT12, AM2320, AM2321, AM2322):

    1=VDD, 2=Data, 3=GND, 4=GND

Sensor with I2C in I2C mode (eg. DHT12, AM2320, AM2321, AM2322):

    1=VDD, 2=SDA, 3=GND, 4=SCL

You should use pull-up resistors for the Data, SDA and SCL pins.

To make newer I2C sensors work in backwards compatible 1-wire mode, you must
connect both pins 3 and 4 to GND. This disables the I2C interface.

DHT22 sensors are now sold under the name AM2302 and are otherwise identical.
