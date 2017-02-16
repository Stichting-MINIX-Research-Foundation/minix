# $NetBSD: Makefile,v 1.1 2013/03/24 15:54:31 christos Exp $

LIBISPRIVATE=	yes

LIB    = dst
SRCS    = dst_support.c dst_api.c hmac_link.c md5_dgst.c base64.c prandom.c

DHCPSRCDIR=	dst

CFLAGS += -DMINIRES_LIB -DHMAC_MD5

.include <bsd.lib.mk>
