#	$NetBSD: Makefile,v 1.8 2014/08/28 12:23:29 joerg Exp $

.include <bsd.own.mk>

CPPFLAGS+=	-DELFSIZE=64
LIB=	ldd_elf64

# XXX Force one member
SRCS=	dummy.c

LIBISPRIVATE=	yes
.PATH: ${.CURDIR}/..

.if ${OBJECT_FMTS:Melf64} != ""

# XXX we need to make sure that we don't accidentally get the elf32
# XXX versions of these.

RTLD_FUNCS	= \
	_rtld_expand_path \
	_rtld_digest_dynamic \
	_rtld_digest_phdr \
	_rtld_load_needed_objects \
	_rtld_load_object \
	_rtld_map_object \
	_rtld_obj_free \
	_rtld_obj_new \
	_rtld_object_add_name \
	_rtld_object_match_name \
	_rtld_add_paths \
	_rtld_process_hints \
	_rtld_sysctl \
	_rtld_tls_allocate \
	_rtld_tls_free \
	_rtld_load_library

.for _d in ${RTLD_FUNCS}
CPPFLAGS+=	-D${_d}=_elf64_${_d}
.endfor

.if (${MACHINE_ARCH} == "mips64el") || (${MACHINE_ARCH} == "mips64eb")
CPPFLAGS+=	-DRTLD_ARCH_SUBDIR=\"${MLIBDIR}\"
.endif

.include "../Makefile.elf"

.endif

.include <bsd.lib.mk>
