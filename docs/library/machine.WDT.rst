.. currentmodule:: machine
.. _machine.WDT:

class WDT -- watchdog timer
===========================

The WDT is used to restart the system when the application crashes and ends
up into a non recoverable state. Once started it cannot be stopped or
reconfigured in any way. After enabling, the application must "feed" the
watchdog periodically to prevent it from expiring and resetting the system.

Example usage::

    from machine import WDT
    wdt = WDT(timeout=2000)  # enable it with a timeout of 2s
    wdt.feed()

On most ports, code can detect a reset due to the WDT by inspecting
`reset_cause` on startup.

|availability_portable|

Note for ESP32/ESP8266: The WDT is per-thread, so you must feed the WDT from the
same thread that created it. There is currently no way to feed a WDT from a
different thread.

Constructors
------------

.. class:: WDT(id=0, timeout=5000)

   Create a WDT object and start it. The timeout must be given in seconds and
   the minimum value that is accepted is 1 second. Once it is running the timeout
   cannot be changed and the WDT cannot be stopped either.

   |machine_ids|

Methods
-------

.. method:: wdt.feed()

   Feed the WDT to prevent it from resetting the system. The application
   should place this call in a sensible place ensuring that the WDT is
   only fed after verifying that everything is functioning correctly.

   |availability_portable|
