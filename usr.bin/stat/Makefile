#	$NetBSD: Makefile,v 1.8 2011/08/17 13:22:33 christos Exp $

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

COPTS.stat.c += -Wno-format-nonliteral

# LSC Until it compiles cleanly...
NOGCCERROR:=yes

.include <bsd.prog.mk>
