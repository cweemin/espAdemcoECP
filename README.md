This is a re-implentation of Mark Kimsal ademco ecp decoder (https://github.com/markkimsal/homesecurity) to work on the 8266. It uses a modified 8266 software serial (https://github.com/plerup/espsoftwareserial) from Peter Lerup. 

Right now I am able to get the Read part completed. As for the write I am still trying to figure how to make it work using the ISR.

========
HARDWARE
========
To connect the hardware, you need 8266 chip (8266-12F prefably). One i2c-safe 2/4 channel bi-directional logic level converter (bss138). And a dc-dc buck converter (stable 3.3v output, low ripple and at least 800ma). 
![alt tag](https://raw.githubusercontent.com/cweemin/espAdemcoECP/master/8266_ademco_sketch.png)

======
CONFIG
======
config.h will have defines for WIFI password/ssid and PIN to use.

=====
USAGE
=====
Once uploaded and connected to the network, you should be able to telnet to it for more debug info. You can connect to the http end with your browser too. So far the zones are configured to my home configuration.

