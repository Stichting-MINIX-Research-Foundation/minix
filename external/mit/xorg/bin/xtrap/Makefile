#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

SUBDIR=	xtrapinfo xtrapout xtrapreset xtrapstats xtrapin xtrapproto xtrapchar

MAN=	xtrap.1

.for _M in ${SUBDIR}
MLINKS+=xtrap.1 ${_M}.1
.endfor

.PATH:	${X11SRCDIR.xtrap}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
