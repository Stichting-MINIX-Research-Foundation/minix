# Makefile for libblockdriver
.include <bsd.own.mk>

LIB=	blockdriver

SRCS=	driver.c drvlib.c driver_st.c driver_mt.c mq.c trace.c

.if ${USE_STATECTL} != "no"
CPPFLAGS+= -DUSE_STATECTL
.endif

.include <bsd.lib.mk>
