#	$NetBSD: Makefile.inc,v 1.14 2014/11/20 07:07:13 matt Exp $

#
# Note: The atomic operations here in these assembly files are atomic
# only with respect to regular memory on uniprocessor systems.  Since
# we don't support any MP m68k systems, this is just fine.  If we ever
# do, then these routines will probably need to be replaced with CAS-
# based routines (CAS generates an atomic bus cycle, whereas these
# others are merely single-instruction).
#

.if defined(LIB) && (${LIB} == "kern" || ${LIB} == "c" || ${LIB} == "pthread" \
	|| ${LIB} == "rump")
.if ${MACHINE_ARCH} == "m68k"

SRCS+=	atomic_add.S atomic_and.S atomic_cas.S atomic_dec.S \
	atomic_inc.S atomic_or.S atomic_swap.S membar_ops_nop.c \
	atomic_sub.S atomic_xor.S atomic_nand.S

.else

SRCS+=  atomic_add_32_cas.c atomic_add_32_nv_cas.c atomic_and_32_cas.c \
	atomic_and_32_nv_cas.c atomic_dec_32_cas.c atomic_dec_32_nv_cas.c \
	atomic_inc_32_cas.c atomic_inc_32_nv_cas.c atomic_or_32_cas.c \
	atomic_or_32_nv_cas.c atomic_swap_32_cas.c membar_ops_nop.c

.if ${LIB} == "c"
SRCS+=  atomic_add_16_cas.c atomic_add_8_cas.c	\
	atomic_sub_32_cas.c atomic_sub_16_cas.c atomic_sub_8_cas.c	\
	atomic_nand_32_cas.c atomic_nand_16_cas.c atomic_nand_8_cas.c	\
	atomic_xor_32_cas.c atomic_xor_16_cas.c atomic_xor_8_cas.c	\
	atomic_and_32_cas.c atomic_and_16_cas.c atomic_and_8_cas.c	\
	atomic_or_32_cas.c atomic_or_16_cas.c atomic_or_8_cas.c		\
	atomic_cas_32_cas.c atomic_cas_16_cas.c atomic_cas_8_cas.c	\
	atomic_swap_32_cas.c atomic_swap_16_cas.c atomic_swap_8_cas.c

SRCS+=	atomic_c11_compare_exchange_cas_8.c \
	atomic_c11_compare_exchange_cas_16.c \
	atomic_c11_compare_exchange_cas_32.c
.endif

.endif
.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread")
.if ${MACHINE_ARCH} == "m68k"

SRCS+=	atomic_init_cas.c

.else

SRCS+=	atomic_init_testset.c
SRCS+=	atomic_cas_68000.S
CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_UP -D__HAVE_ASM_ATOMIC_CAS_16_UP \
	 -D__HAVE_ASM_ATOMIC_CAS_8_UP

.endif
.endif
