/*	$NetBSD: kerberos5.c,v 1.7 2017/01/28 21:31:44 christos Exp $	*/

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

#include "kdc_locl.h"

#define MAX_TIME ((time_t)((1U << 31) - 1))

void
_kdc_fix_time(time_t **t)
{
    if(*t == NULL){
	ALLOC(*t);
	**t = MAX_TIME;
    }
    if(**t == 0) **t = MAX_TIME; /* fix for old clients */
}

static int
realloc_method_data(METHOD_DATA *md)
{
    PA_DATA *pa;
    pa = realloc(md->val, (md->len + 1) * sizeof(*md->val));
    if(pa == NULL)
	return ENOMEM;
    md->val = pa;
    md->len++;
    return 0;
}

static void
set_salt_padata(METHOD_DATA *md, Salt *salt)
{
    if (salt) {
       realloc_method_data(md);
       md->val[md->len - 1].padata_type = salt->type;
       der_copy_octet_string(&salt->salt,
                             &md->val[md->len - 1].padata_value);
    }
}

const PA_DATA*
_kdc_find_padata(const KDC_REQ *req, int *start, int type)
{
    if (req->padata == NULL)
	return NULL;

    while((size_t)*start < req->padata->len){
	(*start)++;
	if(req->padata->val[*start - 1].padata_type == (unsigned)type)
	    return &req->padata->val[*start - 1];
    }
    return NULL;
}

/*
 * This is a hack to allow predefined weak services, like afs to
 * still use weak types
 */

krb5_boolean
_kdc_is_weak_exception(krb5_principal principal, krb5_enctype etype)
{
    if (principal->name.name_string.len > 0 &&
	strcmp(principal->name.name_string.val[0], "afs") == 0 &&
	(etype == (krb5_enctype)ETYPE_DES_CBC_CRC
	 || etype == (krb5_enctype)ETYPE_DES_CBC_MD4
	 || etype == (krb5_enctype)ETYPE_DES_CBC_MD5))
	return TRUE;
    return FALSE;
}


/*
 * Detect if `key' is the using the the precomputed `default_salt'.
 */

static krb5_boolean
is_default_salt_p(const krb5_salt *default_salt, const Key *key)
{
    if (key->salt == NULL)
	return TRUE;
    if (default_salt->salttype != key->salt->type)
	return FALSE;
    if (krb5_data_cmp(&default_salt->saltvalue, &key->salt->salt))
	return FALSE;
    return TRUE;
}

/*
 * return the first appropriate key of `princ' in `ret_key'.  Look for
 * all the etypes in (`etypes', `len'), stopping as soon as we find
 * one, but preferring one that has default salt.
 */

krb5_error_code
_kdc_find_etype(krb5_context context, krb5_boolean use_strongest_session_key,
		krb5_boolean is_preauth, hdb_entry_ex *princ,
		krb5_enctype *etypes, unsigned len,
		krb5_enctype *ret_enctype, Key **ret_key)
{
    krb5_error_code ret;
    krb5_salt def_salt;
    krb5_enctype enctype = (krb5_enctype)ETYPE_NULL;
    const krb5_enctype *p;
    Key *key = NULL;
    int i, k;

    /* We'll want to avoid keys with v4 salted keys in the pre-auth case... */
    ret = krb5_get_pw_salt(context, princ->entry.principal, &def_salt);
    if (ret)
	return ret;

    ret = KRB5KDC_ERR_ETYPE_NOSUPP;

    if (use_strongest_session_key) {

	/*
	 * Pick the strongest key that the KDC, target service, and
	 * client all support, using the local cryptosystem enctype
	 * list in strongest-to-weakest order to drive the search.
	 *
	 * This is not what RFC4120 says to do, but it encourages
	 * adoption of stronger enctypes.  This doesn't play well with
	 * clients that have multiple Kerberos client implementations
	 * available with different supported enctype lists.
	 */

	/* drive the search with local supported enctypes list */
	p = krb5_kerberos_enctypes(context);
	for (i = 0;
	    p[i] != (krb5_enctype)ETYPE_NULL && enctype == (krb5_enctype)ETYPE_NULL;
	    i++) {
	    if (krb5_enctype_valid(context, p[i]) != 0 &&
                !_kdc_is_weak_exception(princ->entry.principal, p[i]))
		continue;

	    /* check that the client supports it too */
	    for (k = 0; k < len && enctype == (krb5_enctype)ETYPE_NULL; k++) {

		if (p[i] != etypes[k])
		    continue;

                /* check target princ support */
		key = NULL;
		while (hdb_next_enctype2key(context, &princ->entry, NULL,
					     p[i], &key) == 0) {
		    if (key->key.keyvalue.length == 0) {
			ret = KRB5KDC_ERR_NULL_KEY;
			continue;
		    }
		    enctype = p[i];
		    ret = 0;
		    if (is_preauth && ret_key != NULL &&
			!is_default_salt_p(&def_salt, key))
			continue;
		}
	    }
	}
    } else {
	/*
	 * Pick the first key from the client's enctype list that is
	 * supported by the cryptosystem and by the given principal.
	 *
	 * RFC4120 says we SHOULD pick the first _strong_ key from the
	 * client's list... not the first key...  If the admin disallows
	 * weak enctypes in krb5.conf and selects this key selection
	 * algorithm, then we get exactly what RFC4120 says.
	 */
	for(i = 0; ret != 0 && i < len; i++) {

	    if (krb5_enctype_valid(context, etypes[i]) != 0 &&
		!_kdc_is_weak_exception(princ->entry.principal, etypes[i]))
		continue;

	    key = NULL;
	    while (ret != 0 &&
                   hdb_next_enctype2key(context, &princ->entry, NULL,
					etypes[i], &key) == 0) {
		if (key->key.keyvalue.length == 0) {
		    ret = KRB5KDC_ERR_NULL_KEY;
		    continue;
		}
                enctype = etypes[i];
		ret = 0;
		if (is_preauth && ret_key != NULL &&
		    !is_default_salt_p(&def_salt, key))
		    continue;
	    }
	}
    }

    if (enctype == (krb5_enctype)ETYPE_NULL) {
        /*
         * if the service principal is one for which there is a known 1DES
         * exception and no other enctype matches both the client request and
         * the service key list, provide a DES-CBC-CRC key.
         */
	if (ret_key == NULL &&
	    _kdc_is_weak_exception(princ->entry.principal, ETYPE_DES_CBC_CRC)) {
            ret = 0;
            enctype = ETYPE_DES_CBC_CRC;
        } else {
            ret = KRB5KDC_ERR_ETYPE_NOSUPP;
        }
    }

    if (ret == 0) {
	if (ret_enctype != NULL)
	    *ret_enctype = enctype;
	if (ret_key != NULL)
	    *ret_key = key;
    }

    krb5_free_salt (context, def_salt);
    return ret;
}

krb5_error_code
_kdc_make_anonymous_principalname (PrincipalName *pn)
{
    pn->name_type = KRB5_NT_PRINCIPAL;
    pn->name_string.len = 1;
    pn->name_string.val = malloc(sizeof(*pn->name_string.val));
    if (pn->name_string.val == NULL)
	return ENOMEM;
    pn->name_string.val[0] = strdup("anonymous");
    if (pn->name_string.val[0] == NULL) {
	free(pn->name_string.val);
	pn->name_string.val = NULL;
	return ENOMEM;
    }
    return 0;
}

static void
_kdc_r_log(kdc_request_t r, int level, const char *fmt, ...)
{
    va_list ap;
    char *s;
    va_start(ap, fmt);
    s = kdc_log_msg_va(r->context, r->config, level, fmt, ap);
    if(s) free(s);
    va_end(ap);
}

static void
_kdc_set_e_text(kdc_request_t r, const char *e_text)
{
    r->e_text = e_text;
    kdc_log(r->context, r->config, 0, "%s", e_text);
}

void
_kdc_log_timestamp(krb5_context context,
		   krb5_kdc_configuration *config,
		   const char *type,
		   KerberosTime authtime, KerberosTime *starttime,
		   KerberosTime endtime, KerberosTime *renew_till)
{
    char authtime_str[100], starttime_str[100],
	endtime_str[100], renewtime_str[100];

    krb5_format_time(context, authtime,
		     authtime_str, sizeof(authtime_str), TRUE);
    if (starttime)
	krb5_format_time(context, *starttime,
			 starttime_str, sizeof(starttime_str), TRUE);
    else
	strlcpy(starttime_str, "unset", sizeof(starttime_str));
    krb5_format_time(context, endtime,
		     endtime_str, sizeof(endtime_str), TRUE);
    if (renew_till)
	krb5_format_time(context, *renew_till,
			 renewtime_str, sizeof(renewtime_str), TRUE);
    else
	strlcpy(renewtime_str, "unset", sizeof(renewtime_str));

    kdc_log(context, config, 5,
	    "%s authtime: %s starttime: %s endtime: %s renew till: %s",
	    type, authtime_str, starttime_str, endtime_str, renewtime_str);
}

/*
 *
 */

#ifdef PKINIT

static krb5_error_code
pa_pkinit_validate(kdc_request_t r, const PA_DATA *pa)
{
    pk_client_params *pkp = NULL;
    char *client_cert = NULL;
    krb5_error_code ret;

    ret = _kdc_pk_rd_padata(r->context, r->config, &r->req, pa, r->client, &pkp);
    if (ret || pkp == NULL) {
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
	_kdc_r_log(r, 5, "Failed to decode PKINIT PA-DATA -- %s",
		   r->client_name);
	goto out;
    }
    
    ret = _kdc_pk_check_client(r->context,
			       r->config,
			       r->clientdb, 
			       r->client,
			       pkp,
			       &client_cert);
    if (ret) {
	_kdc_set_e_text(r, "PKINIT certificate not allowed to "
			"impersonate principal");
	goto out;
    }

    _kdc_r_log(r, 0, "PKINIT pre-authentication succeeded -- %s using %s",
	       r->client_name, client_cert);
    free(client_cert);

    ret = _kdc_pk_mk_pa_reply(r->context, r->config, pkp, r->client,
			      r->sessionetype, &r->req, &r->request,
			      &r->reply_key, &r->session_key, &r->outpadata);
    if (ret) {
	_kdc_set_e_text(r, "Failed to build PK-INIT reply");
	goto out;
    }
#if 0
    ret = _kdc_add_inital_verified_cas(r->context, r->config,
				       pkp, &r->et);
#endif
 out:
    if (pkp)
	_kdc_pk_free_client_param(r->context, pkp);

    return ret;
}

#endif /* PKINIT */

/*
 *
 */

static krb5_error_code
make_pa_enc_challange(krb5_context context, METHOD_DATA *md,
		      krb5_crypto crypto)
{
    PA_ENC_TS_ENC p;
    unsigned char *buf;
    size_t buf_size;
    size_t len;
    EncryptedData encdata;
    krb5_error_code ret;
    int32_t usec;
    int usec2;

    krb5_us_timeofday (context, &p.patimestamp, &usec);
    usec2         = usec;
    p.pausec      = &usec2;

    ASN1_MALLOC_ENCODE(PA_ENC_TS_ENC, buf, buf_size, &p, &len, ret);
    if (ret)
	return ret;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_encrypt_EncryptedData(context,
				     crypto,
				     KRB5_KU_ENC_CHALLENGE_KDC,
				     buf,
				     len,
				     0,
				     &encdata);
    free(buf);
    if (ret)
	return ret;

    ASN1_MALLOC_ENCODE(EncryptedData, buf, buf_size, &encdata, &len, ret);
    free_EncryptedData(&encdata);
    if (ret)
	return ret;
    if(buf_size != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");

    ret = krb5_padata_add(context, md, KRB5_PADATA_ENCRYPTED_CHALLENGE, buf, len);
    if (ret)
	free(buf);
    return ret;
}

static krb5_error_code
pa_enc_chal_validate(kdc_request_t r, const PA_DATA *pa)
{
    krb5_data pepper1, pepper2, ts_data;
    KDC_REQ_BODY *b = &r->req.req_body;
    int invalidPassword = 0;
    EncryptedData enc_data;
    krb5_enctype aenctype;
    krb5_error_code ret;
    struct Key *k;
    size_t size;
    int i;

    heim_assert(r->armor_crypto != NULL, "ENC-CHAL called for non FAST");
    
    if (_kdc_is_anon_request(b)) {
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
	kdc_log(r->context, r->config, 0, "ENC-CHALL doesn't support anon");
	return ret;
    }

    ret = decode_EncryptedData(pa->padata_value.data,
			       pa->padata_value.length,
			       &enc_data,
			       &size);
    if (ret) {
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
	_kdc_r_log(r, 5, "Failed to decode PA-DATA -- %s",
		   r->client_name);
	return ret;
    }

    pepper1.data = "clientchallengearmor";
    pepper1.length = strlen(pepper1.data);
    pepper2.data = "challengelongterm";
    pepper2.length = strlen(pepper2.data);

    krb5_crypto_getenctype(r->context, r->armor_crypto, &aenctype);

    for (i = 0; i < r->client->entry.keys.len; i++) {
	krb5_crypto challangecrypto, longtermcrypto;
	krb5_keyblock challangekey;
	PA_ENC_TS_ENC p;

	k = &r->client->entry.keys.val[i];
	
	ret = krb5_crypto_init(r->context, &k->key, 0, &longtermcrypto);
	if (ret)
	    continue;			
	
	ret = krb5_crypto_fx_cf2(r->context, r->armor_crypto, longtermcrypto,
				 &pepper1, &pepper2, aenctype,
				 &challangekey);
	krb5_crypto_destroy(r->context, longtermcrypto);
	if (ret)
	    continue;
	
	ret = krb5_crypto_init(r->context, &challangekey, 0,
			       &challangecrypto);
	if (ret)
	    continue;
	
	ret = krb5_decrypt_EncryptedData(r->context, challangecrypto,
					 KRB5_KU_ENC_CHALLENGE_CLIENT,
					 &enc_data,
					 &ts_data);
	if (ret) {
	    const char *msg = krb5_get_error_message(r->context, ret);
	    krb5_error_code ret2;
	    char *str = NULL;

	    invalidPassword = 1;

	    ret2 = krb5_enctype_to_string(r->context, k->key.keytype, &str);
	    if (ret2)
		str = NULL;
	    _kdc_r_log(r, 5, "Failed to decrypt ENC-CHAL -- %s "
		       "(enctype %s) error %s",
		       r->client_name, str ? str : "unknown enctype", msg);
	    krb5_free_error_message(r->context, msg);
	    free(str);

	    continue;
	}
	
	ret = decode_PA_ENC_TS_ENC(ts_data.data,
				   ts_data.length,
				   &p,
				   &size);
	krb5_data_free(&ts_data);
	if(ret){
	    krb5_crypto_destroy(r->context, challangecrypto);
	    ret = KRB5KDC_ERR_PREAUTH_FAILED;
	    _kdc_r_log(r, 5, "Failed to decode PA-ENC-TS_ENC -- %s",
		       r->client_name);
	    continue;
	}

	if (labs(kdc_time - p.patimestamp) > r->context->max_skew) {
	    char client_time[100];

	    krb5_crypto_destroy(r->context, challangecrypto);

	    krb5_format_time(r->context, p.patimestamp,
			     client_time, sizeof(client_time), TRUE);

	    ret = KRB5KRB_AP_ERR_SKEW;
	    _kdc_r_log(r, 0, "Too large time skew, "
		       "client time %s is out by %u > %u seconds -- %s",
		       client_time,
		       (unsigned)labs(kdc_time - p.patimestamp),
		       r->context->max_skew,
		       r->client_name);

	    free_PA_ENC_TS_ENC(&p);
	    goto out;
	}

	free_PA_ENC_TS_ENC(&p);

	ret = make_pa_enc_challange(r->context, &r->outpadata,
				    challangecrypto);
	krb5_crypto_destroy(r->context, challangecrypto);
	if (ret)
	    goto out;
					    
	set_salt_padata(&r->outpadata, k->salt);
	krb5_free_keyblock_contents(r->context,  &r->reply_key);
	ret = krb5_copy_keyblock_contents(r->context, &k->key, &r->reply_key);
	if (ret)
	    goto out;

	/*
	 * Success
	 */
	if (r->clientdb->hdb_auth_status)
	    r->clientdb->hdb_auth_status(r->context, r->clientdb, r->client,
					 HDB_AUTH_SUCCESS);
	goto out;
    }

    if (invalidPassword && r->clientdb->hdb_auth_status) {
	r->clientdb->hdb_auth_status(r->context, r->clientdb, r->client,
				     HDB_AUTH_WRONG_PASSWORD);
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
    }
 out:
    free_EncryptedData(&enc_data);

    return ret;
}

static krb5_error_code
pa_enc_ts_validate(kdc_request_t r, const PA_DATA *pa)
{
    EncryptedData enc_data;
    krb5_error_code ret;
    krb5_crypto crypto;
    krb5_data ts_data;
    PA_ENC_TS_ENC p;
    size_t len;
    Key *pa_key;
    char *str;
	
    if (_kdc_is_anon_request(&r->req.req_body)) {
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
	_kdc_set_e_text(r, "ENC-TS doesn't support anon");
	goto out;
    }

    ret = decode_EncryptedData(pa->padata_value.data,
			       pa->padata_value.length,
			       &enc_data,
			       &len);
    if (ret) {
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
	_kdc_r_log(r, 5, "Failed to decode PA-DATA -- %s",
		   r->client_name);
	goto out;
    }
	
    ret = hdb_enctype2key(r->context, &r->client->entry, NULL,
			  enc_data.etype, &pa_key);
    if(ret){
	char *estr;
	_kdc_set_e_text(r, "No key matching entype");
	ret = KRB5KDC_ERR_ETYPE_NOSUPP;
	if(krb5_enctype_to_string(r->context, enc_data.etype, &estr))
	    estr = NULL;
	if(estr == NULL)
	    _kdc_r_log(r, 5,
		       "No client key matching pa-data (%d) -- %s",
		       enc_data.etype, r->client_name);
	else
	    _kdc_r_log(r, 5,
		       "No client key matching pa-data (%s) -- %s",
		       estr, r->client_name);
	free(estr);
	free_EncryptedData(&enc_data);
	goto out;
    }

 try_next_key:
    ret = krb5_crypto_init(r->context, &pa_key->key, 0, &crypto);
    if (ret) {
	const char *msg = krb5_get_error_message(r->context, ret);
	_kdc_r_log(r, 0, "krb5_crypto_init failed: %s", msg);
	krb5_free_error_message(r->context, msg);
	free_EncryptedData(&enc_data);
	goto out;
    }

    ret = krb5_decrypt_EncryptedData (r->context,
				      crypto,
				      KRB5_KU_PA_ENC_TIMESTAMP,
				      &enc_data,
				      &ts_data);
    krb5_crypto_destroy(r->context, crypto);
    /*
     * Since the user might have several keys with the same
     * enctype but with diffrent salting, we need to try all
     * the keys with the same enctype.
     */
    if(ret){
	krb5_error_code ret2;
	const char *msg = krb5_get_error_message(r->context, ret);

	ret2 = krb5_enctype_to_string(r->context,
				      pa_key->key.keytype, &str);
	if (ret2)
	    str = NULL;
	_kdc_r_log(r, 5, "Failed to decrypt PA-DATA -- %s "
		   "(enctype %s) error %s",
		   r->client_name, str ? str : "unknown enctype", msg);
	krb5_free_error_message(r->context, msg);
	free(str);

	if(hdb_next_enctype2key(r->context, &r->client->entry, NULL,
				enc_data.etype, &pa_key) == 0)
	    goto try_next_key;

	free_EncryptedData(&enc_data);

	if (r->clientdb->hdb_auth_status)
	    r->clientdb->hdb_auth_status(r->context, r->clientdb, r->client,
					 HDB_AUTH_WRONG_PASSWORD);

	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }
    free_EncryptedData(&enc_data);
    ret = decode_PA_ENC_TS_ENC(ts_data.data,
			       ts_data.length,
			       &p,
			       &len);
    krb5_data_free(&ts_data);
    if(ret){
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	_kdc_r_log(r, 5, "Failed to decode PA-ENC-TS_ENC -- %s",
		   r->client_name);
	goto out;
    }
    if (labs(kdc_time - p.patimestamp) > r->context->max_skew) {
	char client_time[100];
		
	krb5_format_time(r->context, p.patimestamp,
			 client_time, sizeof(client_time), TRUE);

	ret = KRB5KRB_AP_ERR_SKEW;
	_kdc_r_log(r, 0, "Too large time skew, "
		   "client time %s is out by %u > %u seconds -- %s",
		   client_time,
		   (unsigned)labs(kdc_time - p.patimestamp),
		   r->context->max_skew,
		   r->client_name);

	/*
	 * The following is needed to make windows clients to
	 * retry using the timestamp in the error message, if
	 * there is a e_text, they become unhappy.
	 */
	r->e_text = NULL;
	free_PA_ENC_TS_ENC(&p);
	goto out;
    }
    free_PA_ENC_TS_ENC(&p);

    set_salt_padata(&r->outpadata, pa_key->salt);

    ret = krb5_copy_keyblock_contents(r->context, &pa_key->key, &r->reply_key);
    if (ret)
	return ret;

    ret = krb5_enctype_to_string(r->context, pa_key->key.keytype, &str);
    if (ret)
	str = NULL;
    _kdc_r_log(r, 2, "ENC-TS Pre-authentication succeeded -- %s using %s",
	       r->client_name, str ? str : "unknown enctype");
    free(str);

    ret = 0;

 out:

    return ret;
}

struct kdc_patypes {
    int type;
    char *name;
    unsigned int flags;
#define PA_ANNOUNCE	1
#define PA_REQ_FAST	2 /* only use inside fast */
    krb5_error_code (*validate)(kdc_request_t, const PA_DATA *pa);
};

static const struct kdc_patypes pat[] = {
#ifdef PKINIT
    {
	KRB5_PADATA_PK_AS_REQ, "PK-INIT(ietf)", PA_ANNOUNCE,
	pa_pkinit_validate
    },
    {
	KRB5_PADATA_PK_AS_REQ_WIN, "PK-INIT(win2k)", PA_ANNOUNCE,
	pa_pkinit_validate
    },
    {
	KRB5_PADATA_PKINIT_KX, "Anonymous PK-INIT", PA_ANNOUNCE,
	NULL
    },
#else
    { KRB5_PADATA_PK_AS_REQ, "PK-INIT(ietf)", 0, NULL },
    { KRB5_PADATA_PK_AS_REQ_WIN, "PK-INIT(win2k)", 0, NULL },
    { KRB5_PADATA_PKINIT_KX, "Anonymous PK-INIT", 0, NULL },
#endif
    { KRB5_PADATA_PA_PK_OCSP_RESPONSE , "OCSP", 0, NULL },
    { 
	KRB5_PADATA_ENC_TIMESTAMP , "ENC-TS",
	PA_ANNOUNCE,
	pa_enc_ts_validate
    },
    {
	KRB5_PADATA_ENCRYPTED_CHALLENGE , "ENC-CHAL",
	PA_ANNOUNCE | PA_REQ_FAST,
	pa_enc_chal_validate
    },
    { KRB5_PADATA_REQ_ENC_PA_REP , "REQ-ENC-PA-REP", 0, NULL },
    { KRB5_PADATA_FX_FAST, "FX-FAST", PA_ANNOUNCE, NULL },
    { KRB5_PADATA_FX_ERROR, "FX-ERROR", 0, NULL },
    { KRB5_PADATA_FX_COOKIE, "FX-COOKIE", 0, NULL }
};

static void
log_patypes(krb5_context context,
	    krb5_kdc_configuration *config,
	    METHOD_DATA *padata)
{
    struct rk_strpool *p = NULL;
    char *str;
    size_t n, m;
	
    for (n = 0; n < padata->len; n++) {
	for (m = 0; m < sizeof(pat) / sizeof(pat[0]); m++) {
	    if (padata->val[n].padata_type == pat[m].type) {
		p = rk_strpoolprintf(p, "%s", pat[m].name);
		break;
	    }
	}
	if (m == sizeof(pat) / sizeof(pat[0]))
	    p = rk_strpoolprintf(p, "%d", padata->val[n].padata_type);
	if (p && n + 1 < padata->len)
	    p = rk_strpoolprintf(p, ", ");
	if (p == NULL) {
	    kdc_log(context, config, 0, "out of memory");
	    return;
	}
    }
    if (p == NULL)
	p = rk_strpoolprintf(p, "none");

    str = rk_strpoolcollect(p);
    kdc_log(context, config, 0, "Client sent patypes: %s", str);
    free(str);
}

/*
 *
 */

krb5_error_code
_kdc_encode_reply(krb5_context context,
		  krb5_kdc_configuration *config,
		  krb5_crypto armor_crypto, uint32_t nonce,
		  KDC_REP *rep, EncTicketPart *et, EncKDCRepPart *ek,
		  krb5_enctype etype,
		  int skvno, const EncryptionKey *skey,
		  int ckvno, const EncryptionKey *reply_key,
		  int rk_is_subkey,
		  const char **e_text,
		  krb5_data *reply)
{
    unsigned char *buf;
    size_t buf_size;
    size_t len = 0;
    krb5_error_code ret;
    krb5_crypto crypto;

    ASN1_MALLOC_ENCODE(EncTicketPart, buf, buf_size, et, &len, ret);
    if(ret) {
	const char *msg = krb5_get_error_message(context, ret);
	kdc_log(context, config, 0, "Failed to encode ticket: %s", msg);
	krb5_free_error_message(context, msg);
	return ret;
    }
    if(buf_size != len)
	krb5_abortx(context, "Internal error in ASN.1 encoder");

    ret = krb5_crypto_init(context, skey, etype, &crypto);
    if (ret) {
        const char *msg = krb5_get_error_message(context, ret);
	kdc_log(context, config, 0, "krb5_crypto_init failed: %s", msg);
	krb5_free_error_message(context, msg);
	free(buf);
	return ret;
    }

    ret = krb5_encrypt_EncryptedData(context,
				     crypto,
				     KRB5_KU_TICKET,
				     buf,
				     len,
				     skvno,
				     &rep->ticket.enc_part);
    free(buf);
    krb5_crypto_destroy(context, crypto);
    if(ret) {
	const char *msg = krb5_get_error_message(context, ret);
	kdc_log(context, config, 0, "Failed to encrypt data: %s", msg);
	krb5_free_error_message(context, msg);
	return ret;
    }

    if (armor_crypto) {
	krb5_data data;
	krb5_keyblock *strengthen_key = NULL;
	KrbFastFinished finished;

	kdc_log(context, config, 0, "FAST armor protection");

	memset(&finished, 0, sizeof(finished));
	krb5_data_zero(&data);

	finished.timestamp = kdc_time;
	finished.usec = 0;
	finished.crealm = et->crealm;
	finished.cname = et->cname;

	ASN1_MALLOC_ENCODE(Ticket, data.data, data.length,
			   &rep->ticket, &len, ret);
	if (ret)
	    return ret;
	if (data.length != len)
	    krb5_abortx(context, "internal asn.1 error");

	ret = krb5_create_checksum(context, armor_crypto,
				   KRB5_KU_FAST_FINISHED, 0,
				   data.data, data.length,
				   &finished.ticket_checksum);
	krb5_data_free(&data);
	if (ret)
	    return ret;

	ret = _kdc_fast_mk_response(context, armor_crypto,
				    rep->padata, strengthen_key, &finished,
				    nonce, &data);
	free_Checksum(&finished.ticket_checksum);
	if (ret)
	    return ret;

	if (rep->padata) {
	    free_METHOD_DATA(rep->padata);
	} else {
	    rep->padata = calloc(1, sizeof(*(rep->padata)));
	    if (rep->padata == NULL) {
		krb5_data_free(&data);
		return ENOMEM;
	    }
	}

	ret = krb5_padata_add(context, rep->padata,
			      KRB5_PADATA_FX_FAST,
			      data.data, data.length);
	if (ret)
	    return ret;

	/*
	 * Hide client name of privacy reasons
	 */
	if (1 /* r->fast_options.hide_client_names */) {
	    rep->crealm[0] = '\0';
	    free_PrincipalName(&rep->cname);
	    rep->cname.name_type = 0;
	}
    }

    if(rep->msg_type == krb_as_rep && !config->encode_as_rep_as_tgs_rep)
	ASN1_MALLOC_ENCODE(EncASRepPart, buf, buf_size, ek, &len, ret);
    else
	ASN1_MALLOC_ENCODE(EncTGSRepPart, buf, buf_size, ek, &len, ret);
    if(ret) {
	const char *msg = krb5_get_error_message(context, ret);
	kdc_log(context, config, 0, "Failed to encode KDC-REP: %s", msg);
	krb5_free_error_message(context, msg);
	return ret;
    }
    if(buf_size != len) {
	free(buf);
	kdc_log(context, config, 0, "Internal error in ASN.1 encoder");
	*e_text = "KDC internal error";
	return KRB5KRB_ERR_GENERIC;
    }
    ret = krb5_crypto_init(context, reply_key, 0, &crypto);
    if (ret) {
	const char *msg = krb5_get_error_message(context, ret);
	free(buf);
	kdc_log(context, config, 0, "krb5_crypto_init failed: %s", msg);
	krb5_free_error_message(context, msg);
	return ret;
    }
    if(rep->msg_type == krb_as_rep) {
	krb5_encrypt_EncryptedData(context,
				   crypto,
				   KRB5_KU_AS_REP_ENC_PART,
				   buf,
				   len,
				   ckvno,
				   &rep->enc_part);
	free(buf);
	ASN1_MALLOC_ENCODE(AS_REP, buf, buf_size, rep, &len, ret);
    } else {
	krb5_encrypt_EncryptedData(context,
				   crypto,
				   rk_is_subkey ? KRB5_KU_TGS_REP_ENC_PART_SUB_KEY : KRB5_KU_TGS_REP_ENC_PART_SESSION,
				   buf,
				   len,
				   ckvno,
				   &rep->enc_part);
	free(buf);
	ASN1_MALLOC_ENCODE(TGS_REP, buf, buf_size, rep, &len, ret);
    }
    krb5_crypto_destroy(context, crypto);
    if(ret) {
	const char *msg = krb5_get_error_message(context, ret);
	kdc_log(context, config, 0, "Failed to encode KDC-REP: %s", msg);
	krb5_free_error_message(context, msg);
	return ret;
    }
    if(buf_size != len) {
	free(buf);
	kdc_log(context, config, 0, "Internal error in ASN.1 encoder");
	*e_text = "KDC internal error";
	return KRB5KRB_ERR_GENERIC;
    }
    reply->data = buf;
    reply->length = buf_size;
    return 0;
}

/*
 * Return 1 if the client have only older enctypes, this is for
 * determining if the server should send ETYPE_INFO2 or not.
 */

static int
older_enctype(krb5_enctype enctype)
{
    switch (enctype) {
    case ETYPE_DES_CBC_CRC:
    case ETYPE_DES_CBC_MD4:
    case ETYPE_DES_CBC_MD5:
    case ETYPE_DES3_CBC_SHA1:
    case ETYPE_ARCFOUR_HMAC_MD5:
    case ETYPE_ARCFOUR_HMAC_MD5_56:
    /*
     * The following three is "old" windows enctypes and is needed for
     * windows 2000 hosts.
     */
    case ETYPE_ARCFOUR_MD4:
    case ETYPE_ARCFOUR_HMAC_OLD:
    case ETYPE_ARCFOUR_HMAC_OLD_EXP:
	return 1;
    default:
	return 0;
    }
}

/*
 *
 */

static krb5_error_code
make_etype_info_entry(krb5_context context, ETYPE_INFO_ENTRY *ent, Key *key)
{
    ent->etype = key->key.keytype;
    if(key->salt){
#if 0
	ALLOC(ent->salttype);

	if(key->salt->type == hdb_pw_salt)
	    *ent->salttype = 0; /* or 1? or NULL? */
	else if(key->salt->type == hdb_afs3_salt)
	    *ent->salttype = 2;
	else {
	    kdc_log(context, config, 0, "unknown salt-type: %d",
		    key->salt->type);
	    return KRB5KRB_ERR_GENERIC;
	}
	/* according to `the specs', we can't send a salt if
	   we have AFS3 salted key, but that requires that you
	   *know* what cell you are using (e.g by assuming
	   that the cell is the same as the realm in lower
	   case) */
#elif 0
	ALLOC(ent->salttype);
	*ent->salttype = key->salt->type;
#else
	/*
	 * We shouldn't sent salttype since it is incompatible with the
	 * specification and it breaks windows clients.  The afs
	 * salting problem is solved by using KRB5-PADATA-AFS3-SALT
	 * implemented in Heimdal 0.7 and later.
	 */
	ent->salttype = NULL;
#endif
	krb5_copy_data(context, &key->salt->salt,
		       &ent->salt);
    } else {
	/* we return no salt type at all, as that should indicate
	 * the default salt type and make everybody happy.  some
	 * systems (like w2k) dislike being told the salt type
	 * here. */

	ent->salttype = NULL;
	ent->salt = NULL;
    }
    return 0;
}

static krb5_error_code
get_pa_etype_info(krb5_context context,
		  krb5_kdc_configuration *config,
		  METHOD_DATA *md, Key *ckey)
{
    krb5_error_code ret = 0;
    ETYPE_INFO pa;
    unsigned char *buf;
    size_t len;


    pa.len = 1;
    pa.val = calloc(1, sizeof(pa.val[0]));
    if(pa.val == NULL)
	return ENOMEM;

    ret = make_etype_info_entry(context, &pa.val[0], ckey);
    if (ret) {
	free_ETYPE_INFO(&pa);
	return ret;
    }

    ASN1_MALLOC_ENCODE(ETYPE_INFO, buf, len, &pa, &len, ret);
    free_ETYPE_INFO(&pa);
    if(ret)
	return ret;
    ret = realloc_method_data(md);
    if(ret) {
	free(buf);
	return ret;
    }
    md->val[md->len - 1].padata_type = KRB5_PADATA_ETYPE_INFO;
    md->val[md->len - 1].padata_value.length = len;
    md->val[md->len - 1].padata_value.data = buf;
    return 0;
}

/*
 *
 */

extern int _krb5_AES_SHA1_string_to_default_iterator;
extern int _krb5_AES_SHA2_string_to_default_iterator;

static krb5_error_code
make_s2kparams(int value, size_t len, krb5_data **ps2kparams)
{
    krb5_data *s2kparams;
    krb5_error_code ret;

    ALLOC(s2kparams);
    if (s2kparams == NULL)
	return ENOMEM;
    ret = krb5_data_alloc(s2kparams, len);
    if (ret) {
	free(s2kparams);
	return ret;
    }
    _krb5_put_int(s2kparams->data, value, len);
    *ps2kparams = s2kparams;
    return 0;
}

static krb5_error_code
make_etype_info2_entry(ETYPE_INFO2_ENTRY *ent, Key *key)
{
    krb5_error_code ret;

    ent->etype = key->key.keytype;
    if(key->salt) {
	ALLOC(ent->salt);
	if (ent->salt == NULL)
	    return ENOMEM;
	*ent->salt = malloc(key->salt->salt.length + 1);
	if (*ent->salt == NULL) {
	    free(ent->salt);
	    ent->salt = NULL;
	    return ENOMEM;
	}
	memcpy(*ent->salt, key->salt->salt.data, key->salt->salt.length);
	(*ent->salt)[key->salt->salt.length] = '\0';
    } else
	ent->salt = NULL;

    ent->s2kparams = NULL;

    switch (key->key.keytype) {
    case ETYPE_AES128_CTS_HMAC_SHA1_96:
    case ETYPE_AES256_CTS_HMAC_SHA1_96:
	ret = make_s2kparams(_krb5_AES_SHA1_string_to_default_iterator,
			     4, &ent->s2kparams);
	break;
    case KRB5_ENCTYPE_AES128_CTS_HMAC_SHA256_128:
    case KRB5_ENCTYPE_AES256_CTS_HMAC_SHA384_192:
	ret = make_s2kparams(_krb5_AES_SHA2_string_to_default_iterator,
			     4, &ent->s2kparams);
	break;
    case ETYPE_DES_CBC_CRC:
    case ETYPE_DES_CBC_MD4:
    case ETYPE_DES_CBC_MD5:
	/* Check if this was a AFS3 salted key */
	if(key->salt && key->salt->type == hdb_afs3_salt)
	    ret = make_s2kparams(1, 1, &ent->s2kparams);
	else
	    ret = 0;
	break;
    default:
	ret = 0;
	break;
    }
    return ret;
}

/*
 * Return an ETYPE-INFO2. Enctypes are storted the same way as in the
 * database (client supported enctypes first, then the unsupported
 * enctypes).
 */

static krb5_error_code
get_pa_etype_info2(krb5_context context,
		   krb5_kdc_configuration *config,
		   METHOD_DATA *md, Key *ckey)
{
    krb5_error_code ret = 0;
    ETYPE_INFO2 pa;
    unsigned char *buf;
    size_t len;

    pa.len = 1;
    pa.val = calloc(1, sizeof(pa.val[0]));
    if(pa.val == NULL)
	return ENOMEM;

    ret = make_etype_info2_entry(&pa.val[0], ckey);
    if (ret) {
	free_ETYPE_INFO2(&pa);
	return ret;
    }

    ASN1_MALLOC_ENCODE(ETYPE_INFO2, buf, len, &pa, &len, ret);
    free_ETYPE_INFO2(&pa);
    if(ret)
	return ret;
    ret = realloc_method_data(md);
    if(ret) {
	free(buf);
	return ret;
    }
    md->val[md->len - 1].padata_type = KRB5_PADATA_ETYPE_INFO2;
    md->val[md->len - 1].padata_value.length = len;
    md->val[md->len - 1].padata_value.data = buf;
    return 0;
}

/*
 *
 */

static void
log_as_req(krb5_context context,
	   krb5_kdc_configuration *config,
	   krb5_enctype cetype,
	   krb5_enctype setype,
	   const KDC_REQ_BODY *b)
{
    krb5_error_code ret;
    struct rk_strpool *p;
    char *str;
    size_t i;

    p = rk_strpoolprintf(NULL, "%s", "Client supported enctypes: ");

    for (i = 0; i < b->etype.len; i++) {
	ret = krb5_enctype_to_string(context, b->etype.val[i], &str);
	if (ret == 0) {
	    p = rk_strpoolprintf(p, "%s", str);
	    free(str);
	} else
	    p = rk_strpoolprintf(p, "%d", b->etype.val[i]);
	if (p && i + 1 < b->etype.len)
	    p = rk_strpoolprintf(p, ", ");
	if (p == NULL) {
	    kdc_log(context, config, 0, "out of memory");
	    return;
	}
    }
    if (p == NULL)
	p = rk_strpoolprintf(p, "no encryption types");

    {
	char *cet;
	char *set;

	ret = krb5_enctype_to_string(context, cetype, &cet);
	if(ret == 0) {
	    ret = krb5_enctype_to_string(context, setype, &set);
	    if (ret == 0) {
		p = rk_strpoolprintf(p, ", using %s/%s", cet, set);
		free(set);
	    }
	    free(cet);
	}
	if (ret != 0)
	    p = rk_strpoolprintf(p, ", using enctypes %d/%d",
				 cetype, setype);
    }

    str = rk_strpoolcollect(p);
    kdc_log(context, config, 0, "%s", str);
    free(str);

    {
	char fixedstr[128];
	unparse_flags(KDCOptions2int(b->kdc_options), asn1_KDCOptions_units(),
		      fixedstr, sizeof(fixedstr));
	if(*fixedstr)
	    kdc_log(context, config, 0, "Requested flags: %s", fixedstr);
    }
}

/*
 * verify the flags on `client' and `server', returning 0
 * if they are OK and generating an error messages and returning
 * and error code otherwise.
 */

krb5_error_code
kdc_check_flags(krb5_context context,
		krb5_kdc_configuration *config,
		hdb_entry_ex *client_ex, const char *client_name,
		hdb_entry_ex *server_ex, const char *server_name,
		krb5_boolean is_as_req)
{
    if(client_ex != NULL) {
	hdb_entry *client = &client_ex->entry;

	/* check client */
	if (client->flags.locked_out) {
	    kdc_log(context, config, 0,
		    "Client (%s) is locked out", client_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if (client->flags.invalid) {
	    kdc_log(context, config, 0,
		    "Client (%s) has invalid bit set", client_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if(!client->flags.client){
	    kdc_log(context, config, 0,
		    "Principal may not act as client -- %s", client_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if (client->valid_start && *client->valid_start > kdc_time) {
	    char starttime_str[100];
	    krb5_format_time(context, *client->valid_start,
			     starttime_str, sizeof(starttime_str), TRUE);
	    kdc_log(context, config, 0,
		    "Client not yet valid until %s -- %s",
		    starttime_str, client_name);
	    return KRB5KDC_ERR_CLIENT_NOTYET;
	}

	if (client->valid_end && *client->valid_end < kdc_time) {
	    char endtime_str[100];
	    krb5_format_time(context, *client->valid_end,
			     endtime_str, sizeof(endtime_str), TRUE);
	    kdc_log(context, config, 0,
		    "Client expired at %s -- %s",
		    endtime_str, client_name);
	    return KRB5KDC_ERR_NAME_EXP;
	}

	if (client->flags.require_pwchange &&
	    (server_ex == NULL || !server_ex->entry.flags.change_pw)) {
	    kdc_log(context, config, 0,
		    "Client's key must be changed -- %s", client_name);
	    return KRB5KDC_ERR_KEY_EXPIRED;
	}

	if (client->pw_end && *client->pw_end < kdc_time
	    && (server_ex == NULL || !server_ex->entry.flags.change_pw)) {
	    char pwend_str[100];
	    krb5_format_time(context, *client->pw_end,
			     pwend_str, sizeof(pwend_str), TRUE);
	    kdc_log(context, config, 0,
		    "Client's key has expired at %s -- %s",
		    pwend_str, client_name);
	    return KRB5KDC_ERR_KEY_EXPIRED;
	}
    }

    /* check server */

    if (server_ex != NULL) {
	hdb_entry *server = &server_ex->entry;

	if (server->flags.locked_out) {
	    kdc_log(context, config, 0,
		    "Client server locked out -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}
	if (server->flags.invalid) {
	    kdc_log(context, config, 0,
		    "Server has invalid flag set -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if(!server->flags.server){
	    kdc_log(context, config, 0,
		    "Principal may not act as server -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if(!is_as_req && server->flags.initial) {
	    kdc_log(context, config, 0,
		    "AS-REQ is required for server -- %s", server_name);
	    return KRB5KDC_ERR_POLICY;
	}

	if (server->valid_start && *server->valid_start > kdc_time) {
	    char starttime_str[100];
	    krb5_format_time(context, *server->valid_start,
			     starttime_str, sizeof(starttime_str), TRUE);
	    kdc_log(context, config, 0,
		    "Server not yet valid until %s -- %s",
		    starttime_str, server_name);
	    return KRB5KDC_ERR_SERVICE_NOTYET;
	}

	if (server->valid_end && *server->valid_end < kdc_time) {
	    char endtime_str[100];
	    krb5_format_time(context, *server->valid_end,
			     endtime_str, sizeof(endtime_str), TRUE);
	    kdc_log(context, config, 0,
		    "Server expired at %s -- %s",
		    endtime_str, server_name);
	    return KRB5KDC_ERR_SERVICE_EXP;
	}

	if (server->pw_end && *server->pw_end < kdc_time) {
	    char pwend_str[100];
	    krb5_format_time(context, *server->pw_end,
			     pwend_str, sizeof(pwend_str), TRUE);
	    kdc_log(context, config, 0,
		    "Server's key has expired at -- %s",
		    pwend_str, server_name);
	    return KRB5KDC_ERR_KEY_EXPIRED;
	}
    }
    return 0;
}

/*
 * Return TRUE if `from' is part of `addresses' taking into consideration
 * the configuration variables that tells us how strict we should be about
 * these checks
 */

krb5_boolean
_kdc_check_addresses(krb5_context context,
		     krb5_kdc_configuration *config,
		     HostAddresses *addresses, const struct sockaddr *from)
{
    krb5_error_code ret;
    krb5_address addr;
    krb5_boolean result;
    krb5_boolean only_netbios = TRUE;
    size_t i;

    if(config->check_ticket_addresses == 0)
	return TRUE;

    if(addresses == NULL)
	return config->allow_null_ticket_addresses;

    for (i = 0; i < addresses->len; ++i) {
	if (addresses->val[i].addr_type != KRB5_ADDRESS_NETBIOS) {
	    only_netbios = FALSE;
	}
    }

    /* Windows sends it's netbios name, which I can only assume is
     * used for the 'allowed workstations' check.  This is painful,
     * but we still want to check IP addresses if they happen to be
     * present.
     */

    if(only_netbios)
	return config->allow_null_ticket_addresses;

    ret = krb5_sockaddr2address (context, from, &addr);
    if(ret)
	return FALSE;

    result = krb5_address_search(context, &addr, addresses);
    krb5_free_address (context, &addr);
    return result;
}

/*
 *
 */

static krb5_boolean
send_pac_p(krb5_context context, KDC_REQ *req)
{
    krb5_error_code ret;
    PA_PAC_REQUEST pacreq;
    const PA_DATA *pa;
    int i = 0;

    pa = _kdc_find_padata(req, &i, KRB5_PADATA_PA_PAC_REQUEST);
    if (pa == NULL)
	return TRUE;

    ret = decode_PA_PAC_REQUEST(pa->padata_value.data,
				pa->padata_value.length,
				&pacreq,
				NULL);
    if (ret)
	return TRUE;
    i = pacreq.include_pac;
    free_PA_PAC_REQUEST(&pacreq);
    if (i == 0)
	return FALSE;
    return TRUE;
}

/*
 *
 */

static krb5_error_code
generate_pac(kdc_request_t r, Key *skey)
{
    krb5_error_code ret;
    krb5_pac p = NULL;
    krb5_data data;

    ret = _kdc_pac_generate(r->context, r->client, &p);
    if (ret) {
	_kdc_r_log(r, 0, "PAC generation failed for -- %s",
		   r->client_name);
	return ret;
    }
    if (p == NULL)
	return 0;

    ret = _krb5_pac_sign(r->context, p, r->et.authtime,
			 r->client->entry.principal,
			 &skey->key, /* Server key */
			 &skey->key, /* FIXME: should be krbtgt key */
			 &data);
    krb5_pac_free(r->context, p);
    if (ret) {
	_kdc_r_log(r, 0, "PAC signing failed for -- %s",
		   r->client_name);
	return ret;
    }
    
    ret = _kdc_tkt_add_if_relevant_ad(r->context, &r->et,
				      KRB5_AUTHDATA_WIN2K_PAC,
				      &data);
    krb5_data_free(&data);

    return ret;
}

/*
 *
 */

krb5_boolean
_kdc_is_anonymous(krb5_context context, krb5_principal principal)
{
    if ((principal->name.name_type != KRB5_NT_WELLKNOWN &&
	 principal->name.name_type != KRB5_NT_UNKNOWN) ||
	principal->name.name_string.len != 2 ||
	strcmp(principal->name.name_string.val[0], KRB5_WELLKNOWN_NAME) != 0 ||
	strcmp(principal->name.name_string.val[1], KRB5_ANON_NAME) != 0)
	return 0;
    return 1;
}

static int
require_preauth_p(kdc_request_t r)
{
    return r->config->require_preauth
	|| r->client->entry.flags.require_preauth
	|| r->server->entry.flags.require_preauth;
}


/*
 *
 */

static krb5_error_code
add_enc_pa_rep(kdc_request_t r)
{
    krb5_error_code ret;
    krb5_crypto crypto;
    Checksum checksum;
    krb5_data cdata;
    size_t len;

    ret = krb5_crypto_init(r->context, &r->reply_key, 0, &crypto);
    if (ret)
	return ret;

    ret = krb5_create_checksum(r->context, crypto,
			       KRB5_KU_AS_REQ, 0,
			       r->request.data, r->request.length,
			       &checksum);
    krb5_crypto_destroy(r->context, crypto);
    if (ret)
	return ret;

    ASN1_MALLOC_ENCODE(Checksum, cdata.data, cdata.length,
		       &checksum, &len, ret);
    free_Checksum(&checksum);
    if (ret)
	return ret;
    heim_assert(cdata.length == len, "ASN.1 internal error");

    if (r->ek.encrypted_pa_data == NULL) {
	ALLOC(r->ek.encrypted_pa_data);
	if (r->ek.encrypted_pa_data == NULL)
	    return ENOMEM;
    }
    ret = krb5_padata_add(r->context, r->ek.encrypted_pa_data,
			  KRB5_PADATA_REQ_ENC_PA_REP, cdata.data, cdata.length);
    if (ret)
	return ret;
    
    return krb5_padata_add(r->context, r->ek.encrypted_pa_data,
			   KRB5_PADATA_FX_FAST, NULL, 0);
}

/*
 *
 */

krb5_error_code
_kdc_as_rep(kdc_request_t r,
	    krb5_data *reply,
	    const char *from,
	    struct sockaddr *from_addr,
	    int datagram_reply)
{
    krb5_context context = r->context;
    krb5_kdc_configuration *config = r->config;
    KDC_REQ *req = &r->req;
    KDC_REQ_BODY *b = NULL;
    AS_REP rep;
    KDCOptions f;
    krb5_enctype setype;
    krb5_error_code ret = 0;
    Key *skey;
    int found_pa = 0;
    int i, flags = HDB_F_FOR_AS_REQ;
    METHOD_DATA error_method;
    const PA_DATA *pa;

    memset(&rep, 0, sizeof(rep));
    error_method.len = 0;
    error_method.val = NULL;

    /*
     * Look for FAST armor and unwrap
     */
    ret = _kdc_fast_unwrap_request(r);
    if (ret) {
	_kdc_r_log(r, 0, "FAST unwrap request from %s failed: %d", from, ret);
	goto out;
    }

    b = &req->req_body;
    f = b->kdc_options;

    if (f.canonicalize)
	flags |= HDB_F_CANON;

    if(b->sname == NULL){
	ret = KRB5KRB_ERR_GENERIC;
	_kdc_set_e_text(r, "No server in request");
    } else{
	ret = _krb5_principalname2krb5_principal (context,
						  &r->server_princ,
						  *(b->sname),
						  b->realm);
	if (ret == 0)
	    ret = krb5_unparse_name(context, r->server_princ, &r->server_name);
    }
    if (ret) {
	kdc_log(context, config, 0,
		"AS-REQ malformed server name from %s", from);
	goto out;
    }
    if(b->cname == NULL){
	ret = KRB5KRB_ERR_GENERIC;
	_kdc_set_e_text(r, "No client in request");
    } else {
	ret = _krb5_principalname2krb5_principal (context,
						  &r->client_princ,
						  *(b->cname),
						  b->realm);
	if (ret)
	    goto out;

	ret = krb5_unparse_name(context, r->client_princ, &r->client_name);
    }
    if (ret) {
	kdc_log(context, config, 0,
		"AS-REQ malformed client name from %s", from);
	goto out;
    }

    kdc_log(context, config, 0, "AS-REQ %s from %s for %s",
	    r->client_name, from, r->server_name);

    /*
     *
     */

    if (_kdc_is_anonymous(context, r->client_princ)) {
	if (!_kdc_is_anon_request(b)) {
	    kdc_log(context, config, 0, "Anonymous ticket w/o anonymous flag");
	    ret = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
	    goto out;
	}
    } else if (_kdc_is_anon_request(b)) {
	kdc_log(context, config, 0,
		"Request for a anonymous ticket with non "
		"anonymous client name: %s", r->client_name);
	ret = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
	goto out;
    }

    /*
     *
     */

    ret = _kdc_db_fetch(context, config, r->client_princ,
			HDB_F_GET_CLIENT | flags, NULL,
			&r->clientdb, &r->client);
    if(ret == HDB_ERR_NOT_FOUND_HERE) {
	kdc_log(context, config, 5, "client %s does not have secrets at this KDC, need to proxy",
		r->client_name);
	goto out;
    } else if (ret == HDB_ERR_WRONG_REALM) {
	char *fixed_client_name = NULL;

	ret = krb5_unparse_name(context, r->client->entry.principal,
				&fixed_client_name);
	if (ret) {
	    goto out;
	}

	kdc_log(context, config, 0, "WRONG_REALM - %s -> %s",
		r->client_name, fixed_client_name);
	free(fixed_client_name);

	ret = _kdc_fast_mk_error(context, r,
				 &error_method,
				 r->armor_crypto,
				 &req->req_body,
				 KRB5_KDC_ERR_WRONG_REALM,
				 NULL,
				 r->server_princ,
				 NULL,
				 &r->client->entry.principal->realm,
				 NULL, NULL,
				 reply);
	goto out;
    } else if(ret){
	const char *msg = krb5_get_error_message(context, ret);
	kdc_log(context, config, 0, "UNKNOWN -- %s: %s", r->client_name, msg);
	krb5_free_error_message(context, msg);
	ret = KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
	goto out;
    }
    ret = _kdc_db_fetch(context, config, r->server_princ,
			HDB_F_GET_SERVER|HDB_F_GET_KRBTGT | flags,
			NULL, NULL, &r->server);
    if(ret == HDB_ERR_NOT_FOUND_HERE) {
	kdc_log(context, config, 5, "target %s does not have secrets at this KDC, need to proxy",
		r->server_name);
	goto out;
    } else if(ret){
	const char *msg = krb5_get_error_message(context, ret);
	kdc_log(context, config, 0, "UNKNOWN -- %s: %s", r->server_name, msg);
	krb5_free_error_message(context, msg);
	ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	goto out;
    }

    /*
     * Select a session enctype from the list of the crypto system
     * supported enctypes that is supported by the client and is one of
     * the enctype of the enctype of the service (likely krbtgt).
     *
     * The latter is used as a hint of what enctypes all KDC support,
     * to make sure a newer version of KDC won't generate a session
     * enctype that an older version of a KDC in the same realm can't
     * decrypt.
     */

    ret = _kdc_find_etype(context,
			  krb5_principal_is_krbtgt(context, r->server_princ) ?
			  config->tgt_use_strongest_session_key :
			  config->svc_use_strongest_session_key, FALSE,
			  r->client, b->etype.val, b->etype.len, &r->sessionetype,
			  NULL);
    if (ret) {
	kdc_log(context, config, 0,
		"Client (%s) from %s has no common enctypes with KDC "
		"to use for the session key",
		r->client_name, from);
	goto out;
    }

    /*
     * Pre-auth processing
     */

    if(req->padata){
	unsigned int n;

	log_patypes(context, config, req->padata);

	/* Check if preauth matching */

	for (n = 0; !found_pa && n < sizeof(pat) / sizeof(pat[0]); n++) {
	    if (pat[n].validate == NULL)
		continue;
	    if (r->armor_crypto == NULL && (pat[n].flags & PA_REQ_FAST))
		continue;

	    kdc_log(context, config, 5,
		    "Looking for %s pa-data -- %s", pat[n].name, r->client_name);
	    i = 0;
	    pa = _kdc_find_padata(req, &i, pat[n].type);
	    if (pa) {
		ret = pat[n].validate(r, pa);
		if (ret != 0) {
		    goto out;
		}
		kdc_log(context, config, 0,
			"%s pre-authentication succeeded -- %s",
			pat[n].name, r->client_name);
		found_pa = 1;
		r->et.flags.pre_authent = 1;
	    }
	}
    }

    if (found_pa == 0) {
	Key *ckey = NULL;
	size_t n;

	for (n = 0; n < sizeof(pat) / sizeof(pat[0]); n++) {
	    if ((pat[n].flags & PA_ANNOUNCE) == 0)
		continue;
	    ret = krb5_padata_add(context, &error_method,
				  pat[n].type, NULL, 0);
	    if (ret)
		goto out;
	}

	/*
	 * If there is a client key, send ETYPE_INFO{,2}
	 */
	ret = _kdc_find_etype(context,
			      config->preauth_use_strongest_session_key, TRUE,
			      r->client, b->etype.val, b->etype.len, NULL, &ckey);
	if (ret == 0) {

	    /*
	     * RFC4120 requires:
	     * - If the client only knows about old enctypes, then send
	     *   both info replies (we send 'info' first in the list).
	     * - If the client is 'modern', because it knows about 'new'
	     *   enctype types, then only send the 'info2' reply.
	     *
	     * Before we send the full list of etype-info data, we pick
	     * the client key we would have used anyway below, just pick
	     * that instead.
	     */

	    if (older_enctype(ckey->key.keytype)) {
		ret = get_pa_etype_info(context, config,
					&error_method, ckey);
		if (ret)
		    goto out;
	    }
	    ret = get_pa_etype_info2(context, config,
				     &error_method, ckey);
	    if (ret)
		goto out;
	}

	/* 
	 * send requre preauth is its required or anon is requested,
	 * anon is today only allowed via preauth mechanisms.
	 */
	if (require_preauth_p(r) || _kdc_is_anon_request(b)) {
	    ret = KRB5KDC_ERR_PREAUTH_REQUIRED;
	    _kdc_set_e_text(r, "Need to use PA-ENC-TIMESTAMP/PA-PK-AS-REQ");
	    goto out;
	}

	if (ckey == NULL) {
	    ret = KRB5KDC_ERR_CLIENT_NOTYET;
	    _kdc_set_e_text(r, "Doesn't have a client key available");
	    goto out;
	}
	krb5_free_keyblock_contents(r->context,  &r->reply_key);
	ret = krb5_copy_keyblock_contents(r->context, &ckey->key, &r->reply_key);
	if (ret)
	    goto out;
    }

    if (r->clientdb->hdb_auth_status) {
	r->clientdb->hdb_auth_status(context, r->clientdb, r->client, 
				     HDB_AUTH_SUCCESS);
    }

    /*
     * Verify flags after the user been required to prove its identity
     * with in a preauth mech.
     */

    ret = _kdc_check_access(context, config, r->client, r->client_name,
			    r->server, r->server_name,
			    req, &error_method);
    if(ret)
	goto out;

    /*
     * Select the best encryption type for the KDC with out regard to
     * the client since the client never needs to read that data.
     */

    ret = _kdc_get_preferred_key(context, config,
				 r->server, r->server_name,
				 &setype, &skey);
    if(ret)
	goto out;

    if(f.renew || f.validate || f.proxy || f.forwarded || f.enc_tkt_in_skey
       || (_kdc_is_anon_request(b) && !config->allow_anonymous)) {
	ret = KRB5KDC_ERR_BADOPTION;
	_kdc_set_e_text(r, "Bad KDC options");
	goto out;
    }

    /*
     * Build reply
     */

    rep.pvno = 5;
    rep.msg_type = krb_as_rep;

    if (_kdc_is_anonymous(context, r->client_princ)) {
	Realm anon_realm=KRB5_ANON_REALM;
	ret = copy_Realm(&anon_realm, &rep.crealm);
    } else
	ret = copy_Realm(&r->client->entry.principal->realm, &rep.crealm);
    if (ret)
	goto out;
    ret = _krb5_principal2principalname(&rep.cname, r->client->entry.principal);
    if (ret)
	goto out;

    rep.ticket.tkt_vno = 5;
    copy_Realm(&r->server->entry.principal->realm, &rep.ticket.realm);
    _krb5_principal2principalname(&rep.ticket.sname,
				  r->server->entry.principal);
    /* java 1.6 expects the name to be the same type, lets allow that
     * uncomplicated name-types. */
#define CNT(sp,t) (((sp)->sname->name_type) == KRB5_NT_##t)
    if (CNT(b, UNKNOWN) || CNT(b, PRINCIPAL) || CNT(b, SRV_INST) || CNT(b, SRV_HST) || CNT(b, SRV_XHST))
	rep.ticket.sname.name_type = b->sname->name_type;
#undef CNT

    r->et.flags.initial = 1;
    if(r->client->entry.flags.forwardable && r->server->entry.flags.forwardable)
	r->et.flags.forwardable = f.forwardable;
    else if (f.forwardable) {
	_kdc_set_e_text(r, "Ticket may not be forwardable");
	ret = KRB5KDC_ERR_POLICY;
	goto out;
    }
    if(r->client->entry.flags.proxiable && r->server->entry.flags.proxiable)
	r->et.flags.proxiable = f.proxiable;
    else if (f.proxiable) {
	_kdc_set_e_text(r, "Ticket may not be proxiable");
	ret = KRB5KDC_ERR_POLICY;
	goto out;
    }
    if(r->client->entry.flags.postdate && r->server->entry.flags.postdate)
	r->et.flags.may_postdate = f.allow_postdate;
    else if (f.allow_postdate){
	_kdc_set_e_text(r, "Ticket may not be postdate");
	ret = KRB5KDC_ERR_POLICY;
	goto out;
    }

    /* check for valid set of addresses */
    if(!_kdc_check_addresses(context, config, b->addresses, from_addr)) {
	_kdc_set_e_text(r, "Bad address list in requested");
	ret = KRB5KRB_AP_ERR_BADADDR;
	goto out;
    }

    ret = copy_PrincipalName(&rep.cname, &r->et.cname);
    if (ret)
	goto out;
    ret = copy_Realm(&rep.crealm, &r->et.crealm);
    if (ret)
	goto out;

    {
	time_t start;
	time_t t;
	
	start = r->et.authtime = kdc_time;

	if(f.postdated && req->req_body.from){
	    ALLOC(r->et.starttime);
	    start = *r->et.starttime = *req->req_body.from;
	    r->et.flags.invalid = 1;
	    r->et.flags.postdated = 1; /* XXX ??? */
	}
	_kdc_fix_time(&b->till);
	t = *b->till;

	/* be careful not overflowing */

	if(r->client->entry.max_life)
	    t = start + min(t - start, *r->client->entry.max_life);
	if(r->server->entry.max_life)
	    t = start + min(t - start, *r->server->entry.max_life);
#if 0
	t = min(t, start + realm->max_life);
#endif
	r->et.endtime = t;
	if(f.renewable_ok && r->et.endtime < *b->till){
	    f.renewable = 1;
	    if(b->rtime == NULL){
		ALLOC(b->rtime);
		*b->rtime = 0;
	    }
	    if(*b->rtime < *b->till)
		*b->rtime = *b->till;
	}
	if(f.renewable && b->rtime){
	    t = *b->rtime;
	    if(t == 0)
		t = MAX_TIME;
	    if(r->client->entry.max_renew)
		t = start + min(t - start, *r->client->entry.max_renew);
	    if(r->server->entry.max_renew)
		t = start + min(t - start, *r->server->entry.max_renew);
#if 0
	    t = min(t, start + realm->max_renew);
#endif
	    ALLOC(r->et.renew_till);
	    *r->et.renew_till = t;
	    r->et.flags.renewable = 1;
	}
    }

    if (_kdc_is_anon_request(b))
	r->et.flags.anonymous = 1;

    if(b->addresses){
	ALLOC(r->et.caddr);
	copy_HostAddresses(b->addresses, r->et.caddr);
    }

    r->et.transited.tr_type = DOMAIN_X500_COMPRESS;
    krb5_data_zero(&r->et.transited.contents);

    /* The MIT ASN.1 library (obviously) doesn't tell lengths encoded
     * as 0 and as 0x80 (meaning indefinite length) apart, and is thus
     * incapable of correctly decoding SEQUENCE OF's of zero length.
     *
     * To fix this, always send at least one no-op last_req
     *
     * If there's a pw_end or valid_end we will use that,
     * otherwise just a dummy lr.
     */
    r->ek.last_req.val = malloc(2 * sizeof(*r->ek.last_req.val));
    if (r->ek.last_req.val == NULL) {
	ret = ENOMEM;
	goto out;
    }
    r->ek.last_req.len = 0;
    if (r->client->entry.pw_end
	&& (config->kdc_warn_pwexpire == 0
	    || kdc_time + config->kdc_warn_pwexpire >= *r->client->entry.pw_end)) {
	r->ek.last_req.val[r->ek.last_req.len].lr_type  = LR_PW_EXPTIME;
	r->ek.last_req.val[r->ek.last_req.len].lr_value = *r->client->entry.pw_end;
	++r->ek.last_req.len;
    }
    if (r->client->entry.valid_end) {
	r->ek.last_req.val[r->ek.last_req.len].lr_type  = LR_ACCT_EXPTIME;
	r->ek.last_req.val[r->ek.last_req.len].lr_value = *r->client->entry.valid_end;
	++r->ek.last_req.len;
    }
    if (r->ek.last_req.len == 0) {
	r->ek.last_req.val[r->ek.last_req.len].lr_type  = LR_NONE;
	r->ek.last_req.val[r->ek.last_req.len].lr_value = 0;
	++r->ek.last_req.len;
    }
    r->ek.nonce = b->nonce;
    if (r->client->entry.valid_end || r->client->entry.pw_end) {
	ALLOC(r->ek.key_expiration);
	if (r->client->entry.valid_end) {
	    if (r->client->entry.pw_end)
		*r->ek.key_expiration = min(*r->client->entry.valid_end,
					 *r->client->entry.pw_end);
	    else
		*r->ek.key_expiration = *r->client->entry.valid_end;
	} else
	    *r->ek.key_expiration = *r->client->entry.pw_end;
    } else
	r->ek.key_expiration = NULL;
    r->ek.flags = r->et.flags;
    r->ek.authtime = r->et.authtime;
    if (r->et.starttime) {
	ALLOC(r->ek.starttime);
	*r->ek.starttime = *r->et.starttime;
    }
    r->ek.endtime = r->et.endtime;
    if (r->et.renew_till) {
	ALLOC(r->ek.renew_till);
	*r->ek.renew_till = *r->et.renew_till;
    }
    copy_Realm(&rep.ticket.realm, &r->ek.srealm);
    copy_PrincipalName(&rep.ticket.sname, &r->ek.sname);
    if(r->et.caddr){
	ALLOC(r->ek.caddr);
	copy_HostAddresses(r->et.caddr, r->ek.caddr);
    }

    /*
     * Check and session and reply keys
     */

    if (r->session_key.keytype == ETYPE_NULL) {
	ret = krb5_generate_random_keyblock(context, r->sessionetype, &r->session_key);
	if (ret)
	    goto out;
    }

    if (r->reply_key.keytype == ETYPE_NULL) {
	_kdc_set_e_text(r, "Client have no reply key");
	ret = KRB5KDC_ERR_CLIENT_NOTYET;
	goto out;
    }

    ret = copy_EncryptionKey(&r->session_key, &r->et.key);
    if (ret)
	goto out;

    ret = copy_EncryptionKey(&r->session_key, &r->ek.key);
    if (ret)
	goto out;

    if (r->outpadata.len) {

	ALLOC(rep.padata);
	if (rep.padata == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	ret = copy_METHOD_DATA(&r->outpadata, rep.padata);
	if (ret)
	    goto out;
    }

    /* Add the PAC */
    if (send_pac_p(context, req)) {
	generate_pac(r, skey);
    }

    _kdc_log_timestamp(context, config, "AS-REQ", r->et.authtime, r->et.starttime,
		       r->et.endtime, r->et.renew_till);

    /* do this as the last thing since this signs the EncTicketPart */
    ret = _kdc_add_KRB5SignedPath(context,
				  config,
				  r->server,
				  setype,
				  r->client->entry.principal,
				  NULL,
				  NULL,
				  &r->et);
    if (ret)
	goto out;

    log_as_req(context, config, r->reply_key.keytype, setype, b);

    /*
     * We always say we support FAST/enc-pa-rep
     */

    r->et.flags.enc_pa_rep = r->ek.flags.enc_pa_rep = 1;

    /*
     * Add REQ_ENC_PA_REP if client supports it
     */

    i = 0;
    pa = _kdc_find_padata(req, &i, KRB5_PADATA_REQ_ENC_PA_REP);
    if (pa) {

	ret = add_enc_pa_rep(r);
	if (ret) {
	    const char *msg = krb5_get_error_message(r->context, ret);
	    _kdc_r_log(r, 0, "add_enc_pa_rep failed: %s: %d", msg, ret);
	    krb5_free_error_message(r->context, msg);
	    goto out;
	}
    }

    /*
     *
     */

    ret = _kdc_encode_reply(context, config,
			    r->armor_crypto, req->req_body.nonce,
			    &rep, &r->et, &r->ek, setype, r->server->entry.kvno,
			    &skey->key, r->client->entry.kvno,
			    &r->reply_key, 0, &r->e_text, reply);
    if (ret)
	goto out;

    /*
     * Check if message too large
     */
    if (datagram_reply && reply->length > config->max_datagram_reply_length) {
	krb5_data_free(reply);
	ret = KRB5KRB_ERR_RESPONSE_TOO_BIG;
	_kdc_set_e_text(r, "Reply packet too large");
    }

out:
    free_AS_REP(&rep);

    /*
     * In case of a non proxy error, build an error message.
     */
    if(ret != 0 && ret != HDB_ERR_NOT_FOUND_HERE && reply->length == 0) {
	ret = _kdc_fast_mk_error(context, r,
				 &error_method,
				 r->armor_crypto,
				 &req->req_body,
				 ret, r->e_text,
				 r->server_princ,
				 &r->client_princ->name,
				 &r->client_princ->realm,
				 NULL, NULL,
				 reply);
	if (ret)
	    goto out2;
    }
out2:
    free_EncTicketPart(&r->et);
    free_EncKDCRepPart(&r->ek);
    free_KDCFastState(&r->fast);

    if (error_method.len)
	free_METHOD_DATA(&error_method);
    if (r->outpadata.len)
	free_METHOD_DATA(&r->outpadata);
    if (r->client_princ) {
	krb5_free_principal(context, r->client_princ);
	r->client_princ = NULL;
    }
    if (r->client_name) {
	free(r->client_name);
	r->client_name = NULL;
    }
    if (r->server_princ){
	krb5_free_principal(context, r->server_princ);
	r->server_princ = NULL;
    }
    if (r->server_name) {
	free(r->server_name);
	r->server_name = NULL;
    }
    if (r->client)
	_kdc_free_ent(context, r->client);
    if (r->server)
	_kdc_free_ent(context, r->server);
    if (r->armor_crypto) {
	krb5_crypto_destroy(r->context, r->armor_crypto);
	r->armor_crypto = NULL;
    }
    krb5_free_keyblock_contents(r->context, &r->reply_key);
    krb5_free_keyblock_contents(r->context, &r->session_key);
    return ret;
}

/*
 * Add the AuthorizationData `data´ of `type´ to the last element in
 * the sequence of authorization_data in `tkt´ wrapped in an IF_RELEVANT
 */

krb5_error_code
_kdc_tkt_add_if_relevant_ad(krb5_context context,
			    EncTicketPart *tkt,
			    int type,
			    const krb5_data *data)
{
    krb5_error_code ret;
    size_t size = 0;

    if (tkt->authorization_data == NULL) {
	tkt->authorization_data = calloc(1, sizeof(*tkt->authorization_data));
	if (tkt->authorization_data == NULL) {
	    krb5_set_error_message(context, ENOMEM, "out of memory");
	    return ENOMEM;
	}
    }

    /* add the entry to the last element */
    {
	AuthorizationData ad = { 0, NULL };
	AuthorizationDataElement ade;

	ade.ad_type = type;
	ade.ad_data = *data;

	ret = add_AuthorizationData(&ad, &ade);
	if (ret) {
	    krb5_set_error_message(context, ret, "add AuthorizationData failed");
	    return ret;
	}

	ade.ad_type = KRB5_AUTHDATA_IF_RELEVANT;

	ASN1_MALLOC_ENCODE(AuthorizationData,
			   ade.ad_data.data, ade.ad_data.length,
			   &ad, &size, ret);
	free_AuthorizationData(&ad);
	if (ret) {
	    krb5_set_error_message(context, ret, "ASN.1 encode of "
				   "AuthorizationData failed");
	    return ret;
	}
	if (ade.ad_data.length != size)
	    krb5_abortx(context, "internal asn.1 encoder error");

	ret = add_AuthorizationData(tkt->authorization_data, &ade);
	der_free_octet_string(&ade.ad_data);
	if (ret) {
	    krb5_set_error_message(context, ret, "add AuthorizationData failed");
	    return ret;
	}
    }

    return 0;
}

krb5_boolean
_kdc_is_anon_request(const KDC_REQ_BODY *b)
{
	/* some versions of heimdal use bit 14 instead of 16 for
	   request_anonymous, as indicated in the anonymous draft prior to
	   version 11. Bit 14 is assigned to S4U2Proxy, but all S4U2Proxy
	   requests will have a second ticket; don't consider those anonymous */
	return (b->kdc_options.request_anonymous ||
		(b->kdc_options.constrained_delegation && !b->additional_tickets));
}
