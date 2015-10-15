#	$NetBSD: Makefile,v 1.20 2015/01/29 21:12:10 joerg Exp $

REQUIRETOOLS=	yes
NOLINT=		# defined

.include <bsd.init.mk>

.include "Makefile.inc"

LIB=		stdc++

LIBDPLIBS+=	m ${.CURDIR}/../../../../../lib/libm

#COPTS+=		-pthread
#LDFLAGS+=	-pthread
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
COPTS.random.cc+=	${${ACTIVE_CC} == "clang":? -mrdrnd :}
.endif

CXXFLAGS.clang+=	-stdlib=libstdc++ -std=c++11 -D_GLIBCXX_ABI_TAG_CXX11=
CXXFLAGS+=	${CXXFLAGS.${ACTIVE_CC}}
CWARNFLAGS.clang+=	-Wno-logical-op-parentheses \
			-Wno-deprecated-writable-strings -Wno-parentheses

.include "${.CURDIR}/../libsupc++/Makefile.common"

.if exists(${.CURDIR}/arch/${LIBSTDCXX_MACHINE_ARCH}/defs.mk) && ${MKGCC} != "no"
.include "${.CURDIR}/arch/${LIBSTDCXX_MACHINE_ARCH}/defs.mk"

.if defined(__MINIX)
# by itself, libstdc++ think it is version 6, so use the same number here
# as this is the exact same code, and will allow us to provide this library
# from the base system (here), or through the gcc48 package.
SHLIB_MAJOR=	6
.else
SHLIB_MAJOR=	7
.endif # defined(__MINIX)
SHLIB_MINOR=	3

SUBDIR=		include

SRCS=		${LIBSUPCXXSRCS} ${LIBSTDCXXSRCS}
# affects profiling; can't switch it off just for profiling easily.
#CXXFLAGS+=	${G_SECTION_FLAGS}
#CXXFLAGS+=	-ffunction-sections -fdata-sections 
CXXFLAGS+=	-fno-implicit-templates -fdiagnostics-show-location=once
CPPFLAGS+=	-I${DIST}/libstdc++-v3/include
CPPFLAGS+=	-I${.CURDIR}/arch/${LIBSTDCXX_MACHINE_ARCH}
# include backwards compatible code
CPPFLAGS+=     -D_GLIBCXX_SHARED -DGTHREAD_USE_WEAK -DSUPPORTS_WEAK

CONF=		${DIST}/libstdc++-v3/config

GLIBCPP_INCLUDE_DIR=	${DESTDIR}/usr/include/g++

# Template instantiation bug workaround for -O<2/s
COPTS.cp-demangle.c = -Wno-stack-protector
COPTS.cp-demangle.c = -Wno-stack-protector
COPTS.fstream-inst.cc = -Wno-stack-protector
COPTS.locale-inst.cc = -Wno-stack-protector
COPTS.misc-inst.cc = -Wno-stack-protector
COPTS.ostream-inst.cc = -Wno-stack-protector
COPTS.valarray-inst.cc = -Wno-stack-protector
COPTS.wlocale-inst.cc = -Wno-stack-protector
COPTS.concept-inst.cc=	-fimplicit-templates
COPTS.hashtable_c++0x.cc=  -fimplicit-templates
COPTS.bitmap_allocator.cc = -fimplicit-templates
CPPFLAGS.strstream.cc=	-I$(GLIBCPP_INCLUDE_DIR)/backward -Wno-deprecated

# XXX also need this for some PCHs.
CPPFLAGS.atomic.cc = -std=gnu++0x
CPPFLAGS.compatibility-c++0x.cc = -std=gnu++0x
CPPFLAGS.functexcept.cc = -std=gnu++0x
CPPFLAGS.hash_c++0x.cc = -std=gnu++0x
CPPFLAGS.hashtable_c++0x.cc = -std=gnu++0x
CPPFLAGS.limits.cc = -std=gnu++0x
CPPFLAGS.system_error.cc = -std=gnu++0x
CPPFLAGS.fstream-inst.cc = -std=gnu++0x
CPPFLAGS.string-inst.cc = -std=gnu++0x
CPPFLAGS.wstring-inst.cc = -std=gnu++0x
CPPFLAGS.mutex.cc = -std=gnu++0x
CPPFLAGS.condition_variable.cc = -std=gnu++0x
CPPFLAGS.chrono.cc = -std=gnu++0x
CPPFLAGS.thread.cc = -std=gnu++0x
CPPFLAGS.future.cc = -std=gnu++0x

CPPFLAGS.cp-demangle.cc += -DIN_GLIBCPP_V3
CPPFLAGS.concept-inst.cc += -D_GLIBCXX_CONCEPT_CHECKS
CPPFLAGS.parallel_list.cc += -D_GLIBCXX_PARALLEL
CPPFLAGS.parallel_settings.cc += -D_GLIBCXX_PARALLEL
CPPFLAGS.compatibility-parallel_list.cc += -D_GLIBCXX_PARALLEL

cp-demangle.d cp-demangle.o: cp-demangle.h

# XXX
MKDEPFLAGS+=	-I$(GLIBCPP_INCLUDE_DIR)/backward

GLIBCXX_SRCDIR=	${DIST}/libstdc++-v3

BUILDSYMLINKS+=	\
	${GLIBCXX_SRCDIR}/${G_CCODECVT_CC} codecvt_members.cc \
	${GLIBCXX_SRCDIR}/${G_CCOLLATE_CC} collate_members.cc \
	${GLIBCXX_SRCDIR}/${G_CCTYPE_CC} ctype_members.cc \
	${GLIBCXX_SRCDIR}/${G_CMESSAGES_CC} messages_members.cc \
	${GLIBCXX_SRCDIR}/${G_CMONEY_CC} monetary_members.cc \
	${GLIBCXX_SRCDIR}/${G_CNUMERIC_CC} numeric_members.cc \
	${GLIBCXX_SRCDIR}/${G_CTIME_CC} time_members.cc \
	${GLIBCXX_SRCDIR}/${G_ATOMICITY_SRCDIR}/atomicity.h atomicity.cc \
	${GLIBCXX_SRCDIR}/${G_CLOCALE_CC} c++locale.cc \
	${GLIBCXX_SRCDIR}/${G_BASIC_FILE_CC} basic_file.cc \
	${DIST}/libiberty/cp-demangle.c cp-demangle.c \
	${DIST}/libiberty/cp-demangle.h cp-demangle.h

.if ${HAVE_LIBGCC_EH} == "no"
G_CONFIGLINKS:= ${G_CONFIGLINKS:S,libgcc/config/arm/unwind-arm.h,libgcc/unwind-generic.h,}
.endif

.if ${MKGCC} != "no"
.if defined(G_CONFIGLINKS) && !empty(G_CONFIGLINKS)
BUILDSYMLINKS+=        ${G_CONFIGLINKS}
.for _src _dst in ${G_CONFIGLINKS}
DPSRCS+=       ${_dst}
.endfor
.endif
.endif

.for f in compatibility-c++0x compatibility-atomic-c++0x \
          compatibility-thread-c++0x compatibility-chrono chrono \
	  condition_variable debug functexcept functional future \
	  hash_c++0x hashtable_c++0x limits mutex placeholders \
	  random regex shared_ptr system_error thread
COPTS.${f}.cc+= -std=gnu++11
.endfor

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
.include <bsd.info.mk>

.PATH:	${DIST}/libstdc++-v3/src \
	${DIST}/libstdc++-v3/src/c++98 \
	${DIST}/libstdc++-v3/src/c++11 \
	${DIST}/libstdc++-v3/libsupc++ \
	${DIST}/libstdc++-v3/config/os/bsd/netbsd \
	${CONF}/locale/generic \
	${.CURDIR}/arch/${LIBSTDCXX_MACHINE_ARCH}

${OBJS}: ${.CURDIR}/arch/${LIBSTDCXX_MACHINE_ARCH}/defs.mk
.else
.include <bsd.prog.mk> # do nothing
.endif
