.. currentmodule:: machine

class :class:`SPI`
==================

SPI is a synchronous serial protocol that is driven by a controller. At the
physical level, a bus consists of 3 lines: SCK, MOSI, MISO. Multiple devices
can share the same bus. Each device should have a separate, 4th signal,
CS (Chip Select), to select a particular device on a bus with which
communication takes place. Management of a CS signal should happen in
user code (via the :class:`Pin` class).

Both hardware and software SPI implementations exist via the
:class:`SPI` and :class:`SoftSPI` classes.  Hardware SPI uses underlying
hardware support of the system to perform the reads/writes and is usually
efficient and fast but may have restrictions on which pins can be used.
Software SPI is implemented by bit-banging and can be used on any pin but
is not as efficient.  These classes have the same methods available and
differ primarily in the way they are constructed.

Example usage::

    from machine import SPI, Pin

    spi = SPI(0, baudrate=400000)           # Create SPI peripheral 0 at frequency of 400kHz.
                                            # Depending on the use case, extra parameters may be required
                                            # to select the bus characteristics and/or pins to use.
    cs = Pin(4, mode=Pin.OUT, value=1)      # Create chip-select on pin 4.

    try:
        cs(0)                               # Select peripheral.
        spi.write(b"12345678")              # Write 8 bytes, and don't care about received data.
    finally:
        cs(1)                               # Deselect peripheral.

    try:
        cs(0)                               # Select peripheral.
        rxdata = spi.read(8, 0x42)          # Read 8 bytes while writing 0x42 for each byte.
    finally:
        cs(1)                               # Deselect peripheral.

    rxdata = bytearray(8)
    try:
        cs(0)                               # Select peripheral.
        spi.readinto(rxdata, 0x42)          # Read 8 bytes inplace while writing 0x42 for each byte.
    finally:
        cs(1)                               # Deselect peripheral.

    txdata = b"12345678"
    rxdata = bytearray(len(txdata))
    try:
        cs(0)                               # Select peripheral.
        spi.write_readinto(txdata, rxdata)  # Simultaneously write and read bytes.
    finally:
        cs(1)                               # Deselect peripheral.

Constructors
------------

.. class:: SPI(id, ...)

    Construct an SPI object on the given bus, *id*. Values of *id* depend
    on a particular port and its hardware. Values 0, 1, etc. are commonly used
    to select hardware SPI block #0, #1, etc.

    With no additional parameters, the SPI object is created but not
    initialised (it has the settings from the last initialisation of
    the bus, if any).  If extra arguments are given, the bus is initialised.
    See ``init`` for parameters of initialisation.

.. class:: SoftSPI(baudrate=500000, *, polarity=0, phase=0, bits=8, firstbit=MSB, sck=None, mosi=None, miso=None)

    Construct a new software SPI object.  Additional parameters must be
    given, usually at least *sck*, *mosi* and *miso*, and these are used
    to initialise the bus.  See `SPI.init` for a description of the parameters.

Methods
-------

.. method:: SPI.init(baudrate=1000000, *, polarity=0, phase=0, bits=8, firstbit=SPI.MSB, sck=None, mosi=None, miso=None, pins=(SCK, MOSI, MISO))

    Initialise the SPI bus with the given parameters:

     - ``baudrate`` is the SCK clock rate.
     - ``polarity`` can be 0 or 1, and is the level the idle clock line sits at.
     - ``phase`` can be 0 or 1 to sample data on the first or second clock edge
       respectively.
     - ``bits`` is the width in bits of each transfer. Only 8 is guaranteed to be supported by all hardware.
     - ``firstbit`` can be ``SPI.MSB`` or ``SPI.LSB``.
     - ``sck``, ``mosi``, ``miso`` are pins (:class:`Pin`) objects to use for bus signals. For most
       hardware SPI blocks (as selected by ``id`` parameter to the constructor), pins are fixed
       and cannot be changed. In some cases, hardware blocks allow 2-3 alternative pin sets for
       a hardware SPI block. Arbitrary pin assignments are possible only for a bitbanging SPI driver
       (``id`` = -1).
     - ``pins`` - WiPy port doesn't ``sck``, ``mosi``, ``miso`` arguments, and instead allows to
       specify them as a tuple of ``pins`` parameter.

    In the case of hardware SPI the actual clock frequency may be lower than the
    requested baudrate. This is dependant on the platform hardware. The actual
    rate may be determined by printing the SPI object.

.. method:: SPI.deinit()

    Turn off the SPI bus.

.. method:: SPI.read(nbytes, write=0x00)

    Read a number of bytes specified by ``nbytes`` while continuously writing
    the single byte given by ``write``.
    Returns a ``bytes`` object with the data that was read.

.. method:: SPI.readinto(buf, write=0x00)

    Read into the buffer specified by ``buf`` while continuously writing the
    single byte given by ``write``.
    Returns ``None``.

    Note: on WiPy this function returns the number of bytes read.

.. method:: SPI.write(buf)

    Write the bytes contained in ``buf``.
    Returns ``None``.

    Note: on WiPy this function returns the number of bytes written.

.. method:: SPI.write_readinto(write_buf, read_buf)

    Write the bytes from ``write_buf`` while reading into ``read_buf``.  The
    buffers can be the same or different, but both buffers must have the
    same length.
    Returns ``None``.

    Note: on WiPy this function returns the number of bytes written.

Constants
---------

.. data:: SPI.CONTROLLER

    for initialising the SPI bus to controller; this is only used for the WiPy

.. data:: SPI.MSB
          SoftSPI.MSB

    set the first bit to be the most significant bit

.. data:: SPI.LSB
          SoftSPI.LSB

    set the first bit to be the least significant bit









Software SPI bus
----------------

Software SPI (using bit-banging) works on all pins, and is accessed via the
:class:`SoftSPI` class::

    from machine import Pin, SoftSPI

    # construct a SoftSPI bus on the given pins
    # polarity is the idle state of SCK
    # phase=0 means sample on the first edge of SCK, phase=1 means the second
    spi = SoftSPI(baudrate=100000, polarity=1, phase=0, sck=Pin(0), mosi=Pin(2), miso=Pin(4))

    spi.init(baudrate=200000) # set the baudrate

    spi.read(10)            # read 10 bytes on MISO
    spi.read(10, 0xff)      # read 10 bytes while outputting 0xff on MOSI

    buf = bytearray(50)     # create a buffer
    spi.readinto(buf)       # read into the given buffer (reads 50 bytes in this case)
    spi.readinto(buf, 0xff) # read into the given buffer and output 0xff on MOSI

    spi.write(b'12345')     # write 5 bytes on MOSI

    buf = bytearray(4)      # create a buffer
    spi.write_readinto(b'1234', buf) # write to MOSI and read from MISO into the buffer
    spi.write_readinto(buf, buf) # write buf to MOSI and read MISO back into buf

.. Warning::
    Currently *all* of ``sck``, ``mosi`` and ``miso`` *must* be specified when
    initialising Software SPI.

Hardware SPI bus
----------------

There are two hardware SPI channels that allow faster transmission
rates (up to 80Mhz). These may be used on any IO pins that support the
required direction and are otherwise unused (see :class:`Pin`)
but if they are not configured to their default pins then they need to
pass through an extra layer of GPIO multiplexing, which can impact
their reliability at high speeds. Hardware SPI channels are limited
to 40MHz when used on pins other than the default ones listed below.

=====  ===========  ============
\      HSPI (id=1)   VSPI (id=2)
=====  ===========  ============
sck    14           18
mosi   13           23
miso   12           19
=====  ===========  ============

Hardware SPI is accessed via the :class:`SPI` class and
has the same methods as software SPI above::

    from machine import Pin, SPI

    hspi = SPI(1, 10000000)
    hspi = SPI(1, 10000000, sck=Pin(14), mosi=Pin(13), miso=Pin(12))
    vspi = SPI(2, baudrate=80000000, polarity=0, phase=0, bits=8, firstbit=0, sck=Pin(18), mosi=Pin(23), miso=Pin(19))





ESP8266
-------


Software SPI bus
----------------

There are two SPI drivers. One is implemented in software (bit-banging)
and works on all pins, and is accessed via the :class:`SoftSPI`
class::

    from machine import Pin, SoftSPI

    # construct an SPI bus on the given pins
    # polarity is the idle state of SCK
    # phase=0 means sample on the first edge of SCK, phase=1 means the second
    spi = SoftSPI(baudrate=100000, polarity=1, phase=0, sck=Pin(0), mosi=Pin(2), miso=Pin(4))

    spi.init(baudrate=200000) # set the baudrate

    spi.read(10)            # read 10 bytes on MISO
    spi.read(10, 0xff)      # read 10 bytes while outputting 0xff on MOSI

    buf = bytearray(50)     # create a buffer
    spi.readinto(buf)       # read into the given buffer (reads 50 bytes in this case)
    spi.readinto(buf, 0xff) # read into the given buffer and output 0xff on MOSI

    spi.write(b'12345')     # write 5 bytes on MOSI

    buf = bytearray(4)      # create a buffer
    spi.write_readinto(b'1234', buf) # write to MOSI and read from MISO into the buffer
    spi.write_readinto(buf, buf) # write buf to MOSI and read MISO back into buf


Hardware SPI bus
----------------

The hardware SPI is faster (up to 80Mhz), but only works on following pins:
``MISO`` is GPIO12, ``MOSI`` is GPIO13, and ``SCK`` is GPIO14. It has the same
methods as the bitbanging SPI class above, except for the pin parameters for the
constructor and init (as those are fixed)::

    from machine import Pin, SPI

    hspi = SPI(1, baudrate=80000000, polarity=0, phase=0)

(``SPI(0)`` is used for FlashROM and not available to users.)



imx
---


Software SPI bus
----------------

Software SPI (using bit-banging) works on all pins, and is accessed via the
:class:`SoftSPI` class. ::

    from machine import Pin, SoftSPI

    # construct a SoftSPI bus on the given pins
    # polarity is the idle state of SCK
    # phase=0 means sample on the first edge of SCK, phase=1 means the second
    spi = SoftSPI(baudrate=100000, polarity=1, phase=0, sck=Pin(0), mosi=Pin(2), miso=Pin(4))

    spi.init(baudrate=200000) # set the baudrate

    spi.read(10)            # read 10 bytes on MISO
    spi.read(10, 0xff)      # read 10 bytes while outputting 0xff on MOSI

    buf = bytearray(50)     # create a buffer
    spi.readinto(buf)       # read into the given buffer (reads 50 bytes in this case)
    spi.readinto(buf, 0xff) # read into the given buffer and output 0xff on MOSI

    spi.write(b'12345')     # write 5 bytes on MOSI

    buf = bytearray(4)      # create a buffer
    spi.write_readinto(b'1234', buf) # write to MOSI and read from MISO into the buffer
    spi.write_readinto(buf, buf) # write buf to MOSI and read MISO back into buf

The highest supported baud rate is 500000.

Hardware SPI bus
----------------

There are up to four hardware SPI channels that allow faster transmission
rates (up to 30Mhz).  Hardware SPI is accessed via the
:ref:`machine.SPI <machine.SPI>` class and has the same methods as software SPI above::

    from machine import SPI, Pin

    spi = SPI(0, 10000000)
    cs_pin = Pin(6, Pin.OUT, value=1)
    cs_pin(0)
    spi.write('Hello World')
    cs_pin(1)

For the assignment of Pins to SPI signals, refer to
:ref:`Hardware SPI pinout <mimxrt_spi_pinout>`.
The keyword option cs=n can be used to enable the cs pin 0 or 1 for an automatic cs signal. The
default is cs=-1. Using cs=-1 the automatic cs signal is not created.
In that case, cs has to be set by the script. Clearing that assignment requires a power cycle.

Notes:

1. Even if the highest reliable baud rate at the moment is about 30 Mhz,
   setting a baud rate will not always result in exactly that
   frequency, especially at high baud rates.

2. Sending at higher baud rate is possible. In the tests receiving
   worked up to 60 MHz, sending up to 90 MHz.


stm32
-----

See :ref:`pyb.SPI <pyb.SPI>`. ::

    from pyb import SPI

    spi = SPI(1, SPI.CONTROLLER, baudrate=200000, polarity=1, phase=0)
    spi.send('hello')
    spi.recv(5) # receive 5 bytes on the bus
    spi.send_recv('hello') # send and receive 5 bytes


renesas
-------


The RA MCU has some hardware SPIs (Serial Peripheral Interface).
SPI id is available corresponding to the RA MCU's SPI number which are SPI(0) as SPI0 and SPI(1) as SPI1. If with no additional parameters, :class:`SoftSPI` is called.

See :class:`SPI`. ::

    from machine import SPI, Pin

    spi = SPI(0, baudrate=500000)
    cs = Pin.cpu.P103
    cs(0)
    spi.write(b"12345678")
    cs(1)


rp2
---

Software SPI bus
----------------

Software SPI (using bit-banging) works on all pins, and is accessed via the
:class:`SoftSPI` class::

    from machine import Pin, SoftSPI

    # construct a SoftSPI bus on the given pins
    # polarity is the idle state of SCK
    # phase=0 means sample on the first edge of SCK, phase=1 means the second
    spi = SoftSPI(baudrate=100_000, polarity=1, phase=0, sck=Pin(0), mosi=Pin(2), miso=Pin(4))

    spi.init(baudrate=200000) # set the baudrate

    spi.read(10)            # read 10 bytes on MISO
    spi.read(10, 0xff)      # read 10 bytes while outputting 0xff on MOSI

    buf = bytearray(50)     # create a buffer
    spi.readinto(buf)       # read into the given buffer (reads 50 bytes in this case)
    spi.readinto(buf, 0xff) # read into the given buffer and output 0xff on MOSI

    spi.write(b'12345')     # write 5 bytes on MOSI

    buf = bytearray(4)      # create a buffer
    spi.write_readinto(b'1234', buf) # write to MOSI and read from MISO into the buffer
    spi.write_readinto(buf, buf) # write buf to MOSI and read MISO back into buf

.. Warning::
    Currently *all* of ``sck``, ``mosi`` and ``miso`` *must* be specified when
    initialising Software SPI.

Hardware SPI bus
----------------

The RP2040 has 2 hardware SPI buses which is accessed via the
:class:`SPI` class and has the same methods as software
SPI above::

    from machine import Pin, SPI

    spi = SPI(1, 10_000_000)  # Default assignment: sck=Pin(10), mosi=Pin(11), miso=Pin(8)
    spi = SPI(1, 10_000_000, sck=Pin(14), mosi=Pin(15), miso=Pin(12))
    spi = SPI(0, baudrate=80_000_000, polarity=0, phase=0, bits=8, sck=Pin(6), mosi=Pin(7), miso=Pin(4))


wipy
----


See :class:`SPI`. ::

    from machine import SPI

    # configure the SPI controller @ 2MHz
    spi = SPI(0, SPI.CONTROLLER, baudrate=2_000_000, polarity=0, phase=0)
    spi.write('hello')
    spi.read(5) # receive 5 bytes on the bus
    rbuf = bytearray(5)
    spi.write_readinto('hello', rbuf) # send and receive 5 bytes


zepjyr
------


Hardware SPI bus
----------------

Hardware SPI is accessed via the :class:`SPI` class::

    from machine import SPI

    spi = SPI("SPI_0")          # construct a spi bus with default configuration
    spi.init(baudrate=100000, polarity=0, phase=0, bits=8, firstbit=SPI.MSB) # set configuration

    # equivalently, construct spi bus and set configuration at the same time
    spi = SPI("SPI_0", baudrate=100000, polarity=0, phase=0, bits=8, firstbit=SPI.MSB)
    print(spi)                  # print device name and bus configuration

    spi.read(4)                 # read 4 bytes on MISO
    spi.read(4, write=0xF)      # read 4 bytes while writing 0xF on MOSI

    buf = bytearray(8)          # create a buffer of size 8
    spi.readinto(buf)           # read into the buffer (reads number of bytes equal to the buffer size)
    spi.readinto(buf, 0xF)      # read into the buffer while writing 0xF on MOSI

    spi.write(b'abcd')          # write 4 bytes on MOSI

    buf = bytearray(4)                  # create buffer of size 8
    spi.write_readinto(b'abcd', buf)    # write to MOSI and read from MISO into the buffer
    spi.write_readinto(buf, buf)        # write buf to MOSI and read back into the buf


samd
----


Software SPI bus
----------------

Software SPI (using bit-banging) works on all pins, and is accessed via the
:ref:`machine.SoftSPI <machine.SoftSPI>` class. ::

    from machine import Pin, SoftSPI

    # construct a SoftSPI bus on the given pins
    # polarity is the idle state of SCK
    # phase=0 means sample on the first edge of SCK, phase=1 means the second
    spi = SoftSPI(baudrate=100000, polarity=1, phase=0, sck=Pin(7), mosi=Pin(9), miso=Pin(10))

    spi.init(baudrate=200000) # set the baud rate

    spi.read(10)            # read 10 bytes on MISO
    spi.read(10, 0xff)      # read 10 bytes while outputting 0xff on MOSI

    buf = bytearray(50)     # create a buffer
    spi.readinto(buf)       # read into the given buffer (reads 50 bytes in this case)
    spi.readinto(buf, 0xff) # read into the given buffer and output 0xff on MOSI

    spi.write(b'12345')     # write 5 bytes on MOSI

    buf = bytearray(4)      # create a buffer
    spi.write_readinto(b'1234', buf) # write to MOSI and read from MISO into the buffer
    spi.write_readinto(buf, buf) # write buf to MOSI and read MISO back into buf

The highest supported baud rate is 500000.

Hardware SPI bus
----------------

The SAMD21/SAMD51 MCUs have up to eight hardware so called SERCOM devices, which can be used as UART,
SPI or I2C device, but not every MCU variant and board exposes all
signal pins for users.  Hardware SPI is accessed via the
:ref:`machine.SPI <machine.SPI>` class and has the same methods as software SPI above::

    from machine import SPI

    spi = SPI(1, sck=Pin("SCK"), mosi=Pin("MOSI"), miso=Pin("MISO"), baudrate=10000000)
    spi.write('Hello World')

If miso is not specified, it is not used. For the assignment of Pins to SPI devices and signals, refer to
:ref:`SAMD pinout <samd_pinout>`.

Note: Even if the highest reliable baud rate at the moment is about 24 Mhz,
setting a baud rate will not always result in exactly that frequency, especially
at high baud rates.

