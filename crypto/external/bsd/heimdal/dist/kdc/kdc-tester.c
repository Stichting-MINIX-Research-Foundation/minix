/*	$NetBSD: kdc-tester.c,v 1.2 2017/01/28 21:31:44 christos Exp $	*/

/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include "kdc_locl.h"
#include "send_to_kdc_plugin.h"

struct perf {
    unsigned long as_req;
    unsigned long tgs_req;
    struct timeval start;
    struct timeval stop;
    struct perf *next;
} *ptop;

int detach_from_console = -1;
int daemon_child = -1;
int do_bonjour = -1;

static krb5_kdc_configuration *kdc_config;
static krb5_context kdc_context;

static struct sockaddr_storage sa;
static const char *astr = "0.0.0.0";

static void eval_object(heim_object_t);


/*
 *
 */

static krb5_error_code
plugin_init(krb5_context context, void **pctx)
{
    *pctx = NULL;
    return 0;
}

static void
plugin_fini(void *ctx)
{
}

static krb5_error_code
plugin_send_to_kdc(krb5_context context,
		   void *ctx,
		   krb5_krbhst_info *ho,
		   time_t timeout,
		   const krb5_data *in,
		   krb5_data *out)
{
    return KRB5_PLUGIN_NO_HANDLE;
}

static krb5_error_code
plugin_send_to_realm(krb5_context context,
		     void *ctx,
		     krb5_const_realm realm,
		     time_t timeout,
		     const krb5_data *in,
		     krb5_data *out)
{
    int ret;

    krb5_kdc_update_time(NULL);

    ret = krb5_kdc_process_request(kdc_context, kdc_config,
				   in->data, in->length,
				   out, NULL, astr,
				   (struct sockaddr *)&sa, 0);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_kdc_process_request");

    return 0;
}

static krb5plugin_send_to_kdc_ftable send_to_kdc = {
    KRB5_PLUGIN_SEND_TO_KDC_VERSION_2,
    plugin_init,
    plugin_fini,
    plugin_send_to_kdc,
    plugin_send_to_realm
};

static void
perf_start(struct perf *perf)
{
    memset(perf, 0, sizeof(*perf));

    gettimeofday(&perf->start, NULL);
    perf->next = ptop;
    ptop = perf;
}

static void
perf_stop(struct perf *perf)
{
    gettimeofday(&perf->stop, NULL);
    ptop = perf->next;

    if (ptop) {
	ptop->as_req += perf->as_req;
	ptop->tgs_req += perf->tgs_req;
    }

    timevalsub(&perf->stop, &perf->start);
    printf("time: %lu.%06lu\n",
	   (unsigned long)perf->stop.tv_sec,
	   (unsigned long)perf->stop.tv_usec);

#define USEC_PER_SEC 1000000

    if (perf->as_req) {
	double as_ps = 0.0;
	as_ps = (perf->as_req * USEC_PER_SEC) / (double)((perf->stop.tv_sec * USEC_PER_SEC) + perf->stop.tv_usec);
	printf("as-req/s %.2lf  (total %lu requests)\n", as_ps, perf->as_req);
    }
	    
    if (perf->tgs_req) {
	double tgs_ps = 0.0;
	tgs_ps = (perf->tgs_req * USEC_PER_SEC) / (double)((perf->stop.tv_sec * USEC_PER_SEC) + perf->stop.tv_usec);
	printf("tgs-req/s %.2lf (total %lu requests)\n", tgs_ps, perf->tgs_req);
    }
}

/*
 *
 */

static void
eval_repeat(heim_dict_t o)
{
    heim_object_t or = heim_dict_get_value(o, HSTR("value"));
    heim_number_t n = heim_dict_get_value(o, HSTR("num"));
    int i, num;
    struct perf perf;

    perf_start(&perf);

    heim_assert(or != NULL, "value missing");
    heim_assert(n != NULL, "num missing");

    num = heim_number_get_int(n);
    heim_assert(num >= 0, "num >= 0");

    for (i = 0; i < num; i++)
	eval_object(or);

    perf_stop(&perf);
}

/*
 *
 */

static krb5_error_code
copy_keytab(krb5_context context, krb5_keytab from, krb5_keytab to)
{
    krb5_keytab_entry entry;
    krb5_kt_cursor cursor;
    krb5_error_code ret;

    ret = krb5_kt_start_seq_get(context, from, &cursor);
    if (ret)
	return ret;
    while((ret = krb5_kt_next_entry(context, from, &entry, &cursor)) == 0){
	krb5_kt_add_entry(context, to, &entry);
	krb5_kt_free_entry(context, &entry);
    }
    return krb5_kt_end_seq_get(context, from, &cursor);
}	    

/*
 *
 */

static void
eval_kinit(heim_dict_t o)
{
    heim_string_t user, password, keytab, fast_armor_cc, pk_user_id, ccache;
    krb5_get_init_creds_opt *opt;
    krb5_init_creds_context ctx;
    krb5_principal client;
    krb5_keytab ktmem = NULL;
    krb5_ccache fast_cc = NULL;
    krb5_error_code ret;

    if (ptop)
	ptop->as_req++;

    user = heim_dict_get_value(o, HSTR("client"));
    if (user == NULL)
	krb5_errx(kdc_context, 1, "no client");

    password = heim_dict_get_value(o, HSTR("password"));
    keytab = heim_dict_get_value(o, HSTR("keytab"));
    pk_user_id = heim_dict_get_value(o, HSTR("pkinit-user-cert-id"));
    if (password == NULL && keytab == NULL && pk_user_id == NULL)
	krb5_errx(kdc_context, 1, "password, keytab, nor PKINIT user cert ID");

    ccache = heim_dict_get_value(o, HSTR("ccache"));

    ret = krb5_parse_name(kdc_context, heim_string_get_utf8(user), &client);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_unparse_name");

    /* PKINIT parts */
    ret = krb5_get_init_creds_opt_alloc (kdc_context, &opt);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_get_init_creds_opt_alloc");

    if (pk_user_id) {
	heim_bool_t rsaobj = heim_dict_get_value(o, HSTR("pkinit-use-rsa"));
	int use_rsa = rsaobj ? heim_bool_val(rsaobj) : 0;

	ret = krb5_get_init_creds_opt_set_pkinit(kdc_context, opt,
						 client,
						 heim_string_get_utf8(pk_user_id),
						 NULL, NULL, NULL,
						 use_rsa ? 2 : 0,
						 NULL, NULL, NULL);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_get_init_creds_opt_set_pkinit");
    }

    ret = krb5_init_creds_init(kdc_context, client, NULL, NULL, 0, opt, &ctx);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_init_creds_init");

    fast_armor_cc = heim_dict_get_value(o, HSTR("fast-armor-cc"));
    if (fast_armor_cc) {

	ret = krb5_cc_resolve(kdc_context, heim_string_get_utf8(fast_armor_cc), &fast_cc);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_cc_resolve");

	ret = krb5_init_creds_set_fast_ccache(kdc_context, ctx, fast_cc);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_init_creds_set_fast_ccache");
    }
    
    if (password) {
	ret = krb5_init_creds_set_password(kdc_context, ctx, 
					   heim_string_get_utf8(password));
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_init_creds_set_password");
    }
    if (keytab) {
	krb5_keytab kt = NULL;

	ret = krb5_kt_resolve(kdc_context, heim_string_get_utf8(keytab), &kt);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_kt_resolve");

	ret = krb5_kt_resolve(kdc_context, "MEMORY:keytab", &ktmem);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_kt_resolve(MEMORY)");

	ret = copy_keytab(kdc_context, kt, ktmem);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "copy_keytab");

	krb5_kt_close(kdc_context, kt);

	ret = krb5_init_creds_set_keytab(kdc_context, ctx, ktmem);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_init_creds_set_keytab");
    }

    ret = krb5_init_creds_get(kdc_context, ctx);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_init_creds_get");

    if (ccache) {
	const char *name = heim_string_get_utf8(ccache);
	krb5_creds cred;
	krb5_ccache cc;

	ret = krb5_init_creds_get_creds(kdc_context, ctx, &cred);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_init_creds_get_creds");

	ret = krb5_cc_resolve(kdc_context, name, &cc);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_cc_resolve");

	krb5_init_creds_store(kdc_context, ctx, cc);

	ret = krb5_cc_close(kdc_context, cc);
	if (ret)
	    krb5_err(kdc_context, 1, ret, "krb5_cc_close");

	krb5_free_cred_contents(kdc_context, &cred);
    }

    krb5_init_creds_free(kdc_context, ctx);

    if (ktmem)
	krb5_kt_close(kdc_context, ktmem);
    if (fast_cc)
	krb5_cc_close(kdc_context, fast_cc);
}

/*
 *
 */

static void
eval_kgetcred(heim_dict_t o)
{
    heim_string_t server, ccache;
    krb5_get_creds_opt opt;
    heim_bool_t nostore;
    krb5_error_code ret;
    krb5_ccache cc = NULL;
    krb5_principal s;
    krb5_creds *out = NULL;

    if (ptop)
	ptop->tgs_req++;

    server = heim_dict_get_value(o, HSTR("server"));
    if (server == NULL)
	krb5_errx(kdc_context, 1, "no server");

    ccache = heim_dict_get_value(o, HSTR("ccache"));
    if (ccache == NULL)
	krb5_errx(kdc_context, 1, "no ccache");

    nostore = heim_dict_get_value(o, HSTR("nostore"));
    if (nostore == NULL)
	nostore = heim_bool_create(1);

    ret = krb5_cc_resolve(kdc_context, heim_string_get_utf8(ccache), &cc);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_cc_resolve");

    ret = krb5_parse_name(kdc_context, heim_string_get_utf8(server), &s);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_parse_name");

    ret = krb5_get_creds_opt_alloc(kdc_context, &opt);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_get_creds_opt_alloc");

    if (heim_bool_val(nostore))
	krb5_get_creds_opt_add_options(kdc_context, opt, KRB5_GC_NO_STORE);

    ret = krb5_get_creds(kdc_context, opt, cc, s, &out);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_get_creds");
    
    krb5_free_creds(kdc_context, out);
    krb5_free_principal(kdc_context, s);
    krb5_get_creds_opt_free(kdc_context, opt);
    krb5_cc_close(kdc_context, cc);
}


/*
 *
 */

static void
eval_kdestroy(heim_dict_t o)
{
    heim_string_t ccache = heim_dict_get_value(o, HSTR("ccache"));;
    krb5_error_code ret;
    const char *name;
    krb5_ccache cc;

    heim_assert(ccache != NULL, "ccache_missing");
	
    name = heim_string_get_utf8(ccache);

    ret = krb5_cc_resolve(kdc_context, name, &cc);
    if (ret)
	krb5_err(kdc_context, 1, ret, "krb5_cc_resolve");

    krb5_cc_destroy(kdc_context, cc);
}


/*
 *
 */

static void
eval_array_element(heim_object_t o, void *ptr, int *stop)
{
    eval_object(o);
}

static void
eval_object(heim_object_t o)
{
    heim_tid_t t = heim_get_tid(o);

    if (t == heim_array_get_type_id()) {
	heim_array_iterate_f(o, NULL, eval_array_element);
    } else if (t == heim_dict_get_type_id()) {
	const char *op = heim_dict_get_value(o, HSTR("op"));

	heim_assert(op != NULL, "op missing");

	if (strcmp(op, "repeat") == 0) {
	    eval_repeat(o);
	} else if (strcmp(op, "kinit") == 0) {
	    eval_kinit(o);
	} else if (strcmp(op, "kgetcred") == 0) {
	    eval_kgetcred(o);
	} else if (strcmp(op, "kdestroy") == 0) {
	    eval_kdestroy(o);
	} else {
	    errx(1, "unsupported ops %s", op);
	}

    } else
	errx(1, "unsupported");
}


int
main(int argc, char **argv)
{
    krb5_error_code ret;
    int optidx = 0;

    setprogname(argv[0]);

    ret = krb5_init_context(&kdc_context);
    if (ret == KRB5_CONFIG_BADFORMAT)
	errx (1, "krb5_init_context failed to parse configuration file");
    else if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    ret = krb5_kt_register(kdc_context, &hdb_get_kt_ops);
    if (ret)
	errx (1, "krb5_kt_register(HDB) failed: %d", ret);

    kdc_config = configure(kdc_context, argc, argv, &optidx);

    argc -= optidx;
    argv += optidx;

    if (argc == 0)
	errx(1, "missing operations");

    krb5_plugin_register(kdc_context, PLUGIN_TYPE_DATA,
			 KRB5_PLUGIN_SEND_TO_KDC, &send_to_kdc);

    {
	void *buf;
	size_t size;
	heim_object_t o;

	if (rk_undumpdata(argv[0], &buf, &size))
	    errx(1, "undumpdata: %s", argv[0]);
	
	o = heim_json_create_with_bytes(buf, size, 10, 0, NULL);
	free(buf);
	if (o == NULL)
	    errx(1, "heim_json");
	
	/*
	 * do the work here
	 */
	
	eval_object(o);

	heim_release(o);
    }

    krb5_free_context(kdc_context);
    return 0;
}
