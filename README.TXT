Computer for development
========================

Arduino IDE
-----------

Versions 1.X.X are tested.
https://www.arduino.cc/en/software

USB-Enhanced-SERIAL CH9102 COM port driver
------------------------------------------

Microsoft Windows
https://github.com/Xinyuan-LilyGO/CH9102_Driver

macOS
https://github.com/WCHSoftGroup/ch34xser_macos

Libraries used
==============

Additional Board Manager URLs:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json

Crypto
------

For older version before version 0.4.0:
	Edit libraries/Crypto/src/AES.h
		remove the usage of "CRYPTO_AES_ESP32"
		always use "CRYPTO_AES_DEFAULT"

NTPClient
---------

Synchronize real-time clock from NTP server.

IDE version does not work.
It should be installed from github.
https://github.com/arduino-libraries/NTPClient.git

ESP32Time
---------

Handle date-time using internal clock

LoRa
----

Driver of LoRa

Adafruit SSD1306
----------------

Driver of SSD1306 OLED display

Grove - High Precision RTC
--------------------------

Driver of PCF85063TP Real-time clock

RTClib
------

Driver of DS1307 and DS3231 real-time clocks

OneWire
-------

Driver of 1-wire interface

It is used by dallas temperature sensor.

DallasTemperature
-----------------

Dallas 1-wire temperature sensor

Adafruit BME280 Library
-----------------------

BME280 I2C temperature, pressure and humidity sensor

Adafruit LTR390 Library
-----------------------

LTR390 I2C ambient/ultraviolet light sensor

DFRobot_MAX17043
----------------

MAX17043 battery gauge

Adafruit LC709203F
------------------

LC709203F battery gauge

CONFIGURATION
=============

Create "config.h" in the root directory of repository.
Define or undefine variables for configuration.
The file "config.h.example" is an example of "config.h".

NDEBUG
------

Skip debug codes.

Type: defined or undefined

DEVICE_ID
---------

ID of the arduino device

Each device should have a unique ID.

Type: Single byte integer
Values:
	0
		LoRa-WiFi gateway (optional measurement)
	1-255
		terminal measurement devices

NUMBER_OF_DEVICES
-----------------

Number of devices, including all gateway and measurment devices

Values: 1-256

ENABLE_MEASURE
--------------

Measure data

Type: defined or undefined

Should be defined for devices 1-255, and optional for device 0.

ENABLE_LED
----------

Blink on board LED on error

Type: defined or undefined

ENABLE_COM_OUTPUT
-----------------

Output debug message to USB serial port

Type: defined or undefined

ENABLE_OLED_OUTPUT
------------------

Output message to OLED display

Type: defined or undefined

ENABLE_CLOCK
------------

Grove high precision RTC is used

Values:
	(undefined)
		Use internal clock only
	CLOCK_PCF85063TP
		Grove high precision real-time clock
	CLOCK_DS1307
		DS1307 real-time clock
	CLOCK_DS3231
		DS3231 real-time clock

ENABLE_SD_CARD
--------------

SD card is installed and used to store measurement data

Type: defined or undefined

ENABLE_SLEEP
------------

Go to sleep mode when it is not sending or measuring data

Type: defined or undefined

ENABLE_BATTERY_GAUGE
--------------------

Battery gauge is used to monitor battery usage

Type: undefined or BATTERY_GAUGE_DFROBOT or BATTERY_GAUGE_LC709203F

ENABLE_DALLAS
-------------

Dallas thermometer is used

Type: defined or undefined

ENABLE_BME280
-------------

BME280 (temperature, pressure, humidity) sensor is used

Type: defined or undefined

ENABLE_LTR390
-------------

LTR390 I2C sensor is used

Type: defined or undefined

WIFI_SSID and WIFI_PASS
-----------------------

WiFi access point and password

Type: C strings

HTTP_UPLOAD_FORMAT
---------------

Printf format string of URL to upload data

Type: C string

Position arguments:
	%1$u
		device ID
	%2$lu
		serial number of transmission
	%3$s
		date and time in ISO8601 format
	%4$f, %5$f, %6$f, ...
		measure data, orderred by
			Dallas temperature
			BME280 temperature
			BME280 pressure
			BME280 humidity
			LTR390 ultraviolet
		data can be missing if corresponding ENABLE_* is not defined

HTTP_UPLOAD_LENGTH
---------------

Maximum length of HTTP URL string

SECRET_KEY
----------

Secret key for encryption of LoRa communication

Need to be same for both gateway and terminal sides.

Type: string or byte array with 16-byte size

DATA_FILE_PATH
-------------

File name in SD card to store data in terminal device

Type: C string

CLEANUP_FILE_PATH
-------------

Temporary file name in SD card for clean up data file

Type: C string

ACK_TIMEOUT
-----------

Milliseconds to wait for ACK response

Type: natural number

RESEND_TIMES
------------

Number of times to resend data if ACK is not received

Type: natural number

UPLOAD_INTERVAL
---------------

Maximum period in milliseconds to upload data if SD card is used

This value should be larger than (ACK_TIMEOUT * (RESEND_TIMES + 1)).

Type: natural number
Default: (ACK_TIMEOUT * (RESEND_TIMES + 2))

CLEANLOG_INTERVAL
-----------------

Period in milliseconds to clear data file on SD card

It can be undefined or set to zero to disable this function.

Type: undefined or natural number

MEASURE_INTERVAL
--------------

Period in milliseconds to measure data

This value should be larger than UPLOAD_INTERVAL.

Type: natural number

ROUTER_TOPOLOGY
---------------

Setting of LoRa message routing

Circular routing should be avoided.

Type: array of pairs of device IDs (upstream device, downstream device).

Example: {{2, 1}, {0, 1}, {2, 4}, {4, 3}}

The topology will be:

     -------- [1]
    /        /
   /        /
[0] --- [2]
           \
            \
             [4] --- [3]

Device 1 sends data through device 2 or directly sends to device 0 (gateway device).
Device 4 only sends data through device 2, and device 3 only send through device 4.
Devices send only to device 0 by default if not listed.

SLEEP_MARGIN
------------

Number of milliseconds to wake up before measuring or sending data

Type: natural number

CPU_FREQUENCY
-------------

Change CPU frequency to given value

Type: undefined or 240 or 160 or 80 or 40 or 20

LoRa works only on CPU frequency equal or greater than 20Hz.
WiFi needs 80Hz or higher.

COM_BAUD
--------

Bit rate of USB serial port

Type: positive number

OLED_WIDTH and OLED_HEIGHT
--------------------------

The dimension of OLED display

Type: positive numbers

OLED_ROTATION
-------------

The rotation of the contents on OLED display

Type: integer between in [0, 3]

DALLAS_PIN
----------

Pin number of thermometer data wire

Type: natural number

LORA_BAND
---------

Radio frequency used for LoRa

Type: signed long int
