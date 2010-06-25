# Makefile for filter driver
PROG=	filter
SRCS=	main.c sum.c driver.c util.c optset.c crc.c md5.c

DPADD+= ${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+= -DDEBUG=1 -DDEBUG2=0

.include <bsd.prog.mk>
