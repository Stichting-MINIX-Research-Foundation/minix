#  Makefile for ISA ethernet drivers	May  02, 2000
PROG=	dpeth
SRCS=	3c501.c 3c509.c 3c503.c ne.c wd.c 8390.c devio.c netbuff.c dp.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

CPPFLAGS+=	-DVERBOSE=0

WARNS?= 5

.include <minix.service.mk>
