#	$NetBSD: Makefile.inc,v 1.9 2012/06/19 06:10:09 dholland Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
.if defined(__MINIX)
CPPFLAGS+= -D__NBSD_LIBC=1
USE_FORT?= no
.else
USE_FORT?= yes		# network server
.endif

BINDIR?=	/usr/libexec
WARNS?=		5
