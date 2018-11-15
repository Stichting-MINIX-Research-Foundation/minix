/*	$NetBSD: verify_krb5_conf.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1999 - 2005 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"
#include <krb5/getarg.h>
#include <krb5/parse_bytes.h>
#include <err.h>

/* verify krb5.conf */

static int dumpconfig_flag = 0;
static int version_flag = 0;
static int help_flag	= 0;
static int warn_mit_syntax_flag = 0;

static struct getargs args[] = {
    {"dumpconfig", 0,      arg_flag,       &dumpconfig_flag,
     "show the parsed config files", NULL },
    {"warn-mit-syntax", 0, arg_flag,       &warn_mit_syntax_flag,
     "show the parsed config files", NULL },
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "[config-file]");
    exit (ret);
}

static int
check_bytes(krb5_context context, const char *path, char *data)
{
    if(parse_bytes(data, NULL) == -1) {
	krb5_warnx(context, "%s: failed to parse \"%s\" as size", path, data);
	return 1;
    }
    return 0;
}

static int
check_time(krb5_context context, const char *path, char *data)
{
    if(parse_time(data, NULL) == -1) {
	krb5_warnx(context, "%s: failed to parse \"%s\" as time", path, data);
	return 1;
    }
    return 0;
}

static int
check_numeric(krb5_context context, const char *path, char *data)
{
    long v;
    char *end;
    v = strtol(data, &end, 0);

    if ((v == LONG_MIN || v == LONG_MAX) && errno != 0) {
	krb5_warnx(context, "%s: over/under flow for \"%s\"",
		   path, data);
	return 1;
    }
    if(*end != '\0') {
	krb5_warnx(context, "%s: failed to parse \"%s\" as a number",
		   path, data);
	return 1;
    }
    return 0;
}

static int
check_boolean(krb5_context context, const char *path, char *data)
{
    long int v;
    char *end;
    if(strcasecmp(data, "yes") == 0 ||
       strcasecmp(data, "true") == 0 ||
       strcasecmp(data, "no") == 0 ||
       strcasecmp(data, "false") == 0)
	return 0;
    v = strtol(data, &end, 0);
    if(*end != '\0') {
	krb5_warnx(context, "%s: failed to parse \"%s\" as a boolean",
		   path, data);
	return 1;
    }
    if(v != 0 && v != 1)
	krb5_warnx(context, "%s: numeric value \"%s\" is treated as \"true\"",
		   path, data);
    return 0;
}

static int
check_524(krb5_context context, const char *path, char *data)
{
    if(strcasecmp(data, "yes") == 0 ||
       strcasecmp(data, "no") == 0 ||
       strcasecmp(data, "2b") == 0 ||
       strcasecmp(data, "local") == 0)
	return 0;

    krb5_warnx(context, "%s: didn't contain a valid option `%s'",
	       path, data);
    return 1;
}

static int
check_host(krb5_context context, const char *path, char *data)
{
    int ret;
    char hostname[128];
    const char *p = data;
    struct addrinfo hints;
    char service[32];
    int defport;
    struct addrinfo *ai;

    hints.ai_flags = 0;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;

    hints.ai_addrlen = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    /* XXX data could be a list of hosts that this code can't handle */
    /* XXX copied from krbhst.c */
    if (strncmp(p, "http://", 7) == 0){
        p += 7;
	hints.ai_socktype = SOCK_STREAM;
	strlcpy(service, "http", sizeof(service));
	defport = 80;
    } else if (strncmp(p, "http/", 5) == 0) {
        p += 5;
	hints.ai_socktype = SOCK_STREAM;
	strlcpy(service, "http", sizeof(service));
	defport = 80;
    } else if (strncmp(p, "tcp/", 4) == 0){
        p += 4;
	hints.ai_socktype = SOCK_STREAM;
	strlcpy(service, "kerberos", sizeof(service));
	defport = 88;
    } else if (strncmp(p, "udp/", 4) == 0) {
        p += 4;
	hints.ai_socktype = SOCK_DGRAM;
	strlcpy(service, "kerberos", sizeof(service));
	defport = 88;
    } else {
	hints.ai_socktype = SOCK_DGRAM;
	strlcpy(service, "kerberos", sizeof(service));
	defport = 88;
    }
    if (strsep_copy(&p, ":", hostname, sizeof(hostname)) < 0) {
	return 1;
    }
    hostname[strcspn(hostname, "/")] = '\0';
    if (p != NULL) {
	char *end;
	int tmp = strtol(p, &end, 0);
	if (end == p) {
	    krb5_warnx(context, "%s: failed to parse port number in %s",
		       path, data);
	    return 1;
	}
	defport = tmp;
	snprintf(service, sizeof(service), "%u", defport);
    }
    ret = getaddrinfo(hostname, service, &hints, &ai);
    if (ret == EAI_SERVICE && !isdigit((unsigned char)service[0])) {
	snprintf(service, sizeof(service), "%u", defport);
	ret = getaddrinfo(hostname, service, &hints, &ai);
    }
    if (ret != 0) {
	krb5_warnx(context, "%s: %s (%s)", path, gai_strerror(ret), hostname);
	return 1;
    }
    freeaddrinfo(ai);
    return 0;
}

static int
mit_entry(krb5_context context, const char *path, char *data)
{
    if (warn_mit_syntax_flag)
	krb5_warnx(context, "%s is only used by MIT Kerberos", path);
    return 0;
}

struct s2i {
    const char *s;
    int val;
};

#define L(X) { #X, LOG_ ## X }

static struct s2i syslogvals[] = {
    /* severity */
    L(EMERG),
    L(ALERT),
    L(CRIT),
    L(ERR),
    L(WARNING),
    L(NOTICE),
    L(INFO),
    L(DEBUG),
    /* facility */
    L(AUTH),
#ifdef LOG_AUTHPRIV
    L(AUTHPRIV),
#endif
#ifdef LOG_CRON
    L(CRON),
#endif
    L(DAEMON),
#ifdef LOG_FTP
    L(FTP),
#endif
    L(KERN),
    L(LPR),
    L(MAIL),
#ifdef LOG_NEWS
    L(NEWS),
#endif
    L(SYSLOG),
    L(USER),
#ifdef LOG_UUCP
    L(UUCP),
#endif
    L(LOCAL0),
    L(LOCAL1),
    L(LOCAL2),
    L(LOCAL3),
    L(LOCAL4),
    L(LOCAL5),
    L(LOCAL6),
    L(LOCAL7),
    { NULL, -1 }
};

static int
find_value(const char *s, struct s2i *table)
{
    while(table->s && strcasecmp(table->s, s))
	table++;
    return table->val;
}

static int
check_log(krb5_context context, const char *path, char *data)
{
    /* XXX sync with log.c */
    int min = 0, max = -1, n;
    char c;
    const char *p = data;
#ifdef _WIN32
    const char *q;
#endif

    n = sscanf(p, "%d%c%d/", &min, &c, &max);
    if(n == 2){
	if(ISPATHSEP(c)) {
	    if(min < 0){
		max = -min;
		min = 0;
	    }else{
		max = min;
	    }
	}
    }
    if(n){
#ifdef _WIN32
	q = strrchr(p, '\\');
	if (q != NULL)
	    p = q;
	else
#endif
	p = strchr(p, '/');
	if(p == NULL) {
	    krb5_warnx(context, "%s: failed to parse \"%s\"", path, data);
	    return 1;
	}
	p++;
    }
    if(strcmp(p, "STDERR") == 0 ||
       strcmp(p, "CONSOLE") == 0 ||
       (strncmp(p, "FILE", 4) == 0 && (p[4] == ':' || p[4] == '=')) ||
       (strncmp(p, "DEVICE", 6) == 0 && p[6] == '='))
	return 0;
    if(strncmp(p, "SYSLOG", 6) == 0){
	int ret = 0;
	char severity[128] = "";
	char facility[128] = "";
	p += 6;
	if(*p != '\0')
	    p++;
	if(strsep_copy(&p, ":", severity, sizeof(severity)) != -1)
	    strsep_copy(&p, ":", facility, sizeof(facility));
	if(*severity == '\0')
	    strlcpy(severity, "ERR", sizeof(severity));
 	if(*facility == '\0')
	    strlcpy(facility, "AUTH", sizeof(facility));
	if(find_value(facility, syslogvals) == -1) {
	    krb5_warnx(context, "%s: unknown syslog facility \"%s\"",
		       path, facility);
	    ret++;
	}
	if(find_value(severity, syslogvals) == -1) {
	    krb5_warnx(context, "%s: unknown syslog severity \"%s\"",
		       path, severity);
	    ret++;
	}
	return ret;
    }else{
	krb5_warnx(context, "%s: unknown log type: \"%s\"", path, data);
	return 1;
    }
}

typedef int (*check_func_t)(krb5_context, const char*, char*);
struct entry {
    const char *name;
    int type;
    void *check_data;
    int deprecated;
};

struct entry all_strings[] = {
    { "", krb5_config_string, NULL, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry all_boolean[] = {
    { "", krb5_config_string, check_boolean, 0 },
    { NULL, 0, NULL, 0 }
};


struct entry v4_name_convert_entries[] = {
    { "host", krb5_config_list, all_strings, 0 },
    { "plain", krb5_config_list, all_strings, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry libdefaults_entries[] = {
    { "accept_null_addresses", krb5_config_string, check_boolean, 0 },
    { "allow_weak_crypto", krb5_config_string, check_boolean, 0 },
    { "capath", krb5_config_list, all_strings, 1 },
    { "ccapi_library", krb5_config_string, NULL, 0 },
    { "check_pac", krb5_config_string, check_boolean, 0 },
    { "check-rd-req-server", krb5_config_string, check_boolean, 0 },
    { "clockskew", krb5_config_string, check_time, 0 },
    { "date_format", krb5_config_string, NULL, 0 },
    { "default_as_etypes", krb5_config_string, NULL, 0 },
    { "default_cc_name", krb5_config_string, NULL, 0 },
    { "default_cc_type", krb5_config_string, NULL, 0 },
    { "default_etypes", krb5_config_string, NULL, 0 },
    { "default_etypes_des", krb5_config_string, NULL, 0 },
    { "default_keytab_modify_name", krb5_config_string, NULL, 0 },
    { "default_keytab_name", krb5_config_string, NULL, 0 },
    { "default_keytab_modify_name", krb5_config_string, NULL, 0 },
    { "default_realm", krb5_config_string, NULL, 0 },
    { "default_tgs_etypes", krb5_config_string, NULL, 0 },
    { "dns_canonize_hostname", krb5_config_string, check_boolean, 0 },
    { "dns_proxy", krb5_config_string, NULL, 0 },
    { "dns_lookup_kdc", krb5_config_string, check_boolean, 0 },
    { "dns_lookup_realm", krb5_config_string, check_boolean, 0 },
    { "dns_lookup_realm_labels", krb5_config_string, NULL, 0 },
    { "egd_socket", krb5_config_string, NULL, 0 },
    { "encrypt", krb5_config_string, check_boolean, 0 },
    { "extra_addresses", krb5_config_string, NULL, 0 },
    { "fcache_version", krb5_config_string, check_numeric, 0 },
    { "fcache_strict_checking", krb5_config_string, check_boolean, 0 },
    { "fcc-mit-ticketflags", krb5_config_string, check_boolean, 0 },
    { "forward", krb5_config_string, check_boolean, 0 },
    { "forwardable", krb5_config_string, check_boolean, 0 },
    { "allow_hierarchical_capaths", krb5_config_string, check_boolean, 0 },
    { "host_timeout", krb5_config_string, check_time, 0 },
    { "http_proxy", krb5_config_string, check_host /* XXX */, 0 },
    { "ignore_addresses", krb5_config_string, NULL, 0 },
    { "k5login_authoritative", krb5_config_string, check_boolean, 0 },
    { "k5login_directory", krb5_config_string, NULL, 0 },
    { "kdc_timeout", krb5_config_string, check_time, 0 },
    { "kdc_timesync", krb5_config_string, check_boolean, 0 },
    { "kuserok", krb5_config_string, NULL, 0 },
    { "large_message_size", krb5_config_string, check_numeric, 0 },
    { "log_utc", krb5_config_string, check_boolean, 0 },
    { "max_retries", krb5_config_string, check_numeric, 0 },
    { "maximum_message_size", krb5_config_string, check_numeric, 0 },
    { "moduli", krb5_config_string, NULL, 0 },
    { "name_canon_rules", krb5_config_string, NULL, 0 },
    { "no-addresses", krb5_config_string, check_boolean, 0 },
    { "pkinit_dh_min_bits", krb5_config_string, NULL, 0 },
    { "proxiable", krb5_config_string, check_boolean, 0 },
    { "renew_lifetime", krb5_config_string, check_time, 0 },
    { "scan_interfaces", krb5_config_string, check_boolean, 0 },
    { "srv_lookup", krb5_config_string, check_boolean, 0 },
    { "srv_try_txt", krb5_config_string, check_boolean, 0 },
    { "ticket_lifetime", krb5_config_string, check_time, 0 },
    { "time_format", krb5_config_string, NULL, 0 },
    { "transited_realms_reject", krb5_config_string, NULL, 0 },
    { "use_fallback", krb5_config_string, check_boolean, 0 },
    { "v4_instance_resolve", krb5_config_string, check_boolean, 0 },
    { "v4_name_convert", krb5_config_list, v4_name_convert_entries, 0 },
    { "verify_ap_req_nofail", krb5_config_string, check_boolean, 0 },
    { "warn_pwexpire", krb5_config_string, check_time, 0 },

    /* MIT stuff */
    { "permitted_enctypes", krb5_config_string, mit_entry, 0 },
    { "default_tgs_enctypes", krb5_config_string, mit_entry, 0 },
    { "default_tkt_enctypes", krb5_config_string, mit_entry, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry appdefaults_entries[] = {
    { "afslog", krb5_config_string, check_boolean, 0 },
    { "afs-use-524", krb5_config_string, check_524, 0 },
#if 0
    { "anonymous", krb5_config_string, check_boolean, 0 },
#endif
    { "encrypt", krb5_config_string, check_boolean, 0 },
    { "forward", krb5_config_string, check_boolean, 0 },
    { "forwardable", krb5_config_string, check_boolean, 0 },
    { "krb4_get_tickets", krb5_config_string, check_boolean, 0 },
    { "proxiable", krb5_config_string, check_boolean, 0 },
    { "renew_lifetime", krb5_config_string, check_time, 0 },
    { "no-addresses", krb5_config_string, check_boolean, 0 },
    { "pkinit_anchors", krb5_config_string, NULL, 0 },
    { "pkinit_pool", krb5_config_string, NULL, 0 },
    { "pkinit_require_eku", krb5_config_string, NULL, 0 },
    { "pkinit_require_hostname_match", krb5_config_string, NULL, 0 },
    { "pkinit_require_krbtgt_otherName", krb5_config_string, NULL, 0 },
    { "pkinit_revoke", krb5_config_string, NULL, 0 },
    { "pkinit_trustedCertifiers", krb5_config_string, check_boolean, 0 },
    { "pkinit_win2k", krb5_config_string, NULL, 0 },
    { "pkinit_win2k_require_binding", krb5_config_string, NULL, 0 },
    { "ticket_lifetime", krb5_config_string, check_time, 0 },
    { "", krb5_config_list, appdefaults_entries, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry realms_entries[] = {
    { "admin_server", krb5_config_string, check_host, 0 },
    { "auth_to_local", krb5_config_string, NULL, 0 },
    { "auth_to_local_names", krb5_config_string, NULL, 0 },
    { "default_domain", krb5_config_string, NULL, 0 },
    { "forwardable", krb5_config_string, check_boolean, 0 },
    { "allow_hierarchical_capaths", krb5_config_string, check_boolean, 0 },
    { "kdc", krb5_config_string, check_host, 0 },
    { "kpasswd_server", krb5_config_string, check_host, 0 },
    { "krb524_server", krb5_config_string, check_host, 0 },
    { "kx509_ca", krb5_config_string, NULL, 0 },
    { "kx509_include_pkinit_san", krb5_config_string, check_boolean, 0 },
    { "name_canon_rules", krb5_config_string, NULL, 0 },
    { "no-addresses", krb5_config_string, check_boolean, 0 },
    { "pkinit_anchors", krb5_config_string, NULL, 0 },
    { "pkinit_require_eku", krb5_config_string, NULL, 0 },
    { "pkinit_require_hostname_match", krb5_config_string, NULL, 0 },
    { "pkinit_require_krbtgt_otherName", krb5_config_string, NULL, 0 },
    { "pkinit_trustedCertifiers", krb5_config_string, check_boolean, 0 },
    { "pkinit_win2k", krb5_config_string, NULL, 0 },
    { "pkinit_win2k_require_binding", krb5_config_string, NULL, 0 },
    { "proxiable", krb5_config_string, check_boolean, 0 },
    { "renew_lifetime", krb5_config_string, check_time, 0 },
    { "require_initial_kca_tickets", krb5_config_string, check_boolean, 0 },
    { "ticket_lifetime", krb5_config_string, check_time, 0 },
    { "v4_domains", krb5_config_string, NULL, 0 },
    { "v4_instance_convert", krb5_config_list, all_strings, 0 },
    { "v4_name_convert", krb5_config_list, v4_name_convert_entries, 0 },
    { "warn_pwexpire", krb5_config_string, check_time, 0 },
    { "win2k_pkinit", krb5_config_string, NULL, 0 },

    /* MIT stuff */
    { "admin_keytab", krb5_config_string, mit_entry, 0 },
    { "acl_file", krb5_config_string, mit_entry, 0 },
    { "database_name", krb5_config_string, mit_entry, 0 },
    { "default_principal_expiration", krb5_config_string, mit_entry, 0 },
    { "default_principal_flags", krb5_config_string, mit_entry, 0 },
    { "dict_file", krb5_config_string, mit_entry, 0 },
    { "kadmind_port", krb5_config_string, mit_entry, 0 },
    { "kpasswd_port", krb5_config_string, mit_entry, 0 },
    { "master_kdc", krb5_config_string, mit_entry, 0 },
    { "master_key_name", krb5_config_string, mit_entry, 0 },
    { "master_key_type", krb5_config_string, mit_entry, 0 },
    { "key_stash_file", krb5_config_string, mit_entry, 0 },
    { "max_life", krb5_config_string, mit_entry, 0 },
    { "max_renewable_life", krb5_config_string, mit_entry, 0 },
    { "supported_enctypes", krb5_config_string, mit_entry, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry realms_foobar[] = {
    { "", krb5_config_list, realms_entries, 0 },
    { NULL, 0, NULL, 0 }
};


struct entry kdc_database_entries[] = {
    { "acl_file", krb5_config_string, NULL, 0 },
    { "dbname", krb5_config_string, NULL, 0 },
    { "log_file", krb5_config_string, NULL, 0 },
    { "mkey_file", krb5_config_string, NULL, 0 },
    { "realm", krb5_config_string, NULL, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry kdc_entries[] = {
    { "addresses", krb5_config_string, NULL, 0 },
    { "allow-anonymous", krb5_config_string, check_boolean, 0 },
    { "allow-null-ticket-addresses", krb5_config_string, check_boolean, 0 },
    { "check-ticket-addresses", krb5_config_string, check_boolean, 0 },
    { "database", krb5_config_list, kdc_database_entries, 0 },
    { "detach", krb5_config_string, check_boolean, 0 },
    { "digests_allowed", krb5_config_string, NULL, 0 },
    { "disable-des", krb5_config_string, check_boolean, 0 },
    { "enable-524", krb5_config_string, check_boolean, 0 },
    { "enable-digest", krb5_config_string, check_boolean, 0 },
    { "enable-kaserver", krb5_config_string, check_boolean, 1 },
    { "enable-kerberos4", krb5_config_string, check_boolean, 1 },
    { "enable-kx509", krb5_config_string, check_boolean, 0 },
    { "enable-http", krb5_config_string, check_boolean, 0 },
    { "enable-pkinit", krb5_config_string, check_boolean, 0 },
    { "encode_as_rep_as_tgs_rep", krb5_config_string, check_boolean, 0 },
    { "enforce-transited-policy", krb5_config_string, NULL, 1 },
    { "hdb-ldap-create-base", krb5_config_string, NULL, 0 },
    { "iprop-acl", krb5_config_string, NULL, 0 },
    { "iprop-stats", krb5_config_string, NULL, 0 },
    { "kdc-request-log", krb5_config_string, NULL, 0 },
    { "kdc_warn_pwexpire", krb5_config_string, check_time, 0 },
    { "key-file", krb5_config_string, NULL, 0 },
    { "kx509_ca", krb5_config_string, NULL, 0 },
    { "kx509_include_pkinit_san", krb5_config_string, check_boolean, 0 },
    { "kx509_template", krb5_config_string, NULL, 0 },
    { "logging", krb5_config_string, check_log, 0 },
    { "max-kdc-datagram-reply-length", krb5_config_string, check_bytes, 0 },
    { "max-request", krb5_config_string, check_bytes, 0 },
    { "pkinit_allow_proxy_certificate", krb5_config_string, check_boolean, 0 },
    { "pkinit_anchors", krb5_config_string, NULL, 0 },
    { "pkinit_dh_min_bits", krb5_config_string, check_numeric, 0 },
    { "pkinit_identity", krb5_config_string, NULL, 0 },
    { "pkinit_kdc_friendly_name", krb5_config_string, NULL, 0 },
    { "pkinit_kdc_ocsp", krb5_config_string, NULL, 0 },
    { "pkinit_mappings_file", krb5_config_string, NULL, 0 },
    { "pkinit_pool", krb5_config_string, NULL, 0 },
    { "pkinit_principal_in_certificate", krb5_config_string, check_boolean, 0 },
    { "pkinit_revoke", krb5_config_string, NULL, 0 },
    { "pkinit_win2k_require_binding", krb5_config_string, check_boolean, 0 },
    { "ports", krb5_config_string, NULL, 0 },
    { "preauth-use-strongest-session-key", krb5_config_string, check_boolean, 0 },
    { "require_initial_kca_tickets", krb5_config_string, check_boolean, 0 },
    { "require-preauth", krb5_config_string, check_boolean, 0 },
    { "svc-use-strongest-session-key", krb5_config_string, check_boolean, 0 },
    { "tgt-use-strongest-session-key", krb5_config_string, check_boolean, 0 },
    { "transited-policy", krb5_config_string, NULL, 0 },
    { "use_2b", krb5_config_list, NULL, 0 },
    { "use-strongest-server-key", krb5_config_string, check_boolean, 0 },
    { "v4_realm", krb5_config_string, NULL, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry kadmin_entries[] = {
    { "allow_self_change_password", krb5_config_string, check_boolean, 0 },
    { "default_keys", krb5_config_string, NULL, 0 },
    { "password_lifetime", krb5_config_string, check_time, 0 },
    { "require-preauth", krb5_config_string, check_boolean, 0 },
    { "save-password", krb5_config_string, check_boolean, 0 },
    { "use_v4_salt", krb5_config_string, NULL, 0 },
    { NULL, 0, NULL, 0 }
};
struct entry log_strings[] = {
    { "", krb5_config_string, check_log, 0 },
    { NULL, 0, NULL, 0 }
};


/* MIT stuff */
struct entry kdcdefaults_entries[] = {
    { "kdc_ports", krb5_config_string, mit_entry, 0 },
    { "v4_mode", krb5_config_string, mit_entry, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry capaths_entries[] = {
    { "", krb5_config_list, all_strings, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry kcm_entries[] = {
    { "detach", krb5_config_string, check_boolean, 0 },
    { "disallow-getting-krbtgt", krb5_config_string, check_boolean, 0 },
    { "logging", krb5_config_string, NULL, 0 },
    { "max-request", krb5_config_string, NULL, 0 },
    { "system_ccache", krb5_config_string, NULL, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry password_quality_entries[] = {
    { "check_function", krb5_config_string, NULL, 0 },
    { "check_library", krb5_config_string, NULL, 0 },
    { "external_program", krb5_config_string, NULL, 0 },
    { "min_classes", krb5_config_string, check_numeric, 0 },
    { "min_length", krb5_config_string, check_numeric, 0 },
    { "policies", krb5_config_string, NULL, 0 },
    { "policy_libraries", krb5_config_string, NULL, 0 },
    { "", krb5_config_list, all_strings, 0 },
    { NULL, 0, NULL, 0 }
};

struct entry toplevel_sections[] = {
    { "appdefaults", krb5_config_list, appdefaults_entries, 0 },
    { "capaths", krb5_config_list, capaths_entries, 0 },
    { "domain_realm", krb5_config_list, all_strings, 0 },
    { "gssapi", krb5_config_list, NULL, 0 },
    { "kadmin", krb5_config_list, kadmin_entries, 0 },
    { "kcm", krb5_config_list, kcm_entries, 0 },
    { "kdc", krb5_config_list, kdc_entries, 0 },
    { "libdefaults" , krb5_config_list, libdefaults_entries, 0 },
    { "logging", krb5_config_list, log_strings, 0 },
    { "password_quality", krb5_config_list, password_quality_entries, 0 },
    { "realms", krb5_config_list, realms_foobar, 0 },

    /* MIT stuff */
    { "kdcdefaults", krb5_config_list, kdcdefaults_entries, 0 },
    { NULL, 0, NULL, 0 }
};


static int
check_section(krb5_context context, const char *path, krb5_config_section *cf,
	      struct entry *entries)
{
    int error = 0;
    krb5_config_section *p;
    struct entry *e;

    char *local;

    for(p = cf; p != NULL; p = p->next) {
	local = NULL;
	if (asprintf(&local, "%s/%s", path, p->name) < 0 || local == NULL)
	    errx(1, "out of memory");
	for(e = entries; e->name != NULL; e++) {
	    if(*e->name == '\0' || strcmp(e->name, p->name) == 0) {
		if(e->type != p->type) {
		    krb5_warnx(context, "%s: unknown or wrong type", local);
		    error |= 1;
		} else if(p->type == krb5_config_string && e->check_data != NULL) {
		    error |= (*(check_func_t)e->check_data)(context, local, p->u.string);
		} else if(p->type == krb5_config_list && e->check_data != NULL) {
		    error |= check_section(context, local, p->u.list, e->check_data);
		}
		if(e->deprecated) {
		    krb5_warnx(context, "%s: is a deprecated entry", local);
		    error |= 1;
		}
		break;
	    }
	}
	if(e->name == NULL) {
	    krb5_warnx(context, "%s: unknown entry", local);
	    error |= 1;
	}
	free(local);
    }
    return error;
}


static void
dumpconfig(int level, krb5_config_section *top)
{
    krb5_config_section *x;
    for(x = top; x; x = x->next) {
	switch(x->type) {
	case krb5_config_list:
	    if(level == 0) {
		printf("[%s]\n", x->name);
	    } else {
		printf("%*s%s = {\n", 4 * level, " ", x->name);
	    }
	    dumpconfig(level + 1, x->u.list);
	    if(level > 0)
		printf("%*s}\n", 4 * level, " ");
	    break;
	case krb5_config_string:
	    printf("%*s%s = %s\n", 4 * level, " ", x->name, x->u.string);
	    break;
	}
    }
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_config_section *tmp_cf;
    int optidx = 0;

    setprogname (argv[0]);

    ret = krb5_init_context(&context);
    if (ret == KRB5_CONFIG_BADFORMAT)
	errx (1, "krb5_init_context failed to parse configuration file");
    else if (ret)
	errx (1, "krb5_init_context failed with %d", ret);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    tmp_cf = NULL;
    if(argc == 0)
	krb5_get_default_config_files(&argv);

    while(*argv) {
	ret = krb5_config_parse_file_multi(context, *argv, &tmp_cf);
	if (ret != 0)
	    krb5_warn (context, ret, "krb5_config_parse_file");
	argv++;
    }

    if(dumpconfig_flag)
	dumpconfig(0, tmp_cf);

    return check_section(context, "", tmp_cf, toplevel_sections);
}
