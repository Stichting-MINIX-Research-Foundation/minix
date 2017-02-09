/*	$NetBSD: mcp3k.c,v 1.1 2015/08/18 15:54:20 phx Exp $ */

/*-
 * Copyright (c) 2015 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank Wille.
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

/*
 * Microchip MCP3x0x SAR analog to digital converters.
 * The driver supports various ADCs with different resolutions, operation
 * modes and number of input channels.
 * The reference voltage Vref defaults to the maximum output value in mV,
 * but can be changed via sysctl(3).
 *
 * MCP3001: http://ww1.microchip.com/downloads/en/DeviceDoc/21293C.pdf
 * MCP3002: http://ww1.microchip.com/downloads/en/DeviceDoc/21294E.pdf
 * MCP3004/3008: http://ww1.microchip.com/downloads/en/DeviceDoc/21295C.pdf
 * MCP3201: http://ww1.microchip.com/downloads/en/DeviceDoc/21290D.pdf
 * MCP3204/3208: http://ww1.microchip.com/downloads/en/DeviceDoc/21298c.pdf
 * MCP3301: http://ww1.microchip.com/downloads/en/DeviceDoc/21700E.pdf
 * MPC3302/3304: http://ww1.microchip.com/downloads/en/DeviceDoc/21697F.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/types.h> 
#include <sys/sysctl.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/spi/spivar.h>

#define M3K_MAX_SENSORS		16		/* 8 single-ended & 8 diff. */

/* mcp3x0x model description */
struct mcp3kadc_model {
	uint32_t		name;
	uint8_t			bits;
	uint8_t			channels;
	uint8_t			lead;		/* leading bits to ignore */
	uint8_t			flags;
#define M3K_SGLDIFF		0x01		/* single-ended/differential */
#define M3K_D2D1D0		0x02		/* 3 channel select bits */
#define M3K_MSBF		0x04		/* MSBF select bit */
#define M3K_SIGNED		0x80		/* result is signed */
#define M3K_CTRL_NEEDED		(M3K_SGLDIFF | M3K_D2D1D0 | M3K_MSBF)
};

struct mcp3kadc_softc {
	device_t		sc_dev;
	struct spi_handle 	*sc_sh;
	int			sc_model;
	uint32_t		sc_adc_max;
	int32_t			sc_vref_mv;

	struct sysmon_envsys 	*sc_sme;
	envsys_data_t 		sc_sensors[M3K_MAX_SENSORS];
};

static int	mcp3kadc_match(device_t, cfdata_t, void *);
static void	mcp3kadc_attach(device_t, device_t, void *);
static void	mcp3kadc_envsys_refresh(struct sysmon_envsys *,
		    envsys_data_t *);
static int	sysctl_mcp3kadc_vref(SYSCTLFN_ARGS);

CFATTACH_DECL_NEW(mcp3kadc, sizeof(struct mcp3kadc_softc),
    mcp3kadc_match,  mcp3kadc_attach, NULL, NULL);

static struct mcp3kadc_model mcp3k_models[] = {
	{
		.name = 3001,
		.bits = 10,
		.channels = 1,
		.lead = 3,
		.flags = 0
	},
	{
		.name = 3002,
		.bits = 10,
		.channels = 2,
		.lead = 2,
		.flags = M3K_SGLDIFF | M3K_MSBF
	},
	{
		.name = 3004,
		.bits = 10,
		.channels = 4,
		.lead = 2,
		.flags = M3K_SGLDIFF | M3K_D2D1D0
	},
	{
		.name = 3008,
		.bits = 10,
		.channels = 8,
		.lead = 2,
		.flags = M3K_SGLDIFF | M3K_D2D1D0
	},
	{
		.name = 3201,
		.bits = 12,
		.channels = 1,
		.lead = 3,
		.flags = 0
	},
	{
		.name = 3202,
		.bits = 12,
		.channels = 2,
		.lead = 2,
		.flags = M3K_SGLDIFF | M3K_MSBF
	},
	{
		.name = 3204,
		.bits = 12,
		.channels = 4,
		.lead = 2,
		.flags = M3K_SGLDIFF | M3K_D2D1D0
	},
	{
		.name = 3208,
		.bits = 12,
		.channels = 8,
		.lead = 2,
		.flags = M3K_SGLDIFF | M3K_D2D1D0
	},
	{
		.name = 3301,
		.bits = 13,
		.channels = 1,
		.lead = 3,
		.flags = M3K_SIGNED
	},
	{
		.name = 3302,
		.bits = 13,
		.channels = 4,
		.lead = 2,
		.flags = M3K_SIGNED | M3K_SGLDIFF | M3K_D2D1D0
	},
	{
		.name = 3204,
		.bits = 13,
		.channels = 8,
		.lead = 2,
		.flags = M3K_SIGNED | M3K_SGLDIFF | M3K_D2D1D0
	},
};

static int
mcp3kadc_match(device_t parent, cfdata_t cf, void *aux)
{
	struct spi_attach_args *sa = aux;

	if (strcmp(cf->cf_name, "mcp3kadc") != 0)
		return 0;

	/* configure for 1MHz */
	if (spi_configure(sa->sa_handle, SPI_MODE_0, 1000000))
		return 0;

	return 1;
}

static void
mcp3kadc_attach(device_t parent, device_t self, void *aux)
{
	const struct sysctlnode *rnode, *node;
	struct spi_attach_args *sa;
	struct mcp3kadc_softc *sc;
	struct mcp3kadc_model *model;
	int ch, i;

	sa = aux;
	sc = device_private(self);
	sc->sc_dev = self;
	sc->sc_sh = sa->sa_handle;

	/* device flags define the model */
	sc->sc_model = device_cfdata(sc->sc_dev)->cf_flags;
	model = &mcp3k_models[sc->sc_model];

	aprint_naive(": Analog to Digital converter\n");
	aprint_normal(": MCP%u %u-channel %u-bit ADC\n",
	    (unsigned)model->name, (unsigned)model->channels,
	    (unsigned)model->bits);

	/* set a default Vref in mV according to the chip's ADC resolution */
	sc->sc_vref_mv = 1 << ((model->flags & M3K_SIGNED) ?
	    model->bits - 1 : model->bits);

	/* remember maximum value for this ADC - also used for masking */
	sc->sc_adc_max = (1 << model->bits) - 1;

	/* attach voltage sensors to envsys */
	sc->sc_sme = sysmon_envsys_create();

	/* adc difference from two neighbouring channels */
	for (ch = 0; ch < model->channels; ch++) {
		KASSERT(ch < M3K_MAX_SENSORS);
		sc->sc_sensors[ch].units = ENVSYS_SVOLTS_DC;
		sc->sc_sensors[ch].state = ENVSYS_SINVALID;
		if (model->channels == 1)
			strlcpy(sc->sc_sensors[ch].desc, "adc diff ch0",
			    sizeof(sc->sc_sensors[ch].desc));
		else
			snprintf(sc->sc_sensors[ch].desc,
			    sizeof(sc->sc_sensors[ch].desc),
			    "adc diff ch%d-ch%d", ch, ch ^ 1);
		sc->sc_sensors[ch].private = ch;
		sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensors[ch]);
	}

	if (model->flags & M3K_SGLDIFF) {
		/* adc from single ended channels */
		for (i = 0; i < model->channels; i++, ch++) {
			KASSERT(ch < M3K_MAX_SENSORS);
			sc->sc_sensors[ch].units = ENVSYS_SVOLTS_DC;
			sc->sc_sensors[ch].state = ENVSYS_SINVALID;
			snprintf(sc->sc_sensors[ch].desc,
			    sizeof(sc->sc_sensors[ch].desc),
			    "adc single ch%d", i);
			sc->sc_sensors[ch].private = ch;
			sysmon_envsys_sensor_attach(sc->sc_sme,
			    &sc->sc_sensors[ch]);
		}
	}

	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_refresh = mcp3kadc_envsys_refresh;
	sc->sc_sme->sme_cookie = sc;
	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(self, "unable to register with sysmon\n");
		sysmon_envsys_destroy(sc->sc_sme);
	}

	/* create a sysctl node for adjusting the ADC's reference voltage */
	rnode = node = NULL;
	sysctl_createv(NULL, 0, NULL, &rnode,
	    CTLFLAG_READWRITE,
	    CTLTYPE_NODE, device_xname(sc->sc_dev), NULL,
	    NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL);

	if (rnode != NULL)
		sysctl_createv(NULL, 0, NULL, &node,
		    CTLFLAG_READWRITE | CTLFLAG_OWNDESC,
		    CTLTYPE_INT, "vref",
		    SYSCTL_DESCR("ADC reference voltage"),
		    sysctl_mcp3kadc_vref, 0, (void *)sc, 0,
		    CTL_HW, rnode->sysctl_num, CTL_CREATE, CTL_EOL);
}

static void
mcp3kadc_envsys_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct mcp3kadc_softc *sc;
	struct mcp3kadc_model *model;
	uint8_t buf[2], ctrl;
	int32_t val, scale;

	sc = sme->sme_cookie;
	model = &mcp3k_models[sc->sc_model];
	scale = sc->sc_adc_max + 1;

	if (model->flags & M3K_CTRL_NEEDED) {
		/* we need to send some control bits first */
		ctrl = 1;	/* start bit */

		if (model->flags & M3K_SGLDIFF) {
			/* bit set to select single-ended mode */
			ctrl <<= 1;
			ctrl |= edata->private >= model->channels;
		}

		if (model->flags & M3K_D2D1D0) {
			/* 3 bits select the channel */
			ctrl <<= 3;
			ctrl |= edata->private & (model->channels - 1);
		} else {
			/* 1 bit selects between two channels */
			ctrl <<= 1;
			ctrl |= edata->private & 1;
		}

		if (model->flags & M3K_MSBF) {
			/* bit select MSB first format */
			ctrl <<= 1;
			ctrl |= 1;
		}

		/* send control bits, receive ADC data */
		if (spi_send_recv(sc->sc_sh, 1, &ctrl, 2, buf) != 0) {
			edata->state = ENVSYS_SINVALID;
			return;
		}
	} else {

		/* just read data from the ADC */
		if (spi_recv(sc->sc_sh, 2, buf) != 0) {
			edata->state = ENVSYS_SINVALID;
			return;
		}
	}

	/* extract big-endian ADC data from buffer */
	val = (buf[0] << 8) | buf[1];
	val = (val >> (16 - (model->bits + model->lead))) & sc->sc_adc_max;

	/* sign-extend the result, when needed */
	if (model->flags & M3K_SIGNED) {
		if (val & (1 << (model->bits - 1)))
			val -= sc->sc_adc_max + 1;
		scale >>= 1;	/* MSB is the sign */
	}

	/* scale the value for Vref and convert to mV */
	edata->value_cur = (sc->sc_vref_mv * val / scale) * 1000;
	edata->state = ENVSYS_SVALID;
}

static int
sysctl_mcp3kadc_vref(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct mcp3kadc_softc *sc;
	int32_t t;
	int error;

	node = *rnode;
	sc = node.sysctl_data;

	t = sc->sc_vref_mv;
	node.sysctl_data = &t;

	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	if (t <= 0)
		return EINVAL;

	sc->sc_vref_mv = t;
	return 0;
}
