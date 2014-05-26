# Makefile for all device drivers.
#
.include <bsd.own.mk>

.if ${MKIMAGEONLY} != "yes"
# The default case

# Drivers available on all platforms
SUBDIR= hello \
	log \
	mmc \
	pty \
	random \
	tty \
	uds \
	vnd \
	readclock

.if ${MACHINE_ARCH} == "i386"
SUBDIR+= ahci \
	amddev \
	atl2 \
	at_wini \
	audio \
	dec21140A \
	dp8390 \
	dpeth \
	e1000 \
	fbd \
	filter \
	floppy \
	fxp \
	lance \
	orinoco \
	pci pckbd \
	printer \
	rtl8139 \
	rtl8169 \
	ti1225 \
	vbox \
	acpi \
	virtio_blk \
	virtio_net
.endif

.if ${MACHINE_ARCH} == "earm"
SUBDIR+= bmp085 \
	cat24c256 \
	fb \
	gpio \
	i2c \
	lan8710a \
	sht21 \
	tda19988 \
	tps65217 \
	tps65950 \
	tsl2550
.endif

.endif # ${MKIMAGEONLY} != "yes"


.if ${MKIMAGEONLY} == "yes"
# MKIMAGEONLY builds are specialized builds that are targeted
# at being small.

SUBDIR= tty
.if ${MACHINE_ARCH} == "i386"
SUBDIR+= at_wini \
	floppy \
	pci \
	pckbd
.endif

.endif # ${MKIMAGEONLY} == "yes"

# memory driver must be last for ramdisk image
SUBDIR+= ramdisk .WAIT memory

.include <bsd.subdir.mk>
