#	$NetBSD: xorg-pkg-ver.mk,v 1.8 2015/08/09 10:09:07 aymeric Exp $

# when including this make sure PROG is set so that $X11SRCDIR.$PROG
# is a valid setting.  set XORG_PKG_VER_PROG if PROG is wrong.
# set XORG_PKG_VER_CONFIG_PATH if "configure" at the top-level is wrong.

XORG_PKG_VER_PROG?=	${PROG}

XORG_PKG_VER_CONFIG_PATH?=	configure
_XORG_PKG_CONFIGURE_PATH=	${X11SRCDIR.${XORG_PKG_VER_PROG}}/${XORG_PKG_VER_CONFIG_PATH}

.if exists(${X11SRCDIR.${XORG_PKG_VER_PROG}}/configure)
XORG_PKG_PACKAGE_VERSION!= \
	${TOOL_AWK} -F= '/^PACKAGE_VERSION=/ {			\
	     match($$2, "([0-9]+\\.)+[0-9]+");			\
	     version = substr($$2, RSTART, RLENGTH);		\
	     print version;					\
	     exit 0;						\
	}' ${X11SRCDIR.${XORG_PKG_VER_PROG}}/configure
.if !empty(XORG_PKG_PACKAGE_VERSION)
CPPFLAGS+=	-DPACKAGE_VERSION=\"${XORG_PKG_PACKAGE_VERSION:Q}\"
CPPFLAGS+=	-DVERSION=\"${XORG_PKG_PACKAGE_VERSION:Q}\"
.endif

XORG_PKG_PACKAGE_STRING!= \
	${TOOL_AWK} -F= '/^PACKAGE_STRING=/ {			\
	     match($$2, "[-_a-zA-Z]+[ 	]+([0-9]+\\.)+[0-9]+");	\
	     string = substr($$2, RSTART, RLENGTH);		\
	     print string;					\
	     exit 0;						\
	}' ${X11SRCDIR.${XORG_PKG_VER_PROG}}/configure
.if !empty(XORG_PKG_PACKAGE_STRING)
CPPFLAGS+=	-DPACKAGE_STRING=\"${XORG_PKG_PACKAGE_STRING:Q}\"
.endif

XORG_PKG_PACKAGE_NAME!= \
	${TOOL_AWK} -F= '/^PACKAGE_NAME=/ {			\
	     match($$2, "'"'"'[-_a-zA-Z0-9]+'"'"'");		\
	     name = substr($$2, RSTART, RLENGTH);		\
	     print name;					\
	     exit 0;						\
	}' ${X11SRCDIR.${XORG_PKG_VER_PROG}}/configure
.if !empty(XORG_PKG_PACKAGE_NAME)
CPPFLAGS+=	-DPACKAGE_NAME=\"${XORG_PKG_PACKAGE_NAME:Q}\"
.endif

.endif
