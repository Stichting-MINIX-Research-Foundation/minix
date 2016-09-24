# Makefile for kernel
.include <bsd.own.mk>

PROG=		kernel
BINDIR=		/usr/sbin
MAN=

.if ${MACHINE_ARCH} == "earm" && ${MKLLVM:Uno} == "yes"
# BJG - problems with optimisation of the kernel by llvm 
DBG=-O0
.endif

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

.if ${MKPAE:Uno} != "no"
CPPFLAGS+= -DPAE=1
.endif

.ifdef CONFIG_SMP
SRCS+= smp.c
.endif

.if ${USE_WATCHDOG} != "no"
SRCS+= watchdog.c
CPPFLAGS+= -DUSE_WATCHDOG=1
.endif

# Extra debugging routines
.if ${USE_SYSDEBUG} != "no"
SRCS+= 	debug.c
CPPFLAGS+= -DUSE_SYSDEBUG=1
.endif

# These come last, so the profiling buffer is at the end of the data segment
SRCS+= profile.c do_sprofile.c

.if ${USE_LIVEUPDATE} != "no"
CPPFLAGS+= -DUSE_UPDATE=1
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
# dcvmoole: this is a copy of the "${_P}: ${_P}.bcl.o" block from bsd.prog.mk,
# with two changes: 1) ${OBJS} is added so as to link in objects that have not
# been compiled with bitcode, and 2) we are directly loading the gold plugin
# rather than through ${BITCODE_LD_FLAGS_2ND.kernel}, because LLVMgold will
# not load libLTO when no LTO can be performed (due to the non- bitcode
# objects), causing it to fail on unrecognized -disable-opt/-disable-inlining
# options.  At least I think that's what's going on?  I'm no expert here..
kernel: kernel.bcl.o
	${_MKTARGET_LINK}
	${_CCLINK.kernel} \
		${_LDFLAGS.kernel} \
		-L${DESTDIR}/usr/lib \
		${_LDSTATIC.kernel} -o ${.TARGET} \
		${.TARGET}.bcl.o ${OBJS} ${_PROGLDOPTS} ${_LDADD.kernel} \
		-Wl,-plugin=${GOLD_PLUGIN} \
		-Wl,--allow-multiple-definition
.endif

# Disable magic and ASR passes for the kernel.
USE_MAGIC=no

# Disable coverage profiling for the kernel, at least for now.
MKCOVERAGE=no

.include <minix.service.mk>
