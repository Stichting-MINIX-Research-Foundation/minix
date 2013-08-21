# $NetBSD: Makefile,v 1.3 2011/08/11 23:15:36 joerg Exp $

MODULE=	mga

SRCS=	mgadd.c mgaioctl.c mgarender.c mgastate.c mgatris.c mgapixel.c \
	mgaspan.c mgatex.c mgatexmem.c mga_texstate.c mga_texcombine.c \
	mgavb.c mga_xmesa.c

CWARNFLAGS.clang+=	-Wno-constant-logical-operand

.include "${.CURDIR}/../dri.mk"
