# edparams: customize boot; runs on both installer's and user's machines

PROG=	edparams
BINDIR=	/usr/bin

SRCS=	boot.c rawfs.c
.PATH:	${.CURDIR}/../boot ${.CURDIR}/..
CPPFLAGS= -DUNIX -I${MINIXSRCDIR} -I${.CURDIR}/..

MAN=	# monitor.8

.include <bsd.prog.mk>
