#	$NetBSD: Makefile,v 1.6 2016/01/29 14:42:48 christos Exp $

.include <bsd.own.mk>

BINUTILS_MACHINE_ARCH=${MACHINE_ARCH:C/armv[4-7]/arm/}

.if ${MKBINUTILS} != "no" && exists(${.CURDIR}/common/arch/${BINUTILS_MACHINE_ARCH}/defs.mk)
.include "${.CURDIR}/common/arch/${BINUTILS_MACHINE_ARCH}/defs.mk"

SSUBDIR+=	gas gprof ld ${G_PROGRAMS:C/-new$//:C/sysdump//:C/bfdtest1//:C/bfdtest2//:C/srconv//:cxxfilt=c++filt/}
SUBDIR+=	common .WAIT ${SSUBDIR:O} # alphabetize

.endif	# MKBINUTILS != no

.include <bsd.subdir.mk>
