# Makefile for the VirtualBox backdoor driver (VBOX)
PROG=	vbox
SRCS=	vbox.c hgcm.c err.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBSYS}
LDADD+=	-lsys

.include <minix.service.mk>
