# $NetBSD: Makefile.inc,v 1.22 2008/08/29 00:02:25 gmcgarry Exp $

WARNS?=	1

.include <bsd.own.mk>

USE_FORT?= yes	# network client/server

DIST=	${NETBSDSRCDIR}/dist/dhcp
.PATH:	${DIST}/${DHCPSRCDIR}
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
LDADD+= ${OBJDIR}/lib/libbindexport/dns/libdnsexport.a
LDADD+= ${OBJDIR}/lib/libbindexport/isc/libiscexport.a
DPADD+= ${OBJDIR}/usr.sbin/dhcp/common/libdhcp.a
DPADD+= ${OBJDIR}/usr.sbin/dhcp/omapip/libomapi.a
DPADD+= ${OBJDIR}/usr.sbin/dhcp/dst/libdst.a
DPADD+= ${OBJDIR}/lib/libbindexport/dns/libdnsexport.a
DPADD+= ${OBJDIR}/lib/libbindexport/isc/libiscexport.a

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
