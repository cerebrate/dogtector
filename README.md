# dogtector
Arduino code for the Dogtector, a passive IR sensor coupled to an ESP8266 along with a piezo buzzer.

The function of the Dogtector is to detect when one of our dogs approaches the back door of the house to be let
in, and then both sound a local alert (a beep from the buzzer), and send a notification that they have been
detected there to our home automation server via MQTT (using the topic _dogtector/alert_ ). The home
automation server is also able to enable and disable the Dogtector remotely using messages sent to the topic
_dogtector/command_ , and request enable status, which will be returned on the topic _dogtector/status_ .

The Dogtector also includes a (magnetic reed switch) door sensor, which sends the current open/closed status of
the door to the home automation server via the topic _dogtector/door_ . (While usable as a simple door monitor,
our automation server uses this information to _not_ send Dogtector notifications when the door is open, because
there's no need to let us know when we've just let the dogs out...)

The Dogtector also also makes use of the LEDs built onto the ESP8266. The red LED (pin 0) indicates current status,
illuminating steadily when the Dogtector is on but disabled, and flashing when the Dogtector is active. When active,
the blue LED (pin 2) illuminates when a detection has taken place.

The code for the Dogtector is freely available under the MIT License, should you happen to want it.
