#	$NetBSD: Makefile,v 1.2 2006/10/27 22:36:23 uwe Exp $
#

GNUHOSTDIST=	${.CURDIR}/../../gnu/dist/gmake

CONFIGURE_ENV+=	CC=${HOST_CC:Q} \
		CFLAGS=${HOST_CFLAGS:Q} \
		CPPFLAGS=${HOST_CPPFLAGS:Q} \
		CXX=${HOST_CXX:Q}

CONFIGURE_ARGS=	--program-prefix=${_TOOL_PREFIX}g
CONFIGURE_ARGS+=--disable-nls

.NOTPARALLEL:

.include "${.CURDIR}/../Makefile.gnuhost"
