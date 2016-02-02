LIB=netbn
SRCS= bignum.c digest.c misc.c rand.c
SRCS+= stubs.c
MKMAN=no
WARNS=4
CPPFLAGS+=-I${EXTDIST}

INCS=bn.h digest.h
INCSDIR=/usr/include/netpgp

EXTDIST=	${.CURDIR}/../cipher

.include <bsd.lib.mk>
