# $NetBSD: Makefile.inc,v 1.12 2010/07/25 19:16:18 joerg Exp $

.include <bsd.own.mk>

VERSION!=	cd ${.PARSEDIR}/dist && ${MAKE} -V VERSION

CPPFLAGS+=	-DVERSION=\"${VERSION}\" -DUGLY
CPPFLAGS+=	-UOSNAME -DOSNAME=\"Minix\"

.if (${HOSTPROG:U} == "")
CPPFLAGS+=	-DHAVE_STRLCAT -DHAVE_STRLCPY -DHAVE_STRPTIME -DHAVE_MMAP
.endif

DISTDIR:=	${.PARSEDIR}/dist

.PATH:	${DISTDIR}

.for _LIB in man mdoc roff
MDOCMLOBJDIR.${_LIB} !=	cd ${.PARSEDIR}/lib/lib${_LIB} && ${PRINTOBJDIR}
MDOCMLLIB.${_LIB}=	${MDOCMLOBJDIR.${_LIB}}/lib${_LIB}.a
.endfor

WARNS?=	4
