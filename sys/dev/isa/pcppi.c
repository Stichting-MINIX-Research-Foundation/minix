/* $NetBSD: pcppi.c,v 1.44 2015/05/17 05:20:37 pgoyette Exp $ */

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pcppi.c,v 1.44 2015/05/17 05:20:37 pgoyette Exp $");

#include "attimer.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/bus.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/tty.h>

#include <dev/ic/attimervar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/pcppireg.h>
#include <dev/isa/pcppivar.h>

#include "pckbd.h"
#if NPCKBD > 0
#include <dev/pckbport/pckbdvar.h>

void	pcppi_pckbd_bell(void *, u_int, u_int, u_int, int);
#endif

int	pcppi_match(device_t, cfdata_t, void *);
void	pcppi_isa_attach(device_t, device_t, void *);
void	pcppi_childdet(device_t, device_t);
int	pcppi_rescan(device_t, const char *, const int *);

CFATTACH_DECL3_NEW(pcppi, sizeof(struct pcppi_softc),
    pcppi_match, pcppi_isa_attach, pcppi_detach, NULL, pcppi_rescan,
    pcppi_childdet, DVF_DETACH_SHUTDOWN);

static int pcppisearch(device_t, cfdata_t, const int *, void *);
static void pcppi_bell_stop(struct pcppi_softc *);
static void pcppi_bell_callout(void *);

#if NATTIMER > 0
static void pcppi_attach_speaker(device_t);
static void pcppi_detach_speaker(struct pcppi_softc *);
#endif

int
pcppi_match(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ppi_ioh;
	int have_ppi, rv;
	u_int8_t v, nv;

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* If values are hardwired to something that they can't be, punt. */
	if (ia->ia_nio < 1 ||
	    (ia->ia_io[0].ir_addr != ISA_UNKNOWN_PORT &&
	    ia->ia_io[0].ir_addr != IO_PPI))
		return (0);

	if (ia->ia_niomem > 0 &&
	    (ia->ia_iomem[0].ir_addr != ISA_UNKNOWN_IOMEM))
		return (0);

	if (ia->ia_nirq > 0 &&
	    (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ))
		return (0);

	if (ia->ia_ndrq > 0 &&
	    (ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ))
		return (0);

	rv = 0;
	have_ppi = 0;

	if (bus_space_map(ia->ia_iot, IO_PPI, 1, 0, &ppi_ioh))
		goto lose;
	have_ppi = 1;

	/*
	 * Check for existence of PPI.  Realistically, this is either going to
	 * be here or nothing is going to be here.
	 *
	 * We don't want to have any chance of changing speaker output (which
	 * this test might, if it crashes in the middle, or something;
	 * normally it's be to quick to produce anthing audible), but
	 * many "combo chip" mock-PPI's don't seem to support the top bit
	 * of Port B as a settable bit.  The bottom bit has to be settable,
	 * since the speaker driver hardware still uses it.
	 */
	v = bus_space_read_1(ia->ia_iot, ppi_ioh, 0);		/* XXX */
	bus_space_write_1(ia->ia_iot, ppi_ioh, 0, v ^ 0x01);	/* XXX */
	nv = bus_space_read_1(ia->ia_iot, ppi_ioh, 0);		/* XXX */
	if (((nv ^ v) & 0x01) == 0x01)
		rv = 1;
	bus_space_write_1(ia->ia_iot, ppi_ioh, 0, v);		/* XXX */
	nv = bus_space_read_1(ia->ia_iot, ppi_ioh, 0);		/* XXX */
	if (((nv ^ v) & 0x01) != 0x00) {
		rv = 0;
		goto lose;
	}

	/*
	 * We assume that the programmable interval timer is there.
	 */

lose:
	if (have_ppi)
		bus_space_unmap(ia->ia_iot, ppi_ioh, 1);
	if (rv) {
		ia->ia_io[0].ir_addr = IO_PPI;
		ia->ia_io[0].ir_size = 1;
		ia->ia_nio = 1;

		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
	}
	return (rv);
}

void
pcppi_isa_attach(device_t parent, device_t self, void *aux)
{
        struct pcppi_softc *sc = device_private(self);
        struct isa_attach_args *ia = aux;
        bus_space_tag_t iot;

	sc->sc_dv = self;
        sc->sc_iot = iot = ia->ia_iot;

        sc->sc_size = 1;
        if (bus_space_map(iot, IO_PPI, sc->sc_size, 0, &sc->sc_ppi_ioh))
                panic("pcppi_attach: couldn't map");

	aprint_naive("\n");
	aprint_normal("\n");
        pcppi_attach(sc);
}

void
pcppi_childdet(device_t self, device_t child)
{

	/* we hold no child references, so do nothing */
}

int
pcppi_detach(device_t self, int flags)
{
	int rc;
	struct pcppi_softc *sc = device_private(self);

#if NATTIMER > 0
	pcppi_detach_speaker(sc);
#endif

	if ((rc = config_detach_children(sc->sc_dv, flags)) != 0)
		return rc;

	pmf_device_deregister(self);

#if NPCKBD > 0
	pckbd_unhook_bell(pcppi_pckbd_bell, sc);
#endif
	mutex_spin_enter(&tty_lock);
	pcppi_bell_stop(sc);
	mutex_spin_exit(&tty_lock);

	callout_halt(&sc->sc_bell_ch, NULL);
	callout_destroy(&sc->sc_bell_ch);

	cv_destroy(&sc->sc_slp);

	bus_space_unmap(sc->sc_iot, sc->sc_ppi_ioh, sc->sc_size);

	return 0;
}

void
pcppi_attach(struct pcppi_softc *sc)
{
	device_t self = sc->sc_dv;

	callout_init(&sc->sc_bell_ch, CALLOUT_MPSAFE);
	callout_setfunc(&sc->sc_bell_ch, pcppi_bell_callout, sc);
	cv_init(&sc->sc_slp, "bell");

        sc->sc_bellactive = sc->sc_bellpitch = 0;

#if NPCKBD > 0
	/* Provide a beeper for the PC Keyboard, if there isn't one already. */
	pckbd_hookup_bell(pcppi_pckbd_bell, sc);
#endif
#if NATTIMER > 0
	config_defer(sc->sc_dv, pcppi_attach_speaker);
#endif
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	pcppi_rescan(self, "pcppi", NULL);
}

int
pcppi_rescan(device_t self, const char *ifattr, const int *flags)
{
	struct pcppi_softc *sc = device_private(self);
        struct pcppi_attach_args pa;

	if (!ifattr_match(ifattr, "pcppi"))
		return 0;

	pa.pa_cookie = sc;
	config_search_loc(pcppisearch, sc->sc_dv, "pcppi", NULL, &pa);

	return 0;
}

static int
pcppisearch(device_t parent, cfdata_t cf, const int *locs, void *aux)
{

	if (config_match(parent, cf, aux))
		config_attach_loc(parent, cf, locs, aux, NULL);

	return 0;
}

#if NATTIMER > 0
static void
pcppi_detach_speaker(struct pcppi_softc *sc)
{
	if (sc->sc_timer != NULL) {
		attimer_detach_speaker(sc->sc_timer);
		sc->sc_timer = NULL;
	}
}

static void
pcppi_attach_speaker(device_t self)
{
	struct pcppi_softc *sc = device_private(self);

	if ((sc->sc_timer = attimer_attach_speaker()) == NULL)
		aprint_error_dev(self, "could not find any available timer\n");
	else {
		aprint_normal_dev(sc->sc_timer, "attached to %s\n",
		    device_xname(self));
	}
}
#endif

void
pcppi_bell(pcppi_tag_t self, int pitch, int period, int slp)
{

	mutex_spin_enter(&tty_lock);
	pcppi_bell_locked(self, pitch, period, slp);
	mutex_spin_exit(&tty_lock);
}

void
pcppi_bell_locked(pcppi_tag_t self, int pitch, int period, int slp)
{
	struct pcppi_softc *sc = self;

	if (sc->sc_bellactive) {
		if (sc->sc_timeout) {
			sc->sc_timeout = 0;
			callout_stop(&sc->sc_bell_ch);
		}
		cv_broadcast(&sc->sc_slp);
	}
	if (pitch == 0 || period == 0) {
		pcppi_bell_stop(sc);
		sc->sc_bellpitch = 0;
		return;
	}
	if (!sc->sc_bellactive || sc->sc_bellpitch != pitch) {
#if NATTIMER > 0
		if (sc->sc_timer != NULL)
			attimer_set_pitch(sc->sc_timer, pitch);
#endif
		/* enable speaker */
		bus_space_write_1(sc->sc_iot, sc->sc_ppi_ioh, 0,
			bus_space_read_1(sc->sc_iot, sc->sc_ppi_ioh, 0)
			| PIT_SPKR);
	}
	sc->sc_bellpitch = pitch;

	sc->sc_bellactive = 1;
	if (slp & PCPPI_BELL_POLL) {
		delay((period * 1000000) / hz);
		pcppi_bell_stop(sc);
	} else {
		sc->sc_timeout = 1;
		callout_schedule(&sc->sc_bell_ch, period);
		if (slp & PCPPI_BELL_SLEEP) {
			cv_wait_sig(&sc->sc_slp, &tty_lock);
		}
	}
}

static void
pcppi_bell_callout(void *arg)
{
	struct pcppi_softc *sc = arg;

	mutex_spin_enter(&tty_lock);
	if (sc->sc_timeout != 0) {
		pcppi_bell_stop(sc);
	}
	mutex_spin_exit(&tty_lock);
}

static void
pcppi_bell_stop(struct pcppi_softc *sc)
{

	sc->sc_timeout = 0;

	/* disable bell */
	bus_space_write_1(sc->sc_iot, sc->sc_ppi_ioh, 0,
			  bus_space_read_1(sc->sc_iot, sc->sc_ppi_ioh, 0)
			  & ~PIT_SPKR);
	sc->sc_bellactive = 0;
	cv_broadcast(&sc->sc_slp);
}

#if NPCKBD > 0
void
pcppi_pckbd_bell(void *arg, u_int pitch, u_int period, u_int volume,
    int poll)
{

	/*
	 * Comes in as ms, goes out at ticks; volume ignored.
	 */
	pcppi_bell_locked(arg, pitch, (period * hz) / 1000,
	    poll ? PCPPI_BELL_POLL : 0);
}
#endif /* NPCKBD > 0 */
