:mod:`sys` -- system specific functions
=======================================

.. module:: sys
    :synopsis: system specific functions

|see_cpython_module| :mod:`python:sys`.

This module provides mechansisms for programs to interact with the Python
runtime and virtual machine.

Functions
---------

.. function:: exit(retval=0, /)

    Terminate current program with a given exit code. This function raise as
    :exc:`SystemExit` exception. If an argument is given, its value given as an
    argument to `SystemExit`.

.. function:: atexit(func)

    Register *func* to be called upon termination.  *func* must be a callable
    that takes no arguments, or ``None`` to disable the call.  The :func:`atexit`
    function will return the previous value set by this function, which is
    initially ``None``.

    .. admonition:: Difference to CPython
        :class: attention

        This function is a MicroPython extension intended to provide similar
        functionality to the :mod:`python:atexit` module in CPython.

.. function:: print_exception(exc, file=sys.stdout, /)

    Print exception with a traceback to a file-like object *file* (or
    :data:`sys.stdout` by default).

    .. admonition:: Difference to CPython
        :class: attention

        This is simplified version of a function which appears in the
        :mod:`python:traceback` module in CPython. Unlike
        :func:`python:traceback.print_exception`, this function takes
        only the exception value instead of exception type,
        exception value, and traceback object. The *file* argument should be
        positional and further arguments are not supported.

        A CPython-compatible implementation of the :mod:`python:traceback`
        module can be found in `found in micropython-lib <https://github.com/micropython/micropython-lib/tree/master/python-stdlib/traceback>`_.

.. function:: settrace(tracefunc)

    Enable tracing of bytecode execution.  For details see the :func:`CPython
    documentation <python:sys.settrace>`.

    This function requires a custom MicroPython build as it is typically not
    present in pre-built firmware (due to it affecting performance).  The relevant
    configuration option is ``MICROPY_PY_SYS_SETTRACE``.

Constants
---------

.. data:: argv

    A mutable list of arguments the current program was started with.

    *Note:* This is typically only useful on the :ref:`devices_unix` port.

.. data:: byteorder

    The byte order (endianness) of the system (``"little"`` or ``"big"``).

.. data:: implementation

    Object with information about the current Python implementation. For
    MicroPython, it has following attributes:

    * *name* - string "micropython"
    * *version* - tuple (major, minor, micro), e.g. (1, 7, 0)
    * *_machine* - string describing the underlying machine
    * *_mpy* - supported mpy file-format version (optional attribute)

    This object is the recommended way to distinguish MicroPython from other
    Python implementations (note that it still may not exist in the very
    minimal ports).

    .. admonition:: Difference to CPython
        :class: attention

        CPython mandates more attributes for this object, but the bare minimum
        useful information is implemented in MicroPython.

.. data:: maxsize

    Maximum value which a native integer type can hold on the current platform,
    or maximum value representable by MicroPython integer type, if it's smaller
    than platform max value (that is the case for MicroPython ports without
    long int support).

    This attribute is useful for detecting "bitness" of a platform (32-bit vs
    64-bit, etc.). It's recommended to not compare this attribute to some
    value directly, but instead count number of bits in it::

        bits = 0
        v = sys.maxsize
        while v:
            bits += 1
            v >>= 1
        if bits > 32:
            # 64-bit (or more) platform
            ...
        else:
            # 32-bit (or less) platform
            # Note that on 32-bit platform, value of bits may be less than 32
            # (e.g. 31) due to peculiarities described above, so use "> 16",
            # "> 32", "> 64" style of comparisons.

.. data:: modules

    Dictionary of loaded modules. On some ports, it may not include builtin
    modules.

.. data:: path

    A mutable list of directories to search for imported modules.

    .. admonition:: Difference to CPython
        :class: attention

        On MicroPython, an entry with the value ``".frozen"`` will indicate that import
        should search :term:`frozen modules <frozen module>` at that point in the search.
        If no frozen module is found then search will *not* look for a directory called
        ``.frozen``, instead it will continue with the next entry in ``sys.path``.

        See the :ref:`guide to freezing code <guides_workflow_freezing>`.

.. data:: platform

    The platform that MicroPython is running on. For OS/RTOS ports, this is
    usually an identifier of the OS, e.g. ``"linux"``. For baremetal ports it
    is an identifier of a board, e.g. ``"pyboard"`` for the original MicroPython
    reference board. It thus can be used to distinguish one board from another.
    If you need to check whether your program runs on MicroPython (vs other
    Python implementation), use :data:`sys.implementation` instead.

.. data:: ps1
          ps2

    Mutable attributes holding strings, which are used for the REPL prompt.  The defaults
    give the standard Python prompt of ``>>>`` and ``...``.

.. data:: stderr

    Standard error `stream`.

.. data:: stdin

    Standard input `stream`.

.. data:: stdout

    Standard output `stream`.

.. data:: tracebacklimit

    A mutable attribute holding an integer value which is the maximum number of traceback
    entries to store in an exception.  Set to 0 to disable adding tracebacks.  Defaults
    to 1000.

    Note: this is not available on all ports.

.. data:: version

    Python language version that this implementation conforms to, as a string.

.. data:: version_info

    Python language version that this implementation conforms to, as a tuple of ints.

    .. admonition:: Difference to CPython
        :class: attention

        Only the first three version numbers (major, minor, micro) are supported and
        they can be referenced only by index, not by name.
