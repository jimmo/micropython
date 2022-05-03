:mod:`hashlib` -- hashing algorithms
====================================

.. module:: hashlib
    :synopsis: hashing algorithms

|see_cpython_module| :mod:`python:hashlib`.

This module implements binary data hashing algorithms. The exact inventory of
available algorithms depends on the port or board. Among the algorithms which
may be implemented:

* SHA256 - The current generation, modern hashing algorithm (from the SHA2
  series). It is suitable for cryptographically-secure purposes. Included in
  the MicroPython core and any board is recommended to provide this, unless it
  has particular code size constraints.

* SHA1 - A previous generation algorithm. Not recommended for new usages,
  but SHA1 is a part of number of Internet standards and existing
  applications, so boards targeting network connectivity and
  interoperability will try to provide this.

* MD5 - A legacy algorithm, not considered cryptographically secure. Only
  selected boards, targeting interoperability with legacy applications,
  will offer this.

class :class:`sha256`
---------------------

.. class:: sha256([data])

    Create an SHA256 hasher object and optionally feed ``data`` into it.

class :class:`sha1`
-------------------

.. class:: sha1([data])

    Create an SHA1 hasher object and optionally feed ``data`` into it.

class :class:`md5`
------------------

.. class:: md5([data])

    Create an MD5 hasher object and optionally feed ``data`` into it.

Methods
~~~~~~~

These methods are implemented by all three classes above.

.. method:: hash.update(data)

    Feed more binary data into the hash.

.. method:: hash.digest()

    Return the hash for all data passed through the hash, as a bytes object. After this
    method is called, no more data can be passed to the hash object.

.. method:: hash.hexdigest()

    This method is NOT implemented. Use :func:`binascii.hexlify`
    to achieve a similar effect.::

        binascii.hexlify(hash.digest())
