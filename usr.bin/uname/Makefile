#	$NetBSD: Makefile,v 1.3 1997/01/09 20:23:11 tls Exp $
#	from: @(#)Makefile	5.3 (Berkeley) 5/11/90

PROG=	uname

.if defined(__MINIX)
LINKS+=	${BINDIR}/uname ${BINDIR}/arch
.endif # __MINIX

.include <bsd.prog.mk>
