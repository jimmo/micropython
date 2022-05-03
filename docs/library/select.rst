:mod:`select` -- wait for events on a set of streams
====================================================

.. module:: select
    :synopsis: wait for events on a set of streams

|see_cpython_module| :mod:`python:select`.

This module provides functions to efficiently wait for events on multiple
`streams <stream>` (it' "selects" streams which are ready for operations).

Two mechanisms are available, either using the :class:`Poll` class or
the :func:`select` function.

Functions
---------

.. function:: poll()

    Create an instance of the :class:`Poll` class.

.. function:: select(rlist, wlist, xlist[, timeout])

    Wait for activity on a set of objects.

    This function is provided by some MicroPython ports for compatibility
    and is not efficient. Usage of :class:`Poll` is recommended instead.


class :class:`Poll`
-------------------

.. class:: Poll()

    This can be constructed using the :func:`poll` factory method above.::

        >>> p = select.poll()
        >>> p.register(stream)

Methods
~~~~~~~

.. method:: poll.register(obj[, eventmask])

    Register `stream` *obj* for polling. *eventmask* is logical OR of:

    * ``select.POLLIN``  - data available for reading
    * ``select.POLLOUT`` - more data can be written

    Note that flags like ``select.POLLHUP`` and ``select.POLLERR`` are
    *not* valid as an input eventmask (these are unsolicited events which
    will be returned from `poll()` regardless of whether they are asked
    for). These semantics are per POSIX.

    *eventmask* defaults to ``select.POLLIN | select.POLLOUT``.

    It is OK to call this function multiple times for the same *obj*.
    Successive calls will update *obj*'s eventmask to the value of
    *eventmask* (i.e. will behave as `modify()`).

.. method:: poll.unregister(obj)

    Unregister *obj* from polling.

.. method:: poll.modify(obj, eventmask)

    Modify the *eventmask* for *obj*. If *obj* is not registered, `OSError`
    is raised with error of ENOENT.

.. method:: poll.poll(timeout=-1, /)

    Wait for at least one of the registered objects to become ready or have an
    exceptional condition, with optional timeout in milliseconds (if *timeout*
    arg is not specified or -1, there is no timeout).

    Returns list of (``obj``, ``event``, ...) tuples. There may be other elements in
    tuple, depending on a platform and version, so don't assume that its size is 2.
    The ``event`` element specifies which events happened with a stream and
    is a combination of ``select.POLL*`` constants described above. Note that
    flags ``select.POLLHUP`` and ``select.POLLERR`` can be returned at any time
    (even if were not asked for), and must be acted on accordingly (the
    corresponding stream unregistered from poll and likely closed), because
    otherwise all further invocations of `poll()` may return immediately with
    these flags set for this stream again.

    In case of timeout, an empty list is returned.

    .. admonition:: Difference to CPython
        :class: attention

        Tuples returned may contain more than 2 elements as described above.

.. method:: poll.ipoll(timeout=-1, flags=0, /)

    Like :meth:`poll.poll`, but instead returns an iterator which yields a
    :term:`callee-owned tuple`. This function provides an efficient,
    allocation-free way to poll on streams.

    If *flags* is 1, a one-shot behaviour for events is employed: streams for
    which events have happened will have their event masks automatically reset
    (equivalent to ``poll.modify(obj, 0)``), so new events for such a stream
    won't be processed until a new mask is set with `poll.modify()`. This
    behaviour is useful for asynchronous I/O schedulers.

    .. admonition:: Difference to CPython
        :class: attention

        This function is a MicroPython extension.
