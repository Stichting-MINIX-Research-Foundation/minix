#	$NetBSD: bn.inc,v 1.2 2011/06/06 06:44:57 spz Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/bn/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/bn

.if !defined(BN_SRCS)
BN_SRCS = bn_asm.c
.endif

BN_SRCS += bn_add.c bn_div.c bn_exp.c bn_lib.c bn_ctx.c bn_mod.c \
	bn_print.c bn_rand.c bn_shift.c bn_word.c bn_blind.c bn_mul.c \
	bn_kron.c bn_sqrt.c bn_gcd.c bn_prime.c bn_err.c bn_sqr.c \
	bn_recp.c bn_mont.c bn_mpi.c bn_exp2.c bn_nist.c bn_gf2m.c bn_depr.c \
	bn_const.c bn_x931p.c
SRCS += ${BN_SRCS}

.for cryptosrc in ${BN_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/bn
.endfor
