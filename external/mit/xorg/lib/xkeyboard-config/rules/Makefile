#	$NetBSD: Makefile,v 1.3 2010/05/29 07:58:24 ghen Exp $
#
#	When upgrading, re-assemble the files base, base.lst, evdev and
#	evdev.lst by running ./configure && make in the dist directory,
#	and copy the resulting files back here.

.include <bsd.own.mk>

XKBNAME=	rules
FILES=		README base base.lst base.extras.xml.in base.xml.in \
		evdev evdev.lst evdev.extras.xml.in evdev.xml.in \
		xfree98 xkb.dtd

FILESNAME_base.xml.in=		base.xml
FILESNAME_base.extras.xml.in=	base.extras.xml
FILESNAME_evdev.xml.in=		evdev.xml
FILESNAME_evdev.extras.xml.in=	evdev.extras.xml

SYMLINKS=	base ${FILESDIR}/xorg \
		base.lst ${FILESDIR}/xorg.lst \
		base.xml ${FILESDIR}/xorg.xml

.include "../Makefile.xkbdata"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
