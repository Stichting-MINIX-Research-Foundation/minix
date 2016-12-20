.include <bsd.own.mk>

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	3c90x
SUBDIR+=	atl2
SUBDIR+=	dec21140A
SUBDIR+=	dp8390
SUBDIR+=	dpeth
SUBDIR+=	e1000
SUBDIR+=	fxp
SUBDIR+=	ip1000
SUBDIR+=	lance
SUBDIR+=	rtl8139
SUBDIR+=	rtl8169
SUBDIR+=	virtio_net
SUBDIR+=	vt6105
.endif # ${MACHINE_ARCH} == "i386"

.if ${MACHINE_ARCH} == "earm"
SUBDIR+=	lan8710a
.endif # ${MACHINE_ARCH} == "earm"

.include <bsd.subdir.mk>
