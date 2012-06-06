#	$NetBSD: Makefile,v 1.4 2011/10/17 14:20:54 chs Exp $

REQUIRETOOLS=	yes
NOLINT=		# defined
NOPIC=		# defined
NOPROFILE=	# defined

LIB=		gcc_eh

.include <bsd.own.mk>

.if ${MKGCC} != "no"

.cc: # disable .cc->NULL transform

SRCS+=		${LIB2_EH}
COPTS.unwind-dw2.c = -Wno-stack-protector

MKPIC:=	no

.include <bsd.lib.mk>

.else
.include <bsd.prog.mk> # do nothing
.endif
