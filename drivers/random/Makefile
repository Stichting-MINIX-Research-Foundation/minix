# Makefile for random driver (RANDOM)
PROG=	random
SRCS=	main.c random.c sha2.c rijndael_api.c rijndael_alg.c

.PATH:	${.CURDIR}/aes

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <minix.prog.mk>

