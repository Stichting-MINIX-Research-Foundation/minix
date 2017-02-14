#
# Minimal libc for servers and drivers.
#

# LSC: TODO: Explaination of how this works
#.for f in \
#${f} ${f:C/\.o/.bc/}:	${NETBSDSRCDIR}//${f:C/\.o/.S/}
#OBJS+=	${f} ${f:C/\.o/.bc/}
#CLEANFILES+= ${f} ${f:C/\.o/.bc/}
#
#.if ${USE_BITCODE:Uno} == "yes"
#OBJS+=	${f:C/\.o/.bc/}
#CLEANFILES+= ${f:C/\.o/.bc/}
#.endif # ${USE_BITCODE:Uno} == "yes"

#.endfor

.include <bsd.own.mk>

LIB=		minc

MKPIC=		no # Never used as a dynamic library

LIBSADIR=	${NETBSDSRCDIR}/sys/lib/libsa
LIBSYSDIR=	${NETBSDSRCDIR}/minix/lib/libsys
LIBMINIXCDIR=	${NETBSDSRCDIR}/minix/lib/libc
LIBMINIXCARCHDIR= ${NETBSDSRCDIR}/minix/lib/libc/arch/${MACHINE_CPU}
LIBCDIR=	${NETBSDSRCDIR}/lib/libc
LIBCARCHDIR=	${LIBCDIR}/arch/${MACHINE_CPU}
LIBCOMMONCDIR=	${NETBSDSRCDIR}/common/lib/libc
LIBCOMMONCARCHDIR= ${LIBCOMMONCDIR}/arch/${MACHINE_CPU}

CPPFLAGS+=	-I${LIBCDIR}/include -I${LIBCDIR}
CPPFLAGS+=	-D_LIBMINC

CFLAGS+=	-fno-builtin

#
# Customized versions of libc functions.
#
SRCS+= atoi.c fputs.c _snprintf.c strtol.c
CPPFLAGS._snprintf.c+=	-I${LIBSADIR}

CPPFLAGS.strtol.c+=	-D_STANDALONE
CPPFLAGS.strtol.c+=	-I${LIBCOMMONCDIR}/stdlib
CPPFLAGS.strtol.c+=	-I${NETBSDSRCDIR}/sys

########################################################################
#
# Functions imported from libsa (StandAlone)
#
.for f in \
	errno.o printf.o strerror.o subr_prf.o
${f} ${f:C/\.o/.bc/}:	${LIBSADIR}/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBSADIR}/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

CPPFLAGS.fslib.c+= -I${NETBSDSRCDIR}/minix/fs

# LSC: I would like not to have to copy te file, but I can't
#      have libsa appear first in the .PATH, for a lot of files
#      would be taken from there, which is not the intention ATM.
CPPFLAGS.strerror.c+= -I${LIBSADIR}

# LSC: putchar and kputc have the same role / signature.
CPPFLAGS.subr_prf.c+= -Dputchar=kputc

# Activate optional support, may be deactivated.
CPPFLAGS.subr_prf.c+= -DLIBSA_PRINTF_LONGLONG_SUPPORT
CPPFLAGS.subr_prf.c+= -DLIBSA_PRINTF_WIDTH_SUPPORT

########################################################################
#
# Imports from libsys
#
.for f in \
	kputc.o sys_diagctl.o
${f} ${f:C/\.o/.bc/}:	${LIBSYSDIR}/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBSYSDIR}/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

########################################################################
#
# Shared libc with userspace (/common/lib/libc)
#

.for f in \
	bswap64.o rb.o
${f} ${f:C/\.o/.bc/}:	${LIBCOMMONCDIR}/gen/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCDIR}/gen/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

CPPFLAGS.rb.c+=		-D_LIBC

.for f in \
	sha2.o
${f} ${f:C/\.o/.bc/}:	${LIBCOMMONCDIR}/hash/sha2/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCDIR}/hash/sha2/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

CPPFLAGS.sha2.c+=	-I${NETBSDSRCDIR}/sys

.for f in \
	ashrdi3.o divdi3.o lshrdi3.o moddi3.o \
	udivdi3.o umoddi3.o umodsi3.o udivsi3.o \
	udivmoddi4.o divsi3.o modsi3.o divmoddi4.o \
	divmodsi4.o udivmodsi4.o #qdivrem.o lshldi3.o 
${f} ${f:C/\.o/.bc/}:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

COPTS+=  -Wno-missing-prototypes \
                -Wno-old-style-definition \
                -Wno-strict-prototypes \
                -Wno-uninitialized \
                -Wno-cast-qual



.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

.for f in \
	random.o strtoul.o
${f} ${f:C/\.o/.bc/}:	${LIBCOMMONCDIR}/stdlib/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCDIR}/stdlib/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

CPPFLAGS.strtoul.c+=	-D_STANDALONE
CPPFLAGS.strtoul.c+=	-I${NETBSDSRCDIR}/sys

.for f in \
	strcasecmp.o strcspn.o strncasecmp.o strnlen.o strlcat.o
${f} ${f:C/\.o/.bc/}:	${LIBCOMMONCDIR}/string/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCDIR}/string/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

########################################################################
#
# Functions imported directly from libc.
#

.include "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.libc.inc"

# This file is specifically generated
SRCS+= errlist.c

errlist.c: ${LIBCDIR}/gen/errlist.awk \
	   ${NETBSDSRCDIR}/sys/sys/errno.h
	${TOOL_CAT} ${NETBSDSRCDIR}/sys/sys/errno.h | ${TOOL_SED} 's/(_SIGN//' | ${TOOL_AWK} -f ${LIBCDIR}/gen/errlist.awk > ${.TARGET}
CLEANFILES+=	errlist.c

.for f in \
	_errno.o \
	getprogname.o setprogname.o execle.o sleep.o time.o \
	ctype_.o tolower_.o toupper_.o usleep.o waitpid.o sigsetops.o
${f} ${f:C/\.o/.bc/}:	${LIBCDIR}/gen/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCDIR}/gen/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

CPPFLAGS.ctype_.c+= -I${LIBCDIR}/locale
CPPFLAGS.isctype.c+= -I${LIBCDIR}/locale
CPPFLAGS.tolower_.c+= -I${LIBCDIR}/locale
CPPFLAGS.toupper_.c+= -I${LIBCDIR}/locale

.for f in \
	read_tsc_64.o fslib.o itoa.o
${f} ${f:C/\.o/.bc/}:	${LIBMINIXCDIR}/gen/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBMINIXCDIR}/gen/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

.for f in \
	initfini.o stack_protector.o
${f} ${f:C/\.o/.bc/}:	${LIBCDIR}/misc/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCDIR}/misc/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

CPPFLAGS.stack_protector.c+= -Dxprintf=printf

.for f in \
	regcomp.o regerror.o regexec.o regfree.o
${f} ${f:C/\.o/.bc/}:	${LIBCDIR}/regex/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCDIR}/regex/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

.for f in \
	abort.o atexit.o _env.o exit.o getenv.o \
	ldiv.o malloc.o setenv.o \
	reallocarr.o _rand48.o lrand48.o srand48.o
${f} ${f:C/\.o/.bc/}:	${LIBCDIR}/stdlib/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCDIR}/stdlib/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"

# LSC FIXME: Try to get this out of the loop
CPPFLAGS.${i}+=	-I${LIBCDIR}/stdlib
.endfor

CPPFLAGS.malloc.c+= -D_LIBSYS
# Avoid magic instrumentation of the malloc data variables, since the heap is
# reconstructed upon state transfer. We do need to instrument the malloc
# functions, since we need to hook their mmap/munmap calls.
SECTIONIFY.malloc.c+= -sectionify-no-override \
	-sectionify-data-section-map=.*/magic_malloc_data

.for f in \
	strdup.o
${f} ${f:C/\.o/.bc/}:	${LIBCDIR}/string/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCDIR}/string/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

.for f in \
	access.o brk.o close.o environ.o execve.o fork.o fsync.o \
	getgid.o getpid.o geteuid.o getuid.o gettimeofday.o getvfsstat.o \
	init.o kernel_utils.o kill.o link.o loadname.o lseek.o _mcontext.o \
	minix_rs.o mknod.o mmap.o nanosleep.o open.o pread.o pwrite.o read.o \
	sbrk.o select.o setuid.o sigprocmask.o stack_utils.o \
	stat.o stime.o svrctl.o syscall.o __sysctl.o _ucontext.o umask.o \
	unlink.o wait4.o write.o
${f} ${f:C/\.o/.bc/}:	${LIBMINIXCDIR}/sys/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBMINIXCDIR}/sys/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

.for f in \
	brksize.o _do_kernel_call_intr.o ipc_minix_kerninfo.o _ipc.o ucontext.o
${f} ${f:C/\.o/.bc/}:	${LIBMINIXCARCHDIR}/sys/${f:C/\.o/.S/}
${f} ${f:C/\.o/.go/}:	${LIBMINIXCARCHDIR}/sys/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

.for f in \
	localtime.o
${f} ${f:C/\.o/.bc/}:	${LIBCDIR}/time/${f:C/\.o/.c/}
${f} ${f:C/\.o/.go/}:	${LIBCDIR}/time/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}

.if ${USE_BITCODE:Uno} == "yes"
OBJS+=	${f:C/\.o/.bc/}
CLEANFILES+= ${f:C/\.o/.bc/}
.endif # ${USE_BITCODE:Uno} == "yes"
.endfor

ARCHSUBDIR=${MACHINE_CPU}
.include "${NETBSDSRCDIR}/minix/lib/libc/arch/${MACHINE_CPU}/Makefile.inc"

.include <bsd.lib.mk>
