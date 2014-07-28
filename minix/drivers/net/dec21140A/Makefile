#  Makefile for Dec21140A ethernet board driver
PROG=	dec21140A
SRCS=	dec21140A.c

FILES=$(PROG).conf
FILESNAME=$(PROG)
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

CPPFLAGS+=	-Ddebug=0

.include <minix.service.mk>
