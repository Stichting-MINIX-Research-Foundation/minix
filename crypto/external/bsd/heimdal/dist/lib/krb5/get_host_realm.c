/*	$NetBSD: get_host_realm.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
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
#include <krb5/resolve.h>

/* To automagically find the correct realm of a host (without
 * [domain_realm] in krb5.conf) add a text record for your domain with
 * the name of your realm, like this:
 *
 * _kerberos	IN	TXT	"FOO.SE"
 *
 * The search is recursive, so you can add entries for specific
 * hosts. To find the realm of host a.b.c, it first tries
 * _kerberos.a.b.c, then _kerberos.b.c and so on.
 *
 * This method is described in draft-ietf-cat-krb-dns-locate-03.txt.
 *
 */

static int
copy_txt_to_realms(krb5_context context,
		   const char *domain,
		   struct rk_resource_record *head,
		   krb5_realm **realms)
{
    struct rk_resource_record *rr;
    unsigned int n, i;

    for(n = 0, rr = head; rr; rr = rr->next)
	if (rr->type == rk_ns_t_txt)
	    ++n;

    if (n == 0)
	return -1;

    *realms = malloc ((n + 1) * sizeof(krb5_realm));
    if (*realms == NULL)
	return krb5_enomem(context);;

    for (i = 0; i < n + 1; ++i)
	(*realms)[i] = NULL;

    for (i = 0, rr = head; rr; rr = rr->next) {
	if (rr->type == rk_ns_t_txt) {
	    char *tmp = NULL;
	    int invalid_tld = 1;

	    /* Check for a gTLD controlled interruption */
	    if (strcmp("Your DNS configuration needs immediate "
			"attention see https://icann.org/namecollision",
			rr->u.txt) != 0) {
		invalid_tld = 0;
		tmp = strdup(rr->u.txt);
	    }
	    if (tmp == NULL) {
		for (i = 0; i < n; ++i)
		    free ((*realms)[i]);
		free (*realms);
		if (invalid_tld) {
		    krb5_warnx(context,
			       "Realm lookup failed: "
			       "Domain '%s' needs immediate attention "
			       "see https://icann.org/namecollision",
				domain);
		    return KRB5_KDC_UNREACH;
		}
		return krb5_enomem(context);;
	    }
	    (*realms)[i] = tmp;
	    ++i;
	}
    }
    return 0;
}

static int
dns_find_realm(krb5_context context,
	       const char *domain,
	       krb5_realm **realms)
{
    static const char *default_labels[] = { "_kerberos", NULL };
    char dom[MAXHOSTNAMELEN];
    struct rk_dns_reply *r;
    const char **labels;
    char **config_labels;
    int i, ret = 0;

    config_labels = krb5_config_get_strings(context, NULL, "libdefaults",
					    "dns_lookup_realm_labels", NULL);
    if(config_labels != NULL)
	labels = (const char **)config_labels;
    else
	labels = default_labels;
    if(*domain == '.')
	domain++;
    for (i = 0; labels[i] != NULL; i++) {
	ret = snprintf(dom, sizeof(dom), "%s.%s.", labels[i], domain);
	if(ret < 0 || (size_t)ret >= sizeof(dom)) {
	    ret = krb5_enomem(context);
	    goto out;
	}
    	r = rk_dns_lookup(dom, "TXT");
    	if(r != NULL) {
	    ret = copy_txt_to_realms(context, domain, r->head, realms);
	    rk_dns_free_data(r);
	    if(ret == 0)
		goto out;
	}
    }
    krb5_set_error_message(context, KRB5_KDC_UNREACH,
			    "Realm lookup failed: "
			    "No DNS TXT record for %s",
			    domain);
    ret = KRB5_KDC_UNREACH;
out:
    if (config_labels)
	krb5_config_free_strings(config_labels);
    return ret;
}

/*
 * Try to figure out what realms host in `domain' belong to from the
 * configuration file.
 */

static int
config_find_realm(krb5_context context,
		  const char *domain,
		  krb5_realm **realms)
{
    char **tmp = krb5_config_get_strings (context, NULL,
					  "domain_realm",
					  domain,
					  NULL);

    if (tmp == NULL)
	return -1;
    *realms = tmp;
    return 0;
}

/*
 * This function assumes that `host' is a FQDN (and doesn't handle the
 * special case of host == NULL either).
 * Try to find mapping in the config file or DNS and it that fails,
 * fall back to guessing
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_get_host_realm_int(krb5_context context,
                         const char *host,
                         krb5_boolean use_dns,
                         krb5_realm **realms)
{
    const char *p, *q;
    const char *port;
    krb5_boolean dns_locate_enable;
    krb5_error_code ret = 0;

    /* Strip off any trailing ":port" suffix. */
    port = strchr(host, ':');
    if (port != NULL) {
        host = strndup(host, port - host);
        if (host == NULL)
            return krb5_enomem(context);
    }

    dns_locate_enable = krb5_config_get_bool_default(context, NULL, TRUE,
        "libdefaults", "dns_lookup_realm", NULL);
    for (p = host; p != NULL; p = strchr (p + 1, '.')) {
        if (config_find_realm(context, p, realms) == 0) {
            if (strcasecmp(*realms[0], "dns_locate") != 0)
                break;
	    krb5_free_host_realm(context, *realms);
	    *realms = NULL;
            if (!use_dns)
                continue;
            for (q = host; q != NULL; q = strchr(q + 1, '.'))
                if (dns_find_realm(context, q, realms) == 0)
                    break;
            if (q)
                break;
        } else if (use_dns && dns_locate_enable) {
            if (dns_find_realm(context, p, realms) == 0)
                break;
        }
    }

    /*
     * If 'p' is NULL, we did not find an explicit realm mapping in either the
     * configuration file or DNS.  Try the hostname suffix as a last resort.
     *
     * XXX: If we implement a KDC-specific variant of this function just for
     * referrals, we could check whether we have a cross-realm TGT for the
     * realm in question, and if not try the parent (loop again).
     */
    if (p == NULL) {
        p = strchr(host, '.');
        if (p != NULL) {
            p++;
            *realms = malloc(2 * sizeof(krb5_realm));
            if (*realms != NULL &&
                ((*realms)[0] = strdup(p)) != NULL) {
                strupr((*realms)[0]);
                (*realms)[1] = NULL;
            } else {
                free(*realms);
                ret = krb5_enomem(context);
            }
        } else {
            krb5_set_error_message(context, KRB5_ERR_HOST_REALM_UNKNOWN,
                                   N_("unable to find realm of host %s", ""),
                                   host);
            ret = KRB5_ERR_HOST_REALM_UNKNOWN;
        }
    }

    /* If 'port' is not NULL, we have a copy of 'host' to free. */
    if (port)
        free((void *)host);
    return ret;
}

/*
 * Return the realm(s) of `host' as a NULL-terminated list in
 * `realms'. Free `realms' with krb5_free_host_realm().
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_host_realm(krb5_context context,
		    const char *targethost,
		    krb5_realm **realms)
{
    const char *host = targethost;
    char hostname[MAXHOSTNAMELEN];
    krb5_error_code ret;
    int use_dns;

    if (host == NULL) {
	if (gethostname (hostname, sizeof(hostname))) {
	    *realms = NULL;
	    return errno;
	}
	host = hostname;
    }

    /*
     * If our local hostname is without components, don't even try to dns.
     */

    use_dns = (strchr(host, '.') != NULL);

    ret = _krb5_get_host_realm_int (context, host, use_dns, realms);
    if (ret && targethost != NULL) {
	/*
	 * If there was no realm mapping for the host (and we wasn't
	 * looking for ourself), guess at the local realm, maybe our
	 * KDC knows better then we do and we get a referral back.
	 */
	ret = krb5_get_default_realms(context, realms);
	if (ret) {
	    krb5_set_error_message(context, KRB5_ERR_HOST_REALM_UNKNOWN,
				   N_("Unable to find realm of host %s", ""),
				   host);
	    return KRB5_ERR_HOST_REALM_UNKNOWN;
	}
    }
    return ret;
}
