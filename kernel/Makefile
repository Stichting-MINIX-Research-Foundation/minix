# Makefile for kernel
.include <bsd.own.mk>

BINDIR=	/usr/sbin
MAN=
PROG=	kernel

.include "arch/${MACHINE_ARCH}/Makefile.inc"

SRCS+=	clock.c cpulocals.c interrupt.c main.c proc.c system.c \
	table.c utility.c usermapped_data.c

DPADD+=	${LIBTIMERS} ${LIBSYS} ${LIBEXEC} ${LIBMINLIB}
LDADD+=	-ltimers -lsys -lexec -lminlib -L/usr/lib

LINKERSCRIPT= ${.CURDIR}/arch/${MACHINE_ARCH}/kernel.lds

LDFLAGS+= -T ${LINKERSCRIPT}
LDFLAGS+= -nostdlib 
CFLAGS += -fno-stack-protector

CPPFLAGS+= -D_NETBSD_SOURCE -D_MINIX -D__kernel__ 

# kernel headers are always called through kernel/*.h
CPPFLAGS+= -I${NETBSDSRCDIR}

# kernel headers are always called through kernel/*.hi, this
# time for generated headers, during cross compilation
CPPFLAGS+= -I${.OBJDIR}/..

# Machine-dependent headers, order is important! 
CPPFLAGS+= -I${.CURDIR}/arch/${MACHINE_ARCH}
CPPFLAGS+= -I${.CURDIR}/arch/${MACHINE_ARCH}/include
CPPFLAGS+= -I${NETBSDSRCDIR}/include/arch/${MACHINE_ARCH}/include

.include "system/Makefile.inc"

.ifdef CONFIG_SMP
SRCS+= smp.c
.endif

.if ${USE_WATCHDOG} != "no"
SRCS+= watchdog.c
CPPFLAGS+= -DUSE_WATCHDOG
.endif

.if ${USE_MCONTEXT} != "no"
SRCS+= do_mcontext.c
CPPFLAGS+= -DUSE_MCONTEXT
.endif

# Extra debugging routines
.if ${USE_SYSDEBUG} != "no"
SRCS+= 	debug.c
CPPFLAGS+= -DUSE_SYSDEBUG
.endif

# These come last, so the profiling buffer is at the end of the data segment
SRCS+= profile.c do_sprofile.c

.if ${USE_LIVEUPDATE} != "no"
CPPFLAGS+= -DUSE_UPDATE
.endif

.if ${USE_STATECTL} != "no"
CPPFLAGS+= -DUSE_STATECTL
.endif

.if ${USE_TRACE} != "no"
CPPFLAGS+= -DUSE_TRACE
.endif

CLEANFILES+=extracted-errno.h extracted-mfield.h extracted-mtype.h procoffsets.h

debug.d: extracted-errno.h extracted-mfield.h extracted-mtype.h

extracted-errno.h: extract-errno.sh ../include/errno.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-errno.sh > ${.OBJDIR}/extracted-errno.h

extracted-mfield.h: extract-mfield.sh ../lib/libc/sys-minix/*.c ../lib/libsys/*.c
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-mfield.sh > ${.OBJDIR}/extracted-mfield.h

extracted-mtype.h: extract-mtype.sh ../include/minix/com.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-mtype.sh > ${.OBJDIR}/extracted-mtype.h

.include <minix.bootprog.mk>
