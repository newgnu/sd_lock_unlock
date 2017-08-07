# sd_lock_unlock
This utilizes the SD's built-in lock/unlock/erase features.
I built this using an arduino nano, but anything that supports the SPI and Serial libraries will probably work.

The Nano uses 5v logic, but SD cards require 3.3v. Keep this in mind, and either use a logic level shifter or resistor voltage divider.

```
- N/C
- MISO (Arduino D12)
- GND
- SCK  (Arduino D13)
- VCC  (3.3v)
- GND  
- MOSI (Arduino D11)
- CS   (Arduino D10 - changeable in the code)
\ - N/C
```
