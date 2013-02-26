#	$NetBSD: Makefile,v 1.6 2011/09/08 18:44:38 jmmv Exp $
#
.include <bsd.own.mk>

ATFFILE=	no
TESTSDIR=	${TESTSBASE}/lib/libcurses
TESTS_C+=	slave

PROG=	slave
NOMAN=	"true"
#MAN=	slave.7
SRCS=	slave.c commands.c curses_commands.c

CPPFLAGS+=	-g -I${.CURDIR} -I. -I${.CURDIR}/../director
LDADD+=		-lcurses -lterminfo -Wl,--no-fatal-warnings

COPTS.curses_commands.c += -Wno-format-nonliteral
CWARNFLAGS.clang+=	-Wno-format

.include <bsd.test.mk>
