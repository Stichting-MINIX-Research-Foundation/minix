#	$NetBSD: Makefile,v 1.2 2008/10/14 23:37:19 cube Exp $

.include <bsd.own.mk>

PROG=			rstartd.real
SRCS=			auth.c server.c
RSTARTDBINDIR=		${X11ROOTDIR}/libexec
RSTARTCONFIGDIR=	${X11ETCDIR}/rstart

CPPFLAGS.server.c=	-DSERVERNAME=\"rstartd\" \
			-DDEFAULT_CONFIG=\"${RSTARTCONFIGDIR}/config\"

MAN=			rstart.1 rstartd.1

X11EXTRAMANDEFS+=	-e 's,RSTARTCONFIGDIR,${RSTARTCONFIGDIR},g'

BUILDSYMLINKS=		client.cpp rstart.cpp \
			server.cpp rstartd.cpp

CPPSCRIPTS=		config rstart rstartd
CPPSCRIPTFLAGS_config=	-DBINDIR=${X11BINDIR} -DLIBDIR=${RSTARTCONFIGDIR} \
			-DPACKAGEname=rstart -DENVPREFIX=RSTART
CPPSCRIPTFLAGS_rstart=	-DRSHCMD=rsh -DSERVERNAME=rstartd
CPPSCRIPTFLAGS_rstartd=	-DLIBDIR=${RSTARTCONFIGDIR} -DSERVERNAME=rstartd \
			-DBINDIR=${RSTARTDBINDIR}
SCRIPTS=		rstart rstartd
SCRIPTSDIR=		${X11BINDIR}
CONFIGFILES=		config
FILESDIR=		${RSTARTCONFIGDIR}

SUBDIR=			commands contexts
TARGETS+=		configinstall

.PATH:			${X11SRCDIR.xc}/programs/rstart

.include <bsd.x11.mk>
BINDIR=			${RSTARTDBINDIR}
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
