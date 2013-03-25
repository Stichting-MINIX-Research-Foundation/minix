# Makefile for the PC keyboard/mouse driver (PCKBD)
PROG=	pckbd
SRCS=	pckbd.c table.c

DPADD+=	${LIBINPUTDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-linputdriver -lsys -ltimers

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
