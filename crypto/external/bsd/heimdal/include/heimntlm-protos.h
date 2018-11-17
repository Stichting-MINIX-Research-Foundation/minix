/* This is a generated file */
#ifndef __heimntlm_protos_h__
#define __heimntlm_protos_h__
#ifndef DOXY

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generates an NTLMv1 session random with assosited session master key.
 *
 * @param key the ntlm v1 key
 * @param len length of key
 * @param session generated session nonce, should be freed with heim_ntlm_free_buf().
 * @param master calculated session master key, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_build_ntlm1_master (
	void */*key*/,
	size_t /*len*/,
	struct ntlm_buf */*session*/,
	struct ntlm_buf */*master*/);

/**
 * Generates an NTLMv2 session random with associated session master key.
 *
 * @param key the NTLMv2 key
 * @param len length of key
 * @param blob the NTLMv2 "blob"
 * @param session generated session nonce, should be freed with heim_ntlm_free_buf().
 * @param master calculated session master key, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_build_ntlm2_master (
	void */*key*/,
	size_t /*len*/,
	struct ntlm_buf */*blob*/,
	struct ntlm_buf */*session*/,
	struct ntlm_buf */*master*/);

/**
 * Calculate LMv2 response
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param serverchallenge challenge as sent by the server in the type2 message.
 * @param ntlmv2 calculated session key
 * @param answer ntlm response answer, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_lm2 (
	const void */*key*/,
	size_t /*len*/,
	const char */*username*/,
	const char */*target*/,
	const unsigned char serverchallenge[8],
	unsigned char ntlmv2[16],
	struct ntlm_buf */*answer*/);

/**
 * Calculate NTLMv1 response hash
 *
 * @param key the ntlm v1 key
 * @param len length of key
 * @param challenge sent by the server
 * @param answer calculated answer, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm1 (
	void */*key*/,
	size_t /*len*/,
	unsigned char challenge[8],
	struct ntlm_buf */*answer*/);

/**
 * Calculate NTLMv2 response
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param serverchallenge challenge as sent by the server in the type2 message.
 * @param infotarget infotarget as sent by the server in the type2 message.
 * @param ntlmv2 calculated session key
 * @param answer ntlm response answer, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm2 (
	const void */*key*/,
	size_t /*len*/,
	const char */*username*/,
	const char */*target*/,
	const unsigned char serverchallenge[8],
	const struct ntlm_buf */*infotarget*/,
	unsigned char ntlmv2[16],
	struct ntlm_buf */*answer*/);

/**
     * Third check with empty domain.
 */

int
heim_ntlm_calculate_ntlm2_sess (
	const unsigned char clnt_nonce[8],
	const unsigned char svr_chal[8],
	const unsigned char ntlm_hash[16],
	struct ntlm_buf */*lm*/,
	struct ntlm_buf */*ntlm*/);

int
heim_ntlm_calculate_ntlm2_sess_hash (
	const unsigned char clnt_nonce[8],
	const unsigned char svr_chal[8],
	unsigned char verifier[8]);

/**
 * Decodes an NTLM targetinfo message
 *
 * @param data input data buffer with the encode NTLM targetinfo message
 * @param ucs2 if the strings should be encoded with ucs2 (selected by flag in message).
 * @param ti the decoded target info, should be freed with heim_ntlm_free_targetinfo().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_decode_targetinfo (
	const struct ntlm_buf */*data*/,
	int /*ucs2*/,
	struct ntlm_targetinfo */*ti*/);

int
heim_ntlm_decode_type1 (
	const struct ntlm_buf */*buf*/,
	struct ntlm_type1 */*data*/);

int
heim_ntlm_decode_type2 (
	const struct ntlm_buf */*buf*/,
	struct ntlm_type2 */*type2*/);

int
heim_ntlm_decode_type3 (
	const struct ntlm_buf */*buf*/,
	int /*ucs2*/,
	struct ntlm_type3 */*type3*/);

void
heim_ntlm_derive_ntlm2_sess (
	const unsigned char sessionkey[16],
	const unsigned char */*clnt_nonce*/,
	size_t /*clnt_nonce_length*/,
	const unsigned char svr_chal[8],
	unsigned char derivedkey[16]);

/**
 * Encodes a ntlm_targetinfo message.
 *
 * @param ti the ntlm_targetinfo message to encode.
 * @param ucs2 ignored
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_targetinfo (
	const struct ntlm_targetinfo */*ti*/,
	int /*ucs2*/,
	struct ntlm_buf */*data*/);

/**
 * Encodes an ntlm_type1 message.
 *
 * @param type1 the ntlm_type1 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type1 (
	const struct ntlm_type1 */*type1*/,
	struct ntlm_buf */*data*/);

/**
 * Encodes an ntlm_type2 message.
 *
 * @param type2 the ntlm_type2 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type2 (
	const struct ntlm_type2 */*type2*/,
	struct ntlm_buf */*data*/);

/**
 * Encodes an ntlm_type3 message.
 *
 * @param type3 the ntlm_type3 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * @param[out] mic_offset offset of message integrity code
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type3 (
	const struct ntlm_type3 */*type3*/,
	struct ntlm_buf */*data*/,
	size_t */*mic_offset*/);

/**
 * heim_ntlm_free_buf frees the ntlm buffer
 *
 * @param p buffer to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_buf (struct ntlm_buf */*p*/);

/**
 * Frees the ntlm_targetinfo message
 *
 * @param ti targetinfo to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_targetinfo (struct ntlm_targetinfo */*ti*/);

/**
 * Frees the ntlm_type1 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type1 (struct ntlm_type1 */*data*/);

/**
 * Frees the ntlm_type2 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type2 (struct ntlm_type2 */*data*/);

/**
 * Frees the ntlm_type3 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type3 (struct ntlm_type3 */*data*/);

/**
 * Given a key and encrypted session, unwrap the session key
 *
 * @param baseKey the sessionBaseKey
 * @param encryptedSession encrypted session, type3.session field.
 * @param session generated session nonce, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_keyex_unwrap (
	struct ntlm_buf */*baseKey*/,
	struct ntlm_buf */*encryptedSession*/,
	struct ntlm_buf */*session*/);

int
heim_ntlm_keyex_wrap (
	struct ntlm_buf */*base_session*/,
	struct ntlm_buf */*session*/,
	struct ntlm_buf */*encryptedSession*/);

/**
 * Calculate the NTLM key, the password is assumed to be in UTF8.
 *
 * @param password password to calcute the key for.
 * @param key calcuted key, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_nt_key (
	const char */*password*/,
	struct ntlm_buf */*key*/);

/**
 * Generates an NTLMv2 session key.
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param upper_case_target upper case the target, should not be used only for legacy systems
 * @param ntlmv2 the ntlmv2 session key
 *
 * @return 0 on success, or an error code on failure.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_ntlmv2_key (
	const void */*key*/,
	size_t /*len*/,
	const char */*username*/,
	const char */*target*/,
	int /*upper_case_target*/,
	unsigned char ntlmv2[16]);

time_t
heim_ntlm_ts2unixtime (uint64_t /*t*/);

uint64_t
heim_ntlm_unix2ts_time (time_t /*unix_time*/);

/**
 @defgroup ntlm_core Heimdal NTLM library *
 * The NTLM core functions implement the string2key generation
 * function, message encode and decode function, and the hash function
 * functions.
 */

size_t
heim_ntlm_unparse_flags (
	uint32_t /*flags*/,
	char */*s*/,
	size_t /*len*/);

int
heim_ntlm_v1_base_session (
	void */*key*/,
	size_t /*len*/,
	struct ntlm_buf */*session*/);

int
heim_ntlm_v2_base_session (
	void */*key*/,
	size_t /*len*/,
	struct ntlm_buf */*ntlmResponse*/,
	struct ntlm_buf */*session*/);

/**
 * Verify NTLMv2 response.
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param now the time now (0 if the library should pick it up itself)
 * @param serverchallenge challenge as sent by the server in the type2 message.
 * @param answer ntlm response answer, should be freed with heim_ntlm_free_buf().
 * @param infotarget infotarget as sent by the server in the type2 message.
 * @param ntlmv2 calculated session key
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_verify_ntlm2 (
	const void */*key*/,
	size_t /*len*/,
	const char */*username*/,
	const char */*target*/,
	time_t /*now*/,
	const unsigned char serverchallenge[8],
	const struct ntlm_buf */*answer*/,
	struct ntlm_buf */*infotarget*/,
	unsigned char ntlmv2[16]);

#ifdef __cplusplus
}
#endif

#endif /* DOXY */
#endif /* __heimntlm_protos_h__ */
