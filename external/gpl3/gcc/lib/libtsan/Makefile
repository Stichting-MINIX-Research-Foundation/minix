# $NetBSD: Makefile,v 1.2 2015/01/07 03:49:13 christos Exp $

.include <bsd.own.mk>

.include "../Makefile.inc"

TSAN=${GCCDIST}/libsanitizer
.PATH: ${TSAN}/tsan ${TSAN}/interception ${TSAN}/sanitizer_common

TSAN_SRCS= \
	tsan_clock.cc  \
	tsan_fd.cc  \
	tsan_flags.cc  \
	tsan_interceptors.cc  \
	tsan_interface.cc  \
	tsan_interface_ann.cc  \
	tsan_interface_atomic.cc  \
	tsan_interface_java.cc  \
	tsan_md5.cc  \
	tsan_mman.cc  \
	tsan_mutex.cc  \
	tsan_mutexset.cc  \
	tsan_platform_linux.cc  \
	tsan_platform_mac.cc  \
	tsan_report.cc  \
	tsan_rtl.cc  \
	tsan_rtl_amd64.S  \
	tsan_rtl_mutex.cc  \
	tsan_rtl_report.cc  \
	tsan_rtl_thread.cc  \
	tsan_stat.cc  \
	tsan_suppressions.cc  \
	tsan_symbolize.cc  \
	tsan_symbolize_addr2line_linux.cc  \
	tsan_sync.cc

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

LIB=	tsan
SRCS=	${TSAN_SRCS} ${INTERCEPTION_SRCS} ${SANITIZER_SRCS}
CPPFLAGS+=-I${TSAN}/include -I${TSAN}

LDADD+=	-lstdc++ -lpthread
DPADD+= ${LIBSTDCXX} ${LIBPTHREAD}

.include <bsd.lib.mk>
