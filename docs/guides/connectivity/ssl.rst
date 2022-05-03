.. _guides_connectivity_ssl:

SSL & TLS
=========

.. _guides_connectivity_ssl_axtls:

SSL/TLS limitations (ESP8266 and Unix port only)
------------------------------------------------

ESP8266 uses the `axTLS <http://axtls.sourceforge.net/>`_ library, which is one
of the smallest TLS libraries with compatible licensing. However, it also has
some known issues/limitations:

1. No support for Diffie-Hellman (DH) key exchange and Elliptic-curve
   cryptography (ECC). This means it can't work with sites which require
   the use of these features (it works ok with the typical sites that use
   RSA certificates).
2. Half-duplex communication nature. axTLS uses a single buffer for both
   sending and receiving, which leads to considerable memory saving and
   works well with protocols like HTTP. But there may be problems with
   protocols which don't follow classic request-response model.

Besides axTLS's own limitations, the configuration used for MicroPython is
highly optimized for code size, which leads to additional limitations
(these may be lifted in the future):

3. Optimized RSA algorithms are not enabled, which may lead to slow
   SSL handshakes.
4. Session Reuse is not enabled, which means every connection must undergo
   the full, expensive SSL handshake.

Besides axTLS specific limitations described above, there is another generic
limitation with usage of TLS on the low-memory devices:

5. The TLS standard specifies the maximum length of the TLS record (unit
   of TLS communication, the entire record must be buffered before it can
   be processed) as 16KB. That's almost half of the available ESP8266 memory,
   and inside a more or less advanced application would be hard to allocate
   due to memory fragmentation issues. As a compromise, a smaller buffer is
   used, with the idea that the most interesting usage for SSL would be
   accessing various REST APIs, which usually require much smaller messages.
   The buffers size is on the order of 5KB, and is adjusted from time to
   time, taking as a reference being able to access https://www.google.com .
   The smaller buffer however means that some sites can't be accessed using
   it, and it's not possible to stream large amounts of data. axTLS does
   have support for TLS's Max Fragment Size extension, but no HTTPS website
   does, so use of the extension is really only effective for local
   communication with other devices.

There are also some not implemented features specifically in MicroPython's
``ssl`` module based on axTLS:

6. Certificates are not validated (this makes connections susceptible
   to man-in-the-middle attacks).
7. There is no support for client certificates (scheduled to be fixed in
   1.9.4 release). TODO
