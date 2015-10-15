#	$NetBSD: Makefile,v 1.16 2013/12/19 01:10:19 gdt Exp $

.include <bsd.init.mk>

# We don't install mpfr, just provide it for GCC.
LIBISPRIVATE?= yes

.if ${LIBISPRIVATE} != "yes"
INCS=         mpf2mpfr.h mpfr.h
INCSDIR=      /usr/include
.endif

# XXX get_d.c triggers lint div zero errors (which is valid?)
MKLINT=no

LIB=	mpfr

LIBDPLIBS+= gmp ${.CURDIR}/../../../gmp/lib/libgmp

SRCS=	\
	abort_prec_max.c \
	acos.c \
	acosh.c \
	add.c \
	add1.c \
	add1sp.c \
	add_d.c \
	add_ui.c \
	agm.c \
	ai.c \
	asin.c \
	asinh.c \
	atan.c \
	atan2.c \
	atanh.c \
	bernoulli.c \
	buildopt.c \
	cache.c \
	cbrt.c \
	check.c \
	clear.c \
	clears.c \
	cmp.c \
	cmp2.c \
	cmp_abs.c \
	cmp_d.c \
	cmp_ld.c \
	cmp_si.c \
	cmp_ui.c \
	comparisons.c \
	const_catalan.c \
	const_euler.c \
	const_log2.c \
	const_pi.c \
	constant.c \
	copysign.c \
	cos.c \
	cosh.c \
	cot.c \
	coth.c \
	csc.c \
	csch.c \
	d_div.c \
	d_sub.c \
	digamma.c \
	dim.c \
	div.c \
	div_2exp.c \
	div_2si.c \
	div_2ui.c \
	div_d.c \
	div_ui.c \
	dump.c \
	eint.c \
	eq.c \
	erf.c \
	erfc.c \
	exp.c \
	exp10.c \
	exp2.c \
	exp3.c \
	exp_2.c \
	expm1.c \
	factorial.c \
	fits_intmax.c \
	fits_sint.c \
	fits_slong.c \
	fits_sshort.c \
	fits_uint.c \
	fits_uintmax.c \
	fits_ulong.c \
	fits_ushort.c \
	fma.c \
	fms.c \
	frac.c \
	free_cache.c \
	frexp.c \
	gamma.c \
	gammaonethird.c \
	get_d.c \
	get_d64.c \
	get_exp.c \
	get_f.c \
	get_flt.c \
	get_ld.c \
	get_patches.c \
	get_si.c \
	get_sj.c \
	get_str.c \
	get_ui.c \
	get_uj.c \
	get_z.c \
	get_z_exp.c \
	gmp_op.c \
	grandom.c \
	hypot.c \
	init.c \
	init2.c \
	inits.c \
	inits2.c \
	inp_str.c \
	int_ceil_log2.c \
	isinf.c \
	isinteger.c \
	isnan.c \
	isnum.c \
	isqrt.c \
	isregular.c \
	iszero.c \
	jn.c \
	li2.c \
	lngamma.c \
	log.c \
	log10.c \
	log1p.c \
	log2.c \
	logging.c \
	min_prec.c \
	minmax.c \
	modf.c \
	mp_clz_tab.c \
	mpfr-gmp.c \
	mpn_exp.c \
	mul.c \
	mul_2exp.c \
	mul_2si.c \
	mul_2ui.c \
	mul_d.c \
	mul_ui.c \
	mulders.c \
	neg.c \
	next.c \
	out_str.c \
	pow.c \
	pow_si.c \
	pow_ui.c \
	pow_z.c \
	powerof2.c \
	print_raw.c \
	print_rnd_mode.c \
	printf.c \
	rec_sqrt.c \
	reldiff.c \
	rem1.c \
	rint.c \
	root.c \
	round_near_x.c \
	round_p.c \
	round_prec.c \
	scale2.c \
	sec.c \
	sech.c \
	set.c \
	set_d.c \
	set_d64.c \
	set_dfl_prec.c \
	set_exp.c \
	set_f.c \
	set_flt.c \
	set_inf.c \
	set_ld.c \
	set_nan.c \
	set_prc_raw.c \
	set_prec.c \
	set_q.c \
	set_rnd.c \
	set_si.c \
	set_si_2exp.c \
	set_sj.c \
	set_str.c \
	set_str_raw.c \
	set_ui.c \
	set_ui_2exp.c \
	set_uj.c \
	set_z.c \
	set_z_exp.c \
	set_zero.c \
	setmax.c \
	setmin.c \
	setsign.c \
	sgn.c \
	si_op.c \
	signbit.c \
	sin.c \
	sin_cos.c \
	sinh.c \
	sinh_cosh.c \
	sqr.c \
	sqrt.c \
	sqrt_ui.c \
	stack_interface.c \
	strtofr.c \
	sub.c \
	sub1.c \
	sub1sp.c \
	sub_d.c \
	sub_ui.c \
	subnormal.c \
	sum.c \
	swap.c \
	tan.c \
	tanh.c \
	uceil_log2.c \
	ufloor_log2.c \
	ui_div.c \
	ui_pow.c \
	ui_pow_ui.c \
	ui_sub.c \
	urandom.c \
	urandomb.c \
	vasprintf.c \
	version.c \
	volatile.c \
	yn.c \
	zeta.c \
	zeta_ui.c

SRCS+=	\
	exceptions.c \
	extract.c \
	uceil_exp2.c \
	uceil_log2.c \
	ufloor_log2.c \
	add.c \
	add1.c \
	add_ui.c \
	agm.c \
	clear.c \
	cmp.c \
	cmp_abs.c \
	cmp_si.c \
	cmp_ui.c \
	comparisons.c \
	div_2exp.c \
	div_2si.c \
	div_2ui.c \
	div.c \
	div_ui.c \
	dump.c \
	eq.c \
	exp10.c \
	exp2.c \
	exp3.c \
	exp.c \
	frac.c \
	get_d.c \
	get_exp.c \
	get_str.c \
	init.c \
	inp_str.c \
	isinteger.c \
	isinf.c \
	isnan.c \
	isnum.c \
	const_log2.c \
	log.c \
	modf.c \
	mul_2exp.c \
	mul_2si.c \
	mul_2ui.c \
	mul.c \
	mul_ui.c \
	neg.c \
	next.c \
	out_str.c \
	printf.c \
	vasprintf.c \
	const_pi.c \
	pow.c \
	pow_si.c \
	pow_ui.c \
	print_raw.c \
	print_rnd_mode.c \
	reldiff.c \
	round_prec.c \
	set.c \
	setmax.c \
	setmin.c \
	set_d.c \
	set_dfl_prec.c \
	set_exp.c \
	set_rnd.c \
	set_f.c \
	set_prc_raw.c \
	set_prec.c \
	set_q.c \
	set_si.c \
	set_str.c \
	set_str_raw.c \
	set_ui.c \
	set_z.c \
	sqrt.c \
	sqrt_ui.c \
	sub.c \
	sub1.c \
	sub_ui.c \
	rint.c \
	ui_div.c \
	ui_sub.c \
	urandom.c \
	urandomb.c \
	get_z_exp.c \
	swap.c \
	factorial.c \
	cosh.c \
	sinh.c \
	tanh.c \
	sinh_cosh.c \
	acosh.c \
	asinh.c \
	atanh.c \
	atan.c \
	cmp2.c \
	exp_2.c \
	asin.c \
	const_euler.c \
	cos.c \
	sin.c \
	tan.c \
	fma.c \
	fms.c \
	hypot.c \
	log1p.c \
	expm1.c \
	log2.c \
	log10.c \
	ui_pow.c \
	ui_pow_ui.c \
	minmax.c \
	dim.c \
	signbit.c \
	copysign.c \
	setsign.c \
	gmp_op.c \
	init2.c \
	acos.c \
	sin_cos.c \
	set_nan.c \
	set_inf.c \
	set_zero.c \
	powerof2.c \
	gamma.c \
	set_ld.c \
	get_ld.c \
	cbrt.c \
	volatile.c \
	fits_sshort.c \
	fits_sint.c \
	fits_slong.c \
	fits_ushort.c \
	fits_uint.c \
	fits_ulong.c \
	fits_uintmax.c \
	fits_intmax.c \
	get_si.c \
	get_ui.c \
	zeta.c \
	cmp_d.c \
	erf.c \
	inits.c \
	inits2.c \
	clears.c \
	sgn.c \
	check.c \
	sub1sp.c \
	version.c \
	mpn_exp.c \
	mpfr-gmp.c \
	mp_clz_tab.c \
	sum.c \
	add1sp.c \
	free_cache.c \
	si_op.c \
	cmp_ld.c \
	set_ui_2exp.c \
	set_si_2exp.c \
	set_uj.c \
	set_sj.c \
	get_sj.c \
	get_uj.c \
	get_z.c \
	iszero.c \
	cache.c \
	sqr.c \
	int_ceil_log2.c \
	isqrt.c \
	strtofr.c \
	pow_z.c \
	logging.c \
	mulders.c \
	get_f.c \
	round_p.c \
	erfc.c \
	atan2.c \
	subnormal.c \
	const_catalan.c \
	root.c \
	sec.c \
	csc.c \
	cot.c \
	eint.c \
	sech.c \
	csch.c \
	coth.c \
	round_near_x.c \
	constant.c \
	abort_prec_max.c \
	stack_interface.c \
	lngamma.c \
	zeta_ui.c \
	set_d64.c \
	get_d64.c \
	jn.c \
	yn.c \
	rem1.c \
	get_patches.c \
	add_d.c \
	sub_d.c \
	d_sub.c \
	mul_d.c \
	div_d.c \
	d_div.c \
	li2.c \
	rec_sqrt.c \
	min_prec.c \
	buildopt.c \
	digamma.c \
	bernoulli.c \
	isregular.c \
	set_flt.c \
	get_flt.c \
	scale2.c \
	set_z_exp.c \
	ai.c \
	gammaonethird.c

CPPFLAGS+=	-I. \
		-DHAVE_INTTYPES_H=1 \
		-DHAVE_STDINT_H=1 \
		-DTIME_WITH_SYS_TIME=1 \
		-DHAVE_LOCALE_H=1 \
		-DHAVE_WCHAR_H=1 \
		-DHAVE_STDARG=1 \
		-DHAVE_SYS_TIME_H=1 \
		-DHAVE_STDINT_H=1 \
		-DHAVE_VA_COPY=1 \
		-DHAVE_SETLOCALE=1 \
		-DHAVE_GETTIMEOFDAY=1 \
		-DHAVE_LONG_LONG=1 \
		-DHAVE_INTMAX_T=1 \
		-DMPFR_HAVE_INTMAX_MAX=1 \
		-DMPFR_HAVE_FESETROUND=1 \
		-DHAVE_DENORMS=1 \
		-DHAVE_ROUND=1 \
		-DHAVE_TRUNC=1 \
		-DHAVE_FLOOR=1 \
		-DHAVE_CEIL=1 \
		-DHAVE_ATTRIBUTE_MODE=1

# Find <gmp.h>
GMP_MACHINE_ARCH=${MACHINE_ARCH:S/earmv4/arm/:C/armv[5-7]/arm/}
CPPFLAGS+=	-I${NETBSDSRCDIR}/external/lgpl3/gmp/lib/libgmp/arch/${GMP_MACHINE_ARCH}
CPPFLAGS+=	-I${.CURDIR}/../../dist/src

# XXX
# XXX XXX check mips*
# XXX XXX do something more, more ports!!
# XXX
# XXX missing: arm* sparc sh3* m68k/m68000 (powerpc64/ia64)

# IEEE FP support.  No VAX here.
.if ${MACHINE_ARCH} == "x86_64" || \
    ${MACHINE_ARCH} == "i386"
    ${MACHINE_ARCH} == "mipsel"
CPPFLAGS+=	-DHAVE_LDOUBLE_IEEE_EXT_LITTLE=1
.endif

.if ${MACHINE_ARCH} == "mips64el"
CPPFLAGS+=	-DHAVE_LDOUBLE_IEEE_QUAD_LITTLE=1
.endif

.if ${MACHINE_ARCH} == "sparc64" || \
    ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "powerpc64" || \
    ${MACHINE_ARCH} == "mips64eb"
CPPFLAGS+=	-DHAVE_LDOUBLE_IEEE_QUAD_BIG=1
.endif

WITHOUT_MAN=	yes

.include <bsd.lib.mk>

mparam.h: mparam_h.in
	rm -f ${.TARGET} && cp ${.ALLSRC} ${.TARGET}

${SRCS}: mparam.h
CLEANFILES+= mparam.h

.PATH: ${.CURDIR}/../../dist/src

.if !empty(MACHINE_ARCH:M*arm*)
COPTS += -marm
.endif
.if ${MACHINE_ARCH} == "vax"
COPTS.set_ld.c+=	-Wno-error
.endif

# Don't warn about functions which cannot be stack smash protected as
# there are a lot of them.
COPTS += -Wno-stack-protector
