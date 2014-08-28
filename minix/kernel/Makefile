# Makefile for kernel
.include <bsd.own.mk>

PROG=		kernel
BINDIR=		/usr/sbin
MAN=

.include "arch/${MACHINE_ARCH}/Makefile.inc"

SRCS+=	clock.c cpulocals.c interrupt.c main.c proc.c system.c \
	table.c utility.c usermapped_data.c

LDADD+=	-ltimers -lsys -lexec 

LINKERSCRIPT= ${.CURDIR}/arch/${MACHINE_ARCH}/kernel.lds

.if ${HAVE_GOLD:U} != ""
CFLAGS+= -fno-common
.endif
LDFLAGS+= -T ${LINKERSCRIPT}
LDFLAGS+= -nostdlib -L${DESTDIR}/usr/lib
CFLAGS += -fno-stack-protector

CPPFLAGS+= -D__kernel__ 

# kernel headers are always called through kernel/*.h
CPPFLAGS+= -I${NETBSDSRCDIR}/minix

# kernel headers are always called through kernel/*.h, this
# time for generated headers, during cross compilation
CPPFLAGS+= -I${.OBJDIR}/..

# Machine-dependent headers, order is important! 
CPPFLAGS+= -I${.CURDIR}/arch/${MACHINE_ARCH}
CPPFLAGS+= -I${.CURDIR}/arch/${MACHINE_ARCH}/include
CPPFLAGS+= -I${.CURDIR}/arch/${MACHINE_ARCH}/bsp/include
CPPFLAGS+= -I${NETBSDSRCDIR}/minix/include/arch/${MACHINE_ARCH}/include

.include "system/Makefile.inc"

.ifdef CONFIG_SMP
SRCS+= smp.c
.endif

.if ${USE_WATCHDOG} != "no"
SRCS+= watchdog.c
CPPFLAGS+= -DUSE_WATCHDOG
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

CLEANFILES+=extracted-errno.h extracted-mfield.h extracted-mtype.h procoffsets.h

debug.o debug.d: extracted-errno.h extracted-mfield.h extracted-mtype.h

extracted-errno.h: extract-errno.sh ../../include/errno.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-errno.sh > ${.OBJDIR}/extracted-errno.h

extracted-mfield.h: extract-mfield.sh ../lib/libc/sys/*.c ../lib/libsys/*.c
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-mfield.sh > ${.OBJDIR}/extracted-mfield.h

extracted-mtype.h: extract-mtype.sh ../include/minix/com.h
	${_MKTARGET_CREATE}
	cd ${.CURDIR} ; ${HOST_SH} extract-mtype.sh > ${.OBJDIR}/extracted-mtype.h

.if ${USE_BITCODE:Uno} == "yes"
kernel: kernel.bcl.o
	${_MKTARGET_LINK}
	${_CCLINK.kernel} \
		${_LDFLAGS.kernel} \
		-L${DESTDIR}/usr/lib \
		${_LDSTATIC.kernel} -o ${.TARGET} \
		${.TARGET}.bcl.o ${OBJS} ${_PROGLDOPTS} ${_LDADD.kernel} \
		${BITCODE_LD_FLAGS_2ND.kernel} \
		-Wl,--allow-multiple-definition
.endif

.include <minix.service.mk>
