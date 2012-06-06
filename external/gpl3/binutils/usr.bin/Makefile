#	$NetBSD: Makefile,v 1.3 2009/09/23 19:41:24 skrll Exp $

.include <bsd.own.mk>

.if ${MKBINUTILS} != "no" && exists(${.CURDIR}/common/arch/${MACHINE_ARCH}/defs.mk)
.include "${.CURDIR}/common/arch/${MACHINE_ARCH}/defs.mk"

SSUBDIR+=	gas gprof ld ${G_PROGRAMS:C/-new$//:C/sysdump//:C/srconv//:cxxfilt=c++filt}
SUBDIR+=	common .WAIT ${SSUBDIR:O} # alphabetize

.endif	# MKBINUTILS != no

.include <bsd.subdir.mk>
