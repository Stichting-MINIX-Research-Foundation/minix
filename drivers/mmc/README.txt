This directory contains code to access MMC based devices.

It was created during the initial port of MINIX to the ARM platform. mmbclk
implements a normal MINIX block device. It uses the interfaces defined in
mmchost.h to perform it's operations.

mmchost_mmchs is the MMC host controller driver for the TI omap device range.
It contains the logic to access SD cards on that device.

 drivers/mmc
 |-- Makefile        (The makefile)
 |-- mmclog.h        (A simpel logging system)
 |-- omap_mmc.h      (TI Omap register definitions)
 |-- sdhcreg.h       (BSD headers for the MMC layer)
 |-- sdmmcreg.h      (BSD headers for the MMC layer)
 |-- mmcblk.c        (MINIX 3 block device)
 |-- mmchost.h       (interface between the block device and the MMC layer)
 |-- mmchost_mmchs.c (Driver to use on the ARM port/beagle) 
 '-- README.txt      (This file)


Future work:
============

* Creating a more generic MMC layer
The SD protocol is well defined and the imported the netbsd sdhcreg and
sdmmcreg headers will allow us to make the MMC interface more generic. We would
like  mmchost_mmchs to be split in a generic part and a specific part. 

* 8 bits access
The driver currently only reads data over 1 or 4 bits address lines. Adding support
for 4 or 8 bits(for movinands) mode is very welcome.

* DMA.
The OMAP driver allows the usage of DMA. Adding DMA support will increase the
performance of the driver

* Removal of the PRIVCTL call.
The MMC driver uses memory mapped IO to perform it's operations. On i386 it is
the pci driver that will grant access to certain piece of memory to a driver.
On the ARM port we lack pci and other self describing busses. In the current
driver the driver will itself grant itself access to the correct piece of
memory but this is unwanted behavior.  We currently as thinking about two
possible solutions. The first would be to add the memory ranges in system.conf.
The second would be to modify the PCI driver to grant access to memory based on
a fixed configuration. For example we could use the driver tree to perform
device discovery and granting access to devices.


* TODO removal
The driver contains (quite a few) TODO's where the code need to be improved.
