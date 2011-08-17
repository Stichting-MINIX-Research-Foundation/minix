#	$NetBSD: Makefile,v 1.8 2007/05/28 12:06:25 tls Exp $

.if defined(__MINIX)
USE_FORT=no		# __SSP_FORTIFY_LEVEL=0
.else
USE_FORT?= yes	# data-driven bugs?
.endif
NOMAN=	   # defined

.include <bsd.own.mk>		# for MKDYNAMICROOT definition

.if defined(__MINIX)
MKDYNAMICROOT= no
.endif

PROG=       bzip2recover

.if (${MKDYNAMICROOT} == "no")
LDSTATIC?= -static
.endif

DIST=	   ${NETBSDSRCDIR}/dist/bzip2
.PATH:	   ${DIST}

.include <bsd.prog.mk>
