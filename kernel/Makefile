# Makefile for kernel
.include <bsd.own.mk>

PROG=	kernel

# first-stage, arch-dependent startup code
SRCS=	mpx.S
SRCS+=	start.c table.c main.c proc.c \
	system.c clock.c utility.c debug.c profile.c interrupt.c \
	watchdog.c

DPADD+=	${LIBTIMERS} ${LIBSYS}
LDADD+=	-ltimers -lsys

.if ${COMPILER_TYPE} == "ack"
LDFLAGS+= -.o
.elif ${COMPILER_TYPE} == "gnu"
CPPFLAGS+= -ffreestanding -fno-stack-protector
LDFLAGS+= -T arch/${ARCH}/kernel.lds
LDFLAGS+= -nostdlib -L/usr/gnu/lib
CFLAGS+=-march=i386
DPADD+=	${LIBC}
LDADD+=	-lc
.endif

CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${ARCH}/include -I${MINIXSRCDIR}
AFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${ARCH}/include -I${MINIXSRCDIR}

INSTALLFLAGS+=	-S 0
BINDIR=	/usr/sbin
MAN=

.include "system/Makefile.inc"
.include "arch/${ARCH}/Makefile.inc"
.include <bsd.prog.mk>

debug.d: extracted-errno.h extracted-mfield.h extracted-mtype.h

extracted-errno.h: extract-errno.sh ../include/errno.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; sh extract-errno.sh > ${.OBJDIR}/extracted-errno.h

extracted-mfield.h: extract-mfield.sh ../lib/libc/other/*.c ../lib/libc/posix/*.c ../lib/libsys/*.c
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; sh extract-mfield.sh > ${.OBJDIR}/extracted-mfield.h

extracted-mtype.h: extract-mtype.sh ../include/minix/com.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; sh extract-mtype.sh > ${.OBJDIR}/extracted-mtype.h

clean:
	rm -f extracted-errno.h extracted-mfield.h extracted-mtype.h
