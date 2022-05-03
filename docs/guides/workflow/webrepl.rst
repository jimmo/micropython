.. _guide_workflow_webrepl:

WebREPL
=======

WebREPL (REPL over WebSockets, accessible via a web browser) is an
experimental feature available in ESP32 port. Download web client
from https://github.com/micropython/webrepl (hosted version available
at http://micropython.org/webrepl), and configure it by executing::

    import webrepl_setup

and following on-screen instructions. After reboot, it will be available
for connection. If you disabled automatic start-up on boot, you may
run configured daemon on demand using::

    import webrepl
    webrepl.start()

    # or, start with a specific password
    webrepl.start(password='mypass')

The WebREPL daemon listens on all active interfaces, which can be STA or
AP.  This allows you to connect to the ESP32 via a router (the STA
interface) or directly when connected to its access point.

In addition to terminal/command prompt access, WebREPL also has provision
for file transfer (both upload and download).  The web client has buttons for
the corresponding functions, or you can use the command-line client
``webrepl_cli.py`` from the repository above.

See the `MicroPython forum <https://forum.micropython.org>`_ for other community-supported alternatives
to transfer files to an ESP32 board.



For your convenience, WebREPL client is hosted at
`<http://micropython.org/webrepl>`__. Alternatively, you can install it
locally from the the GitHub repository
`<https://github.com/micropython/webrepl>`__.

Before connecting to WebREPL, you should set a password and enable it via
a normal serial connection. Initial versions of MicroPython for ESP8266
came with WebREPL automatically enabled on the boot and with the
ability to set a password via WiFi on the first connection, but as WebREPL
was becoming more widely known and popular, the initial setup has switched
to a wired connection for improved security::

    import webrepl_setup

Follow the on-screen instructions and prompts. To make any changes active,
you will need to reboot your device.

To use WebREPL connect your computer to the ESP8266's access point
(MicroPython-xxxxxx, see the previous section about this).  If you have
already reconfigured your ESP8266 to connect to a router then you can
skip this part.

Once you are on the same network as the ESP8266 you click the "Connect" button
(if you are connecting via a router then you may need to change the IP address,
by default the IP address is correct when connected to the ESP8266's access
point).  If the connection succeeds then you should see a password prompt.

Once you type the password configured at the setup step above, press Enter once
more and you should get a prompt looking like ``>>>``.  You can now start
typing Python commands!
