# MicroPython aioble module
# MIT license; Copyright (c) 2021 Jim Mussared

from micropython import const

import uasyncio as asyncio

from .core import ble, log_error
from .device import Device


_IRQ_L2CAP_ACCEPT = const(22)
_IRQ_L2CAP_CONNECT = const(23)
_IRQ_L2CAP_DISCONNECT = const(24)
_IRQ_L2CAP_RECV = const(25)
_IRQ_L2CAP_SEND_READY = const(26)


# Once we start listening we're listening forever. (Limitation in NimBLE)
_listening = False


def _l2cap_irq(event, data):
    if event not in (
        _IRQ_L2CAP_CONNECT,
        _IRQ_L2CAP_DISCONNECT,
        _IRQ_L2CAP_RECV,
        _IRQ_L2CAP_SEND_READY,
    ):
        return

    # All the L2CAP events start with (conn_handle, cid, ...)
    if device := Device._connected.get(data[0], None):
        if channel := device._l2cap_channel:
            # Expect to match the cid for this conn handle (unless we're
            # waiting for connection in which case channel._cid is None).
            if channel._cid is not None and channel._cid != data[1]:
                return

            # Update the channel object with new information.
            if event == _IRQ_L2CAP_CONNECT:
                _, channel._cid, _, channel.our_mtu, channel.peer_mtu = data
            elif event == _IRQ_L2CAP_DISCONNECT:
                _, _, psm, status = data
                channel._status = status
                channel._cid = None
            elif event == _IRQ_L2CAP_RECV:
                channel._data_ready = True
            elif event == _IRQ_L2CAP_SEND_READY:
                channel._stalled = False

            # Notify channel.
            channel._event.set()


# The channel was disconnected during a send/recvinto/flush.
class L2CAPDisconnectedError(Exception):
    pass


# Failed to connect to device (argument is status).
class L2CAPConnectionError(Exception):
    pass


class L2CAPChannel:
    def __init__(self, device):
        if not device.is_connected():
            raise ValueError("Not connected")

        if device._l2cap_channel:
            raise ValueError("Already has channel")
        device._l2cap_channel = self

        self.device = device
        # Maximum size that the other side can send to us.
        self.our_mtu = 0
        # Maximum size that we can send.
        self.peer_mtu = 0

        # Set back to None on disconnection.
        self._cid = None
        # Set during disconnection.
        self._status = 0

        # If true, must wait for _IRQ_L2CAP_SEND_READY IRQ before sending.
        self._stalled = False

        # Has received a _IRQ_L2CAP_RECV since the buffer was last emptied.
        self._data_ready = False

        self._event = asyncio.PollingEvent()

    async def recvinto(self, buf, timeout_ms=None):
        # Wait until the data_ready flag is set. This flag is only ever set by
        # the event and cleared by this function.
        with self.device.timeout(timeout_ms):
            while not self._data_ready:
                await self._event.wait()
                if self._cid is None:
                    raise L2CAPDisconnectedError

        if self._cid is None:
            raise L2CAPDisconnectedError

        # Extract up to len(buf) bytes from the channel buffer.
        n = ble.l2cap_recvinto(self.device._conn_handle, self._cid, buf)

        # Check if there's still remaining data in the channel buffers.
        self._data_ready = ble.l2cap_recvinto(self.device._conn_handle, self._cid, None) > 0

        return n

    # Synchronously see if there's data ready.
    def available(self):
        return self._data_ready

    # Waits until the channel is free and then sends buf.
    # len(buf) must be less than min(self.peer_mtu, 2 * self.our_mtu).
    async def send(self, buf, timeout_ms=None):
        await self.flush(timeout_ms)
        # l2cap_send returns True if you can send immediately.
        self._stalled = not ble.l2cap_send(self.device._conn_handle, self._cid, buf)

    async def flush(self, timeout_ms=None):
        # Wait for the _stalled flag to be cleared by the IRQ.
        with self.device.timeout(timeout_ms):
            while self._stalled:
                await self._event.wait()
                if self._cid is None:
                    raise L2CAPDisconnectedError

    async def disconnect(self, timeout_ms=1000):
        if self._cid is None:
            return

        # Wait for the cid to be cleared by the disconnect IRQ.
        with self.device.timeout(timeout_ms):
            ble.l2cap_disconnect(self.device._conn_handle, self._cid)
            while self._cid is not None:
                await self._event.wait()

    # Context manager -- automatically disconnect.
    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_traceback):
        await self.disconnect()


# Use device.l2cap_accept() instead of calling this directly.
async def accept(device, psn, mtu, timeout_ms):
    channel = L2CAPChannel(device)

    # Start the stack listening if necessary.
    if not _listening:
        ble.l2cap_listen(psn, mtu)

    # Wait for the connect irq from the remote device.
    with device.timeout(timeout_ms):
        await channel._event.wait()
        return channel


# Use device.l2cap_connect() instead of calling this directly.
async def connect(device, psn, mtu, timeout_ms):
    if _listening:
        raise ValueError("Can't connect while listening")

    channel = L2CAPChannel(device)

    with device.timeout(timeout_ms):
        ble.l2cap_connect(device._conn_handle, psn, mtu)

        # Wait for the connect irq from the remote device.
        # If the connection fails, we get a disconnect event (with status) instead.
        await channel._event.wait()

        if channel._cid is not None:
            return channel
        else:
            raise L2CAPConnectionError(channel._status)
