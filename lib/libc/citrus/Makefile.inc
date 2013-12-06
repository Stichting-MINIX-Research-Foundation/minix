#	$NetBSD: Makefile.inc,v 1.8 2010/05/30 08:28:53 tnozaki Exp $

# sources
.PATH: ${ARCHDIR}/citrus ${.CURDIR}/citrus

.if !defined(__MINIX)
MODULE_SHLIB_MAJOR!= . ${NETBSDSRCDIR}/lib/i18n_module/shlib_version ; echo $$major
.else
MODULE_SHLIB_MAJOR=1
.endif # !defined(__MINIX)
CPPFLAGS.citrus_module.c+=-DI18NMODULE_MAJOR=${MODULE_SHLIB_MAJOR}
SRCS+=	citrus_bcs.c citrus_bcs_strtol.c citrus_bcs_strtoul.c \
	citrus_csmapper.c citrus_ctype.c citrus_ctype_fallback.c \
	citrus_db.c citrus_db_hash.c citrus_esdb.c citrus_hash.c \
	citrus_iconv.c citrus_lookup.c \
	citrus_mapper.c citrus_memstream.c citrus_mmap.c citrus_module.c \
	citrus_none.c citrus_stdenc.c
SRCS+=	citrus_lc_ctype.c \
	citrus_lc_monetary.c \
	citrus_lc_numeric.c \
	citrus_lc_time.c \
	citrus_lc_messages.c
CPPFLAGS.citrus_bcs_strtol.c+=	-I${LIBCDIR}/../../common/lib/libc/stdlib
CPPFLAGS.citrus_bcs_strtoul.c+=	-I${LIBCDIR}/../../common/lib/libc/stdlib
CPPFLAGS.citrus_lc_ctype.c+=	-I${LIBCDIR}/locale
CPPFLAGS.citrus_lc_monetary.c+=	-I${LIBCDIR}/locale
CPPFLAGS.citrus_lc_numeric.c+=	-I${LIBCDIR}/locale
CPPFLAGS.citrus_lc_time.c+=	-I${LIBCDIR}/locale
CPPFLAGS.citrus_lc_messages.c+=	-I${LIBCDIR}/locale
