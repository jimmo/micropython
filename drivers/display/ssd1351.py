# MicroPython SSD1351 OLED driver, SPI interface
# https://www.newhavendisplay.com/app_notes/SSD1351.pdf

from micropython import const
from time import sleep_ms
from ustruct import pack
import framebuf


# SSD1351 Commands
CMD_SETCOLUMN      = const(0x15)
CMD_SETROW         = const(0x75)
CMD_WRITERAM       = const(0x5c)
CMD_READRAM        = const(0x5d)
CMD_SETREMAP       = const(0xa0)
CMD_STARTLINE      = const(0xa1)
CMD_DISPLAYOFFSET  = const(0xa2)
CMD_DISPLAYALLOFF  = const(0xa4)
CMD_DISPLAYALLON   = const(0xa5)
CMD_NORMALDISPLAY  = const(0xa6)
CMD_INVERTDISPLAY  = const(0xa7)
CMD_FUNCTIONSELECT = const(0xab)
CMD_DISPLAYOFF     = const(0xae)
CMD_DISPLAYON      = const(0xaf)
CMD_PRECHARGE      = const(0xb1)
CMD_DISPLAYENHANCE = const(0xb2)
CMD_CLOCKDIV       = const(0xb3)
CMD_SETVSL         = const(0xb4)
CMD_SETGPIO        = const(0xb5)
CMD_PRECHARGE2     = const(0xb6)
CMD_SETGRAY        = const(0xb8)
CMD_USELUT         = const(0xb9)
CMD_PRECHARGELEVEL = const(0xbb)
CMD_VCOMH          = const(0xbe)
CMD_CONTRASTABC    = const(0xc1)
CMD_CONTRASTMASTER = const(0xc7)
CMD_MUXRATIO       = const(0xca)
CMD_COMMANDLOCK    = const(0xfd)
CMD_HORIZSCROLL    = const(0x96)
CMD_STOPSCROLL     = const(0x9e)
CMD_STARTSCROLL    = const(0x9f)


class SSD1351(framebuf.FrameBuffer):
    def __init__(self, width, height, spi, dc, res, cs):
        self.width = width
        self.height = height
        self.spi = spi
        self.dc = dc
        self.res = res
        self.cs = cs
        self.rate = 10 * 1024 * 1024

        self.dc.init(dc.OUT, value=0)
        self.res.init(res.OUT, value=0)
        self.cs.init(cs.OUT, value=1)

        self.cmd = bytearray(1)
        self.buffer = bytearray(self.width * self.height * 2)
        super().__init__(self.buffer, self.width, self.height, framebuf.RGB565_BE)
        self.init_display()

    def reset(self):
        self.res(1)
        sleep_ms(1)
        self.res(0)
        sleep_ms(10)
        self.res(1)

    def write_cmd(self, cmd, data=None):
        self.spi.init(baudrate=self.rate, polarity=0, phase=0)
        self.cs(1)
        self.dc(0)
        self.cs(0)
        self.cmd[0] = cmd
        self.spi.write(self.cmd)
        self.cs(1)
        if data:
            self.cs(1)
            self.dc(1)
            self.cs(0)
            self.spi.write(data)
            self.cs(1)

    def init_display(self):
        self.reset()
        self.write_cmd(CMD_COMMANDLOCK, bytes([0x12]))
        self.write_cmd(CMD_COMMANDLOCK, bytes([0xb1]))
        self.poweroff()
        self.write_cmd(CMD_CLOCKDIV, bytes([0xf0]))
        self.write_cmd(CMD_MUXRATIO, bytes([0x7f]))
        self.write_cmd(CMD_SETREMAP, bytes([0x74]))
        self.write_cmd(CMD_STARTLINE, bytes([0x00]))
        self.write_cmd(CMD_DISPLAYOFFSET, bytes([0x00]))
        self.write_cmd(CMD_SETGPIO, bytes([0x00]))
        self.write_cmd(CMD_FUNCTIONSELECT, bytes([0x01]))
        self.write_cmd(CMD_PRECHARGE, bytes([0x32]))
        self.write_cmd(CMD_PRECHARGELEVEL, bytes([0x1f]))
        self.write_cmd(CMD_VCOMH, bytes([0x05]))
        self.invert(False)
        self.write_cmd(CMD_CONTRASTABC, bytes([0xc8, 0x80, 0xc8]))
        self.write_cmd(CMD_CONTRASTMASTER, bytes([0x0f]))
        self.write_cmd(CMD_SETVSL, bytes([0xa0, 0xb5, 0x55]))
        self.write_cmd(CMD_PRECHARGE2, bytes([0x01]))
        self.poweron()
        self.fill(0)
        self.show()

    def poweroff(self):
        self.write_cmd(CMD_DISPLAYOFF)

    def poweron(self):
        self.write_cmd(CMD_DISPLAYON)

    def contrast(self, contrast):
        self.write_cmd(CMD_CONTRASTMASTER, bytes([(contrast >> 4) & 0xf]))

    def invert(self, invert):
        self.write_cmd(CMD_INVERTDISPLAY if invert else CMD_NORMALDISPLAY)

    def show(self):
        self.write_cmd(CMD_SETCOLUMN, bytes([0x00, self.width - 1]))
        self.write_cmd(CMD_SETROW, bytes([0x00, self.height - 1]))
        self.write_cmd(CMD_WRITERAM, self.buffer)
