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

#ifndef WRITER_H_
#define WRITER_H_

#include "types.h"
#include "packet.h"
#include "crypto.h"
#include "errors.h"
#include "keyring.h"

/**
 * \ingroup Writer
 * the writer function prototype
 */

typedef struct pgp_writer_t	pgp_writer_t;
typedef unsigned pgp_writer_func_t(const uint8_t *,
	     unsigned,
	     pgp_error_t **,
	     pgp_writer_t *);
typedef unsigned 
pgp_writer_finaliser_t(pgp_error_t **, pgp_writer_t *);
typedef void    pgp_writer_destroyer_t(pgp_writer_t *);

/** Writer settings */
struct pgp_writer_t {
	pgp_writer_func_t	 *writer;	/* the writer itself */
	pgp_writer_finaliser_t *finaliser;	/* the writer's finaliser */
	pgp_writer_destroyer_t *destroyer;	/* the writer's destroyer */
	void			 *arg;	/* writer-specific argument */
	pgp_writer_t	 	 *next;	/* next writer in the stack */
	pgp_io_t		 *io;	/* IO for errors and output */
};


void *pgp_writer_get_arg(pgp_writer_t *);

void pgp_writer_set(pgp_output_t *,
	       pgp_writer_func_t *,
	       pgp_writer_finaliser_t *,
	       pgp_writer_destroyer_t *,
	       void *);
void pgp_writer_push(pgp_output_t *,
		pgp_writer_func_t *,
		pgp_writer_finaliser_t *,
		pgp_writer_destroyer_t *,
		void *);
void pgp_writer_pop(pgp_output_t *);
unsigned pgp_writer_passthrough(const uint8_t *,
		       unsigned,
		       pgp_error_t **,
		       pgp_writer_t *);

void pgp_writer_set_fd(pgp_output_t *, int);
unsigned pgp_writer_close(pgp_output_t *);

unsigned pgp_write(pgp_output_t *, const void *, unsigned);
unsigned pgp_write_length(pgp_output_t *, unsigned);
unsigned pgp_write_ptag(pgp_output_t *, pgp_content_enum);
unsigned pgp_write_scalar(pgp_output_t *, unsigned, unsigned);
unsigned pgp_write_mpi(pgp_output_t *, const BIGNUM *);

void pgp_writer_info_delete(pgp_writer_t *);
unsigned pgp_writer_info_finalise(pgp_error_t **, pgp_writer_t *);

void pgp_push_stream_enc_se_ip(pgp_output_t *, const pgp_key_t *, const char *);

#endif /* WRITER_H_ */
