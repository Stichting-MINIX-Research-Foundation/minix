TSL2550 Driver (Ambient Light Sensor)
=====================================

Overview
--------

This is the driver for the ambient light sensor commonly found on the
WeatherCape expansion board for the BeagleBone.

Interface
---------

This driver implements the character device interface. It supports reading
through /dev/tsl2550b{1,3}s39. When read from, it returns a string containing
a data label, a colon, and the sensor value.

Example output of `cat /dev/tsl2550b3s39`:

ILLUMINANCE     : 830

Illuminance is expressed in lux. Valid values are 0 to 1846.

Limitations
-----------

Extended mode isn't implemented. Normal mode should be sufficient for most
applications.

Testing the Code
----------------

The driver should have been started by a script in /etc/rc.capes/ If not,
this is how you start up an instance:

cd /dev && MAKEDEV tsl2550b3s39
/sbin/minix-service up /service/tsl2550 -label tsl2550.3.39 \
	-dev /dev/tsl2550b3s39 -args 'bus=3 address=0x39'

Getting the sensor value:

cat /dev/tsl2550b3s39

Killing an instance:

/sbin/minix-service down tsl2550.3.39

