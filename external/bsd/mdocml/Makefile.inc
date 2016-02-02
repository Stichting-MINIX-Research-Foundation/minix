# $NetBSD: Makefile.inc,v 1.16 2014/01/05 19:49:57 joerg Exp $

.include <bsd.own.mk>


.if (${HOSTPROG:U} == "")
# Minix: #-DHAVE_MMAP
CPPFLAGS+=	-DHAVE_FGETLN -DHAVE_GETSUBOPT \
		-DHAVE_STRCASESTR -DHAVE_STRLCAT -DHAVE_STRLCPY \
		-DHAVE_STRNLEN -DHAVE_STRPTIME -DHAVE_STRSEP
VERSION!=	cd ${.PARSEDIR}/dist && ${MAKE} -V VERSION
CPPFLAGS+=	-DVERSION=\"${VERSION}\"

.if defined(__MINIX)
CPPFLAGS+=	-UOSNAME -DOSNAME=\"Minix\"
.endif # defined(__MINIX)
.endif

DISTDIR:=	${.PARSEDIR}/dist

.PATH:	${DISTDIR}

.for _LIB in mandoc
MDOCMLOBJDIR.${_LIB} !=	cd ${.PARSEDIR}/lib/lib${_LIB} && ${PRINTOBJDIR}
MDOCMLLIB.${_LIB}=	${MDOCMLOBJDIR.${_LIB}}/lib${_LIB}.a
.endfor

WARNS?=	4
