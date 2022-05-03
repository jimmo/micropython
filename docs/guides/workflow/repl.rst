.. _guides_workflow_repl:

REPL
====

REPL stands for Read Evaluate Print Loop, and is the name given to the
interactive MicroPython prompt that you can access on the device.  Using the
REPL is by far the easiest way to test out your code and run commands.

Accessing the REPL
------------------

There are three ways to access the REPL:

* Serial port (or UART). This is typically via a USB-to-UART bridge, which may
  be included on your board.
* Directly via USB, on chips that have a built-in USB peripheral.
* Network (via WebREPL)

REPL over the serial port
~~~~~~~~~~~~~~~~~~~~~~~~~

The REPL is always available on the UART0 serial peripheral, which is connected
to the pins GPIO1 for TX and GPIO3 for RX.  The baudrate of the REPL is 115200.
If your board has a USB-serial convertor on it then you should be able to access
the REPL directly from your PC.  Otherwise you will need to have a way of
communicating with the UART.

.. tabs::
    .. group-tab:: pyboard / STM32

        Tab 1 content

    .. group-tab:: ESP32

        Once you have the firmware on the device you can access the REPL (Python prompt)
        over UART0 (GPIO1=TX, GPIO3=RX), which might be connected to a USB-serial
        convertor, depending on your board.  The baudrate is 115200.

        The MicroPython REPL is on UART0 (GPIO1=TX, GPIO3=RX) at baudrate 115200.

    .. group-tab:: ESP8266

        The MicroPython REPL is on UART0 (GPIO1=TX, GPIO3=RX) at baudrate 115200.


    .. group-tab:: Zephyr

        Tab 2 content

To access the prompt over USB-serial you need to use a terminal emulator
program.

On Windows TeraTerm is a good choice, on Mac you can use the built-in
``screen`` program, and Linux has ``screen``, ``picocom`` and ``minicom``.  Of
course, there are many other terminal programs that will work, so pick your
favourite!

For example, on Linux you can try running::

    picocom /dev/ttyUSB0 -b115200

Once you have made the connection over the serial port you can test if it is
working by hitting enter a few times.  You should see the Python REPL prompt,
indicated by ``>>>``.

WebREPL
~~~~~~~

WebREPL allows you to use the Python prompt over WiFi or Ethernet, connecting
through a browser. The latest versions of Firefox and Chrome are supported. See
the ref:``WebREPL guide <guides_workflow_webrepl>`` for more information.

Using the REPL
--------------

Once connected, if a program is currently executing, you can press Ctrl-C to
interrupt it and display the prompt (``>>>``).

Once you have a prompt you can start experimenting!  Anything you type at the
prompt will be executed after you press the Enter key.  MicroPython will run
the code that you enter and print the result (if there is one).  If there is an
error with the text that you enter then an error message is printed.

Try typing the following at the prompt::

    >>> print('hello micropython!')
    hello micropython!

Note that you shouldn't type the ``>>>`` arrows, they are there to indicate that
you should type the text after it at the prompt.  And then the line following is
what the device should respond with.  In the end, once you have entered the text
``print("hello micropython!")`` and pressed the Enter key, the output on your screen
should look exactly like it does above.

If you already know some python you can now try some basic commands here.   For
example::

    >>> 1 + 2
    3
    >>> 1 / 2
    0.5
    >>> 12**34
    4922235242952026704037113243122008064

Line editing
~~~~~~~~~~~~

You can edit the current line that you are entering using the left and right
arrow keys to move the cursor, as well as the delete and backspace keys.  Also,
pressing Home or Ctrl-A moves the cursor to the start of the line, and pressing
End or Ctrl-E moves to the end of the line.

Input history
~~~~~~~~~~~~~

The REPL remembers a certain number of previous lines of text that you entered
(typically 8 lines on most ports).  To recall previous lines use the up and
down arrow keys.

Tab completion
~~~~~~~~~~~~~~

Pressing the Tab key will do an auto-completion of the current word that you are
entering.  This can be very useful to find out functions and methods that a
module or object has.  Try it out by typing "pr" and then pressing Tab.  It
should complete to "print".  Or if you have imported a module (e.g.
``import machine``), then you can type "ma" then Tab, then type "." and press
Tab again to see a list of all the functions that the machine module has.

Line continuation and auto-indent
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Certain things that you type will need "continuing", that is, will need more
lines of text to make a proper Python statement.  In this case the prompt will
change to ``...`` and the cursor will auto-indent the correct amount so you can
start typing the next line straight away.  Try this by defining the following
function::

    >>> def say_hi(n):
    ...    print("hello" + n)
    ...
    ...
    ...
    >>>

In the above, you needed to press the Enter key three times in a row to finish
the compound statement (that's the three lines with just dots on them).  The
other way to finish a compound statement is to press backspace to get to the
start of the line, then press the Enter key.  (If you did something wrong and
want to escape the continuation mode then press Ctrl-C; all lines will be
ignored.)

You can now call your function using::

    >>> say_hi("micropython")

Let's now call the function in a loop::

    >>> import time
    >>> while True:
    ...     say_hi("micropython")
    ...     time.sleep_ms(500)
    ...
    ...
    ...
    >>>

To stop the loop press Ctrl-C, which will raise a KeyboardInterrupt exception and
break out of the loop.

The time module provides some useful functions for making delays and doing
timing.  Use tab completion to find out what they are and play around with them!

Paste mode
~~~~~~~~~~

Pressing Ctrl-E will enter a special paste mode.  This allows you to copy and
paste a chunk of text into the REPL.  If you press Ctrl-E you will see the
paste-mode prompt::

    paste mode; Ctrl-C to cancel, Ctrl-D to finish
    ===

You can then paste (or type) your text in.  Note that none of the special keys
or commands work in paste mode (eg Tab or backspace), they are just accepted
as-is.  Press Ctrl-D to finish entering the text and execute it.

Other control commands
~~~~~~~~~~~~~~~~~~~~~~

There are four other control commands:

* Ctrl-A on a blank line will enter raw REPL mode.  This is like a permanent
  paste mode, except that characters are not echoed back.

* Ctrl-B on a blank like goes to normal REPL mode.

* Ctrl-C cancels any input, or interrupts the currently running code.

* Ctrl-D on a blank line will do a soft reset.  This will run main.py again.

Note that Ctrl-A and Ctrl-D do not work with WebREPL.







Zephyr
------

REPL stands for Read Evaluate Print Loop, and is the name given to the
interactive MicroPython prompt that you can access on your board through
Zephyr. It is recommended to use REPL to test out your code and run commands.

REPL over the serial port
-------------------------

The REPL is available on a UART serial peripheral specified for the board by
the ``zephyr,console`` devicetree node. The baudrate of the REPL is 115200.
If your board has a USB-serial convertor on it then you should be able to access
the REPL directly from your PC.

To access the prompt over USB-serial you will need to use a terminal emulator
program. For a Linux or Mac machine, open a terminal and run::

        screen /dev/ttyACM0 115200

You can also try ``picocom`` or ``minicom`` instead of screen. You may have to use
``/dev/ttyACM1`` or a higher number for ``ttyACM``. Additional permissions
may be necessary to access this device (eg group ``uucp`` or ``dialout``, or use sudo).
For Windows, get a terminal software, such as puTTY and connect via a serial session
using the proper COM port.

Using the REPL
--------------

With your serial program open (PuTTY, screen, picocom, etc) you may see a
blank screen with a flashing cursor. Press Enter (or reset the board) and
you should be presented with the following text::

        *** Booting Zephyr OS build zephyr-v3.1.0  ***
        MicroPython v1.19.1-9-g4fd54a475 on 2022-06-17; zephyr-frdm_k64f with mk64f12
        Type "help()" for more information.
        >>>

Now you can try running MicroPython code directly on your board.

Anything you type at the prompt, indicated by ``>>>``, will be executed after you press
the Enter key. If there is an error with the text that you enter then an error
message is printed.

Start by typing the following at the prompt to make sure it is working::

        >>> print("hello world!")
        hello world!

If you already know some python you can now try some basic commands here. For
example::

        >>> 1 + 2
        3
        >>> 1 / 2
        0.5
        >>> 3 * 'Zephyr'
        ZephyrZephyrZephyr

If your board has an LED, you can blink it using the following code::

        >>>import time
        >>>from machine import Pin

        >>>LED = Pin(("GPIO_1", 21), Pin.OUT)
        >>>while True:
        ...    LED.value(1)
        ...    time.sleep(0.5)
        ...    LED.value(0)
        ...    time.sleep(0.5)

The above code uses an LED location for a FRDM-K64F board (port B, pin 21;
following Zephyr conventions ports are identified by "GPIO_x", where *x*
starts from 0). You will need to adjust it for another board using the board's
reference materials.






Original reference
------------------

The MicroPython Interactive Interpreter Mode (aka REPL)
=======================================================

This section covers some characteristics of the MicroPython Interactive
Interpreter Mode. A commonly used term for this is REPL (read-eval-print-loop)
which will be used to refer to this interactive prompt.

Auto-indent
-----------

When typing python statements which end in a colon (for example if, for, while)
then the prompt will change to three dots (...) and the cursor will be indented
by 4 spaces. When you press return, the next line will continue at the same
level of indentation for regular statements or an additional level of indentation
where appropriate. If you press the backspace key then it will undo one
level of indentation.

If your cursor is all the way back at the beginning, pressing RETURN will then
execute the code that you've entered. The following shows what you'd see
after entering a for statement (the underscore shows where the cursor winds up):

    >>> for i in range(30):
    ...     _

If you then enter an if statement, an additional level of indentation will be
provided:

    >>> for i in range(30):
    ...     if i > 3:
    ...         _

Now enter ``break`` followed by RETURN and press BACKSPACE:

    >>> for i in range(30):
    ...     if i > 3:
    ...         break
    ...     _

Finally type ``print(i)``, press RETURN, press BACKSPACE and press RETURN again:

    >>> for i in range(30):
    ...     if i > 3:
    ...         break
    ...     print(i)
    ...
    0
    1
    2
    3
    >>>

Auto-indent won't be applied if the previous two lines were all spaces.  This
means that you can finish entering a compound statement by pressing RETURN
twice, and then a third press will finish and execute.

Auto-completion
---------------

While typing a command at the REPL, if the line typed so far corresponds to
the beginning of the name of something, then pressing TAB will show
possible things that could be entered. For example, first import the machine
module by entering ``import machine`` and pressing RETURN.
Then type ``m`` and press TAB and it should expand to ``machine``.
Enter a dot ``.`` and press TAB again. You should see something like:

    >>> machine.
    __name__        info            unique_id       reset
    bootloader      freq            rng             idle
    sleep           deepsleep       disable_irq     enable_irq
    Pin

The word will be expanded as much as possible until multiple possibilities exist.
For example, type ``machine.Pin.AF3`` and press TAB and it will expand to
``machine.Pin.AF3_TIM``. Pressing TAB a second time will show the possible
expansions:

    >>> machine.Pin.AF3_TIM
    AF3_TIM10       AF3_TIM11       AF3_TIM8        AF3_TIM9
    >>> machine.Pin.AF3_TIM

Interrupting a running program
------------------------------

You can interrupt a running program by pressing Ctrl-C. This will raise a KeyboardInterrupt
which will bring you back to the REPL, providing your program doesn't intercept the
KeyboardInterrupt exception.

For example:

    >>> for i in range(1000000):
    ...     print(i)
    ...
    0
    1
    2
    3
    ...
    6466
    6467
    6468
    Traceback (most recent call last):
        File "<stdin>", line 2, in <module>
    KeyboardInterrupt:
    >>>

Paste mode
----------

If you want to paste some code into your terminal window, the auto-indent feature
will mess things up. For example, if you had the following python code: ::

    def foo():
       print('This is a test to show paste mode')
       print('Here is a second line')
    foo()

and you try to paste this into the normal REPL, then you will see something like
this:

    >>> def foo():
    ...         print('This is a test to show paste mode')
    ...             print('Here is a second line')
    ...             foo()
    ...
    Traceback (most recent call last):
        File "<stdin>", line 3
    IndentationError: unexpected indent

If you press Ctrl-E, then you will enter paste mode, which essentially turns off
the auto-indent feature, and changes the prompt from ``>>>`` to ``===``. For example:

    >>>
    paste mode; Ctrl-C to cancel, Ctrl-D to finish
    === def foo():
    ===     print('This is a test to show paste mode')
    ===     print('Here is a second line')
    === foo()
    ===
    This is a test to show paste mode
    Here is a second line
    >>>

Paste Mode allows blank lines to be pasted. The pasted text is compiled as if
it were a file. Pressing Ctrl-D exits paste mode and initiates the compilation.

Soft reset
----------

A soft reset will reset the python interpreter, but tries not to reset the
method by which you're connected to the MicroPython board (USB-serial, or Wifi).

You can perform a soft reset from the REPL by pressing Ctrl-D, or from your python
code by executing: ::

    machine.soft_reset()

For example, if you reset your MicroPython board, and you execute a dir()
command, you'd see something like this:

    >>> dir()
    ['__name__', 'pyb']

Now create some variables and repeat the dir() command:

    >>> i = 1
    >>> j = 23
    >>> x = 'abc'
    >>> dir()
    ['j', 'x', '__name__', 'pyb', 'i']
    >>>

Now if you enter Ctrl-D, and repeat the dir() command, you'll see that your
variables no longer exist:

.. code-block:: python

    MPY: sync filesystems
    MPY: soft reboot
    MicroPython v1.5-51-g6f70283-dirty on 2015-10-30; PYBv1.0 with STM32F405RG
    Type "help()" for more information.
    >>> dir()
    ['__name__', 'pyb']
    >>>

The special variable _ (underscore)
-----------------------------------

When you use the REPL, you may perform computations and see the results.
MicroPython stores the results of the previous statement in the variable _ (underscore).
So you can use the underscore to save the result in a variable. For example:

    >>> 1 + 2 + 3 + 4 + 5
    15
    >>> x = _
    >>> x
    15
    >>>

Raw mode and raw-paste mode
---------------------------

Raw mode (also called raw REPL) is not something that a person would normally use.
It is intended for programmatic use and essentially behaves like paste mode with
echo turned off, and with optional flow control.

Raw mode is entered using Ctrl-A. You then send your python code, followed by
a Ctrl-D. The Ctrl-D will be acknowledged by 'OK' and then the python code will
be compiled and executed. Any output (or errors) will be sent back. Entering
Ctrl-B will leave raw mode and return the the regular (aka friendly) REPL.

Raw-paste mode is an additional mode within the raw REPL that includes flow control,
and which compiles code as it receives it. This makes it more robust for high-speed
transfer of code into the device, and it also uses less RAM when receiving because
it does not need to store a verbatim copy of the code before compiling (unlike
standard raw mode).

Raw-paste mode uses the following protocol:

#. Enter raw REPL as usual via ctrl-A.

#. Write 3 bytes: ``b"\x05A\x01"`` (ie ctrl-E then "A" then ctrl-A).

#. Read 2 bytes to determine if the device entered raw-paste mode:

    * If the result is ``b"R\x00"`` then the device understands the command but
      doesn't support raw paste.

    * If the result is ``b"R\x01"`` then the device does support raw paste and
      has entered this mode.

    * Otherwise the result should be ``b"ra"`` and the device doesn't support raw
      paste and the string ``b"w REPL; CTRL-B to exit\r\n>"`` should be read and
      discarded.

#. If the device is in raw-paste mode then continue, otherwise fallback to
   standard raw mode.

#. Read 2 bytes, this is the flow control window-size-increment (in bytes)
   stored as a 16-bit unsigned little endian integer.  The initial value for the
   remaining-window-size variable should be set to this number.

#. Write out the code to the device:

    * While there are bytes to send, write up to the remaining-window-size worth
      of bytes, and decrease the remaining-window-size by the number of bytes
      written.

    * If the remaining-window-size is 0, or there is a byte waiting to read, read
      1 byte.  If this byte is ``b"\x01"`` then increase the remaining-window-size
      by the window-size-increment from step 5.  If this byte is ``b"\x04"`` then
      the device wants to end the data reception, and ``b"\x04"`` should be
      written to the device and no more code sent after that.  (Note: if there is
      a byte waiting to be read from the device then it does not need to be read
      and acted upon immediately, the device will continue to consume incoming
      bytes as long as reamining-window-size is greater than 0.)

#. When all code has been written to the device, write ``b"\x04"`` to indicate
   end-of-data.

#. Read from the device until ``b"\x04"`` is received.  At this point the device
   has received and compiled all of the code that was sent and is executing it.

#. The device outputs any characters produced by the executing code.  When (if)
   the code finishes ``b"\x04"`` will be output, followed by any exception that
   was uncaught, followed again by ``b"\x04"``.  It then goes back to the
   standard raw REPL and outputs ``b">"``.

For example, starting at a new line at the normal (friendly) REPL, if you write::

    b"\x01\x05A\x01print(123)\x04"

Then the device will respond with something like::

    b"\r\nraw REPL; CTRL-B to exit\r\n>R\x01\x80\x00\x01\x04123\r\n\x04\x04>"

Broken down over time this looks like::

    # Step 1: enter raw REPL
    write: b"\x01"
    read: b"\r\nraw REPL; CTRL-B to exit\r\n>"

    # Step 2-5: enter raw-paste mode
    write: b"\x05A\x01"
    read: b"R\x01\x80\x00\x01"

    # Step 6-8: write out code
    write: b"print(123)\x04"
    read: b"\x04"

    # Step 9: code executes and result is read
    read: b"123\r\n\x04\x04>"

In this case the flow control window-size-increment is 128 and there are two
windows worth of data immediately available at the start, one from the initial
window-size-increment value and one from the explicit ``b"\x01"`` value that
is sent.  So this means up to 256 bytes can be written to begin with before
waiting or checking for more incoming flow-control characters.

The ``tools/pyboard.py`` program uses the raw REPL, including raw-paste mode, to
execute Python code on a MicroPython-enabled board.




SAMD
----

The MicroPython REPL is on the USB port, configured in VCP mode.
Tab-completion is useful to find out what methods an object has.
Paste mode (Ctrl-E) is useful to paste a large slab of Python code into
the REPL.

The :mod:`machine` module::

    import machine

    machine.freq()            # get the current frequency of the CPU
    machine.freq(96_000_000)  # set the CPU frequency to 96 MHz

The range accepted by the function call is 1_000_000 to 200_000_000 (1 MHz to 200 MHz)
for SAMD51 and 1_000_000 to 48_000_000 (1 MHz to 48 MHz) for SAMD21. The safe
range for SAMD51 according to the data sheet is 96 MHz to 120 MHz.
At frequencies below 8 MHz USB will be disabled. Changing the frequency below 48 MHz
impacts the baud rates of UART, I2C and SPI. These have to be set again after
changing the CPU frequency. The ms and µs timers are not affected by the frequency
change.
