# Makefile for all system servers.
#

.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "yes"

SUBDIR=	ds init mfs pfs pm rs sched vfs vm

.else

SUBDIR=	ds ext2 hgfs inet init ipc is iso9660fs \
	mfs pfs pm procfs rs sched vfs vm devman

.endif

.include <bsd.subdir.mk>
