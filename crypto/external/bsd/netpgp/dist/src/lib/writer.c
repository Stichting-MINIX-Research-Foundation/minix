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
 * This file contains the base functions used by the writers.
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: writer.c,v 1.33 2012/03/05 02:20:18 christos Exp $");
#endif

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_OPENSSL_CAST_H
#include <openssl/cast.h>
#endif

#include "create.h"
#include "writer.h"
#include "keyring.h"
#include "signature.h"
#include "packet.h"
#include "packet-parse.h"
#include "readerwriter.h"
#include "memory.h"
#include "netpgpdefs.h"
#include "version.h"
#include "netpgpdigest.h"


/*
 * return 1 if OK, otherwise 0
 */
static unsigned 
base_write(pgp_output_t *out, const void *src, unsigned len)
{
	return out->writer.writer(src, len, &out->errors, &out->writer);
}

/**
 * \ingroup Core_WritePackets
 *
 * \param src
 * \param len
 * \param output
 * \return 1 if OK, otherwise 0
 */

unsigned 
pgp_write(pgp_output_t *output, const void *src, unsigned len)
{
	return base_write(output, src, len);
}

/**
 * \ingroup Core_WritePackets
 * \param n
 * \param len
 * \param output
 * \return 1 if OK, otherwise 0
 */

unsigned 
pgp_write_scalar(pgp_output_t *output, unsigned n, unsigned len)
{
	uint8_t   c;

	while (len-- > 0) {
		c = n >> (len * 8);
		if (!base_write(output, &c, 1)) {
			return 0;
		}
	}
	return 1;
}

/**
 * \ingroup Core_WritePackets
 * \param bn
 * \param output
 * \return 1 if OK, otherwise 0
 */

unsigned 
pgp_write_mpi(pgp_output_t *output, const BIGNUM *bn)
{
	unsigned	bits;
	uint8_t		buf[NETPGP_BUFSIZ];

	bits = (unsigned)BN_num_bits(bn);
	if (bits > 65535) {
		(void) fprintf(stderr, "pgp_write_mpi: too large %u\n", bits);
		return 0;
	}
	BN_bn2bin(bn, buf);
	return pgp_write_scalar(output, bits, 2) &&
		pgp_write(output, buf, (bits + 7) / 8);
}

/**
 * \ingroup Core_WritePackets
 * \param tag
 * \param output
 * \return 1 if OK, otherwise 0
 */

unsigned 
pgp_write_ptag(pgp_output_t *output, pgp_content_enum tag)
{
	uint8_t   c;

	c = tag | PGP_PTAG_ALWAYS_SET | PGP_PTAG_NEW_FORMAT;
	return base_write(output, &c, 1);
}

/**
 * \ingroup Core_WritePackets
 * \param len
 * \param output
 * \return 1 if OK, otherwise 0
 */

unsigned 
pgp_write_length(pgp_output_t *output, unsigned len)
{
	uint8_t   c[2];

	if (len < 192) {
		c[0] = len;
		return base_write(output, c, 1);
	}
	if (len < 8192 + 192) {
		c[0] = ((len - 192) >> 8) + 192;
		c[1] = (len - 192) % 256;
		return base_write(output, c, 2);
	}
	return pgp_write_scalar(output, 0xff, 1) &&
		pgp_write_scalar(output, len, 4);
}

/*
 * Note that we finalise from the top down, so we don't use writers below
 * that have already been finalised
 */
unsigned 
pgp_writer_info_finalise(pgp_error_t **errors, pgp_writer_t *writer)
{
	unsigned   ret = 1;

	if (writer->finaliser) {
		ret = writer->finaliser(errors, writer);
		writer->finaliser = NULL;
	}
	if (writer->next && !pgp_writer_info_finalise(errors, writer->next)) {
		writer->finaliser = NULL;
		return 0;
	}
	return ret;
}

void 
pgp_writer_info_delete(pgp_writer_t *writer)
{
	/* we should have finalised before deleting */
	if (writer->finaliser) {
		(void) fprintf(stderr, "pgp_writer_info_delete: not done\n");
		return;
	}
	if (writer->next) {
		pgp_writer_info_delete(writer->next);
		free(writer->next);
		writer->next = NULL;
	}
	if (writer->destroyer) {
		writer->destroyer(writer);
		writer->destroyer = NULL;
	}
	writer->writer = NULL;
}

/**
 * \ingroup Core_Writers
 *
 * Set a writer in output. There should not be another writer set.
 *
 * \param output The output structure
 * \param writer
 * \param finaliser
 * \param destroyer
 * \param arg The argument for the writer and destroyer
 */
void 
pgp_writer_set(pgp_output_t *output,
	       pgp_writer_func_t *writer,
	       pgp_writer_finaliser_t *finaliser,
	       pgp_writer_destroyer_t *destroyer,
	       void *arg)
{
	if (output->writer.writer) {
		(void) fprintf(stderr, "pgp_writer_set: already set\n");
	} else {
		output->writer.writer = writer;
		output->writer.finaliser = finaliser;
		output->writer.destroyer = destroyer;
		output->writer.arg = arg;
	}
}

/**
 * \ingroup Core_Writers
 *
 * Push a writer in output. There must already be another writer set.
 *
 * \param output The output structure
 * \param writer
 * \param finaliser
 * \param destroyer
 * \param arg The argument for the writer and destroyer
 */
void 
pgp_writer_push(pgp_output_t *output,
		pgp_writer_func_t *writer,
		pgp_writer_finaliser_t *finaliser,
		pgp_writer_destroyer_t *destroyer,
		void *arg)
{
	pgp_writer_t *copy;

	if ((copy = calloc(1, sizeof(*copy))) == NULL) {
		(void) fprintf(stderr, "pgp_writer_push: bad alloc\n");
	} else if (output->writer.writer == NULL) {
		(void) fprintf(stderr, "pgp_writer_push: no orig writer\n");
	} else {
		*copy = output->writer;
		output->writer.next = copy;

		output->writer.writer = writer;
		output->writer.finaliser = finaliser;
		output->writer.destroyer = destroyer;
		output->writer.arg = arg;
	}
}

void 
pgp_writer_pop(pgp_output_t *output)
{
	pgp_writer_t *next;

	/* Make sure the finaliser has been called. */
	if (output->writer.finaliser) {
		(void) fprintf(stderr,
			"pgp_writer_pop: finaliser not called\n");
	} else if (output->writer.next == NULL) {
		(void) fprintf(stderr,
			"pgp_writer_pop: not a stacked writer\n");
	} else {
		if (output->writer.destroyer) {
			output->writer.destroyer(&output->writer);
		}
		next = output->writer.next;
		output->writer = *next;
		free(next);
	}
}

/**
 * \ingroup Core_Writers
 *
 * Close the writer currently set in output.
 *
 * \param output The output structure
 */
unsigned 
pgp_writer_close(pgp_output_t *output)
{
	unsigned   ret;

	ret = pgp_writer_info_finalise(&output->errors, &output->writer);
	pgp_writer_info_delete(&output->writer);
	return ret;
}

/**
 * \ingroup Core_Writers
 *
 * Get the arg supplied to pgp_createinfo_set_writer().
 *
 * \param writer The writer_info structure
 * \return The arg
 */
void           *
pgp_writer_get_arg(pgp_writer_t *writer)
{
	return writer->arg;
}

/**
 * \ingroup Core_Writers
 *
 * Write to the next writer down in the stack.
 *
 * \param src The data to write.
 * \param len The length of src.
 * \param errors A place to store errors.
 * \param writer The writer_info structure.
 * \return Success - if 0, then errors should contain the error.
 */
static unsigned 
stacked_write(pgp_writer_t *writer, const void *src, unsigned len,
		  pgp_error_t ** errors)
{
	return writer->next->writer(src, len, errors, writer->next);
}

/**
 * \ingroup Core_Writers
 *
 * Free the arg. Many writers just have a calloc()ed lump of storage, this
 * function releases it.
 *
 * \param writer the info structure.
 */
static void 
generic_destroyer(pgp_writer_t *writer)
{
	free(pgp_writer_get_arg(writer));
}

/**
 * \ingroup Core_Writers
 *
 * A writer that just writes to the next one down. Useful for when you
 * want to insert just a finaliser into the stack.
 */
unsigned 
pgp_writer_passthrough(const uint8_t *src,
		       unsigned len,
		       pgp_error_t **errors,
		       pgp_writer_t *writer)
{
	return stacked_write(writer, src, len, errors);
}

/**************************************************************************/

/**
 * \struct dashesc_t
 */
typedef struct {
	unsigned   		 seen_nl:1;
	unsigned		 seen_cr:1;
	pgp_create_sig_t	*sig;
	pgp_memory_t		*trailing;
} dashesc_t;

static unsigned 
dash_esc_writer(const uint8_t *src,
		    unsigned len,
		    pgp_error_t **errors,
		    pgp_writer_t *writer)
{
	dashesc_t	*dash = pgp_writer_get_arg(writer);
	unsigned        n;

	if (pgp_get_debug_level(__FILE__)) {
		unsigned    i = 0;

		(void) fprintf(stderr, "dash_esc_writer writing %u:\n", len);
		for (i = 0; i < len; i++) {
			fprintf(stderr, "0x%02x ", src[i]);
			if (((i + 1) % 16) == 0) {
				(void) fprintf(stderr, "\n");
			} else if (((i + 1) % 8) == 0) {
				(void) fprintf(stderr, "  ");
			}
		}
		(void) fprintf(stderr, "\n");
	}
	/* XXX: make this efficient */
	for (n = 0; n < len; ++n) {
		unsigned        l;

		if (dash->seen_nl) {
			if (src[n] == '-' &&
			    !stacked_write(writer, "- ", 2, errors)) {
				return 0;
			}
			dash->seen_nl = 0;
		}
		dash->seen_nl = src[n] == '\n';

		if (dash->seen_nl && !dash->seen_cr) {
			if (!stacked_write(writer, "\r", 1, errors)) {
				return 0;
			}
			pgp_sig_add_data(dash->sig, "\r", 1);
		}
		dash->seen_cr = src[n] == '\r';

		if (!stacked_write(writer, &src[n], 1, errors)) {
			return 0;
		}

		/* trailing whitespace isn't included in the signature */
		if (src[n] == ' ' || src[n] == '\t') {
			pgp_memory_add(dash->trailing, &src[n], 1);
		} else {
			if ((l = (unsigned)pgp_mem_len(dash->trailing)) != 0) {
				if (!dash->seen_nl && !dash->seen_cr) {
					pgp_sig_add_data(dash->sig,
					pgp_mem_data(dash->trailing), l);
				}
				pgp_memory_clear(dash->trailing);
			}
			pgp_sig_add_data(dash->sig, &src[n], 1);
		}
	}
	return 1;
}

/**
 * \param writer
 */
static void 
dash_escaped_destroyer(pgp_writer_t *writer)
{
	dashesc_t	*dash;

	dash = pgp_writer_get_arg(writer);
	pgp_memory_free(dash->trailing);
	free(dash);
}

/**
 * \ingroup Core_WritersNext
 * \brief Push Clearsigned Writer onto stack
 * \param output
 * \param sig
 */
unsigned 
pgp_writer_push_clearsigned(pgp_output_t *output, pgp_create_sig_t *sig)
{
	static const char     header[] =
		"-----BEGIN PGP SIGNED MESSAGE-----\r\nHash: ";
	const char     *hash;
	dashesc_t      *dash;
	unsigned	ret;

	hash = pgp_text_from_hash(pgp_sig_get_hash(sig));
	if ((dash = calloc(1, sizeof(*dash))) == NULL) {
		PGP_ERROR_1(&output->errors, PGP_E_W, "%s", "Bad alloc");
		return 0;
	}
	ret = (pgp_write(output, header, (unsigned)(sizeof(header) - 1)) &&
		pgp_write(output, hash, (unsigned)strlen(hash)) &&
		pgp_write(output, "\r\n\r\n", 4));

	if (ret == 0) {
		PGP_ERROR_1(&output->errors, PGP_E_W, "%s",
			"Error pushing clearsigned header");
		free(dash);
		return ret;
	}
	dash->seen_nl = 1;
	dash->sig = sig;
	dash->trailing = pgp_memory_new();
	pgp_writer_push(output, dash_esc_writer, NULL,
			dash_escaped_destroyer, dash);
	return ret;
}


/**
 * \struct base64_t
 */
typedef struct {
	unsigned	pos;
	uint8_t		t;
	unsigned	checksum;
} base64_t;

static const char     b64map[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned 
base64_writer(const uint8_t *src,
	      unsigned len,
	      pgp_error_t **errors,
	      pgp_writer_t *writer)
{
	base64_t	*base64;
	unsigned         n;

	base64 = pgp_writer_get_arg(writer);
	for (n = 0; n < len;) {
		base64->checksum = pgp_crc24(base64->checksum, src[n]);
		if (base64->pos == 0) {
			/* XXXXXX00 00000000 00000000 */
			if (!stacked_write(writer,
					&b64map[(unsigned)src[n] >> 2],
					1, errors)) {
				return 0;
			}

			/* 000000XX xxxx0000 00000000 */
			base64->t = (src[n++] & 3) << 4;
			base64->pos = 1;
		} else if (base64->pos == 1) {
			/* 000000xx XXXX0000 00000000 */
			base64->t += (unsigned)src[n] >> 4;
			if (!stacked_write(writer, &b64map[base64->t], 1,
					errors)) {
				return 0;
			}

			/* 00000000 0000XXXX xx000000 */
			base64->t = (src[n++] & 0xf) << 2;
			base64->pos = 2;
		} else if (base64->pos == 2) {
			/* 00000000 0000xxxx XX000000 */
			base64->t += (unsigned)src[n] >> 6;
			if (!stacked_write(writer, &b64map[base64->t], 1,
					errors)) {
				return 0;
			}

			/* 00000000 00000000 00XXXXXX */
			if (!stacked_write(writer,
					&b64map[src[n++] & 0x3f], 1, errors)) {
				return 0;
			}

			base64->pos = 0;
		}
	}

	return 1;
}

static unsigned 
sig_finaliser(pgp_error_t **errors, pgp_writer_t *writer)
{
	static const char	trail[] = "\r\n-----END PGP SIGNATURE-----\r\n";
	base64_t		*base64;
	uint8_t			c[3];

	base64 = pgp_writer_get_arg(writer);
	if (base64->pos) {
		if (!stacked_write(writer, &b64map[base64->t], 1, errors)) {
			return 0;
		}
		if (base64->pos == 1 &&
		    !stacked_write(writer, "==", 2, errors)) {
			return 0;
		}
		if (base64->pos == 2 &&
		    !stacked_write(writer, "=", 1, errors)) {
			return 0;
		}
	}
	/* Ready for the checksum */
	if (!stacked_write(writer, "\r\n=", 3, errors)) {
		return 0;
	}

	base64->pos = 0;		/* get ready to write the checksum */

	c[0] = base64->checksum >> 16;
	c[1] = base64->checksum >> 8;
	c[2] = base64->checksum;
	/* push the checksum through our own writer */
	if (!base64_writer(c, 3, errors, writer)) {
		return 0;
	}

	return stacked_write(writer, trail, (unsigned)(sizeof(trail) - 1), errors);
}

/**
 * \struct linebreak_t
 */
typedef struct {
	unsigned        pos;
} linebreak_t;

#define BREAKPOS	76

static unsigned 
linebreak_writer(const uint8_t *src,
		 unsigned len,
		 pgp_error_t **errors,
		 pgp_writer_t *writer)
{
	linebreak_t	*linebreak;
	unsigned         n;

	linebreak = pgp_writer_get_arg(writer);
	for (n = 0; n < len; ++n, ++linebreak->pos) {
		if (src[n] == '\r' || src[n] == '\n') {
			linebreak->pos = 0;
		}
		if (linebreak->pos == BREAKPOS) {
			if (!stacked_write(writer, "\r\n", 2, errors)) {
				return 0;
			}
			linebreak->pos = 0;
		}
		if (!stacked_write(writer, &src[n], 1, errors)) {
			return 0;
		}
	}

	return 1;
}

/**
 * \ingroup Core_WritersNext
 * \brief Push armoured signature on stack
 * \param output
 */
unsigned 
pgp_writer_use_armored_sig(pgp_output_t *output)
{
	static const char     header[] =
			"\r\n-----BEGIN PGP SIGNATURE-----\r\nVersion: "
			NETPGP_VERSION_STRING
			"\r\n\r\n";
	linebreak_t	*linebreak;
	base64_t   	*base64;

	pgp_writer_pop(output);
	if (pgp_write(output, header, (unsigned)(sizeof(header) - 1)) == 0) {
		PGP_ERROR_1(&output->errors, PGP_E_W, "%s",
			"Error switching to armoured signature");
		return 0;
	}
	if ((linebreak = calloc(1, sizeof(*linebreak))) == NULL) {
		PGP_ERROR_1(&output->errors, PGP_E_W, "%s",
			"pgp_writer_use_armored_sig: Bad alloc");
		return 0;
	}
	pgp_writer_push(output, linebreak_writer, NULL,
			generic_destroyer,
			linebreak);
	base64 = calloc(1, sizeof(*base64));
	if (!base64) {
		PGP_MEMORY_ERROR(&output->errors);
		return 0;
	}
	base64->checksum = CRC24_INIT;
	pgp_writer_push(output, base64_writer, sig_finaliser,
			generic_destroyer, base64);
	return 1;
}

static unsigned 
armoured_message_finaliser(pgp_error_t **errors, pgp_writer_t *writer)
{
	/* TODO: This is same as sig_finaliser apart from trailer. */
	static const char	 trailer[] =
			"\r\n-----END PGP MESSAGE-----\r\n";
	base64_t		*base64;
	uint8_t			 c[3];

	base64 = pgp_writer_get_arg(writer);
	if (base64->pos) {
		if (!stacked_write(writer, &b64map[base64->t], 1, errors)) {
			return 0;
		}
		if (base64->pos == 1 &&
		    !stacked_write(writer, "==", 2, errors)) {
			return 0;
		}
		if (base64->pos == 2 &&
		    !stacked_write(writer, "=", 1, errors)) {
			return 0;
		}
	}
	/* Ready for the checksum */
	if (!stacked_write(writer, "\r\n=", 3, errors)) {
		return 0;
	}

	base64->pos = 0;		/* get ready to write the checksum */

	c[0] = base64->checksum >> 16;
	c[1] = base64->checksum >> 8;
	c[2] = base64->checksum;
	/* push the checksum through our own writer */
	if (!base64_writer(c, 3, errors, writer)) {
		return 0;
	}

	return stacked_write(writer, trailer, (unsigned)strlen(trailer), errors);
}

/**
 \ingroup Core_WritersNext
 \brief Write a PGP MESSAGE
 \todo replace with generic function
*/
void 
pgp_writer_push_armor_msg(pgp_output_t *output)
{
	static const char	 header[] = "-----BEGIN PGP MESSAGE-----\r\n";
	linebreak_t		*linebreak;
	base64_t		*base64;

	pgp_write(output, header, (unsigned)(sizeof(header) - 1));
	pgp_write(output, "\r\n", 2);
	if ((linebreak = calloc(1, sizeof(*linebreak))) == NULL) {
		(void) fprintf(stderr,
			"pgp_writer_push_armor_msg: bad lb alloc\n");
		return;
	}
	pgp_writer_push(output, linebreak_writer, NULL,
		generic_destroyer,
		linebreak);
	if ((base64 = calloc(1, sizeof(*base64))) == NULL) {
		(void) fprintf(stderr,
			"pgp_writer_push_armor_msg: bad alloc\n");
		return;
	}
	base64->checksum = CRC24_INIT;
	pgp_writer_push(output, base64_writer,
		armoured_message_finaliser, generic_destroyer,
		base64);
}

static unsigned 
armoured_finaliser(pgp_armor_type_t type,
			pgp_error_t **errors,
			pgp_writer_t *writer)
{
	static const char     tail_pubkey[] =
			"\r\n-----END PGP PUBLIC KEY BLOCK-----\r\n";
	static const char     tail_private_key[] =
			"\r\n-----END PGP PRIVATE KEY BLOCK-----\r\n";
	const char		*tail = NULL;
	unsigned		 tailsize = 0;
	base64_t		*base64;
	uint8_t		 	 c[3];

	switch (type) {
	case PGP_PGP_PUBLIC_KEY_BLOCK:
		tail = tail_pubkey;
		tailsize = sizeof(tail_pubkey) - 1;
		break;

	case PGP_PGP_PRIVATE_KEY_BLOCK:
		tail = tail_private_key;
		tailsize = sizeof(tail_private_key) - 1;
		break;

	default:
		(void) fprintf(stderr, "armoured_finaliser: unusual type\n");
		return 0;
	}
	base64 = pgp_writer_get_arg(writer);
	if (base64->pos) {
		if (!stacked_write(writer, &b64map[base64->t], 1,
					errors)) {
			return 0;
		}
		if (base64->pos == 1 && !stacked_write(writer, "==", 2,
				errors)) {
			return 0;
		}
		if (base64->pos == 2 && !stacked_write(writer, "=", 1,
				errors)) {
			return 0;
		}
	}
	/* Ready for the checksum */
	if (!stacked_write(writer, "\r\n=", 3, errors)) {
		return 0;
	}
	base64->pos = 0;		/* get ready to write the checksum */
	c[0] = base64->checksum >> 16;
	c[1] = base64->checksum >> 8;
	c[2] = base64->checksum;
	/* push the checksum through our own writer */
	if (!base64_writer(c, 3, errors, writer)) {
		return 0;
	}
	return stacked_write(writer, tail, tailsize, errors);
}

static unsigned 
armored_pubkey_fini(pgp_error_t **errors, pgp_writer_t *writer)
{
	return armoured_finaliser(PGP_PGP_PUBLIC_KEY_BLOCK, errors, writer);
}

static unsigned 
armored_privkey_fini(pgp_error_t **errors, pgp_writer_t *writer)
{
	return armoured_finaliser(PGP_PGP_PRIVATE_KEY_BLOCK, errors, writer);
}

/* \todo use this for other armoured types */
/**
 \ingroup Core_WritersNext
 \brief Push Armoured Writer on stack (generic)
*/
void 
pgp_writer_push_armoured(pgp_output_t *output, pgp_armor_type_t type)
{
	static char     hdr_pubkey[] =
			"-----BEGIN PGP PUBLIC KEY BLOCK-----\r\nVersion: "
			NETPGP_VERSION_STRING
			"\r\n\r\n";
	static char     hdr_private_key[] =
			"-----BEGIN PGP PRIVATE KEY BLOCK-----\r\nVersion: "
			NETPGP_VERSION_STRING
			"\r\n\r\n";
	unsigned    	 hdrsize = 0;
	unsigned	(*finaliser) (pgp_error_t **, pgp_writer_t *);
	base64_t	*base64;
	linebreak_t	*linebreak;
	char           *header = NULL;

	finaliser = NULL;
	switch (type) {
	case PGP_PGP_PUBLIC_KEY_BLOCK:
		header = hdr_pubkey;
		hdrsize = sizeof(hdr_pubkey) - 1;
		finaliser = armored_pubkey_fini;
		break;

	case PGP_PGP_PRIVATE_KEY_BLOCK:
		header = hdr_private_key;
		hdrsize = sizeof(hdr_private_key) - 1;
		finaliser = armored_privkey_fini;
		break;

	default:
		(void) fprintf(stderr,
			"pgp_writer_push_armoured: unusual type\n");
		return;
	}
	if ((linebreak = calloc(1, sizeof(*linebreak))) == NULL) {
		(void) fprintf(stderr,
			"pgp_writer_push_armoured: bad alloc\n");
		return;
	}
	pgp_write(output, header, hdrsize);
	pgp_writer_push(output, linebreak_writer, NULL,
			generic_destroyer,
			linebreak);
	if ((base64 = calloc(1, sizeof(*base64))) == NULL) {
		(void) fprintf(stderr,
			"pgp_writer_push_armoured: bad alloc\n");
		return;
	}
	base64->checksum = CRC24_INIT;
	pgp_writer_push(output, base64_writer, finaliser,
			generic_destroyer, base64);
}

/**************************************************************************/

typedef struct {
	pgp_crypt_t    *crypt;
	int             free_crypt;
} crypt_t;

/*
 * This writer simply takes plaintext as input,
 * encrypts it with the given key
 * and outputs the resulting encrypted text
 */
static unsigned 
encrypt_writer(const uint8_t *src,
	       unsigned len,
	       pgp_error_t **errors,
	       pgp_writer_t *writer)
{
#define BUFSZ 1024		/* arbitrary number */
	uint8_t		encbuf[BUFSZ];
	unsigned        remaining;
	unsigned        done = 0;
	crypt_t		*pgp_encrypt;

	remaining = len;
	pgp_encrypt = (crypt_t *) pgp_writer_get_arg(writer);
	if (!pgp_is_sa_supported(pgp_encrypt->crypt->alg)) {
		(void) fprintf(stderr, "encrypt_writer: not supported\n");
		return 0;
	}
	while (remaining > 0) {
		unsigned        size = (remaining < BUFSZ) ? remaining : BUFSZ;

		/* memcpy(buf,src,size); // \todo copy needed here? */
		pgp_encrypt->crypt->cfb_encrypt(pgp_encrypt->crypt, encbuf,
					src + done, size);

		if (pgp_get_debug_level(__FILE__)) {
			hexdump(stderr, "unencrypted", &src[done], 16);
			hexdump(stderr, "encrypted", encbuf, 16);
		}
		if (!stacked_write(writer, encbuf, size, errors)) {
			if (pgp_get_debug_level(__FILE__)) {
				fprintf(stderr,
					"encrypted_writer: stacked write\n");
			}
			return 0;
		}
		remaining -= size;
		done += size;
	}

	return 1;
}

static void 
encrypt_destroyer(pgp_writer_t *writer)
{
	crypt_t    *pgp_encrypt;

	pgp_encrypt = (crypt_t *) pgp_writer_get_arg(writer);
	if (pgp_encrypt->free_crypt) {
		free(pgp_encrypt->crypt);
	}
	free(pgp_encrypt);
}

/**
\ingroup Core_WritersNext
\brief Push Encrypted Writer onto stack (create SE packets)
*/
void 
pgp_push_enc_crypt(pgp_output_t *output, pgp_crypt_t *pgp_crypt)
{
	/* Create encrypt to be used with this writer */
	/* Remember to free this in the destroyer */
	crypt_t    *pgp_encrypt;

	if ((pgp_encrypt = calloc(1, sizeof(*pgp_encrypt))) == NULL) {
		(void) fprintf(stderr, "pgp_push_enc_crypt: bad alloc\n");
	} else {
		/* Setup the encrypt */
		pgp_encrypt->crypt = pgp_crypt;
		pgp_encrypt->free_crypt = 0;
		/* And push writer on stack */
		pgp_writer_push(output, encrypt_writer, NULL,
			encrypt_destroyer, pgp_encrypt);
	}
}

/**************************************************************************/

typedef struct {
	pgp_crypt_t    *crypt;
} encrypt_se_ip_t;

static unsigned	encrypt_se_ip_writer(const uint8_t *,
		     unsigned,
		     pgp_error_t **,
		     pgp_writer_t *);
static void     encrypt_se_ip_destroyer(pgp_writer_t *);

/* */

/**
\ingroup Core_WritersNext
\brief Push Encrypted SE IP Writer onto stack
*/
int 
pgp_push_enc_se_ip(pgp_output_t *output, const pgp_key_t *pubkey, const char *cipher)
{
	pgp_pk_sesskey_t *encrypted_pk_sesskey;
	encrypt_se_ip_t *se_ip;
	pgp_crypt_t	*encrypted;
	uint8_t		*iv;

	if ((se_ip = calloc(1, sizeof(*se_ip))) == NULL) {
		(void) fprintf(stderr, "pgp_push_enc_se_ip: bad alloc\n");
		return 0;
	}

	/* Create and write encrypted PK session key */
	if ((encrypted_pk_sesskey = pgp_create_pk_sesskey(pubkey, cipher)) == NULL) {
		(void) fprintf(stderr, "pgp_push_enc_se_ip: null pk sesskey\n");
		return 0;
	}
	pgp_write_pk_sesskey(output, encrypted_pk_sesskey);

	/* Setup the se_ip */
	if ((encrypted = calloc(1, sizeof(*encrypted))) == NULL) {
		free(se_ip);
		(void) fprintf(stderr, "pgp_push_enc_se_ip: bad alloc\n");
		return 0;
	}
	pgp_crypt_any(encrypted, encrypted_pk_sesskey->symm_alg);
	if ((iv = calloc(1, encrypted->blocksize)) == NULL) {
		free(se_ip);
		free(encrypted);
		(void) fprintf(stderr, "pgp_push_enc_se_ip: bad alloc\n");
		return 0;
	}
	encrypted->set_iv(encrypted, iv);
	encrypted->set_crypt_key(encrypted, &encrypted_pk_sesskey->key[0]);
	pgp_encrypt_init(encrypted);

	se_ip->crypt = encrypted;

	/* And push writer on stack */
	pgp_writer_push(output, encrypt_se_ip_writer, NULL,
			encrypt_se_ip_destroyer, se_ip);
	/* tidy up */
	free(encrypted_pk_sesskey);
	free(iv);
	return 1;
}

static unsigned 
encrypt_se_ip_writer(const uint8_t *src,
		     unsigned len,
		     pgp_error_t **errors,
		     pgp_writer_t *writer)
{
	const unsigned	 bufsz = 128;
	encrypt_se_ip_t	*se_ip = pgp_writer_get_arg(writer);
	pgp_output_t	*litoutput;
	pgp_output_t	*zoutput;
	pgp_output_t	*output;
	pgp_memory_t	*litmem;
	pgp_memory_t	*zmem;
	pgp_memory_t	*localmem;
	unsigned	 ret = 1;

	pgp_setup_memory_write(&litoutput, &litmem, bufsz);
	pgp_setup_memory_write(&zoutput, &zmem, bufsz);
	pgp_setup_memory_write(&output, &localmem, bufsz);

	/* create literal data packet from source data */
	pgp_write_litdata(litoutput, src, (const int)len, PGP_LDT_BINARY);
	if (pgp_mem_len(litmem) <= len) {
		(void) fprintf(stderr, "encrypt_se_ip_writer: bad len\n");
		return 0;
	}

	/* create compressed packet from literal data packet */
	pgp_writez(zoutput, pgp_mem_data(litmem), (unsigned)pgp_mem_len(litmem));

	/* create SE IP packet set from this compressed literal data */
	pgp_write_se_ip_pktset(output, pgp_mem_data(zmem),
			       (unsigned)pgp_mem_len(zmem),
			       se_ip->crypt);
	if (pgp_mem_len(localmem) <= pgp_mem_len(zmem)) {
		(void) fprintf(stderr,
				"encrypt_se_ip_writer: bad comp len\n");
		return 0;
	}

	/* now write memory to next writer */
	ret = stacked_write(writer, pgp_mem_data(localmem),
				(unsigned)pgp_mem_len(localmem), errors);

	pgp_memory_free(localmem);
	pgp_memory_free(zmem);
	pgp_memory_free(litmem);

	return ret;
}

static void 
encrypt_se_ip_destroyer(pgp_writer_t *writer)
{
	encrypt_se_ip_t	*se_ip;

	se_ip = pgp_writer_get_arg(writer);
	free(se_ip->crypt);
	free(se_ip);
}

unsigned 
pgp_write_se_ip_pktset(pgp_output_t *output,
			const uint8_t *data,
			const unsigned len,
			pgp_crypt_t *crypted)
{
	pgp_output_t	*mdcoutput;
	pgp_memory_t	*mdc;
	uint8_t		 hashed[PGP_SHA1_HASH_SIZE];
	uint8_t		*preamble;
	const size_t	 mdcsize = 1 + 1 + PGP_SHA1_HASH_SIZE;
	size_t		 preamblesize;
	size_t		 bufsize;

	preamblesize = crypted->blocksize + 2;
	if ((preamble = calloc(1, preamblesize)) == NULL) {
		(void) fprintf(stderr, "pgp_write_se_ip_pktset: bad alloc\n");
		return 0;
	}
	bufsize = preamblesize + len + mdcsize;

	if (!pgp_write_ptag(output, PGP_PTAG_CT_SE_IP_DATA) ||
	    !pgp_write_length(output, (unsigned)(1 + bufsize)) ||
	    !pgp_write_scalar(output, PGP_SE_IP_DATA_VERSION, 1)) {
		free(preamble);
		return 0;
	}
	pgp_random(preamble, crypted->blocksize);
	preamble[crypted->blocksize] = preamble[crypted->blocksize - 2];
	preamble[crypted->blocksize + 1] = preamble[crypted->blocksize - 1];

	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "preamble", preamble, preamblesize);
	}

	/* now construct MDC packet and add to the end of the buffer */
	pgp_setup_memory_write(&mdcoutput, &mdc, mdcsize);
	pgp_calc_mdc_hash(preamble, preamblesize, data, len, hashed);
	pgp_write_mdc(mdcoutput, hashed);

	if (pgp_get_debug_level(__FILE__)) {
		hexdump(stderr, "plaintext", data, len);
		hexdump(stderr, "mdc", pgp_mem_data(mdc), PGP_SHA1_HASH_SIZE + 1 + 1);
	}

	/* and write it out */
	pgp_push_enc_crypt(output, crypted);
	if (pgp_get_debug_level(__FILE__)) {
		(void) fprintf(stderr,
			"writing %" PRIsize "u + %u + %" PRIsize "u\n",
			preamblesize, len, pgp_mem_len(mdc));
	}
	if (!pgp_write(output, preamble, (unsigned)preamblesize) ||
	    !pgp_write(output, data, len) ||
	    !pgp_write(output, pgp_mem_data(mdc), (unsigned)pgp_mem_len(mdc))) {
		/* \todo fix cleanup here and in old code functions */
		return 0;
	}

	pgp_writer_pop(output);

	/* cleanup  */
	pgp_teardown_memory_write(mdcoutput, mdc);
	free(preamble);

	return 1;
}

typedef struct {
	int             fd;
} writer_fd_t;

static unsigned 
fd_writer(const uint8_t *src, unsigned len,
	  pgp_error_t **errors,
	  pgp_writer_t *writer)
{
	writer_fd_t	*writerfd;
	int              n;

	writerfd = pgp_writer_get_arg(writer);
	n = (int)write(writerfd->fd, src, len);
	if (n == -1) {
		PGP_SYSTEM_ERROR_1(errors, PGP_E_W_WRITE_FAILED, "write",
				   "file descriptor %d", writerfd->fd);
		return 0;
	}
	if ((unsigned) n != len) {
		PGP_ERROR_1(errors, PGP_E_W_WRITE_TOO_SHORT,
			    "file descriptor %d", writerfd->fd);
		return 0;
	}
	return 1;
}

static void 
writer_fd_destroyer(pgp_writer_t *writer)
{
	free(pgp_writer_get_arg(writer));
}

/**
 * \ingroup Core_WritersFirst
 * \brief Write to a File
 *
 * Set the writer in output to be a stock writer that writes to a file
 * descriptor. If another writer has already been set, then that is
 * first destroyed.
 *
 * \param output The output structure
 * \param fd The file descriptor
 *
 */

void 
pgp_writer_set_fd(pgp_output_t *output, int fd)
{
	writer_fd_t	*writer;

	if ((writer = calloc(1, sizeof(*writer))) == NULL) {
		(void) fprintf(stderr, "pgp_writer_set_fd: bad alloc\n");
	} else {
		writer->fd = fd;
		pgp_writer_set(output, fd_writer, NULL, writer_fd_destroyer, writer);
	}
}

static unsigned 
memory_writer(const uint8_t *src,
		unsigned len,
		pgp_error_t **errors,
		pgp_writer_t *writer)
{
	pgp_memory_t   *mem;

	__PGP_USED(errors);
	mem = pgp_writer_get_arg(writer);
	pgp_memory_add(mem, src, len);
	return 1;
}

/**
 * \ingroup Core_WritersFirst
 * \brief Write to memory
 *
 * Set a memory writer.
 *
 * \param output The output structure
 * \param mem The memory structure
 * \note It is the caller's responsiblity to call pgp_memory_free(mem)
 * \sa pgp_memory_free()
 */

void 
pgp_writer_set_memory(pgp_output_t *output, pgp_memory_t *mem)
{
	pgp_writer_set(output, memory_writer, NULL, NULL, mem);
}

/**************************************************************************/

typedef struct {
	pgp_hash_alg_t	 hash_alg;
	pgp_hash_t		 hash;
	uint8_t			*hashed;
} skey_checksum_t;

static unsigned 
skey_checksum_writer(const uint8_t *src,
	const unsigned len,
	pgp_error_t **errors,
	pgp_writer_t *writer)
{
	skey_checksum_t	*sum;
	unsigned	 ret = 1;

	sum = pgp_writer_get_arg(writer);
	/* add contents to hash */
	sum->hash.add(&sum->hash, src, len);
	/* write to next stacked writer */
	ret = stacked_write(writer, src, len, errors);
	/* tidy up and return */
	return ret;
}

static unsigned
skey_checksum_finaliser(pgp_error_t **errors, pgp_writer_t *writer)
{
	skey_checksum_t *sum;

	sum = pgp_writer_get_arg(writer);
	if (errors && *errors) {
		printf("errors in skey_checksum_finaliser\n");
	}
	(*sum->hash.finish)(&sum->hash, sum->hashed);
	return 1;
}

static void 
skey_checksum_destroyer(pgp_writer_t *writer)
{
	skey_checksum_t *sum;

	sum = pgp_writer_get_arg(writer);
	free(sum);
}

/**
\ingroup Core_WritersNext
\param output
\param seckey
*/
void 
pgp_push_checksum_writer(pgp_output_t *output, pgp_seckey_t *seckey)
{
	/* XXX: push a SHA-1 checksum writer (and change s2k to 254). */
	skey_checksum_t *sum;
	unsigned	 hashsize;

	if ((sum = calloc(1, sizeof(*sum))) == NULL) {
		(void) fprintf(stderr,
			"pgp_push_checksum_writer: bad alloc\n");
	} else {
		/* configure the arg */
		/* Hardcoded SHA1 for just now */
		sum->hash_alg = PGP_HASH_SHA1;
		hashsize = pgp_hash_size(sum->hash_alg);
		if ((sum->hashed = seckey->checkhash) == NULL) {
			sum->hashed = seckey->checkhash = calloc(1, hashsize);
		}
		/* init the hash */
		pgp_hash_any(&sum->hash, sum->hash_alg);
		if (!sum->hash.init(&sum->hash)) {
			(void) fprintf(stderr,
				"pgp_push_checksum_writer: bad hash init\n");
			/* just continue and die */
			/* XXX - agc - no way to return failure */
		}
		pgp_writer_push(output, skey_checksum_writer,
			skey_checksum_finaliser, skey_checksum_destroyer, sum);
	}
}

/**************************************************************************/

#define MAX_PARTIAL_DATA_LENGTH 1073741824

typedef struct {
	pgp_crypt_t	*crypt;
	pgp_memory_t	*mem_data;
	pgp_memory_t	*litmem;
	pgp_output_t	*litoutput;
	pgp_memory_t	*se_ip_mem;
	pgp_output_t	*se_ip_out;
	pgp_hash_t	 hash;
} str_enc_se_ip_t;


static unsigned 
str_enc_se_ip_writer(const uint8_t *src,
			    unsigned len,
			    pgp_error_t **errors,
			    pgp_writer_t *writer);

static unsigned 
str_enc_se_ip_finaliser(pgp_error_t **errors,
			       pgp_writer_t * writer);

static void     str_enc_se_ip_destroyer(pgp_writer_t *writer);

/* */

/**
\ingroup Core_WritersNext
\param output
\param pubkey
*/
void 
pgp_push_stream_enc_se_ip(pgp_output_t *output, const pgp_key_t *pubkey, const char *cipher)
{
	pgp_pk_sesskey_t	*encrypted_pk_sesskey;
	str_enc_se_ip_t		*se_ip;
	const unsigned	 	 bufsz = 1024;
	pgp_crypt_t		*encrypted;
	uint8_t			*iv;

	if ((se_ip = calloc(1, sizeof(*se_ip))) == NULL) {
		(void) fprintf(stderr,
			"pgp_push_stream_enc_se_ip: bad alloc\n");
		return;
	}
	encrypted_pk_sesskey = pgp_create_pk_sesskey(pubkey, cipher);
	pgp_write_pk_sesskey(output, encrypted_pk_sesskey);

	/* Setup the se_ip */
	if ((encrypted = calloc(1, sizeof(*encrypted))) == NULL) {
		free(se_ip);
		(void) fprintf(stderr,
			"pgp_push_stream_enc_se_ip: bad alloc\n");
		return;
	}
	pgp_crypt_any(encrypted, encrypted_pk_sesskey->symm_alg);
	if ((iv = calloc(1, encrypted->blocksize)) == NULL) {
		free(encrypted);
		free(se_ip);
		(void) fprintf(stderr,
			"pgp_push_stream_enc_se_ip: bad alloc\n");
		return;
	}
	encrypted->set_iv(encrypted, iv);
	encrypted->set_crypt_key(encrypted, &encrypted_pk_sesskey->key[0]);
	pgp_encrypt_init(encrypted);

	se_ip->crypt = encrypted;

	se_ip->mem_data = pgp_memory_new();
	pgp_memory_init(se_ip->mem_data, bufsz);

	se_ip->litmem = NULL;
	se_ip->litoutput = NULL;

	pgp_setup_memory_write(&se_ip->se_ip_out, &se_ip->se_ip_mem, bufsz);

	/* And push writer on stack */
	pgp_writer_push(output,
			str_enc_se_ip_writer,
			str_enc_se_ip_finaliser,
			str_enc_se_ip_destroyer, se_ip);
	/* tidy up */
	free(encrypted_pk_sesskey);
	free(iv);
}


/* calculate the partial data length */
static unsigned 
partial_data_len(unsigned len)
{
	unsigned	mask;
	int		i;

	if (len == 0) {
		(void) fprintf(stderr, "partial_data_len: 0 len\n");
		return 0;
	}
	if (len > MAX_PARTIAL_DATA_LENGTH) {
		return MAX_PARTIAL_DATA_LENGTH;
	}
	mask = MAX_PARTIAL_DATA_LENGTH;
	for (i = 0; i <= 30; i++) {
		if (mask & len) {
			break;
		}
		mask >>= 1;
	}
	return mask;
}

static unsigned 
write_partial_len(pgp_output_t *output, unsigned len)
{
	/* len must be a power of 2 from 0 to 30 */
	uint8_t	c;
	int	i;

	for (i = 0; i <= 30; i++) {
		if ((len >> i) & 1) {
			break;
		}
	}
	c = 224 + i;
	return pgp_write(output, &c, 1);
}

static unsigned 
stream_write_litdata(pgp_output_t *output,
			const uint8_t *data,
			unsigned len)
{
	size_t          pdlen;

	while (len > 0) {
		pdlen = partial_data_len(len);
		write_partial_len(output, (unsigned)pdlen);
		pgp_write(output, data, (unsigned)pdlen);
		data += pdlen;
		len -= (unsigned)pdlen;
	}
	return 1;
}

static unsigned
stream_write_litdata_first(pgp_output_t *output,
				const uint8_t *data,
				unsigned len,
				const pgp_litdata_enum type)
{
	/* \todo add filename  */
	/* \todo add date */
	/* \todo do we need to check text data for <cr><lf> line endings ? */

	unsigned	sz_towrite;
	size_t		sz_pd;

	sz_towrite = 1 + 1 + 4 + len;
	sz_pd = (size_t)partial_data_len(sz_towrite);
	if (sz_pd < 512) {
		(void) fprintf(stderr,
			"stream_write_litdata_first: bad sz_pd\n");
		return 0;
	}
	pgp_write_ptag(output, PGP_PTAG_CT_LITDATA);
	write_partial_len(output, (unsigned)sz_pd);
	pgp_write_scalar(output, (unsigned)type, 1);
	pgp_write_scalar(output, 0, 1);
	pgp_write_scalar(output, 0, 4);
	pgp_write(output, data, (unsigned)(sz_pd - 6));

	data += (sz_pd - 6);
	sz_towrite -= (unsigned)sz_pd;

	return stream_write_litdata(output, data, (unsigned)sz_towrite);
}

static unsigned
stream_write_litdata_last(pgp_output_t *output,
				const uint8_t *data,
				unsigned len)
{
	pgp_write_length(output, len);
	return pgp_write(output, data, len);
}

static unsigned
stream_write_se_ip(pgp_output_t *output,
			const uint8_t *data,
			unsigned len,
			str_enc_se_ip_t *se_ip)
{
	size_t          pdlen;

	while (len > 0) {
		pdlen = partial_data_len(len);
		write_partial_len(output, (unsigned)pdlen);

		pgp_push_enc_crypt(output, se_ip->crypt);
		pgp_write(output, data, (unsigned)pdlen);
		pgp_writer_pop(output);

		se_ip->hash.add(&se_ip->hash, data, (unsigned)pdlen);

		data += pdlen;
		len -= (unsigned)pdlen;
	}
	return 1;
}

static unsigned
stream_write_se_ip_first(pgp_output_t *output,
			const uint8_t *data,
			unsigned len,
			str_enc_se_ip_t *se_ip)
{
	uint8_t	*preamble;
	size_t	blocksize;
	size_t 	preamblesize;
	size_t 	sz_towrite;
	size_t 	sz_pd;

	blocksize = se_ip->crypt->blocksize;
	preamblesize = blocksize + 2;
	sz_towrite = preamblesize + 1 + len;
	if ((preamble = calloc(1, preamblesize)) == NULL) {
		(void) fprintf(stderr,
			"stream_write_se_ip_first: bad alloc\n");
		return 0;
	}
	sz_pd = (size_t)partial_data_len((unsigned)sz_towrite);
	if (sz_pd < 512) {
		free(preamble);
		(void) fprintf(stderr,
			"stream_write_se_ip_first: bad sz_pd\n");
		return 0;
	}
	pgp_write_ptag(output, PGP_PTAG_CT_SE_IP_DATA);
	write_partial_len(output, (unsigned)sz_pd);
	pgp_write_scalar(output, PGP_SE_IP_DATA_VERSION, 1);
	pgp_push_enc_crypt(output, se_ip->crypt);

	pgp_random(preamble, blocksize);
	preamble[blocksize] = preamble[blocksize - 2];
	preamble[blocksize + 1] = preamble[blocksize - 1];
	pgp_hash_any(&se_ip->hash, PGP_HASH_SHA1);
	if (!se_ip->hash.init(&se_ip->hash)) {
		free(preamble);
		(void) fprintf(stderr,
			"stream_write_se_ip_first: bad hash init\n");
		return 0;
	}
	pgp_write(output, preamble, (unsigned)preamblesize);
	se_ip->hash.add(&se_ip->hash, preamble, (unsigned)preamblesize);
	pgp_write(output, data, (unsigned)(sz_pd - preamblesize - 1));
	se_ip->hash.add(&se_ip->hash, data, (unsigned)(sz_pd - preamblesize - 1));
	data += (sz_pd - preamblesize - 1);
	sz_towrite -= sz_pd;
	pgp_writer_pop(output);
	stream_write_se_ip(output, data, (unsigned)sz_towrite, se_ip);
	free(preamble);
	return 1;
}

static unsigned
stream_write_se_ip_last(pgp_output_t *output,
			const uint8_t *data,
			unsigned len,
			str_enc_se_ip_t *se_ip)
{
	pgp_output_t	*mdcoutput;
	pgp_memory_t	*mdcmem;
	const size_t	 mdcsize = 1 + 1 + PGP_SHA1_HASH_SIZE;
	uint8_t		 c;
	uint8_t		 hashed[PGP_SHA1_HASH_SIZE];
	size_t		 bufsize = len + mdcsize;

	se_ip->hash.add(&se_ip->hash, data, len);

	/* MDC packet tag */
	c = MDC_PKT_TAG;
	se_ip->hash.add(&se_ip->hash, &c, 1);

	/* MDC packet len */
	c = PGP_SHA1_HASH_SIZE;
	se_ip->hash.add(&se_ip->hash, &c, 1);

	/* finish */
	se_ip->hash.finish(&se_ip->hash, hashed);

	pgp_setup_memory_write(&mdcoutput, &mdcmem, mdcsize);
	pgp_write_mdc(mdcoutput, hashed);

	/* write length of last se_ip chunk */
	pgp_write_length(output, (unsigned)bufsize);

	/* encode everting */
	pgp_push_enc_crypt(output, se_ip->crypt);

	pgp_write(output, data, len);
	pgp_write(output, pgp_mem_data(mdcmem), (unsigned)pgp_mem_len(mdcmem));

	pgp_writer_pop(output);

	pgp_teardown_memory_write(mdcoutput, mdcmem);

	return 1;
}

static unsigned 
str_enc_se_ip_writer(const uint8_t *src,
			    unsigned len,
			    pgp_error_t **errors,
			    pgp_writer_t *writer)
{
	str_enc_se_ip_t	*se_ip;
	unsigned	 ret;
	size_t           datalength;

	se_ip = pgp_writer_get_arg(writer);
	ret = 1;
	if (se_ip->litoutput == NULL) {
		/* first literal data chunk is not yet written */

		pgp_memory_add(se_ip->mem_data, src, len);
		datalength = pgp_mem_len(se_ip->mem_data);

		/* 4.2.2.4. Partial Body Lengths */
		/* The first partial length MUST be at least 512 octets long. */
		if (datalength < 512) {
			return 1;	/* will wait for more data or
						 * end of stream             */
		}
		pgp_setup_memory_write(&se_ip->litoutput,
				&se_ip->litmem, datalength + 32);
		stream_write_litdata_first(se_ip->litoutput,
				pgp_mem_data(se_ip->mem_data),
				(unsigned)datalength,
				PGP_LDT_BINARY);

		stream_write_se_ip_first(se_ip->se_ip_out,
				pgp_mem_data(se_ip->litmem),
				(unsigned)pgp_mem_len(se_ip->litmem), se_ip);
	} else {
		stream_write_litdata(se_ip->litoutput, src, len);
		stream_write_se_ip(se_ip->se_ip_out,
				pgp_mem_data(se_ip->litmem),
				(unsigned)pgp_mem_len(se_ip->litmem), se_ip);
	}

	/* now write memory to next writer */
	ret = stacked_write(writer, pgp_mem_data(se_ip->se_ip_mem),
				(unsigned)pgp_mem_len(se_ip->se_ip_mem), errors);

	pgp_memory_clear(se_ip->litmem);
	pgp_memory_clear(se_ip->se_ip_mem);

	return ret;
}

/* write last chunk of data */
static unsigned 
str_enc_se_ip_finaliser(pgp_error_t **errors, pgp_writer_t *writer)
{
	str_enc_se_ip_t	*se_ip;

	se_ip = pgp_writer_get_arg(writer);
	if (se_ip->litoutput == NULL) {
		/* first literal data chunk was not written */
		/* so we know the total length of data, write a simple packet */

		/* create literal data packet from buffered data */
		pgp_setup_memory_write(&se_ip->litoutput, &se_ip->litmem,
				 pgp_mem_len(se_ip->mem_data) + 32);

		pgp_write_litdata(se_ip->litoutput,
			pgp_mem_data(se_ip->mem_data),
			(const int)pgp_mem_len(se_ip->mem_data),
			PGP_LDT_BINARY);

		/* create SE IP packet set from this literal data */
		pgp_write_se_ip_pktset(se_ip->se_ip_out,
				pgp_mem_data(se_ip->litmem),
				(unsigned)pgp_mem_len(se_ip->litmem),
				se_ip->crypt);

	} else {
		/* finish writing */
		stream_write_litdata_last(se_ip->litoutput, NULL, 0);
		stream_write_se_ip_last(se_ip->se_ip_out,
				pgp_mem_data(se_ip->litmem),
				(unsigned)pgp_mem_len(se_ip->litmem), se_ip);
	}

	/* now write memory to next writer */
	return stacked_write(writer, pgp_mem_data(se_ip->se_ip_mem),
				 (unsigned)pgp_mem_len(se_ip->se_ip_mem), errors);
}

static void 
str_enc_se_ip_destroyer(pgp_writer_t *writer)
{
	str_enc_se_ip_t *se_ip;

	se_ip = pgp_writer_get_arg(writer);
	pgp_memory_free(se_ip->mem_data);
	pgp_teardown_memory_write(se_ip->litoutput, se_ip->litmem);
	pgp_teardown_memory_write(se_ip->se_ip_out, se_ip->se_ip_mem);

	se_ip->crypt->decrypt_finish(se_ip->crypt);

	free(se_ip->crypt);
	free(se_ip);
}
