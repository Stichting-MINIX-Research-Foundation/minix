#	$NetBSD: Makefile,v 1.3 2010/03/26 18:15:15 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../dev/usb ${.CURDIR}/../../../../dev/bluetooth

LIB=	rumpdev_ubt
IOCONF= UBT.ioconf

SRCS=	ubt.c bthub.c

SRCS+=	ubt_at_usb.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
