#	$NetBSD: Makefile,v 1.4 2015/07/08 01:08:24 matt Exp $
#

LIB=		c_vfp

USE_SHLIBDIR=	yes

.include <bsd.own.mk>

CPUFLAGS+=	-mfpu=vfp -marm

SRCS=		vfpsf.S vfpdf.S
AFLAGS.vfpsf.S+=	-mfpu=vfp
AFLAGS.vfpdf.S+=	-mfpu=vfp

.include <bsd.lib.mk>
