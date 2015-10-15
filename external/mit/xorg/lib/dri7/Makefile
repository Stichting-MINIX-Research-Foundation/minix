# $NetBSD: Makefile,v 1.1 2015/01/05 03:45:31 mrg Exp $

# Build MesaLib 7.x r300 driver, which requires LLVM in Mesa 10.x

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
SUBDIR=		libmesa .WAIT r300
.endif

.include <bsd.subdir.mk>
