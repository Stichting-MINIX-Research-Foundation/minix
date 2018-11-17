#	$NetBSD: Makefile,v 1.4 2017/05/21 15:28:37 riastradh Exp $

.include "bsd.own.mk"

# OpenSSL libraries.
SUBDIR= libcrypto libdes

SUBDIR+= .WAIT	libssl		# depends on libcrypto

.include <bsd.subdir.mk>
