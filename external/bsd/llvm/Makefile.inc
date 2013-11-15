#	$NetBSD: Makefile.inc,v 1.63 2013/11/28 14:19:59 joerg Exp $

.if !defined(LLVM_TOPLEVEL_MK)
LLVM_TOPLEVEL_MK=

.include <bsd.own.mk>

SVN_ROOT=		http://llvm.org/svn/llvm-project
SVN_BRANCH=		branches/release_34

COMMON_REVISION=	195771
CLANG_REVISION=		${COMMON_REVISION}
COMPILER_RT_REVISION=	${COMMON_REVISION}
LLD_REVISION=		${COMMON_REVISION}
LLDB_REVISION=		${COMMON_REVISION}
LLVM_REVISION=		${COMMON_REVISION}

MCLINKER_REVISION=	deeb2a77b4165827316f88e0a7ba4ba6b743a080
MCLINKER_ROOT=		https://code.google.com/p/mclinker/

LLVM_VERSION=		3.4
CLANG_VERSION=		3.4

COMPILER_RT_SRCDIR:=	${.PARSEDIR}/dist/compiler-rt
CLANG_SRCDIR:=	${.PARSEDIR}/dist/clang
LLD_SRCDIR:=	${.PARSEDIR}/dist/lld
LLDB_SRCDIR:=	${.PARSEDIR}/dist/lldb
LLVM_SRCDIR:=	${.PARSEDIR}/dist/llvm
MCLINKER_SRCDIR:=	${.PARSEDIR}/dist/mclinker
LLVM_TOPLEVEL:=	${.PARSEDIR}

CPPFLAGS+=	-I. -I${CLANG_SRCDIR}/include -I${LLD_SRCDIR}/include \
		-I${LLVM_SRCDIR}/include -I${MCLINKER_SRCDIR}/include \
		-I${LLDB_SRCDIR}/include \
		-D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS \
		-D__STDC_FORMAT_MACROS -DCLANG_ENABLE_STATIC_ANALYZER

.if defined(HOSTLIB) || defined(HOSTPROG)
LLVM_INCLUDE_CONFIG=	${LLVM_TOOLCONF_OBJDIR}/config/include
CLANG_INCLUDE_CONFIG=	${LLVM_TOOLCONF_OBJDIR}/config/tools/clang/include
MCLINKER_INCLUDE_CONFIG= ${LLVM_TOPLEVEL}/config/mclinker
LLVM_INCLUDE_OBJDIR!=	cd ${NETBSDSRCDIR}/tools/llvm-include && ${PRINTOBJDIR}
LLVM_TOOLCONF_OBJDIR!=	cd ${NETBSDSRCDIR}/tools/llvm && ${PRINTOBJDIR}
HOST_CPPFLAGS+=	${CPPFLAGS}
HOST_CXXFLAGS+=	-O2 -g -fno-rtti -fno-exceptions
.if ${MKLLD:Uno} == "yes"
HOST_CPPFLAGS+=	-std=c++11
.endif
.else
LLVM_INCLUDE_CONFIG=	${LLVM_TOPLEVEL}/config
CLANG_INCLUDE_CONFIG=	${LLVM_TOPLEVEL}/config
MCLINKER_INCLUDE_CONFIG= ${LLVM_TOPLEVEL}/config/mclinker
LLVM_INCLUDE_OBJDIR!=	cd ${LLVM_TOPLEVEL}/include && ${PRINTOBJDIR}

.if ${MKLLD:Uno} == "yes" || ${MKLLDB:Uno} == "yes"
CPPFLAGS+=	-std=c++11
.endif
.if ${MKLLDB:Uno} == "yes"
CPPFLAGS+=	-DLLDB_DISABLE_PYTHON
.endif
CXXFLAGS+=	-fno-rtti -fno-exceptions
CXXFLAGS+=	-ffunction-sections -fdata-sections
LDFLAGS+=	-Wl,--gc-sections
.endif
CPPFLAGS+=	-I${LLVM_INCLUDE_OBJDIR} -I${LLVM_INCLUDE_CONFIG} \
		-I${CLANG_INCLUDE_CONFIG} -I${MCLINKER_INCLUDE_CONFIG}

LLVM_CONFIGURE_ARGS=	\
	--enable-targets=x86,powerpc,sparc,aarch64,arm,mips \
	--with-c-include-dirs=/usr/include/clang-${CLANG_VERSION}:/usr/include \
	--disable-timestamps --prefix=/usr --sysconfdir=/etc/llvm \
	--with-clang-srcdir=${CLANG_SRCDIR} \
	--host=${MACHINE_GNU_PLATFORM} \
	llvm_cv_gnu_make_command=${MAKE}

.for tool in circo dot dotty fdp neato twopi xdot
LLVM_CONFIGURE_ARGS+= ac_cv_path_${tool:tu}="echo ${tool}"
.endfor

.endif
