# $NetBSD: Makefile.inc,v 1.2 2015/07/07 21:40:19 matt Exp $

SRCS+=	byte_swap_2.S byte_swap_4.S byte_swap_8.S
SRCS+=	flt_rounds.c

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c infinityl_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c fpclassifyl_ieee754.c
SRCS+=	fpgetmask.c fpgetround.c fpgetsticky.c
SRCS+=	fpsetmask.c fpsetround.c fpsetsticky.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c isfinitel_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c isinfl_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c isnanl_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c signbitl_ieee754.c
SRCS+=	fixunsgen_ieee754.c fixunsgen64_ieee754.c
SRCS+=	fixunstfsi_ieee754.c fixunstfdi_ieee754.c
SRCS+=	fabs_ieee754.S
#SRCS+=	fabsf_ieee754.S fabsl_ieee754.c

SRCS+=	nanf.c

SRCS+=	setjmp.S
SRCS+=	_setjmp.S
SRCS+=	sigsetjmp.S

SRCS+=	makecontext.c resumecontext.c swapcontext.S

SRCS+=	_lwp.c

#LSRCS.arm.gen=	Lint_bswap16.c Lint_bswap32.c Lint_swapcontext.c
#LSRCS+=		${LSRCS.arm.gen}
#DPSRCS+=	${LSRCS.arm.gen}
#CLEANFILES+=	${LSRCS.arm.gen}
