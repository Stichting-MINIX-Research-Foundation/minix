# $NetBSD: Makefile.inc,v 1.1 2013/02/23 14:16:49 jmmv Exp $

.include <bsd.own.mk>

TOPDIR=		${NETBSDSRCDIR}/external/bsd/kyua-cli
SRCDIR=		${TOPDIR}/dist

# Name of the private libraries (without their lib prefix) to depend on.
KYUA_LIBS?=

# Layout of installed files.
KYUA_BINDIR=		/usr/bin
KYUA_CONFDIR=		/etc/kyua
KYUA_DOCDIR=		/usr/share/doc/kyua-cli
KYUA_EXAMPLESDIR=	/usr/share/examples/kyua-cli
KYUA_MISCDIR=		/usr/share/kyua-cli/misc
KYUA_STOREDIR=		/usr/share/kyua-cli/store
KYUA_TESTERSDIR=	/usr/libexec
KYUA_TESTSDIR=		${TESTSBASE}

KYUA_TARNAME!=	grep 'define PACKAGE_TARNAME' ${TOPDIR}/config.h \
		| cut -d '"' -f 2
KYUA_VERSION!=	grep 'define VERSION' ${TOPDIR}/config.h | cut -d '"' -f 2

CPPFLAGS+=	-DHAVE_CONFIG_H

# Not all source files need these settings, but it's just easier to
# set them in all cases.
CPPFLAGS+=	-DKYUA_CONFDIR=\"${KYUA_CONFDIR}\"
CPPFLAGS+=	-DKYUA_DOCDIR=\"${KYUA_DOCDIR}\"
CPPFLAGS+=	-DKYUA_EXAMPLESDIR=\"${KYUA_EXAMPLESDIR}\"
CPPFLAGS+=	-DKYUA_MISCDIR=\"${KYUA_MISCDIR}\"
CPPFLAGS+=	-DKYUA_STOREDIR=\"${KYUA_STOREDIR}\"
CPPFLAGS+=	-DKYUA_TESTERSDIR=\"${KYUA_TESTERSDIR}\"
CPPFLAGS+=	-DKYUA_TESTSDIR=\"${KYUA_TESTSDIR}\"

CPPFLAGS+=	-I${TOPDIR}		# For config.h.
CPPFLAGS+=	-I${TOPDIR}/lib		# For utils/defs.hpp.
CPPFLAGS+=	-I${SRCDIR}

PRIVATELIBDIR!=	cd ${TOPDIR}/lib; ${PRINTOBJDIR}
.for lib in ${KYUA_LIBS}
LDADD+=		${PRIVATELIBDIR}/${lib}/lib${lib}.a
DPADD+=		${PRIVATELIBDIR}/${lib}/lib${lib}.a
.endfor

.if !empty(KYUA_LIBS:Mutils)
LDADD+=		-llutok -lsqlite3
DPADD+=		${LIBLUTOK} ${LIBSQLITE}
.endif

.if defined(__MINIX)
LDADD+=         -llua -lstdc++ -lgcc_s
DPADD+=         ${LIBLUA} ${LIBSTDCXX} ${LIBGCC_S}
LDFLAGS+=       -Wl,--allow-multiple-definition
.endif # defined(__MINIX)

BUILD_MANPAGE = \
	sed -e 's,__CONFDIR__,${KYUA_CONFDIR},g' \
	    -e 's,__DOCDIR__,${KYUA_DOCDIR},g' \
	    -e 's,__EGDIR__,${KYUA_EXAMPLESDIR},g' \
	    -e 's,__MISCDIR__,${KYUA_MISCDIR},g' \
	    -e 's,__PACKAGE__,${KYUA_TARNAME},g' \
	    -e 's,__STOREDIR__,${KYUA_STOREDIR},g' \
	    -e 's,__TESTERSDIR__,${KYUA_TESTERSDIR},g' \
	    -e 's,__TESTSDIR__,${KYUA_TESTSDIR},g' \
	    -e 's,__VERSION__,${KYUA_VERSION},g' \
	    <${SRCDIR}/doc/$${name}.in >$${name}.tmp; \
	mv $${name}.tmp $${name}
