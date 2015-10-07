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
#ifndef MEMORY_H_
#define MEMORY_H_

#include <sys/types.h>

#include "packet.h"

/** pgp_memory_t
 */
typedef struct pgp_memory_t {
	uint8_t		*buf;
	size_t          length;
	size_t          allocated;
	unsigned	mmapped;
} pgp_memory_t;


pgp_memory_t   *pgp_memory_new(void);
void pgp_memory_free(pgp_memory_t *);
void pgp_memory_init(pgp_memory_t *, size_t);
void pgp_memory_pad(pgp_memory_t *, size_t);
void pgp_memory_add(pgp_memory_t *, const uint8_t *, size_t);
void pgp_memory_place_int(pgp_memory_t *, unsigned, unsigned, size_t);
void pgp_memory_make_packet(pgp_memory_t *, pgp_content_enum);
void pgp_memory_clear(pgp_memory_t *);
void pgp_memory_release(pgp_memory_t *);

void pgp_writer_set_memory(pgp_output_t *, pgp_memory_t *);

size_t pgp_mem_len(const pgp_memory_t *);
void *pgp_mem_data(pgp_memory_t *);
int pgp_mem_readfile(pgp_memory_t *, const char *);

void pgp_random(void *, size_t);

#endif /* MEMORY_H_ */
