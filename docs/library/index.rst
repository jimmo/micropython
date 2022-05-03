.. _micropython_lib:

Library & API reference
=======================

This section describes the modules (function and class libraries) which are
built into MicroPython and available to be used from Python code running on the
device. These modules mirror the standard modules provided in Python, as well
as some MicroPython-specific modules which provide access to hardware features
and other extensions.

Beyond the built-in libraries described in this documentation, many more
modules from the Python standard library, as well as further MicroPython
extensions to it, can be found in :term:`micropython-lib`.

.. contents::

Availability of modules
-----------------------

Due to resource constraints or other limitations, some ports or firmware
versions may not include all the functionality documented here. This means that
a given module, or possibly just particular methods or classes of a module,
will not be available on a given device.

The documentation for each module includes an overview of which :term:`ports
<port>` support that module, but please note that MicroPython is highly
configurable and a given board may customise this further.

On most ports you are able to discover the available, built-in libraries that
can be imported by entering the following at the :term:`REPL`::

    help('modules')

Python standard libraries
-------------------------

The following standard Python libraries have been "micro-ified" to fit in with
the philosophy of MicroPython.  They provide the core functionality of that
module and are intended to be a drop-in replacement for the standard Python
library, however it is important to note that they typically implement a subset
of the functionality.

In some cases these modules provide some MicroPython-specific extensions,
typically to facilitate an optimisation or a way to reduce memory usage.

.. toctree::
    :maxdepth: 1

    array
    binascii
    builtins
    cmath
    collections
    errno
    gc
    hashlib
    heapq
    io
    json
    math
    os
    platform
    random
    re
    select
    socket
    ssl
    struct
    sys
    time
    uasyncio
    zlib
    _thread


MicroPython-specific libraries
------------------------------

Functionality specific to the MicroPython implementation is available in
the following libraries.

.. toctree::
    :maxdepth: 2

    bluetooth
    btree
    cryptolib
    framebuf
    machine
    micropython
    neopixel
    network
    uctypes

The following libraries provide drivers for hardware components.

.. toctree::
    :maxdepth: 1

    wm8960


Port-specific libraries
-----------------------

In some cases the following port/board-specific libraries have functions or
classes similar to those in the :mod:`machine` library.  Where this occurs, the
entry in the port specific library exposes hardware functionality unique to
that platform.

To write portable code, use functions and classes from the :mod:`machine` module.
To access platform-specific hardware use the appropriate library, e.g.
:mod:`pyb` on the :ref:`devices_pyboard`, or :mod:`rp2` on the :ref:`devices_rp2`.


Libraries specific to the pyboard
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following libraries are specific to the pyboard.

.. toctree::
    :maxdepth: 2

    pyb
    stm
    lcd160cr


Libraries specific to the WiPy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following libraries and classes are specific to the WiPy.

.. toctree::
    :maxdepth: 2

    wipy


Libraries specific to the ESP8266 and ESP32
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following libraries are specific to the ESP8266 and ESP32.

.. toctree::
    :maxdepth: 2

    esp
    esp32


Libraries specific to the RP2040
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following libraries are specific to the RP2040, as used in the Raspberry Pi Pico.

.. toctree::
    :maxdepth: 2

    rp2

Libraries specific to Zephyr
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following libraries are specific to the Zephyr port.

.. toctree::
    :maxdepth: 2

    zephyr

Extending built-in libraries
----------------------------

It can useful to extend built-in modules from Python to provide implementations
of lesser-used features that are not included in the standard MicroPython
firmware. The way this can be done is by writing a Python module with the same
name as the built-in module, and either copying it to the
:term:`device filesystem <filesystem>` or :term:`freezing <frozen module>` it into
the firmware, and then wildcard-importing the built-in module into that module.

The way this works is that the built-in modules are actually named ``umodule``
rather than ``module``, but MicroPython will (as a last resort during the
:meth:`module search <sys.path>`) alias any built-in module prefixed with a
``u`` to the non-``u`` version.

For example, in order to extend the built-in :mod:`io` module, a user could
provide ``io.py`` on the filesystem (or frozen), which contains::

    # Bring in all methods and classes from built-in `io`.
    from uio import *

    # Add additional constants not provided by the firmware.
    SEEK_SET = 0
    SEEK_CUR = 1
    SEEK_END = 2

This technique is used extensively in :term:`micropython-lib`. See
:ref:`guides_workflow_packages` for more information.

This applies to both the Python standard libraries (e.g. :mod:`os`, :mod:`time`, etc),
but also the MicroPython libraries too (e.g. :mod:`machine`, :mod:`bluetooth`, etc).
The main exception is the port-specific libraries (:mod:`pyb`, :mod:`esp`, etc).

*Other than in the scenario above when you specifically want to force the use of the
built-in module, we recommend always using* ``import module`` *rather than*
``import umodule``.
