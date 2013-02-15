# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:	${HERE}

# objects we want unpaged from -lminlib
MINLIB_OBJS_UNPAGED=get_bp.o
get_bp.o:	${NETBSDSRCDIR}/lib/libminlib/${MACHINE_ARCH}/get_bp.S

# objects we want unpaged from -lsys
SYS_OBJS_UNPAGED=assert.o kprintf.o stacktrace.o vprintf.o
assert.o:	${NETBSDSRCDIR}/lib/libsys/assert.c
kprintf.o:	${NETBSDSRCDIR}/lib/libsys/kprintf.c
stacktrace.o:	${NETBSDSRCDIR}/lib/libsys/stacktrace.c
vprintf.o:	${NETBSDSRCDIR}/lib/libsys/vprintf.c

# objects we want unpaged from -lminc
MINC_OBJS_UNPAGED=findfp.o strcat.o strlen.o memcpy.o strcpy.o strncmp.o \
	memset.o memmove.o strcmp.o atoi.o ctype_.o strtol.o \
	_errno.o errno.o udivdi3.o umoddi3.o qdivrem.o
MINC_OBJS_UNPAGED+= divsi3.o __aeabi_uldivmod.o 
#_udivsi3.o _divdi3.o  _udivdi3.o _umoddi3.o _dvmd_tls.o _clzsi2.o bpabi.o
memcpy.o:	${NETBSDSRCDIR}/common/lib/libc/string/memcpy.c
memmove.o:	${NETBSDSRCDIR}/common/lib/libc/string/memmove.c
memset.o:	${NETBSDSRCDIR}/common/lib/libc/string/memset.c
strcat.o:	${NETBSDSRCDIR}/common/lib/libc/string/strcat.c
strlen.o:	${NETBSDSRCDIR}/common/lib/libc/string/strlen.c
strcpy.o:	${NETBSDSRCDIR}/common/lib/libc/string/strcpy.c
strcmp.o:	${NETBSDSRCDIR}/common/lib/libc/string/strcmp.c
strncmp.o:	${NETBSDSRCDIR}/common/lib/libc/string/strncmp.c
udivdi3.o:	${NETBSDSRCDIR}/common/lib/libc/quad/udivdi3.c
umoddi3.o:	${NETBSDSRCDIR}/common/lib/libc/quad/umoddi3.c
qdivrem.o:	${NETBSDSRCDIR}/common/lib/libc/quad/qdivrem.c
atoi.o:		${NETBSDSRCDIR}/lib/libc/stdlib/atoi.c
strtol.o:	${NETBSDSRCDIR}/lib/libc/stdlib/strtol.c
findfp.o:	${NETBSDSRCDIR}/lib/libc/stdio/findfp.c
ctype_.o:	${NETBSDSRCDIR}/lib/libc/gen/ctype_.c
_errno.o:	${NETBSDSRCDIR}/lib/libc/gen/_errno.c
errno.o:	${NETBSDSRCDIR}/lib/libc/gen/errno.c
divsi3.o:	${NETBSDSRCDIR}/common/lib/libc/arch/${MACHINE_GNU_ARCH}/gen/divsi3.S
__aeabi_uldivmod.o:	${NETBSDSRCDIR}/common/lib/libc/arch/${MACHINE_GNU_ARCH}/quad/__aeabi_uldivmod.S

# Specific flags needed to compile a few of the object files.
CPPFLAGS.strtol.c+= -I ${NETBSDSRCDIR}/common/lib/libc/stdlib
CPPFLAGS.ctype_.c+= -I ${NETBSDSRCDIR}/lib/libc/locale
CPPFLAGS._errno.c+= -I ${NETBSDSRCDIR}/lib/libc/include
CPPFLAGS.assert.c+= -I ${NETBSDSRCDIR}/lib/libc/include
CPPFLAGS.findfp.c+= -I ${NETBSDSRCDIR}/lib/libc/include

# some object files we give a symbol prefix (or namespace) of __k_unpaged_
# that must live in their own unique namespace.
#
.for unpaged_obj in head.o pre_init.o direct_tty_utils.o \
	pg_utils.o klib.o omap_serial.o utility.o arch_reset.o \
	${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} ${SYS_OBJS_UNPAGED}
unpaged_${unpaged_obj}: ${unpaged_obj}
	${OBJCOPY} --prefix-symbols=__k_unpaged_ ${.OBJDIR}/${unpaged_obj} $@
UNPAGED_OBJS += unpaged_${unpaged_obj}
ORIG_UNPAGED_OBJS += ${unpaged_obj}
.endfor

CLEANFILES+= ${ORIG_UNPAGED_OBJS}

SRCS+=	mpx.S arch_clock.c arch_do_vmctl.c arch_system.c \
	omap_serial.c omap_timer.c omap_intr.c exception.c \
	klib.S memory.c \
	protect.c direct_tty_utils.c arch_reset.c \
	pg_utils.c phys_copy.S phys_memset.S exc.S
OBJS.kernel+=	${UNPAGED_OBJS}

klib.o mpx.o head.o: procoffsets.h

SRCS+= procoffsets.h

PROCOFFSETSCF=procoffsets.cf

.PATH: ${NETBSDSRCDIR}/include/arch/${MACHINE_ARCH}/include

procoffsets.h: ${PROCOFFSETSCF} kernel.h proc.h stackframe.h archtypes.h
	${_MKTARGET_CREATE}
	${TOOL_CAT} ${HERE}/${PROCOFFSETSCF} | \
	  ${TOOL_GENASSYM} -- ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} >$@.tmp && \
	  mv -f $@.tmp $@ 

sconst.h: procoffsets.h
apic_asm.o head.o klib.o mpx.o: sconst.h

