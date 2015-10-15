#	$NetBSD: Makefile.inc,v 1.3 2014/03/09 20:54:01 mrg Exp $

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
	sqr_basecase.c \
	toom_interpolate_5pts.c \
	sbpi1_bdiv_q.c \
	pre_mod_1.c \
	hgcd.c \
	sqrtrem.c \
	bdiv_q_1.c \
	gcdext_1.c \
	toom63_mul.c \
	div_qr_2u_pi1.c \
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
	comb_tables.c \
	sbpi1_bdiv_qr.c \
	sub_err2_n.c \
	scan1.c \
	brootinv.c \
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
	dcpi1_bdiv_qr.c \
	mul_n.c \
	redc_2.c \
	toom6h_mul.c \
	sqrmod_bnm1.c \
	mul_fft.c \
	mulmid.c \
	powm.c \
	rootrem.c \
	toom_interpolate_8pts.c \
	remove.c \
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
	sb_div_r_sec.c		mpn/generic/sb_div_sec.c \
	sbpi1_div_qr_sec.c	mpn/generic/sbpi1_div_sec.c \
	sb_div_qr_sec.c		mpn/generic/sb_div_sec.c \
	sbpi1_div_r_sec.c	mpn/generic/sbpi1_div_sec.c \

ASM_SRCS_LIST= \
	sqr_diag_addlsh1.asm	mpn/ia64/sqr_diag_addlsh1.asm \
	nand_n.asm		mpn/ia64/logops_n.asm \
	rsh1sub_n.asm		mpn/ia64/rsh1aors_n.asm \
	add_n.asm		mpn/ia64/aors_n.asm \
	rsh1add_n.asm		mpn/ia64/rsh1aors_n.asm \
	sublsh1_n.asm		mpn/ia64/aorslsh1_n.asm \
	xor_n.asm		mpn/ia64/logops_n.asm \
	addmul_2.asm		mpn/ia64/addmul_2.asm \
	submul_1.asm		mpn/ia64/submul_1.asm \
	nior_n.asm		mpn/ia64/logops_n.asm \
	tabselect.asm		mpn/ia64/tabselect.asm \
	lshift.asm		mpn/ia64/lorrshift.asm \
	and_n.asm		mpn/ia64/logops_n.asm \
	hamdist.asm		mpn/ia64/hamdist.asm \
	mul_1.asm		mpn/ia64/mul_1.asm \
	addlsh2_n.asm		mpn/ia64/aorslsh2_n.asm \
	divrem_1.asm		mpn/ia64/divrem_1.asm \
	copyd.asm		mpn/ia64/copyd.asm \
	bdiv_dbm1c.asm		mpn/ia64/bdiv_dbm1c.asm \
	dive_1.asm		mpn/ia64/dive_1.asm \
	andn_n.asm		mpn/ia64/logops_n.asm \
	add_n_sub_n.asm		mpn/ia64/add_n_sub_n.asm \
	copyi.asm		mpn/ia64/copyi.asm \
	rshift.asm		mpn/ia64/lorrshift.asm \
	iorn_n.asm		mpn/ia64/logops_n.asm \
	ior_n.asm		mpn/ia64/logops_n.asm \
	mod_34lsub1.asm		mpn/ia64/mod_34lsub1.asm \
	popcount.asm		mpn/ia64/popcount.asm \
	sublsh2_n.asm		mpn/ia64/aorslsh2_n.asm \
	xnor_n.asm		mpn/ia64/logops_n.asm \
	lshiftc.asm		mpn/ia64/lshiftc.asm \
	mode1o.asm		mpn/ia64/mode1o.asm \
	addlsh1_n.asm		mpn/ia64/aorslsh1_n.asm \
	mul_2.asm		mpn/ia64/mul_2.asm \
	invert_limb.asm		mpn/ia64/invert_limb.asm \
	addmul_1.asm		mpn/ia64/addmul_1.asm \
	divrem_2.asm		mpn/ia64/divrem_2.asm \
	sub_n.asm		mpn/ia64/aors_n.asm \
	gcd_1.asm		mpn/ia64/gcd_1.asm \

GMP_LIMB_BITS=64
