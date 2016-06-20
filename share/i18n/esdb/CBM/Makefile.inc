# $NetBSD: Makefile.inc,v 1.1 2011/04/27 20:05:56 mbalmer Exp $

.PATH: ${.CURDIR}/CBM

CODESETS_CBM!=	${TOOL_SED} \
		'/^\#/d;/^ *$$/d;/^[ 	]*/s///;s/[ 	].*//' \
		${.CURDIR}/CBM/esdb.dir.CBM.src
ESDB_CBM:=	${CODESETS_CBM:C/$/.esdb/:S/:/@/}

SRC_esdb.dir+=		esdb.dir.CBM.src
SRC_esdb.alias+=	esdb.alias.CBM.src
FILES+=			${ESDB_CBM}
CLEANFILES+=		${ESDB_CBM}
.for i in ${ESDB_CBM}
FILESDIR_$i:=		${BINDIR}/CBM
.endfor
