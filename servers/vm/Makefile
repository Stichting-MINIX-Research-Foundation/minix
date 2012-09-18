# Makefile for VM server
.include <bsd.own.mk>

PROG=	vm
SRCS=	main.c alloc.c utility.c exit.c fork.c break.c \
	mmap.c slaballoc.c region.c pagefaults.c \
	physravl.c rs.c queryexit.c yieldedavl.c regionavl.c pb.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys -lexec

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+= -I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}
CPPFLAGS+=	-I${NETBSDSRCDIR}

.include "arch/${MACHINE_ARCH}/Makefile.inc"
.include <minix.bootprog.mk>
