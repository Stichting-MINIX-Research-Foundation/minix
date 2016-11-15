/*	$NetBSD: mvsdio.c,v 1.5 2014/03/15 13:33:48 kiyohara Exp $	*/
/*
 * Copyright (c) 2010 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mvsdio.c,v 1.5 2014/03/15 13:33:48 kiyohara Exp $");

#include "opt_mvsdio.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/mutex.h>

#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>
#include <dev/marvell/mvsdioreg.h>

#include <dev/sdmmc/sdmmcvar.h>
#include <dev/sdmmc/sdmmcchip.h>

//#define MVSDIO_DEBUG 1
#ifdef MVSDIO_DEBUG
#define DPRINTF(n, x)	if (mvsdio_debug >= (n)) printf x
int mvsdio_debug = MVSDIO_DEBUG;
#else
#define DPRINTF(n, x)
#endif

struct mvsdio_softc {
	device_t sc_dev;
	device_t sc_sdmmc;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;

	struct kmutex sc_mtx;
	kcondvar_t sc_cv;

	struct sdmmc_command *sc_exec_cmd;
	uint32_t sc_waitintr;
};

static int mvsdio_match(device_t, struct cfdata *, void *);
static void mvsdio_attach(device_t, device_t, void *);

static int mvsdio_intr(void *);

static int mvsdio_host_reset(sdmmc_chipset_handle_t);
static uint32_t mvsdio_host_ocr(sdmmc_chipset_handle_t);
static int mvsdio_host_maxblklen(sdmmc_chipset_handle_t);
#ifdef MVSDIO_CARD_DETECT
int MVSDIO_CARD_DETECT(sdmmc_chipset_handle_t);
#else
static int mvsdio_card_detect(sdmmc_chipset_handle_t);
#endif
#ifdef MVSDIO_WRITE_PROTECT
int MVSDIO_WRITE_PROTECT(sdmmc_chipset_handle_t);
#else
static int mvsdio_write_protect(sdmmc_chipset_handle_t);
#endif
static int mvsdio_bus_power(sdmmc_chipset_handle_t, uint32_t);
static int mvsdio_bus_clock(sdmmc_chipset_handle_t, int);
static int mvsdio_bus_width(sdmmc_chipset_handle_t, int);
static int mvsdio_bus_rod(sdmmc_chipset_handle_t, int);
static void mvsdio_exec_command(sdmmc_chipset_handle_t, struct sdmmc_command *);
static void mvsdio_card_enable_intr(sdmmc_chipset_handle_t, int);
static void mvsdio_card_intr_ack(sdmmc_chipset_handle_t);

static void mvsdio_wininit(struct mvsdio_softc *, enum marvell_tags *);

static struct sdmmc_chip_functions mvsdio_chip_functions = {
	/* host controller reset */
	.host_reset		= mvsdio_host_reset,

	/* host controller capabilities */
	.host_ocr		= mvsdio_host_ocr,
	.host_maxblklen		= mvsdio_host_maxblklen,

	/* card detection */
#ifdef MVSDIO_CARD_DETECT
	.card_detect		= MVSDIO_CARD_DETECT,
#else
	.card_detect		= mvsdio_card_detect,
#endif

	/* write protect */
#ifdef MVSDIO_WRITE_PROTECT
	.write_protect		= MVSDIO_WRITE_PROTECT,
#else
	.write_protect		= mvsdio_write_protect,
#endif

	/* bus power, clock frequency, width, rod */
	.bus_power		= mvsdio_bus_power,
	.bus_clock		= mvsdio_bus_clock,
	.bus_width		= mvsdio_bus_width,
	.bus_rod		= mvsdio_bus_rod,

	/* command execution */
	.exec_command		= mvsdio_exec_command,

	/* card interrupt */
	.card_enable_intr	= mvsdio_card_enable_intr,
	.card_intr_ack		= mvsdio_card_intr_ack,
};

CFATTACH_DECL_NEW(mvsdio_mbus, sizeof(struct mvsdio_softc),
    mvsdio_match, mvsdio_attach, NULL, NULL);


/* ARGSUSED */
static int
mvsdio_match(device_t parent, struct cfdata *match, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT)
		return 0;

	mva->mva_size = MVSDIO_SIZE;
	return 1;
}

/* ARGSUSED */
static void
mvsdio_attach(device_t parent, device_t self, void *aux)
{
	struct mvsdio_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;
	struct sdmmcbus_attach_args saa;
	uint32_t nis, eis;

	aprint_naive("\n");
	aprint_normal(": Marvell Secure Digital Input/Output Interface\n");

	sc->sc_dev = self;
	sc->sc_iot = mva->mva_iot;
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh, mva->mva_offset,
	    mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "Cannot map registers\n");
		return;
	}
	sc->sc_dmat = mva->mva_dmat;

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_SDMMC);
	cv_init(&sc->sc_cv, "mvsdio_intr");

	sc->sc_exec_cmd = NULL;
	sc->sc_waitintr = 0;

	marvell_intr_establish(mva->mva_irq, IPL_SDMMC, mvsdio_intr, sc);

	mvsdio_wininit(sc, mva->mva_tags);

#if BYTE_ORDER == LITTLE_ENDIAN
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC, HC_BIGENDIAN);
#else
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC, HC_LSBFIRST);
#endif
	nis =
	    NIS_CMDCOMPLETE	/* Command Complete */		|
	    NIS_XFERCOMPLETE	/* Transfer Complete */		|
	    NIS_BLOCKGAPEV	/* Block gap event */		|
	    NIS_DMAINT		/* DMA interrupt */		|
	    NIS_CARDINT		/* Card interrupt */		|
	    NIS_READWAITON	/* Read Wait state is on */	|
	    NIS_SUSPENSEON					|
	    NIS_AUTOCMD12COMPLETE	/* Auto_cmd12 is comp */|
	    NIS_UNEXPECTEDRESPDET				|
	    NIS_ERRINT;			/* Error interrupt */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NIS, nis);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISE, nis);

#define NIC_DYNAMIC_CONFIG_INTRS	(NIS_CMDCOMPLETE	| \
					 NIS_XFERCOMPLETE	| \
					 NIS_DMAINT		| \
					 NIS_CARDINT		| \
					 NIS_AUTOCMD12COMPLETE)

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISIE,
	    nis & ~NIC_DYNAMIC_CONFIG_INTRS);

	eis =
	    EIS_CMDTIMEOUTERR	/*Command timeout err*/		|
	    EIS_CMDCRCERR	/* Command CRC Error */		|
	    EIS_CMDENDBITERR	/*Command end bit err*/		|
	    EIS_CMDINDEXERR	/*Command Index Error*/		|
	    EIS_DATATIMEOUTERR	/* Data timeout error */	|
	    EIS_RDDATACRCERR	/* Read data CRC err */		|
	    EIS_RDDATAENDBITERR	/*Rd data end bit err*/		|
	    EIS_AUTOCMD12ERR	/* Auto CMD12 error */		|
	    EIS_CMDSTARTBITERR	/*Cmd start bit error*/		|
	    EIS_XFERSIZEERR	/*Tx size mismatched err*/	|
	    EIS_RESPTBITERR	/* Response T bit err */	|
	    EIS_CRCENDBITERR	/* CRC end bit error */		|
	    EIS_CRCSTARTBITERR	/* CRC start bit err */		|
	    EIS_CRCSTATERR;	/* CRC status error */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_EIS, eis);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_EISE, eis);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_EISIE, eis);

        /*
	 * Attach the generic SD/MMC bus driver.  (The bus driver must
	 * not invoke any chipset functions before it is attached.)
	 */
	memset(&saa, 0, sizeof(saa));
	saa.saa_busname = "sdmmc";
	saa.saa_sct = &mvsdio_chip_functions;
	saa.saa_sch = sc;
	saa.saa_dmat = sc->sc_dmat;
	saa.saa_clkmin = 100;		/* XXXX: 100 kHz from SheevaPlug LSP */
	saa.saa_clkmax = MVSDIO_MAX_CLOCK;
	saa.saa_caps = SMC_CAPS_AUTO_STOP | SMC_CAPS_4BIT_MODE | SMC_CAPS_DMA |
	    SMC_CAPS_SD_HIGHSPEED | SMC_CAPS_MMC_HIGHSPEED;
#ifndef MVSDIO_CARD_DETECT
	saa.saa_caps |= SMC_CAPS_POLL_CARD_DET;
#endif
	sc->sc_sdmmc = config_found(sc->sc_dev, &saa, NULL);
}

static int
mvsdio_intr(void *arg)
{
	struct mvsdio_softc *sc = (struct mvsdio_softc *)arg;
	struct sdmmc_command *cmd = sc->sc_exec_cmd;
	uint32_t nis, eis;
	int handled = 0, error;

	nis = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NIS);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NIS, nis);

	DPRINTF(3, ("%s: intr: NIS=0x%x, NISE=0x%x, NISIE=0x%x\n",
	    __func__, nis,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISE),
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISIE)));

	if (__predict_false(nis & NIS_ERRINT)) {
		sc->sc_exec_cmd = NULL;
		eis = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_EIS);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_EIS, eis);

		DPRINTF(3, ("    EIS=0x%x, EISE=0x%x, EISIE=0x%x\n",
		    eis,
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_EISE),
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_EISIE)));

		if (eis & (EIS_CMDTIMEOUTERR | EIS_DATATIMEOUTERR)) {
			error = ETIMEDOUT;	/* Timeouts */
			DPRINTF(2, ("    Command/Data Timeout (0x%x)\n",
			    eis & (EIS_CMDTIMEOUTERR | EIS_DATATIMEOUTERR)));
		} else {

#define CRC_ERROR	(EIS_CMDCRCERR		| \
			 EIS_RDDATACRCERR	| \
			 EIS_CRCENDBITERR	| \
			 EIS_CRCSTARTBITERR	| \
			 EIS_CRCSTATERR)
			if (eis & CRC_ERROR) {
				error = EIO;		/* CRC errors */
				aprint_error_dev(sc->sc_dev,
				    "CRC Error (0x%x)\n", eis & CRC_ERROR);
			}

#define COMMAND_ERROR	(EIS_CMDENDBITERR	| \
			 EIS_CMDINDEXERR	| \
			 EIS_CMDSTARTBITERR)
			if (eis & COMMAND_ERROR) {
				error = EIO;		/*Other command errors*/
				aprint_error_dev(sc->sc_dev,
				    "Command Error (0x%x)\n",
				    eis & COMMAND_ERROR);
			}

#define MISC_ERROR	(EIS_RDDATAENDBITERR	| \
			 EIS_AUTOCMD12ERR	| \
			 EIS_XFERSIZEERR	| \
			 EIS_RESPTBITERR)
			if (eis & MISC_ERROR) {
				error = EIO;		/* Misc error */
				aprint_error_dev(sc->sc_dev,
				    "Misc Error (0x%x)\n", eis & MISC_ERROR);
			}
		}

		if (cmd != NULL) {
			cmd->c_error = error;
			cv_signal(&sc->sc_cv);
		}
		handled = 1;
	} else if (cmd != NULL &&
	    ((nis & sc->sc_waitintr) || (nis & NIS_UNEXPECTEDRESPDET))) {
		sc->sc_exec_cmd = NULL;
		sc->sc_waitintr = 0;
		if (cmd->c_flags & SCF_RSP_PRESENT) {
			uint16_t rh[MVSDIO_NRH + 1];
			int i, j;

			if (cmd->c_flags & SCF_RSP_136) {
				for (i = 0; i < MVSDIO_NRH; i++)
					rh[i + 1] = bus_space_read_4(sc->sc_iot,
					    sc->sc_ioh, MVSDIO_RH(i));
				rh[0] = 0;
				for (j = 3, i = 1; j >= 0; j--, i += 2) {
					cmd->c_resp[j] =
					    rh[i - 1] << 30 |
					    rh[i + 0] << 14 |
					    rh[i + 1] >> 2;
				}
				cmd->c_resp[3] &= 0x00ffffff;
			} else {
				for (i = 0; i < 3; i++)
					rh[i] = bus_space_read_4(sc->sc_iot,
					    sc->sc_ioh, MVSDIO_RH(i));
				cmd->c_resp[0] =
				    ((rh[0] & 0x03ff) << 22) |
				    ((rh[1]         ) <<  6) |
				    ((rh[2] & 0x003f) <<  0);
				cmd->c_resp[1] = (rh[0] & 0xfc00) >> 10;
				cmd->c_resp[2] = 0;
				cmd->c_resp[3] = 0;
			}
		}
		if (nis & NIS_UNEXPECTEDRESPDET)
			cmd->c_error = EIO;
		cv_signal(&sc->sc_cv);
	}

	if (nis & NIS_CARDINT)
		if (bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISIE) &
		    NIS_CARDINT) {
			sdmmc_card_intr(sc->sc_sdmmc);
			handled = 1;
		}

	return handled;
}

static int
mvsdio_host_reset(sdmmc_chipset_handle_t sch)
{
	struct mvsdio_softc *sc = (struct mvsdio_softc *)sch;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_SR, SR_SWRESET);
	return 0;
}

static uint32_t
mvsdio_host_ocr(sdmmc_chipset_handle_t sch)
{

	return MMC_OCR_3_3V_3_4V | MMC_OCR_3_2V_3_3V;
}

static int
mvsdio_host_maxblklen(sdmmc_chipset_handle_t sch)
{

	return DBS_BLOCKSIZE_MAX;
}

#ifndef MVSDIO_CARD_DETECT
static int
mvsdio_card_detect(sdmmc_chipset_handle_t sch)
{
	struct mvsdio_softc *sc __unused = (struct mvsdio_softc *)sch;

	DPRINTF(2, ("%s: driver lacks card_detect() function.\n",
	    device_xname(sc->sc_dev)));
	return 1;	/* always detect */
}
#endif

#ifndef MVSDIO_WRITE_PROTECT
static int
mvsdio_write_protect(sdmmc_chipset_handle_t sch)
{

	/* Nothing */

	return 0;
}
#endif

static int
mvsdio_bus_power(sdmmc_chipset_handle_t sch, uint32_t ocr)
{
	struct mvsdio_softc *sc = (struct mvsdio_softc *)sch;
	uint32_t reg;

	/* Initial state is Open Drain on CMD line. */
	mutex_enter(&sc->sc_mtx);
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC);
	reg &= ~HC_PUSHPULLEN;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC, reg);
	mutex_exit(&sc->sc_mtx);

	return 0;
}

static int
mvsdio_bus_clock(sdmmc_chipset_handle_t sch, int freq)
{
	struct mvsdio_softc *sc = (struct mvsdio_softc *)sch;
	uint32_t reg;
	int m;

	mutex_enter(&sc->sc_mtx);
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_TM);

	/* Just stop the clock. */
	if (freq == 0) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_TM,
		    reg | TM_STOPCLKEN);
		goto out;
	}

#define FREQ_TO_M(f)	(100000 / (f) - 1)

	m = FREQ_TO_M(freq);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_CDV,
	    m & CDV_CLKDVDRMVALUE_MASK);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_TM,
	    reg & ~TM_STOPCLKEN);

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC);
	if (freq > 25000)
		reg |= HC_HISPEEDEN;
	else
		reg &= ~HC_HISPEEDEN;	/* up to 25 MHz */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC, reg);

out:
	mutex_exit(&sc->sc_mtx);

	return 0;
}

static int
mvsdio_bus_width(sdmmc_chipset_handle_t sch, int width)
{
	struct mvsdio_softc *sc = (struct mvsdio_softc *)sch;
	uint32_t reg, v;

	switch (width) {
	case 1:
		v = 0;
		break;

	case 4:
		v = HC_DATAWIDTH;
		break;

	default:
		DPRINTF(0, ("%s: unsupported bus width (%d)\n",
		    device_xname(sc->sc_dev), width));
		return EINVAL;
	}

	mutex_enter(&sc->sc_mtx);
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC);
	reg &= ~HC_DATAWIDTH;
	reg |= v;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC, reg);
	mutex_exit(&sc->sc_mtx);

	return 0;
}

static int
mvsdio_bus_rod(sdmmc_chipset_handle_t sch, int on)
{
	struct mvsdio_softc *sc = (struct mvsdio_softc *)sch;
	uint32_t reg;

	/* Change Open-drain/Push-pull. */
	mutex_enter(&sc->sc_mtx);
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC);
	if (on)
		reg &= ~HC_PUSHPULLEN;
	else
		reg |= HC_PUSHPULLEN;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC, reg);
	mutex_exit(&sc->sc_mtx);

	return 0;
}

static void
mvsdio_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct mvsdio_softc *sc = (struct mvsdio_softc *)sch;
	uint32_t tm, c, hc, aacc, nisie, wait;
	int blklen;

	DPRINTF(1, ("%s: start cmd %d arg=%#x data=%p dlen=%d flags=%#x\n",
	    device_xname(sc->sc_dev), cmd->c_opcode, cmd->c_arg, cmd->c_data,
	    cmd->c_datalen, cmd->c_flags));

	mutex_enter(&sc->sc_mtx);

	tm = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_TM);

	if (cmd->c_datalen > 0) {
		bus_dma_segment_t *dm_seg =
		    &cmd->c_dmamap->dm_segs[cmd->c_dmaseg];
		bus_addr_t ds_addr = dm_seg->ds_addr + cmd->c_dmaoff;

		blklen = MIN(cmd->c_datalen, cmd->c_blklen);

		if (cmd->c_datalen % blklen > 0) {
			aprint_error_dev(sc->sc_dev,
			    "data not a multiple of %u bytes\n", blklen);
			cmd->c_error = EINVAL;
			goto out;
		}
		if ((uint32_t)cmd->c_data & 0x3) {
			aprint_error_dev(sc->sc_dev,
			    "data not 4byte aligned\n");
			cmd->c_error = EINVAL;
			goto out;
		}

		/* Set DMA Buffer Address */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_DMABA16LSB,
		    ds_addr & 0xffff);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_DMABA16MSB,
		    (ds_addr >> 16) & 0xffff);

		/* Set Data Block Size and Count */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_DBS,
		    DBS_BLOCKSIZE(blklen));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_DBC,
		    DBC_BLOCKCOUNT(cmd->c_datalen / blklen));

		tm &= ~TM_HOSTXFERMODE;			/* Always DMA */
		if (cmd->c_flags & SCF_CMD_READ)
			tm |= TM_DATAXFERTOWARDHOST;
		else
			tm &= ~TM_DATAXFERTOWARDHOST;
		tm |= TM_HWWRDATAEN;
		wait = NIS_XFERCOMPLETE;
	} else {
		tm &= ~TM_HWWRDATAEN;
		wait = NIS_CMDCOMPLETE;
	}

	/* Set Argument in Command */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_AC16LSB,
	    cmd->c_arg & 0xffff);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_AC16MSB,
	    (cmd->c_arg >> 16) & 0xffff);

	/* Set Host Control, exclude PushPullEn, DataWidth, HiSpeedEn. */
	hc = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC);
	hc |= (HC_TIMEOUTVALUE_MAX | HC_TIMEOUTEN);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_HC, hc);

	/* Data Block Gap Control: Resume */

	/* Clock Control: SclkMasterEn */

	if (cmd->c_opcode == MMC_READ_BLOCK_MULTIPLE ||
	    cmd->c_opcode == MMC_WRITE_BLOCK_MULTIPLE) {
		aacc = 0;
#if 1	/* XXXX: need? */
		if (cmd->c_opcode == MMC_READ_BLOCK_MULTIPLE) {
			struct sdmmc_softc *sdmmc =
			    device_private(sc->sc_sdmmc);
			struct sdmmc_function *sf = sdmmc->sc_card;

			aacc = MMC_ARG_RCA(sf->rca);
		}
#endif

		/* Set Argument in Auto Cmd12 Command */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_AACC16LSBT,
		    aacc & 0xffff);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_AACC16MSBT,
		    (aacc >> 16) & 0xffff);

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_IACCT,
		    IACCT_AUTOCMD12BUSYCHKEN	|
		    IACCT_AUTOCMD12INDEXCHKEN	|
		    IACCT_AUTOCMD12INDEX);

		tm |= TM_AUTOCMD12EN;
		wait = NIS_AUTOCMD12COMPLETE;
	} else
		tm &= ~TM_AUTOCMD12EN;

	tm |= TM_INTCHKEN;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_TM, tm);

	c = C_CMDINDEX(cmd->c_opcode);
	if (cmd->c_flags & SCF_RSP_PRESENT) {
		if (cmd->c_flags & SCF_RSP_136)
			c |= C_RESPTYPE_136BR;
		else if (!(cmd->c_flags & SCF_RSP_BSY))
			c |= C_RESPTYPE_48BR;
		else
			c |= C_RESPTYPE_48BRCB;
		c |= C_UNEXPECTEDRESPEN;
	} else
		c |= C_RESPTYPE_NR;
	if (cmd->c_flags & SCF_RSP_CRC)
		c |= C_CMDCRCCHKEN;
	if (cmd->c_flags & SCF_RSP_IDX)
		c |= C_CMDINDEXCHKEN;
	if (cmd->c_datalen > 0)
		c |= (C_DATAPRESENT | C_DATACRC16CHKEN);

	DPRINTF(2, ("%s: TM=0x%x, C=0x%x, HC=0x%x\n", __func__, tm, c, hc));

	nisie = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISIE);
	nisie &= ~(NIS_CMDCOMPLETE | NIS_XFERCOMPLETE | NIS_AUTOCMD12COMPLETE);
	nisie |= wait;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISIE, nisie);

	/* Execute command */
	sc->sc_exec_cmd = cmd;
	sc->sc_waitintr = wait;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_C, c);

	/* Wait interrupt for complete or error or timeout */
	while (sc->sc_exec_cmd == cmd)
		cv_wait(&sc->sc_cv, &sc->sc_mtx);

out:
	mutex_exit(&sc->sc_mtx);

	DPRINTF(1, ("%s: cmd %d done (flags=%08x error=%d)\n",
	    device_xname(sc->sc_dev),
	    cmd->c_opcode, cmd->c_flags, cmd->c_error));
}

static void
mvsdio_card_enable_intr(sdmmc_chipset_handle_t sch, int enable)
{
	struct mvsdio_softc *sc = (struct mvsdio_softc *)sch;
	uint32_t reg;

	mutex_enter(&sc->sc_mtx);
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISIE);
	reg |= NIS_CARDINT;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_NISIE, reg);
	mutex_exit(&sc->sc_mtx);
}

static void
mvsdio_card_intr_ack(sdmmc_chipset_handle_t sch)
{

	/* Nothing */
}


static void
mvsdio_wininit(struct mvsdio_softc *sc, enum marvell_tags *tags)
{
	uint64_t base;
	uint32_t size;
	int window, target, attr, rv, i;

	for (window = 0, i = 0;
	    tags[i] != MARVELL_TAG_UNDEFINED && window < MVSDIO_NWINDOW; i++) {
		rv = marvell_winparams_by_tag(sc->sc_dev, tags[i],
		    &target, &attr, &base, &size);
		if (rv != 0 || size == 0)
			continue;

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_WC(window),
		    WC_WINEN		|
		    WC_TARGET(target)	|
		    WC_ATTR(attr)	|
		    WC_SIZE(size));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_WB(window),
		    WB_BASE(base));
		window++;
	}
	for (; window < MVSDIO_NWINDOW; window++)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVSDIO_WC(window), 0);
}
