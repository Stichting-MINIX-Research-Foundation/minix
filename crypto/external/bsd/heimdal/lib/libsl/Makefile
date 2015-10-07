# $NetBSD: Makefile,v 1.1 2011/04/13 19:16:55 elric Exp $

USE_FORT?= yes	# network protocol library

NOLINT=		# defined

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/sl

LIB=		sl

LIBDPLIBS+=	edit		${NETBSDSRCDIR}/lib/libedit	\
		terminfo	${NETBSDSRCDIR}/lib/libterminfo	\
		roken		${HEIMBASE}/lib/libroken

SRCS=		sl.c
INCS=		sl.h
INCSDIR=	/usr/include/krb5

.if ${USETOOLS} != "yes"
.PHONY:		slc
includes:	slc
SUBDIR=		slc

slc:
	@cd ${.CURDIR}/slc && ${MAKE}
.endif

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.lib.mk>
.if ${USETOOLS} != "yes"
.include <bsd.subdir.mk>
.endif
