/* $NetBSD: joy_ess.c,v 1.6 2011/11/23 23:07:32 jmcneill Exp $ */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: joy_ess.c,v 1.6 2011/11/23 23:07:32 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/essvar.h>
#include <dev/ic/joyvar.h>

static int 	joy_ess_match(device_t, cfdata_t, void *);
static void 	joy_ess_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(joy_ess, sizeof (struct joy_softc),
	      joy_ess_match, joy_ess_attach, NULL, NULL);

static int
joy_ess_match(device_t parent, cfdata_t match, void *aux)
{
	struct audio_attach_args *aa = aux;

	if (aa->type != AUDIODEV_TYPE_AUX)
		return 0;
	return 1;
}

static void
joy_ess_attach(device_t parent, device_t self, void *aux)
{
	struct ess_softc *esc = device_private(parent);
	struct joy_softc *sc = device_private(self);

	aprint_normal("\n");

	sc->sc_iot = esc->sc_joy_iot;
	sc->sc_ioh = esc->sc_joy_ioh;
	sc->sc_dev = self;
	sc->sc_lock = &esc->sc_lock;

	joyattach(sc);
}
