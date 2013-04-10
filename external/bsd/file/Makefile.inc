#	$NetBSD: Makefile.inc,v 1.3 2009/05/08 23:36:42 christos Exp $

DIST=${.CURDIR}/../dist
WARNS=4

BINDIR?= /usr/bin
USE_FORT?= yes	# data-driven bugs?

TOOL_MKMAGIC?=	${.OBJDIR}/file

MFILESDIR?=	/usr/share/misc
MFILES?=	magic.mgc
MAGIC?=		${MFILESDIR}/magic

#this is still required (will not compile without removing it): -DQUICK
CPPFLAGS+=	-DMAGIC='"${MAGIC}"' -DHAVE_CONFIG_H -DBUILTIN_ELF \
		-DELFCORE
CPPFLAGS+=	-I${.CURDIR}/../include -I${DIST}/src

.PATH:		${DIST}/src ${DIST}/doc
