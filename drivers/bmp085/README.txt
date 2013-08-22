BMP085 Driver (Pressure and Temperature Sensor)
===============================================

Overview
--------

This is the driver for the pressure and temperature sensor commonly found on
the WeatherCape expansion board for the BeagleBone.

Interface
---------

This driver implements the character device interface. It supports reading
through /dev/bmp085b{1,3}s77. When read from, it returns a string containing
a data label, a colon, and the sensor value.

Example output of `cat /dev/bmp085b3s77`:

TEMPERATURE     : 23.1
PRESSURE        : 69964

Temperature is expressed in Celsius (a.k.a. centigrade). The resolution is
0.1C.

Pressure is expressed in Pascals. Valid values are 30000 to 110000.
The resolution is 3 Pa.

Limitations
-----------

The measurement resolution is configurable in the chip, but this driver just
uses standard mode. It could probably be implemented with an ioctl() or by
passing an argument via the service command, but it doesn't seem too useful at
this time. See the data sheet for the trade-offs between conversion time,
power consumption, and resolution.

While only the BMP085 is supported at present, the BMP085's predecessor,
SMD500, should be easy to support in this driver with some small changes
to the calculations and coefficients.

As with the SHT21's temperature sensor, the BMP085's temperature sensor
appears to be heated a couple of degrees by the BeagleBone. After power-on,
the readings rise slightly and then level off a few degrees above room
temperature.

Testing the Code
----------------

Starting up an instance:

/bin/service up /usr/sbin/bmp085 -label bmp085.3.77 -dev /dev/bmp085b3s77 \
	-args 'bus=3 address=0x77'

Getting the sensor value:

cat /dev/bmp085b3s77

Killing an instance:

/bin/service down bmp085.3.77

