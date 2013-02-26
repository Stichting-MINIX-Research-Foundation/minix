# $NetBSD: Makefile,v 1.3 2011/01/19 11:43:18 plunky Exp $

PROG=		luac
SRCS=		luac.c print.c
BINDIR=		/usr/bin

#
# statically link ${LIBLUA} as "hidden" symbols need to be accessed
#
DPADD+=		${LIBLUA} ${LIBM}
LDADD+=		${LIBLUA} -lm

.include <bsd.own.mk>

.PATH:		${NETBSDSRCDIR}/external/mit/lua/dist/src \
		${NETBSDSRCDIR}/external/mit/lua/dist/doc

.include <bsd.prog.mk>
