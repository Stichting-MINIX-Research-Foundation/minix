#	$NetBSD: Makefile,v 1.5 2013/05/31 23:42:48 mrg Exp $

.include <bsd.own.mk>

PROG=	sessreg

.include "../xdm/Makefile.xdm"

CPPFLAGS+=	-I${X11SRCDIR.${PROG}}/../include

X11EXTRAMANDEFS+=	-e 's,__ttys_file__,/etc/ttys,g; \
			s,__utmp_file__,${XDMPIDDIR}/utmp,g; \
			s,__wtmp_file__,${XDMLOGDIR}/wtmp,g'

.include <bsd.x11.mk>
.include <bsd.prog.mk>

.PATH:		${X11SRCDIR.${PROG}}
.PATH:		${X11SRCDIR.${PROG}}/man
