:mod:`cryptolib` -- cryptographic ciphers
=========================================

.. module:: cryptolib
    :synopsis: cryptographic ciphers

class :class:`aes`
------------------

.. class:: aes

    .. classmethod:: __init__(key, mode, [IV])

        Initialize cipher object, suitable for encryption/decryption. Note:
        after initialization, the cipher object can be use only either for
        encryption or decryption, however running a decrypt() operation after
        an encrypt() or vice-versa is not supported.

        Parameters are:

            * *key* is an encryption/decryption key (bytes-like).
            * *mode* is one of the ``cryptolib.MODE_`` constants below.
            * *IV* is an initialization vector for CBC mode.
            * For Counter mode, *IV* is the initial value for the counter.

    .. method:: encrypt(in_buf, [out_buf])

        Encrypt *in_buf*. If no *out_buf* is given result is returned as a newly
        allocated `bytes` object. Otherwise, result is written into the mutable
        buffer *out_buf*. *in_buf* and *out_buf* can also refer to the same
        mutable buffer, in which case data is encrypted in-place.

    .. method:: decrypt(in_buf, [out_buf])

        Like `encrypt()`, but for decryption.

Constants
---------

Note: These constants are not available on all ports, but you can use the
numeric values directly.

.. data:: cryptolib.MODE_ECB

    ``1`` -- Electronic Code Book (ECB).

.. data:: cryptolib.MODE_CBC

    ``2`` -- Cipher Block Chaining (CBC).

.. data:: cryptolib.MODE_CTR

    ``6`` -- Counter mode (CTR).
