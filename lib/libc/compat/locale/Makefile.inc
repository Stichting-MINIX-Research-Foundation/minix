# $NetBSD: Makefile.inc,v 1.1 2010/06/07 13:52:30 tnozaki Exp $

.PATH: ${COMPATDIR}/arch/${MACHINE_ARCH}/locale ${COMPATDIR}/locale

CPPFLAGS+=	-I${COMPATDIR}/../locale
SRCS+=		compat_setlocale1.c compat_setlocale32.c
