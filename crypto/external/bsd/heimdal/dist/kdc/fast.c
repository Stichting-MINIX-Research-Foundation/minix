/*	$NetBSD: fast.c,v 1.2 2017/01/28 21:31:44 christos Exp $	*/

/*
 * Copyright (c) 1997-2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 - 2011 Apple Inc. All rights reserved.
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

static krb5_error_code
get_fastuser_crypto(kdc_request_t r, krb5_enctype enctype, krb5_crypto *crypto)
{
    krb5_principal fast_princ;
    hdb_entry_ex *fast_user = NULL;
    Key *cookie_key = NULL;
    krb5_error_code ret;

    *crypto = NULL;

    ret = krb5_make_principal(r->context, &fast_princ,
			      KRB5_WELLKNOWN_ORG_H5L_REALM,
			      KRB5_WELLKNOWN_NAME, "org.h5l.fast-cookie", NULL);
    if (ret)
	goto out;

    ret = _kdc_db_fetch(r->context, r->config, fast_princ,
			HDB_F_GET_CLIENT, NULL, NULL, &fast_user);
    krb5_free_principal(r->context, fast_princ);
    if (ret)
	goto out;

    if (enctype == KRB5_ENCTYPE_NULL)
	ret = _kdc_get_preferred_key(r->context, r->config, fast_user,
				     "fast-cookie", &enctype, &cookie_key);
    else
	ret = hdb_enctype2key(r->context, &fast_user->entry, NULL,
			      enctype, &cookie_key);
    if (ret)
	goto out;

    ret = krb5_crypto_init(r->context, &cookie_key->key, 0, crypto);
    if (ret)
	goto out;

 out:
    if (fast_user)
	_kdc_free_ent(r->context, fast_user);

    return ret;
}


static krb5_error_code
fast_parse_cookie(kdc_request_t r, const PA_DATA *pa)
{
    krb5_crypto crypto = NULL;
    krb5_error_code ret;
    KDCFastCookie data;
    krb5_data d1;
    size_t len;

    ret = decode_KDCFastCookie(pa->padata_value.data,
			       pa->padata_value.length,
			       &data, &len);
    if (ret)
	return ret;

    if (len != pa->padata_value.length || strcmp("H5L1", data.version) != 0) {
	free_KDCFastCookie(&data);
	return KRB5KDC_ERR_POLICY;
    }

    ret = get_fastuser_crypto(r, data.cookie.etype, &crypto);
    if (ret)
	goto out;

    ret = krb5_decrypt_EncryptedData(r->context, crypto,
				     KRB5_KU_H5L_COOKIE,
				     &data.cookie, &d1);
    krb5_crypto_destroy(r->context, crypto);
    if (ret)
	goto out;

    ret = decode_KDCFastState(d1.data, d1.length, &r->fast, &len);
    krb5_data_free(&d1);
    if (ret)
	goto out;

    if (r->fast.expiration < kdc_time) {
	kdc_log(r->context, r->config, 0, "fast cookie expired");
	ret = KRB5KDC_ERR_POLICY;
	goto out;
    }

 out:
    free_KDCFastCookie(&data);

    return ret;
}

static krb5_error_code
fast_add_cookie(kdc_request_t r, METHOD_DATA *method_data)
{
    krb5_crypto crypto = NULL;
    KDCFastCookie shell;
    krb5_error_code ret;
    krb5_data data;
    size_t size;

    memset(&shell, 0, sizeof(shell));

    r->fast.expiration = kdc_time + FAST_EXPIRATION_TIME;

    ASN1_MALLOC_ENCODE(KDCFastState, data.data, data.length, 
		       &r->fast, &size, ret);
    if (ret)
	return ret;
    heim_assert(size == data.length, "internal asn1 encoder error");

    ret = get_fastuser_crypto(r, KRB5_ENCTYPE_NULL, &crypto);
    if (ret)
	goto out;

    ret = krb5_encrypt_EncryptedData(r->context, crypto,
				     KRB5_KU_H5L_COOKIE,
				     data.data, data.length, 0,
				     &shell.cookie);
    krb5_crypto_destroy(r->context, crypto);
    if (ret)
	goto out;
    
    free(data.data);

    shell.version = "H5L1";

    ASN1_MALLOC_ENCODE(KDCFastCookie, data.data, data.length, 
		       &shell, &size, ret);
    free_EncryptedData(&shell.cookie);
    if (ret)
	goto out;
    heim_assert(size == data.length, "internal asn1 encoder error");
    
    ret = krb5_padata_add(r->context, method_data,
			  KRB5_PADATA_FX_COOKIE,
			  data.data, data.length);
 out:
    if (ret)
	free(data.data);
    return ret;
}

krb5_error_code
_kdc_fast_mk_response(krb5_context context,
		      krb5_crypto armor_crypto,
		      METHOD_DATA *pa_data,
		      krb5_keyblock *strengthen_key,
		      KrbFastFinished *finished,
		      krb5uint32 nonce,
		      krb5_data *data)
{
    PA_FX_FAST_REPLY fxfastrep;
    KrbFastResponse fastrep;
    krb5_error_code ret;
    krb5_data buf;
    size_t size;

    memset(&fxfastrep, 0, sizeof(fxfastrep));
    memset(&fastrep, 0, sizeof(fastrep));
    krb5_data_zero(data);

    if (pa_data) {
	fastrep.padata.val = pa_data->val;
	fastrep.padata.len = pa_data->len;
    }
    fastrep.strengthen_key = strengthen_key;
    fastrep.finished = finished;
    fastrep.nonce = nonce;

    ASN1_MALLOC_ENCODE(KrbFastResponse, buf.data, buf.length,
		       &fastrep, &size, ret);
    if (ret)
	return ret;
    if (buf.length != size)
	krb5_abortx(context, "internal asn.1 error");
    
    fxfastrep.element = choice_PA_FX_FAST_REPLY_armored_data;

    ret = krb5_encrypt_EncryptedData(context,
				     armor_crypto,
				     KRB5_KU_FAST_REP,
				     buf.data,
				     buf.length,
				     0,
				     &fxfastrep.u.armored_data.enc_fast_rep);
    krb5_data_free(&buf);
    if (ret)
	return ret;

    ASN1_MALLOC_ENCODE(PA_FX_FAST_REPLY, data->data, data->length,
		       &fxfastrep, &size, ret);
    free_PA_FX_FAST_REPLY(&fxfastrep);
    if (ret)
	return ret;
    if (data->length != size)
	krb5_abortx(context, "internal asn.1 error");
    
    return 0;
}


krb5_error_code
_kdc_fast_mk_error(krb5_context context,
		   kdc_request_t r,
		   METHOD_DATA *error_method,
		   krb5_crypto armor_crypto,
		   const KDC_REQ_BODY *req_body,
		   krb5_error_code outer_error,
		   const char *e_text,
		   krb5_principal error_server,
		   const PrincipalName *error_client_name,
		   const Realm *error_client_realm,
		   time_t *csec, int *cusec,
		   krb5_data *error_msg)
{
    krb5_error_code ret;
    krb5_data e_data;
    size_t size;

    krb5_data_zero(&e_data);

    if (armor_crypto) {
	PA_FX_FAST_REPLY fxfastrep;
	KrbFastResponse fastrep;

	memset(&fxfastrep, 0, sizeof(fxfastrep));
	memset(&fastrep, 0, sizeof(fastrep));
	    
	/* first add the KRB-ERROR to the fast errors */

	ret = krb5_mk_error_ext(context,
				outer_error,
				e_text,
				NULL,
				error_server,
				error_client_name,
				error_client_realm,
				NULL,
				NULL,
				&e_data);
	if (ret)
	    return ret;

	ret = krb5_padata_add(context, error_method,
			      KRB5_PADATA_FX_ERROR,
			      e_data.data, e_data.length);
	if (ret) {
	    krb5_data_free(&e_data);
	    return ret;
	}

	if (/* hide_principal */ 0) {
	    error_client_name = NULL;
	    error_client_realm = NULL;
	    error_server = NULL;
	    e_text = NULL;
	}

	if (r)
	    ret = fast_add_cookie(r, error_method);
	else
	    ret = krb5_padata_add(context, error_method,
				  KRB5_PADATA_FX_COOKIE,
				  NULL, 0);
	if (ret) {
	    kdc_log(r->context, r->config, 0, "failed to add fast cookie with: %d", ret);
	    free_METHOD_DATA(error_method);
	    return ret;
	}
	
	ret = _kdc_fast_mk_response(context, armor_crypto,
				    error_method, NULL, NULL, 
				    req_body->nonce, &e_data);
	free_METHOD_DATA(error_method);
	if (ret)
	    return ret;
	
	ret = krb5_padata_add(context, error_method,
			      KRB5_PADATA_FX_FAST,
			      e_data.data, e_data.length);
	if (ret)
	    return ret;
    }

    if (error_method && error_method->len) {
	ASN1_MALLOC_ENCODE(METHOD_DATA, e_data.data, e_data.length, 
			   error_method, &size, ret);
	if (ret)
	    return ret;
	if (e_data.length != size)
	    krb5_abortx(context, "internal asn.1 error");
    }
    
    ret = krb5_mk_error_ext(context,
			    outer_error,
			    e_text,
			    (e_data.length ? &e_data : NULL),
			    error_server,
			    error_client_name,
			    error_client_realm,
			    csec,
			    cusec,
			    error_msg);
    krb5_data_free(&e_data);

    return ret;
}

krb5_error_code
_kdc_fast_unwrap_request(kdc_request_t r)
{
    krb5_principal armor_server = NULL;
    hdb_entry_ex *armor_user = NULL;
    PA_FX_FAST_REQUEST fxreq;
    krb5_auth_context ac = NULL;
    krb5_ticket *ticket = NULL;
    krb5_flags ap_req_options;
    Key *armor_key = NULL;
    krb5_keyblock armorkey;
    krb5_error_code ret;
    krb5_ap_req ap_req;
    unsigned char *buf = NULL;
    KrbFastReq fastreq;
    size_t len, size;
    krb5_data data;
    const PA_DATA *pa;
    int i = 0;

    /*
     * First look for FX_COOKIE and and process it
     */
    pa = _kdc_find_padata(&r->req, &i, KRB5_PADATA_FX_COOKIE);
    if (pa) {
	ret = fast_parse_cookie(r, pa);
	if (ret)
	    goto out;
    }
			  
    i = 0;
    pa = _kdc_find_padata(&r->req, &i, KRB5_PADATA_FX_FAST);
    if (pa == NULL)
	return 0;

    ret = decode_PA_FX_FAST_REQUEST(pa->padata_value.data,
				    pa->padata_value.length,
				    &fxreq,
				    &len);
    if (ret)
	goto out;
    if (len != pa->padata_value.length) {
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }

    if (fxreq.element != choice_PA_FX_FAST_REQUEST_armored_data) {
	kdc_log(r->context, r->config, 0,
		"AS-REQ FAST contain unknown type: %d", (int)fxreq.element);
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }

    /* pull out armor key */
    if (fxreq.u.armored_data.armor == NULL) {
	kdc_log(r->context, r->config, 0,
		"AS-REQ armor missing");
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }

    if (fxreq.u.armored_data.armor->armor_type != 1) {
	kdc_log(r->context, r->config, 0,
		"AS-REQ armor type not ap-req");
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }
	    
    ret = krb5_decode_ap_req(r->context,
			     &fxreq.u.armored_data.armor->armor_value,
			     &ap_req);
    if(ret) {
	kdc_log(r->context, r->config, 0, "AP-REQ decode failed");
	goto out;
    }

    /* Save that principal that was in the request */
    ret = _krb5_principalname2krb5_principal(r->context,
					     &armor_server,
					     ap_req.ticket.sname,
					     ap_req.ticket.realm);
    if (ret) {
	free_AP_REQ(&ap_req);
	goto out;
    }

    ret = _kdc_db_fetch(r->context, r->config, armor_server,
			HDB_F_GET_SERVER, NULL, NULL, &armor_user);
    if(ret == HDB_ERR_NOT_FOUND_HERE) {
	kdc_log(r->context, r->config, 5,
		"armor key does not have secrets at this KDC, "
		"need to proxy");
	goto out;
    } else if (ret) {
	free_AP_REQ(&ap_req);
	ret = KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
	goto out;
    }

    ret = hdb_enctype2key(r->context, &armor_user->entry, NULL,
			  ap_req.ticket.enc_part.etype,
			  &armor_key);
    if (ret) {
	free_AP_REQ(&ap_req);
	goto out;
    }

    ret = krb5_verify_ap_req2(r->context, &ac, 
			      &ap_req,
			      armor_server,
			      &armor_key->key,
			      0,
			      &ap_req_options,
			      &ticket, 
			      KRB5_KU_AP_REQ_AUTH);
    free_AP_REQ(&ap_req);
    if (ret)
	goto out;

    if (ac->remote_subkey == NULL) {
	krb5_auth_con_free(r->context, ac);
	kdc_log(r->context, r->config, 0,
		"FAST AP-REQ remote subkey missing");
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }		

    ret = _krb5_fast_armor_key(r->context,
			       ac->remote_subkey,
			       &ticket->ticket.key,
			       &armorkey,
			       &r->armor_crypto);
    krb5_auth_con_free(r->context, ac);
    krb5_free_ticket(r->context, ticket);
    if (ret)
	goto out;

    krb5_free_keyblock_contents(r->context, &armorkey);

    /* verify req-checksum of the outer body */

    ASN1_MALLOC_ENCODE(KDC_REQ_BODY, buf, len, &r->req.req_body, &size, ret);
    if (ret)
	goto out;
    if (size != len) {
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }

    ret = krb5_verify_checksum(r->context, r->armor_crypto,
			       KRB5_KU_FAST_REQ_CHKSUM,
			       buf, len, 
			       &fxreq.u.armored_data.req_checksum);
    if (ret) {
	kdc_log(r->context, r->config, 0,
		"FAST request have a bad checksum");
	goto out;
    }

    ret = krb5_decrypt_EncryptedData(r->context, r->armor_crypto,
				     KRB5_KU_FAST_ENC,
				     &fxreq.u.armored_data.enc_fast_req,
				     &data);
    if (ret) {
	kdc_log(r->context, r->config, 0,
		"Failed to decrypt FAST request");
	goto out;
    }

    ret = decode_KrbFastReq(data.data, data.length, &fastreq, &size);
    if (ret) {
	krb5_data_free(&data);
	goto out;
    }
    if (data.length != size) {
	krb5_data_free(&data);
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }		
    krb5_data_free(&data);

    free_KDC_REQ_BODY(&r->req.req_body);
    ret = copy_KDC_REQ_BODY(&fastreq.req_body, &r->req.req_body);
    if (ret)
	goto out;
	    
    /* check for unsupported mandatory options */
    if (FastOptions2int(fastreq.fast_options) & 0xfffc) {
	kdc_log(r->context, r->config, 0,
		"FAST unsupported mandatory option set");
	ret = KRB5KDC_ERR_PREAUTH_FAILED;
	goto out;
    }

    /* KDC MUST ignore outer pa data preauth-14 - 6.5.5 */
    if (r->req.padata)
	free_METHOD_DATA(r->req.padata);
    else
	ALLOC(r->req.padata);

    ret = copy_METHOD_DATA(&fastreq.padata, r->req.padata);
    if (ret)
	goto out;

    free_KrbFastReq(&fastreq);
    free_PA_FX_FAST_REQUEST(&fxreq);

 out:
    if (armor_server)
	krb5_free_principal(r->context, armor_server);
    if(armor_user)
	_kdc_free_ent(r->context, armor_user);

    return ret;
}
