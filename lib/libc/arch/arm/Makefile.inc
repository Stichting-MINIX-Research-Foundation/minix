# $NetBSD: Makefile.inc,v 1.16 2012/09/27 11:20:20 skrll Exp $

.include <bsd.own.mk>

.if !defined(__MINIX)
SRCS+=	__aeabi_read_tp.S __sigaction14_sigtramp.c __sigtramp2.S
.endif

CPPFLAGS += -I.
CPPFLAGS += -DSOFTFLOAT

.if ${MACHINE_ARCH} == "earm" || ${MACHINE_ARCH} == "earmeb" || defined(__MINIX)
SRCS+=	__aeabi_ldivmod.S __aeabi_uldivmod.S
SRCS+=	__aeabi_lcmp.c __aeabi_ulcmp.c
SRCS+=	fixunsgen_ieee754.c fixunssfsi_ieee754.c
SRCS+=	fixunsgen64_ieee754.c fixunsdfsi_ieee754.c
.endif

# for earm, use the 64-bit softfloat
.if ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "armeb"
SOFTFLOAT_BITS=32
.endif

.include <softfloat/Makefile.inc>
