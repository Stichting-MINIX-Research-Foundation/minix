/*	$NetBSD: db_plugin.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 */

#include "krb5_locl.h"
#include "db_plugin.h"

/* Default plugin (DB using binary search of sorted text file) follows */
static heim_base_once_t db_plugins_once = HEIM_BASE_ONCE_INIT;

static krb5_error_code KRB5_LIB_CALL
db_plugins_plcallback(krb5_context context, const void *plug, void *plugctx,
		      void *userctx)
{
    return 0;
}

static void
db_plugins_init(void *arg)
{
    krb5_context context = arg;
    (void)_krb5_plugin_run_f(context, "krb5", KRB5_PLUGIN_DB,
			     KRB5_PLUGIN_DB_VERSION_0, 0, NULL,
			     db_plugins_plcallback);
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_load_db_plugins(krb5_context context)
{
    heim_base_once_f(&db_plugins_once, context, db_plugins_init);
}

