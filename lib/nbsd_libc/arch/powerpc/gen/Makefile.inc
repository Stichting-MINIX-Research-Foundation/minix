#	$NetBSD: Makefile.inc,v 1.24 2009/12/06 07:12:17 uebayasi Exp $

SRCS+=	__setjmp14.S __sigsetjmp14.S _setjmp.S
SRCS+=	bswap16.c bswap32.c bswap64.c
SRCS+=	fabs.c flt_rounds.c
SRCS+=	syncicache.c
SRCS+=  _lwp.c makecontext.c resumecontext.c swapcontext.S

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c infinityl_dbl_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c

SRCS+=	nanf.c

.if (${MKSOFTFLOAT} == "no")
SRCS+=	fpgetround.c fpsetround.c fpgetmask.c fpsetmask.c
SRCS+=	fpgetsticky.c fpsetsticky.c
.endif

LSRCS.powerpc.gen=Lint_swapcontext.c
LSRCS+=		${LSRCS.powerpc.gen}
DPSRCS+=	${LSRCS.powerpc.gen}
CLEANFILES+=	${LSRCS.powerpc.gen}
