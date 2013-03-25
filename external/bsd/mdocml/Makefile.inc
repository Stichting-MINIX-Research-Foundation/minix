# $NetBSD: Makefile.inc,v 1.15 2012/02/16 22:56:12 joerg Exp $

.include <bsd.own.mk>

VERSION!=	cd ${.PARSEDIR}/dist && ${MAKE} -V VERSION

CPPFLAGS+=	-DVERSION=\"${VERSION}\" -DUGLY
CPPFLAGS+=	-UOSNAME -DOSNAME=\"Minix\"

.if (${HOSTPROG:U} == "")
# Minix mmap not yet up to the task
CPPFLAGS+=	-DHAVE_STRLCAT -DHAVE_STRLCPY -DHAVE_STRPTIME #-DHAVE_MMAP
.endif

DISTDIR:=	${.PARSEDIR}/dist

.PATH:	${DISTDIR}

.for _LIB in mandoc
MDOCMLOBJDIR.${_LIB} !=	cd ${.PARSEDIR}/lib/lib${_LIB} && ${PRINTOBJDIR}
MDOCMLLIB.${_LIB}=	${MDOCMLOBJDIR.${_LIB}}/lib${_LIB}.a
.endfor

WARNS?=	4
