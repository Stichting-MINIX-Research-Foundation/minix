#	$NetBSD: Makefile,v 1.7 2013/09/11 23:04:11 joerg Exp $
#

WARNS=		2

LIB=		ukfs
.if defined(__MINIX)
LIBDPLIBS+=	rump	${.CURDIR}/../librump		\
		rumpvfs	${.CURDIR}/../librumpvfs \
		mthread	${.CURDIR}/../../minix/lib/libmthread

CPPFLAGS+=	-Wno-unused-function
.else
LIBDPLIBS+=	rump	${.CURDIR}/../librump		\
		rumpvfs	${.CURDIR}/../librumpvfs \
		pthread	${.CURDIR}/../libpthread
.endif # defined(__MINIX)
CPPFLAGS+=	-I${.CURDIR}

INCS=		ukfs.h
INCSDIR=	/usr/include/rump

SRCS=		ukfs.c ukfs_disklabel.c
MAN=		ukfs.3

.include <bsd.lib.mk>
