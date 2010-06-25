# Makefile for the Realtek RTL8169 ethernet driver (RTL8169)
PROG=	rtl8169
SRCS=	rtl8169.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lnetdriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=	-I${MINIXSRCDIR}

.include <bsd.prog.mk>
