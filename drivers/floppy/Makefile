# Makefile for the floppy disk driver (FLOPPY)
PROG=	floppy
SRCS=	floppy.c liveupdate.c

DPADD+=	${LIBDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-ldriver -lsys -ltimers

MAN=

BINDIR?= /sbin

.include <bsd.prog.mk>
