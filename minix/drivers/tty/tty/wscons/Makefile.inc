# Makefile for framebuffer console driver

.include <bsd.own.mk>

HERE=${.CURDIR}/wscons
.PATH:  ${HERE}

SRCS +=	wsemul_vt100.c
SRCS +=	wsemul_vt100_subr.c
SRCS +=	wsemul_dumb.c
SRCS += wsdisplay_vcons.c


