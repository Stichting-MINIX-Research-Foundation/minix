#	$NetBSD: Makefile,v 1.7 2003/07/25 03:21:17 atatat Exp $

PROG=	stat

.if !defined(HOSTPROG)
LINKS=	${BINDIR}/stat ${BINDIR}/readlink
.if defined(__MINIX)
# To prevent breaking of existing utilities in commands/
LINKS+=	${BINDIR}/stat ${BINDIR}/fstat
LINKS+=	${BINDIR}/stat ${BINDIR}/lstat
.endif
MLINKS=	stat.1 readlink.1
.endif

.include <bsd.prog.mk>
