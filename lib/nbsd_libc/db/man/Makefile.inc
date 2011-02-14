#	$NetBSD: Makefile.inc,v 1.10 2004/04/30 21:13:23 kleink Exp $
#       @(#)Makefile.inc	8.1 (Berkeley) 6/4/93

.PATH: ${.CURDIR}/db/man

MAN+=	btree.3 dbm_clearerr.3 dbopen.3 hash.3 recno.3 mpool.3
MLINKS+= dbm_clearerr.3 dbm_close.3 dbm_clearerr.3 dbm_delete.3
MLINKS+= dbm_clearerr.3 dbm_dirfno.3 dbm_clearerr.3 dbm_error.3
MLINKS+= dbm_clearerr.3 dbm_fetch.3 dbm_clearerr.3 dbm_firstkey.3
MLINKS+= dbm_clearerr.3 dbm_nextkey.3 dbm_clearerr.3 dbm_open.3
MLINKS+= dbm_clearerr.3 dbm_store.3 dbm_clearerr.3 ndbm.3
MLINKS+= dbopen.3 db.3
MLINKS+= mpool.3 mpool_open.3 mpool.3 mpool_filter.3 mpool.3 mpool_new.3
MLINKS+= mpool.3 mpool_get.3 mpool.3 mpool_put.3 mpool.3 mpool_sync.3
MLINKS+= mpool.3 mpool_close.3
