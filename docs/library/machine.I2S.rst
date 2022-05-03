.. currentmodule:: machine

class :class:`I2S`
==================

I2S is a synchronous serial protocol used to connect digital audio devices.
At the physical level, a bus consists of 3 lines: SCK, WS, SD.
The I2S class supports controller operation.  Peripheral operation is not supported.

The I2S class is currently available as a Technical Preview.  During the preview period, feedback from
users is encouraged.  Based on this feedback, the I2S class API and implementation may be changed.

I2S objects can be created and initialized using::

    from machine import I2S
    from machine import Pin

    # ESP32
    sck_pin = Pin(14)   # Serial clock output
    ws_pin = Pin(13)    # Word clock output
    sd_pin = Pin(12)    # Serial data output

    or

    # PyBoards
    sck_pin = Pin("Y6")   # Serial clock output
    ws_pin = Pin("Y5")    # Word clock output
    sd_pin = Pin("Y8")    # Serial data output

    audio_out = I2S(2,
                    sck=sck_pin, ws=ws_pin, sd=sd_pin,
                    mode=I2S.TX,
                    bits=16,
                    format=I2S.MONO,
                    rate=44100,
                    ibuf=20000)

    audio_in = I2S(2,
                   sck=sck_pin, ws=ws_pin, sd=sd_pin,
                   mode=I2S.RX,
                   bits=32,
                   format=I2S.STEREO,
                   rate=22050,
                   ibuf=20000)

3 modes of operation are supported:
 - blocking
 - non-blocking
 - uasyncio

blocking::

    num_written = audio_out.write(buf) # blocks until buf emptied

    num_read = audio_in.readinto(buf) # blocks until buf filled

non-blocking::

    audio_out.irq(i2s_callback)         # i2s_callback is called when buf is emptied
    num_written = audio_out.write(buf)  # returns immediately

    audio_in.irq(i2s_callback)          # i2s_callback is called when buf is filled
    num_read = audio_in.readinto(buf)   # returns immediately

uasyncio::

    swriter = uasyncio.StreamWriter(audio_out)
    swriter.write(buf)
    await swriter.drain()

    sreader = uasyncio.StreamReader(audio_in)
    num_read = await sreader.readinto(buf)

Some codec devices like the WM8960 or SGTL5000 require separate initialization
before they can operate with the I2S class.  For these, separate drivers are
supplied, which also offer methods for controlling volume, audio processing and
other things.  For these drivers see:

- :ref:`wm8960`

Constructor
-----------

.. class:: I2S(id, *, sck, ws, sd, mck=None, mode, bits, format, rate, ibuf)

    Construct an I2S object of the given id:

    - ``id`` identifies a particular I2S bus; it is board and port specific

    Keyword-only parameters that are supported on all ports:

     - ``sck`` is a pin object for the serial clock line
     - ``ws`` is a pin object for the word select line
     - ``sd`` is a pin object for the serial data line
     - ``mck`` is a pin object for the master clock line;
       master clock frequency is sampling rate * 256
     - ``mode`` specifies receive or transmit
     - ``bits`` specifies sample size (bits), 16 or 32
     - ``format`` specifies channel format, STEREO or MONO
     - ``rate`` specifies audio sampling rate (Hz);
       this is the frequency of the ``ws`` signal
     - ``ibuf`` specifies internal buffer length (bytes)

    For all ports, DMA runs continuously in the background and allows user applications to perform other operations while
    sample data is transfered between the internal buffer and the I2S peripheral unit.
    Increasing the size of the internal buffer has the potential to increase the time that user applications can perform non-I2S operations
    before underflow (e.g. ``write`` method) or overflow (e.g. ``readinto`` method).

Methods
-------

.. method:: I2S.init(sck, ...)

  see Constructor for argument descriptions

.. method:: I2S.deinit()

  Deinitialize the I2S bus

.. method::  I2S.readinto(buf)

  Read audio samples into the buffer specified by ``buf``.  ``buf`` must support the buffer protocol, such as bytearray or array.
  "buf" byte ordering is little-endian.  For Stereo format, left channel sample precedes right channel sample. For Mono format,
  the left channel sample data is used.
  Returns number of bytes read

.. method::  I2S.write(buf)

  Write audio samples contained in ``buf``. ``buf`` must support the buffer protocol, such as bytearray or array.
  "buf" byte ordering is little-endian.  For Stereo format, left channel sample precedes right channel sample. For Mono format,
  the sample data is written to both the right and left channels.
  Returns number of bytes written

.. method::  I2S.irq(handler)

  Set a callback. ``handler`` is called when ``buf`` is emptied (``write`` method) or becomes full (``readinto`` method).
  Setting a callback changes the ``write`` and ``readinto`` methods to non-blocking operation.
  ``handler`` is called in the context of the MicroPython scheduler.

.. staticmethod::  I2S.shift(*, buf, bits, shift)

  bitwise shift of all samples contained in ``buf``. ``bits`` specifies sample size in bits. ``shift`` specifies the number of bits to shift each sample.
  Positive for left shift, negative for right shift.
  Typically used for volume control.  Each bit shift changes sample volume by 6dB.

Constants
---------

.. data:: I2S.RX

    for initialising the I2S bus ``mode`` to receive

.. data:: I2S.TX

    for initialising the I2S bus ``mode`` to transmit

.. data:: I2S.STEREO

    for initialising the I2S bus ``format`` to stereo

.. data:: I2S.MONO

    for initialising the I2S bus ``format`` to mono















I2S bus
-------

See :class:`I2S`. ::

    from machine import I2S, Pin

    i2s = I2S(0, sck=Pin(13), ws=Pin(14), sd=Pin(34), mode=I2S.TX, bits=16, format=I2S.STEREO, rate=44100, ibuf=40000) # create I2S object
    i2s.write(buf)             # write buffer of audio samples to I2S device

    i2s = I2S(1, sck=Pin(33), ws=Pin(25), sd=Pin(32), mode=I2S.RX, bits=16, format=I2S.MONO, rate=22050, ibuf=40000) # create I2S object
    i2s.readinto(buf)          # fill buffer with audio samples from I2S device

The I2S class is currently available as a Technical Preview.  During the preview period, feedback from
users is encouraged.  Based on this feedback, the I2S class API and implementation may be changed.

ESP32 has two I2S buses with id=0 and id=1


imx
---

See :class:`I2S`. Example using a Teensy 4.1 board with a simple
external Codec like UDA1334.::

    from machine import I2S, Pin
    i2s = I2S(2, sck=Pin(26), ws=Pin(27), sd=Pin(7),
        mode=I2S.TX, bts=16,format=I2S.STEREO,
        rate=44100,ibuf=40000)
    i2s.write(buf)             # write buffer of audio samples to I2S device


Example for using I2S with a MIMXRT10xx_DEV board::

    from machine import I2S, I2C, Pin
    import wm8960

    i2c=I2C(0)

    wm=wm8960.WM8960(i2c, sample_rate=SAMPLE_RATE_IN_HZ,
        adc_sync=wm8960.sync_dac,
        swap=wm8960.swap_input)

    i2s = I2S(1, sck=Pin("SCK_TX"), ws=Pin("WS_TX"), sd=Pin("SD_RX"),
        mck=Pin("MCK),mode=I2S.RX, bts=16,format=I2S.MONO,
        rate=32000,ibuf=10000)
    i2s.readinto(buf)          # fill buffer with audio samples from I2S device

In this example, the input channels are swapped in the WM8960 driver, since the
on-board microphone is connected to the right channel, but mono audio is taken
from the left channel.  Note, that the sck and ws pins are connected to the TX
signals of the I2S bus.  That is intentional, since at the MW8960 codec these
signals are shared for RX and TX.

Example using the Teensy audio shield::

    from machine import I2C, I2S, Pin
    from sgtl5000 import CODEC
    i2s = I2S(1, sck=Pin(21), ws=Pin(20), sd=Pin(7), mck=Pin(23),
        mode=I2S.TX, bits=16,rate=44100,format=I2S.STEREO,
        ibuf=40000,
    )

    # configure the SGTL5000 codec
    i2c = I2C(0, freq=400000)
    codec = CODEC(0x0A, i2c)
    codec.mute_dac(False)
    codec.dac_volume(0.9, 0.9)
    codec.headphone_select(0)
    codec.mute_headphone(False)
    codec.volume(0.7, 0.7)

    i2s.write(buf)             # write buffer of audio samples to I2S device

The SGTL5000 codec used by the Teensy Audio shield uses the RX signals for both
RX and TX.  Note that the codec is initialized after the I2S device.  That is
essential since MCK is needed for its I2C operation and is provided by the I2S
controller.

MIMXRT boards may have 1 or 2 I2S buses available at the board connectors.
On MIMXRT1010 devices the bus numbers are 1 and 3. The I2S signals have
fixed assignments to GPIO pins. For the assignment of Pins to I2S signals,
refer to :ref:`I2S pinout <devices_mimxrt_pins_i2s>`.



stm32
-----

See :class:`I2S`. ::

    from machine import I2S, Pin

    i2s = I2S(2, sck=Pin('Y6'), ws=Pin('Y5'), sd=Pin('Y8'), mode=I2S.TX, bits=16, format=I2S.STEREO, rate=44100, ibuf=40000) # create I2S object
    i2s.write(buf)             # write buffer of audio samples to I2S device

    i2s = I2S(1, sck=Pin('X5'), ws=Pin('X6'), sd=Pin('Y4'), mode=I2S.RX, bits=16, format=I2S.MONO, rate=22050, ibuf=40000) # create I2S object
    i2s.readinto(buf)          # fill buffer with audio samples from I2S device

The I2S class is currently available as a Technical Preview.  During the preview period, feedback from
users is encouraged.  Based on this feedback, the I2S class API and implementation may be changed.

PYBv1.0/v1.1 has one I2S bus with id=2.
PYBD-SFxW has two I2S buses with id=1 and id=2.
I2S is shared with SPI.




rp2
---


I2S bus
-------

See :class:`I2S`. ::

    from machine import I2S, Pin

    i2s = I2S(0, sck=Pin(16), ws=Pin(17), sd=Pin(18), mode=I2S.TX, bits=16, format=I2S.STEREO, rate=44100, ibuf=40000) # create I2S object
    i2s.write(buf)             # write buffer of audio samples to I2S device

    i2s = I2S(1, sck=Pin(0), ws=Pin(1), sd=Pin(2), mode=I2S.RX, bits=16, format=I2S.MONO, rate=22050, ibuf=40000) # create I2S object
    i2s.readinto(buf)          # fill buffer with audio samples from I2S device

The ``ws`` pin number must be one greater than the ``sck`` pin number.

The I2S class is currently available as a Technical Preview.  During the preview period, feedback from
users is encouraged.  Based on this feedback, the I2S class API and implementation may be changed.

Two I2S buses are supported with id=0 and id=1.
