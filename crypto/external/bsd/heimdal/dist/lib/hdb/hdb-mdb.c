/*	$NetBSD: hdb-mdb.c,v 1.2 2017/01/28 21:31:48 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * Copyright (c) 2011 - Howard Chu, Symas Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hdb_locl.h"

#if HAVE_LMDB

/* LMDB */

#include <lmdb.h>

#define	KILO	1024

typedef struct mdb_info {
    MDB_env *e;
    MDB_txn *t;
    MDB_dbi d;
    MDB_cursor *c;
} mdb_info;

static krb5_error_code
DB_close(krb5_context context, HDB *db)
{
    mdb_info *mi = (mdb_info *)db->hdb_db;

    mdb_cursor_close(mi->c);
    mdb_txn_abort(mi->t);
    mdb_env_close(mi->e);
    mi->c = 0;
    mi->t = 0;
    mi->e = 0;
    return 0;
}

static krb5_error_code
DB_destroy(krb5_context context, HDB *db)
{
    krb5_error_code ret;

    ret = hdb_clear_master_key (context, db);
    free(db->hdb_name);
    free(db->hdb_db);
    free(db);
    return ret;
}

static krb5_error_code
DB_lock(krb5_context context, HDB *db, int operation)
{
    db->lock_count++;
    return 0;
}

static krb5_error_code
DB_unlock(krb5_context context, HDB *db)
{
    if (db->lock_count > 1) {
	db->lock_count--;
	return 0;
    }
    heim_assert(db->lock_count == 1, "HDB lock/unlock sequence does not match");
    db->lock_count--;
    return 0;
}


static krb5_error_code
DB_seq(krb5_context context, HDB *db,
       unsigned flags, hdb_entry_ex *entry, int flag)
{
    mdb_info *mi = db->hdb_db;
    MDB_val key, value;
    krb5_data key_data, data;
    int code;

    key.mv_size = 0;
    value.mv_size = 0;
    code = mdb_cursor_get(mi->c, &key, &value, flag);
    if (code == MDB_NOTFOUND)
	return HDB_ERR_NOENTRY;
    if (code)
	return code;

    key_data.data = key.mv_data;
    key_data.length = key.mv_size;
    data.data = value.mv_data;
    data.length = value.mv_size;
    memset(entry, 0, sizeof(*entry));
    if (hdb_value2entry(context, &data, &entry->entry))
	return DB_seq(context, db, flags, entry, MDB_NEXT);
    if (db->hdb_master_key_set && (flags & HDB_F_DECRYPT)) {
	code = hdb_unseal_keys (context, db, &entry->entry);
	if (code)
	    hdb_free_entry (context, entry);
    }
    if (entry->entry.principal == NULL) {
	entry->entry.principal = malloc(sizeof(*entry->entry.principal));
	if (entry->entry.principal == NULL) {
	    hdb_free_entry (context, entry);
	    krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	    return ENOMEM;
	} else {
	    hdb_key2principal(context, &key_data, entry->entry.principal);
	}
    }
    return 0;
}


static krb5_error_code
DB_firstkey(krb5_context context, HDB *db, unsigned flags, hdb_entry_ex *entry)
{
    mdb_info *mi = db->hdb_db;
    int code;

    /* Always start with a fresh cursor to pick up latest DB state */
    if (mi->t)
	mdb_txn_abort(mi->t);

    code = mdb_txn_begin(mi->e, NULL, MDB_RDONLY, &mi->t);
    if (code)
	return code;

    code = mdb_cursor_open(mi->t, mi->d, &mi->c);
    if (code)
	return code;

    return DB_seq(context, db, flags, entry, MDB_FIRST);
}


static krb5_error_code
DB_nextkey(krb5_context context, HDB *db, unsigned flags, hdb_entry_ex *entry)
{
    return DB_seq(context, db, flags, entry, MDB_NEXT);
}

static krb5_error_code
DB_rename(krb5_context context, HDB *db, const char *new_name)
{
    int ret;
    char *old, *new;

    if (strncmp(new_name, "mdb:", sizeof("mdb:") - 1) == 0)
        new_name += sizeof("mdb:") - 1;
    else if (strncmp(new_name, "lmdb:", sizeof("lmdb:") - 1) == 0)
        new_name += sizeof("lmdb:") - 1;
    if (asprintf(&old, "%s.mdb", db->hdb_name) == -1)
		return ENOMEM;
    if (asprintf(&new, "%s.mdb", new_name) == -1) {
		free(old);
		return ENOMEM;
    }
    ret = rename(old, new);
    free(old);
    free(new);
    if(ret)
	return errno;

    free(db->hdb_name);
    db->hdb_name = strdup(new_name);
    return 0;
}

static krb5_error_code
DB__get(krb5_context context, HDB *db, krb5_data key, krb5_data *reply)
{
    mdb_info *mi = (mdb_info*)db->hdb_db;
    MDB_txn *txn;
    MDB_val k, v;
    int code;

    k.mv_data = key.data;
    k.mv_size = key.length;

    code = mdb_txn_begin(mi->e, NULL, MDB_RDONLY, &txn);
    if (code)
	return code;

    code = mdb_get(txn, mi->d, &k, &v);
    if (code == 0)
	krb5_data_copy(reply, v.mv_data, v.mv_size);
    mdb_txn_abort(txn);
    if(code == MDB_NOTFOUND)
	return HDB_ERR_NOENTRY;
    return code;
}

static krb5_error_code
DB__put(krb5_context context, HDB *db, int replace,
	krb5_data key, krb5_data value)
{
    mdb_info *mi = (mdb_info*)db->hdb_db;
    MDB_txn *txn;
    MDB_val k, v;
    int code;

    k.mv_data = key.data;
    k.mv_size = key.length;
    v.mv_data = value.data;
    v.mv_size = value.length;

    code = mdb_txn_begin(mi->e, NULL, 0, &txn);
    if (code)
	return code;

    code = mdb_put(txn, mi->d, &k, &v, replace ? 0 : MDB_NOOVERWRITE);
    if (code)
	mdb_txn_abort(txn);
    else
	code = mdb_txn_commit(txn);
    if(code == MDB_KEYEXIST)
	return HDB_ERR_EXISTS;
    return code;
}

static krb5_error_code
DB__del(krb5_context context, HDB *db, krb5_data key)
{
    mdb_info *mi = (mdb_info*)db->hdb_db;
    MDB_txn *txn;
    MDB_val k;
    krb5_error_code code;

    k.mv_data = key.data;
    k.mv_size = key.length;

    code = mdb_txn_begin(mi->e, NULL, 0, &txn);
    if (code)
	return code;

    code = mdb_del(txn, mi->d, &k, NULL);
    if (code)
	mdb_txn_abort(txn);
    else
	code = mdb_txn_commit(txn);
    if(code == MDB_NOTFOUND)
	return HDB_ERR_NOENTRY;
    return code;
}

static krb5_error_code
DB_open(krb5_context context, HDB *db, int flags, mode_t mode)
{
    mdb_info *mi = (mdb_info *)db->hdb_db;
    MDB_txn *txn;
    char *fn;
    krb5_error_code ret;
    int myflags = MDB_NOSUBDIR, tmp;

    if((flags & O_ACCMODE) == O_RDONLY)
      myflags |= MDB_RDONLY;

    if (asprintf(&fn, "%s.mdb", db->hdb_name) == -1)
	return krb5_enomem(context);
    if (mdb_env_create(&mi->e)) {
	free(fn);
	return krb5_enomem(context);
    }

    tmp = krb5_config_get_int_default(context, NULL, 0, "kdc",
	"hdb-mdb-maxreaders", NULL);
    if (tmp) {
	ret = mdb_env_set_maxreaders(mi->e, tmp);
	if (ret) {
            free(fn);
	    krb5_set_error_message(context, ret, "setting maxreaders on %s: %s",
		db->hdb_name, mdb_strerror(ret));
	    return ret;
	}
    }

    tmp = krb5_config_get_int_default(context, NULL, 0, "kdc",
	"hdb-mdb-mapsize", NULL);
    if (tmp) {
	size_t maps = tmp;
	maps *= KILO;
	ret = mdb_env_set_mapsize(mi->e, maps);
	if (ret) {
            free(fn);
	    krb5_set_error_message(context, ret, "setting mapsize on %s: %s",
		db->hdb_name, mdb_strerror(ret));
	    return ret;
	}
    }

    ret = mdb_env_open(mi->e, fn, myflags, mode);
    free(fn);
    if (ret) {
fail:
	mdb_env_close(mi->e);
	mi->e = 0;
	krb5_set_error_message(context, ret, "opening %s: %s",
			      db->hdb_name, mdb_strerror(ret));
	return ret;
    }

    ret = mdb_txn_begin(mi->e, NULL, MDB_RDONLY, &txn);
    if (ret)
	goto fail;

    ret = mdb_open(txn, NULL, 0, &mi->d);
    mdb_txn_abort(txn);
    if (ret)
	goto fail;

    if((flags & O_ACCMODE) == O_RDONLY)
	ret = hdb_check_db_format(context, db);
    else
	ret = hdb_init_db(context, db);
    if(ret == HDB_ERR_NOENTRY)
	return 0;
    if (ret) {
	DB_close(context, db);
	krb5_set_error_message(context, ret, "hdb_open: failed %s database %s",
			       (flags & O_ACCMODE) == O_RDONLY ?
			       "checking format of" : "initialize",
			       db->hdb_name);
    }

    return ret;
}

krb5_error_code
hdb_mdb_create(krb5_context context, HDB **db,
	      const char *filename)
{
    *db = calloc(1, sizeof(**db));
    if (*db == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }

    (*db)->hdb_db = calloc(1, sizeof(mdb_info));
    if ((*db)->hdb_db == NULL) {
	free(*db);
	*db = NULL;
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    (*db)->hdb_name = strdup(filename);
    if ((*db)->hdb_name == NULL) {
	free((*db)->hdb_db);
	free(*db);
	*db = NULL;
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    (*db)->hdb_master_key_set = 0;
    (*db)->hdb_openp = 0;
    (*db)->hdb_capability_flags = HDB_CAP_F_HANDLE_ENTERPRISE_PRINCIPAL;
    (*db)->hdb_open  = DB_open;
    (*db)->hdb_close = DB_close;
    (*db)->hdb_fetch_kvno = _hdb_fetch_kvno;
    (*db)->hdb_store = _hdb_store;
    (*db)->hdb_remove = _hdb_remove;
    (*db)->hdb_firstkey = DB_firstkey;
    (*db)->hdb_nextkey= DB_nextkey;
    (*db)->hdb_lock = DB_lock;
    (*db)->hdb_unlock = DB_unlock;
    (*db)->hdb_rename = DB_rename;
    (*db)->hdb__get = DB__get;
    (*db)->hdb__put = DB__put;
    (*db)->hdb__del = DB__del;
    (*db)->hdb_destroy = DB_destroy;
    return 0;
}
#endif /* HAVE_LMDB */
