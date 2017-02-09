#	$NetBSD: Makefile,v 1.1 2010/12/12 12:48:31 pooka Exp $
#

PROG=	rump.halt

.if defined(__MINIX)
LDADD+= -lrumpclient -lmthread
.else
LDADD+= -lrumpclient
.endif
DPADD+=	${RUMPCLIENT}

.include <bsd.prog.mk>
