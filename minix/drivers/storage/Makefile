.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "no"
.  if ${MACHINE_ARCH} == "i386"
SUBDIR+=	ahci
SUBDIR+=	fbd
SUBDIR+=	filter
SUBDIR+=	virtio_blk
.  endif # ${MACHINE_ARCH} == "i386"

SUBDIR+=	mmc
SUBDIR+=	vnd
.endif # ${MKIMAGEONLY} == "no"

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	at_wini
SUBDIR+=	floppy
.endif # ${MACHINE_ARCH} == "i386"

# memory driver must be last for ramdisk image.
# Everything else must be done before ramdisk as it needs
# executables from other targets.
SUBDIR+=	.WAIT ramdisk .WAIT memory

.include <bsd.subdir.mk>
