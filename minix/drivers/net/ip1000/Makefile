# Makefile for the Ethernet drivers
PROG=	ip1000	
SRCS=	ip1000.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

CPPFLAGS+=	-I${NETBSDSRCDIR}/minix

.include <minix.service.mk>
