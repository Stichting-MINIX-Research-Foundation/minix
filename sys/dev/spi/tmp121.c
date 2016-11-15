/* $NetBSD: tmp121.c,v 1.5 2011/06/20 17:31:37 pgoyette Exp $ */

/*-
 * Copyright (c) 2006 Urbana-Champaign Independent Media Center.
 * Copyright (c) 2006 Garrett D'Amore.
 * All rights reserved.
 *
 * Portions of this code were written by Garrett D'Amore for the
 * Champaign-Urbana Community Wireless Network Project.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgements:
 *      This product includes software developed by the Urbana-Champaign
 *      Independent Media Center.
 *	This product includes software developed by Garrett D'Amore.
 * 4. Urbana-Champaign Independent Media Center's name and Garrett
 *    D'Amore's name may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE URBANA-CHAMPAIGN INDEPENDENT
 * MEDIA CENTER AND GARRETT D'AMORE ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE URBANA-CHAMPAIGN INDEPENDENT
 * MEDIA CENTER OR GARRETT D'AMORE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tmp121.c,v 1.5 2011/06/20 17:31:37 pgoyette Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/spi/spivar.h>

struct tmp121temp_softc {
	struct spi_handle *sc_sh;
	
	struct sysmon_envsys *sc_sme;
	envsys_data_t sc_sensor;
};

static int	tmp121temp_match(device_t, cfdata_t, void *);
static void	tmp121temp_attach(device_t, device_t, void *);

static void	tmp121temp_refresh(struct sysmon_envsys *, envsys_data_t *);

CFATTACH_DECL_NEW(tmp121temp, sizeof(struct tmp121temp_softc),
    tmp121temp_match, tmp121temp_attach, NULL, NULL);

static int
tmp121temp_match(device_t parent, cfdata_t cf, void *aux)
{
	struct spi_attach_args *sa = aux;

	/* configure for 10MHz */
	if (spi_configure(sa->sa_handle, SPI_MODE_0, 1000000))
		return 0;

	return 1;
}

static void
tmp121temp_attach(device_t parent, device_t self, void *aux)
{
	struct tmp121temp_softc *sc = device_private(self);
	struct spi_attach_args *sa = aux;

	aprint_naive(": Temperature Sensor\n");	
	aprint_normal(": TI TMP121 Temperature Sensor\n");

	sc->sc_sh = sa->sa_handle;

	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sensor.units = ENVSYS_STEMP;
	sc->sc_sensor.state = ENVSYS_SINVALID;
	strlcpy(sc->sc_sensor.desc, device_xname(self),
	    sizeof(sc->sc_sensor.desc));
	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor)) {
		sysmon_envsys_destroy(sc->sc_sme);
		return;
	}

	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_refresh = tmp121temp_refresh;
	sc->sc_sme->sme_cookie = sc;

	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(self, "unable to register with sysmon\n");
		sysmon_envsys_destroy(sc->sc_sme);
	}
}

static void
tmp121temp_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct tmp121temp_softc *sc = sme->sme_cookie;
	uint16_t		reg;
	int16_t			sreg;
	int			val;

	if (spi_recv(sc->sc_sh, 2, (uint8_t *)&reg) != 0) {
		edata->state = ENVSYS_SINVALID;
		return;
	}

	sreg = (int16_t)be16toh(reg);

	/*
	 * convert to uK:
	 *
	 * TMP121 bits:
	 * D15		: sign bit
	 * D14-D3	: data (D14 is MSB)
	 * D2-D0	: zero
	 *
	 * The data is represented in units of 0.0625 deg C.
	 */
	sreg >>= 3;
	val = sreg * 62500 + 273150000;

	edata->value_cur = val;
	edata->state = ENVSYS_SVALID;
}
