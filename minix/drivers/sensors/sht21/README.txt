SHT21 Driver (Relative Humidity and Temperature Sensor)
=======================================================

Overview
--------

This is the driver for the relative humidity and temperature sensor commonly
found on the WeatherCape expansion board for the BeagleBone.

Interface
---------

This driver implements the character device interface. It supports reading
through /dev/sht21b{1,3}s40. When read from, it returns a string containing
a data label, a colon, and the sensor value.

Example output of `cat /dev/sht21b3s40`:

TEMPERATURE     : 35.014
HUMIDITY        : 25.181

Temperature is expressed in Celsius (a.k.a. centigrade). Valid values are
-40.000 to 125.000.

Humidity is expressed as a percentage. Valid values are 0.000 to 100.000.

Limitations
-----------

Intense activity causes the chip to heat up, affecting the temperature reading.
In order to prevent the chip from self-heating more than 0.1C, the sensor
values will only be read once per second. Subsequent reads within the same
second will return cached temperature and humidity values.

The measurement resolution is configurable in the chip, but this driver just
uses the default maximum resolutions (12-bit for Humidity, 14-bit for
temperature). It could probably be implemented with an ioctl() or by passing
an argument via the minix-service command, but it doesn't seem too useful at
this time. See the data sheet for the trade-off between faster conversion time
and lower resolution.

In testing, the temperature sensor reported a value several degrees higher
than an indoor thermometer placed nearby. It doesn't appear to be a bug in the
driver as the Linux driver reports similar temperature. Additionally, the
BMP085 temperature sensor on the same cape reports a temperature about 2
degrees lower than the SHT21. This could be due to heat produced by the
BeagleBone heating the cape slightly or maybe just a bad chip on the test
board.

Testing the Code
----------------

The driver should have been started by a script in /etc/rc.capes/ If not,
this is how you start up an instance:

cd /dev && MAKEDEV sht21b3s40
/sbin/minix-service up /service/sht21 -label sht21.3.40 -dev /dev/sht21b3s40 \
	-args 'bus=3 address=0x40'

Getting the sensor value:

cat /dev/sht21b3s40

Killing an instance:

/sbin/minix-service down sht21.3.40

