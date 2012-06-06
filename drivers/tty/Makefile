# Makefile for terminal driver (TTY)

PROG=	tty
SRCS=	tty.c console.c keyboard.c pty.c rs232.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

SUBDIR= keymaps

# Needs kernel/const.h, etc
CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.bootprog.mk>
.include <bsd.subdir.mk>
