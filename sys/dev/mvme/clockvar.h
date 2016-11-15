/*	$NetBSD: clockvar.h,v 1.9 2009/05/12 14:38:26 cegger Exp $	*/

/*-
 * Copyright (c) 1996, 2002 The NetBSD Foundation, Inc.
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

#ifndef _MVME_CLOCKVAR_H
#define _MVME_CLOCKVAR_H

#include <dev/clock_subr.h>

/*
 * Definitions exported to ASIC-specific clock attachment.
 */

extern	struct evcnt clock_profcnt;
extern	struct evcnt clock_statcnt;

extern	int clock_statvar;
extern	int clock_statmin;

struct clock_attach_args {
	void			(*ca_initfunc)(void *, int, int);
	void			*ca_arg;
};

void	clock_config(device_t, struct clock_attach_args *,
			struct evcnt *);

/*
 * Macro to compute a new randomized interval.  The intervals are
 * uniformly distributed on [statint - statvar / 2, statint + statvar / 2],
 * and therefore have mean statint, giving a stathz frequency clock.
 *
 * This is gratuitously stolen from sparc/sparc/clock.c
 */
#define CLOCK_NEWINT(statvar, statmin)	({				\
		u_long r, var = (statvar);				\
		do { r = random() & (var - 1); } while (r == 0);	\
		(statmin + r);						\
	})

/*
 * Sun chose the year `68' as their base count, so that
 * cl_year==0 means 1968.
 */
#define YEAR0   1968

/*
 * interrupt level for clock
 */
#define CLOCK_LEVEL 5

#endif /* _MVME_CLOCKVAR_H */
