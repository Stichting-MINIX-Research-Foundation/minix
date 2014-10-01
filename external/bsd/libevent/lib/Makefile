#	$NetBSD: Makefile,v 1.3 2013/09/11 23:04:10 joerg Exp $
.include <bsd.own.mk>

SUBDIR=	 libevent .WAIT
.if ${MKCRYPTO} != "no"
SUBDIR+=	libevent_openssl
.endif
# MINIX3: insufficient pthreads support in the base system for now
#SUBDIR+= libevent_pthreads

.include <bsd.subdir.mk>
