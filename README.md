# OilHeater
OilHeater is a project to heat up the oil pan of an engine to reduce fuel consumption and time needed to heat up the engine
## How it works
At the start of the two boards, they will wait about 5min (this time is depending on much long will it take for my WIFI router to start), when the time passes the arduino will send a handshake that it waits for data from the ESP. The second board will take from a NTP server the current datetime and send it as epochTime to the arduino. Then the arduino will convert it to Julian date, so it will count time by it self. On 01:00 the arduino will check the temperature that is outside by a NTC thermistor. The given value from this sensor will be used in calculating when should the heater start working so that the engine is warmed up at a certain time.

## TODO:
* handle cases when one board will fail to start
* get correct formula of time needed to heat up the oil pan
* create webpage and server with api to control the boards remotely 
