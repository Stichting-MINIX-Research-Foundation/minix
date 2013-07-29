TDA19988 Driver (HDMI Framer)
=============================

Overview
--------

This is the driver for the HDMI Framer chip commonly found on the BeagleBone
Black.

Interface
---------

To make things easy for the frame buffer driver, a block device driver
interface is provided. Read requests cause the TDA19988 driver to read
the EDID and return the data.

Documentation
-------------

The documentation available is rather thin. NXP doesn't provide a
datasheet on their website and they did not respond to my request
for a datasheet. There are a few other sources of information:

 * TDA9983B.pdf - this chip is similar but not the same. Full
 register descriptions are provided. This was suggested to me by
 the author of the Linux driver and someone at BeagleBoard.org as
 neither of them were able to get a full datasheet for the TDA19988.

 * TDA19988.pdf - you can probably find this on the net. It's a
 pre-production draft of the datasheet for the TDA19988. It has
 some information about how things work, but it doesn't give details
 on the registers.

 * LPC4350_FPU_TFT_HDMI-v2.0/Driver/tda19988.c - there's some
 example code from NXP for a Hitex LPC4350 REV A5 which includes
 a BSD licensed driver from NXP. It isn't complete as it only does
 test mode, put-through mode, and EDID reading. Some of the comments
 in the code make it seem like it isn't totally working/tested.

 * linux/drivers/gpu/drm/i2c/tda998x_drv.c - there is a Linux driver
 which implements a lot of functionality. As is always the case,
 great care has to be taken to only study the code to learn the
 functionality of the chip and not reproduce/copy the code.

Limitations
-----------

Currently, only the EDID reading functionality is implemented.

Testing the Code
----------------

Starting up an instance:

/bin/service up /usr/sbin/tda19988 -label tda19988.1.3470 \
	-args 'cec_bus=1 cec_address=0x34 hdmi_bus=1 hdmi_address=0x70'

Killing an instance:

/bin/service down tda19988.1.3470

The driver is meant to be accessed from other drivers using the block
device protocol, so it doesn't have a reserved major number and device file.
However, if you want a simple test from user space, you can create a temporary
device file to read the EDID like this:

cd /dev
mknod tda19988 b 32 0
chmod 600 tda19988
/bin/service up /usr/sbin/tda19988 -label tda19988.1.3470 \
	-dev /dev/tda19988 \
	-args 'cec_bus=1 cec_address=0x34 hdmi_bus=1 hdmi_address=0x70'
dd if=/dev/tda19988 of=/root/edid.dat count=1 bs=128
/bin/service down tda19988.1.3470
hexdump -C /root/edid.dat
rm tda19988

The hexdump should begin with the EDID magic number: 00 ff ff ff ff ff ff 00

