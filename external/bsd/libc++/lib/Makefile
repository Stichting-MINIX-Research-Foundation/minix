#	$NetBSD: Makefile,v 1.4 2013/11/12 16:39:39 joerg Exp $

LIB=		c++
WARNS=		4
NOLINT=		# defined

.include <bsd.init.mk>

.PATH: ${LIBCXX_SRCDIR}/src

SRCS=	algorithm.cpp bind.cpp chrono.cpp condition_variable.cpp debug.cpp \
	exception.cpp future.cpp hash.cpp ios.cpp iostream.cpp locale.cpp \
	memory.cpp mutex.cpp new.cpp optional.cpp random.cpp regex.cpp \
	shared_mutex.cpp stdexcept.cpp \
	string.cpp strstream.cpp system_error.cpp thread.cpp \
	utility.cpp valarray.cpp
# typeinfo.cc: prefer libcxxrt's version

LIBCXXRT_SRCS+= \
	auxhelper.cc dynamic_cast.cc exception.cc guard.cc \
	stdexcept.cc terminate.cc typeinfo.cc libelftc_dem_gnu3.c
# cxa_atexit.c: Solaris-only
# memory.cc: already provided by libc++'s new.cpp

.for src in ${LIBCXXRT_SRCS}
rt_${src}: ${LIBCXXRT_SRCDIR}/src/${src}
	cp ${LIBCXXRT_SRCDIR}/src/${src} rt_${src}

SRCS+=		rt_${src}
DPSRCS+=	rt_${src}
CLEANFILES+=	rt_${src}
.endfor

.include <bsd.own.mk>

CPPFLAGS+=	-nostdinc++ -cxx-isystem ${LIBCXX_SRCDIR}/include -I${LIBCXXRT_SRCDIR}/src
CPPFLAGS+=	-DLIBCXXRT
CXXFLAGS+=	${${ACTIVE_CC} == "clang":? -std=c++11 : -std=c++0x}

CWARNFLAGS.gcc+=	-Wno-error

CWARNFLAGS.clang+=	-Wno-error=unused-variable -Wno-error=missing-noreturn
CWARNFLAGS.clang+=	-Wno-error=missing-prototypes -Wno-error=sometimes-uninitialized
CWARNFLAGS.clang+=	-Wno-error=missing-field-initializers -Wno-error=switch
CWARNFLAGS.clang+=	-Wno-error=implicit-exception-spec-mismatch

LDADD+=	-Wl,-z,defs

.include <bsd.lib.mk>
