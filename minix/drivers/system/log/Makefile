# Makefile for log driver
PROG=	log
SRCS=	log.c diag.c liveupdate.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

.include <minix.service.mk>
