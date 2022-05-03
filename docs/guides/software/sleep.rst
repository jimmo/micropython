.. _guides_software_sleep:

Sleep and low-power modes
=========================

Deep-sleep mode
---------------

The following code can be used to sleep, wake and check the reset cause::

    import machine

    # check if the device woke from a deep sleep
    if machine.reset_cause() == machine.DEEPSLEEP_RESET:
        print('woke from a deep sleep')

    # put the device to sleep for 10 seconds
    machine.deepsleep(10000)

Notes:

* Calling ``deepsleep()`` without an argument will put the device to sleep
  indefinitely
* A software reset does not change the reset cause

Some ESP32 pins (0, 2, 4, 12-15, 25-27, 32-39) are connected to the RTC during
deep-sleep and can be used to wake the device with the ``wake_on_`` functions in
the :mod:`esp32` module. The output-capable RTC pins (all except 34-39) will
also retain their pull-up or pull-down resistor configuration when entering
deep-sleep.

If the pull resistors are not actively required during deep-sleep and are likely
to cause current leakage (for example a pull-up resistor is connected to ground
through a switch), then they should be disabled to save power before entering
deep-sleep mode::

    from machine import Pin, deepsleep

    # configure input RTC pin with pull-up on boot
    pin = Pin(2, Pin.IN, Pin.PULL_UP)

    # disable pull-up and put the device to sleep for 10 seconds
    pin.init(pull=None)
    machine.deepsleep(10000)

Output-configured RTC pins will also retain their output direction and level in
deep-sleep if pad hold is enabled with the ``hold=True`` argument to
``Pin.init()``.

Non-RTC GPIO pins will be disconnected by default on entering deep-sleep.
Configuration of non-RTC pins - including output level - can be retained by
enabling pad hold on the pin and enabling GPIO pad hold during deep-sleep::

    from machine import Pin, deepsleep
    import esp32

    opin = Pin(19, Pin.OUT, value=1, hold=True) # hold output level
    ipin = Pin(21, Pin.IN, Pin.PULL_UP, hold=True) # hold pull-up

    # enable pad hold in deep-sleep for non-RTC GPIO
    esp32.gpio_deep_sleep_hold(True)

    # put the device to sleep for 10 seconds
    deepsleep(10000)

The pin configuration - including the pad hold - will be retained on wake from
sleep. See :class:`machine.Pin` for a further discussion of pad holding.


ESP8266
-------
Deep-sleep mode
---------------

Connect GPIO16 to the reset pin (RST on HUZZAH).  Then the following code
can be used to sleep, wake and check the reset cause::

    import machine

    # configure RTC.ALARM0 to be able to wake the device
    rtc = machine.RTC()
    rtc.irq(trigger=rtc.ALARM0, wake=machine.DEEPSLEEP)

    # check if the device woke from a deep sleep
    if machine.reset_cause() == machine.DEEPSLEEP_RESET:
        print('woke from a deep sleep')

    # set RTC.ALARM0 to fire after 10 seconds (waking the device)
    rtc.alarm(rtc.ALARM0, 10000)

    # put the device to sleep
    machine.deepsleep()


Power control
=============

The ESP8266 provides the ability to change the CPU frequency on the fly, and
enter a deep-sleep state.  Both can be used to manage power consumption.

Changing the CPU frequency
--------------------------

The machine module has a function to get and set the CPU frequency.  To get the
current frequency use::

    >>> import machine
    >>> machine.freq()
    80000000

By default the CPU runs at 80MHz.  It can be changed to 160MHz if you need more
processing power, at the expense of current consumption::

    >>> machine.freq(160000000)
    >>> machine.freq()
    160000000

You can change to the higher frequency just while your code does the heavy
processing and then change back when it's finished.

Deep-sleep mode
---------------

The deep-sleep mode will shut down the ESP8266 and all its peripherals,
including the WiFi (but not including the real-time-clock, which is used to wake
the chip).  This drastically reduces current consumption and is a good way to
make devices that can run for a while on a battery.

To be able to use the deep-sleep feature you must connect GPIO16 to the reset
pin (RST on the Adafruit Feather HUZZAH board).  Then the following code can be
used to sleep and wake the device::

    import machine

    # configure RTC.ALARM0 to be able to wake the device
    rtc = machine.RTC()
    rtc.irq(trigger=rtc.ALARM0, wake=machine.DEEPSLEEP)

    # set RTC.ALARM0 to fire after 10 seconds (waking the device)
    rtc.alarm(rtc.ALARM0, 10000)

    # put the device to sleep
    machine.deepsleep()

Note that when the chip wakes from a deep-sleep it is completely reset,
including all of the memory.  The boot scripts will run as usual and you can
put code in them to check the reset cause to perhaps do something different if
the device just woke from a deep-sleep.  For example, to print the reset cause
you can use::

    if machine.reset_cause() == machine.DEEPSLEEP_RESET:
        print('woke from a deep sleep')
    else:
        print('power on or hard reset')

