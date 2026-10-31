# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

# x86_64 kernel must use kernel code model and no red zone
CFLAGS += -mcmodel=kernel -mno-red-zone

# The kernel must NOT use MMX/SSE/x87: it doesn't save or restore the FPU/vector
# register state for its own execution, and SSE is the x86_64 codegen default
# (so the compiler would otherwise emit e.g. movaps for struct copies, which
# also #GPs on the kernel's unaligned stack locals).  Force integer-only code.
CFLAGS += -mno-mmx -mno-sse -mno-sse2 -mno-3dnow -msoft-float

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:	${HERE}

# objects we want unpaged from -lc
MINLIB_OBJS_UNPAGED= _cpufeature.o _cpuid.o get_bp.o
_cpufeature.o:	${NETBSDSRCDIR}/minix/lib/libc/arch/${MACHINE_ARCH}/_cpufeature.c
_cpuid.o:	${NETBSDSRCDIR}/minix/lib/libc/arch/${MACHINE_ARCH}/_cpuid.S
get_bp.o:	${NETBSDSRCDIR}/minix/lib/libc/arch/${MACHINE_ARCH}/get_bp.S

# objects we want unpaged from -lsys
SYS_OBJS_UNPAGED=assert.o stacktrace.o
assert.o:	${NETBSDSRCDIR}/minix/lib/libsys/assert.c
stacktrace.o:	${NETBSDSRCDIR}/minix/lib/libsys/stacktrace.c

# x86_64: the kernel lives at 0xFFFFFFFF80400000.  All objects linked into it
# must use -mcmodel=kernel (R_X86_64_32S, sign-extended).  Pre-built library
# archives (-lsys, -lexec, -lminc) use -mcmodel=small (R_X86_64_32,
# zero-extended), which overflows at link time.
#
# Fix: compile fresh copies of every needed library object in the kernel
# build context (which inherits CFLAGS += -mcmodel=kernel), then add them
# to OBJS.kernel so the linker resolves those symbols BEFORE reaching the
# archives.  The unpaged objects (MINLIB_OBJS_UNPAGED etc.) are already
# compiled here for the unpaged section; we just also link them paged.

# libexec — only two source files, both needed
exec_elf.o:     ${NETBSDSRCDIR}/minix/lib/libexec/exec_elf.c
exec_general.o: ${NETBSDSRCDIR}/minix/lib/libexec/exec_general.c
CLEANFILES+=    exec_elf.o exec_general.o

# libminc extras not already covered by MINC_OBJS_UNPAGED:
#   init.o       — provides _minix_kerninfo / _minix_ipcvecs globals
#   _errno.o     — provides errno + __errno()
init.o:         ${NETBSDSRCDIR}/minix/lib/libc/sys/init.c
_errno.o:       ${NETBSDSRCDIR}/lib/libc/gen/_errno.c
CPPFLAGS._errno.c+= -I${NETBSDSRCDIR}/lib/libc/include -U_REENTRANT
CLEANFILES+=    init.o _errno.o

# libsys objects the kernel pulls in: shadow-compile so they get the kernel
# CFLAGS (-mcmodel=kernel and, crucially, -mno-sse) instead of the SSE-enabled
# -mcmodel=small versions in libsys.a — the kernel must never execute SSE.
cpuavg.o:       ${NETBSDSRCDIR}/minix/lib/libsys/cpuavg.c
vm_procctl.o:   ${NETBSDSRCDIR}/minix/lib/libsys/vm_procctl.c
CLEANFILES+=    cpuavg.o vm_procctl.o

OBJS.kernel+=   ${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} \
                ${SYS_OBJS_UNPAGED} \
                exec_elf.o exec_general.o init.o _errno.o \
                cpuavg.o vm_procctl.o

# objects we want unpaged from -lminc
MINC_OBJS_UNPAGED= atoi.o \
	printf.o subr_prf.o \
	strcmp.o strcpy.o strlen.o strncmp.o \
	memcpy.o memmove.o memset.o \
	udivdi3.o umoddi3.o qdivrem.o
atoi.o:		${NETBSDSRCDIR}/minix/lib/libminc/atoi.c
printf.o:	${NETBSDSRCDIR}/sys/lib/libsa/printf.c
subr_prf.o:	${NETBSDSRCDIR}/sys/lib/libsa/subr_prf.c
memcpy.o:	${NETBSDSRCDIR}/common/lib/libc/arch/${MACHINE_ARCH}/string/memcpy.S
memmove.o:	${NETBSDSRCDIR}/common/lib/libc/arch/${MACHINE_ARCH}/string/memmove.S
memset.o:	${NETBSDSRCDIR}/common/lib/libc/arch/${MACHINE_ARCH}/string/memset.S
strlen.o:	${NETBSDSRCDIR}/common/lib/libc/arch/${MACHINE_ARCH}/string/strlen.S
strcpy.o:	${NETBSDSRCDIR}/common/lib/libc/arch/${MACHINE_ARCH}/string/strcpy.S
strcmp.o:	${NETBSDSRCDIR}/common/lib/libc/arch/${MACHINE_ARCH}/string/strcmp.S

# the following is required by pre_init.c
strncmp.o:	${NETBSDSRCDIR}/common/lib/libc/string/strncmp.c

# these are required by kprintn.o:
udivdi3.o:	${NETBSDSRCDIR}/common/lib/libc/quad/udivdi3.c
umoddi3.o:	${NETBSDSRCDIR}/common/lib/libc/quad/umoddi3.c
qdivrem.o:	${NETBSDSRCDIR}/common/lib/libc/quad/qdivrem.c

# LSC: putchar and kputc have the same signature. A bit hackish.
CPPFLAGS.subr_prf.c+= -Dputchar=kputc

# Activate optional support, may be deactivated.
CPPFLAGS.subr_prf.c+= -DLIBSA_PRINTF_LONGLONG_SUPPORT -DLIBSA_PRINTF_WIDTH_SUPPORT

# some object files we give a symbol prefix (or namespace) of __k_unpaged_
# that must live in their own unique namespace.
#
.for unpaged_obj in head.o pre_init.o mb2_getparam.o \
	direct_tty_utils.o direct_com_utils.o \
	pg_utils.o klib.o utility.o arch_reset.o \
	io_inb.o io_outb.o \
	${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} ${SYS_OBJS_UNPAGED}

CLEANFILES+=${unpaged_obj}.bin
.if ${USE_BITCODE:Uno} == "yes"
unpaged_${unpaged_obj}: ${unpaged_obj}
	if file -b ${.OBJDIR}/${unpaged_obj} | grep -q '^LLVM'; then \
		${LLC} -O1 -march=x86-64 -mcpu=x86-64 -filetype=obj -o ${.OBJDIR}/${unpaged_obj}.bin ${.OBJDIR}/${unpaged_obj}; \
	else \
		cp ${.OBJDIR}/${unpaged_obj} ${.OBJDIR}/${unpaged_obj}.bin; \
	fi
	${OBJCOPY} --prefix-symbols=__k_unpaged_ ${.OBJDIR}/${unpaged_obj}.bin $@
.else
unpaged_${unpaged_obj}: ${unpaged_obj}
	${OBJCOPY} --prefix-symbols=__k_unpaged_ ${.OBJDIR}/${unpaged_obj} $@
.endif
UNPAGED_OBJS += unpaged_${unpaged_obj}
ORIG_UNPAGED_OBJS += ${unpaged_obj}
.endfor

CLEANFILES+= ${ORIG_UNPAGED_OBJS}

SRCS+=	mpx.S arch_clock.c arch_do_vmctl.c arch_system.c \
	klib.S memory.c protect.c  arch_reset.c pg_utils.c \
	direct_com_utils.c direct_tty_utils.c

SRCS+=	do_iopenable.c do_readbios.c do_sdevio.c exception.c i8259.c oxpcie.c \
	usermapped_data_arch.c \
	io_inb.S io_inl.S io_intr.S io_inw.S io_outb.S io_outl.S io_outw.S \
	usermapped_glo_ipc.S

OBJS.kernel+=	${UNPAGED_OBJS}

.ifdef CONFIG_SMP
SRCS += arch_smp.c trampoline.S
trampoline.o: trampoline.S
	${CC} ${AFLAGS} ${AFLAGS.${<:T}} ${CPPFLAGS} -c ${.IMPSRC} -o ${.TARGET}
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

klib.o mpx.o head.o: procoffsets.h

SRCS+= procoffsets.h

PROCOFFSETSCF=procoffsets.cf

.PATH: ${NETBSDSRCDIR}/minix/include/arch/${MACHINE_ARCH}/include

procoffsets.h: ${PROCOFFSETSCF} kernel.h proc.h stackframe.h archtypes.h
	${_MKTARGET_CREATE}
	${TOOL_CAT} ${HERE}/${PROCOFFSETSCF} | \
		${TOOL_GENASSYM} -- ${CC} ${CFLAGS:N-Wa,*} \
		${CPPFLAGS} ${PROF} ${${USE_BITCODE:Uno} == "yes":? -fno-lto:} \
		${GENASSYM_CPPFLAGS} >$@.tmp && \
	  mv -f $@.tmp $@ 

sconst.h: procoffsets.h
apic_asm.o head.o klib.o mpx.o: sconst.h

