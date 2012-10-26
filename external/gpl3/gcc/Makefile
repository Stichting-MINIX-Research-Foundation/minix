#	$NetBSD: Makefile,v 1.1 2011/06/21 04:53:33 mrg Exp $

.if !defined(__MINIX)
SUBDIR+= lib .WAIT usr.bin
.else
SUBDIR+= lib .WAIT
.endif # !defined(__MINIX)

.include <bsd.subdir.mk>
