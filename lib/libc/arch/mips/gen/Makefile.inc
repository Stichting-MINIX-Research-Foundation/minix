#	$NetBSD: Makefile.inc,v 1.33 2011/03/15 07:40:18 matt Exp $

.if ${MKSOFTFLOAT} == "no"
SRCS+=	fabs.S ldexp.S modf.S
.else
SRCS+=	fabs_ieee754.c ldexp_ieee754.c modf_ieee754.c
.endif

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c infinityl_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c fpclassifyl_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c isinfl_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c isnanl_ieee754.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c isfinitel_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c signbitl_ieee754.c

SRCS+=	nanf.c
SRCS+=	flt_rounds.c

.if ${MKSOFTFLOAT} == "no"
SRCS+=	fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
.endif

SRCS+=	setjmp.S longjmp.c
SRCS+=	_setjmp.S
SRCS+=	sigsetjmp.S
SRCS+=	byte_swap_2.S byte_swap_4.S byte_swap_8.S

SRCS+=	makecontext.c resumecontext.c _resumecontext.S swapcontext.S _lwp.c

# mips abi builtin extensions (used by GCC for lexical-closure trampoline)
SRCS+=	cacheflush.c

CPPFLAGS._lwp.c	+= -D_LIBC_SOURCE

LSRCS.mips.gen=	Lint__setjmp.c Lint_bswap16.c Lint_bswap32.c Lint_swapcontext.c
LSRCS+=		${LSRCS.mips.gen}
DPSRCS+=	${LSRCS.mips.gen}
CLEANFILES+=	${LSRCS.mips.gen}
