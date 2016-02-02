#	$NetBSD: Makefile,v 1.74 2015/01/16 20:10:26 christos Exp $

.include <bsd.own.mk>

GCC_LANGUAGES=c c++ objc
MODULE=			gcc4

GCCDIST=	${.CURDIR}/../../external/gpl3/${EXTERNAL_GCC_SUBDIR}/dist
GNUHOSTDIST=	${GCCDIST}

.if ${HAVE_GCC} == 48
CC_FOR_BUILD=		${HOST_CXX:Q}
LIB_INTL=
.elif ${HAVE_GCC} == 45
UNWIND_H=unwind.h
MKNATIVE=		${.CURDIR}/mknative-gcc.old
.endif

# Defaults
CC_FOR_BUILD?=		${HOST_CC:Q}
LIB_INTL?=		"LIBS=-lintl"
MKNATIVE?=		${.CURDIR}/mknative-gcc

.if ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "sparc64"
MULTILIB_ARGS= --enable-multilib
.else
MULTILIB_ARGS= --disable-multilib
.endif

.if ${MKSOFTFLOAT} != "no" && ${MACHINE_CPU} != "m68k" \
    && ${MACHINE_CPU} != "or1k"
SOFTFLOAT_ARGS=	-with-float=soft
.endif

.include "gcc-version.mk"

COMMON_CONFIGURE_ARGS=	--target=${MACHINE_GNU_PLATFORM} \
			--enable-long-long \
			--enable-threads \
			--with-bugurl=http://www.NetBSD.org/Misc/send-pr.html \
			--with-pkgversion="NetBSD ${NETBSD_GCC_VERSION}" \
			--with-system-zlib \
			--enable-__cxa_atexit \
			# MINIX: --enable-libstdcxx-threads

.if defined(__MINIX)
COMMON_CONFIGURE_ARGS+=	\
			--disable-libssp \
			--enable-visibility \
			--disable-threads \
			--disable-tls

CONFIGURE_ENV+= gcc_cv_ld_hidden=yes 

.if ${HOST_OSTYPE:C/\-.*//} == "Minix"
CONFIGURE_ENV+= LDFLAGS=-lm
.endif #  ${HOST_OSTYPE:C/\-.*//} == "Minix"

.endif # defined(__MINIX)

.if defined(GCC_CONFIG_ARCH.${MACHINE_ARCH})
COMMON_CONFIGURE_ARGS+=	--with-arch=${GCC_CONFIG_ARCH.${MACHINE_ARCH}}
.endif
.if defined(GCC_CONFIG_TUNE.${MACHINE_ARCH})
COMMON_CONFIGURE_ARGS+=	--with-tune=${GCC_CONFIG_TUNE.${MACHINE_ARCH}}
.endif

.if ${HAVE_GCC} == 48
COMMON_CONFIGURE_ARGS+= --enable-libstdcxx-time=rt --enable-lto
.endif

CONFIGURE_ARGS=	${COMMON_CONFIGURE_ARGS}
CONFIGURE_ARGS+= \
		--with-sysroot=${DESTDIR} \
		--with-mpc=${TOOLDIR} \
		--with-mpfr=${TOOLDIR} \
		--with-gmp=${TOOLDIR} \
		--disable-nls \
		${MULTILIB_ARGS} \
		${SOFTFLOAT_ARGS} \
		--program-transform-name="s,^,${MACHINE_GNU_PLATFORM}-," \
		--enable-languages="${GCC_LANGUAGES}"

GCC_CPPFLAGS=	-DNETBSD_TOOLS -DTARGET_SYSTEM_ROOT=0 \
		-DTARGET_SYSTEM_ROOT_RELOCATABLE

MAKE_ARGS=	MACHINE= MAKEINFO=${TOOL_MAKEINFO:Q} \
		LIBGCC= LIBGCC1= LIBGCC1_TEST= LIBGCC2= INSTALL_LIBGCC= \
		EXTRA_PARTS= CPPFLAGS=${GCC_CPPFLAGS:Q} \
		AR=${HOST_AR:Q} RANLIB=${HOST_RANLIB:Q}

CONFIGURE_ENV+= gcc_cv_libc_provides_ssp=yes \
		gcc_cv_as_sparc_gotdata_op=no

ALL_TARGET=	all-gcc
INSTALL_TARGET=	install-gcc

.include "${.CURDIR}/../Makefile.gmakehost"
BUILD_MAKE=${TOOL_GMAKE}

#
# mknative-gcc specific stuff
#

MKNATIVE_CONFIG_TARGET_LIBS=

.if ${HAVE_GCC} == 48
MKNATIVE_CONFIG_TARGET_LIBS+=	configure-target-libgcc
MKNATIVE_CONFIG_TARGET_LIBS+=	configure-target-libgomp
MKNATIVE_TARGET=	gcc48
.elif ${HAVE_GCC} == 45
MKNATIVE_CONFIG_TARGET_LIBS+=	configure-target-libgomp
MKNATIVE_TARGET=	gcc45
.endif

MKNATIVE_CONFIG_TARGET_LIBS+= \
	configure-target-libstdc++-v3 \
	configure-target-libobjc

BINENV=		/usr/bin/env -i


MKNATIVE_ENV=	${BINENV} ${CONFIGURE_ENV:NC*:NLD*} \
			CC_FOR_BUILD=${CC_FOR_BUILD:Q} \
			CXX_FOR_BUILD=${HOST_CXX:Q} \
			CFLAGS_FOR_BUILD="-I${TOOLDIR}/include" \
			CC=${CC:Q}' '${CCADDFLAGS:Q} \
			CXX=${CXX:Q}' '${CCADDFLAGS:Q}' '${CXXADDFLAGS:Q} \
			CPP=${CPP:Q}' '-I${DESTDIR}/usr/include \
			CFLAGS= CPPFLAGS= CXXFLAGS= LDFLAGS= \
			AS=${AS:Q} AWK=${TOOL_AWK:Q} LD=${LD:Q} \
			MSGFMT=${TOOLDIR}/bin/${_TOOL_PREFIX}msgfmt \
			NM=${NM:Q} OBJDUMP=${OBJDUMP:Q} \
			XGETTEXT=${TOOLDIR}/bin/${_TOOL_PREFIX}xgettext \
			${LIB_INTL} \
			ac_cv_prog_cc_cross=yes \
			ac_cv_func_strcoll_works=yes \
			ac_cv_func_elf_getshstrndx=no \
			gcc_cv_func_printf_ptr=yes \
			gcc_cv_libc_provides_ssp=yes \
			gdb_cv_printf_has_long_double=yes \
			gdb_cv_printf_has_long_long=yes \
			gdb_cv_scanf_has_long_double=yes \
			gcc_cv_as_sparc_gotdata_op=no

CXXADDFLAGS=	--sysroot=${DESTDIR}
CCADDFLAGS=	--sysroot=${DESTDIR} -L${DESTDIR}/lib -L${DESTDIR}/usr/lib -B${DESTDIR}/usr/lib/ -I${.OBJDIR}/.native/gcc/include

# NEWCONFIGDIR can be set to a read-write location of the source tree
# in case the version being used is not.
NEWCONFIGDIR?=	${.CURDIR}/../..

bootstrap-libgcc: .configure_done
	@echo 'Creating files needed for libgcc by a native bootstrap build.'
	(cd ${.OBJDIR}/build && ${BUILD_COMMAND} configure-target-libgcc)
	@MAKE=${BUILD_MAKE:Q} ${HOST_SH} ${MKNATIVE} lib${MKNATIVE_TARGET}-bootstrap \
		${.OBJDIR}/build ${NEWCONFIGDIR} ${NETBSDSRCDIR} \
		${MACHINE_GNU_PLATFORM} ${DESTDIR} ${TOOLDIR}

.if ${HAVE_GCC} == "48"
bootstrap-libstdc++: .configure_done
	@echo 'Creating files needed for libstdc++ by a native bootstrap build.'
	(cd ${.OBJDIR}/build && ${BUILD_COMMAND} configure-target-libstdc++-v3)
	@MAKE=${BUILD_MAKE:Q} ${HOST_SH} ${MKNATIVE} libstdc++-bootstrap \
		${.OBJDIR}/build ${NEWCONFIGDIR} ${NETBSDSRCDIR} \
		${MACHINE_GNU_PLATFORM} ${DESTDIR} ${TOOLDIR}
.endif

native-gcc: .native/.configure_done
	@echo 'Extracting GNU GCC configury for a native toolchain.'
	@MAKE=${BUILD_MAKE:Q} ${HOST_SH} ${MKNATIVE} ${MKNATIVE_TARGET} \
		${.OBJDIR}/.native ${NEWCONFIGDIR} ${NETBSDSRCDIR} \
		${MACHINE_GNU_PLATFORM} ${DESTDIR} ${TOOLDIR}

NATIVE_CONFIGURE_ARGS=	${COMMON_CONFIGURE_ARGS}
MPC=		${NETBSDSRCDIR}/external/lgpl3/mpc
MPFR=		${NETBSDSRCDIR}/external/lgpl3/mpfr
GMP=		${NETBSDSRCDIR}/external/lgpl3/gmp
MPCOBJ!=	cd ${MPC}/lib/libmpc && ${PRINTOBJDIR}
MPFROBJ!=	cd ${MPFR}/lib/libmpfr && ${PRINTOBJDIR}
GMPOBJ!=	cd ${GMP}/lib/libgmp && ${PRINTOBJDIR}

GMP_MACHINE_ARCH?=	${MACHINE_ARCH:S/earmv4/arm/:C/armv[5-7]/arm/}

NATIVE_CONFIGURE_ARGS+=	\
			--with-mpc-lib=${MPCOBJ} \
			--with-mpfr-lib=${MPFROBJ} \
			--with-gmp-lib=${GMPOBJ} \
			--with-mpc-include=${MPC}/dist/src \
			--with-mpfr-include=${MPFR}/dist/src \
			--with-gmp-include=${GMP}/lib/libgmp/arch/${GMP_MACHINE_ARCH}

.if !defined(__MINIX)
.if ${MACHINE_ARCH} != "vax"
NATIVE_CONFIGURE_ARGS+= --enable-tls
.endif
.endif # !defined(__MINIX)
.if !empty(MACHINE_ARCH:Mearm*)
NATIVE_CONFIGURE_ARGS+=	--enable-initfini-array
.endif

.if ${MACHINE_ARCH} == "m68000"
NATIVE_CONFIGURE_ARGS+= --disable-shared
.endif

NATIVE_CONFIGURE_ARGS+=	\
			--disable-multilib \
			--disable-symvers \
			--disable-libstdcxx-pch \
			--build=`${GCCDIST}/config.guess` \
			--host=${MACHINE_GNU_PLATFORM} \
			--with-sysroot=${DESTDIR}
MKENV_BUILD_MAKE=cd .native && ${MKNATIVE_ENV} ${BUILD_MAKE}

.native/.configure_done: ${_GNU_CFGSRC} ${.CURDIR}/Makefile
	mkdir .native 2>/dev/null || true
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native && ${MKNATIVE_ENV} \
			${HOST_SH} ${GNUHOSTDIST}/configure \
			${NATIVE_CONFIGURE_ARGS}) && \
		(${MKENV_BUILD_MAKE} all-build-libiberty) && \
		([ "${HAVE_GCC}" = "48" ] && \
		 (${MKENV_BUILD_MAKE} all-libcpp) && \
		 (${MKENV_BUILD_MAKE} all-libbacktrace) && \
		 (${MKENV_BUILD_MAKE} all-libdecnumber) || true)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(${MKENV_BUILD_MAKE} configure-gcc configure-libcpp) && \
		(${MKENV_BUILD_MAKE} configure-libiberty) && \
		(${MKENV_BUILD_MAKE} configure-libdecnumber)
	# edit Makefile so that maybe-all-gcc does not depend on all-gcc any more.
		(cd .native && mv Makefile Makefile.config && \
			${TOOL_SED} -e 's/\(maybe-all-gcc:\) all-gcc/\1/' \
				    -e 's/\(maybe-all-target-libgcc:\) all-target-libgcc/\1/' \
				< Makefile.config > Makefile)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native/gcc && ${MKNATIVE_ENV} ${BUILD_MAKE} -e tree-check.h config.h multilib.h gcov-iov.h)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native/gcc && ${MKNATIVE_ENV} ${BUILD_MAKE} -e libgcc.mvars tconfig.h ${UNWIND_H})
		(cd .native && touch gcc/cc1obj gcc/cc1plus gcc/f771 gcc/libgcc.a gcc/libgcc_s.so)
.for _lib in ${MKNATIVE_CONFIG_TARGET_LIBS}
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native && ${MKNATIVE_ENV} ${BUILD_MAKE} \
			${_lib} \
			ALL_GCC_C= ALL_GCC_CXX= \
			CC_FOR_TARGET=${CC:Q}' '${CCADDFLAGS:Q} \
			CXX_FOR_TARGET=${CXX:Q}' '${CCADDFLAGS:Q}' '${CXXADDFLAGS:Q} \
			RAW_CXX_FOR_TARGET=${CXX:Q}' '${CCADDFLAGS:Q}' '${CXXADDFLAGS:Q} \
			CPP=${CPP:Q}' '-I${DESTDIR}/usr/include \
			ac_cv_prog_cc_cross=yes) && \
	true
.endfor
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native/${MACHINE_GNU_PLATFORM}/libstdc++-v3/include && \
			${MKNATIVE_ENV} ${BUILD_MAKE} \
			CC_FOR_TARGET=${CC:Q}' '${CCADDFLAGS:Q} \
			CXX_FOR_TARGET=${CXX:Q}' '${CCADDFLAGS:Q}' '${CXXADDFLAGS:Q} \
			CPP=${CPP:Q}' '-I${DESTDIR}/usr/include \
			all-local) && \
	true
.if 0
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native/${MACHINE_GNU_PLATFORM}/libiberty && \
			${MKNATIVE_ENV} ${BUILD_MAKE} needed-list)
.endif
	@touch $@

clean: clean.native
clean.native:
	-rm -r -f .native
