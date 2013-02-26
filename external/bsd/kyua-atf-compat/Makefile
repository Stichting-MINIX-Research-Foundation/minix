# $NetBSD: Makefile,v 1.1 2013/02/25 00:20:08 jmmv Exp $

SUBDIR= share tests usr.bin

.for dir in share tests usr.bin
includes-${dir}: .PHONY
	@true
.endfor

.include <bsd.subdir.mk>
