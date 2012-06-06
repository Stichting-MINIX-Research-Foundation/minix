# Makefile for the Orinoco wireless card (Prism chipset)
PROG=	orinoco
SRCS=	orinoco.c hermes.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lnetdriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS.orinoco.c+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
