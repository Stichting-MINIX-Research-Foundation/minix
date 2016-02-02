/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file
 * packet related headers.
 */

#ifndef PACKET_H_
#define PACKET_H_

#include <time.h>

#ifdef HAVE_OPENSSL_BN_H
#include <openssl/bn.h>
#endif

#include "types.h"
#include "errors.h"

/* structure to keep track of printing state variables */
typedef struct pgp_printstate_t {
	unsigned	unarmoured;
	unsigned	skipping;
	int		indent;
} pgp_printstate_t;

/** General-use structure for variable-length data
 */

typedef struct {
	size_t           len;
	uint8_t		*contents;
	uint8_t		 mmapped;	/* contents need an munmap(2) */
} pgp_data_t;

/************************************/
/* Packet Tags - RFC4880, 4.2 */
/************************************/

/** Packet Tag - Bit 7 Mask (this bit is always set).
 * The first byte of a packet is the "Packet Tag".  It always
 * has bit 7 set.  This is the mask for it.
 *
 * \see RFC4880 4.2
 */
#define PGP_PTAG_ALWAYS_SET		0x80

/** Packet Tag - New Format Flag.
 * Bit 6 of the Packet Tag is the packet format indicator.
 * If it is set, the new format is used, if cleared the
 * old format is used.
 *
 * \see RFC4880 4.2
 */
#define PGP_PTAG_NEW_FORMAT		0x40


/** Old Packet Format: Mask for content tag.
 * In the old packet format bits 5 to 2 (including)
 * are the content tag.  This is the mask to apply
 * to the packet tag.  Note that you need to
 * shift by #PGP_PTAG_OF_CONTENT_TAG_SHIFT bits.
 *
 * \see RFC4880 4.2
 */
#define PGP_PTAG_OF_CONTENT_TAG_MASK	0x3c
/** Old Packet Format: Offset for the content tag.
 * As described at #PGP_PTAG_OF_CONTENT_TAG_MASK the
 * content tag needs to be shifted after being masked
 * out from the Packet Tag.
 *
 * \see RFC4880 4.2
 */
#define PGP_PTAG_OF_CONTENT_TAG_SHIFT	2
/** Old Packet Format: Mask for length type.
 * Bits 1 and 0 of the packet tag are the length type
 * in the old packet format.
 *
 * See #pgp_ptag_of_lt_t for the meaning of the values.
 *
 * \see RFC4880 4.2
 */
#define PGP_PTAG_OF_LENGTH_TYPE_MASK	0x03


/** Old Packet Format Lengths.
 * Defines the meanings of the 2 bits for length type in the
 * old packet format.
 *
 * \see RFC4880 4.2.1
 */
typedef enum {
	PGP_PTAG_OLD_LEN_1 = 0x00,	/* Packet has a 1 byte length -
					 * header is 2 bytes long. */
	PGP_PTAG_OLD_LEN_2 = 0x01,	/* Packet has a 2 byte length -
					 * header is 3 bytes long. */
	PGP_PTAG_OLD_LEN_4 = 0x02,	/* Packet has a 4 byte
						 * length - header is 5 bytes
						 * long. */
	PGP_PTAG_OLD_LEN_INDETERMINATE = 0x03	/* Packet has a
						 * indeterminate length. */
} pgp_ptag_of_lt_t;


/** New Packet Format: Mask for content tag.
 * In the new packet format the 6 rightmost bits
 * are the content tag.  This is the mask to apply
 * to the packet tag.  Note that you need to
 * shift by #PGP_PTAG_NF_CONTENT_TAG_SHIFT bits.
 *
 * \see RFC4880 4.2
 */
#define PGP_PTAG_NF_CONTENT_TAG_MASK	0x3f
/** New Packet Format: Offset for the content tag.
 * As described at #PGP_PTAG_NF_CONTENT_TAG_MASK the
 * content tag needs to be shifted after being masked
 * out from the Packet Tag.
 *
 * \see RFC4880 4.2
 */
#define PGP_PTAG_NF_CONTENT_TAG_SHIFT	0

/* PTag Content Tags */
/***************************/

/** Package Tags (aka Content Tags) and signature subpacket types.
 * This enumerates all rfc-defined packet tag values and the
 * signature subpacket type values that we understand.
 *
 * \see RFC4880 4.3
 * \see RFC4880 5.2.3.1
 */
typedef enum {
	PGP_PTAG_CT_RESERVED = 0,	/* Reserved - a packet tag must
					 * not have this value */
	PGP_PTAG_CT_PK_SESSION_KEY = 1,	/* Public-Key Encrypted Session
					 * Key Packet */
	PGP_PTAG_CT_SIGNATURE = 2,	/* Signature Packet */
	PGP_PTAG_CT_SK_SESSION_KEY = 3,	/* Symmetric-Key Encrypted Session
					 * Key Packet */
	PGP_PTAG_CT_1_PASS_SIG = 4,	/* One-Pass Signature
						 * Packet */
	PGP_PTAG_CT_SECRET_KEY = 5,	/* Secret Key Packet */
	PGP_PTAG_CT_PUBLIC_KEY = 6,	/* Public Key Packet */
	PGP_PTAG_CT_SECRET_SUBKEY = 7,	/* Secret Subkey Packet */
	PGP_PTAG_CT_COMPRESSED = 8,	/* Compressed Data Packet */
	PGP_PTAG_CT_SE_DATA = 9,/* Symmetrically Encrypted Data Packet */
	PGP_PTAG_CT_MARKER = 10,/* Marker Packet */
	PGP_PTAG_CT_LITDATA = 11,	/* Literal Data Packet */
	PGP_PTAG_CT_TRUST = 12,	/* Trust Packet */
	PGP_PTAG_CT_USER_ID = 13,	/* User ID Packet */
	PGP_PTAG_CT_PUBLIC_SUBKEY = 14,	/* Public Subkey Packet */
	PGP_PTAG_CT_RESERVED2 = 15,	/* reserved */
	PGP_PTAG_CT_RESERVED3 = 16,	/* reserved */
	PGP_PTAG_CT_USER_ATTR = 17,	/* User Attribute Packet */
	PGP_PTAG_CT_SE_IP_DATA = 18,	/* Sym. Encrypted and Integrity
					 * Protected Data Packet */
	PGP_PTAG_CT_MDC = 19,	/* Modification Detection Code Packet */

	PGP_PARSER_PTAG = 0x100,/* Internal Use: The packet is the "Packet
				 * Tag" itself - used when callback sends
				 * back the PTag. */
	PGP_PTAG_RAW_SS = 0x101,/* Internal Use: content is raw sig subtag */
	PGP_PTAG_SS_ALL = 0x102,/* Internal Use: select all subtags */
	PGP_PARSER_PACKET_END = 0x103,

	/* signature subpackets (0x200-2ff) (type+0x200) */
	/* only those we can parse are listed here */
	PGP_PTAG_SIG_SUBPKT_BASE = 0x200,	/* Base for signature
							 * subpacket types - All
							 * signature type values
							 * are relative to this
							 * value. */
	PGP_PTAG_SS_CREATION_TIME = 0x200 + 2,	/* signature creation time */
	PGP_PTAG_SS_EXPIRATION_TIME = 0x200 + 3,	/* signature
							 * expiration time */

	PGP_PTAG_SS_EXPORT_CERT = 0x200 + 4,	/* exportable certification */
	PGP_PTAG_SS_TRUST = 0x200 + 5,	/* trust signature */
	PGP_PTAG_SS_REGEXP = 0x200 + 6,	/* regular expression */
	PGP_PTAG_SS_REVOCABLE = 0x200 + 7,	/* revocable */
	PGP_PTAG_SS_KEY_EXPIRY = 0x200 + 9,	/* key expiration
							 * time */
	PGP_PTAG_SS_RESERVED = 0x200 + 10,	/* reserved */
	PGP_PTAG_SS_PREFERRED_SKA = 0x200 + 11,	/* preferred symmetric
						 * algs */
	PGP_PTAG_SS_REVOCATION_KEY = 0x200 + 12,	/* revocation key */
	PGP_PTAG_SS_ISSUER_KEY_ID = 0x200 + 16,	/* issuer key ID */
	PGP_PTAG_SS_NOTATION_DATA = 0x200 + 20,	/* notation data */
	PGP_PTAG_SS_PREFERRED_HASH = 0x200 + 21,	/* preferred hash
							 * algs */
	PGP_PTAG_SS_PREF_COMPRESS = 0x200 + 22,	/* preferred
							 * compression
							 * algorithms */
	PGP_PTAG_SS_KEYSERV_PREFS = 0x200 + 23,	/* key server
							 * preferences */
	PGP_PTAG_SS_PREF_KEYSERV = 0x200 + 24,	/* Preferred Key
							 * Server */
	PGP_PTAG_SS_PRIMARY_USER_ID = 0x200 + 25,	/* primary User ID */
	PGP_PTAG_SS_POLICY_URI = 0x200 + 26,	/* Policy URI */
	PGP_PTAG_SS_KEY_FLAGS = 0x200 + 27,	/* key flags */
	PGP_PTAG_SS_SIGNERS_USER_ID = 0x200 + 28,	/* Signer's User ID */
	PGP_PTAG_SS_REVOCATION_REASON = 0x200 + 29,	/* reason for
							 * revocation */
	PGP_PTAG_SS_FEATURES = 0x200 + 30,	/* features */
	PGP_PTAG_SS_SIGNATURE_TARGET = 0x200 + 31,	/* signature target */
	PGP_PTAG_SS_EMBEDDED_SIGNATURE = 0x200 + 32,	/* embedded signature */

	PGP_PTAG_SS_USERDEFINED00 = 0x200 + 100,	/* internal or
							 * user-defined */
	PGP_PTAG_SS_USERDEFINED01 = 0x200 + 101,
	PGP_PTAG_SS_USERDEFINED02 = 0x200 + 102,
	PGP_PTAG_SS_USERDEFINED03 = 0x200 + 103,
	PGP_PTAG_SS_USERDEFINED04 = 0x200 + 104,
	PGP_PTAG_SS_USERDEFINED05 = 0x200 + 105,
	PGP_PTAG_SS_USERDEFINED06 = 0x200 + 106,
	PGP_PTAG_SS_USERDEFINED07 = 0x200 + 107,
	PGP_PTAG_SS_USERDEFINED08 = 0x200 + 108,
	PGP_PTAG_SS_USERDEFINED09 = 0x200 + 109,
	PGP_PTAG_SS_USERDEFINED10 = 0x200 + 110,

	/* pseudo content types */
	PGP_PTAG_CT_LITDATA_HEADER = 0x300,
	PGP_PTAG_CT_LITDATA_BODY = 0x300 + 1,
	PGP_PTAG_CT_SIGNATURE_HEADER = 0x300 + 2,
	PGP_PTAG_CT_SIGNATURE_FOOTER = 0x300 + 3,
	PGP_PTAG_CT_ARMOUR_HEADER = 0x300 + 4,
	PGP_PTAG_CT_ARMOUR_TRAILER = 0x300 + 5,
	PGP_PTAG_CT_SIGNED_CLEARTEXT_HEADER = 0x300 + 6,
	PGP_PTAG_CT_SIGNED_CLEARTEXT_BODY = 0x300 + 7,
	PGP_PTAG_CT_SIGNED_CLEARTEXT_TRAILER = 0x300 + 8,
	PGP_PTAG_CT_UNARMOURED_TEXT = 0x300 + 9,
	PGP_PTAG_CT_ENCRYPTED_SECRET_KEY = 0x300 + 10,	/* In this case the
							 * algorithm specific
							 * fields will not be
							 * initialised */
	PGP_PTAG_CT_SE_DATA_HEADER = 0x300 + 11,
	PGP_PTAG_CT_SE_DATA_BODY = 0x300 + 12,
	PGP_PTAG_CT_SE_IP_DATA_HEADER = 0x300 + 13,
	PGP_PTAG_CT_SE_IP_DATA_BODY = 0x300 + 14,
	PGP_PTAG_CT_ENCRYPTED_PK_SESSION_KEY = 0x300 + 15,

	/* commands to the callback */
	PGP_GET_PASSPHRASE = 0x400,
	PGP_GET_SECKEY = 0x400 + 1,

	/* Errors */
	PGP_PARSER_ERROR = 0x500,	/* Internal Use: Parser Error */
	PGP_PARSER_ERRCODE = 0x500 + 1	/* Internal Use: Parser Error
					 * with errcode returned */
} pgp_content_enum;

enum {
	PGP_REVOCATION_NO_REASON	= 0,
	PGP_REVOCATION_SUPERSEDED	= 1,
	PGP_REVOCATION_COMPROMISED	= 2,
	PGP_REVOCATION_RETIRED		= 3,
	PGP_REVOCATION_NO_LONGER_VALID	= 0x20
};

/** Structure to hold one error code */
typedef struct {
	pgp_errcode_t   errcode;
} pgp_parser_errcode_t;

/** Structure to hold one packet tag.
 * \see RFC4880 4.2
 */
typedef struct {
	unsigned        new_format;	/* Whether this packet tag is new
					 * (1) or old format (0) */
	unsigned        type;	/* content_tag value - See
					 * #pgp_content_enum for meanings */
	pgp_ptag_of_lt_t length_type;	/* Length type (#pgp_ptag_of_lt_t)
					 * - only if this packet tag is old
					 * format.  Set to 0 if new format. */
	unsigned        length;	/* The length of the packet.  This value
				 * is set when we read and compute the length
				 * information, not at the same moment we
				 * create the packet tag structure. Only
	 * defined if #readc is set. *//* XXX: Ben, is this correct? */
	unsigned        position;	/* The position (within the
					 * current reader) of the packet */
	unsigned	size;	/* number of bits */
} pgp_ptag_t;

/** Public Key Algorithm Numbers.
 * OpenPGP assigns a unique Algorithm Number to each algorithm that is part of OpenPGP.
 *
 * This lists algorithm numbers for public key algorithms.
 *
 * \see RFC4880 9.1
 */
typedef enum {
	PGP_PKA_NOTHING	= 0,	/* No PKA */
	PGP_PKA_RSA = 1,	/* RSA (Encrypt or Sign) */
	PGP_PKA_RSA_ENCRYPT_ONLY = 2,	/* RSA Encrypt-Only (deprecated -
					 * \see RFC4880 13.5) */
	PGP_PKA_RSA_SIGN_ONLY = 3,	/* RSA Sign-Only (deprecated -
					 * \see RFC4880 13.5) */
	PGP_PKA_ELGAMAL = 16,	/* Elgamal (Encrypt-Only) */
	PGP_PKA_DSA = 17,	/* DSA (Digital Signature Algorithm) */
	PGP_PKA_RESERVED_ELLIPTIC_CURVE = 18,	/* Reserved for Elliptic
						 * Curve */
	PGP_PKA_RESERVED_ECDSA = 19,	/* Reserved for ECDSA */
	PGP_PKA_ELGAMAL_ENCRYPT_OR_SIGN = 20,	/* Deprecated. */
	PGP_PKA_RESERVED_DH = 21,	/* Reserved for Diffie-Hellman
					 * (X9.42, as defined for
					 * IETF-S/MIME) */
	PGP_PKA_PRIVATE00 = 100,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE01 = 101,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE02 = 102,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE03 = 103,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE04 = 104,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE05 = 105,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE06 = 106,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE07 = 107,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE08 = 108,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE09 = 109,/* Private/Experimental Algorithm */
	PGP_PKA_PRIVATE10 = 110	/* Private/Experimental Algorithm */
} pgp_pubkey_alg_t;

/** Structure to hold one DSA public key params.
 *
 * \see RFC4880 5.5.2
 */
typedef struct {
	BIGNUM         *p;	/* DSA prime p */
	BIGNUM         *q;	/* DSA group order q */
	BIGNUM         *g;	/* DSA group generator g */
	BIGNUM         *y;	/* DSA public key value y (= g^x mod p
				 * with x being the secret) */
} pgp_dsa_pubkey_t;

/** Structure to hold an RSA public key.
 *
 * \see RFC4880 5.5.2
 */
typedef struct {
	BIGNUM         *n;	/* RSA public modulus n */
	BIGNUM         *e;	/* RSA public encryption exponent e */
} pgp_rsa_pubkey_t;

/** Structure to hold an ElGamal public key params.
 *
 * \see RFC4880 5.5.2
 */
typedef struct {
	BIGNUM         *p;	/* ElGamal prime p */
	BIGNUM         *g;	/* ElGamal group generator g */
	BIGNUM         *y;	/* ElGamal public key value y (= g^x mod p
				 * with x being the secret) */
} pgp_elgamal_pubkey_t;

/** Version.
 * OpenPGP has two different protocol versions: version 3 and version 4.
 *
 * \see RFC4880 5.2
 */
typedef enum {
	PGP_V2 = 2,		/* Version 2 (essentially the same as v3) */
	PGP_V3 = 3,		/* Version 3 */
	PGP_V4 = 4		/* Version 4 */
} pgp_version_t;

/** Structure to hold a pgp public key */
typedef struct {
	pgp_version_t		version;/* version of the key (v3, v4...) */
	time_t			birthtime;
	time_t			duration;
		/* validity period of the key in days since
		* creation.  A value of 0 has a special meaning
		* indicating this key does not expire.  Only used with
		* v3 keys.  */
	unsigned		days_valid;	/* v4 duration */
	pgp_pubkey_alg_t	alg;	/* Public Key Algorithm type */
	union {
		pgp_dsa_pubkey_t dsa;	/* A DSA public key */
		pgp_rsa_pubkey_t rsa;	/* An RSA public key */
		pgp_elgamal_pubkey_t elgamal;	/* An ElGamal public key */
	}			key;	/* Public Key Parameters */
} pgp_pubkey_t;

/** Structure to hold data for one RSA secret key
 */
typedef struct {
	BIGNUM         *d;
	BIGNUM         *p;
	BIGNUM         *q;
	BIGNUM         *u;
} pgp_rsa_seckey_t;

/** pgp_dsa_seckey_t */
typedef struct {
	BIGNUM         *x;
} pgp_dsa_seckey_t;

/** pgp_elgamal_seckey_t */
typedef struct {
	BIGNUM         *x;
} pgp_elgamal_seckey_t;

/** s2k_usage_t
 */
typedef enum {
	PGP_S2KU_NONE = 0,
	PGP_S2KU_ENCRYPTED_AND_HASHED = 254,
	PGP_S2KU_ENCRYPTED = 255
} pgp_s2k_usage_t;

/** s2k_specifier_t
 */
typedef enum {
	PGP_S2KS_SIMPLE = 0,
	PGP_S2KS_SALTED = 1,
	PGP_S2KS_ITERATED_AND_SALTED = 3
} pgp_s2k_specifier_t;

/** Symmetric Key Algorithm Numbers.
 * OpenPGP assigns a unique Algorithm Number to each algorithm that is
 * part of OpenPGP.
 *
 * This lists algorithm numbers for symmetric key algorithms.
 *
 * \see RFC4880 9.2
 */
typedef enum {
	PGP_SA_PLAINTEXT = 0,	/* Plaintext or unencrypted data */
	PGP_SA_IDEA = 1,	/* IDEA */
	PGP_SA_TRIPLEDES = 2,	/* TripleDES */
	PGP_SA_CAST5 = 3,	/* CAST5 */
	PGP_SA_BLOWFISH = 4,	/* Blowfish */
	PGP_SA_AES_128 = 7,	/* AES with 128-bit key (AES) */
	PGP_SA_AES_192 = 8,	/* AES with 192-bit key */
	PGP_SA_AES_256 = 9,	/* AES with 256-bit key */
	PGP_SA_TWOFISH = 10,	/* Twofish with 256-bit key (TWOFISH) */
	PGP_SA_CAMELLIA_128 = 100,	/* Camellia with 128-bit key (CAMELLIA) */
	PGP_SA_CAMELLIA_192 = 101,	/* Camellia with 192-bit key */
	PGP_SA_CAMELLIA_256 = 102	/* Camellia with 256-bit key */
} pgp_symm_alg_t;

#define PGP_SA_DEFAULT_CIPHER	PGP_SA_CAST5

/** Hashing Algorithm Numbers.
 * OpenPGP assigns a unique Algorithm Number to each algorithm that is
 * part of OpenPGP.
 *
 * This lists algorithm numbers for hash algorithms.
 *
 * \see RFC4880 9.4
 */
typedef enum {
	PGP_HASH_UNKNOWN = -1,	/* used to indicate errors */
	PGP_HASH_MD5 = 1,	/* MD5 */
	PGP_HASH_SHA1 = 2,	/* SHA-1 */
	PGP_HASH_RIPEMD = 3,	/* RIPEMD160 */

	PGP_HASH_SHA256 = 8,	/* SHA256 */
	PGP_HASH_SHA384 = 9,	/* SHA384 */
	PGP_HASH_SHA512 = 10,	/* SHA512 */
	PGP_HASH_SHA224 = 11	/* SHA224 */
} pgp_hash_alg_t;

#define	PGP_DEFAULT_HASH_ALGORITHM	PGP_HASH_SHA256

void   pgp_calc_mdc_hash(const uint8_t *,
			const size_t,
			const uint8_t *,
			const unsigned,
			uint8_t *);
unsigned   pgp_is_hash_alg_supported(const pgp_hash_alg_t *);

/* Maximum block size for symmetric crypto */
#define PGP_MAX_BLOCK_SIZE	16

/* Maximum key size for symmetric crypto */
#define PGP_MAX_KEY_SIZE	32

/* Salt size for hashing */
#define PGP_SALT_SIZE		8

/* Max hash size */
#define PGP_MAX_HASH_SIZE	64

/** pgp_seckey_t
 */
typedef struct pgp_seckey_t {
	pgp_pubkey_t			pubkey;		/* public key */
	pgp_s2k_usage_t		s2k_usage;
	pgp_s2k_specifier_t		s2k_specifier;
	pgp_symm_alg_t		alg;		/* symmetric alg */
	pgp_hash_alg_t		hash_alg;	/* hash algorithm */
	uint8_t				salt[PGP_SALT_SIZE];
	unsigned			octetc;
	uint8_t				iv[PGP_MAX_BLOCK_SIZE];
	union {
		pgp_rsa_seckey_t		rsa;
		pgp_dsa_seckey_t		dsa;
		pgp_elgamal_seckey_t		elgamal;
	}				key;
	unsigned			checksum;
	uint8_t			       *checkhash;
} pgp_seckey_t;

/** Signature Type.
 * OpenPGP defines different signature types that allow giving
 * different meanings to signatures.  Signature types include 0x10 for
 * generitc User ID certifications (used when Ben signs Weasel's key),
 * Subkey binding signatures, document signatures, key revocations,
 * etc.
 *
 * Different types are used in different places, and most make only
 * sense in their intended location (for instance a subkey binding has
 * no place on a UserID).
 *
 * \see RFC4880 5.2.1
 */
typedef enum {
	PGP_SIG_BINARY = 0x00,	/* Signature of a binary document */
	PGP_SIG_TEXT = 0x01,	/* Signature of a canonical text document */
	PGP_SIG_STANDALONE = 0x02,	/* Standalone signature */

	PGP_CERT_GENERIC = 0x10,/* Generic certification of a User ID and
				 * Public Key packet */
	PGP_CERT_PERSONA = 0x11,/* Persona certification of a User ID and
				 * Public Key packet */
	PGP_CERT_CASUAL = 0x12,	/* Casual certification of a User ID and
				 * Public Key packet */
	PGP_CERT_POSITIVE = 0x13,	/* Positive certification of a
					 * User ID and Public Key packet */

	PGP_SIG_SUBKEY = 0x18,	/* Subkey Binding Signature */
	PGP_SIG_PRIMARY = 0x19,	/* Primary Key Binding Signature */
	PGP_SIG_DIRECT = 0x1f,	/* Signature directly on a key */

	PGP_SIG_REV_KEY = 0x20,	/* Key revocation signature */
	PGP_SIG_REV_SUBKEY = 0x28,	/* Subkey revocation signature */
	PGP_SIG_REV_CERT = 0x30,/* Certification revocation signature */

	PGP_SIG_TIMESTAMP = 0x40,	/* Timestamp signature */

	PGP_SIG_3RD_PARTY = 0x50/* Third-Party Confirmation signature */
} pgp_sig_type_t;

/** Struct to hold params of an RSA signature */
typedef struct pgp_rsa_sig_t {
	BIGNUM         *sig;	/* the signature value (m^d % n) */
} pgp_rsa_sig_t;

/** Struct to hold params of a DSA signature */
typedef struct pgp_dsa_sig_t {
	BIGNUM         *r;	/* DSA value r */
	BIGNUM         *s;	/* DSA value s */
} pgp_dsa_sig_t;

/** pgp_elgamal_signature_t */
typedef struct pgp_elgamal_sig_t {
	BIGNUM         *r;
	BIGNUM         *s;
} pgp_elgamal_sig_t;

#define PGP_KEY_ID_SIZE		8
#define PGP_FINGERPRINT_SIZE	20

/** Struct to hold a signature packet.
 *
 * \see RFC4880 5.2.2
 * \see RFC4880 5.2.3
 */
typedef struct pgp_sig_info_t {
	pgp_version_t   version;/* signature version number */
	pgp_sig_type_t  type;	/* signature type value */
	time_t          birthtime;	/* creation time of the signature */
	time_t          duration;	/* number of seconds it's valid for */
	uint8_t		signer_id[PGP_KEY_ID_SIZE];	/* Eight-octet key ID
							 * of signer */
	pgp_pubkey_alg_t key_alg;	/* public key algorithm number */
	pgp_hash_alg_t hash_alg;	/* hashing algorithm number */
	union {
		pgp_rsa_sig_t	rsa;	/* An RSA Signature */
		pgp_dsa_sig_t	dsa;	/* A DSA Signature */
		pgp_elgamal_sig_t	elgamal;	/* deprecated */
		pgp_data_t	unknown;	/* private or experimental */
	}			sig;	/* signature params */
	size_t          v4_hashlen;
	uint8_t		*v4_hashed;
	unsigned	 birthtime_set:1;
	unsigned	 signer_id_set:1;
	unsigned	 duration_set:1;
} pgp_sig_info_t;

/** Struct used when parsing a signature */
typedef struct pgp_sig_t {
	pgp_sig_info_t info;	/* The signature information */
	/* The following fields are only used while parsing the signature */
	uint8_t		 hash2[2];	/* high 2 bytes of hashed value */
	size_t		 v4_hashstart;	/* only valid if accumulate is set */
	pgp_hash_t     *hash;	/* the hash filled in for the data so far */
} pgp_sig_t;

/** The raw bytes of a signature subpacket */

typedef struct pgp_ss_raw_t {
	pgp_content_enum	 tag;
	size_t          	 length;
	uint8_t			*raw;
} pgp_ss_raw_t;

/** Signature Subpacket : Trust Level */

typedef struct pgp_ss_trust_t {
	uint8_t			 level;		/* Trust Level */
	uint8_t			 amount;	/* Amount */
} pgp_ss_trust_t;

/** Signature Subpacket : Notation Data */
typedef struct pgp_ss_notation_t {
	pgp_data_t		flags;
	pgp_data_t		name;
	pgp_data_t		value;
} pgp_ss_notation_t;

/** Signature Subpacket : Signature Target */
typedef struct pgp_ss_sig_target_t {
	pgp_pubkey_alg_t	pka_alg;
	pgp_hash_alg_t		hash_alg;
	pgp_data_t		hash;
} pgp_ss_sig_target_t;

/** pgp_subpacket_t */
typedef struct pgp_subpacket_t {
	size_t          	 length;
	uint8_t			*raw;
} pgp_subpacket_t;

/** Types of Compression */
typedef enum {
	PGP_C_NONE = 0,
	PGP_C_ZIP = 1,
	PGP_C_ZLIB = 2,
	PGP_C_BZIP2 = 3
} pgp_compression_type_t;

/** pgp_one_pass_sig_t */
typedef struct {
	uint8_t			version;
	pgp_sig_type_t		sig_type;
	pgp_hash_alg_t		hash_alg;
	pgp_pubkey_alg_t	key_alg;
	uint8_t			keyid[PGP_KEY_ID_SIZE];
	unsigned		nested;
} pgp_one_pass_sig_t;

/** Signature Subpacket : Revocation Key */
typedef struct {
	uint8_t   		class;
	uint8_t   		algid;
	uint8_t   		fingerprint[PGP_FINGERPRINT_SIZE];
} pgp_ss_revocation_key_t;

/** Signature Subpacket : Revocation Reason */
typedef struct {
	uint8_t   		 code;
	char			*reason;
} pgp_ss_revocation_t;

/** litdata_type_t */
typedef enum {
	PGP_LDT_BINARY = 'b',
	PGP_LDT_TEXT = 't',
	PGP_LDT_UTF8 = 'u',
	PGP_LDT_LOCAL = 'l',
	PGP_LDT_LOCAL2 = '1'
} pgp_litdata_enum;

/** pgp_litdata_header_t */
typedef struct {
	pgp_litdata_enum	format;
	char			filename[256];
	time_t			mtime;
} pgp_litdata_header_t;

/** pgp_litdata_body_t */
typedef struct {
	unsigned         length;
	uint8_t		*data;
	void		*mem;		/* pgp_memory_t pointer */
} pgp_litdata_body_t;

/** pgp_header_var_t */
typedef struct {
	char           *key;
	char           *value;
} pgp_header_var_t;

/** pgp_headers_t */
typedef struct {
	pgp_header_var_t	*headers;
	unsigned	         headerc;
} pgp_headers_t;

/** pgp_armour_header_t */
typedef struct {
	const char	*type;
	pgp_headers_t	 headers;
} pgp_armour_header_t;

/** pgp_fixed_body_t */
typedef struct pgp_fixed_body_t {
	unsigned        length;
	uint8_t		data[8192];	/* \todo fix hard-coded value? */
} pgp_fixed_body_t;

/** pgp_dyn_body_t */
typedef struct pgp_dyn_body_t {
	unsigned         length;
	uint8_t		*data;
} pgp_dyn_body_t;

enum {
	PGP_SE_IP_DATA_VERSION = 1,
	PGP_PKSK_V3 = 3
};

/** pgp_pk_sesskey_params_rsa_t */
typedef struct {
	BIGNUM         *encrypted_m;
	BIGNUM         *m;
} pgp_pk_sesskey_params_rsa_t;

/** pgp_pk_sesskey_params_elgamal_t */
typedef struct {
	BIGNUM         *g_to_k;
	BIGNUM         *encrypted_m;
} pgp_pk_sesskey_params_elgamal_t;

/** pgp_pk_sesskey_params_t */
typedef union {
	pgp_pk_sesskey_params_rsa_t rsa;
	pgp_pk_sesskey_params_elgamal_t elgamal;
} pgp_pk_sesskey_params_t;

/** pgp_pk_sesskey_t */
typedef struct {
	unsigned			version;
	uint8_t				key_id[PGP_KEY_ID_SIZE];
	pgp_pubkey_alg_t		alg;
	pgp_pk_sesskey_params_t	params;
	pgp_symm_alg_t		symm_alg;
	uint8_t				key[PGP_MAX_KEY_SIZE];
	uint16_t			checksum;
} pgp_pk_sesskey_t;

/** pgp_seckey_passphrase_t */
typedef struct {
	const pgp_seckey_t *seckey;
	char          **passphrase;	/* point somewhere that gets filled
					 * in to work around constness of
					 * content */
} pgp_seckey_passphrase_t;

/** pgp_get_seckey_t */
typedef struct {
	const pgp_seckey_t **seckey;
	const pgp_pk_sesskey_t *pk_sesskey;
} pgp_get_seckey_t;

/** pgp_parser_union_content_t */
typedef union {
	const char 			*error;
	pgp_parser_errcode_t		errcode;
	pgp_ptag_t			ptag;
	pgp_pubkey_t			pubkey;
	pgp_data_t			trust;
	uint8_t				*userid;
	pgp_data_t			userattr;
	pgp_sig_t			sig;
	pgp_ss_raw_t			ss_raw;
	pgp_ss_trust_t		ss_trust;
	unsigned			ss_revocable;
	time_t				ss_time;
	uint8_t				ss_issuer[PGP_KEY_ID_SIZE];
	pgp_ss_notation_t		ss_notation;
	pgp_subpacket_t		packet;
	pgp_compression_type_t	compressed;
	pgp_one_pass_sig_t		one_pass_sig;
	pgp_data_t			ss_skapref;
	pgp_data_t			ss_hashpref;
	pgp_data_t			ss_zpref;
	pgp_data_t			ss_key_flags;
	pgp_data_t			ss_key_server_prefs;
	unsigned			ss_primary_userid;
	char				*ss_regexp;
	char				*ss_policy;
	char				*ss_keyserv;
	pgp_ss_revocation_key_t	ss_revocation_key;
	pgp_data_t			ss_userdef;
	pgp_data_t			ss_unknown;
	pgp_litdata_header_t		litdata_header;
	pgp_litdata_body_t		litdata_body;
	pgp_dyn_body_t		mdc;
	pgp_data_t			ss_features;
	pgp_ss_sig_target_t		ss_sig_target;
	pgp_data_t			ss_embedded_sig;
	pgp_ss_revocation_t		ss_revocation;
	pgp_seckey_t			seckey;
	uint8_t				*ss_signer;
	pgp_armour_header_t		armour_header;
	const char 			*armour_trailer;
	pgp_headers_t			cleartext_head;
	pgp_fixed_body_t		cleartext_body;
	struct pgp_hash_t		*cleartext_trailer;
	pgp_dyn_body_t		unarmoured_text;
	pgp_pk_sesskey_t		pk_sesskey;
	pgp_seckey_passphrase_t	skey_passphrase;
	unsigned			se_ip_data_header;
	pgp_dyn_body_t		se_ip_data_body;
	pgp_fixed_body_t		se_data_body;
	pgp_get_seckey_t		get_seckey;
} pgp_contents_t;

/** pgp_packet_t */
struct pgp_packet_t {
	pgp_content_enum	tag;		/* type of contents */
	uint8_t			critical;	/* for sig subpackets */
	pgp_contents_t	u;		/* union for contents */
};

/** pgp_fingerprint_t */
typedef struct {
	uint8_t			fingerprint[PGP_FINGERPRINT_SIZE];
	unsigned        	length;
	pgp_hash_alg_t	hashtype;
} pgp_fingerprint_t;

int pgp_keyid(uint8_t *, const size_t, const pgp_pubkey_t *, pgp_hash_alg_t);
int pgp_fingerprint(pgp_fingerprint_t *, const pgp_pubkey_t *, pgp_hash_alg_t);

void pgp_finish(void);
void pgp_pubkey_free(pgp_pubkey_t *);
void pgp_userid_free(uint8_t **);
void pgp_data_free(pgp_data_t *);
void pgp_sig_free(pgp_sig_t *);
void pgp_ss_notation_free(pgp_ss_notation_t *);
void pgp_ss_revocation_free(pgp_ss_revocation_t *);
void pgp_ss_sig_target_free(pgp_ss_sig_target_t *);

void pgp_subpacket_free(pgp_subpacket_t *);
void pgp_parser_content_free(pgp_packet_t *);
void pgp_seckey_free(pgp_seckey_t *);
void pgp_pk_sesskey_free(pgp_pk_sesskey_t *);

int pgp_print_packet(pgp_printstate_t *, const pgp_packet_t *);

#define DYNARRAY(type, arr)	\
	unsigned arr##c; unsigned arr##vsize; type *arr##s

#define EXPAND_ARRAY(str, arr) do {					\
	if (str->arr##c == str->arr##vsize) {				\
		void	*__newarr;					\
		char	*__newarrc;					\
		unsigned	__newsize;				\
		__newsize = (str->arr##vsize * 2) + 10; 		\
		if ((__newarrc = __newarr = realloc(str->arr##s,	\
			__newsize * sizeof(*str->arr##s))) == NULL) {	\
			(void) fprintf(stderr, "EXPAND_ARRAY - bad realloc\n"); \
		} else {						\
			(void) memset(&__newarrc[str->arr##vsize * sizeof(*str->arr##s)], \
				0x0, (__newsize - str->arr##vsize) * sizeof(*str->arr##s)); \
			str->arr##s = __newarr;				\
			str->arr##vsize = __newsize;			\
		}							\
	}								\
} while(/*CONSTCOND*/0)

/** pgp_keydata_key_t
 */
typedef union {
	pgp_pubkey_t pubkey;
	pgp_seckey_t seckey;
} pgp_keydata_key_t;


/* sigpacket_t */
typedef struct {
	uint8_t			**userid;
	pgp_subpacket_t	*packet;
} sigpacket_t;

/* user revocation info */
typedef struct pgp_revoke_t {
	uint32_t		 uid;		/* index in uid array */
	uint8_t			 code;		/* revocation code */
	char			*reason;	/* c'mon, spill the beans */
} pgp_revoke_t;

/** signature subpackets */
typedef struct pgp_subsig_t {
	uint32_t		uid;		/* index in userid array in key */
	pgp_sig_t		sig;		/* trust signature */
	uint8_t			trustlevel;	/* level of trust */
	uint8_t			trustamount;	/* amount of trust */
} pgp_subsig_t;

/* describes a user's key */
struct pgp_key_t {
	DYNARRAY(uint8_t *, uid);		/* array of user ids */
	DYNARRAY(pgp_subpacket_t, packet);	/* array of raw subpackets */
	DYNARRAY(pgp_subsig_t, subsig);	/* array of signature subkeys */
	DYNARRAY(pgp_revoke_t, revoke);	/* array of signature revocations */
	pgp_content_enum	type;		/* type of key */
	pgp_keydata_key_t	key;		/* pubkey/seckey data */
	pgp_pubkey_t		sigkey;		/* signature key */
	uint8_t			sigid[PGP_KEY_ID_SIZE];
	pgp_fingerprint_t	sigfingerprint;	/* pgp signature fingerprint */
	pgp_pubkey_t		enckey;		/* encryption key */
	uint8_t			encid[PGP_KEY_ID_SIZE];
	pgp_fingerprint_t	encfingerprint;	/* pgp encryption id fingerprint */
	uint32_t		uid0;		/* primary uid index in uids array */
	uint8_t			revoked;	/* key has been revoked */
	pgp_revoke_t		revocation;	/* revocation reason */
};

#define MDC_PKT_TAG	0xd3

#endif /* PACKET_H_ */
