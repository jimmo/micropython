import micropython
import machine

_ADDR_WRITE = const(0x80)

_REG_FIFO = const(0x00)

_REG_OP_MODE = const(0x01)

_REG_OP_MODE_LORA = const(0x80)
# _REG_OP_MODE_ACCESS_FSK = const(0x40)
# _REG_OP_MODE_LF_MODE = const(0x08)

_REG_OP_MODE_SLEEP = const(0x00)
_REG_OP_MODE_STANDBY = const(0x01)
_REG_OP_MODE_FSTX = const(0x02)
_REG_OP_MODE_TX = const(0x03)
_REG_OP_MODE_FSRX = const(0x04)
_REG_OP_MODE_RX_CONTINUOUS = const(0x05)
_REG_OP_MODE_RX_SINGLE = const(0x06)
_REG_OP_MODE_CAD = const(0x07)

_REG_DIO_MAPPING1 = const(0x40)
_REG_DIO_MAPPING2 = const(0x41)

_REG_VERSION = const(0x42)


class SX1276:
    def __init__(self, spi, cs, nrst, dio0, dio1, dio2=None, dio3=None, dio4=None, dio5=None):
        self._spi = spi
        self._cs = cs
        self._nrst = nrst
        self._cs.init(mode=machine.Pin.OUT)
        self._cs.high()
        self._nrst.init(mode=machine.Pin.OUT)
        self._nrst.low()
        self._nrst.high()
        self._dio0 = dio0
        self._dio0.irq(handler=self._dio0_handler)
        self._dio1 = dio1
        self._dio1.irq(handler=self._dio1_handler)
        self._buf = bytearray(2)
        print(self.version())
        print(self._read_reg(_REG_OP_MODE))

        self._write_reg(_REG_OP_MODE, _REG_OP_MODE_SLEEP)
        self._write_reg(_REG_OP_MODE, _REG_OP_MODE_LORA | _REG_OP_MODE_SLEEP)

        print(self._read_reg(_REG_OP_MODE))

    def version(self):
        return self._read_reg(_REG_VERSION)

    def _dio0_handler(self, p):
        print('dio0', p.value())

    def _dio1_handler(self, p):
        print('dio1', p.value())

    def _read_reg(self, addr):
        self._buf[0] = addr
        self._cs.low()
        self._spi.write(self._buf[0:1])
        v = self._spi.read(1)[0]
        self._cs.high()
        return v

    def _write_reg(self, addr, v):
        self._buf[0] = _ADDR_WRITE | addr
        self._buf[1] = v
        self._cs.low()
        self._spi.write(self._buf[0:1])
        self._spi.write(self._buf[1:2])
        self._cs.high()

abz = SX1276(machine.SPI(1), machine.Pin.board.SX_NSS, machine.Pin.board.SX_NRST, machine.Pin.board.SX_DIO0, machine.Pin.board.SX_DIO1)
#abz = SX1276(machine.SPI(miso=machine.Pin.board.SX_MISO, mosi=machine.Pin.board.SX_MOSI, sck=machine.Pin.board.SX_SCK), machine.Pin.board.SX_NSS, machine.Pin.board.SX_NRST, machine.Pin.board.SX_DIO0, machine.Pin.board.SX_DIO1)
