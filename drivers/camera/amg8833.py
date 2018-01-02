# MicroPython driver for Panasonic AMG8833 "Grid-Eye" 8x8 IR sensor.
# https://industrial.panasonic.com/cdbs/www-data/pdf/ADI8000/ADI8000C59.pdf

from ustruct import unpack
from micropython import const


# Status and configuration registers
AMG8833_PCTL   = const(0x00)
AMG8833_RST    = const(0x01)
AMG8833_FPSC   = const(0x02)
AMG8833_INTC   = const(0x03)
AMG8833_STAT   = const(0x04)
AMG8833_SCLR   = const(0x05)
AMG8833_AVE    = const(0x07)
AMG8833_INTHL  = const(0x08)
AMG8833_INTHH  = const(0x09)
AMG8833_INTLL  = const(0x0A)
AMG8833_INTLH  = const(0x0B)
AMG8833_IHYSL  = const(0x0C)
AMG8833_IHYSH  = const(0x0D)
AMG8833_TTHL   = const(0x0E)
AMG8833_TTHH   = const(0x0F)

# Interrupt result registers
AMG8833_INT0   = const(0x10)
AMG8833_INT1   = const(0x11)
AMG8833_INT2   = const(0x12)
AMG8833_INT3   = const(0x13)
AMG8833_INT4   = const(0x14)
AMG8833_INT5   = const(0x15)
AMG8833_INT6   = const(0x16)
AMG8833_INT7   = const(0x17)


AMG8833_DATA_START = const(0x80)


class AMG8833:
    def __init__(self, i2c, addr):
        self.i2c = i2c
        self.addr = addr
        self.buf = bytearray(64 * 2)
        self.temperatures = [0] * 64

        # Reset
        self.i2c.writeto_mem(self.addr, AMG8833_RST, bytes([0x3f]))
        # Normal mode
        self.i2c.writeto_mem(self.addr, AMG8833_PCTL, bytes([0x00]))
        # Sample rate (0x00 = 10, 0x01 = 1 fps)
        self.i2c.writeto_mem(self.addr, AMG8833_FPSC, bytes([0x00]))

    def thermistor(self):
        data = self.i2c.readfrom_mem(self.addr, AMG8833_TTHL, 2)
        return unpack('>H', data)[0]

    def celcius(self, bh, bl):
        # bh:bl is a 12-bit twos-complement value in quarter-degrees C.
        t = bh << 8 | bl
        if bh & 8:
            t -= (1 << 12)
        return 0.25 * t

    def capture(self):
        self.i2c.readfrom_mem_into(self.addr, AMG8833_DATA_START, self.buf)
        for i in range(64):
            self.temperatures[i] = self.celcius(self.buf[i*2+1], self.buf[i*2])
        return self.temperatures
