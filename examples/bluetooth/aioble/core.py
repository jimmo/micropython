# MicroPython aioble module
# MIT license; Copyright (c) 2021 Jim Mussared

import bluetooth


def log_error(*args):
    print("[aioble] E:", *args)


def log_warn(*args):
    print("[aioble] W:", *args)


def log_info(*args):
    pass
    # print("[aioble] I:", *args)


class GattError(Exception):
    def __init__(self, status):
        self._status = status


def ensure_active():
    if not ble.active():
        try:
            from .security import load_secrets

            load_secrets()
        except:
            pass
        ble.active(True)


def null_irq(event, data):
    return None


def ble_irq(event, data):
    log_info(event, data)

    try:
        from .peripheral import _peripheral_irq
    except:
        _peripheral_irq = null_irq

    try:
        from .central import _central_irq
    except:
        _central_irq = null_irq

    try:
        from .client import _client_irq
    except:
        _client_irq = null_irq

    try:
        from .server import _server_irq
    except:
        _server_irq = null_irq

    try:
        from .security import _security_irq
    except:
        _security_irq = null_irq

    try:
        from .l2cap import _l2cap_irq
    except:
        _l2cap_irq = null_irq

    from .device import _device_irq

    return (
        _peripheral_irq(event, data)
        or _central_irq(event, data)
        or _client_irq(event, data)
        or _server_irq(event, data)
        or _security_irq(event, data)
        or _l2cap_irq(event, data)
        or _device_irq(event, data)
    )


ble = bluetooth.BLE()
ble.irq(ble_irq)
