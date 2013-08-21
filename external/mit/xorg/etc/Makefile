# $NetBSD: Makefile,v 1.1 2010/11/12 17:05:48 tsutsui Exp $

.include <bsd.own.mk>

.if exists(etc.${RELEASEMACHINE}/Makefile.inc)
.include "etc.${RELEASEMACHINE}/Makefile.inc"
.endif

FILESDIR=	${X11ETCDIR}
CONFIGFILES=
CONFIGSYMLINKS=

.if defined(XORGCONF_FILES)
.for file in ${XORGCONF_FILES}
CONFIGFILES+=		${file}
FILESMODE_${file:T}=	644
.endfor
.endif

.include <bsd.prog.mk>
