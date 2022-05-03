.. currentmodule:: machine

class :class:`DAC`
==================

samd
----

DAC (digital to analog conversion)
----------------------------------

The DAC class provides a fast digital to analog conversion. Usage example::

    from machine import DAC

    dac0 = DAC(0)                    # create DAC object on DAC pin A0
    dac0.write(1023)                 # write value, 0-4095 across voltage range 0.0v - 3.3v
    dac1 = DAC(1)                    # create DAC object on DAC pin A1
    dac1.write(2000)                 # write value, 0-4095 across voltage range 0.0v - 3.3v

The resolution of the DAC is 12 bit for SAMD51 and 10 bit for SAMD21. SAMD21 devices
have 1 DAC channel at GPIO PA02, SAMD51 devices have 2 DAC channels at GPIO PA02 and PA05.

DAC Constructor
```````````````

.. class:: DAC(id, *, vref=3)
  :noindex:

The vref arguments defines the output voltage range, the callback option is used for
dac_timed(). Suitable values for vref are:

==== ============================  ================================
vref SAMD21                        SAMD51
==== ============================  ================================
0    Internal voltage reference    Internal bandgap reference (~1V)
1    Analogue voltage supply       Analogue voltage supply
2    External reference            Unbuffered external reference
3    -                             Buffered external reference
==== ============================  ================================

DAC Methods
```````````

.. method:: write(value)

Write a single value to the selected DAC output. The value range is 0-1023 for
SAMD21 and 0-4095 for SAMD51. The voltage range depends on the vref setting.
