/*	$NetBSD: evboards.h,v 1.2 2019/09/19 01:25:29 thorpej Exp $	*/

/*-
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef installboot_evboards_h_included
#define	installboot_evboards_h_included

#include <stdio.h>
#include <prop/proplib.h>

typedef prop_dictionary_t      evb_board;
typedef prop_array_t           evb_ubinstall;
typedef prop_object_iterator_t evb_ubsteps;
typedef prop_dictionary_t      evb_ubstep;

bool		evb_db_load(ib_params *);
evb_board	evb_db_get_board(ib_params *);
void		evb_db_list_boards(ib_params *, FILE *);

const char *	evb_board_get_description(ib_params *, evb_board);
const char *	evb_board_get_uboot_pkg(ib_params *, evb_board);
const char *	evb_board_get_uboot_path(ib_params *, evb_board);
evb_ubinstall	evb_board_get_uboot_install(ib_params *, evb_board);
prop_array_t	evb_board_copy_uboot_media(ib_params *, evb_board);

evb_ubsteps	evb_ubinstall_get_steps(ib_params *, evb_ubinstall);

evb_ubstep	evb_ubsteps_next_step(ib_params *, evb_ubsteps);

const char *	evb_ubstep_get_file_name(ib_params *, evb_ubstep);
uint64_t	evb_ubstep_get_file_offset(ib_params *, evb_ubstep);
uint64_t	evb_ubstep_get_file_size(ib_params *, evb_ubstep);
uint64_t	evb_ubstep_get_image_offset(ib_params *, evb_ubstep);
uint64_t	evb_ubstep_get_input_block_size(ib_params *, evb_ubstep);
uint64_t	evb_ubstep_get_input_pad_size(ib_params *, evb_ubstep);
uint64_t	evb_ubstep_get_output_size(ib_params *, evb_ubstep);
uint64_t	evb_ubstep_get_output_block_size(ib_params *, evb_ubstep);
bool		evb_ubstep_preserves_partial_block(ib_params *, evb_ubstep);

int		evb_uboot_setboot(ib_params *, evb_board);

#endif /* installboot_evboards_h_included */
