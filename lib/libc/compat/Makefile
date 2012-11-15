#	$NetBSD: Makefile,v 1.6 2012/01/22 18:36:16 christos Exp $
#	@(#)Makefile	8.2 (Berkeley) 2/3/94
#

.include "${.CURDIR}/../Makefile.inc"

LIB=		cold
CPPFLAGS+=	-I${.CURDIR}/../include -I${.CURDIR}/..
CPPFLAGS+=	-I${.CURDIR}/../arch/${ARCHSUBDIR}

COMPATDIR=${.CURDIR}

.include "${.CURDIR}/Makefile.inc"

.if !defined(__MINIX)
.include "${.CURDIR}/db/Makefile.inc"
.endif # !defined(__MINIX)
.include "${.CURDIR}/gen/Makefile.inc"
.if !defined(__MINIX)
.include "${.CURDIR}/locale/Makefile.inc"
.include "${.CURDIR}/net/Makefile.inc"
.include "${.CURDIR}/rpc/Makefile.inc"
.include "${.CURDIR}/stdio/Makefile.inc"
.endif # !defined(__MINIX)
.include "${.CURDIR}/stdlib/Makefile.inc"
.if !defined(__MINIX)
.include "${.CURDIR}/sys/Makefile.inc"
.endif # !defined(__MINIX)

COMPATARCHDIR=${.CURDIR}/arch/${ARCHSUBDIR}
.PATH: ${COMPATARCHDIR}/gen ${COMPATARCHDIR}/sys

.include <bsd.lib.mk>
