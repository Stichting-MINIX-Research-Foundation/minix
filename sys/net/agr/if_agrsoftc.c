/*	$NetBSD: if_agrsoftc.c,v 1.4 2009/03/15 21:23:31 cegger Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_agrsoftc.c,v 1.4 2009/03/15 21:23:31 cegger Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <net/agr/if_agrvar_impl.h>

/*
 * these functions are isolated from if_agr.c to avoid inclusion of if_ether.h.
 */

struct agr_softc *
agr_alloc_softc(void)
{
	struct agr_softc *sc;
	union {
		struct ifnet u_if;
		struct ethercom u_ec;
	} *u;

	/*
	 * as we don't know our if_type yet..
	 */

	sc = malloc(sizeof(*sc) - sizeof(sc->sc_if) + sizeof(*u),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	return sc;
}

void
agr_free_softc(struct agr_softc *sc)
{

	free(sc, M_DEVBUF);
}
