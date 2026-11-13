#	$NetBSD: Makefile,v 1.42 2012/05/05 16:03:55 tsutsui Exp $

PROG=	fdisk 
SRCS=	fdisk.c

MAN=	fdisk.8

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
CPPFLAGS+= -DBOOTSEL
.if (${HOSTPROG:U} == "")
CPPFLAGS+= -DUSE_DISKLIST
.endif
.endif

.if ${MACHINE} == "arc"
CPPFLAGS+= -D_PATH_DEFDISK='"/dev/rsd0d"'
.endif

.if ${MACHINE} == "netwinder"
CPPFLAGS+= -D_PATH_DEFDISK='"/dev/rwd0c"'
.endif

.include <bsd.prog.mk>

.if (${HOSTPROG:U} == "")
DPADD+=	${LIBUTIL} ${LIBZ}
LDADD+=	-lutil -lz
.endif
