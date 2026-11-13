#	$NetBSD: Makefile.inc,v 1.7 2005/06/04 16:17:17 lukem Exp $

SRCS+=		rtld_start.S mdreloc.c

# XXX Should not be in CPPFLAGS!
CPPFLAGS+=	-fPIC

CPPFLAGS+=	-DELFSIZE=64
# Process DT_INIT_ARRAY/DT_FINI_ARRAY for shared objects. Without this,
# rtld.c only calls the legacy DT_INIT/_init and never walks .init_array, so
# C++ static constructors in shared libraries (which the toolchain now emits
# into .init_array) silently never run -- breaking exception unwinding
# (libunwind's findPCRange stays NULL), libmthread TSD init, etc.
CPPFLAGS+=	-DHAVE_INITFINI_ARRAY

LDFLAGS+=	-Wl,-e,.rtld_start
