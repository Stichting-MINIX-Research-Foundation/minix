#	$NetBSD: Makefile.inc,v 1.3 2015/02/09 22:44:34 pooka Exp $

.include "${COMPATARCHDIR}/gen/Makefile.inc"
.if ${RUMPRUN} != "yes"
.include "${COMPATARCHDIR}/sys/Makefile.inc"
.endif
