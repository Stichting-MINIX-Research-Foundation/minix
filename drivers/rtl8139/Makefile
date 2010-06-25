# Makefile for the Realtek RTL8139 ethernet driver (RTL8139)
PROG=	rtl8139
SRCS=	rtl8139.c liveupdate.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lnetdriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=	-I${MINIXSRCDIR}

.include <bsd.prog.mk>
