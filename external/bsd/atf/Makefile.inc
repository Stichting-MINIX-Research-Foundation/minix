# $NetBSD: Makefile.inc,v 1.5 2014/02/15 04:19:46 jmmv Exp $

.include <bsd.own.mk>

TOPDIR=		${NETBSDSRCDIR}/external/bsd/atf
SRCDIR=		${TOPDIR}/dist

# Depend on the atf-version.txt target to generate a file that contains
# the version number of the currently imported ATF release and that only
# changes on new imports.
atf-version.txt: atf-version-real.txt
	@cmp -s atf-version.txt atf-version-real.txt \
	    || cp atf-version-real.txt atf-version.txt
atf-version-real.txt: .PHONY
	@grep 'define VERSION' ${TOPDIR}/bconfig.h \
	    | cut -d '"' -f 2 >atf-version-real.txt
CLEANFILES+= atf-version.txt atf-version-real.txt

CPPFLAGS+=	-DHAVE_CONFIG_H

# Not all source files need these settings, but it's just easier to
# set them in all cases.
CPPFLAGS+=	-DATF_ARCH=\"${MACHINE_ARCH}\"
CPPFLAGS+=	-DATF_BUILD_CC=\"/usr/bin/cc\"
CPPFLAGS+=	-DATF_BUILD_CFLAGS=\"\"
CPPFLAGS+=	-DATF_BUILD_CPP=\"/usr/bin/cpp\"
CPPFLAGS+=	-DATF_BUILD_CPPFLAGS=\"\"
CPPFLAGS+=	-DATF_BUILD_CXX=\"/usr/bin/c++\"
CPPFLAGS+=	-DATF_BUILD_CXXFLAGS=\"\"
CPPFLAGS+=	-DATF_CONFDIR=\"/etc/atf\"
CPPFLAGS+=	-DATF_INCLUDEDIR=\"/usr/include\"
CPPFLAGS+=	-DATF_LIBDIR=\"/usr/lib\"
CPPFLAGS+=	-DATF_LIBEXECDIR=\"/usr/libexec\"
CPPFLAGS+=	-DATF_MACHINE=\"${MACHINE}\"
CPPFLAGS+=	-DATF_M4=\"/usr/bin/m4\"
CPPFLAGS+=	-DATF_PKGDATADIR=\"/usr/share/atf\"
CPPFLAGS+=	-DATF_SHELL=\"/bin/sh\"
CPPFLAGS+=	-DATF_WORKDIR=\"/tmp\"

CPPFLAGS+=	-I${TOPDIR}			# For bconfig.h.
CPPFLAGS+=	-I${TOPDIR}/lib/libatf-c	# For atf-c/defs.hpp.
CPPFLAGS+=	-I${SRCDIR}

.if defined(USE_ATF_LIBTOOLS)
LIBTOOLSOBJDIR!=	cd ${TOPDIR}/lib/tools; ${PRINTOBJDIR}
CPPFLAGS+=		-I${LIBTOOLSOBJDIR}
LDADD+=			${LIBTOOLSOBJDIR}/libtools.a
DPADD+=			${LIBTOOLSOBJDIR}/libtools.a
.endif
