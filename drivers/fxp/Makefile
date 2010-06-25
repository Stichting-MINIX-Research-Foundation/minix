# Makefile for Intel Pro/100 driver (FXP)
PROG=	fxp
SRCS=	fxp.c mii.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lnetdriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
