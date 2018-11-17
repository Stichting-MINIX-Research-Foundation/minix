/* This is a generated file */
#ifndef __krb5_private_h__
#define __krb5_private_h__

#include <stdarg.h>

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

#ifndef KRB5_DEPRECATED_FUNCTION
#ifndef __has_extension
#define __has_extension(x) 0
#define KRB5_DEPRECATED_FUNCTIONhas_extension 1
#endif
#if __has_extension(attribute_deprecated_with_message)
#define KRB5_DEPRECATED_FUNCTION(x) __attribute__((__deprecated__(x)))
#elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1 )))
#define KRB5_DEPRECATED_FUNCTION(X) __attribute__((__deprecated__))
#else
#define KRB5_DEPRECATED_FUNCTION(X)
#endif
#ifdef KRB5_DEPRECATED_FUNCTIONhas_extension
#undef __has_extension
#undef KRB5_DEPRECATED_FUNCTIONhas_extension
#endif
#endif /* KRB5_DEPRECATED_FUNCTION */


KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_heim_krb5_ipc_client_clear_target (void);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_heim_krb5_ipc_client_set_target_uid (uid_t /*uid*/);

void
_krb5_DES3_random_to_key (
	krb5_context /*context*/,
	krb5_keyblock */*key*/,
	const void */*data*/,
	size_t /*size*/);

krb5_error_code
_krb5_HMAC_MD5_checksum (
	krb5_context /*context*/,
	struct _krb5_key_data */*key*/,
	const void */*data*/,
	size_t /*len*/,
	unsigned /*usage*/,
	Checksum */*result*/);

krb5_error_code
_krb5_SP800_108_HMAC_KDF (
	krb5_context /*context*/,
	const krb5_data */*kdf_K1*/,
	const krb5_data */*kdf_label*/,
	const krb5_data */*kdf_context*/,
	const EVP_MD */*md*/,
	krb5_data */*kdf_K0*/);

krb5_error_code
_krb5_SP_HMAC_SHA1_checksum (
	krb5_context /*context*/,
	struct _krb5_key_data */*key*/,
	const void */*data*/,
	size_t /*len*/,
	unsigned /*usage*/,
	Checksum */*result*/);

krb5_error_code
_krb5_aes_sha2_md_for_enctype (
	krb5_context /*context*/,
	krb5_enctype /*enctype*/,
	const EVP_MD **/*md*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_build_authenticator (
	krb5_context /*context*/,
	krb5_auth_context /*auth_context*/,
	krb5_enctype /*enctype*/,
	krb5_creds */*cred*/,
	Checksum */*cksum*/,
	krb5_data */*result*/,
	krb5_key_usage /*usage*/);

krb5_error_code
_krb5_build_authpack_subjectPK_EC (
	krb5_context /*context*/,
	krb5_pk_init_ctx /*ctx*/,
	AuthPack */*a*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_cc_allocate (
	krb5_context /*context*/,
	const krb5_cc_ops */*ops*/,
	krb5_ccache */*id*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_config_copy (
	krb5_context /*context*/,
	krb5_config_section */*c*/,
	krb5_config_section **/*head*/);

KRB5_LIB_FUNCTION const void * KRB5_LIB_CALL
_krb5_config_get (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	int /*type*/,
	...);

KRB5_LIB_FUNCTION krb5_config_section * KRB5_LIB_CALL
_krb5_config_get_entry (
	krb5_config_section **/*parent*/,
	const char */*name*/,
	int /*type*/);

KRB5_LIB_FUNCTION const void * KRB5_LIB_CALL
_krb5_config_get_next (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	const krb5_config_binding **/*pointer*/,
	int /*type*/,
	...);

KRB5_LIB_FUNCTION const void * KRB5_LIB_CALL
_krb5_config_vget (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	int /*type*/,
	va_list /*args*/);

KRB5_LIB_FUNCTION const void * KRB5_LIB_CALL
_krb5_config_vget_next (
	krb5_context /*context*/,
	const krb5_config_section */*c*/,
	const krb5_config_binding **/*pointer*/,
	int /*type*/,
	va_list /*args*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_copy_send_to_kdc_func (
	krb5_context /*context*/,
	krb5_context /*to*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_crc_init_table (void);

KRB5_LIB_FUNCTION uint32_t KRB5_LIB_CALL
_krb5_crc_update (
	const char */*p*/,
	size_t /*len*/,
	uint32_t /*res*/);

void KRB5_LIB_FUNCTION
_krb5_debug (
	krb5_context /*context*/,
	int /*level*/,
	const char */*fmt*/,
	...)
     __attribute__ ((__format__ (__printf__, 3, 4)));

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_debug_backtrace (krb5_context /*context*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_derive_key (
	krb5_context /*context*/,
	struct _krb5_encryption_type */*et*/,
	struct _krb5_key_data */*key*/,
	const void */*constant*/,
	size_t /*len*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_des_checksum (
	krb5_context /*context*/,
	const EVP_MD */*evp_md*/,
	struct _krb5_key_data */*key*/,
	const void */*data*/,
	size_t /*len*/,
	Checksum */*cksum*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_des_verify (
	krb5_context /*context*/,
	const EVP_MD */*evp_md*/,
	struct _krb5_key_data */*key*/,
	const void */*data*/,
	size_t /*len*/,
	Checksum */*C*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_dh_group_ok (
	krb5_context /*context*/,
	unsigned long /*bits*/,
	heim_integer */*p*/,
	heim_integer */*g*/,
	heim_integer */*q*/,
	struct krb5_dh_moduli **/*moduli*/,
	char **/*name*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_einval (
	krb5_context /*context*/,
	const char */*func*/,
	unsigned long /*argn*/);

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
_krb5_enctype_requires_random_salt (
	krb5_context /*context*/,
	krb5_enctype /*enctype*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_erase_file (
	krb5_context /*context*/,
	const char */*filename*/);

void
_krb5_evp_cleanup (
	krb5_context /*context*/,
	struct _krb5_key_data */*kd*/);

krb5_error_code
_krb5_evp_encrypt (
	krb5_context /*context*/,
	struct _krb5_key_data */*key*/,
	void */*data*/,
	size_t /*len*/,
	krb5_boolean /*encryptp*/,
	int /*usage*/,
	void */*ivec*/);

krb5_error_code
_krb5_evp_encrypt_cts (
	krb5_context /*context*/,
	struct _krb5_key_data */*key*/,
	void */*data*/,
	size_t /*len*/,
	krb5_boolean /*encryptp*/,
	int /*usage*/,
	void */*ivec*/);

void
_krb5_evp_schedule (
	krb5_context /*context*/,
	struct _krb5_key_type */*kt*/,
	struct _krb5_key_data */*kd*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_expand_default_cc_name (
	krb5_context /*context*/,
	const char */*str*/,
	char **/*res*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_expand_path_tokens (
	krb5_context /*context*/,
	const char */*path_in*/,
	int /*filepath*/,
	char **/*ppath_out*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_expand_path_tokensv (
	krb5_context /*context*/,
	const char */*path_in*/,
	int /*filepath*/,
	char **/*ppath_out*/,
	...);

KRB5_LIB_FUNCTION int KRB5_LIB_CALL
_krb5_extract_ticket (
	krb5_context /*context*/,
	krb5_kdc_rep */*rep*/,
	krb5_creds */*creds*/,
	krb5_keyblock */*key*/,
	krb5_const_pointer /*keyseed*/,
	krb5_key_usage /*key_usage*/,
	krb5_addresses */*addrs*/,
	unsigned /*nonce*/,
	unsigned /*flags*/,
	krb5_data */*request*/,
	krb5_decrypt_proc /*decrypt_proc*/,
	krb5_const_pointer /*decryptarg*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_fast_armor_key (
	krb5_context /*context*/,
	krb5_keyblock */*subkey*/,
	krb5_keyblock */*sessionkey*/,
	krb5_keyblock */*armorkey*/,
	krb5_crypto */*armor_crypto*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_fast_cf2 (
	krb5_context /*context*/,
	krb5_keyblock */*key1*/,
	const char */*pepper1*/,
	krb5_keyblock */*key2*/,
	const char */*pepper2*/,
	krb5_keyblock */*armorkey*/,
	krb5_crypto */*armor_crypto*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_find_capath (
	krb5_context /*context*/,
	const char */*client_realm*/,
	const char */*local_realm*/,
	const char */*server_realm*/,
	krb5_boolean /*use_hierarchical*/,
	char ***/*rpath*/,
	size_t */*npath*/);

KRB5_LIB_FUNCTION struct _krb5_checksum_type * KRB5_LIB_CALL
_krb5_find_checksum (krb5_cksumtype /*type*/);

KRB5_LIB_FUNCTION struct _krb5_encryption_type * KRB5_LIB_CALL
_krb5_find_enctype (krb5_enctype /*type*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_free_capath (
	krb5_context /*context*/,
	char **/*capath*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_free_key_data (
	krb5_context /*context*/,
	struct _krb5_key_data */*key*/,
	struct _krb5_encryption_type */*et*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_free_krbhst_info (krb5_krbhst_info */*hi*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_free_moduli (struct krb5_dh_moduli **/*moduli*/);

KRB5_LIB_FUNCTION void
_krb5_free_name_canon_rules (
	krb5_context /*context*/,
	krb5_name_canon_rule /*rules*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_get_ad (
	krb5_context /*context*/,
	const AuthorizationData */*ad*/,
	krb5_keyblock */*sessionkey*/,
	int /*type*/,
	krb5_data */*data*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_get_cred_kdc_any (
	krb5_context /*context*/,
	krb5_kdc_flags /*flags*/,
	krb5_ccache /*ccache*/,
	krb5_creds */*in_creds*/,
	krb5_principal /*impersonate_principal*/,
	Ticket */*second_ticket*/,
	krb5_creds **/*out_creds*/,
	krb5_creds ***/*ret_tgts*/);

KRB5_LIB_FUNCTION char * KRB5_LIB_CALL
_krb5_get_default_cc_name_from_registry (krb5_context /*context*/);

KRB5_LIB_FUNCTION char * KRB5_LIB_CALL
_krb5_get_default_config_config_files_from_registry (void);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_get_default_principal_local (
	krb5_context /*context*/,
	krb5_principal */*princ*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_get_host_realm_int (
	krb5_context /*context*/,
	const char */*host*/,
	krb5_boolean /*use_dns*/,
	krb5_realm **/*realms*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_get_init_creds_opt_free_pkinit (krb5_get_init_creds_opt */*opt*/);

KRB5_LIB_FUNCTION krb5_ssize_t KRB5_LIB_CALL
_krb5_get_int (
	void */*buffer*/,
	unsigned long */*value*/,
	size_t /*size*/);

KRB5_LIB_FUNCTION krb5_ssize_t KRB5_LIB_CALL
_krb5_get_int64 (
	void */*buffer*/,
	uint64_t */*value*/,
	size_t /*size*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_get_krbtgt (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_realm /*realm*/,
	krb5_creds **/*cred*/);

KRB5_LIB_FUNCTION krb5_error_code
_krb5_get_name_canon_rules (
	krb5_context /*context*/,
	krb5_name_canon_rule */*rules*/);

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
_krb5_have_debug (
	krb5_context /*context*/,
	int /*level*/);

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
_krb5_homedir_access (krb5_context /*context*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_init_etype (
	krb5_context /*context*/,
	krb5_pdu /*pdu_type*/,
	unsigned */*len*/,
	krb5_enctype **/*val*/,
	const krb5_enctype */*etypes*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_internal_hmac (
	krb5_context /*context*/,
	struct _krb5_checksum_type */*cm*/,
	const void */*data*/,
	size_t /*len*/,
	unsigned /*usage*/,
	struct _krb5_key_data */*keyblock*/,
	Checksum */*result*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_kcm_get_initial_ticket (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_principal /*server*/,
	krb5_keyblock */*key*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_kcm_get_ticket (
	krb5_context /*context*/,
	krb5_ccache /*id*/,
	krb5_kdc_flags /*flags*/,
	krb5_enctype /*enctype*/,
	krb5_principal /*server*/);

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
_krb5_kcm_is_running (krb5_context /*context*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_kcm_noop (
	krb5_context /*context*/,
	krb5_ccache /*id*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_kdc_retry (
	krb5_context /*context*/,
	krb5_sendto_ctx /*ctx*/,
	void */*data*/,
	const krb5_data */*reply*/,
	int */*action*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_krbhost_info_move (
	krb5_context /*context*/,
	krb5_krbhst_info */*from*/,
	krb5_krbhst_info **/*to*/);

KRB5_LIB_FUNCTION const char * KRB5_LIB_CALL
_krb5_krbhst_get_realm (krb5_krbhst_handle /*handle*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_kt_principal_not_found (
	krb5_context /*context*/,
	krb5_error_code /*ret*/,
	krb5_keytab /*id*/,
	krb5_const_principal /*principal*/,
	krb5_enctype /*enctype*/,
	int /*kvno*/);

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
_krb5_kuserok (
	krb5_context /*context*/,
	krb5_principal /*principal*/,
	const char */*luser*/,
	krb5_boolean /*an2ln_ok*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_load_ccache_plugins (krb5_context /*context*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_load_db_plugins (krb5_context /*context*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_load_plugins (
	krb5_context /*context*/,
	const char */*name*/,
	const char **/*paths*/);

krb5_error_code
_krb5_make_fast_ap_fxarmor (
	krb5_context /*context*/,
	krb5_ccache /*armor_ccache*/,
	krb5_data */*armor_value*/,
	krb5_keyblock */*armor_key*/,
	krb5_crypto */*armor_crypto*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_mk_req_internal (
	krb5_context /*context*/,
	krb5_auth_context */*auth_context*/,
	const krb5_flags /*ap_req_options*/,
	krb5_data */*in_data*/,
	krb5_creds */*in_creds*/,
	krb5_data */*outbuf*/,
	krb5_key_usage /*checksum_usage*/,
	krb5_key_usage /*encrypt_usage*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_n_fold (
	const void */*str*/,
	size_t /*len*/,
	void */*key*/,
	size_t /*size*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_pac_sign (
	krb5_context /*context*/,
	krb5_pac /*p*/,
	time_t /*authtime*/,
	krb5_principal /*principal*/,
	const krb5_keyblock */*server_key*/,
	const krb5_keyblock */*priv_key*/,
	krb5_data */*data*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_parse_moduli (
	krb5_context /*context*/,
	const char */*file*/,
	struct krb5_dh_moduli ***/*moduli*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_parse_moduli_line (
	krb5_context /*context*/,
	const char */*file*/,
	int /*lineno*/,
	char */*p*/,
	struct krb5_dh_moduli **/*m*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_pk_cert_free (struct krb5_pk_cert */*cert*/);

void
_krb5_pk_eckey_free (void */*eckey*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_pk_kdf (
	krb5_context /*context*/,
	const struct AlgorithmIdentifier */*ai*/,
	const void */*dhdata*/,
	size_t /*dhsize*/,
	krb5_const_principal /*client*/,
	krb5_const_principal /*server*/,
	krb5_enctype /*enctype*/,
	const krb5_data */*as_req*/,
	const krb5_data */*pk_as_rep*/,
	const Ticket */*ticket*/,
	krb5_keyblock */*key*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_pk_load_id (
	krb5_context /*context*/,
	struct krb5_pk_identity **/*ret_id*/,
	const char */*user_id*/,
	const char */*anchor_id*/,
	char * const */*chain_list*/,
	char * const */*revoke_list*/,
	krb5_prompter_fct /*prompter*/,
	void */*prompter_data*/,
	char */*password*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_pk_mk_ContentInfo (
	krb5_context /*context*/,
	const krb5_data */*buf*/,
	const heim_oid */*oid*/,
	struct ContentInfo */*content_info*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_pk_mk_padata (
	krb5_context /*context*/,
	void */*c*/,
	int /*ic_flags*/,
	int /*win2k*/,
	const KDC_REQ_BODY */*req_body*/,
	unsigned /*nonce*/,
	METHOD_DATA */*md*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_pk_octetstring2key (
	krb5_context /*context*/,
	krb5_enctype /*type*/,
	const void */*dhdata*/,
	size_t /*dhsize*/,
	const heim_octet_string */*c_n*/,
	const heim_octet_string */*k_n*/,
	krb5_keyblock */*key*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_pk_rd_pa_reply (
	krb5_context /*context*/,
	const char */*realm*/,
	void */*c*/,
	krb5_enctype /*etype*/,
	const krb5_krbhst_info */*hi*/,
	unsigned /*nonce*/,
	const krb5_data */*req_buffer*/,
	PA_DATA */*pa*/,
	krb5_keyblock **/*key*/);

krb5_error_code
_krb5_pk_rd_pa_reply_ecdh_compute_key (
	krb5_context /*context*/,
	krb5_pk_init_ctx /*ctx*/,
	const unsigned char */*in*/,
	size_t /*in_sz*/,
	unsigned char **/*out*/,
	int */*out_sz*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_plugin_find (
	krb5_context /*context*/,
	enum krb5_plugin_type /*type*/,
	const char */*name*/,
	struct krb5_plugin **/*list*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_plugin_free (struct krb5_plugin */*list*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_plugin_run_f (
	krb5_context /*context*/,
	const char */*module*/,
	const char */*name*/,
	int /*min_version*/,
	int /*flags*/,
	void */*userctx*/,
	krb5_error_code (KRB5_LIB_CALL *func)(krb5_context, const void *, void *, void *));

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_principal2principalname (
	PrincipalName */*p*/,
	const krb5_principal /*from*/);

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
_krb5_principal_compare_PrincipalName (
	krb5_context /*context*/,
	krb5_const_principal /*princ1*/,
	PrincipalName */*princ2*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_principalname2krb5_principal (
	krb5_context /*context*/,
	krb5_principal */*principal*/,
	const PrincipalName /*from*/,
	const Realm /*realm*/);

KRB5_LIB_FUNCTION krb5_ssize_t KRB5_LIB_CALL
_krb5_put_int (
	void */*buffer*/,
	uint64_t /*value*/,
	size_t /*size*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_s4u2self_to_checksumdata (
	krb5_context /*context*/,
	const PA_S4U2Self */*self*/,
	krb5_data */*data*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_sendto_ctx_set_krb5hst (
	krb5_context /*context*/,
	krb5_sendto_ctx /*ctx*/,
	krb5_krbhst_handle /*handle*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_sendto_ctx_set_prexmit (
	krb5_sendto_ctx /*ctx*/,
	krb5_sendto_prexmit /*prexmit*/,
	void */*data*/);

KRB5_LIB_FUNCTION int KRB5_LIB_CALL
_krb5_set_default_cc_name_to_registry (
	krb5_context /*context*/,
	krb5_ccache /*id*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_unload_plugins (
	krb5_context /*context*/,
	const char */*name*/);

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_usage2arcfour (
	krb5_context /*context*/,
	unsigned */*usage*/);

KRB5_LIB_FUNCTION int KRB5_LIB_CALL
_krb5_xlock (
	krb5_context /*context*/,
	int /*fd*/,
	krb5_boolean /*exclusive*/,
	const char */*filename*/);

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_xor8 (
	unsigned char */*a*/,
	const unsigned char */*b*/);

KRB5_LIB_FUNCTION int KRB5_LIB_CALL
_krb5_xunlock (
	krb5_context /*context*/,
	int /*fd*/);

#undef KRB5_DEPRECATED_FUNCTION
#define KRB5_DEPRECATED_FUNCTION(X)

#endif /* __krb5_private_h__ */
