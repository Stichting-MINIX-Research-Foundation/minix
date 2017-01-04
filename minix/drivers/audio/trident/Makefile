# Makefile for the Trident driver
PROG=	trident
SRCS=	trident.c mixer.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
