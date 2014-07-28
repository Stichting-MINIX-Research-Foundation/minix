NOGCCERROR=yes
NOCLANGERROR=yes
CPPFLAGS+= -D_MINIX_SYSTEM

# Makefile for libblockdriver
.include <bsd.own.mk>

LIB=	blockdriver

SRCS=	driver.c drvlib.c driver_st.c driver_mt.c mq.c trace.c

.include <bsd.lib.mk>
