#	$NetBSD: Makefile,v 1.4 2014/03/13 01:46:10 pooka Exp $
#

S!=             cd ${.PARSEDIR}/../../../../;pwd

.include "${.CURDIR}/../../../../modules/zfs/Makefile.zfsmod"

LIB=	rumpfs_zfs

# RUMP stuff
CPPFLAGS+=      -I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump/rumpvfs\
		-I${RUMPTOP}/librump/rumpdev

SRCS+=	zfs_component.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
