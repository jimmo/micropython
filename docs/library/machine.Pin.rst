.. currentmodule:: machine

class :class:`Pin`
==================

A pin object is used to control I/O pins (also known as GPIO - general-purpose
input/output).  Pin objects are commonly associated with a physical pin that can
drive an output voltage and read input voltages.  The pin class has methods to set the mode of
the pin (IN, OUT, etc) and methods to get and set the digital logic level.
For analog control of a pin, see the :class:`ADC` class.

A pin object is constructed by using an identifier which unambiguously
specifies a certain I/O pin.  The allowed forms of the identifier and the
physical pin that the identifier maps to are port-specific.  Possibilities
for the identifier are an integer, a string or a tuple with port and pin
number.

Example::

    from machine import Pin

    # create an output pin on pin #0
    p0 = Pin(0, Pin.OUT)

    # set the value low then high
    p0.value(0)
    p0.value(1)

    # create an input pin on pin #2, with a pull up resistor
    p2 = Pin(2, Pin.IN, Pin.PULL_UP)

    # read and print the pin value
    print(p2.value())

    # reconfigure pin #0 in input mode with a pull down resistor
    p0.init(p0.IN, p0.PULL_DOWN)

    # configure an irq callback
    p0.irq(lambda p:print(p))

Constructors
------------

.. class:: Pin(id, mode=-1, pull=-1, *, value=None, drive=0, alt=-1)

    Access the pin peripheral (GPIO pin) associated with the given ``id``.  If
    additional arguments are given in the constructor then they are used to initialise
    the pin.  Any settings that are not specified will remain in their previous state.

    The arguments are:

     - ``id`` is mandatory and can be an arbitrary object.  Among possible value
       types are: int (an internal Pin identifier), str (a Pin name), and tuple
       (pair of [port, pin]).

     - ``mode`` specifies the pin mode, which can be one of:

       - ``Pin.IN`` - Pin is configured for input.  If viewed as an output the pin
         is in high-impedance state.

       - ``Pin.OUT`` - Pin is configured for (normal) output.

       - ``Pin.OPEN_DRAIN`` - Pin is configured for open-drain output. Open-drain
         output works in the following way: if the output value is set to 0 the pin
         is active at a low level; if the output value is 1 the pin is in a high-impedance
         state.  Not all ports implement this mode, or some might only on certain pins.

       - ``Pin.ALT`` - Pin is configured to perform an alternative function, which is
         port specific.  For a pin configured in such a way any other Pin methods
         (except :meth:`Pin.init`) are not applicable (calling them will lead to undefined,
         or a hardware-specific, result).  Not all ports implement this mode.

       - ``Pin.ALT_OPEN_DRAIN`` - The Same as ``Pin.ALT``, but the pin is configured as
         open-drain.  Not all ports implement this mode.

       - ``Pin.ANALOG`` - Pin is configured for analog input, see the :class:`ADC` class.

     - ``pull`` specifies if the pin has a (weak) pull resistor attached, and can be
       one of:

       - ``None`` - No pull up or down resistor.
       - ``Pin.PULL_UP`` - Pull up resistor enabled.
       - ``Pin.PULL_DOWN`` - Pull down resistor enabled.

     - ``value`` is valid only for Pin.OUT and Pin.OPEN_DRAIN modes and specifies initial
       output pin value if given, otherwise the state of the pin peripheral remains
       unchanged.

     - ``drive`` specifies the output power of the pin and can be one of: ``Pin.DRIVE_0``,
       ``Pin.DRIVE_1``, etc., increasing in drive strength.  The actual current driving
       capabilities are port dependent.  Not all ports implement this argument.

     - ``alt`` specifies an alternate function for the pin and the values it can take are
       port dependent.  This argument is valid only for ``Pin.ALT`` and ``Pin.ALT_OPEN_DRAIN``
       modes.  It may be used when a pin supports more than one alternate function.  If only
       one pin alternate function is supported the this argument is not required.  Not all
       ports implement this argument.

    As specified above, the Pin class allows to set an alternate function for a particular
    pin, but it does not specify any further operations on such a pin.  Pins configured in
    alternate-function mode are usually not used as GPIO but are instead driven by other
    hardware peripherals.  The only operation supported on such a pin is re-initialising,
    by calling the constructor or :meth:`Pin.init` method.  If a pin that is configured in
    alternate-function mode is re-initialised with ``Pin.IN``, ``Pin.OUT``, or
    ``Pin.OPEN_DRAIN``, the alternate function will be removed from the pin.

Methods
-------

.. method:: Pin.init(mode=-1, pull=-1, *, value=None, drive=0, alt=-1)

    Re-initialise the pin using the given parameters.  Only those arguments that
    are specified will be set.  The rest of the pin peripheral state will remain
    unchanged.  See the constructor documentation for details of the arguments.

    Returns ``None``.

.. method:: Pin.value([x])

    This method allows to set and get the value of the pin, depending on whether
    the argument ``x`` is supplied or not.

    If the argument is omitted then this method gets the digital logic level of
    the pin, returning 0 or 1 corresponding to low and high voltage signals
    respectively.  The behaviour of this method depends on the mode of the pin:

     - ``Pin.IN`` - The method returns the actual input value currently present
       on the pin.
     - ``Pin.OUT`` - The behaviour and return value of the method is undefined.
     - ``Pin.OPEN_DRAIN`` - If the pin is in state '0' then the behaviour and
       return value of the method is undefined.  Otherwise, if the pin is in
       state '1', the method returns the actual input value currently present
       on the pin.

    If the argument is supplied then this method sets the digital logic level of
    the pin.  The argument ``x`` can be anything that converts to a boolean.
    If it converts to ``True``, the pin is set to state '1', otherwise it is set
    to state '0'.  The behaviour of this method depends on the mode of the pin:

     - ``Pin.IN`` - The value is stored in the output buffer for the pin.  The
       pin state does not change, it remains in the high-impedance state.  The
       stored value will become active on the pin as soon as it is changed to
       ``Pin.OUT`` or ``Pin.OPEN_DRAIN`` mode.
     - ``Pin.OUT`` - The output buffer is set to the given value immediately.
     - ``Pin.OPEN_DRAIN`` - If the value is '0' the pin is set to a low voltage
       state.  Otherwise the pin is set to high-impedance state.

    When setting the value this method returns ``None``.

.. method:: Pin.__call__([x])

    Pin objects are callable.  The call method provides a (fast) shortcut to set
    and get the value of the pin.  It is equivalent to Pin.value([x]).
    See :meth:`Pin.value` for more details.

.. method:: Pin.on()

    Set pin to "1" output level.

.. method:: Pin.off()

    Set pin to "0" output level.

.. method:: Pin.irq(handler=None, trigger=(Pin.IRQ_FALLING | Pin.IRQ_RISING), *, priority=1, wake=None, hard=False)

    Configure an interrupt handler to be called when the trigger source of the
    pin is active.  If the pin mode is ``Pin.IN`` then the trigger source is
    the external value on the pin.  If the pin mode is ``Pin.OUT`` then the
    trigger source is the output buffer of the pin.  Otherwise, if the pin mode
    is ``Pin.OPEN_DRAIN`` then the trigger source is the output buffer for
    state '0' and the external pin value for state '1'.

    The arguments are:

     - ``handler`` is an optional function to be called when the interrupt
       triggers. The handler must take exactly one argument which is the
       ``Pin`` instance.

     - ``trigger`` configures the event which can generate an interrupt.
       Possible values are:

       - ``Pin.IRQ_FALLING`` interrupt on falling edge.
       - ``Pin.IRQ_RISING`` interrupt on rising edge.
       - ``Pin.IRQ_LOW_LEVEL`` interrupt on low level.
       - ``Pin.IRQ_HIGH_LEVEL`` interrupt on high level.

       These values can be OR'ed together to trigger on multiple events.

     - ``priority`` sets the priority level of the interrupt.  The values it
       can take are port-specific, but higher values always represent higher
       priorities.

     - ``wake`` selects the power mode in which this interrupt can wake up the
       system.  It can be :data:`IDLE`, :data:`SLEEP` or :data:`DEEPSLEEP`.
       These values can also be OR'ed together to make a pin generate interrupts in
       more than one power mode.

     - ``hard`` if true a hardware interrupt is used. This reduces the delay
       between the pin change and the handler being called. Hard interrupt
       handlers may not allocate memory; see :ref:`guides_software_interrupts`.
       Not all ports support this argument.

    This method returns a callback object.

The following methods are not part of the core Pin API and only implemented on certain ports.

.. method:: Pin.low()

    Set pin to "0" output level.

    Availability: nrf, rp2, stm32 ports.

.. method:: Pin.high()

    Set pin to "1" output level.

    Availability: nrf, rp2, stm32 ports.

.. method:: Pin.mode([mode])

    Get or set the pin mode.
    See the constructor documentation for details of the ``mode`` argument.

    Availability: cc3200, stm32 ports.

.. method:: Pin.pull([pull])

    Get or set the pin pull state.
    See the constructor documentation for details of the ``pull`` argument.

    Availability: cc3200, stm32 ports.

.. method:: Pin.drive([drive])

    Get or set the pin drive strength.
    See the constructor documentation for details of the ``drive`` argument.

    Availability: cc3200 port.

Constants
---------

The following constants are used to configure the pin objects.  Note that
not all constants are available on all ports.

.. data:: Pin.IN
          Pin.OUT
          Pin.OPEN_DRAIN
          Pin.ALT
          Pin.ALT_OPEN_DRAIN
          Pin.ANALOG

    Selects the pin mode.

.. data:: Pin.PULL_UP
          Pin.PULL_DOWN
          Pin.PULL_HOLD

    Selects whether there is a pull up/down resistor.  Use the value
    ``None`` for no pull.

.. data:: Pin.DRIVE_0
          Pin.DRIVE_1
          Pin.DRIVE_2

    Selects the pin drive strength.  A port may define additional drive
    constants with increasing number corresponding to increasing drive
    strength.

.. data:: Pin.IRQ_FALLING
          Pin.IRQ_RISING
          Pin.IRQ_LOW_LEVEL
          Pin.IRQ_HIGH_LEVEL

    Selects the IRQ trigger type.

















ESP32
-----

Use the :class:`Pin` class::

    from machine import Pin

    p0 = Pin(0, Pin.OUT)    # create output pin on GPIO0
    p0.on()                 # set pin to "on" (high) level
    p0.off()                # set pin to "off" (low) level
    p0.value(1)             # set pin to on/high

    p2 = Pin(2, Pin.IN)     # create input pin on GPIO2
    print(p2.value())       # get value, 0 or 1

    p4 = Pin(4, Pin.IN, Pin.PULL_UP) # enable internal pull-up resistor
    p5 = Pin(5, Pin.OUT, value=1) # set pin high on creation
    p6 = Pin(6, Pin.OUT, drive=Pin.DRIVE_3) # set maximum drive strength

Available Pins are from the following ranges (inclusive): 0-19, 21-23, 25-27, 32-39.
These correspond to the actual GPIO pin numbers of ESP32 chip.  Note that many
end-user boards use their own adhoc pin numbering (marked e.g. D0, D1, ...).
For mapping between board logical pins and physical chip pins consult your board
documentation.

Four drive strengths are supported, using the ``drive`` keyword argument to the
``Pin()`` constructor or ``Pin.init()`` method, with different corresponding
safe maximum source/sink currents and approximate internal driver resistances:

 - ``Pin.DRIVE_0``: 5mA / 130 ohm
 - ``Pin.DRIVE_1``: 10mA / 60 ohm
 - ``Pin.DRIVE_2``: 20mA / 30 ohm (default strength if not configured)
 - ``Pin.DRIVE_3``: 40mA / 15 ohm

The ``hold=`` keyword argument to ``Pin()`` and ``Pin.init()`` will enable the
ESP32 "pad hold" feature. When set to ``True``, the pin configuration
(direction, pull resistors and output value) will be held and any further
changes (including changing the output level) will not be applied. Setting
``hold=False`` will immediately apply any outstanding pin configuration changes
and release the pin. Using ``hold=True`` while a pin is already held will apply
any configuration changes and then immediately reapply the hold.

Notes:

* Pins 1 and 3 are REPL UART TX and RX respectively

* Pins 6, 7, 8, 11, 16, and 17 are used for connecting the embedded flash,
  and are not recommended for other uses

* Pins 34-39 are input only, and also do not have internal pull-up resistors

* See :ref:`guides_software_sleep` for a discussion of pin behaviour during sleep

There's a higher-level abstraction :class:`Signal`
which can be used to invert a pin. Useful for illuminating active-low LEDs
using ``on()`` or ``value(1)``.

ESP8266
-------

Pins and GPIO
-------------

Use the :class:`Pin` class::

    from machine import Pin

    p0 = Pin(0, Pin.OUT)    # create output pin on GPIO0
    p0.on()                 # set pin to "on" (high) level
    p0.off()                # set pin to "off" (low) level
    p0.value(1)             # set pin to on/high

    p2 = Pin(2, Pin.IN)     # create input pin on GPIO2
    print(p2.value())       # get value, 0 or 1

    p4 = Pin(4, Pin.IN, Pin.PULL_UP) # enable internal pull-up resistor
    p5 = Pin(5, Pin.OUT, value=1) # set pin high on creation

Available pins are: 0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, which correspond
to the actual GPIO pin numbers of ESP8266 chip. Note that many end-user
boards use their own adhoc pin numbering (marked e.g. D0, D1, ...). As
MicroPython supports different boards and modules, physical pin numbering
was chosen as the lowest common denominator. For mapping between board
logical pins and physical chip pins, consult your board documentation.

Note that Pin(1) and Pin(3) are REPL UART TX and RX respectively.
Also note that Pin(16) is a special pin (used for wakeup from deepsleep
mode) and may be not available for use with higher-level classes like
``Neopixel``.

There's a higher-level abstraction :class:`Signal`
which can be used to invert a pin. Useful for illuminating active-low LEDs
using ``on()`` or ``value(1)``.


imx
---


Use the :class:`Pin` class::

    from machine import Pin

    p0 = Pin('D0', Pin.OUT) # create output pin on GPIO0
    p0.on()                 # set pin to "on" (high) level
    p0.off()                # set pin to "off" (low) level
    p0.value(1)             # set pin to on/high

    p2 = Pin('D2', Pin.IN)  # create input pin on GPIO2
    print(p2.value())       # get value, 0 or 1

    p4 = Pin('D4', Pin.IN, Pin.PULL_UP) # enable internal pull-up resistor
    p5 = Pin('D5', Pin.OUT, value=1) # set pin high on creation

    p6 = Pin(pin.cpu.GPIO_B1_15, Pin.OUT) # Use the cpu pin name.

Available Pins follow the ranges and labelling of the respective board, like:

- 0-33 for Teensy 4.0,
- 0-21 for the MIMXRT10xx-EVK board, or 'D0-Dxx', or 'A0-Ann',
- 0-14 for the Olimex RT1010Py board, or 'D0'-'Dxx' and 'A0'-'Ann'
- 'J3_xx', 'J4_xx', 'J5_xx' for the Seeed ARCH MIX board,

or the pin names of the Pin.board or Pin.cpu classes.

Notes:

* The MIMXRT1xxx-EVK boards may have other on-board devices connected to these
  pins, limiting it's use for input or output.
* At the MIMXRT1010_EVK, pins D4, D5 and D9 of the Arduino connector are by
  default not connected to the MCU. For details refer to the schematics.
* At the MIMXRT1170_EVK board, the inner rows of the Arduino connectors are assigned as follows:
    - D16 - D23: J9, odd pin numbers; D17 is by default not connected.
    - D24 - D27: J26, odd pin numbers; J63-J66 have to be closed to enable these pins.
    - D29 - D36: J25, odd pin numbers; D29 and D30 are by default not connected.

There's a higher-level abstraction :class:`Signal`
which can be used to invert a pin.  Useful for illuminating active-low LEDs
using ``on()`` or ``value(1)``.


stm32
-----


See :ref:`pyb.Pin <pyb.Pin>`. ::

    from pyb import Pin

    p_out = Pin('X1', Pin.OUT_PP)
    p_out.high()
    p_out.low()

    p_in = Pin('X2', Pin.IN, Pin.PULL_UP)
    p_in.value() # get value, 0 or 1


renesas
-------


Use the :class:`Pin` class::

    from machine import Pin

    p0 = Pin('P000', Pin.OUT)      # create output pin on P000
    p0.on()                        # set pin to "on" (high) level
    p0.off()                       # set pin to "off" (low) level
    p0.value(1)                    # set pin to on/high

    p2 = Pin(Pin.cpu.P002, Pin.IN) # create input pin on P002
    print(p2.value())              # get value, 0 or 1

    p4 = Pin('P004', Pin.PULL_UP)      # enable internal pull-up register
    p5 = Pin('P005', Pin.OUT, value=1) # set pin high on creation

Pin id is available corresponding to the RA MCU's pin name which are Pin.cpu.P106 and 'P106'. The RA MCU has many feature's pins. However, there are some cases that pin feature is fixed or not connected by the board. Please confirm the board manual for the pin mapping.

The following *drive* keyword argument are available if the port drive capability of the Pin is supported by the MCU::

    Pin.DRIVE_0: Low drive
    Pin.DRIVE_1: Middle drive
    Pin.DRIVE_2: Middle drive for I2C Fast-mode
    Pin.DRIVE_3: High drive

The *alt* keyword argument is not supported.

The following functions are not supported::

    Pin.irq(priority=)  # priority keyword argument is not supported
    Pin.irq(wake=)      # wake keyword argument is not supported
    Pin.irq(hard=)      # hard keyword argument is ignored because hardware interrupt is used
    Pin.mode()
    Pin.pull()
    Pin.drive()


rp2
---


Use the :class:`Pin` class::

    from machine import Pin

    p0 = Pin(0, Pin.OUT)    # create output pin on GPIO0
    p0.on()                 # set pin to "on" (high) level
    p0.off()                # set pin to "off" (low) level
    p0.value(1)             # set pin to on/high

    p2 = Pin(2, Pin.IN)     # create input pin on GPIO2
    print(p2.value())       # get value, 0 or 1

    p4 = Pin(4, Pin.IN, Pin.PULL_UP) # enable internal pull-up resistor
    p5 = Pin(5, Pin.OUT, value=1) # set pin high on creation


cc3200
------

See :class:`Pin`. ::

    from machine import Pin

    # initialize GP2 in gpio mode (alt=0) and make it an output
    p_out = Pin('GP2', mode=Pin.OUT)
    p_out.value(1)
    p_out.value(0)
    p_out.toggle()
    p_out(True)

    # make GP1 an input with the pull-up enabled
    p_in = Pin('GP1', mode=Pin.IN, pull=Pin.PULL_UP)
    p_in() # get value, 0 or 1


zephyr
------


Pins and GPIO
-------------

Use the :class:`Pin` class::

    from machine import Pin

    pin = Pin(("GPIO_1", 21), Pin.IN)   # create input pin on GPIO1
    print(pin)                          # print pin port and number

    pin.init(Pin.OUT, Pin.PULL_UP, value=1)     # reinitialize pin

    pin.value(1)                        # set pin to high
    pin.value(0)                        # set pin to low

    pin.on()                            # set pin to high
    pin.off()                           # set pin to low

    pin = Pin(("GPIO_1", 21), Pin.IN)   # create input pin on GPIO1

    pin = Pin(("GPIO_1", 21), Pin.OUT, value=1)         # set pin high on creation

    pin = Pin(("GPIO_1", 21), Pin.IN, Pin.PULL_UP)      # enable internal pull-up resistor

    switch = Pin(("GPIO_2", 6), Pin.IN)                 # create input pin for a switch
    switch.irq(lambda t: print("SW2 changed"))          # enable an interrupt when switch state is changed


esp8266 tut
-----------

The way to connect your board to the external world, and control other
components, is through the GPIO pins.  Not all pins are available to use,
in most cases only pins 0, 2, 4, 5, 12, 13, 14, 15, and 16 can be used.

The pins are available in the machine module, so make sure you import that
first.  Then you can create a pin using::

    >>> pin = machine.Pin(0)

Here, the "0" is the pin that you want to access.  Usually you want to
configure the pin to be input or output, and you do this when constructing
it.  To make an input pin use::

    >>> pin = machine.Pin(0, machine.Pin.IN, machine.Pin.PULL_UP)

You can either use PULL_UP or None for the input pull-mode.  If it's
not specified then it defaults to None, which is no pull resistor. GPIO16
has no pull-up mode.
You can read the value on the pin using::

    >>> pin.value()
    0

The pin on your board may return 0 or 1 here, depending on what it's connected
to.  To make an output pin use::

    >>> pin = machine.Pin(0, machine.Pin.OUT)

Then set its value using::

    >>> pin.value(0)
    >>> pin.value(1)

Or::

    >>> pin.off()
    >>> pin.on()

External interrupts
-------------------

All pins except number 16 can be configured to trigger a hard interrupt if their
input changes.  You can set code (a callback function) to be executed on the
trigger.

Let's first define a callback function, which must take a single argument,
being the pin that triggered the function.  We will make the function just print
the pin::

    >>> def callback(p):
    ...     print('pin change', p)

Next we will create two pins and configure them as inputs::

    >>> from machine import Pin
    >>> p0 = Pin(0, Pin.IN)
    >>> p2 = Pin(2, Pin.IN)

An finally we need to tell the pins when to trigger, and the function to call
when they detect an event::

    >>> p0.irq(trigger=Pin.IRQ_FALLING, handler=callback)
    >>> p2.irq(trigger=Pin.IRQ_RISING | Pin.IRQ_FALLING, handler=callback)

We set pin 0 to trigger only on a falling edge of the input (when it goes from
high to low), and set pin 2 to trigger on both a rising and falling edge.  After
entering this code you can apply high and low voltages to pins 0 and 2 to see
the interrupt being executed.

A hard interrupt will trigger as soon as the event occurs and will interrupt any
running code, including Python code.  As such your callback functions are
limited in what they can do (they cannot allocate memory, for example) and
should be as short and simple as possible.



samd
----

Use the :ref:`machine.Pin <machine.Pin>` class::

    from machine import Pin

    p0 = Pin('D0', Pin.OUT) # create output pin on GPIO0
    p0.on()                 # set pin to "on" (high) level
    p0.off()                # set pin to "off" (low) level
    p0.value(1)             # set pin to on/high

    p2 = Pin('D2', Pin.IN)  # create input pin on GPIO2
    print(p2.value())       # get value, 0 or 1

    p4 = Pin('D4', Pin.IN, Pin.PULL_UP) # enable internal pull-up resistor
    p7 = Pin("PA07", Pin.OUT, value=1) # set pin high on creation

Pins can be denoted by a string or a number. The string is either the
pin label of the respective board, like "D0" or "SDA", or in the form
"Pxnn", where x is A,B,C or D, and nn a two digit number in the range 0-31.
Examples: "PA03", PD31".

Pin numbers are the MCU port numbers in the range::

    PA0..PA31:  0..31
    PB0..PB31: 32..63
    PC0..PC31: 64..95
    PD0..PD31: 96..127

Note: On Adafruit Feather and ItsyBity boards, pin D5 is connected to an external
gate output and can therefore only be used as input.
