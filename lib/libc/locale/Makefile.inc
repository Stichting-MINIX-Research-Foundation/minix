#	from: @(#)Makefile.inc	5.1 (Berkeley) 2/18/91
#	$NetBSD: Makefile.inc,v 1.64 2013/08/18 20:03:48 joerg Exp $

# locale sources
.PATH: ${ARCHDIR}/locale ${.CURDIR}/locale

SRCS+=	setlocale.c __mb_cur_max.c \
	duplocale.c global_locale.c fix_grouping.c \
	freelocale.c localeconv.c newlocale.c nl_langinfo.c \
	generic_lc_all.c dummy_lc_collate.c \
	wcstol.c wcstoll.c wcstoimax.c wcstoul.c wcstoull.c wcstoumax.c \
	wcstod.c wcstof.c wcstold.c wcscoll.c wcsxfrm.c wcsftime.c

# citrus multibyte locale support
# we have quirk for libc.a - see the last part of lib/libc/Makefile
CPPFLAGS+=	-DWITH_RUNE -I${.CURDIR}
SRCS+=	_wctrans.c _wctype.c rune.c runetable.c \
	multibyte_c90.c multibyte_amd1.c iswctype_mb.c
CPPFLAGS.rune.c+=		-I${LIBCDIR}/citrus
CPPFLAGS.runetable.c+=		-I${LIBCDIR}/citrus
CPPFLAGS.multibyte_c90.c+=	-I${LIBCDIR}/citrus
CPPFLAGS.multibyte_amd1.c+=	-I${LIBCDIR}/citrus

MAN+=	setlocale.3 nl_langinfo.3

MAN+=	mbtowc.3 mbstowcs.3 wctomb.3 wcstombs.3 mblen.3 \

MAN+=	btowc.3 mbrtowc.3 mbsrtowcs.3 \
	wctob.3 wcrtomb.3 wcsrtombs.3 \
	mbrlen.3 mbsinit.3

MAN+=	iswalnum.3 wctype.3 iswctype.3 \
	towlower.3 wctrans.3 towctrans.3 \
	wcwidth.3

MAN+=	wcstol.3 wcstod.3 \
	wcscoll.3 wcsxfrm.3 \
	wcsftime.3

MLINKS+=setlocale.3 localeconv.3

MLINKS+=iswalnum.3 iswalpha.3 iswalnum.3 iswblank.3 \
	iswalnum.3 iswcntrl.3 iswalnum.3 iswdigit.3 \
	iswalnum.3 iswgraph.3 iswalnum.3 iswlower.3 \
	iswalnum.3 iswprint.3 iswalnum.3 iswpunct.3 \
	iswalnum.3 iswspace.3 iswalnum.3 iswupper.3 \
	iswalnum.3 iswxdigit.3

MLINKS+=towlower.3 towupper.3

MLINKS+=wcstod.3 wcstof.3 wcstod.3 wcstold.3
MLINKS+=wcstol.3 wcstoll.3 wcstol.3 wcstoimax.3 \
	wcstol.3 wcstoul.3 wcstol.3 wcstoull.3 wcstol.3 wcstoumax.3

COPTS.wcsftime.c = -Wno-format-nonliteral
