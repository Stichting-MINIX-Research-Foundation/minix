# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:	${HERE}

# objects we want unpaged from -lminlib
MINLIB_OBJS_UNPAGED=_cpufeature.o _cpuid.o get_bp.o
_cpufeature.o:	${NETBSDSRCDIR}/lib/libminlib/${MACHINE_ARCH}/_cpufeature.c
_cpuid.o:	${NETBSDSRCDIR}/lib/libminlib/${MACHINE_ARCH}/_cpuid.S
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

# Specific flags needed to compile a few of the object files.
CPPFLAGS.strtol.c+= -I ${NETBSDSRCDIR}/common/lib/libc/stdlib
CPPFLAGS.ctype_.c+= -I ${NETBSDSRCDIR}/lib/libc/locale
CPPFLAGS._errno.c+= -I ${NETBSDSRCDIR}/lib/libc/include
CPPFLAGS.assert.c+= -I ${NETBSDSRCDIR}/lib/libc/include
CPPFLAGS.findfp.c+= -I ${NETBSDSRCDIR}/lib/libc/include

# some object files we give a symbol prefix (or namespace) of __k_unpaged_
# that must live in their own unique namespace.
#
.for unpaged_obj in head.o pre_init.o direct_tty_utils.o io_outb.o \
	io_inb.o pg_utils.o klib.o utility.o arch_reset.o \
	${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} ${SYS_OBJS_UNPAGED}
unpaged_${unpaged_obj}: ${unpaged_obj}
	${OBJCOPY} --prefix-symbols=__k_unpaged_ ${.OBJDIR}/${unpaged_obj} $@
UNPAGED_OBJS += unpaged_${unpaged_obj}
ORIG_UNPAGED_OBJS += ${unpaged_obj}
.endfor

CLEANFILES+= ${ORIG_UNPAGED_OBJS}

SRCS+=	mpx.S arch_clock.c arch_do_vmctl.c arch_system.c \
	do_iopenable.c do_readbios.c do_sdevio.c exception.c i8259.c io_inb.S \
	io_inl.S io_intr.S io_inw.S io_outb.S io_outl.S io_outw.S klib.S memory.c \
	oxpcie.c protect.c direct_tty_utils.c arch_reset.c \
	pg_utils.c usermapped_glo_ipc.S usermapped_data_arch.c

OBJS.kernel+=	${UNPAGED_OBJS}

.ifdef CONFIG_SMP
SRCS += arch_smp.c trampoline.S
.endif

.if ${USE_ACPI} != "no"
SRCS+= acpi.c
CPPFLAGS+= -DUSE_ACPI
.endif

.if ${USE_APIC} != "no"
SRCS+= apic.c apic_asm.S
CPPFLAGS+= -DUSE_APIC
.endif

.if ${USE_DEBUGREG} != "no"
SRCS+=  breakpoints.c debugreg.S
.endif

.if ${USE_WATCHDOG} != "no"
SRCS+= arch_watchdog.c
CPPFLAGS+= -DUSE_WATCHDOG
.endif

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

