#	$NetBSD: Makefile.inc,v 1.9 2005/09/13 01:44:09 christos Exp $
#       @(#)Makefile.inc	8.1 (Berkeley) 6/4/93

.PATH: ${.CURDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbmdatum.c ndbm.c
