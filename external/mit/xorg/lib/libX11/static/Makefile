#	$NetBSD: Makefile,v 1.2 2011/08/11 23:15:38 joerg Exp $

NOPIC=	1

.include <bsd.own.mk>

.include "../Makefile.libx11"

.if 0
SRCS+=	imCallbk.c imDefFlt.c imDefIc.c imDefIm.c imDefLkup.c imDispch.c \
	imEvToWire.c imExten.c imImSw.c imInsClbk.c imInt.c imLcFlt.c \
	imLcGIc.c imLcIc.c imLcIm.c imLcLkup.c imLcPrs.c imLcSIc.c imRm.c \
	imRmAttr.c imThaiFlt.c imThaiIc.c imThaiIm.c imTrX.c imTrans.c \
	imTransR.c lcEuc.c lcGenConv.c lcJis.c lcSjis.c lcUTF8Load.c \
	omDefault.c omGeneric.c omImText.c omText.c omTextEsc.c omTextExt.c \
	omTextPer.c omXChar.c
.endif

SRCS+=	\
	XlcDL.c \
	XlcSL.c

CWARNFLAGS.clang+=	-Wno-parentheses

.include <bsd.x11.mk>
.include <bsd.lib.mk>
