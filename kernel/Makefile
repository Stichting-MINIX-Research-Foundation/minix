# Makefile for kernel
.include <bsd.own.mk>

PROG=	kernel

# first-stage, arch-dependent startup code
SRCS=	mpx.S
SRCS+=	start.c table.c main.c proc.c \
	system.c clock.c utility.c debug.c interrupt.c \
	watchdog.c cpulocals.c

.ifdef CONFIG_SMP
SRCS += smp.c
.endif

DPADD+=	${LIBTIMERS} ${LIBSYS} ${LIBEXEC}
LDADD+=	-ltimers -lsys -lexec

CFLAGS += -D__kernel__

.if ${COMPILER_TYPE} == "ack"
LDFLAGS+= -.o
.elif ${COMPILER_TYPE} == "gnu"
CPPFLAGS+= -ffreestanding -fno-stack-protector -D_NETBSD_SOURCE
LDFLAGS+= -T ${.CURDIR}/arch/${ARCH}/kernel.lds
LDFLAGS+= -nostdlib -L${LIBDIR}
LDADD+= -lminlib
DPADD+= ${LIBMINLIB}
.if ${CC} == "gcc"
LDADD+=	-lgcc -lsys -lgcc -lminc
.elif ${CC} == "clang"
LDADD+= -L/usr/pkg/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic -lminc
DPADD+=	${LIBC}
.endif
.endif

CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${ARCH}/include -I${MINIXSRCDIR}
AFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${ARCH}/include -I${MINIXSRCDIR}

INSTALLFLAGS+=	-S 0
BINDIR=	/usr/sbin
MAN=

.include "system/Makefile.inc"
.include "arch/${ARCH}/Makefile.inc"

# These come last, so the profiling buffer is at the end of the data segment
SRCS+= profile.c do_sprofile.c

.include <bsd.prog.mk>

debug.d: extracted-errno.h extracted-mfield.h extracted-mtype.h

CLEANFILES+=extracted-errno.h extracted-mfield.h extracted-mtype.h

extracted-errno.h: extract-errno.sh ../include/errno.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; sh extract-errno.sh > ${.OBJDIR}/extracted-errno.h

extracted-mfield.h: extract-mfield.sh ../lib/libc/other/*.c ../lib/libc/posix/*.c ../lib/libsys/*.c
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; sh extract-mfield.sh > ${.OBJDIR}/extracted-mfield.h

extracted-mtype.h: extract-mtype.sh ../common/include/minix/com.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; sh extract-mtype.sh > ${.OBJDIR}/extracted-mtype.h

clean:
	rm -f extracted-errno.h extracted-mfield.h extracted-mtype.h
