/* This is a generated file */
#ifndef __kadm5_protos_h__
#define __kadm5_protos_h__
#ifndef DOXY

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

kadm5_ret_t
kadm5_ad_init_with_password (
	const char */*client_name*/,
	const char */*password*/,
	const char */*service_name*/,
	kadm5_config_params */*realm_params*/,
	unsigned long /*struct_version*/,
	unsigned long /*api_version*/,
	void **/*server_handle*/);

kadm5_ret_t
kadm5_ad_init_with_password_ctx (
	krb5_context /*context*/,
	const char */*client_name*/,
	const char */*password*/,
	const char */*service_name*/,
	kadm5_config_params */*realm_params*/,
	unsigned long /*struct_version*/,
	unsigned long /*api_version*/,
	void **/*server_handle*/);

krb5_error_code
kadm5_add_passwd_quality_verifier (
	krb5_context /*context*/,
	const char */*check_library*/);

int
kadm5_all_keys_are_bogus (
	size_t /*n_keys*/,
	krb5_key_data */*keys*/);

const char *
kadm5_check_password_quality (
	krb5_context /*context*/,
	krb5_principal /*principal*/,
	krb5_data */*pwd_data*/);

kadm5_ret_t
kadm5_chpass_principal (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	const char */*password*/);

kadm5_ret_t
kadm5_chpass_principal_3 (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	krb5_boolean /*keepold*/,
	int /*n_ks_tuple*/,
	krb5_key_salt_tuple */*ks_tuple*/,
	const char */*password*/);

kadm5_ret_t
kadm5_chpass_principal_with_key (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	int /*n_key_data*/,
	krb5_key_data */*key_data*/);

kadm5_ret_t
kadm5_chpass_principal_with_key_3 (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	int /*keepold*/,
	int /*n_key_data*/,
	krb5_key_data */*key_data*/);

kadm5_ret_t
kadm5_create_policy (
	void */*server_handle*/,
	kadm5_policy_ent_t /*policy*/,
	long /*mask*/);

kadm5_ret_t
kadm5_create_principal (
	void */*server_handle*/,
	kadm5_principal_ent_t /*princ*/,
	uint32_t /*mask*/,
	const char */*password*/);

kadm5_ret_t
kadm5_create_principal_3 (
	void */*server_handle*/,
	kadm5_principal_ent_t /*princ*/,
	uint32_t /*mask*/,
	int /*n_ks_tuple*/,
	krb5_key_salt_tuple */*ks_tuple*/,
	char */*password*/);

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
kadm5_decrypt_key (
	void */*server_handle*/,
	kadm5_principal_ent_t /*entry*/,
	int32_t /*ktype*/,
	int32_t /*stype*/,
	int32_t /*kvno*/,
	krb5_keyblock */*keyblock*/,
	krb5_keysalt */*keysalt*/,
	int */*kvnop*/);

kadm5_ret_t
kadm5_delete_policy (
	void */*server_handle*/,
	char */*name*/);

kadm5_ret_t
kadm5_delete_principal (
	void */*server_handle*/,
	krb5_principal /*princ*/);

kadm5_ret_t
kadm5_destroy (void */*server_handle*/);

kadm5_ret_t
kadm5_flush (void */*server_handle*/);

void
kadm5_free_key_data (
	void */*server_handle*/,
	int16_t */*n_key_data*/,
	krb5_key_data */*key_data*/);

void
kadm5_free_name_list (
	void */*server_handle*/,
	char **/*names*/,
	int */*count*/);

kadm5_ret_t
kadm5_free_policy_ent (kadm5_policy_ent_t /*ent*/);

void
kadm5_free_principal_ent (
	void */*server_handle*/,
	kadm5_principal_ent_t /*princ*/);

kadm5_ret_t
kadm5_get_policies (
	void */*server_handle*/,
	char */*exp*/,
	char ***/*pols*/,
	int */*count*/);

kadm5_ret_t
kadm5_get_policy (
	void */*server_handle*/,
	char */*policy*/,
	kadm5_policy_ent_t /*ent*/);

kadm5_ret_t
kadm5_get_principal (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	kadm5_principal_ent_t /*out*/,
	uint32_t /*mask*/);

kadm5_ret_t
kadm5_get_principals (
	void */*server_handle*/,
	const char */*expression*/,
	char ***/*princs*/,
	int */*count*/);

kadm5_ret_t
kadm5_get_privs (
	void */*server_handle*/,
	uint32_t */*privs*/);

kadm5_ret_t
kadm5_init_with_creds (
	const char */*client_name*/,
	krb5_ccache /*ccache*/,
	const char */*service_name*/,
	kadm5_config_params */*realm_params*/,
	unsigned long /*struct_version*/,
	unsigned long /*api_version*/,
	void **/*server_handle*/);

kadm5_ret_t
kadm5_init_with_creds_ctx (
	krb5_context /*context*/,
	const char */*client_name*/,
	krb5_ccache /*ccache*/,
	const char */*service_name*/,
	kadm5_config_params */*realm_params*/,
	unsigned long /*struct_version*/,
	unsigned long /*api_version*/,
	void **/*server_handle*/);

kadm5_ret_t
kadm5_init_with_password (
	const char */*client_name*/,
	const char */*password*/,
	const char */*service_name*/,
	kadm5_config_params */*realm_params*/,
	unsigned long /*struct_version*/,
	unsigned long /*api_version*/,
	void **/*server_handle*/);

kadm5_ret_t
kadm5_init_with_password_ctx (
	krb5_context /*context*/,
	const char */*client_name*/,
	const char */*password*/,
	const char */*service_name*/,
	kadm5_config_params */*realm_params*/,
	unsigned long /*struct_version*/,
	unsigned long /*api_version*/,
	void **/*server_handle*/);

kadm5_ret_t
kadm5_init_with_skey (
	const char */*client_name*/,
	const char */*keytab*/,
	const char */*service_name*/,
	kadm5_config_params */*realm_params*/,
	unsigned long /*struct_version*/,
	unsigned long /*api_version*/,
	void **/*server_handle*/);

kadm5_ret_t
kadm5_init_with_skey_ctx (
	krb5_context /*context*/,
	const char */*client_name*/,
	const char */*keytab*/,
	const char */*service_name*/,
	kadm5_config_params */*realm_params*/,
	unsigned long /*struct_version*/,
	unsigned long /*api_version*/,
	void **/*server_handle*/);

kadm5_ret_t
kadm5_lock (void */*server_handle*/);

kadm5_ret_t
kadm5_modify_policy (
	void */*server_handle*/,
	kadm5_policy_ent_t /*policy*/,
	uint32_t /*mask*/);

kadm5_ret_t
kadm5_modify_principal (
	void */*server_handle*/,
	kadm5_principal_ent_t /*princ*/,
	uint32_t /*mask*/);

kadm5_ret_t
kadm5_randkey_principal (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	krb5_keyblock **/*new_keys*/,
	int */*n_keys*/);

kadm5_ret_t
kadm5_randkey_principal_3 (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	krb5_boolean /*keepold*/,
	int /*n_ks_tuple*/,
	krb5_key_salt_tuple */*ks_tuple*/,
	krb5_keyblock **/*new_keys*/,
	int */*n_keys*/);

kadm5_ret_t
kadm5_rename_principal (
	void */*server_handle*/,
	krb5_principal /*source*/,
	krb5_principal /*target*/);

kadm5_ret_t
kadm5_ret_key_data (
	krb5_storage */*sp*/,
	krb5_key_data */*key*/);

kadm5_ret_t
kadm5_ret_principal_ent (
	krb5_storage */*sp*/,
	kadm5_principal_ent_t /*princ*/);

kadm5_ret_t
kadm5_ret_principal_ent_mask (
	krb5_storage */*sp*/,
	kadm5_principal_ent_t /*princ*/,
	uint32_t */*mask*/);

kadm5_ret_t
kadm5_ret_tl_data (
	krb5_storage */*sp*/,
	krb5_tl_data */*tl*/);

/**
 * This function is allows the caller to set new keys for a principal.
 * This is a trivial wrapper around kadm5_setkey_principal_3().
 */

kadm5_ret_t
kadm5_setkey_principal (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	krb5_keyblock */*new_keys*/,
	int /*n_keys*/);

/**
 * This function is allows the caller to set new keys for a principal.
 * This is a simple wrapper around kadm5_get_principal() and
 * kadm5_modify_principal().
 */

kadm5_ret_t
kadm5_setkey_principal_3 (
	void */*server_handle*/,
	krb5_principal /*princ*/,
	krb5_boolean /*keepold*/,
	int /*n_ks_tuple*/,
	krb5_key_salt_tuple */*ks_tuple*/,
	krb5_keyblock */*keyblocks*/,
	int /*n_keys*/);

void
kadm5_setup_passwd_quality_check (
	krb5_context /*context*/,
	const char */*check_library*/,
	const char */*check_function*/);

int
kadm5_some_keys_are_bogus (
	size_t /*n_keys*/,
	krb5_key_data */*keys*/);

kadm5_ret_t
kadm5_store_fake_key_data (
	krb5_storage */*sp*/,
	krb5_key_data */*key*/);

kadm5_ret_t
kadm5_store_key_data (
	krb5_storage */*sp*/,
	krb5_key_data */*key*/);

kadm5_ret_t
kadm5_store_principal_ent (
	krb5_storage */*sp*/,
	kadm5_principal_ent_t /*princ*/);

kadm5_ret_t
kadm5_store_principal_ent_mask (
	krb5_storage */*sp*/,
	kadm5_principal_ent_t /*princ*/,
	uint32_t /*mask*/);

kadm5_ret_t
kadm5_store_principal_ent_nokeys (
	krb5_storage */*sp*/,
	kadm5_principal_ent_t /*princ*/);

kadm5_ret_t
kadm5_store_tl_data (
	krb5_storage */*sp*/,
	krb5_tl_data */*tl*/);

kadm5_ret_t
kadm5_unlock (void */*server_handle*/);

#ifdef __cplusplus
}
#endif

#endif /* DOXY */
#endif /* __kadm5_protos_h__ */
