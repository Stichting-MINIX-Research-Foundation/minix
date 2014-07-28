# Makefile for the Intel Pro/1000 driver.
PROG=	e1000
SRCS=	e1000.c

FILES=$(PROG).conf
FILESNAME=$(PROG)
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lnetdriver -lsys -ltimers

.include <minix.service.mk>
