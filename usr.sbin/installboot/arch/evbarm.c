/*	$NetBSD: evbarm.c,v 1.1 2019/05/07 05:02:42 thorpej Exp $	*/

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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD: evbarm.c,v 1.1 2019/05/07 05:02:42 thorpej Exp $");
#endif  /* !__lint */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "installboot.h"
#include "evboards.h"

static int	evbarm_setboot(ib_params *);
static int	evbarm_clearboot(ib_params *);
static int	evbarm_editboot(ib_params *);
static void	evbarm_usage(ib_params *);

struct ib_mach ib_mach_evbarm = {
	.name		=	"evbarm",
	.setboot	=	evbarm_setboot,
	.clearboot	=	evbarm_clearboot,
	.editboot	=	evbarm_editboot,
	.usage		=	evbarm_usage,
	.valid_flags	=	IB_BOARD | IB_DTB | IB_MEDIA,
	.mach_flags	=	MF_UBOOT,
};

static int
evbarm_setboot(ib_params *params)
{
	evb_board board;
	int rv = 0;

	if (!evb_db_load(params)) {
		warnx("Unable to load board db.");
		return 0;
	}

	board = evb_db_get_board(params);
	if (board == NULL)
		goto out;

	rv = evb_uboot_setboot(params, board);

 out:
	if (params->mach_data) {
		prop_object_release(params->mach_data);
		params->mach_data = NULL;
	}
	return rv;
}

static int
evbarm_clearboot(ib_params *params)
{

	return no_clearboot(params);
}

static int
evbarm_editboot(ib_params *params)
{

	return no_editboot(params);
}

static void
evbarm_usage(ib_params *params)
{

	if (!evb_db_load(params)) {
		warnx("Unable to load board db.");
		return;
	}

	fprintf(stderr, "Known boards (for -o board=...) are:\n");
	evb_db_list_boards(params, stderr);

	if (params->mach_data) {
		prop_object_release(params->mach_data);
		params->mach_data = NULL;
	}
}
