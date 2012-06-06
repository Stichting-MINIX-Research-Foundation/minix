# Makefile for the Realtek RTL8139 ethernet driver (RTL8139)
PROG=	rtl8139
SRCS=	rtl8139.c liveupdate.c

FILES=$(PROG).conf
FILESNAME=$(PROG)
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lnetdriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
