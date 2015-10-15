
.include <bsd.own.mk>

.include "../Makefile.inc"

ASAN=${GCCDIST}/libsanitizer
.PATH: ${ASAN}/asan ${ASAN}/interception ${ASAN}/sanitizer_common

ASAN_SRCS= \
	asan_allocator.cc \
	asan_allocator2.cc \
	asan_interceptors.cc \
	asan_posix.cc \
	asan_new_delete.cc \
	asan_rtl.cc \
	asan_stats.cc \
	asan_globals.cc \
	asan_thread_registry.cc \
	asan_fake_stack.cc \
	asan_linux.cc \
	asan_report.cc \
	asan_malloc_linux.cc \
	asan_poisoning.cc \
	asan_stack.cc \
	asan_thread.cc \
	asan_preinit.cc

INTERCEPTION_SRCS= \
	interception_linux.cc \
	interception_type_test.cc

SANITIZER_SRCS= \
	sanitizer_allocator.cc \
	sanitizer_common.cc \
	sanitizer_flags.cc \
	sanitizer_libc.cc \
	sanitizer_netbsd.cc \
	sanitizer_mac.cc \
	sanitizer_posix.cc \
	sanitizer_platform_limits_posix.cc \
	sanitizer_printf.cc \
	sanitizer_stackdepot.cc \
	sanitizer_stacktrace.cc \
	sanitizer_symbolizer.cc \
	sanitizer_symbolizer_linux.cc \
	sanitizer_win.cc

# The linux build does this to avoid preinit sections on shared libraries
CSHLIBFLAGS+= -DPIC

LIB=	asan
SRCS=	${ASAN_SRCS} ${INTERCEPTION_SRCS} ${SANITIZER_SRCS}
CPPFLAGS+=-I${ASAN}/include -I${ASAN}
CPPFLAGS.sanitizer_netbsd.cc+=-I${GCCDIST}/gcc/ginclude

LIBDPLIBS+=    stdc++ ${.CURDIR}/../libstdc++-v3
LIBDPLIBS+=    pthread ${.CURDIR}/../../../../../lib/libpthread

.if ${MACHINE_ARCH} == "vax"
COPTS.asan_allocator2.cc += -O1
COPTS.sanitizer_mac.cc += -O1
COPTS.sanitizer_netbsd.cc += -O1
COPTS.sanitizer_printf.cc += -O1
COPTS.sanitizer_stackdepot.cc += -O1
.endif

.include <bsd.lib.mk>
