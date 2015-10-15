/*-
 * Copyright (c) 2012,2013,2014,2015 Alistair Crooks <agc@NetBSD.org>
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
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mman.h>

#include <arpa/inet.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bzlib.h"
#include "zlib.h"

#include "array.h"
#include "b64.h"
#include "bn.h"
#include "bufgap.h"
#include "digest.h"
#include "misc.h"
#include "pgpsum.h"
#include "rsa.h"
#include "verify.h"

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

#ifndef __dead
#define __dead				__attribute__((__noreturn__))
#endif

#ifndef __printflike
#define __printflike(n, m)		__attribute__((format(printf,n,m)))
#endif

#define BITS_TO_BYTES(b)		(((b) + (CHAR_BIT - 1)) / CHAR_BIT)

/* packet types */
#define SIGNATURE_PKT			2
#define ONEPASS_SIGNATURE_PKT		4
#define PUBKEY_PKT			6
#define COMPRESSED_DATA_PKT		8
#define MARKER_PKT			10
#define LITDATA_PKT			11
#define TRUST_PKT			12
#define USERID_PKT			13
#define PUB_SUBKEY_PKT			14
#define USER_ATTRIBUTE_PKT		17

/* only allow certain packets at certain times */
#define PUBRING_ALLOWED			"\002\006\014\015\016\021"
#define SIGNATURE_ALLOWED		"\002\004\010\013"

/* actions to do on close */
#define FREE_MEM			0x01
#define UNMAP_MEM			0x02

/* types of pubkey we encounter */
#define PUBKEY_RSA_ENCRYPT_OR_SIGN	1
#define PUBKEY_RSA_ENCRYPT		2
#define PUBKEY_RSA_SIGN			3
#define PUBKEY_ELGAMAL_ENCRYPT		16
#define PUBKEY_DSA			17
#define PUBKEY_ELLIPTIC_CURVE		18
#define PUBKEY_ECDSA			19
#define PUBKEY_ELGAMAL_ENCRYPT_OR_SIGN	20

/* hash algorithm definitions */
#define PGPV_HASH_MD5			1
#define PGPV_HASH_SHA1			2
#define PGPV_HASH_RIPEMD		3
#define PGPV_HASH_SHA256		8
#define PGPV_HASH_SHA384		9
#define PGPV_HASH_SHA512		10

/* pubkey defs for bignums */
#define RSA_N				0
#define RSA_E				1
#define DSA_P				0
#define DSA_Q				1
#define DSA_G				2
#define DSA_Y				3
#define ELGAMAL_P			0
#define ELGAMAL_G			1
#define ELGAMAL_Y			2

/* sesskey indices */
#define RSA_SESSKEY_ENCRYPTED_M		0
#define RSA_SESSKEY_M			1
#define ELGAMAL_SESSKEY_G_TO_K		0
#define ELGAMAL_SESSKEY_ENCRYPTED_M	1

/* seckey indices */
#define RSA_SECKEY_D			0
#define RSA_SECKEY_P			1
#define RSA_SECKEY_Q			2
#define RSA_SECKEY_U			3
#define DSA_SECKEY_X			0
#define ELGAMAL_SECKEY_X		0

/* signature mpi indices in bignumber array */
#define RSA_SIG				0
#define DSA_R				0
#define DSA_S				1
#define ELGAMAL_SIG_R			0
#define ELGAMAL_SIG_S			1

/* signature types */
#define SIGTYPE_BINARY_DOC		0x00	/* Signature of a binary document */
#define SIGTYPE_TEXT			0x01	/* Signature of a canonical text document */
#define SIGTYPE_STANDALONE		0x02	/* Standalone signature */

#define SIGTYPE_GENERIC_USERID		0x10	/* Generic certification of a User ID and Public Key packet */
#define SIGTYPE_PERSONA_USERID		0x11	/* Persona certification of a User ID and Public Key packet */
#define SIGTYPE_CASUAL_USERID		0x12	/* Casual certification of a User ID and Public Key packet */
#define SIGTYPE_POSITIVE_USERID		0x13	/* Positive certification of a User ID and Public Key packet */

#define SIGTYPE_SUBKEY_BINDING		0x18	/* Subkey Binding Signature */
#define SIGTYPE_PRIMARY_KEY_BINDING	0x19	/* Primary Key Binding Signature */
#define SIGTYPE_DIRECT_KEY		0x1f	/* Signature directly on a key */

#define SIGTYPE_KEY_REVOCATION		0x20	/* Key revocation signature */
#define SIGTYPE_SUBKEY_REVOCATION	0x28	/* Subkey revocation signature */
#define SIGTYPE_CERT_REVOCATION		0x30	/* Certification revocation signature */

#define SIGTYPE_TIMESTAMP_SIG		0x40	/* Timestamp signature */
#define SIGTYPE_3RDPARTY		0x50	/* Third-Party Confirmation signature */

/* Forward declarations */
static int read_all_packets(pgpv_t */*pgp*/, pgpv_mem_t */*mem*/, const char */*op*/);
static int read_binary_file(pgpv_t */*pgp*/, const char */*op*/, const char */*fmt*/, ...) __printflike(3, 4);
static int read_binary_memory(pgpv_t */*pgp*/, const char */*op*/, const void */*memory*/, size_t /*size*/);

/* read a file into the pgpv_mem_t struct */
static int
read_file(pgpv_t *pgp, const char *f)
{
	struct stat	 st;
	pgpv_mem_t	*mem;

	ARRAY_EXPAND(pgp->areas);
	ARRAY_COUNT(pgp->areas) += 1;
	mem = &ARRAY_LAST(pgp->areas);
	memset(mem, 0x0, sizeof(*mem));
	if ((mem->fp = fopen(f, "r")) == NULL) {
		fprintf(stderr, "can't read '%s'", f);
		return 0;
	}
	fstat(fileno(mem->fp), &st);
	mem->size = (size_t)st.st_size;
	mem->mem = mmap(NULL, mem->size, PROT_READ, MAP_SHARED, fileno(mem->fp), 0);
	mem->dealloc = UNMAP_MEM;
	return 1;
}

/* DTRT and free resources */
static int
closemem(pgpv_mem_t *mem)
{
	switch(mem->dealloc) {
	case FREE_MEM:
		free(mem->mem);
		mem->size = 0;
		break;
	case UNMAP_MEM:
		munmap(mem->mem, mem->size);
		fclose(mem->fp);
		break;
	}
	return 1;
}

/* make a reference to a memory area, and its offset */
static void
make_ref(pgpv_t *pgp, uint8_t mement, pgpv_ref_t *ref)
{
	ref->mem = mement;
	ref->offset = ARRAY_ELEMENT(pgp->areas, ref->mem).cc;
	ref->vp = pgp;
}

/* return the pointer we wanted originally */
static uint8_t *
get_ref(pgpv_ref_t *ref)
{
	pgpv_mem_t	*mem;
	pgpv_t		*pgp = (pgpv_t *)ref->vp;;

	mem = &ARRAY_ELEMENT(pgp->areas, ref->mem);
	return &mem->mem[ref->offset];
}

#define IS_PARTIAL(x)		((x) >= 224 && (x) < 255)
#define DECODE_PARTIAL(x)	(1 << ((x) & 0x1f))

#define PKT_LENGTH(m, off)						\
	((m[off] < 192) ? (m[off]) : 					\
	 (m[off] < 224) ? ((m[off] - 192) << 8) + (m[off + 1]) + 192 :	\
	 (m[off + 1] << 24) | ((m[off + 2]) << 16) | ((m[off + 3]) << 8)  | (m[off + 4]))

#define PKT_LENGTH_LENGTH(m, off)					\
	((m[off] < 192) ? 1 : (m[off] < 224) ? 2 : 5)

/* fix up partial body lengths, return new size */
static size_t
fixup_partials(pgpv_t *pgp, uint8_t *p, size_t totlen, size_t filesize, size_t *cc)
{
	pgpv_mem_t	*mem;
	size_t		 partial;
	size_t		 newcc;

	if (totlen > filesize) {
		printf("fixup_partial: filesize %zu is less than encoded size %zu\n", filesize, totlen);
		return 0;
	}
	ARRAY_EXPAND(pgp->areas);
	ARRAY_COUNT(pgp->areas) += 1;
	mem = &ARRAY_LAST(pgp->areas);
	mem->size = totlen;
	if ((mem->mem = calloc(1, mem->size + 5)) == NULL) {
		printf("fixup_partial: can't allocate %zu length\n", totlen);
		return 0;
	}
	newcc = 0;
	mem->dealloc = FREE_MEM;
	for (*cc = 0 ; *cc < totlen ; newcc += partial, *cc += partial + 1) {
		if (IS_PARTIAL(p[*cc])) {
			partial = DECODE_PARTIAL(p[*cc]);
			memcpy(&mem->mem[newcc], &p[*cc + 1], partial);
		} else {
			partial = PKT_LENGTH(p, *cc);
			*cc += PKT_LENGTH_LENGTH(p, *cc);
			memcpy(&mem->mem[newcc], &p[*cc], partial);
			newcc += partial;
			*cc += partial;
			break;
		}
	}
	return newcc;
}

/* get the weirdo packet length */
static size_t
get_pkt_len(uint8_t newfmt, uint8_t *p, size_t filesize, int isprimary)
{
	size_t	lenbytes;
	size_t	len;

	if (newfmt) {
		if (IS_PARTIAL(*p)) {
			if (!isprimary) {
				/* for sub-packets, only 1, 2 or 4 byte sizes allowed */
				return ((*p - 192) << 8) + *(p + 1) + 192;
			}
			lenbytes = 1;
			for (len = DECODE_PARTIAL(*p) ; IS_PARTIAL(p[len + lenbytes]) ; lenbytes++) {
				len += DECODE_PARTIAL(p[len + lenbytes]);
			}
			len += get_pkt_len(newfmt, &p[len + lenbytes], filesize, 1);
			return len;
		}
		return PKT_LENGTH(p, 0);
	} else {
		switch(*--p & 0x3) {
		case 0:
			return *(p + 1);
		case 1:
			return (*(p + 1) << 8) | *(p + 2);
		case 2:
			return (*(p + 1) << 24) | (*(p + 2) << 16) | (*(p + 3) << 8)  | *(p + 4);
		default:
			return filesize;
		}
	}
}

static const uint8_t	base64s[] =
/* 000 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 016 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 032 */       "\0\0\0\0\0\0\0\0\0\0\0?\0\0\0@"
/* 048 */       "56789:;<=>\0\0\0\0\0\0"
/* 064 */       "\0\1\2\3\4\5\6\7\10\11\12\13\14\15\16\17"
/* 080 */       "\20\21\22\23\24\25\26\27\30\31\32\0\0\0\0\0"
/* 096 */       "\0\33\34\35\36\37 !\"#$%&'()"
/* 112 */       "*+,-./01234\0\0\0\0\0"
/* 128 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 144 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 160 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 176 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 192 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 208 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 224 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
/* 240 */       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";


/* short function to decode from base64 */
/* inspired by an ancient copy of b64.c, then rewritten, the bugs are all mine */
static int
frombase64(char *dst, const char *src, size_t size, int flag)
{
	uint8_t	out[3];
	uint8_t	in[4];
	uint8_t	b;
	size_t	srcc;
	int	dstc;
	int	gotc;
	int	i;

	USE_ARG(flag);
	for (dstc = 0, srcc = 0 ; srcc < size; ) {
		for (gotc = 0, i = 0; i < 4 && srcc < size; i++) {
			for (b = 0x0; srcc < size && b == 0x0 ; ) {
				b = base64s[(unsigned)src[srcc++]];
			}
			if (srcc < size) {
				gotc += 1;
				if (b) {
					in[i] = (uint8_t)(b - 1);
				}
			} else {
				in[i] = 0x0;
			}
		}
		if (gotc) {
			out[0] = (uint8_t)((unsigned)in[0] << 2 |
						(unsigned)in[1] >> 4);
			out[1] = (uint8_t)((unsigned)in[1] << 4 |
						(unsigned)in[2] >> 2);
			out[2] = (uint8_t)(((in[2] << 6) & 0xc0) | in[3]);
			for (i = 0; i < gotc - 1; i++) {
				*dst++ = out[i];
			}
			dstc += gotc - 1;
		}
	}
	return dstc;
}

/* get the length of the packet length field */
static unsigned
get_pkt_len_len(uint8_t newfmt, uint8_t *p, int isprimary)
{
	if (newfmt) {
		if (IS_PARTIAL(*p)) {
			return (isprimary) ? 1 : 2;
		}
		return PKT_LENGTH_LENGTH(p, 0);
	} else {
		switch(*--p & 0x3) {
		case 0:
			return 1;
		case 1:
			return 2;
		case 2:
			return 4;
		default:
			return 0;
		}
	}
}

/* copy the 32bit integer in memory in network order */
static unsigned
fmt_32(uint8_t *p, uint32_t a)
{
	a = pgp_hton32(a);
	memcpy(p, &a, sizeof(a));
	return sizeof(a);
}

/* copy the 16bit integer in memory in network order */
static unsigned
fmt_16(uint8_t *p, uint16_t a)
{
	a = pgp_hton16(a);
	memcpy(p, &a, sizeof(a));
	return sizeof(a);
}

/* format a binary string in memory */
static size_t
fmt_binary(char *s, size_t size, const uint8_t *bin, unsigned len)
{
	unsigned	i;
	size_t		cc;

	for (cc = 0, i = 0 ; i < len && cc < size ; i++) {
		cc += snprintf(&s[cc], size - cc, "%02x", bin[i]);
	}
	return cc;
}

/* format an mpi into memory */
static unsigned
fmt_binary_mpi(pgpv_bignum_t *mpi, uint8_t *p, size_t size)
{
	unsigned	 bytes;
	BIGNUM		*bn;

	bytes = BITS_TO_BYTES(mpi->bits);
	if ((size_t)bytes + 2 + 1 > size) {
		fprintf(stderr, "truncated mpi");
		return 0;
	}
	bn = (BIGNUM *)mpi->bn;
	if (bn == NULL || BN_is_zero(bn)) {
		fmt_32(p, 0);
		return 2 + 1;
	}
	fmt_16(p, mpi->bits);
	BN_bn2bin(bn, &p[2]);
	return bytes + 2;
}

/* dump an mpi value onto stdout */
static size_t
fmt_mpi(char *s, size_t size, pgpv_bignum_t *bn, const char *name, int pbits)
{
	size_t	 cc;
	char	*buf;

	cc = snprintf(s, size, "%s=", name);
	if (pbits) {
		cc += snprintf(&s[cc], size - cc, "[%u bits] ", bn->bits);
	}
	buf = BN_bn2hex(bn->bn);
	cc += snprintf(&s[cc], size - cc, "%s\n", buf);
	free(buf);
	return cc;
}

#define ALG_IS_RSA(alg)	(((alg) == PUBKEY_RSA_ENCRYPT_OR_SIGN) ||	\
			 ((alg) == PUBKEY_RSA_ENCRYPT) ||		\
			 ((alg) == PUBKEY_RSA_SIGN))

#define ALG_IS_DSA(alg)	((alg) == PUBKEY_DSA)

/* format key mpis into memory */
static unsigned
fmt_key_mpis(pgpv_pubkey_t *pubkey, uint8_t *buf, size_t size)
{
	size_t	cc;

	cc = 0;
	buf[cc++] = pubkey->version;
	cc += fmt_32(&buf[cc], (uint32_t)pubkey->birth); /* XXX - do this portably! */
	buf[cc++] = pubkey->keyalg;	/* XXX - sign, or encrypt and sign? */
	switch(pubkey->keyalg) {
	case PUBKEY_RSA_ENCRYPT_OR_SIGN:
	case PUBKEY_RSA_ENCRYPT:
	case PUBKEY_RSA_SIGN:
		cc += fmt_binary_mpi(&pubkey->bn[RSA_N], &buf[cc], size - cc);
		cc += fmt_binary_mpi(&pubkey->bn[RSA_E], &buf[cc], size - cc);
		break;
	case PUBKEY_DSA:
		cc += fmt_binary_mpi(&pubkey->bn[DSA_P], &buf[cc], size - cc);
		cc += fmt_binary_mpi(&pubkey->bn[DSA_Q], &buf[cc], size - cc);
		cc += fmt_binary_mpi(&pubkey->bn[DSA_G], &buf[cc], size - cc);
		cc += fmt_binary_mpi(&pubkey->bn[DSA_Y], &buf[cc], size - cc);
		break;
	default:
		cc += fmt_binary_mpi(&pubkey->bn[ELGAMAL_P], &buf[cc], size - cc);
		cc += fmt_binary_mpi(&pubkey->bn[ELGAMAL_G], &buf[cc], size - cc);
		cc += fmt_binary_mpi(&pubkey->bn[ELGAMAL_Y], &buf[cc], size - cc);
		break;
	}
	return (unsigned)cc;
}

/* calculate the fingerprint, RFC 4880, section 12.2 */
static int
pgpv_calc_fingerprint(pgpv_fingerprint_t *fingerprint, pgpv_pubkey_t *pubkey, const char *hashtype)
{
	digest_t	 fphash;
	uint16_t	 cc;
	uint8_t		 ch = 0x99;
	uint8_t		 buf[8192 + 2 + 1];
	uint8_t		 len[2];

	memset(&fphash, 0x0, sizeof(fphash));
	if (pubkey->version == 4) {
		/* v4 keys */
		fingerprint->hashalg = digest_get_alg(hashtype);
		digest_init(&fphash, (unsigned)fingerprint->hashalg);
		cc = fmt_key_mpis(pubkey, buf, sizeof(buf));
		digest_update(&fphash, &ch, 1);
		fmt_16(len, cc);
		digest_update(&fphash, len, 2);
		digest_update(&fphash, buf, (unsigned)cc);
		fingerprint->len = digest_final(fingerprint->v, &fphash);
		return 1;
	}
	if (ALG_IS_RSA(pubkey->keyalg)) {
		/* v3 keys are RSA */
		fingerprint->hashalg = digest_get_alg("md5");
		digest_init(&fphash, (unsigned)fingerprint->hashalg);
		if (pubkey->bn[RSA_N].bn && pubkey->bn[RSA_E].bn) {
			cc = fmt_binary_mpi(&pubkey->bn[RSA_N], buf, sizeof(buf));
			digest_update(&fphash, &buf[2], (unsigned)(cc - 2));
			cc = fmt_binary_mpi(&pubkey->bn[RSA_E], buf, sizeof(buf));
			digest_update(&fphash, &buf[2], (unsigned)(cc - 2));
			fingerprint->len = digest_final(fingerprint->v, &fphash);
			return 1;
		}
	}
	if (pubkey->bn[RSA_N].bn) {
		if ((cc = fmt_binary_mpi(&pubkey->bn[RSA_N], buf, sizeof(buf))) >= PGPV_KEYID_LEN) {
			memcpy(fingerprint->v, &buf[cc - PGPV_KEYID_LEN], PGPV_KEYID_LEN);
			fingerprint->len = PGPV_KEYID_LEN;
			return 1;
		}
	}
	/* exhausted all avenues, really */
	memset(fingerprint->v, 0xff, fingerprint->len = PGPV_KEYID_LEN);
	return 1;
}

/* format a fingerprint into memory */
static size_t
fmt_fingerprint(char *s, size_t size, pgpv_fingerprint_t *fingerprint, const char *name)
{
	unsigned	i;
	size_t		cc;

	cc = snprintf(s, size, "%s ", name);
	for (i = 0 ; i < fingerprint->len ; i++) {
		cc += snprintf(&s[cc], size - cc, "%02hhx%s",
			fingerprint->v[i], (i % 2 == 1) ? " " : "");
	}
	cc += snprintf(&s[cc], size - cc, "\n");
	return cc;
}

/* calculate keyid from a pubkey */
static int 
calc_keyid(pgpv_pubkey_t *key, const char *hashtype)
{
	pgpv_calc_fingerprint(&key->fingerprint, key, hashtype);
	memcpy(key->keyid, &key->fingerprint.v[key->fingerprint.len - PGPV_KEYID_LEN], PGPV_KEYID_LEN);
	return 1;
}

/* convert a hex string to a 64bit key id (in big endian byte order */
static void
str_to_keyid(const char *s, uint8_t *keyid)
{
	uint64_t	u64;

	u64 = (uint64_t)strtoull(s, NULL, 16);
	u64 =   ((u64 & 0x00000000000000FFUL) << 56) | 
		((u64 & 0x000000000000FF00UL) << 40) | 
		((u64 & 0x0000000000FF0000UL) << 24) | 
		((u64 & 0x00000000FF000000UL) <<  8) | 
		((u64 & 0x000000FF00000000UL) >>  8) | 
		((u64 & 0x0000FF0000000000UL) >> 24) | 
		((u64 & 0x00FF000000000000UL) >> 40) | 
		((u64 & 0xFF00000000000000UL) >> 56);
	memcpy(keyid, &u64, PGPV_KEYID_LEN);
}

#define PKT_ALWAYS_ON			0x80
#define PKT_NEWFMT_MASK			0x40
#define PKT_NEWFMT_TAG_MASK		0x3f
#define PKT_OLDFMT_TAG_MASK		0x3c

#define SUBPKT_CRITICAL_MASK		0x80
#define SUBPKT_TAG_MASK			0x7f

#define SUBPKT_SIG_BIRTH		2
#define SUBPKT_SIG_EXPIRY		3
#define SUBPKT_EXPORT_CERT		4
#define SUBPKT_TRUST_SIG		5
#define SUBPKT_REGEXP			6
#define SUBPKT_REVOCABLE		7
#define SUBPKT_KEY_EXPIRY		9
#define SUBPKT_BWD_COMPAT		10
#define SUBPKT_PREF_SYMMETRIC_ALG	11
#define SUBPKT_REVOCATION_KEY		12
#define SUBPKT_ISSUER			16
#define SUBPKT_NOTATION			20
#define SUBPKT_PREF_HASH_ALG		21
#define SUBPKT_PREF_COMPRESS_ALG	22
#define SUBPKT_KEY_SERVER_PREFS		23
#define SUBPKT_PREF_KEY_SERVER		24
#define SUBPKT_PRIMARY_USER_ID		25
#define SUBPKT_POLICY_URI		26
#define SUBPKT_KEY_FLAGS		27
#define SUBPKT_SIGNER_ID		28
#define SUBPKT_REVOCATION_REASON	29
#define SUBPKT_FEATURES			30
#define SUBPKT_SIGNATURE_TARGET		31
#define SUBPKT_EMBEDDED_SIGNATURE	32

#define UNCOMPRESSED			0
#define ZIP_COMPRESSION			1
#define ZLIB_COMPRESSION		2
#define BZIP2_COMPRESSION		3

/* get a 16 bit integer, in host order */
static uint16_t
get_16(uint8_t *p)
{
	uint16_t	u16;

	memcpy(&u16, p, sizeof(u16));
	return pgp_ntoh16(u16);
}

/* get a 32 bit integer, in host order */
static uint32_t
get_32(uint8_t *p)
{
	uint32_t	u32;

	memcpy(&u32, p, sizeof(u32));
	return pgp_ntoh32(u32);
}

#define HOURSECS	(int64_t)(60 * 60)
#define DAYSECS		(int64_t)(24 * 60 * 60)
#define MONSECS		(int64_t)(30 * DAYSECS)
#define YEARSECS	(int64_t)(365 * DAYSECS)

/* format (human readable) time into memory */
static size_t
fmt_time(char *s, size_t size, const char *header, int64_t n, const char *trailer, int relative)
{
	struct tm	tm;
	time_t		elapsed;
	time_t		now;
	time_t		t;
	size_t		cc;

	t = (time_t)n;
	now = time(NULL);
	elapsed = now - t;
	gmtime_r(&t, &tm);            
	cc = snprintf(s, size, "%s%04d-%02d-%02d", header,
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	if (relative) {
		cc += snprintf(&s[cc], size - cc, " (%lldy %lldm %lldd %lldh %s)",
			llabs((long long)elapsed / YEARSECS),
			llabs(((long long)elapsed % YEARSECS) / MONSECS),
			llabs(((long long)elapsed % MONSECS) / DAYSECS),
			llabs(((long long)elapsed % DAYSECS) / HOURSECS),
			(now > t) ? "ago" : "ahead");
	}
	cc += snprintf(&s[cc], size - cc, "%s", trailer);
	return cc;
}

/* dump key mpis to stdout */
static void
print_key_mpis(pgpv_bignum_t *v, uint8_t keyalg)
{
	char	s[8192];

	switch(keyalg) {
	case PUBKEY_RSA_ENCRYPT_OR_SIGN:
	case PUBKEY_RSA_ENCRYPT:
	case PUBKEY_RSA_SIGN:
		fmt_mpi(s, sizeof(s), &v[RSA_N], "rsa.n", 1);
		printf("%s", s);
		fmt_mpi(s, sizeof(s), &v[RSA_E], "rsa.e", 1);
		printf("%s", s);
		break;
	case PUBKEY_ELGAMAL_ENCRYPT:
		fmt_mpi(s, sizeof(s), &v[ELGAMAL_P], "elgamal.p", 1);
		printf("%s", s);
		fmt_mpi(s, sizeof(s), &v[ELGAMAL_Y], "elgamal.y", 1);
		printf("%s", s);
		break;
	case PUBKEY_DSA:
		fmt_mpi(s, sizeof(s), &v[DSA_P], "dsa.p", 1);
		printf("%s", s);
		fmt_mpi(s, sizeof(s), &v[DSA_Q], "dsa.q", 1);
		printf("%s", s);
		fmt_mpi(s, sizeof(s), &v[DSA_G], "dsa.g", 1);
		printf("%s", s);
		fmt_mpi(s, sizeof(s), &v[DSA_Y], "dsa.y", 1);
		printf("%s", s);
		break;
	default:
		printf("hi, unusual keyalg %u\n", keyalg);
		break;
	}
}

/* get an mpi, including 2 byte length */
static int
get_mpi(pgpv_bignum_t *mpi, uint8_t *p, size_t pktlen, size_t *off)
{
	size_t	bytes;

	mpi->bits = get_16(p);
	if ((bytes = (size_t)BITS_TO_BYTES(mpi->bits)) > pktlen) {
		return 0;
	}
	*off += sizeof(mpi->bits);
	mpi->bn = BN_bin2bn(&p[sizeof(mpi->bits)], (int)bytes, NULL);
	*off += bytes;
	return 1;
}

/* read mpis in signature */
static int
read_signature_mpis(pgpv_sigpkt_t *sigpkt, uint8_t *p, size_t pktlen)
{
	size_t	off;

	off = 0;
	switch(sigpkt->sig.keyalg) {
	case PUBKEY_RSA_ENCRYPT_OR_SIGN:
	case PUBKEY_RSA_SIGN:
	case PUBKEY_RSA_ENCRYPT:
		if (!get_mpi(&sigpkt->sig.bn[RSA_SIG], p, pktlen, &off)) {
			printf("sigpkt->version %d, rsa sig weird\n", sigpkt->sig.version);
			return 0;
		}
		break;
	case PUBKEY_DSA:
	case PUBKEY_ECDSA:
	case PUBKEY_ELGAMAL_ENCRYPT_OR_SIGN: /* deprecated */
		if (!get_mpi(&sigpkt->sig.bn[DSA_R], p, pktlen, &off) ||
		    !get_mpi(&sigpkt->sig.bn[DSA_S], &p[off], pktlen, &off)) {
			printf("sigpkt->version %d, dsa/elgamal sig weird\n", sigpkt->sig.version);
			return 0;
		}
		break;
	default:
		printf("weird type of sig! %d\n", sigpkt->sig.keyalg);
		return 0;
	}
	return 1;
}

/* add the signature sub packet to the signature packet */
static int
add_subpacket(pgpv_sigpkt_t *sigpkt, uint8_t tag, uint8_t *p, uint16_t len)
{
	pgpv_sigsubpkt_t	subpkt;

	memset(&subpkt, 0x0, sizeof(subpkt));
	subpkt.s.size = len;
	subpkt.critical = 0;
	subpkt.tag = tag;
	subpkt.s.data = p;
	ARRAY_APPEND(sigpkt->subpkts, subpkt);
	return 1;
}

/* read the subpackets in the signature */
static int
read_sig_subpackets(pgpv_sigpkt_t *sigpkt, uint8_t *p, size_t pktlen)
{
	pgpv_sigsubpkt_t	 subpkt;
	const int		 is_subpkt = 0;
	unsigned		 lenlen;
	unsigned		 i;
	uint8_t			*start;

	start = p;
	for (i = 0 ; (unsigned)(p - start) < sigpkt->subslen ; i++) {
		memset(&subpkt, 0x0, sizeof(subpkt));
		subpkt.s.size = get_pkt_len(1, p, 0, is_subpkt);
		lenlen = get_pkt_len_len(1, p, is_subpkt);
		if (lenlen > pktlen) {
			printf("weird lenlen %u\n", lenlen);
			return 0;
		}
		p += lenlen;
		subpkt.critical = (*p & SUBPKT_CRITICAL_MASK);
		subpkt.tag = (*p & SUBPKT_TAG_MASK);
		p += 1;
		switch(subpkt.tag) {
		case SUBPKT_SIG_BIRTH:
			sigpkt->sig.birth = (int64_t)get_32(p);
			break;
		case SUBPKT_SIG_EXPIRY:
			sigpkt->sig.expiry = (int64_t)get_32(p);
			break;
		case SUBPKT_KEY_EXPIRY:
			sigpkt->sig.keyexpiry = (int64_t)get_32(p);
			break;
		case SUBPKT_ISSUER:
			sigpkt->sig.signer = p;
			break;
		case SUBPKT_SIGNER_ID:
			sigpkt->sig.signer = p;
			break;
		case SUBPKT_TRUST_SIG:
			sigpkt->sig.trustsig = *p;
			break;
		case SUBPKT_REGEXP:
			sigpkt->sig.regexp = (char *)(void *)p;
			break;
		case SUBPKT_REVOCABLE:
			sigpkt->sig.revocable = *p;
			break;
		case SUBPKT_PREF_SYMMETRIC_ALG:
			sigpkt->sig.pref_symm_alg = *p;
			break;
		case SUBPKT_REVOCATION_KEY:
			sigpkt->sig.revoke_sensitive = (*p & 0x40);
			sigpkt->sig.revoke_alg = p[1];
			sigpkt->sig.revoke_fingerprint = &p[2];
			break;
		case SUBPKT_NOTATION:
			sigpkt->sig.notation = *p;
			break;
		case SUBPKT_PREF_HASH_ALG:
			sigpkt->sig.pref_hash_alg = *p;
			break;
		case SUBPKT_PREF_COMPRESS_ALG:
			sigpkt->sig.pref_compress_alg = *p;
			break;
		case SUBPKT_PREF_KEY_SERVER:
			sigpkt->sig.pref_key_server = (char *)(void *)p;
			break;
		case SUBPKT_KEY_SERVER_PREFS:
			sigpkt->sig.key_server_modify = *p;
			break;
		case SUBPKT_KEY_FLAGS:
			sigpkt->sig.type_key = *p;
			break;
		case SUBPKT_PRIMARY_USER_ID:
			sigpkt->sig.primary_userid = *p;
			break;
		case SUBPKT_POLICY_URI:
			sigpkt->sig.policy = (char *)(void *)p;
			break;
		case SUBPKT_FEATURES:
			sigpkt->sig.features = (char *)(void *)p;
			break;
		case SUBPKT_REVOCATION_REASON:
			sigpkt->sig.revoked = *p++ + 1;
			sigpkt->sig.why_revoked = (char *)(void *)p;
			break;
		default:
			printf("Ignoring unusual/reserved signature subpacket %d\n", subpkt.tag);
			break;
		}
		subpkt.s.data = p;
		p += subpkt.s.size - 1;
		ARRAY_APPEND(sigpkt->subpkts, subpkt);
	}
	return 1;
}

/* parse signature packet */
static int
read_sigpkt(pgpv_t *pgp, uint8_t mement, pgpv_sigpkt_t *sigpkt, uint8_t *p, size_t pktlen)
{
	unsigned	 lenlen;
	uint8_t		*base;

	make_ref(pgp, mement, &sigpkt->sig.hashstart);
	base = p;
	switch(sigpkt->sig.version = *p++) {
	case 2:
	case 3:
		if ((lenlen = *p++) != 5) {
			printf("read_sigpkt: hashed length not 5\n");
			return 0;
		}
		sigpkt->sig.hashlen = lenlen;
		/* put birthtime into a subpacket */
		sigpkt->sig.type = *p++;
		add_subpacket(sigpkt, SUBPKT_SIG_BIRTH, p, sizeof(uint32_t));
		sigpkt->sig.birth = (int64_t)get_32(p);
		p += sizeof(uint32_t);
		sigpkt->sig.signer = p;
		add_subpacket(sigpkt, SUBPKT_SIGNER_ID, p, PGPV_KEYID_LEN);
		p += PGPV_KEYID_LEN;
		sigpkt->sig.keyalg = *p++;
		sigpkt->sig.hashalg = *p++;
		sigpkt->sig.hash2 = p;
		if (!read_signature_mpis(sigpkt, sigpkt->sig.mpi = p + 2, pktlen)) {
			printf("read_sigpkt: can't read sigs v3\n");
			return 0;
		}
		break;
	case 4:
		sigpkt->sig.type = *p++;
		sigpkt->sig.keyalg = *p++;
		sigpkt->sig.hashalg = *p++;
		sigpkt->subslen = get_16(p);
		p += sizeof(sigpkt->subslen);
		if (!read_sig_subpackets(sigpkt, p, pktlen)) {
			printf("read_sigpkt: can't read sig subpackets, v4\n");
			return 0;
		}
		if (!sigpkt->sig.signer) {
			sigpkt->sig.signer = get_ref(&sigpkt->sig.hashstart) + 16;
		}
		p += sigpkt->subslen;
		sigpkt->sig.hashlen = (unsigned)(p - base);
		sigpkt->unhashlen = get_16(p);
		p += sizeof(sigpkt->unhashlen) + sigpkt->unhashlen;
		sigpkt->sig.hash2 = p;
		if (!read_signature_mpis(sigpkt, sigpkt->sig.mpi = p + 2, pktlen)) {
			printf("read_sigpkt: can't read sigs, v4\n");
			return 0;
		}
		break;
	default:
		printf("read_sigpkt: unusual signature version (%u)\n", sigpkt->sig.version);
		break;
	}
	return 1;
}


/* this parses compressed data, decompresses it, and calls the parser again */
static int
read_compressed(pgpv_t *pgp, pgpv_compress_t *compressed, uint8_t *p, size_t len)
{
	pgpv_mem_t	*unzmem;
	bz_stream	 bz;
	z_stream	 z;
	int		 ok = 0;

	compressed->compalg = *p;
	compressed->s.size = len;
	if ((compressed->s.data = calloc(1, len)) == NULL) {
		printf("read_compressed: can't allocate %zu length\n", len);
		return 0;
	}
	switch(compressed->compalg) {
	case UNCOMPRESSED:
		printf("not implemented %d compression yet\n", compressed->compalg);
		return 0;
	default:
		break;
	}
	ARRAY_EXPAND(pgp->areas);
	ARRAY_COUNT(pgp->areas) += 1;
	unzmem = &ARRAY_LAST(pgp->areas);
	unzmem->size = len * 10;
	unzmem->dealloc = FREE_MEM;
	if ((unzmem->mem = calloc(1, unzmem->size)) == NULL) {
		printf("read_compressed: calloc failed!\n");
		return 0;
	}
	switch(compressed->compalg) {
	case ZIP_COMPRESSION:
	case ZLIB_COMPRESSION:
		memset(&z, 0x0, sizeof(z));
		z.next_in = p + 1;
		z.avail_in = (unsigned)(len - 1);
		z.total_in = (unsigned)(len - 1);
		z.next_out = unzmem->mem;
		z.avail_out = (unsigned)unzmem->size;
		z.total_out = (unsigned)unzmem->size;
		break;
	case BZIP2_COMPRESSION:
		memset(&bz, 0x0, sizeof(bz));
		bz.avail_in = (unsigned)(len - 1);
		bz.next_in = (char *)(void *)p + 1;
		bz.next_out = (char *)(void *)unzmem->mem;
		bz.avail_out = (unsigned)unzmem->size;
		break;
	}
	switch(compressed->compalg) {
	case ZIP_COMPRESSION:
		ok = (inflateInit2(&z, -15) == Z_OK);
		break;
	case ZLIB_COMPRESSION:
		ok = (inflateInit(&z) == Z_OK);
		break;
	case BZIP2_COMPRESSION:
		ok = (BZ2_bzDecompressInit(&bz, 1, 0) == BZ_OK);
		break;
	}
	if (!ok) {
		printf("read_compressed: initialisation failed!\n");
		return 0;
	}
	switch(compressed->compalg) {
	case ZIP_COMPRESSION:
	case ZLIB_COMPRESSION:
		ok = (inflate(&z, Z_FINISH) == Z_STREAM_END);
		unzmem->size = z.total_out;
		break;
	case BZIP2_COMPRESSION:
		ok = (BZ2_bzDecompress(&bz) == BZ_STREAM_END);
		unzmem->size = ((uint64_t)bz.total_out_hi32 << 32) | bz.total_out_lo32;
		break;
	}
	if (!ok) {
		printf("read_compressed: inflate failed!\n");
		return 0;
	}
	return 1;
}

/* parse one pass signature packet */
static int
read_onepass_sig(pgpv_onepass_t *onepasspkt, uint8_t *mem)
{
	onepasspkt->version = mem[0];
	onepasspkt->type = mem[1];
	onepasspkt->hashalg = mem[2];
	onepasspkt->keyalg = mem[3];
	memcpy(onepasspkt->keyid, &mem[4], sizeof(onepasspkt->keyid));
	onepasspkt->nested = mem[12];
	return 1;
}

/* parse public key packet */
static int
read_pubkey(pgpv_pubkey_t *pubkey, uint8_t *mem, size_t pktlen, int pbn)
{
	size_t		 off;

	off = 0;
	pubkey->version = mem[off++];
	pubkey->birth = get_32(&mem[off]);
	off += 4;
	if (pubkey->version == 2 || pubkey->version == 3) {
		pubkey->expiry = get_16(&mem[off]) * DAYSECS;
		off += 2;
	}
	if ((pubkey->keyalg = mem[off++]) == 0) {
		pubkey->keyalg = PUBKEY_RSA_ENCRYPT_OR_SIGN;
		printf("got unusual pubkey keyalg %u\n", mem[off - 1]);
	}
	switch(pubkey->keyalg) {
	case PUBKEY_RSA_ENCRYPT_OR_SIGN:
	case PUBKEY_RSA_ENCRYPT:
	case PUBKEY_RSA_SIGN:
		if (!get_mpi(&pubkey->bn[RSA_N], &mem[off], pktlen, &off) ||
		    !get_mpi(&pubkey->bn[RSA_E], &mem[off], pktlen, &off)) {
			return 0;
		}
		break;
	case PUBKEY_ELGAMAL_ENCRYPT:
	case PUBKEY_ELGAMAL_ENCRYPT_OR_SIGN:
		if (!get_mpi(&pubkey->bn[ELGAMAL_P], &mem[off], pktlen, &off) ||
		    !get_mpi(&pubkey->bn[ELGAMAL_Y], &mem[off], pktlen, &off)) {
			return 0;
		}
		break;
	case PUBKEY_DSA:
		if (!get_mpi(&pubkey->bn[DSA_P], &mem[off], pktlen, &off) ||
		    !get_mpi(&pubkey->bn[DSA_Q], &mem[off], pktlen, &off) ||
		    !get_mpi(&pubkey->bn[DSA_G], &mem[off], pktlen, &off) ||
		    !get_mpi(&pubkey->bn[DSA_Y], &mem[off], pktlen, &off)) {
			return 0;
		}
		break;
	default:
		printf("hi, different type of pubkey here %u\n", pubkey->keyalg);
		break;
	}
	if (pbn) {
		print_key_mpis(pubkey->bn, pubkey->keyalg);
	}
	return 1;
}

/* parse a user attribute */
static int
read_userattr(pgpv_userattr_t *userattr, uint8_t *p, size_t pktlen)
{
	pgpv_string_t	subattr;
	const int 	is_subpkt = 0;
	const int	indian = 1;
	unsigned	lenlen;
	uint16_t	imagelen;
	size_t		cc;

	userattr->len = pktlen;
	for (cc = 0 ; cc < pktlen ; cc += subattr.size + lenlen + 1) {
		subattr.size = get_pkt_len(1, p, 0, is_subpkt);
		lenlen = get_pkt_len_len(1, p, is_subpkt);
		if (lenlen > pktlen) {
			printf("weird lenlen %u\n", lenlen);
			return 0;
		}
		p += lenlen;
		if (*p++ != 1) {
			printf("image type (%u) != 1. weird packet\n", *(p - 1));
		}
		memcpy(&imagelen, p, sizeof(imagelen));
		if (!*(const char *)(const void *)&indian) {
			/* big endian - byteswap length */
			imagelen = (((unsigned)imagelen & 0xff) << 8) | (((unsigned)imagelen >> 8) & 0xff);
		}
		subattr.data = p + 3;
		p += subattr.size;
		ARRAY_APPEND(userattr->subattrs, subattr);
	}
	return 1;
}

#define LITDATA_BINARY	'b'
#define LITDATA_TEXT	't'
#define LITDATA_UTF8	'u'

/* parse literal packet */
static int
read_litdata(pgpv_t *pgp, pgpv_litdata_t *litdata, uint8_t *p, size_t size)
{
	size_t	cc;

	cc = 0;
	switch(litdata->format = p[cc++]) {
	case LITDATA_BINARY:
	case LITDATA_TEXT:
	case LITDATA_UTF8:
		litdata->namelen = 0;
		break;
	default:
		printf("weird litdata format %u\n", litdata->format);
		break;
	}
	litdata->namelen = p[cc++];
	litdata->filename = &p[cc];
	cc += litdata->namelen;
	litdata->secs = get_32(&p[cc]);
	cc += 4;
	litdata->s.data = &p[cc];
	litdata->len = litdata->s.size = size - cc;
	litdata->mem = ARRAY_COUNT(pgp->areas) - 1;
	litdata->offset = cc;
	return 1;
}

/* parse a single packet */
static int
read_pkt(pgpv_t *pgp, pgpv_mem_t *mem)
{
	const int	 isprimary = 1;
	pgpv_pkt_t	 pkt;
	pgpv_mem_t	*newmem;
	unsigned	 lenlen;
	uint8_t		 ispartial;
	size_t		 size;

	memset(&pkt, 0x0, sizeof(pkt));
	pkt.tag = mem->mem[mem->cc++];
	if (!(pkt.tag & PKT_ALWAYS_ON)) {
		printf("BAD PACKET - bit 7 not 1, offset %zu!\n", mem->cc - 1);
	}
	pkt.newfmt = (pkt.tag & PKT_NEWFMT_MASK);
	pkt.tag = (pkt.newfmt) ?
		(pkt.tag & PKT_NEWFMT_TAG_MASK) :
		(((unsigned)pkt.tag & PKT_OLDFMT_TAG_MASK) >> 2);
	ispartial = (pkt.newfmt && IS_PARTIAL(mem->mem[mem->cc]));
	pkt.s.size = get_pkt_len(pkt.newfmt, &mem->mem[mem->cc], mem->size - mem->cc, isprimary);
	lenlen = get_pkt_len_len(pkt.newfmt, &mem->mem[mem->cc], isprimary);
	pkt.offset = mem->cc;
	mem->cc += lenlen;
	pkt.mement = (uint8_t)(mem - ARRAY_ARRAY(pgp->areas));
	pkt.s.data = &mem->mem[mem->cc];
	if (strchr(mem->allowed, pkt.tag) == NULL) {
		printf("packet %d not allowed for operation %s\n", pkt.tag, pgp->op);
		return 0;
	}
	size = pkt.s.size;
	if (ispartial) {
		pkt.s.size = fixup_partials(pgp, &mem->mem[mem->cc - lenlen], pkt.s.size, mem->size, &size);
		newmem = &ARRAY_LAST(pgp->areas);
		pkt.mement = (uint8_t)(newmem - ARRAY_ARRAY(pgp->areas));
		pkt.s.data = newmem->mem;
		size -= 1;
	}
	switch(pkt.tag) {
	case SIGNATURE_PKT:
		if (!read_sigpkt(pgp, pkt.mement, &pkt.u.sigpkt, pkt.s.data, pkt.s.size)) {
			return 0;
		}
		break;
	case ONEPASS_SIGNATURE_PKT:
		read_onepass_sig(&pkt.u.onepass, pkt.s.data);
		break;
	case PUBKEY_PKT:
	case PUB_SUBKEY_PKT:
		break;
	case LITDATA_PKT:
		read_litdata(pgp, &pkt.u.litdata, pkt.s.data, pkt.s.size);
		break;
	case TRUST_PKT:
		pkt.u.trust.level = pkt.s.data[0];
		pkt.u.trust.amount = pkt.s.data[1];
		break;
	case USERID_PKT:
		pkt.u.userid.size = pkt.s.size;
		pkt.u.userid.data = pkt.s.data;
		break;
	case COMPRESSED_DATA_PKT:
		read_compressed(pgp, &pkt.u.compressed, pkt.s.data, pkt.s.size);
		ARRAY_APPEND(pgp->pkts, pkt);
		read_all_packets(pgp, &ARRAY_LAST(pgp->areas), pgp->op);
		break;
	case USER_ATTRIBUTE_PKT:
		read_userattr(&pkt.u.userattr, pkt.s.data, pkt.s.size);
		break;
	default:
		printf("hi, need to implement %d, offset %zu\n", pkt.tag, mem->cc);
		break;
	}
	mem->cc += size;
	if (pkt.tag != COMPRESSED_DATA_PKT) {
		/* compressed was added earlier to preserve pkt ordering */
		ARRAY_APPEND(pgp->pkts, pkt);
	}
	return 1;
}

/* checks the tag type of a packet */
static int
pkt_is(pgpv_t *pgp, int wanted)
{
	return (ARRAY_ELEMENT(pgp->pkts, pgp->pkt).tag == wanted);
}

/* checks the packet is a signature packet, and the signature type is the expected one */
static int
pkt_sigtype_is(pgpv_t *pgp, int wanted)
{
	if (!pkt_is(pgp, SIGNATURE_PKT)) {
		return 0;
	}
	return (ARRAY_ELEMENT(pgp->pkts, pgp->pkt).u.sigpkt.sig.type == wanted);
}

/* check for expected type of packet, and move to the next */
static int
pkt_accept(pgpv_t *pgp, int expected)
{
	int	got;

	if ((got = ARRAY_ELEMENT(pgp->pkts, pgp->pkt).tag) == expected) {
		pgp->pkt += 1;
		return 1;
	}
	printf("problem at token %zu, expcted %d, got %d\n", pgp->pkt, expected, got);
	return 0;
}

/* recognise signature (and trust) packet */
static int
recog_signature(pgpv_t *pgp, pgpv_signature_t *signature)
{
	if (!pkt_is(pgp, SIGNATURE_PKT)) {
		printf("recog_signature: not a signature packet\n");
		return 0;
	}
	memcpy(signature, &ARRAY_ELEMENT(pgp->pkts, pgp->pkt).u.sigpkt.sig, sizeof(*signature));
	pgp->pkt += 1;
	if (pkt_is(pgp, TRUST_PKT)) {
		pkt_accept(pgp, TRUST_PKT);
	}
	return 1;
}

/* recognise user id packet */
static int
recog_userid(pgpv_t *pgp, pgpv_signed_userid_t *userid)
{
	pgpv_signature_t	 signature;
	pgpv_pkt_t		*pkt;

	memset(userid, 0x0, sizeof(*userid));
	if (!pkt_is(pgp, USERID_PKT)) {
		printf("recog_userid: not %d\n", USERID_PKT);
		return 0;
	}
	pkt = &ARRAY_ELEMENT(pgp->pkts, pgp->pkt);
	userid->userid.size = pkt->s.size;
	userid->userid.data = pkt->s.data;
	pgp->pkt += 1;
	while (pkt_is(pgp, SIGNATURE_PKT)) {
		if (!recog_signature(pgp, &signature)) {
			printf("recog_userid: can't recognise signature/trust\n");
			return 0;
		}
		ARRAY_APPEND(userid->sigs, signature);
		if (signature.primary_userid) {
			userid->primary_userid = signature.primary_userid;
		}
		if (signature.revoked) {
			userid->revoked = signature.revoked;
		}
	}
	return 1;
}

/* recognise user attributes packet */
static int
recog_userattr(pgpv_t *pgp, pgpv_signed_userattr_t *userattr)
{
	pgpv_signature_t	 signature;

	memset(userattr, 0x0, sizeof(*userattr));
	if (!pkt_is(pgp, USER_ATTRIBUTE_PKT)) {
		printf("recog_userattr: not %d\n", USER_ATTRIBUTE_PKT);
		return 0;
	}
	userattr->userattr = ARRAY_ELEMENT(pgp->pkts, pgp->pkt).u.userattr;
	pgp->pkt += 1;
	while (pkt_is(pgp, SIGNATURE_PKT)) {
		if (!recog_signature(pgp, &signature)) {
			printf("recog_userattr: can't recognise signature/trust\n");
			return 0;
		}
		ARRAY_APPEND(userattr->sigs, signature);
		if (signature.revoked) {
			userattr->revoked = signature.revoked;
		}
	}
	return 1;
}

/* recognise a sub key */
static int
recog_subkey(pgpv_t *pgp, pgpv_signed_subkey_t *subkey)
{
	pgpv_signature_t	 signature;
	pgpv_pkt_t		*pkt;

	pkt = &ARRAY_ELEMENT(pgp->pkts, pgp->pkt);
	memset(subkey, 0x0, sizeof(*subkey));
	read_pubkey(&subkey->subkey, pkt->s.data, pkt->s.size, 0);
	pgp->pkt += 1;
	if (pkt_sigtype_is(pgp, SIGTYPE_KEY_REVOCATION) ||
	    pkt_sigtype_is(pgp, SIGTYPE_SUBKEY_REVOCATION) ||
	    pkt_sigtype_is(pgp, SIGTYPE_CERT_REVOCATION)) {
		recog_signature(pgp, &signature);
		subkey->revoc_self_sig = signature;
	}
	do {
		if (!pkt_is(pgp, SIGNATURE_PKT)) {
			printf("recog_subkey: not signature packet at %zu\n", pgp->pkt);
			return 0;
		}
		if (!recog_signature(pgp, &signature)) {
			printf("recog_subkey: bad signature/trust at %zu\n", pgp->pkt);
			return 0;
		}
		ARRAY_APPEND(subkey->sigs, signature);
		if (signature.keyexpiry) {
			/* XXX - check it's a good key expiry */
			subkey->subkey.expiry = signature.keyexpiry;
		}
	} while (pkt_is(pgp, SIGNATURE_PKT));
	return 1;
}

/* use a sparse map for the text strings here to save space */
static const char	*keyalgs[] = {
	"[Unknown]",
	"RSA (Encrypt or Sign)",
	"RSA (Encrypt Only)",
	"RSA (Sign Only)",
	"Elgamal (Encrypt Only)",
	"DSA",
	"Elliptic Curve",
	"ECDSA",
	"Elgamal (Encrypt or Sign)"
};

#define MAX_KEYALG	21

static const char *keyalgmap = "\0\01\02\03\0\0\0\0\0\0\0\0\0\0\0\0\04\05\06\07\010\011";

/* return human readable name for key algorithm */
static const char *
fmtkeyalg(uint8_t keyalg)
{
	return keyalgs[(uint8_t)keyalgmap[(keyalg >= MAX_KEYALG) ? 0 : keyalg]];
}

/* return the number of bits in the public key */
static unsigned
numkeybits(const pgpv_pubkey_t *pubkey)
{
	switch(pubkey->keyalg) {
	case PUBKEY_RSA_ENCRYPT_OR_SIGN:
	case PUBKEY_RSA_ENCRYPT:
	case PUBKEY_RSA_SIGN:
		return pubkey->bn[RSA_N].bits;
	case PUBKEY_DSA:
	case PUBKEY_ECDSA:
		return pubkey->bn[DSA_P].bits;
		//return BITS_TO_BYTES(pubkey->bn[DSA_Q].bits) * 64;
	case PUBKEY_ELGAMAL_ENCRYPT:
	case PUBKEY_ELGAMAL_ENCRYPT_OR_SIGN:
		return pubkey->bn[ELGAMAL_P].bits;
	default:
		return 0;
	}
}

/* print a public key */
static size_t
fmt_pubkey(char *s, size_t size, pgpv_pubkey_t *pubkey, const char *leader)
{
	size_t	cc;

	cc = snprintf(s, size, "%s %u/%s ", leader, numkeybits(pubkey), fmtkeyalg(pubkey->keyalg));
	cc += fmt_binary(&s[cc], size - cc, pubkey->keyid, PGPV_KEYID_LEN);
	cc += fmt_time(&s[cc], size - cc, " ", pubkey->birth, "", 0);
	if (pubkey->expiry) {
		cc += fmt_time(&s[cc], size - cc, " [Expiry ", pubkey->birth + pubkey->expiry, "]", 0);
	}
	cc += snprintf(&s[cc], size - cc, "\n");
	cc += fmt_fingerprint(&s[cc], size - cc, &pubkey->fingerprint, "fingerprint  ");
	return cc;
}

/* we add 1 to revocation value to denote compromised */
#define COMPROMISED	(0x02 + 1)

/* format a userid - used to order the userids when formatting */
static size_t
fmt_userid(char *s, size_t size, pgpv_primarykey_t *primary, uint8_t u)
{
	pgpv_signed_userid_t	*userid;

	userid = &ARRAY_ELEMENT(primary->signed_userids, u);
	return snprintf(s, size, "uid           %.*s%s\n",
			(int)userid->userid.size, userid->userid.data,
			(userid->revoked == COMPROMISED) ? " [COMPROMISED AND REVOKED]" :
			(userid->revoked) ? " [REVOKED]" : "");
}

/* format a trust sig - used to order the userids when formatting */
static size_t
fmt_trust(char *s, size_t size, pgpv_signed_userid_t *userid, uint32_t u)
{
	pgpv_signature_t	*sig;
	size_t			 cc;

	sig = &ARRAY_ELEMENT(userid->sigs, u);
	cc = snprintf(s, size, "trust          ");
	cc += fmt_binary(&s[cc], size - cc, sig->signer, 8);
	return cc + snprintf(&s[cc], size - cc, "\n");
}

/* print a primary key, per RFC 4880 */
static size_t
fmt_primary(char *s, size_t size, pgpv_primarykey_t *primary, unsigned subkey, const char *modifiers)
{
	pgpv_signed_userid_t	*userid;
	pgpv_pubkey_t		*pubkey;
	unsigned		 i;
	unsigned		 j;
	size_t			 cc;

	pubkey = (subkey == 0) ? &primary->primary : &ARRAY_ELEMENT(primary->signed_subkeys, subkey - 1).subkey;
	cc = fmt_pubkey(s, size, pubkey, "signature    ");
	cc += fmt_userid(&s[cc], size - cc, primary, primary->primary_userid);
	for (i = 0 ; i < ARRAY_COUNT(primary->signed_userids) ; i++) {
		if (i != primary->primary_userid) {
			cc += fmt_userid(&s[cc], size - cc, primary, i);
			if (strcasecmp(modifiers, "trust") == 0) {
				userid = &ARRAY_ELEMENT(primary->signed_userids, i);
				for (j = 0 ; j < ARRAY_COUNT(userid->sigs) ; j++) {
					cc += fmt_trust(&s[cc], size - cc, userid, j);
				}
			}
		}
	}
	if (strcasecmp(modifiers, "subkeys") == 0) {
		for (i = 0 ; i < ARRAY_COUNT(primary->signed_subkeys) ; i++) {
			cc += fmt_pubkey(&s[cc], size - cc, &ARRAY_ELEMENT(primary->signed_subkeys, i).subkey, "encryption");
		}
	}
	cc += snprintf(&s[cc], size - cc, "\n");
	return cc;
}


/* check the padding on the signature */
static int
rsa_padding_check_none(uint8_t *to, int tlen, const uint8_t *from, int flen, int num)
{
	USE_ARG(num);
	if (flen > tlen) {
		printf("from length larger than to length\n");
		return -1;
	}
	(void) memset(to, 0x0, (size_t)(tlen - flen));
	(void) memcpy(to + tlen - flen, from, (size_t)flen);
	return tlen;
}

#define RSA_MAX_MODULUS_BITS	16384
#define RSA_SMALL_MODULUS_BITS	3072
#define RSA_MAX_PUBEXP_BITS	64 /* exponent limit enforced for "large" modulus only */

/* check against the exponent/moudulo operation */
static int
lowlevel_rsa_public_check(const uint8_t *encbuf, int enclen, uint8_t *dec, const rsa_pubkey_t *rsa)
{
	uint8_t		*decbuf;
	BIGNUM		*decbn;
	BIGNUM		*encbn;
	int		 decbytes;
	int		 nbytes;
	int		 r;

	nbytes = 0;
	r = -1;
	decbuf = NULL;
	decbn = encbn = NULL;
	if (BN_num_bits(rsa->n) > RSA_MAX_MODULUS_BITS) {
		printf("rsa r modulus too large\n");
		goto err;
	}
	if (BN_cmp(rsa->n, rsa->e) <= 0) {
		printf("rsa r bad n value\n");
		goto err;
	}
	if (BN_num_bits(rsa->n) > RSA_SMALL_MODULUS_BITS &&
	    BN_num_bits(rsa->e) > RSA_MAX_PUBEXP_BITS) {
		printf("rsa r bad exponent limit\n");
		goto err;
	}
	nbytes = BN_num_bytes(rsa->n);
	if ((encbn = BN_new()) == NULL ||
	    (decbn = BN_new()) == NULL ||
	    (decbuf = calloc(1, (size_t)nbytes)) == NULL) {
		printf("allocation failure\n");
		goto err;
	}
	if (enclen > nbytes) {
		printf("rsa r > mod len\n");
		goto err;
	}
	if (BN_bin2bn(encbuf, enclen, encbn) == NULL) {
		printf("null encrypted BN\n");
		goto err;
	}
	if (BN_cmp(encbn, rsa->n) >= 0) {
		printf("rsa r data too large for modulus\n");
		goto err;
	}
	if (BN_mod_exp(decbn, encbn, rsa->e, rsa->n, NULL) < 0) {
		printf("BN_mod_exp < 0\n");
		goto err;
	}
	decbytes = BN_num_bytes(decbn);
	(void) BN_bn2bin(decbn, decbuf);
	if ((r = rsa_padding_check_none(dec, nbytes, decbuf, decbytes, 0)) < 0) {
		printf("rsa r padding check failed\n");
	}
err:
	BN_free(encbn);
	BN_free(decbn);
	if (decbuf != NULL) {
		(void) memset(decbuf, 0x0, nbytes);
		free(decbuf);
	}
	return r;
}

/* verify */
static int
rsa_public_decrypt(int enclen, const unsigned char *enc, unsigned char *dec, RSA *rsa, int padding)
{
	rsa_pubkey_t	pub;
	int		ret;

	if (enc == NULL || dec == NULL || rsa == NULL) {
		return 0;
	}
	USE_ARG(padding);
	(void) memset(&pub, 0x0, sizeof(pub));
	pub.n = BN_dup(rsa->n);
	pub.e = BN_dup(rsa->e);
	ret = lowlevel_rsa_public_check(enc, enclen, dec, &pub);
	BN_free(pub.n);
	BN_free(pub.e);
	return ret;
}

#define SUBKEY_LEN(x)	(80 + 80)
#define SIG_LEN		80
#define UID_LEN		80

/* return worst case number of bytes needed to format a primary key */
static size_t
estimate_primarykey_size(pgpv_primarykey_t *primary)
{
	size_t		cc;

	cc = SUBKEY_LEN("signature") +
		(ARRAY_COUNT(primary->signed_userids) * UID_LEN) +
		(ARRAY_COUNT(primary->signed_subkeys) * SUBKEY_LEN("encrypt uids"));
	return cc;
}

/* use public decrypt to verify a signature */
static int 
pgpv_rsa_public_decrypt(uint8_t *out, const uint8_t *in, size_t length, const pgpv_pubkey_t *pubkey)
{
	RSA            *orsa;
	int             n;

	if ((orsa = calloc(1, sizeof(*orsa))) == NULL) {
		return 0;
	}
	orsa->n = pubkey->bn[RSA_N].bn;
	orsa->e = pubkey->bn[RSA_E].bn;
	n = rsa_public_decrypt((int)length, in, out, orsa, RSA_NO_PADDING);
	orsa->n = orsa->e = NULL;
	free(orsa);
	return n;
}

/* verify rsa signature */
static int
rsa_verify(uint8_t *calculated, unsigned calclen, uint8_t hashalg, pgpv_bignum_t *bn, pgpv_pubkey_t *pubkey)
{
	unsigned	 prefixlen;
	unsigned	 decryptc;
	unsigned	 i;
	uint8_t		 decrypted[8192];
	uint8_t		 sigbn[8192];
	uint8_t		 prefix[64];
	size_t		 keysize;

	keysize = BITS_TO_BYTES(pubkey->bn[RSA_N].bits);
	BN_bn2bin(bn[RSA_SIG].bn, sigbn);
	decryptc = pgpv_rsa_public_decrypt(decrypted, sigbn, BITS_TO_BYTES(bn[RSA_SIG].bits), pubkey);
	if (decryptc != keysize || (decrypted[0] != 0 || decrypted[1] != 1)) {
		return 0;
	}
	if ((prefixlen = digest_get_prefix((unsigned)hashalg, prefix, sizeof(prefix))) == 0) {
		printf("rsa_verify: unknown hash algorithm: %d\n", hashalg);
		return 0;
	}
	for (i = 2 ; i < keysize - prefixlen - calclen - 1 ; i++) {
		if (decrypted[i] != 0xff) {
			return 0;
		}
	}
	if (decrypted[i++] != 0x0) {
		return 0;
	}
	if (memcmp(&decrypted[i], prefix, prefixlen) != 0) {
		printf("rsa_verify: wrong hash algorithm\n");
		return 0;
	}
	return memcmp(&decrypted[i + prefixlen], calculated, calclen) == 0;
}

/* return 1 if bn <= 0 */
static int
bignum_is_bad(BIGNUM *bn)
{
	return BN_is_zero(bn) || BN_is_negative(bn);
}

#define BAD_BIGNUM(s, k)	\
	(bignum_is_bad((s)->bn) || BN_cmp((s)->bn, (k)->bn) >= 0)

#ifndef DSA_MAX_MODULUS_BITS
#define DSA_MAX_MODULUS_BITS      10000
#endif

/* verify DSA signature */
static int
verify_dsa_sig(uint8_t *calculated, unsigned calclen, pgpv_bignum_t *sig, pgpv_pubkey_t *pubkey)
{
	unsigned	  qbits;
	uint8_t		  calcnum[128];
	uint8_t		  signum[128];
	BIGNUM		 *M;
	BIGNUM		 *W;
	BIGNUM		 *t1;
	int		  ret;

	if (pubkey->bn[DSA_P].bn == NULL ||
	    pubkey->bn[DSA_Q].bn == NULL ||
	    pubkey->bn[DSA_G].bn == NULL) {
		return 0;
	}
	M = W = t1 = NULL;
	qbits = pubkey->bn[DSA_Q].bits;
	switch(qbits) {
	case 160:
	case 224:
	case 256:
		break;
	default:
		printf("dsa: bad # of Q bits\n");
		return 0;
	}
	if (pubkey->bn[DSA_P].bits > DSA_MAX_MODULUS_BITS) {
		printf("dsa: p too large\n");
		return 0;
	}
	if (calclen > SHA256_DIGEST_LENGTH) {
		printf("dsa: digest too long\n");
		return 0;
	}
	ret = 0;
	if ((M = BN_new()) == NULL || (W = BN_new()) == NULL || (t1 = BN_new()) == NULL ||
	    BAD_BIGNUM(&sig[DSA_R], &pubkey->bn[DSA_Q]) ||
	    BAD_BIGNUM(&sig[DSA_S], &pubkey->bn[DSA_Q]) ||
	    BN_mod_inverse(W, sig[DSA_S].bn, pubkey->bn[DSA_Q].bn, NULL) == NULL) {
		goto done;
	}
	if (calclen > qbits / 8) {
		calclen = qbits / 8;
	}
	if (BN_bin2bn(calculated, (int)calclen, M) == NULL ||
	    !BN_mod_mul(M, M, W, pubkey->bn[DSA_Q].bn, NULL) ||
	    !BN_mod_mul(W, sig[DSA_R].bn, W, pubkey->bn[DSA_Q].bn, NULL) ||
	    !BN_mod_exp(t1, pubkey->bn[DSA_G].bn, M, pubkey->bn[DSA_P].bn, NULL) ||
	    !BN_mod_exp(W, pubkey->bn[DSA_Y].bn, W, pubkey->bn[DSA_P].bn, NULL) ||
	    !BN_mod_mul(t1, t1, W, pubkey->bn[DSA_P].bn, NULL) ||
	    !BN_div(NULL, t1, t1, pubkey->bn[DSA_Q].bn, NULL)) {
		goto done;
	}
	/* only compare the first q bits */
	BN_bn2bin(t1, calcnum);
	BN_bn2bin(sig[DSA_R].bn, signum);
	ret = memcmp(calcnum, signum, BITS_TO_BYTES(qbits)) == 0;
done:
	if (M) {
		BN_free(M);
	}
	if (W) {
		BN_free(W);
	}
	if (t1) {
		BN_free(t1);
	}
	return ret;
}

#define TIME_SNPRINTF(_cc, _buf, _size, _fmt, _val)	do {		\
	time_t	 _t;							\
	char	*_s;							\
									\
	_t = _val;							\
	_s = ctime(&_t);						\
	_cc += snprintf(_buf, _size, _fmt, _s);				\
} while(/*CONSTCOND*/0)

/* check dates on signature and key are valid */
static size_t
valid_dates(pgpv_signature_t *signature, pgpv_pubkey_t *pubkey, char *buf, size_t size)
{
	time_t	 now;
	time_t	 t;
	size_t	 cc;

	cc = 0;
	if (signature->birth < pubkey->birth) {
		TIME_SNPRINTF(cc, buf, size, "Signature time (%.24s) was before pubkey creation ", signature->birth);
		TIME_SNPRINTF(cc, &buf[cc], size - cc, "(%s)\n", pubkey->birth);
		return cc;
	}
	now = time(NULL);
	if (signature->expiry != 0) {
		if ((t = signature->birth + signature->expiry) < now) {
			TIME_SNPRINTF(cc, buf, size, "Signature expired on %.24s\n", t);
			return cc;
		}
	}
	if (now < signature->birth) {
		TIME_SNPRINTF(cc, buf, size, "Signature not valid before %.24s\n", signature->birth);
		return cc;
	}
	return 0;
}

/* check if the signing key has expired */
static int
key_expired(pgpv_pubkey_t *pubkey, char *buf, size_t size)
{
	time_t	 now;
	time_t	 t;
	size_t	 cc;

	now = time(NULL);
	cc = 0;
	if (pubkey->expiry != 0) {
		if ((t = pubkey->birth + pubkey->expiry) < now) {
			TIME_SNPRINTF(cc, buf, size, "Pubkey expired on %.24s\n", t);
			return (int)cc;
		}
	}
	if (now < pubkey->birth) {
		TIME_SNPRINTF(cc, buf, size, "Pubkey not valid before %.24s\n", pubkey->birth);
		return (int)cc;
	}
	return 0;
}

/* find the leading onepass packet */
static size_t
find_onepass(pgpv_cursor_t *cursor, size_t datastart)
{
	size_t	pkt;

	for (pkt = datastart ; pkt < ARRAY_COUNT(cursor->pgp->pkts) ; pkt++) {
		if (ARRAY_ELEMENT(cursor->pgp->pkts, pkt).tag == ONEPASS_SIGNATURE_PKT) {
			return pkt + 1;
		}
	}
	snprintf(cursor->why, sizeof(cursor->why), "No signature to verify");
	return 0;
}

static const char	*armor_begins[] = {
	"-----BEGIN PGP SIGNED MESSAGE-----\n",
	"-----BEGIN PGP MESSAGE-----\n",
	NULL
};

/* return non-zero if the buf introduces an armored message */
static int
is_armored(const char *buf, size_t size)
{
	const char	**arm;
	const char	 *nl;
	size_t		  n;

	if ((nl = memchr(buf, '\n', size)) == NULL) {
		return 0;
	}
	n = (size_t)(nl - buf);
	for (arm = armor_begins ; *arm ; arm++) {
		if (strncmp(buf, *arm, n) == 0) {
			return 1;
		}
	}
	return 0;
}

/* find first occurrence of pat binary string in block */
static void *
find_bin_string(const void *blockarg, size_t blen, const void *pat, size_t plen)
{
	const uint8_t	*block;
	const uint8_t	*bp;

	if (plen == 0) {
		return __UNCONST(blockarg);
	}
	if (blen < plen) {
		return NULL;
	}
	for (bp = block = blockarg ; (size_t)(bp - block) < blen - plen + 1 ; bp++) {
		if (memcmp(bp, pat, plen) == 0) {
			return __UNCONST(bp);
		}
	}
	return NULL;
}

#define SIGSTART	"-----BEGIN PGP SIGNATURE-----\n"
#define SIGEND		"-----END PGP SIGNATURE-----\n"

/* for ascii armor, we don't get a onepass packet - make one */
static const char 	*cons_onepass = "\304\015\003\0\0\0\0\377\377\377\377\377\377\377\377\1";

/* read ascii armor */
static int
read_ascii_armor(pgpv_cursor_t *cursor, pgpv_mem_t *mem, const char *filename)
{
	pgpv_onepass_t	*onepass;
	pgpv_sigpkt_t	*sigpkt;
	pgpv_pkt_t	 litdata;
	uint8_t		 binsig[8192];
	uint8_t		*datastart;
	uint8_t		*sigend;
	uint8_t		*p;
	size_t		 binsigsize;

	/* cons up litdata pkt */
	memset(&litdata, 0x0, sizeof(litdata));
	litdata.u.litdata.mem = ARRAY_COUNT(cursor->pgp->areas) - 1;
	p = mem->mem;
	/* jump over signed message line */
	if ((p = find_bin_string(mem->mem, mem->size, "\n\n",  2)) == NULL) {
		snprintf(cursor->why, sizeof(cursor->why), "malformed armor at offset 0");
		return 0;
	}
	p += 2;
	litdata.tag = LITDATA_PKT;
	litdata.s.data = p;
	litdata.u.litdata.offset = (size_t)(p - mem->mem);
	litdata.u.litdata.filename = (uint8_t *)strdup(filename);
	if ((p = find_bin_string(datastart = p, mem->size - litdata.offset, SIGSTART, strlen(SIGSTART))) == NULL) {
		snprintf(cursor->why, sizeof(cursor->why),
			"malformed armor - no sig - at %zu", (size_t)(p - mem->mem));
		return 0;
	}
	litdata.u.litdata.len = litdata.s.size = (size_t)(p - datastart);
	p += strlen(SIGSTART);
	if ((p = find_bin_string(p, mem->size, "\n\n",  2)) == NULL) {
		snprintf(cursor->why, sizeof(cursor->why),
			"malformed armed signature at %zu", (size_t)(p - mem->mem));
		return 0;
	}
	p += 2;
	sigend = find_bin_string(p, mem->size, SIGEND, strlen(SIGEND));
	binsigsize = b64decode((char *)p, (size_t)(sigend - p), binsig, sizeof(binsig));

	read_binary_memory(cursor->pgp, "signature", cons_onepass, 15);
	ARRAY_APPEND(cursor->pgp->pkts, litdata);
	read_binary_memory(cursor->pgp, "signature", binsig, binsigsize - 3);
	/* XXX - hardwired - 3 is format and length */

	/* fix up packets in the packet array now we have them there */
	onepass = &ARRAY_ELEMENT(cursor->pgp->pkts, ARRAY_COUNT(cursor->pgp->pkts) - 1 - 2).u.onepass;
	sigpkt = &ARRAY_LAST(cursor->pgp->pkts).u.sigpkt;
	memcpy(onepass->keyid, sigpkt->sig.signer, sizeof(onepass->keyid));
	onepass->hashalg = sigpkt->sig.hashalg;
	onepass->keyalg = sigpkt->sig.keyalg;
	return 1;
}

/* read ascii armor from a file */
static int
read_ascii_armor_file(pgpv_cursor_t *cursor, const char *filename)
{
	/* cons up litdata pkt */
	read_file(cursor->pgp, filename);
	return read_ascii_armor(cursor, &ARRAY_LAST(cursor->pgp->areas), filename);
}

/* read ascii armor from memory */
static int
read_ascii_armor_memory(pgpv_cursor_t *cursor, const void *p, size_t size)
{
	pgpv_mem_t	*mem;

	/* cons up litdata pkt */
	ARRAY_EXPAND(cursor->pgp->areas);
	ARRAY_COUNT(cursor->pgp->areas) += 1;
	mem = &ARRAY_LAST(cursor->pgp->areas);
	memset(mem, 0x0, sizeof(*mem));
	mem->size = size;
	mem->mem = __UNCONST(p);
	mem->dealloc = 0;
	return read_ascii_armor(cursor, mem, "[stdin]");
}

/* set up the data to verify */
static int
setup_data(pgpv_cursor_t *cursor, pgpv_t *pgp, const void *p, ssize_t size)
{
	FILE		*fp;
	char		 buf[BUFSIZ];

	if (cursor == NULL || pgp == NULL || p == NULL) {
		return 0;
	}
	memset(cursor, 0x0, sizeof(*cursor));
	ARRAY_APPEND(pgp->datastarts, pgp->pkt);
	cursor->pgp = pgp;
	if (size < 0) {
		/* we have a file name in p */
		if ((fp = fopen(p, "r")) == NULL) {
			snprintf(cursor->why, sizeof(cursor->why), "No such file '%s'", (const char *)p);
			return 0;
		}
		if (fgets(buf, (int)sizeof(buf), fp) == NULL) {
			fclose(fp);
			snprintf(cursor->why, sizeof(cursor->why), "can't read file '%s'", (const char *)p);
			return 0;
		}
		if (is_armored(buf, sizeof(buf))) {
			read_ascii_armor_file(cursor, p);
		} else {
			read_binary_file(pgp, "signature", "%s", (const char *)p);
		}
		fclose(fp);
	} else {
		if (is_armored(p, (size_t)size)) {
			read_ascii_armor_memory(cursor, p, (size_t)size);
		} else {
			read_binary_memory(pgp, "signature", p, (size_t)size);
		}
	}
	return 1;
}

/* get the data and size from litdata packet */
static uint8_t *
get_literal_data(pgpv_cursor_t *cursor, pgpv_litdata_t *litdata, size_t *size)
{
	pgpv_mem_t	*mem;

	if (litdata->s.data == NULL && litdata->s.size == 0) {
		mem = &ARRAY_ELEMENT(cursor->pgp->areas, litdata->mem);
		*size = litdata->len;
		return &mem->mem[litdata->offset]; 
	}
	*size = litdata->s.size;
	return litdata->s.data;
}

/*
RFC 4880 describes the structure of v4 keys as:

           Primary-Key
              [Revocation Self Signature]
              [Direct Key Signature...]
               User ID [Signature ...]
              [User ID [Signature ...] ...]
              [User Attribute [Signature ...] ...]
              [[Subkey [Binding-Signature-Revocation]
                      Primary-Key-Binding-Signature] ...]

and that's implemented below as a recursive descent parser.
It has had to be modified, though: see the comment

	some keys out there have user ids where they shouldn't

to look like:

           Primary-Key
              [Revocation Self Signature]
              [Direct Key Signature...]
              [User ID [Signature ...]
                 [User ID [Signature ...] ...]
                 [User Attribute [Signature ...] ...]
                 [Subkey [Binding-Signature-Revocation]
                        Primary-Key-Binding-Signature] ...]

to accommodate keyrings set up by gpg
*/

/* recognise a primary key */
static int
recog_primary_key(pgpv_t *pgp, pgpv_primarykey_t *primary)
{
	pgpv_signed_userattr_t	 userattr;
	pgpv_signed_userid_t	 userid;
	pgpv_signed_subkey_t	 subkey;
	pgpv_signature_t	 signature;
	pgpv_pkt_t		*pkt;

	pkt = &ARRAY_ELEMENT(pgp->pkts, pgp->pkt);
	memset(primary, 0x0, sizeof(*primary));
	read_pubkey(&primary->primary, pkt->s.data, pkt->s.size, 0);
	pgp->pkt += 1;
	if (pkt_sigtype_is(pgp, SIGTYPE_KEY_REVOCATION)) {
		if (!recog_signature(pgp, &primary->revoc_self_sig)) {
			printf("recog_primary_key: no signature/trust at PGPV_SIGTYPE_KEY_REVOCATION\n");
			return 0;
		}
	}
	while (pkt_sigtype_is(pgp, SIGTYPE_DIRECT_KEY)) {
		if (!recog_signature(pgp, &signature)) {
			printf("recog_primary_key: no signature/trust at PGPV_SIGTYPE_DIRECT_KEY\n");
			return 0;
		}
		if (signature.keyexpiry) {
			/* XXX - check it's a good key expiry */
			primary->primary.expiry = signature.keyexpiry;
		}
		ARRAY_APPEND(primary->direct_sigs, signature);
	}
	/* some keys out there have user ids where they shouldn't */
	do {
		if (!recog_userid(pgp, &userid)) {
			printf("recog_primary_key: not userid\n");
			return 0;
		}
		ARRAY_APPEND(primary->signed_userids, userid);
		if (userid.primary_userid) {
			primary->primary_userid = ARRAY_COUNT(primary->signed_userids) - 1;
		}
		while (pkt_is(pgp, USERID_PKT)) {
			if (!recog_userid(pgp, &userid)) {
				printf("recog_primary_key: not signed secondary userid\n");
				return 0;
			}
			ARRAY_APPEND(primary->signed_userids, userid);
			if (userid.primary_userid) {
				primary->primary_userid = ARRAY_COUNT(primary->signed_userids) - 1;
			}
		}
		while (pkt_is(pgp, USER_ATTRIBUTE_PKT)) {
			if (!recog_userattr(pgp, &userattr)) {
				printf("recog_primary_key: not signed user attribute\n");
				return 0;
			}
			ARRAY_APPEND(primary->signed_userattrs, userattr);
		}
		while (pkt_is(pgp, PUB_SUBKEY_PKT)) {
			if (!recog_subkey(pgp, &subkey)) {
				printf("recog_primary_key: not signed public subkey\n");
				return 0;
			}
			calc_keyid(&subkey.subkey, "sha1");
			ARRAY_APPEND(primary->signed_subkeys, subkey);
		}
	} while (pgp->pkt < ARRAY_COUNT(pgp->pkts) && pkt_is(pgp, USERID_PKT));
	primary->fmtsize = estimate_primarykey_size(primary);
	return 1;
}

/* parse all of the packets for a given operation */
static int
read_all_packets(pgpv_t *pgp, pgpv_mem_t *mem, const char *op)
{
	pgpv_primarykey_t	 primary;

	if (op == NULL) {
		return 0;
	}
	if (strcmp(pgp->op = op, "pubring") == 0) {
		mem->allowed = PUBRING_ALLOWED;
		/* pubrings have thousands of small packets */
		ARRAY_EXPAND_SIZED(pgp->pkts, 0, 5000);
	} else if (strcmp(op, "signature") == 0) {
		mem->allowed = SIGNATURE_ALLOWED;
	} else {
		mem->allowed = "";
	}
	for (mem->cc = 0; mem->cc < mem->size ; ) {
		if (!read_pkt(pgp, mem)) {
			return 0;
		}
	}
	if (strcmp(op, "pubring") == 0) {
		for (pgp->pkt = 0; pgp->pkt < ARRAY_COUNT(pgp->pkts) && recog_primary_key(pgp, &primary) ; ) {
			calc_keyid(&primary.primary, "sha1");
			ARRAY_APPEND(pgp->primaries, primary);
		}
		if (pgp->pkt < ARRAY_COUNT(pgp->pkts)) {
			printf("short pubring recognition???\n");
		}
	}
	pgp->pkt = ARRAY_COUNT(pgp->pkts);
	return 1;
}

/* create a filename, read it, and then parse according to "op" */
static int
read_binary_file(pgpv_t *pgp, const char *op, const char *fmt, ...)
{
	va_list	args;
	char	buf[1024];

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (!read_file(pgp, buf)) {
		return 0;
	}
	return read_all_packets(pgp, &ARRAY_LAST(pgp->areas), op);
}

/* get a bignum from the buffer gap */
static int
getbignum(pgpv_bignum_t *bignum, bufgap_t *bg, char *buf, const char *header)
{
	uint32_t	 len;

	USE_ARG(header);
	(void) bufgap_getbin(bg, &len, sizeof(len));
	len = pgp_ntoh32(len);
	(void) bufgap_seek(bg, sizeof(len), BGFromHere, BGByte);
	(void) bufgap_getbin(bg, buf, len);
	bignum->bn = BN_bin2bn((const uint8_t *)buf, (int)len, NULL);
	bignum->bits = BN_num_bits(bignum->bn);
	(void) bufgap_seek(bg, len, BGFromHere, BGByte);
	return 1;
}

/* structure for searching for constant strings */
typedef struct str_t {
	const char	*s;		/* string */
	size_t		 len;		/* its length */
	int		 type;		/* return type */
} str_t;

static str_t	pkatypes[] = {
	{	"ssh-rsa",	7,	PUBKEY_RSA_SIGN	},
	{	"ssh-dss",	7,	PUBKEY_DSA	},
	{	"ssh-dsa",	7,	PUBKEY_DSA	},
	{	NULL,		0,	0		}
};

/* look for a string in the given array */
static int
findstr(str_t *array, const char *name)
{
	str_t	*sp;

	for (sp = array ; sp->s ; sp++) {
		if (strncmp(name, sp->s, sp->len) == 0) {
			return sp->type;
		}
	}
	return -1;
}

/* read public key from the ssh pubkey file */
static __printflike(3, 4) int
read_ssh_file(pgpv_t *pgp, pgpv_primarykey_t *primary, const char *fmt, ...)
{
	pgpv_signed_userid_t	 userid;
	pgpv_pubkey_t		*pubkey;
	struct stat		 st;
	bufgap_t		 bg;
	uint32_t		 len;
	int64_t			 off;
	va_list			 args;
	char			 hostname[256];
	char			 owner[256];
	char			*space;
	char		 	*buf;
	char		 	*bin;
	char			 f[1024];
	int			 ok;
	int			 cc;

	USE_ARG(pgp);
	memset(primary, 0x0, sizeof(*primary));
	(void) memset(&bg, 0x0, sizeof(bg));
	va_start(args, fmt);
	vsnprintf(f, sizeof(f), fmt, args);
	va_end(args);
	if (!bufgap_open(&bg, f)) {
		(void) fprintf(stderr, "pgp_ssh2pubkey: can't open '%s'\n", f);
		return 0;
	}
	(void)stat(f, &st);
	if ((buf = calloc(1, (size_t)st.st_size)) == NULL) {
		(void) fprintf(stderr, "can't calloc %zu bytes for '%s'\n", (size_t)st.st_size, f);
		bufgap_close(&bg);
		return 0;
	}
	if ((bin = calloc(1, (size_t)st.st_size)) == NULL) {
		(void) fprintf(stderr, "can't calloc %zu bytes for '%s'\n", (size_t)st.st_size, f);
		(void) free(buf);
		bufgap_close(&bg);
		return 0;
	}

	/* move past ascii type of key */
	while (bufgap_peek(&bg, 0) != ' ') {
		if (!bufgap_seek(&bg, 1, BGFromHere, BGByte)) {
			(void) fprintf(stderr, "bad key file '%s'\n", f);
			(void) free(buf);
			bufgap_close(&bg);
			return 0;
		}
	}
	if (!bufgap_seek(&bg, 1, BGFromHere, BGByte)) {
		(void) fprintf(stderr, "bad key file '%s'\n", f);
		(void) free(buf);
		bufgap_close(&bg);
		return 0;
	}
	off = bufgap_tell(&bg, BGFromBOF, BGByte);

	if (bufgap_size(&bg, BGByte) - off < 10) {
		(void) fprintf(stderr, "bad key file '%s'\n", f);
		(void) free(buf);
		bufgap_close(&bg);
		return 0;
	}

	/* convert from base64 to binary */
	cc = bufgap_getbin(&bg, buf, (size_t)bg.bcc);
	if ((space = strchr(buf, ' ')) != NULL) {
		cc = (int)(space - buf);
	}
	cc = frombase64(bin, buf, (size_t)cc, 0);
	bufgap_delete(&bg, (uint64_t)bufgap_tell(&bg, BGFromEOF, BGByte));
	bufgap_insert(&bg, bin, cc);
	bufgap_seek(&bg, off, BGFromBOF, BGByte);

	/* get the type of key */
	(void) bufgap_getbin(&bg, &len, sizeof(len));
	len = pgp_ntoh32(len);
	if (len >= st.st_size) {
		(void) fprintf(stderr, "bad public key file '%s'\n", f);
		return 0;
	}
	(void) bufgap_seek(&bg, sizeof(len), BGFromHere, BGByte);
	(void) bufgap_getbin(&bg, buf, len);
	(void) bufgap_seek(&bg, len, BGFromHere, BGByte);

	pubkey = &primary->primary;
	pubkey->hashalg = digest_get_alg("sha256"); /* gets fixed up later */
	pubkey->version = 4;
	pubkey->birth = 0; /* gets fixed up later */
	/* get key type */
	ok = 1;
	switch (pubkey->keyalg = findstr(pkatypes, buf)) {
	case PUBKEY_RSA_ENCRYPT_OR_SIGN:
	case PUBKEY_RSA_SIGN:
		getbignum(&pubkey->bn[RSA_E], &bg, buf, "RSA E");
		getbignum(&pubkey->bn[RSA_N], &bg, buf, "RSA N");
		break;
	case PUBKEY_DSA:
		getbignum(&pubkey->bn[DSA_P], &bg, buf, "DSA P");
		getbignum(&pubkey->bn[DSA_Q], &bg, buf, "DSA Q");
		getbignum(&pubkey->bn[DSA_G], &bg, buf, "DSA G");
		getbignum(&pubkey->bn[DSA_Y], &bg, buf, "DSA Y");
		break;
	default:
		(void) fprintf(stderr, "Unrecognised pubkey type %d for '%s'\n",
				pubkey->keyalg, f);
		ok = 0;
		break;
	}

	/* check for stragglers */
	if (ok && bufgap_tell(&bg, BGFromEOF, BGByte) > 0) {
		printf("%"PRIi64" bytes left\n", bufgap_tell(&bg, BGFromEOF, BGByte));
		printf("[%s]\n", bufgap_getstr(&bg));
		ok = 0;
	}
	if (ok) {
		memset(&userid, 0x0, sizeof(userid));
		(void) gethostname(hostname, sizeof(hostname));
		if (strlen(space + 1) - 1 == 0) {
			(void) snprintf(owner, sizeof(owner), "<root@%s>",
					hostname);
		} else {
			(void) snprintf(owner, sizeof(owner), "<%.*s>",
				(int)strlen(space + 1) - 1,
				space + 1);
		}
		calc_keyid(pubkey, "sha1");
		userid.userid.size = asprintf((char **)(void *)&userid.userid.data,
						"%s (%s) %s",
						hostname,
						f,
						owner);
		ARRAY_APPEND(primary->signed_userids, userid);
		primary->fmtsize = estimate_primarykey_size(primary) + 1024;
	}
	(void) free(bin);
	(void) free(buf);
	bufgap_close(&bg);
	return ok;
}

/* parse memory according to "op" */
static int
read_binary_memory(pgpv_t *pgp, const char *op, const void *memory, size_t size)
{
	pgpv_mem_t	*mem;

	ARRAY_EXPAND(pgp->areas);
	ARRAY_COUNT(pgp->areas) += 1;
	mem = &ARRAY_LAST(pgp->areas);
	memset(mem, 0x0, sizeof(*mem));
	mem->size = size;
	mem->mem = __UNCONST(memory);
	mem->dealloc = 0;
	return read_all_packets(pgp, mem, op);
}

/* fixup the detached signature packets */
static int
fixup_detached(pgpv_cursor_t *cursor, const char *f)
{
	pgpv_onepass_t	*onepass;
	const char	*dot;
	pgpv_pkt_t	 sigpkt;
	pgpv_pkt_t	 litdata;
	pgpv_mem_t	*mem;
	size_t		 el;
	char		 original[MAXPATHLEN];

	/* cons up litdata pkt */
	if ((dot = strrchr(f, '.')) == NULL || strcasecmp(dot, ".sig") != 0) {
		printf("weird filename '%s'\n", f);
		return 0;
	}
	/* hold sigpkt in a temp var while we insert onepass and litdata */
	el = ARRAY_COUNT(cursor->pgp->pkts) - 1;
	sigpkt = ARRAY_ELEMENT(cursor->pgp->pkts, el);
	ARRAY_DELETE(cursor->pgp->pkts, el);
	ARRAY_EXPAND(cursor->pgp->pkts);
	/* get onepass packet, append to packets */
	read_binary_memory(cursor->pgp, "signature", cons_onepass, 15);
	onepass = &ARRAY_ELEMENT(cursor->pgp->pkts, el).u.onepass;
	/* read the original file into litdata */
	snprintf(original, sizeof(original), "%.*s", (int)(dot - f), f);
	if (!read_file(cursor->pgp, original)) {
		printf("can't read file '%s'\n", original);
		return 0;
	}
	memset(&litdata, 0x0, sizeof(litdata));
	mem = &ARRAY_LAST(cursor->pgp->areas);
	litdata.tag = LITDATA_PKT;
	litdata.s.data = mem->mem;
	litdata.u.litdata.format = LITDATA_BINARY;
	litdata.u.litdata.offset = 0;
	litdata.u.litdata.filename = (uint8_t *)strdup(original);
	litdata.u.litdata.mem = ARRAY_COUNT(cursor->pgp->areas) - 1;
	litdata.u.litdata.len = litdata.s.size = mem->size;
	ARRAY_APPEND(cursor->pgp->pkts, litdata);
	ARRAY_APPEND(cursor->pgp->pkts, sigpkt);
	memcpy(onepass->keyid, sigpkt.u.sigpkt.sig.signer, sizeof(onepass->keyid));
	onepass->hashalg = sigpkt.u.sigpkt.sig.hashalg;
	onepass->keyalg = sigpkt.u.sigpkt.sig.keyalg;
	return 1;
}

/* match the calculated signature against the one in the signature packet */
static int
match_sig(pgpv_cursor_t *cursor, pgpv_signature_t *signature, pgpv_pubkey_t *pubkey, uint8_t *data, size_t size)
{
	unsigned	calclen;
	uint8_t		calculated[64];
	int		match;

	calclen = pgpv_digest_memory(calculated, sizeof(calculated),
		data, size,
		get_ref(&signature->hashstart), signature->hashlen,
		(signature->type == SIGTYPE_TEXT) ? 't' : 'b');
	if (ALG_IS_RSA(signature->keyalg)) {
		match = rsa_verify(calculated, calclen, signature->hashalg, signature->bn, pubkey);
	} else if (ALG_IS_DSA(signature->keyalg)) {
		match = verify_dsa_sig(calculated, calclen, signature->bn, pubkey);
	} else {
		snprintf(cursor->why, sizeof(cursor->why), "Signature type %u not recognised", signature->keyalg);
		return 0;
	}
	if (!match && signature->type == SIGTYPE_TEXT) {
		/* second try for cleartext data, ignoring trailing whitespace */
		calclen = pgpv_digest_memory(calculated, sizeof(calculated),
			data, size,
			get_ref(&signature->hashstart), signature->hashlen, 'w');
		if (ALG_IS_RSA(signature->keyalg)) {
			match = rsa_verify(calculated, calclen, signature->hashalg, signature->bn, pubkey);
		} else if (ALG_IS_DSA(signature->keyalg)) {
			match = verify_dsa_sig(calculated, calclen, signature->bn, pubkey);
		}
	}
	if (!match) {
		snprintf(cursor->why, sizeof(cursor->why), "Signature on data did not match");
		return 0;
	}
	if (valid_dates(signature, pubkey, cursor->why, sizeof(cursor->why)) > 0) {
		return 0;
	}
	if (key_expired(pubkey, cursor->why, sizeof(cursor->why))) {
		return 0;
	}
	if (signature->revoked) {
		snprintf(cursor->why, sizeof(cursor->why), "Signature was revoked");
		return 0;
	}
	return 1;
}

/* check return value from getenv */
static const char *
nonnull_getenv(const char *key)
{
	char	*value;

	return ((value = getenv(key)) == NULL) ? "" : value;
}

/************************************************************************/
/* start of exported functions */
/************************************************************************/

/* close all stuff */
int
pgpv_close(pgpv_t *pgp)
{
	unsigned	i;

	if (pgp == NULL) {
		return 0;
	}
	for (i = 0 ; i < ARRAY_COUNT(pgp->areas) ; i++) {
		if (ARRAY_ELEMENT(pgp->areas, i).size > 0) {
			closemem(&ARRAY_ELEMENT(pgp->areas, i));
		}
	}
	return 1;
}

#define NO_SUBKEYS	0

/* return the formatted entry for the primary key desired */
size_t
pgpv_get_entry(pgpv_t *pgp, unsigned ent, char **s, const char *modifiers)
{
	unsigned	subkey;
	unsigned	prim;
	size_t		cc;

	prim = ((ent >> 8) & 0xffffff);
	subkey = (ent & 0xff);
	if (s == NULL || pgp == NULL || prim >= ARRAY_COUNT(pgp->primaries)) {
		return 0;
	}
	*s = NULL;
	cc = ARRAY_ELEMENT(pgp->primaries, prim).fmtsize;
	if (modifiers == NULL || (strcasecmp(modifiers, "trust") != 0 && strcasecmp(modifiers, "subkeys") != 0)) {
		modifiers = "no-subkeys";
	}
	if (strcasecmp(modifiers, "trust") == 0) {
		cc *= 2048;
	}
	if ((*s = calloc(1, cc)) == NULL) {
		return 0;
	}
	return fmt_primary(*s, cc, &ARRAY_ELEMENT(pgp->primaries, prim), subkey, modifiers);
}

/* fixup key id, with birth, keyalg and hashalg value from signature */
static int
fixup_ssh_keyid(pgpv_t *pgp, pgpv_signature_t *signature, const char *hashtype)
{
	pgpv_pubkey_t	*pubkey;
	unsigned	 i;

	for (i = 0 ; i < ARRAY_COUNT(pgp->primaries) ; i++) {
		pubkey = &ARRAY_ELEMENT(pgp->primaries, i).primary;
		pubkey->keyalg = signature->keyalg;
		calc_keyid(pubkey, hashtype);
	}
	return 1;
}

/* find key id */
static int
find_keyid(pgpv_t *pgp, const char *strkeyid, uint8_t *keyid, unsigned *sub)
{
	pgpv_signed_subkey_t	*subkey;
	pgpv_primarykey_t	*prim;
	unsigned		 i;
	unsigned		 j;
	uint8_t			 binkeyid[PGPV_KEYID_LEN];
	size_t			 off;
	size_t			 cmp;

	if (strkeyid == NULL && keyid == NULL) {
		return 0;
	}
	if (strkeyid) {
		str_to_keyid(strkeyid, binkeyid);
		cmp = strlen(strkeyid) / 2;
	} else {
		memcpy(binkeyid, keyid, sizeof(binkeyid));
		cmp = PGPV_KEYID_LEN;
	}
	*sub = 0;
	off = PGPV_KEYID_LEN - cmp;
	for (i = 0 ; i < ARRAY_COUNT(pgp->primaries) ; i++) {
		prim = &ARRAY_ELEMENT(pgp->primaries, i);
		if (memcmp(&prim->primary.keyid[off], &binkeyid[off], cmp) == 0) {
			return i;
		}
		for (j = 0 ; j < ARRAY_COUNT(prim->signed_subkeys) ; j++) {
			subkey = &ARRAY_ELEMENT(prim->signed_subkeys, j);
			if (memcmp(&subkey->subkey.keyid[off], &binkeyid[off], cmp) == 0) {
				*sub = j + 1;
				return i;
			}
		}

	}
	return -1;
}

/* match the signature with the id indexed by 'primary' */
static int
match_sig_id(pgpv_cursor_t *cursor, pgpv_signature_t *signature, pgpv_litdata_t *litdata, unsigned primary, unsigned sub)
{
	pgpv_primarykey_t	*prim;
	pgpv_pubkey_t		*pubkey;
	uint8_t			*data;
	size_t			 insize;

	cursor->sigtime = signature->birth;
	/* calc hash on data packet */
	data = get_literal_data(cursor, litdata, &insize);
	if (sub == 0) {
		pubkey = &ARRAY_ELEMENT(cursor->pgp->primaries, primary).primary;
		return match_sig(cursor, signature, pubkey, data, insize);
	}
	prim = &ARRAY_ELEMENT(cursor->pgp->primaries, primary);
	pubkey = &ARRAY_ELEMENT(prim->signed_subkeys, sub - 1).subkey;
	return match_sig(cursor, signature, pubkey, data, insize);
}

/* return the packet type */
static const char *
get_packet_type(uint8_t tag)
{
	switch(tag) {
	case SIGNATURE_PKT:
		return "signature packet";
	case ONEPASS_SIGNATURE_PKT:
		return "onepass signature packet";
	case PUBKEY_PKT:
		return "pubkey packet";
	case COMPRESSED_DATA_PKT:
		return "compressed data packet";
	case MARKER_PKT:
		return "marker packet";
	case LITDATA_PKT:
		return "litdata packet";
	case TRUST_PKT:
		return "trust packet";
	case USERID_PKT:
		return "userid packet";
	case PUB_SUBKEY_PKT:
		return "public subkey packet";
	case USER_ATTRIBUTE_PKT:
		return "user attribute packet";
	default:
		return "[UNKNOWN]";
	}
}

/* get an element from the found array */
int
pgpv_get_cursor_element(pgpv_cursor_t *cursor, size_t element)
{
	if (cursor && element < ARRAY_COUNT(cursor->found)) {
		return (int)ARRAY_ELEMENT(cursor->found, element);
	}
	return -1;
}

/* verify the signed packets we have */
size_t
pgpv_verify(pgpv_cursor_t *cursor, pgpv_t *pgp, const void *p, ssize_t size)
{
	pgpv_signature_t	*signature;
	pgpv_onepass_t		*onepass;
	pgpv_litdata_t		*litdata;
	unsigned		 sub;
	size_t			 pkt;
	char			 strkeyid[PGPV_STR_KEYID_LEN];
	int			 j;

	if (cursor == NULL || pgp == NULL || p == NULL) {
		return 0;
	}
	if (!setup_data(cursor, pgp, p, size)) {
		snprintf(cursor->why, sizeof(cursor->why), "No input data");
		return 0;
	}
	if (ARRAY_COUNT(cursor->pgp->pkts) == ARRAY_LAST(cursor->pgp->datastarts) + 1) {
		/* got detached signature here */
		if (!fixup_detached(cursor, p)) {
			snprintf(cursor->why, sizeof(cursor->why), "Can't read signed file '%s'", (const char *)p);
			return 0;
		}
	}
	if ((pkt = find_onepass(cursor, ARRAY_LAST(cursor->pgp->datastarts))) == 0) {
		snprintf(cursor->why, sizeof(cursor->why), "No signature found");
		return 0;
	}
	pkt -= 1;
	onepass = &ARRAY_ELEMENT(cursor->pgp->pkts, pkt).u.onepass;
	litdata = &ARRAY_ELEMENT(cursor->pgp->pkts, pkt + 1).u.litdata;
	signature = &ARRAY_ELEMENT(cursor->pgp->pkts, pkt + 2).u.sigpkt.sig;
	/* sanity check values in signature and onepass agree */
	if (signature->birth == 0) {
		fmt_time(cursor->why, sizeof(cursor->why), "Signature creation time [",
			signature->birth, "] out of range", 0);
		return 0;
	}
	if (memcmp(onepass->keyid, signature->signer, PGPV_KEYID_LEN) != 0) {
		fmt_binary(strkeyid, sizeof(strkeyid), onepass->keyid, (unsigned)sizeof(onepass->keyid));
		snprintf(cursor->why, sizeof(cursor->why), "Signature key id %s does not match onepass keyid",
			strkeyid);
		return 0;
	}
	if (onepass->hashalg != signature->hashalg) {
		snprintf(cursor->why, sizeof(cursor->why), "Signature hashalg %u does not match onepass hashalg %u",
			signature->hashalg, onepass->hashalg);
		return 0;
	}
	if (onepass->keyalg != signature->keyalg) {
		snprintf(cursor->why, sizeof(cursor->why), "Signature keyalg %u does not match onepass keyalg %u",
			signature->keyalg, onepass->keyalg);
		return 0;
	}
	if (cursor->pgp->ssh) {
		fixup_ssh_keyid(cursor->pgp, signature, "sha1");
	}
	sub = 0;
	if ((j = find_keyid(cursor->pgp, NULL, onepass->keyid, &sub)) < 0) {
		fmt_binary(strkeyid, sizeof(strkeyid), onepass->keyid, (unsigned)sizeof(onepass->keyid));
		snprintf(cursor->why, sizeof(cursor->why), "Signature key id %s not found ", strkeyid);
		return 0;
	}
	if (!match_sig_id(cursor, signature, litdata, (unsigned)j, sub)) {
		return 0;
	}
	ARRAY_APPEND(cursor->datacookies, pkt);
	j = ((j & 0xffffff) << 8) | (sub & 0xff);
	ARRAY_APPEND(cursor->found, j);
	return pkt + 1;
}

/* set up the pubkey keyring */
int
pgpv_read_pubring(pgpv_t *pgp, const void *keyring, ssize_t size)
{
	if (pgp == NULL) {
		return 0;
	}
	if (keyring) {
		return (size > 0) ?
			read_binary_memory(pgp, "pubring", keyring, (size_t)size) :
			read_binary_file(pgp, "pubring", "%s", (const char *)keyring);
	}
	return read_binary_file(pgp, "pubring", "%s/%s", nonnull_getenv("HOME"), ".gnupg/pubring.gpg");
}

/* set up the pubkey keyring from ssh pub key */
int
pgpv_read_ssh_pubkeys(pgpv_t *pgp, const void *keyring, ssize_t size)
{
	pgpv_primarykey_t	primary;

	USE_ARG(size);
	if (pgp == NULL) {
		return 0;
	}
	if (keyring) {
		if (!read_ssh_file(pgp, &primary, "%s", (const char *)keyring)) {
			return 0;
		}
	} else if (!read_ssh_file(pgp, &primary, "%s/%s", nonnull_getenv("HOME"), ".ssh/id_rsa.pub")) {
		return 0;
	}
	ARRAY_APPEND(pgp->primaries, primary);
	pgp->ssh = 1;
	return 1;
}

/* get verified data as a string, return its size */
size_t
pgpv_get_verified(pgpv_cursor_t *cursor, size_t cookie, char **ret)
{
	pgpv_litdata_t		*litdata;
	uint8_t			*data;
	size_t			 size;
	size_t			 pkt;

	if (ret == NULL || cursor == NULL || cookie == 0) {
		return 0;
	}
	*ret = NULL;
	if ((pkt = find_onepass(cursor, cookie - 1)) == 0) {
		return 0;
	}
	litdata = &ARRAY_ELEMENT(cursor->pgp->pkts, pkt).u.litdata;
	data = get_literal_data(cursor, litdata, &size);
	if ((*ret = calloc(1, size)) == NULL) {
		return 0;
	}
	memcpy(*ret, data, size);
	return size;
}

#define KB(x)	((x) * 1024)

/* dump all packets */
size_t
pgpv_dump(pgpv_t *pgp, char **data)
{
	ssize_t	 dumpc;
	size_t	 alloc;
	size_t	 pkt;
	size_t	 cc;
	size_t	 n;
	char	 buf[800];
	char	*newdata;

	cc = alloc = 0;
	*data = NULL;
	for (pkt = 0 ; pkt < ARRAY_COUNT(pgp->pkts) ; pkt++) {
		if (cc + KB(64) >= alloc) {
			if ((newdata = realloc(*data, alloc + KB(64))) == NULL) {
				return cc;
			}
			alloc += KB(64);
			*data = newdata;
		}
		memset(buf, 0x0, sizeof(buf));
		dumpc = netpgp_hexdump(ARRAY_ELEMENT(pgp->pkts, pkt).s.data,
				MIN((sizeof(buf) / 80) * 16,
				ARRAY_ELEMENT(pgp->pkts, pkt).s.size),
				buf, sizeof(buf));
		n = snprintf(&(*data)[cc], alloc - cc,
			"[%zu] off %zu, len %zu, tag %u, %s\n%.*s",
			pkt,
			ARRAY_ELEMENT(pgp->pkts, pkt).offset,
			ARRAY_ELEMENT(pgp->pkts, pkt).s.size,
			ARRAY_ELEMENT(pgp->pkts, pkt).tag,
			get_packet_type(ARRAY_ELEMENT(pgp->pkts, pkt).tag),
			(int)dumpc, buf);
		cc += n;
	}
	return cc;
}
