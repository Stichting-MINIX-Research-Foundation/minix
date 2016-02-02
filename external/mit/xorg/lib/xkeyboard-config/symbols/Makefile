#	$NetBSD: Makefile,v 1.10 2014/07/11 19:19:16 mrg Exp $

.include <bsd.own.mk>

XKBNAME=	symbols

FILES=	af al altwin am apl ara at az \
	ba bd be bg br brai bt bw by \
	ca capslock cd ch cm cn compose ctrl cz \
	de dk \
	ee empty epo es et eu eurosign \
	fi fo fr \
	gb ge gh gn gr group \
	hr hu \
	ie il in inet iq ir is it \
	jp \
	ke keypad kg kh kpdl kr kz \
	la latam latin level3 level5 lk lt lv \
	ma mao md me mk ml mm mn mt mv \
	nbsp ng nl no np \
	olpc \
	pc ph pk pl pt \
	ro rs ru rupeesign \
	se shift si sk sn srvr_ctrl sy \
	terminate th tj tm tr tw typo tz \
	ua us uz \
	vn \
	za

SUBDIR=		digital_vndr fujitsu_vndr hp_vndr macintosh_vndr \
		nec_vndr nokia_vndr sgi_vndr sharp_vndr sony_vndr sun_vndr \
		xfree68_vndr

XKBDIR=		1

.include "../Makefile.xkbdata"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
