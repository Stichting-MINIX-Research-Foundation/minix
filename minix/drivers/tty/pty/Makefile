# Makefile for pseudo terminal driver (PTY)
PROG=	pty
SRCS=	tty.c pty.c ptyfs.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lchardriver -lsys -ltimers

.include <minix.service.mk>
