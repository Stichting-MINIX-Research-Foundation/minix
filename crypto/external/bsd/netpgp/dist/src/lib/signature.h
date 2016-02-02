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

#ifndef SIGNATURE_H_
#define SIGNATURE_H_

#include <sys/types.h>

#include <inttypes.h>

#include "packet.h"
#include "create.h"
#include "memory.h"

typedef struct pgp_create_sig_t	 pgp_create_sig_t;

pgp_create_sig_t *pgp_create_sig_new(void);
void pgp_create_sig_delete(pgp_create_sig_t *);

unsigned pgp_check_useridcert_sig(const pgp_pubkey_t *,
			  const uint8_t *,
			  const pgp_sig_t *,
			  const pgp_pubkey_t *,
			  const uint8_t *);
unsigned pgp_check_userattrcert_sig(const pgp_pubkey_t *,
			  const pgp_data_t *,
			  const pgp_sig_t *,
			  const pgp_pubkey_t *,
			  const uint8_t *);
unsigned pgp_check_subkey_sig(const pgp_pubkey_t *,
			   const pgp_pubkey_t *,
			   const pgp_sig_t *,
			   const pgp_pubkey_t *,
			   const uint8_t *);
unsigned pgp_check_direct_sig(const pgp_pubkey_t *,
			   const pgp_sig_t *,
			   const pgp_pubkey_t *,
			   const uint8_t *);
unsigned pgp_check_hash_sig(pgp_hash_t *,
			 const pgp_sig_t *,
			 const pgp_pubkey_t *);
void pgp_sig_start_key_sig(pgp_create_sig_t *,
				  const pgp_pubkey_t *,
				  const uint8_t *,
				  pgp_sig_type_t);
void pgp_start_sig(pgp_create_sig_t *,
			const pgp_seckey_t *,
			const pgp_hash_alg_t,
			const pgp_sig_type_t);

void pgp_sig_add_data(pgp_create_sig_t *, const void *, size_t);
pgp_hash_t *pgp_sig_get_hash(pgp_create_sig_t *);
unsigned   pgp_end_hashed_subpkts(pgp_create_sig_t *);
unsigned pgp_write_sig(pgp_output_t *, pgp_create_sig_t *,
			const pgp_pubkey_t *, const pgp_seckey_t *);
unsigned   pgp_add_time(pgp_create_sig_t *, int64_t, const char *);
unsigned pgp_add_issuer_keyid(pgp_create_sig_t *,
			const uint8_t *);
void pgp_add_primary_userid(pgp_create_sig_t *, unsigned);

/* Standard Interface */
unsigned   pgp_sign_file(pgp_io_t *,
			const char *,
			const char *,
			const pgp_seckey_t *,
			const char *,
			const int64_t,
			const uint64_t,
			const unsigned,
			const unsigned,
			const unsigned);

int pgp_sign_detached(pgp_io_t *,
			const char *,
			char *,
			pgp_seckey_t *,
			const char *,
			const int64_t,
			const uint64_t,
			const unsigned,
			const unsigned);

/* armoured stuff */
unsigned pgp_crc24(unsigned, uint8_t);

void pgp_reader_push_dearmour(pgp_stream_t *);

void pgp_reader_pop_dearmour(pgp_stream_t *);
unsigned pgp_writer_push_clearsigned(pgp_output_t *, pgp_create_sig_t *);
void pgp_writer_push_armor_msg(pgp_output_t *);

typedef enum {
	PGP_PGP_MESSAGE = 1,
	PGP_PGP_PUBLIC_KEY_BLOCK,
	PGP_PGP_PRIVATE_KEY_BLOCK,
	PGP_PGP_MULTIPART_MESSAGE_PART_X_OF_Y,
	PGP_PGP_MULTIPART_MESSAGE_PART_X,
	PGP_PGP_SIGNATURE
} pgp_armor_type_t;

#define CRC24_INIT 0xb704ceL

unsigned pgp_writer_use_armored_sig(pgp_output_t *);

void pgp_writer_push_armoured(pgp_output_t *, pgp_armor_type_t);

pgp_memory_t   *pgp_sign_buf(pgp_io_t *,
				const void *,
				const size_t,
				const pgp_seckey_t *,
				const int64_t,
				const uint64_t,
				const char *,
				const unsigned,
				const unsigned);

unsigned pgp_keyring_read_from_mem(pgp_io_t *,
				pgp_keyring_t *,
				const unsigned,
				pgp_memory_t *);

#endif /* SIGNATURE_H_ */
