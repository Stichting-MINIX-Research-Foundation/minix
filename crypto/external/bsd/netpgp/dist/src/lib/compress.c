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
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: compress.c,v 1.23 2012/03/05 02:20:18 christos Exp $");
#endif

#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif

#include <string.h>

#include "packet-parse.h"
#include "errors.h"
#include "netpgpdefs.h"
#include "crypto.h"
#include "memory.h"
#include "writer.h"

#define DECOMPRESS_BUFFER	1024

typedef struct {
	pgp_compression_type_t type;
	pgp_region_t   *region;
	uint8_t   	in[DECOMPRESS_BUFFER];
	uint8_t   	out[DECOMPRESS_BUFFER];
	z_stream        zstream;/* ZIP and ZLIB */
	size_t          offset;
	int             inflate_ret;
} z_decompress_t;

#ifdef HAVE_BZLIB_H
typedef struct {
	pgp_compression_type_t type;
	pgp_region_t   *region;
	char            in[DECOMPRESS_BUFFER];
	char            out[DECOMPRESS_BUFFER];
	bz_stream       bzstream;	/* BZIP2 */
	size_t          offset;
	int             inflate_ret;
} bz_decompress_t;
#endif

typedef struct {
	z_stream        stream;
	uint8_t  	*src;
	uint8_t  	*dst;
} compress_t;

/*
 * \todo remove code duplication between this and
 * bzip2_compressed_data_reader
 */
static int 
zlib_compressed_data_reader(pgp_stream_t *stream, void *dest, size_t length,
			    pgp_error_t **errors,
			    pgp_reader_t *readinfo,
			    pgp_cbdata_t *cbinfo)
{
	z_decompress_t *z = pgp_reader_get_arg(readinfo);
	size_t           len;
	size_t		 cc;
	char		*cdest = dest;

	if (z->type != PGP_C_ZIP && z->type != PGP_C_ZLIB) {
		(void) fprintf(stderr,
			"zlib_compressed_data_reader: weird type %d\n",
			z->type);
		return 0;
	}

	if (z->inflate_ret == Z_STREAM_END &&
	    z->zstream.next_out == &z->out[z->offset]) {
		return 0;
	}
	if (pgp_get_debug_level(__FILE__)) {
		(void) fprintf(stderr,
			"zlib_compressed_data_reader: length %" PRIsize "d\n",
			length);
	}
	for (cc = 0 ; cc < length ; cc += len) {
		if (&z->out[z->offset] == z->zstream.next_out) {
			int             ret;

			z->zstream.next_out = z->out;
			z->zstream.avail_out = sizeof(z->out);
			z->offset = 0;
			if (z->zstream.avail_in == 0) {
				unsigned        n = z->region->length;

				if (!z->region->indeterminate) {
					n -= z->region->readc;
					if (n > sizeof(z->in)) {
						n = sizeof(z->in);
					}
				} else {
					n = sizeof(z->in);
				}
				if (!pgp_stacked_limited_read(stream, z->in, n,
						z->region,
						errors, readinfo, cbinfo)) {
					return -1;
				}

				z->zstream.next_in = z->in;
				z->zstream.avail_in = (z->region->indeterminate) ?
					z->region->last_read : n;
			}
			ret = inflate(&z->zstream, Z_SYNC_FLUSH);
			if (ret == Z_STREAM_END) {
				if (!z->region->indeterminate &&
				    z->region->readc != z->region->length) {
					PGP_ERROR_1(cbinfo->errors,
						PGP_E_P_DECOMPRESSION_ERROR,
						"%s",
						"Compressed stream ended before packet end.");
				}
			} else if (ret != Z_OK) {
				(void) fprintf(stderr, "ret=%d\n", ret);
				PGP_ERROR_1(cbinfo->errors,
					PGP_E_P_DECOMPRESSION_ERROR, "%s",
					z->zstream.msg);
			}
			z->inflate_ret = ret;
		}
		if (z->zstream.next_out <= &z->out[z->offset]) {
			(void) fprintf(stderr, "Out of memory in buffer\n");
			return 0;
		}
		len = (size_t)(z->zstream.next_out - &z->out[z->offset]);
		if (len > length) {
			len = length;
		}
		(void) memcpy(&cdest[cc], &z->out[z->offset], len);
		z->offset += len;
	}

	return (int)length;
}

#ifdef HAVE_BZLIB_H
/* \todo remove code duplication between this and zlib_compressed_data_reader */
static int 
bzip2_compressed_data_reader(pgp_stream_t *stream, void *dest, size_t length,
			     pgp_error_t **errors,
			     pgp_reader_t *readinfo,
			     pgp_cbdata_t *cbinfo)
{
	bz_decompress_t *bz = pgp_reader_get_arg(readinfo);
	size_t		len;
	size_t		 cc;
	char		*cdest = dest;

	if (bz->type != PGP_C_BZIP2) {
		(void) fprintf(stderr, "Weird type %d\n", bz->type);
		return 0;
	}
	if (bz->inflate_ret == BZ_STREAM_END &&
	    bz->bzstream.next_out == &bz->out[bz->offset]) {
		return 0;
	}
	for (cc = 0 ; cc < length ; cc += len) {
		if (&bz->out[bz->offset] == bz->bzstream.next_out) {
			int             ret;

			bz->bzstream.next_out = (char *) bz->out;
			bz->bzstream.avail_out = sizeof(bz->out);
			bz->offset = 0;
			if (bz->bzstream.avail_in == 0) {
				unsigned        n = bz->region->length;

				if (!bz->region->indeterminate) {
					n -= bz->region->readc;
					if (n > sizeof(bz->in))
						n = sizeof(bz->in);
				} else
					n = sizeof(bz->in);

				if (!pgp_stacked_limited_read(stream,
						(uint8_t *) bz->in,
						n, bz->region,
						errors, readinfo, cbinfo))
					return -1;

				bz->bzstream.next_in = bz->in;
				bz->bzstream.avail_in =
					(bz->region->indeterminate) ?
					 bz->region->last_read : n;
			}
			ret = BZ2_bzDecompress(&bz->bzstream);
			if (ret == BZ_STREAM_END) {
				if (!bz->region->indeterminate &&
				    bz->region->readc != bz->region->length)
					PGP_ERROR_1(cbinfo->errors,
						PGP_E_P_DECOMPRESSION_ERROR,
						"%s",
						"Compressed stream ended before packet end.");
			} else if (ret != BZ_OK) {
				PGP_ERROR_1(cbinfo->errors,
					PGP_E_P_DECOMPRESSION_ERROR,
					"Invalid return %d from BZ2_bzDecompress", ret);
			}
			bz->inflate_ret = ret;
		}
		if (bz->bzstream.next_out <= &bz->out[bz->offset]) {
			(void) fprintf(stderr, "Out of bz memroy\n");
			return 0;
		}
		len = (size_t)(bz->bzstream.next_out - &bz->out[bz->offset]);
		if (len > length) {
			len = length;
		}
		(void) memcpy(&cdest[cc], &bz->out[bz->offset], len);
		bz->offset += len;
	}

	return (int)length;
}
#endif

/**
 * \ingroup Core_Compress
 *
 * \param *region 	Pointer to a region
 * \param *stream 	How to parse
 * \param type Which compression type to expect
*/

int 
pgp_decompress(pgp_region_t *region, pgp_stream_t *stream,
	       pgp_compression_type_t type)
{
	z_decompress_t z;
#ifdef HAVE_BZLIB_H
	bz_decompress_t bz;
#endif
	const int	printerrors = 1;
	int             ret;

	switch (type) {
	case PGP_C_ZIP:
	case PGP_C_ZLIB:
		(void) memset(&z, 0x0, sizeof(z));

		z.region = region;
		z.offset = 0;
		z.type = type;

		z.zstream.next_in = Z_NULL;
		z.zstream.avail_in = 0;
		z.zstream.next_out = z.out;
		z.zstream.zalloc = Z_NULL;
		z.zstream.zfree = Z_NULL;
		z.zstream.opaque = Z_NULL;

		break;

#ifdef HAVE_BZLIB_H
	case PGP_C_BZIP2:
		(void) memset(&bz, 0x0, sizeof(bz));

		bz.region = region;
		bz.offset = 0;
		bz.type = type;

		bz.bzstream.next_in = NULL;
		bz.bzstream.avail_in = 0;
		bz.bzstream.next_out = bz.out;
		bz.bzstream.bzalloc = NULL;
		bz.bzstream.bzfree = NULL;
		bz.bzstream.opaque = NULL;
#endif

		break;

	default:
		PGP_ERROR_1(&stream->errors,
			PGP_E_ALG_UNSUPPORTED_COMPRESS_ALG,
			"Compression algorithm %d is not yet supported", type);
		return 0;
	}

	switch (type) {
	case PGP_C_ZIP:
		/* LINTED */ /* this is a lint problem in zlib.h header */
		ret = (int)inflateInit2(&z.zstream, -15);
		break;

	case PGP_C_ZLIB:
		/* LINTED */ /* this is a lint problem in zlib.h header */
		ret = (int)inflateInit(&z.zstream);
		break;

#ifdef HAVE_BZLIB_H
	case PGP_C_BZIP2:
		ret = BZ2_bzDecompressInit(&bz.bzstream, 1, 0);
		break;
#endif

	default:
		PGP_ERROR_1(&stream->errors,
			PGP_E_ALG_UNSUPPORTED_COMPRESS_ALG,
			"Compression algorithm %d is not yet supported", type);
		return 0;
	}

	switch (type) {
	case PGP_C_ZIP:
	case PGP_C_ZLIB:
		if (ret != Z_OK) {
			PGP_ERROR_1(&stream->errors,
				PGP_E_P_DECOMPRESSION_ERROR,
"Cannot initialise ZIP or ZLIB stream for decompression: error=%d", ret);
			return 0;
		}
		pgp_reader_push(stream, zlib_compressed_data_reader,
					NULL, &z);
		break;

#ifdef HAVE_BZLIB_H
	case PGP_C_BZIP2:
		if (ret != BZ_OK) {
			PGP_ERROR_1(&stream->errors,
				PGP_E_P_DECOMPRESSION_ERROR,
"Cannot initialise BZIP2 stream for decompression: error=%d", ret);
			return 0;
		}
		pgp_reader_push(stream, bzip2_compressed_data_reader,
					NULL, &bz);
		break;
#endif

	default:
		PGP_ERROR_1(&stream->errors,
			PGP_E_ALG_UNSUPPORTED_COMPRESS_ALG,
			"Compression algorithm %d is not yet supported", type);
		return 0;
	}

	ret = pgp_parse(stream, !printerrors);

	pgp_reader_pop(stream);

	return ret;
}

/**
\ingroup Core_WritePackets
\brief Writes Compressed packet
\param data Data to write out
\param len Length of data
\param output Write settings
\return 1 if OK; else 0
*/

unsigned 
pgp_writez(pgp_output_t *out, const uint8_t *data, const unsigned len)
{
	compress_t	*zip;
	size_t		 sz_in;
	size_t		 sz_out;
	int              ret;
	int              r = 0;

	/* compress the data */
	const int       level = Z_DEFAULT_COMPRESSION;	/* \todo allow varying
							 * levels */

	if ((zip = calloc(1, sizeof(*zip))) == NULL) {
		(void) fprintf(stderr, "pgp_writez: bad alloc\n");
		return 0;
	}
	zip->stream.zalloc = Z_NULL;
	zip->stream.zfree = Z_NULL;
	zip->stream.opaque = NULL;

	/* all other fields set to zero by use of calloc */

	/* LINTED */ /* this is a lint problem in zlib.h header */
	if ((int)deflateInit(&zip->stream, level) != Z_OK) {
		(void) fprintf(stderr, "pgp_writez: can't initialise\n");
		return 0;
	}
	/* do necessary transformation */
	/* copy input to maintain const'ness of src */
	if (zip->src != NULL || zip->dst != NULL) {
		(void) fprintf(stderr, "pgp_writez: non-null streams\n");
		return 0;
	}

	sz_in = len * sizeof(uint8_t);
	sz_out = ((101 * sz_in) / 100) + 12;	/* from zlib webpage */
	if ((zip->src = calloc(1, sz_in)) == NULL) {
		free(zip);
		(void) fprintf(stderr, "pgp_writez: bad alloc2\n");
		return 0;
	}
	if ((zip->dst = calloc(1, sz_out)) == NULL) {
		free(zip->src);
		free(zip);
		(void) fprintf(stderr, "pgp_writez: bad alloc3\n");
		return 0;
	}
	(void) memcpy(zip->src, data, len);

	/* setup stream */
	zip->stream.next_in = zip->src;
	zip->stream.avail_in = (unsigned)sz_in;
	zip->stream.total_in = 0;

	zip->stream.next_out = zip->dst;
	zip->stream.avail_out = (unsigned)sz_out;
	zip->stream.total_out = 0;

	do {
		r = deflate(&zip->stream, Z_FINISH);
	} while (r != Z_STREAM_END);

	/* write it out */
	ret = pgp_write_ptag(out, PGP_PTAG_CT_COMPRESSED) &&
		pgp_write_length(out, (unsigned)(zip->stream.total_out + 1))&&
		pgp_write_scalar(out, PGP_C_ZLIB, 1) &&
		pgp_write(out, zip->dst, (unsigned)zip->stream.total_out);

	free(zip->src);
	free(zip->dst);
	free(zip);
	return ret;
}
