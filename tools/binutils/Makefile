#	$NetBSD: Makefile,v 1.31 2020/04/04 00:47:30 christos Exp $

.include <bsd.hostinit.mk>

MODULE=		binutils

GNUHOSTDIST=	${.CURDIR}/../../external/gpl3/binutils/dist

BRANDING?=	\
	--with-pkgversion="NetBSD Binutils nb1" \
	--with-bugurl="http://www.NetBSD.org/support/send-pr.html" \
	--with-lib-path="=/usr/lib" --with-sysroot

CONFIGURE_ARGS=	--target=${MACHINE_GNU_PLATFORM} --disable-nls \
		--program-transform-name="s,^,${MACHINE_GNU_PLATFORM}-," \
		--disable-werror --disable-initfini-array \
		${BRANDING}

.if defined(__MINIX) && ! exists(${GNUHOSTDIST}/gas/m68k-parse.c)
# MINIX: LSC: Make sure we trigger the fetch rule
.include "${.CURDIR}/../../minix/Makefile.fetchgnu"

${GNUHOSTDIST}/gas/m68k-parse.c: ${fetch_done}
	@true
.endif # defined(__MINIX)

build/gas/m68k-parse.c: ${GNUHOSTDIST}/gas/m68k-parse.c
	@mkdir build 2>/dev/null || true
	@mkdir build/gas 2>/dev/null || true
	cat ${GNUHOSTDIST}/gas/m68k-parse.c > ${.TARGET}

.configure_done: build/gas/m68k-parse.c

MAKE_ARGS=	MACHINE= MAKEINFO=${TOOL_MAKEINFO:Q}

ALL_TARGET=	all-binutils all-gas all-ld
INSTALL_TARGET=	install-binutils install-gas install-ld
.if ${MKCROSSGPROF:Uno} != "no"
ALL_TARGET+=	all-gprof
INSTALL_TARGET+=install-gprof
.endif

.include "${.CURDIR}/../Makefile.gnuhost"

CCADDFLAGS=	-I${GNUHOSTDIST}/include  -I${DESTDIR}/usr/include
CCADDFLAGS+=	-B${DESTDIR}/usr/lib/
LDADDFLAGS=	-L${DESTDIR}/lib -L${DESTDIR}/usr/lib

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
			CFLAGS= CPPFLAGS= CXXFLAGS= \
			LDFLAGS=${LDADDFLAGS:Q} \
			MSGFMT=${TOOLDIR}/bin/${_TOOL_PREFIX}msgfmt \
			XGETTEXT=${TOOLDIR}/bin/${_TOOL_PREFIX}xgettext \
			LIBS=-lintl \
			ac_cv_prog_cc_cross=yes \
			ac_cv_func_strcoll_works=yes \
			${HOST_SH} ${GNUHOSTDIST}/configure \
			--build=`${GNUHOSTDIST}/config.guess` \
			--host=${MACHINE_GNU_PLATFORM} \
			--target=${MACHINE_GNU_PLATFORM} \
			--disable-initfini-array \
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
