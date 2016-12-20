# Makefile for the CMI8738 driver
PROG=	cmi8738
SRCS=	cmi8738.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
