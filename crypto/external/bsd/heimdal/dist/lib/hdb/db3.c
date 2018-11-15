/*	$NetBSD: db3.c,v 1.2 2017/01/28 21:31:48 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2006 Kungliga Tekniska Högskolan
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

#include <fcntl.h>

#if HAVE_DB3

#ifdef HAVE_DBHEADER
#include <db.h>
#elif HAVE_DB6_DB_H
#include <db6/db.h>
#elif HAVE_DB5_DB_H
#include <db5/db.h>
#elif HAVE_DB4_DB_H
#include <db4/db.h>
#elif HAVE_DB3_DB_H
#include <db3/db.h>
#else
#include <db.h>
#endif

typedef struct {
    HDB hdb;            /* generic members */
    int lock_fd;        /* DB3-specific */
} DB3_HDB;


static krb5_error_code
DB_close(krb5_context context, HDB *db)
{
    DB3_HDB *db3 = (DB3_HDB *)db;
    DB *d = (DB*)db->hdb_db;
    DBC *dbcp = (DBC*)db->hdb_dbc;

    heim_assert(d != 0, "Closing already closed HDB");

    if (dbcp != NULL)
	dbcp->c_close(dbcp);
    if (d != NULL)
	d->close(d, 0);
    if (db3->lock_fd >= 0)
	close(db3->lock_fd);

    db3->lock_fd = -1;
    db->hdb_dbc = 0;
    db->hdb_db = 0;

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
    DBT key, value;
    DBC *dbcp = db->hdb_dbc;
    krb5_data key_data, data;
    int code;

    memset(&key, 0, sizeof(DBT));
    memset(&value, 0, sizeof(DBT));
    code = (*dbcp->c_get)(dbcp, &key, &value, flag);
    if (code == DB_NOTFOUND)
	return HDB_ERR_NOENTRY;
    if (code)
	return code;

    key_data.data = key.data;
    key_data.length = key.size;
    data.data = value.data;
    data.length = value.size;
    memset(entry, 0, sizeof(*entry));
    if (hdb_value2entry(context, &data, &entry->entry))
	return DB_seq(context, db, flags, entry, DB_NEXT);
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
    return DB_seq(context, db, flags, entry, DB_FIRST);
}


static krb5_error_code
DB_nextkey(krb5_context context, HDB *db, unsigned flags, hdb_entry_ex *entry)
{
    return DB_seq(context, db, flags, entry, DB_NEXT);
}

static krb5_error_code
DB_rename(krb5_context context, HDB *db, const char *new_name)
{
    int ret;
    char *old, *new;

    if (strncmp(new_name, "db:", sizeof("db:") - 1) == 0)
        new_name += sizeof("db:") - 1;
    else if (strncmp(new_name, "db3:", sizeof("db3:") - 1) == 0)
        new_name += sizeof("db3:") - 1;

    ret = asprintf(&old, "%s.db", db->hdb_name);
    if (ret == -1)
	return ENOMEM;
    ret = asprintf(&new, "%s.db", new_name);
    if (ret == -1) {
	free(old);
	return ENOMEM;
    }
    ret = rename(old, new);
    free(old);
    if(ret) {
	free(new);
	return errno;
    }

    free(db->hdb_name);
    new[strlen(new) - 3] = '\0';
    db->hdb_name = new;
    return 0;
}

static krb5_error_code
DB__get(krb5_context context, HDB *db, krb5_data key, krb5_data *reply)
{
    DB *d = (DB*)db->hdb_db;
    DBT k, v;
    int code;

    memset(&k, 0, sizeof(DBT));
    memset(&v, 0, sizeof(DBT));
    k.data = key.data;
    k.size = key.length;
    k.flags = 0;
    code = (*d->get)(d, NULL, &k, &v, 0);
    if(code == DB_NOTFOUND)
	return HDB_ERR_NOENTRY;
    if(code)
	return code;

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

    memset(&k, 0, sizeof(DBT));
    memset(&v, 0, sizeof(DBT));
    k.data = key.data;
    k.size = key.length;
    k.flags = 0;
    v.data = value.data;
    v.size = value.length;
    v.flags = 0;
    code = (*d->put)(d, NULL, &k, &v, replace ? 0 : DB_NOOVERWRITE);
    if(code == DB_KEYEXIST)
	return HDB_ERR_EXISTS;
    if (code) {
        /*
         * Berkeley DB 3 and up have a terrible error reporting
         * interface...
         *
         * DB->err() doesn't output a string.
         * DB->set_errcall()'s callback function doesn't have a void *
         * argument that can be used to place the error somewhere.
         *
         * The only thing we could do is fopen()/fdopen() a file, set it
         * with DB->set_errfile(), then call DB->err(), then read the
         * message from the file, unset it with DB->set_errfile(), close
         * it and delete it.  That's a lot of work... so we don't do it.
         */
        if (code == EACCES || code == ENOSPC || code == EINVAL) {
            krb5_set_error_message(context, code,
                                   "Database %s put error: %s",
                                   db->hdb_name, strerror(code));
        } else {
            code = HDB_ERR_UK_SERROR;
            krb5_set_error_message(context, code,
                                   "Database %s put error: unknown (%d)",
                                   db->hdb_name, code);
        }
	return code;
    }
    code = (*d->sync)(d, 0);
    if (code) {
        if (code == EACCES || code == ENOSPC || code == EINVAL) {
            krb5_set_error_message(context, code,
                                   "Database %s put sync error: %s",
                                   db->hdb_name, strerror(code));
        } else {
            code = HDB_ERR_UK_SERROR;
            krb5_set_error_message(context, code,
                                   "Database %s put sync error: unknown (%d)",
                                   db->hdb_name, code);
        }
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
    memset(&k, 0, sizeof(DBT));
    k.data = key.data;
    k.size = key.length;
    k.flags = 0;
    code = (*d->del)(d, NULL, &k, 0);
    if(code == DB_NOTFOUND)
	return HDB_ERR_NOENTRY;
    if (code) {
        if (code == EACCES || code == ENOSPC || code == EINVAL) {
            krb5_set_error_message(context, code,
                                   "Database %s del error: %s",
                                   db->hdb_name, strerror(code));
        } else {
            code = HDB_ERR_UK_SERROR;
            krb5_set_error_message(context, code,
                                   "Database %s del error: unknown (%d)",
                                   db->hdb_name, code);
        }
	return code;
    }
    code = (*d->sync)(d, 0);
    if (code) {
        if (code == EACCES || code == ENOSPC || code == EINVAL) {
            krb5_set_error_message(context, code,
                                   "Database %s del sync error: %s",
                                   db->hdb_name, strerror(code));
        } else {
            code = HDB_ERR_UK_SERROR;
            krb5_set_error_message(context, code,
                                   "Database %s del sync error: unknown (%d)",
                                   db->hdb_name, code);
        }
        return code;
    }
    return 0;
}

#define RD_CACHE_SZ 0x8000     /* Minimal read cache size */
#define WR_CACHE_SZ 0x8000     /* Minimal write cache size */

static int
_open_db(DB *d, char *fn, int myflags, int flags, mode_t mode, int *fd)
{
    int ret;
    int cache_size = (myflags & DB_RDONLY) ? RD_CACHE_SZ : WR_CACHE_SZ;

    *fd = open(fn, flags, mode);

    if (*fd == -1)
       return errno;

    /*
     * Without DB_FCNTL_LOCKING, the DB library complains when initializing
     * a database in an empty file. Since the database is our lock file,
     * we create it before Berkeley DB does, so a new DB always starts empty.
     */
    myflags |= DB_FCNTL_LOCKING;

    ret = flock(*fd, (myflags&DB_RDONLY) ? LOCK_SH : LOCK_EX);
    if (ret == -1) {
	ret = errno;
	close(*fd);
	*fd = -1;
	return ret;
    }

    d->set_cachesize(d, 0, cache_size, 0);

#if (DB_VERSION_MAJOR > 4) || ((DB_VERSION_MAJOR == 4) && (DB_VERSION_MINOR >= 1))
    ret = (*d->open)(d, NULL, fn, NULL, DB_BTREE, myflags, mode);
#else
    ret = (*d->open)(d, fn, NULL, DB_BTREE, myflags, mode);
#endif

    if (ret != 0) {
	close(*fd);
	*fd = -1;
    }

    return ret;
}

static krb5_error_code
DB_open(krb5_context context, HDB *db, int flags, mode_t mode)
{
    DB3_HDB *db3 = (DB3_HDB *)db;
    DBC *dbc = NULL;
    char *fn;
    krb5_error_code ret;
    DB *d;
    int myflags = 0;
    int aret;

    heim_assert(db->hdb_db == 0, "Opening already open HDB");

    if (flags & O_CREAT)
      myflags |= DB_CREATE;

    if (flags & O_EXCL)
      myflags |= DB_EXCL;

    if((flags & O_ACCMODE) == O_RDONLY)
      myflags |= DB_RDONLY;

    if (flags & O_TRUNC)
      myflags |= DB_TRUNCATE;

    aret = asprintf(&fn, "%s.db", db->hdb_name);
    if (aret == -1) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }

    if (db_create(&d, NULL, 0) != 0) {
	free(fn);
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }
    db->hdb_db = d;

    /* From here on out always DB_close() before returning on error */

    ret = _open_db(d, fn, myflags, flags, mode, &db3->lock_fd);
    free(fn);
    if (ret == ENOENT) {
	/* try to open without .db extension */
	ret = _open_db(d, db->hdb_name, myflags, flags, mode, &db3->lock_fd);
    }

    if (ret) {
	DB_close(context, db);
	krb5_set_error_message(context, ret, "opening %s: %s",
			       db->hdb_name, strerror(ret));
	return ret;
    }

#ifndef DB_CURSOR_BULK
# define DB_CURSOR_BULK 0	/* Missing with DB < 4.8 */
#endif
    ret = (*d->cursor)(d, NULL, &dbc, DB_CURSOR_BULK);

    if (ret) {
	DB_close(context, db);
	krb5_set_error_message(context, ret, "d->cursor: %s", strerror(ret));
        return ret;
    }
    db->hdb_dbc = dbc;

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
hdb_db3_create(krb5_context context, HDB **db,
	       const char *filename)
{
    DB3_HDB **db3 = (DB3_HDB **)db;
    *db3 = calloc(1, sizeof(**db3));    /* Allocate space for the larger db3 */
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

    (*db3)->lock_fd = -1;
    return 0;
}
#endif /* HAVE_DB3 */
