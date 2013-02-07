#ifndef DST_H
#define DST_H

#ifndef HAS_DST_KEY
typedef struct dst_key {
	char	*dk_key_name;   /* name of the key */
	int	dk_key_size;    /* this is the size of the key in bits */
	int	dk_proto;       /* what protocols this key can be used for */
	int	dk_alg;         /* algorithm number from key record */
	unsigned dk_flags;     /* and the flags of the public key */
	unsigned dk_id;        /* identifier of the key */
} DST_KEY;
#endif /* HAS_DST_KEY */

/* 
 * DST Crypto API defintions 
 */
void     dst_init(void);
int      dst_check_algorithm(const int);

int dst_sign_data(const int mode,	 /* specifies INIT/UPDATE/FINAL/ALL */
		  DST_KEY *in_key,	 /* the key to use */
		  void **context,	 /* pointer to state structure */
		  const u_char *data,	 /* data to be signed */
		  const unsigned len,	 /* length of input data */
		  u_char *signature,	 /* buffer to write signature to */
		  const unsigned sig_len); /* size of output buffer */

int dst_verify_data(const int mode,	 /* specifies INIT/UPDATE/FINAL/ALL */
		    DST_KEY *in_key,	 /* the key to use */
		    void **context,	 /* pointer to state structure */
		    const u_char *data,  /* data to be verified */
		    const unsigned len,	 /* length of input data */
		    const u_char *signature,/* buffer containing signature */
		    const unsigned sig_len);	 /* length of signature */


DST_KEY *dst_read_key(const char *in_name,   /* name of key */
		      const unsigned in_id, /* key tag identifier */
		      const int in_alg,      /* key algorithm */
		      const int key_type);   /* Private/PublicKey wanted*/

int      dst_write_key(const DST_KEY *key,  /* key to write out */
		       const int key_type); /* Public/Private */

DST_KEY *dst_dnskey_to_key(const char *in_name,	/* KEY record name */
			   const u_char *key,	/* KEY RDATA */
			   const unsigned len);	/* size of input buffer*/


int      dst_key_to_dnskey(const DST_KEY *key,	/* key to translate */
			   u_char *out_storage,	/* output buffer */
			   const unsigned out_len); /* size of out_storage*/


DST_KEY *dst_buffer_to_key(const char *key_name,  /* name of the key */
			   const int alg,	  /* algorithm */
			   const unsigned flags,  /* dns flags */
			   const int protocol,	  /* dns protocol */
			   const u_char *key_buf, /* key in dns wire fmt */
			   const unsigned key_len);	  /* size of key */


int     dst_key_to_buffer(DST_KEY *key, u_char *out_buff, unsigned buf_len);

DST_KEY *dst_generate_key(const char *name,    /* name of new key */
			  const int bits,      /* size of new key */
			  const int exp,       /* alg dependent parameter*/
			  const unsigned flags,     /* key DNS flags */
			  const int protocol, /* key DNS protocol */
			  const int alg);       /* key algorithm to generate */

DST_KEY *dst_free_key(DST_KEY *f_key);
int      dst_compare_keys(const DST_KEY *key1, const DST_KEY *key2);

int	dst_sig_size(DST_KEY *key);

int     dst_random(const int mode, unsigned wanted, u_char *outran);


/* support for dns key tags/ids */
u_int16_t dst_s_dns_key_id(const u_char *dns_key_rdata,
			   const unsigned rdata_len);
u_int16_t dst_s_id_calc(const u_char *key_data, const unsigned key_len);

/* Used by callers as well as by the library.  */
#define RAW_KEY_SIZE    8192        /* large enough to store any key */

/* DST_API control flags */
/* These are used used in functions dst_sign_data and dst_verify_data */
#define SIG_MODE_INIT		1  /* initalize digest */
#define SIG_MODE_UPDATE		2  /* add data to digest */
#define SIG_MODE_FINAL		4  /* generate/verify signature */
#define SIG_MODE_ALL		(SIG_MODE_INIT|SIG_MODE_UPDATE|SIG_MODE_FINAL)

/* Flags for dst_read_private_key()  */
#define DST_FORCE_READ		0x1000000
#define DST_CAN_SIGN		0x010F
#define DST_NO_AUTHEN		0x8000
#define DST_EXTEND_FLAG         0x1000
#define DST_STANDARD		0
#define DST_PRIVATE             0x2000000
#define DST_PUBLIC              0x4000000
#define DST_RAND_SEMI           1
#define DST_RAND_STD            2
#define DST_RAND_KEY            3
#define DST_RAND_DSS            4


/* DST algorithm codes */
#define KEY_RSA			1
#define KEY_DH			2
#define KEY_DSA			3
#define KEY_PRIVATE		254
#define KEY_EXPAND		255
#define KEY_HMAC_MD5		157
#define KEY_HMAC_SHA1		158
#define UNKNOWN_KEYALG		0
#define DST_MAX_ALGS            KEY_HMAC_SHA1

/* DST constants to locations in KEY record  changes in new KEY record */
#define DST_FLAGS_SIZE		2
#define DST_KEY_PROT		2
#define DST_KEY_ALG		3
#define DST_EXT_FLAG            4
#define DST_KEY_START		4

#ifndef SIGN_F_NOKEY 
#define SIGN_F_NOKEY		0xC000
#endif

/* error codes from dst routines */
#define SIGN_INIT_FAILURE	(-23)
#define SIGN_UPDATE_FAILURE	(-24)
#define SIGN_FINAL_FAILURE	(-25)
#define VERIFY_INIT_FAILURE	(-26)
#define VERIFY_UPDATE_FAILURE	(-27)
#define VERIFY_FINAL_FAILURE	(-28)
#define MISSING_KEY_OR_SIGNATURE (-30)
#define UNSUPPORTED_KEYALG	(-31)

#endif /* DST_H */
