.. _devices_stm32_specific:

Port-specific functionality
===========================

Internal LEDs
-------------

See :ref:`pyb.LED <pyb.LED>`. ::

    from pyb import LED

    led = LED(1) # 1=red, 2=green, 3=yellow, 4=blue
    led.toggle()
    led.on()
    led.off()

    # LEDs 3 and 4 support PWM intensity (0-255)
    LED(4).intensity()    # get intensity
    LED(4).intensity(128) # set intensity to half

Internal switch
---------------

See :ref:`pyb.Switch <pyb.Switch>`. ::

    from pyb import Switch

    sw = Switch()
    sw.value() # returns True or False
    sw.callback(lambda: pyb.LED(1).toggle())


Servo control
-------------

See :ref:`pyb.Servo <pyb.Servo>`. ::

    from pyb import Servo

    s1 = Servo(1) # servo on position 1 (X1, VIN, GND)
    s1.angle(45) # move to 45 degrees
    s1.angle(-60, 1500) # move to -60 degrees in 1500ms
    s1.speed(50) # for continuous rotation servos


External interrupts
-------------------

See :ref:`pyb.ExtInt <pyb.ExtInt>`. ::

    from pyb import Pin, ExtInt

    callback = lambda e: print("intr")
    ext = ExtInt(Pin('Y1'), ExtInt.IRQ_RISING, Pin.PULL_NONE, callback)


Internal accelerometer
----------------------

See :ref:`pyb.Accel <pyb.Accel>`. ::

    from pyb import Accel

    accel = Accel()
    print(accel.x(), accel.y(), accel.z(), accel.tilt())
