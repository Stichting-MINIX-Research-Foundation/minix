# Makefile for the Realtek RTL8139 ethernet driver (RTL8139)
PROG=	rtl8139
SRCS=	rtl8139.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

CPPFLAGS+=	-I${NETBSDSRCDIR}/minix

WARNS?= 5

.include <minix.service.mk>
