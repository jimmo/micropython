:mod:`zlib` -- zlib decompression
=================================

.. module:: zlib
    :synopsis: zlib decompression

|see_cpython_module| :mod:`python:zlib`.

This module allows decompression of binary data compressed with the
`DEFLATE algorithm <https://en.wikipedia.org/wiki/DEFLATE>`_
(commonly used in the zlib library and gzip archiver).

Note: Compression is not yet implemented.

Functions
---------

.. function:: decompress(data, wbits=0, bufsize=0, /)

    Return decompressed *data* as bytes.

    *wbits* is the log-base-2 of the DEFLATE dictionary window
    size used during compression. Allowed values are 8-15, corresponding to a dictionary size of 256-32768.

    Additionally, if the *wbits* value is positive, *data* is assumed to be
    zlib stream (with zlib header). Otherwise, if it's negative, it's assumed
    to be raw DEFLATE stream.

    The *bufsize* parameter is for compatibility with CPython and is ignored.

.. class:: DecompIO(stream, wbits=0, /)

    Create a `stream` wrapper which allows transparent decompression of
    compressed data in another *stream*. This allows processing of compressed
    streams with data larger than available heap size. In addition to values
    described in :func:`decompress`, *wbits* may take values 24..31
    (16 + 8..15), meaning that input stream has a gzip header.

    .. admonition:: Difference to CPython
        :class: attention

        This class is MicroPython extension. It's included on provisional
        basis and may be changed considerably or removed in later versions.
