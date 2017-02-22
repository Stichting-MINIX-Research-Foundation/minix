TPS65217 Driver (Power Management IC)
=====================================

Overview
--------

This driver is for the power management chip commonly found on the BeagleBone
and the BeagleBone Black.

Testing the Code
----------------

Starting up an instance:

/sbin/minix-service up /usr/sbin/tps65217 -label tps65217.1.24 \
	-args 'bus=1 address=0x24'

Killing an instance:

/sbin/minix-service down tps65217.1.24
