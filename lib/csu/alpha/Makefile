#	$NetBSD: Makefile,v 1.24 2008/07/21 15:34:58 lukem Exp $

.include <bsd.own.mk>

ELFSIZE=	64

#Uncomment the next line to enable the new .init fallthru
CPPFLAGS+=	-I${.CURDIR}

OBJS+=	crtfm.o

.include "${.CURDIR}/../common_elf/Makefile.inc"

crtfm.o: crtfm.c
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${.ALLSRC} -o ${.TARGET}.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	rm -f ${.TARGET}.o
