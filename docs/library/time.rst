:mod:`time` -- time related functions
=====================================

.. module:: time
    :synopsis: time related functions

|see_cpython_module| :mod:`python:time`.

This module provides functions for getting the current time and date, measuring
time intervals, and for delays.

Absolute/calendar time
----------------------

**Time Epoch**: The :term:`unix port` uses the standard epoch for POSIX systems
which is 1970-01-01 00:00:00 UTC. However, some embedded ports use 2000-01-01 00:00:00
UTC as their epoch. Epoch year may be determined with ``gmtime(0)[0]``.

**Maintaining actual calendar date/time**: This requires a
Real Time Clock (RTC). On systems with an underlying OS (including some
RTOS), an RTC may be implicit. Setting and maintaining actual calendar
time is responsibility of the OS/RTOS and is done outside of MicroPython,
it just uses the OS API to query date/time. On :term:`bare-metal` ports
however, the system time depends on the :class:`machine.RTC` object.
The current calendar time may be set using ``machine.RTC().datetime(tuple)``
function, and maintained by the following means:

* By a backup battery (which may be an additional, optional component for
  a particular board).
* Using networked time protocol (requires setup by a port/user).
* Set manually by a user on each power-up (many boards then maintain
  RTC time across hard resets, though some may require setting it again
  in such case).

If actual calendar time is not maintained with a system/MicroPython RTC,
the functions below which require reference to current absolute time may
behave not as expected.

.. function:: gmtime([secs])
              localtime([secs])

    Convert the time *secs* expressed in seconds since the Epoch (see above) into an
    8-tuple which contains: ``(year, month, mday, hour, minute, second, weekday, yearday)``
    If *secs* is not provided or None, then the current time from the RTC is used.

    The :func:`gmtime` function returns a date-time tuple in UTC,
    and :func:`localtime` returns a date-time tuple in local time.

    The format of the entries in the 8-tuple are:

    * year includes the century (for example 2014).
    * month   is 1-12
    * mday    is 1-31
    * hour    is 0-23
    * minute  is 0-59
    * second  is 0-59
    * weekday is 0-6 for Mon-Sun
    * yearday is 1-366

.. function:: mktime()

    This is the inverse of the :func:`localtime` function. It's argument is a
    full 8-tuple which expresses a time as per localtime. It returns an integer
    which is the number of seconds since Jan 1, 2000.

    TODO (is this the right epoch)?

.. function:: time()

    Returns the number of seconds, as an integer, since the Epoch, assuming that
    underlying RTC is set and maintained as described above. If an RTC is not set, this
    function returns number of seconds since a port-specific reference point in time (for
    embedded boards without a battery-backed RTC, usually since power up or reset). If you
    want to develop portable MicroPython application, you should not rely on this function
    to provide higher than second precision.  If you need higher precision, absolute
    timestamps, use :func:`time_ns`.  If relative times are acceptable then use the
    :func:`ticks_ms` and :func:`ticks_us` functions.  If you need calendar time,
    :func:`gmtime` or :func:`localtime` without an argument is a better choice.

    .. admonition:: Difference to CPython
        :class: attention

        In CPython, this function returns the number of seconds since the Unix
        epoch (1970-01-01 00:00 UTC), as a floating-point number, usually with
        microsecond precision. With MicroPython, only the :term:`unix port`
        uses the same epoch, and if floating-point precision allows, returns
        sub-second precision.

        Embedded hardware usually doesn't have floating-point precision to
        represent both long time ranges and subsecond precision, so they use an
        integer value with second precision. Some embedded hardware also lacks
        battery-powered RTC, so returns number of seconds since last power-up
        or from other relative, hardware-specific point(e.g. reset).

.. function:: time_ns()

    Similar to :func:`time` but returns nanoseconds since the Epoch, as an integer (usually
    a big integer, so this will allocate on the heap).

    .. admonition:: Difference to CPython
        :class: attention

        This is a MicroPython extension.


Sleeping and time measurement
-----------------------------

TODO: high level overview of ticks and wraparound.

.. function:: sleep(seconds)

    Sleep for the given number of seconds. Some boards may accept *seconds* as a
    floating-point number to sleep for a fractional number of seconds. Note that
    other boards may not accept a floating-point argument, for compatibility with
    them use the :func:`sleep_ms` and :func:`sleep_us` functions instead.

.. function:: sleep_ms(ms)

    Delay for given number of milliseconds, should be positive or 0.

    This function will delay for at least the given number of milliseconds, but
    may take longer than that if other processing must take place, for example
    interrupt handlers or other threads.  Passing in 0 for *ms* will still allow
    this other processing to occur.  Use :func:`sleep_us` for more precise delays.

    .. admonition:: Difference to CPython
        :class: attention

        This is a MicroPython extension.

.. function:: sleep_us(us)

    Delay for given number of microseconds, should be positive or 0.

    This function attempts to provide an accurate delay of at least *us*
    microseconds, but it may take longer if the system has other higher priority
    processing to perform.

    .. admonition:: Difference to CPython
        :class: attention

        This is a MicroPython extension.

.. function:: ticks_ms()

    Returns an increasing millisecond counter with an arbitrary reference point, that
    wraps around after some value.

    The wrap-around value is not explicitly exposed, but we will
    refer to it as *TICKS_MAX* to simplify discussion. Period of the values is
    *TICKS_PERIOD = TICKS_MAX + 1*. *TICKS_PERIOD* is guaranteed to be a power of
    two, but otherwise may differ from port to port. The same period value is used
    for all of `ticks_ms()`, `ticks_us()`, `ticks_cpu()` functions (for
    simplicity). Thus, these functions will return a value in range [*0* ..
    *TICKS_MAX*], inclusive, total *TICKS_PERIOD* values. Note that only
    non-negative values are used. For the most part, you should treat values returned
    by these functions as opaque. The only operations available for them are
    `ticks_diff()` and `ticks_add()` functions described below.

    Note: Performing standard mathematical operations (+, -) or relational
    operators (<, <=, >, >=) directly on these value will lead to invalid
    result. Performing mathematical operations and then passing their results
    as arguments to `ticks_diff()` or `ticks_add()` will also lead to
    invalid results from the latter functions.

    .. admonition:: Difference to CPython
        :class: attention

        This is a MicroPython extension.

.. function:: ticks_us()

    Just like :func:`ticks_ms` above, but in microseconds.

    .. admonition:: Difference to CPython
        :class: attention

        This is a MicroPython extension.

.. function:: ticks_cpu()

    Similar to :func:`ticks_ms` and :func:`ticks_us`, but with the highest
    possible resolution available to the system. This is usually CPU clock
    cycles, and that's why the function is named that way.

    However, it doesn't have to be a CPU clock, some other timing source
    available in a system (e.g. high-resolution timer) can be used instead. The
    exact timing unit(resolution) of this function is not specified
    on :mod:`time` module level, but documentation for a specific port may
    provide more specific information. This function is intended for very fine
    benchmarking or very tight real-time loops. Avoid using it in portable
    code.

    Availability: Not every port implements this function.

    .. admonition:: Difference to CPython
        :class: attention

        This is a MicroPython extension.

.. function:: ticks_add(ticks, delta)

    Given a *ticks* value, this function allows to calculate a ticks
    value *delta* ticks before (negative delta) or after (positive delta) it,
    following modular-arithmetic definition of tick values
    (see :func:`ticks_ms` above).

    The *ticks* parameter must be a direct result of a previous call to the
    :func:`ticks_ms`, :func:`ticks_us`, or :func:`ticks_cpu` functions, or from
    a previous call to :func:`ticks_add`. However, *delta* can be an arbitrary
    integer number or numeric expression.

    :func:`ticks_add` is useful for calculating deadlines for events/tasks.
     (Note: you must use :func:`ticks_diff` function to work with deadlines.)

    Examples::

        # Find out what ticks value there was 100ms ago
        print(ticks_add(time.ticks_ms(), -100))

        # Calculate deadline for operation and test for it
        deadline = ticks_add(time.ticks_ms(), 200)
        while ticks_diff(deadline, time.ticks_ms()) > 0:
            do_a_little_of_something()

        # Find out what TICKS_MAX is used by this port
        print(ticks_add(0, -1))

    .. admonition:: Difference to CPython
        :class: attention

        This is a MicroPython extension.

.. function:: ticks_diff(ticks1, ticks2)

    Measure ticks difference between values returned from :func:`ticks_ms`,
    :func:`ticks_us`, or :func:`ticks_cpu` functions, as a signed value which
    may wrap around.

    The argument order is the same as for subtraction
    operator, ``ticks_diff(ticks1, ticks2)`` has the same meaning as ``ticks1 - ticks2``.
    However, values returned by the :func:`ticks_ms`, etc. functions may wrap around, so
    directly using subtraction on them will produce incorrect result. That is why
    :func:`ticks_diff` is needed, it implements modular (or more specifically,
    ring) arithmetic to produce correct result even for wrap-around values
    (as long as they not too distant in-between, see below).

    The function returns **signed** values in the range [*-TICKS_PERIOD/2* .. *TICKS_PERIOD/2-1*]
    (that's a typical range definition for two's-complement signed binary integers).

    If the result is negative, it means that *ticks1* occurred earlier in time than
    *ticks2*. Otherwise, it means that *ticks1* occurred after *ticks2*. This holds
    **only** if *ticks1* and *ticks2* are apart from each other for no more than
    *TICKS_PERIOD/2-1* ticks. If that does not hold, incorrect result will be returned.
    Specifically, if two tick values are apart for *TICKS_PERIOD/2-1* ticks, that value
    will be returned by the function. However, if *TICKS_PERIOD/2* of real-time ticks
    has passed between them, the function will return *-TICKS_PERIOD/2* instead, i.e.
    result value will wrap around to the negative range of possible values.

    *An informal explanation of the need for these functions:* Suppose you are locked in
    a room with no means to monitor the passing of time except a standard 12-notch clock.
    Then if you look at dial-plate now, and don't look again for another 13 hours (e.g.,
    if you have a long sleep), then once you finally look again, it may seem to you that
    only 1 hour has passed. To avoid this mistake, just look at the clock regularly. Your
    application should do the same. This "long sleep" metaphor also maps directly to
    application behaviour: don't let your application run any single task for too long.
    Run tasks in steps, and do time-keeping in-between.

    `ticks_diff()` is designed to accommodate various usage patterns, among them:

    * Polling with timeout. In this case, the order of events is known, and you will deal
      only with positive return values from :func:`ticks_diff`::

        # Wait for GPIO pin to be asserted, but at most 500us
        start = time.ticks_us()
        while pin.value() == 0:
            if time.ticks_diff(time.ticks_us(), start) > 500:
                raise TimeoutError

    * Scheduling events. In this case, :func:`ticks_diff` result may be negative
      if an event is overdue::

        # This code snippet is not optimized
        now = time.ticks_ms()
        scheduled_time = task.scheduled_time()
        if ticks_diff(scheduled_time, now) > 0:
            print("Too early, let's nap")
            sleep_ms(ticks_diff(scheduled_time, now))
            task.run()
        elif ticks_diff(scheduled_time, now) == 0:
            print("Right at time!")
            task.run()
        elif ticks_diff(scheduled_time, now) < 0:
            print("Oops, running late, tell task to run faster!")
            task.run(run_faster=true)

    Note: Do not pass :func:`time` values to :func:`ticks_diff`, you should use
    normal mathematical operations on them. But note that :func:`time` may (and will)
    also overflow. This is known as the `Year 2038 problem <https://en.wikipedia.org/wiki/Year_2038_problem>`_.

    .. admonition:: Difference to CPython
        :class: attention

        This is a MicroPython extension.



SAMD Delay and timing
---------------------

Use the :mod:`time <time>` module::

    import time

    time.sleep(1)           # sleep for 1 second
    time.sleep_ms(500)      # sleep for 500 milliseconds
    time.sleep_us(10)       # sleep for 10 microseconds
    start = time.ticks_ms() # get millisecond counter
    delta = time.ticks_diff(time.ticks_ms(), start) # compute time difference


SAMD Clock and time
-------------------

Two groups of functions are provided for time information. All boards have the
datetime(), mktime() and time() functions. Boards with a 32kHz crystal also
provide an RTC() module. The epoch start date is 1.1.2000.

Use the :mod:`time <time>` module::

    import time

    date_time = time.localtime()        # Show the actual date/time information
    date_time = time.localtime(seconds) # decode the date/time form the seconds value
    seconds = time.mktime(date_time_tuple) # Convert seconds to a datetime tuple
    second = time.time()                # Return the actual system time.

The format of the date_time tuple follows the standard. The µs value of the date_time
tuple is ignored. On boards without the RTC module, time.localtime(seconds) sets the
system time. Use of the Use the :mod:`RTC <RTC>` module::

    from machine import RTC
    rtc = RTC()
    date_time = rtc.datetime()      # return the actual date & time.
    rtc.datetime(date_time_tuple)   # Set date & time, ignoring weekday
    date_time = rtc.now()           # Return date & time in Unix order.
    rtc.calibration(value)          # Set a calibration factor

The weekday value set will be ignored and calculated in the returned tuple from the
actual date. rtc.now() is only provided at SAMD51 boards. The value used in the
rtc.calibration() call has a range from -127 - 127. It defines roughly a
ppm quantity, by which the clock can run faster or slower.
