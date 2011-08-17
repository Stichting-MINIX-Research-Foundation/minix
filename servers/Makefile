# Makefile for all system servers.
#

.include <bsd.own.mk>

.if ${BUILDAVFS} == "yes"
VFS= "avfs"
PFS= "apfs"
.else
VFS= "vfs"
PFS= "pfs"
.endif

.if ${MKIMAGEONLY} == "yes"

SUBDIR=	ds init mfs ${PFS} pm rs sched ${VFS} vm

.else

SUBDIR=	ds ext2 hgfs inet init ipc is iso9660fs \
	mfs ${PFS} pm procfs rs sched ${VFS} vm devman

.endif

.include <bsd.subdir.mk>
