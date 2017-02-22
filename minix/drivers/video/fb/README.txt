Frame Buffer Driver
===================

Overview
--------

This is the driver for the frame buffer. Currently it only supports the
DM37XX (BeagleBoard-xM).

Testing the Code
----------------

Starting up an instance:

minix-service up /service/fb -dev /dev/fb0 -args edid.0=cat24c256.3.50

The arguments take the following form:

	edid.X=L where X is the frame buffer device (usually 0) and L is
	the service label of the service to perform the EDID reading. In
	the example above, it's the EEPROM with slave address 0x50 on
	the 3rd I2C bus. If you want to use the defaults and skip EDID
	reading, you may omit the arguments.

