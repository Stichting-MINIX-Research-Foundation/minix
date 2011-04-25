# installboot: install the boot package; runs on the installer's machine

PROG=	installboot
BINDIR=	/usr/bin

SRCS=	installboot.c rawfs.c
.PATH:	${.CURDIR}/..
CPPFLAGS= -I${MINIXSRCDIR} -I${.CURDIR}/..

MAN=	# installboot.8

.include <bsd.prog.mk>
