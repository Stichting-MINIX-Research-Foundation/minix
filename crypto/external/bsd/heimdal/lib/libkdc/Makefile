# $NetBSD: Makefile,v 1.3.4.1 2018/07/16 13:32:27 sborrill Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

USE_FORT?= yes	# network protocol library
NOLINT=

.PATH:		${HEIMDIST}/kdc ${HEIMBASE}/include

LIB=		kdc

LIBDPLIBS+=	krb5		${.CURDIR}/../libkrb5		\
		crypto		${SSLBASE}/lib/libcrypto	\
		hdb		${.CURDIR}/../libhdb		\
		hx509		${.CURDIR}/../libhx509		\
		asn1		${.CURDIR}/../libasn1		\
		heimbase	${.CURDIR}/../libheimbase	\
		heimntlm	${.CURDIR}/../libheimntlm	\
		roken		${.CURDIR}/../libroken

SRCS =	default_config.c	\
        set_dbinfo.c		\
        digest.c		\
        fast.c			\
        kerberos5.c		\
        krb5tgs.c		\
        pkinit.c		\
        pkinit-ec.c		\
        log.c			\
        misc.c			\
        kx509.c			\
        process.c		\
        windc.c

INCSDIR=/usr/include/krb5
INCS= kdc-protos.h kdc.h

CPPFLAGS+= -I${HEIMDIST}/lib/krb5

# Prevent collision with old MIT Kerberos includes -- require manual
# intervention of the operator.
.BEGIN:
.ifmake includes
	@if [ -f ${DESTDIR}${INCSDIR}/kadm_err.h ]; then \
		echo "Error: you must first remove the MIT headers from ${DESTDIR}${INCSDIR}" >&2; \
		false; \
	fi
.endif

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.lib.mk>
