#	$NetBSD: Makefile.inc,v 1.4 2014/02/26 15:35:05 christos Exp $

SRCS+= \
	add.c \
	add_1.c \
	bdiv_dbm1c.c \
	bdiv_q.c \
	bdiv_q_1.c \
	bdiv_qr.c \
	binvert.c \
	cmp.c \
	dcpi1_bdiv_q.c \
	dcpi1_bdiv_qr.c \
	dcpi1_div_q.c \
	dcpi1_div_qr.c \
	dcpi1_divappr_q.c \
	div_q.c \
	div_qr_2n_pi1.c \
	dive_1.c \
	diveby3.c \
	divexact.c \
	divis.c \
	divrem.c \
	divrem_1.c \
	divrem_2.c \
	dump.c \
	fib2_ui.c \
	gcd.c \
	gcd_1.c \
	gcd_subdiv_step.c \
	gcdext.c \
	gcdext_1.c \
	gcdext_lehmer.c \
	get_d.c \
	get_str.c \
	hgcd.c \
	hgcd2.c \
	hgcd_appr.c \
	hgcd_matrix.c \
	hgcd_reduce.c \
	hgcd_step.c \
	invert.c \
	invertappr.c \
	jacbase.c \
	lshiftc.c \
	matrix22_mul.c \
	matrix22_mul1_inverse_vector.c \
	mod_1.c \
	mod_1_1.c \
	mod_1_2.c \
	mod_1_3.c \
	mod_1_4.c \
	mod_34lsub1.c \
	mode1o.c \
	mu_bdiv_q.c \
	mu_bdiv_qr.c \
	mu_div_q.c \
	mu_div_qr.c \
	mu_divappr_q.c \
	mul.c \
	mul_basecase.c \
	mul_fft.c \
	mul_n.c \
	mullo_basecase.c \
	mullo_n.c \
	mulmod_bnm1.c \
	neg.c \
	nussbaumer_mul.c \
	perfpow.c \
	perfsqr.c \
	pow_1.c \
	powlo.c \
	powm.c \
	powm_sec.c \
	pre_divrem_1.c \
	pre_mod_1.c \
	random.c \
	random2.c \
	redc_1.c \
	redc_2.c \
	redc_n.c \
	remove.c \
	rootrem.c \
	sbpi1_bdiv_q.c \
	sbpi1_bdiv_qr.c \
	sbpi1_div_q.c \
	sbpi1_div_qr.c \
	sbpi1_divappr_q.c \
	scan0.c \
	scan1.c \
	set_str.c \
	sqr.c \
	sqr_basecase.c \
	sqrmod_bnm1.c \
	sqrtrem.c \
	sub.c \
	sub_1.c \
	subcnd_n.c \
	tdiv_qr.c \
	toom22_mul.c \
	toom2_sqr.c \
	toom32_mul.c \
	toom33_mul.c \
	toom3_sqr.c \
	toom42_mul.c \
	toom43_mul.c \
	toom44_mul.c \
	toom4_sqr.c \
	toom52_mul.c \
	toom53_mul.c \
	toom62_mul.c \
	toom63_mul.c \
	toom6_sqr.c \
	toom6h_mul.c \
	toom8_sqr.c \
	toom8h_mul.c \
	toom_couple_handling.c \
	toom_eval_dgr3_pm1.c \
	toom_eval_dgr3_pm2.c \
	toom_eval_pm1.c \
	toom_eval_pm2.c \
	toom_eval_pm2exp.c \
	toom_eval_pm2rexp.c \
	toom_interpolate_12pts.c \
	toom_interpolate_16pts.c \
	toom_interpolate_5pts.c \
	toom_interpolate_6pts.c \
	toom_interpolate_7pts.c \
	toom_interpolate_8pts.c \
	trialdiv.c \
	zero.c \

C_SRCS_LIST= \
	hamdist.c		mpn/generic/popham.c \
	popcount.c		mpn/generic/popham.c \

ASM_SRCS_LIST= \
	nand_n.asm		mpn/powerpc64/logops_n.asm \
	sqr_diagonal.asm	mpn/powerpc64/mode32/sqr_diagonal.asm \
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

GMP_LIMB_BITS=64

M4FLAGS= -DPIC
