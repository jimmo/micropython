.. currentmodule:: machine

class :class:`RTC`
==================

The RTC is an independent clock that keeps track of the date
and time.

Example usage::

    rtc = machine.RTC()
    rtc.datetime((2020, 1, 21, 2, 10, 32, 36, 0))
    print(rtc.datetime())


Constructors
------------

.. class:: RTC(id=0, ...)

    Create an RTC object. See init for parameters of initialization.

Methods
-------

.. method:: RTC.datetime([datetimetuple])

    Get or set the date and time of the RTC.

    With no arguments, this method returns an 8-tuple with the current
    date and time.  With 1 argument (being an 8-tuple) it sets the date
    and time.

    The 8-tuple has the following format:

       (year, month, day, weekday, hours, minutes, seconds, subseconds)

    The meaning of the ``subseconds`` field is hardware dependent.

.. method:: RTC.init(datetime)

    Initialise the RTC. Datetime is a tuple of the form:

        ``(year, month, day[, hour[, minute[, second[, microsecond[, tzinfo]]]]])``

.. method:: RTC.now()

    Get get the current datetime tuple.

.. method:: RTC.deinit()

    Resets the RTC to the time of January 1, 2015 and starts running it again.

.. method:: RTC.alarm(id, time, *, repeat=False)

    Set the RTC alarm. Time might be either a millisecond value to program the alarm to
    current time + time_in_ms in the future, or a datetimetuple. If the time passed is in
    milliseconds, repeat can be set to ``True`` to make the alarm periodic.

.. method:: RTC.alarm_left(alarm_id=0)

    Get the number of milliseconds left before the alarm expires.

.. method:: RTC.cancel(alarm_id=0)

    Cancel a running alarm.

.. method:: RTC.irq(*, trigger, handler=None, wake=machine.IDLE)

    Create an irq object triggered by a real time clock alarm.

        - ``trigger`` must be ``RTC.ALARM0``
        - ``handler`` is the function to be called when the callback is triggered.
        - ``wake`` specifies the sleep mode from where this interrupt can wake
          up the system.

Constants
---------

.. data:: RTC.ALARM0

    irq trigger source














See :class:`RTC` ::

    from machine import RTC

    rtc = RTC()
    rtc.datetime((2017, 8, 23, 1, 12, 48, 0, 0)) # set a specific date and time
    rtc.datetime() # get date and time



Real time clock (RTC) (ESP8266)
-------------------------------

See :class:`RTC` ::

    from machine import RTC

    rtc = RTC()
    rtc.datetime((2017, 8, 23, 1, 12, 48, 0, 0)) # set a specific date and time
    rtc.datetime() # get date and time

    # synchronize with ntp
    # need to be connected to wifi
    import ntptime
    ntptime.settime() # set the rtc datetime from the remote server
    rtc.datetime()    # get the date and time in UTC

.. note:: Not all methods are implemented: `RTC.now()`, `RTC.irq(handler=*) <RTC.irq>`
          (using a custom handler), `RTC.init()` and `RTC.deinit()` are
          currently not supported.


imx
---


See :class:`RTC`::

    from machine import RTC

    rtc = RTC()
    rtc.datetime((2017, 8, 23, 1, 12, 48, 0, 0)) # set a specific date and time
    rtc.datetime() # get date and time
    rtc.now() # return date and time in CPython format.

The i.MXRT MCU supports battery backup of the RTC.  By connecting a battery of
1.5-3.6V, time and date are maintained in the absence of the main power.  The
current drawn from the battery is ~20µA, which is rather high.  A CR2032 coin
cell will last for about one year.


stm32
-----

See :ref:`pyb.RTC <pyb.RTC>` ::

    from pyb import RTC

    rtc = RTC()
    rtc.datetime((2017, 8, 23, 1, 12, 48, 0, 0)) # set a specific date and time
    rtc.datetime() # get date and time


renesas
-------

See :class:`RTC` ::

    from machine import RTC

    rtc = RTC()
    rtc.datetime((2017, 8, 23, 1, 12, 48, 0, 0)) # set a specific date and time
                                                 # time, eg 2017/8/23 1:12:48
    rtc.datetime() # get date and time


rp2
---

same


wipy
----


See :class:`RTC` ::

    from machine import RTC

    rtc = RTC() # init with default time and date
    rtc = RTC(datetime=(2015, 8, 29, 9, 0, 0, 0, None)) # init with a specific time and date
    print(rtc.now())

    def alarm_handler (rtc_o):
        pass
        # do some non blocking operations
        # warning printing on an irq via telnet is not
        # possible, only via UART

    # create a RTC alarm that expires after 5 seconds
    rtc.alarm(time=5000, repeat=False)

    # enable RTC interrupts
    rtc_i = rtc.irq(trigger=RTC.ALARM0, handler=alarm_handler, wake=machine.SLEEP)

    # go into suspended mode waiting for the RTC alarm to expire and wake us up
    machine.lightsleep()
