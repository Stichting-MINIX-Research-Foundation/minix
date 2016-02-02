#	$NetBSD: Makefile,v 1.6 2012/07/13 19:32:51 christos Exp $

WARNS?=	2	# XXX -Wcast-qual

NOMAN=	# defined
BINDIR=/usr/bin

.include <bsd.own.mk>

USE_FORT?= yes	# cryptographic software

# RCSid:
#	$Id: Makefile,v 1.6 2012/07/13 19:32:51 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that 
#	the above copyright notice and this notice are
#	left intact. 
#      
#	Please send copies of changes and bug-fixes to:
#	sjg@quick.com.au
#

PROG=	openssl

SRCS=	openssl.c
SRCS+=	apps.c ecparam.c ec.c pkeyparam.c genpkey.c pkey.c pkeyutl.c ts.c cms.c
SRCS+=	s_cb.c s_socket.c
SRCS+=	app_rand.c
SRCS+=	verify.c asn1pars.c req.c dgst.c dh.c dhparam.c enc.c passwd.c gendh.c errstr.c ca.c \
	pkcs7.c crl2p7.c crl.c \
	rsa.c rsautl.c dsa.c dsaparam.c \
	x509.c genrsa.c gendsa.c prime.c s_server.c s_client.c speed.c \
	s_time.c version.c sess_id.c srp.c \
	ciphers.c nseq.c pkcs12.c pkcs8.c spkac.c smime.c rand.c engine.c ocsp.c

CPPFLAGS+= -DMONOLITH -DUSE_SSL -I${OPENSSLSRC}

# with normal OpenSSL, OPENSSL_NO_RC5 changes ABI due to change in
# struct/union.  we nuked it in our header files and it is now safe.
.if ${MKCRYPTO_RC5} == "no"
CPPFLAGS+= -DOPENSSL_NO_RC5
.else
LDADD+=	-lcrypto_rc5
DPADD+=	${LIBCRYPTO_RC5}
.endif

# this must be _after_ the libcrypto_rc5/libcrypto_idea entries.
LDADD+=	-lssl -lcrypto -lcrypt
DPADD+=	${LIBSSL} ${LIBCRYPTO} ${LIBCRYPT}

CRYPTODIST=	${NETBSDSRCDIR}/crypto
.include "${NETBSDSRCDIR}/crypto/Makefile.openssl"
.PATH:	${OPENSSLSRC}/apps

#LINKS=	openssl verify \
#	openssl asn1pars \
#	openssl req \
#	openssl dgst \
#	openssl dh \
#	openssl dhparam \
#	openssl enc \
#	openssl passwd \
#	openssl gendh \
#	openssl errstr \
#	openssl ca \
#	openssl crl \
#	openssl rsa \
#	openssl rsautl \
#	openssl dsa \
#	openssl dsaparam \
#	openssl x509 \
#	openssl genrsa \
#	openssl gendsa \
#	openssl s_server \
#	openssl s_client \
#	openssl speed \
#	openssl s_time \
#	openssl version \
#	openssl pkcs7 \
#	openssl crl2pkcs7 \
#	openssl sess_id \
#	openssl ciphers \
#	openssl nseq \
#	openssl pkcs12 \
#	openssl pkcs8 \
#	openssl spkac \
#	openssl smime \
#	openssl rand \
#	openssl engine \
#	openssl ocsp

.if ${MKSHARE} != "no"
FILES=  CA.sh CA.pl openssl.cnf
FILESDIR=/usr/share/examples/openssl
.endif

.include <bsd.prog.mk>
