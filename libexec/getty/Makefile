#	$NetBSD: Makefile,v 1.20 2013/08/11 05:42:41 dholland Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93

PROG=	getty
SRCS=	main.c init.c subr.c
DPADD+=	${LIBUTIL} ${LIBTERMINFO}
LDADD+=	-lutil -lterminfo
MAN=	getty.8 gettytab.5 ttys.5

.include <bsd.prog.mk>
