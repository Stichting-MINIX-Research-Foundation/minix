#	$NetBSD: Makefile,v 1.7 2007/07/03 16:29:24 joerg Exp $

HOSTPROGNAME=	${MACHINE_GNU_PLATFORM}-install
HOST_SRCDIR=	usr.bin/xinstall
CPPFLAGS+=	-I${.CURDIR}/../compat/sys
CPPFLAGS+=	-DTARGET_STRIP=\"${STRIP}\"

.include "${.CURDIR}/../Makefile.host"

INSTALL=	./xinstall
