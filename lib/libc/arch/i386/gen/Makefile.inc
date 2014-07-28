#	$NetBSD: Makefile.inc,v 1.27 2011/03/26 19:51:41 christos Exp $

# objects built from assembler sources (need lint stubs)
SRCS+=	alloca.S byte_swap_2.S byte_swap_4.S fabs.S \
	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S \
	fpsetmask.S fpsetround.S fpsetsticky.S fpgetprec.S \
	fpsetprec.S

SRCS+=	setjmp.S _setjmp.S sigsetjmp.S

.if defined(__MINIX)
# Already defined in minix/lib/libc/arch/i386/sys
.else
SRCS+=	resumecontext.S swapcontext.S
.endif

# objects built from C sources
.if defined(__MINIX)
SRCS+=	bswap64.c
.else
SRCS+=	bswap64.c _lwp.c makecontext.c
.endif

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c

SRCS+=	nanf.c infinityl.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c signbitl.c

# "internal" objects (don't provide part of the user-visible API)
SRCS+=	divsi3.S fixdfsi.S fixunsdfsi.S udivsi3.S

LSRCS.i386.gen=\
	Lint__setjmp.c Lint_alloca.c Lint_bswap16.c Lint_bswap32.c \
	Lint_fabs.c \
	Lint___setjmp14.c Lint___sigsetjmp14.c Lint_flt_rounds.c \
	Lint_fpgetmask.c Lint_fpgetround.c Lint_fpgetsticky.c \
	Lint_fpsetmask.c Lint_fpsetround.c Lint_fpsetsticky.c \
	Lint_resumecontext.c Lint_swapcontext.c Lint__setjmp.c
LSRCS+=		${LSRCS.i386.gen}
DPSRCS+=	${LSRCS.i386.gen}
CLEANFILES+=	${LSRCS.i386.gen}
