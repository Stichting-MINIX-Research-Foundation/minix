#	$NetBSD: Makefile,v 1.11 2011/03/06 18:15:30 bouyer Exp $

HOSTPROGNAME=	${_TOOL_PREFIX}installboot
HOST_SRCDIR=	usr.sbin/installboot

HOST_CPPFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/../mips-elf2ecoff
HOST_CPPFLAGS+=	-I${TOOLDIR}/include/nbinclude

.include "${.CURDIR}/../Makefile.nbincludes"
.include "${.CURDIR}/../Makefile.host"

