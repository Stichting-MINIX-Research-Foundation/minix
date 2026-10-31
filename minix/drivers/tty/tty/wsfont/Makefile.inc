# Makefile for framebuffer console driver

.include <bsd.own.mk>

HERE=${.CURDIR}/wsfont
.PATH:  ${HERE}

SRCS +=	wsfont.c

