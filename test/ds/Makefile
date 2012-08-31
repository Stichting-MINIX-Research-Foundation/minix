# Makefile for DS tests
PROG=	dstest subs
SRCS.dstest=	dstest.c
SRCS.subs=	subs.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin

.include "Makefile.inc"
.include <minix.service.mk>
