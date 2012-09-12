#	$NetBSD: bsd.init.mk,v 1.2 2003/07/28 02:38:33 lukem Exp $

# <bsd.init.mk> includes Makefile.inc and <bsd.own.mk>; this is used at the
# top of all <bsd.*.mk> files which actually "build something".

.if !defined(_BSD_INIT_MK_)
_BSD_INIT_MK_=1

.-include "${.CURDIR}/../Makefile.inc"
.include <bsd.own.mk>
.MAIN:		all

.endif	# !defined(_BSD_INIT_MK_)
