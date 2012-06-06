#	$NetBSD: Makefile,v 1.5 2011/08/09 13:00:04 joerg Exp $

REQUIRETOOLS=	yes
NOLINT=		# defined

.include <bsd.own.mk>

LIB=		stdc++
LDADD=		-lgcc_s

LIBDPLIBS+=	m ${.CURDIR}/../../../../../lib/libm

UNSUPPORTED_COMPILER.clang=	# defined
CWARNFLAGS.clang+=	-Wno-logical-op-parentheses \
			-Wno-deprecated-writable-strings -Wno-parentheses

.if exists(${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk) && ${MKGCC} != "no"
.include "${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk"

SHLIB_MAJOR=	7
SHLIB_MINOR=	1

SUBDIR=		include

.include "${.CURDIR}/../libsupc++/Makefile.common"

SRCS=		${LIBSUPCXXSRCS} ${LIBSTDCXXSRCS}
# affects profiling; can't switch it off just for profiling easily.
#CXXFLAGS+=	${G_SECTION_FLAGS}
#CXXFLAGS+=	-ffunction-sections -fdata-sections 
CXXFLAGS+=	-fno-implicit-templates -fdiagnostics-show-location=once
CPPFLAGS+=	-I${DIST}/libstdc++-v3/include
CPPFLAGS+=	-I${.CURDIR}/arch/${MACHINE_ARCH}

CONF=		${DIST}/libstdc++-v3/config

#BUILDSYMLINKS+=	${CONF}/io/basic_file_stdio.cc		basic_file.cc \
#		${CONF}/locale/generic/c_locale.cc	c++locale.cc

GLIBCPP_INCLUDE_DIR=	${DESTDIR}/usr/include/g++

COPTS.cp-demangle.c = -Wno-stack-protector
COPTS.fstream-inst.cc = -Wno-stack-protector
COPTS.locale-inst.cc = -Wno-stack-protector
COPTS.misc-inst.cc = -Wno-stack-protector
COPTS.ostream-inst.cc = -Wno-stack-protector
COPTS.valarray-inst.cc = -Wno-stack-protector
COPTS.wlocale-inst.cc = -Wno-stack-protector
COPTS.concept-inst.cc=	-D_GLIBCPP_CONCEPT_CHECKS -fimplicit-templates
CPPFLAGS.strstream.cc=	-I$(GLIBCPP_INCLUDE_DIR)/backward -Wno-deprecated

# For GCC 4.5
.if ${HAVE_GCC} >= 45
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
.endif

# XXX
MKDEPFLAGS+=	-I$(GLIBCPP_INCLUDE_DIR)/backward

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
.include <bsd.info.mk>

GLIBCXX_SRCDIR=	${DIST}/libstdc++-v3

codecvt_members.cc: ${GLIBCXX_SRCDIR}/${G_CCODECVT_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_CCODECVT_CC} ${.TARGET}

collate_members.cc: ${GLIBCXX_SRCDIR}/${G_CCOLLATE_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_CCOLLATE_CC} ${.TARGET}

ctype_members.cc: ${GLIBCXX_SRCDIR}/${G_CCTYPE_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_CCTYPE_CC} ${.TARGET}

messages_members.cc: ${GLIBCXX_SRCDIR}/${G_CMESSAGES_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_CMESSAGES_CC} ${.TARGET}

monetary_members.cc: ${GLIBCXX_SRCDIR}/${G_CMONEY_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_CMONEY_CC} ${.TARGET}

numeric_members.cc: ${GLIBCXX_SRCDIR}/${G_CNUMERIC_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_CNUMERIC_CC} ${.TARGET}

time_members.cc: ${GLIBCXX_SRCDIR}/${G_CTIME_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_CTIME_CC} ${.TARGET}

atomicity.cc: ${G_atomicity_file}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${G_atomicity_file} ${.TARGET}

c++locale.cc: ${GLIBCXX_SRCDIR}/${G_CLOCALE_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_CLOCALE_CC} ${.TARGET}

basic_file.cc: ${GLIBCXX_SRCDIR}/${G_BASIC_FILE_CC}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${GLIBCXX_SRCDIR}/${G_BASIC_FILE_CC} ${.TARGET}


CLEANFILES+=	atomicity.cc basic_file.cc c++locale.cc codecvt_members.cc
CLEANFILES+=	collate_members.cc ctype_members.cc messages_members.cc
CLEANFILES+=	monetary_members.cc numeric_members.cc time_members.cc

${SRCS}: unwind.h
unwind.h: ${G_UNWIND_H}
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	ln -s ${G_UNWIND_H} ${.TARGET}
CLEANFILES+= unwind.h

.PATH:	${DIST}/libstdc++-v3/src \
	${DIST}/libstdc++-v3/libsupc++ \
	${DIST}/libiberty \
	${CONF}/locale/generic \
	${.CURDIR}/arch/${MACHINE_ARCH}

${OBJS}: ${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk
.else
.include <bsd.prog.mk> # do nothing
.endif
