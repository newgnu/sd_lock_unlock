# sd_lock_unlock
This utilizes the SD's built-in lock/unlock/erase features.
I built this using an arduino nano, but anything that supports the SPI and Serial libraries will probably work.

I built this during a caffiene and booze fueled haze over a weekend while trying to understand the SD's SPI protocol better. It's functional, but it ain't pretty. Everything's done manually in one file, and I tried to comment as coherently as possible.

The Nano uses 5v logic, but SD cards require 3.3v. Keep this in mind, and either use a logic level shifter or resistor voltage divider.

```
- N/C
- MISO (Nano D12)
- GND
- SCK  (Nano D13)
- VCC  (3.3v)
- GND  
- MOSI (Nano D11)
- CS   (Nano D10 - changeable in the code)
\ - N/C
```
