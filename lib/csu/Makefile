#	$NetBSD: Makefile,v 1.31 2012/06/16 19:19:14 joerg Exp $

.include <bsd.own.mk>

.if ${USE_COMPILERCRTSTUFF} != "yes"

.if defined(CSU_MACHINE_ARCH)
.  if !exists(${.CURDIR}/arch/${CSU_MACHINE_ARCH}/Makefile.inc)
.  error CSU_MACHINE_ARCH (${CSU_MACHINE_ARCH}) is unsupported
.  endif
.elif exists(${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc)
CSU_MACHINE_ARCH=	${MACHINE_ARCH}
.elif exists(${.CURDIR}/arch/${MACHINE_CPU}/Makefile.inc)
CSU_MACHINE_ARCH=	${MACHINE_CPU}
.else
.error Architecture (${MACHINE_ARCH} or ${MACHINE_CPU}) unsupported
.endif

ARCHDIR:=	${.CURDIR}/arch/${CSU_MACHINE_ARCH}

.PATH:	${ARCHDIR}
.  include "${ARCHDIR}/Makefile.inc"

.  include "${.CURDIR}/common/Makefile.inc"

.else

CSU_MACHINE_ARCH?=	${MACHINE_ARCH}

.  if exists(${CSU_MACHINE_ARCH}_elf/Makefile)
SUBDIR=	${CSU_MACHINE_ARCH}_elf
.  elif exists(${CSU_MACHINE_ARCH}/Makefile)
SUBDIR=	${CSU_MACHINE_ARCH}
.  elif exists(${MACHINE_CPU}_elf/Makefile)
SUBDIR=	${MACHINE_CPU}_elf
.  elif exists(${MACHINE_CPU}/Makefile)
SUBDIR=	${MACHINE_CPU}
.  else
.BEGIN:
	@echo no SUBDIR for ${MACHINE_ARCH}_elf, ${MACHINE_ARCH} nor \
	    ${MACHINE_CPU}
	@false
.  endif

.  include <bsd.subdir.mk>
.endif
