#	$NetBSD: Makefile.inc,v 1.9 2009/01/04 17:54:29 pooka Exp $

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
.if ${MACHINE_ARCH} != "m68000"

SRCS+=	atomic_add.S atomic_and.S atomic_cas.S atomic_dec.S \
	atomic_inc.S atomic_or.S atomic_swap.S membar_ops_nop.c

.else

SRCS+=  atomic_add_32_cas.c atomic_add_32_nv_cas.c atomic_and_32_cas.c \
	atomic_and_32_nv_cas.c atomic_dec_32_cas.c atomic_dec_32_nv_cas.c \
	atomic_inc_32_cas.c atomic_inc_32_nv_cas.c atomic_or_32_cas.c \
	atomic_or_32_nv_cas.c atomic_swap_32_cas.c membar_ops_nop.c

.endif
.endif

.if defined(LIB) && (${LIB} == "c" || ${LIB} == "pthread")
.if ${MACHINE_ARCH} != "m68000"

SRCS+=	atomic_init_cas.c

.else

SRCS+=	atomic_init_testset.c
SRCS+=	atomic_cas_68000.S
CPPFLAGS+= -D__HAVE_ASM_ATOMIC_CAS_UP

.endif
.endif
