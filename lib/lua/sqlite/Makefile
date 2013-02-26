#	$NetBSD: Makefile,v 1.3 2012/03/03 11:32:14 mbalmer Exp $

LUA_MODULES=		sqlite
LUA_SRCS.sqlite=	sqlite.c

LUA_DPLIBS+=		sqlite3 \
			${.CURDIR}/../../../external/public-domain/sqlite/lib

.include <bsd.lua.mk>
