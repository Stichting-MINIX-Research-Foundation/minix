#	$NetBSD: Makefile.inc,v 1.5 2009/01/04 17:54:29 pooka Exp $

.if defined(LIB) && (${LIB} == "kern" || ${LIB} == "c" || ${LIB} == "pthread" \
	|| ${LIB} == "rump")

SRCS+=	atomic_add.S atomic_and.S atomic_cas.S atomic_dec.S \
	atomic_inc.S atomic_or.S atomic_swap.S membar_ops.S

.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread")

SRCS+=	atomic_init_cas.c

.endif
