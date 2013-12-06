#	$NetBSD: Makefile,v 1.3 2013/08/19 22:22:23 matt Exp $
#

LIB=		c_vfp

USE_SHLIBDIR=	yes

.include <bsd.own.mk>

CPUFLAGS+=	-mfpu=vfp -marm

SRCS=		vfpsf.S vfpdf.S

.include <bsd.lib.mk>
