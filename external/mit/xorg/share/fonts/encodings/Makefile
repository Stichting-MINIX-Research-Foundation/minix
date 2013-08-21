#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:23 mrg Exp $

.include <bsd.own.mk>

ENCDIR=	encodings
ENCARGS=-e large

GZFILES=adobe-dingbats.enc.gz adobe-standard.enc.gz adobe-symbol.enc.gz \
	armscii-8.enc.gz ascii-0.enc.gz dec-special.enc.gz \
	ibm-cp437.enc.gz ibm-cp850.enc.gz ibm-cp852.enc.gz ibm-cp866.enc.gz \
	iso8859-11.enc.gz iso8859-13.enc.gz iso8859-16.enc.gz iso8859-6.16.enc.gz \
	iso8859-6.8x.enc.gz \
	microsoft-cp1250.enc.gz microsoft-cp1251.enc.gz microsoft-cp1252.enc.gz \
	microsoft-cp1253.enc.gz microsoft-cp1254.enc.gz microsoft-cp1255.enc.gz \
	microsoft-cp1256.enc.gz microsoft-cp1257.enc.gz microsoft-cp1258.enc.gz \
	microsoft-win3.1.enc.gz \
	mulearabic-0.enc.gz mulearabic-1.enc.gz mulearabic-2.enc.gz \
	mulelao-1.enc.gz suneu-greek.enc.gz tcvn-0.enc.gz tis620-2.enc.gz \
	viscii1.1-1.enc.gz

SUBDIR=	large

.include "Makefile.enc"

.PATH:  	${X11SRCDIR.encodings}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
