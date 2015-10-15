/*	$NetBSD: heim-auth.h,v 1.1.1.2 2014/04/24 12:45:51 pettai Exp $	*/

/*
 * Generate challange for APOP and CRAM-MD5
 */

char *
heim_generate_challenge(const char *hostname); /* hostname can be NULL, the local hostname is used */

/*
 * APOP
 */

char *
heim_apop_create(const char *challenge, const char *password);

int
heim_apop_verify(const char *challenge, const char *password, const char *response);

/*
 * CRAM-MD5
 */

typedef struct heim_HMAC_MD5_STATE_s {
    uint32_t istate[4];
    uint32_t ostate[4];
} heim_CRAM_MD5_STATE;

typedef struct heim_cram_md5 *heim_cram_md5;

char *
heim_cram_md5_create(const char *challenge, const char *password);

int
heim_cram_md5_verify(const char *challenge, const char *password, const char *response);

void
heim_cram_md5_export(const char *password, heim_CRAM_MD5_STATE *state);

heim_cram_md5
heim_cram_md5_import(void *data, size_t len);

int
heim_cram_md5_verify_ctx(heim_cram_md5 ctx, const char *challenge, const char *response);

void
heim_cram_md5_free(heim_cram_md5 ctx);

/*
 * DIGEST-MD5
 *
 * heim_digest_t d;
 *
 * d = heim_digest_create(1, HEIM_DIGEST_TYPE_DIGEST_MD5_HTTP);
 *
 * if ((s = heim_digest_generate_challange(d)) != NULL) abort();
 * send_to_client(s);
 * response = read_from_client();
 *
 * heim_digest_parse_response(d, response);
 *
 * const char *user = heim_digest_get_key(d, "username");
 * heim_digest_set_key(d, "password", "sommar17");
 *
 * if (heim_digest_verify(d, &response)) abort();
 *
 * send_to_client(response);
 *
 * heim_digest_release(d);
 */

typedef struct heim_digest_desc *heim_digest_t;

heim_digest_t
heim_digest_create(int server, int type);

#define HEIM_DIGEST_TYPE_AUTO				0
#define HEIM_DIGEST_TYPE_RFC2069			1
#define HEIM_DIGEST_TYPE_MD5				2
#define HEIM_DIGEST_TYPE_MD5_SESS			3

void
heim_digest_init_set_key(heim_digest_t context, const char *key, const char *value);

const char *
heim_digest_generate_challenge(heim_digest_t context);

int
heim_digest_parse_challenge(heim_digest_t context, const char *challenge);

int
heim_digest_parse_response(heim_digest_t context, const char *response);

const char *
heim_digest_get_key(heim_digest_t context, const char *key);

int
heim_digest_set_key(heim_digest_t context, const char *key, const char *value);

void
heim_digest_set_user_password(heim_digest_t context, const char *password);

void
heim_digest_set_user_h1hash(heim_digest_t context, void *ptr, size_t size);

int
heim_digest_verify(heim_digest_t context, char **response);

const char *
heim_digest_create_response(heim_digest_t context);

void
heim_digest_get_session_key(heim_digest_t context, void **key, size_t *keySize);

void
heim_digest_release(heim_digest_t context);
