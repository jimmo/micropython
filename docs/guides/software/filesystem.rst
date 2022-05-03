.. _guides_software_filesystem:

Working with the filesystem
===========================

.. contents::

This tutorial describes how MicroPython provides an on-device filesystem,
allowing standard Python file I/O methods to be used with persistent storage.

MicroPython automatically creates a default configuration and auto-detects the
primary filesystem, so this tutorial will be mostly useful if you want to modify
the partitioning, filesystem type, or use custom block devices.

The filesystem is typically backed by internal flash memory on the device, but
can also use external flash, RAM, or a custom block device.

On some ports (e.g. STM32), the filesystem may also be available over USB MSC to
a host PC. :ref:`guides_workflow_pyboard.py` also provides a way for the host PC
to access to the filesystem on all ports.

Note: This is mainly for use on bare-metal ports like STM32 and ESP32. On ports
with an operating system (e.g. the Unix port) the filesystem is provided by the
host OS.

VFS
---

MicroPython implements a Unix-like Virtual File System (VFS) layer. All mounted
filesystems are combined into a single virtual filesystem, starting at the root
``/``. Filesystems are mounted into directories in this structure, and at
startup the working directory is changed to where the primary filesystem is
mounted.

On STM32 / Pyboard, the internal flash is mounted at ``/flash``, and optionally
the SDCard at ``/sd``. On ESP8266/ESP32, the primary filesystem is mounted at
``/``.

Block devices
-------------

A block device is an instance of a class that implements the
:class:`os.AbstractBlockDev` protocol.

Built-in block devices
~~~~~~~~~~~~~~~~~~~~~~

Ports provide built-in block devices to access their primary flash.

On power-on, MicroPython will attempt to detect the filesystem on the default
flash and configure and mount it automatically. If no filesystem is found,
MicroPython will attempt to create a FAT filesystem spanning the entire flash.
Ports can also provide a mechanism to "factory reset" the primary flash, usually
by some combination of button presses at power on.

STM32 / Pyboard
...............

The :ref:`pyb.Flash <pyb.Flash>` class provides access to the internal flash. On some
boards which have larger external flash (e.g. Pyboard D), it will use that
instead. The ``start`` kwarg should always be specified, i.e.
``pyb.Flash(start=0)``.

Note: For backwards compatibility, when constructed with no arguments (i.e.
``pyb.Flash()``), it only implements the simple block interface and reflects the
virtual device presented to USB MSC (i.e. it includes a virtual partition table
at the start).

ESP8266
.......

The internal flash is exposed as a block device object which is created in the
``flashbdev`` module on start up. This object is by default added as a global
variable so it can usually be accessed simply as ``bdev``. This implements the
extended interface.

ESP32
.....

The :class:`esp32.Partition` class implements a block device for partitions
defined for the board. Like ESP8266, there is a global variable ``bdev`` which
points to the default partition. This implements the extended interface.

Custom block devices
~~~~~~~~~~~~~~~~~~~~

The following class implements a simple block device that stores its data in
RAM using a ``bytearray``::

    class RAMBlockDev:
        def __init__(self, block_size, num_blocks):
            self.block_size = block_size
            self.data = bytearray(block_size * num_blocks)

        def readblocks(self, block_num, buf):
            for i in range(len(buf)):
                buf[i] = self.data[block_num * self.block_size + i]

        def writeblocks(self, block_num, buf):
            for i in range(len(buf)):
                self.data[block_num * self.block_size + i] = buf[i]

        def ioctl(self, op, arg):
            if op == 4: # get number of blocks
                return len(self.data) // self.block_size
            if op == 5: # get block size
                return self.block_size

It can be used as follows::

    import os

    bdev = RAMBlockDev(512, 50)
    os.VfsFat.mkfs(bdev)
    os.mount(bdev, '/ramdisk')

An example of a block device that supports both the simple and extended
interface (i.e. both signatures and behaviours of the
:meth:`os.AbstractBlockDev.readblocks` and
:meth:`os.AbstractBlockDev.writeblocks` methods) is::

    class RAMBlockDev:
        def __init__(self, block_size, num_blocks):
            self.block_size = block_size
            self.data = bytearray(block_size * num_blocks)

        def readblocks(self, block_num, buf, offset=0):
            addr = block_num * self.block_size + offset
            for i in range(len(buf)):
                buf[i] = self.data[addr + i]

        def writeblocks(self, block_num, buf, offset=None):
            if offset is None:
                # do erase, then write
                for i in range(len(buf) // self.block_size):
                    self.ioctl(6, block_num + i)
                offset = 0
            addr = block_num * self.block_size + offset
            for i in range(len(buf)):
                self.data[addr + i] = buf[i]

        def ioctl(self, op, arg):
            if op == 4: # block count
                return len(self.data) // self.block_size
            if op == 5: # block size
                return self.block_size
            if op == 6: # block erase
                return 0

As it supports the extended interface, it can be used with :class:`littlefs
<os.VfsLfs2>`::

    import os

    bdev = RAMBlockDev(512, 50)
    os.VfsLfs2.mkfs(bdev)
    os.mount(bdev, '/ramdisk')

Once mounted, the filesystem (regardless of its type) can be used as it
normally would be used from Python code, for example::

    with open('/ramdisk/hello.txt', 'w') as f:
        f.write('Hello world')
    print(open('/ramdisk/hello.txt').read())

Filesystems
-----------

MicroPython ports can provide implementations of :class:`FAT <os.VfsFat>`,
:class:`littlefs v1 <os.VfsLfs1>` and :class:`littlefs v2 <os.VfsLfs2>`.

The following table shows which filesystems are included in the firmware by
default for given port/board combinations, however they can be optionally
enabled in a custom firmware build.

====================  =====  ===========  ===========
Board                 FAT    littlefs v1  littlefs v2
====================  =====  ===========  ===========
pyboard 1.0, 1.1, D   Yes    No           Yes
Other STM32           Yes    No           No
ESP8266 (1M)          No     No           Yes
ESP8266 (2M+)         Yes    No           Yes
ESP32                 Yes    No           Yes
====================  =====  ===========  ===========

FAT
~~~

The main advantage of the FAT filesystem is that it can be accessed over USB MSC
on supported boards (e.g. STM32) without any additional drivers required on the
host PC.

However, FAT is not tolerant to power failure during writes and this can lead to
filesystem corruption. For applications that do not require USB MSC, it is
recommended to use littlefs instead.

To format the entire flash using FAT::

    # ESP8266 and ESP32
    import os
    os.umount('/')
    os.VfsFat.mkfs(bdev)
    os.mount(bdev, '/')

    # STM32
    import os, pyb
    os.umount('/flash')
    os.VfsFat.mkfs(pyb.Flash(start=0))
    os.mount(pyb.Flash(start=0), '/flash')
    os.chdir('/flash')

Littlefs
~~~~~~~~

Littlefs_ is a filesystem designed for flash-based devices, and is much more
resistant to filesystem corruption.

.. note:: There are reports of littlefs v1 and v2 failing in certain
          situations, for details see `littlefs issue 347`_  and
          `littlefs issue 295`_.

To format the entire flash using littlefs v2::

    # ESP8266 and ESP32
    import os
    os.umount('/')
    os.VfsLfs2.mkfs(bdev)
    os.mount(bdev, '/')

    # STM32
    import os, pyb
    os.umount('/flash')
    os.VfsLfs2.mkfs(pyb.Flash(start=0))
    os.mount(pyb.Flash(start=0), '/flash')
    os.chdir('/flash')

A littlefs filesystem can be still be accessed on a PC over USB MSC using the
`littlefs FUSE driver`_.  Note that you must specify both the ``--block_size``
and ``--block_count`` options to override the defaults.  For example (after
building the littlefs-fuse executable)::

    $ ./lfs --block_size=4096 --block_count=512 -o allow_other /dev/sdb1 mnt

This will allow the board's littlefs filesystem to be accessed at the ``mnt``
directory.  To get the correct values of ``block_size`` and ``block_count`` use::

    import pyb
    f = pyb.Flash(start=0)
    f.ioctl(1, 1)  # initialise flash in littlefs raw-block mode
    block_count = f.ioctl(4, 0)
    block_size = f.ioctl(5, 0)

.. _littlefs FUSE driver: https://github.com/littlefs-project/littlefs-fuse
.. _Littlefs: https://github.com/littlefs-project/littlefs
.. _littlefs issue 295: https://github.com/littlefs-project/littlefs/issues/295
.. _littlefs issue 347: https://github.com/littlefs-project/littlefs/issues/347

Hybrid (STM32)
~~~~~~~~~~~~~~

By using the ``start`` and ``len`` kwargs to :class:`pyb.Flash`, you can create
block devices spanning a subset of the flash device.

For example, to configure the first 256kiB as FAT (and available over USB MSC),
and the remainder as littlefs::

    import os, pyb
    os.umount('/flash')
    p1 = pyb.Flash(start=0, len=256*1024)
    p2 = pyb.Flash(start=256*1024)
    os.VfsFat.mkfs(p1)
    os.VfsLfs2.mkfs(p2)
    os.mount(p1, '/flash')
    os.mount(p2, '/data')
    os.chdir('/flash')

This might be useful to make your Python files, configuration and other
rarely-modified content available over USB MSC, but allowing for frequently
changing application data to reside on littlefs with better resilience to power
failure, etc.

The partition at offset ``0`` will be mounted automatically (and the filesystem
type automatically detected), but you can add::

    import os, pyb
    p2 = pyb.Flash(start=256*1024)
    os.mount(p2, '/data')

to ``boot.py`` to mount the data partition.

Hybrid (ESP32)
~~~~~~~~~~~~~~

On ESP32, if you build custom firmware, you can modify ``partitions.csv`` to
define an arbitrary partition layout.

At boot, the partition named "vfs" will be mounted at ``/`` by default, but any
additional partitions can be mounted in your ``boot.py`` using::

    import esp32, os
    p = esp32.Partition.find(esp32.Partition.TYPE_DATA, label='foo')
    os.mount(p, '/foo')




Filesystems
===========

The internal filesystem
=======================

If your devices has 1Mbyte or more of storage then it will be set up (upon first
boot) to contain a filesystem.  This filesystem uses the FAT format and is
stored in the flash after the MicroPython firmware.

Creating and reading files
--------------------------

MicroPython on the ESP8266 supports the standard way of accessing files in
Python, using the built-in ``open()`` function.

To create a file try::

    >>> f = open('data.txt', 'w')
    >>> f.write('some data')
    9
    >>> f.close()

The "9" is the number of bytes that were written with the ``write()`` method.
Then you can read back the contents of this new file using::

    >>> f = open('data.txt')
    >>> f.read()
    'some data'
    >>> f.close()

Note that the default mode when opening a file is to open it in read-only mode,
and as a text file.  Specify ``'wb'`` as the second argument to ``open()`` to
open for writing in binary mode, and ``'rb'`` to open for reading in binary
mode.

Listing file and more
---------------------

The os module can be used for further control over the filesystem.  First
import the module::

    >>> import os

Then try listing the contents of the filesystem::

    >>> os.listdir()
    ['boot.py', 'port_config.py', 'data.txt']

You can make directories::

    >>> os.mkdir('dir')

And remove entries::

    >>> os.remove('data.txt')

Start up scripts
----------------

There are two files that are treated specially by the ESP8266 when it starts up:
boot.py and main.py.  The boot.py script is executed first (if it exists) and
then once it completes the main.py script is executed.  You can create these
files yourself and populate them with the code that you want to run when the
device starts up.

Accessing the filesystem via WebREPL
------------------------------------

You can access the filesystem over WebREPL using the web client in a browser
or via the command-line tool. Please refer to Quick Reference and Tutorial
sections for more information about WebREPL.





zephyr
------


Disk Access
-----------

Use the :ref:`zephyr.DiskAccess <zephyr.DiskAccess>` class to support filesystem::

    import os
    from zephyr import DiskAccess

    block_dev = DiskAccess('SDHC')      # create a block device object for an SD card
    os.VfsFat.mkfs(block_dev)           # create FAT filesystem object using the disk storage block
    os.mount(block_dev, '/sd')          # mount the filesystem at the SD card subdirectory

    # with the filesystem mounted, files can be manipulated as normal
    with open('/sd/hello.txt','w') as f:     # open a new file in the directory
        f.write('Hello world')                  # write to the file
    print(open('/sd/hello.txt').read())      # print contents of the file


Flash Area
----------

Use the :ref:`zephyr.FlashArea <zephyr.FlashArea>` class to support filesystem::

    import os
    from zephyr import FlashArea

    block_dev = FlashArea(4, 4096)      # creates a block device object in the frdm-k64f flash scratch partition
    os.VfsLfs2.mkfs(block_dev)          # create filesystem in lfs2 format using the flash block device
    os.mount(block_dev, '/flash')       # mount the filesystem at the flash subdirectory

    # with the filesystem mounted, files can be manipulated as normal
    with open('/flash/hello.txt','w') as f:     # open a new file in the directory
        f.write('Hello world')                  # write to the file
    print(open('/flash/hello.txt').read())      # print contents of the file


zephyr
------


Filesystems and Storage
=======================

Storage modules support virtual filesystem with FAT and littlefs formats, backed by either
Zephyr DiskAccess or FlashArea (flash map) APIs depending on which the board supports.

See `os Filesystem Mounting <https://docs.micropython.org/en/latest/library/os.html?highlight=os#filesystem-mounting>`_.

Disk Access
-----------

The :ref:`zephyr.DiskAccess <zephyr.DiskAccess>` class can be used to access storage devices, such as SD cards.
This class uses `Zephyr Disk Access API <https://docs.zephyrproject.org/latest/services/storage/disk/access.html>`_ and
implements the `os.AbstractBlockDev` protocol.

For use with SD card controllers, SD cards must be present at boot & not removed; they will
be auto detected and initialized by filesystem at boot. Use the disk driver interface and a
file system to access SD cards via disk access (see below).

Example usage of FatFS with an SD card on the mimxrt1050_evk board::

    import os
    from zephyr import DiskAccess
    bdev = zephyr.DiskAccess('SDHC')        # create block device object using DiskAccess
    os.VfsFat.mkfs(bdev)                    # create FAT filesystem object using the disk storage block
    os.mount(bdev, '/sd')                   # mount the filesystem at the SD card subdirectory
    with open('/sd/hello.txt','w') as f:    # open a new file in the directory
        f.write('Hello world')              # write to the file
    print(open('/sd/hello.txt').read())     # print contents of the file


Flash Area
----------

The :ref:`zephyr.FlashArea <zephyr.FlashArea>` class can be used to implement a low-level storage system or
customize filesystem configurations. To store persistent data on the device, using a higher-level filesystem
API is recommended (see below).

This class uses `Zephyr Flash map API <https://docs.zephyrproject.org/latest/services/storage/flash_map/flash_map.html>`_ and
implements the `os.AbstractBlockDev` protocol.

Example usage with the internal flash on the reel_board or the rv32m1_vega_ri5cy board::

    import os
    from zephyr import FlashArea
    bdev = FlashArea(FlashArea.STORAGE, 4096)   # create block device object using FlashArea
    os.VfsLfs2.mkfs(bdev)                       # create Little filesystem object using the flash area block
    os.mount(bdev, '/flash')                    # mount the filesystem at the flash storage subdirectory
    with open('/flash/hello.txt','w') as f:     # open a new file in the directory
        f.write('Hello world')                  # write to the file
    print(open('/flash/hello.txt').read())      # print contents of the file

For boards such as the frdm_k64f in which the MicroPython application spills into the default flash storage
partition, use the scratch partition by replacing ``FlashArea.STORAGE`` with the integer value 4.
