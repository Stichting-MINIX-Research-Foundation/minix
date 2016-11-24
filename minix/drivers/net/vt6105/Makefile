# Makefile for the VIA Technology 6105/6106S Ethernet driver (vt6105)
PROG=	vt6105
SRCS=	vt6105.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

CPPFLAGS+=	-I${NETBSDSRCDIR}/minix

.include <minix.service.mk>
