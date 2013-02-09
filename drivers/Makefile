# Makefile for all device drivers.
#

.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "yes"

.if ${MACHINE_ARCH} == "i386"
SUBDIR=	at_wini floppy log tty pci
.endif

.if ${MACHINE_ARCH} == "earm"
SUBDIR= log tty
.endif

.else # ${MKIMAGEONLY} != "yes"

.if ${MACHINE_ARCH} == "i386"
SUBDIR= ahci amddev atl2 at_wini audio dec21140A dp8390 dpeth \
	e1000 fbd filter floppy fxp hello lance log mmc orinoco pci printer \
	random readclock rtl8139 rtl8169 ti1225 tty vbox acpi \
	virtio_blk virtio_net
.endif

.if ${MACHINE_ARCH} == "earm"
SUBDIR=  gpio mmc log tty random
.endif

.endif # ${MKIMAGEONLY} != "yes"

# memory driver must be last for ramdisk image
SUBDIR+= ramdisk .WAIT memory

.include <bsd.subdir.mk>
