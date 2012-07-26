#	$NetBSD: Makefile.inc,v 1.32.12.1 2011/01/09 20:43:30 riz Exp $

.if !defined(BINDEXPORT_MAKEFILE_INC)
BINDEXPORT_MAKEFILE_INC=yes

USE_FORT?= yes	# network client/server

WARNS?=	1

.include <bsd.own.mk>

.if ${MKCRYPTO} == "no"
NAMED_USE_OPENSSL?=no
.endif

NAMED_USE_PTHREADS?=no

NAMED_USE_OPENSSL?=no

IDIST=		${NETBSDSRCDIR}/dist/bind
EXPORTDIR=      ${NETBSDSRCDIR}/lib/libbindexport

.include "${IDIST}/version"

VERSION=${MAJORVER}.${MINORVER}.${PATCHVER}${RELEASETYPE}${RELEASEVER}
SYSCONFDIR=/etc
LOCALSTATEDIR=/var

CPPFLAGS+=  -I${EXPORTDIR} \
	-I${EXPORTDIR}/dns/include \
	-I${EXPORTDIR}/isc/include \
	-DNS_LOCALSTATEDIR=\"${LOCALSTATEDIR}\" \
	-DNS_SYSCONFDIR=\"${SYSCONFDIR}\" \
	-DSESSION_KEYFILE=\"${LOCALSTATEDIR}/run/named/session.key\" \
	-DVERSION=\"${VERSION}\"

.if (${USE_INET6} != "no")
CPPFLAGS+=	-DWANT_IPV6
.endif

.if defined(HAVE_GCC) && ${HAVE_GCC} == 4
COPTS+=	-Wno-pointer-sign
.endif


.if ${NAMED_USE_PTHREADS} == "yes"
.if !defined (LIB) || empty(LIB)
LDADD+= -lpthread
DPADD+= ${LIBPTHREAD}
.else
LIBDPLIBS+=      pthread  ${.CURDIR}/../libpthread
.endif
.endif

.if ${NAMED_USE_OPENSSL} == "yes"
CPPFLAGS+=-DOPENSSL
.if !defined (LIB) || empty(LIB)
LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}
.else
LIBDPLIBS+=	crypto	${.CURDIR}/../libcrypto
.endif
.endif

.if ${NAMED_USE_PTHREADS} == "yes"
CPPFLAGS+=-DISC_PLATFORM_USETHREADS -I${IDIST}/lib/isc/pthreads/include
.else
CPPFLAGS+=-I${IDIST}/lib/isc/nothreads/include
.endif

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
.endif
