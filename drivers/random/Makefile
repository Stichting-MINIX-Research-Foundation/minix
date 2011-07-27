# Makefile for random driver (RANDOM)
PROG=	random
SRCS=	main.c random.c rijndael_api.c rijndael_alg.c

.PATH:	${.CURDIR}/aes

DPADD+=	${LIBDRIVER} ${LIBSYS} ${LIBMINIXUTIL}
LDADD+=	-ldriver -lsys -lminixutil

MAN=

BINDIR?= /usr/sbin

CPPFLAGS.random.c+=	-I${MINIXSRCDIR}

.include <minix.service.mk>

