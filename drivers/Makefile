# Makefile for all device drivers.
#

.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "yes"

.if ${MACHINE_ARCH} == "i386"
SUBDIR=	at_wini floppy tty pci pckbd
.endif

.if ${MACHINE_ARCH} == "earm"
SUBDIR= tty
.endif

.else # ${MKIMAGEONLY} != "yes"

.if ${MACHINE_ARCH} == "i386"
SUBDIR= ahci amddev atl2 at_wini audio dec21140A dp8390 dpeth \
	e1000 fbd filter floppy fxp hello lance log mmc orinoco pci pckbd \
	printer random readclock rtl8139 rtl8169 ti1225 tty vbox acpi \
	virtio_blk virtio_net vnd
.endif

.if ${MACHINE_ARCH} == "earm"
SUBDIR= bmp085 cat24c256 fb gpio i2c mmc lan8710a log readclock \
	sht21 tda19988 tps65217 tps65950 tsl2550 tty random vnd
.endif

.endif # ${MKIMAGEONLY} != "yes"

# memory driver must be last for ramdisk image
SUBDIR+= ramdisk .WAIT memory

.include <bsd.subdir.mk>
