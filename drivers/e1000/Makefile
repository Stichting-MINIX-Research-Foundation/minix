#
# Makefile for the Intel Pro/1000 driver.
#
PROG=	e1000
SRCS=	e1000.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lnetdriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
