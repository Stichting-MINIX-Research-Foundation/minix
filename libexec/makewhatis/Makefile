#	$NetBSD: Makefile,v 1.20 2009/03/16 02:24:56 lukem Exp $

WARNS?=	2	# XXX: const issues

.include <bsd.own.mk>

PROG=	makewhatis
SRCS=	makewhatis.c manconf.c
.PATH:	${NETBSDSRCDIR}/usr.bin/man
CPPFLAGS+=-I${NETBSDSRCDIR}/usr.bin

MAN=	${PROG}.8

.ifndef HOSTPROG
DPADD+=	${LIBUTIL} ${LIBZ}
LDADD+=	-lutil -lz
.endif

.include <bsd.prog.mk>
