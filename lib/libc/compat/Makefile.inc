#	$NetBSD: Makefile.inc,v 1.7 2012/01/22 18:36:16 christos Exp $

CPPFLAGS+=	-I${NETBSDSRCDIR}/sys

COMPATARCHDIR=${COMPATDIR}/arch/${ARCHSUBDIR}
.PATH: ${COMPATARCHDIR}/gen ${COMPATARCHDIR}/sys

.if !defined(__MINIX)
.include "${COMPATDIR}/db/Makefile.inc"
.include "${COMPATDIR}/locale/Makefile.inc"
.endif #!defined(__MINIX)
.include "${COMPATDIR}/gen/Makefile.inc"
.if !defined(__MINIX)
.include "${COMPATDIR}/net/Makefile.inc"
.include "${COMPATDIR}/rpc/Makefile.inc"
.include "${COMPATDIR}/stdio/Makefile.inc"
.if !defined(__MINIX)
.include "${COMPATDIR}/stdlib/Makefile.inc"
.endif #!defined(__MINIX)
.include "${COMPATDIR}/sys/Makefile.inc"
.include "${COMPATDIR}/time/Makefile.inc"
.include "${COMPATARCHDIR}/Makefile.inc"
.endif #!defined(__MINIX)
