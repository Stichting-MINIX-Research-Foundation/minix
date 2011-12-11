# Makefile for all device drivers.
#

.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "yes"

# memory driver must be last for ramdisk image
SUBDIR=	at_wini bios_wini floppy log tty pci .WAIT ramdisk .WAIT memory

.else

# memory driver must be last for ramdisk image
SUBDIR+= ahci amddev atl2 at_wini audio bios_wini dec21140A dp8390 dpeth \
	e1000 fbd filter floppy fxp hello lance log orinoco pci printer \
	random readclock rtl8139 rtl8169 ti1225 tty vbox acpi \
	.WAIT ramdisk .WAIT memory

.endif

.include <bsd.subdir.mk>
