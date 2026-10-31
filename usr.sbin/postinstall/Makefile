# $NetBSD: Makefile,v 1.3 2019/06/14 01:54:16 christos Exp $

.include <bsd.own.mk>

FILES=		postinstall
MAN=		postinstall.8

FILESDIR=	/usr/sbin
FILESMODE=	${BINMODE}
FILESBUILD=	yes

ARCHSUBDIRS	!= ${TOOL_SED} -n '/ARCHDIR_SUBDIR/s/[[:space:]]//gp' \
	${NETBSDSRCDIR}/compat/archdirs.mk

.SUFFIXES: .in
.in:
	${_MKTARGET_CREATE}
	@rm -f ${.TARGET}
	${TOOL_SED} -e "s#@ARCHSUBDIRS@#${ARCHSUBDIRS}#" < ${.ALLSRC} > ${.TARGET}

CLEANFILES+= postinstall

.include <bsd.prog.mk>
