#	$NetBSD: Makefile.inc,v 1.14 2013/11/08 22:42:52 matt Exp $

ARMV6= ${CPUFLAGS:M-march=armv6*} ${CPUFLAGS:M-mcpu=arm11*}
ARMV6+= ${CFLAGS:M-march=armv6*:} ${CFLAGS:M-mcpu=arm11*}
ARMV6+= ${CPPFLAGS:M-march=armv6*:} ${CPPFLAGS:M-mcpu=arm11*}
ARMV7= ${CPUFLAGS:M-march=armv7*} ${CPUFLAGS:M-mcpu=cortex*}
ARMV7+= ${CFLAGS:M-march=armv7*:} ${CFLAGS:M-mcpu=cortex*}
ARMV7+= ${CPPFLAGS:M-march=armv7*:} ${CPPFLAGS:M-mcpu=cortex*}
.if empty(CFLAGS:M-march=*) && empty(CFLAGS:M-mcpu=*) \
    && empty(CPPFLAGS:M-march=*) && empty(CPPFLAGS:M-mcpu=*) \
    && empty(CPUFLAGS:M-march=*) && empty(CPUFLAGS:M-mcpu=*)
ARMV6+= ${MACHINE_ARCH:Mearmv6*}
ARMV7+= ${MACHINE_ARCH:Mearmv7*}
.endif

.if defined(LIB) && (${LIB} == "kern" || ${LIB} == "c" || ${LIB} == "pthread" \
	|| ${LIB} == "rump" || (defined(__MINIX) && ${LIB} == "sys"))

.if empty(ARMV6) && empty(ARMV7)
SRCS.atomic+=	atomic_add_32_cas.c atomic_add_32_nv_cas.c \
		atomic_and_32_cas.c atomic_and_32_nv_cas.c \
		atomic_dec_32_cas.c atomic_dec_32_nv_cas.c \
		atomic_inc_32_cas.c atomic_inc_32_nv_cas.c \
		atomic_or_32_cas.c atomic_or_32_nv_cas.c \
		atomic_swap_32_cas.c membar_ops_nop.c
.else
.for op in add and cas nand or xor
.for sz in 8 16 32 64
SRCS.atomic+=	atomic_${op}_${sz}.S
.endfor
.endfor
SRCS.atomic+=	atomic_dec_32.S atomic_dec_64.S
SRCS.atomic+=	atomic_inc_32.S atomic_inc_64.S
SRCS.atomic+=	atomic_swap.S atomic_swap_16.S atomic_swap_64.S
SRCS.atomic+=	membar_ops.S
.for op in add and nand or sub xor
SRCS.atomic+=	sync_fetch_and_${op}_8.S
.endfor
.for sz in 1 2 4 8
SRCS.atomic+=	sync_bool_compare_and_swap_${sz}.S
.endfor
.endif

.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread" || ${LIB} == "rump")

SRCS.atomic+=	atomic_simplelock.c
.if empty(ARMV7)
CPUFLAGS.atomic_simplelock.c+=	-marm
.endif

.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread")

.if empty(ARMV6) && empty(ARMV7)
SRCS.atomic+=	atomic_init_testset.c
SRCS.atomic+=	atomic_cas_up.S
CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_UP
.else
SRCS.atomic+=	atomic_init_cas.c
.endif

.endif

.if !empty(ARMV6)
.for f in ${SRCS.atomic:M*.S}
CPUFLAGS.$f+=	-marm
.endfor
.endif

SRCS+=	${SRCS.atomic}
