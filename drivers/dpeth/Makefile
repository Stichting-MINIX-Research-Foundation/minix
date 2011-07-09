##
##  Makefile for ISA ethernet drivers	May  02, 2000
##
PROG=	dpeth
SRCS=	3c501.c 3c509.c 3c503.c ne.c wd.c 8390.c devio.c netbuff.c dp.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

MAN=

BINDIR?= /usr/sbin

debug=		0
CPPFLAGS+=	-Ddebug=$(debug)

.include <minix.service.mk>
