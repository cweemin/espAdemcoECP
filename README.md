This is a re-implentation of Mark Kimsal ademco ecp decoder (https://github.com/markkimsal/homesecurity) to work on the 8266. It uses a modified 8266 software serial (https://github.com/plerup/espsoftwareserial) from Peter Lerup.

Right now I am able to get the Read part completed. As for the write I am still trying to figure how to make it work using the ISR.

========
HARDWARE
========
To connect the hardware, you need 8266 chip (8266-12F prefably). One i2c-safe 2/4 channel bi-directional logic level converter (bss138). And a dc-dc buck converter (stable 3.3v output, low ripple and at least 800ma).

            - |BUCK---3.3V--------------------
            |                                |
            |                                |
__________  v     _________________________  v _______________
|P    12V |------>|HV      I2C           LV|---|Vcc    8266   |
|A    TX  |------>|HVCH1   Logic      LVCH1|-->|GPIO5         |
|N    RX  |<------|HVCH2   Converter  LVCH2|<--|GPIO4         |
|E    GND |------>|GND                GND  |-->|GND           |
|L        |

======
CONFIG
======
config.h will have defines for WIFI password/ssid and PIN to use.


