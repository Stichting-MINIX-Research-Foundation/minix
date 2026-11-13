# $NetBSD: abi.mk,v 1.1 2021/06/16 05:21:08 rin Exp $

.if !empty(MACHINE_ARCH:Mearm*hf*)
CPPFLAGS+=	-DCOMPILER_RT_ARMHF_TARGET
.endif
