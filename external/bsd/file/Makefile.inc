#	$NetBSD: Makefile.inc,v 1.5 2014/01/17 20:24:20 martin Exp $

WARNS=4
DIST=${NETBSDSRCDIR}/external/bsd/file/dist

.include <bsd.own.mk>

BINDIR?= /usr/bin
USE_FORT?= yes	# data-driven bugs?

TOOL_MKMAGIC?=	${.OBJDIR}/file

MFILESDIR?=	/usr/share/misc
MFILES?=	magic.mgc
MAGIC?=		${MFILESDIR}/magic

VERSION != ${TOOL_SED} -ne "s/^PACKAGE_VERSION='\([^']*\)'/\1/p" ${DIST}/configure

CPPFLAGS+=	-DMAGIC='"${MAGIC}"' -DHAVE_CONFIG_H -DQUICK -DBUILTIN_ELF \
		-DELFCORE -DVERSION='"${VERSION}"'
CPPFLAGS+=	-I${.CURDIR}/../include -I${DIST}/src

.if ${ACTIVE_CC}=="gcc" && ${HAVE_GCC}=="48"
CPPFLAGS.readelf.c	+= -Wno-error=maybe-uninitialized
.endif

.PATH:		${DIST}/src ${DIST}/doc
