# Makefile for the Sound Blaster 16 driver (SB16)
PROG=	sb16
SRCS=	sb16.c mixer.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
