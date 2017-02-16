# $NetBSD: Makefile.inc,v 1.15 2015/09/27 21:01:27 mlelstv Exp $

WARNS?=	1	# XXX -Wshadow -Wcast-qual -Wsign-compare

.include <bsd.own.mk>

USE_FORT?= yes	# network client/server
CWARNFLAGS.clang+=	-Wno-tautological-compare \
			-Wno-conversion -Wno-constant-logical-operand \
			-Wno-format-security -Wno-error=unused-const-variable

DIST:=	${.PARSEDIR}/dist
BIND:=	${.PARSEDIR}/../bind/dist
.PATH:	${DIST}/${DHCPSRCDIR}

COBJDIR!=cd ${.PARSEDIR}/lib/common && ${PRINTOBJDIR}
OMOBJDIR!=cd ${.PARSEDIR}/lib/omapip && ${PRINTOBJDIR}
DSTOBJDIR!=cd ${.PARSEDIR}/lib/dst && ${PRINTOBJDIR}

.if (${USE_INET6} != "no")
CPPFLAGS+=	-DDHCPv6
.endif
.if !defined(__MINIX)
CPPFLAGS+= -DISC_PLATFORM_USETHREADS -DHAVE_CONFIG_H
.else # defined(__MINIX)
CPPFLAGS+= -DHAVE_CONFIG_H
.endif # defined(__MINIX)
CPPFLAGS+= -I${DIST} -I${DIST}/includes -I${DIST}/../include
CPPFLAGS+= -I${BIND} -I${BIND}/includes -I${BIND}/../include
.for dir in isc dns irs
CPPFLAGS+= -I${BIND}/lib/${dir}/include
CPPFLAGS+= -I${BIND}/lib/${dir}/unix/include
.if !defined(__MINIX)
CPPFLAGS+= -I${BIND}/lib/${dir}/pthreads/include
.else # defined(__MINIX)
CPPFLAGS+= -I${BIND}/lib/${dir}/nothreads/include
.endif # defined(__MINIX)
.endfor
CPPFLAGS+= -DLOCALSTATEDIR='"/var"'
LDADD+= ${COBJDIR}/libdhcp.a
LDADD+= ${OMOBJDIR}/libomapi.a ${DSTOBJDIR}/libdst.a
.if defined(PROG) && ${PROG} == "dhclient"
LDADD+=-Wl,-Bstatic
.endif
LDADD+= -lirs -lisccfg -ldns -lisc
.if !defined(__MINIX)
LDADD+=-lpthread
.endif # !defined(__MINIX)
.if (${MKCRYPTO} != "no")
.if (${MKKERBEROS} != "no")
LDADD+= -lgssapi -lkrb5 -lhx509 -lheimntlm -lheimbase \
	-lcom_err  -lroken -lasn1 -lwind
DPADD+= ${LIBGSSAPI} ${LIBKRB5} ${LIBHX509} ${LIBHEIMNTLM} ${LIBHEIMBASE} \
	${LIBCOM_ERR} ${LIBROKEN} ${LIBASN1} ${LIBWIND}
.endif
# MINIX3 note: partial dynamic linking results in an unusable binary somehow
.if defined(PROG) && ${PROG} == "dhclient" && !defined(__MINIX)
LDADD+=-Wl,-Bdynamic
.endif
.if !defined(__MINIX)
LDADD+= -lcrypto -lipsec -lcrypt
DPADD+= ${LIBCRYPTO} ${LIBIPSEC} ${LIBCRYPT}
.else # defined(__MINIX)
LDADD+= -lcrypto -lcrypt
DPADD+= ${LIBCRYPTO} ${LIBCRYPT}
.endif # defined(__MINIX)
.endif
.if defined(PROG) && ${PROG} == "dhclient" && !defined(__MINIX)
LDADD+=-Wl,-Bdynamic
.endif
DPADD+= ${COBJDIR}/libdhcp.a
DPADD+= ${OMOBJDIR}/libomapi.a ${DSTOBJDIR}/libdst.a
DPADD+=	${LIBDNS} ${LIBISC}
.if !defined(__MINIX)
DPADD+= ${LIBPTHREAD}
.endif # !defined(__MINIX)
