/*	$NetBSD: mpu_isapnp.c,v 1.20 2011/12/07 17:35:01 jakllsch Exp $	*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpu_isapnp.c,v 1.20 2011/12/07 17:35:01 jakllsch Exp $");

#include "midi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/audioio.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/ic/mpuvar.h>

static int	mpu_isapnp_match(device_t, cfdata_t, void *);
static void	mpu_isapnp_attach(device_t, device_t, void *);

struct mpu_isapnp_softc {
	void *sc_ih;
	kmutex_t sc_lock;

	struct mpu_softc sc_mpu;
};

CFATTACH_DECL_NEW(mpu_isapnp, sizeof(struct mpu_isapnp_softc),
    mpu_isapnp_match, mpu_isapnp_attach, NULL, NULL);

static int
mpu_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_mpu_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return pri;
}

static void
mpu_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct mpu_isapnp_softc *sc = device_private(self);
	struct isapnp_attach_args *ipa = aux;

	aprint_normal("\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(self, "error in region allocation\n");
		return;
	}

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_AUDIO);

	sc->sc_mpu.iot = ipa->ipa_iot;
	sc->sc_mpu.ioh = ipa->ipa_io[0].h;
	sc->sc_mpu.lock = &sc->sc_lock;

	mutex_enter(&sc->sc_lock);

	if (!mpu_find(&sc->sc_mpu)) {
		aprint_error_dev(self, "find failed\n");
		mutex_exit(&sc->sc_lock);
		mutex_destroy(&sc->sc_lock);
		return;
	}

	mutex_exit(&sc->sc_lock);

	aprint_normal_dev(self, "%s %s\n", ipa->ipa_devident,
	       ipa->ipa_devclass);

	sc->sc_mpu.model = "Roland MPU-401 MIDI UART";

	midi_attach_mi(&mpu_midi_hw_if, &sc->sc_mpu, self);

	sc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_AUDIO, mpu_intr, &sc->sc_mpu);
}
