# Makefile for Intel Pro/100 driver (FXP)
PROG=	fxp
SRCS=	fxp.c mii.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

WARNS?= 5

.include <minix.service.mk>
