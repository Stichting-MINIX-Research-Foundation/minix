# $NetBSD: Makefile,v 1.4 2012/01/20 16:31:30 joerg Exp $
# FreeBSD: src/usr.bin/locale/Makefile,v 1.4 2003/06/25 23:05:11 phantom Exp

WARNS?=	2	# XXX -Wcast-qual issues

PROG = locale

CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libc/locale
CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libc/citrus

.include <bsd.prog.mk>
