# $NetBSD: Makefile.inc,v 1.1 2013/03/14 07:10:05 jmmv Exp $

TOPDIR=		${NETBSDSRCDIR}/external/bsd/atf
SRCDIR=		${TOPDIR}/dist

ATF_VERSION!=	grep 'define VERSION' ${TOPDIR}/lib/libatf-c/bconfig.h \
		| cut -d '"' -f 2
