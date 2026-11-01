# Makefile for the Bochs/QEMU standard VGA "DRM-lite" driver
PROG=	bochs
SRCS=	bochs.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

WARNS?=	5

.include <minix.service.mk>
