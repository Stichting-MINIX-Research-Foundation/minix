#	$NetBSD: Makefile.inc,v 1.1 2012/12/26 19:39:26 martin Exp $

SRCS+= \
	trialdiv.c \
	powlo.c \
	redc_2.c \
	bdiv_q_1.c \
	toom62_mul.c \
	gcd_lehmer.c \
	mod_1_3.c \
	sub.c \
	bdiv_qr.c \
	sbpi1_divappr_q.c \
	sqrmod_bnm1.c \
	toom6_sqr.c \
	toom44_mul.c \
	hgcd.c \
	tdiv_qr.c \
	gcd_subdiv_step.c \
	nussbaumer_mul.c \
	mod_1_4.c \
	sbpi1_bdiv_q.c \
	toom43_mul.c \
	gcdext.c \
	mu_bdiv_qr.c \
	sbpi1_div_qr.c \
	toom_interpolate_6pts.c \
	toom6h_mul.c \
	toom52_mul.c \
	toom42_mul.c \
	gcdext_lehmer.c \
	pow_1.c \
	lshiftc.c \
	sub_1.c \
	zero.c \
	dcpi1_bdiv_qr.c \
	sbpi1_bdiv_qr.c \
	matrix22_mul.c \
	perfpow.c \
	random2.c \
	dump.c \
	mod_34lsub1.c \
	dcpi1_divappr_q.c \
	invert.c \
	toom_interpolate_8pts.c \
	toom33_mul.c \
	fib2_ui.c \
	powm.c \
	divexact.c \
	toom_eval_pm2exp.c \
	toom4_sqr.c \
	toom32_mul.c \
	get_d.c \
	jacbase.c \
	gcdext_subdiv_step.c \
	gcdext_1.c \
	mul_basecase.c \
	add_1.c \
	redc_1_sec.c \
	mu_divappr_q.c \
	toom_interpolate_12pts.c \
	toom_interpolate_5pts.c \
	toom3_sqr.c \
	scan0.c \
	set_str.c \
	get_str.c \
	mul_n.c \
	pre_mod_1.c \
	subcnd_n.c \
	redc_n.c \
	dcpi1_div_q.c \
	sbpi1_div_q.c \
	binvert.c \
	toom_eval_pm2rexp.c \
	toom_eval_pm1.c \
	toom_couple_handling.c \
	gcd.c \
	perfsqr.c \
	scan1.c \
	mod_1.c \
	divrem.c \
	add.c \
	remove.c \
	toom_interpolate_7pts.c \
	toom_eval_pm2.c \
	toom8_sqr.c \
	toom63_mul.c \
	toom22_mul.c \
	mullo_n.c \
	hgcd2.c \
	rootrem.c \
	random.c \
	mul.c \
	bdiv_q.c \
	mu_bdiv_q.c \
	toom_eval_dgr3_pm1.c \
	toom8h_mul.c \
	mullo_basecase.c \
	sqrtrem.c \
	sqr_basecase.c \
	powm_sec.c \
	dcpi1_bdiv_q.c \
	mu_div_q.c \
	mu_div_qr.c \
	invertappr.c \
	toom_interpolate_16pts.c \
	toom_eval_dgr3_pm2.c \
	toom2_sqr.c \
	mod_1_1.c \
	divis.c \
	diveby3.c \
	com.c \
	redc_1.c \
	dcpi1_div_qr.c \
	mulmod_bnm1.c \
	toom53_mul.c \
	div_q.c \
	cmp.c \
	sqr.c \
	mul_fft.c \
	mod_1_2.c \
	neg.c \


C_SRCS_LIST= \


ASM_SRCS_LIST= \
	ior_n.asm		mpn/ia64/logops_n.asm \
	nand_n.asm		mpn/ia64/logops_n.asm \
	addmul_1.asm		mpn/ia64/addmul_1.asm \
	addlsh1_n.asm		mpn/ia64/aorslsh1_n.asm \
	iorn_n.asm		mpn/ia64/logops_n.asm \
	gcd_1.asm		mpn/ia64/gcd_1.asm \
	popcount.asm		mpn/ia64/popcount.asm \
	mul_2.asm		mpn/ia64/mul_2.asm \
	copyd.asm		mpn/ia64/copyd.asm \
	xor_n.asm		mpn/ia64/logops_n.asm \
	sqr_diagonal.asm		mpn/ia64/sqr_diagonal.asm \
	andn_n.asm		mpn/ia64/logops_n.asm \
	submul_1.asm		mpn/ia64/submul_1.asm \
	mul_1.asm		mpn/ia64/mul_1.asm \
	sublsh1_n.asm		mpn/ia64/aorslsh1_n.asm \
	copyi.asm		mpn/ia64/copyi.asm \
	and_n.asm		mpn/ia64/logops_n.asm \
	hamdist.asm		mpn/ia64/hamdist.asm \
	sub_n.asm		mpn/ia64/aors_n.asm \
	nior_n.asm		mpn/ia64/logops_n.asm \
	add_n.asm		mpn/ia64/aors_n.asm \
	invert_limb.asm		mpn/ia64/invert_limb.asm \
	bdiv_dbm1c.asm		mpn/ia64/bdiv_dbm1c.asm \
	rshift.asm		mpn/ia64/lorrshift.asm \
	rsh1sub_n.asm		mpn/ia64/rsh1aors_n.asm \
	xnor_n.asm		mpn/ia64/logops_n.asm \
	divrem_2.asm		mpn/ia64/divrem_2.asm \
	rsh1add_n.asm		mpn/ia64/rsh1aors_n.asm \
	dive_1.asm		mpn/ia64/dive_1.asm \
	lshift.asm		mpn/ia64/lorrshift.asm \
	addmul_2.asm		mpn/ia64/addmul_2.asm \
	mode1o.asm		mpn/ia64/mode1o.asm \
	divrem_1.asm		mpn/ia64/divrem_1.asm \


gen-trialdivtab: gen-trialdivtab.c
	${HOST_CC} -o ${.OBJDIR}/gen-trialdivtab ${.ALLSRC} -lm

# XXX
GMP_LIMB_BITS=64

trialdiv.c: trialdivtab.h
trialdivtab.h: gen-trialdivtab
	${.OBJDIR}/gen-trialdivtab $(GMP_LIMB_BITS) 8000 > ${.OBJDIR}/trialdivtab.h

CLEANFILES+=	trialdivtab.h
