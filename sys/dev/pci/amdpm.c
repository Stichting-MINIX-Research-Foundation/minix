/*	$NetBSD: amdpm.c,v 1.39 2015/04/13 16:33:25 riastradh Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Enami Tsugutomo.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: amdpm.c,v 1.39 2015/04/13 16:33:25 riastradh Exp $");

#include "opt_amdpm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/rndpool.h>
#include <sys/rndsource.h>
#include <sys/mutex.h>

#include <sys/bus.h>
#include <dev/ic/acpipmtimer.h>

#include <dev/i2c/i2cvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/amdpmreg.h>
#include <dev/pci/amdpmvar.h>
#include <dev/pci/amdpm_smbusreg.h>

static void	amdpm_rnd_callout(void *);
static void	amdpm_rnd_callout_locked(void *);

#ifdef AMDPM_RND_COUNTERS
#define	AMDPM_RNDCNT_INCR(ev)	(ev)->ev_count++
#endif

static int
amdpm_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AMD) {
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_AMD_PBC768_PMC:
		case PCI_PRODUCT_AMD_PBC8111_ACPI:
			return (1);
		}
	}
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NVIDIA) {
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_NVIDIA_XBOX_SMBUS:
			return (1);
		}
	}

	return (0);
}

static void
amdpm_rnd_get(size_t bytes, void *priv)
{
	struct amdpm_softc *sc = priv;

	mutex_enter(&sc->sc_mutex);
	sc->sc_rnd_need = bytes;
	amdpm_rnd_callout_locked(sc);
	mutex_exit(&sc->sc_mutex);
}

static void
amdpm_attach(device_t parent, device_t self, void *aux)
{
	struct amdpm_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pcireg_t confreg, pmptrreg;
	u_int32_t pmreg;
	int i;

	pci_aprint_devinfo(pa, NULL);

	sc->sc_dev = self;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NVIDIA_XBOX_SMBUS)
		sc->sc_nforce = 1;
	else
		sc->sc_nforce = 0;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_iot = pa->pa_iot;
	sc->sc_pa = pa;

#if 0
	aprint_normal_dev(self, "");
	pci_conf_print(pa->pa_pc, pa->pa_tag, NULL);
#endif

	confreg = pci_conf_read(pa->pa_pc, pa->pa_tag, AMDPM_CONFREG);
	/* enable pm i/o space for AMD-8111 and nForce */
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_PBC8111_ACPI ||
	    sc->sc_nforce)
		confreg |= AMDPM_PMIOEN;

	/* Enable random number generation for everyone */
	pci_conf_write(pa->pa_pc, pa->pa_tag, AMDPM_CONFREG,
	    confreg | AMDPM_RNGEN);
	confreg = pci_conf_read(pa->pa_pc, pa->pa_tag, AMDPM_CONFREG);

	if ((confreg & AMDPM_PMIOEN) == 0) {
		aprint_error_dev(self, "PMxx space isn't enabled\n");
		return;
	}

	if (sc->sc_nforce) {
		pmptrreg = pci_conf_read(pa->pa_pc, pa->pa_tag, NFORCE_PMPTR);
		aprint_normal_dev(self, "power management at 0x%04x\n",
		    NFORCE_PMBASE(pmptrreg));
		if (bus_space_map(sc->sc_iot, NFORCE_PMBASE(pmptrreg),
		    AMDPM_PMSIZE, 0, &sc->sc_ioh)) {
			aprint_error_dev(self, "failed to map PMxx space\n");
			return;
		}
	} else {
		pmptrreg = pci_conf_read(pa->pa_pc, pa->pa_tag, AMDPM_PMPTR);
		if (bus_space_map(sc->sc_iot, AMDPM_PMBASE(pmptrreg),
		    AMDPM_PMSIZE, 0, &sc->sc_ioh)) {
			aprint_error_dev(self, "failed to map PMxx space\n");
			return;
		}
	}

	/* don't attach a timecounter on nforce boards */
	if ((confreg & AMDPM_TMRRST) == 0 && (confreg & AMDPM_STOPTMR) == 0 &&
	    !sc->sc_nforce) {
		acpipmtimer_attach(self, sc->sc_iot, sc->sc_ioh,
		  AMDPM_TMR, ((confreg & AMDPM_TMR32) ? ACPIPMT_32BIT : 0));
	}

	/* XXX this mutex is IPL_VM because it can be taken by rnd_getmore() */
	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_VM);

	/* try to attach devices on the smbus */
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_PBC8111_ACPI ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_PBC768_PMC ||
	    sc->sc_nforce) {
		amdpm_smbus_attach(sc);
	}

	if (confreg & AMDPM_RNGEN) {
		/* Check to see if we can read data from the RNG. */
		(void) bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    AMDPM_RNGDATA);
		for (i = 0; i < 1000; i++) {
			pmreg = bus_space_read_4(sc->sc_iot,
			    sc->sc_ioh, AMDPM_RNGSTAT);
			if (pmreg & AMDPM_RNGDONE)
				break;
			delay(1);
		}
		if ((pmreg & AMDPM_RNGDONE) != 0) {
			aprint_normal_dev(self, ""
			    "random number generator enabled (apprx. %dms)\n",
			    i);
			callout_init(&sc->sc_rnd_ch, CALLOUT_MPSAFE);
			rndsource_setcb(&sc->sc_rnd_source,
					amdpm_rnd_get, sc);
			rnd_attach_source(&sc->sc_rnd_source,
			    device_xname(self), RND_TYPE_RNG,
			    RND_FLAG_COLLECT_VALUE|RND_FLAG_HASCB);
#ifdef AMDPM_RND_COUNTERS
			evcnt_attach_dynamic(&sc->sc_rnd_hits, EVCNT_TYPE_MISC,
			    NULL, device_xname(self), "rnd hits");
			evcnt_attach_dynamic(&sc->sc_rnd_miss, EVCNT_TYPE_MISC,
			    NULL, device_xname(self), "rnd miss");
			for (i = 0; i < 256; i++) {
				evcnt_attach_dynamic(&sc->sc_rnd_data[i],
				    EVCNT_TYPE_MISC, NULL, device_xname(self),
				    "rnd data");
			}
#endif
			sc->sc_rnd_need = RND_POOLBITS / NBBY;
			amdpm_rnd_callout(sc);
		}
	}
}

CFATTACH_DECL_NEW(amdpm, sizeof(struct amdpm_softc),
    amdpm_match, amdpm_attach, NULL, NULL);

static void
amdpm_rnd_callout_locked(void *v)
{
	struct amdpm_softc *sc = v;
	u_int32_t rngreg;
#ifdef AMDPM_RND_COUNTERS
	int i;
#endif

	if (sc->sc_rnd_need < 1) {
		callout_stop(&sc->sc_rnd_ch);
		return;
	}

	if ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, AMDPM_RNGSTAT) &
	    AMDPM_RNGDONE) != 0) {
		rngreg = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    AMDPM_RNGDATA);
		rnd_add_data(&sc->sc_rnd_source, &rngreg,
		    sizeof(rngreg), sizeof(rngreg) * NBBY);
		sc->sc_rnd_need -= sizeof(rngreg);
#ifdef AMDPM_RND_COUNTERS
		AMDPM_RNDCNT_INCR(&sc->sc_rnd_hits);
		for (i = 0; i < sizeof(rngreg); i++, rngreg >>= NBBY)
			AMDPM_RNDCNT_INCR(&sc->sc_rnd_data[rngreg & 0xff]);
#endif
	}
#ifdef AMDPM_RND_COUNTERS
	else
		AMDPM_RNDCNT_INCR(&sc->sc_rnd_miss);
#endif
	if (sc->sc_rnd_need > 0) {
		callout_reset(&sc->sc_rnd_ch, 1, amdpm_rnd_callout, sc);
	}
}

static void
amdpm_rnd_callout(void *v)
{
	struct amdpm_softc *sc = v;

	mutex_enter(&sc->sc_mutex);
	amdpm_rnd_callout_locked(v);
	mutex_exit(&sc->sc_mutex);
}
