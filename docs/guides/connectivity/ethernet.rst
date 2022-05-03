.. _guides_connectivity_ethernet:

Ethernet
========

imx
---

All MIMXRT boards except the MIMXRT1011 based boards and Teensy 4.0 support
Ethernet.  Example usage::

    import network

    lan = network.LAN(0)
    lan.active(True)

If there is a DHCP server in the LAN, the IP address is supplied by that server.
Otherwise, the IP address can be set with lan.ifconfig().  The default address
is 192.168.0.1.

Teensy 4.1 does not have an Ethernet jack on the board, but PJRC offers an
adapter for self-assembly.  The Seeed ARCH MIX board has no PHY hardware on the
board, however you can attach external PHY interfaces.  By default, the firmware
for Seeed Arch Mix uses the driver for a LAN8720 PHY.  The MIMXRT1170_EVK is
equipped with two Ethernet ports, which are addressed as LAN(0) for the 100M
port and LAN(1) for the 1G port.

For details of the network interface refer to the class :ref:`network.LAN <network.LAN>`.
