.. currentmodule:: machine
.. _machine.Timer:

class Timer -- control hardware timers
======================================

Hardware timers deal with timing of periods and events. Timers are one of the
more flexible and variable hardware peripherals in MCUs and SoCs, differing
greatly across the range of devices that MicroPython runs on. MicroPython's
Timer class defines the minimum portable functionality as executing a callback
with a given period (or once after some delay), however most ports will provide
additional functionality.

See discussion of :ref:`important constraints <machine_callbacks>` on
Timer callbacks.

.. note::

    Memory can't be allocated inside irq handlers (an interrupt) and so
    exceptions raised within a handler don't give much information.  See
    :func:`micropython.alloc_emergency_exception_buf` for how to get around this
    limitation.

If you are using a WiPy board please refer to :ref:`machine.TimerWiPy <machine.TimerWiPy>`
instead of this class.

|availability_portable|

Constructors
------------

.. class:: Timer(id, ...)

   Construct a new timer object using a port-specific id.

   |machine_ids|

   |availability_cc3200_esp_nrf|

.. class:: Timer(-1, ...)

   Constructs a virtual timer.

   |availability_portable_wip|

Methods
-------

.. method:: Timer.init(\*, mode=Timer.PERIODIC, period=-1, tick_hz=1000, freq=None, callback=None)

   Initialise the timer. Example::

       tim.init(period=100, callback=my_handler)    # periodic with 100ms period
       tim.init(mode=Timer.ONE_SHOT, period=1000)   # one shot firing after 1000ms

   Either the ``period`` or the ``freq`` must be specified. If ``period`` is
   specified, then ``tick_hz`` may be overriden from the default of ``1000``.

   Keyword arguments:

     - ``mode`` can be one of:

       - ``Timer.ONE_SHOT`` - The timer runs once until the configured
         period of the channel expires.
       - ``Timer.PERIODIC`` - The timer runs periodically at the configured
         frequency of the channel (default).

     - ``period`` is the period in ticks (which will be one millisecond per
       tick by default).
       |availability_portable|

     - ``tick_hz`` is the in milliseconds.
       |availability_portable|

     - ``freq`` allows setting the frequency directly (overrides ``period``
       and ``tick_hz``).
       |availability_portable|

     - ``callback`` will be invoked when the timer fires. It must accept a
       single argument (the timer instance).
       |availability_portable|

   |availability_portable|

.. method:: Timer.deinit()

   Deinitialises the timer. Stops the timer, and disables the timer peripheral.

   |availability_portable|

Constants
---------

.. data:: Timer.ONE_SHOT
          Timer.PERIODIC

   Timer operating modes.

   |availability_portable|
