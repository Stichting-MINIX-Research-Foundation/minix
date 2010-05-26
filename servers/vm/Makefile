# Makefile for VM server
.include <minix.own.mk>

PROG=	vm
SRCS=	main.c alloc.c utility.c exec.c exit.c fork.c break.c \
	signal.c mmap.c slaballoc.c region.c pagefaults.c addravl.c \
	physravl.c rs.c queryexit.c yieldedavl.c


DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 128k

CPPFLAGS+= -I${.CURDIR} -Iarch/${ARCH}
CPPFLAGS+=	-I${MINIXSRCDIR}

.include "arch/${ARCH}/Makefile.inc"
.include <minix.prog.mk>
