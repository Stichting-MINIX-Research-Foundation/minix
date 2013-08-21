# $NetBSD: Makefile,v 1.3 2011/08/11 23:15:36 joerg Exp $

MODULE=	i810

SRCS=	i810context.c i810ioctl.c i810render.c i810screen.c i810span.c \
	i810state.c i810tex.c i810texmem.c i810texstate.c i810tris.c \
	i810vb.c

CWARNFLAGS.clang+=	-Wno-constant-logical-operand

.include "${.CURDIR}/../dri.mk"
