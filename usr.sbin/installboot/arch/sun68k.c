/*	$NetBSD: sun68k.c,v 1.21 2008/04/28 20:24:16 martin Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: sun68k.c,v 1.21 2008/04/28 20:24:16 martin Exp $");
#endif	/* !__lint */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>

#include "installboot.h"

static int sun68k_clearboot(ib_params *);
static int sun68k_setboot(ib_params *);

struct ib_mach ib_mach_sun2 =
	{ "sun2", sun68k_setboot, sun68k_clearboot, no_editboot,
		IB_STAGE2START };

struct ib_mach ib_mach_sun3 =
	{ "sun3", sun68k_setboot, sun68k_clearboot, no_editboot,
		IB_STAGE2START };

static struct bbinfo_params bbparams = {
	SUN68K_BBINFO_MAGIC,
	SUN68K_BOOT_BLOCK_OFFSET,
	SUN68K_BOOT_BLOCK_BLOCKSIZE,
	SUN68K_BOOT_BLOCK_MAX_SIZE,
	0,
	BBINFO_BIG_ENDIAN,
};

static int
sun68k_clearboot(ib_params *params)
{

	assert(params != NULL);

	return (shared_bbinfo_clearboot(params, &bbparams, NULL));
}

static int
sun68k_setboot(ib_params *params)
{

	assert(params != NULL);

	return (shared_bbinfo_setboot(params, &bbparams, NULL));
}
