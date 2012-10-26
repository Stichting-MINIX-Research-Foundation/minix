#	$NetBSD: Makefile.inc,v 1.10 2012/09/11 20:51:25 matt Exp $

ARMV6= ${CPUFLAGS:M-march=armv7*} ${CPUFLAGS:M-mcpu=cortex*}
ARMV6+= ${CPUFLAGS:M-march=armv6*} ${CPUFLAGS:M-mcpu=arm11*}
ARMV6+= ${CFLAGS:M-march=armv7*:} ${CFLAGS:M-mcpu=cortex*}
ARMV6+= ${CFLAGS:M-march=armv6*:} ${CFLAGS:M-mcpu=arm11*}
ARMV6+= ${CPPFLAGS:M-march=armv7*:} ${CPPFLAGS:M-mcpu=cortex*}
ARMV6+= ${CPPFLAGS:M-march=armv6*:} ${CPPFLAGS:M-mcpu=arm11*}

.if defined(LIB) && (${LIB} == "kern" || ${LIB} == "c" || ${LIB} == "pthread" \
	|| ${LIB} == "rump" || (defined(__MINIX) && ${LIB} == "sys"))

.if empty(ARMV6)
SRCS.atomic+=	atomic_add_32_cas.c atomic_add_32_nv_cas.c \
		atomic_and_32_cas.c atomic_and_32_nv_cas.c \
		atomic_dec_32_cas.c atomic_dec_32_nv_cas.c \
		atomic_inc_32_cas.c atomic_inc_32_nv_cas.c \
		atomic_or_32_cas.c atomic_or_32_nv_cas.c \
		atomic_swap_32_cas.c membar_ops_nop.c
.else
SRCS.atomic+=	atomic_add_32.S atomic_and_32.S atomic_cas_32.S
SRCS.atomic+=	atomic_dec_32.S atomic_inc_32.S atomic_or_32.S
SRCS.atomic+=	atomic_swap.S membar_ops.S
SRCS.atomic+=	atomic_add_64.S atomic_and_64.S atomic_cas_64.S
SRCS.atomic+=	atomic_dec_64.S atomic_inc_64.S atomic_or_64.S
SRCS.atomic+=	atomic_swap_64.S
.endif

.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread")

.if empty(ARMV6)
SRCS.atomic+=	atomic_init_testset.c
SRCS.atomic+=	atomic_cas_up.S
CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_UP
.else
SRCS.atomic+=	atomic_init_cas.c
.endif

.endif

SRCS+=	${SRCS.atomic}
