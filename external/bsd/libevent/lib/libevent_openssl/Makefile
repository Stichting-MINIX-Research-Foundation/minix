#	$NetBSD: Makefile,v 1.2 2013/09/11 23:04:10 joerg Exp $

.include <bsd.own.mk>

LIB=event_openssl

SRCS= \
bufferevent_openssl.c

LIBDPLIBS+= \
	event	${.CURDIR}/../libevent \
	crypto	${NETBSDSRCDIR}/crypto/external/bsd/openssl/lib/libcrypto \
	ssl	${NETBSDSRCDIR}/crypto/external/bsd/openssl/lib/libssl

.include <bsd.lib.mk>
