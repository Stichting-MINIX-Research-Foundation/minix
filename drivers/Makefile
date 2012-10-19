# Makefile for all device drivers.
#

.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "yes"

# memory driver must be last for ramdisk image
SUBDIR=	at_wini floppy log tty pci ramdisk memory

.else

# memory driver must be last for ramdisk image
SUBDIR+= ahci amddev atl2 at_wini audio dec21140A dp8390 dpeth \
	e1000 fbd filter floppy fxp hello lance log mmc orinoco pci printer \
	random readclock rtl8139 rtl8169 ti1225 tty vbox acpi \
	memory ramdisk
.endif

.include <bsd.subdir.mk>
