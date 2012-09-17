#	$NetBSD: Makefile.inc,v 1.31 2011/11/22 15:25:28 joerg Exp $

SRCS+=	alloca.S fabs.S

# Common ieee754 constants and functions
SRCS+=	infinityf_ieee754.c infinity_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c
SRCS+=	isfinitef_ieee754.c isfinited_ieee754.c
SRCS+=	signbitf_ieee754.c signbitd_ieee754.c

SRCS+=	nanf.c

# 68000-based machines use a double-extended `long double' type
# for which the generic ieee754 versions can be used
.if	${MACHINE_ARCH} == "m68000"
SRCS+=	infinityl_dbl_ieee754.c
.else
SRCS+=	infinityl.c
SRCS+=	fpclassifyl.c isfinitel.c isinfl.c isnanl.c signbitl.c
.endif

SRCS+=	ashlsi3.S ashrsi3.S \
	lshlsi3.S lshrsi3.S \
	negdf2.S negsf2.S
SRCS+=	bswap16.S bswap32.S bswap64.S
SRCS+=	_lwp.c
CPPFLAGS._lwp.c	+= -D_LIBC_SOURCE

# 68000-based machines build with a libgcc that includes
# much of the (soft)float and integer support that would 
# otherwise be compiled here.
.if	(${MACHINE_ARCH} == "m68000" || ${MKSOFTFLOAT} == "yes")
SRCS+=	modf_ieee754.c			# generic ieee754 version
SRCS+=	flt_rounds_softfloat.S
.if ${MKSOFTFLOAT} != "yes"
SRCS+=	fpfake.c
.endif
.else
SRCS+=	modf.S
SRCS+=	flt_rounds.S fpgetmask.S fpgetround.S fpgetsticky.S fpsetmask.S \
	fpsetround.S fpsetsticky.S
SRCS+=	adddf3.S addsf3.S cmpdf2.S cmpsf2.S divdf3.S \
	divsf3.S divsi3.S extendsfdf2.S fixdfsi.S fixunsdfsi.S \
	floatsidf.S modsi3.S muldf3.S mulsf3.S mulsi3.S \
	subdf3.S subsf3.S truncdfsf2.S udivsi3.S \
	umodsi3.S umulsi3.S
.endif

SRCS+=	setjmp.S longjmp.c
SRCS+=	_setjmp.S
SRCS+=	sigsetjmp.S

SRCS+=	makecontext.c resumecontext.S swapcontext.S

.ifdef M68040
SRCS+=	ldexp_ieee754.c			# generic ieee754 version
.elifdef MKSOFTFLOAT
SRCS+=	ldexp_ieee754.c			# generic ieee754 version
.elifdef M68060
SRCS+=	ldexp_ieee754.c			# generic ieee754 version
.elif	${MACHINE_ARCH} == "m68000"
SRCS+=	ldexp_ieee754.c			# generic ieee754 version
.else
SRCS+=	ldexp_881.c
.endif

LSRCS.m68k.gen=	Lint_bswap16.c Lint_bswap32.c Lint_bswap64.c \
		Lint_resumecontext.c Lint_swapcontext.c
LSRCS+=		${LSRCS.m68k.gen}
DPSRCS+=	${LSRCS.m68k.gen}
CLEANFILES+=	${LSRCS.m68k.gen}
