/*	$NetBSD: hdb.h,v 1.2 2017/01/28 21:31:48 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
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

/* Id */

#ifndef __HDB_H__
#define __HDB_H__

#include <stdio.h>

#include <krb5/krb5.h>

#include <krb5/hdb_err.h>

#include <krb5/heim_asn1.h>
#include <krb5/hdb_asn1.h>

struct hdb_dbinfo;

enum hdb_lockop{ HDB_RLOCK, HDB_WLOCK };

/* flags for various functions */
#define HDB_F_DECRYPT		1	/* decrypt keys */
#define HDB_F_REPLACE		2	/* replace entry */
#define HDB_F_GET_CLIENT	4	/* fetch client */
#define HDB_F_GET_SERVER	8	/* fetch server */
#define HDB_F_GET_KRBTGT	16	/* fetch krbtgt */
#define HDB_F_GET_ANY		28	/* fetch any of client,server,krbtgt */
#define HDB_F_CANON		32	/* want canonicalition */
#define HDB_F_ADMIN_DATA	64	/* want data that kdc don't use  */
#define HDB_F_KVNO_SPECIFIED	128	/* we want a particular KVNO */
#define HDB_F_CURRENT_KVNO	256	/* we want the current KVNO */
#define HDB_F_LIVE_CLNT_KVNOS	512	/* we want all live keys for pre-auth */
#define HDB_F_LIVE_SVC_KVNOS	1024	/* we want all live keys for tix */
#define HDB_F_ALL_KVNOS		2048	/* we want all the keys, live or not */
#define HDB_F_FOR_AS_REQ	4096	/* fetch is for a AS REQ */
#define HDB_F_FOR_TGS_REQ	8192	/* fetch is for a TGS REQ */
#define HDB_F_PRECHECK		16384	/* check that the operation would succeed */

/* hdb_capability_flags */
#define HDB_CAP_F_HANDLE_ENTERPRISE_PRINCIPAL 1
#define HDB_CAP_F_HANDLE_PASSWORDS	2
#define HDB_CAP_F_PASSWORD_UPDATE_KEYS	4
#define HDB_CAP_F_SHARED_DIRECTORY      8

/* auth status values */
#define HDB_AUTH_SUCCESS		0
#define HDB_AUTH_WRONG_PASSWORD		1
#define HDB_AUTH_INVALID_SIGNATURE	2

/* key usage for master key */
#define HDB_KU_MKEY	0x484442

typedef struct hdb_master_key_data *hdb_master_key;

/**
 * hdb_entry_ex is a wrapper structure around the hdb_entry structure
 * that allows backends to keep a pointer to the backing store, ie in
 * ->hdb_fetch_kvno(), so that we the kadmin/kpasswd backend gets around to
 * ->hdb_store(), the backend doesn't need to lookup the entry again.
 */

typedef struct hdb_entry_ex {
    void *ctx;
    hdb_entry entry;
    void (*free_entry)(krb5_context, struct hdb_entry_ex *);
} hdb_entry_ex;


/**
 * HDB backend function pointer structure
 *
 * The HDB structure is what the KDC and kadmind framework uses to
 * query the backend database when talking about principals.
 */

typedef struct HDB {
    void *hdb_db;
    void *hdb_dbc; /** don't use, only for DB3 */
    char *hdb_name;
    int hdb_master_key_set;
    hdb_master_key hdb_master_key;
    int hdb_openp;
    int hdb_capability_flags;
    int lock_count;
    int lock_type;
    /**
     * Open (or create) the a Kerberos database.
     *
     * Open (or create) the a Kerberos database that was resolved with
     * hdb_create(). The third and fourth flag to the function are the
     * same as open(), thus passing O_CREAT will create the data base
     * if it doesn't exists.
     *
     * Then done the caller should call hdb_close(), and to release
     * all resources hdb_destroy().
     */
    krb5_error_code (*hdb_open)(krb5_context, struct HDB*, int, mode_t);
    /**
     * Close the database for transaction
     *
     * Closes the database for further transactions, wont release any
     * permanant resources. the database can be ->hdb_open-ed again.
     */
    krb5_error_code (*hdb_close)(krb5_context, struct HDB*);
    /**
     * Free an entry after use.
     */
    void	    (*hdb_free)(krb5_context, struct HDB*, hdb_entry_ex*);
    /**
     * Fetch an entry from the backend
     *
     * Fetch an entry from the backend, flags are what type of entry
     * should be fetch: client, server, krbtgt.
     * knvo (if specified and flags HDB_F_KVNO_SPECIFIED set) is the kvno to get
     */
    krb5_error_code (*hdb_fetch_kvno)(krb5_context, struct HDB*,
				      krb5_const_principal, unsigned, krb5_kvno,
				      hdb_entry_ex*);
    /**
     * Store an entry to database
     */
    krb5_error_code (*hdb_store)(krb5_context, struct HDB*,
				 unsigned, hdb_entry_ex*);
    /**
     * Remove an entry from the database.
     */
    krb5_error_code (*hdb_remove)(krb5_context, struct HDB*,
				  unsigned, krb5_const_principal);
    /**
     * As part of iteration, fetch one entry
     */
    krb5_error_code (*hdb_firstkey)(krb5_context, struct HDB*,
				    unsigned, hdb_entry_ex*);
    /**
     * As part of iteration, fetch next entry
     */
    krb5_error_code (*hdb_nextkey)(krb5_context, struct HDB*,
				   unsigned, hdb_entry_ex*);
    /**
     * Lock database
     *
     * A lock can only be held by one consumers. Transaction can still
     * happen on the database while the lock is held, so the entry is
     * only useful for syncroning creation of the database and renaming of the database.
     */
    krb5_error_code (*hdb_lock)(krb5_context, struct HDB*, int);
    /**
     * Unlock database
     */
    krb5_error_code (*hdb_unlock)(krb5_context, struct HDB*);
    /**
     * Rename the data base.
     *
     * Assume that the database is not hdb_open'ed and not locked.
     */
    krb5_error_code (*hdb_rename)(krb5_context, struct HDB*, const char*);
    /**
     * Get an hdb_entry from a classical DB backend
     *
     * This function takes a principal key (krb5_data) and returns all
     * data related to principal in the return krb5_data. The returned
     * encoded entry is of type hdb_entry or hdb_entry_alias.
     */
    krb5_error_code (*hdb__get)(krb5_context, struct HDB*,
				krb5_data, krb5_data*);
    /**
     * Store an hdb_entry from a classical DB backend
     *
     * This function takes a principal key (krb5_data) and encoded
     * hdb_entry or hdb_entry_alias as the data to store.
     *
     * For a file-based DB, this must synchronize to disk when done.
     * This is sub-optimal for kadm5_s_rename_principal(), and for
     * kadm5_s_modify_principal() when using principal aliases; to
     * improve this so that only one fsync() need be done
     * per-transaction will require HDB API extensions.
     */
    krb5_error_code (*hdb__put)(krb5_context, struct HDB*, int,
				krb5_data, krb5_data);
    /**
     * Delete and hdb_entry from a classical DB backend
     *
     * This function takes a principal key (krb5_data) naming the record
     * to delete.
     *
     * Same discussion as in @ref HDB::hdb__put
     */
    krb5_error_code (*hdb__del)(krb5_context, struct HDB*, krb5_data);
    /**
     * Destroy the handle to the database.
     *
     * Destroy the handle to the database, deallocate all memory and
     * related resources. Does not remove any permanent data. Its the
     * logical reverse of hdb_create() function that is the entry
     * point for the module.
     */
    krb5_error_code (*hdb_destroy)(krb5_context, struct HDB*);
    /**
     * Get the list of realms this backend handles.
     * This call is optional to support. The returned realms are used
     * for announcing the realms over bonjour. Free returned array
     * with krb5_free_host_realm().
     */
    krb5_error_code (*hdb_get_realms)(krb5_context, struct HDB *, krb5_realm **);
    /**
     * Change password.
     *
     * Will update keys for the entry when given password.  The new
     * keys must be written into the entry and will then later be
     * ->hdb_store() into the database. The backend will still perform
     * all other operations, increasing the kvno, and update
     * modification timestamp.
     *
     * The backend needs to call _kadm5_set_keys() and perform password
     * quality checks.
     */
    krb5_error_code (*hdb_password)(krb5_context, struct HDB*, hdb_entry_ex*, const char *, int);

    /**
     * Auth feedback
     *
     * This is a feedback call that allows backends that provides
     * lockout functionality to register failure and/or successes.
     *
     * In case the entry is locked out, the backend should set the
     * hdb_entry.flags.locked-out flag.
     */
    krb5_error_code (*hdb_auth_status)(krb5_context, struct HDB *, hdb_entry_ex *, int);
    /**
     * Check if delegation is allowed.
     */
    krb5_error_code (*hdb_check_constrained_delegation)(krb5_context, struct HDB *, hdb_entry_ex *, krb5_const_principal);

    /**
     * Check if this name is an alias for the supplied client for PKINIT userPrinicpalName logins
     */
    krb5_error_code (*hdb_check_pkinit_ms_upn_match)(krb5_context, struct HDB *, hdb_entry_ex *, krb5_const_principal);

    /**
     * Check if s4u2self is allowed from this client to this server
     */
    krb5_error_code (*hdb_check_s4u2self)(krb5_context, struct HDB *, hdb_entry_ex *, krb5_const_principal);
}HDB;

#define HDB_INTERFACE_VERSION	9

struct hdb_method {
    int			version;
    krb5_error_code	(*init)(krb5_context, void **);
    void		(*fini)(void *);
    const char *prefix;
    krb5_error_code (*create)(krb5_context, HDB **, const char *filename);
};

/* dump entry format, for hdb_print_entry() */
typedef enum hdb_dump_format {
    HDB_DUMP_HEIMDAL = 0,
    HDB_DUMP_MIT = 1,
} hdb_dump_format_t;

struct hdb_print_entry_arg {
    FILE *out;
    hdb_dump_format_t fmt;
};

typedef krb5_error_code (*hdb_foreach_func_t)(krb5_context, HDB*,
					      hdb_entry_ex*, void*);
extern krb5_kt_ops hdb_kt_ops;
extern krb5_kt_ops hdb_get_kt_ops;

extern const int hdb_interface_version;

#include <krb5/hdb-protos.h>

#endif /* __HDB_H__ */
