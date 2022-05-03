.. _guides_hardware_addressable_leds:

Addressable LEDs
================

NeoPixel and APA106 driver
--------------------------

Use the ``neopixel`` and ``apa106`` modules::

    from machine import Pin
    from neopixel import NeoPixel

    pin = Pin(0, Pin.OUT)   # set GPIO0 to output to drive NeoPixels
    np = NeoPixel(pin, 8)   # create NeoPixel driver on GPIO0 for 8 pixels
    np[0] = (255, 255, 255) # set the first pixel to white
    np.write()              # write data to all pixels
    r, g, b = np[0]         # get first pixel colour


The APA106 driver extends NeoPixel, but internally uses a different colour order::

    from apa106 import APA106
    ap = APA106(pin, 8)
    r, g, b = ap[0]

.. Warning::
    By default ``NeoPixel`` is configured to control the more popular *800kHz*
    units. It is possible to use alternative timing to control other (typically
    400kHz) devices by passing ``timing=0`` when constructing the
    ``NeoPixel`` object.

For low-level driving of a NeoPixel see `machine.bitstream`.
This low-level driver uses an RMT channel by default.  To configure this see
`RMT.bitstream_channel`.

APA102 (DotStar) uses a different driver as it has an additional clock pin.

ESP8266
-------

Use the ``apa102`` module::

    from machine import Pin
    from apa102 import APA102

    clock = Pin(14, Pin.OUT)     # set GPIO14 to output to drive the clock
    data = Pin(13, Pin.OUT)      # set GPIO13 to output to drive the data
    apa = APA102(clock, data, 8) # create APA102 driver on the clock and the data pin for 8 pixels
    apa[0] = (255, 255, 255, 31) # set the first pixel to white with a maximum brightness of 31
    apa.write()                  # write data to all pixels
    r, g, b, brightness = apa[0] # get first pixel colour

For low-level driving of an APA102::

    import esp
    esp.apa102_write(clock_pin, data_pin, rgbi_buf)


ESP8266 neo
-----------

Use the ``neopixel`` module::

    from machine import Pin
    from neopixel import NeoPixel

    pin = Pin(0, Pin.OUT)   # set GPIO0 to output to drive NeoPixels
    np = NeoPixel(pin, 8)   # create NeoPixel driver on GPIO0 for 8 pixels
    np[0] = (255, 255, 255) # set the first pixel to white
    np.write()              # write data to all pixels
    r, g, b = np[0]         # get first pixel colour

.. Warning::
    By default ``NeoPixel`` is configured to control the more popular *800kHz*
    units. It is possible to use alternative timing to control other (typically
    400kHz) devices by passing ``timing=0`` when constructing the
    ``NeoPixel`` object.

For low-level driving of a NeoPixel see `machine.bitstream`.


rp2
---


Use the ``neopixel`` and ``apa106`` modules::

    from machine import Pin
    from neopixel import NeoPixel

    pin = Pin(0, Pin.OUT)   # set GPIO0 to output to drive NeoPixels
    np = NeoPixel(pin, 8)   # create NeoPixel driver on GPIO0 for 8 pixels
    np[0] = (255, 255, 255) # set the first pixel to white
    np.write()              # write data to all pixels
    r, g, b = np[0]         # get first pixel colour


The APA106 driver extends NeoPixel, but internally uses a different colour order::

    from apa106 import APA106
    ap = APA106(pin, 8)
    r, g, b = ap[0]

APA102 (DotStar) uses a different driver as it has an additional clock pin.



esp8266 tut
-----------

APA102 LEDs, also known as DotStar LEDs, are individually addressable
full-colour RGB LEDs, generally in a string formation. They differ from
NeoPixels in that they require two pins to control - both a Clock and Data pin.
They can operate at a much higher data and PWM frequencies than NeoPixels and
are more suitable for persistence-of-vision effects.

To create an APA102 object do the following::

    >>> import machine, apa102
    >>> strip = apa102.APA102(machine.Pin(5), machine.Pin(4), 60)

This configures an 60 pixel APA102 strip with clock on GPIO5 and data on GPIO4.
You can adjust the pin numbers and the number of pixels to suit your needs.

The RGB colour data, as well as a brightness level, is sent to the APA102 in a
certain order.  Usually this is ``(Red, Green, Blue, Brightness)``.
If you are using one of the newer APA102C LEDs the green and blue are swapped,
so the order is ``(Red, Blue, Green, Brightness)``.
The APA102 has more of a square lens while the APA102C has more of a round one.
If you are using a APA102C strip and would prefer to provide colours in RGB
order instead of RBG, you can customise the tuple colour order like so::

    >>> strip.ORDER = (0, 2, 1, 3)

To set the colour of pixels use::

    >>> strip[0] = (255, 255, 255, 31) # set to white, full brightness
    >>> strip[1] = (255, 0, 0, 31) # set to red, full brightness
    >>> strip[2] = (0, 255, 0, 15) # set to green, half brightness
    >>> strip[3] = (0, 0, 255, 7)  # set to blue, quarter brightness

Use the ``write()`` method to output the colours to the LEDs::

    >>> strip.write()

Demonstration::

    import time
    import machine, apa102

    # 1M strip with 60 LEDs
    strip = apa102.APA102(machine.Pin(5), machine.Pin(4), 60)

    brightness = 1  # 0 is off, 1 is dim, 31 is max

    # Helper for converting 0-255 offset to a colour tuple
    def wheel(offset, brightness):
        # The colours are a transition r - g - b - back to r
        offset = 255 - offset
        if offset < 85:
            return (255 - offset * 3, 0, offset * 3, brightness)
        if offset < 170:
            offset -= 85
            return (0, offset * 3, 255 - offset * 3, brightness)
        offset -= 170
        return (offset * 3, 255 - offset * 3, 0, brightness)

    # Demo 1: RGB RGB RGB
    red = 0xff0000
    green = red >> 8
    blue = red >> 16
    for i in range(strip.n):
        colour = red >> (i % 3) * 8
        strip[i] = ((colour & red) >> 16, (colour & green) >> 8, (colour & blue), brightness)
    strip.write()

    # Demo 2: Show all colours of the rainbow
    for i in range(strip.n):
        strip[i] = wheel((i * 256 // strip.n) % 255, brightness)
    strip.write()

    # Demo 3: Fade all pixels together through rainbow colours, offset each pixel
    for r in range(5):
        for n in range(256):
            for i in range(strip.n):
                strip[i] = wheel(((i * 256 // strip.n) + n) & 255, brightness)
            strip.write()
        time.sleep_ms(25)

    # Demo 4: Same colour, different brightness levels
    for b in range(31,-1,-1):
        strip[0] = (255, 153, 0, b)
        strip.write()
        time.sleep_ms(250)

    # End: Turn off all the LEDs
    strip.fill((0, 0, 0, 0))
    strip.write()


NeoPixels, also known as WS2812 LEDs, are full-colour LEDs that are connected in
serial, are individually addressable, and can have their red, green and blue
components set between 0 and 255.  They require precise timing to control them
and there is a special neopixel module to do just this.

To create a NeoPixel object do the following::

    >>> import machine, neopixel
    >>> np = neopixel.NeoPixel(machine.Pin(4), 8)

This configures a NeoPixel strip on GPIO4 with 8 pixels.  You can adjust the
"4" (pin number) and the "8" (number of pixel) to suit your set up.

To set the colour of pixels use::

    >>> np[0] = (255, 0, 0) # set to red, full brightness
    >>> np[1] = (0, 128, 0) # set to green, half brightness
    >>> np[2] = (0, 0, 64)  # set to blue, quarter brightness

For LEDs with more than 3 colours, such as RGBW pixels or RGBY pixels, the
NeoPixel class takes a ``bpp`` parameter. To setup a NeoPixel object for an
RGBW Pixel, do the following::

    >>> import machine, neopixel
    >>> np = neopixel.NeoPixel(machine.Pin(4), 8, bpp=4)

In a 4-bpp mode, remember to use 4-tuples instead of 3-tuples to set the colour.
For example to set the first three pixels use::

    >>> np[0] = (255, 0, 0, 128) # Orange in an RGBY Setup
    >>> np[1] = (0, 255, 0, 128) # Yellow-green in an RGBY Setup
    >>> np[2] = (0, 0, 255, 128) # Green-blue in an RGBY Setup

Then use the ``write()`` method to output the colours to the LEDs::

    >>> np.write()

The following demo function makes a fancy show on the LEDs::

    import time

    def demo(np):
        n = np.n

        # cycle
        for i in range(4 * n):
            for j in range(n):
                np[j] = (0, 0, 0)
            np[i % n] = (255, 255, 255)
            np.write()
            time.sleep_ms(25)

        # bounce
        for i in range(4 * n):
            for j in range(n):
                np[j] = (0, 0, 128)
            if (i // n) % 2 == 0:
                np[i % n] = (0, 0, 0)
            else:
                np[n - 1 - (i % n)] = (0, 0, 0)
            np.write()
            time.sleep_ms(60)

        # fade in/out
        for i in range(0, 4 * 256, 8):
            for j in range(n):
                if (i // 256) % 2 == 0:
                    val = i & 0xff
                else:
                    val = 255 - (i & 0xff)
                np[j] = (val, 0, 0)
            np.write()

        # clear
        for i in range(n):
            np[i] = (0, 0, 0)
        np.write()

Execute it using::

    >>> demo(np)
