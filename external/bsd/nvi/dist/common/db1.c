/*
 * DB1->3 compatibility layer
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>

#include "../common/vi_db.h"
#include "../common/dbinternal.h"

/*
 * DB_ENV emulation
 */

static int db1_dbenv_close(DB_ENV *, u_int32_t);
static int db1_dbenv_open(DB_ENV *, char *, u_int32_t, int);
static int db1_dbenv_remove(DB_ENV *, char *, u_int32_t);

int
db_env_create(DB_ENV **dbenvp, u_int32_t flags) {
	DB_ENV *dbenv;

	assert(flags == 0);

	dbenv = malloc(sizeof *dbenv);
	if (dbenv == NULL)
		return -1;

	dbenv->close = db1_dbenv_close;
	dbenv->open = db1_dbenv_open;
	dbenv->remove = db1_dbenv_remove;

	dbenv->base_path = NULL;
	dbenv->mode = 0;

	*dbenvp = dbenv;
	return 0;
}

static int
db1_dbenv_close(DB_ENV *dbenv, u_int32_t flags) {
	assert(flags == 0);

	if (dbenv->base_path != NULL)
		free(dbenv->base_path);

	free(dbenv);
	return 0;
}

static int
db1_dbenv_open(DB_ENV *dbenv, char *base_path, u_int32_t flags, int mode) {

	/* We ignore flags on purpose */

	dbenv->base_path = strdup(base_path);
	if (dbenv->base_path == NULL)
		return ENOSPC;

	dbenv->mode = mode != 0? mode : 0660;
	return 0;
}

static int
db1_dbenv_remove(DB_ENV *dbenv_fake, char *base_path, u_int32_t flags) {
	/* dbenv_fake is not a useful environment */
	/* XXX check if we have to remove files here */

	return 0;
}

/*
 * DB emulation
 */
static int db1_db_close(DB *, u_int32_t);
static int db1_db_open(DB *, const char *, const char *, DBTYPE, u_int32_t, int);
static int db1_db_sync(DB *, u_int32_t);
static int db1_db_get(DB *, DB_TXN *, DBT *, DBT *, u_int32_t);
static int db1_db_put(DB *, DB_TXN *, DBT *, DBT *, u_int32_t);
static int db1_db_del(DB *, DB_TXN *, DBT *, u_int32_t);
static int db1_db_set_flags(DB *, u_int32_t);
static int db1_db_set_pagesize(DB *, u_int32_t);
static int db1_db_set_re_delim(DB *, int);
static int db1_db_set_re_source(DB *, const char *);
static int db1_db_cursor(DB *, DB_TXN *, DBC **, u_int32_t);

int
db_create(DB **dbp, DB_ENV *dbenv, u_int32_t flags) {
	assert(flags == 0);

	*dbp = malloc(sizeof **dbp);
	if (*dbp == NULL)
		return -1;

	(*dbp)->type = DB_UNKNOWN;
	(*dbp)->actual_db = NULL;
	(*dbp)->_pagesize = 0;
	(*dbp)->_flags = 0;
	memset(&(*dbp)->_recno_info, 0, sizeof (RECNOINFO));

	(*dbp)->close = db1_db_close;
	(*dbp)->open = db1_db_open;
	(*dbp)->sync = db1_db_sync;
	(*dbp)->get = db1_db_get;
	(*dbp)->put = db1_db_put;
	(*dbp)->del = db1_db_del;
	(*dbp)->set_flags = db1_db_set_flags;
	(*dbp)->set_pagesize = db1_db_set_pagesize;
	(*dbp)->set_re_delim = db1_db_set_re_delim;
	(*dbp)->set_re_source = db1_db_set_re_source;
	(*dbp)->cursor = db1_db_cursor;

	return 0;
}

const char *
db_strerror(int error) {
	return error > 0? strerror(error) : "record not found";
}

static int
db1_db_close(DB *db, u_int32_t flags) {
	if (flags & DB_NOSYNC) {
		/* XXX warn user? */
	}
	db->actual_db->close(db->actual_db);

	db->type = DB_UNKNOWN;
	db->actual_db = NULL;
	db->_pagesize = 0;
	db->_flags = 0;
	memset(&db->_recno_info, 0, sizeof (RECNOINFO));

	return 0;
}

static int
db1_db_open(DB *db, const char *file, const char *database, DBTYPE type,
						u_int32_t flags, int mode) {
	int oldflags = 0;

	assert(database == NULL && !(flags & ~(DB_CREATE | DB_TRUNCATE)));

	db->type = type;

	if (flags & DB_CREATE)
		oldflags |= O_CREAT;
	if (flags & DB_TRUNCATE)
		oldflags |= O_TRUNC;

	if (type == DB_RECNO) {
		const char *tmp = file;

		/* The interface is reversed in DB3 */
		file = db->_recno_info.bfname;
		db->_recno_info.bfname = __UNCONST(tmp);

		/* ... and so, we should avoid to truncate the main file! */
		oldflags &= ~O_TRUNC;

		db->_recno_info.flags =
			db->_flags & DB_SNAPSHOT? R_SNAPSHOT : 0;
		db->_recno_info.psize = db->_pagesize;
	}

	db->actual_db = dbopen(file, oldflags, mode, type,
				type == DB_RECNO? &db->_recno_info : NULL);

	return db->actual_db == NULL? errno : 0;
}

static int
db1_db_sync(DB *db, u_int32_t flags) {
	assert(flags == 0);

	return db->actual_db->sync(db->actual_db, db->type == DB_UNKNOWN?
					R_RECNOSYNC : 0) == 0? 0 : errno;
}

static int
db1_db_get(DB *db, DB_TXN *txnid, DBT *key, DBT *data, u_int32_t flags) {
	int err;
	DBT_v1 data1;

	assert(flags == 0 && txnid == NULL);

	err = db->actual_db->get(db->actual_db, (DBT_v1 *) key, &data1, flags);
	if (err == 1)
		return DB_NOTFOUND;
	else if (err == -1)
		return errno;

	if (data->flags & DB_DBT_USERMEM) {
		data->size = data1.size;
		if (data1.size > data->ulen)
			return DB_BUFFER_SMALL;

		memcpy(data->data, data1.data, data1.size);
	}

	return 0;
}

static int
db1_db_put(DB *db, DB_TXN *txnid, DBT *key, DBT *data, u_int32_t flags) {
	int err;
	DB_old *db_v1 = db->actual_db;
	DBT data1;
	DBT key1;
	recno_t recno = 1;

	assert((flags & ~DB_APPEND) == 0 && txnid == NULL);

	key1 = *key;

	if (flags & DB_APPEND) {
		if (db_v1->seq(db_v1, (DBT_v1 *)(void *)key,
		    (DBT_v1 *)(void *)&data1, R_LAST) == 1) {
			key1.data = &recno;
			key1.size = sizeof recno;
		}
	}
	err = db_v1->put(db_v1, (DBT_v1 *)(void *)&key1, (DBT_v1 *)(void *)data,
	    0);

	return err == -1? errno : err;
}

static int
db1_db_del(DB *db, DB_TXN *txnid, DBT *key, u_int32_t flags) {
	int err;
	DB_old *db_v1 = db->actual_db;

	assert(txnid == NULL && flags == 0);

	err = db_v1->del(db_v1, (DBT_v1 *) key, 0);
	return err == -1? errno : err;
}

static int
db1_db_set_flags(DB *db, u_int32_t flags) {
	assert((flags & ~(DB_RENUMBER | DB_SNAPSHOT)) == 0);

	/* Can't prevent renumbering from happening with DB1 */
	assert((flags | db->_flags) & DB_RENUMBER);


	db->_flags |= flags;

	return 0;
}

static int
db1_db_set_pagesize(DB *db, u_int32_t pagesize) {
	db->_pagesize = pagesize;

	return 0;
}

static int
db1_db_set_re_delim(DB *db, int re_delim) {
	db->_recno_info.bval = re_delim;

	return 0;
}

static int
db1_db_set_re_source(DB *db, const char *re_source) {
	db->_recno_info.bfname = __UNCONST(re_source);

	return 0;
}

/* DBC emulation. Very basic, only one cursor at a time, enough for vi */

static int db1_dbc_close(DBC *);
static int db1_dbc_get(DBC *, DBT *, DBT *, u_int32_t);
static int db1_dbc_put(DBC *, DBT *, DBT *, u_int32_t);

static int
db1_db_cursor(DB *db, DB_TXN *txn, DBC **cursorp, u_int32_t flags) {
	DBC *cursor;

	assert(txn == NULL && flags == 0);

	cursor = malloc(sizeof *cursor);
	if (cursor == NULL)
		return -1;

	cursor->db = db;
	cursor->pos_key.data = &cursor->pos;
	cursor->pos_key.size = sizeof cursor->pos;
	cursor->c_close = db1_dbc_close;
	cursor->c_get = db1_dbc_get;
	cursor->c_put = db1_dbc_put;

	*cursorp = cursor;

	return 0;
}

static int
db1_dbc_close(DBC *cursor) {
	free(cursor);
	return 0;
}

static int
db1_dbc_get(DBC *cursor, DBT *key, DBT *data, u_int32_t flags) {
	DB *db = cursor->db;
	DB_old *db_v1 = db->actual_db;
	int ret = 0;


	switch(flags) {
	case DB_SET:
		ret = db_v1->seq(db_v1, (DBT_v1 *) key, (DBT_v1 *) data,
			R_CURSOR);
		cursor->pos = * (db_recno_t *) key->data;
		break;
	case DB_FIRST:
		ret = db_v1->seq(db_v1, (DBT_v1 *) key, (DBT_v1 *) data,
			R_FIRST);
		if (ret == 1)
			ret = DB_NOTFOUND;
		cursor->pos = * (db_recno_t *) key->data;
		break;
	case DB_LAST:
		ret = db_v1->seq(db_v1, (DBT_v1 *) key, (DBT_v1 *) data,
			R_LAST);
		if (ret == 1)
			ret = DB_NOTFOUND;
		cursor->pos = * (db_recno_t *) key->data;
		break;
	default:
		abort();
	}

	return ret;
}

static int
db1_dbc_put(DBC *cursor, DBT *key, DBT *data, u_int32_t flags) {
	DB *db = cursor->db;
	DB_old *db_v1 = db->actual_db;
	int ret = 0;

	assert((flags & ~(DB_BEFORE | DB_AFTER)) == 0);

	ret = db_v1->put(db_v1, &cursor->pos_key, (DBT_v1 *) data,
		flags == DB_BEFORE? R_IBEFORE : R_IAFTER);

	return ret == -1? errno : ret;
}
