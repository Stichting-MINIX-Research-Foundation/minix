# Makefile for the DDE kit Test driver (ddekitest)
PROG=	 ddekittest_driver
SRCS=	 ddekittest_driver.c
FILES=	 system.conf
SCRIPTS= ddekittest.sh
#uncomment the following for proper debugging
#CFLAGS+=-g
#LDFLAGS+= -Ttext=0x4000000
LDADD+=	 -lsys -lddekit 
DPADD+=	 ${LIBSYS} ${LIBDDEKIT} 

MAN=

BINDIR?= /usr/tests/minix-posix/ddekit
FILESDIR?= /usr/tests/minix-posix/ddekit

.include <minix.service.mk>
