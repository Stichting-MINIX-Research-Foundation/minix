#	$NetBSD: Makefile.inc,v 1.32 2013/07/18 21:34:33 matt Exp $

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
.if	${MACHINE_ARCH} != "m68k"
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
.endif

.if ${MACHINE_ARCH} == "m68k"
SRCS+=	divsi3.S modsi3.S mulsi3.S udivsi3.S umodsi3.S umulsi3.S
.endif

SRCS+=	setjmp.S longjmp.c
SRCS+=	_setjmp.S
SRCS+=	sigsetjmp.S

SRCS+=	makecontext.c resumecontext.S swapcontext.S

SRCS+=	ldexp_ieee754.c			# generic ieee754 version

LSRCS.m68k.gen=	Lint_bswap16.c Lint_bswap32.c Lint_bswap64.c \
		Lint_resumecontext.c Lint_swapcontext.c
LSRCS+=		${LSRCS.m68k.gen}
DPSRCS+=	${LSRCS.m68k.gen}
CLEANFILES+=	${LSRCS.m68k.gen}
