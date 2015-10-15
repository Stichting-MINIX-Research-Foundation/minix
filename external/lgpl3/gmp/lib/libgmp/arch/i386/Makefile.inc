#	$NetBSD: Makefile.inc,v 1.8 2013/12/04 00:49:18 mrg Exp $

SRCS+= \
	random.c \
	toom_interpolate_7pts.c \
	sbpi1_divappr_q.c \
	random2.c \
	mu_bdiv_q.c \
	mulmid_basecase.c \
	jacobi_2.c \
	toom32_mul.c \
	toom44_mul.c \
	toom8h_mul.c \
	toom2_sqr.c \
	zero.c \
	mod_1_4.c \
	gcdext.c \
	add_err3_n.c \
	binvert.c \
	mu_div_q.c \
	invertappr.c \
	add_n_sub_n.c \
	dump.c \
	mu_divappr_q.c \
	dcpi1_div_qr.c \
	hgcd_reduce.c \
	matrix22_mul1_inverse_vector.c \
	toom6_sqr.c \
	hgcd_step.c \
	sub_err3_n.c \
	mod_1.c \
	toom42_mulmid.c \
	divexact.c \
	jacobi.c \
	powlo.c \
	mul.c \
	set_str.c \
	toom42_mul.c \
	toom54_mul.c \
	toom_interpolate_6pts.c \
	dcpi1_divappr_q.c \
	toom_eval_dgr3_pm2.c \
	mod_1_3.c \
	com.c \
	toom_couple_handling.c \
	add.c \
	div_qr_2.c \
	toom_interpolate_12pts.c \
	perfsqr.c \
	toom53_mul.c \
	toom_eval_pm2exp.c \
	mu_div_qr.c \
	toom_interpolate_16pts.c \
	bdiv_q.c \
	toom22_mul.c \
	bsqrtinv.c \
	toom4_sqr.c \
	div_q.c \
	jacbase.c \
	addcnd_n.c \
	hgcd_matrix.c \
	mullo_n.c \
	toom33_mul.c \
	toom_eval_dgr3_pm1.c \
	mod_1_2.c \
	sub_1.c \
	add_err2_n.c \
	trialdiv.c \
	add_1.c \
	toom_interpolate_5pts.c \
	sbpi1_bdiv_q.c \
	pre_mod_1.c \
	hgcd.c \
	sqrtrem.c \
	gcdext_1.c \
	toom63_mul.c \
	div_qr_2u_pi1.c \
	toom8_sqr.c \
	neg.c \
	gcdext_lehmer.c \
	divis.c \
	dcpi1_div_q.c \
	subcnd_n.c \
	toom_eval_pm2.c \
	hgcd2.c \
	toom62_mul.c \
	comb_tables.c \
	sbpi1_bdiv_qr.c \
	sub_err2_n.c \
	scan1.c \
	brootinv.c \
	pre_divrem_1.c \
	perfpow.c \
	get_str.c \
	mulmod_bnm1.c \
	mullo_basecase.c \
	tdiv_qr.c \
	div_qr_2n_pi1.c \
	toom43_mul.c \
	mod_1_1.c \
	matrix22_mul.c \
	powm_sec.c \
	divrem.c \
	bsqrt.c \
	gcd_1.c \
	dcpi1_bdiv_qr.c \
	mul_n.c \
	redc_2.c \
	toom6h_mul.c \
	sqrmod_bnm1.c \
	mul_fft.c \
	mulmid.c \
	powm.c \
	rootrem.c \
	mode1o.c \
	toom_interpolate_8pts.c \
	remove.c \
	lshiftc.c \
	cmp.c \
	toom_eval_pm1.c \
	hgcd_appr.c \
	scan0.c \
	gcd_subdiv_step.c \
	sbpi1_div_qr.c \
	invert.c \
	sub.c \
	toom_eval_pm2rexp.c \
	broot.c \
	dcpi1_bdiv_q.c \
	hgcd2_jacobi.c \
	add_err1_n.c \
	mulmid_n.c \
	redc_1.c \
	sqr.c \
	nussbaumer_mul.c \
	mu_bdiv_qr.c \
	pow_1.c \
	get_d.c \
	toom52_mul.c \
	sbpi1_div_q.c \
	diveby3.c \
	fib2_ui.c \
	bdiv_qr.c \
	hgcd_jacobi.c \
	toom3_sqr.c \
	gcd.c \
	redc_n.c \
	sub_err1_n.c \

C_SRCS_LIST= \
	xor_n.c			mpn/generic/logops_n.c \
	sb_div_r_sec.c		mpn/generic/sb_div_sec.c \
	nand_n.c		mpn/generic/logops_n.c \
	nior_n.c		mpn/generic/logops_n.c \
	hamdist.c		mpn/generic/popham.c \
	popcount.c		mpn/generic/popham.c \
	sbpi1_div_qr_sec.c	mpn/generic/sbpi1_div_sec.c \
	and_n.c			mpn/generic/logops_n.c \
	ior_n.c			mpn/generic/logops_n.c \
	sb_div_qr_sec.c		mpn/generic/sb_div_sec.c \
	xnor_n.c		mpn/generic/logops_n.c \
	iorn_n.c		mpn/generic/logops_n.c \
	sbpi1_div_r_sec.c	mpn/generic/sbpi1_div_sec.c \
	andn_n.c		mpn/generic/logops_n.c \

ASM_SRCS_LIST= \
	add_n.asm		mpn/x86/aors_n.asm \
	sqr_basecase.asm	mpn/x86/sqr_basecase.asm \
	bdiv_q_1.asm		mpn/x86/bdiv_q_1.asm \
	submul_1.asm		mpn/x86/aorsmul_1.asm \
	umul.asm		mpn/x86/umul.asm \
	mul_basecase.asm	mpn/x86/mul_basecase.asm \
	tabselect.asm		mpn/x86/tabselect.asm \
	lshift.asm		mpn/x86/lshift.asm \
	mul_1.asm		mpn/x86/mul_1.asm \
	divrem_1.asm		mpn/x86/divrem_1.asm \
	copyd.asm		mpn/x86/copyd.asm \
	bdiv_dbm1c.asm		mpn/x86/bdiv_dbm1c.asm \
	udiv.asm		mpn/x86/udiv.asm \
	dive_1.asm		mpn/x86/dive_1.asm \
	copyi.asm		mpn/x86/copyi.asm \
	rshift.asm		mpn/x86/rshift.asm \
	mod_34lsub1.asm		mpn/x86/mod_34lsub1.asm \
	addmul_1.asm		mpn/x86/aorsmul_1.asm \
	divrem_2.asm		mpn/x86/divrem_2.asm \
	sub_n.asm		mpn/x86/aors_n.asm \

#ASM_SRCS_LIST= \
#	submul_1.asm		mpn/x86/aorsmul_1.asm \
#	bdiv_dbm1c.asm		mpn/x86/bdiv_dbm1c.asm \
#	addmul_1.asm		mpn/x86/aorsmul_1.asm \
#	divrem_2.asm		mpn/x86/divrem_2.asm \

# XXX using C versions
#	add_n.asm		mpn/x86/aors_n.asm \
#	bdiv_q_1.asm		mpn/x86_64/bdiv_q_1.asm \
#	copyd.asm		mpn/x86/copyd.asm \
#	copyi.asm		mpn/x86/copyi.asm \
#	dive_1.asm		mpn/x86/dive_1.asm \
#	divrem_1.asm		mpn/x86/divrem_1.asm \
#	invert_limb.asm		mpn/x86/invert_limb.asm \
#	lshift.asm		mpn/x86/lshift.asm \
#	rshift.asm		mpn/x86/rshift.asm \
#	mod_34lsub1.asm		mpn/x86/mod_34lsub1.asm \
#	umul.asm		mpn/x86/umul.asm \
#	mul_1.asm		mpn/x86/mul_1.asm \
#	sqr_basecase.asm	mpn/x86/sqr_basecase.asm \
#	mul_basecase.asm	mpn/x86/mul_basecase.asm \
#	sub_n.asm		mpn/x86/aors_n.asm \

#	udiv.asm		mpn/x86/udiv.asm \

#SRCS+=	add_n.c bdiv_q_1.c copyd.c copyi.c dive_1.c lshift.c rshift.c mod_34lsub1.c
#SRCS+=	mul_1.c sqr_basecase.c mul_basecase.c sub_n.c divrem_1.c
# XXX? put in C_SRCS_LIST?
# divrem.c

GMP_LIMB_BITS=32
