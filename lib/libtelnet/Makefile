#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$NetBSD: Makefile,v 1.36 2012/08/10 12:20:10 joerg Exp $

USE_FORT?= yes	# network protocol library

LIBISPRIVATE=	yes

.include <bsd.own.mk>

WARNS?=	5

LIB=	telnet
SRCS=	auth.c encrypt.c genget.c getent.c misc.c

CPPFLAGS+= -DHAS_CGETENT
CPPFLAGS+= -I${.CURDIR}

.if ${MKCRYPTO} != "no"
SRCS+=	enc_des.c
CPPFLAGS+= -DENCRYPTION -DAUTHENTICATION
CPPFLAGS+= -DDES_ENCRYPTION
.endif

.if ${USE_KERBEROS} != "no"
SRCS+=	kerberos5.c
CPPFLAGS+= -DKRB5
.endif

.if ${USE_PAM} != "no" && ${MKCRYPTO} != "no"
SRCS+=	sra.c pk.c
CPPFLAGS+= -DSRA
.endif

.for f in auth enc_des kerberos5 pk
COPTS.${f}.c+=  -Wno-pointer-sign
.endfor

.include <bsd.lib.mk>
