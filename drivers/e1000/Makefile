#
# Makefile for the Intel Pro/1000 driver.
#
PROG=	e1000
SRCS=	e1000.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <minix.prog.mk>
