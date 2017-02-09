/* $NetBSD: if_cs_pcmcia.c,v 1.21 2015/04/13 16:33:25 riastradh Exp $ */

/*-
 * Copyright (c)2001 YAMAMOTO Takashi,
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
__KERNEL_RCSID(0, "$NetBSD: if_cs_pcmcia.c,v 1.21 2015/04/13 16:33:25 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/cs89x0reg.h>
#include <dev/ic/cs89x0var.h>

struct cs_pcmcia_softc;

static int cs_pcmcia_match(device_t, cfdata_t, void *);
static int cs_pcmcia_validate_config(struct pcmcia_config_entry *);
static void cs_pcmcia_attach(device_t, device_t, void *);
static int cs_pcmcia_detach(device_t, int);
static int cs_pcmcia_enable(struct cs_softc *);
static void cs_pcmcia_disable(struct cs_softc *);

struct cs_pcmcia_softc {
	struct cs_softc sc_cs; /* real "cs" softc */

	struct pcmcia_function *sc_pf;

	int sc_state;
#define	CS_PCMCIA_ATTACHED	3
};

CFATTACH_DECL_NEW(cs_pcmcia, sizeof(struct cs_pcmcia_softc),
    cs_pcmcia_match, cs_pcmcia_attach, cs_pcmcia_detach, cs_activate);

static int
cs_pcmcia_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_IBM &&
	    pa->product == PCMCIA_PRODUCT_IBM_ETHERJET)
		return (1);
	return (0);
}

static int
cs_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_memspace != 0 ||
	    cfe->num_iospace != 1 ||
	    cfe->iospace[0].length < CS8900_IOSIZE)
		return (EINVAL);
	return (0);
}

static void
cs_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct cs_pcmcia_softc *psc = device_private(self);
	struct cs_softc *sc = &psc->sc_cs;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_function *pf;
	int error;

	sc->sc_dev = self;
	pf = psc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, cs_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n",
		    error);
		return;
	}

	cfe = pf->cfe;
	sc->sc_iot = cfe->iospace[0].handle.iot;
	sc->sc_ioh = cfe->iospace[0].handle.ioh;
	sc->sc_irq = -1;
#define CS_PCMCIA_HACK_FOR_CARDBUS
#ifdef CS_PCMCIA_HACK_FOR_CARDBUS
	/*
	 * XXX is there a generic way to know if it's a cardbus or not?
	 */
	sc->sc_cfgflags |= CFGFLG_CARDBUS_HACK;
#endif

	error = cs_pcmcia_enable(sc);
	if (error)
		goto fail;

	sc->sc_enable = cs_pcmcia_enable;
	sc->sc_disable = cs_pcmcia_disable;

	/* chip attach */
	error = cs_attach(sc, 0, 0, 0, 0);
	if (error)
		goto fail2;

	cs_pcmcia_disable(sc);
	psc->sc_state = CS_PCMCIA_ATTACHED;
	return;

fail2:
	cs_pcmcia_disable(sc);
fail:
	pcmcia_function_unconfigure(pf);
}

static int
cs_pcmcia_detach(device_t self, int flags)
{
	struct cs_pcmcia_softc *psc = device_private(self);
	struct cs_softc *sc = &psc->sc_cs;
	int error;

	if (psc->sc_state != CS_PCMCIA_ATTACHED)
		return (0);

	error = cs_detach(sc);
	if (error)
		return (error);

	pcmcia_function_unconfigure(psc->sc_pf);

	return (0);
}

static int
cs_pcmcia_enable(struct cs_softc *sc)
{
	struct cs_pcmcia_softc *psc = (void *)sc;
	int error;

	sc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, cs_intr, sc);
	if (!sc->sc_ih)
		return (EIO);

	error = pcmcia_function_enable(psc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return (error);
}

static void
cs_pcmcia_disable(struct cs_softc *sc)
{
	struct cs_pcmcia_softc *psc = (void *)sc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);
	sc->sc_ih = 0;
}
