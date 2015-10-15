# $NetBSD: Makefile.inc,v 1.28 2015/08/20 16:13:40 pooka Exp $

.include <bsd.own.mk>

.if ${RUMPRUN} != "yes" && !defined(__MINIX)
SRCS+=	__aeabi_read_tp.S __sigaction14_sigtramp.c __sigtramp2.S
.endif

.if empty(LIBC_MACHINE_ARCH:Mearmv7*)
AFLAGS+= -marm
.endif

CPPFLAGS += -I.

.if !empty(LIBC_MACHINE_ARCH:Mearm*)
SRCS+=	arm_initfini.c
.endif

.if ${MKSOFTFLOAT} != "no"
CPPFLAGS += -DSOFTFLOAT

# for earm, use the 64-bit softfloat
.if ${LIBC_MACHINE_ARCH} == "arm" || ${LIBC_MACHINE_ARCH} == "armeb"
SOFTFLOAT_BITS=32
SRCS+=	floatunsidf_ieee754.c floatunsisf_ieee754.c
.endif

.include <softfloat/Makefile.inc>
.else
.PATH : ${ARCHDIR}/hardfloat
SRCS.hardfloat+=	fpgetround.c fpgetsticky.S fpgetmask.S fabs_ieee754.S
SRCS.hardfloat+=	fpsetround.c fpsetmask.S fpsetsticky.S
.for f in ${SRCS.hardfloat}
CPPFLAGS.${f} += -mfpu=vfp
.endfor
SRCS+=	${SRCS.hardfloat}
.endif
