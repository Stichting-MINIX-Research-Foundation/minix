#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:23 mrg Exp $

.include <bsd.own.mk>

ENCDIR=	encodings/large

GZFILES= big5.eten-0.enc.gz big5hkscs-0.enc.gz \
	cns11643-1.enc.gz cns11643-2.enc.gz cns11643-3.enc.gz \
	gb18030-0.enc.gz gb18030.2000-0.enc.gz gb18030.2000-1.enc.gz gb2312.1980-0.enc.gz \
	gbk-0.enc.gz \
	jisx0201.1976-0.enc.gz jisx0208.1990-0.enc.gz jisx0212.1990-0.enc.gz \
	ksc5601.1987-0.enc.gz ksc5601.1992-3.enc.gz \
	sun.unicode.india-0.enc.gz

.PATH:  	${X11SRCDIR.encodings}/large

.include "../Makefile.enc"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
