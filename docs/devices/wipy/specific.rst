.. _devices_wipy_specific:

Port-specific functionality
===========================

Telnet and FTP server
---------------------

See :class:`network.Server` ::

    from network import Server

    # init with new user, password and seconds timeout
    server = Server(login=('user', 'password'), timeout=60)
    server.timeout(300) # change the timeout
    server.timeout() # get the timeout
    server.isrunning() # check whether the server is running or not

Heart beat LED
--------------

See :mod:`wipy`. ::

    import wipy

    wipy.heartbeat(False)  # disable the heartbeat LED
    wipy.heartbeat(True)   # enable the heartbeat LED
    wipy.heartbeat()       # get the heartbeat state

machine.Pin
-----------

On the WiPy board the pins are identified by their string id::

    from machine import Pin
    g = machine.Pin('GP9', mode=Pin.OUT, pull=None, drive=Pin.MED_POWER, alt=-1)

You can also configure the Pin to generate interrupts. For instance::

    from machine import Pin

    def pincb(pin):
        print(pin.id())

    pin_int = Pin('GP10', mode=Pin.IN, pull=Pin.PULL_DOWN)
    pin_int.irq(trigger=Pin.IRQ_RISING, handler=pincb)
    # the callback can be triggered manually
    pin_int.irq()()
    # to disable the callback
    pin_int.irq().disable()

Now every time a falling edge is seen on the gpio pin, the callback will be
executed. Caution: mechanical push buttons have "bounce" and pushing or
releasing a switch will often generate multiple edges.
See: https://www.eng.utah.edu/~cs5780/debouncing.pdf for a detailed
explanation, along with various techniques for debouncing.

All pin objects go through the pin mapper to come up with one of the
gpio pins.

For the ``drive`` parameter the strengths are:

  - ``Pin.LOW_POWER`` - 2mA drive capability.
  - ``Pin.MED_POWER`` - 4mA drive capability.
  - ``Pin.HIGH_POWER`` - 6mA drive capability.

For the ``alt`` parameter please refer to the pinout and alternate functions
table at <https://raw.githubusercontent.com/wipy/wipy/master/docs/PinOUT.png>`_
for the specific alternate functions that each pin supports.

For interrupts, the ``priority`` can take values in the range 1-7.  And the
``wake`` parameter has the following properties:

  - If ``wake_from=machine.Sleep.ACTIVE`` any pin can wake the board.
  - If ``wake_from=machine.Sleep.SUSPENDED`` pins ``GP2``, ``GP4``, ``GP10``,
    ``GP11``, ``GP17`` or ``GP24`` can wake the board. Note that only 1
    of this pins can be enabled as a wake source at the same time, so, only
    the last enabled pin as a ``machine.Sleep.SUSPENDED`` wake source will have effect.
  - If ``wake_from=machine.Sleep.SUSPENDED`` pins ``GP2``, ``GP4``, ``GP10``,
    ``GP11``, ``GP17`` and ``GP24`` can wake the board. In this case all of the
    6 pins can be enabled as a ``machine.Sleep.HIBERNATE`` wake source at the same time.

Additional Pin methods:

.. method:: machine.Pin.alt_list()

    Returns a list of the alternate functions supported by the pin. List items are
    a tuple of the form: ``('ALT_FUN_NAME', ALT_FUN_INDEX)``

machine.I2C
-----------

On the WiPy there is a single hardware I2C peripheral, identified by "0".  By
default this is the peripheral that is used when constructing an I2C instance.
The default pins are GP23 for SCL and GP13 for SDA, and one can create the
default I2C peripheral simply by doing::

    i2c = machine.I2C()

The pins and frequency can be specified as::

    i2c = machine.I2C(freq=400000, scl='GP23', sda='GP13')

Only certain pins can be used as SCL/SDA.  Please refer to the pinout for further
information.

Adhoc way to control telnet/FTP server via network module
---------------------------------------------------------

The ``Server`` class controls the behaviour and the configuration of the FTP and telnet
services running on the WiPy. Any changes performed using this class' methods will
affect both.

Example::

    import network
    server = network.Server()
    server.deinit() # disable the server
    # enable the server again with new settings
    server.init(login=('user', 'password'), timeout=600)

.. class:: network.Server(id, ...)

    Create a server instance, see ``init`` for parameters of initialization.

.. method:: server.init(*, login=('micro', 'python'), timeout=300)

    Init (and effectively start the server). Optionally a new ``user``, ``password``
    and ``timeout`` (in seconds) can be passed.

.. method:: server.deinit()

    Stop the server

.. method:: server.timeout([timeout_in_seconds])

    Get or set the server timeout.

.. method:: server.isrunning()

    Returns ``True`` if the server is running, ``False`` otherwise.

Adhoc VFS-like support
----------------------

WiPy doesn't implement full MicroPython VFS support, instead the following
functions are defined in the ``os`` module:

.. function:: mount(block_device, mount_point, *, readonly=False)

    Mounts a block device (like an ``SD`` object) in the specified mount
    point. Example::

        os.mount(sd, '/sd')

.. function:: unmount(path)

    Unmounts a previously mounted block device from the given path.

.. function:: mkfs(block_device or path)

    Formats the specified path, must be either ``/flash`` or ``/sd``.
    A block device can also be passed like an ``SD`` object before
    being mounted.

Blynk and the WiPy
------------------

Blynk provides iOS and Android apps to control any hardware over the Internet
or directly using Bluetooth. You can easily build graphic interfaces for all
your projects by simply dragging and dropping widgets, right on your smartphone.

Before anything else, make sure that your WiPy is running
the latest software, check :ref:`OTA How-To <wipy_firmware_upgrade>` for instructions.

1. Get the `Blynk library <https://github.com/vshymanskyy/blynk-library-python/blob/master/BlynkLib.py>`_ and put it in ``/flash/lib/`` via FTP.
2. Get the `Blynk example for WiPy <https://github.com/vshymanskyy/blynk-library-python/blob/master/examples/hardware/PyCom_WiPy.py>`_, edit the network settings, and afterwards
   upload it to ``/flash/`` via FTP as well.
3. Follow the instructions on each example to setup the Blynk dashboard on your smartphone or tablet.
4. Give it a try, for instance::

    >>> execfile('sync_virtual.py')
