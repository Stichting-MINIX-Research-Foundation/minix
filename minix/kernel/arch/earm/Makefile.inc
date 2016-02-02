# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:	${HERE}

# objects we want unpaged from -lc
MINLIB_OBJS_UNPAGED= get_bp.o
get_bp.o:	${NETBSDSRCDIR}/minix/lib/libc/arch/arm/get_bp.S 

# objects we want unpaged from -lsys
SYS_OBJS_UNPAGED=assert.o stacktrace.o
assert.o:	${NETBSDSRCDIR}/minix/lib/libsys/assert.c
stacktrace.o:	${NETBSDSRCDIR}/minix/lib/libsys/stacktrace.c

# objects we want unpaged from -lminc
MINC_OBJS_UNPAGED= atoi.o \
	printf.o subr_prf.o \
	strcmp.o strcpy.o strlen.o strncmp.o \
	memcpy.o memmove.o memset.o 
MINC_OBJS_UNPAGED+= divmodsi4.o divsi3.o udivsi3.o umodsi3.o \
	umoddi3.o udivmoddi4.o __aeabi_idiv0.o aeabi_idivmod.o aeabi_uidivmod.o \
	udivmodsi4.o aeabi_uldivmod.o
atoi.o:		${NETBSDSRCDIR}/minix/lib/libminc/atoi.c
printf.o:	${NETBSDSRCDIR}/sys/lib/libsa/printf.c
subr_prf.o:	${NETBSDSRCDIR}/sys/lib/libsa/subr_prf.c
memcpy.o:	${NETBSDSRCDIR}/common/lib/libc/arch/arm/string/memcpy.S
memmove.o:	${NETBSDSRCDIR}/common/lib/libc/arch/arm/string/memmove.S
memset.o:	${NETBSDSRCDIR}/common/lib/libc/arch/arm/string/memset.S
strlen.o:	${NETBSDSRCDIR}/common/lib/libc/arch/arm/string/strlen.S
strcpy.o:	${NETBSDSRCDIR}/common/lib/libc/arch/arm/string/strcpy.S
strcmp.o:	${NETBSDSRCDIR}/common/lib/libc/arch/arm/string/strcmp.S
__aeabi_idiv0.o:	${NETBSDSRCDIR}/common/lib/libc/arch/arm/gen/__aeabi_idiv0.c
CPPFLAGS.__aeabi_idiv0.c+= -D_STANDALONE -I${NETBSDSRCDIR}/sys

divsi3.o:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/divsi3.c
udivsi3.o:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/udivsi3.c
umodsi3.o:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/umodsi3.c
umoddi3.o:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/umoddi3.c
udivmoddi4.o:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/udivmoddi4.c
divmodsi4.o:		${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/arm/divmodsi4.S
udivmodsi4.o:		${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/arm/udivmodsi4.S
aeabi_idivmod.o:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/arm/aeabi_idivmod.S
aeabi_uidivmod.o:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/arm/aeabi_uidivmod.S
aeabi_uldivmod.o:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/arm/aeabi_uldivmod.S

# the following is required by pre_init.c
strncmp.o:	${NETBSDSRCDIR}/common/lib/libc/string/strncmp.c

# LSC: putchar and kputc have the same signature. A bit hackish.
CPPFLAGS.subr_prf.c+= -Dputchar=kputc

# Activate optional support, may be deactivated.
CPPFLAGS.subr_prf.c+= -DLIBSA_PRINTF_LONGLONG_SUPPORT -DLIBSA_PRINTF_WIDTH_SUPPORT

.include "bsp/ti/Makefile.inc"

# some object files we give a symbol prefix (or namespace) of __k_unpaged_
# that must live in their own unique namespace.
#
.for unpaged_obj in head.o pre_init.o direct_tty_utils.o \
	pg_utils.o klib.o utility.o arch_reset.o \
	${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} ${SYS_OBJS_UNPAGED} ${BSP_OBJS_UNPAGED}
unpaged_${unpaged_obj}: ${unpaged_obj}
	${OBJCOPY} --prefix-symbols=__k_unpaged_ ${.OBJDIR}/${unpaged_obj} $@
UNPAGED_OBJS += unpaged_${unpaged_obj}
ORIG_UNPAGED_OBJS += ${unpaged_obj}
.endfor


CLEANFILES+= ${ORIG_UNPAGED_OBJS}

SRCS+=	mpx.S arch_clock.c arch_do_vmctl.c arch_system.c do_padconf.c \
	exception.c hw_intr.c klib.S memory.c \
	protect.c direct_tty_utils.c arch_reset.c \
	pg_utils.c phys_copy.S phys_memset.S exc.S

OBJS.kernel+=	${UNPAGED_OBJS}

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

