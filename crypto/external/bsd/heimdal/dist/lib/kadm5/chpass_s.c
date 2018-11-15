/*	$NetBSD: chpass_s.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997-2006 Kungliga Tekniska Högskolan
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

__RCSID("$NetBSD: chpass_s.c,v 1.2 2017/01/28 21:31:49 christos Exp $");

static kadm5_ret_t
change(void *server_handle,
       krb5_principal princ,
       int keepold,
       int n_ks_tuple,
       krb5_key_salt_tuple *ks_tuple,
       const char *password,
       int cond)
{
    kadm5_server_context *context = server_handle;
    hdb_entry_ex ent;
    kadm5_ret_t ret;
    Key *keys;
    size_t num_keys;
    int existsp = 0;

    memset(&ent, 0, sizeof(ent));
    if (!context->keep_open) {
	ret = context->db->hdb_open(context->context, context->db, O_RDWR, 0);
	if(ret)
	    return ret;
    }

    ret = kadm5_log_init(context);
    if (ret)
        goto out;

    ret = context->db->hdb_fetch_kvno(context->context, context->db, princ,
				      HDB_F_DECRYPT|HDB_F_GET_ANY|HDB_F_ADMIN_DATA, 0, &ent);
    if (ret)
	goto out2;

    if (keepold || cond) {
	/*
	 * We save these for now so we can handle password history checking;
	 * we handle keepold further below.
	 */
	ret = hdb_add_current_keys_to_history(context->context, &ent.entry);
	if (ret)
	    goto out3;
    }

    if (context->db->hdb_capability_flags & HDB_CAP_F_HANDLE_PASSWORDS) {
	ret = context->db->hdb_password(context->context, context->db,
					&ent, password, cond);
	if (ret)
	    goto out3;
    } else {

	num_keys = ent.entry.keys.len;
	keys     = ent.entry.keys.val;

	ent.entry.keys.len = 0;
	ent.entry.keys.val = NULL;

	ret = _kadm5_set_keys(context, &ent.entry, n_ks_tuple, ks_tuple,
			      password);
	if(ret) {
	    _kadm5_free_keys(context->context, num_keys, keys);
	    goto out3;
	}
	_kadm5_free_keys(context->context, num_keys, keys);

	if (cond) {
	    HDB_extension *ext;

	    ext = hdb_find_extension(&ent.entry, choice_HDB_extension_data_hist_keys);
	    if (ext != NULL)
		existsp = _kadm5_exists_keys_hist(ent.entry.keys.val,
						  ent.entry.keys.len,
						  &ext->data.u.hist_keys);
	}

	if (existsp) {
	    ret = KADM5_PASS_REUSE;
	    krb5_set_error_message(context->context, ret,
				   "Password reuse forbidden");
	    goto out3;
	}
    }
    ent.entry.kvno++;

    ent.entry.flags.require_pwchange = 0;

    if (!keepold) {
	HDB_extension ext;

	memset(&ext, 0, sizeof (ext));
        ext.mandatory = FALSE;
	ext.data.element = choice_HDB_extension_data_hist_keys;
	ret = hdb_replace_extension(context->context, &ent.entry, &ext);
	if (ret)
	    goto out3;
    }

    ret = hdb_seal_keys(context->context, context->db, &ent.entry);
    if (ret)
        goto out3;

    ret = _kadm5_set_modifier(context, &ent.entry);
    if(ret)
	goto out3;

    ret = _kadm5_bump_pw_expire(context, &ent.entry);
    if (ret)
	goto out3;

    /* This logs the change for iprop and writes to the HDB */
    ret = kadm5_log_modify(context, &ent.entry,
                           KADM5_ATTRIBUTES | KADM5_PRINCIPAL |
                           KADM5_MOD_NAME | KADM5_MOD_TIME |
                           KADM5_KEY_DATA | KADM5_KVNO |
                           KADM5_PW_EXPIRATION | KADM5_TL_DATA);

 out3:
    hdb_free_entry(context->context, &ent);
 out2:
    (void) kadm5_log_end(context);
 out:
    if (!context->keep_open) {
        kadm5_ret_t ret2;
        ret2 = context->db->hdb_close(context->context, context->db);
        if (ret == 0 && ret2 != 0)
            ret = ret2;
    }
    return _kadm5_error_code(ret);
}



/*
 * change the password of `princ' to `password' if it's not already that.
 */

kadm5_ret_t
kadm5_s_chpass_principal_cond(void *server_handle,
			      krb5_principal princ,
			      int keepold,
			      const char *password)
{
    return change (server_handle, princ, keepold, 0, NULL, password, 1);
}

/*
 * change the password of `princ' to `password'
 */

kadm5_ret_t
kadm5_s_chpass_principal(void *server_handle,
			 krb5_principal princ,
			 int keepold,
			 int n_ks_tuple,
			 krb5_key_salt_tuple *ks_tuple,
			 const char *password)
{
    return change (server_handle, princ, keepold,
	n_ks_tuple, ks_tuple, password, 0);
}

/*
 * change keys for `princ' to `keys'
 */

kadm5_ret_t
kadm5_s_chpass_principal_with_key(void *server_handle,
				  krb5_principal princ,
				  int keepold,
				  int n_key_data,
				  krb5_key_data *key_data)
{
    kadm5_server_context *context = server_handle;
    hdb_entry_ex ent;
    kadm5_ret_t ret;

    memset(&ent, 0, sizeof(ent));
    if (!context->keep_open) {
	ret = context->db->hdb_open(context->context, context->db, O_RDWR, 0);
	if(ret)
	    return ret;
    }

    ret = kadm5_log_init(context);
    if (ret)
        goto out;

    ret = context->db->hdb_fetch_kvno(context->context, context->db, princ, 0,
				      HDB_F_GET_ANY|HDB_F_ADMIN_DATA, &ent);
    if (ret == HDB_ERR_NOENTRY)
	goto out2;
    if (keepold) {
	ret = hdb_add_current_keys_to_history(context->context, &ent.entry);
	if (ret)
	    goto out3;
    }
    ret = _kadm5_set_keys2(context, &ent.entry, n_key_data, key_data);
    if (ret)
	goto out3;
    ent.entry.kvno++;
    ret = _kadm5_set_modifier(context, &ent.entry);
    if (ret)
	goto out3;
    ret = _kadm5_bump_pw_expire(context, &ent.entry);
    if (ret)
	goto out3;

    if (keepold) {
	ret = hdb_seal_keys(context->context, context->db, &ent.entry);
	if (ret)
	    goto out3;
    } else {
	HDB_extension ext;

	memset(&ext, 0, sizeof (ext));
	ext.mandatory = FALSE;
	ext.data.element = choice_HDB_extension_data_hist_keys;
	ext.data.u.hist_keys.len = 0;
	ext.data.u.hist_keys.val = NULL;
	hdb_replace_extension(context->context, &ent.entry, &ext);
    }

    /* This logs the change for iprop and writes to the HDB */
    ret = kadm5_log_modify(context, &ent.entry,
                           KADM5_PRINCIPAL | KADM5_MOD_NAME |
                           KADM5_MOD_TIME | KADM5_KEY_DATA | KADM5_KVNO |
                           KADM5_PW_EXPIRATION | KADM5_TL_DATA);

 out3:
    hdb_free_entry(context->context, &ent);
 out2:
    (void) kadm5_log_end(context);
 out:
    if (!context->keep_open) {
        kadm5_ret_t ret2;
        ret2 = context->db->hdb_close(context->context, context->db);
        if (ret == 0 && ret2 != 0)
            ret = ret2;
    }
    return _kadm5_error_code(ret);
}
