.include <minix.own.mk>

.PATH:	${.CURDIR}/${ARCH}

AFLAGS+=	-O -D_MINIX -D_POSIX_SOURCE

SRCS=	crtso.s
OBJS=	crtso.o

realall: ${OBJS}

FILES=${OBJS}
FILESDIR=${LIBDIR}
CLEANFILES=${OBJS}

.include <minix.prog.mk>
