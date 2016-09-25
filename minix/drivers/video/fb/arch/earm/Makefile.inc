# Makefile for arch-dependent framebuffer code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

.if ${BSP_NAME} == omap
SRCS=	fb_arch_omap.c
.elif ${BSP_NAME} == rpi
SRCS=	fb_arch_rpi.c
.else
.error Unknown bsp ${BSP_NAME}
.endif
