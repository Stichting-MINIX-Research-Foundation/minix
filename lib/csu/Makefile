#	$NetBSD: Makefile,v 1.33 2013/09/10 16:45:33 matt Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
USE_BITCODE:=	no
.endif # defined(__MINIX)

.if ${USE_COMPILERCRTSTUFF} != "yes"

CSU_MACHINE_ARCH?=	${MACHINE_ARCH}

.if !empty(CSU_MACHINE_ARCH:Mearm*)
ARCHDIR:=	${.CURDIR}/arch/earm
.elif exists(${.CURDIR}/arch/${CSU_MACHINE_ARCH}/Makefile.inc)
ARCHDIR:=	${.CURDIR}/arch/${CSU_MACHINE_ARCH}
.elif exists(${.CURDIR}/arch/${MACHINE_CPU}/Makefile.inc)
ARCHDIR:=	${.CURDIR}/arch/${MACHINE_CPU}
.else
.error Architecture (${CSU_MACHINE_ARCH} or ${MACHINE_CPU}) unsupported
.endif


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
