# $NetBSD: Makefile,v 1.18 2012/04/13 12:31:19 njoly Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libm

TESTS_C+=	t_acos
TESTS_C+=	t_asin
TESTS_C+=	t_atan
TESTS_C+=	t_cbrt
TESTS_C+=	t_ceil
TESTS_C+=	t_cos
TESTS_C+=	t_cosh
TESTS_C+=	t_erf
TESTS_C+=	t_exp
TESTS_C+=	t_infinity
TESTS_C+=	t_ldexp
TESTS_C+=	t_log
TESTS_C+=	t_pow
TESTS_C+=	t_round
.if !defined(__MINIX)
TESTS_C+=	t_scalbn
.endif # !defined(__MINIX)
TESTS_C+=	t_sin
TESTS_C+=	t_sinh
TESTS_C+=	t_sqrt
TESTS_C+=	t_tan
TESTS_C+=	t_tanh

LDADD+=		-lm
#COPTS+=	-Wfloat-equal

.include <bsd.test.mk>
