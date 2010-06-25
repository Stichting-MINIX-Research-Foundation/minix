##
##  Makefile for Dec21140A ethernet board driver
##
PROG=	dec21140A
SRCS=	dec21140A.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

MAN=

BINDIR?= /usr/sbin

debug=		0
CPPFLAGS+=	-Ddebug=${debug} -ws -w -Wall

.include <bsd.prog.mk>
