import framebuf_utils
import framebuf

# Emulate a display driver subclassed from framebuf.FrameBuffer
class Display(framebuf.FrameBuffer):
    def __init__(self):
        self.buf = bytearray(4 * 4 * 2)
        super().__init__(self.buf, 4, 4, framebuf.RGB565)

device = Display()

def foo():
    width = 2  # Glyph dimensions
    height = 2
    i = 0

    while True:
        buf = bytearray(width * height // 8 + 1)
        fbc = framebuf.FrameBuffer(buf, width, height, framebuf.MONO_HMSB)
        fbc.pixel(0, 0, 1)
        print(buf)

        framebuf_utils.render(framebuf.FrameBuffer, device, fbc, 1, 1, 0x5555, 0xaaaa)
        print(device.buf)
        print(device.pixel(0, 0))
        print(device.pixel(1, 1))
        print(device.pixel(2, 1))

        i += 1
        print(i)

    # width = 20  # Glyph dimensions
    # height = 40
    # i = 0
    # while True:
    #     # Source monochrome glyph
    #     buf = bytearray(width * height // 8)
    #     fbc = framebuf_r.FrameBuffer(buf, width, height, framebuf_r.MONO_HMSB)
    #     # Destination: temporary color frame buffer for the glyph
    #     bufd = bytearray(width * height * 2)
    #     fbd = framebuf_r.FrameBuffer(bufd, width, height, framebuf_r.RGB565)
    #     # Render it in specified colors to the temporary buffer
    #     framebuf_j.render(fbd, fbc, 0, 0, 0x5555, 0xaaaa)  # fbc must be a framebuf_r otherwise we get a TypeError
    #     # Instantiate a FrameBuffer pointed at color glyph
    #     # Cannot be a framebuf_r
    #     fbx = framebuf.FrameBuffer(bufd, width, height, framebuf.RGB565)
    #     device.blit(fbx, 0, 0)
    #     i += 1
    #     print(i)

foo()
