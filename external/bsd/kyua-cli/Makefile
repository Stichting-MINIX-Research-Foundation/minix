# $NetBSD: Makefile,v 1.2 2013/02/24 03:06:15 jmmv Exp $

SUBDIR= etc lib .WAIT share tests usr.bin

.for dir in share tests usr.bin
includes-${dir}: .PHONY
	@true
.endfor

.include <bsd.subdir.mk>
