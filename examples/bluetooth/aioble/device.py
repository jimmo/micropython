# MicroPython aioble module
# MIT license; Copyright (c) 2021 Jim Mussared

from micropython import const

import uasyncio as asyncio
import binascii

from .core import ble


_IRQ_MTU_EXCHANGED = const(21)


# Raised by `with device.timeout()`.
class DeviceDisconnectedError(Exception):
    pass


def _device_irq(event, data):
    if event == _IRQ_MTU_EXCHANGED:
        conn_handle, mtu = data
        if device := Device._connected.get(conn_handle, None):
            device.mtu = mtu
            if device._mtu_event:
                device._mtu_event.set()


# Context manager to allow an operation to be cancelled by timeout or device
# disconnection. Don't use this directly -- use `with device.timeout(ms):`
# instead.
class DeviceTimeout:
    def __init__(self, device, timeout_ms):
        self._device = device
        self._timeout_ms = timeout_ms

        # We allow either (or both) device and timeout_ms to be None. This
        # allows this to be used either as a just-disconnect, just-timeout, or
        # no-op.

        # This task is active while the operation is in progress. It sleeps
        # until the timeout, and then cancels the working task. If the working
        # task completes, __exit__ will cancel the sleep.
        self._timeout_task = None

        # This is the task waiting for the actual operation to complete.
        # Usually this is waiting on an event that will be set() by an IRQ
        # handler.
        self._task = asyncio.current_task()

        # Tell the device that if it disconnects, it should cancel this
        # operation (by cancelling self._task).
        if device:
            device._timeouts.append(self)

    async def _timeout_sleep(self):
        try:
            await asyncio.sleep_ms(self._timeout_ms)
        except asyncio.CancelledError:
            # The operation completed successfully and this timeout task was
            # cancelled by __exit__.
            return

        # The sleep completed, so we should trigger the timeout. Set
        # self._timeout_task to None so that we can tell the difference
        # between a disconnect and a timeout in __exit__.
        self._timeout_task = None
        self._task.cancel()

    def __enter__(self):
        if self._timeout_ms:
            # Schedule the timeout waiter.
            self._timeout_task = asyncio.create_task(self._timeout_sleep())

    def __exit__(self, exc_type, exc_val, exc_traceback):
        # One of five things happened:
        # 1 - The operation completed successfully.
        # 2 - The operation timed out.
        # 3 - The device disconnected.
        # 4 - The operation failed for a different exception.
        # 5 - The task was cancelled by something else.

        # Don't need the device to tell us about disconnection anymore.
        if self._device:
            self._device._timeouts.remove(self)

        try:
            if exc_type == asyncio.CancelledError:
                # Case 2, we started a timeout and it's completed.
                if self._timeout_ms and self._timeout_task is None:
                    raise asyncio.TimeoutError

                # Case 3, we have a disconnected device.
                if self._device and self._device._conn_handle is None:
                    raise DeviceDisconnectedError

                # Case 5, something else cancelled us.
                # Allow the cancellation to propagate.
                return

            # Case 1 & 4. Either way, just stop the timeout task and let the
            # exception (if case 4) propagate.
        finally:
            # In all cases, if the timeout is still running, cancel it.
            if self._timeout_task:
                self._timeout_task.cancel()


class Device:
    # Global map of connection handle to active devices (for IRQ mapping).
    _connected = {}

    def __init__(self, addr_type, addr):
        # Public properties
        self.addr_type = addr_type
        self.addr = addr
        self.encrypted = False
        self.authenticated = False
        self.bonded = False
        self.key_size = False
        self.mtu = None

        self._conn_handle = None

        # This event is fired by the IRQ both for connection and disconnection
        # and controls the device_task.
        self._event = None

        # If we're waiting for a pending MTU exchange.
        self._mtu_event = None

        # In-progress client discovery instance (e.g. services, chars,
        # descriptors) used for IRQ mapping.
        self._discover = None
        # Map of value handle to characteristic (so that IRQs with
        # conn_handle,value_handle can route to them). See
        # ClientCharacteristic._find for where this is used.
        self._characteristics = {}

        self._task = None

        # DeviceTimeout instances that are currently waiting on this device
        # and need to be notified if disconnection occurs.
        self._timeouts = []

        # Fired by the encryption update event.
        self._pair_event = None

        # Active L2CAP channel for this device.
        # TODO: Support more than one concurrent channel.
        self._l2cap_channel = None

    def __eq__(self, rhs):
        return self.addr_type == rhs.addr_type and self.addr == rhs.addr

    def __hash__(self):
        return hash((self.addr_type, self.addr))

    def __str__(self):
        return "Device({},{}{})".format(
            self.addr_type,
            binascii.hexlify(self.addr),
            ",{}".format(self._conn_handle) if self._conn_handle is not None else "",
        )

    # While connected, this tasks waits for disconnection then cleans up.
    async def device_task(self):
        assert self._conn_handle is not None

        # Wait for the (either central or peripheral) disconnected irq.
        await self._event.wait()

        # Mark the device as disconnected.
        del Device._connected[self._conn_handle]
        self._conn_handle = None

        # Cancel any in-progress operations on this device.
        for t in self._timeouts:
            t._task.cancel()

    async def connect(self, timeout_ms=2000):
        if self.is_connected():
            return

        # Forward to implementation in central.py.
        from .central import _connect

        await _connect(self, timeout_ms)

        # Start the device task that will clean up after disconnection.
        self._run_device_task()

    def _run_device_task(self):
        # Event will be already created this if we initiated connection.
        self._event = self._event or asyncio.PollingEvent()

        self._task = asyncio.create_task(self.device_task())

    async def disconnect(self, timeout_ms=2000):
        await self.disconnected(timeout_ms, disconnect=True)

    async def disconnected(self, timeout_ms=2000, disconnect=False):
        if not self.is_connected():
            return

        # The task must have been created after successful connection.
        assert self._task

        if disconnect:
            ble.gap_disconnect(self._conn_handle)

        with DeviceTimeout(None, timeout_ms):
            await self._task

    # Retrieve a single service matching this uuid.
    async def service(self, uuid):
        result = None
        # Make sure loop runs to completion.
        async for service in self.services(uuid):
            if not result and service.uuid == uuid:
                result = service
        return result

    # Search for all services (optionally by uuid).
    # Use with `async for`, e.g.
    #     async for service in device.services():
    # Note: must allow the loop to run to completion.
    # TODO: disconnection / timeout
    def services(self, uuid=None):
        from .client import ClientDiscover, ClientService

        return ClientDiscover(self, ClientService, self, uuid)

    async def pair(self, *args, **kwargs):
        from .security import pair_device

        await pair_device(self, *args, **kwargs)

    def is_connected(self):
        return self._conn_handle is not None

    # Use with `with` to simplify disconnection and timeout handling.
    def timeout(self, timeout_ms):
        return DeviceTimeout(self, timeout_ms)

    async def exchange_mtu(self, mtu=None):
        if not self.is_connected():
            raise ValueError("Not connected")

        if mtu:
            ble.config(mtu=mtu)

        self._mtu_event = self._mtu_event or asyncio.PollingEvent()
        ble.gattc_exchange_mtu(self._conn_handle)
        await self._mtu_event.wait()
        return self.mtu

    # Wait for a connection on an L2CAP connection-oriented-channel.
    async def l2cap_accept(self, psm, mtu, timeout_ms=None):
        from .l2cap import accept

        return await accept(self, psm, mtu, timeout_ms)

    # Attempt to connect to a listening device.
    async def l2cap_connect(self, psm, mtu, timeout_ms=1000):
        from .l2cap import connect

        return await connect(self, psm, mtu, timeout_ms)

    # Context manager -- automatically disconnect.
    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_traceback):
        await self.disconnect()
