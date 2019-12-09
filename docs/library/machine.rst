:mod:`machine` --- functions related to the hardware
====================================================

.. module:: machine
   :synopsis: functions related to the hardware

The ``machine`` module contains specific functions related to the hardware
on a particular board. Most functions in this module allow to achieve direct
and unrestricted access to and control of hardware blocks on a system
(like CPU, timers, buses, etc.). Used incorrectly, this can lead to
malfunction, lockups, crashes of your board, and in extreme cases, hardware
damage.

The functions and classes provided by this module are intended to provide a
common abstraction, such that code (and especially drivers) written using this
module will be portable between different MicroPython ports. See
:ref:`portable_apis` for more information. Any function or class marked as
**Portable** must be provided by all major ports (currently STM32, ESP32,
ESP8266).

.. _machine_callbacks:

Many of the classes in the ``machine`` module make use of callbacks. These
callbacks should be considered as executing in an interrupt context. This is
true for both physical devices with IDs >= 0 and "virtual" devices with negative
IDs like -1 (these "virtual" devices are still thin shims on top of real
hardware and real hardware interrupts). See :ref:`isr_rules` for more
information.

Reset related functions
-----------------------

.. function:: reset()

   Resets the device in a manner similar to pushing the external RESET
   button.

   |availability_portable|

.. function:: reset_cause()

   Get the reset cause. See :ref:`constants <machine_constants>` for the possible return values.

   |availability_portable|

Interrupt related functions
---------------------------

.. function:: disable_irq()

   Disable interrupt requests.
   Returns the previous IRQ state which should be considered an opaque value.
   This return value should be passed to the `enable_irq()` function to restore
   interrupts to their original state, before `disable_irq()` was called.

   |availability_portable|

.. function:: enable_irq(state)

   Re-enable interrupt requests.
   The *state* parameter should be the value that was returned from the most
   recent call to the `disable_irq()` function.

   |availability_portable|

Power related functions
-----------------------

.. function:: freq()

    Returns CPU frequency in hertz.

   |availability_portable|

.. function:: idle()

   Gates the clock to the CPU, useful to reduce power consumption at any time during
   short or long periods. Peripherals continue working and execution resumes as soon
   as any interrupt is triggered (on many ports this includes system timer
   interrupt occurring at regular intervals on the order of millisecond).

   |availability_portable|

.. function:: sleep()

   |availability_deprecated| Use `lightsleep()` instead with no arguments.

.. function:: lightsleep([time_ms])
              deepsleep([time_ms])

   Stops execution in an attempt to enter a low power state.

   If *time_ms* is specified then this will be the maximum time in milliseconds that
   the sleep will last for.  Otherwise the sleep can last indefinitely.

   With or without a timout, execution may resume at any time if there are events
   that require processing.  Such events, or wake sources, should be configured before
   sleeping, like `Pin` change or `RTC` timeout.

   The precise behaviour and power-saving capabilities of lightsleep and deepsleep is
   highly dependent on the underlying hardware, but the general properties are:

   * A lightsleep has full RAM and state retention.  Upon wake execution is resumed
     from the point where the sleep was requested, with all subsystems operational.

   * A deepsleep may not retain RAM or any other state of the system (for example
     peripherals or network interfaces).  Upon wake execution is resumed from the main
     script, similar to a hard or power-on reset. The `reset_cause()` function will
     return `machine.DEEPSLEEP` and this can be used to distinguish a deepsleep wake
     from other resets.

   |availability_portable|

.. function:: wake_reason()

   Get the wake reason. See :ref:`constants <machine_constants>` for the possible return values.

   |availability_cc3200_esp32|

Miscellaneous functions
-----------------------

.. function:: unique_id()

   Returns a byte string with a unique identifier of a board/SoC. It will vary
   from a board/SoC instance to another, if the underlying hardware allows.
   Length varies by hardware (so use substring of the full value if you expect a
   short ID). In some MicroPython ports, the ID corresponds to the network MAC
   address.

   |availability_portable|

.. function:: time_pulse_us(pin, pulse_level, timeout_us=1000000)

   Time a pulse on the given *pin*, and return the duration of the pulse in
   microseconds.  The *pulse_level* argument should be 0 to time a low pulse
   or 1 to time a high pulse.

   If the current input value of the pin is different to *pulse_level*,
   the function first (*) waits until the pin input becomes equal to *pulse_level*,
   then (**) times the duration that the pin is equal to *pulse_level*.
   If the pin is already equal to *pulse_level* then timing starts straight away.

   The function will return -2 if there was timeout waiting for condition marked
   (*) above, and -1 if there was timeout during the main measurement, marked (**)
   above. The timeout is the same for both cases and given by *timeout_us* (which
   is in microseconds).

   |availability_portable|

.. function:: rng()

   Return a 24-bit software generated random number.

   Availability: STM32, WiPy (CC3200).

.. _machine_constants:

Constants
---------

.. data:: machine.IDLE
          machine.SLEEP
          machine.DEEPSLEEP

    IRQ wake values.

    |availability_portable|

.. data:: machine.PWRON_RESET
          machine.HARD_RESET
          machine.WDT_RESET
          machine.DEEPSLEEP_RESET
          machine.SOFT_RESET

    Reset causes.

    |availability_portable|

.. data:: machine.PIN_WAKE

    Wake-up reasons.

    |availability_cc3200_esp32|

.. data:: machine.WLAN_WAKE
          machine.RTC_WAKE

    Additional wake-up reasons for CC3200.

    |availability_cc3200|

.. data:: machine.EXT0_WAKE
          machine.EXT1_WAKE
          machine.TIMER_WAKE
          machine.TOUCHPAD_WAKE
          machine.ULP_WAKE

    Additional wake-up reasons for ESP32.

    |availability_ESP32|


Classes
-------

The following classes are part of the portable API (provided by all ports)

.. toctree::
   :maxdepth: 1

   machine.Pin.rst
   machine.Signal.rst
   machine.ADC.rst
   machine.UART.rst
   machine.SPI.rst
   machine.I2C.rst
   machine.RTC.rst
   machine.Timer.rst
   machine.WDT.rst

These classes are provided by some, but not all, ports. See their documentation
for details.

.. toctree::
   :maxdepth: 1

   machine.SDCard.rst
   machine.SD.rst

Port-specific information
-------------------------

These guides provide additional information specific to using the machine API on
the various ports.

.. toctree::
   :maxdepth: 1

   machine_pyboard.rst
   machine_stm32.rst
   machine_esp32.rst
   machine_esp8266.rst
   machine_nrf.rst
   machine_cc3200.rst
