.include <bsd.own.mk>

SUBDIR+=	mfs
SUBDIR+=	pfs

.if ${MKIMAGEONLY} == "no"
SUBDIR+=	ext2
SUBDIR+=	isofs
SUBDIR+=	procfs
SUBDIR+=	ptyfs

.  if ${MACHINE_ARCH} == "i386"
SUBDIR+=	hgfs
SUBDIR+=	vbfs
.  endif # ${MACHINE_ARCH} == "i386"
.endif # ${MKIMAGEONLY} == "no"

.include <bsd.subdir.mk>
