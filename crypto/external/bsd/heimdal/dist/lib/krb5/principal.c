/*	$NetBSD: principal.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997-2007 Kungliga Tekniska Högskolan
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

/**
 * @page krb5_principal_intro The principal handing functions.
 *
 * A Kerberos principal is a email address looking string that
 * contains two parts separated by @.  The second part is the kerberos
 * realm the principal belongs to and the first is a list of 0 or
 * more components. For example
 * @verbatim
lha@SU.SE
host/hummel.it.su.se@SU.SE
host/admin@H5L.ORG
@endverbatim
 *
 * See the library functions here: @ref krb5_principal
 */

#include "krb5_locl.h"
#ifdef HAVE_RES_SEARCH
#define USE_RESOLVER
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#include <fnmatch.h>
#include <krb5/resolve.h>

#define princ_num_comp(P) ((P)->name.name_string.len)
#define princ_type(P) ((P)->name.name_type)
#define princ_comp(P) ((P)->name.name_string.val)
#define princ_ncomp(P, N) ((P)->name.name_string.val[(N)])
#define princ_realm(P) ((P)->realm)

static krb5_error_code
set_default_princ_type(krb5_principal p, NAME_TYPE defnt)
{
    if (princ_num_comp(p) > 1 && strcmp(princ_ncomp(p, 0), KRB5_TGS_NAME) == 0)
        princ_type(p) = KRB5_NT_SRV_INST;
    else if (princ_num_comp(p) > 1 && strcmp(princ_ncomp(p, 0), "host") == 0)
        princ_type(p) = KRB5_NT_SRV_HST;
    else if (princ_num_comp(p) > 1 && strcmp(princ_ncomp(p, 0), "kca_service") == 0)
	princ_type(p) = KRB5_NT_SRV_HST;
    else if (princ_num_comp(p) == 2 &&
             strcmp(princ_ncomp(p, 0), KRB5_WELLKNOWN_NAME) == 0)
        princ_type(p) = KRB5_NT_WELLKNOWN;
    else if (princ_num_comp(p) == 1 && strchr(princ_ncomp(p, 0), '@') != NULL)
        princ_type(p) = KRB5_NT_SMTP_NAME;
    else
        princ_type(p) = defnt;
    return 0;
}

static krb5_error_code append_component(krb5_context, krb5_principal,
                                        const char *, size_t);

/**
 * Frees a Kerberos principal allocated by the library with
 * krb5_parse_name(), krb5_make_principal() or any other related
 * principal functions.
 *
 * @param context A Kerberos context.
 * @param p a principal to free.
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_free_principal(krb5_context context,
		    krb5_principal p)
{
    if(p){
	free_Principal(p);
	free(p);
    }
}

/**
 * Set the type of the principal
 *
 * @param context A Kerberos context.
 * @param principal principal to set the type for
 * @param type the new type
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_principal_set_type(krb5_context context,
			krb5_principal principal,
			int type)
{
    princ_type(principal) = type;
}

/**
 * Get the type of the principal
 *
 * @param context A Kerberos context.
 * @param principal principal to get the type for
 *
 * @return the type of principal
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION int KRB5_LIB_CALL
krb5_principal_get_type(krb5_context context,
			krb5_const_principal principal)
{
    return princ_type(principal);
}

/**
 * Get the realm of the principal
 *
 * @param context A Kerberos context.
 * @param principal principal to get the realm for
 *
 * @return realm of the principal, don't free or use after krb5_principal is freed
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION const char* KRB5_LIB_CALL
krb5_principal_get_realm(krb5_context context,
			 krb5_const_principal principal)
{
    return princ_realm(principal);
}

KRB5_LIB_FUNCTION const char* KRB5_LIB_CALL
krb5_principal_get_comp_string(krb5_context context,
			       krb5_const_principal principal,
			       unsigned int component)
{
    if(component >= princ_num_comp(principal))
       return NULL;
    return princ_ncomp(principal, component);
}

/**
 * Get number of component is principal.
 *
 * @param context Kerberos 5 context
 * @param principal principal to query
 *
 * @return number of components in string
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION unsigned int KRB5_LIB_CALL
krb5_principal_get_num_comp(krb5_context context,
			    krb5_const_principal principal)
{
    return princ_num_comp(principal);
}

/**
 * Parse a name into a krb5_principal structure, flags controls the behavior.
 *
 * @param context Kerberos 5 context
 * @param name name to parse into a Kerberos principal
 * @param flags flags to control the behavior
 * @param principal returned principal, free with krb5_free_principal().
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_parse_name_flags(krb5_context context,
		      const char *name,
		      int flags,
		      krb5_principal *principal)
{
    krb5_error_code ret;
    heim_general_string *comp;
    heim_general_string realm = NULL;
    int ncomp;

    const char *p;
    char *q;
    char *s;
    char *start;

    int n;
    char c;
    int got_realm = 0;
    int first_at = 1;
    int no_realm = flags & KRB5_PRINCIPAL_PARSE_NO_REALM;
    int require_realm = flags & KRB5_PRINCIPAL_PARSE_REQUIRE_REALM;
    int enterprise = flags & KRB5_PRINCIPAL_PARSE_ENTERPRISE;
    int ignore_realm = flags & KRB5_PRINCIPAL_PARSE_IGNORE_REALM;
    int no_def_realm = flags & KRB5_PRINCIPAL_PARSE_NO_DEF_REALM;

    *principal = NULL;

    if (no_realm && require_realm) {
	krb5_set_error_message(context, KRB5_ERR_NO_SERVICE,
			       N_("Can't require both realm and "
				  "no realm at the same time", ""));
	return KRB5_ERR_NO_SERVICE;
    }

    /* count number of component,
     * enterprise names only have one component
     */
    ncomp = 1;
    if (!enterprise) {
	for (p = name; *p; p++) {
	    if (*p=='\\') {
		if (!p[1]) {
		    krb5_set_error_message(context, KRB5_PARSE_MALFORMED,
					   N_("trailing \\ in principal name", ""));
		    return KRB5_PARSE_MALFORMED;
		}
		p++;
	    } else if (*p == '/')
		ncomp++;
	    else if (*p == '@')
		break;
	}
    }
    comp = calloc(ncomp, sizeof(*comp));
    if (comp == NULL)
	return krb5_enomem(context);

    n = 0;
    p = start = q = s = strdup(name);
    if (start == NULL) {
	free(comp);
	return krb5_enomem(context);
    }
    while (*p) {
	c = *p++;
	if (c == '\\') {
	    c = *p++;
	    if (c == 'n')
		c = '\n';
	    else if (c == 't')
		c = '\t';
	    else if (c == 'b')
		c = '\b';
	    else if (c == '0')
		c = '\0';
	    else if (c == '\0') {
		ret = KRB5_PARSE_MALFORMED;
		krb5_set_error_message(context, ret,
				       N_("trailing \\ in principal name", ""));
		goto exit;
	    }
	} else if (enterprise && first_at) {
	    if (c == '@')
		first_at = 0;
	} else if ((c == '/' && !enterprise) || c == '@') {
	    if (got_realm) {
		ret = KRB5_PARSE_MALFORMED;
		krb5_set_error_message(context, ret,
				       N_("part after realm in principal name", ""));
		goto exit;
	    } else {
		comp[n] = malloc(q - start + 1);
		if (comp[n] == NULL) {
		    ret = krb5_enomem(context);
		    goto exit;
		}
		memcpy(comp[n], start, q - start);
		comp[n][q - start] = 0;
		n++;
	    }
	    if (c == '@')
		got_realm = 1;
	    start = q;
	    continue;
	}
	if (got_realm && (c == '/' || c == '\0')) {
	    ret = KRB5_PARSE_MALFORMED;
	    krb5_set_error_message(context, ret,
				   N_("part after realm in principal name", ""));
	    goto exit;
	}
	*q++ = c;
    }
    if (got_realm) {
	if (no_realm) {
	    ret = KRB5_PARSE_MALFORMED;
	    krb5_set_error_message(context, ret,
				   N_("realm found in 'short' principal "
				      "expected to be without one", ""));
	    goto exit;
	}
	if (!ignore_realm) {
	    realm = malloc(q - start + 1);
	    if (realm == NULL) {
		ret = krb5_enomem(context);
		goto exit;
	    }
	    memcpy(realm, start, q - start);
	    realm[q - start] = 0;
	}
    } else {
	if (require_realm) {
	    ret = KRB5_PARSE_MALFORMED;
	    krb5_set_error_message(context, ret,
				   N_("realm NOT found in principal "
				      "expected to be with one", ""));
	    goto exit;
	} else if (no_realm || no_def_realm) {
	    realm = NULL;
	} else {
	    ret = krb5_get_default_realm(context, &realm);
	    if (ret)
		goto exit;
	}

	comp[n] = malloc(q - start + 1);
	if (comp[n] == NULL) {
	    ret = krb5_enomem(context);
	    goto exit;
	}
	memcpy(comp[n], start, q - start);
	comp[n][q - start] = 0;
	n++;
    }
    *principal = calloc(1, sizeof(**principal));
    if (*principal == NULL) {
	ret = krb5_enomem(context);
	goto exit;
    }
    (*principal)->name.name_string.val = comp;
    princ_num_comp(*principal) = n;
    (*principal)->realm = realm;
    if (enterprise)
        princ_type(*principal) = KRB5_NT_ENTERPRISE_PRINCIPAL;
    else
        set_default_princ_type(*principal, KRB5_NT_PRINCIPAL);
    free(s);
    return 0;
exit:
    while (n>0) {
	free(comp[--n]);
    }
    free(comp);
    krb5_free_default_realm(context, realm);
    free(s);
    return ret;
}

/**
 * Parse a name into a krb5_principal structure
 *
 * @param context Kerberos 5 context
 * @param name name to parse into a Kerberos principal
 * @param principal returned principal, free with krb5_free_principal().
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_parse_name(krb5_context context,
		const char *name,
		krb5_principal *principal)
{
    return krb5_parse_name_flags(context, name, 0, principal);
}

static const char quotable_chars[] = " \n\t\b\\/@";
static const char replace_chars[] = " ntb\\/@";

#define add_char(BASE, INDEX, LEN, C) do { if((INDEX) < (LEN)) (BASE)[(INDEX)++] = (C); }while(0);

static size_t
quote_string(const char *s, char *out, size_t idx, size_t len, int display)
{
    const char *p, *q;
    for(p = s; *p && idx < len; p++){
	q = strchr(quotable_chars, *p);
	if (q && display) {
	    add_char(out, idx, len, replace_chars[q - quotable_chars]);
	} else if (q) {
	    add_char(out, idx, len, '\\');
	    add_char(out, idx, len, replace_chars[q - quotable_chars]);
	}else
	    add_char(out, idx, len, *p);
    }
    if(idx < len)
	out[idx] = '\0';
    return idx;
}


static krb5_error_code
unparse_name_fixed(krb5_context context,
		   krb5_const_principal principal,
		   char *name,
		   size_t len,
		   int flags)
{
    size_t idx = 0;
    size_t i;
    int short_form = (flags & KRB5_PRINCIPAL_UNPARSE_SHORT) != 0;
    int no_realm = (flags & KRB5_PRINCIPAL_UNPARSE_NO_REALM) != 0;
    int display = (flags & KRB5_PRINCIPAL_UNPARSE_DISPLAY) != 0;

    if (!no_realm && princ_realm(principal) == NULL) {
	krb5_set_error_message(context, ERANGE,
			       N_("Realm missing from principal, "
				  "can't unparse", ""));
	return ERANGE;
    }

    for(i = 0; i < princ_num_comp(principal); i++){
	if(i)
	    add_char(name, idx, len, '/');
	idx = quote_string(princ_ncomp(principal, i), name, idx, len, display);
	if(idx == len) {
	    krb5_set_error_message(context, ERANGE,
				   N_("Out of space printing principal", ""));
	    return ERANGE;
	}
    }
    /* add realm if different from default realm */
    if(short_form && !no_realm) {
	krb5_realm r;
	krb5_error_code ret;
	ret = krb5_get_default_realm(context, &r);
	if(ret)
	    return ret;
	if(strcmp(princ_realm(principal), r) != 0)
	    short_form = 0;
	krb5_free_default_realm(context, r);
    }
    if(!short_form && !no_realm) {
	add_char(name, idx, len, '@');
	idx = quote_string(princ_realm(principal), name, idx, len, display);
	if(idx == len) {
	    krb5_set_error_message(context, ERANGE,
				   N_("Out of space printing "
				      "realm of principal", ""));
	    return ERANGE;
	}
    }
    return 0;
}

/**
 * Unparse the principal name to a fixed buffer
 *
 * @param context A Kerberos context.
 * @param principal principal to unparse
 * @param name buffer to write name to
 * @param len length of buffer
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_unparse_name_fixed(krb5_context context,
			krb5_const_principal principal,
			char *name,
			size_t len)
{
    return unparse_name_fixed(context, principal, name, len, 0);
}

/**
 * Unparse the principal name to a fixed buffer. The realm is skipped
 * if its a default realm.
 *
 * @param context A Kerberos context.
 * @param principal principal to unparse
 * @param name buffer to write name to
 * @param len length of buffer
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_unparse_name_fixed_short(krb5_context context,
			      krb5_const_principal principal,
			      char *name,
			      size_t len)
{
    return unparse_name_fixed(context, principal, name, len,
			      KRB5_PRINCIPAL_UNPARSE_SHORT);
}

/**
 * Unparse the principal name with unparse flags to a fixed buffer.
 *
 * @param context A Kerberos context.
 * @param principal principal to unparse
 * @param flags unparse flags
 * @param name buffer to write name to
 * @param len length of buffer
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_unparse_name_fixed_flags(krb5_context context,
			      krb5_const_principal principal,
			      int flags,
			      char *name,
			      size_t len)
{
    return unparse_name_fixed(context, principal, name, len, flags);
}

static krb5_error_code
unparse_name(krb5_context context,
	     krb5_const_principal principal,
	     char **name,
	     int flags)
{
    size_t len = 0, plen;
    size_t i;
    krb5_error_code ret;
    /* count length */
    if (princ_realm(principal)) {
	plen = strlen(princ_realm(principal));

	if(strcspn(princ_realm(principal), quotable_chars) == plen)
	    len += plen;
	else
	    len += 2*plen;
	len++; /* '@' */
    }
    for(i = 0; i < princ_num_comp(principal); i++){
	plen = strlen(princ_ncomp(principal, i));
	if(strcspn(princ_ncomp(principal, i), quotable_chars) == plen)
	    len += plen;
	else
	    len += 2*plen;
	len++;
    }
    len++; /* '\0' */
    *name = malloc(len);
    if(*name == NULL)
	return krb5_enomem(context);
    ret = unparse_name_fixed(context, principal, *name, len, flags);
    if(ret) {
	free(*name);
	*name = NULL;
    }
    return ret;
}

/**
 * Unparse the Kerberos name into a string
 *
 * @param context Kerberos 5 context
 * @param principal principal to query
 * @param name resulting string, free with krb5_xfree()
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_unparse_name(krb5_context context,
		  krb5_const_principal principal,
		  char **name)
{
    return unparse_name(context, principal, name, 0);
}

/**
 * Unparse the Kerberos name into a string
 *
 * @param context Kerberos 5 context
 * @param principal principal to query
 * @param flags flag to determine the behavior
 * @param name resulting string, free with krb5_xfree()
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_unparse_name_flags(krb5_context context,
			krb5_const_principal principal,
			int flags,
			char **name)
{
    return unparse_name(context, principal, name, flags);
}

/**
 * Unparse the principal name to a allocated buffer. The realm is
 * skipped if its a default realm.
 *
 * @param context A Kerberos context.
 * @param principal principal to unparse
 * @param name returned buffer, free with krb5_xfree()
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_unparse_name_short(krb5_context context,
			krb5_const_principal principal,
			char **name)
{
    return unparse_name(context, principal, name, KRB5_PRINCIPAL_UNPARSE_SHORT);
}

/**
 * Set a new realm for a principal, and as a side-effect free the
 * previous realm.
 *
 * @param context A Kerberos context.
 * @param principal principal set the realm for
 * @param realm the new realm to set
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_principal_set_realm(krb5_context context,
			 krb5_principal principal,
			 krb5_const_realm realm)
{
    if (princ_realm(principal))
	free(princ_realm(principal));

    if (realm == NULL)
	princ_realm(principal) = NULL;
    else if ((princ_realm(principal) = strdup(realm)) == NULL)
	return krb5_enomem(context);
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_principal_set_comp_string(krb5_context context,
			       krb5_principal principal,
			       unsigned int k,
                               const char *component)
{
    char *s;
    size_t i;

    for (i = princ_num_comp(principal); i <= k; i++)
        append_component(context, principal, "", 0);
    s = strdup(component);
    if (s == NULL)
        return krb5_enomem(context);
    free(princ_ncomp(principal, k));
    princ_ncomp(principal, k) = s;
    return 0;
}

#ifndef HEIMDAL_SMALLER
/**
 * Build a principal using vararg style building
 *
 * @param context A Kerberos context.
 * @param principal returned principal
 * @param rlen length of realm
 * @param realm realm name
 * @param ... a list of components ended with NULL.
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_build_principal(krb5_context context,
		     krb5_principal *principal,
		     int rlen,
		     krb5_const_realm realm,
		     ...)
{
    krb5_error_code ret;
    va_list ap;
    va_start(ap, realm);
    ret = krb5_build_principal_va(context, principal, rlen, realm, ap);
    va_end(ap);
    return ret;
}
#endif

/**
 * Build a principal using vararg style building
 *
 * @param context A Kerberos context.
 * @param principal returned principal
 * @param realm realm name
 * @param ... a list of components ended with NULL.
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

/* coverity[+alloc : arg-*1] */
KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_make_principal(krb5_context context,
		    krb5_principal *principal,
		    krb5_const_realm realm,
		    ...)
{
    krb5_error_code ret;
    krb5_realm r = NULL;
    va_list ap;
    if(realm == NULL) {
	ret = krb5_get_default_realm(context, &r);
	if(ret)
	    return ret;
	realm = r;
    }
    va_start(ap, realm);
    ret = krb5_build_principal_va(context, principal, strlen(realm), realm, ap);
    va_end(ap);
    if(r)
	krb5_free_default_realm(context, r);
    return ret;
}

static krb5_error_code
append_component(krb5_context context, krb5_principal p,
		 const char *comp,
		 size_t comp_len)
{
    heim_general_string *tmp;
    size_t len = princ_num_comp(p);

    tmp = realloc(princ_comp(p), (len + 1) * sizeof(*tmp));
    if(tmp == NULL)
	return krb5_enomem(context);
    princ_comp(p) = tmp;
    princ_ncomp(p, len) = malloc(comp_len + 1);
    if (princ_ncomp(p, len) == NULL)
	return krb5_enomem(context);
    memcpy (princ_ncomp(p, len), comp, comp_len);
    princ_ncomp(p, len)[comp_len] = '\0';
    princ_num_comp(p)++;
    return 0;
}

static krb5_error_code
va_ext_princ(krb5_context context, krb5_principal p, va_list ap)
{
    krb5_error_code ret = 0;

    while (1){
	const char *s;
	int len;

	if ((len = va_arg(ap, int)) == 0)
	    break;
	s = va_arg(ap, const char*);
	if ((ret = append_component(context, p, s, len)) != 0)
	    break;
    }
    return ret;
}

static krb5_error_code
va_princ(krb5_context context, krb5_principal p, va_list ap)
{
    krb5_error_code ret = 0;

    while (1){
	const char *s;

	if ((s = va_arg(ap, const char*)) == NULL)
	    break;
	if ((ret = append_component(context, p, s, strlen(s))) != 0)
	    break;
    }
    return ret;
}

static krb5_error_code
build_principal(krb5_context context,
		krb5_principal *principal,
		int rlen,
		krb5_const_realm realm,
		krb5_error_code (*func)(krb5_context, krb5_principal, va_list),
		va_list ap)
{
    krb5_error_code ret;
    krb5_principal p;

    *principal = NULL;
    p = calloc(1, sizeof(*p));
    if (p == NULL)
	return krb5_enomem(context);

    princ_realm(p) = strdup(realm);
    if (p->realm == NULL) {
	free(p);
	return krb5_enomem(context);
    }

    ret = func(context, p, ap);
    if (ret == 0) {
	*principal = p;
        set_default_princ_type(p, KRB5_NT_PRINCIPAL);
    } else
	krb5_free_principal(context, p);
    return ret;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_build_principal_va(krb5_context context,
			krb5_principal *principal,
			int rlen,
			krb5_const_realm realm,
			va_list ap)
{
    return build_principal(context, principal, rlen, realm, va_princ, ap);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_build_principal_va_ext(krb5_context context,
			    krb5_principal *principal,
			    int rlen,
			    krb5_const_realm realm,
			    va_list ap)
{
    return build_principal(context, principal, rlen, realm, va_ext_princ, ap);
}


KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_build_principal_ext(krb5_context context,
			 krb5_principal *principal,
			 int rlen,
			 krb5_const_realm realm,
			 ...)
{
    krb5_error_code ret;
    va_list ap;
    va_start(ap, realm);
    ret = krb5_build_principal_va_ext(context, principal, rlen, realm, ap);
    va_end(ap);
    return ret;
}

/**
 * Copy a principal
 *
 * @param context A Kerberos context.
 * @param inprinc principal to copy
 * @param outprinc copied principal, free with krb5_free_principal()
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */


KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_copy_principal(krb5_context context,
		    krb5_const_principal inprinc,
		    krb5_principal *outprinc)
{
    krb5_principal p = malloc(sizeof(*p));
    if (p == NULL)
	return krb5_enomem(context);
    if(copy_Principal(inprinc, p)) {
	free(p);
	return krb5_enomem(context);
    }
    *outprinc = p;
    return 0;
}

/**
 * Return TRUE iff princ1 == princ2 (without considering the realm)
 *
 * @param context Kerberos 5 context
 * @param princ1 first principal to compare
 * @param princ2 second principal to compare
 *
 * @return non zero if equal, 0 if not
 *
 * @ingroup krb5_principal
 * @see krb5_principal_compare()
 * @see krb5_realm_compare()
 */

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
krb5_principal_compare_any_realm(krb5_context context,
				 krb5_const_principal princ1,
				 krb5_const_principal princ2)
{
    size_t i;
    if(princ_num_comp(princ1) != princ_num_comp(princ2))
	return FALSE;
    for(i = 0; i < princ_num_comp(princ1); i++){
	if(strcmp(princ_ncomp(princ1, i), princ_ncomp(princ2, i)) != 0)
	    return FALSE;
    }
    return TRUE;
}

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
_krb5_principal_compare_PrincipalName(krb5_context context,
				      krb5_const_principal princ1,
				      PrincipalName *princ2)
{
    size_t i;
    if (princ_num_comp(princ1) != princ2->name_string.len)
	return FALSE;
    for(i = 0; i < princ_num_comp(princ1); i++){
	if(strcmp(princ_ncomp(princ1, i), princ2->name_string.val[i]) != 0)
	    return FALSE;
    }
    return TRUE;
}


/**
 * Compares the two principals, including realm of the principals and returns
 * TRUE if they are the same and FALSE if not.
 *
 * @param context Kerberos 5 context
 * @param princ1 first principal to compare
 * @param princ2 second principal to compare
 *
 * @ingroup krb5_principal
 * @see krb5_principal_compare_any_realm()
 * @see krb5_realm_compare()
 */

/*
 * return TRUE iff princ1 == princ2
 */

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
krb5_principal_compare(krb5_context context,
		       krb5_const_principal princ1,
		       krb5_const_principal princ2)
{
    if (!krb5_realm_compare(context, princ1, princ2))
	return FALSE;
    return krb5_principal_compare_any_realm(context, princ1, princ2);
}

/**
 * return TRUE iff realm(princ1) == realm(princ2)
 *
 * @param context Kerberos 5 context
 * @param princ1 first principal to compare
 * @param princ2 second principal to compare
 *
 * @ingroup krb5_principal
 * @see krb5_principal_compare_any_realm()
 * @see krb5_principal_compare()
 */

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
krb5_realm_compare(krb5_context context,
		   krb5_const_principal princ1,
		   krb5_const_principal princ2)
{
    return strcmp(princ_realm(princ1), princ_realm(princ2)) == 0;
}

/**
 * return TRUE iff princ matches pattern
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
krb5_principal_match(krb5_context context,
		     krb5_const_principal princ,
		     krb5_const_principal pattern)
{
    size_t i;
    if(princ_num_comp(princ) != princ_num_comp(pattern))
	return FALSE;
    if(fnmatch(princ_realm(pattern), princ_realm(princ), 0) != 0)
	return FALSE;
    for(i = 0; i < princ_num_comp(princ); i++){
	if(fnmatch(princ_ncomp(pattern, i), princ_ncomp(princ, i), 0) != 0)
	    return FALSE;
    }
    return TRUE;
}

/*
 * This is the original krb5_sname_to_principal(), renamed to be a
 * helper of the new one.
 */
static krb5_error_code
krb5_sname_to_principal_old(krb5_context context,
			    const char *realm,
			    const char *hostname,
			    const char *sname,
			    int32_t type,
			    krb5_principal *ret_princ)
{
    krb5_error_code ret;
    char localhost[MAXHOSTNAMELEN];
    char **realms = NULL, *host = NULL;

    if(type != KRB5_NT_SRV_HST && type != KRB5_NT_UNKNOWN) {
	krb5_set_error_message(context, KRB5_SNAME_UNSUPP_NAMETYPE,
			       N_("unsupported name type %d", ""),
			       (int)type);
	return KRB5_SNAME_UNSUPP_NAMETYPE;
    }
    if(hostname == NULL) {
	ret = gethostname(localhost, sizeof(localhost) - 1);
	if (ret != 0) {
	    ret = errno;
	    krb5_set_error_message(context, ret,
				   N_("Failed to get local hostname", ""));
	    return ret;
	}
	localhost[sizeof(localhost) - 1] = '\0';
	hostname = localhost;
    }
    if(sname == NULL)
	sname = "host";
    if(type == KRB5_NT_SRV_HST) {
	if (realm)
	    ret = krb5_expand_hostname(context, hostname, &host);
	else
	    ret = krb5_expand_hostname_realms(context, hostname,
					      &host, &realms);
	if (ret)
	    return ret;
	strlwr(host);
	hostname = host;
	if (!realm)
	    realm = realms[0];
    } else if (!realm) {
	ret = krb5_get_host_realm(context, hostname, &realms);
	if(ret)
	    return ret;
	realm = realms[0];
    }

    ret = krb5_make_principal(context, ret_princ, realm, sname,
			      hostname, NULL);
    if(host)
	free(host);
    if (realms)
	krb5_free_host_realm(context, realms);
    return ret;
}

static const struct {
    const char *type;
    int32_t value;
} nametypes[] = {
    { "UNKNOWN", KRB5_NT_UNKNOWN },
    { "PRINCIPAL", KRB5_NT_PRINCIPAL },
    { "SRV_INST", KRB5_NT_SRV_INST },
    { "SRV_HST", KRB5_NT_SRV_HST },
    { "SRV_XHST", KRB5_NT_SRV_XHST },
    { "UID", KRB5_NT_UID },
    { "X500_PRINCIPAL", KRB5_NT_X500_PRINCIPAL },
    { "SMTP_NAME", KRB5_NT_SMTP_NAME },
    { "ENTERPRISE_PRINCIPAL", KRB5_NT_ENTERPRISE_PRINCIPAL },
    { "WELLKNOWN", KRB5_NT_WELLKNOWN },
    { "SRV_HST_DOMAIN", KRB5_NT_SRV_HST_DOMAIN },
    { "ENT_PRINCIPAL_AND_ID", KRB5_NT_ENT_PRINCIPAL_AND_ID },
    { "MS_PRINCIPAL", KRB5_NT_MS_PRINCIPAL },
    { "MS_PRINCIPAL_AND_ID", KRB5_NT_MS_PRINCIPAL_AND_ID },
    { "SRV_HST_NEEDS_CANON", KRB5_NT_SRV_HST_NEEDS_CANON },
    { NULL, 0 }
};

/**
 * Parse nametype string and return a nametype integer
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_parse_nametype(krb5_context context, const char *str, int32_t *nametype)
{
    size_t i;

    for(i = 0; nametypes[i].type; i++) {
	if (strcasecmp(nametypes[i].type, str) == 0) {
	    *nametype = nametypes[i].value;
	    return 0;
	}
    }
    krb5_set_error_message(context, KRB5_PARSE_MALFORMED,
			   N_("Failed to find name type %s", ""), str);
    return KRB5_PARSE_MALFORMED;
}

/**
 * Returns true if name is Kerberos NULL name
 *
 * @ingroup krb5_principal
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_principal_is_null(krb5_context context, krb5_const_principal principal)
{
    if (principal->name.name_type == KRB5_NT_WELLKNOWN &&
	principal->name.name_string.len == 2 &&
	strcmp(principal->name.name_string.val[0], "WELLKNOWN") == 0 &&
	strcmp(principal->name.name_string.val[1], "NULL") == 0)
	return TRUE;
    return FALSE;
}

const char _krb5_wellknown_lkdc[] = "WELLKNOWN:COM.APPLE.LKDC";
static const char lkdc_prefix[] = "LKDC:";

/**
 * Returns true if name is Kerberos an LKDC realm
 *
 * @ingroup krb5_principal
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_realm_is_lkdc(const char *realm)
{

    return strncmp(realm, lkdc_prefix, sizeof(lkdc_prefix)-1) == 0 ||
	strncmp(realm, _krb5_wellknown_lkdc, sizeof(_krb5_wellknown_lkdc) - 1) == 0;
}

/**
 * Returns true if name is Kerberos an LKDC realm
 *
 * @ingroup krb5_principal
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_principal_is_lkdc(krb5_context context, krb5_const_principal principal)
{
    return krb5_realm_is_lkdc(principal->realm);
}

/**
 * Returns true if name is Kerberos an LKDC realm
 *
 * @ingroup krb5_principal
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_principal_is_pku2u(krb5_context context, krb5_const_principal principal)
{
    return strcmp(principal->realm, KRB5_PKU2U_REALM_NAME) == 0;
}

/**
 * Check if the cname part of the principal is a krbtgt principal
 *
 * @ingroup krb5_principal
 */

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
krb5_principal_is_krbtgt(krb5_context context, krb5_const_principal p)
{
    return p->name.name_string.len == 2 &&
	strcmp(p->name.name_string.val[0], KRB5_TGS_NAME) == 0;
}

/**
 * Returns true iff name is an WELLKNOWN:ORG.H5L.HOSTBASED-SERVICE
 *
 * @ingroup krb5_principal
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_principal_is_gss_hostbased_service(krb5_context context,
					krb5_const_principal principal)
{
    if (principal == NULL)
	return FALSE;
    if (principal->name.name_string.len != 2)
	return FALSE;
    if (strcmp(principal->name.name_string.val[1], KRB5_GSS_HOSTBASED_SERVICE_NAME) != 0)
	return FALSE;
    return TRUE;
}

/**
 * Check if the cname part of the principal is a initial or renewed krbtgt principal
 *
 * @ingroup krb5_principal
 */

krb5_boolean KRB5_LIB_FUNCTION
krb5_principal_is_root_krbtgt(krb5_context context, krb5_const_principal p)
{
    return p->name.name_string.len == 2 &&
	strcmp(p->name.name_string.val[0], KRB5_TGS_NAME) == 0 &&
	strcmp(p->name.name_string.val[1], p->realm) == 0;
}

static int
tolower_ascii(int c)
{
    if (c >= 'A' || c <= 'Z')
        return 'a' + (c - 'A');
    return c;
}

typedef enum krb5_name_canon_rule_type {
	KRB5_NCRT_BOGUS = 0,
	KRB5_NCRT_AS_IS,
	KRB5_NCRT_QUALIFY,
	KRB5_NCRT_NSS
} krb5_name_canon_rule_type;

#ifdef UINT8_MAX
#define MAXDOTS UINT8_MAX
#else
#define MAXDOTS (255U)
#endif
#ifdef UINT16_MAX
#define MAXORDER UINT16_MAX
#else
#define MAXORDER (65535U)
#endif

struct krb5_name_canon_rule_data {
	krb5_name_canon_rule_type type;
	krb5_name_canon_rule_options options;
	uint8_t mindots;          /* match this many dots or more */
	uint8_t maxdots;          /* match no more than this many dots */
        uint16_t explicit_order;    /* given order */
        uint16_t order;             /* actual order */
	char *match_domain;         /* match this stem */
	char *match_realm;          /* match this realm */
	char *domain;               /* qualify with this domain */
	char *realm;                /* qualify with this realm */
};

/**
 * Create a principal for the given service running on the given
 * hostname. If KRB5_NT_SRV_HST is used, the hostname is canonicalized
 * according the configured name canonicalization rules, with
 * canonicalization delayed in some cases.  One rule involves DNS, which
 * is insecure unless DNSSEC is used, but we don't use DNSSEC-capable
 * resolver APIs here, so that if DNSSEC is used we wouldn't know it.
 *
 * Canonicalization is immediate (not delayed) only when there is only
 * one canonicalization rule and that rule indicates that we should do a
 * host lookup by name (i.e., DNS).
 *
 * @param context A Kerberos context.
 * @param hostname hostname to use
 * @param sname Service name to use
 * @param type name type of principal, use KRB5_NT_SRV_HST or KRB5_NT_UNKNOWN.
 * @param ret_princ return principal, free with krb5_free_principal().
 *
 * @return An krb5 error code, see krb5_get_error_message().
 *
 * @ingroup krb5_principal
 */

/* coverity[+alloc : arg-*4] */
KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sname_to_principal(krb5_context context,
			const char *hostname,
			const char *sname,
			int32_t type,
			krb5_principal *ret_princ)
{
    char *realm, *remote_host;
    krb5_error_code ret;
    register char *cp;
    char localname[MAXHOSTNAMELEN];

    *ret_princ = NULL;

    if ((type != KRB5_NT_UNKNOWN) &&
	(type != KRB5_NT_SRV_HST))
	return KRB5_SNAME_UNSUPP_NAMETYPE;

    /* if hostname is NULL, use local hostname */
    if (hostname == NULL) {
	if (gethostname(localname, MAXHOSTNAMELEN))
	    return errno;
	hostname = localname;
    }

    /* if sname is NULL, use "host" */
    if (sname == NULL)
	sname = "host";

    remote_host = strdup(hostname);
    if (remote_host == NULL)
	return krb5_enomem(context);

    if (type == KRB5_NT_SRV_HST) {
	krb5_name_canon_rule rules;

	/* Lower-case the hostname, because that's the convention */
	for (cp = remote_host; *cp; cp++)
	    if (isupper((int) (*cp)))
		*cp = tolower((int) (*cp));

        /*
         * If there is only one name canon rule and it says to
         * canonicalize the old way, do that now, as we used to.
         */
	ret = _krb5_get_name_canon_rules(context, &rules);
	if (ret) {
	    _krb5_debug(context, 5, "Failed to get name canon rules: ret = %d",
			ret);
	    free(remote_host);
	    return ret;
	}
	if (rules[0].type == KRB5_NCRT_NSS &&
            rules[1].type == KRB5_NCRT_BOGUS) {
	    _krb5_debug(context, 5, "Using nss for name canon immediately");
            ret = krb5_sname_to_principal_old(context, rules[0].realm,
                                              remote_host, sname,
                                              KRB5_NT_SRV_HST, ret_princ);
	    free(remote_host);
	    return ret;
	}
    }

    /* Remove trailing dots */
    if (remote_host[0]) {
	for (cp = remote_host + strlen(remote_host)-1;
             *cp == '.' && cp > remote_host;
             cp--) {
            *cp = '\0';
        }
    }

    realm = ""; /* "Referral realm" */

    ret = krb5_build_principal(context, ret_princ, strlen(realm),
				  realm, sname, remote_host,
				  (char *)0);

    if (ret == 0 && type == KRB5_NT_SRV_HST) {
	/*
	 * Hostname canonicalization is done elsewhere (in
	 * krb5_get_credentials() and krb5_kt_get_entry()).
	 *
         * We overload the name type to indicate to those functions that
         * this principal name requires canonicalization.
         *
         * We can't use the empty realm to denote the need to
         * canonicalize the hostname too: it would mean that users who
         * want to assert knowledge of a service's realm must also know
         * the canonical hostname, but in practice they don't.
	 */
	(*ret_princ)->name.name_type = KRB5_NT_SRV_HST_NEEDS_CANON;

	_krb5_debug(context, 5, "Building a delayed canon principal for %s/%s@",
		sname, remote_host);
    }

    free(remote_host);
    return ret;
}

static void
tolower_str(char *s)
{
    for (; *s != '\0'; s++) {
        if (isupper(*s))
            *s = tolower_ascii(*s);
    }
}

static krb5_error_code
rule_parse_token(krb5_context context, krb5_name_canon_rule rule,
		 const char *tok)
{
    long int n;
    int needs_type = rule->type == KRB5_NCRT_BOGUS;

    /*
     * Rules consist of a sequence of tokens, some of which indicate
     * what type of rule the rule is, and some of which set rule options
     * or ancilliary data.  Last rule type token wins.
     */

    /* Rule type tokens: */
    if (needs_type && strcmp(tok, "as-is") == 0) {
        rule->type = KRB5_NCRT_AS_IS;
    } else if (needs_type && strcmp(tok, "qualify") == 0) {
        rule->type = KRB5_NCRT_QUALIFY;
    } else if (needs_type && strcmp(tok, "nss") == 0) {
        rule->type = KRB5_NCRT_NSS;
    /* Rule options: */
    } else if (strcmp(tok, "use_fast") == 0) {
	rule->options |= KRB5_NCRO_USE_FAST;
    } else if (strcmp(tok, "use_dnssec") == 0) {
	rule->options |= KRB5_NCRO_USE_DNSSEC;
    } else if (strcmp(tok, "ccache_only") == 0) {
	rule->options |= KRB5_NCRO_GC_ONLY;
    } else if (strcmp(tok, "no_referrals") == 0) {
	rule->options |= KRB5_NCRO_NO_REFERRALS;
    } else if (strcmp(tok, "use_referrals") == 0) {
	rule->options &= ~KRB5_NCRO_NO_REFERRALS;
        if (rule->realm == NULL) {
            rule->realm = strdup("");
            if (rule->realm == NULL)
                return krb5_enomem(context);
        }
    } else if (strcmp(tok, "lookup_realm") == 0) {
        rule->options |= KRB5_NCRO_LOOKUP_REALM;
        free(rule->realm);
        rule->realm = NULL;
    /* Rule ancilliary data: */
    } else if (strncmp(tok, "domain=", strlen("domain=")) == 0) {
	free(rule->domain);
	rule->domain = strdup(tok + strlen("domain="));
	if (rule->domain == NULL)
	    return krb5_enomem(context);
        tolower_str(rule->domain);
    } else if (strncmp(tok, "realm=", strlen("realm=")) == 0) {
	free(rule->realm);
	rule->realm = strdup(tok + strlen("realm="));
	if (rule->realm == NULL)
	    return krb5_enomem(context);
    } else if (strncmp(tok, "match_domain=", strlen("match_domain=")) == 0) {
	free(rule->match_domain);
	rule->match_domain = strdup(tok + strlen("match_domain="));
	if (rule->match_domain == NULL)
	    return krb5_enomem(context);
        tolower_str(rule->match_domain);
    } else if (strncmp(tok, "match_realm=", strlen("match_realm=")) == 0) {
	free(rule->match_realm);
	rule->match_realm = strdup(tok + strlen("match_realm="));
	if (rule->match_realm == NULL)
	    return krb5_enomem(context);
    } else if (strncmp(tok, "mindots=", strlen("mindots=")) == 0) {
	errno = 0;
	n = strtol(tok + strlen("mindots="), NULL, 10);
	if (errno == 0 && n > 0 && n <= MAXDOTS)
	    rule->mindots = n;
    } else if (strncmp(tok, "maxdots=", strlen("maxdots=")) == 0) {
	errno = 0;
	n = strtol(tok + strlen("maxdots="), NULL, 10);
	if (errno == 0 && n > 0 && n <= MAXDOTS)
	    rule->maxdots = n;
    } else if (strncmp(tok, "order=", strlen("order=")) == 0) {
	errno = 0;
	n = strtol(tok + strlen("order="), NULL, 10);
	if (errno == 0 && n > 0 && n <= MAXORDER)
	    rule->explicit_order = n;
    } else {
        _krb5_debug(context, 5,
                    "Unrecognized name canonicalization rule token %s", tok);
        return EINVAL;
    }
    return 0;
}

static int
rule_cmp(const void *a, const void *b)
{
    krb5_const_name_canon_rule left = a;
    krb5_const_name_canon_rule right = b;

    if (left->type == KRB5_NCRT_BOGUS &&
        right->type == KRB5_NCRT_BOGUS)
        return 0;
    if (left->type == KRB5_NCRT_BOGUS)
        return 1;
    if (right->type == KRB5_NCRT_BOGUS)
        return -1;
    if (left->explicit_order < right->explicit_order)
        return -1;
    if (left->explicit_order > right->explicit_order)
        return 1;
    return left->order - right->order;
}

static krb5_error_code
parse_name_canon_rules(krb5_context context, char **rulestrs,
		       krb5_name_canon_rule *rules)
{
    krb5_error_code ret;
    char *tok;
    char *cp;
    char **cpp;
    size_t n;
    size_t i, k;
    int do_sort = 0;
    krb5_name_canon_rule r;

    *rules = NULL;

    for (n =0, cpp = rulestrs; cpp != NULL && *cpp != NULL; cpp++)
	n++;

    n += 2; /* Always at least one rule; two for the default case */

    if ((r = calloc(n, sizeof (*r))) == NULL)
	return krb5_enomem(context);

    for (k = 0; k < n; k++) {
        r[k].type = KRB5_NCRT_BOGUS;
        r[k].match_domain = NULL;
        r[k].match_realm = NULL;
        r[k].domain = NULL;
        r[k].realm = NULL;
    }

    for (i = 0, k = 0; i < n && rulestrs != NULL && rulestrs[i] != NULL; i++) {
	cp = rulestrs[i];
        r[k].explicit_order = MAXORDER; /* mark order, see below */
        r[k].maxdots = MAXDOTS;
        r[k].order = k;         /* default order */

        /* Tokenize and parse value */
	do {
	    tok = cp;
	    cp = strchr(cp, ':');   /* XXX use strtok_r() */
	    if (cp)
		*cp++ = '\0';       /* delimit token */
	    ret = rule_parse_token(context, &r[k], tok);
            if (ret == EINVAL) {
                r[k].type = KRB5_NCRT_BOGUS;
                break;
            }
            if (ret) {
                _krb5_free_name_canon_rules(context, r);
                return ret;
            }
	} while (cp && *cp);
        if (r[k].explicit_order != MAXORDER)
            do_sort = 1;

	/* Validate parsed rule */
	if (r[k].type == KRB5_NCRT_BOGUS ||
	    (r[k].type == KRB5_NCRT_QUALIFY && !r[k].domain) ||
	    (r[k].type == KRB5_NCRT_NSS && r[k].domain)) {
	    /* Invalid rule; mark it so and clean up */
	    r[k].type = KRB5_NCRT_BOGUS;
	    free(r[k].match_domain);
	    free(r[k].match_realm);
	    free(r[k].domain);
	    free(r[k].realm);
	    r[k].realm = NULL;
	    r[k].domain = NULL;
	    r[k].match_domain = NULL;
	    r[k].match_realm = NULL;
            _krb5_debug(context, 5,
                        "Ignoring invalid name canonicalization rule %lu",
                        (unsigned long)i);
	    continue;
	}
	k++; /* good rule */
    }

    if (do_sort) {
        /*
         * Note that we make make this a stable sort by using appareance
         * and explicit order.
         */
        qsort(r, n, sizeof(r[0]), rule_cmp);
    }

    if (r[0].type == KRB5_NCRT_BOGUS) {
        /* No rules, or no valid rules */
        r[0].type = KRB5_NCRT_NSS;
    }

    *rules = r;
    return 0; /* We don't communicate bad rule errors here */
}

/*
 * This exists only because the hostname canonicalization behavior in Heimdal
 * (and other implementations of Kerberos) has been to use getaddrinfo(),
 * unsafe though it is, for ages.  We can't fix it in one day.
 */
static void
make_rules_safe(krb5_context context, krb5_name_canon_rule rules)
{
    /*
     * If the only rule were to use the name service (getaddrinfo()) then we're
     * bound to fail.  We could try to convert that rule to an as-is rule, but
     * when we do get a validating resolver we'd be unhappy that we did such a
     * conversion.  Better let the user get failures and make them think about
     * their naming rules.
     */
    if (rules == NULL)
        return;
    for (; rules[0].type != KRB5_NCRT_BOGUS; rules++) {
        if (rules->type == KRB5_NCRT_NSS)
            rules->options |= KRB5_NCRO_USE_DNSSEC;
        else
            rules->options |= KRB5_NCRO_USE_FAST;
    }
}

/**
 * This function returns an array of host-based service name
 * canonicalization rules.  The array of rules is organized as a list.
 * See the definition of krb5_name_canon_rule.
 *
 * @param context A Kerberos context.
 * @param rules   Output location for array of rules.
 */
KRB5_LIB_FUNCTION krb5_error_code
_krb5_get_name_canon_rules(krb5_context context, krb5_name_canon_rule *rules)
{
    krb5_error_code ret;
    char **values = NULL;

    *rules = context->name_canon_rules;
    if (*rules != NULL)
        return 0;

    values = krb5_config_get_strings(context, NULL,
                                     "libdefaults", "name_canon_rules", NULL);
    ret = parse_name_canon_rules(context, values, rules);
    krb5_config_free_strings(values);
    if (ret)
	return ret;

    if (krb5_config_get_bool_default(context, NULL, FALSE,
                                     "libdefaults", "safe_name_canon", NULL))
        make_rules_safe(context, *rules);

    heim_assert(rules != NULL && (*rules)[0].type != KRB5_NCRT_BOGUS,
                "internal error in parsing principal name "
                "canonicalization rules");

    /* Memoize */
    context->name_canon_rules = *rules;

    return 0;
}

static krb5_error_code
get_host_realm(krb5_context context, const char *hostname, char **realm)
{
    krb5_error_code ret;
    char **hrealms = NULL;

    *realm = NULL;
    ret = krb5_get_host_realm(context, hostname, &hrealms);
    if (ret)
	return ret;
    if (hrealms == NULL)
	return KRB5_ERR_HOST_REALM_UNKNOWN; /* krb5_set_error() already done */
    if (hrealms[0] == NULL) {
	krb5_free_host_realm(context, hrealms);
	return KRB5_ERR_HOST_REALM_UNKNOWN; /* krb5_set_error() already done */
    }
    *realm = strdup(hrealms[0]);
    krb5_free_host_realm(context, hrealms);
    if (*realm == NULL)
        return krb5_enomem(context);
    return 0;
}

static int
is_domain_suffix(const char *domain, const char *suffix)
{
    size_t dlen = strlen(domain);
    size_t slen = strlen(suffix);

    if (dlen < slen + 2)
        return 0;

    if (strcasecmp(domain + (dlen - slen), suffix) != 0)
        return 0;

    if (domain[(dlen - slen) - 1] != '.')
        return 0;
    return 1;
}

/*
 * Applies a name canonicalization rule to a principal.
 *
 * Returns zero and no out_princ if the rule does not match.
 * Returns zero and an out_princ if the rule does match.
 */
static krb5_error_code
apply_name_canon_rule(krb5_context context, krb5_name_canon_rule rules,
                      size_t rule_idx, krb5_const_principal in_princ,
                      krb5_principal *out_princ,
                      krb5_name_canon_rule_options *rule_opts)
{
    krb5_name_canon_rule rule = &rules[rule_idx];
    krb5_error_code ret;
    unsigned int ndots = 0;
    krb5_principal nss = NULL;
    const char *sname = NULL;
    const char *orig_hostname = NULL;
    const char *new_hostname = NULL;
    const char *new_realm = NULL;
    const char *port = "";
    const char *cp;
    char *hostname_sans_port = NULL;
    char *hostname_with_port = NULL;
    char *tmp_hostname = NULL;
    char *tmp_realm = NULL;

    *out_princ = NULL; /* Signal no match */

    if (rule_opts != NULL)
	*rule_opts = rule->options;

    if (rule->type == KRB5_NCRT_BOGUS)
	return 0; /* rule doesn't apply */

    sname = krb5_principal_get_comp_string(context, in_princ, 0);
    orig_hostname = krb5_principal_get_comp_string(context, in_princ, 1);

    /*
     * Some apps want to use the very non-standard svc/hostname:port@REALM
     * form.  We do our best to support that here :(
     */
    port = strchr(orig_hostname, ':');
    if (port != NULL) {
        hostname_sans_port = strndup(orig_hostname, port - orig_hostname);
        if (hostname_sans_port == NULL)
            return krb5_enomem(context);
        orig_hostname = hostname_sans_port;
    }

    _krb5_debug(context, 5, N_("Applying a name rule (type %d) to %s", ""),
                rule->type, orig_hostname);

    if (rule->mindots > 0 || rule->maxdots > 0) {
        for (cp = strchr(orig_hostname, '.'); cp && *cp; cp = strchr(cp + 1, '.'))
            ndots++;
    }
    if (rule->mindots > 0 && ndots < rule->mindots)
            return 0;
    if (ndots > rule->maxdots)
            return 0;

    if (rule->match_domain != NULL &&
        !is_domain_suffix(orig_hostname, rule->match_domain))
        return 0;

    if (rule->match_realm != NULL &&
        strcmp(rule->match_realm, in_princ->realm) != 0)
          return 0;

    new_realm = rule->realm;
    switch (rule->type) {
    case KRB5_NCRT_AS_IS:
	break;

    case KRB5_NCRT_QUALIFY:
	heim_assert(rule->domain != NULL,
		    "missing domain for qualify name canon rule");
        if (asprintf(&tmp_hostname, "%s.%s", orig_hostname,
                     rule->domain) == -1 || tmp_hostname == NULL) {
            ret = krb5_enomem(context);
            goto out;
        }
        new_hostname = tmp_hostname;
	break;

    case KRB5_NCRT_NSS:
        if ((rule->options & KRB5_NCRO_USE_DNSSEC)) {
            ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
            krb5_set_error_message(context, ret,
                                   "Secure hostname resolution not supported");
            goto out;
        }
	_krb5_debug(context, 5, "Using name service lookups");
	ret = krb5_sname_to_principal_old(context, rule->realm,
					  orig_hostname, sname,
					  KRB5_NT_SRV_HST,
					  &nss);
	if (rules[rule_idx + 1].type != KRB5_NCRT_BOGUS &&
            (ret == KRB5_ERR_BAD_HOSTNAME ||
	     ret == KRB5_ERR_HOST_REALM_UNKNOWN)) {
	    /*
	     * Bad hostname / realm unknown -> rule inapplicable if
	     * there's more rules.  If it's the last rule then we want
	     * to return all errors from krb5_sname_to_principal_old()
	     * here.
	     */
            ret = 0;
            goto out;
        }
        if (ret)
            goto out;

        new_hostname = krb5_principal_get_comp_string(context, nss, 1);
        new_realm = krb5_principal_get_realm(context, nss);
        break;

    default:
        /* Can't happen */
        ret = 0;
	goto out;
    }

    /*
     * This rule applies.
     *
     * Copy in_princ and mutate the copy per the matched rule.
     *
     * This way we apply to principals with two or more components, such as
     * domain-based names.
     */
    ret = krb5_copy_principal(context, in_princ, out_princ);
    if (ret)
        goto out;

    if (new_realm == NULL && (rule->options & KRB5_NCRO_LOOKUP_REALM) != 0) {
        ret = get_host_realm(context, new_hostname, &tmp_realm);
        if (ret)
            goto out;
        new_realm = tmp_realm;
    }

    /* If we stripped off a :port, add it back in */
    if (port != NULL && new_hostname != NULL) {
        if (asprintf(&hostname_with_port, "%s%s", new_hostname, port) == -1 ||
            hostname_with_port == NULL) {
            ret = krb5_enomem(context);
            goto out;
        }
        new_hostname = hostname_with_port;
    }

    if (new_realm != NULL)
        krb5_principal_set_realm(context, *out_princ, new_realm);
    if (new_hostname != NULL)
        krb5_principal_set_comp_string(context, *out_princ, 1, new_hostname);
    if (princ_type(*out_princ) == KRB5_NT_SRV_HST_NEEDS_CANON)
        princ_type(*out_princ) = KRB5_NT_SRV_HST;

    /* Trace rule application */
    {
	krb5_error_code ret2;
	char *unparsed;

	ret2 = krb5_unparse_name(context, *out_princ, &unparsed);
	if (ret2) {
	    _krb5_debug(context, 5,
                        N_("Couldn't unparse canonicalized princicpal (%d)",
                           ""),
                        ret);
	} else {
	    _krb5_debug(context, 5,
                        N_("Name canon rule application yields %s", ""),
                        unparsed);
	    free(unparsed);
	}
    }

out:
    free(hostname_sans_port);
    free(hostname_with_port);
    free(tmp_hostname);
    free(tmp_realm);
    krb5_free_principal(context, nss);
    if (ret)
	krb5_set_error_message(context, ret,
			       N_("Name canon rule application failed", ""));
    return ret;
}

/**
 * Free name canonicalization rules
 */
KRB5_LIB_FUNCTION void
_krb5_free_name_canon_rules(krb5_context context, krb5_name_canon_rule rules)
{
    size_t k;

    if (rules == NULL)
        return;

    for (k = 0; rules[k].type != KRB5_NCRT_BOGUS; k++) {
	free(rules[k].match_domain);
	free(rules[k].match_realm);
	free(rules[k].domain);
	free(rules[k].realm);
    }
    free(rules);
}

struct krb5_name_canon_iterator_data {
    krb5_name_canon_rule	rules;
    krb5_const_principal	in_princ;   /* given princ */
    krb5_const_principal	out_princ;  /* princ to be output */
    krb5_principal		tmp_princ;  /* to be freed */
    int				is_trivial; /* no canon to be done */
    int				done;       /* no more rules to be applied */
    size_t                      cursor;     /* current/next rule */
};

/**
 * Initialize name canonicalization iterator.
 *
 * @param context   Kerberos context
 * @param in_princ  principal name to be canonicalized OR
 * @param iter	    output iterator object
 */
KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_name_canon_iterator_start(krb5_context context,
			       krb5_const_principal in_princ,
			       krb5_name_canon_iterator *iter)
{
    krb5_error_code ret;
    krb5_name_canon_iterator state;

    *iter = NULL;

    state = calloc(1, sizeof (*state));
    if (state == NULL)
	return krb5_enomem(context);
    state->in_princ = in_princ;

    if (princ_type(state->in_princ) == KRB5_NT_SRV_HST_NEEDS_CANON) {
	ret = _krb5_get_name_canon_rules(context, &state->rules);
	if (ret)
	    goto out;
    } else {
	/* Name needs no canon -> trivial iterator: in_princ is canonical */
	state->is_trivial = 1;
    }

    *iter = state;
    return 0;

out:
    krb5_free_name_canon_iterator(context, state);
    return krb5_enomem(context);
}

/*
 * Helper for name canon iteration.
 */
static krb5_error_code
name_canon_iterate(krb5_context context,
                   krb5_name_canon_iterator *iter,
                   krb5_name_canon_rule_options *rule_opts)
{
    krb5_error_code ret;
    krb5_name_canon_iterator state = *iter;

    if (rule_opts)
	*rule_opts = 0;

    if (state == NULL)
	return 0;

    if (state->done) {
	krb5_free_name_canon_iterator(context, state);
	*iter = NULL;
	return 0;
    }

    if (state->is_trivial && !state->done) {
        state->out_princ = state->in_princ;
	state->done = 1;
	return 0;
    }

    heim_assert(state->rules != NULL &&
                state->rules[state->cursor].type != KRB5_NCRT_BOGUS,
                "Internal error during name canonicalization");

    do {
	krb5_free_principal(context, state->tmp_princ);
	ret = apply_name_canon_rule(context, state->rules, state->cursor,
	    state->in_princ, &state->tmp_princ, rule_opts);
	if (ret) {
            krb5_free_name_canon_iterator(context, state);
            *iter = NULL;
	    return ret;
        }
	state->cursor++;
    } while (state->tmp_princ == NULL &&
             state->rules[state->cursor].type != KRB5_NCRT_BOGUS);

    if (state->rules[state->cursor].type == KRB5_NCRT_BOGUS)
        state->done = 1;

    state->out_princ = state->tmp_princ;
    if (state->tmp_princ == NULL) {
	krb5_free_name_canon_iterator(context, state);
	*iter = NULL;
	return 0;
    }
    return 0;
}

/**
 * Iteratively apply name canon rules, outputing a principal and rule
 * options each time.  Iteration completes when the @iter is NULL on
 * return or when an error is returned.  Callers must free the iterator
 * if they abandon it mid-way.
 *
 * @param context   Kerberos context
 * @param iter	    name canon rule iterator (input/output)
 * @param try_princ output principal name
 * @param rule_opts output rule options
 */
KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_name_canon_iterate(krb5_context context,
                        krb5_name_canon_iterator *iter,
                        krb5_const_principal *try_princ,
                        krb5_name_canon_rule_options *rule_opts)
{
    krb5_error_code ret;

    *try_princ = NULL;

    ret = name_canon_iterate(context, iter, rule_opts);
    if (*iter)
	*try_princ = (*iter)->out_princ;
    return ret;
}

/**
 * Free a name canonicalization rule iterator.
 */
KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_free_name_canon_iterator(krb5_context context,
			      krb5_name_canon_iterator iter)
{
    if (iter == NULL)
	return;
    if (iter->tmp_princ)
        krb5_free_principal(context, iter->tmp_princ);
    free(iter);
}
