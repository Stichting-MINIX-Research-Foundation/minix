/* $NetBSD: ym_acpi.c,v 1.14 2011/06/02 14:12:25 tsutsui Exp $ */

/*
 * Copyright (c) 2006 Jasper Wallace <jasper@pointless.net>
 * All rights reserved.
 *
 * Copyright (c) 2002 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ym_acpi.c,v 1.14 2011/06/02 14:12:25 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/acpi/acpivar.h>

#include <dev/audio_if.h>

#include <dev/ic/ad1848reg.h>
#include <dev/ic/opl3sa3reg.h>

#include <dev/isa/ad1848var.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/ymvar.h>


static int	ym_acpi_match(device_t, cfdata_t, void *);
static void	ym_acpi_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ym_acpi, sizeof(struct ym_softc), ym_acpi_match,
    ym_acpi_attach, NULL, NULL);

/*
 * ym_acpi_match: autoconf(9) match routine
 */
static int
ym_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;
	if (!(aa->aa_node->ad_devinfo->Valid & ACPI_VALID_HID))
		return 0;
	if (!aa->aa_node->ad_devinfo->HardwareId.String)
		return 0;
	/* Yamaha OPL3-SA2 or OPL3-SA3 */
	if (strcmp("YMH0021", aa->aa_node->ad_devinfo->HardwareId.String))
		return 0;

	return 1;
}

/*
 * ym_acpi_attach: autoconf(9) attach routine
 */
static void
ym_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct ym_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_io *sb_io, *codec_io, *opl_io, *control_io;
#if NMPU_YM > 0
	struct acpi_io *mpu_io;
#endif
	struct acpi_irq *irq;
	struct acpi_drq *playdrq, *recdrq;
	struct ad1848_softc *ac = &sc->sc_ad1848.sc_ad1848;
	ACPI_STATUS rv;

	ac->sc_dev = self;
	/* Parse our resources */
	rv = acpi_resource_parse(self,
	    aa->aa_node->ad_handle, "_CRS", &res,
	    &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/*
	 * sc_sb_ioh	 @ 0
	 * sc_ioh	 @ 1
	 * sc_opl_ioh	 @ 2
	 * sc_mpu_ioh	 @ 3
	 * sc_controlioh @ 4
	 */

	/* Find and map our i/o registers */
	sc->sc_iot = aa->aa_iot;
	sb_io	 = acpi_res_io(&res, 0);
	codec_io = acpi_res_io(&res, 1);
	opl_io	 = acpi_res_io(&res, 2);
#if NMPU_YM > 0
	mpu_io	 = acpi_res_io(&res, 3);
#endif
	control_io = acpi_res_io(&res, 4);

	if (sb_io == NULL || codec_io == NULL || opl_io == NULL ||
#if NMPU_YM > 0
	    mpu_io == NULL ||
#endif
	    control_io == NULL) {
		aprint_error_dev(self, "unable to find i/o registers resource\n");
		goto out;
	}
	if (bus_space_map(sc->sc_iot, sb_io->ar_base, sb_io->ar_length,
	    0, &sc->sc_sb_ioh) != 0) {
		aprint_error_dev(self, "unable to map i/o registers (sb)\n");
		goto out;
	}
	if (bus_space_map(sc->sc_iot, codec_io->ar_base, codec_io->ar_length,
	    0, &sc->sc_ioh) != 0) {
		aprint_error_dev(self, "unable to map i/o registers (codec)\n");
		goto out;
	}
	if (bus_space_map(sc->sc_iot, opl_io->ar_base, opl_io->ar_length,
	    0, &sc->sc_opl_ioh) != 0) {
		aprint_error_dev(self, "unable to map i/o registers (opl)\n");
		goto out;
	}
#if NMPU_YM > 0
	if (bus_space_map(sc->sc_iot, mpu_io->ar_base, mpu_io->ar_length,
	    0, &sc->sc_mpu_ioh) != 0) {
		aprint_error_dev(self, "unable to map i/o registers (mpu)\n");
		goto out;
	}
#endif
	if (bus_space_map(sc->sc_iot, control_io->ar_base,
	    control_io->ar_length, 0, &sc->sc_controlioh) != 0) {
		aprint_error_dev(self, "unable to map i/o registers (control)\n");
		goto out;
	}

	sc->sc_ic = aa->aa_ic;

	/* Find our IRQ */
	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(self, "unable to find irq resource\n");
		/* XXX bus_space_unmap */
		goto out;
	}
	sc->ym_irq = irq->ar_irq;

	/* Find our playback and record DRQs */
	playdrq = acpi_res_drq(&res, 0);
	recdrq = acpi_res_drq(&res, 1);
	if (playdrq == NULL) {
		aprint_error_dev(self, "unable to find drq resources\n");
		/* XXX bus_space_unmap */
		goto out;
	}
	if (recdrq == NULL) {
		/* half-duplex mode */
		sc->ym_recdrq = sc->ym_playdrq = playdrq->ar_drq;
	} else {
		sc->ym_playdrq = playdrq->ar_drq;
		sc->ym_recdrq = recdrq->ar_drq;
	}

	ac->sc_iot = sc->sc_iot;
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, WSS_CODEC,
	    AD1848_NPORT, &ac->sc_ioh)) {
		aprint_error_dev(self, "bus_space_subregion failed\n");
		/* XXX cleanup */
		goto out;
	}

	aprint_normal_dev(self, "");

	ac->mode = 2;
	ac->MCE_bit = MODE_CHANGE_ENABLE;

	sc->sc_ad1848.sc_ic = sc->sc_ic;

	/* Attach our ym device */
	ym_attach(sc);

 out:
	acpi_resource_cleanup(&res);
}
