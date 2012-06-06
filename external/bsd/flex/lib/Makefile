#	$NetBSD: Makefile,v 1.2 2009/10/28 11:20:07 njoly Exp $

NOPIC=		# defined

.include <bsd.own.mk>

LIB=		l
SRCS=		libmain.c libyywrap.c

.if ${MKSTATICLIB} != "no"
LINKS=		${LIBDIR}/libl.a ${LIBDIR}/libfl.a
.endif

.PATH:		${NETBSDSRCDIR}/usr.bin/lex

.if ${MKPROFILE} != "no"
LINKS+=		${LIBDIR}/libl_p.a ${LIBDIR}/libfl_p.a
.endif

.if ${MKDEBUGLIB} != "no"
LINKS+=		${LIBDIR}/libl_g.a ${LIBDIR}/libfl_g.a
.endif

.include <bsd.lib.mk>
