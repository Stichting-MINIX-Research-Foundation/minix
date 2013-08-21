#	$NetBSD: Makefile,v 1.33 2011/08/02 09:11:38 mrg Exp $

SUBDIR=		common init ddc dixmods doc dri dri2 i2c loader \
		xf86modes exa \
		xorgos ramdac shadowfb vbe vgahw \
		x86emu xaa xf86config

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "powerpc"
SUBDIR+=	int10 
.endif

SUBDIR+=	.WAIT utils

SUBDIR+=	.WAIT Xorg

.include <bsd.subdir.mk>
