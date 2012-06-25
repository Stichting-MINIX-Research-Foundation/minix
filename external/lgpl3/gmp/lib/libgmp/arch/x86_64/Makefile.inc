#	$NetBSD: Makefile.inc,v 1.3 2011/07/03 06:43:17 mrg Exp $

SRCS+= \
	random.c \
	toom_interpolate_7pts.c \
	sbpi1_divappr_q.c \
	random2.c \
	mu_bdiv_q.c \
	toom32_mul.c \
	toom44_mul.c \
	toom8h_mul.c \
	toom2_sqr.c \
	zero.c \
	gcdext.c \
	binvert.c \
	mu_div_q.c \
	invertappr.c \
	dump.c \
	dcpi1_div_qr.c \
	mu_divappr_q.c \
	redc_1_sec.c \
	toom6_sqr.c \
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
	toom_interpolate_16pts.c \
	mu_div_qr.c \
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
	toom_interpolate_5pts.c \
	sbpi1_bdiv_q.c \
	pre_mod_1.c \
	hgcd.c \
	sqrtrem.c \
	gcdext_1.c \
	toom63_mul.c \
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
	mul_n.c \
	dcpi1_bdiv_qr.c \
	redc_2.c \
	toom6h_mul.c \
	mul_fft.c \
	sqrmod_bnm1.c \
	powm.c \
	rootrem.c \
	toom_interpolate_8pts.c \
	remove.c \
	cmp.c \
	toom_eval_pm1.c \
	scan0.c \
	gcd_subdiv_step.c \
	sbpi1_div_qr.c \
	invert.c \
	sub.c \
	toom_eval_pm2rexp.c \
	dcpi1_bdiv_q.c \
	sqr.c \
	nussbaumer_mul.c \
	pow_1.c \
	gcd_lehmer.c \
	mu_bdiv_qr.c \
	get_d.c \
	toom52_mul.c \
	sbpi1_div_q.c \
	diveby3.c \
	fib2_ui.c \
	bdiv_qr.c \
	toom3_sqr.c \
	gcd.c \
	redc_n.c \

ASM_SRCS_LIST= \
	nand_n.asm		mpn/x86_64/logops_n.asm \
	rsh1sub_n.asm		mpn/x86_64/rsh1aors_n.asm \
	add_n.asm		mpn/x86_64/aors_n.asm \
	rsh1add_n.asm		mpn/x86_64/rsh1aors_n.asm \
	sqr_basecase.asm	mpn/x86_64/sqr_basecase.asm \
	rsblsh2_n.asm		mpn/x86_64/aorrlsh2_n.asm \
	sublsh1_n.asm		mpn/x86_64/sublsh1_n.asm \
	bdiv_q_1.asm		mpn/x86_64/bdiv_q_1.asm \
	addaddmul_1msb0.asm	mpn/x86_64/addaddmul_1msb0.asm \
	xor_n.asm		mpn/x86_64/logops_n.asm \
	rsblsh_n.asm		mpn/x86_64/aorrlsh_n.asm \
	addmul_2.asm		mpn/x86_64/addmul_2.asm \
	submul_1.asm		mpn/x86_64/aorsmul_1.asm \
	nior_n.asm		mpn/x86_64/logops_n.asm \
	mul_basecase.asm	mpn/x86_64/mul_basecase.asm \
	lshift.asm		mpn/x86_64/lshift.asm \
	and_n.asm		mpn/x86_64/logops_n.asm \
	hamdist.asm		mpn/x86_64/popham.asm \
	mul_1.asm		mpn/x86_64/mul_1.asm \
	addlsh2_n.asm		mpn/x86_64/aorrlsh2_n.asm \
	divrem_1.asm		mpn/x86_64/divrem_1.asm \
	copyd.asm		mpn/x86_64/copyd.asm \
	bdiv_dbm1c.asm		mpn/x86_64/bdiv_dbm1c.asm \
	rsblsh1_n.asm		mpn/x86_64/aorrlsh1_n.asm \
	dive_1.asm		mpn/x86_64/dive_1.asm \
	andn_n.asm		mpn/x86_64/logops_n.asm \
	com.asm			mpn/x86_64/com.asm \
	copyi.asm		mpn/x86_64/copyi.asm \
	rshift.asm		mpn/x86_64/rshift.asm \
	addlsh_n.asm		mpn/x86_64/aorrlsh_n.asm \
	iorn_n.asm		mpn/x86_64/logops_n.asm \
	ior_n.asm		mpn/x86_64/logops_n.asm \
	mod_34lsub1.asm		mpn/x86_64/mod_34lsub1.asm \
	popcount.asm		mpn/x86_64/popham.asm \
	xnor_n.asm		mpn/x86_64/logops_n.asm \
	lshiftc.asm		mpn/x86_64/lshiftc.asm \
	mode1o.asm		mpn/x86_64/mode1o.asm \
	addlsh1_n.asm		mpn/x86_64/aorrlsh1_n.asm \
	mul_2.asm		mpn/x86_64/mul_2.asm \
	invert_limb.asm		mpn/x86_64/invert_limb.asm \
	addmul_1.asm		mpn/x86_64/aorsmul_1.asm \
	divrem_2.asm		mpn/x86_64/divrem_2.asm \
	sub_n.asm		mpn/x86_64/aors_n.asm \

# XXX using C versions
#	redc_1.asm		mpn/x86_64/redc_1.asm
#	gcd_1.asm		mpn/x86_64/gcd_1.asm
#	mod_1_4.asm		mpn/x86_64/mod_1_4.asm
SRCS+=	redc_1.c gcd_1.c mod_1_4.c

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

.for _build _src in ${ASM_SRCS_LIST}
${_build:R}.s: Makefile dummy ${.OBJDIR}/config.m4
	${TOOL_M4} -DCONFIG_TOP_SRCDIR=${DIST} -I${.CURDIR} -I${.OBJDIR}/dummy -I${DIST}/ -I${DIST}/mpn -DOPERATION_${.TARGET:R} ${DIST}/${_src} > ${.TARGET}

SRCS+=		${_build:R}.s
CLEANFILES+=	${_build:R}.s

.endfor
