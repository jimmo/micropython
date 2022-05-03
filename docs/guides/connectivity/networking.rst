.. _guides_connectivity_networking:

Networking
==========

Once your device is connected to a network (e.g. via :ref:`WiFi <guides_connectivity_wifi>`
or :ref:`Ethernet <guides_connectivity_ethernet>`) you can use sockets to
communicate with other devices. A socket represents an endpoint on a network
device, and when two sockets are connected together communication can proceed.

Internet protocols are built on top of sockets, such as email (SMTP), the web
(HTTP), telnet, ssh, among many others.  Each of these protocols is assigned
a specific port, which is just an integer.  Given an IP address and a port
number you can connect to a remote device and start talking with it.

In many cases, it won't be necessary to use sockets directly and you can use a
higher-level library. For example, see the guide to using
:ref:`HTTP from MicroPython <guides_connectivity_http>`.

TCP sockets
~~~~~~~~~~~

The building block of most of the internet is the TCP socket.  These sockets
provide a reliable stream of bytes between the connected network devices.
This part of the tutorial will show how to use TCP sockets in a few different
cases.

Star Wars Asciimation
---------------------

The simplest thing to do is to download data from the internet.  In this case
we will use the Star Wars Asciimation service provided by the blinkenlights.nl
website.  It uses the telnet protocol on port 23 to stream data to anyone that
connects.  It's very simple to use because it doesn't require you to
authenticate (give a username or password), you can just start downloading data
straight away.

The first thing to do is make sure we have the socket module available::

    >>> import socket

Then get the IP address of the server::

    >>> addr_info = socket.getaddrinfo("towel.blinkenlights.nl", 23)

The ``getaddrinfo`` function actually returns a list of addresses, and each
address has more information than we need.  We want to get just the first valid
address, and then just the IP address and port of the server.  To do this use::

    >>> addr = addr_info[0][-1]

If you type ``addr_info`` and ``addr`` at the prompt you will see exactly what
information they hold.

Using the IP address we can make a socket and connect to the server::

    >>> s = socket.socket()
    >>> s.connect(addr)

Now that we are connected we can download and display the data::

    >>> while True:
    ...     data = s.recv(500)
    ...     print(str(data, 'utf8'), end='')
    ...

When this loop executes it should start showing the animation (use ctrl-C to
interrupt it).

You should also be able to run this same code on your PC using normal Python if
you want to try it out there.

HTTP GET request
----------------

The next example shows how to download a webpage.  HTTP uses port 80 and you
first need to send a "GET" request before you can download anything.  As part
of the request you need to specify the page to retrieve.

Let's define a function that can download and print a URL::

    def http_get(url):
        import socket
        _, _, host, path = url.split('/', 3)
        addr = socket.getaddrinfo(host, 80)[0][-1]
        s = socket.socket()
        s.connect(addr)
        s.send(bytes('GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n' % (path, host), 'utf8'))
        while True:
            data = s.recv(100)
            if data:
                print(str(data, 'utf8'), end='')
            else:
                break
        s.close()

Then you can try::

    >>> http_get('https://micropython.org/ks/test.html')

This should retrieve the webpage and print the HTML to the console.

Simple HTTP server
------------------

The following code creates an simple HTTP server which serves a single webpage
that contains a table with the state of all the GPIO pins::

    import machine
    # ESP8266, ESP32
    pins = [machine.Pin(i, machine.Pin.IN) for i in (0, 2, 4, 5, 12, 13, 14, 15)]
    # STM32
    pins = [machine.Pin(i, machine.Pin.IN) for i in (0, 2, 4, 5, 12, 13, 14, 15)]
    # pyboard
    pins = [machine.Pin(f'X{i+1}', machine.Pin.IN) for i in range(12)]

    html = """<!DOCTYPE html>
    <html>
        <head> <title>Pins</title> </head>
        <body> <h1>Pins</h1>
            <table border="1"> <tr><th>Pin</th><th>Value</th></tr> %s </table>
        </body>
    </html>
    """

    import socket
    addr = socket.getaddrinfo('0.0.0.0', 80)[0][-1]

    s = socket.socket()
    s.bind(addr)
    s.listen(1)

    print('listening on', addr)

    while True:
        cl, addr = s.accept()
        print('client connected from', addr)
        cl_file = cl.makefile('rwb', 0)
        while True:
            line = cl_file.readline()
            if not line or line == b'\r\n':
                break
        rows = ['<tr><td>%s</td><td>%d</td></tr>' % (str(p), p.value()) for p in pins]
        response = html % '\n'.join(rows)
        cl.send('HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n')
        cl.send(response)
        cl.close()
