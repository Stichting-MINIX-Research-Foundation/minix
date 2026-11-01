.include <bsd.own.mk>

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	bochs
.endif # ${MACHINE_ARCH} == "i386"

.if ${MACHINE_ARCH} == "earm"
SUBDIR+=	fb
SUBDIR+=	tda19988
.endif # ${MACHINE_ARCH} == "earm"

.include <bsd.subdir.mk>
