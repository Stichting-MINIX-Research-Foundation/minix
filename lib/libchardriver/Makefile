# Makefile for libchardriver
.include <bsd.own.mk>

LIB=	chardriver

SRCS=	chardriver.c

.if ${USE_STATECTL} != "no"
CPPFLAGS+= -DUSE_STATECTL
.endif

.include <bsd.lib.mk>
