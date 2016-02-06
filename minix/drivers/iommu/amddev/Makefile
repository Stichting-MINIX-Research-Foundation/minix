# Makefile for AMD's DEV
PROG=	amddev
SRCS=	amddev.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+= ${LIBSYS}
LDADD+=	-lsys

.include <minix.service.mk>
