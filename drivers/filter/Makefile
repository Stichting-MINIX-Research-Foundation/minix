# Makefile for filter driver
PROG=	filter
SRCS=	main.c sum.c driver.c util.c crc.c md5.c

DPADD+= ${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+= -DDEBUG=1 -DDEBUG2=0

.include <minix.service.mk>
