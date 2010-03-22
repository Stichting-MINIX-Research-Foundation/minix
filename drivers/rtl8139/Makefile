# Makefile for the Realtek RTL8139 ethernet driver (RTL8139)
PROG=	rtl8139
SRCS=	rtl8139.c liveupdate.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <minix.prog.mk>
