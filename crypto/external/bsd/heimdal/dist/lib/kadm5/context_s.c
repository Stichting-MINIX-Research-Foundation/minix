/*	$NetBSD: context_s.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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

#include "kadm5_locl.h"

__RCSID("$NetBSD: context_s.c,v 1.2 2017/01/28 21:31:49 christos Exp $");

static kadm5_ret_t
kadm5_s_lock(void *server_handle)
{
    kadm5_server_context *context = server_handle;
    kadm5_ret_t ret;

    if (context->keep_open) {
	/*
	 * We open/close around every operation, but we retain the DB
	 * open if the DB was locked with a prior call to kadm5_lock(),
	 * so if it's open here that must be because the DB is locked.
	 */
	heim_assert(context->db->lock_count > 0,
		    "Internal error in tracking HDB locks");
	return KADM5_ALREADY_LOCKED;
    }

    ret = context->db->hdb_open(context->context, context->db, O_RDWR, 0);
    if (ret)
	return ret;

    ret = context->db->hdb_lock(context->context, context->db, HDB_WLOCK);
    if (ret) {
        (void) context->db->hdb_close(context->context, context->db);
	return ret;
    }

    /*
     * Attempt to recover the log.  This will generally fail on slaves,
     * and we can't tell if we're on a slave here.
     *
     * Perhaps we could set a flag in the kadm5_server_context to
     * indicate whether a read has been done without recovering the log,
     * in which case we could fail any subsequent writes.
     */
    if (kadm5_log_init(context) == 0)
        (void) kadm5_log_end(context);

    context->keep_open = 1;
    return 0;
}

static kadm5_ret_t
kadm5_s_unlock(void *server_handle)
{
    kadm5_server_context *context = server_handle;
    kadm5_ret_t ret;

    if (!context->keep_open)
	return KADM5_NOT_LOCKED;

    context->keep_open = 0;
    ret = context->db->hdb_unlock(context->context, context->db);
    (void) context->db->hdb_close(context->context, context->db);
    return ret;
}

static void
set_funcs(kadm5_server_context *c)
{
#define SET(C, F) (C)->funcs.F = kadm5_s_ ## F
    SET(c, chpass_principal);
    SET(c, chpass_principal_with_key);
    SET(c, create_principal);
    SET(c, delete_principal);
    SET(c, destroy);
    SET(c, flush);
    SET(c, get_principal);
    SET(c, get_principals);
    SET(c, get_privs);
    SET(c, modify_principal);
    SET(c, randkey_principal);
    SET(c, rename_principal);
    SET(c, lock);
    SET(c, unlock);
    SET(c, setkey_principal_3);
}

#ifndef NO_UNIX_SOCKETS

static void
set_socket_name(krb5_context context, struct sockaddr_un *un)
{
    const char *fn = kadm5_log_signal_socket(context);

    memset(un, 0, sizeof(*un));
    un->sun_family = AF_UNIX;
    strlcpy (un->sun_path, fn, sizeof(un->sun_path));

}
#else

static void
set_socket_info(krb5_context context, struct addrinfo **info)
{
    kadm5_log_signal_socket_info(context, 0, info);
}

#endif

static kadm5_ret_t
find_db_spec(kadm5_server_context *ctx)
{
    krb5_context context = ctx->context;
    struct hdb_dbinfo *info, *d;
    krb5_error_code ret;
    int aret;

    if (ctx->config.realm) {
	/* fetch the databases */
	ret = hdb_get_dbinfo(context, &info);
	if (ret)
	    return ret;

	d = NULL;
	while ((d = hdb_dbinfo_get_next(info, d)) != NULL) {
	    const char *p = hdb_dbinfo_get_realm(context, d);

	    /* match default (realm-less) */
	    if(p != NULL && strcmp(ctx->config.realm, p) != 0)
		continue;

	    p = hdb_dbinfo_get_dbname(context, d);
	    if (p) {
		ctx->config.dbname = strdup(p);
                if (ctx->config.dbname == NULL)
                    return ENOMEM;
            }

	    p = hdb_dbinfo_get_acl_file(context, d);
	    if (p) {
		ctx->config.acl_file = strdup(p);
                if (ctx->config.acl_file == NULL)
                    return ENOMEM;
            }

	    p = hdb_dbinfo_get_mkey_file(context, d);
	    if (p) {
		ctx->config.stash_file = strdup(p);
                if (ctx->config.stash_file == NULL)
                    return ENOMEM;
            }

	    p = hdb_dbinfo_get_log_file(context, d);
	    if (p) {
		ctx->log_context.log_file = strdup(p);
                if (ctx->log_context.log_file == NULL)
                    return ENOMEM;
            }
	    break;
	}
	hdb_free_dbinfo(context, &info);
    }

    /* If any of the values was unset, pick up the default value */

    if (ctx->config.dbname == NULL) {
	ctx->config.dbname = strdup(hdb_default_db(context));
        if (ctx->config.dbname == NULL)
            return ENOMEM;
    }
    if (ctx->config.acl_file == NULL) {
	aret = asprintf(&ctx->config.acl_file, "%s/kadmind.acl",
			hdb_db_dir(context));
	if (aret == -1)
	    return ENOMEM;
    }
    if (ctx->config.stash_file == NULL) {
	aret = asprintf(&ctx->config.stash_file, "%s/m-key",
			hdb_db_dir(context));
	if (aret == -1)
	    return ENOMEM;
    }
    if (ctx->log_context.log_file == NULL) {
	aret = asprintf(&ctx->log_context.log_file, "%s/log",
			hdb_db_dir(context));
	if (aret == -1)
	    return ENOMEM;
    }

#ifndef NO_UNIX_SOCKETS
    set_socket_name(context, &ctx->log_context.socket_name);
#else
    set_socket_info(context, &ctx->log_context.socket_info);
#endif

    return 0;
}

kadm5_ret_t
_kadm5_s_init_context(kadm5_server_context **ctx,
		      kadm5_config_params *params,
		      krb5_context context)
{
    kadm5_ret_t ret = 0;

    *ctx = calloc(1, sizeof(**ctx));
    if (*ctx == NULL)
	return ENOMEM;
    (*ctx)->log_context.socket_fd = rk_INVALID_SOCKET;

    set_funcs(*ctx);
    (*ctx)->context = context;
    krb5_add_et_list (context, initialize_kadm5_error_table_r);

#define is_set(M) (params && params->mask & KADM5_CONFIG_ ## M)
    if (is_set(REALM)) {
	(*ctx)->config.realm = strdup(params->realm);
        if ((*ctx)->config.realm == NULL)
            return ENOMEM;
    } else {
	ret = krb5_get_default_realm(context, &(*ctx)->config.realm);
        if (ret)
            return ret;
    }
    if (is_set(DBNAME)) {
	(*ctx)->config.dbname = strdup(params->dbname);
        if ((*ctx)->config.dbname == NULL)
            return ENOMEM;
    }
    if (is_set(ACL_FILE)) {
	(*ctx)->config.acl_file = strdup(params->acl_file);
        if ((*ctx)->config.acl_file == NULL)
            return ENOMEM;
    }
    if (is_set(STASH_FILE)) {
	(*ctx)->config.stash_file = strdup(params->stash_file);
        if ((*ctx)->config.stash_file == NULL)
            return ENOMEM;
    }

    find_db_spec(*ctx);

    /* PROFILE can't be specified for now */
    /* KADMIND_PORT is supposed to be used on the server also,
       but this doesn't make sense */
    /* ADMIN_SERVER is client only */
    /* ADNAME is not used at all (as far as I can tell) */
    /* ADB_LOCKFILE ditto */
    /* DICT_FILE */
    /* ADMIN_KEYTAB */
    /* MKEY_FROM_KEYBOARD is not supported */
    /* MKEY_NAME neither */
    /* ENCTYPE */
    /* MAX_LIFE */
    /* MAX_RLIFE */
    /* EXPIRATION */
    /* FLAGS */
    /* ENCTYPES */

    return 0;
}

HDB *
_kadm5_s_get_db(void *server_handle)
{
    kadm5_server_context *context = server_handle;
    return context->db;
}
