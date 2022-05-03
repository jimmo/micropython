.. currentmodule:: machine

class :class:`Timer`
====================

Hardware timers deal with timing of periods and events. Timers are perhaps
the most flexible and heterogeneous kind of hardware in MCUs and SoCs,
differently greatly from a model to a model. MicroPython's Timer class
defines a baseline operation of executing a callback with a given period
(or once after some delay), and allow specific boards to define more
non-standard behaviour (which thus won't be portable to other boards).

See discussion of :ref:`important constraints <machine_callbacks>` on
Timer callbacks. TODO

.. note::

    Memory can't be allocated inside irq handlers (an interrupt) and so
    exceptions raised within a handler don't give much information.  See
    :func:`micropython.alloc_emergency_exception_buf` for how to get around this
    limitation.

If you are using a WiPy board please refer to :class:`TimerWiPy`
instead of this class.

Constructors
------------

.. class:: Timer(id, /, ...)

    Construct a new timer object of the given ``id``. ``id`` of -1 constructs a
    virtual timer (if supported by a board).
    ``id`` shall not be passed as a keyword argument.

    See ``init`` for parameters of initialisation.

Methods
-------

.. method:: Timer.init(*, mode=Timer.PERIODIC, period=-1, callback=None)

    Initialise the timer. Example::

       def mycallback(t):
           pass

       # periodic with 100ms period
       tim.init(period=100, callback=mycallback)

       # one shot firing after 1000ms
       tim.init(mode=Timer.ONE_SHOT, period=1000, callback=mycallback)

    Keyword arguments:

     - ``mode`` can be one of:

       - ``Timer.ONE_SHOT`` - The timer runs once until the configured
         period of the channel expires.
       - ``Timer.PERIODIC`` - The timer runs periodically at the configured
         frequency of the channel.

     - ``period`` - The timer period, in milliseconds.

     - ``callback`` - The callable to call upon expiration of the timer period.
       The callback must take one argument, which is passed the Timer object.
       The ``callback`` argument shall be specified. Otherwise an exception
       will occurr upon timer expiration:
       ``TypeError: 'NoneType' object isn't callable``

.. method:: Timer.deinit()

    Deinitialises the timer. Stops the timer, and disables the timer peripheral.

Constants
---------

.. data:: Timer.ONE_SHOT
          Timer.PERIODIC

    Timer operating mode.

















ESP32
------

The ESP32 port has four hardware timers. Use the :class:`Timer` class
with a timer ID from 0 to 3 (inclusive)::

    from machine import Timer

    tim0 = Timer(0)
    tim0.init(period=5000, mode=Timer.ONE_SHOT, callback=lambda t:print(0))

    tim1 = Timer(1)
    tim1.init(period=2000, mode=Timer.PERIODIC, callback=lambda t:print(1))

The period is in milliseconds.

Virtual timers are not currently supported on this port.



ESP8266
-------

Virtual (RTOS-based) timers are supported. Use the :class:`Timer` class
with timer ID of -1::

    from machine import Timer

    tim = Timer(-1)
    tim.init(period=5000, mode=Timer.ONE_SHOT, callback=lambda t:print(1))
    tim.init(period=2000, mode=Timer.PERIODIC, callback=lambda t:print(2))

The period is in milliseconds.


imx
---


The i.MXRT port has three hardware timers. Use the :class:`Timer` class
with a timer ID from 0 to 2 (inclusive)::

    from machine import Timer

    tim0 = Timer(0)
    tim0.init(period=5000, mode=Timer.ONE_SHOT, callback=lambda t:print(0))

    tim1 = Timer(1)
    tim1.init(period=2000, mode=Timer.PERIODIC, callback=lambda t:print(1))

The period is in milliseconds.

Virtual timers are not currently supported on this port.


stm32
-----

See :ref:`pyb.Timer <pyb.Timer>`. ::

    from pyb import Timer

    tim = Timer(1, freq=1000)
    tim.counter() # get counter value
    tim.freq(0.5) # 0.5 Hz
    tim.callback(lambda t: pyb.LED(1).toggle())



renesas
-------

The RA MCU's system timer peripheral provides a global microsecond timebase and generates interrupts for it. The software timer is available currently and there are unlimited number of them (memory permitting). There is no need to specify the timer id (id=-1 is supported at the moment) as it will default to this.

Use the :mod:`Timer` class::

    from machine import Timer

    tim = Timer(-1)
    tim.init(period=5000, mode=Timer.ONE_SHOT, callback=lambda t:print(1))
    tim.init(period=2000, mode=Timer.PERIODIC, callback=lambda t: print(2))

Following functions are not supported at the present::
    Timer(id)  # hardware timer is not supported.

rp2
---



RP2040's system timer peripheral provides a global microsecond timebase and
generates interrupts for it.  The software timer is available currently,
and there are unlimited number of them (memory permitting). There is no need
to specify the timer id (id=-1 is supported at the moment) as it will default
to this.

Use the :mod:`Timer` class::

    from machine import Timer

    tim = Timer(period=5000, mode=Timer.ONE_SHOT, callback=lambda t:print(1))
    tim.init(period=2000, mode=Timer.PERIODIC, callback=lambda t:print(2))



wipy
----

See :class:`TimerWiPy` and :class:`Pin`.
Timer ``id``'s take values from 0 to 3.::

    from machine import Timer
    from machine import Pin

    tim = Timer(0, mode=Timer.PERIODIC)
    tim_a = tim.channel(Timer.A, freq=1000)
    tim_a.freq(5) # 5 Hz

    p_out = Pin('GP2', mode=Pin.OUT)
    tim_a.irq(trigger=Timer.TIMEOUT, handler=lambda t: p_out.toggle())

Hardware timers
---------------

Timers can be used for a great variety of tasks, calling a function periodically,
counting events, and generating a PWM signal are among the most common use cases.
Each timer consists of two 16-bit channels and this channels can be tied together to
form one 32-bit timer. The operating mode needs to be configured per timer, but then
the period (or the frequency) can be independently configured on each channel.
By using the callback method, the timer event can call a Python function.

Example usage to toggle an LED at a fixed frequency::

    from machine import Timer
    from machine import Pin
    led = Pin('GP16', mode=Pin.OUT)                  # enable GP16 as output to drive the LED
    tim = Timer(3)                                   # create a timer object using timer 3
    tim.init(mode=Timer.PERIODIC)                    # initialize it in periodic mode
    tim_ch = tim.channel(Timer.A, freq=5)            # configure channel A at a frequency of 5Hz
    tim_ch.irq(handler=lambda t:led.toggle(), trigger=Timer.TIMEOUT)        # toggle a LED on every cycle of the timer

Example using named function for the callback::

    from machine import Timer
    from machine import Pin
    tim = Timer(1, mode=Timer.PERIODIC, width=32)
    tim_a = tim.channel(Timer.A | Timer.B, freq=1)   # 1 Hz frequency requires a 32 bit timer

    led = Pin('GP16', mode=Pin.OUT) # enable GP16 as output to drive the LED

    def tick(timer):                # we will receive the timer object when being called
        global led
        led.toggle()                # toggle the LED

    tim_a.irq(handler=tick, trigger=Timer.TIMEOUT)         # create the interrupt

Further examples::

    from machine import Timer
    tim1 = Timer(1, mode=Timer.ONE_SHOT)                               # initialize it in one shot mode
    tim2 = Timer(2, mode=Timer.PWM)                                    # initialize it in PWM mode
    tim1_ch = tim1.channel(Timer.A, freq=10, polarity=Timer.POSITIVE)  # start the event counter with a frequency of 10Hz and triggered by positive edges
    tim2_ch = tim2.channel(Timer.B, freq=10000, duty_cycle=5000)       # start the PWM on channel B with a 50% duty cycle
    tim2_ch.freq(20)                                                   # set the frequency (can also get)
    tim2_ch.duty_cycle(3010)                                           # set the duty cycle to 30.1% (can also get)
    tim2_ch.duty_cycle(3020, Timer.NEGATIVE)                           # set the duty cycle to 30.2% and change the polarity to negative
    tim2_ch.period(2000000)                                            # change the period to 2 seconds


Additional constants for Timer class
------------------------------------

.. data:: Timer.PWM

    PWM timer operating mode.

.. data:: Timer.A
.. data:: Timer.B

    Selects the timer channel. Must be ORed (``Timer.A`` | ``Timer.B``) when
    using a 32-bit timer.

.. data:: Timer.POSITIVE
.. data:: Timer.NEGATIVE

    Timer channel polarity selection (only relevant in PWM mode).

.. data:: Timer.TIMEOUT
.. data:: Timer.MATCH

    Timer channel IRQ triggers.


SAMD
----

The SAMD21/SAMD51 uses software timers. Use the :ref:`machine.Timer <machine.Timer>` class::

    from machine import Timer

    tim0 = Timer()
    tim0.init(period=5000, mode=Timer.ONE_SHOT, callback=lambda t:print(0))

    tim1 = Timer()
    tim1.init(period=2000, mode=Timer.PERIODIC, callback=lambda t:print(1))

The period is in milliseconds.
