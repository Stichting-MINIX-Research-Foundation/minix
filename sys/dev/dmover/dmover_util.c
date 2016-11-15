/*	$NetBSD: dmover_util.c,v 1.5 2005/12/11 12:21:20 christos Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * dmover_util.c: Utility functions for dmover-api.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dmover_util.c,v 1.5 2005/12/11 12:21:20 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/dmover/dmovervar.h>

/****************************************************************************
 * Well-known data mover function names.
 ****************************************************************************/

const char dmover_funcname_zero[] = "zero";
const char dmover_funcname_fill8[] = "fill8";
const char dmover_funcname_copy[] = "copy";
const char dmover_funcname_iscsi_crc32c[] = "iscsi-crc32c";
const char dmover_funcname_xor2[] = "xor2";
const char dmover_funcname_xor3[] = "xor3";
const char dmover_funcname_xor4[] = "xor4";
const char dmover_funcname_xor5[] = "xor5";
const char dmover_funcname_xor6[] = "xor6";
const char dmover_funcname_xor7[] = "xor7";
const char dmover_funcname_xor8[] = "xor8";

/****************************************************************************
 * Utility functions.
 ****************************************************************************/

/*
 * dmover_algdesc_lookup:
 *
 *	Look up the algdesc in the provided array by name.
 */
const struct dmover_algdesc *
dmover_algdesc_lookup(const struct dmover_algdesc *dad, int ndad,
    const char *name)
{

	for (; ndad != 0; ndad--, dad++) {
		if (name[0] == dad->dad_name[0] &&
		    strcmp(name, dad->dad_name) == 0)
			return (dad);
	}
	return (NULL);
}
