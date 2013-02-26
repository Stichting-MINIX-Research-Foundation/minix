#	$NetBSD: Makefile,v 1.52 2012/09/18 07:05:15 skrll Exp $

.include <bsd.own.mk>

.ifdef __MINIX
HAVE_GCC= 45

GCC_LANGUAGES=c
.else
GCC_LANGUAGES=c c++ objc
.endif
MODULE=			gcc4

MKNATIVE_CONFIG_TARGET_LIBS= \
	configure-target-libstdc++-v3 \
	configure-target-libobjc

.if ${HAVE_GCC} == 45
MKNATIVE_TARGET=	gcc45
GNUHOSTDIST=		${.CURDIR}/../../external/gpl3/gcc/dist
MKNATIVE_CONFIG_TARGET_LIBS+= \
	configure-target-libgomp
.else
MKNATIVE_TARGET=	gcc4
.endif

BINENV=		/usr/bin/env -i

VAX_CONFIGURE_ARGS=

.if ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "sparc64"
MULTILIB_ARGS= --enable-multilib
.else
MULTILIB_ARGS= --disable-multilib
.endif

.if ${MKSOFTFLOAT} != "no"
SOFTFLOAT_ARGS=	-with-float=soft
.endif

# LSC FIXME We are not using the correct target ATM, so overrides for now...
COMMON_CONFIGURE_ARGS=	--target=${MACHINE_GNU_PLATFORM} \
			--disable-libssp \
			--disable-threads \
			--disable-visibility \
			--disable-libunwind \
			--enable-long-long \
			--enable-threads \
			--with-bugurl=http://www.NetBSD.org/Misc/send-pr.html \
			--with-pkgversion="NetBSD nb1 20120916" \
			--with-system-zlib \
			${VAX_CONFIGURE_ARGS} \
			--enable-__cxa_atexit
.if defined(GCC_CONFIG_ARCH.${MACHINE_ARCH})
COMMON_CONFIGURE_ARGS+=	--with-arch=${GCC_CONFIG_ARCH.${MACHINE_ARCH}}
.endif
.if defined(GCC_CONFIG_TUNE.${MACHINE_ARCH})
COMMON_CONFIGURE_ARGS+=	--with-tune=${GCC_CONFIG_TUNE.${MACHINE_ARCH}}
.endif

CONFIGURE_ARGS=	${COMMON_CONFIGURE_ARGS}
.if ${HAVE_GCC} >= 45
CONFIGURE_ARGS+= \
		--with-mpc=${TOOLDIR} \
		--with-mpfr=${TOOLDIR} \
		--with-gmp=${TOOLDIR}
.endif
CONFIGURE_ARGS+= \
		--disable-nls \
		${MULTILIB_ARGS} \
		${SOFTFLOAT_ARGS} \
		--program-transform-name="s,^,${MACHINE_GNU_PLATFORM}-," \
		--enable-languages="${GCC_LANGUAGES}"

GCC_CPPFLAGS=	-DNETBSD_TOOLS -DTARGET_SYSTEM_ROOT=0 \
		-DTARGET_SYSTEM_ROOT_RELOCATABLE

.if ${HOST_OSTYPE:C/\-.*//} == "Minix"
CONFIGURE_ENV+= LDFLAGS=-lm
.endif

MAKE_ARGS=	MACHINE= MAKEINFO=${TOOL_MAKEINFO:Q} \
		LIBGCC= LIBGCC1= LIBGCC1_TEST= LIBGCC2= INSTALL_LIBGCC= \
		EXTRA_PARTS= CPPFLAGS=${GCC_CPPFLAGS:Q} \
		AR=${HOST_AR:Q} RANLIB=${HOST_RANLIB:Q}

CONFIGURE_ENV+= gcc_cv_libc_provides_ssp=yes \
		gcc_cv_as_sparc_gotdata_op=no

MKNATIVE_ENV=	${BINENV} ${CONFIGURE_ENV:NC*:NLD*} \
			CC_FOR_BUILD=${HOST_CC:Q} \
			CC=${CC:Q}' '${CCADDFLAGS:Q} \
			CXX=${CXX:Q}' '${CCADDFLAGS:Q}' '${CXXADDFLAGS:Q} \
			CPP=${CPP:Q}' '-I${DESTDIR}/usr/include \
			CFLAGS= CPPFLAGS= CXXFLAGS= LDFLAGS= \
			AS=${AS:Q} AWK=${TOOL_AWK:Q} LD=${LD:Q} \
			MSGFMT=${TOOLDIR}/bin/${_TOOL_PREFIX}msgfmt \
			NM=${NM:Q} OBJDUMP=${OBJDUMP:Q} \
			XGETTEXT=${TOOLDIR}/bin/${_TOOL_PREFIX}xgettext \
			LIBS=-lintl \
			ac_cv_prog_cc_cross=yes \
			ac_cv_func_strcoll_works=yes \
			ac_cv_func_elf_getshstrndx=no \
			gcc_cv_func_printf_ptr=yes \
			gcc_cv_libc_provides_ssp=yes \
			gdb_cv_printf_has_long_double=yes \
			gdb_cv_printf_has_long_long=yes \
			gdb_cv_scanf_has_long_double=yes \
			gcc_cv_as_sparc_gotdata_op=no

ALL_TARGET=	all-gcc
INSTALL_TARGET=	install-gcc

.include "${.CURDIR}/../Makefile.gmakehost"
BUILD_MAKE=${TOOL_GMAKE}

#
# mknative-gcc specific stuff
#

.if ${HAVE_GCC} < 45
GCCSRCDIR=${.CURDIR}/../../gnu/dist/gcc4
.else
GCCSRCDIR=${.CURDIR}/../../external/gpl3/gcc/dist
.endif

CXXADDFLAGS=	--sysroot=${DESTDIR}
CCADDFLAGS=	--sysroot=${DESTDIR} -L${DESTDIR}/lib -L${DESTDIR}/usr/lib -B${DESTDIR}/usr/lib/ -I${.OBJDIR}/.native/gcc/include

NEWCONFIGDIR?=	${.CURDIR}/../..
MKNATIVE?=	${.CURDIR}/mknative-gcc

bootstrap-libgcc: .configure_done
	@echo 'Creating files needed for libgcc by a native bootstrap build.'
	@MAKE=${BUILD_MAKE:Q} ${HOST_SH} ${MKNATIVE} lib${MKNATIVE_TARGET} \
		${.OBJDIR}/build ${NEWCONFIGDIR} ${MACHINE_GNU_PLATFORM} \
		${DESTDIR}

native-gcc: .native/.configure_done
	@echo 'Extracting GNU GCC configury for a native toolchain.'
	@MAKE=${BUILD_MAKE:Q} ${HOST_SH} ${MKNATIVE} ${MKNATIVE_TARGET} \
		${.OBJDIR}/.native ${NEWCONFIGDIR} ${MACHINE_GNU_PLATFORM} \
		${DESTDIR}

NATIVE_CONFIGURE_ARGS=	${COMMON_CONFIGURE_ARGS}
.if ${HAVE_GCC} >= 45
NATIVE_CONFIGURE_ARGS+=	\
			--with-mpc=${DESTDIR}/usr \
			--with-mpfr=${DESTDIR}/usr \
			--with-gmp=${DESTDIR}/usr

.  if ${MACHINE_ARCH} != "vax"
NATIVE_CONFIGURE_ARGS+= --enable-tls
.  endif
.endif
NATIVE_CONFIGURE_ARGS+=	\
			--disable-multilib \
			--disable-symvers \
			--disable-libstdcxx-pch \
			--build=`${GCCSRCDIR}/config.guess` \
			--host=${MACHINE_GNU_PLATFORM}

.native/.configure_done: ${_GNU_CFGSRC} ${.CURDIR}/Makefile
	mkdir .native 2>/dev/null || true
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native && ${MKNATIVE_ENV} \
			${HOST_SH} ${GNUHOSTDIST}/configure \
			${NATIVE_CONFIGURE_ARGS}) && \
		(cd .native && ${MKNATIVE_ENV} ${BUILD_MAKE} all-build-libiberty)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native && ${MKNATIVE_ENV} ${BUILD_MAKE} configure-gcc configure-libcpp) && \
		(cd .native && ${MKNATIVE_ENV} ${BUILD_MAKE} configure-libiberty) && \
		(cd .native && ${MKNATIVE_ENV} ${BUILD_MAKE} configure-libdecnumber)
	# edit Makefile so that maybe-all-gcc does not depend on all-gcc any more.
		(cd .native && mv Makefile Makefile.config && \
			${TOOL_SED} -e 's/\(maybe-all-gcc:\) all-gcc/\1/' \
				    -e 's/\(maybe-all-target-libgcc:\) all-target-libgcc/\1/' \
				< Makefile.config > Makefile)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native/gcc && ${MKNATIVE_ENV} ${BUILD_MAKE} -e tree-check.h config.h multilib.h gcov-iov.h)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native/gcc && ${MKNATIVE_ENV} ${BUILD_MAKE} -e libgcc.mvars tconfig.h unwind.h)
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
