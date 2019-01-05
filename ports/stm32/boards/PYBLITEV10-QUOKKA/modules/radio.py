from micropython import const
import machine
import time
import quokka

_SPI_SYNC_MASTER_WRITE = const(0x85)
_SPI_SYNC_MASTER_READ = const(0xca)
_SPI_SYNC_SLAVE_IDLE = const(0x95)
_SPI_SYNC_SLAVE_REPLY = const(0xda)
_SPI_SYNC_SLAVE_BUSY = const(0xb3)
_SPI_SYNC_SLAVE_ERROR = const(0xf1)


class RadioError(Exception):
    pass


spi = quokka._internal_spi
cs = machine.Pin('Y4', machine.Pin.OUT)
buf68 = bytearray(68)


def _init():
    # Check version
    _sync()
    ret = _cmd(b'\x02', 3)
    if not (len(ret) == 3 and ret[0] == 0x02):
        raise RadioError('cannot get version', ret)
    if not (ret[1] == 1 and ret[2] == 0):
        raise RadioError('version mismatch', ret)

def _chk(buf):
    c = 0xff
    for b in buf:
        c = (c ^ b) & 0xff
    return c

def _xfer(hdr, payload):
    buf = memoryview(buf68)[0:2 + len(payload)]
    start = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), start) < 500:
        buf[0] = hdr
        buf[1:1 + len(payload)] = payload
        buf[-1] = _chk(payload)
        cs(0)
        spi.write_readinto(buf, buf)
        cs(1)
        if buf[0] == _SPI_SYNC_SLAVE_BUSY:
            #print('wait for slave', buf)
            time.sleep_ms(20)
        else:
            return buf
    raise RadioError('xfer: timeout')

def _wr(payload):
    buf = _xfer(_SPI_SYNC_MASTER_WRITE, payload)
    if buf[0] != _SPI_SYNC_SLAVE_IDLE:
        raise RadioError('_wr: slave not idle', bytes(buf))
    #print('WR', payload, '->', buf)

def _rd(nrecv_max):
    payload = bytearray(nrecv_max + 1) # +1 for received checksum
    buf = _xfer(_SPI_SYNC_MASTER_READ, payload)
    if buf[0] != _SPI_SYNC_SLAVE_REPLY:
        raise RadioError('_rd: slave no reply', bytes(buf))
    l = buf[1]
    if _chk(buf[1:3 + l]):
        raise RadioError('_rd: checksum failed', bytes(buf))
    out = bytes(buf[2:2 + l])
    #print('RD', payload, buf, l, out)
    return out

def _cmd(payload, nrecv_max):
    #print('Radio _cmd:', payload, nrecv_max)
    _wr(payload)
    return _rd(nrecv_max)

def _sync():
    # Get SPI into known sync state
    buf = bytearray(10)
    for i in range(5):
      _xfer(_SPI_SYNC_MASTER_READ, buf)
      ret = _xfer(_SPI_SYNC_MASTER_READ, buf)
      if ret[0] == _SPI_SYNC_SLAVE_IDLE:
          return
      time.sleep_ms(50)
    raise RadioError('_sync: slave not in sync', bytes(ret))

##############################################################
# Public API

def on():
    _sync()
    ret = _cmd(b'\x10\x01', 1)
    if not (len(ret) == 1 and ret[0] == 0x10):
        raise RadioError('enable: could not enable', ret)

def off():
    ret = _cmd(b'\x10\x00', 1)
    if not (len(ret) == 1 and ret[0] == 0x10):
        raise RadioError('enable: could not disable', ret)

def config(**kwargs):
    cfgs = {
        'power': 0x44,
        'length': 0x41,
        'queue': 0x42,
        'channel': 0x43,
        'power': 0x44,
        'data_rate': 0x45,
        'address': 0x86,
        'group': 0x47,
    }
    cmd = b'\x12'
    for arg, val in kwargs.items():
        if arg not in cfgs:
            raise ValueError('unknown argument %s' % arg)
        id = cfgs[arg]
        if id >> 6 == 1:
            cmd += bytes([id, int(val)])
        else:
            cmd += id.to_bytes(1, 'little') + int(val).to_bytes(4, 'little')
    ret = _cmd(cmd, 2)
    if not (len(ret) == 2 and ret[0] == 0x12):
        raise ValueError('config: could not set', ret)
    if ret[1] != 0:
        for name, id in cfgs.items():
            if id & 0x3f == 256 - ret[1]:
                raise ValueError('value out of range for argument %s' % name)
        raise ValueError('value out of range')

def send_bytes(msg):
    ret = _cmd(b'\x13' + msg, 2)
    if not (len(ret) == 2 and ret[0] == 0x13 and ret[1] == len(msg)):
        raise RadioError('send_bytes: could not send', ret)

def send(msg):
    if isinstance(msg, str):
        msg = bytes(msg, 'utf8')
    send_bytes(b'\x01\x00\x01' + msg)

def receive_full():
    ret = _cmd(b'\x14', 64)
    if not (ret[0] == 0x14):
        raise RadioError('receive_full: could not receive', ret)
    l = ret[1]
    if l < 5:
        return None
    l -= 5
    return ret[2:2 + l], -ret[2 + l], 0

def receive_bytes():
    msg = receive_full()
    if not msg:
        return msg
    return msg[0]

def receive():
    msg = receive_full()
    if not msg:
        return msg
    data = msg[0]
    if len(data) < 3 or not(data[0] == 1 and data[1] == 0 and data[2] == 1):
        raise ValueError('received packet is not a string')
    return str(data[3:], 'utf8')

def ble_on():
    _cmd(b'\x20\x01', 5)

def ble_off():
    _cmd(b'\x20\x00', 5)

def ble_adv(data):
    cmd = b'\x21'
    for t, d in data:
        cmd += bytearray([1 + len(d), t]) + d
    _cmd(cmd, 5)

def version():
    ret = _cmd(b'\x02', 3)
    if not (ret[0] == 0x02):
        return None
    return str(ret[1]) + '.' + str(ret[2])

_init()
