.. currentmodule:: machine

class :class:`UART`
===================

UART implements the standard UART/USART duplex serial communications protocol.  At
the physical level it consists of 2 lines: RX and TX.  The unit of communication
is a character (not to be confused with a string character) which can be 8 or 9
bits wide.

UART objects can be created and initialised using::

    from machine import UART

    uart = UART(1, 9600)                         # init with given baudrate
    uart.init(9600, bits=8, parity=None, stop=1) # init with given parameters

Supported parameters differ on a board:

Pyboard: Bits can be 7, 8 or 9. Stop can be 1 or 2. With *parity=None*,
only 8 and 9 bits are supported.  With parity enabled, only 7 and 8 bits
are supported.

WiPy/CC3200: Bits can be 5, 6, 7, 8. Stop can be 1 or 2.

A UART object acts like a `stream` object and reading and writing is done
using the standard stream methods::

    uart.read(10)       # read 10 characters, returns a bytes object
    uart.read()         # read all available characters
    uart.readline()     # read a line
    uart.readinto(buf)  # read and store into the given buffer
    uart.write('abc')   # write the 3 characters

Constructors
------------

.. class:: UART(id, ...)

    Construct a UART object of the given id.

Methods
-------

.. method:: UART.init(baudrate=9600, bits=8, parity=None, stop=1, *, ...)

    Initialise the UART bus with the given parameters:

     - *baudrate* is the clock rate.
     - *bits* is the number of bits per character, 7, 8 or 9.
     - *parity* is the parity, ``None``, 0 (even) or 1 (odd).
     - *stop* is the number of stop bits, 1 or 2.

    Additional keyword-only parameters that may be supported by a port are:

     - *tx* specifies the TX pin to use.
     - *rx* specifies the RX pin to use.
     - *rts* specifies the RTS (output) pin to use for hardware receive flow control.
     - *cts* specifies the CTS (input) pin to use for hardware transmit flow control.
     - *txbuf* specifies the length in characters of the TX buffer.
     - *rxbuf* specifies the length in characters of the RX buffer.
     - *timeout* specifies the time to wait for the first character (in ms).
     - *timeout_char* specifies the time to wait between characters (in ms).
     - *invert* specifies which lines to invert.

         - ``0`` will not invert lines (idle state of both lines is logic high).
         - ``UART.INV_TX`` will invert TX line (idle state of TX line now logic low).
         - ``UART.INV_RX`` will invert RX line (idle state of RX line now logic low).
         - ``UART.INV_TX | UART.INV_RX`` will invert both lines (idle state at logic low).

     - *flow* specifies which hardware flow control signals to use. The value
       is a bitmask.

         - ``0`` will ignore hardware flow control signals.
         - ``UART.RTS`` will enable receive flow control by using the RTS output pin to
           signal if the receive FIFO has sufficient space to accept more data.
         - ``UART.CTS`` will enable transmit flow control by pausing transmission when the
           CTS input pin signals that the receiver is running low on buffer space.
         - ``UART.RTS | UART.CTS`` will enable both, for full hardware flow control.

    On the WiPy only the following keyword-only parameter is supported:

     - *pins* is a 4 or 2 item list indicating the TX, RX, RTS and CTS pins (in that order).
       Any of the pins can be None if one wants the UART to operate with limited functionality.
       If the RTS pin is given the the RX pin must be given as well. The same applies to CTS.
       When no pins are given, then the default set of TX and RX pins is taken, and hardware
       flow control will be disabled. If *pins* is ``None``, no pin assignment will be made.

   .. note::
     It is possible to call ``init()`` multiple times on the same object in
     order to reconfigure  UART on the fly. That allows using single UART
     peripheral to serve different devices attached to different GPIO pins.
     Only one device can be served at a time in that case.
     Also do not call ``deinit()`` as it will prevent calling ``init()``
     again.

.. method:: UART.deinit()

    Turn off the UART bus.

   .. note::
     You will not be able to call ``init()`` on the object after ``deinit()``.
     A new instance needs to be created in that case.

.. method:: UART.any()

    Returns an integer counting the number of characters that can be read without
    blocking.  It will return 0 if there are no characters available and a positive
    number if there are characters.  The method may return 1 even if there is more
    than one character available for reading.

    For more sophisticated querying of available characters use select.poll::

        poll = select.poll()
        poll.register(uart, select.POLLIN)
        poll.poll(timeout)

.. method:: UART.read([nbytes])

    Read characters.  If ``nbytes`` is specified then read at most that many bytes,
    otherwise read as much data as possible. It may return sooner if a timeout
    is reached. The timeout is configurable in the constructor.

    Return value: a bytes object containing the bytes read in.  Returns ``None``
    on timeout.

.. method:: UART.readinto(buf[, nbytes])

    Read bytes into the ``buf``.  If ``nbytes`` is specified then read at most
    that many bytes.  Otherwise, read at most ``len(buf)`` bytes. It may return sooner if a timeout
    is reached. The timeout is configurable in the constructor.

    Return value: number of bytes read and stored into ``buf`` or ``None`` on
    timeout.

.. method:: UART.readline()

    Read a line, ending in a newline character. It may return sooner if a timeout
    is reached. The timeout is configurable in the constructor.

    Return value: the line read or ``None`` on timeout.

.. method:: UART.write(buf)

    Write the buffer of bytes to the bus.

    Return value: number of bytes written or ``None`` on timeout.

.. method:: UART.sendbreak()

    Send a break condition on the bus. This drives the bus low for a duration
    longer than required for a normal transmission of a character.

.. method:: UART.irq(trigger, priority=1, handler=None, wake=machine.IDLE)

    Create a callback to be triggered when data is received on the UART.

        - *trigger* can only be :data:`UART.RX_ANY`
        - *priority* level of the interrupt. Can take values in the range 1-7.
          Higher values represent higher priorities.
        - *handler* an optional function to be called when new characters arrive.
        - *wake* can only be :data:`machine.IDLE`.

    .. note::

        The handler will be called whenever any of the following two conditions are met:

            - 8 new characters have been received.
            - At least 1 new character is waiting in the Rx buffer and the Rx line has been
              silent for the duration of 1 complete frame.

        This means that when the handler function is called there will be between 1 to 8
        characters waiting.

    Returns an irq object.

    Availability: WiPy.

.. method:: UART.flush()

   Waits until all data has been sent. In case of a timeout, an exception is raised. The timeout
   duration depends on the tx buffer size and the baud rate. Unless flow control is enabled, a timeout
   should not occur.

   .. note::

       For the rp2, esp8266 and nrf ports the call returns while the last byte is sent.
       If required, a one character wait time has to be added in the calling script.

   Availability: rp2, esp32, esp8266, mimxrt, cc3200, stm32, nrf ports, renesas-ra

.. method:: UART.txdone()

   Tells whether all data has been sent or no data transfer is happening. In this case,
   it returns ``True``. If a data transmission is ongoing it returns ``False``.

   .. note::

       For the rp2, esp8266 and nrf ports the call may return ``True`` even if the last byte
       of a transfer is still being sent. If required, a one character wait time has to be
       added in the calling script.

   Availability: rp2, esp32, esp8266, mimxrt, cc3200, stm32, nrf ports, renesas-ra

Constants
---------

.. data:: UART.RX_ANY

    IRQ trigger sources

    Availability: WiPy.



















See :class:`UART`.

ESP32
-----

    from machine import UART

    uart1 = UART(1, baudrate=9600, tx=33, rx=32)
    uart1.write('hello')  # write 5 bytes
    uart1.read(5)         # read up to 5 bytes

The ESP32 has three hardware UARTs: UART0, UART1 and UART2.
They each have default GPIO assigned to them, however depending on your
ESP32 variant and board, these pins may conflict with embedded flash,
onboard PSRAM or peripherals.

Any GPIO can be used for hardware UARTs using the GPIO matrix, except for
input-only pins 34-39 that can be used as ``rx``. To avoid conflicts simply
provide ``tx`` and ``rx`` pins when constructing. The default pins listed
below.

=====  =====  =====  =====
\      UART0  UART1  UART2
=====  =====  =====  =====
tx     1      10     17
rx     3      9      16
=====  =====  =====  =====

ESP8266
-------

See :class:`UART`. ::

    from machine import UART
    uart = UART(0, baudrate=9600)
    uart.write('hello')
    uart.read(5) # read up to 5 bytes

Two UARTs are available. UART0 is on Pins 1 (TX) and 3 (RX). UART0 is
bidirectional, and by default is used for the REPL. UART1 is on Pins 2
(TX) and 8 (RX) however Pin 8 is used to connect the flash chip, so
UART1 is TX only.

When UART0 is attached to the REPL, all incoming chars on UART(0) go
straight to stdin so uart.read() will always return None.  Use
sys.stdin.read() if it's needed to read characters from the UART(0)
while it's also used for the REPL (or detach, read, then reattach).
When detached the UART(0) can be used for other purposes.

If there are no objects in any of the dupterm slots when the REPL is
started (on hard or soft reset) then UART(0) is automatically attached.
Without this, the only way to recover a board without a REPL would be to
completely erase and reflash (which would install the default boot.py which
attaches the REPL).

To detach the REPL from UART0, use::

    import os
    os.dupterm(None, 1)

The REPL is attached by default. If you have detached it, to reattach
it use::

    import os, machine
    uart = machine.UART(0, 115200)
    os.dupterm(uart, 1)


imx
---


See :class:`UART`. ::

    from machine import UART

    uart1 = UART(1, baudrate=115200)
    uart1.write('hello')  # write 5 bytes
    uart1.read(5)         # read up to 5 bytes

The i.MXRT has up to eight hardware UARTs, but not every board exposes all
TX and RX pins for users. For the assignment of Pins to UART signals,
refer to the :ref:`UART pinout <devices_mimxrt_pins_uart>`.



stm32
-----


See :ref:`pyb.UART <pyb.UART>`. ::

    from pyb import UART

    uart = UART(1, 9600)
    uart.write('hello')
    uart.read(5) # read up to 5 bytes



renesas
-------


The RA MCU has some hardware UARTs called SCI (Serial Communication Interface).
UART id is available corresponding to the RA MCU's SCI number which are UART(0) as SCI0 and UART(1) as SCI1.

See :class:`UART`. ::

    from machine import UART

    uart1 = UART(1, 115200)
    uart1.write('hello')    # write 5 bytes
    uart1.read(5)           # read up to 5 bytes

rp2
---


There are two UARTs, UART0 and UART1. UART0 can be mapped to GPIO 0/1, 12/13
and 16/17, and UART1 to GPIO 4/5 and 8/9.


See :class:`UART`. ::

    from machine import UART, Pin
    uart1 = UART(1, baudrate=9600, tx=Pin(4), rx=Pin(5))
    uart1.write('hello')  # write 5 bytes
    uart1.read(5)         # read up to 5 bytes

.. note::

    REPL over UART is disabled by default. You can see the :ref:`overview
    <devices_rp2>` for details on how to enable REPL over UART.


wipy
----


See :class:`UART`. ::

    from machine import UART
    uart = UART(0, baudrate=9600)
    uart.write('hello')
    uart.read(5) # read up to 5 bytes

samd
----

See :ref:`machine.UART <machine.UART>`. ::

    # Use UART 3 on a ItsyBitsy M4 board
    from machine import UART

    uart3 = UART(3, tx=Pin(1), rx=Pin(0), baudrate=115200)
    uart3.write('hello')  # write 5 bytes
    uart3.read(5)         # read up to 5 bytes

The SAMD21/SAMD51 MCUs have up to eight hardware so called SERCOM devices, which can be used as UART,
SPI or I2C device, but not every MCU variant and board exposes all
TX and RX pins for users. For the assignment of Pins to devices and UART signals,
refer to the :ref:`SAMD pinout <samd_pinout>`.
