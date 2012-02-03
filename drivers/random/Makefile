# Makefile for random driver (RANDOM)
PROG=	random
SRCS=	main.c random.c rijndael_api.c rijndael_alg.c

.PATH:	${.CURDIR}/aes

DPADD+=	${LIBCHARDRIVER} ${LIBSYS} ${LIBMINIXUTIL}
LDADD+=	-lchardriver -lsys 
.if ${COMPILER_TYPE} != "gnu"
LDADD+= -lminixutil
.endif

MAN=

BINDIR?= /usr/sbin

CPPFLAGS.random.c+=	-I${MINIXSRCDIR}

.include <minix.service.mk>

