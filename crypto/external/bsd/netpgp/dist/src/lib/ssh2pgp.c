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
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_OPENSSL_CAST_H
#include <openssl/cast.h>
#endif

#include <openssl/pem.h>

#include "bufgap.h"

#include "packet-parse.h"
#include "netpgpdefs.h"
#include "netpgpsdk.h"
#include "crypto.h"
#include "netpgpdigest.h"
#include "ssh2pgp.h"

/* structure for earching for constant strings */
typedef struct str_t {
	const char	*s;		/* string */
	size_t		 len;		/* its length */
	int		 type;		/* return type */
} str_t;

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&x
#endif

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

/* get a bignum from the buffer gap */
static BIGNUM *
getbignum(bufgap_t *bg, char *buf, const char *header)
{
	uint32_t	 len;
	BIGNUM		*bignum;

	(void) bufgap_getbin(bg, &len, sizeof(len));
	len = ntohl(len);
	(void) bufgap_seek(bg, sizeof(len), BGFromHere, BGByte);
	(void) bufgap_getbin(bg, buf, len);
	bignum = BN_bin2bn((const uint8_t *)buf, (int)len, NULL);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, header, (const uint8_t *)(void *)buf, len);
	}
	(void) bufgap_seek(bg, len, BGFromHere, BGByte);
	return bignum;
}

#if 0
static int
putbignum(bufgap_t *bg, BIGNUM *bignum)
{
	uint32_t	 len;

	len = BN_num_bytes(bignum);
	(void) bufgap_insert(bg, &len, sizeof(len));
	(void) bufgap_insert(bg, buf, len);
	bignum = BN_bin2bn((const uint8_t *)buf, (int)len, NULL);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, header, buf, (int)len);
	}
	(void) bufgap_seek(bg, len, BGFromHere, BGByte);
	return bignum;
}
#endif

static str_t	pkatypes[] = {
	{	"ssh-rsa",	7,	PGP_PKA_RSA	},
	{	"ssh-dss",	7,	PGP_PKA_DSA	},
	{	"ssh-dsa",	7,	PGP_PKA_DSA	},
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

/* convert an ssh (host) pubkey to a pgp pubkey */
int
pgp_ssh2pubkey(pgp_io_t *io, const char *f, pgp_key_t *key, pgp_hash_alg_t hashtype)
{
	pgp_pubkey_t	*pubkey;
	struct stat	 st;
	bufgap_t	 bg;
	uint32_t	 len;
	int64_t		 off;
	uint8_t		*userid;
	char		 hostname[256];
	char		 owner[256];
	char		*space;
	char	 	*buf;
	char	 	*bin;
	int		 ok;
	int		 cc;

	(void) memset(&bg, 0x0, sizeof(bg));
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
		bufgap_seek(&bg, 1, BGFromHere, BGByte);
	}
	bufgap_seek(&bg, 1, BGFromHere, BGByte);
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
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, NULL, (const uint8_t *)(const void *)buf, (size_t)cc);
	}
	cc = frombase64(bin, buf, (size_t)cc, 0);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "decoded base64:", (const uint8_t *)(const void *)bin, (size_t)cc);
	}
	bufgap_delete(&bg, (uint64_t)bufgap_tell(&bg, BGFromEOF, BGByte));
	bufgap_insert(&bg, bin, cc);
	bufgap_seek(&bg, off, BGFromBOF, BGByte);

	/* get the type of key */
	(void) bufgap_getbin(&bg, &len, sizeof(len));
	len = ntohl(len);
	(void) bufgap_seek(&bg, sizeof(len), BGFromHere, BGByte);
	(void) bufgap_getbin(&bg, buf, len);
	(void) bufgap_seek(&bg, len, BGFromHere, BGByte);

	(void) memset(key, 0x0, sizeof(*key));
	pubkey = &key->key.seckey.pubkey;
	pubkey->version = PGP_V4;
	pubkey->birthtime = 0;
	/* get key type */
	ok = 1;
	switch (pubkey->alg = findstr(pkatypes, buf)) {
	case PGP_PKA_RSA:
		/* get the 'e' param of the key */
		pubkey->key.rsa.e = getbignum(&bg, buf, "RSA E");
		/* get the 'n' param of the key */
		pubkey->key.rsa.n = getbignum(&bg, buf, "RSA N");
		break;
	case PGP_PKA_DSA:
		/* get the 'p' param of the key */
		pubkey->key.dsa.p = getbignum(&bg, buf, "DSA P");
		/* get the 'q' param of the key */
		pubkey->key.dsa.q = getbignum(&bg, buf, "DSA Q");
		/* get the 'g' param of the key */
		pubkey->key.dsa.g = getbignum(&bg, buf, "DSA G");
		/* get the 'y' param of the key */
		pubkey->key.dsa.y = getbignum(&bg, buf, "DSA Y");
		break;
	default:
		(void) fprintf(stderr, "Unrecognised pubkey type %d for '%s'\n",
				pubkey->alg, f);
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
		(void) memset(&userid, 0x0, sizeof(userid));
		(void) gethostname(hostname, sizeof(hostname));
		if (strlen(space + 1) - 1 == 0) {
			(void) snprintf(owner, sizeof(owner), "<root@%s>",
					hostname);
		} else {
			(void) snprintf(owner, sizeof(owner), "<%.*s>",
				(int)strlen(space + 1) - 1,
				space + 1);
		}
		(void) pgp_asprintf((char **)(void *)&userid,
						"%s (%s) %s",
						hostname,
						f,
						owner);
		pgp_keyid(key->sigid, sizeof(key->sigid), pubkey, hashtype);
		pgp_add_userid(key, userid);
		pgp_fingerprint(&key->sigfingerprint, pubkey, hashtype);
		free(userid);
		if (pgp_get_debug_level(__FILE__)) {
			/*pgp_print_keydata(io, keyring, key, "pub", pubkey, 0);*/
			__PGP_USED(io); /* XXX */
		}
	}
	(void) free(bin);
	(void) free(buf);
	bufgap_close(&bg);
	return ok;
}

/* convert an ssh (host) seckey to a pgp seckey */
int
pgp_ssh2seckey(pgp_io_t *io, const char *f, pgp_key_t *key, pgp_pubkey_t *pubkey, pgp_hash_alg_t hashtype)
{
	pgp_crypt_t	crypted;
	pgp_hash_t	hash;
	unsigned	done = 0;
	unsigned	i = 0;
	uint8_t		sesskey[CAST_KEY_LENGTH];
	uint8_t		hashed[PGP_SHA1_HASH_SIZE];
	BIGNUM		*tmp;

	__PGP_USED(io);
	/* XXX - check for rsa/dsa */
	if (!openssl_read_pem_seckey(f, key, "ssh-rsa", 0)) {
		return 0;
	}
	if (pgp_get_debug_level(__FILE__)) {
		/*pgp_print_keydata(io, key, "sec", &key->key.seckey.pubkey, 0);*/
		/* XXX */
	}
	/* let's add some sane defaults */
	(void) memcpy(&key->key.seckey.pubkey, pubkey, sizeof(*pubkey));
	key->key.seckey.s2k_usage = PGP_S2KU_ENCRYPTED_AND_HASHED;
	key->key.seckey.alg = PGP_SA_CAST5;
	key->key.seckey.s2k_specifier = PGP_S2KS_SALTED;
	key->key.seckey.hash_alg = PGP_HASH_SHA1;
	if (key->key.seckey.pubkey.alg == PGP_PKA_RSA) {
		/* openssh and openssl have p and q swapped */
		tmp = key->key.seckey.key.rsa.p;
		key->key.seckey.key.rsa.p = key->key.seckey.key.rsa.q;
		key->key.seckey.key.rsa.q = tmp;
	}
	for (done = 0, i = 0; done < CAST_KEY_LENGTH; i++) {
		unsigned 	j;
		uint8_t		zero = 0;
		int             needed;
		int             size;

		needed = CAST_KEY_LENGTH - done;
		size = MIN(needed, PGP_SHA1_HASH_SIZE);

		pgp_hash_any(&hash, key->key.seckey.hash_alg);
		if (!hash.init(&hash)) {
			(void) fprintf(stderr, "write_seckey_body: bad alloc\n");
			return 0;
		}

		/* preload if iterating  */
		for (j = 0; j < i; j++) {
			/*
			 * Coverity shows a DEADCODE error on this
			 * line. This is expected since the hardcoded
			 * use of SHA1 and CAST5 means that it will
			 * not used. This will change however when
			 * other algorithms are supported.
			 */
			hash.add(&hash, &zero, 1);
		}

		if (key->key.seckey.s2k_specifier == PGP_S2KS_SALTED) {
			hash.add(&hash, key->key.seckey.salt, PGP_SALT_SIZE);
		}
		hash.finish(&hash, hashed);

		/*
		 * if more in hash than is needed by session key, use
		 * the leftmost octets
		 */
		(void) memcpy(&sesskey[i * PGP_SHA1_HASH_SIZE],
				hashed, (unsigned)size);
		done += (unsigned)size;
		if (done > CAST_KEY_LENGTH) {
			(void) fprintf(stderr,
				"write_seckey_body: short add\n");
			return 0;
		}
	}
	pgp_crypt_any(&crypted, key->key.seckey.alg);
	crypted.set_iv(&crypted, key->key.seckey.iv);
	crypted.set_crypt_key(&crypted, sesskey);
	pgp_encrypt_init(&crypted);
	key->key.seckey.pubkey.alg = PGP_PKA_RSA;
	pgp_fingerprint(&key->sigfingerprint, pubkey, hashtype);
	pgp_keyid(key->sigid, sizeof(key->sigid), pubkey, hashtype);
	return 1;
}

/* read a key from the ssh file, and add it to a keyring */
int
pgp_ssh2_readkeys(pgp_io_t *io, pgp_keyring_t *pubring,
		pgp_keyring_t *secring, const char *pubfile,
		const char *secfile, unsigned hashtype)
{
	pgp_key_t		*pubkey;
	pgp_key_t		*seckey;
	pgp_key_t		 key;

	pubkey = NULL;
	(void) memset(&key, 0x0, sizeof(key));
	if (pubfile) {
		if (pgp_get_debug_level(__FILE__)) {
			(void) fprintf(io->errs, "pgp_ssh2_readkeys: pubfile '%s'\n", pubfile);
		}
		if (!pgp_ssh2pubkey(io, pubfile, &key, (pgp_hash_alg_t)hashtype)) {
			(void) fprintf(io->errs, "pgp_ssh2_readkeys: can't read pubkeys '%s'\n", pubfile);
			return 0;
		}
		EXPAND_ARRAY(pubring, key);
		pubkey = &pubring->keys[pubring->keyc++];
		(void) memcpy(pubkey, &key, sizeof(key));
		pubkey->type = PGP_PTAG_CT_PUBLIC_KEY;
	}
	if (secfile) {
		if (pgp_get_debug_level(__FILE__)) {
			(void) fprintf(io->errs, "pgp_ssh2_readkeys: secfile '%s'\n", secfile);
		}
		if (pubkey == NULL) {
			pubkey = &pubring->keys[0];
		}
		if (!pgp_ssh2seckey(io, secfile, &key, &pubkey->key.pubkey, (pgp_hash_alg_t)hashtype)) {
			(void) fprintf(io->errs, "pgp_ssh2_readkeys: can't read seckeys '%s'\n", secfile);
			return 0;
		}
		EXPAND_ARRAY(secring, key);
		seckey = &secring->keys[secring->keyc++];
		(void) memcpy(seckey, &key, sizeof(key));
		seckey->type = PGP_PTAG_CT_SECRET_KEY;
	}
	return 1;
}
