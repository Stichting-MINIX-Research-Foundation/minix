# Makefile for the Orinoco wireless card (Prism chipset)
PROG=	orinoco
SRCS=	orinoco.c hermes.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS.orinoco.c+=	-I${MINIXSRCDIR}

.include <minix.prog.mk>
