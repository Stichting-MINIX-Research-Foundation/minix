# Makefile for terminal driver (TTY)

PROG=	tty

.include "arch/${MACHINE_ARCH}/Makefile.inc"

SRCS +=	tty.c pty.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

SUBDIR= keymaps

# Needs kernel/const.h, etc
CPPFLAGS+=	-I${.CURDIR}	-I${NETBSDSRCDIR}

.include <minix.bootprog.mk>
.include <bsd.subdir.mk>
