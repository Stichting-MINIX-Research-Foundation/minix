.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "no"
.  if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
SUBDIR+=	ti1225
.  endif # ${MACHINE_ARCH} == "i386" || x86_64

.  if ${MACHINE_ARCH} == "earm"
SUBDIR+=	i2c
.  endif # ${MACHINE_ARCH} == "earm"
.endif # ${MKIMAGEONLY} == "no"

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
SUBDIR+=	pci
.endif # ${MACHINE_ARCH} == "i386" || x86_64

.include <bsd.subdir.mk>
