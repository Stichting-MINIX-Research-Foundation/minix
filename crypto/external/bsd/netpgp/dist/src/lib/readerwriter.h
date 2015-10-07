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

#ifndef READERWRITER_H_
#define READERWRITER_H_

#include "create.h"

#include "memory.h"

/* if this is defined, we'll use mmap in preference to file ops */
#define USE_MMAP_FOR_FILES      1

void pgp_reader_set_fd(pgp_stream_t *, int);
void pgp_reader_set_mmap(pgp_stream_t *, int);
void pgp_reader_set_memory(pgp_stream_t *, const void *, size_t);

/* Do a sum mod 65536 of all bytes read (as needed for secret keys) */
void pgp_reader_push_sum16(pgp_stream_t *);
uint16_t pgp_reader_pop_sum16(pgp_stream_t *);

void pgp_reader_push_se_ip_data(pgp_stream_t *, pgp_crypt_t *,
				pgp_region_t *);
void pgp_reader_pop_se_ip_data(pgp_stream_t *);

/* */
unsigned pgp_write_mdc(pgp_output_t *, const uint8_t *);
unsigned pgp_write_se_ip_pktset(pgp_output_t *, const uint8_t *,
		       const unsigned,
		       pgp_crypt_t *);
void pgp_push_enc_crypt(pgp_output_t *, pgp_crypt_t *);
int pgp_push_enc_se_ip(pgp_output_t *, const pgp_key_t *, const char *);

/* Secret Key checksum */
void pgp_push_checksum_writer(pgp_output_t *, pgp_seckey_t *);
unsigned pgp_pop_skey_checksum_writer(pgp_output_t *);


/* memory writing */
void pgp_setup_memory_write(pgp_output_t **, pgp_memory_t **, size_t);
void pgp_teardown_memory_write(pgp_output_t *, pgp_memory_t *);

/* memory reading */
void pgp_setup_memory_read(pgp_io_t *,
				pgp_stream_t **,
				pgp_memory_t *,
				void *,
				pgp_cb_ret_t callback(const pgp_packet_t *,
					pgp_cbdata_t *),
				unsigned);
void pgp_teardown_memory_read(pgp_stream_t *, pgp_memory_t *);

/* file writing */
int pgp_setup_file_write(pgp_output_t **, const char *, unsigned);
void pgp_teardown_file_write(pgp_output_t *, int);

/* file appending */
int pgp_setup_file_append(pgp_output_t **, const char *);
void pgp_teardown_file_append(pgp_output_t *, int);

/* file reading */
int pgp_setup_file_read(pgp_io_t *,
			pgp_stream_t **,
			const char *,
			void *,
			pgp_cb_ret_t callback(const pgp_packet_t *,
		    			pgp_cbdata_t *),
			unsigned);
void pgp_teardown_file_read(pgp_stream_t *, int);

unsigned pgp_reader_set_accumulate(pgp_stream_t *, unsigned);

/* useful callbacks */
pgp_cb_ret_t pgp_litdata_cb(const pgp_packet_t *, pgp_cbdata_t *);
pgp_cb_ret_t pgp_pk_sesskey_cb(const pgp_packet_t *, pgp_cbdata_t *);
pgp_cb_ret_t pgp_get_seckey_cb(const pgp_packet_t *, pgp_cbdata_t *);

int pgp_getpassphrase(void *, char *, size_t);

#endif /* READERWRITER_H_ */
