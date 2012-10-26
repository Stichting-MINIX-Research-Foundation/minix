
# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:	${HERE}

# objects we want unpaged from -lminlib, -lminc
MINLIB_OBJS_UNPAGED=get_bp.o
MINC_OBJS_UNPAGED=strcat.o strlen.o memcpy.o strcpy.o strncmp.o memset.o \
	memmove.o strcmp.o atoi.o ctype_.o _stdfile.o strtol.o _errno.o errno.o
#	udivdi3.o umoddi3.o qdivrem.o
SYS_OBJS_UNPAGED=kprintf.o vprintf.o assert.o stacktrace.o
LIBGCC_OBJS_UNPAGED=_divsi3.o _udivsi3.o _divdi3.o  _udivdi3.o _umoddi3.o \
	_dvmd_tls.o _aeabi_uldivmod.o _clzsi2.o bpabi.o

# some object files we give a symbol prefix (or namespace) of __k_unpaged_
# that must live in their own unique namespace.
#
.for UNPAGED_OBJ in head.o pre_init.o direct_tty_utils.o \
	pg_utils.o klib.o omap_serial.o utility.o arch_reset.o \
	${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} ${SYS_OBJS_UNPAGED} ${LIBGCC_OBJS_UNPAGED}
unpaged_${UNPAGED_OBJ}: ${UNPAGED_OBJ}
	${OBJCOPY} --prefix-symbols=__k_unpaged_ ${UNPAGED_OBJ} unpaged_${UNPAGED_OBJ}
UNPAGED_OBJS += unpaged_${UNPAGED_OBJ}
ORIG_UNPAGED_OBJS += ${UNPAGED_OBJ}
.endfor

# we have to extract some object files from libminc.a and libminlib.a
${MINLIB_OBJS_UNPAGED} ${MINC_OBJS_UNPAGED} ${SYS_OBJS_UNPAGED} ${LIBGCC_OBJS_UNPAGED}: ${LIBMINLIB} ${LIBMINC} ${LIBSYS} ${LIBGCC}
	${AR} x ${LIBMINLIB} ${MINLIB_OBJS_UNPAGED}
	${AR} x ${LIBMINC} ${MINC_OBJS_UNPAGED}
	${AR} x ${LIBSYS} ${SYS_OBJS_UNPAGED}
	${AR} x ${LIBGCC} ${LIBGCC_OBJS_UNPAGED}

CLEANFILES+= $(ORIG_UNPAGED_OBJS)

SRCS+=	mpx.S arch_clock.c arch_do_vmctl.c arch_system.c \
	omap_serial.c omap_timer.c omap_intr.c exception.c \
	io_intr.S klib.S memory.c \
	protect.c direct_tty_utils.c arch_reset.c \
	pg_utils.c phys_copy.S phys_memset.S exc.S
OBJS.kernel+=	${UNPAGED_OBJS}

klib.d mpx.d head.d: procoffsets.h

# It's OK to hardcode the arch as arm here as this and procoffsets.cf
# are arm-specific.
TMP=procoffsets.h.tmp
INCLS=../include/arch/arm/include/
PROCOFFSETSCF=procoffsets.cf

procoffsets.h: ${PROCOFFSETSCF} kernel.h proc.h ${INCLS}/stackframe.h ${INCLS}/archtypes.h
	${_MKTARGET_CREATE}
	${TOOL_CAT} ${HERE}/${PROCOFFSETSCF} | \
	  ${TOOL_GENASSYM} -- ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} >$TMP && \
	  mv -f $TMP $@ 

