/*	$NetBSD: wdc_pcmcia.c,v 1.124 2013/10/12 16:49:01 christos Exp $ */

/*-
 * Copyright (c) 1998, 2003, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Onno van der Linden and by Manuel Bouyer.
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
__KERNEL_RCSID(0, "$NetBSD: wdc_pcmcia.c,v 1.124 2013/10/12 16:49:01 christos Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/wdcreg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define	bus_space_write_multi_stream_2	bus_space_write_multi_2
#define	bus_space_write_multi_stream_4	bus_space_write_multi_4
#define	bus_space_read_multi_stream_2	bus_space_read_multi_2
#define	bus_space_read_multi_stream_4	bus_space_read_multi_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

#define WDC_PCMCIA_REG_NPORTS      8
#define WDC_PCMCIA_AUXREG_OFFSET   (WDC_PCMCIA_REG_NPORTS + 6)
#define WDC_PCMCIA_AUXREG_NPORTS   2

struct wdc_pcmcia_softc {
	struct wdc_softc sc_wdcdev;
	struct ata_channel *wdc_chanlist[1];
	struct ata_channel ata_channel;
	struct ata_queue wdc_chqueue;
	struct wdc_regs wdc_regs;

	struct pcmcia_function *sc_pf;
	void *sc_ih;

	int sc_state;
#define WDC_PCMCIA_ATTACHED	3
};

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_read_region_stream_2 bus_space_read_region_2
#define bus_space_read_region_stream_4 bus_space_read_region_4
#define bus_space_write_region_stream_2 bus_space_write_region_2
#define bus_space_write_region_stream_4 bus_space_write_region_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

static int wdc_pcmcia_match(device_t, cfdata_t, void *);
static int wdc_pcmcia_validate_config_io(struct pcmcia_config_entry *);
static int wdc_pcmcia_validate_config_memory(struct pcmcia_config_entry *);
static void wdc_pcmcia_attach(device_t, device_t, void *);
static int wdc_pcmcia_detach(device_t, int);

CFATTACH_DECL_NEW(wdc_pcmcia, sizeof(struct wdc_pcmcia_softc),
    wdc_pcmcia_match, wdc_pcmcia_attach, wdc_pcmcia_detach, NULL);

static const struct wdc_pcmcia_product {
	struct pcmcia_product wdc_product;
	int wdc_ndrive;
} wdc_pcmcia_products[] = {
	{ { PCMCIA_VENDOR_DIGITAL,
	  PCMCIA_PRODUCT_DIGITAL_MOBILE_MEDIA_CDROM,
	  {NULL, "Digital Mobile Media CD-ROM", NULL, NULL} }, 2 },

	{ { PCMCIA_VENDOR_IBM,
	  PCMCIA_PRODUCT_IBM_PORTABLE_CDROM,
	  {NULL, "PCMCIA Portable CD-ROM Drive", NULL, NULL} }, 2 },

	/* The TEAC IDE/Card II is used on the Sony Vaio */
	{ { PCMCIA_VENDOR_TEAC,
	  PCMCIA_PRODUCT_TEAC_IDECARDII,
	  PCMCIA_CIS_TEAC_IDECARDII }, 2 },

	/*
	 * A fujitsu rebranded panasonic drive that reports
	 * itself as function "scsi", disk interface 0
	 */
	{ { PCMCIA_VENDOR_PANASONIC,
	  PCMCIA_PRODUCT_PANASONIC_KXLC005,
	  PCMCIA_CIS_PANASONIC_KXLC005 }, 2 },

	{ { PCMCIA_VENDOR_SANDISK,
	  PCMCIA_PRODUCT_SANDISK_SDCFB,
	  PCMCIA_CIS_SANDISK_SDCFB }, 1 },

	/*
	 * EXP IDE/ATAPI DVD Card use with some DVD players.
	 * Does not have a vendor ID or product ID.
	 */
	{ { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_EXP_EXPMULTIMEDIA }, 2 },

	/* Mobile Dock 2, neither vendor ID nor product ID */
	{ { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"SHUTTLE TECHNOLOGY LTD.", "PCCARD-IDE/ATAPI Adapter", NULL, NULL} }, 2 },

	/* Toshiba Portege 3110 CD, neither vendor ID nor product ID */
	{ { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"FREECOM", "PCCARD-IDE", NULL, NULL} }, 2 },

	/* Random CD-ROM, (badged AMACOM), neither vendor ID nor product ID */
	{ { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"PCMCIA", "CD-ROM", NULL, NULL} }, 2 },

	/* IO DATA CBIDE2, with neither vendor ID nor product ID */
	{ { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_IODATA_CBIDE2 }, 2 },

	/* TOSHIBA PA2673U(IODATA_CBIDE2 OEM), */
	/*  with neither vendor ID nor product ID */
	{ { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_TOSHIBA_CBIDE2 }, 2 },

	/*
	 * Novac PCMCIA-IDE Card for HD530P IDE Box,
	 * with neither vendor ID nor product ID
	 */
	{ { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"PCMCIA", "PnPIDE", NULL, NULL} }, 2 },

	{ { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"PCMCIA", "IDE CARD", NULL, NULL} }, 2 },

};
static const size_t wdc_pcmcia_nproducts =
    sizeof(wdc_pcmcia_products) / sizeof(wdc_pcmcia_products[0]);

static int	wdc_pcmcia_enable(device_t, int);
static void	wdc_pcmcia_datain_memory(struct ata_channel *, int, void *,
					 size_t);
static void	wdc_pcmcia_dataout_memory(struct ata_channel *, int, void *,
					  size_t);

static int
wdc_pcmcia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->pf->function == PCMCIA_FUNCTION_DISK &&
	    pa->pf->pf_funce_disk_interface == PCMCIA_TPLFE_DDI_PCCARD_ATA)
		return (1);
	if (pcmcia_product_lookup(pa, wdc_pcmcia_products, wdc_pcmcia_nproducts,
	    sizeof(wdc_pcmcia_products[0]), NULL))
		return (2);
	return (0);
}

static int
wdc_pcmcia_validate_config_io(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1 || cfe->num_iospace > 2)
		return (EINVAL);
	cfe->num_memspace = 0;
	return (0);
}

static int
wdc_pcmcia_validate_config_memory(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_MEMORY ||
	    cfe->num_memspace > 1 ||
	    cfe->memspace[0].length < 2048)
		return (EINVAL);
	cfe->num_iospace = 0;
	return (0);
}

static void
wdc_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct wdc_pcmcia_softc *sc = device_private(self);
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct wdc_regs *wdr;
	const struct wdc_pcmcia_product *wdcp;
	bus_size_t offset;
	int i;
	int error;

	aprint_naive("\n");

	sc->sc_wdcdev.sc_atac.atac_dev = self;
	sc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf,
	    wdc_pcmcia_validate_config_io);
	if (error)
		/*XXXmem16|common*/
		error = pcmcia_function_configure(pa->pf,
		    wdc_pcmcia_validate_config_memory);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n", error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16;

	sc->sc_wdcdev.regs = wdr = &sc->wdc_regs;

	if (cfe->iftype == PCMCIA_IFTYPE_MEMORY) {
		wdr->cmd_iot = cfe->memspace[0].handle.memt;
		wdr->cmd_baseioh = cfe->memspace[0].handle.memh;
		offset = cfe->memspace[0].offset;
		wdr->ctl_iot = cfe->memspace[0].handle.memt;
		if (bus_space_subregion(cfe->memspace[0].handle.memt,
		    cfe->memspace[0].handle.memh,
		    offset + WDC_PCMCIA_AUXREG_OFFSET, WDC_PCMCIA_AUXREG_NPORTS,
		    &wdr->ctl_ioh))
			goto fail;
	} else {
		wdr->cmd_iot = cfe->iospace[0].handle.iot;
		wdr->cmd_baseioh = cfe->iospace[0].handle.ioh;
		offset = 0;
		if (cfe->num_iospace == 1) {
			wdr->ctl_iot = cfe->iospace[0].handle.iot;
			if (bus_space_subregion(cfe->iospace[0].handle.iot,
			    cfe->iospace[0].handle.ioh,
			    WDC_PCMCIA_AUXREG_OFFSET, WDC_PCMCIA_AUXREG_NPORTS,
			    &wdr->ctl_ioh))
				goto fail;
		} else {
			wdr->ctl_iot = cfe->iospace[1].handle.iot;
			wdr->ctl_ioh = cfe->iospace[1].handle.ioh;
		}
	}

	for (i = 0; i < WDC_PCMCIA_REG_NPORTS; i++) {
		if (bus_space_subregion(wdr->cmd_iot,
		    wdr->cmd_baseioh,
		    offset + i, i == 0 ? 4 : 1,
		    &wdr->cmd_iohs[i]) != 0) {
			aprint_error_dev(self, "can't subregion I/O space\n");
			goto fail;
		}
	}

	if (cfe->iftype == PCMCIA_IFTYPE_MEMORY) {
		aprint_normal_dev(self, "memory mapped mode\n");
		wdr->data32iot = cfe->memspace[0].handle.memt;
		if (bus_space_subregion(cfe->memspace[0].handle.memt,
		    cfe->memspace[0].handle.memh, offset + 1024, 1024,
		    &wdr->data32ioh))
			goto fail;
		sc->sc_wdcdev.datain_pio = wdc_pcmcia_datain_memory;
		sc->sc_wdcdev.dataout_pio = wdc_pcmcia_dataout_memory;
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_NOIRQ;
	} else {
		aprint_normal_dev(self, "i/o mapped mode\n");
		wdr->data32iot = wdr->cmd_iot;
		wdr->data32ioh = wdr->cmd_iohs[wd_data];
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA32;
	}

	sc->sc_wdcdev.sc_atac.atac_pio_cap = 0;
	sc->wdc_chanlist[0] = &sc->ata_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanlist;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 1;
	sc->ata_channel.ch_channel = 0;
	sc->ata_channel.ch_atac = &sc->sc_wdcdev.sc_atac;
	sc->ata_channel.ch_queue = &sc->wdc_chqueue;
	wdcp = pcmcia_product_lookup(pa, wdc_pcmcia_products,
	    wdc_pcmcia_nproducts, sizeof(wdc_pcmcia_products[0]), NULL);
	sc->sc_wdcdev.wdc_maxdrives = wdcp ? wdcp->wdc_ndrive : 2;
	wdc_init_shadow_regs(&sc->ata_channel);

	error = wdc_pcmcia_enable(self, 1);
	if (error)
		goto fail;

	/* We can enable and disable the controller. */
	sc->sc_wdcdev.sc_atac.atac_atapi_adapter._generic.adapt_enable =
	    wdc_pcmcia_enable;
	sc->sc_wdcdev.sc_atac.atac_atapi_adapter._generic.adapt_refcnt = 1;

	/*
	 * Some devices needs some more delay after power up to stabilize
	 * and probe properly, so give them half a second.
	 * See PR 25659 for details.
	 */
	config_pending_incr(self);
	tsleep(wdc_pcmcia_attach, PWAIT, "wdcattach", hz / 2);

	wdcattach(&sc->ata_channel);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "unable to establish power handler\n");

	config_pending_decr(self);
	ata_delref(&sc->ata_channel);
	sc->sc_state = WDC_PCMCIA_ATTACHED;
	return;

fail:
	pcmcia_function_unconfigure(pa->pf);
}

static int
wdc_pcmcia_detach(device_t self, int flags)
{
	struct wdc_pcmcia_softc *sc = device_private(self);
	int error;

	if (sc->sc_state != WDC_PCMCIA_ATTACHED)
		return (0);

	pmf_device_deregister(self);

	if ((error = wdcdetach(self, flags)) != 0)
		return (error);

	pcmcia_function_unconfigure(sc->sc_pf);

	return (0);
}

static int
wdc_pcmcia_enable(device_t self, int onoff)
{
	struct wdc_pcmcia_softc *sc = device_private(self);
	int error;

#if 1
	/*
	 * XXX temporary kludge: we need to allow enabling while (cold)
	 * for some hpc* ports which attach pcmcia devices too early.
	 * This is problematic because pcmcia code uses tsleep() in
	 * the attach code path, but it seems to work somehow.
	 */
	if (doing_shutdown)
		return (EIO);
#else
	if (cold || doing_shutdown)
		return (EIO);
#endif

	if (onoff) {
		/* Establish the interrupt handler. */
		sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_BIO,
		    wdcintr, &sc->ata_channel);
		if (!sc->sc_ih)
			return (EIO);

		error = pcmcia_function_enable(sc->sc_pf);
		if (error) {
			pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
			sc->sc_ih = 0;
			return (error);
		}
	} else {
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return (0);
}

static void
wdc_pcmcia_datain_memory(struct ata_channel *chp, int flags, void *buf,
    size_t len)
{
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(chp);

	while (len > 0) {
		size_t n;

		n = min(len, 1024);
		if ((flags & ATA_DRIVE_CAP32) && (n & 3) == 0)
			bus_space_read_region_stream_4(wdr->data32iot,
			    wdr->data32ioh, 0, buf, n >> 2);
		else
			bus_space_read_region_stream_2(wdr->data32iot,
			    wdr->data32ioh, 0, buf, n >> 1);
		buf = (char *)buf + n;
		len -= n;
	}
}

static void
wdc_pcmcia_dataout_memory(struct ata_channel *chp, int flags, void *buf,
    size_t len)
{
	struct wdc_regs *wdr = CHAN_TO_WDC_REGS(chp);

	while (len > 0) {
		size_t n;

		n = min(len, 1024);
		if ((flags & ATA_DRIVE_CAP32) && (n & 3) == 0)
			bus_space_write_region_stream_4(wdr->data32iot,
			    wdr->data32ioh, 0, buf, n >> 2);
		else
			bus_space_write_region_stream_2(wdr->data32iot,
			    wdr->data32ioh, 0, buf, n >> 1);
		buf = (char *)buf + n;
		len -= n;
	}
}
