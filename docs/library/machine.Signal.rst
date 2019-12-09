.. currentmodule:: machine
.. _machine.Signal:

class Signal -- control and sense external I/O devices
======================================================

The Signal class is a simple extension of the `Pin` class that works in terms of
the logical state of the thing connected to the pin (*on* or *off*), rather than
its electrical value (*high* or *low*). It has a flag that tracks whether the
logical state is inverted from the electrical state (i.e. active-low).

For example, if you have a LED connected to a pin, the `Signal` class can
abstract whether or not that pin is active-low or active-high. Similarly for a
NC or NO relay.

Example::

    from machine import Pin, Signal

    # Suppose you have an active-high LED on pin 0
    led1_pin = Pin(0, Pin.OUT)
    # ... and active-low LED on pin 1
    led2_pin = Pin(1, Pin.OUT)

    # Now to light up both of them using Pin class, you'll need to set
    # them to different values
    led1_pin.value(1)
    led2_pin.value(0)

    # Signal class allows to abstract away active-high/active-low
    # difference
    led1 = Signal(led1_pin, invert=False)
    led2 = Signal(led2_pin, invert=True)

    # Now lighting up them looks the same
    led1.value(1)
    led2.value(1)

    # Even better:
    led1.on()
    led2.on()

Some guidelines on when `Signal` vs `Pin` should be used:

* Use Signal: If you want to control simple on/off (including software
  PWM!) devices like LEDs, multi-segment indicators, relays, buzzers, or
  read simple binary sensors, like normally open or normally closed buttons,
  pulled high or low, reed switches, moisture/flame detectors, etc. etc.
  Summing up, if you have a real physical device/sensor requiring GPIO
  access, you likely should use a Signal.

* Use Pin: If you implement a higher-level protocol or bus to communicate
  with more complex devices.

Constructors
------------

.. class:: Signal(pin_obj, invert=False)
           Signal(pin_arguments..., \*, invert=False)

   Create a `Signal` object. There are two ways to create it:

   * By wrapping existing `Pin` object - universal method which works for
     any port.

     Availability: Portable

   * By passing required `Pin` parameters directly to the Signal constructor,
     skipping the need to create intermediate Pin object.

     Availability: STM32, ESP32, ESP8266

   The arguments are:

     - ``pin_obj`` is an existing Pin object.

     - ``pin_arguments`` are the same arguments that can be passed to the Pin
       constructor.

     - ``invert`` - if True, the signal will be inverted (active low).

Methods
-------

.. method:: Signal.value([x])

   This method allows setting and getting the value of the signal, depending on
   whether the argument *x* is supplied or not.

   If the argument is omitted then this method returns the signal level -- 1 meaning
   the signal is asserted (active) and 0 meaning the signal is inactive.

   If the argument is supplied then this method sets the signal level. The
   argument *x* can be anything that converts to a boolean. If it converts
   to ``True``, the signal is active, otherwise it is inactive.

   Correspondence between signal being active and actual logic level on the
   underlying pin depends on whether signal is inverted (active-low) or not. For
   a non-inverted signal, the active status corresponds to logical 1, and
   inactive to logical 0. For inverted/active-low signal, the active status
   corresponds to logical 0, while inactive to logical 1.

.. method:: Signal.on()

   Activate signal.

.. method:: Signal.off()

   Deactivate signal.
