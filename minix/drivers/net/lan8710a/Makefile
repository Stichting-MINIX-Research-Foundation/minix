# Makefile for the lan8710a ethernet driver.
PROG=   lan8710a
SRCS=   lan8710a.c

FILES=$(PROG).conf
FILESNAME=$(PROG)
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lnetdriver -lsys -ltimers

.include <minix.service.mk>
