# $NetBSD: Makefile,v 1.11 2017/05/21 15:28:42 riastradh Exp $

.include <bsd.own.mk>

#__MINIX: Unsupported: threads
SUBDIR+=bf bn cast conf des dh dsa ec ecdh ecdsa engine evp hmac lhash \
	md2 md4 md5 rand rc2 rc4 ripemd rsa sha sha1 srp x509v3

SUBDIR+=idea mdc2
SUBDIR+=rc5

TESTSDIR=	${TESTSBASE}/crypto/libcrypto

TESTS_SH=	t_certs
TESTS_SH+=	t_ciphers
TESTS_SH+=	t_hashes
TESTS_SH+=	t_libcrypto
TESTS_SH+=	t_pubkey

.include <bsd.test.mk>
