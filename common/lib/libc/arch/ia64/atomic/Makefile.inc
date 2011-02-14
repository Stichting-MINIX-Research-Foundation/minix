#	$NetBSD: Makefile.inc,v 1.3 2009/01/04 17:54:29 pooka Exp $

.if defined(LIB) && (${LIB} == "kern" || ${LIB} == "c"  || ${LIB} == "pthread" \
	|| ${LIB} == "rump")

SRCS+=	atomic_add_32_cas.c atomic_add_32_nv_cas.c \
	atomic_add_64_cas.c atomic_add_64_nv_cas.c \
	atomic_and_32_cas.c atomic_and_32_nv_cas.c \
	atomic_and_64_cas.c atomic_and_64_nv_cas.c \
	atomic_or_32_cas.c atomic_or_32_nv_cas.c \
	atomic_or_64_cas.c atomic_or_64_nv_cas.c

SRCS+=	atomic.S

.endif

.if defined(LIB) && (${LIB} == "c"  || ${LIB} == "pthread")

SRCS += atomic_init_cas.c

.endif
