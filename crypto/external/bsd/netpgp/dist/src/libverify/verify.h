/*-
 * Copyright (c) 2012 Alistair Crooks <agc@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef NETPGP_VERIFY_H_
#define NETPGP_VERIFY_H_	20120928

#include <sys/types.h>

#include <inttypes.h>

#ifndef PGPV_ARRAY
/* creates 2 unsigned vars called "name"c and "name"size in current scope */
/* also creates an array called "name"s in current scope */
#define PGPV_ARRAY(type, name)						\
	unsigned name##c; unsigned name##vsize; type *name##s
#endif

/* 64bit key ids */
#define PGPV_KEYID_LEN		8
#define PGPV_STR_KEYID_LEN	(PGPV_KEYID_LEN + PGPV_KEYID_LEN + 1)

/* bignum structure */
typedef struct pgpv_bignum_t {
	void			*bn;	/* hide the implementation details */
	uint16_t		 bits;	/* cached number of bits */
} pgpv_bignum_t;

/* right now, our max binary digest length is 20 bytes */
#define PGPV_MAX_HASH_LEN	20

/* fingerprint */
typedef struct pgpv_fingerprint_t {
	uint8_t			hashalg;	/* algorithm for digest */
	uint8_t			v[PGPV_MAX_HASH_LEN];	/* the digest */
	uint32_t		len;		/* its length */
} pgpv_fingerprint_t;

/* specify size for array of bignums */
#define PGPV_MAX_PUBKEY_BN	4

/* public key */
typedef struct pgpv_pubkey_t {
	pgpv_fingerprint_t	 fingerprint;	/* key fingerprint i.e. digest */
	uint8_t			 keyid[PGPV_KEYID_LEN];	/* last 8 bytes of v4 keys */
	int64_t		 	 birth;		/* creation time */
	int64_t			 expiry;	/* expiry time */
	pgpv_bignum_t		 bn[PGPV_MAX_PUBKEY_BN];	/* bignums */
	uint8_t			 keyalg;	/* key algorithm */
	uint8_t			 hashalg;	/* hash algorithm */
	uint8_t			 version;	/* key version */
} pgpv_pubkey_t;

#define PGPV_MAX_SESSKEY_BN	2

/* a (size, byte array) string */
typedef struct pgpv_string_t {
	size_t			 size;
	uint8_t			*data;
} pgpv_string_t;

typedef struct pgpv_ref_t {
	void			*vp;
	size_t			 offset;
	unsigned		 mem;
} pgpv_ref_t;

#define PGPV_MAX_SECKEY_BN	4

typedef struct pgpv_compress_t {
	pgpv_string_t		 s;
	uint8_t			 compalg;
} pgpv_compress_t;

/* a packet dealing with trust */
typedef struct pgpv_trust_t {
	uint8_t			level;
	uint8_t			amount;
} pgpv_trust_t;

/* a signature sub packet */
typedef struct pgpv_sigsubpkt_t {
	pgpv_string_t		 s;
	uint8_t			 tag;
	uint8_t			 critical;
} pgpv_sigsubpkt_t;

#define PGPV_MAX_SIG_BN		2

typedef struct pgpv_signature_t {
	uint8_t			*signer;		/* key id of signer */
	pgpv_ref_t		 hashstart;
	uint8_t			*hash2;
	uint8_t			*mpi;
	int64_t			 birth;
	int64_t			 keyexpiry;
	int64_t			 expiry;
	uint32_t		 hashlen;
	uint8_t			 version;
	uint8_t			 type;
	uint8_t			 keyalg;
	uint8_t			 hashalg;
	uint8_t			 trustlevel;
	uint8_t			 trustamount;
	pgpv_bignum_t		 bn[PGPV_MAX_SIG_BN];
	char			*regexp;
	char			*pref_key_server;
	char			*policy;
	char			*features;
	char			*why_revoked;
	uint8_t			*revoke_fingerprint;
	uint8_t			 revoke_alg;
	uint8_t			 revoke_sensitive;
	uint8_t			 trustsig;
	uint8_t			 revocable;
	uint8_t			 pref_symm_alg;
	uint8_t			 pref_hash_alg;
	uint8_t			 pref_compress_alg;
	uint8_t			 key_server_modify;
	uint8_t			 notation;
	uint8_t			 type_key;
	uint8_t			 primary_userid;
	uint8_t			 revoked;	/* subtract 1 to get real reason, 0 == not revoked */
} pgpv_signature_t;

/* a signature packet */
typedef struct pgpv_sigpkt_t {
	pgpv_signature_t	 sig;
	uint16_t		 subslen;
	uint16_t		 unhashlen;
	PGPV_ARRAY(pgpv_sigsubpkt_t, subpkts);
} pgpv_sigpkt_t;

/* a one-pass signature packet */
typedef struct pgpv_onepass_t {
	uint8_t			 keyid[PGPV_KEYID_LEN];
	uint8_t			 version;
	uint8_t			 type;
	uint8_t			 hashalg;
	uint8_t			 keyalg;
	uint8_t			 nested;
} pgpv_onepass_t;

/* a literal data packet */
typedef struct pgpv_litdata_t {
	uint8_t			*filename;
	pgpv_string_t		 s;
	uint32_t		 secs;
	uint8_t			 namelen;
	char			 format;
	unsigned		 mem;
	size_t			 offset;
	size_t			 len;
} pgpv_litdata_t;

/* user attributes - images */
typedef struct pgpv_userattr_t {
	size_t 			 len;
	PGPV_ARRAY(pgpv_string_t, subattrs);
} pgpv_userattr_t;

/* a general PGP packet */
typedef struct pgpv_pkt_t {
	uint8_t			 tag;
	uint8_t			 newfmt;
	uint8_t			 allocated;
	uint8_t			 mement;
	size_t			 offset;
	pgpv_string_t		 s;
	union {
		pgpv_sigpkt_t	sigpkt;
		pgpv_onepass_t	onepass;
		pgpv_litdata_t	litdata;
		pgpv_compress_t	compressed;
		pgpv_trust_t	trust;
		pgpv_pubkey_t	pubkey;
		pgpv_string_t	userid;
		pgpv_userattr_t	userattr;
	} u;
} pgpv_pkt_t;

/* a memory structure */
typedef struct pgpv_mem_t {
	size_t			 size;
	size_t			 cc;
	uint8_t			*mem;
	FILE			*fp;
	uint8_t			 dealloc;
	const char		*allowed;	/* the types of packet that are allowed */
} pgpv_mem_t;

/* packet parser */

typedef struct pgpv_signed_userid_t {
	pgpv_string_t	 	 userid;
	PGPV_ARRAY(pgpv_signature_t, sigs);
	uint8_t			 primary_userid;
	uint8_t			 revoked;
} pgpv_signed_userid_t;

typedef struct pgpv_signed_userattr_t {
	pgpv_userattr_t	 	 userattr;
	PGPV_ARRAY(pgpv_signature_t, sigs);
	uint8_t			 revoked;
} pgpv_signed_userattr_t;

typedef struct pgpv_signed_subkey_t {
	pgpv_pubkey_t	 	 subkey;
	pgpv_signature_t 	 revoc_self_sig;
	PGPV_ARRAY(pgpv_signature_t, sigs);
} pgpv_signed_subkey_t;

typedef struct pgpv_primarykey_t {
	pgpv_pubkey_t 		 primary;
	pgpv_signature_t 	 revoc_self_sig;
	PGPV_ARRAY(pgpv_signature_t, direct_sigs);
	PGPV_ARRAY(pgpv_signed_userid_t, signed_userids);
	PGPV_ARRAY(pgpv_signed_userattr_t, signed_userattrs);
	PGPV_ARRAY(pgpv_signed_subkey_t, signed_subkeys);
	size_t			 fmtsize;
	uint8_t			 primary_userid;
} pgpv_primarykey_t;

/* everything stems from this structure */
typedef struct pgpv_t {
	PGPV_ARRAY(pgpv_pkt_t, 	 pkts);		/* packet array */
	PGPV_ARRAY(pgpv_primarykey_t,	 primaries);	/* array of primary keys */
	PGPV_ARRAY(pgpv_mem_t,	 areas);	/* areas we read packets from */
	PGPV_ARRAY(size_t,	 datastarts);	/* starts of data packets */
	size_t		 	 pkt;		/* when parsing, current pkt number */
	const char		*op;		/* the operation we're doing */
} pgpv_t;

#define PGPV_REASON_LEN		128

/* when searching, we define a cursor, and fill in an array of subscripts */
typedef struct pgpv_cursor_t {
	pgpv_t			*pgp;			/* pointer to pgp tree */
	char			*field;			/* field we're searching on */
	char			*op;			/* operation we're doing */
	char			*value;			/* value we're searching for */
	void			*ptr;			/* for regexps etc */
	PGPV_ARRAY(uint32_t,	 found);		/* array of matched subscripts */
	PGPV_ARRAY(size_t,	 datacookies);		/* cookies to retrieve matched data */
	int64_t			 sigtime;		/* time of signature */
	char			 why[PGPV_REASON_LEN];	/* reason for bad signature */
} pgpv_cursor_t;

#ifndef __BEGIN_DECLS
#  if defined(__cplusplus)
#  define __BEGIN_DECLS           extern "C" {
#  define __END_DECLS             }
#  else
#  define __BEGIN_DECLS
#  define __END_DECLS
#  endif
#endif

__BEGIN_DECLS

int pgpv_read_pubring(pgpv_t */*pgp*/, const void */*keyringfile/mem*/, ssize_t /*size*/);

size_t pgpv_verify(pgpv_cursor_t */*cursor*/, pgpv_t */*pgp*/, const void */*mem/file*/, ssize_t /*size*/);
size_t pgpv_get_verified(pgpv_cursor_t */*cursor*/, size_t /*cookie*/, char **/*ret*/);

size_t pgpv_get_entry(pgpv_t */*pgp*/, unsigned /*ent*/, char **/*ret*/);

int pgpv_close(pgpv_t */*pgp*/);

__END_DECLS

#endif
