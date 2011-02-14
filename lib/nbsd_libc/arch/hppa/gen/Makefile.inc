#	$NetBSD: Makefile.inc,v 1.15 2006/06/28 14:46:32 drochner Exp $

SRCS+=	bswap16.c bswap32.c bswap64.c
SRCS+=	fabs.c
SRCS+=	__sigsetjmp14.S __setjmp14.S _setjmp.S __longjmp14.c

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c infinityl_dbl_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c

SRCS+=	nanf.c

SRCS+=	makecontext.c resumecontext.c _resumecontext.S swapcontext.S _lwp.c

SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c
