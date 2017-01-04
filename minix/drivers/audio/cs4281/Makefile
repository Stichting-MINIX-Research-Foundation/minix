# Makefile for the CS4281 driver
PROG=	cs4281
SRCS=	cs4281.c mixer.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
