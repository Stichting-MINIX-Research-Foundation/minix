# Makefile for the input server

PROG=	input
SRCS=	input.c evdev.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

.include <minix.service.mk>

