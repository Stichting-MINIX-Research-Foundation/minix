#	$NetBSD: Makefile,v 1.6 2012/01/22 18:36:16 christos Exp $
#	@(#)Makefile	8.2 (Berkeley) 2/3/94
#

.include "${.CURDIR}/../Makefile.inc"

LIB=		cold
CPPFLAGS+=	-I${.CURDIR}/../include -I${.CURDIR}/..
CPPFLAGS+=	-I${.CURDIR}/../arch/${ARCHSUBDIR}

COMPATDIR=${.CURDIR}

.include "${.CURDIR}/Makefile.inc"

.include "${.CURDIR}/db/Makefile.inc"
.include "${.CURDIR}/gen/Makefile.inc"
.include "${.CURDIR}/locale/Makefile.inc"
.include "${.CURDIR}/net/Makefile.inc"
.if !defined(__MINIX)
.include "${.CURDIR}/rpc/Makefile.inc"
.endif # !defined(__MINIX)
.include "${.CURDIR}/stdio/Makefile.inc"
.include "${.CURDIR}/stdlib/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"

COMPATARCHDIR=${.CURDIR}/arch/${ARCHSUBDIR}
.PATH: ${COMPATARCHDIR}/gen ${COMPATARCHDIR}/sys

.include <bsd.lib.mk>
