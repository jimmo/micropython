import neopixel
_neopixels = neopixel.NeoPixel()
_neopixels.clear()
_neopixels.show()
_neopixels = None


import framebuf
import pyb
import machine
import time
import zlib


class QuokkaButton():
  def __init__(self, pin):
    self._pin = pin
    self._last = False
    self._was = False

  def _tick(self):
    # Update was_pressed state.
    if self.is_pressed() and not self._last:
      self._was = True
    self._last = self.is_pressed()

  def was_pressed(self):
    r = self._was
    self._was = False
    return r

  def is_pressed(self):
    return not self._pin.value()


class QuokkaPin():
  def __init__(self, name):
    self._name = name
    self._mode_input()

  def _mode_input(self):
    self._pin = machine.Pin(self._name, machine.Pin.IN, machine.Pin.PULL_NONE)

  def _mode_output(self):
    self._pin = machine.Pin(self._name, machine.Pin.OUT)

  def _mode_pwm(self):
    self._pin = pyb.Pin(self._name)
    if self._name == 'X4':
      self._channel = pyb.Timer(2, freq=1000).channel(4, pyb.Timer.PWM, pin=self._pin)
    elif self._name == 'X3':
      self._channel = pyb.Timer(2, freq=1000).channel(3, pyb.Timer.PWM, pin=self._pin)
    elif self._name == 'X2':
      self._channel = pyb.Timer(2, freq=1000).channel(2, pyb.Timer.PWM, pin=self._pin)
    elif self._name == 'X1':
      self._channel = pyb.Timer(2, freq=1000).channel(1, pyb.Timer.PWM, pin=self._pin)
    elif self._name == 'Y12':
      self._channel = pyb.Timer(1, freq=1000).channel(3, pyb.Timer.PWM, pin=self._pin)
    else:
      raise ValueError('PWM not on pin')

  def on(self):
    self._mode_output()
    self._pin.on()

  def off(self):
    self._mode_output()
    self._pin.off()

  def toggle(self):
    self._mode_output()
    self._pin.toggle()

  def write_digital(self, b):
    self._mode_output()
    self._pin.value(1 if b else 0)

  def read_digital(self):
    self._mode_input()
    return self._pin.value()

  def write_analog(self, percent):
    self._mode_pwm()
    self._channel.pulse_width_percent(percent)


class QuokkaPinAnalogIn(QuokkaPin):
  def __init__(self, name):
    super().__init__(name)

  def _mode_analog(self):
    self._mode_input()
    self._adc = pyb.ADC(self._name)

  def read_analog(self):
    self._mode_analog()
    return self._adc.read()


class QuokkaPinDac(QuokkaPinAnalogIn):
  def __init__(self, name):
    super().__init__(name)

  def _mode_dac(self):
    self._mode_output()

  def write_dac(self, v):
    return


class QuokkaGrove():
  def __init__(self, p0, p1, analog=False):
    if analog:
      if p0 == 'X5':
        self.pin0 = QuokkaPinDac(p0)
      else:
        self.pin0 = QuokkaPinAnalogIn(p0)
      self.pin1 = QuokkaPinAnalogIn(p1)
    else:
      self.pin0 = QuokkaPin(p0)
      self.pin1 = QuokkaPin(p1)


class QuokkaGroves():
  def __init__(self):
    self.all = (self.a, self.b, self.c, self.d, self.e, self.f,)

    
red = pyb.LED(1)
green = pyb.LED(2)
orange = pyb.LED(3)
blue = pyb.LED(4)
leds = (red, orange, green, blue,)

def clear_leds(self):
  for l in leds:
    l.off()


button_a = QuokkaButton(machine.Pin('X18'))
button_c = QuokkaButton(machine.Pin('X19'))
button_d = QuokkaButton(machine.Pin('X20'))
button_b = QuokkaButton(machine.Pin('X21'))
button_usr = QuokkaButton(machine.Pin('X17'))
buttons = (button_a, button_b, button_c, button_d, button_usr,)


grove_a = QuokkaGrove('X9', 'X10') # I2C SCL/SDA
grove_b = QuokkaGrove('Y2', 'Y1')  # UART 6
grove_c = QuokkaGrove('X4', 'X3', analog=True)  # ADC, UART 2
grove_d = QuokkaGrove('X6', 'X8')  # SPI CLK/MOSI
grove_e = QuokkaGrove('X2', 'X1', analog=True)  # ADC, UART 4
grove_f = QuokkaGrove('X5', 'Y12', analog=True) # ADC, DAC pin 0
    

sleep = time.sleep_ms
sleep_us = time.sleep_us


_internal_spi = machine.SPI('Y', baudrate=2000000)
spi = machine.SPI('X')
i2c = machine.I2C('X')

import drivers

class QuokkaDisplay(drivers.SSD1306_SPI):
  def __init__(self, spi):
    super().__init__(128, 64, spi, machine.Pin('X11', machine.Pin.OUT), machine.Pin('X22', machine.Pin.OUT), machine.Pin('Y5', machine.Pin.OUT), external_vcc=True)

    self.clear()

  def clear(self):
    self.text_x = 0
    self.text_y = 0
    self.fill(0)
    self.show()

  def print(self, *args, color=1, end='\n'):
    text = ' '.join(str(x) for x in args) + end
    for c in text:
      if self.text_x >= 128 or c == '\n':
        self.text_x = 0
        self.text_y += 8
      if self.text_y >= 64:
        self.scroll(0, -8)
        self.fill_rect(0, 56, 128, 8, 0)
        self.text_y -= 8
      if c != '\n':
        self.text(c, self.text_x, self.text_y, color)
        self.text_x += 8
    self.show()

  def text(self, text, x=0, y=0, color=1, scale=1):
    if scale == 1:
      super().text(text, x, y, color)
    else:
      buf = bytearray(len(text) * 8 * scale * scale)
      fb = framebuf.FrameBuffer(buf, len(text) * 8 * scale, 8 * scale, framebuf.MONO_VLSB)
      bg = 1 - color
      fb.fill(bg)
      fb.text(text, 0, 0, color)
      self.scale_blit(fb, x, y, scale, bg)

  def scale_blit(self, fb, x, y, scale=1, key=0):
    if scale == 1:
      self.blit(fb, x, y, key)
    else:
      w = 0
      h = 0
      for xx in range(self.width):
        if fb.pixel(xx, 0) is None:
          break
        w = xx
      for yy in range(self.height):
        if fb.pixel(0, yy) is None:
          break
        h = yy
      print(w, h)
      
      for xx in range(w):
        for yy in range(h):
          c = fb.pixel(xx, yy)
          if c != key:
            self.fill_rect(x + xx * scale, y + yy * scale, scale, scale, c)


class Image:
  @staticmethod
  def load(filename, compressed=None):
    if compressed is None:
      compressed = filename.endswith('.qimz')
    with open(filename, 'rb') as f:
      wh = f.read(2)
      width = int(wh[0])
      height = int(wh[1])
      im_buf = bytearray(f.read())
    if compressed:
      im_buf = zlib.decompress(im_buf)
    return framebuf.FrameBuffer(im_buf, width, height, framebuf.MONO_HLSB)


display = QuokkaDisplay(_internal_spi)

_imu = drivers.MPU9250('Y')

accelerometer = _imu.accel
compass = _imu.mag
gyro = _imu.gyro

# Mirror a few micro:bit functions.
def temperature():
  return _imu.temperature

def running_time():
  return pyb.millis()

def _on_tick(t):
  for b in buttons:
    b._tick()

# Currently used for button was_pressed.
timer_tick = pyb.Timer(1, freq=200, callback=_on_tick)

__all__ = ['display', 'button_a', 'button_b', 'button_c', 'button_d', 'button_usr', 'buttons', 'grove_a', 'grove_b', 'grove_c', 'grove_d', 'grove_e', 'grove_f', 'red', 'orange', 'green', 'blue', 'leds', 'clear_leds', 'sleep', 'sleep_us', 'temperature', 'running_time', 'accelerometer', 'gyro', 'compass', 'Image']
