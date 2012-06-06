#	$NetBSD: Makefile.inc,v 1.3 2009/11/01 00:12:00 joerg Exp $

BINDIR?=	/usr/sbin
WARNS?=		1

LDADD+=-lz
.ifndef HOSTPROG
DPADD+=${LIBZ}
.endif
