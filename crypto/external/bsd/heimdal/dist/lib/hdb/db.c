/*	$NetBSD: db.c,v 1.2 2017/01/28 21:31:48 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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

#if defined(HAVE_DB1)

#if defined(HAVE_DB_185_H)
#include <db_185.h>
#elif defined(HAVE_DB_H)
#include <db.h>
#endif

typedef struct {
    HDB hdb;            /* generic members */
    int lock_fd;        /* DB-specific */
} DB1_HDB;

static krb5_error_code
DB_close(krb5_context context, HDB *db)
{
    DB1_HDB *db1 = (DB1_HDB *)db;
    DB *d = (DB*)db->hdb_db;

    heim_assert(d != 0, "Closing already closed HDB");

    (*d->close)(d);
    db->hdb_db = 0;

    if (db1->lock_fd >= 0) {
	close(db1->lock_fd);
	db1->lock_fd = -1;
    }

    return 0;
}

static krb5_error_code
DB_destroy(krb5_context context, HDB *db)
{
    krb5_error_code ret;

    ret = hdb_clear_master_key (context, db);
    free(db->hdb_name);
    free(db);
    return ret;
}

static krb5_error_code
DB_lock(krb5_context context, HDB *db, int operation)
{

    return 0;
}

static krb5_error_code
DB_unlock(krb5_context context, HDB *db)
{

    return 0;
}


static krb5_error_code
DB_seq(krb5_context context, HDB *db,
       unsigned flags, hdb_entry_ex *entry, int flag)
{
    DB *d = (DB*)db->hdb_db;
    DBT key, value;
    krb5_data key_data, data;
    int code;

    code = (*d->seq)(d, &key, &value, flag);
    if(code == -1) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s seq error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    if(code == 1) {
	krb5_clear_error_message(context);
	return HDB_ERR_NOENTRY;
    }

    key_data.data = key.data;
    key_data.length = key.size;
    data.data = value.data;
    data.length = value.size;
    memset(entry, 0, sizeof(*entry));
    if (hdb_value2entry(context, &data, &entry->entry))
	return DB_seq(context, db, flags, entry, R_NEXT);
    if (db->hdb_master_key_set && (flags & HDB_F_DECRYPT)) {
	code = hdb_unseal_keys (context, db, &entry->entry);
	if (code)
	    hdb_free_entry (context, entry);
    }
    if (code == 0 && entry->entry.principal == NULL) {
	entry->entry.principal = malloc(sizeof(*entry->entry.principal));
	if (entry->entry.principal == NULL) {
	    code = ENOMEM;
	    krb5_set_error_message(context, code, "malloc: out of memory");
	    hdb_free_entry (context, entry);
	} else {
	    hdb_key2principal(context, &key_data, entry->entry.principal);
	}
    }
    return code;
}


static krb5_error_code
DB_firstkey(krb5_context context, HDB *db, unsigned flags, hdb_entry_ex *entry)
{
    return DB_seq(context, db, flags, entry, R_FIRST);
}


static krb5_error_code
DB_nextkey(krb5_context context, HDB *db, unsigned flags, hdb_entry_ex *entry)
{
    return DB_seq(context, db, flags, entry, R_NEXT);
}

static krb5_error_code
DB_rename(krb5_context context, HDB *db, const char *new_name)
{
    int ret;
    char *old, *new;

    if (strncmp(new_name, "db:", sizeof("db:") - 1) == 0)
        new_name += sizeof("db:") - 1;
    else if (strncmp(new_name, "db1:", sizeof("db1:") - 1) == 0)
        new_name += sizeof("db1:") - 1;
    asprintf(&old, "%s.db", db->hdb_name);
    asprintf(&new, "%s.db", new_name);
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
    DB *d = (DB*)db->hdb_db;
    DBT k, v;
    int code;

    k.data = key.data;
    k.size = key.length;
    code = (*d->get)(d, &k, &v, 0);
    if(code < 0) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s get error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    if(code == 1) {
	krb5_clear_error_message(context);
	return HDB_ERR_NOENTRY;
    }

    krb5_data_copy(reply, v.data, v.size);
    return 0;
}

static krb5_error_code
DB__put(krb5_context context, HDB *db, int replace,
	krb5_data key, krb5_data value)
{
    DB *d = (DB*)db->hdb_db;
    DBT k, v;
    int code;

    k.data = key.data;
    k.size = key.length;
    v.data = value.data;
    v.size = value.length;
    krb5_clear_error_message(context);
    code = (*d->put)(d, &k, &v, replace ? 0 : R_NOOVERWRITE);
    if(code < 0) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s put error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    if(code == 1) {
	return HDB_ERR_EXISTS;
    }
    code = (*d->sync)(d, 0);
    if (code == -1) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s put sync error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    return 0;
}

static krb5_error_code
DB__del(krb5_context context, HDB *db, krb5_data key)
{
    DB *d = (DB*)db->hdb_db;
    DBT k;
    krb5_error_code code;
    k.data = key.data;
    k.size = key.length;
    krb5_clear_error_message(context);
    code = (*d->del)(d, &k, 0);
    if (code == 1)
        return HDB_ERR_NOENTRY;
    if (code < 0) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s del error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    code = (*d->sync)(d, 0);
    if (code == -1) {
	code = errno;
	krb5_set_error_message(context, code, "Database %s del sync error: %s",
			       db->hdb_name, strerror(code));
	return code;
    }
    return 0;
}

static DB *
_open_db(char *fn, int flags, int mode, int *fd)
{
#ifndef O_EXLOCK
    int op;
    int ret;

    *fd = open(fn, flags, mode);
    if (*fd == -1)
	return NULL;

    if ((flags & O_ACCMODE) == O_RDONLY)
	op = LOCK_SH;
    else
	op = LOCK_EX;

    ret = flock(*fd, op);
    if (ret == -1) {
	int saved_errno;

	saved_errno = errno;
	close(*fd);
	errno = saved_errno;
	return NULL;
    }
#else
    if ((flags & O_ACCMODE) == O_RDONLY)
	flags |= O_SHLOCK;
    else
	flags |= O_EXLOCK;
#endif

    return dbopen(fn, flags, mode, DB_BTREE, NULL);
}

static krb5_error_code
DB_open(krb5_context context, HDB *db, int flags, mode_t mode)
{
    DB1_HDB *db1 = (DB1_HDB *)db;
    char *fn;
    krb5_error_code ret;

    asprintf(&fn, "%s.db", db->hdb_name);
    if (fn == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    db->hdb_db = _open_db(fn, flags, mode, &db1->lock_fd);
    free(fn);
    /* try to open without .db extension */
    if(db->hdb_db == NULL && errno == ENOENT)
	db->hdb_db = _open_db(db->hdb_name, flags, mode, &db1->lock_fd);
    if(db->hdb_db == NULL) {
	krb5_set_error_message(context, errno, "dbopen (%s): %s",
			      db->hdb_name, strerror(errno));
	return errno;
    }
    if((flags & O_ACCMODE) == O_RDONLY)
	ret = hdb_check_db_format(context, db);
    else
	ret = hdb_init_db(context, db);
    if(ret == HDB_ERR_NOENTRY) {
	krb5_clear_error_message(context);
	return 0;
    }
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
hdb_db1_create(krb5_context context, HDB **db,
	       const char *filename)
{
    DB1_HDB **db1 = (DB1_HDB **)db;
    *db = calloc(1, sizeof(**db1));	/* Allocate space for the larger db1 */
    if (*db == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }

    (*db)->hdb_db = NULL;
    (*db)->hdb_name = strdup(filename);
    if ((*db)->hdb_name == NULL) {
	free(*db);
	*db = NULL;
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    (*db)->hdb_master_key_set = 0;
    (*db)->hdb_openp = 0;
    (*db)->hdb_capability_flags = HDB_CAP_F_HANDLE_ENTERPRISE_PRINCIPAL;
    (*db)->hdb_open = DB_open;
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

    (*db1)->lock_fd = -1;
    return 0;
}

#endif /* defined(HAVE_DB1) */
