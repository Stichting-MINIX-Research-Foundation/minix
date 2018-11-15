/*	$NetBSD: transited.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2001, 2003 Kungliga Tekniska Högskolan
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

/* this is an attempt at one of the most horrible `compression'
   schemes that has ever been invented; it's so amazingly brain-dead
   that words can not describe it, and all this just to save a few
   silly bytes */

struct tr_realm {
    char *realm;
    unsigned leading_space:1;
    unsigned leading_slash:1;
    unsigned trailing_dot:1;
    struct tr_realm *next;
};

static void
free_realms(struct tr_realm *r)
{
    struct tr_realm *p;
    while(r){
	p = r;
	r = r->next;
	free(p->realm);
	free(p);
    }
}

static int
make_path(krb5_context context, struct tr_realm *r,
	  const char *from, const char *to)
{
    struct tr_realm *tmp;
    const char *p;

    if(strlen(from) < strlen(to)){
	const char *str;
	str = from;
	from = to;
	to = str;
    }

    if(strcmp(from + strlen(from) - strlen(to), to) == 0){
	p = from;
	while(1){
	    p = strchr(p, '.');
	    if(p == NULL) {
		krb5_clear_error_message (context);
		return KRB5KDC_ERR_POLICY;
	    }
	    p++;
	    if(strcmp(p, to) == 0)
		break;
	    tmp = calloc(1, sizeof(*tmp));
	    if(tmp == NULL)
		return krb5_enomem(context);
	    tmp->next = r->next;
	    r->next = tmp;
	    tmp->realm = strdup(p);
	    if(tmp->realm == NULL){
		r->next = tmp->next;
		free(tmp);
		return krb5_enomem(context);
	    }
	}
    }else if(strncmp(from, to, strlen(to)) == 0){
	p = from + strlen(from);
	while(1){
	    while(p >= from && *p != '/') p--;
	    if(p == from)
		return KRB5KDC_ERR_POLICY;

	    if(strncmp(to, from, p - from) == 0)
		break;
	    tmp = calloc(1, sizeof(*tmp));
	    if(tmp == NULL)
		return krb5_enomem(context);
	    tmp->next = r->next;
	    r->next = tmp;
	    tmp->realm = malloc(p - from + 1);
	    if(tmp->realm == NULL){
		r->next = tmp->next;
		free(tmp);
		return krb5_enomem(context);
	    }
	    memcpy(tmp->realm, from, p - from);
	    tmp->realm[p - from] = '\0';
	    p--;
	}
    } else {
	krb5_clear_error_message (context);
	return KRB5KDC_ERR_POLICY;
    }

    return 0;
}

static int
make_paths(krb5_context context,
	   struct tr_realm *realms, const char *client_realm,
	   const char *server_realm)
{
    struct tr_realm *r;
    int ret;
    const char *prev_realm = client_realm;
    const char *next_realm = NULL;
    for(r = realms; r; r = r->next){
	/* it *might* be that you can have more than one empty
	   component in a row, at least that's how I interpret the
	   "," exception in 1510 */
	if(r->realm[0] == '\0'){
	    while(r->next && r->next->realm[0] == '\0')
		r = r->next;
	    if(r->next)
		next_realm = r->next->realm;
	    else
		next_realm = server_realm;
	    ret = make_path(context, r, prev_realm, next_realm);
	    if(ret){
		free_realms(realms);
		return ret;
	    }
	}
	prev_realm = r->realm;
    }
    return 0;
}

static int
expand_realms(krb5_context context,
	      struct tr_realm *realms, const char *client_realm)
{
    struct tr_realm *r;
    const char *prev_realm = NULL;
    for(r = realms; r; r = r->next){
	if(r->trailing_dot){
	    char *tmp;
	    size_t len;

	    if(prev_realm == NULL)
		prev_realm = client_realm;

	    len = strlen(r->realm) + strlen(prev_realm) + 1;

	    tmp = realloc(r->realm, len);
	    if(tmp == NULL){
		free_realms(realms);
		return krb5_enomem(context);
	    }
	    r->realm = tmp;
	    strlcat(r->realm, prev_realm, len);
	}else if(r->leading_slash && !r->leading_space && prev_realm){
	    /* yet another exception: if you use x500-names, the
               leading realm doesn't have to be "quoted" with a space */
	    char *tmp;
	    size_t len = strlen(r->realm) + strlen(prev_realm) + 1;

	    tmp = malloc(len);
	    if(tmp == NULL){
		free_realms(realms);
		return krb5_enomem(context);
	    }
	    strlcpy(tmp, prev_realm, len);
	    strlcat(tmp, r->realm, len);
	    free(r->realm);
	    r->realm = tmp;
	}
	prev_realm = r->realm;
    }
    return 0;
}

static struct tr_realm *
make_realm(char *realm)
{
    struct tr_realm *r;
    char *p, *q;
    int quote = 0;
    r = calloc(1, sizeof(*r));
    if(r == NULL){
	free(realm);
	return NULL;
    }
    r->realm = realm;
    for(p = q = r->realm; *p; p++){
	if(p == r->realm && *p == ' '){
	    r->leading_space = 1;
	    continue;
	}
	if(q == r->realm && *p == '/')
	    r->leading_slash = 1;
	if(quote){
	    *q++ = *p;
	    quote = 0;
	    continue;
	}
	if(*p == '\\'){
	    quote = 1;
	    continue;
	}
	if(p[0] == '.' && p[1] == '\0')
	    r->trailing_dot = 1;
	*q++ = *p;
    }
    *q = '\0';
    return r;
}

static struct tr_realm*
append_realm(struct tr_realm *head, struct tr_realm *r)
{
    struct tr_realm *p;
    if(head == NULL){
	r->next = NULL;
	return r;
    }
    p = head;
    while(p->next) p = p->next;
    p->next = r;
    return head;
}

static int
decode_realms(krb5_context context,
	      const char *tr, int length, struct tr_realm **realms)
{
    struct tr_realm *r = NULL;

    char *tmp;
    int quote = 0;
    const char *start = tr;
    int i;

    for(i = 0; i < length; i++){
	if(quote){
	    quote = 0;
	    continue;
	}
	if(tr[i] == '\\'){
	    quote = 1;
	    continue;
	}
	if(tr[i] == ','){
	    tmp = malloc(tr + i - start + 1);
	    if(tmp == NULL)
		return krb5_enomem(context);
	    memcpy(tmp, start, tr + i - start);
	    tmp[tr + i - start] = '\0';
	    r = make_realm(tmp);
	    if(r == NULL){
		free_realms(*realms);
		return krb5_enomem(context);
	    }
	    *realms = append_realm(*realms, r);
	    start = tr + i + 1;
	}
    }
    tmp = malloc(tr + i - start + 1);
    if(tmp == NULL){
	free(*realms);
	return krb5_enomem(context);
    }
    memcpy(tmp, start, tr + i - start);
    tmp[tr + i - start] = '\0';
    r = make_realm(tmp);
    if(r == NULL){
	free_realms(*realms);
	return krb5_enomem(context);
    }
    *realms = append_realm(*realms, r);

    return 0;
}


KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_domain_x500_decode(krb5_context context,
			krb5_data tr, char ***realms, unsigned int *num_realms,
			const char *client_realm, const char *server_realm)
{
    struct tr_realm *r = NULL;
    struct tr_realm *p, **q;
    int ret;

    if(tr.length == 0) {
	*realms = NULL;
	*num_realms = 0;
	return 0;
    }

    /* split string in components */
    ret = decode_realms(context, tr.data, tr.length, &r);
    if(ret)
	return ret;

    /* apply prefix rule */
    ret = expand_realms(context, r, client_realm);
    if(ret)
	return ret;

    ret = make_paths(context, r, client_realm, server_realm);
    if(ret)
	return ret;

    /* remove empty components and count realms */
    *num_realms = 0;
    for(q = &r; *q; ){
	if((*q)->realm[0] == '\0'){
	    p = *q;
	    *q = (*q)->next;
	    free(p->realm);
	    free(p);
	}else{
	    q = &(*q)->next;
	    (*num_realms)++;
	}
    }
    if (*num_realms + 1 > UINT_MAX/sizeof(**realms))
	return ERANGE;

    {
	char **R;
	R = malloc((*num_realms + 1) * sizeof(*R));
	if (R == NULL)
	    return krb5_enomem(context);
	*realms = R;
	while(r){
	    *R++ = r->realm;
	    p = r->next;
	    free(r);
	    r = p;
	}
    }
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_domain_x500_encode(char **realms, unsigned int num_realms,
			krb5_data *encoding)
{
    char *s = NULL;
    int len = 0;
    unsigned int i;
    krb5_data_zero(encoding);
    if (num_realms == 0)
	return 0;
    for(i = 0; i < num_realms; i++){
	len += strlen(realms[i]);
	if(realms[i][0] == '/')
	    len++;
    }
    len += num_realms - 1;
    s = malloc(len + 1);
    if (s == NULL)
	return ENOMEM;
    *s = '\0';
    for(i = 0; i < num_realms; i++){
	if(i)
	    strlcat(s, ",", len + 1);
	if(realms[i][0] == '/')
	    strlcat(s, " ", len + 1);
	strlcat(s, realms[i], len + 1);
    }
    encoding->data = s;
    encoding->length = strlen(s);
    return 0;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_free_capath(krb5_context context, char **capath)
{
    char **s;

    for (s = capath; s && *s; ++s)
        free(*s);
    free(capath);
}

struct hier_iter {
    const char *local_realm;
    const char *server_realm;
    const char *lr;     /* Pointer into tail of local realm */
    const char *sr;     /* Pointer into tail of server realm */
    size_t llen;        /* Length of local_realm */
    size_t slen;        /* Length of server_realm */
    size_t len;         /* Length of common suffix */
    size_t num;         /* Path element count */
};

/*
 * Step up from local_realm to common suffix, or else down to server_realm.
 */
static const char *
hier_next(struct hier_iter *state)
{
    const char *lr = state->lr;
    const char *sr = state->sr;
    const char *lsuffix = state->local_realm + state->llen - state->len;
    const char *server_realm = state->server_realm;

    if (lr != NULL) {
        while (lr < lsuffix)
            if (*lr++ == '.')
                return state->lr = lr;
        state->lr = NULL;
    }
    if (sr != NULL) {
        while (--sr >= server_realm)
            if (sr == server_realm || sr[-1] == '.')
                return state->sr = sr;
        state->sr = NULL;
    }
    return NULL;
}

static void
hier_init(struct hier_iter *state, const char *local_realm, const char *server_realm)
{
    size_t llen;
    size_t slen;
    size_t len = 0;
    const char *lr;
    const char *sr;

    state->local_realm = local_realm;
    state->server_realm = server_realm;
    state->llen = llen = strlen(local_realm);
    state->slen = slen = strlen(server_realm);
    state->len = 0;
    state->num = 0;

    if (slen == 0 || llen == 0)
        return;

    /* Find first difference from the back */
    for (lr = local_realm + llen, sr = server_realm + slen;
         lr != local_realm && sr != server_realm;
         --lr, --sr) {
        if (lr[-1] != sr[-1])
            break;
        if (lr[-1] == '.')
            len = llen - (lr - local_realm);
    }

    /* Nothing in common? */
    if (*lr == '\0')
        return;

    /* Everything in common? */
    if (llen == slen && lr == local_realm)
        return;

    /* Is one realm is a suffix of the other? */
    if ((llen < slen && lr == local_realm && sr[-1] == '.') ||
        (llen > slen && sr == server_realm && lr[-1] == '.'))
        len = llen - (lr - local_realm);

    state->len = len;
    /* `lr` starts at local realm and walks up the tree to common suffix */
    state->lr = local_realm;
    /* `sr` starts at common suffix in server realm and walks down the tree */
    state->sr = server_realm + slen - len;

    /* Count elements and reset */
    while (hier_next(state) != NULL)
        ++state->num;
    state->lr = local_realm;
    state->sr = server_realm + slen - len;
}

/*
 * Find a referral path from client_realm to server_realm via local_realm.
 * Either via [capaths] or hierarchicaly.
 */
KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_find_capath(krb5_context context,
                  const char *client_realm,
                  const char *local_realm,
                  const char *server_realm,
                  krb5_boolean use_hierarchical,
                  char ***rpath,
                  size_t *npath)
{
    char **confpath;
    char **capath;
    struct hier_iter hier_state;
    char **rp;
    const char *r;

    *rpath = NULL;
    *npath = 0;

    confpath = krb5_config_get_strings(context, NULL, "capaths",
                                       client_realm, server_realm, NULL);
    if (confpath == NULL)
        confpath = krb5_config_get_strings(context, NULL, "capaths",
                                           local_realm, server_realm, NULL);
    /*
     * With a [capaths] setting from the client to the server we look for our
     * own realm in the list.  If our own realm is not present, we return the
     * full list.  Otherwise, we return our realm's successors, or possibly
     * NULL.  Ignoring a [capaths] settings risks loops plus would violate
     * explicit policy and the principle of least surpise.
     */
    if (confpath != NULL) {
        char **start = confpath;
        size_t i;
        size_t n;

	for (rp = start; *rp; rp++)
            if (strcmp(*rp, local_realm) == 0)
                start = rp+1;
        n = rp - start;

        if (n == 0) {
            krb5_config_free_strings(confpath);
            return 0;
        }

        capath = calloc(n + 1, sizeof(*capath));
        if (capath == NULL) {
            krb5_config_free_strings(confpath);
            return krb5_enomem(context);
        }

	for (i = 0, rp = start; *rp; rp++) {
            if ((capath[i++] = strdup(*rp)) == NULL) {
                _krb5_free_capath(context, capath);
                krb5_config_free_strings(confpath);
                return krb5_enomem(context);
            }
        }
        krb5_config_free_strings(confpath);
        capath[i] = NULL;
        *rpath = capath;
        *npath = n;
        return 0;
    }

    /* The use_hierarchical flag makes hierarchical path lookup unconditional */
    if (! use_hierarchical &&
        ! krb5_config_get_bool_default(context, NULL, TRUE, "libdefaults",
                                       "allow_hierarchical_capaths", NULL))
        return 0;

    /*
     * When validating transit paths, local_realm == client_realm.  Otherwise,
     * with hierarchical referrals, they may differ, and we may be building a
     * path forward from our own realm!
     */
    hier_init(&hier_state, local_realm, server_realm);
    if (hier_state.num == 0)
        return 0;

    rp = capath = calloc(hier_state.num + 1, sizeof(*capath));
    if (capath == NULL)
        return krb5_enomem(context);
    while ((r = hier_next(&hier_state)) != NULL) {
        if ((*rp++ = strdup(r)) == NULL) {
            _krb5_free_capath(context, capath);
            return krb5_enomem(context);
        }
    }

    *rp = NULL;
    *rpath = capath;
    *npath = hier_state.num;
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_check_transited(krb5_context context,
		     krb5_const_realm client_realm,
		     krb5_const_realm server_realm,
		     krb5_realm *realms,
		     unsigned int num_realms,
		     int *bad_realm)
{
    krb5_error_code ret = 0;
    char **capath = NULL;
    size_t num_capath = 0;
    size_t i = 0;
    size_t j = 0;

    /* In transit checks hierarchical capaths are optional */
    ret = _krb5_find_capath(context, client_realm, client_realm, server_realm,
                            FALSE, &capath, &num_capath);
    if (ret)
        return ret;

    for (i = 0; i < num_realms; i++) {
	for (j = 0; j < num_capath; ++j) {
	    if (strcmp(realms[i], capath[j]) == 0)
		break;
	}
	if (j == num_capath) {
            _krb5_free_capath(context, capath);
	    krb5_set_error_message (context, KRB5KRB_AP_ERR_ILL_CR_TKT,
				    N_("no transit allowed "
				       "through realm %s from %s to %s", ""),
				       realms[i], client_realm, server_realm);
	    if (bad_realm)
		*bad_realm = i;
	    return KRB5KRB_AP_ERR_ILL_CR_TKT;
	}
    }

    _krb5_free_capath(context, capath);
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_check_transited_realms(krb5_context context,
			    const char *const *realms,
			    unsigned int num_realms,
			    int *bad_realm)
{
    size_t i;
    int ret = 0;
    char **bad_realms = krb5_config_get_strings(context, NULL,
						"libdefaults",
						"transited_realms_reject",
						NULL);
    if(bad_realms == NULL)
	return 0;

    for(i = 0; i < num_realms; i++) {
	char **p;
	for(p = bad_realms; *p; p++)
	    if(strcmp(*p, realms[i]) == 0) {
		ret = KRB5KRB_AP_ERR_ILL_CR_TKT;
		krb5_set_error_message (context, ret,
					N_("no transit allowed "
					   "through realm %s", ""),
					*p);
		if(bad_realm)
		    *bad_realm = i;
		break;
	    }
    }
    krb5_config_free_strings(bad_realms);
    return ret;
}

#if 0
int
main(int argc, char **argv)
{
    krb5_data x;
    char **r;
    int num, i;
    x.data = argv[1];
    x.length = strlen(x.data);
    if(domain_expand(x, &r, &num, argv[2], argv[3]))
	exit(1);
    for(i = 0; i < num; i++)
	printf("%s\n", r[i]);
    return 0;
}
#endif
