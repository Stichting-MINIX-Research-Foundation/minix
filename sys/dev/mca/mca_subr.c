/*	$NetBSD: mca_subr.c,v 1.10 2009/03/14 15:36:18 dsl Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * Copyright (c) 1996-1999 Scott D. Telford.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Scott Telford <s.telford@ed.ac.uk>.
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
 * MCA Bus subroutines
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mca_subr.c,v 1.10 2009/03/14 15:36:18 dsl Exp $");

#include "opt_mcaverbose.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>

#ifdef MCAVERBOSE
/*
 * Descriptions of of known MCA devices
 */
struct mca_knowndev {
	int		 id;		/* MCA ID */
	const char	*name;		/* text description */
};

#include <dev/mca/mcadevs_data.h>

#endif /* MCAVERBOSE */

void
mca_devinfo(int id, char *cp, size_t l)
{
#ifdef MCAVERBOSE
	const struct mca_knowndev *kdp;

	kdp = mca_knowndevs;
        for (; kdp->name != NULL && kdp->id != id; kdp++);

	if (kdp->name != NULL)
		snprintf(cp, l, "%s (0x%04x)", kdp->name, id);
	else
#endif /* MCAVERBOSE */
		snprintf(cp, l, "product 0x%04x", id);
}

/*
 * Returns true if the device should be attempted to be matched
 * even through it's disabled. Apparently, some devices were
 * designed this way.
 */
int
mca_match_disabled(int id)
{
	switch (id) {
	case MCA_PRODUCT_SKNETG:
		return (1);
	}

	return (0);
}
