#	$NetBSD: Makefile,v 1.29 2020/10/30 20:05:00 rillig Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

PROG=	xinstall
SRCS=	xinstall.c getid.c
MAN=	install.1

.PATH:		${NETBSDSRCDIR}/usr.sbin/mtree
CPPFLAGS+=	-I${NETBSDSRCDIR}/usr.sbin/mtree

.if (${HOSTPROG:U} == "")
CPPFLAGS+=	-DHAVE_POSIX_SPAWN
DPADD+= ${LIBUTIL}
LDADD+= -lutil
.endif

COPTS.xinstall.c += -Wno-format-nonliteral

PROGNAME=install

CWARNFLAGS.gcc+=	${GCC_NO_FORMAT_TRUNCATION}

.include <bsd.prog.mk>
