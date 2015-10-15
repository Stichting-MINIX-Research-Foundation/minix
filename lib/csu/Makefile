#	$NetBSD: Makefile,v 1.35 2014/08/10 23:25:49 matt Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
USE_BITCODE:=	no
.endif # defined(__MINIX)

CSU_MACHINE_ARCH?=	${MACHINE_ARCH}
CSU_MACHINE_CPU?=	${MACHINE_CPU}

.if !empty(CSU_MACHINE_ARCH:Mearm*)
ARCHDIR:=	${.CURDIR}/arch/earm
.elif exists(${.CURDIR}/arch/${CSU_MACHINE_ARCH}/Makefile.inc)
ARCHDIR:=	${.CURDIR}/arch/${CSU_MACHINE_ARCH}
.elif exists(${.CURDIR}/arch/${CSU_MACHINE_CPU}/Makefile.inc)
ARCHDIR:=	${.CURDIR}/arch/${CSU_MACHINE_CPU}
.else
.error Architecture (${CSU_MACHINE_ARCH} or ${CSU_MACHINE_CPU}) unsupported
.endif


.PATH:	${ARCHDIR}
.include "${ARCHDIR}/Makefile.inc"
.include "${.CURDIR}/common/Makefile.inc"
