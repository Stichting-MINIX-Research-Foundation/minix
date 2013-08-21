#	$NetBSD: Makefile,v 1.3 2009/09/07 21:07:03 mrg Exp $

SUBDIR=	conf.d conf.avail

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.fontconfig}
CONFIGFILES= \
	fonts.conf \
	fonts.dtd

FILESDIR= \
	/etc/fonts

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
