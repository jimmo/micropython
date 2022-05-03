.. _devices_esp32_specific:

Port-specific functionality
===========================

esp & esp32 modules
-------------------

The ESP32 port includes some features that are specific to the ESP32 and are not
available on other ports. They are available in the :mod:`esp` and :mod:`esp32`
modules.

The :mod:`esp` module::

    import esp

    esp.osdebug(None)       # turn off vendor O/S debugging messages
    esp.osdebug(0)          # redirect vendor O/S debugging messages to UART(0)

    # low level methods to interact with flash storage
    esp.flash_size()
    esp.flash_user_start()
    esp.flash_erase(sector_no)
    esp.flash_write(byte_offset, buffer)
    esp.flash_read(byte_offset, buffer)

The :mod:`esp32` module::

    import esp32

    esp32.hall_sensor()     # read the internal hall sensor
    esp32.raw_temperature() # read the internal temperature of the MCU, in Fahrenheit
    esp32.ULP()             # access to the Ultra-Low-Power Co-processor

Note that the temperature sensor in the ESP32 will typically read higher than
ambient due to the IC getting warm while it runs.  This effect can be minimised
by reading the temperature sensor immediately after waking up from sleep.

Remote Control Transceiver (RMT)
--------------------------------

The RMT allows generation of accurate digital pulses with 12.5ns resolution.
See :ref:`esp32.RMT <esp32.RMT>` for details.  Usage is::

    import esp32
    from machine import Pin

    r = esp32.RMT(0, pin=Pin(18), clock_div=8)
    r   # RMT(channel=0, pin=18, source_freq=80000000, clock_div=8)
    # The channel resolution is 100ns (1/(source_freq/clock_div)).
    r.write_pulses((1, 20, 2, 40), 0) # Send 0 for 100ns, 1 for 2000ns, 0 for 200ns, 1 for 4000ns


Accessing peripherals directly via registers
--------------------------------------------

The ESP32's peripherals can be controlled via direct register reads and writes.
This requires reading the datasheet to know what registers to use and what
values to write to them.  The following example shows how to turn on and change
the prescaler of the MCPWM0 peripheral.

.. code-block:: python3

    from micropython import const
    from machine import mem32

    # Define the register addresses that will be used.
    DR_REG_DPORT_BASE = const(0x3FF00000)
    DPORT_PERIP_CLK_EN_REG = const(DR_REG_DPORT_BASE + 0x0C0)
    DPORT_PERIP_RST_EN_REG = const(DR_REG_DPORT_BASE + 0x0C4)
    DPORT_PWM0_CLK_EN = const(1 << 17)
    MCPWM0 = const(0x3FF5E000)
    MCPWM1 = const(0x3FF6C000)

    # Enable CLK and disable RST.
    print(hex(mem32[DPORT_PERIP_CLK_EN_REG] & 0xffffffff))
    print(hex(mem32[DPORT_PERIP_RST_EN_REG] & 0xffffffff))
    mem32[DPORT_PERIP_CLK_EN_REG] |= DPORT_PWM0_CLK_EN
    mem32[DPORT_PERIP_RST_EN_REG] &= ~DPORT_PWM0_CLK_EN
    print(hex(mem32[DPORT_PERIP_CLK_EN_REG] & 0xffffffff))
    print(hex(mem32[DPORT_PERIP_RST_EN_REG] & 0xffffffff))

    # Change the MCPWM0 prescaler.
    print(hex(mem32[MCPWM0])) # read PWM_CLK_CFG_REG (reset value = 0)
    mem32[MCPWM0] = 0x55      # change PWM_CLK_PRESCALE
    print(hex(mem32[MCPWM0])) # read PWM_CLK_CFG_REG

Note that before a peripheral can be used its clock must be enabled and it must
be taken out of reset.  In the above example the following registers are used
for this:

- ``DPORT_PERI_CLK_EN_REG``: used to enable a peripheral clock

- ``DPORT_PERI_RST_EN_REG``: used to reset (or take out of reset) a peripheral

The MCPWM0 peripheral is in bit position 17 of the above two registers, hence
the value of ``DPORT_PWM0_CLK_EN``.
