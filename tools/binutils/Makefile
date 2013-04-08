#	$NetBSD: Makefile,v 1.22 2012/04/15 08:37:32 mrg Exp $

.include <bsd.own.mk>

MODULE=		binutils

GNUHOSTDIST=	${.CURDIR}/../../external/gpl3/binutils/dist

BRANDING?=	\
	--with-pkgversion="NetBSD Binutils nb1" \
	--with-bugurl="http://www.NetBSD.org/support/send-pr.html" \
	--with-lib-path="=/usr/lib" --with-sysroot

CONFIGURE_ARGS=	--target=${MACHINE_GNU_PLATFORM} --disable-nls \
		--program-transform-name="s,^,${MACHINE_GNU_PLATFORM}-," \
		--disable-werror \
		${BRANDING}

MAKE_ARGS=	MACHINE= MAKEINFO=${TOOL_MAKEINFO:Q}

ALL_TARGET=	all-binutils all-gas all-ld
INSTALL_TARGET=	install-binutils install-gas install-ld
.if ${MKCROSSGPROF:Uno} != "no"
ALL_TARGET+=	all-gprof
INSTALL_TARGET+=install-gprof
.endif

.if defined(__MINIX)
CONFIGURE_ARGS+=	\
	--with-ld=default \
	--with-lto
#LSC: FIXME Not yet gold, need to create
# external/gpl3/binutils/usr.bin/gold directory & needed Makefiles
#	--with-gold=yes
#ALL_TARGET+=	all-gold
#INSTALL_TARGET+=	install-gold
.endif # defined(__MINIX)
.include "${.CURDIR}/../Makefile.gnuhost"

CCADDFLAGS=	-I${DESTDIR}/usr/include -L${DESTDIR}/lib -L${DESTDIR}/usr/lib -B${DESTDIR}/usr/lib/

# Force avoiding possibly non-executable install-sh.
CONFIGURE_ENV+= ac_cv_path_mkdir="${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-install -d"

NEWCONFIGDIR?=	${.CURDIR}/../..
MKNATIVE?=	${.CURDIR}/mknative-binutils

native-binutils: .native/.configure_done
	@echo 'Extracting GNU binutils configury for a native toolchain.'
	MAKE=${MAKE:Q} ${HOST_SH} ${MKNATIVE} binutils \
		${.OBJDIR}/.native ${NEWCONFIGDIR} ${MACHINE_GNU_PLATFORM}

.native/.configure_done: ${_GNU_CFGSRC} ${.CURDIR}/Makefile
	mkdir .native 2>/dev/null || true
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native && ${CONFIGURE_ENV:NC*:NLD*} \
			CC_FOR_BUILD=${HOST_CC:Q} \
			CC=${CC:Q}' '${CCADDFLAGS:Q} \
			CXX=${CXX:Q}' '${CCADDFLAGS:Q} \
			CPP=${CPP:Q}' '-I${DESTDIR}/usr/include \
			CFLAGS= CPPFLAGS= CXXFLAGS= LDFLAGS= \
			MSGFMT=${TOOLDIR}/bin/${_TOOL_PREFIX}msgfmt \
			XGETTEXT=${TOOLDIR}/bin/${_TOOL_PREFIX}xgettext \
			LIBS=-lintl \
			ac_cv_prog_cc_cross=yes \
			ac_cv_func_strcoll_works=yes \
			${HOST_SH} ${GNUHOSTDIST}/configure \
			--build=`${GNUHOSTDIST}/config.guess` \
			--host=${MACHINE_GNU_PLATFORM} \
			--target=${MACHINE_GNU_PLATFORM} \
			${BRANDING} \
		)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native && ${MAKE} configure-host)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native/bfd && ${MAKE} bfd.h bfdver.h)
	PATH=${TOOLDIR}/bin:$$PATH; export PATH; \
		(cd .native/ld && ${MAKE} ldemul-list.h)
	@touch $@

clean: clean.native
clean.native:
	-rm -r -f .native
