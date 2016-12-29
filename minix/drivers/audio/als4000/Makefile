# Makefile for the Sound Blaster 16 driver (SB16)
PROG=	als4000
SRCS=	als4000.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
