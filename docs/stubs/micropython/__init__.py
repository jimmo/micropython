# SPDX-License-Identifier: MIT
# Copyright (c) 2021 The Pybricks Authors
#
# Documentation copied from:
# https://raw.githubusercontent.com/micropython/micropython/master/docs/library/micropython.rst
# Copyright (c) 2014-2021, Damien P. George, Paul Sokolovsky, and contributors

"""
Access and control MicroPython internals.
"""

from typing import Any, overload


def xfoo() -> int:
    """
    foo() -> int

    Returns:
        a foo
    """


@overload
def xconst(value: int) -> int:
    ...


@overload
def xconst(value: str) -> str:
    ...


@overload
def xconst(value: float) -> float:
    ...


@overload
def xconst(value: tuple) -> tuple:
    ...


def xconst(value):
    """
    const(value) -> Any

    Declares the value as a constant, which makes your code more efficient.

    To reduce memory usage further, prefix its name with an
    underscore (``_ORANGES``). This constant can only be used within the
    same file.

    If you want to import the value from another module, use a name without an
    underscore (``APPLES``). This uses a bit more memory.

    Arguments:
        value (int or float or str or tuple): The literal to be made constant.

    Returns:
        The constant value.
    """


@overload
def xopt_level() -> int:
    ...


@overload
def xopt_level(level: int) -> None:
    ...


def xopt_level(*args):
    """
    Sets the optimization level for code compiled on the hub:

    0. Assertion statements are enabled. The built-in ``__debug__`` variable
       is ``True``. Script line numbers are saved, so they can be reported when
       an Exception occurs.
    1. Assertions are ignored and ``__debug__`` is ``False``.
       Script line numbers are saved.
    2. Assertions are ignored and ``__debug__`` is ``False``.
       Script line numbers are saved.
    3. Assertions are ignored and ``__debug__`` is ``False``.
       Script line numbers are *not* saved.

    This applies only to code that you run in the REPL, because regular scripts
    are already compiled before they are sent to the hub.

    Arguments:
        level (int): The level to be set.

    Returns:
        If no argument is given, this returns the current optimization level.

    """


@overload
def xmem_info() -> None:
    ...


@overload
def xmem_info(verbose: Any) -> None:
    ...


def xmem_info(*args):
    """
    mem_info()
    mem_info(verbose)

    Prints information about stack and heap memory usage.

    Arguments:
        verbose: If any value is given, it also prints out the entire heap.
            This indicates which blocks are used and which are free.
    """


@overload
def xqstr_info() -> None:
    ...


@overload
def xqstr_info(verbose: Any) -> None:
    ...


def xqstr_info(*args):
    """
    qstr_info()
    qstr_info(verbose)

    Prints how many strings are interned and how much RAM they use.

    MicroPython uses string interning to save both RAM and ROM.
    This avoids having to store duplicate copies of the same string.

    Arguments:
        verbose: If any value is given, it also prints out the names of all
            RAM-interned strings.
    """


def xstack_use() -> int:
    """
    stack_use() -> int

    Checks the amount of stack that is being used. This can be used to
    compute differences in stack usage at different points in a script.

    Returns:
        The amount of stack in use.
    """


def xheap_lock() -> None:
    """
    heap_lock()

    Locks the heap. When locked, no memory allocation can occur. A
    ``MemoryError`` will be raised if any heap allocation is attempted.
    """


def xheap_unlock() -> int:
    """
    heap_unlock() -> int

    Unlocks the heap. Memory allocation is now allowed again.

    If :func:`heap_lock()` was called multiple times, :func:`heap_unlock()`
    must be called the same number of times to make the heap available again.

    Returns:
        The lock depth after unlocking. It is ``0`` once it is unlocked.
    """


def xkbd_intr(chr: int) -> None:
    """
    kbd_intr(chr)

    Sets the character that raises a ``KeyboardInterrupt`` exception when
    you type it in the input window. By default it is set to ``3``,
    corresponding to :kbd:`Ctrl-C`.

    Arguments:
        chr (int): Character that should raise the ``KeyboardInterrupt``.
            Choose ``-1`` to disable this feature.
    """
