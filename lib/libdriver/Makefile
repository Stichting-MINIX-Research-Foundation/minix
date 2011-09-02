# Makefile for libdriver
.include <bsd.own.mk>

LIB=	driver

SRCS=	driver.c drvlib.c mq.c

.if ${USE_STATECTL} != "no"
CPPFLAGS+= -DUSE_STATECTL
.endif

.include <bsd.lib.mk>
