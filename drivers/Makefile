# Makefile for all device drivers.
#

.include <bsd.own.mk>

# memory driver must be last for ramdisk image
.if ${COMPILER_TYPE} == "gnu"
SUBDIR = acpi
.endif

SUBDIR+= ahci amddev atl2 at_wini audio bios_wini dec21140A dp8390 dpeth \
	e1000 filter floppy fxp hello lance log orinoco pci printer \
	random readclock rtl8139 rtl8169 sb16 ti1225 tty \
	.WAIT ramdisk .WAIT memory

# memory driver must be last for ramdisk image
IMAGE_SUBDIR=	at_wini bios_wini floppy log tty pci .WAIT ramdisk .WAIT memory

.include <bsd.subdir.mk>
