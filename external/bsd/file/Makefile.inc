#	$NetBSD: Makefile.inc,v 1.3 2009/05/08 23:36:42 christos Exp $

DIST=${.CURDIR}/../dist
WARNS=4

BINDIR?= /usr/bin
USE_FORT?= no	# data-driven bugs?

.if ${USETOOLS} == "yes" 
TOOL_MKMAGIC?=	${.OBJDIR}/file
.else
TOOL_MKMAGIC= ${.CURDIR}/../bin/file
.endif

MFILESDIR?=	/usr/share/misc
MFILES?=	magic.mgc
MAGIC?=		${MFILESDIR}/magic

#-DQUICK
CPPFLAGS+=	-DMAGIC='"${MAGIC}"' -DHAVE_CONFIG_H  -DBUILTIN_ELF \
		-DELFCORE
CPPFLAGS+=	-I${.CURDIR}/../include -I${DIST}/src

.PATH:		${DIST}/src ${DIST}/doc
