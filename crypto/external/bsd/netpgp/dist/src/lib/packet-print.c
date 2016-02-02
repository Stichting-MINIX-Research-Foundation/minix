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

/*
 * ! \file \brief Standard API print functions
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: packet-print.c,v 1.42 2012/02/22 06:29:40 agc Exp $");
#endif

#include <string.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "crypto.h"
#include "keyring.h"
#include "packet-show.h"
#include "signature.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "netpgpsdk.h"
#include "packet.h"
#include "netpgpdigest.h"
#include "mj.h"

/* static functions */

static void 
print_indent(int indent)
{
	int             i;

	for (i = 0; i < indent; i++) {
		printf("  ");
	}
}

static void 
print_name(int indent, const char *name)
{
	print_indent(indent);
	if (name) {
		printf("%s: ", name);
	}
}

static void 
print_hexdump(int indent, const char *name, const uint8_t *data, unsigned len)
{
	print_name(indent, name);
	hexdump(stdout, NULL, data, len);
}

static void 
hexdump_data(int indent, const char *name, const uint8_t *data, unsigned len)
{
	print_name(indent, name);
	hexdump(stdout, NULL, data, len);
}

static void 
print_uint(int indent, const char *name, unsigned val)
{
	print_name(indent, name);
	printf("%u\n", val);
}

static void 
showtime(const char *name, time_t t)
{
	printf("%s=%" PRItime "d (%.24s)", name, (long long) t, ctime(&t));
}

static void 
print_time(int indent, const char *name, time_t t)
{
	print_indent(indent);
	printf("%s: ", name);
	showtime("time", t);
	printf("\n");
}

static void 
print_string_and_value(int indent, const char *name, const char *str, uint8_t value)
{
	print_name(indent, name);
	printf("%s (0x%x)\n", str, value);
}

static void 
print_tagname(int indent, const char *str)
{
	print_indent(indent);
	printf("%s packet\n", str);
}

static void 
print_data(int indent, const char *name, const pgp_data_t *data)
{
	print_hexdump(indent, name, data->contents, (unsigned)data->len);
}

static void 
print_bn(int indent, const char *name, const BIGNUM *bn)
{
	print_indent(indent);
	printf("%s=", name);
	if (bn) {
		BN_print_fp(stdout, bn);
		putchar('\n');
	} else {
		puts("(unset)");
	}
}

static void 
print_packet_hex(const pgp_subpacket_t *pkt)
{
	hexdump(stdout, "packet contents:", pkt->raw, pkt->length);
}

static void 
print_escaped(const uint8_t *data, size_t length)
{
	while (length-- > 0) {
		if ((*data >= 0x20 && *data < 0x7f && *data != '%') ||
		    *data == '\n') {
			putchar(*data);
		} else {
			printf("%%%02x", *data);
		}
		++data;
	}
}

static void 
print_string(int indent, const char *name, const char *str)
{
	print_name(indent, name);
	print_escaped((const uint8_t *) str, strlen(str));
	putchar('\n');
}

static void 
print_utf8_string(int indent, const char *name, const uint8_t *str)
{
	/* \todo Do this better for non-English character sets */
	print_string(indent, name, (const char *) str);
}

static void 
print_duration(int indent, const char *name, time_t t)
{
	int             mins, hours, days, years;

	print_indent(indent);
	printf("%s: ", name);
	printf("duration %" PRItime "d seconds", (long long) t);

	mins = (int)(t / 60);
	hours = mins / 60;
	days = hours / 24;
	years = days / 365;

	printf(" (approx. ");
	if (years) {
		printf("%d %s", years, years == 1 ? "year" : "years");
	} else if (days) {
		printf("%d %s", days, days == 1 ? "day" : "days");
	} else if (hours) {
		printf("%d %s", hours, hours == 1 ? "hour" : "hours");
	}
	printf(")\n");
}

static void 
print_boolean(int indent, const char *name, uint8_t boolval)
{
	print_name(indent, name);
	printf("%s\n", (boolval) ? "Yes" : "No");
}

static void 
print_text_breakdown(int indent, pgp_text_t *text)
{
	const char     *prefix = ".. ";
	unsigned        i;

	/* these were recognised */
	for (i = 0; i < text->known.used; i++) {
		print_indent(indent);
		printf("%s", prefix);
		printf("%s\n", text->known.strings[i]);
	}
	/*
	 * these were not recognised. the strings will contain the hex value
	 * of the unrecognised value in string format - see
	 * process_octet_str()
	 */
	if (text->unknown.used) {
		printf("\n");
		print_indent(indent);
		printf("Not Recognised: ");
	}
	for (i = 0; i < text->unknown.used; i++) {
		print_indent(indent);
		printf("%s", prefix);
		printf("%s\n", text->unknown.strings[i]);
	}
}

static void 
print_headers(const pgp_headers_t *h)
{
	unsigned        i;

	for (i = 0; i < h->headerc; ++i) {
		printf("%s=%s\n", h->headers[i].key, h->headers[i].value);
	}
}

static void 
print_block(int indent, const char *name, const uint8_t *str, size_t length)
{
	int             o = (int)length;

	print_indent(indent);
	printf(">>>>> %s >>>>>\n", name);

	print_indent(indent);
	for (; length > 0; --length) {
		if (*str >= 0x20 && *str < 0x7f && *str != '%') {
			putchar(*str);
		} else if (*str == '\n') {
			putchar(*str);
			print_indent(indent);
		} else {
			printf("%%%02x", *str);
		}
		++str;
	}
	if (o && str[-1] != '\n') {
		putchar('\n');
		print_indent(indent);
		fputs("[no newline]", stdout);
	} else {
		print_indent(indent);
	}
	printf("<<<<< %s <<<<<\n", name);
}

/* return the number of bits in the public key */
static int
numkeybits(const pgp_pubkey_t *pubkey)
{
	switch(pubkey->alg) {
	case PGP_PKA_RSA:
	case PGP_PKA_RSA_ENCRYPT_ONLY:
	case PGP_PKA_RSA_SIGN_ONLY:
		return BN_num_bytes(pubkey->key.rsa.n) * 8;
	case PGP_PKA_DSA:
		switch(BN_num_bytes(pubkey->key.dsa.q)) {
		case 20:
			return 1024;
		case 28:
			return 2048;
		case 32:
			return 3072;
		default:
			return 0;
		}
	case PGP_PKA_ELGAMAL:
		return BN_num_bytes(pubkey->key.elgamal.y) * 8;
	default:
		return -1;
	}
}

/* return the hexdump as a string */
static char *
strhexdump(char *dest, const uint8_t *src, size_t length, const char *sep)
{
	unsigned i;
	int	n;

	for (n = 0, i = 0 ; i < length ; i += 2) {
		n += snprintf(&dest[n], 3, "%02x", *src++);
		n += snprintf(&dest[n], 10, "%02x%s", *src++, sep);
	}
	return dest;
}

/* return the time as a string */
static char * 
ptimestr(char *dest, size_t size, time_t t)
{
	struct tm      *tm;

	tm = gmtime(&t);
	(void) snprintf(dest, size, "%04d-%02d-%02d",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday);
	return dest;
}

/* print the sub key binding signature info */
static int
psubkeybinding(char *buf, size_t size, const pgp_key_t *key, const char *expired)
{
	char	keyid[512];
	char	t[32];

	return snprintf(buf, size, "encryption %d/%s %s %s %s\n",
		numkeybits(&key->enckey),
		pgp_show_pka(key->enckey.alg),
		strhexdump(keyid, key->encid, PGP_KEY_ID_SIZE, ""),
		ptimestr(t, sizeof(t), key->enckey.birthtime),
		expired);
}

static int
isrevoked(const pgp_key_t *key, unsigned uid)
{
	unsigned	r;

	for (r = 0 ; r < key->revokec ; r++) {
		if (key->revokes[r].uid == uid) {
			return r;
		}
	}
	return -1;
}

#ifndef KB
#define KB(x)	((x) * 1024)
#endif

/* print into a string (malloc'ed) the pubkeydata */
int
pgp_sprint_keydata(pgp_io_t *io, const pgp_keyring_t *keyring,
		const pgp_key_t *key, char **buf, const char *header,
		const pgp_pubkey_t *pubkey, const int psigs)
{
	const pgp_key_t	*trustkey;
	unsigned	 	 from;
	unsigned		 i;
	unsigned		 j;
	time_t			 now;
	char			 uidbuf[KB(128)];
	char			 keyid[PGP_KEY_ID_SIZE * 3];
	char			 fp[(PGP_FINGERPRINT_SIZE * 3) + 1];
	char			 expired[128];
	char			 t[32];
	int			 cc;
	int			 n;
	int			 r;

	if (key == NULL || key->revoked) {
		return -1;
	}
	now = time(NULL);
	if (pubkey->duration > 0) {
		cc = snprintf(expired, sizeof(expired),
			(pubkey->birthtime + pubkey->duration < now) ?
			"[EXPIRED " : "[EXPIRES ");
		ptimestr(&expired[cc], sizeof(expired) - cc,
			pubkey->birthtime + pubkey->duration);
		cc += 10;
		cc += snprintf(&expired[cc], sizeof(expired) - cc, "]");
	} else {
		expired[0] = 0x0;
	}
	for (i = 0, n = 0; i < key->uidc; i++) {
		if ((r = isrevoked(key, i)) >= 0 &&
		    key->revokes[r].code == PGP_REVOCATION_COMPROMISED) {
			continue;
		}
		n += snprintf(&uidbuf[n], sizeof(uidbuf) - n, "uid%s%s%s\n",
				(psigs) ? "    " : "              ",
				key->uids[i],
				(isrevoked(key, i) >= 0) ? " [REVOKED]" : "");
		for (j = 0 ; j < key->subsigc ; j++) {
			if (psigs) {
				if (key->subsigs[j].uid != i) {
					continue;
				}
			} else {
				if (!(key->subsigs[j].sig.info.version == 4 &&
					key->subsigs[j].sig.info.type == PGP_SIG_SUBKEY &&
					i == key->uidc - 1)) {
						continue;
				}
			}
			from = 0;
			trustkey = pgp_getkeybyid(io, keyring, key->subsigs[j].sig.info.signer_id, &from, NULL);
			if (key->subsigs[j].sig.info.version == 4 &&
					key->subsigs[j].sig.info.type == PGP_SIG_SUBKEY) {
				psubkeybinding(&uidbuf[n], sizeof(uidbuf) - n, key, expired);
			} else {
				n += snprintf(&uidbuf[n], sizeof(uidbuf) - n,
					"sig        %s  %s  %s\n",
					strhexdump(keyid, key->subsigs[j].sig.info.signer_id, PGP_KEY_ID_SIZE, ""),
					ptimestr(t, sizeof(t), key->subsigs[j].sig.info.birthtime),
					(trustkey) ? (char *)trustkey->uids[trustkey->uid0] : "[unknown]");
			}
		}
	}
	return pgp_asprintf(buf, "%s %d/%s %s %s %s\nKey fingerprint: %s\n%s",
		header,
		numkeybits(pubkey),
		pgp_show_pka(pubkey->alg),
		strhexdump(keyid, key->sigid, PGP_KEY_ID_SIZE, ""),
		ptimestr(t, sizeof(t), pubkey->birthtime),
		expired,
		strhexdump(fp, key->sigfingerprint.fingerprint, key->sigfingerprint.length, " "),
		uidbuf);
}

/* return the key info as a JSON encoded string */
int
pgp_sprint_mj(pgp_io_t *io, const pgp_keyring_t *keyring,
		const pgp_key_t *key, mj_t *keyjson, const char *header,
		const pgp_pubkey_t *pubkey, const int psigs)
{
	const pgp_key_t	*trustkey;
	unsigned	 	 from;
	unsigned		 i;
	unsigned		 j;
	mj_t			 sub_obj;
	char			 keyid[PGP_KEY_ID_SIZE * 3];
	char			 fp[(PGP_FINGERPRINT_SIZE * 3) + 1];
	int			 r;

	if (key == NULL || key->revoked) {
		return -1;
	}
	(void) memset(keyjson, 0x0, sizeof(*keyjson));
	mj_create(keyjson, "object");
	mj_append_field(keyjson, "header", "string", header, -1);
	mj_append_field(keyjson, "key bits", "integer", (int64_t) numkeybits(pubkey));
	mj_append_field(keyjson, "pka", "string", pgp_show_pka(pubkey->alg), -1);
	mj_append_field(keyjson, "key id", "string", strhexdump(keyid, key->sigid, PGP_KEY_ID_SIZE, ""), -1);
	mj_append_field(keyjson, "fingerprint", "string",
		strhexdump(fp, key->sigfingerprint.fingerprint, key->sigfingerprint.length, " "), -1);
	mj_append_field(keyjson, "birthtime", "integer", pubkey->birthtime);
	mj_append_field(keyjson, "duration", "integer", pubkey->duration);
	for (i = 0; i < key->uidc; i++) {
		if ((r = isrevoked(key, i)) >= 0 &&
		    key->revokes[r].code == PGP_REVOCATION_COMPROMISED) {
			continue;
		}
		(void) memset(&sub_obj, 0x0, sizeof(sub_obj));
		mj_create(&sub_obj, "array");
		mj_append(&sub_obj, "string", key->uids[i], -1);
		mj_append(&sub_obj, "string", (r >= 0) ? "[REVOKED]" : "", -1);
		mj_append_field(keyjson, "uid", "array", &sub_obj);
		mj_delete(&sub_obj);
		for (j = 0 ; j < key->subsigc ; j++) {
			if (psigs) {
				if (key->subsigs[j].uid != i) {
					continue;
				}
			} else {
				if (!(key->subsigs[j].sig.info.version == 4 &&
					key->subsigs[j].sig.info.type == PGP_SIG_SUBKEY &&
					i == key->uidc - 1)) {
						continue;
				}
			}
			(void) memset(&sub_obj, 0x0, sizeof(sub_obj));
			mj_create(&sub_obj, "array");
			if (key->subsigs[j].sig.info.version == 4 &&
					key->subsigs[j].sig.info.type == PGP_SIG_SUBKEY) {
				mj_append(&sub_obj, "integer", (int64_t)numkeybits(&key->enckey));
				mj_append(&sub_obj, "string",
					(const char *)pgp_show_pka(key->enckey.alg), -1);
				mj_append(&sub_obj, "string",
					strhexdump(keyid, key->encid, PGP_KEY_ID_SIZE, ""), -1);
				mj_append(&sub_obj, "integer", (int64_t)key->enckey.birthtime);
				mj_append_field(keyjson, "encryption", "array", &sub_obj);
				mj_delete(&sub_obj);
			} else {
				mj_append(&sub_obj, "string",
					strhexdump(keyid, key->subsigs[j].sig.info.signer_id, PGP_KEY_ID_SIZE, ""), -1);
				mj_append(&sub_obj, "integer",
					(int64_t)(key->subsigs[j].sig.info.birthtime));
				from = 0;
				trustkey = pgp_getkeybyid(io, keyring, key->subsigs[j].sig.info.signer_id, &from, NULL);
				mj_append(&sub_obj, "string",
					(trustkey) ? (char *)trustkey->uids[trustkey->uid0] : "[unknown]", -1);
				mj_append_field(keyjson, "sig", "array", &sub_obj);
				mj_delete(&sub_obj);
			}
		}
	}
	if (pgp_get_debug_level(__FILE__)) {
		char	*buf;

		mj_asprint(&buf, keyjson, 1);
		(void) fprintf(stderr, "pgp_sprint_mj: '%s'\n", buf);
		free(buf);
	}
	return 1;
}

int
pgp_hkp_sprint_keydata(pgp_io_t *io, const pgp_keyring_t *keyring,
		const pgp_key_t *key, char **buf,
		const pgp_pubkey_t *pubkey, const int psigs)
{
	const pgp_key_t	*trustkey;
	unsigned	 	 from;
	unsigned	 	 i;
	unsigned	 	 j;
	char			 keyid[PGP_KEY_ID_SIZE * 3];
	char		 	 uidbuf[KB(128)];
	char		 	 fp[(PGP_FINGERPRINT_SIZE * 3) + 1];
	int		 	 n;

	if (key->revoked) {
		return -1;
	}
	for (i = 0, n = 0; i < key->uidc; i++) {
		n += snprintf(&uidbuf[n], sizeof(uidbuf) - n,
			"uid:%lld:%lld:%s\n",
			(long long)pubkey->birthtime,
			(long long)pubkey->duration,
			key->uids[i]);
		for (j = 0 ; j < key->subsigc ; j++) {
			if (psigs) {
				if (key->subsigs[j].uid != i) {
					continue;
				}
			} else {
				if (!(key->subsigs[j].sig.info.version == 4 &&
					key->subsigs[j].sig.info.type == PGP_SIG_SUBKEY &&
					i == key->uidc - 1)) {
						continue;
				}
			}
			from = 0;
			trustkey = pgp_getkeybyid(io, keyring, key->subsigs[j].sig.info.signer_id, &from, NULL);
			if (key->subsigs[j].sig.info.version == 4 &&
					key->subsigs[j].sig.info.type == PGP_SIG_SUBKEY) {
				n += snprintf(&uidbuf[n], sizeof(uidbuf) - n, "sub:%d:%d:%s:%lld:%lld\n",
					numkeybits(pubkey),
					key->subsigs[j].sig.info.key_alg,
					strhexdump(keyid, key->subsigs[j].sig.info.signer_id, PGP_KEY_ID_SIZE, ""),
					(long long)(key->subsigs[j].sig.info.birthtime),
					(long long)pubkey->duration);
			} else {
				n += snprintf(&uidbuf[n], sizeof(uidbuf) - n,
					"sig:%s:%lld:%s\n",
					strhexdump(keyid, key->subsigs[j].sig.info.signer_id, PGP_KEY_ID_SIZE, ""),
					(long long)key->subsigs[j].sig.info.birthtime,
					(trustkey) ? (char *)trustkey->uids[trustkey->uid0] : "");
			}
		}
	}
	return pgp_asprintf(buf, "pub:%s:%d:%d:%lld:%lld\n%s",
		strhexdump(fp, key->sigfingerprint.fingerprint, PGP_FINGERPRINT_SIZE, ""),
		pubkey->alg,
		numkeybits(pubkey),
		(long long)pubkey->birthtime,
		(long long)pubkey->duration,
		uidbuf);
}

/* print the key data for a pub or sec key */
void
pgp_print_keydata(pgp_io_t *io, const pgp_keyring_t *keyring,
		const pgp_key_t *key, const char *header,
		const pgp_pubkey_t *pubkey, const int psigs)
{
	char	*cp;

	if (pgp_sprint_keydata(io, keyring, key, &cp, header, pubkey, psigs) >= 0) {
		(void) fprintf(io->res, "%s", cp);
		free(cp);
	}
}

/**
\ingroup Core_Print
\param pubkey
*/
void
pgp_print_pubkey(const pgp_pubkey_t *pubkey)
{
	printf("------- PUBLIC KEY ------\n");
	print_uint(0, "Version", (unsigned)pubkey->version);
	print_time(0, "Creation Time", pubkey->birthtime);
	if (pubkey->version == PGP_V3) {
		print_uint(0, "Days Valid", pubkey->days_valid);
	}
	print_string_and_value(0, "Algorithm", pgp_show_pka(pubkey->alg),
			       pubkey->alg);
	switch (pubkey->alg) {
	case PGP_PKA_DSA:
		print_bn(0, "p", pubkey->key.dsa.p);
		print_bn(0, "q", pubkey->key.dsa.q);
		print_bn(0, "g", pubkey->key.dsa.g);
		print_bn(0, "y", pubkey->key.dsa.y);
		break;

	case PGP_PKA_RSA:
	case PGP_PKA_RSA_ENCRYPT_ONLY:
	case PGP_PKA_RSA_SIGN_ONLY:
		print_bn(0, "n", pubkey->key.rsa.n);
		print_bn(0, "e", pubkey->key.rsa.e);
		break;

	case PGP_PKA_ELGAMAL:
	case PGP_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
		print_bn(0, "p", pubkey->key.elgamal.p);
		print_bn(0, "g", pubkey->key.elgamal.g);
		print_bn(0, "y", pubkey->key.elgamal.y);
		break;

	default:
		(void) fprintf(stderr,
			"pgp_print_pubkey: Unusual algorithm\n");
	}

	printf("------- end of PUBLIC KEY ------\n");
}

int
pgp_sprint_pubkey(const pgp_key_t *key, char *out, size_t outsize)
{
	char	fp[(PGP_FINGERPRINT_SIZE * 3) + 1];
	int	cc;

	cc = snprintf(out, outsize, "key=%s\nname=%s\ncreation=%lld\nexpiry=%lld\nversion=%d\nalg=%d\n",
		strhexdump(fp, key->sigfingerprint.fingerprint, PGP_FINGERPRINT_SIZE, ""),
		key->uids[key->uid0],
		(long long)key->key.pubkey.birthtime,
		(long long)key->key.pubkey.days_valid,
		key->key.pubkey.version,
		key->key.pubkey.alg);
	switch (key->key.pubkey.alg) {
	case PGP_PKA_DSA:
		cc += snprintf(&out[cc], outsize - cc,
			"p=%s\nq=%s\ng=%s\ny=%s\n",
			BN_bn2hex(key->key.pubkey.key.dsa.p),
			BN_bn2hex(key->key.pubkey.key.dsa.q),
			BN_bn2hex(key->key.pubkey.key.dsa.g),
			BN_bn2hex(key->key.pubkey.key.dsa.y));
		break;
	case PGP_PKA_RSA:
	case PGP_PKA_RSA_ENCRYPT_ONLY:
	case PGP_PKA_RSA_SIGN_ONLY:
		cc += snprintf(&out[cc], outsize - cc,
			"n=%s\ne=%s\n",
			BN_bn2hex(key->key.pubkey.key.rsa.n),
			BN_bn2hex(key->key.pubkey.key.rsa.e));
		break;
	case PGP_PKA_ELGAMAL:
	case PGP_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
		cc += snprintf(&out[cc], outsize - cc,
			"p=%s\ng=%s\ny=%s\n",
			BN_bn2hex(key->key.pubkey.key.elgamal.p),
			BN_bn2hex(key->key.pubkey.key.elgamal.g),
			BN_bn2hex(key->key.pubkey.key.elgamal.y));
		break;
	default:
		(void) fprintf(stderr,
			"pgp_print_pubkey: Unusual algorithm\n");
	}
	return cc;
}

/**
\ingroup Core_Print
\param type
\param seckey
*/
static void
print_seckey_verbose(const pgp_content_enum type,
				const pgp_seckey_t *seckey)
{
	printf("------- SECRET KEY or ENCRYPTED SECRET KEY ------\n");
	print_tagname(0, (type == PGP_PTAG_CT_SECRET_KEY) ?
			"SECRET_KEY" :
			"ENCRYPTED_SECRET_KEY");
	/* pgp_print_pubkey(key); */
	printf("S2K Usage: %d\n", seckey->s2k_usage);
	if (seckey->s2k_usage != PGP_S2KU_NONE) {
		printf("S2K Specifier: %d\n", seckey->s2k_specifier);
		printf("Symmetric algorithm: %d (%s)\n", seckey->alg,
		       pgp_show_symm_alg(seckey->alg));
		printf("Hash algorithm: %d (%s)\n", seckey->hash_alg,
		       pgp_show_hash_alg((uint8_t)seckey->hash_alg));
		if (seckey->s2k_specifier != PGP_S2KS_SIMPLE) {
			print_hexdump(0, "Salt", seckey->salt,
					(unsigned)sizeof(seckey->salt));
		}
		if (seckey->s2k_specifier == PGP_S2KS_ITERATED_AND_SALTED) {
			printf("Octet count: %u\n", seckey->octetc);
		}
		print_hexdump(0, "IV", seckey->iv, pgp_block_size(seckey->alg));
	}
	/* no more set if encrypted */
	if (type == PGP_PTAG_CT_ENCRYPTED_SECRET_KEY) {
		return;
	}
	switch (seckey->pubkey.alg) {
	case PGP_PKA_RSA:
		print_bn(0, "d", seckey->key.rsa.d);
		print_bn(0, "p", seckey->key.rsa.p);
		print_bn(0, "q", seckey->key.rsa.q);
		print_bn(0, "u", seckey->key.rsa.u);
		break;

	case PGP_PKA_DSA:
		print_bn(0, "x", seckey->key.dsa.x);
		break;

	default:
		(void) fprintf(stderr,
			"print_seckey_verbose: unusual algorithm\n");
	}
	if (seckey->s2k_usage == PGP_S2KU_ENCRYPTED_AND_HASHED) {
		print_hexdump(0, "Checkhash", seckey->checkhash,
				PGP_CHECKHASH_SIZE);
	} else {
		printf("Checksum: %04x\n", seckey->checksum);
	}
	printf("------- end of SECRET KEY or ENCRYPTED SECRET KEY ------\n");
}


/**
\ingroup Core_Print
\param tag
\param key
*/
static void 
print_pk_sesskey(pgp_content_enum tag,
			 const pgp_pk_sesskey_t * key)
{
	print_tagname(0, (tag == PGP_PTAG_CT_PK_SESSION_KEY) ?
		"PUBLIC KEY SESSION KEY" :
		"ENCRYPTED PUBLIC KEY SESSION KEY");
	printf("Version: %d\n", key->version);
	print_hexdump(0, "Key ID", key->key_id, (unsigned)sizeof(key->key_id));
	printf("Algorithm: %d (%s)\n", key->alg,
	       pgp_show_pka(key->alg));
	switch (key->alg) {
	case PGP_PKA_RSA:
		print_bn(0, "encrypted_m", key->params.rsa.encrypted_m);
		break;

	case PGP_PKA_ELGAMAL:
		print_bn(0, "g_to_k", key->params.elgamal.g_to_k);
		print_bn(0, "encrypted_m", key->params.elgamal.encrypted_m);
		break;

	default:
		(void) fprintf(stderr,
			"print_pk_sesskey: unusual algorithm\n");
	}
	if (tag == PGP_PTAG_CT_PK_SESSION_KEY) {
		printf("Symmetric algorithm: %d (%s)\n", key->symm_alg,
		       pgp_show_symm_alg(key->symm_alg));
		print_hexdump(0, "Key", key->key, pgp_key_size(key->symm_alg));
		printf("Checksum: %04x\n", key->checksum);
	}
}

static void 
start_subpacket(int *indent, int type)
{
	*indent += 1;
	print_indent(*indent);
	printf("-- %s (type 0x%02x)\n",
	       pgp_show_ss_type((pgp_content_enum)type),
	       type - PGP_PTAG_SIG_SUBPKT_BASE);
}

static void 
end_subpacket(int *indent)
{
	*indent -= 1;
}

/**
\ingroup Core_Print
\param contents
*/
int 
pgp_print_packet(pgp_printstate_t *print, const pgp_packet_t *pkt)
{
	const pgp_contents_t	*content = &pkt->u;
	pgp_text_t		*text;
	const char		*str;

	if (print->unarmoured && pkt->tag != PGP_PTAG_CT_UNARMOURED_TEXT) {
		print->unarmoured = 0;
		puts("UNARMOURED TEXT ends");
	}
	if (pkt->tag == PGP_PARSER_PTAG) {
		printf("=> PGP_PARSER_PTAG: %s\n",
			pgp_show_packet_tag((pgp_content_enum)content->ptag.type));
	} else {
		printf("=> %s\n", pgp_show_packet_tag(pkt->tag));
	}

	switch (pkt->tag) {
	case PGP_PARSER_ERROR:
		printf("parse error: %s\n", content->error);
		break;

	case PGP_PARSER_ERRCODE:
		printf("parse error: %s\n",
		       pgp_errcode(content->errcode.errcode));
		break;

	case PGP_PARSER_PACKET_END:
		print_packet_hex(&content->packet);
		break;

	case PGP_PARSER_PTAG:
		if (content->ptag.type == PGP_PTAG_CT_PUBLIC_KEY) {
			print->indent = 0;
			printf("\n*** NEXT KEY ***\n");
		}
		printf("\n");
		print_indent(print->indent);
		printf("==== ptag new_format=%u type=%u length_type=%d"
		       " length=0x%x (%u) position=0x%x (%u)\n",
		       content->ptag.new_format,
		       content->ptag.type, content->ptag.length_type,
		       content->ptag.length, content->ptag.length,
		       content->ptag.position, content->ptag.position);
		print_tagname(print->indent, pgp_show_packet_tag((pgp_content_enum)content->ptag.type));
		break;

	case PGP_PTAG_CT_SE_DATA_HEADER:
		print_tagname(print->indent, "SYMMETRIC ENCRYPTED DATA");
		break;

	case PGP_PTAG_CT_SE_IP_DATA_HEADER:
		print_tagname(print->indent, 
			"SYMMETRIC ENCRYPTED INTEGRITY PROTECTED DATA HEADER");
		printf("Version: %d\n", content->se_ip_data_header);
		break;

	case PGP_PTAG_CT_SE_IP_DATA_BODY:
		print_tagname(print->indent, 
			"SYMMETRIC ENCRYPTED INTEGRITY PROTECTED DATA BODY");
		hexdump(stdout, "data", content->se_data_body.data,
			content->se_data_body.length);
		break;

	case PGP_PTAG_CT_PUBLIC_KEY:
	case PGP_PTAG_CT_PUBLIC_SUBKEY:
		print_tagname(print->indent, (pkt->tag == PGP_PTAG_CT_PUBLIC_KEY) ?
			"PUBLIC KEY" :
			"PUBLIC SUBKEY");
		pgp_print_pubkey(&content->pubkey);
		break;

	case PGP_PTAG_CT_TRUST:
		print_tagname(print->indent, "TRUST");
		print_data(print->indent, "Trust", &content->trust);
		break;

	case PGP_PTAG_CT_USER_ID:
		print_tagname(print->indent, "USER ID");
		print_utf8_string(print->indent, "userid", content->userid);
		break;

	case PGP_PTAG_CT_SIGNATURE:
		print_tagname(print->indent, "SIGNATURE");
		print_indent(print->indent);
		print_uint(print->indent, "Signature Version",
				   (unsigned)content->sig.info.version);
		if (content->sig.info.birthtime_set) {
			print_time(print->indent, "Signature Creation Time",
				   content->sig.info.birthtime);
		}
		if (content->sig.info.duration_set) {
			print_uint(print->indent, "Signature Duration",
				   (unsigned)content->sig.info.duration);
		}

		print_string_and_value(print->indent, "Signature Type",
			    pgp_show_sig_type(content->sig.info.type),
				       content->sig.info.type);

		if (content->sig.info.signer_id_set) {
			hexdump_data(print->indent, "Signer ID",
					   content->sig.info.signer_id,
				  (unsigned)sizeof(content->sig.info.signer_id));
		}

		print_string_and_value(print->indent, "Public Key Algorithm",
			pgp_show_pka(content->sig.info.key_alg),
				     content->sig.info.key_alg);
		print_string_and_value(print->indent, "Hash Algorithm",
			pgp_show_hash_alg((uint8_t)
				content->sig.info.hash_alg),
			(uint8_t)content->sig.info.hash_alg);
		print_uint(print->indent, "Hashed data len",
			(unsigned)content->sig.info.v4_hashlen);
		print_indent(print->indent);
		hexdump_data(print->indent, "hash2", &content->sig.hash2[0], 2);
		switch (content->sig.info.key_alg) {
		case PGP_PKA_RSA:
		case PGP_PKA_RSA_SIGN_ONLY:
			print_bn(print->indent, "sig", content->sig.info.sig.rsa.sig);
			break;

		case PGP_PKA_DSA:
			print_bn(print->indent, "r", content->sig.info.sig.dsa.r);
			print_bn(print->indent, "s", content->sig.info.sig.dsa.s);
			break;

		case PGP_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
			print_bn(print->indent, "r", content->sig.info.sig.elgamal.r);
			print_bn(print->indent, "s", content->sig.info.sig.elgamal.s);
			break;

		default:
			(void) fprintf(stderr,
				"pgp_print_packet: Unusual algorithm\n");
			return 0;
		}

		if (content->sig.hash)
			printf("data hash is set\n");

		break;

	case PGP_PTAG_CT_COMPRESSED:
		print_tagname(print->indent, "COMPRESSED");
		print_uint(print->indent, "Compressed Data Type",
			(unsigned)content->compressed);
		break;

	case PGP_PTAG_CT_1_PASS_SIG:
		print_tagname(print->indent, "ONE PASS SIGNATURE");

		print_uint(print->indent, "Version", (unsigned)content->one_pass_sig.version);
		print_string_and_value(print->indent, "Signature Type",
		    pgp_show_sig_type(content->one_pass_sig.sig_type),
				       content->one_pass_sig.sig_type);
		print_string_and_value(print->indent, "Hash Algorithm",
			pgp_show_hash_alg((uint8_t)content->one_pass_sig.hash_alg),
			(uint8_t)content->one_pass_sig.hash_alg);
		print_string_and_value(print->indent, "Public Key Algorithm",
			pgp_show_pka(content->one_pass_sig.key_alg),
			content->one_pass_sig.key_alg);
		hexdump_data(print->indent, "Signer ID",
				   content->one_pass_sig.keyid,
				   (unsigned)sizeof(content->one_pass_sig.keyid));
		print_uint(print->indent, "Nested", content->one_pass_sig.nested);
		break;

	case PGP_PTAG_CT_USER_ATTR:
		print_tagname(print->indent, "USER ATTRIBUTE");
		print_hexdump(print->indent, "User Attribute",
			      content->userattr.contents,
			      (unsigned)content->userattr.len);
		break;

	case PGP_PTAG_RAW_SS:
		if (pkt->critical) {
			(void) fprintf(stderr, "contents are critical\n");
			return 0;
		}
		start_subpacket(&print->indent, pkt->tag);
		print_uint(print->indent, "Raw Signature Subpacket: tag",
			(unsigned)(content->ss_raw.tag -
		   	(unsigned)PGP_PTAG_SIG_SUBPKT_BASE));
		print_hexdump(print->indent, "Raw Data",
			      content->ss_raw.raw,
			      (unsigned)content->ss_raw.length);
		break;

	case PGP_PTAG_SS_CREATION_TIME:
		start_subpacket(&print->indent, pkt->tag);
		print_time(print->indent, "Signature Creation Time", content->ss_time);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_EXPIRATION_TIME:
		start_subpacket(&print->indent, pkt->tag);
		print_duration(print->indent, "Signature Expiration Time",
			content->ss_time);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_KEY_EXPIRY:
		start_subpacket(&print->indent, pkt->tag);
		print_duration(print->indent, "Key Expiration Time", content->ss_time);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_TRUST:
		start_subpacket(&print->indent, pkt->tag);
		print_string(print->indent, "Trust Signature", "");
		print_uint(print->indent, "Level", (unsigned)content->ss_trust.level);
		print_uint(print->indent, "Amount", (unsigned)content->ss_trust.amount);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_REVOCABLE:
		start_subpacket(&print->indent, pkt->tag);
		print_boolean(print->indent, "Revocable", content->ss_revocable);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_REVOCATION_KEY:
		start_subpacket(&print->indent, pkt->tag);
		/* not yet tested */
		printf("  revocation key: class=0x%x",
		       content->ss_revocation_key.class);
		if (content->ss_revocation_key.class & 0x40) {
			printf(" (sensitive)");
		}
		printf(", algid=0x%x", content->ss_revocation_key.algid);
		hexdump(stdout, "fingerprint", content->ss_revocation_key.fingerprint,
				PGP_FINGERPRINT_SIZE);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_ISSUER_KEY_ID:
		start_subpacket(&print->indent, pkt->tag);
		print_hexdump(print->indent, "Issuer Key Id",
			      content->ss_issuer, (unsigned)sizeof(content->ss_issuer));
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_PREFERRED_SKA:
		start_subpacket(&print->indent, pkt->tag);
		print_data(print->indent, "Preferred Symmetric Algorithms",
			   &content->ss_skapref);
		text = pgp_showall_ss_skapref(&content->ss_skapref);
		print_text_breakdown(print->indent, text);
		pgp_text_free(text);

		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_PRIMARY_USER_ID:
		start_subpacket(&print->indent, pkt->tag);
		print_boolean(print->indent, "Primary User ID",
			      content->ss_primary_userid);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_PREFERRED_HASH:
		start_subpacket(&print->indent, pkt->tag);
		print_data(print->indent, "Preferred Hash Algorithms",
			   &content->ss_hashpref);
		text = pgp_showall_ss_hashpref(&content->ss_hashpref);
		print_text_breakdown(print->indent, text);
		pgp_text_free(text);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_PREF_COMPRESS:
		start_subpacket(&print->indent, pkt->tag);
		print_data(print->indent, "Preferred Compression Algorithms",
			   &content->ss_zpref);
		text = pgp_showall_ss_zpref(&content->ss_zpref);
		print_text_breakdown(print->indent, text);
		pgp_text_free(text);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_KEY_FLAGS:
		start_subpacket(&print->indent, pkt->tag);
		print_data(print->indent, "Key Flags", &content->ss_key_flags);

		text = pgp_showall_ss_key_flags(&content->ss_key_flags);
		print_text_breakdown(print->indent, text);
		pgp_text_free(text);

		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_KEYSERV_PREFS:
		start_subpacket(&print->indent, pkt->tag);
		print_data(print->indent, "Key Server Preferences",
			   &content->ss_key_server_prefs);
		text = pgp_show_keyserv_prefs(&content->ss_key_server_prefs);
		print_text_breakdown(print->indent, text);
		pgp_text_free(text);

		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_FEATURES:
		start_subpacket(&print->indent, pkt->tag);
		print_data(print->indent, "Features", &content->ss_features);
		text = pgp_showall_ss_features(content->ss_features);
		print_text_breakdown(print->indent, text);
		pgp_text_free(text);

		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_NOTATION_DATA:
		start_subpacket(&print->indent, pkt->tag);
		print_indent(print->indent);
		printf("Notation Data:\n");

		print->indent++;
		print_data(print->indent, "Flags", &content->ss_notation.flags);
		text = pgp_showall_notation(content->ss_notation);
		print_text_breakdown(print->indent, text);
		pgp_text_free(text);

		print_data(print->indent, "Name", &content->ss_notation.name);

		print_data(print->indent, "Value", &content->ss_notation.value);

		print->indent--;
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_REGEXP:
		start_subpacket(&print->indent, pkt->tag);
		print_hexdump(print->indent, "Regular Expression",
			      (uint8_t *) content->ss_regexp,
			      (unsigned)strlen(content->ss_regexp));
		print_string(print->indent, NULL, content->ss_regexp);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_POLICY_URI:
		start_subpacket(&print->indent, pkt->tag);
		print_string(print->indent, "Policy URL", content->ss_policy);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_SIGNERS_USER_ID:
		start_subpacket(&print->indent, pkt->tag);
		print_utf8_string(print->indent, "Signer's User ID", content->ss_signer);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_PREF_KEYSERV:
		start_subpacket(&print->indent, pkt->tag);
		print_string(print->indent, "Preferred Key Server", content->ss_keyserv);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_EMBEDDED_SIGNATURE:
		start_subpacket(&print->indent, pkt->tag);
		end_subpacket(&print->indent);/* \todo print out contents? */
		break;

	case PGP_PTAG_SS_USERDEFINED00:
	case PGP_PTAG_SS_USERDEFINED01:
	case PGP_PTAG_SS_USERDEFINED02:
	case PGP_PTAG_SS_USERDEFINED03:
	case PGP_PTAG_SS_USERDEFINED04:
	case PGP_PTAG_SS_USERDEFINED05:
	case PGP_PTAG_SS_USERDEFINED06:
	case PGP_PTAG_SS_USERDEFINED07:
	case PGP_PTAG_SS_USERDEFINED08:
	case PGP_PTAG_SS_USERDEFINED09:
	case PGP_PTAG_SS_USERDEFINED10:
		start_subpacket(&print->indent, pkt->tag);
		print_hexdump(print->indent, "Internal or user-defined",
			      content->ss_userdef.contents,
			      (unsigned)content->ss_userdef.len);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_RESERVED:
		start_subpacket(&print->indent, pkt->tag);
		print_hexdump(print->indent, "Reserved",
			      content->ss_userdef.contents,
			      (unsigned)content->ss_userdef.len);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_SS_REVOCATION_REASON:
		start_subpacket(&print->indent, pkt->tag);
		print_hexdump(print->indent, "Revocation Reason",
			      &content->ss_revocation.code,
			      1);
		str = pgp_show_ss_rr_code(content->ss_revocation.code);
		print_string(print->indent, NULL, str);
		end_subpacket(&print->indent);
		break;

	case PGP_PTAG_CT_LITDATA_HEADER:
		print_tagname(print->indent, "LITERAL DATA HEADER");
		printf("  literal data header format=%c filename='%s'\n",
		       content->litdata_header.format,
		       content->litdata_header.filename);
		showtime("    modification time",
			 content->litdata_header.mtime);
		printf("\n");
		break;

	case PGP_PTAG_CT_LITDATA_BODY:
		print_tagname(print->indent, "LITERAL DATA BODY");
		printf("  literal data body length=%u\n",
		       content->litdata_body.length);
		printf("    data=");
		print_escaped(content->litdata_body.data,
			      content->litdata_body.length);
		printf("\n");
		break;

	case PGP_PTAG_CT_SIGNATURE_HEADER:
		print_tagname(print->indent, "SIGNATURE");
		print_indent(print->indent);
		print_uint(print->indent, "Signature Version",
				   (unsigned)content->sig.info.version);
		if (content->sig.info.birthtime_set) {
			print_time(print->indent, "Signature Creation Time",
				content->sig.info.birthtime);
		}
		if (content->sig.info.duration_set) {
			print_uint(print->indent, "Signature Duration",
				   (unsigned)content->sig.info.duration);
		}
		print_string_and_value(print->indent, "Signature Type",
			    pgp_show_sig_type(content->sig.info.type),
				       content->sig.info.type);
		if (content->sig.info.signer_id_set) {
			hexdump_data(print->indent, "Signer ID",
				content->sig.info.signer_id,
				(unsigned)sizeof(content->sig.info.signer_id));
		}
		print_string_and_value(print->indent, "Public Key Algorithm",
			pgp_show_pka(content->sig.info.key_alg),
				     content->sig.info.key_alg);
		print_string_and_value(print->indent, "Hash Algorithm",
			pgp_show_hash_alg((uint8_t)content->sig.info.hash_alg),
			(uint8_t)content->sig.info.hash_alg);
		print_uint(print->indent, "Hashed data len",
			(unsigned)content->sig.info.v4_hashlen);

		break;

	case PGP_PTAG_CT_SIGNATURE_FOOTER:
		print_indent(print->indent);
		hexdump_data(print->indent, "hash2", &content->sig.hash2[0], 2);

		switch (content->sig.info.key_alg) {
		case PGP_PKA_RSA:
			print_bn(print->indent, "sig", content->sig.info.sig.rsa.sig);
			break;

		case PGP_PKA_DSA:
			print_bn(print->indent, "r", content->sig.info.sig.dsa.r);
			print_bn(print->indent, "s", content->sig.info.sig.dsa.s);
			break;

		case PGP_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
			print_bn(print->indent, "r", content->sig.info.sig.elgamal.r);
			print_bn(print->indent, "s", content->sig.info.sig.elgamal.s);
			break;

		case PGP_PKA_PRIVATE00:
		case PGP_PKA_PRIVATE01:
		case PGP_PKA_PRIVATE02:
		case PGP_PKA_PRIVATE03:
		case PGP_PKA_PRIVATE04:
		case PGP_PKA_PRIVATE05:
		case PGP_PKA_PRIVATE06:
		case PGP_PKA_PRIVATE07:
		case PGP_PKA_PRIVATE08:
		case PGP_PKA_PRIVATE09:
		case PGP_PKA_PRIVATE10:
			print_data(print->indent, "Private/Experimental",
			   &content->sig.info.sig.unknown);
			break;

		default:
			(void) fprintf(stderr,
				"pgp_print_packet: Unusual key algorithm\n");
			return 0;
		}
		break;

	case PGP_GET_PASSPHRASE:
		print_tagname(print->indent, "PGP_GET_PASSPHRASE");
		break;

	case PGP_PTAG_CT_SECRET_KEY:
		print_tagname(print->indent, "PGP_PTAG_CT_SECRET_KEY");
		print_seckey_verbose(pkt->tag, &content->seckey);
		break;

	case PGP_PTAG_CT_ENCRYPTED_SECRET_KEY:
		print_tagname(print->indent, "PGP_PTAG_CT_ENCRYPTED_SECRET_KEY");
		print_seckey_verbose(pkt->tag, &content->seckey);
		break;

	case PGP_PTAG_CT_ARMOUR_HEADER:
		print_tagname(print->indent, "ARMOUR HEADER");
		print_string(print->indent, "type", content->armour_header.type);
		break;

	case PGP_PTAG_CT_SIGNED_CLEARTEXT_HEADER:
		print_tagname(print->indent, "SIGNED CLEARTEXT HEADER");
		print_headers(&content->cleartext_head);
		break;

	case PGP_PTAG_CT_SIGNED_CLEARTEXT_BODY:
		print_tagname(print->indent, "SIGNED CLEARTEXT BODY");
		print_block(print->indent, "signed cleartext", content->cleartext_body.data,
			    content->cleartext_body.length);
		break;

	case PGP_PTAG_CT_SIGNED_CLEARTEXT_TRAILER:
		print_tagname(print->indent, "SIGNED CLEARTEXT TRAILER");
		printf("hash algorithm: %d\n",
		       content->cleartext_trailer->alg);
		printf("\n");
		break;

	case PGP_PTAG_CT_UNARMOURED_TEXT:
		if (!print->unarmoured) {
			print_tagname(print->indent, "UNARMOURED TEXT");
			print->unarmoured = 1;
		}
		putchar('[');
		print_escaped(content->unarmoured_text.data,
			      content->unarmoured_text.length);
		putchar(']');
		break;

	case PGP_PTAG_CT_ARMOUR_TRAILER:
		print_tagname(print->indent, "ARMOUR TRAILER");
		print_string(print->indent, "type", content->armour_header.type);
		break;

	case PGP_PTAG_CT_PK_SESSION_KEY:
	case PGP_PTAG_CT_ENCRYPTED_PK_SESSION_KEY:
		print_pk_sesskey(pkt->tag, &content->pk_sesskey);
		break;

	case PGP_GET_SECKEY:
		print_pk_sesskey(PGP_PTAG_CT_ENCRYPTED_PK_SESSION_KEY,
				    content->get_seckey.pk_sesskey);
		break;

	default:
		print_tagname(print->indent, "UNKNOWN PACKET TYPE");
		fprintf(stderr, "pgp_print_packet: unknown tag=%d (0x%x)\n",
			pkt->tag, pkt->tag);
		return 0;
	}
	return 1;
}

static pgp_cb_ret_t 
cb_list_packets(const pgp_packet_t *pkt, pgp_cbdata_t *cbinfo)
{
	pgp_print_packet(&cbinfo->printstate, pkt);
	return PGP_RELEASE_MEMORY;
}

/**
\ingroup Core_Print
\param filename
\param armour
\param keyring
\param cb_get_passphrase
*/
int 
pgp_list_packets(pgp_io_t *io,
			char *filename,
			unsigned armour,
			pgp_keyring_t *secring,
			pgp_keyring_t *pubring,
			void *passfp,
			pgp_cbfunc_t *cb_get_passphrase)
{
	pgp_stream_t	*stream = NULL;
	const unsigned	 accumulate = 1;
	const int	 printerrors = 1;
	int		 fd;

	fd = pgp_setup_file_read(io, &stream, filename, NULL, cb_list_packets,
				accumulate);
	pgp_parse_options(stream, PGP_PTAG_SS_ALL, PGP_PARSE_PARSED);
	stream->cryptinfo.secring = secring;
	stream->cryptinfo.pubring = pubring;
	stream->cbinfo.passfp = passfp;
	stream->cryptinfo.getpassphrase = cb_get_passphrase;
	if (armour) {
		pgp_reader_push_dearmour(stream);
	}
	pgp_parse(stream, printerrors);
	pgp_teardown_file_read(stream, fd);
	return 1;
}
