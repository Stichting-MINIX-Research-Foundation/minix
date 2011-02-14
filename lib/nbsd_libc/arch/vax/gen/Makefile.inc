#	$NetBSD: Makefile.inc,v 1.17 2010/07/06 05:59:52 mrg Exp $

SRCS+=	byte_swap_2.S byte_swap_4.S bswap64.S \
	fabs.S frexp.c \
	fpclassifyf.c fpclassifyd.c \
	infinityf.c infinity.c infinityl.c \
	isfinitef.c isfinited.c \
	ldexp.S \
	makecontext.c modf.S \
	resumecontext.c \
	signbitf.c signbitd.c \
	swapcontext.S \
	udiv.S urem.S \
	__setjmp14.S __sigsetjmp14.S _lwp.c _setjmp.S 

# ABI compatibility for libc.so.12
SRCS+=	isinf.c isnan.c

LSRCS.vax.gen=	Lint_bswap16.c Lint_bswap32.c Lint_bswap64.c
LSRCS+=		${LSRCS.vax.gen}
DPSRCS+=	${LSRCS.vax.gen}
CLEANFILES+=	${LSRCS.vax.gen}
