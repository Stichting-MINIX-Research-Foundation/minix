#	$NetBSD: bsd.gcc.mk,v 1.4 2012/07/19 19:42:45 christos Exp $

.if !defined(_BSD_GCC_MK_)
_BSD_GCC_MK_=1

.if defined(EXTERNAL_TOOLCHAIN)
_GCC_CRTBEGIN!=		${CC} --print-file-name=crtbegin.o
_GCC_CRTBEGINS!=	${CC} --print-file-name=crtbeginS.o
_GCC_CRTEND!=		${CC} --print-file-name=crtend.o
_GCC_CRTENDS!=		${CC} --print-file-name=crtendS.o
_GCC_CRTI!=		${CC} --print-file-name=crti.o
_GCC_CRTN!=		${CC} --print-file-name=crtn.o
_GCC_CRTDIR!=		dirname ${_GCC_CRTBEGIN}
_GCC_LIBGCCDIR!=	dirname `${CC} --print-libgcc-file-name`
.else
_GCC_CRTBEGIN?=		${DESTDIR}/usr/lib/crtbegin.o
_GCC_CRTBEGINS?=	${DESTDIR}/usr/lib/crtbeginS.o
_GCC_CRTEND?=		${DESTDIR}/usr/lib/crtend.o
_GCC_CRTENDS?=		${DESTDIR}/usr/lib/crtendS.o
_GCC_CRTI?=		${DESTDIR}/usr/lib/crti.o
_GCC_CRTN?=		${DESTDIR}/usr/lib/crtn.o
_GCC_CRTDIR?=		${DESTDIR}/usr/lib
_GCC_LIBGCCDIR?=	${DESTDIR}/usr/lib
.endif

.endif	# ! defined(_BSD_GCC_MK_)
