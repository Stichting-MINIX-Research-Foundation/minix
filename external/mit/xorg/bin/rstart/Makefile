#	$NetBSD: Makefile,v 1.1 2015/07/23 07:34:38 mrg Exp $

.include <bsd.own.mk>

PROG=			rstartd.real
SRCS=			auth.c server.c
RSTARTDBINDIR=		${X11ROOTDIR}/libexec
RSTARTCONFIGDIR=	${X11ETCDIR}/rstart

CPPFLAGS.server.c=	-DSERVERNAME=\"rstartd\" \
			-DDEFAULT_CONFIG=\"${RSTARTCONFIGDIR}/config\"

MAN=			rstart.1 rstartd.1

X11EXTRAMANDEFS+=	-e 's,RSTARTCONFIGDIR,${RSTARTCONFIGDIR},g'

SEDCMD=	sed -e 's/@RSH@/ssh/' -e 's/@rstartdir@/${RSTARTCONFIGDIR}/'

rstart: rstart.in
	${SEDCMD} < ${.IMPSRC} > ${.TARGET}
rstartd: rstartd.in
	${SEDCMD} < ${.IMPSRC} > ${.TARGET}
config: config.in
	${SEDCMD} < ${.IMPSRC} > ${.TARGET}

SCRIPTS=		rstart rstartd
SCRIPTSDIR=		${X11BINDIR}
CONFIGFILES=		config
FILESDIR=		${RSTARTCONFIGDIR}

SUBDIR=			commands contexts
TARGETS+=		configinstall

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man
.PATH:	${X11SRCDIR.${PROG}}/scripts

.include <bsd.x11.mk>
BINDIR=			${RSTARTDBINDIR}
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
