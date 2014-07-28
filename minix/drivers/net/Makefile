.include <bsd.own.mk>

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	atl2
SUBDIR+=	dec21140A
SUBDIR+=	dp8390
SUBDIR+=	dpeth
SUBDIR+=	e1000
SUBDIR+=	fxp
SUBDIR+=	lance
SUBDIR+=	orinoco
SUBDIR+=	rtl8139
SUBDIR+=	rtl8169
SUBDIR+=	virtio_net
.endif # ${MACHINE_ARCH} == "i386"

.if ${MACHINE_ARCH} == "earm"
SUBDIR+=	lan8710a
.endif # ${MACHINE_ARCH} == "earm"

SUBDIR+=	uds

.include <bsd.subdir.mk>
