                        _   _                   _        _   _             
    __      _____  __ _| |_| |__   ___ _ __ ___| |_ __ _| |_(_) ___  _ __  
    \ \ /\ / / _ \/ _` | __| '_ \ / _ \ '__/ __| __/ _` | __| |/ _ \| '_ \ 
     \ V  V /  __/ (_| | |_| | | |  __/ |  \__ \ || (_| | |_| | (_) | | | |
      \_/\_/ \___|\__,_|\__|_| |_|\___|_|  |___/\__\__,_|\__|_|\___/|_| |_|

                  Minix Demo for the BeagleBone Weather Cape
                Based on https://github.com/jadonk/bonescript


Overview
--------

This is a demo of the BeagleBone Weather Cape. It's a fork of the original
bonescript weatherstation demo, modified to work on the Minix operating
system. The main changes are conversion from nodejs to JSON and mbar to hPa.

Requirements
------------

Hardware:
	BeagleBone
	BeagleBone Weather Cape
	Network (doesn't have to be connected to the Internet)

Software:
	Web Browser with HTML5 support and JavaScript enabled.


Setup
-----

This demo is meant to work 'out of the box'. It requires the BeagleBone be
connected to the network and have the BeagleBone Weather cape attached at
boot.

	1) Attach the BeagleBone Weather Cape.
	2) Connect a network cable to the BeagleBone.
	3) Power on the BeagleBone.
	4) Configure the network by running these commands.
		# netconf
		# reboot
	5) Enter the BeagleBone's IP address into your web browser.

Usage
-----

This demo is a web application. You just need to point your web browser at
the BeagleBone's IP address to view a nice display of the sensor values. You
can get your BeagleBone's IP address by running `ifconfig` on the BeagleBone.
If the address is 192.168.12.138, you'd enter http://192.168.12.138/ into your
web browser.

