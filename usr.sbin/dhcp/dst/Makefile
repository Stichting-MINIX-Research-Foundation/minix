# $NetBSD: Makefile,v 1.8 2008/05/03 14:48:32 lukem Exp $

LIBISPRIVATE=	yes

LIB    = dst
SRCS    = dst_support.c dst_api.c hmac_link.c md5_dgst.c base64.c prandom.c

DHCPSRCDIR=	dst

CFLAGS += -DMINIRES_LIB -DHMAC_MD5

.include <bsd.lib.mk>
