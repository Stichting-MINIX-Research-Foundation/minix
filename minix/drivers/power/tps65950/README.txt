TPS65950 Driver (Power Management IC)
=====================================

Overview
--------

This driver is for the power management chip commonly found on the
BeagleBoard-xM.

Limitations
-----------

The TPS65950 has a pin labelled MSECURE which provides a form of write
protection. Depending on the pin's state (high or low), writing to certain
registers is disabled or enabled. The pin is driven by a GPIO on the SoC. 
There isn't a good way to access that pin on the SoC yet, so the PMIC
is always in the default insecure mode. It isn't really insecure as this
driver is the only one that the i2c bus driver will allow to access the
PMIC.

This is a huge chip. It has two I2C controllers, one with 4 slave addresses
hosting 256 registers per address. The TRM is over 900 pages. Given the
limited usefulness of some peripherals on the chip, not everything is
implemented.

Testing the Code
----------------

Starting up an instance:

/sbin/minix-service up /service/tps65950 -label tps65950.1.48 \
	-args 'bus=1 address=0x48'

Killing an instance:

/sbin/minix-service down tps65950.1.48
