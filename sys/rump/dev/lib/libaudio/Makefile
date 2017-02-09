#	$NetBSD: Makefile,v 1.3 2014/11/18 04:29:06 nonaka Exp $
#

.PATH:	${.CURDIR}/../../../../dev

LIB=	rumpdev_audio
IOCONF=	AUDIO.ioconf

SRCS=	audio.c auconv.c aurateconv.c auvolconv.c mulaw.c
SRCS+=	audio_component.c

CPPFLAGS+=	-I${.CURDIR}/../../../librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
