#	$NetBSD: Makefile,v 1.3 2014/07/23 07:16:14 alnsn Exp $
#
# Public Domain.
#

.PATH:	${.CURDIR}/../../../../external/bsd/sljit/sljit \
	${.CURDIR}/../../../../external/bsd/sljit/dist/sljit_src

LIB=	rumpkern_sljit

SRCS=	sljitLir.c sljit_mod.c

# NOTE This is not the best place for icache sync routine but only
# sljit uses it at the moment.
# XXX Think about a good hypercall interface (hi, pooka!) and move
# this stuff to rumpuser.
.if !empty(MACHINE_ARCH:Mmips*)
SRCS+=			cache.c
RUMPCOMP_USER_SRCS=	sljit_rump.c
.PATH:			${.CURDIR}/arch/mips

RUMPCOMP_INCS_DIR:=	${.PARSEDIR}
RUMPCOMP_USER_CPPFLAGS=-I${RUMPCOMP_INCS_DIR}
.endif

.if !empty(MACHINE_ARCH:Marm*) || !empty(MACHINE_ARCH:Mearm*)
SRCS+=			cpufunc.c
RUMPCOMP_USER_SRCS=	sljit_rump.c
.PATH:			${.CURDIR}/arch/arm

RUMPCOMP_INCS_DIR:=	${.PARSEDIR}
RUMPCOMP_USER_CPPFLAGS=-I${RUMPCOMP_INCS_DIR}

# Link to libarm to get arm_sync_icache(2)
LDADD+=	-larm
.endif

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
