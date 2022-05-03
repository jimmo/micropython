.. _guides_connectivity_wifi:

WiFi
====

On supported devices, WiFi can be configued using the
:ref:`network.WLAN <network.WLAN>` class.

The :mod:`network` module::

    import network

    wlan = network.WLAN(network.STA_IF) # create station interface
    wlan.active(True)       # activate the interface
    wlan.scan()             # scan for access points
    wlan.isconnected()      # check if the station is connected to an AP
    wlan.connect('ssid', 'key') # connect to an AP
    wlan.config('mac')      # get the interface's MAC address
    wlan.ifconfig()         # get the interface's IP/netmask/gw/DNS addresses

    ap = network.WLAN(network.AP_IF) # create access-point interface
    ap.active(True)         # activate the interface
    ap.config(ssid='ESP-AP') # set the SSID of the access point
    ap.config(max_clients=10) # set how many clients can connect to the network

A useful function for connecting to your local WiFi network is::

    def do_connect():
        import network
        wlan = network.WLAN(network.STA_IF)
        wlan.active(True)
        if not wlan.isconnected():
            print('connecting to network...')
            wlan.connect('ssid', 'key')
            while not wlan.isconnected():
                pass
        print('network config:', wlan.ifconfig())

Once the network is established the :mod:`socket <socket>` module can be used
to create and use TCP/UDP sockets as usual, and the ``urequests`` module for
convenient HTTP requests.

TODO(ports)
After a call to ``wlan.connect()``, the device will by default retry to connect
**forever**, even when the authentication failed or no AP is in range.
``wlan.status()`` will return ``network.STAT_CONNECTING`` in this state until a
connection succeeds or the interface gets disabled.  This can be changed by
calling ``wlan.config(reconnects=n)``, where n are the number of desired reconnect
attempts (0 means it won't retry, -1 will restore the default behaviour of trying
to reconnect forever).


TODO(ports)
max_clients


wipy
----


See :ref:`network.WLAN <network.WLAN>` and :mod:`machine`. ::

    import machine
    from network import WLAN

    # configure the WLAN subsystem in station mode (the default is AP)
    wlan = WLAN(mode=WLAN.STA)
    # go for fixed IP settings
    wlan.ifconfig(config=('192.168.0.107', '255.255.255.0', '192.168.0.1', '8.8.8.8'))
    wlan.scan()     # scan for available networks
    wlan.connect(ssid='mynetwork', auth=(WLAN.WPA2, 'mynetworkkey'))
    while not wlan.isconnected():
        pass
    print(wlan.ifconfig())
    # enable wake on WLAN
    wlan.irq(trigger=WLAN.ANY_EVENT, wake=machine.SLEEP)
    # go to sleep
    machine.lightsleep()
    # now, connect to the FTP or the Telnet server and the WiPy will wake-up





esp8266
-------

The network module is used to configure the WiFi connection.  There are two WiFi
interfaces, one for the station (when the ESP8266 connects to a router) and one
for the access point (for other devices to connect to the ESP8266).  Create
instances of these objects using::

    >>> import network
    >>> sta_if = network.WLAN(network.STA_IF)
    >>> ap_if = network.WLAN(network.AP_IF)

You can check if the interfaces are active by::

    >>> sta_if.active()
    False
    >>> ap_if.active()
    True

You can also check the network settings of the interface by::

    >>> ap_if.ifconfig()
    ('192.168.4.1', '255.255.255.0', '192.168.4.1', '8.8.8.8')

The returned values are: IP address, netmask, gateway, DNS.

Configuration of the WiFi
-------------------------

Upon a fresh install the ESP8266 is configured in access point mode, so the
AP_IF interface is active and the STA_IF interface is inactive.  You can
configure the module to connect to your own network using the STA_IF interface.

First activate the station interface::

    >>> sta_if.active(True)

Then connect to your WiFi network::

    >>> sta_if.connect('<your SSID>', '<your key>')

To check if the connection is established use::

    >>> sta_if.isconnected()

Once established you can check the IP address::

    >>> sta_if.ifconfig()
    ('192.168.0.2', '255.255.255.0', '192.168.0.1', '8.8.8.8')

You can then disable the access-point interface if you no longer need it::

    >>> ap_if.active(False)

Here is a function you can run (or put in your boot.py file) to automatically
connect to your WiFi network::

    def do_connect():
        import network
        sta_if = network.WLAN(network.STA_IF)
        if not sta_if.isconnected():
            print('connecting to network...')
            sta_if.active(True)
            sta_if.connect('<ssid>', '<key>')
            while not sta_if.isconnected():
                pass
        print('network config:', sta_if.ifconfig())



wipy
----

The WLAN is a system feature of the WiPy, therefore it is always enabled
(even while in ``machine.SLEEP``), except when deepsleep mode is entered.

In order to retrieve the current WLAN instance, do::

    >>> from network import WLAN
    >>> wlan = WLAN() # we call the constructor without params

You can check the current mode (which is always ``WLAN.AP`` after power up)::

    >>> wlan.mode()

.. warning::
    When you change the WLAN mode following the instructions below, your WLAN
    connection to the WiPy will be broken. This means you will not be able
    to run these commands interactively over the WLAN.

    There are two ways around this::
     1. put this setup code into your :ref:`boot.py file<wipy_filesystem>` so that it gets executed automatically after reset.
     2. :ref:`duplicate the REPL on UART <wipy_uart>`, so that you can run commands via USB.

Connecting to your home router
------------------------------

The WLAN network card always boots in ``WLAN.AP`` mode, so we must first configure
it as a station::

    from network import WLAN
    wlan = WLAN(mode=WLAN.STA)


Now you can proceed to scan for networks::

    nets = wlan.scan()
    for net in nets:
        if net.ssid == 'mywifi':
            print('Network found!')
            wlan.connect(net.ssid, auth=(net.sec, 'mywifikey'), timeout=5000)
            while not wlan.isconnected():
                machine.idle() # save power while waiting
            print('WLAN connection succeeded!')
            break

Assigning a static IP address when booting
------------------------------------------

If you want your WiPy to connect to your home router after boot-up, and with a fixed
IP address so that you can access it via telnet or FTP, use the following script as /flash/boot.py::

    import machine
    from network import WLAN
    wlan = WLAN() # get current object, without changing the mode

    if machine.reset_cause() != machine.SOFT_RESET:
       wlan.init(WLAN.STA)
       # configuration below MUST match your home router settings!!
       wlan.ifconfig(config=('192.168.178.107', '255.255.255.0', '192.168.178.1', '8.8.8.8'))

    if not wlan.isconnected():
       # change the line below to match your network ssid, security and password
       wlan.connect('mywifi', auth=(WLAN.WPA2, 'mywifikey'), timeout=5000)
       while not wlan.isconnected():
           machine.idle() # save power while waiting

.. note::

    Notice how we check for the reset cause and the connection status, this is crucial in order
    to be able to soft reset the WiPy during a telnet session without breaking the connection.
