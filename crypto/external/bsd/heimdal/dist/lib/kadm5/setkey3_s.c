/*	$NetBSD: setkey3_s.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997-2001, 2003, 2005-2006 Kungliga Tekniska Högskolan
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

/**
 * Server-side function to set new keys for a principal.
 */
kadm5_ret_t
kadm5_s_setkey_principal_3(void *server_handle,
			   krb5_principal princ,
			   krb5_boolean keepold,
			   int n_ks_tuple,
			   krb5_key_salt_tuple *ks_tuple,
			   krb5_keyblock *keyblocks, int n_keys)
{
    kadm5_server_context *context = server_handle;
    hdb_entry_ex ent;
    kadm5_ret_t ret = 0;

    memset(&ent, 0, sizeof(ent));
    if (!context->keep_open)
	ret = context->db->hdb_open(context->context, context->db, O_RDWR, 0);
    if (ret)
	return ret;

    ret = kadm5_log_init(context);
    if (ret) {
        if (!context->keep_open)
            context->db->hdb_close(context->context, context->db);
        return ret;
    }

    ret = context->db->hdb_fetch_kvno(context->context, context->db, princ,
				      HDB_F_GET_ANY|HDB_F_ADMIN_DATA, 0, &ent);
    if (ret) {
        (void) kadm5_log_end(context);
        if (!context->keep_open)
            context->db->hdb_close(context->context, context->db);
        return ret;
    }

    if (keepold) {
        ret = hdb_add_current_keys_to_history(context->context, &ent.entry);
    } else
	ret = hdb_clear_extension(context->context, &ent.entry,
				  choice_HDB_extension_data_hist_keys);

    /*
     * Though in practice all real calls to this function will pass an empty
     * ks_tuple, and cannot in any case employ any salts that require
     * additional data, we go the extra mile to set any requested salt type
     * along with a zero length salt value.  While we're at it we check that
     * each ks_tuple's enctype matches the corresponding key enctype.
     */
    if (ret == 0) {
	int i;

	free_Keys(&ent.entry.keys);
	for (i = 0; i < n_keys; ++i) {
	    Key k;
	    Salt s;

	    k.mkvno = 0;
	    k.key = keyblocks[i];
	    if (n_ks_tuple == 0)
		k.salt = 0;
	    else {
		if (ks_tuple[i].ks_enctype != keyblocks[i].keytype) {
		    ret = KADM5_SETKEY3_ETYPE_MISMATCH;
		    break;
		}
		s.type = ks_tuple[i].ks_salttype;
		s.salt.data = 0;
		s.opaque = 0;
		k.salt = &s;
	    }
	    if ((ret = add_Keys(&ent.entry.keys, &k)) != 0)
		break;
	}
    }

    if (ret == 0) {
	ent.entry.kvno++;
	ent.entry.flags.require_pwchange = 0;
	hdb_entry_set_pw_change_time(context->context, &ent.entry, 0);
	hdb_entry_clear_password(context->context, &ent.entry);

	if ((ret = hdb_seal_keys(context->context, context->db,
				 &ent.entry)) == 0
	    && (ret = _kadm5_set_modifier(context, &ent.entry)) == 0
	    && (ret = _kadm5_bump_pw_expire(context, &ent.entry)) == 0)
	    ret = kadm5_log_modify(context, &ent.entry,
                                   KADM5_ATTRIBUTES | KADM5_PRINCIPAL |
                                   KADM5_MOD_NAME | KADM5_MOD_TIME |
                                   KADM5_KEY_DATA | KADM5_KVNO |
                                   KADM5_PW_EXPIRATION | KADM5_TL_DATA);
    }

    hdb_free_entry(context->context, &ent);
    (void) kadm5_log_end(context);
    if (!context->keep_open)
	context->db->hdb_close(context->context, context->db);
    return _kadm5_error_code(ret);
}
