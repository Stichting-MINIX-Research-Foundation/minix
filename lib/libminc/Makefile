#
# Minimal libc for servers and drivers.
#

.include <bsd.own.mk>

LIB=		minc

CPPFLAGS.sched_start.c+=	-I${MINIXSRCDIR}

LIBCSRCDIR=${MINIXSRCDIR}/lib/nbsd_libc

MACHINE_ARCH=${MACHINE}

#
# Shared libc with userspace (/common/lib/libc)
#
.include "${MINIXSRCDIR}/common/lib/libc/Makefile.inc"
.include "${.CURDIR}/arch/${MACHINE}/Makefile.libc.inc"
.if empty(SRCS:Mbyte_swap_8.*)
SRCS+= bswap64.c
.endif

.if (${MACHINE_ARCH} != "alpha") && \
    (${MACHINE_ARCH} != "mips64eb") && \
    (${MACHINE_ARCH} != "mips64el") && \
    (${MACHINE_ARCH} != "powerpc64") && \
    (${MACHINE_ARCH} != "sparc64") && \
    (${MACHINE_ARCH} != "x86_64")
# Quad support
SRCS+= 	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c \
	iordi3.c lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c \
	notdi2.c qdivrem.c subdi3.c ucmpdi2.c udivdi3.c umoddi3.c \
	xordi3.c
.endif
.for i in \
	random.c strtoll.c strtoul.c strtoull.c strtoumax.c \
	memchr.c memcmp.c memcpy.c memmove.c popcount32.c popcount64.c \
	strcasecmp.c strcat.c strchr.c strcmp.c strcpy.c strlcat.c \
	strlcpy.c strlen.c strncasecmp.c strncmp.c strncpy.c strrchr.c \
	strsep.c strstr.c ptree.c rb.c sha1.c sha2.c
SRCS+= ${i}
CPPFLAGS.${i}+= -D_LIBC -I${LIBCSRCDIR}/include
.endfor
.if empty(SRCS:Mmemset2.*)
SRCS+=	memset.c
.endif

#
# Customized versions of libc functions.
#
SRCS+= 	calloc.c malloc.c
SRCS+= 	fputs.c _stdfile.c
CPPFLAGS.fputs.c+= -I${LIBCSRCDIR}/include


#
# Functions imported directly from the libc.
#

# Import from stdio
.for i in sprintf.c fprintf.c perror.c vsnprintf.c snprintf.c
.PATH.c: ${LIBCSRCDIR}/stdio
CPPFLAGS.${i}= -I${LIBCSRCDIR}/stdio -I${LIBCSRCDIR}/include
SRCS+=	${i}
.endfor
# Import from stdlib
.for i in abort.c atexit.c atoi.c exit.c getenv.c \
	getopt.c ldiv.c setenv.c
.PATH.c: ${LIBCSRCDIR}/stdlib
SRCS+= ${i}
CPPFLAGS.${i}+= -I${LIBCSRCDIR}/stdlib -I${LIBCSRCDIR}/include 
.endfor
# Import from gen

errlist.c: ${LIBCSRCDIR}/gen/errlist.awk \
	   ${MINIXSRCDIR}/nbsd_include/sys/errno.h
	cat ${MINIXSRCDIR}/nbsd_include/sys/errno.h | ${TOOL_SED} 's/(_SIGN//' | ${TOOL_AWK} -f ${LIBCSRCDIR}/gen/errlist.awk > ${.TARGET}

.for i in assert.c _errno.c errno.c _env.c errlist.c \
	execle.c getprogname.c sleep.c time.c \
	ctype_.c isctype.c tolower_.c toupper_.c \
	infinityf_ieee754.c infinity_ieee754.c \
	fpclassifyf_ieee754.c fpclassifyd_ieee754.c \
	isinff_ieee754.c isinfd_ieee754.c \
	isnanf_ieee754.c isnand_ieee754.c \
	isfinitef_ieee754.c isfinited_ieee754.c \
	signbitf_ieee754.c signbitd_ieee754.c \
	nanf.c infinityl.c 
.PATH.c: ${LIBCSRCDIR}/arch/${MACHINE}/gen ${LIBCSRCDIR}/gen/minix \
	${LIBCSRCDIR}/gen ${LIBCSRCDIR}
SRCS+= ${i}
CPPFLAGS.${i}+= -I${LIBCSRCDIR}/include
.endfor
CPPFLAGS.ctype_.c+= -I${LIBCSRCDIR}/locale
CPPFLAGS.isctype.c+= -I${LIBCSRCDIR}/locale
CPPFLAGS.tolower_.c+= -I${LIBCSRCDIR}/locale
CPPFLAGS.toupper_.c+= -I${LIBCSRCDIR}/locale
# Import from regex
.for i in regcomp.c regerror.c regexec.c regfree.c
.PATH.c: ${LIBCSRCDIR}/regex
SRCS+= ${i}
CPPFLAGS.${i}+= -I${LIBCSRCDIR}/include
.endfor
# Import from string
.for i in strcspn.c strerror.c strerror_r.c strtol.c \
	bcmp.S bcopy.S bzero.S 
.PATH.S: ${LIBCSRCDIR}/arch/${MACHINE}/string
.PATH.c: ${LIBCSRCDIR}/string
SRCS+= ${i}
CPPFLAGS.${i}+= -D_LIBC -I${LIBCSRCDIR}/include
.endfor
CPPFLAGS.strtol.c+=     -I${LIBCSRCDIR}/../../common/lib/libc/stdlib
# Import from time
.for i in asctime.c localtime.c strftime.c
.PATH.c: ${LIBCSRCDIR}/time
SRCS+= ${i}
CPPFLAGS.${i}+= -I${LIBCSRCDIR}/include
.endfor
# Import from locale
.for i in _def_time.c
.PATH.c: ${LIBCSRCDIR}/locale
SRCS+= ${i}
CPPFLAGS.${i}+= -I${LIBCSRCDIR}/include -I${LIBCSRCDIR}/locale
.endfor
# Import from sys-minix
.for i in access.c brk.c close.c environ.c execve.c fork.c \
	getgid.c getpid.c getuid.c gettimeofday.c loadname.c \
	link.c mknod.c mmap.c nanosleep.c open.c read.c sbrk.c \
	select.c setuid.c stat.c stime.c syscall.c umask.c \
	unlink.c waitpid.c \
	brksize.S _ipc.S _senda.S
.PATH.c: ${LIBCSRCDIR}/sys-minix
.PATH.S: ${LIBCSRCDIR}/arch/${MACHINE}/sys-minix
SRCS+= ${i}
CPPFLAGS.${i}+= -I${LIBCSRCDIR}/include
.endfor

CPPFLAGS+= -D_LIBSYS

.include <bsd.lib.mk>
