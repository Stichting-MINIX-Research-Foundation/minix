#	$NetBSD: bsd.gcc.mk,v 1.11 2015/06/07 15:04:28 matt Exp $

.if !defined(_BSD_GCC_MK_)
_BSD_GCC_MK_=1

.if defined(EXTERNAL_TOOLCHAIN)
_GCC_CRTBEGIN!=		${CC} --print-file-name=crtbegin.o
.ifndef _GCC_CRTBEGINS
_GCC_CRTBEGINS!=	${CC} --print-file-name=crtbeginS.o
.endif
_GCC_CRTEND!=		${CC} --print-file-name=crtend.o
.ifndef _GCC_CRTENDS
_GCC_CRTENDS!=		${CC} --print-file-name=crtendS.o
.endif
.ifndef _GCC_CRTI
_GCC_CRTI!=		${CC} --print-file-name=crti.o
.endif
.ifndef _GCC_CRTN
_GCC_CRTN!=		${CC} --print-file-name=crtn.o
.endif
_GCC_CRTDIR!=		dirname ${_GCC_CRTBEGIN}
_GCC_LIBGCCDIR!=	dirname `${CC} --print-libgcc-file-name`
.else
_GCC_CRTBEGIN?=		${DESTDIR}/usr/lib/${MLIBDIR:D${MLIBDIR}/}crtbegin.o
_GCC_CRTBEGINS?=	${DESTDIR}/usr/lib/${MLIBDIR:D${MLIBDIR}/}crtbeginS.o
_GCC_CRTEND?=		${DESTDIR}/usr/lib/${MLIBDIR:D${MLIBDIR}/}crtend.o
_GCC_CRTENDS?=		${DESTDIR}/usr/lib/${MLIBDIR:D${MLIBDIR}/}crtendS.o
_GCC_CRTI?=		${DESTDIR}/usr/lib/${MLIBDIR:D${MLIBDIR}/}crti.o
_GCC_CRTN?=		${DESTDIR}/usr/lib/${MLIBDIR:D${MLIBDIR}/}crtn.o
_GCC_CRTDIR?=		${DESTDIR}/usr/lib${MLIBDIR:D/${MLIBDIR}}
_GCC_LIBGCCDIR?=	${DESTDIR}/usr/lib${MLIBDIR:D/${MLIBDIR}}
.endif

.endif	# ! defined(_BSD_GCC_MK_)
