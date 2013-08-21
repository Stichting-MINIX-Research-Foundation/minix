# $NetBSD: Makefile,v 1.1 2009/06/16 00:26:11 mrg Exp $

MODULE=	swrast

CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/swrast

DRI_EXTRA_PATHS=	${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/swrast
SRCS= \
	swrast.c \
        swrast_span.c

COPTS.swrast.c=		-Wno-error	# XXX const

.include "${.CURDIR}/../dri.mk"
