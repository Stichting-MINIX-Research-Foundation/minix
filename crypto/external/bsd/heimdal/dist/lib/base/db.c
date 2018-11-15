/*	$NetBSD: db.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 2011, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is a pluggable simple DB abstraction, with a simple get/set/
 * delete key/value pair interface.
 *
 * Plugins may provide any of the following optional features:
 *
 *  - tables -- multiple attribute/value tables in one DB
 *  - locking
 *  - transactions (i.e., allow any heim_object_t as key or value)
 *  - transcoding of values
 *
 * Stackable plugins that provide missing optional features are
 * possible.
 *
 * Any plugin that provides locking will also provide transactions, but
 * those transactions will not be atomic in the face of failures (a
 * memory-based rollback log is used).
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#else
#include <sys/file.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include "baselocl.h"
#include <krb5/base64.h>

#define HEIM_ENOMEM(ep) \
    (((ep) && !*(ep)) ? \
	heim_error_get_code((*(ep) = heim_error_create_enomem())) : ENOMEM)

#define HEIM_ERROR_HELPER(ep, ec, args) \
    (((ep) && !*(ep)) ? \
	heim_error_get_code((*(ep) = heim_error_create args)) : (ec))

#define HEIM_ERROR(ep, ec, args) \
    (ec == ENOMEM) ? HEIM_ENOMEM(ep) : HEIM_ERROR_HELPER(ep, ec, args);

static heim_string_t to_base64(heim_data_t, heim_error_t *);
static heim_data_t from_base64(heim_string_t, heim_error_t *);

static int open_file(const char *, int , int, int *, heim_error_t *);
static int read_json(const char *, heim_object_t *, heim_error_t *);
static struct heim_db_type json_dbt;

static void db_dealloc(void *ptr);

struct heim_type_data db_object = {
    HEIM_TID_DB,
    "db-object",
    NULL,
    db_dealloc,
    NULL,
    NULL,
    NULL,
    NULL
};


static heim_base_once_t db_plugin_init_once = HEIM_BASE_ONCE_INIT;

static heim_dict_t db_plugins;

typedef struct db_plugin {
    heim_string_t               name;
    heim_db_plug_open_f_t       openf;
    heim_db_plug_clone_f_t      clonef;
    heim_db_plug_close_f_t      closef;
    heim_db_plug_lock_f_t       lockf;
    heim_db_plug_unlock_f_t     unlockf;
    heim_db_plug_sync_f_t       syncf;
    heim_db_plug_begin_f_t      beginf;
    heim_db_plug_commit_f_t     commitf;
    heim_db_plug_rollback_f_t   rollbackf;
    heim_db_plug_copy_value_f_t copyf;
    heim_db_plug_set_value_f_t  setf;
    heim_db_plug_del_key_f_t    delf;
    heim_db_plug_iter_f_t       iterf;
    void                        *data;
} db_plugin_desc, *db_plugin;

struct heim_db_data {
    db_plugin           plug;
    heim_string_t       dbtype;
    heim_string_t       dbname;
    heim_dict_t         options;
    void                *db_data;
    heim_data_t		to_release;
    heim_error_t        error;
    int                 ret;
    unsigned int        in_transaction:1;
    unsigned int	ro:1;
    unsigned int	ro_tx:1;
    heim_dict_t         set_keys;
    heim_dict_t         del_keys;
    heim_string_t       current_table;
};

static int
db_do_log_actions(heim_db_t db, heim_error_t *error);
static int
db_replay_log(heim_db_t db, heim_error_t *error);

static HEIMDAL_MUTEX db_type_mutex = HEIMDAL_MUTEX_INITIALIZER;

static void
db_init_plugins_once(void *arg)
{
    db_plugins = heim_retain(arg);
}

static void
plugin_dealloc(void *arg)
{
    db_plugin plug = arg;

    heim_release(plug->name);
}

/** heim_db_register
 * @brief Registers a DB type for use with heim_db_create().
 *
 * @param dbtype Name of DB type
 * @param data   Private data argument to the dbtype's openf method
 * @param plugin Structure with DB type methods (function pointers)
 *
 * Backends that provide begin/commit/rollback methods must provide ACID
 * semantics.
 *
 * The registered DB type will have ACID semantics for backends that do
 * not provide begin/commit/rollback methods but do provide lock/unlock
 * and rdjournal/wrjournal methods (using a replay log journalling
 * scheme).
 *
 * If the registered DB type does not natively provide read vs. write
 * transaction isolation but does provide a lock method then the DB will
 * provide read/write transaction isolation.
 *
 * @return ENOMEM on failure, else 0.
 *
 * @addtogroup heimbase
 */
int
heim_db_register(const char *dbtype,
		 void *data,
		 struct heim_db_type *plugin)
{
    heim_dict_t plugins;
    heim_string_t s;
    db_plugin plug, plug2;
    int ret = 0;

    if ((plugin->beginf != NULL && plugin->commitf == NULL) ||
	(plugin->beginf != NULL && plugin->rollbackf == NULL) ||
	(plugin->lockf != NULL && plugin->unlockf == NULL) ||
	plugin->copyf == NULL)
	heim_abort("Invalid DB plugin; make sure methods are paired");

    /* Initialize */
    plugins = heim_dict_create(11);
    if (plugins == NULL)
	return ENOMEM;
    heim_base_once_f(&db_plugin_init_once, plugins, db_init_plugins_once);
    heim_release(plugins);
    heim_assert(db_plugins != NULL, "heim_db plugin table initialized");

    s = heim_string_create(dbtype);
    if (s == NULL)
	return ENOMEM;

    plug = heim_alloc(sizeof (*plug), "db_plug", plugin_dealloc);
    if (plug == NULL) {
	heim_release(s);
	return ENOMEM;
    }

    plug->name = heim_retain(s);
    plug->openf = plugin->openf;
    plug->clonef = plugin->clonef;
    plug->closef = plugin->closef;
    plug->lockf = plugin->lockf;
    plug->unlockf = plugin->unlockf;
    plug->syncf = plugin->syncf;
    plug->beginf = plugin->beginf;
    plug->commitf = plugin->commitf;
    plug->rollbackf = plugin->rollbackf;
    plug->copyf = plugin->copyf;
    plug->setf = plugin->setf;
    plug->delf = plugin->delf;
    plug->iterf = plugin->iterf;
    plug->data = data;

    HEIMDAL_MUTEX_lock(&db_type_mutex);
    plug2 = heim_dict_get_value(db_plugins, s);
    if (plug2 == NULL)
	ret = heim_dict_set_value(db_plugins, s, plug);
    HEIMDAL_MUTEX_unlock(&db_type_mutex);
    heim_release(plug);
    heim_release(s);

    return ret;
}

static void
db_dealloc(void *arg)
{
    heim_db_t db = arg;
    heim_assert(!db->in_transaction,
		"rollback or commit heim_db_t before releasing it");
    if (db->db_data)
	(void) db->plug->closef(db->db_data, NULL);
    heim_release(db->to_release);
    heim_release(db->dbtype);
    heim_release(db->dbname);
    heim_release(db->options);
    heim_release(db->set_keys);
    heim_release(db->del_keys);
    heim_release(db->error);
}

struct dbtype_iter {
    heim_db_t           db;
    const char          *dbname;
    heim_dict_t         options;
    heim_error_t        *error;
};

/*
 * Helper to create a DB handle with the first registered DB type that
 * can open the given DB.  This is useful when the app doesn't know the
 * DB type a priori.  This assumes that DB types can "taste" DBs, either
 * from the filename extension or from the actual file contents.
 */
static void
dbtype_iter2create_f(heim_object_t dbtype, heim_object_t junk, void *arg)
{
    struct dbtype_iter *iter_ctx = arg;

    if (iter_ctx->db != NULL)
	return;
    iter_ctx->db = heim_db_create(heim_string_get_utf8(dbtype),
				  iter_ctx->dbname, iter_ctx->options,
				  iter_ctx->error);
}

/**
 * Open a database of the given dbtype.
 *
 * Database type names can be composed of one or more pseudo-DB types
 * and one concrete DB type joined with a '+' between each.  For
 * example: "transaction+bdb" might be a Berkeley DB with a layer above
 * that provides transactions.
 *
 * Options may be provided via a dict (an associative array).  Existing
 * options include:
 *
 *  - "create", with any value (create if DB doesn't exist)
 *  - "exclusive", with any value (exclusive create)
 *  - "truncate", with any value (truncate the DB)
 *  - "read-only", with any value (disallow writes)
 *  - "sync", with any value (make transactions durable)
 *  - "journal-name", with a string value naming a journal file name
 *
 * @param dbtype  Name of DB type
 * @param dbname  Name of DB (likely a file path)
 * @param options Options dict
 * @param db      Output open DB handle
 * @param error   Output error  object
 *
 * @return a DB handle
 *
 * @addtogroup heimbase
 */
heim_db_t
heim_db_create(const char *dbtype, const char *dbname,
	       heim_dict_t options, heim_error_t *error)
{
    heim_string_t s;
    char *p;
    db_plugin plug;
    heim_db_t db;
    int ret = 0;

    if (options == NULL) {
	options = heim_dict_create(11);
	if (options == NULL) {
	    if (error)
		*error = heim_error_create_enomem();
	    return NULL;
	}
    } else {
	(void) heim_retain(options);
    }

    if (db_plugins == NULL) {
	heim_release(options);
	return NULL;
    }

    if (dbtype == NULL || *dbtype == '\0') {
	struct dbtype_iter iter_ctx = { NULL, dbname, options, error};

	/* Try all dbtypes */
	heim_dict_iterate_f(db_plugins, &iter_ctx, dbtype_iter2create_f);
	heim_release(options);
	return iter_ctx.db;
    } else if (strstr(dbtype, "json")) {
	(void) heim_db_register(dbtype, NULL, &json_dbt);
    }

    /*
     * Allow for dbtypes that are composed from pseudo-dbtypes chained
     * to a real DB type with '+'.  For example a pseudo-dbtype might
     * add locking, transactions, transcoding of values, ...
     */
    p = strchr(dbtype, '+');
    if (p != NULL)
	s = heim_string_create_with_bytes(dbtype, p - dbtype);
    else
	s = heim_string_create(dbtype);
    if (s == NULL) {
	heim_release(options);
	return NULL;
    }

    HEIMDAL_MUTEX_lock(&db_type_mutex);
    plug = heim_dict_get_value(db_plugins, s);
    HEIMDAL_MUTEX_unlock(&db_type_mutex);
    heim_release(s);
    if (plug == NULL) {
	if (error)
	    *error = heim_error_create(ENOENT,
				       N_("Heimdal DB plugin not found: %s", ""),
				       dbtype);
	heim_release(options);
	return NULL;
    }

    db = _heim_alloc_object(&db_object, sizeof(*db));
    if (db == NULL) {
	heim_release(options);
	return NULL;
    }

    db->in_transaction = 0;
    db->ro_tx = 0;
    db->set_keys = NULL;
    db->del_keys = NULL;
    db->plug = plug;
    db->options = options;

    ret = plug->openf(plug->data, dbtype, dbname, options, &db->db_data, error);
    if (ret) {
	heim_release(db);
	if (error && *error == NULL)
	    *error = heim_error_create(ENOENT,
				       N_("Heimdal DB could not be opened: %s", ""),
				       dbname);
	return NULL;
    }

    ret = db_replay_log(db, error);
    if (ret) {
	heim_release(db);
	return NULL;
    }

    if (plug->clonef == NULL) {
	db->dbtype = heim_string_create(dbtype);
	db->dbname = heim_string_create(dbname);

	if (!db->dbtype || ! db->dbname) {
	    heim_release(db);
	    if (error)
		*error = heim_error_create_enomem();
	    return NULL;
	}
    }

    return db;
}

/**
 * Clone (duplicate) an open DB handle.
 *
 * This is useful for multi-threaded applications.  Applications must
 * synchronize access to any given DB handle.
 *
 * Returns EBUSY if there is an open transaction for the input db.
 *
 * @param db      Open DB handle
 * @param error   Output error object
 *
 * @return a DB handle
 *
 * @addtogroup heimbase
 */
heim_db_t
heim_db_clone(heim_db_t db, heim_error_t *error)
{
    heim_db_t result;
    int ret;

    if (heim_get_tid(db) != HEIM_TID_DB)
	heim_abort("Expected a database");
    if (db->in_transaction)
	heim_abort("DB handle is busy");

    if (db->plug->clonef == NULL) {
	return heim_db_create(heim_string_get_utf8(db->dbtype),
			      heim_string_get_utf8(db->dbname),
			      db->options, error);
    }

    result = _heim_alloc_object(&db_object, sizeof(*result));
    if (result == NULL) {
	if (error)
	    *error = heim_error_create_enomem();
	return NULL;
    }

    result->set_keys = NULL;
    result->del_keys = NULL;
    ret = db->plug->clonef(db->db_data, &result->db_data, error);
    if (ret) {
	heim_release(result);
	if (error && !*error)
	    *error = heim_error_create(ENOENT,
				       N_("Could not re-open DB while cloning", ""));
	return NULL;
    }
    db->db_data = NULL;
    return result;
}

/**
 * Open a transaction on the given db.
 *
 * @param db    Open DB handle
 * @param error Output error object
 *
 * @return 0 on success, system error otherwise
 *
 * @addtogroup heimbase
 */
int
heim_db_begin(heim_db_t db, int read_only, heim_error_t *error)
{
    int ret;

    if (heim_get_tid(db) != HEIM_TID_DB)
	return EINVAL;

    if (db->in_transaction && (read_only || !db->ro_tx || (!read_only && !db->ro_tx)))
	heim_abort("DB already in transaction");

    if (db->plug->setf == NULL || db->plug->delf == NULL)
	return EINVAL;

    if (db->plug->beginf) {
	ret = db->plug->beginf(db->db_data, read_only, error);
        if (ret)
            return ret;
    } else if (!db->in_transaction) {
	/* Try to emulate transactions */

	if (db->plug->lockf == NULL)
	    return EINVAL; /* can't lock? -> no transactions */

	/* Assume unlock provides sync/durability */
	ret = db->plug->lockf(db->db_data, read_only, error);
	if (ret)
	    return ret;

	ret = db_replay_log(db, error);
	if (ret) {
	    ret = db->plug->unlockf(db->db_data, error);
	    return ret;
	}

	db->set_keys = heim_dict_create(11);
	if (db->set_keys == NULL)
	    return ENOMEM;
	db->del_keys = heim_dict_create(11);
	if (db->del_keys == NULL) {
	    heim_release(db->set_keys);
	    db->set_keys = NULL;
	    return ENOMEM;
	}
    } else {
	heim_assert(read_only == 0, "Internal error");
	ret = db->plug->lockf(db->db_data, 0, error);
	if (ret)
	    return ret;
    }
    db->in_transaction = 1;
    db->ro_tx = !!read_only;
    return 0;
}

/**
 * Commit an open transaction on the given db.
 *
 * @param db    Open DB handle
 * @param error Output error object
 *
 * @return 0 on success, system error otherwise
 *
 * @addtogroup heimbase
 */
int
heim_db_commit(heim_db_t db, heim_error_t *error)
{
    int ret, ret2;
    heim_string_t journal_fname = NULL;

    if (heim_get_tid(db) != HEIM_TID_DB)
	return EINVAL;
    if (!db->in_transaction)
	return 0;
    if (db->plug->commitf == NULL && db->plug->lockf == NULL)
	return EINVAL;

    if (db->plug->commitf != NULL) {
	ret = db->plug->commitf(db->db_data, error);
	if (ret)
	    (void) db->plug->rollbackf(db->db_data, error);

	db->in_transaction = 0;
	db->ro_tx = 0;
	return ret;
    }

    if (db->ro_tx) {
	ret = 0;
	goto done;
    }

    if (db->options == NULL)
	journal_fname = heim_dict_get_value(db->options, HSTR("journal-filename"));

    if (journal_fname != NULL) {
	heim_array_t a;
	heim_string_t journal_contents;
	size_t len, bytes;
	int save_errno;

	/* Create contents for replay log */
	ret = ENOMEM;
	a = heim_array_create();
	if (a == NULL)
	    goto err;
	ret = heim_array_append_value(a, db->set_keys);
	if (ret) {
	    heim_release(a);
	    goto err;
	}
	ret = heim_array_append_value(a, db->del_keys);
	if (ret) {
	    heim_release(a);
	    goto err;
	}
	journal_contents = heim_json_copy_serialize(a, 0, error);
	heim_release(a);

	/* Write replay log */
	if (journal_fname != NULL) {
	    int fd;

	    ret = open_file(heim_string_get_utf8(journal_fname), 1, 0, &fd, error);
	    if (ret) {
		heim_release(journal_contents);
		goto err;
	    }
	    len = strlen(heim_string_get_utf8(journal_contents));
	    bytes = write(fd, heim_string_get_utf8(journal_contents), len);
	    save_errno = errno;
	    heim_release(journal_contents);
	    ret = close(fd);
	    if (bytes != len) {
		/* Truncate replay log */
		(void) open_file(heim_string_get_utf8(journal_fname), 1, 0, NULL, error);
		ret = save_errno;
		goto err;
	    }
	    if (ret)
		goto err;
	}
    }

    /* Apply logged actions */
    ret = db_do_log_actions(db, error);
    if (ret)
	return ret;

    if (db->plug->syncf != NULL) {
	/* fsync() or whatever */
	ret = db->plug->syncf(db->db_data, error);
	if (ret)
	    return ret;
    }

    /* Truncate replay log and we're done */
    if (journal_fname != NULL) {
	int fd;

	ret2 = open_file(heim_string_get_utf8(journal_fname), 1, 0, &fd, error);
	if (ret2 == 0)
	    (void) close(fd);
    }

    /*
     * Clean up; if we failed to remore the replay log that's OK, we'll
     * handle that again in heim_db_commit()
     */
done:
    heim_release(db->set_keys);
    heim_release(db->del_keys);
    db->set_keys = NULL;
    db->del_keys = NULL;
    db->in_transaction = 0;
    db->ro_tx = 0;

    ret2 = db->plug->unlockf(db->db_data, error);
    if (ret == 0)
	ret = ret2;

    return ret;

err:
    return HEIM_ERROR(error, ret,
		      (ret, N_("Error while committing transaction: %s", ""),
		       strerror(ret)));
}

/**
 * Rollback an open transaction on the given db.
 *
 * @param db    Open DB handle
 * @param error Output error object
 *
 * @return 0 on success, system error otherwise
 *
 * @addtogroup heimbase
 */
int
heim_db_rollback(heim_db_t db, heim_error_t *error)
{
    int ret = 0;

    if (heim_get_tid(db) != HEIM_TID_DB)
	return EINVAL;
    if (!db->in_transaction)
	return 0;

    if (db->plug->rollbackf != NULL)
	ret = db->plug->rollbackf(db->db_data, error);
    else if (db->plug->unlockf != NULL)
	ret = db->plug->unlockf(db->db_data, error);

    heim_release(db->set_keys);
    heim_release(db->del_keys);
    db->set_keys = NULL;
    db->del_keys = NULL;
    db->in_transaction = 0;
    db->ro_tx = 0;

    return ret;
}

/**
 * Get type ID of heim_db_t objects.
 *
 * @addtogroup heimbase
 */
heim_tid_t
heim_db_get_type_id(void)
{
    return HEIM_TID_DB;
}

heim_data_t
_heim_db_get_value(heim_db_t db, heim_string_t table, heim_data_t key,
		   heim_error_t *error)
{
    heim_release(db->to_release);
    db->to_release = heim_db_copy_value(db, table, key, error);
    return db->to_release;
}

/**
 * Lookup a key's value in the DB.
 *
 * Returns 0 on success, -1 if the key does not exist in the DB, or a
 * system error number on failure.
 *
 * @param db    Open DB handle
 * @param key   Key
 * @param error Output error object
 *
 * @return the value (retained), if there is one for the given key
 *
 * @addtogroup heimbase
 */
heim_data_t
heim_db_copy_value(heim_db_t db, heim_string_t table, heim_data_t key,
		   heim_error_t *error)
{
    heim_object_t v;
    heim_data_t result;

    if (heim_get_tid(db) != HEIM_TID_DB)
	return NULL;

    if (error != NULL)
	*error = NULL;

    if (table == NULL)
	table = HSTR("");

    if (db->in_transaction) {
	heim_string_t key64;

	key64 = to_base64(key, error);
	if (key64 == NULL) {
	    if (error)
		*error = heim_error_create_enomem();
	    return NULL;
	}

	v = heim_path_copy(db->set_keys, error, table, key64, NULL);
	if (v != NULL) {
	    heim_release(key64);
	    return v;
	}
	v = heim_path_copy(db->del_keys, error, table, key64, NULL); /* can't be NULL */
	heim_release(key64);
	if (v != NULL)
	    return NULL;
    }

    result = db->plug->copyf(db->db_data, table, key, error);

    return result;
}

/**
 * Set a key's value in the DB.
 *
 * @param db    Open DB handle
 * @param key   Key
 * @param value Value (if NULL the key will be deleted, but empty is OK)
 * @param error Output error object
 *
 * @return 0 on success, system error otherwise
 *
 * @addtogroup heimbase
 */
int
heim_db_set_value(heim_db_t db, heim_string_t table,
		  heim_data_t key, heim_data_t value, heim_error_t *error)
{
    heim_string_t key64 = NULL;
    int ret;

    if (error != NULL)
	*error = NULL;

    if (table == NULL)
	table = HSTR("");

    if (value == NULL)
	/* Use heim_null_t instead of NULL */
	return heim_db_delete_key(db, table, key, error);

    if (heim_get_tid(db) != HEIM_TID_DB)
	return EINVAL;

    if (heim_get_tid(key) != HEIM_TID_DATA)
	return HEIM_ERROR(error, EINVAL,
			  (EINVAL, N_("DB keys must be data", "")));

    if (db->plug->setf == NULL)
	return EBADF;

    if (!db->in_transaction) {
	ret = heim_db_begin(db, 0, error);
	if (ret)
	    goto err;
	heim_assert(db->in_transaction, "Internal error");
	ret = heim_db_set_value(db, table, key, value, error);
	if (ret) {
	    (void) heim_db_rollback(db, NULL);
	    return ret;
	}
	return heim_db_commit(db, error);
    }

    /* Transaction emulation */
    heim_assert(db->set_keys != NULL, "Internal error");
    key64 = to_base64(key, error);
    if (key64 == NULL)
	return HEIM_ENOMEM(error);

    if (db->ro_tx) {
	ret = heim_db_begin(db, 0, error);
	if (ret)
	    goto err;
    }
    ret = heim_path_create(db->set_keys, 29, value, error, table, key64, NULL);
    if (ret)
	goto err;
    heim_path_delete(db->del_keys, error, table, key64, NULL);
    heim_release(key64);

    return 0;

err:
    heim_release(key64);
    return HEIM_ERROR(error, ret,
		      (ret, N_("Could not set a dict value while while "
		       "setting a DB value", "")));
}

/**
 * Delete a key and its value from the DB
 *
 *
 * @param db    Open DB handle
 * @param key   Key
 * @param error Output error object
 *
 * @return 0 on success, system error otherwise
 *
 * @addtogroup heimbase
 */
int
heim_db_delete_key(heim_db_t db, heim_string_t table, heim_data_t key,
		   heim_error_t *error)
{
    heim_string_t key64 = NULL;
    int ret;

    if (error != NULL)
	*error = NULL;

    if (table == NULL)
	table = HSTR("");

    if (heim_get_tid(db) != HEIM_TID_DB)
	return EINVAL;

    if (db->plug->delf == NULL)
	return EBADF;

    if (!db->in_transaction) {
	ret = heim_db_begin(db, 0, error);
	if (ret)
	    goto err;
	heim_assert(db->in_transaction, "Internal error");
	ret = heim_db_delete_key(db, table, key, error);
	if (ret) {
	    (void) heim_db_rollback(db, NULL);
	    return ret;
	}
	return heim_db_commit(db, error);
    }

    /* Transaction emulation */
    heim_assert(db->set_keys != NULL, "Internal error");
    key64 = to_base64(key, error);
    if (key64 == NULL)
	return HEIM_ENOMEM(error);
    if (db->ro_tx) {
	ret = heim_db_begin(db, 0, error);
	if (ret)
	    goto err;
    }
    ret = heim_path_create(db->del_keys, 29, heim_number_create(1), error, table, key64, NULL);
    if (ret)
	goto err;
    heim_path_delete(db->set_keys, error, table, key64, NULL);
    heim_release(key64);

    return 0;

err:
    heim_release(key64);
    return HEIM_ERROR(error, ret,
		      (ret, N_("Could not set a dict value while while "
		       "deleting a DB value", "")));
}

/**
 * Iterate a callback function over keys and values from a DB.
 *
 * @param db        Open DB handle
 * @param iter_data Callback function's private data
 * @param iter_f    Callback function, called once per-key/value pair
 * @param error     Output error object
 *
 * @addtogroup heimbase
 */
void
heim_db_iterate_f(heim_db_t db, heim_string_t table, void *iter_data,
		  heim_db_iterator_f_t iter_f, heim_error_t *error)
{
    if (error != NULL)
	*error = NULL;

    if (heim_get_tid(db) != HEIM_TID_DB)
	return;

    if (!db->in_transaction)
	db->plug->iterf(db->db_data, table, iter_data, iter_f, error);
}

static void
db_replay_log_table_set_keys_iter(heim_object_t key, heim_object_t value,
				  void *arg)
{
    heim_db_t db = arg;
    heim_data_t k, v;

    if (db->ret)
	return;

    k = from_base64((heim_string_t)key, &db->error);
    if (k == NULL) {
	db->ret = ENOMEM;
	return;
    }
    v = (heim_data_t)value;

    db->ret = db->plug->setf(db->db_data, db->current_table, k, v, &db->error);
    heim_release(k);
}

static void
db_replay_log_table_del_keys_iter(heim_object_t key, heim_object_t value,
				  void *arg)
{
    heim_db_t db = arg;
    heim_data_t k;

    if (db->ret) {
	db->ret = ENOMEM;
	return;
    }

    k = from_base64((heim_string_t)key, &db->error);
    if (k == NULL)
	return;

    db->ret = db->plug->delf(db->db_data, db->current_table, k, &db->error);
    heim_release(k);
}

static void
db_replay_log_set_keys_iter(heim_object_t table, heim_object_t table_dict,
			    void *arg)
{
    heim_db_t db = arg;

    if (db->ret)
	return;

    db->current_table = table;
    heim_dict_iterate_f(table_dict, db, db_replay_log_table_set_keys_iter);
}

static void
db_replay_log_del_keys_iter(heim_object_t table, heim_object_t table_dict,
			    void *arg)
{
    heim_db_t db = arg;

    if (db->ret)
	return;

    db->current_table = table;
    heim_dict_iterate_f(table_dict, db, db_replay_log_table_del_keys_iter);
}

static int
db_do_log_actions(heim_db_t db, heim_error_t *error)
{
    int ret;

    if (error)
	*error = NULL;

    db->ret = 0;
    db->error = NULL;
    if (db->set_keys != NULL)
	heim_dict_iterate_f(db->set_keys, db, db_replay_log_set_keys_iter);
    if (db->del_keys != NULL)
	heim_dict_iterate_f(db->del_keys, db, db_replay_log_del_keys_iter);

    ret = db->ret;
    db->ret = 0;
    if (error && db->error) {
	*error = db->error;
	db->error = NULL;
    } else {
	heim_release(db->error);
	db->error = NULL;
    }
    return ret;
}

static int
db_replay_log(heim_db_t db, heim_error_t *error)
{
    int ret;
    heim_string_t journal_fname = NULL;
    heim_object_t journal;
    size_t len;

    heim_assert(!db->in_transaction, "DB transaction not open");
    heim_assert(db->set_keys == NULL && db->set_keys == NULL, "DB transaction not open");

    if (error)
	*error = NULL;

    if (db->options == NULL)
	return 0;

    journal_fname = heim_dict_get_value(db->options, HSTR("journal-filename"));
    if (journal_fname == NULL)
	return 0;

    ret = read_json(heim_string_get_utf8(journal_fname), &journal, error);
    if (ret == ENOENT) {
        heim_release(journal_fname);
	return 0;
    }
    if (ret == 0 && journal == NULL) {
        heim_release(journal_fname);
	return 0;
    }
    if (ret != 0) {
        heim_release(journal_fname);
	return ret;
    }

    if (heim_get_tid(journal) != HEIM_TID_ARRAY) {
        heim_release(journal_fname);
	return HEIM_ERROR(error, EINVAL,
			  (ret, N_("Invalid journal contents; delete journal",
				   "")));
    }

    len = heim_array_get_length(journal);

    if (len > 0)
	db->set_keys = heim_array_get_value(journal, 0);
    if (len > 1)
	db->del_keys = heim_array_get_value(journal, 1);
    ret = db_do_log_actions(db, error);
    if (ret) {
        heim_release(journal_fname);
	return ret;
    }

    /* Truncate replay log and we're done */
    ret = open_file(heim_string_get_utf8(journal_fname), 1, 0, NULL, error);
    heim_release(journal_fname);
    if (ret)
	return ret;
    heim_release(db->set_keys);
    heim_release(db->del_keys);
    db->set_keys = NULL;
    db->del_keys = NULL;

    return 0;
}

static
heim_string_t to_base64(heim_data_t data, heim_error_t *error)
{
    char *b64 = NULL;
    heim_string_t s = NULL;
    const heim_octet_string *d;
    int ret;

    d = heim_data_get_data(data);
    ret = rk_base64_encode(d->data, d->length, &b64);
    if (ret < 0 || b64 == NULL)
	goto enomem;
    s = heim_string_ref_create(b64, free);
    if (s == NULL)
	goto enomem;
    return s;

enomem:
    free(b64);
    if (error)
	*error = heim_error_create_enomem();
    return NULL;
}

static
heim_data_t from_base64(heim_string_t s, heim_error_t *error)
{
    void *buf;
    size_t len;
    heim_data_t d;

    buf = malloc(strlen(heim_string_get_utf8(s)));
    if (buf == NULL)
	goto enomem;

    len = rk_base64_decode(heim_string_get_utf8(s), buf);
    d = heim_data_ref_create(buf, len, free);
    if (d == NULL)
	goto enomem;
    return d;

enomem:
    free(buf);
    if (error)
	*error = heim_error_create_enomem();
    return NULL;
}


static int
open_file(const char *dbname, int for_write, int excl, int *fd_out, heim_error_t *error)
{
#ifdef WIN32
    HANDLE hFile;
    int ret = 0;

    if (fd_out)
	*fd_out = -1;

    if (for_write)
	hFile = CreateFile(dbname, GENERIC_WRITE | GENERIC_READ, 0,
			   NULL, /* we'll close as soon as we read */
			   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    else
	hFile = CreateFile(dbname, GENERIC_READ, FILE_SHARE_READ,
			   NULL, /* we'll close as soon as we read */
			   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
	ret = GetLastError();
	_set_errno(ret); /* CreateFile() does not set errno */
	goto err;
    }
    if (fd_out == NULL) {
	(void) CloseHandle(hFile);
	return 0;
    }

    *fd_out = _open_osfhandle((intptr_t) hFile, 0);
    if (*fd_out < 0) {
	ret = errno;
	(void) CloseHandle(hFile);
	goto err;
    }

    /* No need to lock given share deny mode */
    return 0;

err:
    if (error != NULL) {
	char *s = NULL;
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		      0, ret, 0, (LPTSTR) &s, 0, NULL);
	*error = heim_error_create(ret, N_("Could not open JSON file %s: %s", ""),
				   dbname, s ? s : "<error formatting error>");
	LocalFree(s);
    }
    return ret;
#else
    int ret = 0;
    int fd;

    if (fd_out)
	*fd_out = -1;

    if (for_write && excl)
	fd = open(dbname, O_CREAT | O_EXCL | O_WRONLY, 0600);
    else if (for_write)
	fd = open(dbname, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    else
	fd = open(dbname, O_RDONLY);
    if (fd < 0) {
	if (error != NULL)
	    *error = heim_error_create(ret, N_("Could not open JSON file %s: %s", ""),
				       dbname, strerror(errno));
	return errno;
    }

    if (fd_out == NULL) {
	(void) close(fd);
	return 0;
    }

    ret = flock(fd, for_write ? LOCK_EX : LOCK_SH);
    if (ret == -1) {
	/* Note that we if O_EXCL we're leaving the [lock] file around */
	(void) close(fd);
	return HEIM_ERROR(error, errno,
			  (errno, N_("Could not lock JSON file %s: %s", ""),
			   dbname, strerror(errno)));
    }

    *fd_out = fd;
    
    return 0;
#endif
}

static int
read_json(const char *dbname, heim_object_t *out, heim_error_t *error)
{
    struct stat st;
    char *str = NULL;
    int ret;
    int fd = -1;
    ssize_t bytes;

    *out = NULL;
    ret = open_file(dbname, 0, 0, &fd, error);
    if (ret)
	return ret;

    ret = fstat(fd, &st);
    if (ret == -1) {
	(void) close(fd);
	return HEIM_ERROR(error, errno,
			  (ret, N_("Could not stat JSON DB %s: %s", ""),
			   dbname, strerror(errno)));
    }

    if (st.st_size == 0) {
	(void) close(fd);
	return 0;
    }

    str = malloc(st.st_size + 1);
    if (str == NULL) {
	 (void) close(fd);
	return HEIM_ENOMEM(error);
    }

    bytes = read(fd, str, st.st_size);
     (void) close(fd);
    if (bytes != st.st_size) {
	free(str);
	if (bytes >= 0)
	    errno = EINVAL; /* ?? */
	return HEIM_ERROR(error, errno,
			  (ret, N_("Could not read JSON DB %s: %s", ""),
			   dbname, strerror(errno)));
    }
    str[st.st_size] = '\0';
    *out = heim_json_create(str, 10, 0, error);
    free(str);
    if (*out == NULL)
	return (error && *error) ? heim_error_get_code(*error) : EINVAL;
    return 0;
}

typedef struct json_db {
    heim_dict_t dict;
    heim_string_t dbname;
    heim_string_t bkpname;
    int fd;
    time_t last_read_time;
    unsigned int read_only:1;
    unsigned int locked:1;
    unsigned int locked_needs_unlink:1;
} *json_db_t;

static int
json_db_open(void *plug, const char *dbtype, const char *dbname,
	     heim_dict_t options, void **db, heim_error_t *error)
{
    json_db_t jsondb;
    heim_dict_t contents = NULL;
    heim_string_t dbname_s = NULL;
    heim_string_t bkpname_s = NULL;

    if (error)
	*error = NULL;
    if (dbtype && *dbtype && strcmp(dbtype, "json"))
	return HEIM_ERROR(error, EINVAL, (EINVAL, N_("Wrong DB type", "")));
    if (dbname && *dbname && strcmp(dbname, "MEMORY") != 0) {
	char *ext = strrchr(dbname, '.');
	char *bkpname;
	size_t len;
	int ret;

	if (ext == NULL || strcmp(ext, ".json") != 0)
	    return HEIM_ERROR(error, EINVAL,
			      (EINVAL, N_("JSON DB files must end in .json",
					  "")));

	if (options) {
	    heim_object_t vc, ve, vt;

	    vc = heim_dict_get_value(options, HSTR("create"));
	    ve = heim_dict_get_value(options, HSTR("exclusive"));
	    vt = heim_dict_get_value(options, HSTR("truncate"));
	    if (vc && vt) {
		ret = open_file(dbname, 1, ve ? 1 : 0, NULL, error);
		if (ret)
		    return ret;
	    } else if (vc || ve || vt) {
		return HEIM_ERROR(error, EINVAL,
				  (EINVAL, N_("Invalid JSON DB open options",
					      "")));
	    }
	    /*
	     * We don't want cloned handles to truncate the DB, eh?
	     *
	     * We should really just create a copy of the options dict
	     * rather than modify the caller's!  But for that it'd be
	     * nicer to have copy utilities in heimbase, something like
	     * this:
	     *
	     * heim_object_t heim_copy(heim_object_t src, int depth,
	     *                         heim_error_t *error);
	     * 
	     * so that options = heim_copy(options, 1); means copy the
	     * dict but nothing else (whereas depth == 0 would mean
	     * heim_retain(), and depth > 1 would be copy that many
	     * levels).
	     */
	    heim_dict_delete_key(options, HSTR("create"));
	    heim_dict_delete_key(options, HSTR("exclusive"));
	    heim_dict_delete_key(options, HSTR("truncate"));
	}
	dbname_s = heim_string_create(dbname);
	if (dbname_s == NULL)
	    return HEIM_ENOMEM(error);
	
	len = snprintf(NULL, 0, "%s~", dbname);
	bkpname = malloc(len + 2);
	if (bkpname == NULL) {
	    heim_release(dbname_s);
	    return HEIM_ENOMEM(error);
	}
	(void) snprintf(bkpname, len + 1, "%s~", dbname);
	bkpname_s = heim_string_create(bkpname);
	free(bkpname);
	if (bkpname_s == NULL) {
	    heim_release(dbname_s);
	    return HEIM_ENOMEM(error);
	}

	ret = read_json(dbname, (heim_object_t *)&contents, error);
	if (ret) {
	    heim_release(bkpname_s);
	    heim_release(dbname_s);
	    return ret;
        }

	if (contents != NULL && heim_get_tid(contents) != HEIM_TID_DICT) {
	    heim_release(bkpname_s);
	    heim_release(dbname_s);
	    return HEIM_ERROR(error, EINVAL,
			      (EINVAL, N_("JSON DB contents not valid JSON",
					  "")));
        }
    }

    jsondb = heim_alloc(sizeof (*jsondb), "json_db", NULL);
    if (jsondb == NULL) {
	heim_release(contents);
	heim_release(dbname_s);
	heim_release(bkpname_s);
	return ENOMEM;
    }

    jsondb->last_read_time = time(NULL);
    jsondb->fd = -1;
    jsondb->dbname = dbname_s;
    jsondb->bkpname = bkpname_s;
    jsondb->read_only = 0;

    if (contents != NULL)
	jsondb->dict = contents;
    else {
	jsondb->dict = heim_dict_create(29);
	if (jsondb->dict == NULL) {
	    heim_release(jsondb);
	    return ENOMEM;
	}
    }

    *db = jsondb;
    return 0;
}

static int
json_db_close(void *db, heim_error_t *error)
{
    json_db_t jsondb = db;

    if (error)
	*error = NULL;
    if (jsondb->fd > -1)
	(void) close(jsondb->fd);
    jsondb->fd = -1;
    heim_release(jsondb->dbname);
    heim_release(jsondb->bkpname);
    heim_release(jsondb->dict);
    heim_release(jsondb);
    return 0;
}

static int
json_db_lock(void *db, int read_only, heim_error_t *error)
{
    json_db_t jsondb = db;
    int ret;

    heim_assert(jsondb->fd == -1 || (jsondb->read_only && !read_only),
		"DB locks are not recursive");

    jsondb->read_only = read_only ? 1 : 0;
    if (jsondb->fd > -1)
	return 0;

    ret = open_file(heim_string_get_utf8(jsondb->bkpname), 1, 1, &jsondb->fd, error);
    if (ret == 0) {
	jsondb->locked_needs_unlink = 1;
	jsondb->locked = 1;
    }
    return ret;
}

static int
json_db_unlock(void *db, heim_error_t *error)
{
    json_db_t jsondb = db;
    int ret = 0;

    heim_assert(jsondb->locked, "DB not locked when unlock attempted");
    if (jsondb->fd > -1)
	ret = close(jsondb->fd);
    jsondb->fd = -1;
    jsondb->read_only = 0;
    jsondb->locked = 0;
    if (jsondb->locked_needs_unlink)
	unlink(heim_string_get_utf8(jsondb->bkpname));
    jsondb->locked_needs_unlink = 0;
    return ret;
}

static int
json_db_sync(void *db, heim_error_t *error)
{
    json_db_t jsondb = db;
    size_t len, bytes;
    heim_error_t e;
    heim_string_t json;
    const char *json_text = NULL;
    int ret = 0;
    int fd = -1;
#ifdef WIN32
    int tries = 3;
#endif

    heim_assert(jsondb->fd > -1, "DB not locked when sync attempted");

    json = heim_json_copy_serialize(jsondb->dict, 0, &e);
    if (json == NULL) {
	if (error)
	    *error = e;
	else
	    heim_release(e);
	return heim_error_get_code(e);
    }

    json_text = heim_string_get_utf8(json);
    len = strlen(json_text);
    errno = 0;

#ifdef WIN32
    while (tries--) {
	ret = open_file(heim_string_get_utf8(jsondb->dbname), 1, 0, &fd, error);
	if (ret == 0)
	    break;
	sleep(1);
    }
    if (ret) {
	heim_release(json);
	return ret;
    }
#else
    fd = jsondb->fd;
#endif /* WIN32 */

    bytes = write(fd, json_text, len);
    heim_release(json);
    if (bytes != len)
	return errno ? errno : EIO;
    ret = fsync(fd);
    if (ret)
	return ret;

#ifdef WIN32
    ret = close(fd);
    if (ret)
	return GetLastError();
#else
    ret = rename(heim_string_get_utf8(jsondb->bkpname), heim_string_get_utf8(jsondb->dbname));
    if (ret == 0) {
	jsondb->locked_needs_unlink = 0;
	return 0;
    }
#endif /* WIN32 */

    return errno;
}

static heim_data_t
json_db_copy_value(void *db, heim_string_t table, heim_data_t key,
		  heim_error_t *error)
{
    json_db_t jsondb = db;
    heim_string_t key_string;
    const heim_octet_string *key_data = heim_data_get_data(key);
    struct stat st;
    heim_data_t result;

    if (error)
	*error = NULL;

    if (strnlen(key_data->data, key_data->length) != key_data->length) {
	HEIM_ERROR(error, EINVAL,
		   (EINVAL, N_("JSON DB requires keys that are actually "
			       "strings", "")));
	return NULL;
    }

    if (stat(heim_string_get_utf8(jsondb->dbname), &st) == -1) {
	HEIM_ERROR(error, errno,
		   (errno, N_("Could not stat JSON DB file", "")));
	return NULL;
    }

    if (st.st_mtime > jsondb->last_read_time ||
	st.st_ctime > jsondb->last_read_time) {
	heim_dict_t contents = NULL;
	int ret;

	/* Ignore file is gone (ENOENT) */
	ret = read_json(heim_string_get_utf8(jsondb->dbname),
		(heim_object_t *)&contents, error);
	if (ret)
	    return NULL;
	if (contents == NULL)
	    contents = heim_dict_create(29);
	heim_release(jsondb->dict);
	jsondb->dict = contents;
	jsondb->last_read_time = time(NULL);
    }

    key_string = heim_string_create_with_bytes(key_data->data,
					       key_data->length);
    if (key_string == NULL) {
	(void) HEIM_ENOMEM(error);
	return NULL;
    }

    result = heim_path_copy(jsondb->dict, error, table, key_string, NULL);
    heim_release(key_string);
    return result;
}

static int
json_db_set_value(void *db, heim_string_t table,
		  heim_data_t key, heim_data_t value, heim_error_t *error)
{
    json_db_t jsondb = db;
    heim_string_t key_string;
    const heim_octet_string *key_data = heim_data_get_data(key);
    int ret;

    if (error)
	*error = NULL;

    if (strnlen(key_data->data, key_data->length) != key_data->length)
	return HEIM_ERROR(error, EINVAL,
			  (EINVAL,
			   N_("JSON DB requires keys that are actually strings",
			      "")));

    key_string = heim_string_create_with_bytes(key_data->data,
					       key_data->length);
    if (key_string == NULL)
	return HEIM_ENOMEM(error);

    if (table == NULL)
	table = HSTR("");

    ret = heim_path_create(jsondb->dict, 29, value, error, table, key_string, NULL);
    heim_release(key_string);
    return ret;
}

static int
json_db_del_key(void *db, heim_string_t table, heim_data_t key,
		heim_error_t *error)
{
    json_db_t jsondb = db;
    heim_string_t key_string;
    const heim_octet_string *key_data = heim_data_get_data(key);

    if (error)
	*error = NULL;

    if (strnlen(key_data->data, key_data->length) != key_data->length)
	return HEIM_ERROR(error, EINVAL,
			  (EINVAL,
			   N_("JSON DB requires keys that are actually strings",
			      "")));

    key_string = heim_string_create_with_bytes(key_data->data,
					       key_data->length);
    if (key_string == NULL)
	return HEIM_ENOMEM(error);

    if (table == NULL)
	table = HSTR("");

    heim_path_delete(jsondb->dict, error, table, key_string, NULL);
    heim_release(key_string);
    return 0;
}

struct json_db_iter_ctx {
    heim_db_iterator_f_t        iter_f;
    void                        *iter_ctx;
};

static void json_db_iter_f(heim_object_t key, heim_object_t value, void *arg)
{
    struct json_db_iter_ctx *ctx = arg;
    const char *key_string;
    heim_data_t key_data;

    key_string = heim_string_get_utf8((heim_string_t)key);
    key_data = heim_data_ref_create(key_string, strlen(key_string), NULL);
    ctx->iter_f(key_data, (heim_object_t)value, ctx->iter_ctx);
    heim_release(key_data);
}

static void
json_db_iter(void *db, heim_string_t table, void *iter_data,
	     heim_db_iterator_f_t iter_f, heim_error_t *error)
{
    json_db_t jsondb = db;
    struct json_db_iter_ctx ctx;
    heim_dict_t table_dict;

    if (error)
	*error = NULL;

    if (table == NULL)
	table = HSTR("");

    table_dict = heim_dict_get_value(jsondb->dict, table);
    if (table_dict == NULL)
	return;

    ctx.iter_ctx = iter_data;
    ctx.iter_f = iter_f;

    heim_dict_iterate_f(table_dict, &ctx, json_db_iter_f);
}

static struct heim_db_type json_dbt = {
    1, json_db_open, NULL, json_db_close,
    json_db_lock, json_db_unlock, json_db_sync,
    NULL, NULL, NULL,
    json_db_copy_value, json_db_set_value,
    json_db_del_key, json_db_iter
};

