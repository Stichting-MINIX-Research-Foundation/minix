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
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: validate.c,v 1.44 2012/03/05 02:20:18 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <string.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "packet-parse.h"
#include "packet-show.h"
#include "keyring.h"
#include "signature.h"
#include "netpgpsdk.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "memory.h"
#include "packet.h"
#include "crypto.h"
#include "validate.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif


static int 
keydata_reader(pgp_stream_t *stream, void *dest, size_t length, pgp_error_t **errors,
	       pgp_reader_t *readinfo,
	       pgp_cbdata_t *cbinfo)
{
	validate_reader_t *reader = pgp_reader_get_arg(readinfo);

	__PGP_USED(stream);
	__PGP_USED(errors);
	__PGP_USED(cbinfo);
	if (reader->offset == reader->key->packets[reader->packet].length) {
		reader->packet += 1;
		reader->offset = 0;
	}
	if (reader->packet == reader->key->packetc) {
		return 0;
	}

	/*
	 * we should never be asked to cross a packet boundary in a single
	 * read
	 */
	if (reader->key->packets[reader->packet].length <
			reader->offset + length) {
		(void) fprintf(stderr, "keydata_reader: weird length\n");
		return 0;
	}

	(void) memcpy(dest,
		&reader->key->packets[reader->packet].raw[reader->offset],
		length);
	reader->offset += (unsigned)length;

	return (int)length;
}

static void 
free_sig_info(pgp_sig_info_t *sig)
{
	free(sig->v4_hashed);
	free(sig);
}

static void 
copy_sig_info(pgp_sig_info_t *dst, const pgp_sig_info_t *src)
{
	(void) memcpy(dst, src, sizeof(*src));
	if ((dst->v4_hashed = calloc(1, src->v4_hashlen)) == NULL) {
		(void) fprintf(stderr, "copy_sig_info: bad alloc\n");
	} else {
		(void) memcpy(dst->v4_hashed, src->v4_hashed, src->v4_hashlen);
	}
}

static int 
add_sig_to_list(const pgp_sig_info_t *sig, pgp_sig_info_t **sigs,
			unsigned *count)
{
	pgp_sig_info_t	*newsigs;

	if (*count == 0) {
		newsigs = calloc(*count + 1, sizeof(pgp_sig_info_t));
	} else {
		newsigs = realloc(*sigs,
				(*count + 1) * sizeof(pgp_sig_info_t));
	}
	if (newsigs == NULL) {
		(void) fprintf(stderr, "add_sig_to_list: alloc failure\n");
		return 0;
	}
	*sigs = newsigs;
	copy_sig_info(&(*sigs)[*count], sig);
	*count += 1;
	return 1;
}

/*
The hash value is calculated by the following method:
+ hash the data using the given digest algorithm
+ hash the hash value onto the end
+ hash the trailer - 6 bytes
  [PGP_V4][0xff][len >> 24][len >> 16][len >> 8][len & 0xff]
to give the final hash value that is checked against the one in the signature
*/

/* Does the signed hash match the given hash? */
unsigned
check_binary_sig(const uint8_t *data,
		const unsigned len,
		const pgp_sig_t *sig,
		const pgp_pubkey_t *signer)
{
	unsigned    hashedlen;
	pgp_hash_t	hash;
	unsigned	n;
	uint8_t		hashout[PGP_MAX_HASH_SIZE];
	uint8_t		trailer[6];

	pgp_hash_any(&hash, sig->info.hash_alg);
	if (!hash.init(&hash)) {
		(void) fprintf(stderr, "check_binary_sig: bad hash init\n");
		return 0;
	}
	hash.add(&hash, data, len);
	switch (sig->info.version) {
	case PGP_V3:
		trailer[0] = sig->info.type;
		trailer[1] = (unsigned)(sig->info.birthtime) >> 24;
		trailer[2] = (unsigned)(sig->info.birthtime) >> 16;
		trailer[3] = (unsigned)(sig->info.birthtime) >> 8;
		trailer[4] = (uint8_t)(sig->info.birthtime);
		hash.add(&hash, trailer, 5);
		break;

	case PGP_V4:
		if (pgp_get_debug_level(__FILE__)) {
			hexdump(stderr, "v4 hash", sig->info.v4_hashed,
					sig->info.v4_hashlen);
		}
		hash.add(&hash, sig->info.v4_hashed, (unsigned)sig->info.v4_hashlen);
		trailer[0] = 0x04;	/* version */
		trailer[1] = 0xFF;
		hashedlen = (unsigned)sig->info.v4_hashlen;
		trailer[2] = (uint8_t)(hashedlen >> 24);
		trailer[3] = (uint8_t)(hashedlen >> 16);
		trailer[4] = (uint8_t)(hashedlen >> 8);
		trailer[5] = (uint8_t)(hashedlen);
		hash.add(&hash, trailer, 6);
		break;

	default:
		(void) fprintf(stderr, "Invalid signature version %d\n",
				sig->info.version);
		return 0;
	}

	n = hash.finish(&hash, hashout);
	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stdout, "hash out", hashout, n);
	}
	return pgp_check_sig(hashout, n, sig, signer);
}

pgp_cb_ret_t
pgp_validate_key_cb(const pgp_packet_t *pkt, pgp_cbdata_t *cbinfo)
{
	const pgp_contents_t	 *content = &pkt->u;
	const pgp_key_t	 *signer;
	validate_key_cb_t	 *key;
	pgp_pubkey_t		 *sigkey;
	pgp_error_t		**errors;
	pgp_io_t		 *io;
	unsigned		  from;
	unsigned		  valid = 0;

	io = cbinfo->io;
	if (pgp_get_debug_level(__FILE__)) {
		(void) fprintf(io->errs, "%s\n",
				pgp_show_packet_tag(pkt->tag));
	}
	key = pgp_callback_arg(cbinfo);
	errors = pgp_callback_errors(cbinfo);
	switch (pkt->tag) {
	case PGP_PTAG_CT_PUBLIC_KEY:
		if (key->pubkey.version != 0) {
			(void) fprintf(io->errs,
				"pgp_validate_key_cb: version bad\n");
			return PGP_FINISHED;
		}
		key->pubkey = content->pubkey;
		return PGP_KEEP_MEMORY;

	case PGP_PTAG_CT_PUBLIC_SUBKEY:
		if (key->subkey.version) {
			pgp_pubkey_free(&key->subkey);
		}
		key->subkey = content->pubkey;
		return PGP_KEEP_MEMORY;

	case PGP_PTAG_CT_SECRET_KEY:
		key->seckey = content->seckey;
		key->pubkey = key->seckey.pubkey;
		return PGP_KEEP_MEMORY;

	case PGP_PTAG_CT_USER_ID:
		if (key->userid) {
			pgp_userid_free(&key->userid);
		}
		key->userid = content->userid;
		key->last_seen = ID;
		return PGP_KEEP_MEMORY;

	case PGP_PTAG_CT_USER_ATTR:
		if (content->userattr.len == 0) {
			(void) fprintf(io->errs,
			"pgp_validate_key_cb: user attribute length 0");
			return PGP_FINISHED;
		}
		(void) fprintf(io->outs, "user attribute, length=%d\n",
			(int) content->userattr.len);
		if (key->userattr.len) {
			pgp_data_free(&key->userattr);
		}
		key->userattr = content->userattr;
		key->last_seen = ATTRIBUTE;
		return PGP_KEEP_MEMORY;

	case PGP_PTAG_CT_SIGNATURE:	/* V3 sigs */
	case PGP_PTAG_CT_SIGNATURE_FOOTER:	/* V4 sigs */
		from = 0;
		signer = pgp_getkeybyid(io, key->keyring,
					 content->sig.info.signer_id,
					 &from, &sigkey);
		if (!signer) {
			if (!add_sig_to_list(&content->sig.info,
				&key->result->unknown_sigs,
				&key->result->unknownc)) {
					(void) fprintf(io->errs,
					"pgp_validate_key_cb: user attribute length 0");
					return PGP_FINISHED;
			}
			break;
		}
		if (sigkey == &signer->enckey) {
			(void) fprintf(io->errs,
				"WARNING: signature made with encryption key\n");
		}
		switch (content->sig.info.type) {
		case PGP_CERT_GENERIC:
		case PGP_CERT_PERSONA:
		case PGP_CERT_CASUAL:
		case PGP_CERT_POSITIVE:
		case PGP_SIG_REV_CERT:
			valid = (key->last_seen == ID) ?
			    pgp_check_useridcert_sig(&key->pubkey,
					key->userid,
					&content->sig,
					pgp_get_pubkey(signer),
					key->reader->key->packets[
						key->reader->packet].raw) :
			    pgp_check_userattrcert_sig(&key->pubkey,
					&key->userattr,
					&content->sig,
				       pgp_get_pubkey(signer),
					key->reader->key->packets[
						key->reader->packet].raw);
			break;

		case PGP_SIG_SUBKEY:
			/*
			 * XXX: we should also check that the signer is the
			 * key we are validating, I think.
			 */
			valid = pgp_check_subkey_sig(&key->pubkey,
				&key->subkey,
				&content->sig,
				pgp_get_pubkey(signer),
				key->reader->key->packets[
					key->reader->packet].raw);
			break;

		case PGP_SIG_DIRECT:
			valid = pgp_check_direct_sig(&key->pubkey,
				&content->sig,
				pgp_get_pubkey(signer),
				key->reader->key->packets[
					key->reader->packet].raw);
			break;

		case PGP_SIG_STANDALONE:
		case PGP_SIG_PRIMARY:
		case PGP_SIG_REV_KEY:
		case PGP_SIG_REV_SUBKEY:
		case PGP_SIG_TIMESTAMP:
		case PGP_SIG_3RD_PARTY:
			PGP_ERROR_1(errors, PGP_E_UNIMPLEMENTED,
				"Sig Verification type 0x%02x not done yet\n",
				content->sig.info.type);
			break;

		default:
			PGP_ERROR_1(errors, PGP_E_UNIMPLEMENTED,
				    "Unexpected signature type 0x%02x\n",
				    	content->sig.info.type);
		}

		if (valid) {
			if (!add_sig_to_list(&content->sig.info,
				&key->result->valid_sigs,
				&key->result->validc)) {
				PGP_ERROR_1(errors, PGP_E_UNIMPLEMENTED, "%s",
				    "Can't add good sig to list\n");
			}
		} else {
			PGP_ERROR_1(errors, PGP_E_V_BAD_SIGNATURE, "%s",
			    "Bad Sig");
			if (!add_sig_to_list(&content->sig.info,
				&key->result->invalid_sigs,
				&key->result->invalidc)) {
				PGP_ERROR_1(errors, PGP_E_UNIMPLEMENTED, "%s",
				    "Can't add good sig to list\n");
			}
		}
		break;

		/* ignore these */
	case PGP_PARSER_PTAG:
	case PGP_PTAG_CT_SIGNATURE_HEADER:
	case PGP_PARSER_PACKET_END:
		break;

	case PGP_GET_PASSPHRASE:
		if (key->getpassphrase) {
			return key->getpassphrase(pkt, cbinfo);
		}
		break;

	case PGP_PTAG_CT_TRUST:
		/* 1 byte for level (depth), 1 byte for trust amount */
		printf("trust dump\n");
		printf("Got trust\n");
		//hexdump(stdout, (const uint8_t *)content->trust.data, 10, " ");
		//hexdump(stdout, (const uint8_t *)&content->ss_trust, 2, " ");
		//printf("Trust level %d, amount %d\n", key->trust.level, key->trust.amount);
		break;

	default:
		(void) fprintf(stderr, "unexpected tag=0x%x\n", pkt->tag);
		return PGP_FINISHED;
	}
	return PGP_RELEASE_MEMORY;
}

pgp_cb_ret_t
validate_data_cb(const pgp_packet_t *pkt, pgp_cbdata_t *cbinfo)
{
	const pgp_contents_t	 *content = &pkt->u;
	const pgp_key_t	 *signer;
	validate_data_cb_t	 *data;
	pgp_pubkey_t		 *sigkey;
	pgp_error_t		**errors;
	pgp_io_t		 *io;
	unsigned		  from;
	unsigned		  valid = 0;

	io = cbinfo->io;
	if (pgp_get_debug_level(__FILE__)) {
		(void) fprintf(io->errs, "validate_data_cb: %s\n",
				pgp_show_packet_tag(pkt->tag));
	}
	data = pgp_callback_arg(cbinfo);
	errors = pgp_callback_errors(cbinfo);
	switch (pkt->tag) {
	case PGP_PTAG_CT_SIGNED_CLEARTEXT_HEADER:
		/*
		 * ignore - this gives us the "Armor Header" line "Hash:
		 * SHA1" or similar
		 */
		break;

	case PGP_PTAG_CT_LITDATA_HEADER:
		/* ignore */
		break;

	case PGP_PTAG_CT_LITDATA_BODY:
		data->data.litdata_body = content->litdata_body;
		data->type = LITDATA;
		pgp_memory_add(data->mem, data->data.litdata_body.data,
				       data->data.litdata_body.length);
		return PGP_KEEP_MEMORY;

	case PGP_PTAG_CT_SIGNED_CLEARTEXT_BODY:
		data->data.cleartext_body = content->cleartext_body;
		data->type = SIGNED_CLEARTEXT;
		pgp_memory_add(data->mem, data->data.cleartext_body.data,
			       data->data.cleartext_body.length);
		return PGP_KEEP_MEMORY;

	case PGP_PTAG_CT_SIGNED_CLEARTEXT_TRAILER:
		/* this gives us an pgp_hash_t struct */
		break;

	case PGP_PTAG_CT_SIGNATURE:	/* V3 sigs */
	case PGP_PTAG_CT_SIGNATURE_FOOTER:	/* V4 sigs */
		if (pgp_get_debug_level(__FILE__)) {
			hexdump(io->outs, "hashed data", content->sig.info.v4_hashed,
					content->sig.info.v4_hashlen);
			hexdump(io->outs, "signer id", content->sig.info.signer_id,
				sizeof(content->sig.info.signer_id));
		}
		from = 0;
		signer = pgp_getkeybyid(io, data->keyring,
					 content->sig.info.signer_id, &from, &sigkey);
		if (!signer) {
			PGP_ERROR_1(errors, PGP_E_V_UNKNOWN_SIGNER,
			    "%s", "Unknown Signer");
			if (!add_sig_to_list(&content->sig.info,
					&data->result->unknown_sigs,
					&data->result->unknownc)) {
				PGP_ERROR_1(errors, PGP_E_V_UNKNOWN_SIGNER,
				    "%s", "Can't add unknown sig to list");
			}
			break;
		}
		if (sigkey == &signer->enckey) {
			(void) fprintf(io->errs,
				"WARNING: signature made with encryption key\n");
		}
		if (content->sig.info.birthtime_set) {
			data->result->birthtime = content->sig.info.birthtime;
		}
		if (content->sig.info.duration_set) {
			data->result->duration = content->sig.info.duration;
		}
		switch (content->sig.info.type) {
		case PGP_SIG_BINARY:
		case PGP_SIG_TEXT:
			if (pgp_mem_len(data->mem) == 0 &&
			    data->detachname) {
				/* check we have seen some data */
				/* if not, need to read from detached name */
				(void) fprintf(io->errs,
				"netpgp: assuming signed data in \"%s\"\n",
					data->detachname);
				data->mem = pgp_memory_new();
				pgp_mem_readfile(data->mem, data->detachname);
			}
			if (pgp_get_debug_level(__FILE__)) {
				hexdump(stderr, "sig dump", (const uint8_t *)(const void *)&content->sig,
					sizeof(content->sig));
			}
			valid = check_binary_sig(pgp_mem_data(data->mem),
					(const unsigned)pgp_mem_len(data->mem),
					&content->sig,
					pgp_get_pubkey(signer));
			break;

		default:
			PGP_ERROR_1(errors, PGP_E_UNIMPLEMENTED,
				    "No Sig Verification type 0x%02x yet\n",
				    content->sig.info.type);
			break;

		}

		if (valid) {
			if (!add_sig_to_list(&content->sig.info,
					&data->result->valid_sigs,
					&data->result->validc)) {
				PGP_ERROR_1(errors, PGP_E_V_BAD_SIGNATURE,
				    "%s", "Can't add good sig to list");
			}
		} else {
			PGP_ERROR_1(errors, PGP_E_V_BAD_SIGNATURE,
			    "%s", "Bad Signature");
			if (!add_sig_to_list(&content->sig.info,
					&data->result->invalid_sigs,
					&data->result->invalidc)) {
				PGP_ERROR_1(errors, PGP_E_V_BAD_SIGNATURE, "%s",
					"Can't add good sig to list");
			}
		}
		break;

		/* ignore these */
	case PGP_PARSER_PTAG:
	case PGP_PTAG_CT_SIGNATURE_HEADER:
	case PGP_PTAG_CT_ARMOUR_HEADER:
	case PGP_PTAG_CT_ARMOUR_TRAILER:
	case PGP_PTAG_CT_1_PASS_SIG:
		break;

	case PGP_PARSER_PACKET_END:
		break;

	default:
		PGP_ERROR_1(errors, PGP_E_V_NO_SIGNATURE, "%s", "No signature");
		break;
	}
	return PGP_RELEASE_MEMORY;
}

static void 
keydata_destroyer(pgp_reader_t *readinfo)
{
	free(pgp_reader_get_arg(readinfo));
}

void 
pgp_keydata_reader_set(pgp_stream_t *stream, const pgp_key_t *key)
{
	validate_reader_t *data;

	if ((data = calloc(1, sizeof(*data))) == NULL) {
		(void) fprintf(stderr, "pgp_keydata_reader_set: bad alloc\n");
	} else {
		data->key = key;
		data->packet = 0;
		data->offset = 0;
		pgp_reader_set(stream, keydata_reader, keydata_destroyer, data);
	}
}

static char *
fmtsecs(int64_t n, char *buf, size_t size)
{
	if (n > 365 * 24 * 60 * 60) {
		n /= (365 * 24 * 60 * 60);
		(void) snprintf(buf, size, "%" PRId64 " year%s", n, (n == 1) ? "" : "s");
		return buf;
	}
	if (n > 30 * 24 * 60 * 60) {
		n /= (30 * 24 * 60 * 60);
		(void) snprintf(buf, size, "%" PRId64 " month%s", n, (n == 1) ? "" : "s");
		return buf;
	}
	if (n > 24 * 60 * 60) {
		n /= (24 * 60 * 60);
		(void) snprintf(buf, size, "%" PRId64 " day%s", n, (n == 1) ? "" : "s");
		return buf;
	}
	if (n > 60 * 60) {
		n /= (60 * 60);
		(void) snprintf(buf, size, "%" PRId64 " hour%s", n, (n == 1) ? "" : "s");
		return buf;
	}
	if (n > 60) {
		n /= 60;
		(void) snprintf(buf, size, "%" PRId64 " minute%s", n, (n == 1) ? "" : "s");
		return buf;
	}
	(void) snprintf(buf, size, "%" PRId64 " second%s", n, (n == 1) ? "" : "s");
	return buf;
}

/**
 * \ingroup HighLevel_Verify
 * \brief Indicicates whether any errors were found
 * \param result Validation result to check
 * \return 0 if any invalid signatures or unknown signers
 	or no valid signatures; else 1
 */
static unsigned 
validate_result_status(FILE *errs, const char *f, pgp_validation_t *val)
{
	time_t	now;
	time_t	t;
	char	buf[128];

	now = time(NULL);
	if (now < val->birthtime) {
		/* signature is not valid yet! */
		if (f) {
			(void) fprintf(errs, "\"%s\": ", f);
		} else {
			(void) fprintf(errs, "memory ");
		}
		(void) fprintf(errs,
			"signature not valid until %.24s (%s)\n",
			ctime(&val->birthtime),
			fmtsecs((int64_t)(val->birthtime - now), buf, sizeof(buf)));
		return 0;
	}
	if (val->duration != 0 && now > val->birthtime + val->duration) {
		/* signature has expired */
		t = val->duration + val->birthtime;
		if (f) {
			(void) fprintf(errs, "\"%s\": ", f);
		} else {
			(void) fprintf(errs, "memory ");
		}
		(void) fprintf(errs,
			"signature not valid after %.24s (%s ago)\n",
			ctime(&t),
			fmtsecs((int64_t)(now - t), buf, sizeof(buf)));
		return 0;
	}
	return val->validc && !val->invalidc && !val->unknownc;
}

/**
 * \ingroup HighLevel_Verify
 * \brief Validate all signatures on a single key against the given keyring
 * \param result Where to put the result
 * \param key Key to validate
 * \param keyring Keyring to use for validation
 * \param cb_get_passphrase Callback to use to get passphrase
 * \return 1 if all signatures OK; else 0
 * \note It is the caller's responsiblity to free result after use.
 * \sa pgp_validate_result_free()
 */
unsigned 
pgp_validate_key_sigs(pgp_validation_t *result,
	const pgp_key_t *key,
	const pgp_keyring_t *keyring,
	pgp_cb_ret_t cb_get_passphrase(const pgp_packet_t *,
						pgp_cbdata_t *))
{
	pgp_stream_t	*stream;
	validate_key_cb_t	 keysigs;
	const int		 printerrors = 1;

	(void) memset(&keysigs, 0x0, sizeof(keysigs));
	keysigs.result = result;
	keysigs.getpassphrase = cb_get_passphrase;

	stream = pgp_new(sizeof(*stream));
	/* pgp_parse_options(&opt,PGP_PTAG_CT_SIGNATURE,PGP_PARSE_PARSED); */

	keysigs.keyring = keyring;

	pgp_set_callback(stream, pgp_validate_key_cb, &keysigs);
	stream->readinfo.accumulate = 1;
	pgp_keydata_reader_set(stream, key);

	/* Note: Coverity incorrectly reports an error that keysigs.reader */
	/* is never used. */
	keysigs.reader = stream->readinfo.arg;

	pgp_parse(stream, !printerrors);

	pgp_pubkey_free(&keysigs.pubkey);
	if (keysigs.subkey.version) {
		pgp_pubkey_free(&keysigs.subkey);
	}
	pgp_userid_free(&keysigs.userid);
	pgp_data_free(&keysigs.userattr);

	pgp_stream_delete(stream);

	return (!result->invalidc && !result->unknownc && result->validc);
}

/**
   \ingroup HighLevel_Verify
   \param result Where to put the result
   \param ring Keyring to use
   \param cb_get_passphrase Callback to use to get passphrase
   \note It is the caller's responsibility to free result after use.
   \sa pgp_validate_result_free()
*/
unsigned 
pgp_validate_all_sigs(pgp_validation_t *result,
	    const pgp_keyring_t *ring,
	    pgp_cb_ret_t cb_get_passphrase(const pgp_packet_t *,
	    					pgp_cbdata_t *))
{
	unsigned	n;

	(void) memset(result, 0x0, sizeof(*result));
	for (n = 0; n < ring->keyc; ++n) {
		pgp_validate_key_sigs(result, &ring->keys[n], ring,
				cb_get_passphrase);
	}
	return validate_result_status(stderr, "keyring", result);
}

/**
   \ingroup HighLevel_Verify
   \brief Frees validation result and associated memory
   \param result Struct to be freed
   \note Must be called after validation functions
*/
void 
pgp_validate_result_free(pgp_validation_t *result)
{
	if (result != NULL) {
		if (result->valid_sigs) {
			free_sig_info(result->valid_sigs);
		}
		if (result->invalid_sigs) {
			free_sig_info(result->invalid_sigs);
		}
		if (result->unknown_sigs) {
			free_sig_info(result->unknown_sigs);
		}
		free(result);
		/* result = NULL; - XXX unnecessary */
	}
}

/**
   \ingroup HighLevel_Verify
   \brief Verifies the signatures in a signed file
   \param result Where to put the result
   \param filename Name of file to be validated
   \param armoured Treat file as armoured, if set
   \param keyring Keyring to use
   \return 1 if signatures validate successfully;
   	0 if signatures fail or there are no signatures
   \note After verification, result holds the details of all keys which
   have passed, failed and not been recognised.
   \note It is the caller's responsiblity to call
   	pgp_validate_result_free(result) after use.
*/
unsigned 
pgp_validate_file(pgp_io_t *io,
			pgp_validation_t *result,
			const char *infile,
			const char *outfile,
			const int user_says_armoured,
			const pgp_keyring_t *keyring)
{
	validate_data_cb_t	 validation;
	pgp_stream_t		*parse = NULL;
	struct stat		 st;
	const char		*signame;
	const int		 printerrors = 1;
	unsigned		 ret;
	char			 f[MAXPATHLEN];
	char			*dataname;
	int			 realarmour;
	int			 outfd = 0;
	int			 infd;
	int			 cc;

	if (stat(infile, &st) < 0) {
		(void) fprintf(io->errs,
			"pgp_validate_file: can't open '%s'\n", infile);
		return 0;
	}
	realarmour = user_says_armoured;
	dataname = NULL;
	signame = NULL;
	cc = snprintf(f, sizeof(f), "%s", infile);
	if (strcmp(&f[cc - 4], ".sig") == 0) {
		/* we've been given a sigfile as infile */
		f[cc - 4] = 0x0;
		/* set dataname to name of file which was signed */
		dataname = f;
		signame = infile;
	} else if (strcmp(&f[cc - 4], ".asc") == 0) {
		/* we've been given an armored sigfile as infile */
		f[cc - 4] = 0x0;
		/* set dataname to name of file which was signed */
		dataname = f;
		signame = infile;
		realarmour = 1;
	} else {
		signame = infile;
	}
	(void) memset(&validation, 0x0, sizeof(validation));
	infd = pgp_setup_file_read(io, &parse, signame, &validation,
				validate_data_cb, 1);
	if (infd < 0) {
		return 0;
	}

	if (dataname) {
		validation.detachname = netpgp_strdup(dataname);
	}

	/* Set verification reader and handling options */
	validation.result = result;
	validation.keyring = keyring;
	validation.mem = pgp_memory_new();
	pgp_memory_init(validation.mem, 128);
	/* Note: Coverity incorrectly reports an error that validation.reader */
	/* is never used. */
	validation.reader = parse->readinfo.arg;

	if (realarmour) {
		pgp_reader_push_dearmour(parse);
	}

	/* Do the verification */
	pgp_parse(parse, !printerrors);

	/* Tidy up */
	if (realarmour) {
		pgp_reader_pop_dearmour(parse);
	}
	pgp_teardown_file_read(parse, infd);

	ret = validate_result_status(io->errs, infile, result);

	/* this is triggered only for --cat output */
	if (outfile) {
		/* need to send validated output somewhere */
		if (strcmp(outfile, "-") == 0) {
			outfd = STDOUT_FILENO;
		} else {
			outfd = open(outfile, O_WRONLY | O_CREAT, 0666);
		}
		if (outfd < 0) {
			/* even if the signature was good, we can't
			* write the file, so send back a bad return
			* code */
			ret = 0;
		} else if (validate_result_status(io->errs, infile, result)) {
			unsigned	 len;
			char		*cp;
			int		 i;

			len = (unsigned)pgp_mem_len(validation.mem);
			cp = pgp_mem_data(validation.mem);
			for (i = 0 ; i < (int)len ; i += cc) {
				cc = (int)write(outfd, &cp[i], (unsigned)(len - i));
				if (cc < 0) {
					(void) fprintf(io->errs,
						"netpgp: short write\n");
					ret = 0;
					break;
				}
			}
			if (strcmp(outfile, "-") != 0) {
				(void) close(outfd);
			}
		}
	}
	pgp_memory_free(validation.mem);
	return ret;
}

/**
   \ingroup HighLevel_Verify
   \brief Verifies the signatures in a pgp_memory_t struct
   \param result Where to put the result
   \param mem Memory to be validated
   \param user_says_armoured Treat data as armoured, if set
   \param keyring Keyring to use
   \return 1 if signature validates successfully; 0 if not
   \note After verification, result holds the details of all keys which
   have passed, failed and not been recognised.
   \note It is the caller's responsiblity to call
   	pgp_validate_result_free(result) after use.
*/

unsigned 
pgp_validate_mem(pgp_io_t *io,
			pgp_validation_t *result,
			pgp_memory_t *mem,
			pgp_memory_t **cat,
			const int user_says_armoured,
			const pgp_keyring_t *keyring)
{
	validate_data_cb_t	 validation;
	pgp_stream_t		*stream = NULL;
	const int		 printerrors = 1;
	int			 realarmour;

	pgp_setup_memory_read(io, &stream, mem, &validation, validate_data_cb, 1);
	/* Set verification reader and handling options */
	(void) memset(&validation, 0x0, sizeof(validation));
	validation.result = result;
	validation.keyring = keyring;
	validation.mem = pgp_memory_new();
	pgp_memory_init(validation.mem, 128);
	/* Note: Coverity incorrectly reports an error that validation.reader */
	/* is never used. */
	validation.reader = stream->readinfo.arg;

	if ((realarmour = user_says_armoured) != 0 ||
	    strncmp(pgp_mem_data(mem),
	    		"-----BEGIN PGP MESSAGE-----", 27) == 0) {
		realarmour = 1;
	}
	if (realarmour) {
		pgp_reader_push_dearmour(stream);
	}

	/* Do the verification */
	pgp_parse(stream, !printerrors);

	/* Tidy up */
	if (realarmour) {
		pgp_reader_pop_dearmour(stream);
	}
	pgp_teardown_memory_read(stream, mem);

	/* this is triggered only for --cat output */
	if (cat) {
		/* need to send validated output somewhere */
		*cat = validation.mem;
	} else {
		pgp_memory_free(validation.mem);
	}

	return validate_result_status(io->errs, NULL, result);
}
