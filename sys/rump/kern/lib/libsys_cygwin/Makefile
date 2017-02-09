#	$NetBSD: Makefile,v 1.4 2015/05/09 12:03:10 pooka Exp $
#

LIB=	rumpkern_sys_cygwin

SRCS=	rump_cygwin_compat.c
SRCS+=	sys_cygwin_component.c

# XXX
CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern

SYS_P=rump_cygwin_sys
.include "../Makefile.sys"

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
