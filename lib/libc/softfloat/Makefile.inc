#	$NetBSD: Makefile.inc,v 1.10 2011/07/04 02:53:15 mrg Exp $

SOFTFLOAT_BITS?=64
.PATH:		${ARCHDIR}/softfloat \
		${.CURDIR}/softfloat/bits${SOFTFLOAT_BITS} ${.CURDIR}/softfloat

CPPFLAGS+=	-I${ARCHDIR}/softfloat -I${.CURDIR}/softfloat
CPPFLAGS+=	-DSOFTFLOAT_FOR_GCC

SRCS.softfloat= softfloat.c

SRCS.softfloat+=fpgetround.c fpsetround.c fpgetmask.c fpsetmask.c \
		fpgetsticky.c fpsetsticky.c

SRCS.softfloat+=eqsf2.c nesf2.c gtsf2.c gesf2.c ltsf2.c lesf2.c negsf2.c \
		eqdf2.c nedf2.c gtdf2.c gedf2.c ltdf2.c ledf2.c negdf2.c \
		eqtf2.c netf2.c gttf2.c getf2.c lttf2.c letf2.c negtf2.c \
		nexf2.c gtxf2.c gexf2.c negxf2.c unordsf2.c unorddf2.c

SRCS+=		${SRCS.softfloat}

# XXX
.if defined(HAVE_GCC) && ${HAVE_GCC} >= 45 && \
    (${MACHINE_CPU} == "arm" || \
     ${MACHINE_CPU} == "mips" || \
     ${MACHINE_CPU} == "sh3")
COPTS.softfloat.c+=	-Wno-enum-compare
.endif
