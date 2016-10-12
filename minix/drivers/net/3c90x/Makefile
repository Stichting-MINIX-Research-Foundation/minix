# Makefile for the 3Com 3C90xB/C EtherLink driver (3c90x)
PROG=	3c90x
SRCS=	3c90x.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

WARNS?= 5

.include <minix.service.mk>
