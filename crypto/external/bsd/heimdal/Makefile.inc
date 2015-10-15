# $NetBSD: Makefile.inc,v 1.3 2014/03/04 21:08:08 joerg Exp $

HEIMBASE?=	${NETBSDSRCDIR}/crypto/external/bsd/heimdal
HEIMDIST=	${HEIMBASE}/dist

SSLBASE?=	${NETBSDSRCDIR}/crypto/external/bsd/openssl

LIBVERSDIR!=	cd ${HEIMBASE}/lib/libvers && ${PRINTOBJDIR}
LIBVERS=	${LIBVERSDIR}/libvers.a

LIBIPCDIR!=	cd ${HEIMBASE}/lib/libipc && ${PRINTOBJDIR}
LIBIPC=		${LIBIPCDIR}/libipc.a
.if ${MKPICLIB} != "yes"
LIBIPC_PIC=	${LIBIPCDIR}/libipc.a
.else
LIBIPC_PIC=	${LIBIPCDIR}/libipc_pic.a
.endif

.if ${USETOOLS} != "yes"
COMPILEETOBJ!=     cd ${HEIMBASE}/lib/libcom_err/compile_et && ${PRINTOBJDIR}
TOOL_COMPILE_ET=   ${COMPILEETOBJ}/compile_et

ASN1COMPILEOBJ!=   cd ${HEIMBASE}/lib/libasn1/asn1_compile && ${PRINTOBJDIR}
TOOL_ASN1_COMPILE= ${ASN1COMPILEOBJ}/asn1_compile

SLCOBJ!=	cd ${HEIMBASE}/lib/libsl/slc && ${PRINTOBJDIR}
TOOL_SLC=	${SLCOBJ}/slc
.endif

#
# We now define what are standard CPP flags across Heimdal:

.if (${USE_INET6} != "no")
CPPFLAGS+=-DHAVE_IPV6
HOST_CPPFLAGS+=-DHAVE_IPV6
.endif

.if ${MKPIC} == "no"
CPPFLAGS+=	-DNO_DLOPEN
.endif

CPPFLAGS+=	-DHAVE_CONFIG_H
HOST_CPPFLAGS+=	-DHAVE_CONFIG_H

CPPFLAGS+= ${.PATH:S/^/-I/}
CPPFLAGS+= -I${HEIMBASE}/include
CPPFLAGS+= -I${HEIMDIST}/include
CWARNFLAGS.clang+=	-Wno-error=absolute-value
