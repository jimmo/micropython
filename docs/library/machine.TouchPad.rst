class :class:`TouchPad`
=======================

Use the ``TouchPad`` class in the :mod:`machine` module::

    from machine import TouchPad, Pin

    t = TouchPad(Pin(14))
    t.read()              # Returns a smaller number when touched

``TouchPad.read`` returns a value relative to the capacitive variation. Small numbers (typically in
the *tens*) are common when a pin is touched, larger numbers (above *one thousand*) when
no touch is present. However the values are *relative* and can vary depending on the board
and surrounding composition so some calibration may be required.

There are ten capacitive touch-enabled pins that can be used on the ESP32: 0, 2, 4, 12, 13
14, 15, 27, 32, 33. Trying to assign to any other pins will result in a ``ValueError``.

Note that TouchPads can be used to wake an ESP32 from sleep::

    import machine
    from machine import TouchPad, Pin
    import esp32

    t = TouchPad(Pin(14))
    t.config(500)               # configure the threshold at which the pin is considered touched
    esp32.wake_on_touch(True)
    machine.lightsleep()        # put the MCU to sleep until a touchpad is touched

For more details on touchpads refer to `Espressif Touch Sensor
<https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/touch_pad.html>`_.

