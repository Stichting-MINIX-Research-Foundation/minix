#	$NetBSD: Makefile.inc,v 1.25 2009/12/06 07:12:17 uebayasi Exp $

SRCS+=	setjmp.S _setjmp.S sigsetjmp.S

SRCS+=	makecontext.c resumecontext.c swapcontext.S

SRCS+=	_lwp.c

SRCS+=	fabs.c flt_rounds.c bswap16.c bswap32.c bswap64.c

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c infinityl_dbl_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c

SRCS+=	nanf.c

LSRCS.sh3.gen=	Lint__setjmp.c Lint___setjmp14.c Lint___sigsetjmp14.c \
		Lint_swapcontext.c

LSRCS+=		${LSRCS.sh3.gen}
DPSRCS+=	${LSRCS.sh3.gen}
CLEANFILES+=	${LSRCS.sh3.gen}
