#	$NetBSD: Makefile.inc,v 1.1 2014/09/03 19:34:25 matt Exp $

.for op in add and cas nand or swap xor
.for sz in 32
SRCS.atomic+=	atomic_${op}_${sz}.S
.endfor
.for sz in 8 16
SRCS.atomic+=	atomic_${op}_${sz}_cas.c
.endfor
.endfor
SRCS.atomic+=	atomic_dec_32.S atomic_inc_32.S
SRCS.atomic+=	membar_ops.S
SRCS.atomic+=	atomic_cas_by_cas32.c
.if defined(LIB) && ${LIB} != "kern"
SRCS.atomic+=	sync_bool_compare_and_swap_4.S
.endif

#.if defined(LIB) && (${LIB} != "kern")
#SRCS.atomic+=	atomic_simplelock.c
#.endif

.if defined(LIB) && (${LIB} != "kern" && ${LIB} != "rump")

SRCS.atomic+=	atomic_init_cas.c

.endif #LIB

SRCS+=	${SRCS.atomic}
