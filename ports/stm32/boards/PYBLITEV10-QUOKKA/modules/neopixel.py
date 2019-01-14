import machine
import quokka

class NeoPixel():
  def __init__(self, pin=None, num=8, brightness=100):
    if not pin:
      pin = machine.Pin('X12', machine.Pin.OUT)
    if isinstance(pin, quokka.QuokkaPin):
      pin = pin._pin
    pin.init(mode=machine.Pin.OUT)
    self._pin = pin
    self._pin.off()
    self._num = num
    self._buf = bytearray(num*3)
    self.brightness = brightness
    self.show()

  def set_brightness(self, brightness):
    self.brightness = brightness

  def _rainbow(h):
    # h is hue between 0-119.
    if h < 20:
      return (255, (h * 255) // 20, 0,)
    elif h < 40:
      return (((40-h) * 255) // 20, 255, 0,)
    elif h < 60:
      return (0, 255, ((h-40) * 255) // 20,)
    elif h < 80:
      return (0, ((80-h) * 255) // 20, 255,)
    elif h < 100:
      return (((h-80) * 255) // 20, 0, 255,)
    else:
      return (255, 0, ((120-h) * 255) // 20,)

  def set_pixel(self, n, r, g, b):
    if n < 0 or n >= self._num:
      return
    if self.brightness < 100:
      r = self.scale_colour(r)
      g = self.scale_colour(g)
      b = self.scale_colour(b)
    self._buf[n*3] = g
    self._buf[n*3+1] = r
    self._buf[n*3+2] = b

  def __setitem__(self, key, value):
    if isinstance(value, int):
      r, g, b = NeoPixel._rainbow(value)
      b = b * 3 // 2
      value = r, g, b
    if isinstance(key, int):
      self.set_pixel(key, value[0], value[1], value[2])
    else:
      for i in range(key.start, key.stop, key.step):
        self.set_pixel(i, value[0], value[1], value[2])

  def get_pixel(self, n):
    r = self._buf[n*3+1]
    g = self._buf[n*3]
    b = self._buf[n*3 + 2]
    return r, g, b

  def __getitem__(self, key):
    return self.get_pixel(key)

  def show(self):
    self._pin.neo(self._buf)

  def clear(self):
    self.all(0, 0, 0)

  def all(self, r, g, b):
    for i in range(self._num):
      self.set_pixel(i, r, g, b,)

  def scale_colour(self, colour):
    return int(colour*self.brightness/100)
