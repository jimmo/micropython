# MicroPython aioble module
# MIT license; Copyright (c) 2021 Jim Mussared

from micropython import const
import bluetooth
import uasyncio as asyncio

from .core import ensure_active, ble, log_info, log_error, log_warn
from .device import Device, DeviceTimeout

_registered_characteristics = {}

_IRQ_GATTS_WRITE = const(3)
_IRQ_GATTS_READ_REQUEST = const(4)
_IRQ_GATTS_INDICATE_DONE = const(20)

_FLAG_READ = const(0x0002)
_FLAG_WRITE_NO_RESPONSE = const(0x0004)
_FLAG_WRITE = const(0x0008)
_FLAG_NOTIFY = const(0x0010)
_FLAG_INDICATE = const(0x0020)

_FLAG_READ_ENCRYPTED = const(0x0200)
_FLAG_READ_AUTHENTICATED = const(0x0400)
_FLAG_READ_AUTHORIZED = const(0x0800)
_FLAG_WRITE_ENCRYPTED = const(0x1000)
_FLAG_WRITE_AUTHENTICATED = const(0x2000)
_FLAG_WRITE_AUTHORIZED = const(0x4000)

_FLAG_DESC_READ = const(1)
_FLAG_DESC_WRITE = const(2)


def _server_irq(event, data):
    if event == _IRQ_GATTS_WRITE:
        conn_handle, attr_handle = data
        Characteristic._remote_write(conn_handle, attr_handle)
    elif event == _IRQ_GATTS_READ_REQUEST:
        conn_handle, attr_handle = data
        return Characteristic._remote_read(conn_handle, attr_handle)
    elif event == _IRQ_GATTS_INDICATE_DONE:
        conn_handle, value_handle, status = data
        Characteristic._indicate_done(conn_handle, value_handle, status)


class Service:
    def __init__(self, uuid):
        self.uuid = uuid
        self.characteristics = []

    # Generate tuple for gatts_register_services.
    def _tuple(self):
        return (self.uuid, tuple(c._tuple() for c in self.characteristics))


class BaseCharacteristic:
    def _register(self, value_handle):
        self._value_handle = value_handle
        _registered_characteristics[value_handle] = self

    # Generate tuple for gatts_register_services.
    def _tuple(self):
        return (self.uuid, self.flags)

    # Read value from local db.
    def read(self):
        return ble.gatts_read(self._value_handle)

    # Write value to local db.
    def write(self, data):
        ble.gatts_write(self._value_handle, data)

    # Wait for a write on this characteristic.
    # Returns the device that did the write.
    async def written(self, timeout_ms=None):
        if not self._write_event:
            raise ValueError()
        data = self._write_device
        if data is None:
            with DeviceTimeout(None, timeout_ms):
                await self._write_event.wait()
                data = self._write_device
        self._write_device = None
        return data

    def on_read(self, device):
        return 0

    def _remote_write(conn_handle, value_handle):
        if characteristic := _registered_characteristics.get(value_handle, None):
            characteristic._write_device = Device._connected.get(conn_handle, None)
            characteristic._write_event.set()

    def _remote_read(conn_handle, value_handle):
        if characteristic := _registered_characteristics.get(value_handle, None):
            return characteristic.on_read(Device._connected.get(conn_handle, None))


class Characteristic(BaseCharacteristic):
    def __init__(self, service, uuid, read=False, write=False, notify=False, indicate=False):
        service.characteristics.append(self)
        self.descriptors = []

        flags = 0
        if read:
            flags |= _FLAG_READ
        if write:
            flags |= _FLAG_WRITE
            self._write_device = None
            self._write_event = asyncio.PollingEvent()
        if notify:
            flags |= _FLAG_NOTIFY
        if indicate:
            flags |= _FLAG_INDICATE
            # TODO: This should probably be a dict of device to (ev, status).
            # Right now we just support a single indication at a time.
            self._indicate_device = None
            self._indicate_event = asyncio.PollingEvent()
            self._indicate_status = None

        self.uuid = uuid
        self.flags = flags
        self._value_handle = None

    def notify(self, device, data=None):
        if not (self.flags & _FLAG_NOTIFY):
            raise ValueError("Not supported")
        ble.gatts_notify(device._conn_handle, self._value_handle, data)

    async def indicate(self, device, timeout_ms=1000):
        if not (self.flags & _FLAG_INDICATE):
            raise ValueError("Not supported")
        if self._indicate_device is not None:
            raise ValueError("In progress")
        if not device.is_connected():
            raise ValueError("Not connected")

        self._indicate_device = device
        self._indicate_status = None

        try:
            with device.timeout(timeout_ms):
                ble.gatts_indicate(device._conn_handle, self._value_handle)
                await self._indicate_event.wait()
                return self._indicate_status
        finally:
            self._indicate_device = None

    def _indicate_done(conn_handle, value_handle, status):
        if characteristic := _registered_characteristics.get(value_handle, None):
            if device := Device._connected.get(conn_handle, None):
                # See TODO in __init__ to support multiple concurrent indications.
                assert device == characteristic._indicate_device
                characteristic._indicate_status = status
                characteristic._indicate_event.set()


class BufferedCharacteristic(Characteristic):
    def __init__(self, service, uuid, max_len=20, append=False):
        super().__init__(service, uuid, read=True)
        self._max_len = max_len
        self._append = append

    def _register(self, value_handle):
        super()._register(value_handle)
        ble.gatts_set_buffer(value_handle, self._max_len, self._append)


class Descriptor(BaseCharacteristic):
    def __init__(self, characteristic, uuid, read=False, write=False):
        characteristic.descriptors.append(self)

        # Workaround for https://github.com/micropython/micropython/issues/6864
        flags = 0
        if read:
            flags |= _FLAG_DESC_READ
        if write:
            self._write_device = None
            self._write_event = asyncio.PollingEvent()
            flags |= _FLAG_DESC_WRITE

        self.uuid = uuid
        self.flags = flags
        self._value_handle = None


# Turn the Service/Characteristic/Descriptor classes into a registration tuple
# and then extract their value handles.
def register_services(*services):
    ensure_active()
    _registered_characteristics.clear()
    handles = ble.gatts_register_services(tuple(s._tuple() for s in services))
    for i in range(len(services)):
        service_handles = handles[i]
        service = services[i]
        n = 0
        for characteristic in service.characteristics:
            characteristic._register(service_handles[n])
            n += 1
            for descriptor in characteristic.descriptors:
                descriptor._register(service_handles[n])
                n += 1
