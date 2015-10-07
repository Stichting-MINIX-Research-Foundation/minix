# $NetBSD: Makefile,v 1.1 2011/04/13 19:16:56 elric Exp $

NOLINT=		# defined

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/wind

LIB=		wind

LIBDPLIBS+=	com_err ${HEIMBASE}/lib/libcom_err	\
		crypt ${NETBSDSRCDIR}/lib/libcrypt	\
		roken ${HEIMBASE}/lib/libroken


HEIMSRCS = wind_err.et

SRCS=				\
	bidi.c			\
	bidi_table.c		\
	combining.c		\
	combining_table.c	\
	doxygen.c		\
	errorlist.c		\
	errorlist_table.c	\
	ldap.c			\
	map.c			\
	map_table.c		\
	normalize.c		\
	normalize_table.c	\
	punycode.c		\
	stringprep.c		\
	utf8.c

INCSDIR=/usr/include/krb5
INCS= wind.h ${COMPILE_ET_INCS}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.lib.mk>
