# Makefile for all system servers.
#

.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "yes"

SUBDIR=	ds input mfs pfs pm rs sched vfs vm

.else

SUBDIR=	ds ext2 inet input ipc is iso9660fs \
	mfs pfs pm procfs rs sched vfs vm devman

.if ${MACHINE_ARCH} == "i386"
SUBDIR+= hgfs vbfs
.endif

.if ${MKLWIP} == "yes"
SUBDIR += lwip
.endif

.endif

.include <bsd.subdir.mk>
