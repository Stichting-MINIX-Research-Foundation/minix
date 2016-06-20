# $NetBSD: Makefile.inc,v 1.2 2008/10/25 22:35:36 apb Exp $

.PATH: ${.CURDIR}/MISC

CODESETS_MISC!=	${TOOL_SED} \
			'/^\#/d;/^ *$$/d;/^[ 	]*/s///;s/[ 	].*//' \
			${.CURDIR}/MISC/esdb.dir.MISC.src
ESDB_MISC:=	${CODESETS_MISC:C/$/.esdb/:S/:/@/}

SRC_esdb.dir+=		esdb.dir.MISC.src
SRC_esdb.alias+=	esdb.alias.MISC.src
FILES+=			${ESDB_MISC}
CLEANFILES+=		${ESDB_MISC}
.for i in ${ESDB_MISC}
FILESDIR_$i:=		${BINDIR}/MISC
.endfor
