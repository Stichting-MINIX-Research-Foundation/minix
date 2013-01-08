
# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:	${HERE}

# objects we want unpaged from -lminlib, -lminc
MINLIB_OBJS_UNPAGED=_cpufeature.o _cpuid.o get_bp.o
MINC_OBJS_UNPAGED=strcat.o strlen.o memcpy.o strcpy.o strncmp.o memset.o \
	memmove.o strcmp.o atoi.o ctype_.o _stdfile.o strtol.o _errno.o errno.o \
	udivdi3.o umoddi3.o qdivrem.o
SYS_OBJS_UNPAGED=kprintf.o vprintf.o assert.o stacktrace.o

# some object files we give a symbol prefix (or namespace) of __k_unpaged_
# that must live in their own unique namespace.
#
.for UNPAGED_OBJ in head.o pre_init.o direct_tty_utils.o io_outb.o \
	io_inb.o pg_utils.o klib.o utility.o arch_reset.o \
	${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} ${SYS_OBJS_UNPAGED}
unpaged_${UNPAGED_OBJ}: ${UNPAGED_OBJ}
	${OBJCOPY} --prefix-symbols=__k_unpaged_ ${UNPAGED_OBJ} unpaged_${UNPAGED_OBJ}
UNPAGED_OBJS += unpaged_${UNPAGED_OBJ}
ORIG_UNPAGED_OBJS += ${UNPAGED_OBJ}
.endfor

# we have to extract some object files from libminc.a and libminlib.a
${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} ${SYS_OBJS_UNPAGED}: ${LIBMINLIB} ${LIBMINC} ${LIBSYS}
	${AR} x ${LIBMINLIB} ${MINLIB_OBJS_UNPAGED}
	${AR} x ${LIBMINC} ${MINC_OBJS_UNPAGED}
	${AR} x ${LIBSYS} ${SYS_OBJS_UNPAGED}

CLEANFILES+= $(ORIG_UNPAGED_OBJS)

SRCS+=	mpx.S arch_clock.c arch_do_vmctl.c arch_system.c \
	do_iopenable.c do_readbios.c do_sdevio.c exception.c i8259.c io_inb.S \
	io_inl.S io_intr.S io_inw.S io_outb.S io_outl.S io_outw.S klib.S memory.c \
	oxpcie.c protect.c direct_tty_utils.c arch_reset.c \
	pg_utils.c usermapped_glo_ipc.S usermapped_data_arch.c

OBJS.kernel+=	${UNPAGED_OBJS}

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

sconst.h: procoffsets.h
apic_asm.S head.S klib16.S klib.S mpx.S: sconst.h
SRCS+= procoffsets.h
CPPFLAGS+=-I.

# It's OK to hardcode the arch as i386 here as this and procoffsets.cf
# are i386-specific.
TMP=procoffsets.h.tmp
INCLS=../include/arch/i386/include/
PROCOFFSETSCF=procoffsets.cf

procoffsets.h: ${PROCOFFSETSCF} kernel.h proc.h ${INCLS}/stackframe.h ${INCLS}/archtypes.h
	${_MKTARGET_CREATE}
	${TOOL_CAT} ${HERE}/${PROCOFFSETSCF} | \
	  ${TOOL_GENASSYM} -- ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} >$TMP && \
	  mv -f $TMP $@ 

.ifdef CONFIG_SMP
SRCS += arch_smp.c trampoline.S
.endif


