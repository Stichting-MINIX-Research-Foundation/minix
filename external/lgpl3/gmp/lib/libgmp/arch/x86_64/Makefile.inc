#	$NetBSD: Makefile.inc,v 1.6 2013/12/04 00:49:19 mrg Exp $

SRCS+= \
	random.c \
	toom_interpolate_7pts.c \
	sbpi1_divappr_q.c \
	random2.c \
	mu_bdiv_q.c \
	jacobi_2.c \
	toom32_mul.c \
	toom44_mul.c \
	toom8h_mul.c \
	toom2_sqr.c \
	zero.c \
	gcdext.c \
	binvert.c \
	mu_div_q.c \
	invertappr.c \
	add_n_sub_n.c \
	dump.c \
	hgcd_reduce.c \
	dcpi1_div_qr.c \
	mu_divappr_q.c \
	matrix22_mul1_inverse_vector.c \
	toom6_sqr.c \
	hgcd_step.c \
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
	toom_couple_handling.c \
	add.c \
	toom_interpolate_12pts.c \
	div_qr_2.c \
	perfsqr.c \
	toom53_mul.c \
	toom_eval_pm2exp.c \
	toom_interpolate_16pts.c \
	mu_div_qr.c \
	bdiv_q.c \
	toom22_mul.c \
	bsqrtinv.c \
	toom4_sqr.c \
	div_q.c \
	jacbase.c \
	hgcd_matrix.c \
	mullo_n.c \
	toom33_mul.c \
	toom_eval_dgr3_pm1.c \
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
	toom_eval_pm2.c \
	hgcd2.c \
	toom62_mul.c \
	comb_tables.c \
	sbpi1_bdiv_qr.c \
	scan1.c \
	brootinv.c \
	perfpow.c \
	get_str.c \
	mulmod_bnm1.c \
	tdiv_qr.c \
	toom43_mul.c \
	matrix22_mul.c \
	powm_sec.c \
	divrem.c \
	bsqrt.c \
	dcpi1_bdiv_qr.c \
	mul_n.c \
	redc_2.c \
	toom6h_mul.c \
	mul_fft.c \
	sqrmod_bnm1.c \
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
	mulmid_n.c \
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

C_SRCS_LIST= \
	sb_div_r_sec.c		mpn/generic/sb_div_sec.c \
	sbpi1_div_qr_sec.c		mpn/generic/sbpi1_div_sec.c \
	sb_div_qr_sec.c		mpn/generic/sb_div_sec.c \
	sbpi1_div_r_sec.c		mpn/generic/sbpi1_div_sec.c \

ASM_SRCS_LIST= \
	nand_n.asm		mpn/x86_64/logops_n.asm \
	sub_err1_n.asm		mpn/x86_64/aors_err1_n.asm \
	add_err2_n.asm		mpn/x86_64/aors_err2_n.asm \
	rsh1sub_n.asm		mpn/x86_64/rsh1aors_n.asm \
	mod_1_2.asm		mpn/x86_64/mod_1_2.asm \
	add_n.asm		mpn/x86_64/aors_n.asm \
	rsh1add_n.asm		mpn/x86_64/rsh1aors_n.asm \
	sqr_basecase.asm		mpn/x86_64/sqr_basecase.asm \
	rsblsh2_n.asm		mpn/x86_64/aorrlsh2_n.asm \
	sublsh1_n.asm		mpn/x86_64/sublsh1_n.asm \
	bdiv_q_1.asm		mpn/x86_64/bdiv_q_1.asm \
	addcnd_n.asm		mpn/x86_64/aorscnd_n.asm \
	addaddmul_1msb0.asm		mpn/x86_64/addaddmul_1msb0.asm \
	xor_n.asm		mpn/x86_64/logops_n.asm \
	sub_err3_n.asm		mpn/x86_64/aors_err3_n.asm \
	rsblsh_n.asm		mpn/x86_64/k8/aorrlsh_n.asm \
	addmul_2.asm		mpn/x86_64/addmul_2.asm \
	submul_1.asm		mpn/x86_64/aorsmul_1.asm \
	nior_n.asm		mpn/x86_64/logops_n.asm \
	mul_basecase.asm		mpn/x86_64/mul_basecase.asm \
	tabselect.asm		mpn/x86_64/tabselect.asm \
	lshift.asm		mpn/x86_64/lshift.asm \
	add_err1_n.asm		mpn/x86_64/aors_err1_n.asm \
	and_n.asm		mpn/x86_64/logops_n.asm \
	hamdist.asm		mpn/x86_64/popham.asm \
	div_qr_2u_pi1.asm		mpn/x86_64/div_qr_2u_pi1.asm \
	mulmid_basecase.asm		mpn/x86_64/mulmid_basecase.asm \
	mul_1.asm		mpn/x86_64/mul_1.asm \
	addlsh2_n.asm		mpn/x86_64/aorrlsh2_n.asm \
	divrem_1.asm		mpn/x86_64/divrem_1.asm \
	copyd.asm		mpn/x86_64/copyd.asm \
	bdiv_dbm1c.asm		mpn/x86_64/bdiv_dbm1c.asm \
	div_qr_2n_pi1.asm		mpn/x86_64/div_qr_2n_pi1.asm \
	rsblsh1_n.asm		mpn/x86_64/aorrlsh1_n.asm \
	dive_1.asm		mpn/x86_64/dive_1.asm \
	andn_n.asm		mpn/x86_64/logops_n.asm \
	com.asm		mpn/x86_64/com.asm \
	copyi.asm		mpn/x86_64/copyi.asm \
	sub_err2_n.asm		mpn/x86_64/aors_err2_n.asm \
	add_err3_n.asm		mpn/x86_64/aors_err3_n.asm \
	mod_1_1.asm		mpn/x86_64/mod_1_1.asm \
	subcnd_n.asm		mpn/x86_64/aorscnd_n.asm \
	rshift.asm		mpn/x86_64/rshift.asm \
	addlsh_n.asm		mpn/x86_64/k8/aorrlsh_n.asm \
	iorn_n.asm		mpn/x86_64/logops_n.asm \
	ior_n.asm		mpn/x86_64/logops_n.asm \
	mod_34lsub1.asm		mpn/x86_64/mod_34lsub1.asm \
	popcount.asm		mpn/x86_64/popham.asm \
	invert_limb_table.asm		mpn/x86_64/invert_limb_table.asm \
	xnor_n.asm		mpn/x86_64/logops_n.asm \
	redc_1.asm		mpn/x86_64/redc_1.asm \
	lshiftc.asm		mpn/x86_64/lshiftc.asm \
	mode1o.asm		mpn/x86_64/mode1o.asm \
	mullo_basecase.asm		mpn/x86_64/mullo_basecase.asm \
	addlsh1_n.asm		mpn/x86_64/aorrlsh1_n.asm \
	mul_2.asm		mpn/x86_64/mul_2.asm \
	invert_limb.asm		mpn/x86_64/invert_limb.asm \
	addmul_1.asm		mpn/x86_64/aorsmul_1.asm \
	divrem_2.asm		mpn/x86_64/divrem_2.asm \
	mod_1_4.asm		mpn/x86_64/mod_1_4.asm \
	sub_n.asm		mpn/x86_64/aors_n.asm \
	gcd_1.asm		mpn/x86_64/gcd_1.asm \

# XXX using C versions
#	redc_1.asm		mpn/x86_64/redc_1.asm
#	gcd_1.asm		mpn/x86_64/gcd_1.asm
#	mod_1_4.asm		mpn/x86_64/mod_1_4.asm
#SRCS+=	redc_1.c gcd_1.c mod_1_4.c

GMP_LIMB_BITS=64
