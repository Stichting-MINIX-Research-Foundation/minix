#	$NetBSD: Makefile,v 1.5 2013/01/18 18:09:56 christos Exp $

# RCSid:
#	Id: Makefile,v 1.33 1998/11/11 11:53:53 sjg Exp
#
#	@(#) Copyright (c) 1994 Simon J. Gerraty
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

.include <bsd.own.mk>

USE_FORT?=yes	# cryptographic software and network library

# XXX There's a bit of work to do before we can enable warnings.
WARNS=0
CWARNFLAGS.clang+=	-Wno-unused-value

LIB=	ssl
CPPFLAGS+= -Dlib${LIB} -I${OPENSSLSRC} -I${OPENSSLSRC}/crypto

CRYPTODIST=	${NETBSDSRCDIR}/crypto

.include "${NETBSDSRCDIR}/crypto/Makefile.openssl"
.PATH: ${OPENSSLSRC} ${OPENSSLSRC}/crypto/pqueue

.include "srcs.inc"

LIBDPLIBS+=crypto ${.CURDIR}/../libcrypto

AFLAGS+=-DELF

# This is from the include/openssl directory; see ../libcrypto/Makefile
INCS=	dtls1.h ssl.h ssl2.h ssl3.h ssl23.h tls1.h kssl.h srtp.h
INCSDIR=/usr/include/openssl

# this rebuilds the `srcs.inc' and the .inc files it reads.
# note that we have no idea, rc5 or rsa here so we include
# them explicitly above if we are using these ciphers.
# XXX
# we patch `ssl.inc' as we need to be able to turn off SSLv2.
update_inc:
	(cd ${.CURDIR}; \
	find ${OPENSSLSRC}/ssl -name Makefile | \
		perl ${OPENSSLSRC}/extsrcs.pl 2> srcs.inc; \
	patch -s ssl.inc < ssl.diff )

PKGCONFIG=libssl openssl
.include "${.CURDIR}/../../pkgconfig.mk"

.include <bsd.lib.mk>
