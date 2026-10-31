# Makefile for terminal driver (TTY)
PROG=	tty

SRCS +=	tty.c

.include "arch/${MACHINE_ARCH}/Makefile.inc"
.include "wscons/Makefile.inc"
.include "wsfont/Makefile.inc"
.include "rasops/Makefile.inc"

DPADD+=	${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lchardriver -lsys -ltimers

SUBDIR= keymaps

# Needs kernel/const.h, etc
CPPFLAGS+=	-I${.CURDIR}	-I${NETBSDSRCDIR}

.include <minix.service.mk>
.include <bsd.subdir.mk>
