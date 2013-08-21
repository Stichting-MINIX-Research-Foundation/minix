#	$NetBSD: Makefile,v 1.2 2011/08/11 23:15:35 joerg Exp $

.include <bsd.own.mk>

PROG=	fonttosfnt
SRCS=	fonttosfnt.c read.c write.c struct.c util.c

# XXX XVENDORNAME and XVENDORNAMESHORT should live in a common location
CPPFLAGS+=	-DXFREE86_FT2 -I${DESTDIR}${X11INCDIR}/freetype2 \
		-DXVENDORNAMESHORT=${XVENDORNAMESHORT} \
		-DXVENDORNAME=${XVENDORNAME}
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/fonts \
		-I${X11SRCDIR.xc}/extras/freetype2/include


LDADD+=	-lfreetype -lfontenc -lz -lm
DPADD+=	 ${LIBFREETYPE} ${LIBFONTENC} ${LIBZ} ${LIBM}

.PATH:	${X11SRCDIR.${PROG}}

CWARNFLAGS.clang+=	-Wno-format

.include <bsd.x11.mk>
.include <bsd.prog.mk>
