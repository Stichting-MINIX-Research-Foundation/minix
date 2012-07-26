#	$NetBSD: Makefile.inc,v 1.32.12.1 2011/01/09 20:43:30 riz Exp $

.if !defined(BIND9_MAKEFILE_INC)
BIND9_MAKEFILE_INC=yes

USE_FORT?= yes	# network client/server

WARNS?=	1

.include <bsd.own.mk>

.if ${MKCRYPTO} == "no"
NAMED_USE_OPENSSL?=no
.endif

NAMED_USE_PTHREADS?=no

NAMED_USE_OPENSSL?=no

IDIST=		${NETBSDSRCDIR}/dist/bind
BIND_SRCDIR=	${NETBSDSRCDIR}/usr.sbin/bind
BIND_HTMLDIR=	/usr/share/doc/html/bind9

.include "${IDIST}/version"

VERSION=${MAJORVER}.${MINORVER}.${PATCHVER}${RELEASETYPE}${RELEASEVER}
SYSCONFDIR=/etc
LOCALSTATEDIR=/var

CPPFLAGS+=-I${BIND_SRCDIR}/include \
	-I${IDIST} \
	-I${IDIST}/lib/dns/include -I${IDIST}/lib/dns/unix/include \
	-I${IDIST}/lib/isc/include -I${IDIST}/lib/isc/unix/include \
	-I${IDIST}/lib/bind9/include \
	-I${IDIST}/lib/isccfg/include \
	-I${IDIST}/lib/isccc/include \
	-I${IDIST}/lib/lwres/include -I${IDIST}/lib/lwres/unix/include \
	-I${IDIST}/lib/dns/sec/dst/include \
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

.if !defined(LIB) || empty(LIB)
# NOTE: the order of these libraries is important...
LDADD+=		-lbind9 -ldns -llwres -lisccfg -lisccc -lisc
DPADD+=		${LIBBIND9} ${LIBDNS} ${LIBLWRES}
DPADD+=		${LIBISCCFG} ${LIBISCCC} ${LIBISC}
.else
CPPFLAGS+= -DLIBINTERFACE=${LIBINTERFACE} \
	   -DLIBREVISION=${LIBREVISION} -DLIBAGE=${LIBAGE}
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
