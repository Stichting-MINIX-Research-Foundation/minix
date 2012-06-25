#	$NetBSD: Makefile.inc,v 1.1 2011/10/29 09:16:54 mrg Exp $

SRCS+= \
	random.c \
	divrem_2.c \
	toom_interpolate_7pts.c \
	sbpi1_divappr_q.c \
	random2.c \
	mu_bdiv_q.c \
	toom32_mul.c \
	toom44_mul.c \
	toom8h_mul.c \
	toom2_sqr.c \
	zero.c \
	mod_1_4.c \
	gcdext.c \
	binvert.c \
	mu_div_q.c \
	invertappr.c \
	dump.c \
	mu_divappr_q.c \
	dcpi1_div_qr.c \
	redc_1_sec.c \
	toom6_sqr.c \
	divrem_1.c \
	mod_1.c \
	divexact.c \
	powlo.c \
	mul.c \
	set_str.c \
	toom42_mul.c \
	toom_interpolate_6pts.c \
	dcpi1_divappr_q.c \
	toom_eval_dgr3_pm2.c \
	mod_1_3.c \
	toom_couple_handling.c \
	add.c \
	toom_interpolate_12pts.c \
	perfsqr.c \
	toom53_mul.c \
	toom_eval_pm2exp.c \
	mu_div_qr.c \
	toom_interpolate_16pts.c \
	mod_34lsub1.c \
	bdiv_q.c \
	toom22_mul.c \
	toom4_sqr.c \
	div_q.c \
	jacbase.c \
	gcdext_subdiv_step.c \
	mullo_n.c \
	toom33_mul.c \
	toom_eval_dgr3_pm1.c \
	mod_1_2.c \
	sub_1.c \
	trialdiv.c \
	add_1.c \
	sqr_basecase.c \
	toom_interpolate_5pts.c \
	sbpi1_bdiv_q.c \
	pre_mod_1.c \
	hgcd.c \
	bdiv_dbm1c.c \
	sqrtrem.c \
	bdiv_q_1.c \
	gcdext_1.c \
	toom63_mul.c \
	mul_basecase.c \
	toom8_sqr.c \
	neg.c \
	gcdext_lehmer.c \
	divis.c \
	dcpi1_div_q.c \
	subcnd_n.c \
	toom_eval_pm2.c \
	hgcd2.c \
	toom62_mul.c \
	sbpi1_bdiv_qr.c \
	scan1.c \
	pre_divrem_1.c \
	perfpow.c \
	get_str.c \
	mullo_basecase.c \
	mulmod_bnm1.c \
	tdiv_qr.c \
	toom43_mul.c \
	mod_1_1.c \
	matrix22_mul.c \
	powm_sec.c \
	divrem.c \
	gcd_1.c \
	dcpi1_bdiv_qr.c \
	mul_n.c \
	redc_2.c \
	toom6h_mul.c \
	mul_fft.c \
	sqrmod_bnm1.c \
	powm.c \
	rootrem.c \
	mode1o.c \
	toom_interpolate_8pts.c \
	remove.c \
	lshiftc.c \
	dive_1.c \
	cmp.c \
	toom_eval_pm1.c \
	scan0.c \
	gcd_subdiv_step.c \
	sbpi1_div_qr.c \
	invert.c \
	sub.c \
	toom_eval_pm2rexp.c \
	dcpi1_bdiv_q.c \
	redc_1.c \
	sqr.c \
	nussbaumer_mul.c \
	mu_bdiv_qr.c \
	pow_1.c \
	gcd_lehmer.c \
	get_d.c \
	toom52_mul.c \
	sbpi1_div_q.c \
	diveby3.c \
	fib2_ui.c \
	bdiv_qr.c \
	toom3_sqr.c \
	gcd.c \
	redc_n.c \

C_SRCS_LIST= \
	hamdist.c		mpn/generic/popham.c \
	popcount.c		mpn/generic/popham.c \

ASM_SRCS_LIST= \
	nand_n.asm		mpn/powerpc64/logops_n.asm \
	sqr_diagonal.asm		mpn/powerpc64/sqr_diagonal.asm \
	add_n.asm		mpn/powerpc64/mode32/add_n.asm \
	xor_n.asm		mpn/powerpc64/logops_n.asm \
	submul_1.asm		mpn/powerpc64/mode32/submul_1.asm \
	umul.asm		mpn/powerpc64/umul.asm \
	nior_n.asm		mpn/powerpc64/logops_n.asm \
	lshift.asm		mpn/powerpc64/lshift.asm \
	and_n.asm		mpn/powerpc64/logops_n.asm \
	mul_1.asm		mpn/powerpc64/mode32/mul_1.asm \
	copyd.asm		mpn/powerpc64/copyd.asm \
	andn_n.asm		mpn/powerpc64/logops_n.asm \
	com.asm		mpn/powerpc64/com.asm \
	copyi.asm		mpn/powerpc64/copyi.asm \
	rshift.asm		mpn/powerpc64/rshift.asm \
	iorn_n.asm		mpn/powerpc64/logops_n.asm \
	ior_n.asm		mpn/powerpc64/logops_n.asm \
	xnor_n.asm		mpn/powerpc64/logops_n.asm \
	addmul_1.asm		mpn/powerpc64/mode32/addmul_1.asm \
	sub_n.asm		mpn/powerpc64/mode32/sub_n.asm \

gen-trialdivtab: gen-trialdivtab.c
	${HOST_CC} -o ${.OBJDIR}/gen-trialdivtab ${.ALLSRC} -lm

# XXX
GMP_LIMB_BITS=64

trialdiv.c: trialdivtab.h
trialdivtab.h: gen-trialdivtab
	${.OBJDIR}/gen-trialdivtab $(GMP_LIMB_BITS) 8000 > ${.OBJDIR}/trialdivtab.h

CLEANFILES+=	trialdivtab.h

# XXX stuff looks for "../config.m4"
dummy:
	mkdir dummy
${.OBJDIR}/config.m4: ${.CURDIR}/arch/${MACHINE_ARCH}/config.m4
	cp ${.CURDIR}/arch/${MACHINE_ARCH}/config.m4 ${.TARGET}

BUILD_S_INCS=	-I${.CURDIR} -I${.OBJDIR}/dummy -I${DIST}/ -I${DIST}/mpn

M4FLAGS= -DPIC
.for _build _src in ${ASM_SRCS_LIST}
${_build:R}.s: Makefile dummy ${.OBJDIR}/config.m4
	${TOOL_M4} -DCONFIG_TOP_SRCDIR=${DIST} ${BUILD_S_INCS} \
		${M4FLAGS} ${M4FLAGS.${.TARGET:R}} \
		-DOPERATION_${.TARGET:R} ${DIST}/${_src} > ${.TARGET}

SRCS+=		${_build:R}.s
CLEANFILES+=	${_build:R}.s

.endfor

.for _build _src in ${C_SRCS_LIST}
mpn_${_build}: Makefile
	echo '#include "${_src}"' > ${.TARGET}

CPPFLAGS.mpn_${_build}+= -DOPERATION_${_build:R}

SRCS+=		mpn_${_build}
CLEANFILES+=	mpn_${_build}
.endfor
