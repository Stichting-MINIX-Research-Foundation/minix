#	$NetBSD: Makefile,v 1.1 2012/06/17 18:48:47 joerg Exp $

LIB=		clang_asan
USE_SHLIBDIR=	yes
WARNS=		4
CWARNFLAGS+=	-Wno-missing-noreturn

.include <bsd.init.mk>

LDFLAGS+=	-nodefaultlibs

.PATH: ${COMPILER_RT_SRCDIR}/lib/asan

SRCS+=		asan_allocator.cc asan_globals.cc \
		asan_poisoning.cc asan_posix.cc asan_printf.cc \
		asan_rtl.cc asan_stack.cc asan_stats.cc \
		asan_thread.cc asan_thread_registry.cc

.PATH: ${COMPILER_RT_SRCDIR}/lib/sanitizer_common

SRCS+=		sanitizer_allocator.cc sanitizer_common.cc \
		sanitizer_libc.cc \
		sanitizer_posix.cc sanitizer_printf.cc sanitizer_symbolizer.cc

CPPFLAGS+=-I${COMPILER_RT_SRCDIR}/lib
CPPFLAGS+=-I${COMPILER_RT_SRCDIR}/lib/sanitizer_common

.include <bsd.lib.mk>
