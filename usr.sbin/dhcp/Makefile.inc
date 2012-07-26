# $NetBSD: Makefile.inc,v 1.22 2008/08/29 00:02:25 gmcgarry Exp $

WARNS?=	1

.include <bsd.own.mk>

USE_FORT?= yes	# network client/server

DIST=	${NETBSDSRCDIR}/dist/dhcp
.PATH:	${DIST}/${DHCPSRCDIR}

COBJDIR!=cd $(.CURDIR)/../common && ${PRINTOBJDIR}
OMOBJDIR!=cd $(.CURDIR)/../omapip && ${PRINTOBJDIR}
DSTOBJDIR!=cd $(.CURDIR)/../dst && ${PRINTOBJDIR}

.if defined(HAVE_GCC) && ${HAVE_GCC} == 4
COPTS+=	-fno-strict-aliasing
.endif

BINDDIR= ${NETBSDSRCDIR}/dist/bind
BINDINCLUDES= -I${NETBSDSRCDIR}/lib/libbindexport/dns/include \
	-I${NETBSDSRCDIR}/lib/libbindexport/isc/include \

CPPFLAGS+= -I${DIST} -I${DIST}/includes -Wno-unused -I{.CURDIR}
CPPFLAGS+= ${BINDINCLUDES}

CPPFLAGS+=  -D_NETBSD_SOURCE

LDADD+= ${COBJDIR}/libdhcp.a
LDADD+= ${OMOBJDIR}/libomapi.a ${DSTOBJDIR}/libdst.a
LDADD+= ${NETBSDSRCDIR}/lib/libbindexport/dns/libdnsexport.a
LDADD+= ${NETBSDSRCDIR}/lib/libbindexport/isc/libiscexport.a
DPADD+= ${COBJDIR}/libdhcp.a
DPADD+= ${OMOBJDIR}/libomapi.a ${DSTOBJDIR}/libdst.a
DPADD+= ${NETBSDSRCDIR}/lib/libbindexport/dns/libdnsexport.a
DPADD+= ${NETBSDSRCDIR}/lib/libbindexport/isc/libiscexport.a

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
