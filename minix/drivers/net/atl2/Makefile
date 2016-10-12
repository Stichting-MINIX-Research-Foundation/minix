# Makefile for the Attansic/Atheros L2 ethernet driver (ATL2)
PROG=	atl2
SRCS=	atl2.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

WARNS?= 5

.include <minix.service.mk>
