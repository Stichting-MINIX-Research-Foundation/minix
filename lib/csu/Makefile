.include <minix.own.mk>

.PATH:	${.CURDIR}/${ARCH}

SRCS=	crtso.S
OBJS=	crtso.o

realall: ${OBJS}

FILES=${OBJS}
FILESDIR=${LIBDIR}
CLEANFILES=${OBJS}

.include <minix.prog.mk>
