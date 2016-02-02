#	$NetBSD: Makefile.inc,v 1.29 2015/06/07 15:06:24 matt Exp $

.ifnmake obj
.include "${NETBSDSRCDIR}/common/lib/libc/arch/arm/features.mk"
.endif

.if defined(LIB) && (${LIB} == "kern" || ${LIB} == "c" || ${LIB} == "pthread" \
	|| ${LIB} == "rump" || (defined(__MINIX) && ${LIB} == "sys"))

.if "${FEAT_LDREX}" == "no"
SRCS.atomic+=	atomic_add_32_cas.c atomic_add_32_nv_cas.c \
		atomic_and_32_cas.c atomic_and_32_nv_cas.c \
		atomic_dec_32_cas.c atomic_dec_32_nv_cas.c \
		atomic_inc_32_cas.c atomic_inc_32_nv_cas.c \
		atomic_or_32_cas.c atomic_or_32_nv_cas.c \
		atomic_swap_32_cas.c membar_ops_nop.c

.if ${LIB} == "c"
SRCS.atomic+=	atomic_xor_32_cas.c atomic_xor_16_cas.c atomic_xor_8_cas.c \
		atomic_sub_32_cas.c atomic_sub_16_cas.c atomic_sub_8_cas.c \
		atomic_nand_32_cas.c atomic_nand_16_cas.c atomic_nand_8_cas.c \
		atomic_or_16_cas.c atomic_or_8_cas.c \
		atomic_and_16_cas.c atomic_and_8_cas.c \
		atomic_add_16_cas.c atomic_add_8_cas.c \
		atomic_swap_16_cas.c atomic_swap_8_cas.c \
		atomic_cas_32_cas.c atomic_cas_16_cas.c atomic_cas_8_cas.c \
		atomic_load.c atomic_store.c \
		atomic_and_16_nv_cas.c atomic_and_8_nv_cas.c	\
		atomic_c11_compare_exchange_cas_32.c	\
		atomic_c11_compare_exchange_cas_16.c	\
		atomic_c11_compare_exchange_cas_8.c
.endif

.else
.for op in add and cas nand or xor
.for sz in 8 16 32 64
SRCS.atomic+=	atomic_${op}_${sz}.S
.endfor
.endfor
SRCS.atomic+=	atomic_dec_32.S atomic_dec_64.S
SRCS.atomic+=	atomic_inc_32.S atomic_inc_64.S
SRCS.atomic+=	atomic_sub_64.S
SRCS.atomic+=	atomic_swap.S atomic_swap_16.S atomic_swap_64.S
SRCS.atomic+=	membar_ops.S
.if defined(LIB) && ${LIB} != "kern" && ${LIB} != "rump"
.for op in add and nand or sub xor
SRCS.atomic+=	sync_fetch_and_${op}_8.S
.endfor
.for sz in 1 2 4 8
SRCS.atomic+=	sync_bool_compare_and_swap_${sz}.S
.endfor
.endif
.endif

.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread" || ${LIB} == "rump")

SRCS.atomic+=	atomic_simplelock.c
.if "${FEAT_THUMB2}" == "no"
CPUFLAGS.atomic_simplelock.c+=	-marm
.endif

.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread")

.if "${FEAT_LDREX}" == "no"
SRCS.atomic+=	atomic_init_testset.c
SRCS.atomic+=	atomic_cas_up.S
CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_UP
CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_16_UP
CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_8_UP
.if "${FEAT_EABI}" == "yes" && "${FEAT_LDRD}" == "yes"
CPPFLAGS+= -D__HAVE_ATOMIC_CAS_64_UP -D__HAVE_ASM_ATOMIC_CAS_64_UP
.endif
.else
SRCS.atomic+=	atomic_init_cas.c
.endif #FEAT_LDREX

.endif #LIB

.if "${FEAT_THUMB2}" == "no"
.for f in ${SRCS.atomic:M*.S}
CPUFLAGS.$f+=	-marm
.endfor
.endif

SRCS+=	${SRCS.atomic}
