# Makefile for the floppy disk driver (FLOPPY)
PROG=	floppy
SRCS=	floppy.c liveupdate.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lblockdriver -lsys -ltimers

.include <minix.service.mk>
