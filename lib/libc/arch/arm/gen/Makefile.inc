# $NetBSD: Makefile.inc,v 1.28 2014/01/29 23:37:18 joerg Exp $

SRCS+=	alloca.S byte_swap_2.S byte_swap_4.S bswap64.c \
	fabs_ieee754.c flt_rounds.c

CPUFLAGS.divide.S+=		-marm

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c infinityl_dbl_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c

.if !empty(LIBC_MACHINE_ARCH:Mearm*)
SRCS+=	__aeabi_fcmpun.c __aeabi_dcmpun.c
SRCS+=	__aeabi_idiv0.c __aeabi_ldiv0.c
.endif

AFLAGS._setjmp.S=	-mfpu=vfp
AFLAGS.setjmp.S=	-mfpu=vfp

SRCS+=	nanf.c

SRCS+=	setjmp.S
SRCS+=	_setjmp.S
SRCS+=	sigsetjmp.S

.if defined(__MINIX)
# Already defined in minix/lib/libc/arch/arm/sys
.else
SRCS+=	makecontext.c resumecontext.c swapcontext.S

SRCS+=	_lwp.c
.endif

LSRCS.arm.gen=	Lint_bswap16.c Lint_bswap32.c Lint_swapcontext.c
LSRCS+=		${LSRCS.arm.gen}
DPSRCS+=	${LSRCS.arm.gen}
CLEANFILES+=	${LSRCS.arm.gen}
