#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

PROG=	mkcfm
SRCS=	mkcfm.c arith.c curves.c fontfcn.c hints.c lines.c objects.c \
	paths.c regions.c scanfont.c spaces.c t1funcs.c t1info.c \
	t1io.c t1malloc.c t1snap.c t1stub.c token.c type1.c util.c \
	afm.c cidchar.c

CPPFLAGS+=	-DBUILDCID -DCID_ALL_CHARS
CPPFLAGS+=	-I${X11SRCDIR.xc}/lib/font/Type1 \
		-I${X11SRCDIR.xc}/include/fonts \
		-I${X11SRCDIR.xc}/lib/font/include \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${X11SRCDIR.xc}/programs/Xserver/include

LDADD+=	-lm
DPADD+=	${LIBM}

.PATH:	${X11SRCDIR.xc}/programs/${PROG}
.PATH:	${X11SRCDIR.xc}/lib/font/Type1

.include <bsd.x11.mk>
.include <bsd.prog.mk>
