.. currentmodule:: machine

class :class:`WDT`
==================

The WDT is used to restart the system when the application crashes and ends
up into a non recoverable state. Once started it cannot be stopped or
reconfigured in any way. After enabling, the application must "feed" the
watchdog periodically to prevent it from expiring and resetting the system.

Example usage::

    from machine import WDT
    wdt = WDT(timeout=2000)  # enable it with a timeout of 2s
    wdt.feed()

Availability of this class: pyboard, WiPy, esp8266, esp32, rp2040, mimxrt.

TODO: :func:`reset_cause`.

Constructors
------------

.. class:: WDT(id=0, timeout=5000)

    Create a WDT object and start it. The timeout must be given in milliseconds.
    Once it is running the timeout cannot be changed and the WDT cannot be stopped either.

    Notes: On the esp32 the minimum timeout is 1 second. On the esp8266 a timeout
    cannot be specified, it is determined by the underlying system. On rp2040 devices,
    the maximum timeout is 8388 ms.

Methods
-------

.. method:: WDT.feed()

    Feed the WDT to prevent it from resetting the system. The application
    should place this call in a sensible place ensuring that the WDT is
    only fed after verifying that everything is functioning correctly.















See :class:`WDT`. ::

    from machine import WDT

    # enable the WDT with a timeout of 5s (1s is the minimum)
    wdt = WDT(timeout=5000)
    wdt.feed()

ESP8266
-------

See :class:`WDT`. ::

    from machine import WDT

    # enable the WDT
    wdt = WDT()
    wdt.feed()


rp2
---


The RP2040 has a watchdog which is a countdown timer that can restart
parts of the chip if it reaches zero.

See :class:`WDT`. ::

    from machine import WDT

    # enable the WDT with a timeout of 5s (1s is the minimum)
    wdt = WDT(timeout=5000)
    wdt.feed()

The maximum value for timeout is 8388 ms.


wipy
----


See :class:`WDT`. ::

    from machine import WDT

    # enable the WDT with a timeout of 5s (1s is the minimum)
    wdt = WDT(timeout=5000)
    wdt.feed()
