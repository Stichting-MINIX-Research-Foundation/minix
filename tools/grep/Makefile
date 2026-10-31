#	$NetBSD: Makefile,v 1.4 2018/08/12 09:03:42 christos Exp $
#

HOSTPROGNAME=   ${_TOOL_PREFIX}grep
HOST_SRCDIR=    usr.bin/grep

HOST_CPPFLAGS+=	-DWITHOUT_NLS -DWITHOUT_BZ2 -DWITHOUT_GZIP

.include "${.CURDIR}/../Makefile.host"
