.include <bsd.own.mk>

SUBDIR+=	mfs
SUBDIR+=	pfs

.if ${MKIMAGEONLY} == "no"
SUBDIR+=	ext2
#LSC: Commented until the fixed version is ready to be merged in.
#SUBDIR+=	iso9660fs
SUBDIR+=	procfs

.  if ${MACHINE_ARCH} == "i386"
SUBDIR+=	hgfs
SUBDIR+=	vbfs
.  endif # ${MACHINE_ARCH} == "i386"
.endif # ${MKIMAGEONLY} == "no"

.include <bsd.subdir.mk>
