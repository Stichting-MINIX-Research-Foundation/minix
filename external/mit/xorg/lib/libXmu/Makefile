#	$NetBSD: Makefile,v 1.3 2010/11/21 06:01:50 mrg Exp $

.include <bsd.own.mk>

LIB=	Xmu
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/include/X11/Xmu

SRCS=	AllCmap.c Atoms.c ClientWin.c Clip.c CloseHook.c CmapAlloc.c \
	CrCmap.c CrPixFBit.c CursorName.c CvtCache.c CvtStdSel.c \
	DefErrMsg.c DelCmap.c DisplayQue.c Distinct.c DrRndRect.c \
	DrawLogo.c EditresCom.c ExtAgent.c FToCback.c GetHost.c \
	GrayPixmap.c Initer.c LocBitmap.c Lookup.c LookupCmap.c \
	Lower.c RdBitF.c ScrOfWin.c ShapeWidg.c StdCmap.c StrToBS.c \
	StrToBmap.c StrToCurs.c StrToGrav.c StrToJust.c StrToLong.c \
	StrToOrnt.c StrToShap.c StrToWidg.c UpdMapHint.c VisCmap.c \
	WidgetNode.c Xct.c

INCS=	Atoms.h CharSet.h CloseHook.h Converters.h CurUtil.h \
	CvtCache.h DisplayQue.h Drawing.h Editres.h EditresP.h \
	Error.h ExtAgent.h Initer.h Lookup.h Misc.h StdCmap.h \
	StdSel.h SysUtil.h WidgetNode.h WinUtil.h Xct.h Xmu.h \
	WhitePoint.h
INCSDIR=${X11INCDIR}/X11/Xmu

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-DHAS_SNPRINTF
CPPFLAGS+=	-DXTSTRINGDEFINES	#XTODO is this correct?
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/Xmu

CPPFLAGS.CvtStdSel.c=	${X11FLAGS.CONNECTION}
CPPFLAGS.LocBitmap.c=	-DBITMAPDIR=\"${X11INCDIR}/X11/bitmaps\"

LIBDPLIBS=\
	Xt	${.CURDIR}/../libXt \
	SM	${.CURDIR}/../libSM \
	ICE	${.CURDIR}/../libICE \
	Xext	${.CURDIR}/../libXext \
	X11	${.CURDIR}/../libX11/dynamic

PKGCONFIG=	xmu xmuu
PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
