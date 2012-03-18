# Makefile for the sys_vumap test.
PROG=	vumaptest vumaprelay
SRCS.vumaptest=	vumaptest.c
SRCS.vumaprelay= vumaprelay.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin

.include "Makefile.inc"
.include <minix.service.mk>
