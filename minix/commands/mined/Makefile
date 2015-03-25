# Makefile for mined

PROG=	mined
SRCS=	mined1.c mined2.c

CPPFLAGS+=	-I${.CURDIR}/../../../lib/libterminfo

DPADD+=	${LIBTERMINFO}
LDADD+=	-lterminfo

.include <bsd.prog.mk>
