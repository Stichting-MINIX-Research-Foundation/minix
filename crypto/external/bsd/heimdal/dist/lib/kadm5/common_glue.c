/*	$NetBSD: common_glue.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

__RCSID("$NetBSD: common_glue.c,v 1.2 2017/01/28 21:31:49 christos Exp $");

#define __CALL(F, P) (*((kadm5_common_context*)server_handle)->funcs.F)P
#define __CALLABLE(F) (((kadm5_common_context*)server_handle)->funcs.F != 0)

kadm5_ret_t
kadm5_chpass_principal(void *server_handle,
		       krb5_principal princ,
		       const char *password)
{
    return __CALL(chpass_principal, (server_handle, princ, 0,
		  0, NULL, password));
}

kadm5_ret_t
kadm5_chpass_principal_3(void *server_handle,
		         krb5_principal princ,
		         krb5_boolean keepold,
		         int n_ks_tuple,
		         krb5_key_salt_tuple *ks_tuple,
		         const char *password)
{
    return __CALL(chpass_principal, (server_handle, princ, keepold,
		  n_ks_tuple, ks_tuple, password));
}

kadm5_ret_t
kadm5_chpass_principal_with_key(void *server_handle,
				krb5_principal princ,
				int n_key_data,
				krb5_key_data *key_data)
{
    return __CALL(chpass_principal_with_key,
		  (server_handle, princ, 0, n_key_data, key_data));
}

kadm5_ret_t
kadm5_chpass_principal_with_key_3(void *server_handle,
				  krb5_principal princ,
				  int keepold,
				  int n_key_data,
				  krb5_key_data *key_data)
{
    return __CALL(chpass_principal_with_key,
		  (server_handle, princ, keepold, n_key_data, key_data));
}

kadm5_ret_t
kadm5_create_principal_3(void *server_handle,
			 kadm5_principal_ent_t princ,
			 uint32_t mask,
			 int n_ks_tuple,
			 krb5_key_salt_tuple *ks_tuple,
			 char *password)
{
    return __CALL(create_principal,
		  (server_handle, princ, mask, n_ks_tuple, ks_tuple, password));
}

kadm5_ret_t
kadm5_create_principal(void *server_handle,
		       kadm5_principal_ent_t princ,
		       uint32_t mask,
		       const char *password)
{
    return __CALL(create_principal,
		  (server_handle, princ, mask, 0, NULL, password));
}

kadm5_ret_t
kadm5_delete_principal(void *server_handle,
		       krb5_principal princ)
{
    return __CALL(delete_principal, (server_handle, princ));
}

kadm5_ret_t
kadm5_destroy (void *server_handle)
{
    return __CALL(destroy, (server_handle));
}

kadm5_ret_t
kadm5_flush (void *server_handle)
{
    return __CALL(flush, (server_handle));
}

kadm5_ret_t
kadm5_get_principal(void *server_handle,
		    krb5_principal princ,
		    kadm5_principal_ent_t out,
		    uint32_t mask)
{
    return __CALL(get_principal, (server_handle, princ, out, mask));
}

/**
 * Extract decrypted keys from kadm5_principal_ent_t object.  Mostly a
 * no-op for Heimdal because we fetch the entry with decrypted keys.
 * Sadly this is not fully a no-op, as we have to allocate a copy.
 *
 * @server_handle is the kadm5 handle
 * @entry is the HDB entry for the principal in question
 * @ktype is the enctype to get a key for, or -1 to get the first one
 * @stype is the salttype to get a key for, or -1 to get the first match
 * @kvno is the kvno to search for, or -1 to get the first match (highest kvno)
 * @keyblock is where the key will be placed
 * @keysalt, if not NULL, is where the salt will be placed
 * @kvnop, if not NULL, is where the selected kvno will be placed
 */
kadm5_ret_t
kadm5_decrypt_key(void *server_handle,
                  kadm5_principal_ent_t entry,
		  int32_t ktype, int32_t stype,
		  int32_t kvno, krb5_keyblock *keyblock,
                  krb5_keysalt *keysalt, int *kvnop)
{
    size_t i;

    if (kvno < 1 || stype != -1)
	return KADM5_DECRYPT_USAGE_NOSUPP;

    for (i = 0; i < entry->n_key_data; i++) {
	if (ktype != entry->key_data[i].key_data_kvno)
	    continue;

	keyblock->keytype = ktype;
	keyblock->keyvalue.length = entry->key_data[i].key_data_length[0];
	keyblock->keyvalue.data = malloc(keyblock->keyvalue.length);
	if (keyblock->keyvalue.data == NULL)
	    return ENOMEM;
	memcpy(keyblock->keyvalue.data,
	       entry->key_data[i].key_data_contents[0],
	       keyblock->keyvalue.length);
    }

    return 0;
}

kadm5_ret_t
kadm5_modify_principal(void *server_handle,
		       kadm5_principal_ent_t princ,
		       uint32_t mask)
{
    return __CALL(modify_principal, (server_handle, princ, mask));
}

kadm5_ret_t
kadm5_randkey_principal(void *server_handle,
			krb5_principal princ,
			krb5_keyblock **new_keys,
			int *n_keys)
{
    return __CALL(randkey_principal, (server_handle, princ, FALSE, 0, NULL,
		  new_keys, n_keys));
}

kadm5_ret_t
kadm5_randkey_principal_3(void *server_handle,
			  krb5_principal princ,
			  krb5_boolean keepold,
			  int n_ks_tuple,
			  krb5_key_salt_tuple *ks_tuple,
			  krb5_keyblock **new_keys,
			  int *n_keys)
{
    return __CALL(randkey_principal, (server_handle, princ, keepold,
				      n_ks_tuple, ks_tuple, new_keys, n_keys));
}

kadm5_ret_t
kadm5_rename_principal(void *server_handle,
		       krb5_principal source,
		       krb5_principal target)
{
    return __CALL(rename_principal, (server_handle, source, target));
}

kadm5_ret_t
kadm5_get_principals(void *server_handle,
		     const char *expression,
		     char ***princs,
		     int *count)
{
    return __CALL(get_principals, (server_handle, expression, princs, count));
}

kadm5_ret_t
kadm5_get_privs(void *server_handle,
		uint32_t *privs)
{
    return __CALL(get_privs, (server_handle, privs));
}


/**
 * This function is allows the caller to set new keys for a principal.
 * This is a trivial wrapper around kadm5_setkey_principal_3().
 */
kadm5_ret_t
kadm5_setkey_principal(void *server_handle,
                       krb5_principal princ,
                       krb5_keyblock *new_keys,
                       int n_keys)
{
    return kadm5_setkey_principal_3(server_handle, princ, 0, 0, NULL,
				    new_keys, n_keys);
}

/**
 * This function is allows the caller to set new keys for a principal.
 * This is a simple wrapper around kadm5_get_principal() and
 * kadm5_modify_principal().
 */
kadm5_ret_t
kadm5_setkey_principal_3(void *server_handle,
                         krb5_principal princ,
                         krb5_boolean keepold,
                         int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                         krb5_keyblock *keyblocks,
                         int n_keys)
{
    kadm5_principal_ent_rec princ_ent;
    kadm5_ret_t ret;
    krb5_key_data *new_key_data = NULL;
    size_t i;

    if (n_keys < 1)
	return EINVAL;
    if (n_ks_tuple > 0 && n_ks_tuple != n_keys)
	return KADM5_SETKEY3_ETYPE_MISMATCH;

    /*
     * If setkey_principal_3 is defined in the server handle, use that.
     */
    if (__CALLABLE(setkey_principal_3))
	return __CALL(setkey_principal_3,
		      (server_handle, princ, keepold, n_ks_tuple, ks_tuple,
		       keyblocks, n_keys));

    /*
     * Otherwise, simulate it via a get, update, modify sequence.
     */
    ret = kadm5_get_principal(server_handle, princ, &princ_ent,
                              KADM5_KVNO | KADM5_PRINCIPAL | KADM5_KEY_DATA);
    if (ret)
	return ret;

    if (keepold) {
        new_key_data = calloc((n_keys + princ_ent.n_key_data),
                              sizeof(*new_key_data));
	if (new_key_data == NULL) {
	    ret = ENOMEM;
	    goto out;
	}

	memcpy(&new_key_data[n_keys], &princ_ent.key_data[0],
		princ_ent.n_key_data * sizeof (princ_ent.key_data[0]));
    } else {
	new_key_data = calloc(n_keys, sizeof(*new_key_data));
	if (new_key_data == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
    }

    princ_ent.kvno++;
    for (i = 0; i < n_keys; i++) {
	new_key_data[i].key_data_ver = 2;

	/* Key */
	new_key_data[i].key_data_kvno = princ_ent.kvno;
	new_key_data[i].key_data_type[0] = keyblocks[i].keytype;
	new_key_data[i].key_data_length[0] = keyblocks[i].keyvalue.length;
	new_key_data[i].key_data_contents[0] =
	    malloc(keyblocks[i].keyvalue.length);
	if (new_key_data[i].key_data_contents[0] == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	memcpy(new_key_data[i].key_data_contents[0],
	       keyblocks[i].keyvalue.data,
	       keyblocks[i].keyvalue.length);

	/*
	 * Salt (but there's no salt, just salttype, which is kinda
	 * silly -- what's the point of setkey_3() then, besides
	 * keepold?!)
	 */
	new_key_data[i].key_data_type[1] = 0;
	if (n_ks_tuple > 0) {
	    if (ks_tuple[i].ks_enctype != keyblocks[i].keytype) {
		ret = KADM5_SETKEY3_ETYPE_MISMATCH;
                goto out;
            }
	    new_key_data[i].key_data_type[1] = ks_tuple[i].ks_salttype;
	}
	new_key_data[i].key_data_length[1] = 0;
	new_key_data[i].key_data_contents[1] = NULL;
    }

    /* Free old keys */
    if (!keepold) {
	for (i = 0; i < princ_ent.n_key_data; i++) {
	    free(princ_ent.key_data[i].key_data_contents[0]);
	    free(princ_ent.key_data[i].key_data_contents[1]);
	}
    }
    free(princ_ent.key_data);
    princ_ent.key_data = new_key_data;
    princ_ent.n_key_data = n_keys + (keepold ? princ_ent.n_key_data : 0);
    new_key_data = NULL;

    /* Modify the principal */
    ret = kadm5_modify_principal(server_handle, &princ_ent, KADM5_KVNO | KADM5_KEY_DATA);

out:
    if (new_key_data != NULL) {
	for (i = 0; i < n_keys; i++) {
	    free(new_key_data[i].key_data_contents[0]);
	    free(new_key_data[i].key_data_contents[1]);
	}
	free(new_key_data);
    }
    kadm5_free_principal_ent(server_handle, &princ_ent);
    return ret;
}


kadm5_ret_t
kadm5_lock(void *server_handle)
{
    return __CALL(lock, (server_handle));
}

kadm5_ret_t
kadm5_unlock(void *server_handle)
{
    return __CALL(unlock, (server_handle));
}


kadm5_ret_t
kadm5_create_policy(void *server_handle,
	                         kadm5_policy_ent_t policy, long mask)
{
    return KADM5_POLICY_OP_NOSUPP;
}

kadm5_ret_t
kadm5_delete_policy(void *server_handle, char *name)
{
    return KADM5_POLICY_OP_NOSUPP;
}


kadm5_ret_t
kadm5_modify_policy(void *server_handle, kadm5_policy_ent_t policy,
		    uint32_t mask)
{
    return KADM5_POLICY_OP_NOSUPP;
}

kadm5_ret_t
kadm5_get_policy(void *server_handle, char *policy, kadm5_policy_ent_t ent)
{
    memset(ent, 0, sizeof (*ent));
    return KADM5_POLICY_OP_NOSUPP;
}


kadm5_ret_t
kadm5_get_policies(void *server_handle, char *exp, char ***pols, int *count)
{
    *count = 0;
    *pols = NULL;

    return KADM5_POLICY_OP_NOSUPP;
}

kadm5_ret_t
kadm5_free_policy_ent(kadm5_policy_ent_t ent)
{
    if (ent->policy)
	free(ent->policy);
    /*
     * Not clear if we should free ent or not.  It might be an automatic
     * struct, so we don't free it for now, just in case.
     */
    return 0;
}

