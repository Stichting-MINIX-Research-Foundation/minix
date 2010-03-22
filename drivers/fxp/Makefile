# Makefile for Intel Pro/100 driver (FXP)
PROG=	fxp
SRCS=	fxp.c mii.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <minix.prog.mk>
