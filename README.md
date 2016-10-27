# Nano_WebRelay8
An Android sketch which provides a small webserver to control releays connected to your Arduino Nano with ATmega328.
This sketch provides a small webserver to control releays connected to your Arduino Nano with ATmega328.

# What the sketch does
It is a small web server which listens to http requests to (de-)activate a certain relay. 

This sketch cannot be directly used by Homeautomation software like OpenHab (because of the handling of POST requests.
So take a look at the [OpenHab](https://github.com/iwanzarembo/Nano_WebRelay8/tree/openhab) branch if you want to use 
it with OpenHab. 

# Features
- Initial GET request returns a JSON array with the current status of the relay.  
  e.g.  {"r":[0,0,1,0,0,0,0,0]}  
  -> 0 means the relay is turned off  
  -> 1 means the relay is turned on  
  Due to performance reasons only the status is returned. But you can see that the first entry is for the first relay, the second for the second and so on.
- Flag to enable the OpenHab specific return JSON which looks like {"r":["OFF","OFF","OFF","OFF","OFF","OFF","OFF","OFF"]}  
  Just un-comment the OPENHAB define flag
- Flag to use DHCP or hard coded IP address. Just un-comment the DHCP flag
- Only POST requests can change the status of the relays.  
  The request data must follow the pattern: relay number = 0 or 1 or 2 => e.g. 0=1&1=2&2=1&3=0&4=1&5=1&6=1&7=1  
  -> The relay number can only be an integer between 0 and 7  
  -> 0 turns the relay off  
  -> 1 turns the relay on  
  -> 2 changes the status of the relay. So a turned off relay would be switched on and the other way around  
  OpenHab is sending the POST data in the URL, so this sketch is checking if the POST data is in the URL and if
  yes, then it will be used and the real POST data is ignored.
- A GET request to /about will show a JSON with the version of the sketch running on the board.  
  e.g.  {"version" : "0.2.1" }
  
Additionally it is implementing a little bit of security like the maximum request size (default 
set to 480 bytes).

# Hardware and Libraries used
- An Ethernet Shield with the ENC28J60 chip, which requires the library  
  See https://github.com/ntruchsess/arduino_uip or http://playground.arduino.cc/Hardware/ArduinoEthernet
- An 8 Kanal 5V Relay Module 

# Error messages
The application only returns error codes and a HTTP 500 error to save the already very limited memory. 
- 1 = The request is too big, please send one with less bytes or check the maxSize constant to increase the value.
- 2 = Only GET and POST is supported, but something else has been received.
- 3 = The command must be at least 3 chars long! e.g. 0=1
- 4 = The application only supports relay numbers between 0 and the value in numRelays. Change it if you have more relays.
- 5 = The application only supports the values 0 for off, 1 for on and 2 to invert the status, everything else will not work!
- 6 = The POST data does not follow the required pattern, see the description in the features section.

Found a bug? Please create an issue at https://github.com/iwanzarembo/Nano_WebRelay8

# Version History
- 0.2 2015/01/04
  * First version with usage of Strings
- 0.3 2015/01/21
  * Enhanced code without usage of strings
  * Additional error handling
  * Reduced the maximum request size
- 0.3.OPENHAB 2016/06/08
  * Return a JSON String which can be directly used by OpenHab
  * Read query string also in POST request (in that case the real post data is ignored!)
- 1.0.0 2016/10/28
  * Add a flag to return OpenHab JSON string or a minified one
  * Add a flag to use DHCP address or hard coded IP
  * POST data can now be part of the URL e.g. curl http://arduino/?1=2 --data ""
  
  
