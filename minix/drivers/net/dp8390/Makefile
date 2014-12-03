# Makefile for dp8390 driver
PROG=	dp8390
SRCS=	3c503.c dp8390.c ne2000.c rtl8029.c wdeth.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

.include <minix.service.mk>
