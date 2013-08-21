# $NetBSD: Makefile,v 1.3 2011/08/11 23:15:37 joerg Exp $

MODULE=	savage

SRCS=	savage_xmesa.c savagedd.c savagestate.c savagetex.c savagetris.c \
	savagerender.c savageioctl.c savagespan.c

CWARNFLAGS.clang+=	-Wno-constant-logical-operand -Wno-error

.include "${.CURDIR}/../dri.mk"
