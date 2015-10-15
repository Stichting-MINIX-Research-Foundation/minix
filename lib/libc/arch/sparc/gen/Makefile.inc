#	$NetBSD: Makefile.inc,v 1.23 2014/07/12 19:21:48 nakayama Exp $

SRCS+=	fabs.S modf.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c fpsetsticky.c

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c infinityl_dbl_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c

SRCS+=	nanf.c

SRCS+=	setjmp.S longjmp.c
SRCS+=	_setjmp.S
SRCS+=	sigsetjmp.S

SRCS+=	makecontext.c resumecontext.c _lwp.c swapcontext.S

.if ${MACHINE} == "sparc64"
.PATH: ${NETBSDSRCDIR}/lib/libarch/sparc/v8
SRCS+=	sparc_v8.S
.else
SRCS+=	mul.S umul.S
.endif
SRCS+=	fixunsdfsi.S saveregs.S
SRCS+=	bswap16.c bswap32.c bswap64.c

LSRCS.sparc.gen=	Lint_swapcontext.c
LSRCS+=		${LSRCS.sparc.gen}
DPSRCS+=	${LSRCS.sparc.gen}
CLEANFILES+=	${LSRCS.sparc.gen}
