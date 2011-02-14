#	$NetBSD: Makefile.inc,v 1.1 2010/04/25 00:54:46 joerg Exp $

# Constant database reader/writer

.PATH:	${.CURDIR}/cdb

SRCS+=		cdbr.c cdbw.c

MAN+=		cdbr.3 cdbw.3 cdb.5

MLINKS+=	cdbr.3 cdbr_open.3
MLINKS+=	cdbr.3 cdbr_entries.3
MLINKS+=	cdbr.3 cdbr_get.3
MLINKS+=	cdbr.3 cdbr_find.3
MLINKS+=	cdbr.3 cdbr_close.3
MLINKS+=	cdbw.3 cdbw_open.3
MLINKS+=	cdbw.3 cdbw_put.3
MLINKS+=	cdbw.3 cdbw_put_data.3
MLINKS+=	cdbw.3 cdbw_put_key.3
MLINKS+=	cdbw.3 cdbw_output.3
MLINKS+=	cdbw.3 cdbw_close.3
