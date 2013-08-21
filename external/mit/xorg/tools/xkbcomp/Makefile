#	$NetBSD: Makefile,v 1.4 2013/05/31 23:43:05 mrg Exp $

NOMAN=		1

.include <bsd.own.mk>

HOSTPROG=	xkbcomp
.PATH:		${X11SRCDIR.xkbcomp}
.PATH:		${X11SRCDIR.X11}/src

SRCS=		xkbcomp.c xkbscan.c expr.c vmod.c indicators.c misc.c \
		keymap.c keycodes.c keytypes.c compat.c action.c alias.c \
		symbols.c geometry.c xkbpath.c listing.c \
		xkbparse.y parseutils.c utils.c

SRCS+=		xkbcomp-stubs.c xkbcomp-KeyBind.c

.PATH:		${X11SRCDIR.xkbfile}/src
SRCS+=		xkbatom.c xkberrs.c xkbout.c xkmread.c xkbtext.c \
		cout.c xkmout.c

SRCS+=		xkbfile-xkbmisc.c

.PATH:		${X11SRCDIR.X11}/src/xkb
SRCS+=		KeysymStr.c StrKeysym.c Quarks.c Xrm.c
SRCS+=		XKB.c XKBAlloc.c XKBGAlloc.c XKBMAlloc.c XKBMisc.c \
		XKBGeom.c XKBRdBuf.c 

HOST_CPPFLAGS+=	-I${DESTDIR}${X11INCDIR} \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I. \
		-I${X11SRCDIR.X11}/src \
		-I${X11SRCDIR.X11}/src/xkb \
		-I${X11SRCDIR.xkbfile}/include/X11/extensions \
		-I${X11SRCDIR.${HOSTPROG}} \
		-DDFLT_XKB_CONFIG_ROOT=\"${X11LIBDIR}/xkb\" -DXKB \
		-DHAVE_STRCASECMP -DHAVE_STRDUP \
		-DPACKAGE_STRING="\"NetBSD tool xkbcomp 1.2.4\"" \
		-DPACKAGE_VERSION="\"1.2.4\""


.include "${NETBSDSRCDIR}/external/mit/xorg/tools/makekeys/Makefile.makekeys"

.include <bsd.x11.mk>
.include <bsd.hostprog.mk>
