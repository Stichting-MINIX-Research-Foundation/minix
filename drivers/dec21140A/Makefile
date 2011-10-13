##
##  Makefile for Dec21140A ethernet board driver
##
PROG=	dec21140A
SRCS=	dec21140A.c

FILES=$(PROG).conf
FILESNAME=$(PROG)
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

MAN=

BINDIR?= /usr/sbin

debug=		0
CPPFLAGS+=	-Ddebug=${debug} -w

.include <minix.service.mk>
