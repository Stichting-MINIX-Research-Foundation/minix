.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "no"
SUBDIR+=	audio
SUBDIR+=	clock
SUBDIR+=	eeprom
SUBDIR+=	examples
SUBDIR+=	iommu
SUBDIR+=	net
SUBDIR+=	power
SUBDIR+=	printer
SUBDIR+=	sensors
SUBDIR+=	system
SUBDIR+=	usb
SUBDIR+=	video
SUBDIR+=	vmm_guest
.endif # ${MKIMAGEONLY} == "no"

# The following folders contain drivers which are always required
SUBDIR+=	bus
SUBDIR+=	tty
SUBDIR+=	hid

# BJG - build storage last as the ramdisk depends on some other things
SUBDIR+=	.WAIT storage

.include <bsd.subdir.mk>
