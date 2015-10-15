#	$NetBSD: Makefile.inc,v 1.18 2014/10/13 08:59:41 martin Exp $

.include <bsd.own.mk>

# This is called with MACHINE=sparc64 && MACHINE_ARCH=sparc when building
# 32 bit kernels for ultrasparc hardware
.if ${MACHINE} == "sparc64"

SPARC64DIR=	${.PARSEDIR}/../../sparc64/atomic
.PATH.c: ${SPARC64DIR}
.PATH.S: ${SPARC64DIR}

.include "${SPARC64DIR}/Makefile.inc"

.else

. if defined(LIB)

.  if (${LIB} == "kern" || ${LIB} == "c"  || ${LIB} == "pthread"	\
    || ${LIB} == "rump")
SRCS+=	atomic_add_32_cas.c atomic_add_32_nv_cas.c atomic_and_32_cas.c \
	atomic_and_32_nv_cas.c atomic_dec_32_cas.c atomic_dec_32_nv_cas.c \
	atomic_inc_32_cas.c atomic_inc_32_nv_cas.c atomic_or_32_cas.c \
	atomic_or_32_nv_cas.c atomic_swap_32_cas.c membar_ops.S	\
	atomic_cas_32_cas.c atomic_nand_32_cas.c atomic_sub_32_cas.c	\
	atomic_xor_32_cas.c
.  endif

.  if ${LIB} == "c"
SRCS+=	atomic_nand_16_cas.c atomic_nand_8_cas.c	\
	atomic_and_16_cas.c atomic_and_8_cas.c		\
	atomic_xor_16_cas.c atomic_xor_8_cas.c		\
	atomic_add_16_cas.c atomic_add_8_cas.c		\
	atomic_cas_16_cas.c atomic_cas_8_cas.c		\
	atomic_sub_16_cas.c atomic_sub_8_cas.c		\
	atomic_or_16_cas.c atomic_or_8_cas.c		\
	atomic_swap_16_cas.c atomic_swap_8_cas.c	\
	atomic_c11_compare_exchange_cas_32.c		\
	atomic_c11_compare_exchange_cas_16.c		\
	atomic_c11_compare_exchange_cas_8.c
.  endif

.  if (${LIB} == "kern" || ${LIB} == "rump")
SRCS+=	atomic_cas.S
.  endif

.  if (${LIB} == "c" || ${LIB} == "pthread")
CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_UP -D__HAVE_ASM_ATOMIC_CAS_16_UP	\
	-D__HAVE_ASM_ATOMIC_CAS_8_UP
SRCS+=	atomic_init_testset.c atomic_cas_up.S
.  endif

. endif

.endif
