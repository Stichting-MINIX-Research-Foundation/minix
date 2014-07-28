TPS65217 Driver (Power Management IC)
=====================================

Overview
--------

This driver is for the power management chip commonly found on the BeagleBone
and the BeagleBone Black.

Testing the Code
----------------

Starting up an instance:

/bin/service up /usr/sbin/tps65217 -label tps65217.1.24 \
	-args 'bus=1 address=0x24'

Killing an instance:

/bin/service down tps65217.1.24
