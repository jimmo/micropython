.. _devices_esp32_install:

Installation
============

This guide is an extension to the :ref:`general installation guide <guides_workflow_install>`
with information specific to the ESP32 family of microcontrollers.

Getting the firmware
--------------------

Pre-built firmware images for ESP32 boards can be obtained from the
`MicroPython downloads page <https://micropython.org/download/?port=esp32>`_.

Generic boards
~~~~~~~~~~~~~~

If your exact board is not available, then you can use the "generic" firmware
which should work on any ESP32 board for a given family (e.g. ESP32, ESP32S2,
ESP32C3, etc). If your board includes SPIRAM (also known as PSRAM) then you can
use the SPIRAM firmware to enable this.

Deploying the firmware
----------------------

To load a firmware image onto an ESP32 device you need to use the ``esptool.py``
tool provided by Espressif. You can find this tool here:
`<https://github.com/espressif/esptool/>`__, or install it using pip::

    pip install esptool

To use esptool, the board will need to be put into bootloader mode. Most boards
with a USB connection support being put into bootloader mode automatically by
esptool. Otherwise the exact procedure for these steps is highly dependent on
the particular board and you will need to refer to its documentation for
details.

For best results it is recommended to first erase the entire flash of your
device before putting on new MicroPython firmware.

esptool versions starting with 1.3 support both Python 2.7 and Python 3.4 (or newer).
An older version (at least 1.2.1 is needed) works fine but will require Python
2.7.

Using esptool.py you can erase the flash with the command::

    esptool.py --port /dev/ttyUSB0 erase_flash

And then deploy the new firmware using::

    esptool.py --chip esp32 --port /dev/ttyUSB0 write_flash -z 0x1000 esp32-20180511-v1.9.4.bin

TODO: other chips

Notes:

* You might need to change the "port" setting to something else relevant for your
  PC
* You may need to reduce the baudrate if you get errors when flashing
  (eg down to 115200 by adding ``--baud 115200`` into the command)
* For some boards with a particular FlashROM configuration you may need to
  change the flash mode (eg by adding ``-fm dio`` into the command)
* The filename of the firmware should match the file that you have

If the above commands run without error then MicroPython should be installed on
your board and you should be able to access :ref:``the REPL <guides_workflow_repl>``!

Troubleshooting installation problems
-------------------------------------

If you experience problems during flashing or with running firmware immediately
after it, here are troubleshooting recommendations:

* Be aware of and try to exclude hardware problems.  There are 2 common
  problems: bad power source quality, and worn-out/defective FlashROM.
  Speaking of power source, not just raw amperage is important, but also low
  ripple and noise/EMI in general.  The most reliable and convenient power
  source is a USB port.

* The flashing instructions above use flashing speed of 460800 baud, which is
  good compromise between speed and stability. However, depending on your
  module/board, USB-UART convertor, cables, host OS, etc., the above baud
  rate may be too high and lead to errors. Try a more common 115200 baud
  rate instead in such cases.

* To catch incorrect flash content (e.g. from a defective sector on a chip),
  add ``--verify`` switch to the commands above.

* If you still experience problems with flashing the firmware please
  refer to esptool.py project page, https://github.com/espressif/esptool
  for additional documentation and a bug tracker where you can report problems.

* If you are able to flash the firmware but the ``--verify`` option returns
  errors even after multiple retries the you may have a defective FlashROM chip.
