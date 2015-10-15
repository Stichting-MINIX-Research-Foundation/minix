#	$NetBSD: Makefile.inc,v 1.13 2014/10/13 13:00:55 martin Exp $

.if defined(LIB) && (${LIB} == "kern" || ${LIB} == "c" || ${LIB} == "pthread" \
	|| ${LIB} == "rump")

.if ${MACHINE_ARCH:Mmips64*} == ""
SRCS+=	atomic_add_32_cas.c atomic_add_32_nv_cas.c \
	atomic_and_32_cas.c atomic_and_32_nv_cas.c \
	atomic_dec_32_cas.c atomic_dec_32_nv_cas.c \
	atomic_inc_32_cas.c atomic_inc_32_nv_cas.c \
	atomic_or_32_cas.c atomic_or_32_nv_cas.c \
	atomic_swap_32_cas.c \
	atomic_add_64_cas.c atomic_add_64_nv_cas.c \
	atomic_and_64_cas.c atomic_and_64_nv_cas.c \
	atomic_dec_64_cas.c atomic_dec_64_nv_cas.c \
	atomic_inc_64_cas.c atomic_inc_64_nv_cas.c \
	atomic_or_64_cas.c atomic_or_64_nv_cas.c \
	atomic_swap_64_cas.c

CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_UP

.else
SRCS+=	atomic_add.S atomic_dec.S atomic_inc.S
SRCS+=	atomic_and.S atomic_or.S
SRCS+=	atomic_swap.S
.endif

SRCS+=	membar_ops.S
.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread")

.if ${MACHINE_ARCH:Mmips64*} == ""
SRCS+=	atomic_init_testset.c atomic_cas_up.S
.else
SRCS+=	atomic_cas.S atomic_init_cas.c atomic_cas_by_cas32.c
.endif

.endif

.if defined(LIB) && ${LIB} == "c"
SRCS+=	atomic_xor_32_cas.c atomic_xor_16_cas.c atomic_xor_8_cas.c	\
	atomic_sub_32_cas.c atomic_sub_16_cas.c atomic_sub_8_cas.c	\
	atomic_nand_32_cas.c atomic_nand_16_cas.c atomic_nand_8_cas.c	\
	atomic_or_16_cas.c atomic_or_8_cas.c				\
	atomic_and_16_cas.c atomic_and_8_cas.c				\
	atomic_add_16_cas.c atomic_add_8_cas.c				\
	atomic_swap_16_cas.c atomic_swap_8_cas.c			\
	atomic_cas_32_cas.c atomic_cas_16_cas.c atomic_cas_8_cas.c	\
	atomic_c11_compare_exchange_cas_32.c				\
	atomic_c11_compare_exchange_cas_16.c				\
	atomic_c11_compare_exchange_cas_8.c
.endif

.if defined(LIB) && (${LIB} == "kern" || ${LIB} == "rump")

SRCS+=	membar_ops.S

.endif
