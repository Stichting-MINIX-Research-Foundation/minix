/*	$NetBSD: apm_apmdevif.c,v 1.1 2009/04/03 04:17:03 uwe Exp $ */

/*
 * Copyright (c) 2009 Valeriy E. Ushakov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: apm_apmdevif.c,v 1.1 2009/04/03 04:17:03 uwe Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/selinfo.h> /* XXX: for apm_softc that is exposed here */

#include <dev/hpc/apm/apmvar.h>

static void	apm_apmdevif_attach(device_t, device_t, void *);
static int	apm_apmdevif_match(device_t, cfdata_t, void *);

CFATTACH_DECL_NEW(apm_apmdevif, sizeof(struct apm_softc),
    apm_apmdevif_match, apm_apmdevif_attach, NULL, NULL);


static int
apm_apmdevif_match(device_t parent, cfdata_t match, void *aux)
{

	return apm_match();
}

static void
apm_apmdevif_attach(device_t parent, device_t self, void *aux)
{
	struct apm_softc *sc;
	struct apmdev_attach_args *aaa = aux;

	sc = device_private(self);
	sc->sc_dev = self;

	sc->sc_detail = aaa->apm_detail;
	sc->sc_vers = aaa->apm_detail & 0xffff; /* XXX: magic */

	sc->sc_ops = aaa->accessops;
	sc->sc_cookie = aaa->accesscookie;

	apm_attach(sc);
}

int
apmprint(void *aux, const char *pnp)
{
	if (pnp)
		aprint_normal("apm at %s", pnp);

	return (UNCONF);
}
