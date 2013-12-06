# Makefile for all device drivers.
#

.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "yes"

.if ${MACHINE_ARCH} == "i386"
SUBDIR=	at_wini floppy pci pckbd
.endif

.if ${MACHINE_ARCH} == "earm"
SUBDIR= 
.endif

# Drivers available on all platforms
SUBDIR+= tty

.else # ${MKIMAGEONLY} != "yes"

.if ${MACHINE_ARCH} == "i386"
SUBDIR= ahci amddev atl2 at_wini audio dec21140A dp8390 dpeth \
	e1000 fbd filter floppy fxp hello lance orinoco pci pckbd \
	printer rtl8139 rtl8169 ti1225 vbox acpi \
	virtio_blk virtio_net
.endif

.if ${MACHINE_ARCH} == "earm"
SUBDIR= bmp085 cat24c256 fb gpio i2c lan8710a \
	sht21 tda19988 tps65217 tps65950 tsl2550
.endif

# Drivers available on all platforms
SUBDIR+= log mmc random tty uds vnd readclock

.endif # ${MKIMAGEONLY} != "yes"

# memory driver must be last for ramdisk image
SUBDIR+= ramdisk .WAIT memory

.include <bsd.subdir.mk>
