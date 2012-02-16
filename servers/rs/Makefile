# Makefile for Reincarnation Server (RS)
PROG=	rs
SRCS=	exec.c main.c request.c manager.c table.c utility.c memory.c error.c

DPADD+=	${LIBSYS} ${LIBEXEC}
LDADD+=	-lsys -lexec

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=	-I${MINIXSRCDIR}

.include <minix.bootprog.mk>
