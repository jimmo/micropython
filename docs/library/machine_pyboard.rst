.. currentmodule:: machine
.. _machine_pyboard:

pyboard
=======

The various pyboard models are based on the :ref:`STM32 port <machine_stm32>`, so
this extends on the information provided there.

Pyboard pin layout
------------------

The pyboard is designed so that pins are grouped into two "halves" or
"positions", named "X" and "Y".

Each of these positions provides 12 pins, one SPI bus, one I2C bus, and at least
two UARTs.

Pin
---

Pins IDs can be specified using either their "board" name (e.g. ``X1``) or their
"cpu" name (e.g. "PA0").

In addition to passing the pin ID as a string to the `Pin` constructor, they can
be accessed as ``machine.Pin.board.NAME`` and ``machine.Pin.cpu.NAME``.

For example, the following four examples are all identical::

    from machine import Pin

    p = Pin('X2', mode=Pin.OUT)

    p = Pin('PA1', mode=Pin.OUT)

    p = machine.Pin.board.X2
    p.init(mode=Pin.OUT)  # or p.mode(Pin.OUT)

    p = machine.Pin.cpu.PA1
    p.init(mode=Pin.OUT)  # or p.mode(Pin.OUT)


I2C
---

The I2C peripheral is typically specified by the position name. i.e. ``'X'`` or
``'Y'``::

   from machine import I2C
   i2c = I2C('X')
   print(i2c.scan())

On all pyboards, a given named I2C peripheral (e.g. ``'X'``) will always use
pins ``SCL='X9'`` and ``SCK='X10'``. Similarly, ``'Y'`` will be on ``'Y9'`` and
``'Y10'``.

You can also use the STM32 peripheral ID numbers (e.g. ``1`` and ``2`` for
``'X'`` and ``'Y'`` respectively).


SPI
---

Similar to I2C, the SPI peripheral can be specified using the position name.

On all pyboards, a given named SPI peripheral (e.g. ``'X'``) will always use
pins ``MOSI='X8'``, ``MISO='X7'``, ``SCK='X6'`` and ``nSS='X5'``. Similarly,
``'Y'`` will be on ``'Y8'``, ``'Y7'``, ``'Y6'`` and ``'Y5'``.


UART
----

The available UARTs vary across different devices, however the ID is the
corresponding STM32 UART ID.

The pyboard diagrams show the UARTs and which pins they are associated with.


Pin alternate functions
-----------------------

Although it is not possible to use arbitrary pins for the :class:`I2C`,
:class:`SPI` and :class:`UART` peripherals, it is possible to use the STM32
"Alternate Function" feature to select from a small number of alternate pins.

After constructing the instance of the peripheral, you can initialise the
additional pins using the `Pin.ALT` mode::

   from machine import I2C
   # Start of on default assignment of X9/X10
   i2c = I2C('X')
   # Remap SCL to ...
   # TODO example

The alternate function mapping is MCU model-specific.
