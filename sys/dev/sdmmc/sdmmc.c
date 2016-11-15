/*	$NetBSD: sdmmc.c,v 1.31 2015/08/09 13:18:46 mlelstv Exp $	*/
/*	$OpenBSD: sdmmc.c,v 1.18 2009/01/09 10:58:38 jsg Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*-
 * Copyright (C) 2007, 2008, 2009 NONAKA Kimihiro <nonaka@netbsd.org>
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

/*
 * Host controller independent SD/MMC bus driver based on information
 * from SanDisk SD Card Product Manual Revision 2.2 (SanDisk), SDIO
 * Simple Specification Version 1.0 (SDIO) and the Linux "mmc" driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sdmmc.c,v 1.31 2015/08/09 13:18:46 mlelstv Exp $");

#ifdef _KERNEL_OPT
#include "opt_sdmmc.h"
#endif

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/callout.h>

#include <machine/vmparam.h>

#include <dev/sdmmc/sdmmc_ioreg.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

#ifdef SDMMC_DEBUG
int sdmmcdebug = 0;
static void sdmmc_dump_command(struct sdmmc_softc *, struct sdmmc_command *);
#define DPRINTF(n,s)	do { if ((n) <= sdmmcdebug) printf s; } while (0)
#else
#define	DPRINTF(n,s)	do {} while (0)
#endif

#define	DEVNAME(sc)	SDMMCDEVNAME(sc)

static int sdmmc_match(device_t, cfdata_t, void *);
static void sdmmc_attach(device_t, device_t, void *);
static int sdmmc_detach(device_t, int);

CFATTACH_DECL_NEW(sdmmc, sizeof(struct sdmmc_softc),
    sdmmc_match, sdmmc_attach, sdmmc_detach, NULL);

static void sdmmc_doattach(device_t);
static void sdmmc_task_thread(void *);
static void sdmmc_discover_task(void *);
static void sdmmc_polling_card(void *);
static void sdmmc_card_attach(struct sdmmc_softc *);
static void sdmmc_card_detach(struct sdmmc_softc *, int);
static int sdmmc_print(void *, const char *);
static int sdmmc_enable(struct sdmmc_softc *);
static void sdmmc_disable(struct sdmmc_softc *);
static int sdmmc_scan(struct sdmmc_softc *);
static int sdmmc_init(struct sdmmc_softc *);

static int
sdmmc_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sdmmcbus_attach_args *saa = (struct sdmmcbus_attach_args *)aux;

	if (strcmp(saa->saa_busname, cf->cf_name) == 0)
		return 1;
	return 0;
}

static void
sdmmc_attach(device_t parent, device_t self, void *aux)
{
	struct sdmmc_softc *sc = device_private(self);
	struct sdmmcbus_attach_args *saa = (struct sdmmcbus_attach_args *)aux;
	int error;

	aprint_normal("\n");
	aprint_naive("\n");

	sc->sc_dev = self;
	sc->sc_sct = saa->saa_sct;
	sc->sc_spi_sct = saa->saa_spi_sct;
	sc->sc_sch = saa->saa_sch;
	sc->sc_dmat = saa->saa_dmat;
	sc->sc_clkmin = saa->saa_clkmin;
	sc->sc_clkmax = saa->saa_clkmax;
	sc->sc_busclk = sc->sc_clkmax;
	sc->sc_buswidth = 1;
	sc->sc_caps = saa->saa_caps;

	if (ISSET(sc->sc_caps, SMC_CAPS_DMA)) {
		error = bus_dmamap_create(sc->sc_dmat, MAXPHYS, SDMMC_MAXNSEGS,
		    MAXPHYS, 0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &sc->sc_dmap);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "couldn't create dma map. (error=%d)\n", error);
			return;
		}
	}

	SIMPLEQ_INIT(&sc->sf_head);
	TAILQ_INIT(&sc->sc_tskq);
	TAILQ_INIT(&sc->sc_intrq);

	sdmmc_init_task(&sc->sc_discover_task, sdmmc_discover_task, sc);
	sdmmc_init_task(&sc->sc_intr_task, sdmmc_intr_task, sc);

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_tskq_mtx, MUTEX_DEFAULT, IPL_SDMMC);
	mutex_init(&sc->sc_discover_task_mtx, MUTEX_DEFAULT, IPL_SDMMC);
	mutex_init(&sc->sc_intr_task_mtx, MUTEX_DEFAULT, IPL_SDMMC);
	cv_init(&sc->sc_tskq_cv, "mmctaskq");

	if (ISSET(sc->sc_caps, SMC_CAPS_POLL_CARD_DET)) {
		callout_init(&sc->sc_card_detect_ch, 0);
		callout_reset(&sc->sc_card_detect_ch, hz,
		    sdmmc_polling_card, sc);
	}

	if (!pmf_device_register(self, NULL, NULL)) {
		aprint_error_dev(self, "couldn't establish power handler\n");
	}

	SET(sc->sc_flags, SMF_INITED);

	/*
	 * Create the event thread that will attach and detach cards
	 * and perform other lengthy operations.
	 */
	config_pending_incr(self);
	config_interrupts(self, sdmmc_doattach);
}

static int
sdmmc_detach(device_t self, int flags)
{
	struct sdmmc_softc *sc = device_private(self);
	int error;

	mutex_enter(&sc->sc_tskq_mtx);
	sc->sc_dying = 1;
	cv_signal(&sc->sc_tskq_cv);
	while (sc->sc_tskq_lwp != NULL)
		cv_wait(&sc->sc_tskq_cv, &sc->sc_tskq_mtx);
	mutex_exit(&sc->sc_tskq_mtx);

	pmf_device_deregister(self);

	error = config_detach_children(self, flags);
	if (error)
		return error;

	if (ISSET(sc->sc_caps, SMC_CAPS_DMA)) {
		bus_dmamap_unload(sc->sc_dmat, sc->sc_dmap);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmap);
	}

	if (ISSET(sc->sc_caps, SMC_CAPS_POLL_CARD_DET)) {
		callout_halt(&sc->sc_card_detect_ch, NULL);
		callout_destroy(&sc->sc_card_detect_ch);
	}

	cv_destroy(&sc->sc_tskq_cv);
	mutex_destroy(&sc->sc_intr_task_mtx);
	mutex_destroy(&sc->sc_discover_task_mtx);
	mutex_destroy(&sc->sc_tskq_mtx);
	mutex_destroy(&sc->sc_mtx);

	return 0;
}

static void
sdmmc_doattach(device_t dev)
{
	struct sdmmc_softc *sc = device_private(dev);

	if (kthread_create(PRI_BIO, 0, NULL,
	    sdmmc_task_thread, sc, &sc->sc_tskq_lwp, "%s", device_xname(dev))) {
		aprint_error_dev(dev, "couldn't create task thread\n");
	}
}

void
sdmmc_add_task(struct sdmmc_softc *sc, struct sdmmc_task *task)
{

	mutex_enter(&sc->sc_tskq_mtx);
	task->onqueue = 1;
	task->sc = sc;
	TAILQ_INSERT_TAIL(&sc->sc_tskq, task, next);
	cv_broadcast(&sc->sc_tskq_cv);
	mutex_exit(&sc->sc_tskq_mtx);
}

static inline void
sdmmc_del_task1(struct sdmmc_softc *sc, struct sdmmc_task *task)
{

	TAILQ_REMOVE(&sc->sc_tskq, task, next);
	task->sc = NULL;
	task->onqueue = 0;
}

void
sdmmc_del_task(struct sdmmc_task *task)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)task->sc;

	if (sc != NULL) {
		mutex_enter(&sc->sc_tskq_mtx);
		sdmmc_del_task1(sc, task);
		mutex_exit(&sc->sc_tskq_mtx);
	}
}

static void
sdmmc_task_thread(void *arg)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)arg;
	struct sdmmc_task *task;

	sdmmc_discover_task(sc);
	config_pending_decr(sc->sc_dev);

	mutex_enter(&sc->sc_tskq_mtx);
	for (;;) {
		task = TAILQ_FIRST(&sc->sc_tskq);
		if (task != NULL) {
			sdmmc_del_task1(sc, task);
			mutex_exit(&sc->sc_tskq_mtx);
			(*task->func)(task->arg);
			mutex_enter(&sc->sc_tskq_mtx);
		} else {
			/* Check for the exit condition. */
			if (sc->sc_dying)
				break;
			cv_wait(&sc->sc_tskq_cv, &sc->sc_tskq_mtx);
		}
	}
	/* time to die. */
	sc->sc_dying = 0;
	if (ISSET(sc->sc_flags, SMF_CARD_PRESENT)) {
		/*
		 * sdmmc_card_detach() may issue commands,
		 * so temporarily drop the interrupt-blocking lock.
		 */
		mutex_exit(&sc->sc_tskq_mtx);
		sdmmc_card_detach(sc, DETACH_FORCE);
		mutex_enter(&sc->sc_tskq_mtx);
	}
	sc->sc_tskq_lwp = NULL;
	cv_broadcast(&sc->sc_tskq_cv);
	mutex_exit(&sc->sc_tskq_mtx);
	kthread_exit(0);
}

void
sdmmc_needs_discover(device_t dev)
{
	struct sdmmc_softc *sc = device_private(dev);

	if (!ISSET(sc->sc_flags, SMF_INITED))
		return;

	mutex_enter(&sc->sc_discover_task_mtx);
	if (!sdmmc_task_pending(&sc->sc_discover_task))
		sdmmc_add_task(sc, &sc->sc_discover_task);
	mutex_exit(&sc->sc_discover_task_mtx);
}

static void
sdmmc_discover_task(void *arg)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)arg;
	int card_detect, card_present;

	mutex_enter(&sc->sc_discover_task_mtx);
	card_detect = sdmmc_chip_card_detect(sc->sc_sct, sc->sc_sch);
	card_present = ISSET(sc->sc_flags, SMF_CARD_PRESENT);
	if (card_detect)
		SET(sc->sc_flags, SMF_CARD_PRESENT);
	else
		CLR(sc->sc_flags, SMF_CARD_PRESENT);
	mutex_exit(&sc->sc_discover_task_mtx);

	if (card_detect) {
		if (!card_present) {
			sdmmc_card_attach(sc);
			mutex_enter(&sc->sc_discover_task_mtx);
			if (!ISSET(sc->sc_flags, SMF_CARD_ATTACHED))
				CLR(sc->sc_flags, SMF_CARD_PRESENT);
			mutex_exit(&sc->sc_discover_task_mtx);
		}
	} else {
		if (card_present)
			sdmmc_card_detach(sc, DETACH_FORCE);
	}
}

static void
sdmmc_polling_card(void *arg)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)arg;
	int card_detect, card_present;

	mutex_enter(&sc->sc_discover_task_mtx);
	card_detect = sdmmc_chip_card_detect(sc->sc_sct, sc->sc_sch);
	card_present = ISSET(sc->sc_flags, SMF_CARD_PRESENT);
	mutex_exit(&sc->sc_discover_task_mtx);

	if (card_detect != card_present)
		sdmmc_needs_discover(sc->sc_dev);

	callout_schedule(&sc->sc_card_detect_ch, hz);
}

/*
 * Called from process context when a card is present.
 */
static void
sdmmc_card_attach(struct sdmmc_softc *sc)
{
	struct sdmmc_function *sf;
	struct sdmmc_attach_args saa;
	int error;

	DPRINTF(1,("%s: attach card\n", DEVNAME(sc)));

	CLR(sc->sc_flags, SMF_CARD_ATTACHED);

	/*
	 * Power up the card (or card stack).
	 */
	error = sdmmc_enable(sc);
	if (error) {
		if (!ISSET(sc->sc_caps, SMC_CAPS_POLL_CARD_DET)) {
			aprint_error_dev(sc->sc_dev, "couldn't enable card: %d\n", error);
		}
		goto err;
	}

	/*
	 * Scan for I/O functions and memory cards on the bus,
	 * allocating a sdmmc_function structure for each.
	 */
	error = sdmmc_scan(sc);
	if (error) {
		aprint_error_dev(sc->sc_dev, "no functions\n");
		goto err;
	}

	/*
	 * Initialize the I/O functions and memory cards.
	 */
	error = sdmmc_init(sc);
	if (error) {
		aprint_error_dev(sc->sc_dev, "init failed\n");
		goto err;
	}

	SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list) {
		if (ISSET(sc->sc_flags, SMF_IO_MODE) && sf->number < 1)
			continue;

		memset(&saa, 0, sizeof saa);
		saa.manufacturer = sf->cis.manufacturer;
		saa.product = sf->cis.product;
		saa.interface = sf->interface;
		saa.sf = sf;

		sf->child =
		    config_found_ia(sc->sc_dev, "sdmmc", &saa, sdmmc_print);
	}

	SET(sc->sc_flags, SMF_CARD_ATTACHED);
	return;

err:
	sdmmc_card_detach(sc, DETACH_FORCE);
}

/*
 * Called from process context with DETACH_* flags from <sys/device.h>
 * when cards are gone.
 */
static void
sdmmc_card_detach(struct sdmmc_softc *sc, int flags)
{
	struct sdmmc_function *sf, *sfnext;

	DPRINTF(1,("%s: detach card\n", DEVNAME(sc)));

	if (ISSET(sc->sc_flags, SMF_CARD_ATTACHED)) {
		SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list) {
			if (sf->child != NULL) {
				config_detach(sf->child, DETACH_FORCE);
				sf->child = NULL;
			}
		}

		KASSERT(TAILQ_EMPTY(&sc->sc_intrq));

		CLR(sc->sc_flags, SMF_CARD_ATTACHED);
	}

	/* Power down. */
	sdmmc_disable(sc);

	/* Free all sdmmc_function structures. */
	for (sf = SIMPLEQ_FIRST(&sc->sf_head); sf != NULL; sf = sfnext) {
		sfnext = SIMPLEQ_NEXT(sf, sf_list);
		sdmmc_function_free(sf);
	}
	SIMPLEQ_INIT(&sc->sf_head);
	sc->sc_function_count = 0;
	sc->sc_fn0 = NULL;
}

static int
sdmmc_print(void *aux, const char *pnp)
{
	struct sdmmc_attach_args *sa = aux;
	struct sdmmc_function *sf = sa->sf;
	struct sdmmc_cis *cis = &sf->sc->sc_fn0->cis;
	int i, x;

	if (pnp) {
		if (sf->number == 0)
			return QUIET;

		for (i = 0; i < 4 && cis->cis1_info[i]; i++)
			printf("%s%s", i ? ", " : "\"", cis->cis1_info[i]);
		if (i != 0)
			printf("\"");

		if ((cis->manufacturer != SDMMC_VENDOR_INVALID &&
		    cis->product != SDMMC_PRODUCT_INVALID) ||
		    sa->interface != SD_IO_SFIC_NO_STANDARD) {
			x = !!(cis->manufacturer != SDMMC_VENDOR_INVALID);
			x += !!(cis->product != SDMMC_PRODUCT_INVALID);
			x += !!(sa->interface != SD_IO_SFIC_NO_STANDARD);
			printf("%s(", i ? " " : "");
			if (cis->manufacturer != SDMMC_VENDOR_INVALID)
				printf("manufacturer 0x%x%s",
				    cis->manufacturer, (--x == 0) ?  "" : ", ");
			if (cis->product != SDMMC_PRODUCT_INVALID)
				printf("product 0x%x%s",
				    cis->product, (--x == 0) ?  "" : ", ");
			if (sa->interface != SD_IO_SFIC_NO_STANDARD)
				printf("standard function interface code 0x%x",
				    sf->interface);
			printf(")");
		}
		printf("%sat %s", i ? " " : "", pnp);
	}
	if (sf->number > 0)
		printf(" function %d", sf->number);

	if (!pnp) {
		for (i = 0; i < 3 && cis->cis1_info[i]; i++)
			printf("%s%s", i ? ", " : " \"", cis->cis1_info[i]);
		if (i != 0)
			printf("\"");
	}
	return UNCONF;
}

static int
sdmmc_enable(struct sdmmc_softc *sc)
{
	int error;

	/*
	 * Calculate the equivalent of the card OCR from the host
	 * capabilities and select the maximum supported bus voltage.
	 */
	error = sdmmc_chip_bus_power(sc->sc_sct, sc->sc_sch,
	    sdmmc_chip_host_ocr(sc->sc_sct, sc->sc_sch));
	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't supply bus power\n");
		goto out;
	}

	/*
	 * Select the minimum clock frequency.
	 */
	error = sdmmc_chip_bus_clock(sc->sc_sct, sc->sc_sch, SDMMC_SDCLK_400K,
	    false);
	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't supply clock\n");
		goto out;
	}

	/* XXX wait for card to power up */
	sdmmc_delay(100000);

	if (!ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE)) {
		/* Initialize SD I/O card function(s). */
		error = sdmmc_io_enable(sc);
		if (error) {
			DPRINTF(1, ("%s: sdmmc_io_enable failed %d\n", DEVNAME(sc), error));
			goto out;
		}
	}

	/* Initialize SD/MMC memory card(s). */
	if (ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE) ||
	    ISSET(sc->sc_flags, SMF_MEM_MODE)) {
		error = sdmmc_mem_enable(sc);
		if (error) {
			DPRINTF(1, ("%s: sdmmc_mem_enable failed %d\n", DEVNAME(sc), error));
			goto out;
		}
	}

out:
	if (error)
		sdmmc_disable(sc);
	return error;
}

static void
sdmmc_disable(struct sdmmc_softc *sc)
{
	/* XXX complete commands if card is still present. */

	if (!ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE)) {
		/* Make sure no card is still selected. */
		(void)sdmmc_select_card(sc, NULL);
	}

	/* Turn off bus power and clock. */
	(void)sdmmc_chip_bus_width(sc->sc_sct, sc->sc_sch, 1);
	(void)sdmmc_chip_bus_clock(sc->sc_sct, sc->sc_sch, SDMMC_SDCLK_OFF,
	    false);
	(void)sdmmc_chip_bus_power(sc->sc_sct, sc->sc_sch, 0);
	sc->sc_busclk = sc->sc_clkmax;
}

/*
 * Set the lowest bus voltage supported by the card and the host.
 */
int
sdmmc_set_bus_power(struct sdmmc_softc *sc, uint32_t host_ocr,
    uint32_t card_ocr)
{
	uint32_t bit;

	/* Mask off unsupported voltage levels and select the lowest. */
	DPRINTF(1,("%s: host_ocr=%x ", DEVNAME(sc), host_ocr));
	host_ocr &= card_ocr;
	for (bit = 4; bit < 23; bit++) {
		if (ISSET(host_ocr, (1 << bit))) {
			host_ocr &= (3 << bit);
			break;
		}
	}
	DPRINTF(1,("card_ocr=%x new_ocr=%x\n", card_ocr, host_ocr));

	if (host_ocr == 0 ||
	    sdmmc_chip_bus_power(sc->sc_sct, sc->sc_sch, host_ocr) != 0)
		return 1;
	return 0;
}

struct sdmmc_function *
sdmmc_function_alloc(struct sdmmc_softc *sc)
{
	struct sdmmc_function *sf;

	sf = malloc(sizeof *sf, M_DEVBUF, M_WAITOK|M_ZERO);
	if (sf == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't alloc memory (sdmmc function)\n");
		return NULL;
	}

	sf->sc = sc;
	sf->number = -1;
	sf->cis.manufacturer = SDMMC_VENDOR_INVALID;
	sf->cis.product = SDMMC_PRODUCT_INVALID;
	sf->cis.function = SDMMC_FUNCTION_INVALID;
	sf->width = 1;

	if (ISSET(sc->sc_flags, SMF_MEM_MODE) &&
	    ISSET(sc->sc_caps, SMC_CAPS_DMA) &&
	    !ISSET(sc->sc_caps, SMC_CAPS_MULTI_SEG_DMA)) {
		bus_dma_segment_t ds;
		int rseg, error;

		error = bus_dmamap_create(sc->sc_dmat, MAXPHYS, 1,
		    MAXPHYS, 0, BUS_DMA_WAITOK, &sf->bbuf_dmap);
		if (error)
			goto fail1;
		error = bus_dmamem_alloc(sc->sc_dmat, MAXPHYS,
		    PAGE_SIZE, 0, &ds, 1, &rseg, BUS_DMA_WAITOK);
		if (error)
			goto fail2;
		error = bus_dmamem_map(sc->sc_dmat, &ds, 1, MAXPHYS,
		    &sf->bbuf, BUS_DMA_WAITOK);
		if (error)
			goto fail3;
		error = bus_dmamap_load(sc->sc_dmat, sf->bbuf_dmap,
		    sf->bbuf, MAXPHYS, NULL,
		    BUS_DMA_WAITOK|BUS_DMA_READ|BUS_DMA_WRITE);
		if (error)
			goto fail4;
		error = bus_dmamap_create(sc->sc_dmat, MAXPHYS, 1,
		    MAXPHYS, 0, BUS_DMA_WAITOK, &sf->sseg_dmap);
		if (!error)
			goto out;

		bus_dmamap_unload(sc->sc_dmat, sf->bbuf_dmap);
fail4:
		bus_dmamem_unmap(sc->sc_dmat, sf->bbuf, MAXPHYS);
fail3:
		bus_dmamem_free(sc->sc_dmat, &ds, 1);
fail2:
		bus_dmamap_destroy(sc->sc_dmat, sf->bbuf_dmap);
fail1:
		free(sf, M_DEVBUF);
		sf = NULL;
	}
out:

	return sf;
}

void
sdmmc_function_free(struct sdmmc_function *sf)
{
	struct sdmmc_softc *sc = sf->sc;

	if (ISSET(sc->sc_flags, SMF_MEM_MODE) &&
	    ISSET(sc->sc_caps, SMC_CAPS_DMA) &&
	    !ISSET(sc->sc_caps, SMC_CAPS_MULTI_SEG_DMA)) {
		bus_dmamap_destroy(sc->sc_dmat, sf->sseg_dmap);
		bus_dmamap_unload(sc->sc_dmat, sf->bbuf_dmap);
		bus_dmamem_unmap(sc->sc_dmat, sf->bbuf, MAXPHYS);
		bus_dmamem_free(sc->sc_dmat,
		    sf->bbuf_dmap->dm_segs, sf->bbuf_dmap->dm_nsegs);
		bus_dmamap_destroy(sc->sc_dmat, sf->bbuf_dmap);
	}

	free(sf, M_DEVBUF);
}

/*
 * Scan for I/O functions and memory cards on the bus, allocating a
 * sdmmc_function structure for each.
 */
static int
sdmmc_scan(struct sdmmc_softc *sc)
{

	if (!ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE)) {
		/* Scan for I/O functions. */
		if (ISSET(sc->sc_flags, SMF_IO_MODE))
			sdmmc_io_scan(sc);
	}

	/* Scan for memory cards on the bus. */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE))
		sdmmc_mem_scan(sc);

	/* There should be at least one function now. */
	if (SIMPLEQ_EMPTY(&sc->sf_head)) {
		aprint_error_dev(sc->sc_dev, "couldn't identify card\n");
		return 1;
	}
	return 0;
}

/*
 * Initialize all the distinguished functions of the card, be it I/O
 * or memory functions.
 */
static int
sdmmc_init(struct sdmmc_softc *sc)
{
	struct sdmmc_function *sf;

	/* Initialize all identified card functions. */
	SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list) {
		if (!ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE)) {
			if (ISSET(sc->sc_flags, SMF_IO_MODE) &&
			    sdmmc_io_init(sc, sf) != 0) {
				aprint_error_dev(sc->sc_dev,
				    "i/o init failed\n");
			}
		}

		if (ISSET(sc->sc_flags, SMF_MEM_MODE) &&
		    sdmmc_mem_init(sc, sf) != 0) {
			aprint_error_dev(sc->sc_dev, "mem init failed\n");
		}
	}

	/* Any good functions left after initialization? */
	SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list) {
		if (!ISSET(sf->flags, SFF_ERROR))
			return 0;
	}

	/* No, we should probably power down the card. */
	return 1;
}

void
sdmmc_delay(u_int usecs)
{

	delay(usecs);
}

int
sdmmc_app_command(struct sdmmc_softc *sc, struct sdmmc_function *sf, struct sdmmc_command *cmd)
{
	struct sdmmc_command acmd;
	int error;

	DPRINTF(1,("sdmmc_app_command: start\n"));

	/* Don't lock */

	memset(&acmd, 0, sizeof(acmd));
	acmd.c_opcode = MMC_APP_CMD;
	acmd.c_arg = (sf != NULL) ? (sf->rca << 16) : 0;
	acmd.c_flags = SCF_CMD_AC | SCF_RSP_R1 | SCF_RSP_SPI_R1;

	error = sdmmc_mmc_command(sc, &acmd);
	if (error == 0) {
		if (!ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE) &&
		    !ISSET(MMC_R1(acmd.c_resp), MMC_R1_APP_CMD)) {
			/* Card does not support application commands. */
			error = ENODEV;
		} else {
			error = sdmmc_mmc_command(sc, cmd);
		}
	}
	DPRINTF(1,("sdmmc_app_command: done (error=%d)\n", error));
	return error;
}

/*
 * Execute MMC command and data transfers.  All interactions with the
 * host controller to complete the command happen in the context of
 * the current process.
 */
int
sdmmc_mmc_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	int error;

	DPRINTF(1,("sdmmc_mmc_command: cmd=%d, arg=%#x, flags=%#x\n",
	    cmd->c_opcode, cmd->c_arg, cmd->c_flags));

	/* Don't lock */

#if defined(DIAGNOSTIC) || defined(SDMMC_DEBUG)
	if (cmd->c_data && !ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE)) {
		if (sc->sc_card == NULL)
			panic("%s: deselected card\n", DEVNAME(sc));
	}
#endif

	sdmmc_chip_exec_command(sc->sc_sct, sc->sc_sch, cmd);

#ifdef SDMMC_DEBUG
	sdmmc_dump_command(sc, cmd);
#endif

	error = cmd->c_error;

	DPRINTF(1,("sdmmc_mmc_command: error=%d\n", error));

	if (error &&
	   (cmd->c_opcode == MMC_READ_BLOCK_MULTIPLE ||
	    cmd->c_opcode == MMC_WRITE_BLOCK_MULTIPLE)) {
		sdmmc_stop_transmission(sc);
	}

	return error;
}

/*
 * Send the "STOP TRANSMISSION" command
 */
void
sdmmc_stop_transmission(struct sdmmc_softc *sc)
{
	struct sdmmc_command cmd;

	DPRINTF(1,("sdmmc_stop_transmission\n"));

	/* Don't lock */

	memset(&cmd, 0, sizeof(cmd));
	cmd.c_opcode = MMC_STOP_TRANSMISSION;
	cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1B | SCF_RSP_SPI_R1B;

	(void)sdmmc_mmc_command(sc, &cmd);
}

/*
 * Send the "GO IDLE STATE" command.
 */
void
sdmmc_go_idle_state(struct sdmmc_softc *sc)
{
	struct sdmmc_command cmd;

	DPRINTF(1,("sdmmc_go_idle_state\n"));

	/* Don't lock */

	memset(&cmd, 0, sizeof(cmd));
	cmd.c_opcode = MMC_GO_IDLE_STATE;
	cmd.c_flags = SCF_CMD_BC | SCF_RSP_R0 | SCF_RSP_SPI_R1;

	(void)sdmmc_mmc_command(sc, &cmd);
}

/*
 * Retrieve (SD) or set (MMC) the relative card address (RCA).
 */
int
sdmmc_set_relative_addr(struct sdmmc_softc *sc, struct sdmmc_function *sf)
{
	struct sdmmc_command cmd;
	int error;

	/* Don't lock */

	if (ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE)) {
		aprint_error_dev(sc->sc_dev,
			"sdmmc_set_relative_addr: SMC_CAPS_SPI_MODE set");
		return EIO;
	}

	memset(&cmd, 0, sizeof(cmd));
	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		cmd.c_opcode = SD_SEND_RELATIVE_ADDR;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R6;
	} else {
		cmd.c_opcode = MMC_SET_RELATIVE_ADDR;
		cmd.c_arg = MMC_ARG_RCA(sf->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
	}
	error = sdmmc_mmc_command(sc, &cmd);
	if (error)
		return error;

	if (ISSET(sc->sc_flags, SMF_SD_MODE))
		sf->rca = SD_R6_RCA(cmd.c_resp);

	return 0;
}

int
sdmmc_select_card(struct sdmmc_softc *sc, struct sdmmc_function *sf)
{
	struct sdmmc_command cmd;
	int error;

	/* Don't lock */

	if (ISSET(sc->sc_caps, SMC_CAPS_SPI_MODE)) {
		aprint_error_dev(sc->sc_dev,
			"sdmmc_select_card: SMC_CAPS_SPI_MODE set");
		return EIO;
	}

	if (sc->sc_card == sf
	 || (sf && sc->sc_card && sc->sc_card->rca == sf->rca)) {
		sc->sc_card = sf;
		return 0;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.c_opcode = MMC_SELECT_CARD;
	cmd.c_arg = (sf == NULL) ? 0 : MMC_ARG_RCA(sf->rca);
	cmd.c_flags = SCF_CMD_AC | ((sf == NULL) ? SCF_RSP_R0 : SCF_RSP_R1);
	error = sdmmc_mmc_command(sc, &cmd);
	if (error == 0 || sf == NULL)
		sc->sc_card = sf;

	return error;
}

#ifdef SDMMC_DEBUG
static void
sdmmc_dump_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	int i;

	DPRINTF(1,("%s: cmd %u arg=%#x data=%p dlen=%d flags=%#x (error %d)\n",
	    DEVNAME(sc), cmd->c_opcode, cmd->c_arg, cmd->c_data,
	    cmd->c_datalen, cmd->c_flags, cmd->c_error));

	if (cmd->c_error || sdmmcdebug < 1)
		return;

	aprint_normal_dev(sc->sc_dev, "resp=");
	if (ISSET(cmd->c_flags, SCF_RSP_136))
		for (i = 0; i < sizeof cmd->c_resp; i++)
			aprint_normal("%02x ", ((uint8_t *)cmd->c_resp)[i]);
	else if (ISSET(cmd->c_flags, SCF_RSP_PRESENT))
		for (i = 0; i < 4; i++)
			aprint_normal("%02x ", ((uint8_t *)cmd->c_resp)[i]);
	else
		aprint_normal("none");
	aprint_normal("\n");
}

void
sdmmc_dump_data(const char *title, void *ptr, size_t size)
{
	char buf[16];
	uint8_t *p = ptr;
	int i, j;

	printf("sdmmc_dump_data: %s\n", title ? title : "");
	printf("--------+--------------------------------------------------+------------------+\n");
	printf("offset  | +0 +1 +2 +3 +4 +5 +6 +7  +8 +9 +a +b +c +d +e +f | data             |\n");
	printf("--------+--------------------------------------------------+------------------+\n");
	for (i = 0; i < (int)size; i++) {
		if ((i % 16) == 0) {
			printf("%08x| ", i);
		} else if ((i % 16) == 8) {
			printf(" ");
		}

		printf("%02x ", p[i]);
		buf[i % 16] = p[i];

		if ((i % 16) == 15) {
			printf("| ");
			for (j = 0; j < 16; j++) {
				if (buf[j] >= 0x20 && buf[j] <= 0x7e) {
					printf("%c", buf[j]);
				} else {
					printf(".");
				}
			}
			printf(" |\n");
		}
	}
	if ((i % 16) != 0) {
		j = (i % 16);
		for (; j < 16; j++) {
			printf("   ");
			if ((j % 16) == 8) {
				printf(" ");
			}
		}

		printf("| ");
		for (j = 0; j < (i % 16); j++) {
			if (buf[j] >= 0x20 && buf[j] <= 0x7e) {
				printf("%c", buf[j]);
			} else {
				printf(".");
			}
		}
		for (; j < 16; j++) {
			printf(" ");
		}
		printf(" |\n");
	}
	printf("--------+--------------------------------------------------+------------------+\n");
}
#endif
