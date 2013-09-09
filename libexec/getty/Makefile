#	$NetBSD: Makefile,v 1.19 2010/02/03 15:34:43 roy Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93

WARNS?=	2	# XXX: many const & sign-compare issues

PROG=	getty
SRCS=	main.c init.c subr.c
DPADD+=	${LIBUTIL} ${LIBTERMINFO}
LDADD+=	-lutil -lterminfo
MAN=	getty.8 gettytab.5 ttys.5

.include <bsd.prog.mk>
