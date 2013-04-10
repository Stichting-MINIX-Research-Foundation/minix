#	$NetBSD: Makefile,v 1.1 2009/05/08 17:28:01 christos Exp $

.include <bsd.own.mk>
.include <bsd.sys.mk>

.include "../Makefile.inc"

.if ${MKSHARE} != "no"
FILESDIR=	${MFILESDIR}
FILES=		${MFILES}
.endif

PROG=		file
LDADD+=		-lmagic -lz
DPADD+=		${LIBMAGIC} ${LIBZ}
MAN=		file.1 magic.5

CLEANFILES+=	magic.mgc
.if ${MKSHARE} != "no"
realall:	file magic.mgc
.endif

.if ${MKSHARE} != "no"
magic.mgc: ${TOOL_MKMAGIC}
	${_MKTARGET_CREATE}
	${TOOL_MKMAGIC} -C -m ${DIST}/magic/magdir
	@mv magdir.mgc ${.TARGET}

.else
magic.mgc:
.endif

.include <bsd.prog.mk>
