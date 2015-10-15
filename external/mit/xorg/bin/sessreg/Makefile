#	$NetBSD: Makefile,v 1.6 2014/08/24 18:27:16 apb Exp $

.include <bsd.own.mk>

PROG=	sessreg

.include "../xdm/Makefile.xdm"

CPPFLAGS+=	-I${X11SRCDIR.${PROG}}/../include

X11EXTRAMANDEFS+=	-e 's,__ttys_file__,/etc/ttys,g' \
			-e 's,__utmp_file__,${XDMPIDDIR}/utmp,g' \
			-e 's,__wtmp_file__,${XDMLOGDIR}/wtmp,g'

.include <bsd.x11.mk>
.include <bsd.prog.mk>

.PATH:		${X11SRCDIR.${PROG}}
.PATH:		${X11SRCDIR.${PROG}}/man
