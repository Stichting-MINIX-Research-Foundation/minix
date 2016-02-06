# Makefile for the ES1370 sounddriver
PROG=	es1370
SRCS=	es1370.c ak4531.c pci_helper.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
