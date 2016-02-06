# Makefile for the Texas Instruments PCI1225 PC Card controller driver (ti1225)
PROG=	ti1225
SRCS=	ti1225.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBSYS}
LDADD+=	-lsys

.include <minix.service.mk>
