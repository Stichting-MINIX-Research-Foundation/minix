# Makefile for the ALS4000 driver
PROG=	als4000
SRCS=	als4000.c mixer.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
