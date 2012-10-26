# Makefile for kernel
.include <bsd.own.mk>

PROG=	kernel

.include "arch/${MACHINE_ARCH}/Makefile.inc"

SRCS+=	clock.c cpulocals.c interrupt.c main.c proc.c system.c \
	table.c utility.c usermapped_data.c

LINKERSCRIPT=${.CURDIR}/arch/${MACHINE_ARCH}/kernel.lds

DPADD+=	${LIBTIMERS} ${LIBSYS} ${LIBEXEC} ${LINKERSCRIPT}
LDADD+=	-ltimers -lsys -lexec

CFLAGS += -D__kernel__ 

CPPFLAGS+= -fno-stack-protector -D_NETBSD_SOURCE -D_MINIX
LDFLAGS+= -T ${LINKERSCRIPT}
LDFLAGS+= -nostdlib -L${DESTDIR}/${LIBDIR}
LDADD+= -lminlib
DPADD+= ${LIBMINLIB}
.if !empty(CC:M*gcc)
LDADD+=	-lgcc -lsys -lgcc -lminc
.elif !empty(CC:M*clang)
LDADD+= -L/usr/pkg/compiler-rt/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic -lminc
DPADD+=	${LIBC}
.endif

CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}/include -I${NETBSDSRCDIR}

BINDIR=	/usr/sbin
MAN=

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

.include <bsd.prog.mk>

debug.d: extracted-errno.h extracted-mfield.h extracted-mtype.h

CLEANFILES+=extracted-errno.h extracted-mfield.h extracted-mtype.h procoffsets.h

extracted-errno.h: extract-errno.sh ../include/errno.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-errno.sh > ${.OBJDIR}/extracted-errno.h

extracted-mfield.h: extract-mfield.sh ../lib/libc/sys-minix/*.c ../lib/libsys/*.c
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-mfield.sh > ${.OBJDIR}/extracted-mfield.h

extracted-mtype.h: extract-mtype.sh ../include/minix/com.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-mtype.sh > ${.OBJDIR}/extracted-mtype.h

clean:
	rm -f extracted-errno.h extracted-mfield.h extracted-mtype.h


