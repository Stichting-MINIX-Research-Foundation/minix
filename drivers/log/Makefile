# Makefile for log driver
PROG=	log
SRCS=	log.c diag.c liveupdate.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /sbin

.include <minix.prog.mk>
