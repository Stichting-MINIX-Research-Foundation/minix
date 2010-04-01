# Makefile for the Realtek RTL8169 ethernet driver (RTL8169)
PROG=	rtl8169
SRCS=	rtl8169.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=	-I${MINIXSRCDIR}

.include <minix.prog.mk>
