.. currentmodule:: machine

class :class:`ADC`
==================

The ADC class provides an interface to analog-to-digital convertors, and
represents a single endpoint that can sample a continuous voltage and
convert it to a discretised value.

For extra control over ADC sampling see :class:`ADCBlock`
(only available on some ports).

Example usage::

    from machine import ADC

    adc = ADC(pin)        # create an ADC object acting on a pin
    val = adc.read_u16()  # read a raw analog value in the range 0-65535
    val = adc.read_uv()   # read an analog value in µV (microvolts)

.. tabs::
    .. group-tab:: STM32

        TODO(pyb)
        See also :ref:`pyb.Pin <pyb.Pin>` and :ref:`pyb.ADC <pyb.ADC>`.

    .. group-tab:: RP2

        RP2040 has five ADC channels in total, four of which are 12-bit SAR based
        ADCs: GP26, GP27, GP28 and GP29.

        The input signal for ADC0, ADC1, ADC2 and ADC3 can be connected with GP26,
        GP27, GP28, GP29 respectively (On Pico board, GP29 is connected to VSYS).

        The standard ADC range is 0-3.3V.

        The fifth channel is connected to the in-built temperature sensor and can be
        used for measuring the temperature.

    .. group-tab:: ESP32

        On the ESP32, ADC functionality is available on pins 32-39 (ADC block 1) and
        pins 0, 2, 4, 12-15 and 25-27 (ADC block 2).

        ADC block 2 is also used by WiFi and so attempting to read analog values from
        block 2 pins when WiFi is active will raise an exception.

        The internal ADC reference voltage is typically 1.1V, but varies slightly from
        package to package. The ADC is less linear close to the reference voltage
        (particularly at higher attenuations) and has a minimum measurement voltage
        around 100mV, voltages at or below this will read as 0. To read voltages
        accurately, it is recommended to use the ``read_uv()`` method (see below).

        .. Warning::
            Note that the absolute maximum voltage rating for input pins is 3.6V. Going
            near to this boundary risks damage to the IC!

    .. group-tab:: ESP8266

        The ADC on channel 0 is available on a dedicated pin (pin 6)::

            adc = ADC(0)

        Note that input voltages on the ADC pin must be between 0v and 1.0v so you
        must use a voltage divider circuit to measure larger voltages.

    .. group-tab:: i.MXRT

        On the i.MXRT ADC functionality is available on :ref:`Pins labeled 'Ann' <devices_mimxrt_pins>`.

    .. group-tab:: Renesas RA

        Pin id is available corresponding to the RA MCU's pin name which are 'P000'
        as AN000 (analog channel 000). The RA MCU has many analog channels.
        However, there are some cases that pin feature is fixed or not available by
        the board. Please confirm the MCU and board manual for the pin mapping.

        The resolution of the ADC is 12 bit with 10 to 11 bit accuracy, irrespective
        of the value returned by read_u16(). If you need a higher resolution or
        better accuracy, use an external ADC.

    .. group-tab:: CC3200

        The ADC class described here is not supported. See CC3200-specific
        documentation at :class:`ADCWiPy`.

Constructors
------------

.. class:: ADC(id, *, sample_ns, atten)

    Access the ADC associated with a source identified by *id*.  This
    *id* may be an integer (usually specifying a channel number), a
    :class:`Pin` object, or other value supported by the
    underlying machine.

    .. tabs::

        .. group-tab:: STM32

            The *id* can be an integer (channel number) or a :class:`Pin`
            object, or a string (pin name).

        .. group-tab:: Renesas RA

            The *id* can be an integer (channel number) or a :class:`Pin`
            object, or a string (pin name).

        .. group-tab:: i.MXRT

            The *id* can be an integer (channel number) or a :class:`Pin`
            object, or a string (pin name).

        .. group-tab:: ESP32

            The *id* can be an integer (channel number) or a :class:`Pin`
            object.

        .. group-tab:: RP2

            The *id* can be an integer (channel number) or a :class:`Pin`
            object.

        .. group-tab:: ESP8266

            The *id* must be zero, corresponding to the dedicated ADC on pin 6.

    If additional keyword-arguments are given then they will configure
    various aspects of the ADC.  If not given, these settings will take
    previous or default values.  The settings are:

     - *sample_ns* is the sampling time in nanoseconds.

     - *atten* specifies the input attenuation, allowing voltages above the
        reference voltage to be read.

    .. tabs::
        .. group-tab:: ESP32

            ESP32 does not support different timings for ADC sampling and so the
            ``sample_ns`` keyword argument is not supported.

            The ESP32 port provides standard values for the *atten* argument. Valid
            values (and approximate linear measurement ranges) are:

              - ``ADC.ATTN_0DB``: No attenuation (100mV - 950mV)
              - ``ADC.ATTN_2_5DB``: 2.5dB attenuation (100mV - 1250mV)
              - ``ADC.ATTN_6DB``: 6dB attenuation (150mV - 1750mV)
              - ``ADC.ATTN_11DB``: 11dB attenuation (150mV - 2450mV)

        .. group-tab:: Other ports

            Keyword arguments are not supported. TODO


Methods
-------

.. method:: ADC.init(*, sample_ns, atten)

    Apply the given settings to the ADC.  Only those arguments that are
    specified will be changed.  See the ADC constructor above for what the
    arguments are.

.. method:: ADC.block()

    Return the :class:`ADCBlock` instance associated with
    this ADC object.

    This method only exists if the port supports the
    :class:`ADCBlock` class.

.. method:: ADC.read_u16()

    Take an analog reading and return an integer in the range 0-65535.
    The return value represents the raw reading taken by the ADC, scaled
    such that the minimum value is 0 and the maximum value is 65535.

.. method:: ADC.read_uv()

    Take an analog reading and return an integer value with units of µV
    (microvolts). It is up to the particular port whether or not this value is
    calibrated, and how calibration is done.

    .. tabs::
        .. group-tab:: ESP32

            This method uses the known characteristics of the ADC and per-package
            eFuse values - set during manufacture - to return a calibrated input
            voltage(before attenuation) in microvolts. The returned value has only
            millivolt resolution (i.e., will always be a multiple of 1000
            microvolts).

            The calibration is only valid across the linear range of the ADC. In
            particular, an input tied to ground will read as a value above 0
            microvolts. Within the linear range, however, more accurate and
            consistent results will be obtained than using `read_u16()` and scaling
            the result with a constant.


Legacy methods:
---------------

These methods are only supported on some ports and should not be used in new code.

.. method:: ADC.read()

    This method returns the raw ADC value ranged according to the resolution of
    the block, e.g., 0-4095 for 12-bit resolution.

.. method:: ADC.atten(atten)

    Equivalent to ``ADC.init(atten=atten)``.

.. method:: ADC.width(bits)

    Equivalent to ``ADC.block().init(bits=bits)``.

    .. tabs::
        .. group-tab:: ESP32

            For compatibility, the ``ADC`` object also provides constants matching the
            supported ADC resolutions:

            - ``ADC.WIDTH_9BIT`` = 9
            - ``ADC.WIDTH_10BIT`` = 10
            - ``ADC.WIDTH_11BIT`` = 11
            - ``ADC.WIDTH_12BIT`` = 12

samd
----

On the SAMD21/SAMD51 ADC functionality is available on Pins labelled 'Ann'.

Use the :ref:`machine.ADC <machine.ADC>` class::

    from machine import ADC

    adc0 = ADC(Pin("A0"))            # create ADC object on ADC pin, average=16
    adc0.read_u16()                  # read value, 0-65536 across voltage range 0.0v - 3.3v
    adc1 = ADC(Pin("A1"), average=1) # create ADC object on ADC pin, average=1

The resolution of the ADC is 12 bit with 12 bit accuracy, irrespective of the
value returned by read_u16(). If you need a higher resolution or better accuracy, use
an external ADC.

ADC Constructor
```````````````

.. class:: ADC(dest, *, average=16, vref=n)
  :noindex:

Construct and return a new ADC object using the following parameters:

  - *dest* is the Pin object on which the ADC is output.

Keyword arguments:

  - *average* is used to reduce the noise. With a value of 16 the LSB noise is about 1 digit.
  - *vref* sets the reference voltage for the ADC.

    The default setting is for 3.3V. Other values are:

    ==== ==============================  ===============================
    vref SAMD21                          SAMD51
    ==== ==============================  ===============================
    0    1.0V voltage reference          internal bandgap reference (1V)
    1    1/1.48 Analogue voltage supply  Analogue voltage supply
    2    1/2 Analogue voltage supply     1/2 Analogue voltage supply
    3    External reference A            External reference A
    4    External reference B            External reference B
    5    -                               External reference C
    ==== ==============================  ===============================

ADC Methods
```````````

.. method:: read_u16()

Read a single ADC value as unsigned 16 bit quantity. The voltage range is defined
by the vref option of the constructor, the resolutions by the bits option.
