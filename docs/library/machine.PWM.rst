.. currentmodule:: machine

class :class:`PWM`
==================

This class provides pulse width modulation output.

Example usage::

    from machine import PWM

    pwm = PWM(pin)          # create a PWM object on a pin
    pwm.duty_u16(32768)     # set duty to 50%

    # reinitialise with a period of 200us, duty of 5us
    pwm.init(freq=5000, duty_ns=5000)

    pwm.duty_ns(3000)       # set pulse width to 3us

    pwm.deinit()

Constructors
------------

.. class:: PWM(dest, *, freq, duty_u16, duty_ns)

    Construct and return a new PWM object using the following parameters:

        - *dest* is the entity on which the PWM is output, which is usually a
          :class:`Pin` object, but a port may allow other values,
          like integers.
        - *freq* should be an integer which sets the frequency in Hz for the
          PWM cycle.
        - *duty_u16* sets the duty cycle as a ratio ``duty_u16 / 65535``.
        - *duty_ns* sets the pulse width in nanoseconds.

    Setting *freq* may affect other PWM objects if the objects share the same
    underlying PWM generator (this is hardware specific).
    Only one of *duty_u16* and *duty_ns* should be specified at a time.

Methods
-------

.. method:: PWM.init(*, freq, duty_u16, duty_ns)

    Modify settings for the PWM object.  See the above constructor for details
    about the parameters.

.. method:: PWM.deinit()

    Disable the PWM output.

.. method:: PWM.freq([value])

    Get or set the current frequency of the PWM output.

    With no arguments the frequency in Hz is returned.

    With a single *value* argument the frequency is set to that value in Hz.  The
    method may raise a ``ValueError`` if the frequency is outside the valid range.

.. method:: PWM.duty_u16([value])

    Get or set the current duty cycle of the PWM output, as an unsigned 16-bit
    value in the range 0 to 65535 inclusive.

    With no arguments the duty cycle is returned.

    With a single *value* argument the duty cycle is set to that value, measured
    as the ratio ``value / 65535``.

.. method:: PWM.duty_ns([value])

    Get or set the current pulse width of the PWM output, as a value in nanoseconds.

    With no arguments the pulse width in nanoseconds is returned.

    With a single *value* argument the pulse width is set to that value.

Specific PWM class implementations
----------------------------------

The following concrete class(es) implement enhancements to the PWM class.

    | :ref:`pyb.Timer for PyBoard <pyb.Timer>`

Limitations of PWM
------------------

* Not all frequencies can be generated with absolute accuracy due to
  the discrete nature of the computing hardware.  Typically the PWM frequency
  is obtained by dividing some integer base frequency by an integer divider.
  For example, if the base frequency is 80MHz and the required PWM frequency is
  300kHz the divider must be a non-integer number 80000000 / 300000 = 266.67.
  After rounding the divider is set to 267 and the PWM frequency will be
  80000000 / 267 = 299625.5 Hz, not 300kHz.  If the divider is set to 266 then
  the PWM frequency will be 80000000 / 266 = 300751.9 Hz, but again not 300kHz.

  Some ports like the RP2040 one use a fractional divider, which allow a finer
  granularity of the frequency at higher frequencies by switching the PWM
  pulse duration between two adjacent values, such that the resulting average
  frequency is more close to the intended one, at the cost of spectral purity. 

* The duty cycle has the same discrete nature and its absolute accuracy is not
  achievable.  On most hardware platforms the duty will be applied at the next
  frequency period.  Therefore, you should wait more than "1/frequency" before
  measuring the duty.

* The frequency and the duty cycle resolution are usually interdependent.
  The higher the PWM frequency the lower the duty resolution which is available,
  and vice versa. For example, a 300kHz PWM frequency can have a duty cycle
  resolution of 8 bit, not 16-bit as may be expected.  In this case, the lowest
  8 bits of *duty_u16* are insignificant. So::

    pwm=PWM(Pin(13), freq=300_000, duty_u16=2**16//2)

  and::

    pwm=PWM(Pin(13), freq=300_000, duty_u16=2**16//2 + 255)

  will generate PWM with the same 50% duty cycle.


















PWM can be enabled on all output-enabled pins. The base frequency can
range from 1Hz to 40MHz but there is a tradeoff; as the base frequency
*increases* the duty resolution *decreases*. See
`LED Control <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html>`_
for more details.

Use the :class:`PWM` class::

    from machine import Pin, PWM

    pwm0 = PWM(Pin(0))         # create PWM object from a pin
    freq = pwm0.freq()         # get current frequency (default 5kHz)
    pwm0.freq(1000)            # set PWM frequency from 1Hz to 40MHz

    duty = pwm0.duty()         # get current duty cycle, range 0-1023 (default 512, 50%)
    pwm0.duty(256)             # set duty cycle from 0 to 1023 as a ratio duty/1023, (now 25%)

    duty_u16 = pwm0.duty_u16() # get current duty cycle, range 0-65535
    pwm0.duty_u16(2**16*3//4)  # set duty cycle from 0 to 65535 as a ratio duty_u16/65535, (now 75%)

    duty_ns = pwm0.duty_ns()   # get current pulse width in ns
    pwm0.duty_ns(250_000)      # set pulse width in nanoseconds from 0 to 1_000_000_000/freq, (now 25%)

    pwm0.deinit()              # turn off PWM on the pin

    pwm2 = PWM(Pin(2), freq=20000, duty=512)  # create and configure in one go
    print(pwm2)                               # view PWM settings

ESP chips have different hardware peripherals:

=====================================================  ========  ========  ========
Hardware specification                                    ESP32  ESP32-S2  ESP32-C3
-----------------------------------------------------  --------  --------  --------
Number of groups (speed modes)                                2         1         1
Number of timers per group                                    4         4         4
Number of channels per group                                  8         8         6
-----------------------------------------------------  --------  --------  --------
Different PWM frequencies (groups * timers)                   8         4         4
Total PWM channels (Pins, duties) (groups * channels)        16         8         6
=====================================================  ========  ========  ========

A maximum number of PWM channels (Pins) are available on the ESP32 - 16 channels,
but only 8 different PWM frequencies are available, the remaining 8 channels must
have the same frequency.  On the other hand, 16 independent PWM duty cycles are
possible at the same frequency.

See more examples in the TODO esp pwm tutorial.

ESP8266
-------

PWM can be enabled on all pins except Pin(16).  There is a single frequency
for all channels, with range between 1 and 1000 (measured in Hz).  The duty
cycle is between 0 and 1023 inclusive.

Use the :class:`PWM` class::

    from machine import Pin, PWM

    pwm0 = PWM(Pin(0))      # create PWM object from a pin
    pwm0.freq()             # get current frequency
    pwm0.freq(1000)         # set frequency
    pwm0.duty()             # get current duty cycle
    pwm0.duty(200)          # set duty cycle
    pwm0.deinit()           # turn off PWM on the pin

    pwm2 = PWM(Pin(2), freq=500, duty=512) # create and configure in one go



imx
---

The i.MXRT has up to four dedicated PWM modules with four FLEXPWM submodules each
and up to four QTMR modules with four channels, which can be used to generate
a PWM signal or signal pair.

The PWM functions are provided by the :class:`PWM` class.
It supports all basic methods listed for that class and a few additional methods for
handling signal groups. ::

    # Samples for Teensy
    #

    from machine import Pin, PWM

    pwm2 = PWM(Pin(2))      # create PWM object from a pin
    pwm2.freq()             # get current frequency
    pwm2.freq(1000)         # set frequency
    pwm2.duty_u16()         # get current duty cycle, range 0-65535
    pwm2.duty_u16(200)      # set duty cycle, range 0-65535
    pwm2.deinit()           # turn off PWM on the pin
    # create a complementary signal pair on Pin 2 and 3
    pwm2 = PWM((2, 3), freq=2000, duty_ns=20000)

    # Create a group of four synchronized signals.
    # Start with Pin(4) at submodule 0, which creates the sync pulse.
    pwm4 = PWM(Pin(4), freq=1000, align=PWM.HEAD)
    # Pins 5, 6, and 9 are pins at the same module
    pwm5 = PWM(Pin(5), freq=1000, duty_u16=10000, align=PWM.HEAD, sync=True)
    pwm6 = PWM(Pin(6), freq=1000, duty_u16=20000, align=PWM.HEAD, sync=True)
    pwm9 = PWM(Pin(9), freq=1000, duty_u16=30000, align=PWM.HEAD, sync=True)

    pwm3                    # show the PWM objects properties


PWM Constructor
~~~~~~~~~~~~~~~

.. class:: PWM(dest, freq, duty_u16, duty_ns, *, center, align, invert, sync, xor, deadtime)
  :noindex:

    Construct and return a new PWM object using the following parameters:

        - *dest* is the entity on which the PWM is output, which is usually a
          :class:`Pin` object, but a port may allow other values,
          like integers or strings, which designate a Pin in the :class:`Pin` class.
          *dest* is either a single object or a two element object tuple.
          If the object tuple is specified, the two pins act in complementary
          mode. These two pins must be the A/B channels of the same submodule.

    PWM objects are either provided by a FLEXPWM module or a QTMR module.
    The i.MXRT devices have either two or four FLEXPWM and QTMR modules.
    Each FLEXPWM module has four submodules with three channels, each,
    called A, B and X.  Each QTMR module has four channels.
    Each FLEXPWM submodule or QTMR channel may be set to different parameters.
    Not every channel is routed to a board pin.  Details are listed below.

    Setting *freq* affects the three channels of the same FLEXPWM submodule.
    Only one of *duty_u16* and *duty_ns* should be specified at a time.

    Keyword arguments:

        - *freq* should be an integer which sets the frequency in Hz for the
          PWM cycle. The valid frequency range is 15 Hz resp. 18Hz resp. 24Hz up to > 1 MHz.
        - *duty_u16* sets the duty cycle as a ratio ``duty_u16 / 65536``.
          The duty cycle of a X channel can only be changed, if the A and B channel
          of the respective submodule is not used. Otherwise the duty_16 value of the
          X channel is 32768 (50%).
        - *duty_ns* sets the pulse width in nanoseconds. The limitation for X channels
          apply as well.
        - *center*\=value. An integer sets the center of the pulse within the pulse period.
          The range is 0-65535. The resulting pulse will last from center - duty_u16/2 to
          center + duty_u16/2.
        - *align*\=value. Shortcuts for the pulse center setting, causing the pulse either at
          the center of the frame (value=0), the leading edge at the begin (value=1) or the
          trailing edge at the end of a pulse period (value=2).
        - *invert*\=True|False channel_mask. Setting a bit in the mask inverts the respective channel.
          Bit 0 inverts the first specified channel, bit 2 the second. The default is 0.
        - *sync*\=True|False. If a channel of a module's submodule 0 is already active, other
          submodules of the same module can be forced to be synchronous to submodule 0. Their
          pulse period start then at at same clock cycle. The default is False.
        - *xor*\=0|1|2. If set to 1 or 2, the channel will output the XOR'd signal from channels
          A or B. If set to 1 on channel A or B, both A and B will show the same signal. If set
          to 2, A and B will show alternating signals. For details and an illustration, please
          refer to the MCU's reference manual, chapter "Double Switching PWMs".
        - *deadtime*\=time_ns. This setting affects complementary channels and defines a deadtime
          between an edge of a first channel and the edge of the next channel, in which both
          channels are set to low. That allows connected H-bridges to switch off one side
          of a push-pull driver before switching on the other side.

PWM Methods
~~~~~~~~~~~

The methods are identical to the generic :class:`PWM` class,
with additional keyword arguments to the init() method, matchings those of the constructor.

Each FLEX submodule or QTMR module may run at different frequencies.  The PWM signal
is created by dividing the pwm_clk signal by an integral factor, according to the formula::

    f = pwm_clk / (2**n * m)

with n being in the range of 0..7, and m in the range of 2..65536. pmw_clk is 125Mhz
for MIMXRT1010/1015/1020, 150 MHz for MIMXRT1050/1060/1064 and 160MHz for MIMXRT1170.
The lowest frequency is pwm_clk/2**23 (15, 18, 20Hz). The highest frequency with
U16 resolution is pwm_clk/2**16 (1907, 2288, 2441 Hz), the highest frequency
with 1 percent resolution is pwm_clk/100 (1.25, 1.5, 1.6 MHz). The highest achievable
frequency is pwm_clk/3 for the A/B channels, and pwm_clk/2 for the X channels and QTMR
signal.

PWM Pin Assignment
~~~~~~~~~~~~~~~~~~

Pins are specified in the same way as for the Pin class.  For the assignment of Pins
to PWM signals, refer to the :ref:`PWM pinout <devices_mimxrt_pins_pwm>`.




stm32
-----


See :ref:`pyb.Pin <pyb.Pin>` and :ref:`pyb.Timer <pyb.Timer>`. ::

    from pyb import Pin, Timer

    p = Pin('X1') # X1 has TIM2, CH1
    tim = Timer(2, freq=1000)
    ch = tim.channel(1, Timer.PWM, pin=p)
    ch.pulse_width_percent(50)


rp2
---


There are 8 independent channels each of which have 2 outputs making it 16
PWM channels in total which can be clocked from 7Hz to 125Mhz.

Use the :class:`PWM` class::

    from machine import Pin, PWM

    pwm0 = PWM(Pin(0))      # create PWM object from a pin
    pwm0.freq()             # get current frequency
    pwm0.freq(1000)         # set frequency
    pwm0.duty_u16()         # get current duty cycle, range 0-65535
    pwm0.duty_u16(200)      # set duty cycle, range 0-65535
    pwm0.deinit()           # turn off PWM on the pin


wipy
----

See :class:`Pin` and :class:`Timer`. ::

    from machine import Timer

    # timer 1 in PWM mode and width must be 16 buts
    tim = Timer(1, mode=Timer.PWM, width=16)

    # enable channel A @1KHz with a 50.55% duty cycle
    tim_a = tim.channel(Timer.A, freq=1000, duty_cycle=5055)


ESP32 tutorial
--------------

Pulse width modulation (PWM) is a way to get an artificial analog output on a
digital pin.  It achieves this by rapidly toggling the pin from low to high.
There are two parameters associated with this: the frequency of the toggling,
and the duty cycle.  The duty cycle is defined to be how long the pin is high
compared with the length of a single period (low plus high time).  Maximum
duty cycle is when the pin is high all of the time, and minimum is when it is
low all of the time.

* More comprehensive example with all 16 PWM channels and 8 timers::

    from machine import Pin, PWM
    try:
        f = 100  # Hz
        d = 1024 // 16  # 6.25%
        pins = (15, 2, 4, 16, 18, 19, 22, 23, 25, 26, 27, 14 , 12, 13, 32, 33)
        pwms = []
        for i, pin in enumerate(pins):
            pwms.append(PWM(Pin(pin), freq=f * (i // 2 + 1), duty= 1023 if i==15 else d * (i + 1)))
            print(pwms[i])
    finally:
        for pwm in pwms:
            try:
                pwm.deinit()
            except:
                pass

  Output is::

    PWM(Pin(15), freq=100, duty=64, resolution=10, mode=0, channel=0, timer=0)
    PWM(Pin(2), freq=100, duty=128, resolution=10, mode=0, channel=1, timer=0)
    PWM(Pin(4), freq=200, duty=192, resolution=10, mode=0, channel=2, timer=1)
    PWM(Pin(16), freq=200, duty=256, resolution=10, mode=0, channel=3, timer=1)
    PWM(Pin(18), freq=300, duty=320, resolution=10, mode=0, channel=4, timer=2)
    PWM(Pin(19), freq=300, duty=384, resolution=10, mode=0, channel=5, timer=2)
    PWM(Pin(22), freq=400, duty=448, resolution=10, mode=0, channel=6, timer=3)
    PWM(Pin(23), freq=400, duty=512, resolution=10, mode=0, channel=7, timer=3)
    PWM(Pin(25), freq=500, duty=576, resolution=10, mode=1, channel=0, timer=0)
    PWM(Pin(26), freq=500, duty=640, resolution=10, mode=1, channel=1, timer=0)
    PWM(Pin(27), freq=600, duty=704, resolution=10, mode=1, channel=2, timer=1)
    PWM(Pin(14), freq=600, duty=768, resolution=10, mode=1, channel=3, timer=1)
    PWM(Pin(12), freq=700, duty=832, resolution=10, mode=1, channel=4, timer=2)
    PWM(Pin(13), freq=700, duty=896, resolution=10, mode=1, channel=5, timer=2)
    PWM(Pin(32), freq=800, duty=960, resolution=10, mode=1, channel=6, timer=3)
    PWM(Pin(33), freq=800, duty=1023, resolution=10, mode=1, channel=7, timer=3)

* Example of a smooth frequency change::

    from utime import sleep
    from machine import Pin, PWM

    F_MIN = 500
    F_MAX = 1000

    f = F_MIN
    delta_f = 1

    p = PWM(Pin(5), f)
    print(p)

    while True:
        p.freq(f)

        sleep(10 / F_MIN)

        f += delta_f
        if f >= F_MAX or f <= F_MIN:
            delta_f = -delta_f

  See PWM wave at Pin(5) with an oscilloscope.

* Example of a smooth duty change::

    from utime import sleep
    from machine import Pin, PWM

    DUTY_MAX = 2**16 - 1

    duty_u16 = 0
    delta_d = 16

    p = PWM(Pin(5), 1000, duty_u16=duty_u16)
    print(p)

    while True:
        p.duty_u16(duty_u16)

        sleep(1 / 1000)

        duty_u16 += delta_d
        if duty_u16 >= DUTY_MAX:
            duty_u16 = DUTY_MAX
            delta_d = -delta_d
        elif duty_u16 <= 0:
            duty_u16 = 0
            delta_d = -delta_d

  See PWM wave at Pin(5) with an oscilloscope.

Note: the Pin.OUT mode does not need to be specified.  The channel is initialized
to PWM mode internally once for each Pin that is passed to the PWM constructor.

The following code is wrong::

    pwm = PWM(Pin(5, Pin.OUT), freq=1000, duty=512)  # Pin(5) in PWM mode here
    pwm = PWM(Pin(5, Pin.OUT), freq=500, duty=256)  # Pin(5) in OUT mode here, PWM is off

Use this code instead::

    pwm = PWM(Pin(5), freq=1000, duty=512)
    pwm.init(freq=500, duty=256)



esp8266 tut
-----------

Pulse width modulation (PWM) is a way to get an artificial analog output on a
digital pin.  It achieves this by rapidly toggling the pin from low to high.
There are two parameters associated with this: the frequency of the toggling,
and the duty cycle.  The duty cycle is defined to be how long the pin is high
compared with the length of a single period (low plus high time).  Maximum
duty cycle is when the pin is high all of the time, and minimum is when it is
low all of the time.

On the ESP8266 the pins 0, 2, 4, 5, 12, 13, 14 and 15 all support PWM.  The
limitation is that they must all be at the same frequency, and the frequency
must be between 1Hz and 1kHz.

To use PWM on a pin you must first create the pin object, for example::

    >>> import machine
    >>> p12 = machine.Pin(12)

Then create the PWM object using::

    >>> pwm12 = machine.PWM(p12)

You can set the frequency and duty cycle using::

    >>> pwm12.freq(500)
    >>> pwm12.duty(512)

Note that the duty cycle is between 0 (all off) and 1023 (all on), with 512
being a 50% duty. Values beyond this min/max will be clipped. If you
print the PWM object then it will tell you its current configuration::

    >>> pwm12
    PWM(12, freq=500, duty=512)

You can also call the ``freq()`` and ``duty()`` methods with no arguments to
get their current values.

The pin will continue to be in PWM mode until you deinitialise it using::

    >>> pwm12.deinit()

Fading an LED
-------------

Let's use the PWM feature to fade an LED.  Assuming your board has an LED
connected to pin 2 (ESP-12 modules do) we can create an LED-PWM object using::

    >>> led = machine.PWM(machine.Pin(2), freq=1000)

Notice that we can set the frequency in the PWM constructor.

For the next part we will use timing and some math, so import these modules::

    >>> import time, math

Then create a function to pulse the LED::

    >>> def pulse(l, t):
    ...     for i in range(20):
    ...         l.duty(int(math.sin(i / 10 * math.pi) * 500 + 500))
    ...         time.sleep_ms(t)

You can try this function out using::

    >>> pulse(led, 50)

For a nice effect you can pulse many times in a row::

    >>> for i in range(10):
    ...     pulse(led, 20)

Remember you can use ctrl-C to interrupt the code.

Control a hobby servo
---------------------

Hobby servo motors can be controlled using PWM.  They require a frequency of
50Hz and then a duty between about 40 and 115, with 77 being the centre value.
If you connect a servo to the power and ground pins, and then the signal line
to pin 12 (other pins will work just as well), you can control the motor using::

    >>> servo = machine.PWM(machine.Pin(12), freq=50)
    >>> servo.duty(40)
    >>> servo.duty(115)
    >>> servo.duty(77)

samd
----

Up to five timer device of the SAMD21/SAMD51 MCUs are used for creating PWM signals.

The PWM functions are provided by the :ref:`machine.PWM <machine.PWM>` class.
It supports all basic methods listed for that class. ::

    # Samples for Adafruit ItsyBitsy M4 Express

    from machine import Pin, PWM

    pwm = PWM(Pin(7))      # create PWM object from a pin
    pwm.freq()             # get current frequency
    pwm.freq(1000)         # set frequency
    pwm.duty_u16()         # get current duty cycle, range 0-65535
    pwm.duty_u16(200)      # set duty cycle, range 0-65535
    pwm.deinit()           # turn off PWM on the pin

    pwm                    # show the PWM objects properties


PWM Constructor
```````````````

.. class:: PWM(dest, freq, duty_u16, duty_ns, *, invert, device)
  :noindex:

    Construct and return a new PWM object using the following parameters:

      - *dest* is the Pin object on which the PWM is output.

    PWM objects are provided by TCC timer module. The TCC timer modules have up
    to six channels and eight outputs. All channels of a module run at the same
    frequency, but allow for different duty cycles. Outputs are assigned to channels
    in modulo-n fashion, where n is the number of channels. Outputs of a channel
    have the same frequency and duty rate, but may have different polarity.
    So if for instance a module has four channels, output 0 and 4, 1 and 5,
    2 and 6, 3, and 7 share the same frequency and duty rate.

    Only one of *duty_u16* and *duty_ns* should be specified at a time.

    Keyword arguments:

      - *freq* should be an integer which sets the frequency in Hz for the
        PWM cycle. The valid frequency range is 1 Hz to 24 MHz.
      - *duty_u16* sets the duty cycle as a ratio ``duty_u16 / 65536``.
        The duty cycle of a X channel can only be changed, if the A and B channel
        of the respective submodule is not used. Otherwise the duty_16 value of the
        X channel is 32768 (50%).
      - *duty_ns* sets the pulse width in nanoseconds. The limitation for X channels
        apply as well.
      - *invert*\=True|False. Setting a bit inverts the respective output.
      - *device*\=n Use TCC module n if available. At some pins two TCC modules could be
        used. If not device is mentioned, the software tries to use a module which is not yet
        used for a PWM signal. But if pins shall have the same frequency and/or duty cycle
        to be changed synchronously, they must be driven by the same TCC module.

PWM Methods
```````````

The methods are identical to the generic :ref:`machine.PWM <machine.PWM>` class,
with additional keyword arguments to the init() method, matchings those of the constructor.

PWM Pin Assignment
``````````````````

Pins are specified in the same way as for the Pin class.  For the assignment of Pins
to PWM signals, refer to the :ref:`SAMD pinout <samd_pinout>`.

