/*	$NetBSD: kinit.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 1997-2007 Kungliga Tekniska Högskolan
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

#include "kuser_locl.h"

#ifdef __APPLE__
#include <Security/Security.h>
#endif

#ifndef NO_NTLM
#include <krb5/heimntlm.h>
#endif

#ifndef SIGINFO
#define SIGINFO SIGUSR1
#endif

int forwardable_flag	= -1;
int proxiable_flag	= -1;
int renewable_flag	= -1;
int renew_flag		= 0;
int pac_flag		= -1;
int validate_flag	= 0;
int version_flag	= 0;
int help_flag		= 0;
int addrs_flag		= -1;
struct getarg_strings extra_addresses;
int anonymous_flag	= 0;
char *lifetime 		= NULL;
char *renew_life	= NULL;
char *server_str	= NULL;
char *cred_cache	= NULL;
char *start_str		= NULL;
static int switch_cache_flags = 1;
struct getarg_strings etype_str;
int use_keytab		= 0;
char *keytab_str	= NULL;
static krb5_keytab kt	= NULL;
int do_afslog		= -1;
int fcache_version;
char *password_file	= NULL;
char *pk_user_id	= NULL;
int pk_enterprise_flag = 0;
struct hx509_certs_data *ent_user_id = NULL;
char *pk_x509_anchors	= NULL;
int pk_use_enckey	= 0;
static int canonicalize_flag = 0;
static int enterprise_flag = 0;
static int ok_as_delegate_flag = 0;
static char *fast_armor_cache_string = NULL;
static int use_referrals_flag = 0;
static int windows_flag = 0;
#ifndef NO_NTLM
static char *ntlm_domain;
#endif


static struct getargs args[] = {
    /*
     * used by MIT
     * a: ~A
     * V: verbose
     * F: ~f
     * P: ~p
     * C: v4 cache name?
     * 5:
     *
     * old flags
     * 4:
     * 9:
     */
    { "afslog", 	0  , arg_flag, &do_afslog,
      NP_("obtain afs tokens", ""), NULL },

    { "cache", 		'c', arg_string, &cred_cache,
      NP_("credentials cache", ""), "cachename" },

    { "forwardable",	'F', arg_negative_flag, &forwardable_flag,
      NP_("get tickets not forwardable", ""), NULL },

    { NULL,		'f', arg_flag, &forwardable_flag,
      NP_("get forwardable tickets", ""), NULL },

    { "keytab",         't', arg_string, &keytab_str,
      NP_("keytab to use", ""), "keytabname" },

    { "lifetime",	'l', arg_string, &lifetime,
      NP_("lifetime of tickets", ""), "time" },

    { "proxiable",	'p', arg_flag, &proxiable_flag,
      NP_("get proxiable tickets", ""), NULL },

    { "renew",          'R', arg_flag, &renew_flag,
      NP_("renew TGT", ""), NULL },

    { "renewable",	0,   arg_flag, &renewable_flag,
      NP_("get renewable tickets", ""), NULL },

    { "renewable-life",	'r', arg_string, &renew_life,
      NP_("renewable lifetime of tickets", ""), "time" },

    { "server", 	'S', arg_string, &server_str,
      NP_("server to get ticket for", ""), "principal" },

    { "start-time",	's', arg_string, &start_str,
      NP_("when ticket gets valid", ""), "time" },

    { "use-keytab",     'k', arg_flag, &use_keytab,
      NP_("get key from keytab", ""), NULL },

    { "validate",	'v', arg_flag, &validate_flag,
      NP_("validate TGT", ""), NULL },

    { "enctypes",	'e', arg_strings, &etype_str,
      NP_("encryption types to use", ""), "enctypes" },

    { "fcache-version", 0,   arg_integer, &fcache_version,
      NP_("file cache version to create", ""), NULL },

    { "addresses",	'A',   arg_negative_flag,	&addrs_flag,
      NP_("request a ticket with no addresses", ""), NULL },

    { "extra-addresses",'a', arg_strings,	&extra_addresses,
      NP_("include these extra addresses", ""), "addresses" },

    { "anonymous",	0,   arg_flag,	&anonymous_flag,
      NP_("request an anonymous ticket", ""), NULL },

    { "request-pac",	0,   arg_flag,	&pac_flag,
      NP_("request a Windows PAC", ""), NULL },

    { "password-file",	0,   arg_string, &password_file,
      NP_("read the password from a file", ""), NULL },

    { "canonicalize",0,   arg_flag, &canonicalize_flag,
      NP_("canonicalize client principal", ""), NULL },

    { "enterprise",0,   arg_flag, &enterprise_flag,
      NP_("parse principal as a KRB5-NT-ENTERPRISE name", ""), NULL },
#ifdef PKINIT
    { "pk-enterprise",	0,	arg_flag,	&pk_enterprise_flag,
      NP_("use enterprise name from certificate", ""), NULL },

    { "pk-user",	'C',	arg_string,	&pk_user_id,
      NP_("principal's public/private/certificate identifier", ""), "id" },

    { "x509-anchors",	'D',  arg_string, &pk_x509_anchors,
      NP_("directory with CA certificates", ""), "directory" },

    { "pk-use-enckey",	0,  arg_flag, &pk_use_enckey,
      NP_("Use RSA encrypted reply (instead of DH)", ""), NULL },
#endif
#ifndef NO_NTLM
    { "ntlm-domain",	0,  arg_string, &ntlm_domain,
      NP_("NTLM domain", ""), "domain" },
#endif

    { "change-default",  0,  arg_negative_flag, &switch_cache_flags,
      NP_("switch the default cache to the new credentials cache", ""), NULL },

    { "ok-as-delegate",	0,  arg_flag, &ok_as_delegate_flag,
      NP_("honor ok-as-delegate on tickets", ""), NULL },

    { "fast-armor-cache",	0,  arg_string, &fast_armor_cache_string,
      NP_("use this credential cache as FAST armor cache", ""), "cache" },

    { "use-referrals",	0,  arg_flag, &use_referrals_flag,
      NP_("only use referrals, no dns canalisation", ""), NULL },

    { "windows",	0,  arg_flag, &windows_flag,
      NP_("get windows behavior", ""), NULL },

    { "version", 	0,   arg_flag, &version_flag, NULL, NULL },
    { "help",		0,   arg_flag, &help_flag, NULL, NULL }
};

static void
usage(int ret)
{
    arg_printusage_i18n(args, sizeof(args)/sizeof(*args), N_("Usage: ", ""),
			NULL, "[principal [command]]", getarg_i18n);
    exit(ret);
}

static krb5_error_code
get_server(krb5_context context,
	   krb5_principal client,
	   const char *server,
	   krb5_principal *princ)
{
    krb5_const_realm realm;
    if (server)
	return krb5_parse_name(context, server, princ);

    realm = krb5_principal_get_realm(context, client);
    return krb5_make_principal(context, princ, realm,
			       KRB5_TGS_NAME, realm, NULL);
}

static krb5_error_code
copy_configs(krb5_context context,
	     krb5_ccache dst,
	     krb5_ccache src,
	     krb5_principal start_ticket_server)
{
    krb5_error_code ret;
    const char *cfg_names[] = {"realm-config", "FriendlyName", NULL};
    const char *cfg_names_w_pname[] = {"fast_avail", NULL};
    krb5_data cfg_data;
    size_t i;

    for (i = 0; cfg_names[i]; i++) {
	ret = krb5_cc_get_config(context, src, NULL, cfg_names[i], &cfg_data);
	if (ret == KRB5_CC_NOTFOUND || ret == KRB5_CC_END) {
	    continue;
	} else if (ret) {
	    krb5_warn(context, ret, "krb5_cc_get_config");
	    return ret;
	}
	ret = krb5_cc_set_config(context, dst, NULL, cfg_names[i], &cfg_data);
	if (ret)
	    krb5_warn(context, ret, "krb5_cc_set_config");
    }
    for (i = 0; start_ticket_server && cfg_names_w_pname[i]; i++) {
	ret = krb5_cc_get_config(context, src, start_ticket_server,
				 cfg_names_w_pname[i], &cfg_data);
	if (ret == KRB5_CC_NOTFOUND || ret == KRB5_CC_END) {
	    continue;
	} else if (ret) {
	    krb5_warn(context, ret, "krb5_cc_get_config");
	    return ret;
	}
	ret = krb5_cc_set_config(context, dst, start_ticket_server,
				 cfg_names_w_pname[i], &cfg_data);
	if (ret && ret != KRB5_CC_NOTFOUND)
	    krb5_warn(context, ret, "krb5_cc_set_config");
    }
    /*
     * We don't copy cc configs for any other principals though (mostly
     * those are per-target time offsets and the like, so it's bad to
     * lose them, but hardly the end of the world, and as they may not
     * expire anyways, it's good to let them go).
     */
    return 0;
}

static krb5_error_code
renew_validate(krb5_context context,
	       int renew,
	       int validate,
	       krb5_ccache cache,
	       const char *server,
	       krb5_deltat life)
{
    krb5_error_code ret;
    krb5_ccache tempccache = NULL;
    krb5_creds in, *out = NULL;
    krb5_kdc_flags flags;

    memset(&in, 0, sizeof(in));

    ret = krb5_cc_get_principal(context, cache, &in.client);
    if (ret) {
	krb5_warn(context, ret, "krb5_cc_get_principal");
	return ret;
    }
    ret = get_server(context, in.client, server, &in.server);
    if (ret) {
	krb5_warn(context, ret, "get_server");
	goto out;
    }

    if (renew) {
	/*
	 * no need to check the error here, it's only to be
	 * friendly to the user
	 */
	krb5_get_credentials(context, KRB5_GC_CACHED, cache, &in, &out);
    }

    flags.i = 0;
    flags.b.renewable         = flags.b.renew = renew;
    flags.b.validate          = validate;

    if (forwardable_flag != -1)
	flags.b.forwardable       = forwardable_flag;
    else if (out)
	flags.b.forwardable 	  = out->flags.b.forwardable;

    if (proxiable_flag != -1)
	flags.b.proxiable         = proxiable_flag;
    else if (out)
	flags.b.proxiable 	  = out->flags.b.proxiable;

    if (anonymous_flag)
	flags.b.request_anonymous = anonymous_flag;
    if (life)
	in.times.endtime = time(NULL) + life;

    if (out) {
	krb5_free_creds(context, out);
	out = NULL;
    }


    ret = krb5_get_kdc_cred(context,
			    cache,
			    flags,
			    NULL,
			    NULL,
			    &in,
			    &out);
    if (ret) {
	krb5_warn(context, ret, "krb5_get_kdc_cred");
	goto out;
    }

    ret = krb5_cc_new_unique(context, krb5_cc_get_type(context, cache),
			     NULL, &tempccache);
    if (ret) {
	krb5_warn(context, ret, "krb5_cc_new_unique");
	goto out;
    }

    ret = krb5_cc_initialize(context, tempccache, in.client);
    if (ret) {
	krb5_warn(context, ret, "krb5_cc_initialize");
	goto out;
    }

    ret = krb5_cc_store_cred(context, tempccache, out);
    if (ret) {
	krb5_warn(context, ret, "krb5_cc_store_cred");
	goto out;
    }

    /*
     * We want to preserve cc configs as some are security-relevant, and
     * anyways it's the friendly thing to do.
     */
    ret = copy_configs(context, tempccache, cache, out->server);
    if (ret)
	goto out;

    ret = krb5_cc_move(context, tempccache, cache);
    if (ret) {
	krb5_warn(context, ret, "krb5_cc_move");
	goto out;
    }
    tempccache = NULL;

out:
    if (tempccache)
	krb5_cc_close(context, tempccache);
    if (out)
	krb5_free_creds(context, out);
    krb5_free_cred_contents(context, &in);
    return ret;
}

#ifndef NO_NTLM

static krb5_error_code
store_ntlmkey(krb5_context context, krb5_ccache id,
	      const char *domain, struct ntlm_buf *buf)
{
    krb5_error_code ret;
    krb5_data data;
    char *name;
    int aret;

    ret = krb5_cc_get_config(context, id, NULL, "default-ntlm-domain", &data);
    if (ret == 0) {
        krb5_data_free(&data);
    } else {
        data.length = strlen(domain);
        data.data = rk_UNCONST(domain);
        ret = krb5_cc_set_config(context, id, NULL, "default-ntlm-domain", &data);
        if (ret != 0)
            return ret;
    }

    aret = asprintf(&name, "ntlm-key-%s", domain);
    if (aret == -1 || name == NULL)
	return krb5_enomem(context);

    data.length = buf->length;
    data.data = buf->data;

    ret = krb5_cc_set_config(context, id, NULL, name, &data);
    free(name);
    return ret;
}
#endif

static krb5_error_code
get_new_tickets(krb5_context context,
		krb5_principal principal,
		krb5_ccache ccache,
		krb5_deltat ticket_life,
		int interactive)
{
    krb5_error_code ret;
    krb5_creds cred;
    char passwd[256];
    krb5_deltat start_time = 0;
    krb5_deltat renew = 0;
    const char *renewstr = NULL;
    krb5_enctype *enctype = NULL;
    krb5_ccache tempccache = NULL;
    krb5_init_creds_context ctx = NULL;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_prompter_fct prompter = krb5_prompter_posix;
#ifndef NO_NTLM
    struct ntlm_buf ntlmkey;
    memset(&ntlmkey, 0, sizeof(ntlmkey));
#endif
    passwd[0] = '\0';

    if (!interactive)
	prompter = NULL;

    if (password_file) {
	FILE *f;

	if (strcasecmp("STDIN", password_file) == 0)
	    f = stdin;
	else
	    f = fopen(password_file, "r");
	if (f == NULL) {
	    krb5_warnx(context, "Failed to open the password file %s",
		       password_file);
	    return errno;
	}

	if (fgets(passwd, sizeof(passwd), f) == NULL) {
	    krb5_warnx(context, N_("Failed to read password from file %s", ""),
		       password_file);
	    fclose(f);
	    return EINVAL; /* XXX Need a better error */
	}
	if (f != stdin)
	    fclose(f);
	passwd[strcspn(passwd, "\n")] = '\0';
    }

#ifdef __APPLE__
    if (passwd[0] == '\0') {
	const char *realm;
	OSStatus osret;
	UInt32 length;
	void *buffer;
	char *name;

	realm = krb5_principal_get_realm(context, principal);

	ret = krb5_unparse_name_flags(context, principal,
				      KRB5_PRINCIPAL_UNPARSE_NO_REALM, &name);
	if (ret)
	    goto nopassword;

	osret = SecKeychainFindGenericPassword(NULL, strlen(realm), realm,
					       strlen(name), name,
					       &length, &buffer, NULL);
	free(name);
	if (osret == noErr && length < sizeof(passwd) - 1) {
	    memcpy(passwd, buffer, length);
	    passwd[length] = '\0';
	}
    nopassword:
	do { } while(0);
    }
#endif

    memset(&cred, 0, sizeof(cred));

    ret = krb5_get_init_creds_opt_alloc(context, &opt);
    if (ret) {
	krb5_warn(context, ret, "krb5_get_init_creds_opt_alloc");
	goto out;
    }

    krb5_get_init_creds_opt_set_default_flags(context, "kinit",
	krb5_principal_get_realm(context, principal), opt);

    if (forwardable_flag != -1)
	krb5_get_init_creds_opt_set_forwardable(opt, forwardable_flag);
    if (proxiable_flag != -1)
	krb5_get_init_creds_opt_set_proxiable(opt, proxiable_flag);
    if (anonymous_flag)
	krb5_get_init_creds_opt_set_anonymous(opt, anonymous_flag);
    if (pac_flag != -1)
	krb5_get_init_creds_opt_set_pac_request(context, opt,
						pac_flag ? TRUE : FALSE);
    if (canonicalize_flag)
	krb5_get_init_creds_opt_set_canonicalize(context, opt, TRUE);
    if (pk_enterprise_flag || enterprise_flag || canonicalize_flag || windows_flag)
	krb5_get_init_creds_opt_set_win2k(context, opt, TRUE);
    if (pk_user_id || ent_user_id || anonymous_flag) {
	ret = krb5_get_init_creds_opt_set_pkinit(context, opt,
						 principal,
						 pk_user_id,
						 pk_x509_anchors,
						 NULL,
						 NULL,
						 pk_use_enckey ? 2 : 0 |
						 anonymous_flag ? 4 : 0,
						 prompter,
						 NULL,
						 passwd);
	if (ret) {
	    krb5_warn(context, ret, "krb5_get_init_creds_opt_set_pkinit");
	    goto out;
	}
	if (ent_user_id)
	    krb5_get_init_creds_opt_set_pkinit_user_certs(context, opt, ent_user_id);
    }

    if (addrs_flag != -1)
	krb5_get_init_creds_opt_set_addressless(context, opt,
						addrs_flag ? FALSE : TRUE);

    if (renew_life == NULL && renewable_flag)
	renewstr = "6 months";
    if (renew_life)
	renewstr = renew_life;
    if (renewstr) {
	renew = parse_time(renewstr, "s");
	if (renew < 0)
	    errx(1, "unparsable time: %s", renewstr);

	krb5_get_init_creds_opt_set_renew_life(opt, renew);
    }

    if (ticket_life != 0)
	krb5_get_init_creds_opt_set_tkt_life(opt, ticket_life);

    if (start_str) {
	int tmp = parse_time(start_str, "s");
	if (tmp < 0)
	    errx(1, N_("unparsable time: %s", ""), start_str);

	start_time = tmp;
    }

    if (etype_str.num_strings) {
	int i;

	enctype = malloc(etype_str.num_strings * sizeof(*enctype));
	if (enctype == NULL)
	    errx(1, "out of memory");
	for(i = 0; i < etype_str.num_strings; i++) {
	    ret = krb5_string_to_enctype(context,
					 etype_str.strings[i],
					 &enctype[i]);
	    if (ret)
		errx(1, "unrecognized enctype: %s", etype_str.strings[i]);
	}
	krb5_get_init_creds_opt_set_etype_list(opt, enctype,
					       etype_str.num_strings);
    }

    ret = krb5_init_creds_init(context, principal, prompter, NULL, start_time, opt, &ctx);
    if (ret) {
	krb5_warn(context, ret, "krb5_init_creds_init");
	goto out;
    }

    if (server_str) {
	ret = krb5_init_creds_set_service(context, ctx, server_str);
	if (ret) {
	    krb5_warn(context, ret, "krb5_init_creds_set_service");
	    goto out;
	}
    }

    if (fast_armor_cache_string) {
	krb5_ccache fastid;
	
	ret = krb5_cc_resolve(context, fast_armor_cache_string, &fastid);
	if (ret) {
	    krb5_warn(context, ret, "krb5_cc_resolve(FAST cache)");
	    goto out;
	}
	
	ret = krb5_init_creds_set_fast_ccache(context, ctx, fastid);
	if (ret) {
	    krb5_warn(context, ret, "krb5_init_creds_set_fast_ccache");
	    goto out;
	}
    }

    if (use_keytab || keytab_str) {
	ret = krb5_init_creds_set_keytab(context, ctx, kt);
	if (ret) {
	    krb5_warn(context, ret, "krb5_init_creds_set_keytab");
	    goto out;
	}
    } else if (pk_user_id || ent_user_id || anonymous_flag) {

    } else if (!interactive && passwd[0] == '\0') {
	static int already_warned = 0;

	if (!already_warned)
	    krb5_warnx(context, "Not interactive, failed to get "
	      "initial ticket");
	krb5_get_init_creds_opt_free(context, opt);
	already_warned = 1;
	return 0;
    } else {

	if (passwd[0] == '\0') {
	    char *p, *prompt;
	    int aret = 0;

	    ret = krb5_unparse_name(context, principal, &p);
	    if (ret)
		errx(1, "failed to generate passwd prompt: not enough memory");

	    aret = asprintf(&prompt, N_("%s's Password: ", ""), p);
	    free(p);
	    if (aret == -1)
		errx(1, "failed to generate passwd prompt: not enough memory");

	    if (UI_UTIL_read_pw_string(passwd, sizeof(passwd)-1, prompt, 0)){
		memset(passwd, 0, sizeof(passwd));
		errx(1, "failed to read password");
	    }
	    free(prompt);
	}

	if (passwd[0]) {
	    ret = krb5_init_creds_set_password(context, ctx, passwd);
	    if (ret) {
		krb5_warn(context, ret, "krb5_init_creds_set_password");
		goto out;
	    }
	}
    }

    ret = krb5_init_creds_get(context, ctx);

#ifndef NO_NTLM
    if (ntlm_domain && passwd[0])
	heim_ntlm_nt_key(passwd, &ntlmkey);
#endif
    memset(passwd, 0, sizeof(passwd));

    switch(ret){
    case 0:
	break;
    case KRB5_LIBOS_PWDINTR: /* don't print anything if it was just C-c:ed */
	exit(1);
    case KRB5KRB_AP_ERR_BAD_INTEGRITY:
    case KRB5KRB_AP_ERR_MODIFIED:
    case KRB5KDC_ERR_PREAUTH_FAILED:
    case KRB5_GET_IN_TKT_LOOP:
	krb5_warnx(context, N_("Password incorrect", ""));
	goto out;
    case KRB5KRB_AP_ERR_V4_REPLY:
	krb5_warnx(context, N_("Looks like a Kerberos 4 reply", ""));
	goto out;
    case KRB5KDC_ERR_KEY_EXPIRED:
	krb5_warnx(context, N_("Password expired", ""));
	goto out;
    default:
	krb5_warn(context, ret, "krb5_get_init_creds");
	goto out;
    }

    krb5_process_last_request(context, opt, ctx);

    ret = krb5_init_creds_get_creds(context, ctx, &cred);
    if (ret) {
	krb5_warn(context, ret, "krb5_init_creds_get_creds");
	goto out;
    }

    if (ticket_life != 0) {
	if (labs(cred.times.endtime - cred.times.starttime - ticket_life) > 30) {
	    char life[64];
	    unparse_time_approx(cred.times.endtime - cred.times.starttime,
				life, sizeof(life));
	    krb5_warnx(context, N_("NOTICE: ticket lifetime is %s", ""), life);
	}
    }
    if (renew_life) {
	if (labs(cred.times.renew_till - cred.times.starttime - renew) > 30) {
	    char life[64];
	    unparse_time_approx(cred.times.renew_till - cred.times.starttime,
				life, sizeof(life));
	    krb5_warnx(context,
		       N_("NOTICE: ticket renewable lifetime is %s", ""),
		       life);
	}
    }
    krb5_free_cred_contents(context, &cred);

    ret = krb5_cc_new_unique(context, krb5_cc_get_type(context, ccache),
			     NULL, &tempccache);
    if (ret) {
	krb5_warn(context, ret, "krb5_cc_new_unique");
	goto out;
    }

    ret = krb5_init_creds_store(context, ctx, tempccache);
    if (ret) {
	krb5_warn(context, ret, "krb5_init_creds_store");
	goto out;
    }

    krb5_init_creds_free(context, ctx);
    ctx = NULL;

    ret = krb5_cc_move(context, tempccache, ccache);
    if (ret) {
	krb5_warn(context, ret, "krb5_cc_move");
	goto out;
    }
    tempccache = NULL;

    if (switch_cache_flags)
	krb5_cc_switch(context, ccache);

#ifndef NO_NTLM
    if (ntlm_domain && ntlmkey.data)
	store_ntlmkey(context, ccache, ntlm_domain, &ntlmkey);
#endif

    if (ok_as_delegate_flag || windows_flag || use_referrals_flag) {
	unsigned char d = 0;
	krb5_data data;

	if (ok_as_delegate_flag || windows_flag)
	    d |= 1;
	if (use_referrals_flag || windows_flag)
	    d |= 2;

	data.length = 1;
	data.data = &d;

	krb5_cc_set_config(context, ccache, NULL, "realm-config", &data);
    }

out:
    krb5_get_init_creds_opt_free(context, opt);
    if (ctx)
	krb5_init_creds_free(context, ctx);
    if (tempccache)
	krb5_cc_close(context, tempccache);

    if (enctype)
	free(enctype);

    return ret;
}

static time_t
ticket_lifetime(krb5_context context, krb5_ccache cache, krb5_principal client,
		const char *server, time_t *renew)
{
    krb5_creds in_cred, *cred;
    krb5_error_code ret;
    time_t timeout;
    time_t curtime;

    memset(&in_cred, 0, sizeof(in_cred));

    if (renew != NULL)
        *renew = 0;

    ret = krb5_cc_get_principal(context, cache, &in_cred.client);
    if (ret) {
	krb5_warn(context, ret, "krb5_cc_get_principal");
	return 0;
    }
    ret = get_server(context, in_cred.client, server, &in_cred.server);
    if (ret) {
	krb5_free_principal(context, in_cred.client);
	krb5_warn(context, ret, "get_server");
	return 0;
    }

    ret = krb5_get_credentials(context, KRB5_GC_CACHED,
			       cache, &in_cred, &cred);
    krb5_free_principal(context, in_cred.client);
    krb5_free_principal(context, in_cred.server);
    if (ret) {
	krb5_warn(context, ret, "krb5_get_credentials");
	return 0;
    }
    curtime = time(NULL);
    timeout = cred->times.endtime - curtime;
    if (timeout < 0)
	timeout = 0;
    if (renew) {
	*renew = cred->times.renew_till - curtime;
	if (*renew < 0)
	    *renew = 0;
    }
    krb5_free_creds(context, cred);
    return timeout;
}

static time_t expire;

static char siginfo_msg[1024] = "No credentials\n";

static void
update_siginfo_msg(time_t exp, const char *srv)
{
    /* Note that exp is relative time */
    memset(siginfo_msg, 0, sizeof(siginfo_msg));
    memcpy(&siginfo_msg, "Updating...\n", sizeof("Updating...\n"));
    if (exp) {
	if (srv == NULL) {
	    snprintf(siginfo_msg, sizeof(siginfo_msg),
		     N_("kinit: TGT expires in %llu seconds\n", ""),
		     (unsigned long long)expire);
	} else {
	    snprintf(siginfo_msg, sizeof(siginfo_msg),
		     N_("kinit: Ticket for %s expired\n", ""), srv);
	}
	return;
    }

    /* Expired creds */
    if (srv == NULL) {
	snprintf(siginfo_msg, sizeof(siginfo_msg),
		 N_("kinit: TGT expired\n", ""));
    } else {
	snprintf(siginfo_msg, sizeof(siginfo_msg),
		 N_("kinit: Ticket for %s expired\n", ""), srv);
    }
}

#ifdef HAVE_SIGACTION
static void
handle_siginfo(int sig)
{
    struct iovec iov[2];

    iov[0].iov_base = rk_UNCONST(siginfo_msg);
    iov[0].iov_len = strlen(siginfo_msg);
    iov[1].iov_base = "\n";
    iov[1].iov_len = 1;

    writev(STDERR_FILENO, iov, sizeof(iov)/sizeof(iov[0]));
}
#endif

struct renew_ctx {
    krb5_context context;
    krb5_ccache  ccache;
    krb5_principal principal;
    krb5_deltat ticket_life;
    krb5_deltat timeout;
};

static time_t
renew_func(void *ptr)
{
    krb5_error_code ret;
    struct renew_ctx *ctx = ptr;
    time_t renew_expire;
    static time_t exp_delay = 1;

    /*
     * NOTE: We count on the ccache implementation to notice changes to the
     * actual ccache filesystem/whatever objects.  There should be no ccache
     * types for which this is not the case, but it might not hurt to
     * re-krb5_cc_resolve() after each successful renew_validate()/
     * get_new_tickets() call.
     */

    expire = ticket_lifetime(ctx->context, ctx->ccache, ctx->principal,
			     server_str, &renew_expire);

    /*
     * When a keytab is available to obtain new tickets, if we are within
     * half of the original ticket lifetime of the renew limit, get a new
     * TGT instead of renewing the existing TGT.  Note, ctx->ticket_life
     * is zero by default (without a '-l' option) and cannot be used to
     * set the time scale on which we decide whether we're "close to the
     * renew limit".
     */
    if (use_keytab || keytab_str)
	expire += ctx->timeout;
    if (renew_expire > expire) {
	ret = renew_validate(ctx->context, 1, validate_flag, ctx->ccache,
		       server_str, ctx->ticket_life);
    } else {
	ret = get_new_tickets(ctx->context, ctx->principal, ctx->ccache,
			      ctx->ticket_life, 0);
    }
    expire = ticket_lifetime(ctx->context, ctx->ccache, ctx->principal,
			     server_str, &renew_expire);

#ifndef NO_AFS
    if (ret == 0 && server_str == NULL && do_afslog && k_hasafs())
	krb5_afslog(ctx->context, ctx->ccache, NULL, NULL);
#endif

    update_siginfo_msg(expire, server_str);

    /*
     * If our tickets have expired and we been able to either renew them
     * or obtain new tickets, then we still call this function but we use
     * an exponential backoff.  This should take care of the case where
     * we are using stored credentials but the KDC has been unavailable
     * for some reason...
     */

    if (expire < 1) {
	/*
	 * We can't ask to keep spamming stderr but not syslog, so we warn
	 * only once.
	 */
	if (exp_delay == 1) {
	    krb5_warnx(ctx->context, N_("NOTICE: Could not renew/refresh "
					"tickets", ""));
	}
        if (exp_delay < 7200)
	    exp_delay += exp_delay / 2 + 1;
	return exp_delay;
    }
    exp_delay = 1;

    return expire / 2 + 1;
}

static void
set_princ_realm(krb5_context context,
		krb5_principal principal,
		const char *realm)
{
    krb5_error_code ret;

    if ((ret = krb5_principal_set_realm(context, principal, realm)) != 0)
	krb5_err(context, 1, ret, "krb5_principal_set_realm");
}

static void
parse_name_realm(krb5_context context,
		 const char *name,
		 int flags,
		 const char *realm,
		 krb5_principal *princ)
{
    krb5_error_code ret;
    
    if (realm)
	flags |= KRB5_PRINCIPAL_PARSE_NO_DEF_REALM;
    if ((ret = krb5_parse_name_flags(context, name, flags, princ)) != 0)
	krb5_err(context, 1, ret, "krb5_parse_name_flags");
    if (realm && krb5_principal_get_realm(context, *princ) == NULL)
	set_princ_realm(context, *princ, realm);
}

static char *
get_default_realm(krb5_context context)
{
    char *realm;
    krb5_error_code ret;

    if ((ret = krb5_get_default_realm(context, &realm)) != 0)
        krb5_err(context, 1, ret, "krb5_get_default_realm");
    return realm;
}

static void
get_default_principal(krb5_context context, krb5_principal *princ)
{
    krb5_error_code ret;

    if ((ret = krb5_get_default_principal(context, princ)) != 0)
	krb5_err(context, 1, ret, "krb5_get_default_principal");
}

static char *
get_user_realm(krb5_context context)
{
    krb5_error_code ret;
    char *user_realm = NULL;

    /*
     * If memory allocation fails, we don't try to use the wrong realm,
     * that will trigger misleading error messages complicate support.
     */
    krb5_appdefault_string(context, "kinit", NULL, "user_realm", "",
			   &user_realm);
    if (user_realm == NULL) {
	ret = krb5_enomem(context);
	krb5_err(context, 1, ret, "krb5_appdefault_string");
    }

    if (*user_realm == 0) {
	free(user_realm);
	user_realm = NULL;
    }

    return user_realm;
}

static void
get_princ(krb5_context context, krb5_principal *principal, const char *name)
{
    krb5_error_code ret;
    krb5_principal tmp;
    int parseflags = 0;
    char *user_realm;

    if (name == NULL) {
	krb5_ccache ccache;

	/* If credential cache provides a client principal, use that. */
	if (krb5_cc_default(context, &ccache) == 0) {
	    ret = krb5_cc_get_principal(context, ccache, principal);
	    krb5_cc_close(context, ccache);
	    if (ret == 0)
		return;
	}
    }

    user_realm = get_user_realm(context);

    if (name) {
	if (canonicalize_flag || enterprise_flag)
	    parseflags |= KRB5_PRINCIPAL_PARSE_ENTERPRISE;

	parse_name_realm(context, name, parseflags, user_realm, &tmp);

	if (user_realm && krb5_principal_get_num_comp(context, tmp) > 1) {
	    /* Principal is instance qualified, reparse with default realm. */
	    krb5_free_principal(context, tmp);
	    parse_name_realm(context, name, parseflags, NULL, principal);
	} else {
	    *principal = tmp;
	}
    } else {
	get_default_principal(context, principal);
	if (user_realm)
	    set_princ_realm(context, *principal, user_realm);
    }

    if (user_realm)
	free(user_realm);
}

static void
get_princ_kt(krb5_context context,
	     krb5_principal *principal,
	     char *name)
{
    krb5_error_code ret;
    krb5_principal tmp;
    krb5_ccache ccache;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;
    char *def_realm;

    if (name == NULL) {
	/*
	 * If the credential cache exists and specifies a client principal,
	 * use that.
	 */
	if (krb5_cc_default(context, &ccache) == 0) {
	    ret = krb5_cc_get_principal(context, ccache, principal);
	    krb5_cc_close(context, ccache);
	    if (ret == 0)
		return;
	}
    }

    if (name) {
	/* If the principal specifies an explicit realm, just use that. */
	int parseflags = KRB5_PRINCIPAL_PARSE_NO_DEF_REALM;

	parse_name_realm(context, name, parseflags, NULL, &tmp);
	if (krb5_principal_get_realm(context, tmp) != NULL) {
	    *principal = tmp;
	    return;
	}
    } else {
	/* Otherwise, search keytab for bare name of the default principal. */
	get_default_principal(context, &tmp);
	set_princ_realm(context, tmp, NULL);
    }

    def_realm = get_default_realm(context);

    ret = krb5_kt_start_seq_get(context, kt, &cursor);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kt_start_seq_get");

    while (ret == 0 &&
           krb5_kt_next_entry(context, kt, &entry, &cursor) == 0) {
        const char *realm;

        if (!krb5_principal_compare_any_realm(context, tmp, entry.principal))
            continue;
        if (*principal &&
	    krb5_principal_compare(context, *principal, entry.principal))
            continue;
        /* The default realm takes precedence */
        realm = krb5_principal_get_realm(context, entry.principal);
        if (*principal && strcmp(def_realm, realm) == 0) {
            krb5_free_principal(context, *principal);
            ret = krb5_copy_principal(context, entry.principal, principal);
            break;
        }
        if (!*principal)
            ret = krb5_copy_principal(context, entry.principal, principal);
    }
    if (ret != 0 || (ret = krb5_kt_end_seq_get(context, kt, &cursor)) != 0)
	krb5_err(context, 1, ret, "get_princ_kt");
    if (!*principal) {
	if (name)
	    parse_name_realm(context, name, 0, NULL, principal);
	else
	    krb5_err(context, 1, KRB5_CC_NOTFOUND, "get_princ_kt");
    }

    krb5_free_principal(context, tmp);
    free(def_realm);
}

static krb5_error_code
get_switched_ccache(krb5_context context,
		    const char * type,
		    krb5_principal principal,
		    krb5_ccache *ccache)
{
    krb5_error_code ret;

#ifdef _WIN32
    if (strcmp(type, "API") == 0) {
	/*
	 * Windows stores the default ccache name in the
	 * registry which is shared across multiple logon
	 * sessions for the same user.  The API credential
	 * cache provides a unique name space per logon
	 * session.  Therefore there is no need to generate
	 * a unique ccache name.  Instead use the principal
	 * name.  This provides a friendlier user experience.
	 */
	char * unparsed_name;
	char * cred_cache;

	ret = krb5_unparse_name(context, principal,
				&unparsed_name);
	if (ret)
	    krb5_err(context, 1, ret,
		     N_("unparsing principal name", ""));

	ret = asprintf(&cred_cache, "API:%s", unparsed_name);
	krb5_free_unparsed_name(context, unparsed_name);
	if (ret == -1 || cred_cache == NULL)
	    krb5_err(context, 1, ret,
		      N_("building credential cache name", ""));

	ret = krb5_cc_resolve(context, cred_cache, ccache);
	free(cred_cache);
    } else if (strcmp(type, "MSLSA") == 0) {
	/*
	 * The Windows MSLSA cache when it is writeable
	 * stores tickets for multiple client principals
	 * in a single credential cache.
	 */
	ret = krb5_cc_resolve(context, "MSLSA:", ccache);
    } else {
	ret = krb5_cc_new_unique(context, type, NULL, ccache);
    }
#else /* !_WIN32 */
    ret = krb5_cc_new_unique(context, type, NULL, ccache);
#endif /* _WIN32 */

    return ret;
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache  ccache;
    krb5_principal principal = NULL;
    int optidx = 0;
    krb5_deltat ticket_life = 0;
#ifdef HAVE_SIGACTION
    struct sigaction sa;
#endif

    setprogname(argv[0]);

    setlocale(LC_ALL, "");
    bindtextdomain("heimdal_kuser", HEIMDAL_LOCALEDIR);
    textdomain("heimdal_kuser");

    ret = krb5_init_context(&context);
    if (ret == KRB5_CONFIG_BADFORMAT)
	errx(1, "krb5_init_context failed to parse configuration file");
    else if (ret)
	errx(1, "krb5_init_context failed: %d", ret);

    if (getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage(0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    /*
     * Open the keytab now, we use the keytab to determine the principal's
     * realm when the requested principal has no realm.
     */
    if (use_keytab || keytab_str) {
	if (keytab_str)
	    ret = krb5_kt_resolve(context, keytab_str, &kt);
	else
	    ret = krb5_kt_default(context, &kt);
	if (ret)
	    krb5_err(context, 1, ret, "resolving keytab");
    }

    if (pk_enterprise_flag) {
	ret = krb5_pk_enterprise_cert(context, pk_user_id,
				      argv[0], &principal,
				      &ent_user_id);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_pk_enterprise_certs");

	pk_user_id = NULL;

    } else if (anonymous_flag) {

	ret = krb5_make_principal(context, &principal, argv[0],
				  KRB5_WELLKNOWN_NAME, KRB5_ANON_NAME,
				  NULL);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_make_principal");
	krb5_principal_set_type(context, principal, KRB5_NT_WELLKNOWN);

    } else if (use_keytab || keytab_str) {
	get_princ_kt(context, &principal, argv[0]);
    } else {
	get_princ(context, &principal, argv[0]);
    }

    if (fcache_version)
	krb5_set_fcache_version(context, fcache_version);

    if (renewable_flag == -1)
	/* this seems somewhat pointless, but whatever */
	krb5_appdefault_boolean(context, "kinit",
				krb5_principal_get_realm(context, principal),
				"renewable", FALSE, &renewable_flag);
    if (do_afslog == -1)
	krb5_appdefault_boolean(context, "kinit",
				krb5_principal_get_realm(context, principal),
				"afslog", TRUE, &do_afslog);

    if (cred_cache)
	ret = krb5_cc_resolve(context, cred_cache, &ccache);
    else {
	if (argc > 1) {
	    char s[1024];
	    ret = krb5_cc_new_unique(context, NULL, NULL, &ccache);
	    if (ret)
		krb5_err(context, 1, ret, "creating cred cache");
	    snprintf(s, sizeof(s), "%s:%s",
		     krb5_cc_get_type(context, ccache),
		     krb5_cc_get_name(context, ccache));
	    setenv("KRB5CCNAME", s, 1);
	} else {
	    ret = krb5_cc_cache_match(context, principal, &ccache);
	    if (ret) {
		const char *type;
		ret = krb5_cc_default(context, &ccache);
		if (ret)
		    krb5_err(context, 1, ret,
			     N_("resolving credentials cache", ""));

		/*
		 * Check if the type support switching, and we do,
		 * then do that instead over overwriting the current
		 * default credential
		 */
		type = krb5_cc_get_type(context, ccache);
		if (krb5_cc_support_switch(context, type)) {
		    krb5_cc_close(context, ccache);
		    ret = get_switched_ccache(context, type, principal,
					      &ccache);
		}
	    }
	}
    }
    if (ret)
	krb5_err(context, 1, ret, N_("resolving credentials cache", ""));

#ifndef NO_AFS
    if (argc > 1 && k_hasafs())
	k_setpag();
#endif

    if (lifetime) {
	int tmp = parse_time(lifetime, "s");
	if (tmp < 0)
	    errx(1, N_("unparsable time: %s", ""), lifetime);

	ticket_life = tmp;
    }

    if (addrs_flag == 0 && extra_addresses.num_strings > 0)
	krb5_errx(context, 1,
		  N_("specifying both extra addresses and "
		     "no addresses makes no sense", ""));
    {
	int i;
	krb5_addresses addresses;
	memset(&addresses, 0, sizeof(addresses));
	for(i = 0; i < extra_addresses.num_strings; i++) {
	    ret = krb5_parse_address(context, extra_addresses.strings[i],
				     &addresses);
	    if (ret == 0) {
		krb5_add_extra_addresses(context, &addresses);
		krb5_free_addresses(context, &addresses);
	    }
	}
	free_getarg_strings(&extra_addresses);
    }

    if (renew_flag || validate_flag) {
	ret = renew_validate(context, renew_flag, validate_flag,
			     ccache, server_str, ticket_life);

#ifndef NO_AFS
	if (ret == 0 && server_str == NULL && do_afslog && k_hasafs())
	    krb5_afslog(context, ccache, NULL, NULL);
#endif

	exit(ret != 0);
    }

    ret = get_new_tickets(context, principal, ccache, ticket_life, 1);
    if (ret)
	exit(1);

#ifndef NO_AFS
    if (ret == 0 && server_str == NULL && do_afslog && k_hasafs())
	krb5_afslog(context, ccache, NULL, NULL);
#endif

    if (argc > 1) {
	struct renew_ctx ctx;
	time_t timeout;

	timeout = ticket_lifetime(context, ccache, principal,
				  server_str, NULL) / 2;

	ctx.context = context;
	ctx.ccache = ccache;
	ctx.principal = principal;
	ctx.ticket_life = ticket_life;
	ctx.timeout = timeout;

#ifdef HAVE_SIGACTION
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = handle_siginfo;

	sigaction(SIGINFO, &sa, NULL);
#endif

	ret = simple_execvp_timed(argv[1], argv+1,
				  renew_func, &ctx, timeout);
#define EX_NOEXEC	126
#define EX_NOTFOUND	127
	if (ret == EX_NOEXEC)
	    krb5_warnx(context, N_("permission denied: %s", ""), argv[1]);
	else if (ret == EX_NOTFOUND)
	    krb5_warnx(context, N_("command not found: %s", ""), argv[1]);

	krb5_cc_destroy(context, ccache);
#ifndef NO_AFS
	if (k_hasafs())
	    k_unlog();
#endif
    } else {
	krb5_cc_close(context, ccache);
	ret = 0;
    }
    krb5_free_principal(context, principal);
    if (kt)
	krb5_kt_close(context, kt);
    krb5_free_context(context);
    return ret;
}
