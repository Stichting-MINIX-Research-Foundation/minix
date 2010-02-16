#	$NetBSD: bsd.init.mk,v 1.2 2003/07/28 02:38:33 lukem Exp $

# <minix.init.mk> includes Makefile.inc and <minix.own.mk>; this is used at the
# top of all <minix.*.mk> files which actually "build something".

.if !defined(_MINIX_INIT_MK_)
_MINIX_INIT_MK_=1

.-include "${.CURDIR}/../Makefile.inc"
.include <minix.own.mk>
.MAIN:		all

.endif	# !defined(_MINIX_INIT_MK_)
