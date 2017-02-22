CAT24C256 Driver (EEPROM)
=========================

Overview
--------

This is the driver for the EEPROM chip commonly found on the BeagleBone
and the BeagleBone Black as well as capes and expansion boards.

Testing the Code
----------------

Starting up an instance:

/sbin/minix-service up /service/cat24c256 -dev /dev/eepromb1s50 -label cat24c256.1.50 -args 'bus=1 address=0x50'

