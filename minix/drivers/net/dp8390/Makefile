# Makefile for dp8390 driver
PROG=	dp8390
SRCS=	3c503.c dp8390.c ne2000.c rtl8029.c wdeth.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

WARNS?= 5

.include <minix.service.mk>
