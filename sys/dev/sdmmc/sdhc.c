/*	$NetBSD: sdhc.c,v 1.88 2015/10/06 14:32:51 mlelstv Exp $	*/
/*	$OpenBSD: sdhc.c,v 1.25 2009/01/13 19:44:20 grange Exp $	*/

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

/*
 * SD Host Controller driver based on the SD Host Controller Standard
 * Simplified Specification Version 1.00 (www.sdcard.com).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sdhc.c,v 1.88 2015/10/06 14:32:51 mlelstv Exp $");

#ifdef _KERNEL_OPT
#include "opt_sdmmc.h"
#endif

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/atomic.h>

#include <dev/sdmmc/sdhcreg.h>
#include <dev/sdmmc/sdhcvar.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

#ifdef SDHC_DEBUG
int sdhcdebug = 1;
#define DPRINTF(n,s)	do { if ((n) <= sdhcdebug) printf s; } while (0)
void	sdhc_dump_regs(struct sdhc_host *);
#else
#define DPRINTF(n,s)	do {} while (0)
#endif

#define SDHC_COMMAND_TIMEOUT	hz
#define SDHC_BUFFER_TIMEOUT	hz
#define SDHC_TRANSFER_TIMEOUT	hz
#define SDHC_DMA_TIMEOUT	(hz*3)
#define SDHC_TUNING_TIMEOUT	hz

struct sdhc_host {
	struct sdhc_softc *sc;		/* host controller device */

	bus_space_tag_t iot;		/* host register set tag */
	bus_space_handle_t ioh;		/* host register set handle */
	bus_size_t ios;			/* host register space size */
	bus_dma_tag_t dmat;		/* host DMA tag */

	device_t sdmmc;			/* generic SD/MMC device */

	u_int clkbase;			/* base clock frequency in KHz */
	int maxblklen;			/* maximum block length */
	uint32_t ocr;			/* OCR value from capabilities */

	uint8_t regs[14];		/* host controller state */

	uint16_t intr_status;		/* soft interrupt status */
	uint16_t intr_error_status;	/* soft error status */
	kmutex_t intr_lock;
	kcondvar_t intr_cv;

	callout_t tuning_timer;
	int tuning_timing;
	u_int tuning_timer_count;
	u_int tuning_timer_pending;

	int specver;			/* spec. version */

	uint32_t flags;			/* flags for this host */
#define SHF_USE_DMA		0x0001
#define SHF_USE_4BIT_MODE	0x0002
#define SHF_USE_8BIT_MODE	0x0004
#define SHF_MODE_DMAEN		0x0008 /* needs SDHC_DMA_ENABLE in mode */
#define SHF_USE_ADMA2_32	0x0010
#define SHF_USE_ADMA2_64	0x0020
#define SHF_USE_ADMA2_MASK	0x0030

	bus_dmamap_t		adma_map;
	bus_dma_segment_t	adma_segs[1];
	void			*adma2;
};

#define HDEVNAME(hp)	(device_xname((hp)->sc->sc_dev))

static uint8_t
hread1(struct sdhc_host *hp, bus_size_t reg)
{

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS))
		return bus_space_read_1(hp->iot, hp->ioh, reg);
	return bus_space_read_4(hp->iot, hp->ioh, reg & -4) >> (8 * (reg & 3));
}

static uint16_t
hread2(struct sdhc_host *hp, bus_size_t reg)
{

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS))
		return bus_space_read_2(hp->iot, hp->ioh, reg);
	return bus_space_read_4(hp->iot, hp->ioh, reg & -4) >> (8 * (reg & 2));
}

#define HREAD1(hp, reg)		hread1(hp, reg)
#define HREAD2(hp, reg)		hread2(hp, reg)
#define HREAD4(hp, reg)		\
	(bus_space_read_4((hp)->iot, (hp)->ioh, (reg)))


static void
hwrite1(struct sdhc_host *hp, bus_size_t o, uint8_t val)
{

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
		bus_space_write_1(hp->iot, hp->ioh, o, val);
	} else {
		const size_t shift = 8 * (o & 3);
		o &= -4;
		uint32_t tmp = bus_space_read_4(hp->iot, hp->ioh, o);
		tmp = (val << shift) | (tmp & ~(0xff << shift));
		bus_space_write_4(hp->iot, hp->ioh, o, tmp);
	}
}

static void
hwrite2(struct sdhc_host *hp, bus_size_t o, uint16_t val)
{

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
		bus_space_write_2(hp->iot, hp->ioh, o, val);
	} else {
		const size_t shift = 8 * (o & 2);
		o &= -4;
		uint32_t tmp = bus_space_read_4(hp->iot, hp->ioh, o);
		tmp = (val << shift) | (tmp & ~(0xffff << shift));
		bus_space_write_4(hp->iot, hp->ioh, o, tmp);
	}
}

#define HWRITE1(hp, reg, val)		hwrite1(hp, reg, val)
#define HWRITE2(hp, reg, val)		hwrite2(hp, reg, val)
#define HWRITE4(hp, reg, val)						\
	bus_space_write_4((hp)->iot, (hp)->ioh, (reg), (val))

#define HCLR1(hp, reg, bits)						\
	do if (bits) HWRITE1((hp), (reg), HREAD1((hp), (reg)) & ~(bits)); while (0)
#define HCLR2(hp, reg, bits)						\
	do if (bits) HWRITE2((hp), (reg), HREAD2((hp), (reg)) & ~(bits)); while (0)
#define HCLR4(hp, reg, bits)						\
	do if (bits) HWRITE4((hp), (reg), HREAD4((hp), (reg)) & ~(bits)); while (0)
#define HSET1(hp, reg, bits)						\
	do if (bits) HWRITE1((hp), (reg), HREAD1((hp), (reg)) | (bits)); while (0)
#define HSET2(hp, reg, bits)						\
	do if (bits) HWRITE2((hp), (reg), HREAD2((hp), (reg)) | (bits)); while (0)
#define HSET4(hp, reg, bits)						\
	do if (bits) HWRITE4((hp), (reg), HREAD4((hp), (reg)) | (bits)); while (0)

static int	sdhc_host_reset(sdmmc_chipset_handle_t);
static int	sdhc_host_reset1(sdmmc_chipset_handle_t);
static uint32_t	sdhc_host_ocr(sdmmc_chipset_handle_t);
static int	sdhc_host_maxblklen(sdmmc_chipset_handle_t);
static int	sdhc_card_detect(sdmmc_chipset_handle_t);
static int	sdhc_write_protect(sdmmc_chipset_handle_t);
static int	sdhc_bus_power(sdmmc_chipset_handle_t, uint32_t);
static int	sdhc_bus_clock_ddr(sdmmc_chipset_handle_t, int, bool);
static int	sdhc_bus_width(sdmmc_chipset_handle_t, int);
static int	sdhc_bus_rod(sdmmc_chipset_handle_t, int);
static void	sdhc_card_enable_intr(sdmmc_chipset_handle_t, int);
static void	sdhc_card_intr_ack(sdmmc_chipset_handle_t);
static void	sdhc_exec_command(sdmmc_chipset_handle_t,
		    struct sdmmc_command *);
static int	sdhc_signal_voltage(sdmmc_chipset_handle_t, int);
static int	sdhc_execute_tuning1(struct sdhc_host *, int);
static int	sdhc_execute_tuning(sdmmc_chipset_handle_t, int);
static void	sdhc_tuning_timer(void *);
static int	sdhc_start_command(struct sdhc_host *, struct sdmmc_command *);
static int	sdhc_wait_state(struct sdhc_host *, uint32_t, uint32_t);
static int	sdhc_soft_reset(struct sdhc_host *, int);
static int	sdhc_wait_intr(struct sdhc_host *, int, int, bool);
static void	sdhc_transfer_data(struct sdhc_host *, struct sdmmc_command *);
static int	sdhc_transfer_data_dma(struct sdhc_host *, struct sdmmc_command *);
static int	sdhc_transfer_data_pio(struct sdhc_host *, struct sdmmc_command *);
static void	sdhc_read_data_pio(struct sdhc_host *, uint8_t *, u_int);
static void	sdhc_write_data_pio(struct sdhc_host *, uint8_t *, u_int);
static void	esdhc_read_data_pio(struct sdhc_host *, uint8_t *, u_int);
static void	esdhc_write_data_pio(struct sdhc_host *, uint8_t *, u_int);

static struct sdmmc_chip_functions sdhc_functions = {
	/* host controller reset */
	.host_reset = sdhc_host_reset,

	/* host controller capabilities */
	.host_ocr = sdhc_host_ocr,
	.host_maxblklen = sdhc_host_maxblklen,

	/* card detection */
	.card_detect = sdhc_card_detect,

	/* write protect */
	.write_protect = sdhc_write_protect,

	/* bus power, clock frequency, width and ROD(OpenDrain/PushPull) */
	.bus_power = sdhc_bus_power,
	.bus_clock = NULL,	/* see sdhc_bus_clock_ddr */
	.bus_width = sdhc_bus_width,
	.bus_rod = sdhc_bus_rod,

	/* command execution */
	.exec_command = sdhc_exec_command,

	/* card interrupt */
	.card_enable_intr = sdhc_card_enable_intr,
	.card_intr_ack = sdhc_card_intr_ack,

	/* UHS functions */
	.signal_voltage = sdhc_signal_voltage,
	.bus_clock_ddr = sdhc_bus_clock_ddr,
	.execute_tuning = sdhc_execute_tuning,
};

static int
sdhc_cfprint(void *aux, const char *pnp)
{
	const struct sdmmcbus_attach_args * const saa = aux;
	const struct sdhc_host * const hp = saa->saa_sch;

	if (pnp) {
		aprint_normal("sdmmc at %s", pnp);
	}
	for (size_t host = 0; host < hp->sc->sc_nhosts; host++) {
		if (hp->sc->sc_host[host] == hp) {
			aprint_normal(" slot %zu", host);
		}
	}

	return UNCONF;
}

/*
 * Called by attachment driver.  For each SD card slot there is one SD
 * host controller standard register set. (1.3)
 */
int
sdhc_host_found(struct sdhc_softc *sc, bus_space_tag_t iot,
    bus_space_handle_t ioh, bus_size_t iosize)
{
	struct sdmmcbus_attach_args saa;
	struct sdhc_host *hp;
	uint32_t caps, caps2;
	uint16_t sdhcver;
	int error;

	/* Allocate one more host structure. */
	hp = malloc(sizeof(struct sdhc_host), M_DEVBUF, M_WAITOK|M_ZERO);
	if (hp == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't alloc memory (sdhc host)\n");
		goto err1;
	}
	sc->sc_host[sc->sc_nhosts++] = hp;

	/* Fill in the new host structure. */
	hp->sc = sc;
	hp->iot = iot;
	hp->ioh = ioh;
	hp->ios = iosize;
	hp->dmat = sc->sc_dmat;

	mutex_init(&hp->intr_lock, MUTEX_DEFAULT, IPL_SDMMC);
	cv_init(&hp->intr_cv, "sdhcintr");
	callout_init(&hp->tuning_timer, CALLOUT_MPSAFE);
	callout_setfunc(&hp->tuning_timer, sdhc_tuning_timer, hp);

	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		sdhcver = HREAD4(hp, SDHC_ESDHC_HOST_CTL_VERSION);
	} else {
		sdhcver = HREAD2(hp, SDHC_HOST_CTL_VERSION);
	}
	aprint_normal_dev(sc->sc_dev, "SDHC ");
	hp->specver = SDHC_SPEC_VERSION(sdhcver);
	switch (SDHC_SPEC_VERSION(sdhcver)) {
	case SDHC_SPEC_VERS_100:
		aprint_normal("1.0");
		break;

	case SDHC_SPEC_VERS_200:
		aprint_normal("2.0");
		break;

	case SDHC_SPEC_VERS_300:
		aprint_normal("3.0");
		break;

	case SDHC_SPEC_VERS_400:
		aprint_normal("4.0");
		break;

	default:
		aprint_normal("unknown version(0x%x)",
		    SDHC_SPEC_VERSION(sdhcver));
		break;
	}
	aprint_normal(", rev %u", SDHC_VENDOR_VERSION(sdhcver));

	/*
	 * Reset the host controller and enable interrupts.
	 */
	(void)sdhc_host_reset(hp);

	/* Determine host capabilities. */
	if (ISSET(sc->sc_flags, SDHC_FLAG_HOSTCAPS)) {
		caps = sc->sc_caps;
		caps2 = sc->sc_caps2;
	} else {
		caps = sc->sc_caps = HREAD4(hp, SDHC_CAPABILITIES);
		if (hp->specver >= SDHC_SPEC_VERS_300) {
			caps2 = sc->sc_caps2 = HREAD4(hp, SDHC_CAPABILITIES2);
		} else {
			caps2 = sc->sc_caps2 = 0;
		}
	}

	const u_int retuning_mode = (caps2 >> SDHC_RETUNING_MODES_SHIFT) &
	    SDHC_RETUNING_MODES_MASK;
	if (retuning_mode == SDHC_RETUNING_MODE_1) {
		hp->tuning_timer_count = (caps2 >> SDHC_TIMER_COUNT_SHIFT) &
		    SDHC_TIMER_COUNT_MASK;
		if (hp->tuning_timer_count == 0xf)
			hp->tuning_timer_count = 0;
		if (hp->tuning_timer_count)
			hp->tuning_timer_count =
			    1 << (hp->tuning_timer_count - 1);
	}

	/*
	 * Use DMA if the host system and the controller support it.
	 * Suports integrated or external DMA egine, with or without
	 * SDHC_DMA_ENABLE in the command.
	 */
	if (ISSET(sc->sc_flags, SDHC_FLAG_FORCE_DMA) ||
	    (ISSET(sc->sc_flags, SDHC_FLAG_USE_DMA &&
	     ISSET(caps, SDHC_DMA_SUPPORT)))) {
		SET(hp->flags, SHF_USE_DMA);

		if (ISSET(sc->sc_flags, SDHC_FLAG_USE_ADMA2) &&
		    ISSET(caps, SDHC_ADMA2_SUPP)) {
			SET(hp->flags, SHF_MODE_DMAEN);
			/*
			 * 64-bit mode was present in the 2.00 spec, removed
			 * from 3.00, and re-added in 4.00 with a different
			 * descriptor layout. We only support 2.00 and 3.00
			 * descriptors for now.
			 */
			if (hp->specver == SDHC_SPEC_VERS_200 &&
			    ISSET(caps, SDHC_64BIT_SYS_BUS)) {
				SET(hp->flags, SHF_USE_ADMA2_64);
				aprint_normal(", 64-bit ADMA2");
			} else {
				SET(hp->flags, SHF_USE_ADMA2_32);
				aprint_normal(", 32-bit ADMA2");
			}
		} else {
			if (!ISSET(sc->sc_flags, SDHC_FLAG_EXTERNAL_DMA) ||
			    ISSET(sc->sc_flags, SDHC_FLAG_EXTDMA_DMAEN))
				SET(hp->flags, SHF_MODE_DMAEN);
			if (sc->sc_vendor_transfer_data_dma) {
				aprint_normal(", platform DMA");
			} else {
				aprint_normal(", SDMA");
			}
		}
	} else {
		aprint_normal(", PIO");
	}

	/*
	 * Determine the base clock frequency. (2.2.24)
	 */
	if (hp->specver >= SDHC_SPEC_VERS_300) {
		hp->clkbase = SDHC_BASE_V3_FREQ_KHZ(caps);
	} else {
		hp->clkbase = SDHC_BASE_FREQ_KHZ(caps);
	}
	if (hp->clkbase == 0 ||
	    ISSET(sc->sc_flags, SDHC_FLAG_NO_CLKBASE)) {
		if (sc->sc_clkbase == 0) {
			/* The attachment driver must tell us. */
			aprint_error_dev(sc->sc_dev,
			    "unknown base clock frequency\n");
			goto err;
		}
		hp->clkbase = sc->sc_clkbase;
	}
	if (hp->clkbase < 10000 || hp->clkbase > 10000 * 256) {
		/* SDHC 1.0 supports only 10-63 MHz. */
		aprint_error_dev(sc->sc_dev,
		    "base clock frequency out of range: %u MHz\n",
		    hp->clkbase / 1000);
		goto err;
	}
	aprint_normal(", %u kHz", hp->clkbase);

	/*
	 * XXX Set the data timeout counter value according to
	 * capabilities. (2.2.15)
	 */
	HWRITE1(hp, SDHC_TIMEOUT_CTL, SDHC_TIMEOUT_MAX);
#if 1
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED))
		HWRITE4(hp, SDHC_NINTR_STATUS, SDHC_CMD_TIMEOUT_ERROR << 16);
#endif

	if (ISSET(caps, SDHC_EMBEDDED_SLOT))
		aprint_normal(", embedded slot");

	/*
	 * Determine SD bus voltage levels supported by the controller.
	 */
	aprint_normal(",");
	if (ISSET(caps, SDHC_HIGH_SPEED_SUPP)) {
		SET(hp->ocr, MMC_OCR_HCS);
		aprint_normal(" HS");
	}
	if (ISSET(caps2, SDHC_SDR50_SUPP)) {
		SET(hp->ocr, MMC_OCR_S18A);
		aprint_normal(" SDR50");
	}
	if (ISSET(caps2, SDHC_DDR50_SUPP)) {
		SET(hp->ocr, MMC_OCR_S18A);
		aprint_normal(" DDR50");
	}
	if (ISSET(caps2, SDHC_SDR104_SUPP)) {
		SET(hp->ocr, MMC_OCR_S18A);
		aprint_normal(" SDR104 HS200");
	}
	if (ISSET(caps, SDHC_VOLTAGE_SUPP_1_8V)) {
		SET(hp->ocr, MMC_OCR_1_7V_1_8V | MMC_OCR_1_8V_1_9V);
		aprint_normal(" 1.8V");
	}
	if (ISSET(caps, SDHC_VOLTAGE_SUPP_3_0V)) {
		SET(hp->ocr, MMC_OCR_2_9V_3_0V | MMC_OCR_3_0V_3_1V);
		aprint_normal(" 3.0V");
	}
	if (ISSET(caps, SDHC_VOLTAGE_SUPP_3_3V)) {
		SET(hp->ocr, MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V);
		aprint_normal(" 3.3V");
	}
	if (hp->specver >= SDHC_SPEC_VERS_300) {
		aprint_normal(", re-tuning mode %d", retuning_mode + 1);
		if (hp->tuning_timer_count)
			aprint_normal(" (%us timer)", hp->tuning_timer_count);
	}

	/*
	 * Determine the maximum block length supported by the host
	 * controller. (2.2.24)
	 */
	switch((caps >> SDHC_MAX_BLK_LEN_SHIFT) & SDHC_MAX_BLK_LEN_MASK) {
	case SDHC_MAX_BLK_LEN_512:
		hp->maxblklen = 512;
		break;

	case SDHC_MAX_BLK_LEN_1024:
		hp->maxblklen = 1024;
		break;

	case SDHC_MAX_BLK_LEN_2048:
		hp->maxblklen = 2048;
		break;

	case SDHC_MAX_BLK_LEN_4096:
		hp->maxblklen = 4096;
		break;

	default:
		aprint_error_dev(sc->sc_dev, "max block length unknown\n");
		goto err;
	}
	aprint_normal(", %u byte blocks", hp->maxblklen);
	aprint_normal("\n");

	if (ISSET(hp->flags, SHF_USE_ADMA2_MASK)) {
		int rseg;

		/* Allocate ADMA2 descriptor memory */
		error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE,
		    PAGE_SIZE, hp->adma_segs, 1, &rseg, BUS_DMA_WAITOK);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "ADMA2 dmamem_alloc failed (%d)\n", error);
			goto adma_done;
		}
		error = bus_dmamem_map(sc->sc_dmat, hp->adma_segs, rseg,
		    PAGE_SIZE, (void **)&hp->adma2, BUS_DMA_WAITOK);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "ADMA2 dmamem_map failed (%d)\n", error);
			goto adma_done;
		}
		error = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE,
		    0, BUS_DMA_WAITOK, &hp->adma_map);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "ADMA2 dmamap_create failed (%d)\n", error);
			goto adma_done;
		}
		error = bus_dmamap_load(sc->sc_dmat, hp->adma_map,
		    hp->adma2, PAGE_SIZE, NULL,
		    BUS_DMA_WAITOK|BUS_DMA_WRITE);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "ADMA2 dmamap_load failed (%d)\n", error);
			goto adma_done;
		}

		memset(hp->adma2, 0, PAGE_SIZE);

adma_done:
		if (error)
			CLR(hp->flags, SHF_USE_ADMA2_MASK);
	}

	/*
	 * Attach the generic SD/MMC bus driver.  (The bus driver must
	 * not invoke any chipset functions before it is attached.)
	 */
	memset(&saa, 0, sizeof(saa));
	saa.saa_busname = "sdmmc";
	saa.saa_sct = &sdhc_functions;
	saa.saa_sch = hp;
	saa.saa_dmat = hp->dmat;
	saa.saa_clkmax = hp->clkbase;
	if (ISSET(sc->sc_flags, SDHC_FLAG_HAVE_CGM))
		saa.saa_clkmin = hp->clkbase / 256 / 2046;
	else if (ISSET(sc->sc_flags, SDHC_FLAG_HAVE_DVS))
		saa.saa_clkmin = hp->clkbase / 256 / 16;
	else if (hp->sc->sc_clkmsk != 0)
		saa.saa_clkmin = hp->clkbase / (hp->sc->sc_clkmsk >>
		    (ffs(hp->sc->sc_clkmsk) - 1));
	else if (hp->specver >= SDHC_SPEC_VERS_300)
		saa.saa_clkmin = hp->clkbase / 0x3ff;
	else
		saa.saa_clkmin = hp->clkbase / 256;
	saa.saa_caps = SMC_CAPS_4BIT_MODE|SMC_CAPS_AUTO_STOP;
	if (ISSET(sc->sc_flags, SDHC_FLAG_8BIT_MODE))
		saa.saa_caps |= SMC_CAPS_8BIT_MODE;
	if (ISSET(caps, SDHC_HIGH_SPEED_SUPP))
		saa.saa_caps |= SMC_CAPS_SD_HIGHSPEED;
	if (ISSET(caps2, SDHC_SDR104_SUPP))
		saa.saa_caps |= SMC_CAPS_UHS_SDR104 |
				SMC_CAPS_UHS_SDR50 |
				SMC_CAPS_MMC_HS200;
	if (ISSET(caps2, SDHC_SDR50_SUPP))
		saa.saa_caps |= SMC_CAPS_UHS_SDR50;
	if (ISSET(caps2, SDHC_DDR50_SUPP))
		saa.saa_caps |= SMC_CAPS_UHS_DDR50;
	if (ISSET(hp->flags, SHF_USE_DMA)) {
		saa.saa_caps |= SMC_CAPS_DMA;
		if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED))
			saa.saa_caps |= SMC_CAPS_MULTI_SEG_DMA;
	}
	if (ISSET(sc->sc_flags, SDHC_FLAG_SINGLE_ONLY))
		saa.saa_caps |= SMC_CAPS_SINGLE_ONLY;
	if (ISSET(sc->sc_flags, SDHC_FLAG_POLL_CARD_DET))
		saa.saa_caps |= SMC_CAPS_POLL_CARD_DET;
	hp->sdmmc = config_found(sc->sc_dev, &saa, sdhc_cfprint);

	return 0;

err:
	callout_destroy(&hp->tuning_timer);
	cv_destroy(&hp->intr_cv);
	mutex_destroy(&hp->intr_lock);
	free(hp, M_DEVBUF);
	sc->sc_host[--sc->sc_nhosts] = NULL;
err1:
	return 1;
}

int
sdhc_detach(struct sdhc_softc *sc, int flags)
{
	struct sdhc_host *hp;
	int rv = 0;

	for (size_t n = 0; n < sc->sc_nhosts; n++) {
		hp = sc->sc_host[n];
		if (hp == NULL)
			continue;
		if (hp->sdmmc != NULL) {
			rv = config_detach(hp->sdmmc, flags);
			if (rv)
				break;
			hp->sdmmc = NULL;
		}
		/* disable interrupts */
		if ((flags & DETACH_FORCE) == 0) {
			mutex_enter(&hp->intr_lock);
			if (ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
				HWRITE4(hp, SDHC_NINTR_SIGNAL_EN, 0);
			} else {
				HWRITE2(hp, SDHC_NINTR_SIGNAL_EN, 0);
			}
			sdhc_soft_reset(hp, SDHC_RESET_ALL);
			mutex_exit(&hp->intr_lock);
		}
		callout_halt(&hp->tuning_timer, NULL);
		callout_destroy(&hp->tuning_timer);
		cv_destroy(&hp->intr_cv);
		mutex_destroy(&hp->intr_lock);
		if (hp->ios > 0) {
			bus_space_unmap(hp->iot, hp->ioh, hp->ios);
			hp->ios = 0;
		}
		if (ISSET(hp->flags, SHF_USE_ADMA2_MASK)) {
			bus_dmamap_unload(sc->sc_dmat, hp->adma_map);
			bus_dmamap_destroy(sc->sc_dmat, hp->adma_map);
			bus_dmamem_unmap(sc->sc_dmat, hp->adma2, PAGE_SIZE);
			bus_dmamem_free(sc->sc_dmat, hp->adma_segs, 1);
		}
		free(hp, M_DEVBUF);
		sc->sc_host[n] = NULL;
	}

	return rv;
}

bool
sdhc_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct sdhc_softc *sc = device_private(dev);
	struct sdhc_host *hp;
	size_t i;

	/* XXX poll for command completion or suspend command
	 * in progress */

	/* Save the host controller state. */
	for (size_t n = 0; n < sc->sc_nhosts; n++) {
		hp = sc->sc_host[n];
		if (ISSET(sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
			for (i = 0; i < sizeof hp->regs; i += 4) {
				uint32_t v = HREAD4(hp, i);
				hp->regs[i + 0] = (v >> 0);
				hp->regs[i + 1] = (v >> 8);
				if (i + 3 < sizeof hp->regs) {
					hp->regs[i + 2] = (v >> 16);
					hp->regs[i + 3] = (v >> 24);
				}
			}
		} else {
			for (i = 0; i < sizeof hp->regs; i++) {
				hp->regs[i] = HREAD1(hp, i);
			}
		}
	}
	return true;
}

bool
sdhc_resume(device_t dev, const pmf_qual_t *qual)
{
	struct sdhc_softc *sc = device_private(dev);
	struct sdhc_host *hp;
	size_t i;

	/* Restore the host controller state. */
	for (size_t n = 0; n < sc->sc_nhosts; n++) {
		hp = sc->sc_host[n];
		(void)sdhc_host_reset(hp);
		if (ISSET(sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
			for (i = 0; i < sizeof hp->regs; i += 4) {
				if (i + 3 < sizeof hp->regs) {
					HWRITE4(hp, i,
					    (hp->regs[i + 0] << 0)
					    | (hp->regs[i + 1] << 8)
					    | (hp->regs[i + 2] << 16)
					    | (hp->regs[i + 3] << 24));
				} else {
					HWRITE4(hp, i,
					    (hp->regs[i + 0] << 0)
					    | (hp->regs[i + 1] << 8));
				}
			}
		} else {
			for (i = 0; i < sizeof hp->regs; i++) {
				HWRITE1(hp, i, hp->regs[i]);
			}
		}
	}
	return true;
}

bool
sdhc_shutdown(device_t dev, int flags)
{
	struct sdhc_softc *sc = device_private(dev);
	struct sdhc_host *hp;

	/* XXX chip locks up if we don't disable it before reboot. */
	for (size_t i = 0; i < sc->sc_nhosts; i++) {
		hp = sc->sc_host[i];
		(void)sdhc_host_reset(hp);
	}
	return true;
}

/*
 * Reset the host controller.  Called during initialization, when
 * cards are removed, upon resume, and during error recovery.
 */
static int
sdhc_host_reset1(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	uint32_t sdhcimask;
	int error;

	KASSERT(mutex_owned(&hp->intr_lock));

	/* Disable all interrupts. */
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
		HWRITE4(hp, SDHC_NINTR_SIGNAL_EN, 0);
	} else {
		HWRITE2(hp, SDHC_NINTR_SIGNAL_EN, 0);
	}

	/*
	 * Reset the entire host controller and wait up to 100ms for
	 * the controller to clear the reset bit.
	 */
	error = sdhc_soft_reset(hp, SDHC_RESET_ALL);
	if (error)
		goto out;

	/* Set data timeout counter value to max for now. */
	HWRITE1(hp, SDHC_TIMEOUT_CTL, SDHC_TIMEOUT_MAX);
#if 1
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED))
		HWRITE4(hp, SDHC_NINTR_STATUS, SDHC_CMD_TIMEOUT_ERROR << 16);
#endif

	/* Enable interrupts. */
	sdhcimask = SDHC_CARD_REMOVAL | SDHC_CARD_INSERTION |
	    SDHC_BUFFER_READ_READY | SDHC_BUFFER_WRITE_READY |
	    SDHC_DMA_INTERRUPT | SDHC_BLOCK_GAP_EVENT |
	    SDHC_TRANSFER_COMPLETE | SDHC_COMMAND_COMPLETE;
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
		sdhcimask |= SDHC_EINTR_STATUS_MASK << 16;
		HWRITE4(hp, SDHC_NINTR_STATUS_EN, sdhcimask);
		sdhcimask ^=
		    (SDHC_EINTR_STATUS_MASK ^ SDHC_EINTR_SIGNAL_MASK) << 16;
		sdhcimask ^= SDHC_BUFFER_READ_READY ^ SDHC_BUFFER_WRITE_READY;
		HWRITE4(hp, SDHC_NINTR_SIGNAL_EN, sdhcimask);
	} else {
		HWRITE2(hp, SDHC_NINTR_STATUS_EN, sdhcimask);
		HWRITE2(hp, SDHC_EINTR_STATUS_EN, SDHC_EINTR_STATUS_MASK);
		sdhcimask ^= SDHC_BUFFER_READ_READY ^ SDHC_BUFFER_WRITE_READY;
		HWRITE2(hp, SDHC_NINTR_SIGNAL_EN, sdhcimask);
		HWRITE2(hp, SDHC_EINTR_SIGNAL_EN, SDHC_EINTR_SIGNAL_MASK);
	}

out:
	return error;
}

static int
sdhc_host_reset(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	int error;

	mutex_enter(&hp->intr_lock);
	error = sdhc_host_reset1(sch);
	mutex_exit(&hp->intr_lock);

	return error;
}

static uint32_t
sdhc_host_ocr(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;

	return hp->ocr;
}

static int
sdhc_host_maxblklen(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;

	return hp->maxblklen;
}

/*
 * Return non-zero if the card is currently inserted.
 */
static int
sdhc_card_detect(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	int r;

	if (hp->sc->sc_vendor_card_detect)
		return (*hp->sc->sc_vendor_card_detect)(hp->sc);

	r = ISSET(HREAD4(hp, SDHC_PRESENT_STATE), SDHC_CARD_INSERTED);

	return r ? 1 : 0;
}

/*
 * Return non-zero if the card is currently write-protected.
 */
static int
sdhc_write_protect(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	int r;

	if (hp->sc->sc_vendor_write_protect)
		return (*hp->sc->sc_vendor_write_protect)(hp->sc);

	r = ISSET(HREAD4(hp, SDHC_PRESENT_STATE), SDHC_WRITE_PROTECT_SWITCH);

	return r ? 0 : 1;
}

/*
 * Set or change SD bus voltage and enable or disable SD bus power.
 * Return zero on success.
 */
static int
sdhc_bus_power(sdmmc_chipset_handle_t sch, uint32_t ocr)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	uint8_t vdd;
	int error = 0;
	const uint32_t pcmask =
	    ~(SDHC_BUS_POWER | (SDHC_VOLTAGE_MASK << SDHC_VOLTAGE_SHIFT));

	mutex_enter(&hp->intr_lock);

	/*
	 * Disable bus power before voltage change.
	 */
	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)
	    && !ISSET(hp->sc->sc_flags, SDHC_FLAG_NO_PWR0))
		HWRITE1(hp, SDHC_POWER_CTL, 0);

	/* If power is disabled, reset the host and return now. */
	if (ocr == 0) {
		(void)sdhc_host_reset1(hp);
		callout_halt(&hp->tuning_timer, &hp->intr_lock);
		goto out;
	}

	/*
	 * Select the lowest voltage according to capabilities.
	 */
	ocr &= hp->ocr;
	if (ISSET(ocr, MMC_OCR_1_7V_1_8V|MMC_OCR_1_8V_1_9V)) {
		vdd = SDHC_VOLTAGE_1_8V;
	} else if (ISSET(ocr, MMC_OCR_2_9V_3_0V|MMC_OCR_3_0V_3_1V)) {
		vdd = SDHC_VOLTAGE_3_0V;
	} else if (ISSET(ocr, MMC_OCR_3_2V_3_3V|MMC_OCR_3_3V_3_4V)) {
		vdd = SDHC_VOLTAGE_3_3V;
	} else {
		/* Unsupported voltage level requested. */
		error = EINVAL;
		goto out;
	}

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		/*
		 * Enable bus power.  Wait at least 1 ms (or 74 clocks) plus
		 * voltage ramp until power rises.
		 */

		if (ISSET(hp->sc->sc_flags, SDHC_FLAG_SINGLE_POWER_WRITE)) {
			HWRITE1(hp, SDHC_POWER_CTL,
			    (vdd << SDHC_VOLTAGE_SHIFT) | SDHC_BUS_POWER);
		} else {
			HWRITE1(hp, SDHC_POWER_CTL,
			    HREAD1(hp, SDHC_POWER_CTL) & pcmask);
			sdmmc_delay(1);
			HWRITE1(hp, SDHC_POWER_CTL,
			    (vdd << SDHC_VOLTAGE_SHIFT));
			sdmmc_delay(1);
			HSET1(hp, SDHC_POWER_CTL, SDHC_BUS_POWER);
			sdmmc_delay(10000);
		}

		/*
		 * The host system may not power the bus due to battery low,
		 * etc.  In that case, the host controller should clear the
		 * bus power bit.
		 */
		if (!ISSET(HREAD1(hp, SDHC_POWER_CTL), SDHC_BUS_POWER)) {
			error = ENXIO;
			goto out;
		}
	}

out:
	mutex_exit(&hp->intr_lock);

	return error;
}

/*
 * Return the smallest possible base clock frequency divisor value
 * for the CLOCK_CTL register to produce `freq' (KHz).
 */
static bool
sdhc_clock_divisor(struct sdhc_host *hp, u_int freq, u_int *divp)
{
	u_int div;

	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_HAVE_CGM)) {
		for (div = hp->clkbase / freq; div <= 0x3ff; div++) {
			if ((hp->clkbase / div) <= freq) {
				*divp = SDHC_SDCLK_CGM
				    | ((div & 0x300) << SDHC_SDCLK_XDIV_SHIFT)
				    | ((div & 0x0ff) << SDHC_SDCLK_DIV_SHIFT);
				//freq = hp->clkbase / div;
				return true;
			}
		}
		/* No divisor found. */
		return false;
	}
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_HAVE_DVS)) {
		u_int dvs = (hp->clkbase + freq - 1) / freq;
		u_int roundup = dvs & 1;
		for (dvs >>= 1, div = 1; div <= 256; div <<= 1, dvs >>= 1) {
			if (dvs + roundup <= 16) {
				dvs += roundup - 1;
				*divp = (div << SDHC_SDCLK_DIV_SHIFT)
				    |   (dvs << SDHC_SDCLK_DVS_SHIFT);
				DPRINTF(2,
				    ("%s: divisor for freq %u is %u * %u\n",
				    HDEVNAME(hp), freq, div * 2, dvs + 1));
				//freq = hp->clkbase / (div * 2) * (dvs + 1);
				return true;
			}
			/*
			 * If we drop bits, we need to round up the divisor.
			 */
			roundup |= dvs & 1;
		}
		/* No divisor found. */
		return false;
	}
	if (hp->sc->sc_clkmsk != 0) {
		div = howmany(hp->clkbase, freq);
		if (div > (hp->sc->sc_clkmsk >> (ffs(hp->sc->sc_clkmsk) - 1)))
			return false;
		*divp = div << (ffs(hp->sc->sc_clkmsk) - 1);
		//freq = hp->clkbase / div;
		return true;
	}
	if (hp->specver >= SDHC_SPEC_VERS_300) {
		div = howmany(hp->clkbase, freq);
		div = div > 1 ? howmany(div, 2) : 0;
		if (div > 0x3ff)
			return false;
		*divp = (((div >> 8) & SDHC_SDCLK_XDIV_MASK)
			 << SDHC_SDCLK_XDIV_SHIFT) |
			(((div >> 0) & SDHC_SDCLK_DIV_MASK)
			 << SDHC_SDCLK_DIV_SHIFT);
		//freq = hp->clkbase / (div ? div * 2 : 1);
		return true;
	} else {
		for (div = 1; div <= 256; div *= 2) {
			if ((hp->clkbase / div) <= freq) {
				*divp = (div / 2) << SDHC_SDCLK_DIV_SHIFT;
				//freq = hp->clkbase / div;
				return true;
			}
		}
		/* No divisor found. */
		return false;
	}
	/* No divisor found. */
	return false;
}

/*
 * Set or change SDCLK frequency or disable the SD clock.
 * Return zero on success.
 */
static int
sdhc_bus_clock_ddr(sdmmc_chipset_handle_t sch, int freq, bool ddr)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	u_int div;
	u_int timo;
	int16_t reg;
	int error = 0;
	bool present __diagused;

	mutex_enter(&hp->intr_lock);

#ifdef DIAGNOSTIC
	present = ISSET(HREAD4(hp, SDHC_PRESENT_STATE), SDHC_CMD_INHIBIT_MASK);

	/* Must not stop the clock if commands are in progress. */
	if (present && sdhc_card_detect(hp)) {
		aprint_normal_dev(hp->sc->sc_dev,
		    "%s: command in progress\n", __func__);
	}
#endif

	if (hp->sc->sc_vendor_bus_clock) {
		error = (*hp->sc->sc_vendor_bus_clock)(hp->sc, freq);
		if (error != 0)
			goto out;
	}

	/*
	 * Stop SD clock before changing the frequency.
	 */
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		HCLR4(hp, SDHC_CLOCK_CTL, 0xfff8);
		if (freq == SDMMC_SDCLK_OFF) {
			HSET4(hp, SDHC_CLOCK_CTL, 0x80f0);
			goto out;
		}
	} else {
		HCLR2(hp, SDHC_CLOCK_CTL, SDHC_SDCLK_ENABLE);
		if (freq == SDMMC_SDCLK_OFF)
			goto out;
	}

	if (hp->specver >= SDHC_SPEC_VERS_300) {
		HCLR2(hp, SDHC_HOST_CTL2, SDHC_UHS_MODE_SELECT_MASK);
		if (freq > 100000) {
			HSET2(hp, SDHC_HOST_CTL2, SDHC_UHS_MODE_SELECT_SDR104);
		} else if (freq > 50000) {
			HSET2(hp, SDHC_HOST_CTL2, SDHC_UHS_MODE_SELECT_SDR50);
		} else if (freq > 25000) {
			if (ddr) {
				HSET2(hp, SDHC_HOST_CTL2,
				    SDHC_UHS_MODE_SELECT_DDR50);
			} else {
				HSET2(hp, SDHC_HOST_CTL2,
				    SDHC_UHS_MODE_SELECT_SDR25);
			}
		} else if (freq > 400) {
			HSET2(hp, SDHC_HOST_CTL2, SDHC_UHS_MODE_SELECT_SDR12);
		}
	}

	/*
	 * Slow down Ricoh 5U823 controller that isn't reliable
	 * at 100MHz bus clock.
	 */
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_SLOW_SDR50)) {
		if (freq == 100000)
			--freq;
	}

	/*
	 * Set the minimum base clock frequency divisor.
	 */
	if (!sdhc_clock_divisor(hp, freq, &div)) {
		/* Invalid base clock frequency or `freq' value. */
		aprint_error_dev(hp->sc->sc_dev,
			"Invalid bus clock %d kHz\n", freq);
		error = EINVAL;
		goto out;
	}
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		HWRITE4(hp, SDHC_CLOCK_CTL,
		    div | (SDHC_TIMEOUT_MAX << 16));
	} else {
		reg = HREAD2(hp, SDHC_CLOCK_CTL);
		reg &= (SDHC_INTCLK_STABLE | SDHC_INTCLK_ENABLE);
		HWRITE2(hp, SDHC_CLOCK_CTL, reg | div);
	}

	/*
	 * Start internal clock.  Wait 10ms for stabilization.
	 */
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		sdmmc_delay(10000);
		HSET4(hp, SDHC_CLOCK_CTL,
		    8 | SDHC_INTCLK_ENABLE | SDHC_INTCLK_STABLE);
	} else {
		HSET2(hp, SDHC_CLOCK_CTL, SDHC_INTCLK_ENABLE);
		for (timo = 1000; timo > 0; timo--) {
			if (ISSET(HREAD2(hp, SDHC_CLOCK_CTL),
			    SDHC_INTCLK_STABLE))
				break;
			sdmmc_delay(10);
		}
		if (timo == 0) {
			error = ETIMEDOUT;
			DPRINTF(1,("%s: timeout\n", __func__));
			goto out;
		}
	}

	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		HSET1(hp, SDHC_SOFTWARE_RESET, SDHC_INIT_ACTIVE);
		/*
		 * Sending 80 clocks at 400kHz takes 200us.
		 * So delay for that time + slop and then
		 * check a few times for completion.
		 */
		sdmmc_delay(210);
		for (timo = 10; timo > 0; timo--) {
			if (!ISSET(HREAD1(hp, SDHC_SOFTWARE_RESET),
			    SDHC_INIT_ACTIVE))
				break;
			sdmmc_delay(10);
		}
		DPRINTF(2,("%s: %u init spins\n", __func__, 10 - timo));

		/*
		 * Enable SD clock.
		 */
		HSET4(hp, SDHC_CLOCK_CTL, SDHC_SDCLK_ENABLE);
	} else {
		/*
		 * Enable SD clock.
		 */
		HSET2(hp, SDHC_CLOCK_CTL, SDHC_SDCLK_ENABLE);

		if (freq > 25000 &&
		    !ISSET(hp->sc->sc_flags, SDHC_FLAG_NO_HS_BIT))
			HSET1(hp, SDHC_HOST_CTL, SDHC_HIGH_SPEED);
		else
			HCLR1(hp, SDHC_HOST_CTL, SDHC_HIGH_SPEED);
	}

out:
	mutex_exit(&hp->intr_lock);

	return error;
}

static int
sdhc_bus_width(sdmmc_chipset_handle_t sch, int width)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	int reg;

	switch (width) {
	case 1:
	case 4:
		break;

	case 8:
		if (ISSET(hp->sc->sc_flags, SDHC_FLAG_8BIT_MODE))
			break;
		/* FALLTHROUGH */
	default:
		DPRINTF(0,("%s: unsupported bus width (%d)\n",
		    HDEVNAME(hp), width));
		return 1;
	}

	mutex_enter(&hp->intr_lock);

	reg = HREAD1(hp, SDHC_HOST_CTL);
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		reg &= ~(SDHC_4BIT_MODE|SDHC_ESDHC_8BIT_MODE);
		if (width == 4)
			reg |= SDHC_4BIT_MODE;
		else if (width == 8)
			reg |= SDHC_ESDHC_8BIT_MODE;
	} else {
		reg &= ~SDHC_4BIT_MODE;
		if (hp->specver >= SDHC_SPEC_VERS_300) {
			reg &= ~SDHC_8BIT_MODE;
		}
		if (width == 4) {
			reg |= SDHC_4BIT_MODE;
		} else if (width == 8 && hp->specver >= SDHC_SPEC_VERS_300) {
			reg |= SDHC_8BIT_MODE;
		}
	}
	HWRITE1(hp, SDHC_HOST_CTL, reg);

	mutex_exit(&hp->intr_lock);

	return 0;
}

static int
sdhc_bus_rod(sdmmc_chipset_handle_t sch, int on)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;

	if (hp->sc->sc_vendor_rod)
		return (*hp->sc->sc_vendor_rod)(hp->sc, on);

	return 0;
}

static void
sdhc_card_enable_intr(sdmmc_chipset_handle_t sch, int enable)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		mutex_enter(&hp->intr_lock);
		if (enable) {
			HSET2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
			HSET2(hp, SDHC_NINTR_SIGNAL_EN, SDHC_CARD_INTERRUPT);
		} else {
			HCLR2(hp, SDHC_NINTR_SIGNAL_EN, SDHC_CARD_INTERRUPT);
			HCLR2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
		}
		mutex_exit(&hp->intr_lock);
	}
}

static void
sdhc_card_intr_ack(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		mutex_enter(&hp->intr_lock);
		HSET2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
		mutex_exit(&hp->intr_lock);
	}
}

static int
sdhc_signal_voltage(sdmmc_chipset_handle_t sch, int signal_voltage)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;

	mutex_enter(&hp->intr_lock);
	switch (signal_voltage) {
	case SDMMC_SIGNAL_VOLTAGE_180:
		HSET2(hp, SDHC_HOST_CTL2, SDHC_1_8V_SIGNAL_EN);
		break;
	case SDMMC_SIGNAL_VOLTAGE_330:
		HCLR2(hp, SDHC_HOST_CTL2, SDHC_1_8V_SIGNAL_EN);
		break;
	default:
		return EINVAL;
	}
	mutex_exit(&hp->intr_lock);

	return 0;
}

/*
 * Sampling clock tuning procedure (UHS)
 */
static int
sdhc_execute_tuning1(struct sdhc_host *hp, int timing)
{
	struct sdmmc_command cmd;
	uint8_t hostctl;
	int opcode, error, retry = 40;

	KASSERT(mutex_owned(&hp->intr_lock));

	hp->tuning_timing = timing;

	switch (timing) {
	case SDMMC_TIMING_MMC_HS200:
		opcode = MMC_SEND_TUNING_BLOCK_HS200;
		break;
	case SDMMC_TIMING_UHS_SDR50:
		if (!ISSET(hp->sc->sc_caps2, SDHC_TUNING_SDR50))
			return 0;
		/* FALLTHROUGH */
	case SDMMC_TIMING_UHS_SDR104:
		opcode = MMC_SEND_TUNING_BLOCK;
		break;
	default:
		return EINVAL;
	}

	hostctl = HREAD1(hp, SDHC_HOST_CTL);

	/* enable buffer read ready interrupt */
	HSET2(hp, SDHC_NINTR_SIGNAL_EN, SDHC_BUFFER_READ_READY);
	HSET2(hp, SDHC_NINTR_STATUS_EN, SDHC_BUFFER_READ_READY);

	/* disable DMA */
	HCLR1(hp, SDHC_HOST_CTL, SDHC_DMA_SELECT);

	/* reset tuning circuit */
	HCLR2(hp, SDHC_HOST_CTL2, SDHC_SAMPLING_CLOCK_SEL);

	/* start of tuning */
	HWRITE2(hp, SDHC_HOST_CTL2, SDHC_EXECUTE_TUNING);

	do {
		memset(&cmd, 0, sizeof(cmd));
		cmd.c_opcode = opcode;
		cmd.c_arg = 0;
		cmd.c_flags = SCF_CMD_ADTC | SCF_CMD_READ | SCF_RSP_R1;
		if (ISSET(hostctl, SDHC_8BIT_MODE)) {
			cmd.c_blklen = cmd.c_datalen = 128;
		} else {
			cmd.c_blklen = cmd.c_datalen = 64;
		}

		error = sdhc_start_command(hp, &cmd);
		if (error)
			break;

		if (!sdhc_wait_intr(hp, SDHC_BUFFER_READ_READY,
		    SDHC_TUNING_TIMEOUT, false)) {
			break;
		}

		delay(1000);
	} while (HREAD2(hp, SDHC_HOST_CTL2) & SDHC_EXECUTE_TUNING && --retry);

	/* disable buffer read ready interrupt */
	HCLR2(hp, SDHC_NINTR_SIGNAL_EN, SDHC_BUFFER_READ_READY);
	HCLR2(hp, SDHC_NINTR_STATUS_EN, SDHC_BUFFER_READ_READY);

	if (HREAD2(hp, SDHC_HOST_CTL2) & SDHC_EXECUTE_TUNING) {
		HCLR2(hp, SDHC_HOST_CTL2,
		    SDHC_SAMPLING_CLOCK_SEL|SDHC_EXECUTE_TUNING);
		sdhc_soft_reset(hp, SDHC_RESET_DAT|SDHC_RESET_CMD);
		aprint_error_dev(hp->sc->sc_dev,
		    "tuning did not complete, using fixed sampling clock\n");
		return EIO;		/* tuning did not complete */
	}

	if ((HREAD2(hp, SDHC_HOST_CTL2) & SDHC_SAMPLING_CLOCK_SEL) == 0) {
		HCLR2(hp, SDHC_HOST_CTL2,
		    SDHC_SAMPLING_CLOCK_SEL|SDHC_EXECUTE_TUNING);
		sdhc_soft_reset(hp, SDHC_RESET_DAT|SDHC_RESET_CMD);
		aprint_error_dev(hp->sc->sc_dev,
		    "tuning failed, using fixed sampling clock\n");
		return EIO;		/* tuning failed */
	}

	if (hp->tuning_timer_count) {
		callout_schedule(&hp->tuning_timer,
		    hz * hp->tuning_timer_count);
	}

	return 0;		/* tuning completed */
}

static int
sdhc_execute_tuning(sdmmc_chipset_handle_t sch, int timing)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	int error;

	mutex_enter(&hp->intr_lock);
	error = sdhc_execute_tuning1(hp, timing);
	mutex_exit(&hp->intr_lock);
	return error;
}

static void
sdhc_tuning_timer(void *arg)
{
	struct sdhc_host *hp = arg;

	atomic_swap_uint(&hp->tuning_timer_pending, 1);
}

static int
sdhc_wait_state(struct sdhc_host *hp, uint32_t mask, uint32_t value)
{
	uint32_t state;
	int timeout;

	for (timeout = 10000; timeout > 0; timeout--) {
		if (((state = HREAD4(hp, SDHC_PRESENT_STATE)) & mask) == value)
			return 0;
		sdmmc_delay(10);
	}
	aprint_error_dev(hp->sc->sc_dev, "timeout waiting for mask %#x value %#x (state=%#x)\n",
	    mask, value, state);
	return ETIMEDOUT;
}

static void
sdhc_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct sdhc_host *hp = (struct sdhc_host *)sch;
	int error;
	bool probing;

	mutex_enter(&hp->intr_lock);

	if (atomic_cas_uint(&hp->tuning_timer_pending, 1, 0) == 1) {
		(void)sdhc_execute_tuning1(hp, hp->tuning_timing);
	}

	if (cmd->c_data && ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		const uint16_t ready = SDHC_BUFFER_READ_READY | SDHC_BUFFER_WRITE_READY;
		if (ISSET(hp->flags, SHF_USE_DMA)) {
			HCLR2(hp, SDHC_NINTR_SIGNAL_EN, ready);
			HCLR2(hp, SDHC_NINTR_STATUS_EN, ready);
		} else {
			HSET2(hp, SDHC_NINTR_SIGNAL_EN, ready);
			HSET2(hp, SDHC_NINTR_STATUS_EN, ready);
		}
	}

	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_NO_TIMEOUT)) {
		const uint16_t eintr = SDHC_CMD_TIMEOUT_ERROR;
		if (cmd->c_data != NULL) {
			HCLR2(hp, SDHC_EINTR_SIGNAL_EN, eintr);
			HCLR2(hp, SDHC_EINTR_STATUS_EN, eintr);
		} else {
			HSET2(hp, SDHC_EINTR_SIGNAL_EN, eintr);
			HSET2(hp, SDHC_EINTR_STATUS_EN, eintr);
		}
	}

	/*
	 * Start the MMC command, or mark `cmd' as failed and return.
	 */
	error = sdhc_start_command(hp, cmd);
	if (error) {
		cmd->c_error = error;
		goto out;
	}

	/*
	 * Wait until the command phase is done, or until the command
	 * is marked done for any other reason.
	 */
	probing = (cmd->c_flags & SCF_TOUT_OK) != 0;
	if (!sdhc_wait_intr(hp, SDHC_COMMAND_COMPLETE, SDHC_COMMAND_TIMEOUT, probing)) {
		DPRINTF(1,("%s: timeout for command\n", __func__));
		cmd->c_error = ETIMEDOUT;
		goto out;
	}

	/*
	 * The host controller removes bits [0:7] from the response
	 * data (CRC) and we pass the data up unchanged to the bus
	 * driver (without padding).
	 */
	if (cmd->c_error == 0 && ISSET(cmd->c_flags, SCF_RSP_PRESENT)) {
		cmd->c_resp[0] = HREAD4(hp, SDHC_RESPONSE + 0);
		if (ISSET(cmd->c_flags, SCF_RSP_136)) {
			cmd->c_resp[1] = HREAD4(hp, SDHC_RESPONSE + 4);
			cmd->c_resp[2] = HREAD4(hp, SDHC_RESPONSE + 8);
			cmd->c_resp[3] = HREAD4(hp, SDHC_RESPONSE + 12);
			if (ISSET(hp->sc->sc_flags, SDHC_FLAG_RSP136_CRC)) {
				cmd->c_resp[0] = (cmd->c_resp[0] >> 8) |
				    (cmd->c_resp[1] << 24);
				cmd->c_resp[1] = (cmd->c_resp[1] >> 8) |
				    (cmd->c_resp[2] << 24);
				cmd->c_resp[2] = (cmd->c_resp[2] >> 8) |
				    (cmd->c_resp[3] << 24);
				cmd->c_resp[3] = (cmd->c_resp[3] >> 8);
			}
		}
	}
	DPRINTF(1,("%s: resp = %08x\n", HDEVNAME(hp), cmd->c_resp[0]));

	/*
	 * If the command has data to transfer in any direction,
	 * execute the transfer now.
	 */
	if (cmd->c_error == 0 && cmd->c_data != NULL)
		sdhc_transfer_data(hp, cmd);
	else if (ISSET(cmd->c_flags, SCF_RSP_BSY)) {
		if (!sdhc_wait_intr(hp, SDHC_TRANSFER_COMPLETE, hz * 10, false)) {
			DPRINTF(1,("%s: sdhc_exec_command: RSP_BSY\n",
			    HDEVNAME(hp)));
			cmd->c_error = ETIMEDOUT;
			goto out;
		}
	}

out:
	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)
	    && !ISSET(hp->sc->sc_flags, SDHC_FLAG_NO_LED_ON)) {
		/* Turn off the LED. */
		HCLR1(hp, SDHC_HOST_CTL, SDHC_LED_ON);
	}
	SET(cmd->c_flags, SCF_ITSDONE);

	mutex_exit(&hp->intr_lock);

	DPRINTF(1,("%s: cmd %d %s (flags=%08x error=%d)\n", HDEVNAME(hp),
	    cmd->c_opcode, (cmd->c_error == 0) ? "done" : "abort",
	    cmd->c_flags, cmd->c_error));
}

static int
sdhc_start_command(struct sdhc_host *hp, struct sdmmc_command *cmd)
{
	struct sdhc_softc * const sc = hp->sc;
	uint16_t blksize = 0;
	uint16_t blkcount = 0;
	uint16_t mode;
	uint16_t command;
	uint32_t pmask;
	int error;

	KASSERT(mutex_owned(&hp->intr_lock));

	DPRINTF(1,("%s: start cmd %d arg=%08x data=%p dlen=%d flags=%08x, status=%#x\n",
	    HDEVNAME(hp), cmd->c_opcode, cmd->c_arg, cmd->c_data,
	    cmd->c_datalen, cmd->c_flags, HREAD4(hp, SDHC_NINTR_STATUS)));

	/*
	 * The maximum block length for commands should be the minimum
	 * of the host buffer size and the card buffer size. (1.7.2)
	 */

	/* Fragment the data into proper blocks. */
	if (cmd->c_datalen > 0) {
		blksize = MIN(cmd->c_datalen, cmd->c_blklen);
		blkcount = cmd->c_datalen / blksize;
		if (cmd->c_datalen % blksize > 0) {
			/* XXX: Split this command. (1.7.4) */
			aprint_error_dev(sc->sc_dev,
			    "data not a multiple of %u bytes\n", blksize);
			return EINVAL;
		}
	}

	/* Check limit imposed by 9-bit block count. (1.7.2) */
	if (blkcount > SDHC_BLOCK_COUNT_MAX) {
		aprint_error_dev(sc->sc_dev, "too much data\n");
		return EINVAL;
	}

	/* Prepare transfer mode register value. (2.2.5) */
	mode = SDHC_BLOCK_COUNT_ENABLE;
	if (ISSET(cmd->c_flags, SCF_CMD_READ))
		mode |= SDHC_READ_MODE;
	if (blkcount > 1) {
		mode |= SDHC_MULTI_BLOCK_MODE;
		/* XXX only for memory commands? */
		mode |= SDHC_AUTO_CMD12_ENABLE;
	}
	if (cmd->c_dmamap != NULL && cmd->c_datalen > 0 &&
	    ISSET(hp->flags,  SHF_MODE_DMAEN)) {
		mode |= SDHC_DMA_ENABLE;
	}

	/*
	 * Prepare command register value. (2.2.6)
	 */
	command = (cmd->c_opcode & SDHC_COMMAND_INDEX_MASK) << SDHC_COMMAND_INDEX_SHIFT;

	if (ISSET(cmd->c_flags, SCF_RSP_CRC))
		command |= SDHC_CRC_CHECK_ENABLE;
	if (ISSET(cmd->c_flags, SCF_RSP_IDX))
		command |= SDHC_INDEX_CHECK_ENABLE;
	if (cmd->c_datalen > 0)
		command |= SDHC_DATA_PRESENT_SELECT;

	if (!ISSET(cmd->c_flags, SCF_RSP_PRESENT))
		command |= SDHC_NO_RESPONSE;
	else if (ISSET(cmd->c_flags, SCF_RSP_136))
		command |= SDHC_RESP_LEN_136;
	else if (ISSET(cmd->c_flags, SCF_RSP_BSY))
		command |= SDHC_RESP_LEN_48_CHK_BUSY;
	else
		command |= SDHC_RESP_LEN_48;

	/* Wait until command and optionally data inhibit bits are clear. (1.5) */
	pmask = SDHC_CMD_INHIBIT_CMD;
	if (cmd->c_flags & SCF_CMD_ADTC)
		pmask |= SDHC_CMD_INHIBIT_DAT;
	error = sdhc_wait_state(hp, pmask, 0);
	if (error) {
		(void) sdhc_soft_reset(hp, SDHC_RESET_DAT|SDHC_RESET_CMD);
		device_printf(sc->sc_dev, "command or data phase inhibited\n");
		return error;
	}

	DPRINTF(1,("%s: writing cmd: blksize=%d blkcnt=%d mode=%04x cmd=%04x\n",
	    HDEVNAME(hp), blksize, blkcount, mode, command));

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		blksize |= (MAX(0, PAGE_SHIFT - 12) & SDHC_DMA_BOUNDARY_MASK) <<
		    SDHC_DMA_BOUNDARY_SHIFT;	/* PAGE_SIZE DMA boundary */
	}

	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		/* Alert the user not to remove the card. */
		HSET1(hp, SDHC_HOST_CTL, SDHC_LED_ON);
	}

	/* Set DMA start address. */
	if (ISSET(hp->flags, SHF_USE_ADMA2_MASK) && cmd->c_data != NULL) {
		for (int seg = 0; seg < cmd->c_dmamap->dm_nsegs; seg++) {
			bus_addr_t paddr =
			    cmd->c_dmamap->dm_segs[seg].ds_addr;
			uint16_t len =
			    cmd->c_dmamap->dm_segs[seg].ds_len == 65536 ?
			    0 : cmd->c_dmamap->dm_segs[seg].ds_len;
			uint16_t attr =
			    SDHC_ADMA2_VALID | SDHC_ADMA2_ACT_TRANS;
			if (seg == cmd->c_dmamap->dm_nsegs - 1) {
				attr |= SDHC_ADMA2_END;
			}
			if (ISSET(hp->flags, SHF_USE_ADMA2_32)) {
				struct sdhc_adma2_descriptor32 *desc =
				    hp->adma2;
				desc[seg].attribute = htole16(attr);
				desc[seg].length = htole16(len);
				desc[seg].address = htole32(paddr);
			} else {
				struct sdhc_adma2_descriptor64 *desc =
				    hp->adma2;
				desc[seg].attribute = htole16(attr);
				desc[seg].length = htole16(len);
				desc[seg].address = htole32(paddr & 0xffffffff);
				desc[seg].address_hi = htole32(
				    (uint64_t)paddr >> 32);
			}
		}
		if (ISSET(hp->flags, SHF_USE_ADMA2_32)) {
			struct sdhc_adma2_descriptor32 *desc = hp->adma2;
			desc[cmd->c_dmamap->dm_nsegs].attribute = htole16(0);
		} else {
			struct sdhc_adma2_descriptor64 *desc = hp->adma2;
			desc[cmd->c_dmamap->dm_nsegs].attribute = htole16(0);
		}
		bus_dmamap_sync(sc->sc_dmat, hp->adma_map, 0, PAGE_SIZE,
		    BUS_DMASYNC_PREWRITE);
		HCLR1(hp, SDHC_HOST_CTL, SDHC_DMA_SELECT);
		HSET1(hp, SDHC_HOST_CTL, SDHC_DMA_SELECT_ADMA2);

		const bus_addr_t desc_addr = hp->adma_map->dm_segs[0].ds_addr;

		HWRITE4(hp, SDHC_ADMA_SYSTEM_ADDR, desc_addr & 0xffffffff);
		if (ISSET(hp->flags, SHF_USE_ADMA2_64)) {
			HWRITE4(hp, SDHC_ADMA_SYSTEM_ADDR + 4,
			    (uint64_t)desc_addr >> 32);
		}
	} else if (ISSET(mode, SDHC_DMA_ENABLE) &&
	    !ISSET(sc->sc_flags, SDHC_FLAG_EXTERNAL_DMA)) {
		HWRITE4(hp, SDHC_DMA_ADDR, cmd->c_dmamap->dm_segs[0].ds_addr);
	}

	/*
	 * Start a CPU data transfer.  Writing to the high order byte
	 * of the SDHC_COMMAND register triggers the SD command. (1.5)
	 */
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
		HWRITE4(hp, SDHC_BLOCK_SIZE, blksize | (blkcount << 16));
		HWRITE4(hp, SDHC_ARGUMENT, cmd->c_arg);
		HWRITE4(hp, SDHC_TRANSFER_MODE, mode | (command << 16));
	} else {
		HWRITE2(hp, SDHC_BLOCK_SIZE, blksize);
		HWRITE2(hp, SDHC_BLOCK_COUNT, blkcount);
		HWRITE4(hp, SDHC_ARGUMENT, cmd->c_arg);
		HWRITE2(hp, SDHC_TRANSFER_MODE, mode);
		HWRITE2(hp, SDHC_COMMAND, command);
	}

	return 0;
}

static void
sdhc_transfer_data(struct sdhc_host *hp, struct sdmmc_command *cmd)
{
	struct sdhc_softc *sc = hp->sc;
	int error;

	KASSERT(mutex_owned(&hp->intr_lock));

	DPRINTF(1,("%s: data transfer: resp=%08x datalen=%u\n", HDEVNAME(hp),
	    MMC_R1(cmd->c_resp), cmd->c_datalen));

#ifdef SDHC_DEBUG
	/* XXX I forgot why I wanted to know when this happens :-( */
	if ((cmd->c_opcode == 52 || cmd->c_opcode == 53) &&
	    ISSET(MMC_R1(cmd->c_resp), 0xcb00)) {
		aprint_error_dev(hp->sc->sc_dev,
		    "CMD52/53 error response flags %#x\n",
		    MMC_R1(cmd->c_resp) & 0xff00);
	}
#endif

	if (cmd->c_dmamap != NULL) {
		if (hp->sc->sc_vendor_transfer_data_dma != NULL) {
			error = hp->sc->sc_vendor_transfer_data_dma(sc, cmd);
			if (error == 0 && !sdhc_wait_intr(hp,
			    SDHC_TRANSFER_COMPLETE, SDHC_DMA_TIMEOUT, false)) {
				DPRINTF(1,("%s: timeout\n", __func__));
				error = ETIMEDOUT;
			}
		} else {
			error = sdhc_transfer_data_dma(hp, cmd);
		}
	} else
		error = sdhc_transfer_data_pio(hp, cmd);
	if (error)
		cmd->c_error = error;
	SET(cmd->c_flags, SCF_ITSDONE);

	DPRINTF(1,("%s: data transfer done (error=%d)\n",
	    HDEVNAME(hp), cmd->c_error));
}

static int
sdhc_transfer_data_dma(struct sdhc_host *hp, struct sdmmc_command *cmd)
{
	bus_dma_segment_t *dm_segs = cmd->c_dmamap->dm_segs;
	bus_addr_t posaddr;
	bus_addr_t segaddr;
	bus_size_t seglen;
	u_int seg = 0;
	int error = 0;
	int status;

	KASSERT(mutex_owned(&hp->intr_lock));
	KASSERT(HREAD2(hp, SDHC_NINTR_STATUS_EN) & SDHC_DMA_INTERRUPT);
	KASSERT(HREAD2(hp, SDHC_NINTR_SIGNAL_EN) & SDHC_DMA_INTERRUPT);
	KASSERT(HREAD2(hp, SDHC_NINTR_STATUS_EN) & SDHC_TRANSFER_COMPLETE);
	KASSERT(HREAD2(hp, SDHC_NINTR_SIGNAL_EN) & SDHC_TRANSFER_COMPLETE);

	for (;;) {
		status = sdhc_wait_intr(hp,
		    SDHC_DMA_INTERRUPT|SDHC_TRANSFER_COMPLETE,
		    SDHC_DMA_TIMEOUT, false);

		if (status & SDHC_TRANSFER_COMPLETE) {
			break;
		}
		if (!status) {
			DPRINTF(1,("%s: timeout\n", __func__));
			error = ETIMEDOUT;
			break;
		}

		if (ISSET(hp->flags, SHF_USE_ADMA2_MASK)) {
			continue;
		}

		if ((status & SDHC_DMA_INTERRUPT) == 0) {
			continue;
		}

		/* DMA Interrupt (boundary crossing) */

		segaddr = dm_segs[seg].ds_addr;
		seglen = dm_segs[seg].ds_len;
		posaddr = HREAD4(hp, SDHC_DMA_ADDR);

		if ((seg == (cmd->c_dmamap->dm_nsegs-1)) && (posaddr == (segaddr + seglen))) {
			continue;
		}
		if ((posaddr >= segaddr) && (posaddr < (segaddr + seglen)))
			HWRITE4(hp, SDHC_DMA_ADDR, posaddr);
		else if ((posaddr >= segaddr) && (posaddr == (segaddr + seglen)) && (seg + 1) < cmd->c_dmamap->dm_nsegs)
			HWRITE4(hp, SDHC_DMA_ADDR, dm_segs[++seg].ds_addr);
		KASSERT(seg < cmd->c_dmamap->dm_nsegs);
	}

	if (ISSET(hp->flags, SHF_USE_ADMA2_MASK)) {
		bus_dmamap_sync(hp->sc->sc_dmat, hp->adma_map, 0,
		    PAGE_SIZE, BUS_DMASYNC_POSTWRITE);
	}

	return error;
}

static int
sdhc_transfer_data_pio(struct sdhc_host *hp, struct sdmmc_command *cmd)
{
	uint8_t *data = cmd->c_data;
	void (*pio_func)(struct sdhc_host *, uint8_t *, u_int);
	u_int len, datalen;
	u_int imask;
	u_int pmask;
	int error = 0;

	KASSERT(mutex_owned(&hp->intr_lock));

	if (ISSET(cmd->c_flags, SCF_CMD_READ)) {
		imask = SDHC_BUFFER_READ_READY;
		pmask = SDHC_BUFFER_READ_ENABLE;
		if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
			pio_func = esdhc_read_data_pio;
		} else {
			pio_func = sdhc_read_data_pio;
		}
	} else {
		imask = SDHC_BUFFER_WRITE_READY;
		pmask = SDHC_BUFFER_WRITE_ENABLE;
		if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
			pio_func = esdhc_write_data_pio;
		} else {
			pio_func = sdhc_write_data_pio;
		}
	}
	datalen = cmd->c_datalen;

	KASSERT(mutex_owned(&hp->intr_lock));
	KASSERT(HREAD2(hp, SDHC_NINTR_STATUS_EN) & imask);
	KASSERT(HREAD2(hp, SDHC_NINTR_STATUS_EN) & SDHC_TRANSFER_COMPLETE);
	KASSERT(HREAD2(hp, SDHC_NINTR_SIGNAL_EN) & SDHC_TRANSFER_COMPLETE);

	while (datalen > 0) {
		if (!ISSET(HREAD4(hp, SDHC_PRESENT_STATE), imask)) {
			if (ISSET(hp->sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
				HSET4(hp, SDHC_NINTR_SIGNAL_EN, imask);
			} else {
				HSET2(hp, SDHC_NINTR_SIGNAL_EN, imask);
			}
			if (!sdhc_wait_intr(hp, imask, SDHC_BUFFER_TIMEOUT, false)) {
				DPRINTF(1,("%s: timeout\n", __func__));
				error = ETIMEDOUT;
				break;
			}

			error = sdhc_wait_state(hp, pmask, pmask);
			if (error)
				break;
		}

		len = MIN(datalen, cmd->c_blklen);
		(*pio_func)(hp, data, len);
		DPRINTF(2,("%s: pio data transfer %u @ %p\n",
		    HDEVNAME(hp), len, data));

		data += len;
		datalen -= len;
	}

	if (error == 0 && !sdhc_wait_intr(hp, SDHC_TRANSFER_COMPLETE,
	    SDHC_TRANSFER_TIMEOUT, false)) {
		DPRINTF(1,("%s: timeout for transfer\n", __func__));
		error = ETIMEDOUT;
	}

	return error;
}

static void
sdhc_read_data_pio(struct sdhc_host *hp, uint8_t *data, u_int datalen)
{

	if (((__uintptr_t)data & 3) == 0) {
		while (datalen > 3) {
			*(uint32_t *)data = le32toh(HREAD4(hp, SDHC_DATA));
			data += 4;
			datalen -= 4;
		}
		if (datalen > 1) {
			*(uint16_t *)data = le16toh(HREAD2(hp, SDHC_DATA));
			data += 2;
			datalen -= 2;
		}
		if (datalen > 0) {
			*data = HREAD1(hp, SDHC_DATA);
			data += 1;
			datalen -= 1;
		}
	} else if (((__uintptr_t)data & 1) == 0) {
		while (datalen > 1) {
			*(uint16_t *)data = le16toh(HREAD2(hp, SDHC_DATA));
			data += 2;
			datalen -= 2;
		}
		if (datalen > 0) {
			*data = HREAD1(hp, SDHC_DATA);
			data += 1;
			datalen -= 1;
		}
	} else {
		while (datalen > 0) {
			*data = HREAD1(hp, SDHC_DATA);
			data += 1;
			datalen -= 1;
		}
	}
}

static void
sdhc_write_data_pio(struct sdhc_host *hp, uint8_t *data, u_int datalen)
{

	if (((__uintptr_t)data & 3) == 0) {
		while (datalen > 3) {
			HWRITE4(hp, SDHC_DATA, htole32(*(uint32_t *)data));
			data += 4;
			datalen -= 4;
		}
		if (datalen > 1) {
			HWRITE2(hp, SDHC_DATA, htole16(*(uint16_t *)data));
			data += 2;
			datalen -= 2;
		}
		if (datalen > 0) {
			HWRITE1(hp, SDHC_DATA, *data);
			data += 1;
			datalen -= 1;
		}
	} else if (((__uintptr_t)data & 1) == 0) {
		while (datalen > 1) {
			HWRITE2(hp, SDHC_DATA, htole16(*(uint16_t *)data));
			data += 2;
			datalen -= 2;
		}
		if (datalen > 0) {
			HWRITE1(hp, SDHC_DATA, *data);
			data += 1;
			datalen -= 1;
		}
	} else {
		while (datalen > 0) {
			HWRITE1(hp, SDHC_DATA, *data);
			data += 1;
			datalen -= 1;
		}
	}
}

static void
esdhc_read_data_pio(struct sdhc_host *hp, uint8_t *data, u_int datalen)
{
	uint16_t status = HREAD2(hp, SDHC_NINTR_STATUS);
	uint32_t v;

	const size_t watermark = (HREAD4(hp, SDHC_WATERMARK_LEVEL) >> SDHC_WATERMARK_READ_SHIFT) & SDHC_WATERMARK_READ_MASK;
	size_t count = 0;

	while (datalen > 3 && !ISSET(status, SDHC_TRANSFER_COMPLETE)) {
		if (count == 0) {
			/*
			 * If we've drained "watermark" words, we need to wait
			 * a little bit so the read FIFO can refill.
			 */
			sdmmc_delay(10);
			count = watermark;
		}
		v = HREAD4(hp, SDHC_DATA);
		v = le32toh(v);
		*(uint32_t *)data = v;
		data += 4;
		datalen -= 4;
		status = HREAD2(hp, SDHC_NINTR_STATUS);
		count--;
	}
	if (datalen > 0 && !ISSET(status, SDHC_TRANSFER_COMPLETE)) {
		if (count == 0) {
			sdmmc_delay(10);
		}
		v = HREAD4(hp, SDHC_DATA);
		v = le32toh(v);
		do {
			*data++ = v;
			v >>= 8;
		} while (--datalen > 0);
	}
}

static void
esdhc_write_data_pio(struct sdhc_host *hp, uint8_t *data, u_int datalen)
{
	uint16_t status = HREAD2(hp, SDHC_NINTR_STATUS);
	uint32_t v;

	const size_t watermark = (HREAD4(hp, SDHC_WATERMARK_LEVEL) >> SDHC_WATERMARK_WRITE_SHIFT) & SDHC_WATERMARK_WRITE_MASK;
	size_t count = watermark;

	while (datalen > 3 && !ISSET(status, SDHC_TRANSFER_COMPLETE)) {
		if (count == 0) {
			sdmmc_delay(10);
			count = watermark;
		}
		v = *(uint32_t *)data;
		v = htole32(v);
		HWRITE4(hp, SDHC_DATA, v);
		data += 4;
		datalen -= 4;
		status = HREAD2(hp, SDHC_NINTR_STATUS);
		count--;
	}
	if (datalen > 0 && !ISSET(status, SDHC_TRANSFER_COMPLETE)) {
		if (count == 0) {
			sdmmc_delay(10);
		}
		v = *(uint32_t *)data;
		v = htole32(v);
		HWRITE4(hp, SDHC_DATA, v);
	}
}

/* Prepare for another command. */
static int
sdhc_soft_reset(struct sdhc_host *hp, int mask)
{
	int timo;

	KASSERT(mutex_owned(&hp->intr_lock));

	DPRINTF(1,("%s: software reset reg=%08x\n", HDEVNAME(hp), mask));

	/* Request the reset.  */
	HWRITE1(hp, SDHC_SOFTWARE_RESET, mask);

	/*
	 * If necessary, wait for the controller to set the bits to
	 * acknowledge the reset.
	 */
	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_WAIT_RESET) &&
	    ISSET(mask, (SDHC_RESET_DAT | SDHC_RESET_CMD))) {
		for (timo = 10000; timo > 0; timo--) {
			if (ISSET(HREAD1(hp, SDHC_SOFTWARE_RESET), mask))
				break;
			/* Short delay because I worry we may miss it...  */
			sdmmc_delay(1);
		}
		if (timo == 0)
			DPRINTF(1,("%s: timeout for reset on\n", __func__));
			return ETIMEDOUT;
	}

	/*
	 * Wait for the controller to clear the bits to indicate that
	 * the reset has completed.
	 */
	for (timo = 10; timo > 0; timo--) {
		if (!ISSET(HREAD1(hp, SDHC_SOFTWARE_RESET), mask))
			break;
		sdmmc_delay(10000);
	}
	if (timo == 0) {
		DPRINTF(1,("%s: timeout reg=%08x\n", HDEVNAME(hp),
		    HREAD1(hp, SDHC_SOFTWARE_RESET)));
		return ETIMEDOUT;
	}

	if (ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED)) {
		HSET4(hp, SDHC_DMA_CTL, SDHC_DMA_SNOOP);
	}

	return 0;
}

static int
sdhc_wait_intr(struct sdhc_host *hp, int mask, int timo, bool probing)
{
	int status, error, nointr;

	KASSERT(mutex_owned(&hp->intr_lock));

	mask |= SDHC_ERROR_INTERRUPT;

	nointr = 0;
	status = hp->intr_status & mask;
	while (status == 0) {
		if (cv_timedwait(&hp->intr_cv, &hp->intr_lock, timo)
		    == EWOULDBLOCK) {
			nointr = 1;
			break;
		}
		status = hp->intr_status & mask;
	}
	error = hp->intr_error_status;

	DPRINTF(2,("%s: intr status %#x error %#x\n", HDEVNAME(hp), status,
	    error));

	hp->intr_status &= ~status;
	hp->intr_error_status &= ~error;

	if (ISSET(status, SDHC_ERROR_INTERRUPT)) {
		if (ISSET(error, SDHC_DMA_ERROR))
			device_printf(hp->sc->sc_dev,"dma error\n");
		if (ISSET(error, SDHC_ADMA_ERROR))
			device_printf(hp->sc->sc_dev,"adma error\n");
		if (ISSET(error, SDHC_AUTO_CMD12_ERROR))
			device_printf(hp->sc->sc_dev,"auto_cmd12 error\n");
		if (ISSET(error, SDHC_CURRENT_LIMIT_ERROR))
			device_printf(hp->sc->sc_dev,"current limit error\n");
		if (ISSET(error, SDHC_DATA_END_BIT_ERROR))
			device_printf(hp->sc->sc_dev,"data end bit error\n");
		if (ISSET(error, SDHC_DATA_CRC_ERROR))
			device_printf(hp->sc->sc_dev,"data crc error\n");
		if (ISSET(error, SDHC_DATA_TIMEOUT_ERROR))
			device_printf(hp->sc->sc_dev,"data timeout error\n");
		if (ISSET(error, SDHC_CMD_INDEX_ERROR))
			device_printf(hp->sc->sc_dev,"cmd index error\n");
		if (ISSET(error, SDHC_CMD_END_BIT_ERROR))
			device_printf(hp->sc->sc_dev,"cmd end bit error\n");
		if (ISSET(error, SDHC_CMD_CRC_ERROR))
			device_printf(hp->sc->sc_dev,"cmd crc error\n");
		if (ISSET(error, SDHC_CMD_TIMEOUT_ERROR)) {
			if (!probing)
				device_printf(hp->sc->sc_dev,"cmd timeout error\n");
#ifdef SDHC_DEBUG
			else if (sdhcdebug > 0)
				device_printf(hp->sc->sc_dev,"cmd timeout (expected)\n");
#endif
		}
		if ((error & ~SDHC_EINTR_STATUS_MASK) != 0)
			device_printf(hp->sc->sc_dev,"vendor error %#x\n",
				(error & ~SDHC_EINTR_STATUS_MASK));
		if (error == 0)
			device_printf(hp->sc->sc_dev,"no error\n");

		/* Command timeout has higher priority than command complete. */
		if (ISSET(error, SDHC_CMD_TIMEOUT_ERROR))
			CLR(status, SDHC_COMMAND_COMPLETE);

		/* Transfer complete has higher priority than data timeout. */
		if (ISSET(status, SDHC_TRANSFER_COMPLETE))
			CLR(error, SDHC_DATA_TIMEOUT_ERROR);
	}

	if (nointr ||
	    (ISSET(status, SDHC_ERROR_INTERRUPT) && error)) {
		if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED))
			(void)sdhc_soft_reset(hp, SDHC_RESET_CMD|SDHC_RESET_DAT);
		hp->intr_error_status = 0;
		status = 0;
	}

	return status;
}

/*
 * Established by attachment driver at interrupt priority IPL_SDMMC.
 */
int
sdhc_intr(void *arg)
{
	struct sdhc_softc *sc = (struct sdhc_softc *)arg;
	struct sdhc_host *hp;
	int done = 0;
	uint16_t status;
	uint16_t error;

	/* We got an interrupt, but we don't know from which slot. */
	for (size_t host = 0; host < sc->sc_nhosts; host++) {
		hp = sc->sc_host[host];
		if (hp == NULL)
			continue;

		mutex_enter(&hp->intr_lock);

		if (ISSET(sc->sc_flags, SDHC_FLAG_32BIT_ACCESS)) {
			/* Find out which interrupts are pending. */
			uint32_t xstatus = HREAD4(hp, SDHC_NINTR_STATUS);
			status = xstatus;
			error = xstatus >> 16;
			if (ISSET(sc->sc_flags, SDHC_FLAG_ENHANCED)) {
				if ((error & SDHC_NINTR_STATUS_MASK) != 0)
					SET(status, SDHC_ERROR_INTERRUPT);
			}
			if (error)
				xstatus |= SDHC_ERROR_INTERRUPT;
			else if (!ISSET(status, SDHC_NINTR_STATUS_MASK))
				goto next_port; /* no interrupt for us */
			/* Acknowledge the interrupts we are about to handle. */
			HWRITE4(hp, SDHC_NINTR_STATUS, xstatus);
		} else {
			/* Find out which interrupts are pending. */
			error = 0;
			status = HREAD2(hp, SDHC_NINTR_STATUS);
			if (!ISSET(status, SDHC_NINTR_STATUS_MASK))
				goto next_port; /* no interrupt for us */
			/* Acknowledge the interrupts we are about to handle. */
			HWRITE2(hp, SDHC_NINTR_STATUS, status);
			if (ISSET(status, SDHC_ERROR_INTERRUPT)) {
				/* Acknowledge error interrupts. */
				error = HREAD2(hp, SDHC_EINTR_STATUS);
				HWRITE2(hp, SDHC_EINTR_STATUS, error);
			}
		}

		DPRINTF(2,("%s: interrupt status=%x error=%x\n", HDEVNAME(hp),
		    status, error));

		/* Claim this interrupt. */
		done = 1;

		if (ISSET(status, SDHC_ERROR_INTERRUPT) &&
		    ISSET(error, SDHC_ADMA_ERROR)) {
			uint8_t adma_err = HREAD1(hp, SDHC_ADMA_ERROR_STATUS);
			printf("%s: ADMA error, status %02x\n", HDEVNAME(hp),
			    adma_err);
		}

		/*
		 * Wake up the sdmmc event thread to scan for cards.
		 */
		if (ISSET(status, SDHC_CARD_REMOVAL|SDHC_CARD_INSERTION)) {
			if (hp->sdmmc != NULL) {
				sdmmc_needs_discover(hp->sdmmc);
			}
			if (ISSET(sc->sc_flags, SDHC_FLAG_ENHANCED)) {
				HCLR4(hp, SDHC_NINTR_STATUS_EN,
				    status & (SDHC_CARD_REMOVAL|SDHC_CARD_INSERTION));
				HCLR4(hp, SDHC_NINTR_SIGNAL_EN,
				    status & (SDHC_CARD_REMOVAL|SDHC_CARD_INSERTION));
			}
		}

		/*
		 * Schedule re-tuning process (UHS).
		 */
		if (ISSET(status, SDHC_RETUNING_EVENT)) {
			atomic_swap_uint(&hp->tuning_timer_pending, 1);
		}

		/*
		 * Wake up the blocking process to service command
		 * related interrupt(s).
		 */
		if (ISSET(status, SDHC_COMMAND_COMPLETE|SDHC_ERROR_INTERRUPT|
		    SDHC_BUFFER_READ_READY|SDHC_BUFFER_WRITE_READY|
		    SDHC_TRANSFER_COMPLETE|SDHC_DMA_INTERRUPT)) {
			hp->intr_error_status |= error;
			hp->intr_status |= status;
			if (ISSET(sc->sc_flags, SDHC_FLAG_ENHANCED)) {
				HCLR4(hp, SDHC_NINTR_SIGNAL_EN,
				    status & (SDHC_BUFFER_READ_READY|SDHC_BUFFER_WRITE_READY));
			}
			cv_broadcast(&hp->intr_cv);
		}

		/*
		 * Service SD card interrupts.
		 */
		if (!ISSET(sc->sc_flags, SDHC_FLAG_ENHANCED)
		    && ISSET(status, SDHC_CARD_INTERRUPT)) {
			DPRINTF(0,("%s: card interrupt\n", HDEVNAME(hp)));
			HCLR2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
			sdmmc_card_intr(hp->sdmmc);
		}
next_port:
		mutex_exit(&hp->intr_lock);
	}

	return done;
}

kmutex_t *
sdhc_host_lock(struct sdhc_host *hp)
{
	return &hp->intr_lock;
}

#ifdef SDHC_DEBUG
void
sdhc_dump_regs(struct sdhc_host *hp)
{

	printf("0x%02x PRESENT_STATE:    %x\n", SDHC_PRESENT_STATE,
	    HREAD4(hp, SDHC_PRESENT_STATE));
	if (!ISSET(hp->sc->sc_flags, SDHC_FLAG_ENHANCED))
		printf("0x%02x POWER_CTL:        %x\n", SDHC_POWER_CTL,
		    HREAD1(hp, SDHC_POWER_CTL));
	printf("0x%02x NINTR_STATUS:     %x\n", SDHC_NINTR_STATUS,
	    HREAD2(hp, SDHC_NINTR_STATUS));
	printf("0x%02x EINTR_STATUS:     %x\n", SDHC_EINTR_STATUS,
	    HREAD2(hp, SDHC_EINTR_STATUS));
	printf("0x%02x NINTR_STATUS_EN:  %x\n", SDHC_NINTR_STATUS_EN,
	    HREAD2(hp, SDHC_NINTR_STATUS_EN));
	printf("0x%02x EINTR_STATUS_EN:  %x\n", SDHC_EINTR_STATUS_EN,
	    HREAD2(hp, SDHC_EINTR_STATUS_EN));
	printf("0x%02x NINTR_SIGNAL_EN:  %x\n", SDHC_NINTR_SIGNAL_EN,
	    HREAD2(hp, SDHC_NINTR_SIGNAL_EN));
	printf("0x%02x EINTR_SIGNAL_EN:  %x\n", SDHC_EINTR_SIGNAL_EN,
	    HREAD2(hp, SDHC_EINTR_SIGNAL_EN));
	printf("0x%02x CAPABILITIES:     %x\n", SDHC_CAPABILITIES,
	    HREAD4(hp, SDHC_CAPABILITIES));
	printf("0x%02x MAX_CAPABILITIES: %x\n", SDHC_MAX_CAPABILITIES,
	    HREAD4(hp, SDHC_MAX_CAPABILITIES));
}
#endif
