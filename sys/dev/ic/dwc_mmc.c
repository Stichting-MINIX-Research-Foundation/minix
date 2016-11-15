/* $NetBSD: dwc_mmc.c,v 1.7 2015/08/09 13:01:21 jmcneill Exp $ */

/*-
 * Copyright (c) 2014 Jared D. McNeill <jmcneill@invisible.ca>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_dwc_mmc.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dwc_mmc.c,v 1.7 2015/08/09 13:01:21 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <dev/sdmmc/sdmmcvar.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmc_ioreg.h>

#include <dev/ic/dwc_mmc_reg.h>
#include <dev/ic/dwc_mmc_var.h>

static int	dwc_mmc_host_reset(sdmmc_chipset_handle_t);
static uint32_t	dwc_mmc_host_ocr(sdmmc_chipset_handle_t);
static int	dwc_mmc_host_maxblklen(sdmmc_chipset_handle_t);
static int	dwc_mmc_card_detect(sdmmc_chipset_handle_t);
static int	dwc_mmc_write_protect(sdmmc_chipset_handle_t);
static int	dwc_mmc_bus_power(sdmmc_chipset_handle_t, uint32_t);
static int	dwc_mmc_bus_clock(sdmmc_chipset_handle_t, int);
static int	dwc_mmc_bus_width(sdmmc_chipset_handle_t, int);
static int	dwc_mmc_bus_rod(sdmmc_chipset_handle_t, int);
static void	dwc_mmc_exec_command(sdmmc_chipset_handle_t,
				     struct sdmmc_command *);
static void	dwc_mmc_card_enable_intr(sdmmc_chipset_handle_t, int);
static void	dwc_mmc_card_intr_ack(sdmmc_chipset_handle_t);

static int	dwc_mmc_set_clock(struct dwc_mmc_softc *, u_int);
static int	dwc_mmc_update_clock(struct dwc_mmc_softc *);
static int	dwc_mmc_wait_rint(struct dwc_mmc_softc *, uint32_t, int);
static int	dwc_mmc_pio_wait(struct dwc_mmc_softc *,
				 struct sdmmc_command *);
static int	dwc_mmc_pio_transfer(struct dwc_mmc_softc *,
				     struct sdmmc_command *);

#ifdef DWC_MMC_DEBUG
static void	dwc_mmc_print_rint(struct dwc_mmc_softc *, const char *,
				   uint32_t);
#endif

void		dwc_mmc_dump_regs(void);

static struct sdmmc_chip_functions dwc_mmc_chip_functions = {
	.host_reset = dwc_mmc_host_reset,
	.host_ocr = dwc_mmc_host_ocr,
	.host_maxblklen = dwc_mmc_host_maxblklen,
	.card_detect = dwc_mmc_card_detect,
	.write_protect = dwc_mmc_write_protect,
	.bus_power = dwc_mmc_bus_power,
	.bus_clock = dwc_mmc_bus_clock,
	.bus_width = dwc_mmc_bus_width,
	.bus_rod = dwc_mmc_bus_rod,
	.exec_command = dwc_mmc_exec_command,
	.card_enable_intr = dwc_mmc_card_enable_intr,
	.card_intr_ack = dwc_mmc_card_intr_ack,
};

#define MMC_WRITE(sc, reg, val) \
	bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))
#define MMC_READ(sc, reg) \
	bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (reg))

void
dwc_mmc_init(struct dwc_mmc_softc *sc)
{
	struct sdmmcbus_attach_args saa;

	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_BIO);
	cv_init(&sc->sc_intr_cv, "dwcmmcirq");

	dwc_mmc_host_reset(sc);
	dwc_mmc_bus_width(sc, 1);

	memset(&saa, 0, sizeof(saa));
	saa.saa_busname = "sdmmc";
	saa.saa_sct = &dwc_mmc_chip_functions;
	saa.saa_sch = sc;
	saa.saa_clkmin = 400;
	if (sc->sc_clock_max) {
		saa.saa_clkmax = sc->sc_clock_max;
	} else {
		saa.saa_clkmax = sc->sc_clock_freq / 1000;
	}
	saa.saa_caps = SMC_CAPS_4BIT_MODE|
		       SMC_CAPS_8BIT_MODE|
		       SMC_CAPS_SD_HIGHSPEED|
		       SMC_CAPS_MMC_HIGHSPEED|
		       SMC_CAPS_AUTO_STOP;

#if notyet
	saa.saa_dmat = sc->sc_dmat;
	saa.saa_caps |= SMC_CAPS_DMA|
			SMC_CAPS_MULTI_SEG_DMA;
#endif

	sc->sc_sdmmc_dev = config_found(sc->sc_dev, &saa, NULL);
}

int
dwc_mmc_intr(void *priv)
{
	struct dwc_mmc_softc *sc = priv;
	uint32_t mint, rint;

	mutex_enter(&sc->sc_intr_lock);
	rint = MMC_READ(sc, DWC_MMC_RINTSTS_REG);
	mint = MMC_READ(sc, DWC_MMC_MINTSTS_REG);
	if (!rint && !mint) {
		mutex_exit(&sc->sc_intr_lock);
		return 0;
	}
	MMC_WRITE(sc, DWC_MMC_RINTSTS_REG, rint);
	MMC_WRITE(sc, DWC_MMC_MINTSTS_REG, mint);

#ifdef DWC_MMC_DEBUG
	dwc_mmc_print_rint(sc, "irq", rint);
#endif

	if (rint & DWC_MMC_INT_CARDDET) {
		rint &= ~DWC_MMC_INT_CARDDET;
		if (sc->sc_sdmmc_dev) {
			sdmmc_needs_discover(sc->sc_sdmmc_dev);
		}
	}

	if (rint) {
		sc->sc_intr_rint |= rint;
		cv_broadcast(&sc->sc_intr_cv);
	}

	mutex_exit(&sc->sc_intr_lock);

	return 1;
}

static int
dwc_mmc_set_clock(struct dwc_mmc_softc *sc, u_int freq)
{
	u_int pll_freq, clk_div;

	pll_freq = sc->sc_clock_freq / 1000;
	clk_div = (pll_freq / freq) >> 1;
	if (pll_freq % freq)
		clk_div++;

	MMC_WRITE(sc, DWC_MMC_CLKDIV_REG,
	    __SHIFTIN(clk_div, DWC_MMC_CLKDIV_CLK_DIVIDER0));
	return dwc_mmc_update_clock(sc);
}

static int
dwc_mmc_update_clock(struct dwc_mmc_softc *sc)
{
	uint32_t cmd;
	int retry;

	cmd = DWC_MMC_CMD_START_CMD |
	      DWC_MMC_CMD_UPDATE_CLOCK_REGS_ONLY |
	      DWC_MMC_CMD_WAIT_PRVDATA_COMPLETE;

	if (sc->sc_flags & DWC_MMC_F_USE_HOLD_REG)
		cmd |= DWC_MMC_CMD_USE_HOLD_REG;

	MMC_WRITE(sc, DWC_MMC_CMD_REG, cmd);
	retry = 0xfffff;
	while (--retry > 0) {
		cmd = MMC_READ(sc, DWC_MMC_CMD_REG);
		if ((cmd & DWC_MMC_CMD_START_CMD) == 0)
			break;
		delay(10);
	}

	if (retry == 0) {
		device_printf(sc->sc_dev, "timeout updating clock\n");
		return ETIMEDOUT;
	}

	return 0;
}

static int
dwc_mmc_wait_rint(struct dwc_mmc_softc *sc, uint32_t mask, int timeout)
{
	int retry, error;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (sc->sc_intr_rint & mask)
		return 0;

	retry = timeout / hz;

	while (retry > 0) {
		error = cv_timedwait(&sc->sc_intr_cv, &sc->sc_intr_lock, hz);
		if (error && error != EWOULDBLOCK)
			return error;
		if (sc->sc_intr_rint & mask)
			return 0;
		--retry;
	}

	return ETIMEDOUT;
}

static int
dwc_mmc_pio_wait(struct dwc_mmc_softc *sc, struct sdmmc_command *cmd)
{
	int retry = 0xfffff;
	uint32_t bit = (cmd->c_flags & SCF_CMD_READ) ?
	    DWC_MMC_STATUS_FIFO_EMPTY : DWC_MMC_STATUS_FIFO_FULL;

	while (--retry > 0) {
		uint32_t status = MMC_READ(sc, DWC_MMC_STATUS_REG);
		if (!(status & bit))
			return 0;
		delay(10);
	}

#ifdef DWC_MMC_DEBUG
	device_printf(sc->sc_dev, "%s: timed out\n", __func__);
#endif

	return ETIMEDOUT;
}

static int
dwc_mmc_pio_transfer(struct dwc_mmc_softc *sc, struct sdmmc_command *cmd)
{
	uint32_t *datap = (uint32_t *)cmd->c_data;
	int i;

	for (i = 0; i < (cmd->c_resid >> 2); i++) {
		if (dwc_mmc_pio_wait(sc, cmd))
			return ETIMEDOUT;
		if (cmd->c_flags & SCF_CMD_READ) {
			datap[i] = MMC_READ(sc, DWC_MMC_FIFO_BASE_REG);
		} else {
			MMC_WRITE(sc, DWC_MMC_FIFO_BASE_REG, datap[i]);
		}
	}

	return 0;
}
				     
static int
dwc_mmc_host_reset(sdmmc_chipset_handle_t sch)
{
	struct dwc_mmc_softc *sc = sch;
	int retry = 1000;
	uint32_t ctrl, fifoth;
	uint32_t rx_wmark, tx_wmark;

	if (sc->sc_flags & DWC_MMC_F_PWREN_CLEAR) {
		MMC_WRITE(sc, DWC_MMC_PWREN_REG, 0);
	} else {
		MMC_WRITE(sc, DWC_MMC_PWREN_REG, DWC_MMC_PWREN_POWER_ENABLE);
	}

	MMC_WRITE(sc, DWC_MMC_CTRL_REG, DWC_MMC_CTRL_RESET_ALL);
	while (--retry > 0) {
		ctrl = MMC_READ(sc, DWC_MMC_CTRL_REG);
		if ((ctrl & DWC_MMC_CTRL_RESET_ALL) == 0)
			break;
		delay(100);
	}

	MMC_WRITE(sc, DWC_MMC_CLKSRC_REG, 0);

	MMC_WRITE(sc, DWC_MMC_TMOUT_REG, 0xffffff40);
	MMC_WRITE(sc, DWC_MMC_RINTSTS_REG, 0xffffffff);

	MMC_WRITE(sc, DWC_MMC_INTMASK_REG,
	    DWC_MMC_INT_CD | DWC_MMC_INT_ACD | DWC_MMC_INT_DTO |
	    DWC_MMC_INT_ERROR | DWC_MMC_INT_CARDDET |
	    DWC_MMC_INT_RXDR | DWC_MMC_INT_TXDR);

	rx_wmark = (sc->sc_fifo_depth / 2) - 1;
	tx_wmark = sc->sc_fifo_depth / 2;
	fifoth = __SHIFTIN(DWC_MMC_FIFOTH_DMA_MULTIPLE_TXN_SIZE_16,
			   DWC_MMC_FIFOTH_DMA_MULTIPLE_TXN_SIZE);
	fifoth |= __SHIFTIN(rx_wmark, DWC_MMC_FIFOTH_RX_WMARK);
	fifoth |= __SHIFTIN(tx_wmark, DWC_MMC_FIFOTH_TX_WMARK);
	MMC_WRITE(sc, DWC_MMC_FIFOTH_REG, fifoth);

	ctrl = MMC_READ(sc, DWC_MMC_CTRL_REG);
	ctrl |= DWC_MMC_CTRL_INT_ENABLE;
	MMC_WRITE(sc, DWC_MMC_CTRL_REG, ctrl);

	return 0;
}

static uint32_t
dwc_mmc_host_ocr(sdmmc_chipset_handle_t sch)
{
	return MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V;
}

static int
dwc_mmc_host_maxblklen(sdmmc_chipset_handle_t sch)
{
	return 32768;
}

static int
dwc_mmc_card_detect(sdmmc_chipset_handle_t sch)
{
	struct dwc_mmc_softc *sc = sch;
	uint32_t cdetect;

	cdetect = MMC_READ(sc, DWC_MMC_CDETECT_REG);
	return !(cdetect & DWC_MMC_CDETECT_CARD_DETECT_N);
}

static int
dwc_mmc_write_protect(sdmmc_chipset_handle_t sch)
{
	struct dwc_mmc_softc *sc = sch;
	uint32_t wrtprt;

	wrtprt = MMC_READ(sc, DWC_MMC_WRTPRT_REG);
	return !!(wrtprt & DWC_MMC_WRTPRT_WRITE_PROTECT);
}

static int
dwc_mmc_bus_power(sdmmc_chipset_handle_t sch, uint32_t ocr)
{
	return 0;
}

static int
dwc_mmc_bus_clock(sdmmc_chipset_handle_t sch, int freq)
{
	struct dwc_mmc_softc *sc = sch;
	uint32_t clkena;

#ifdef DWC_MMC_DEBUG
	device_printf(sc->sc_dev, "%s: freq %d\n", __func__, freq);
#endif

	MMC_WRITE(sc, DWC_MMC_CLKENA_REG, 0);
	if (dwc_mmc_update_clock(sc) != 0)
		return ETIMEDOUT;

	if (freq) {
		if (dwc_mmc_set_clock(sc, freq) != 0)
			return EIO;

		clkena = DWC_MMC_CLKENA_CCLK_ENABLE;
		clkena |= DWC_MMC_CLKENA_CCLK_LOW_POWER; /* XXX SD/MMC only */
		MMC_WRITE(sc, DWC_MMC_CLKENA_REG, clkena);
		if (dwc_mmc_update_clock(sc) != 0)
			return ETIMEDOUT;
	}

	delay(1000);

	sc->sc_cur_freq = freq;

	return 0;
}

static int
dwc_mmc_bus_width(sdmmc_chipset_handle_t sch, int width)
{
	struct dwc_mmc_softc *sc = sch;
	uint32_t ctype;

	switch (width) {
	case 1:
		ctype = DWC_MMC_CTYPE_CARD_WIDTH_1;
		break;
	case 4:
		ctype = DWC_MMC_CTYPE_CARD_WIDTH_4;
		break;
	case 8:
		ctype = DWC_MMC_CTYPE_CARD_WIDTH_8;
		break;
	default:
		return EINVAL;
	}

	MMC_WRITE(sc, DWC_MMC_CTYPE_REG, ctype);

	return 0;
}

static int
dwc_mmc_bus_rod(sdmmc_chipset_handle_t sch, int on)
{
	return ENOTSUP;
}

static void
dwc_mmc_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct dwc_mmc_softc *sc = sch;
	uint32_t cmdval = DWC_MMC_CMD_START_CMD;
	uint32_t ctrl;

#ifdef DWC_MMC_DEBUG
	device_printf(sc->sc_dev, "exec opcode=%d flags=%#x\n",
	    cmd->c_opcode, cmd->c_flags);
#endif

	if (sc->sc_flags & DWC_MMC_F_FORCE_CLK) {
		cmd->c_error = dwc_mmc_bus_clock(sc, sc->sc_cur_freq);
		if (cmd->c_error)
			return;
	}

	if (sc->sc_flags & DWC_MMC_F_USE_HOLD_REG)
		cmdval |= DWC_MMC_CMD_USE_HOLD_REG;

	mutex_enter(&sc->sc_intr_lock);
	if (cmd->c_opcode == 0)
		cmdval |= DWC_MMC_CMD_SEND_INIT;
	if (cmd->c_flags & SCF_RSP_PRESENT)
		cmdval |= DWC_MMC_CMD_RESP_EXPECTED;
	if (cmd->c_flags & SCF_RSP_136)
		cmdval |= DWC_MMC_CMD_RESP_LEN;
	if (cmd->c_flags & SCF_RSP_CRC)
		cmdval |= DWC_MMC_CMD_CHECK_RESP_CRC;

	if (cmd->c_datalen > 0) {
		unsigned int nblks;

		cmdval |= DWC_MMC_CMD_DATA_EXPECTED;
		cmdval |= DWC_MMC_CMD_WAIT_PRVDATA_COMPLETE;
		if (!ISSET(cmd->c_flags, SCF_CMD_READ)) {
			cmdval |= DWC_MMC_CMD_WR;
		}

		nblks = cmd->c_datalen / cmd->c_blklen;
		if (nblks == 0 || (cmd->c_datalen % cmd->c_blklen) != 0)
			++nblks;

		if (nblks > 1) {
			cmdval |= DWC_MMC_CMD_SEND_AUTO_STOP;
		}

		MMC_WRITE(sc, DWC_MMC_BLKSIZ_REG, cmd->c_blklen);
		MMC_WRITE(sc, DWC_MMC_BYTCNT_REG, nblks * cmd->c_blklen);
	}

	sc->sc_intr_rint = 0;

	MMC_WRITE(sc, DWC_MMC_CMDARG_REG, cmd->c_arg);

	cmd->c_resid = cmd->c_datalen;
	MMC_WRITE(sc, DWC_MMC_CMD_REG, cmdval | cmd->c_opcode);

	cmd->c_error = dwc_mmc_wait_rint(sc,
	    DWC_MMC_INT_ERROR|DWC_MMC_INT_CD, hz * 10);
	if (cmd->c_error == 0 && (sc->sc_intr_rint & DWC_MMC_INT_ERROR)) {
#ifdef DWC_MMC_DEBUG
		dwc_mmc_print_rint(sc, "exec1", sc->sc_intr_rint);
#endif
		if (sc->sc_intr_rint & DWC_MMC_INT_RTO) {
			cmd->c_error = ETIMEDOUT;
		} else {
			cmd->c_error = EIO;
		}
	}
	if (cmd->c_error) {
		goto done;
	}

	if (cmd->c_datalen > 0) {
		cmd->c_error = dwc_mmc_pio_transfer(sc, cmd);
		if (cmd->c_error) {
			goto done;
		}

		cmd->c_error = dwc_mmc_wait_rint(sc,
		    DWC_MMC_INT_ERROR|DWC_MMC_INT_ACD|DWC_MMC_INT_DTO,
		    hz * 10);
		if (cmd->c_error == 0 &&
		    (sc->sc_intr_rint & DWC_MMC_INT_ERROR)) {
#ifdef DWC_MMC_DEBUG
			dwc_mmc_print_rint(sc, "exec2", sc->sc_intr_rint);
#endif
			cmd->c_error = ETIMEDOUT;
		}
		if (cmd->c_error) {
			goto done;
		}
	}

	if (cmd->c_flags & SCF_RSP_PRESENT) {
		if (cmd->c_flags & SCF_RSP_136) {
			cmd->c_resp[0] = MMC_READ(sc, DWC_MMC_RESP0_REG);
			cmd->c_resp[1] = MMC_READ(sc, DWC_MMC_RESP1_REG);
			cmd->c_resp[2] = MMC_READ(sc, DWC_MMC_RESP2_REG);
			cmd->c_resp[3] = MMC_READ(sc, DWC_MMC_RESP3_REG);
			if (cmd->c_flags & SCF_RSP_CRC) {
				cmd->c_resp[0] = (cmd->c_resp[0] >> 8) |
				    (cmd->c_resp[1] << 24);
				cmd->c_resp[1] = (cmd->c_resp[1] >> 8) |
				    (cmd->c_resp[2] << 24);
				cmd->c_resp[2] = (cmd->c_resp[2] >> 8) |
				    (cmd->c_resp[3] << 24);
				cmd->c_resp[3] = (cmd->c_resp[3] >> 8);
			}
		} else {
			cmd->c_resp[0] = MMC_READ(sc, DWC_MMC_RESP0_REG);
		}
	}

done:
	cmd->c_flags |= SCF_ITSDONE;
	mutex_exit(&sc->sc_intr_lock);

	ctrl = MMC_READ(sc, DWC_MMC_CTRL_REG);
	ctrl |= DWC_MMC_CTRL_FIFO_RESET;
	MMC_WRITE(sc, DWC_MMC_CTRL_REG, ctrl);
}

static void
dwc_mmc_card_enable_intr(sdmmc_chipset_handle_t sch, int enable)
{
}

static void
dwc_mmc_card_intr_ack(sdmmc_chipset_handle_t sch)
{
}

#ifdef DWC_MMC_DEBUG
static void
dwc_mmc_print_rint(struct dwc_mmc_softc *sc, const char *tag, uint32_t rint)
{
	char buf[128];
	snprintb(buf, sizeof(buf), DWC_MMC_INT_BITS, rint);
	device_printf(sc->sc_dev, "[%s] rint %s\n", tag, buf);
}
#endif

void
dwc_mmc_dump_regs(void)
{
	static const struct {
		const char *name;
		unsigned int reg;
	} regs[] = {
		{ "CTRL", DWC_MMC_CTRL_REG },
		{ "PWREN", DWC_MMC_PWREN_REG },
		{ "CLKDIV", DWC_MMC_CLKDIV_REG },
		{ "CLKENA", DWC_MMC_CLKENA_REG },
		{ "TMOUT", DWC_MMC_TMOUT_REG },
		{ "CTYPE", DWC_MMC_CTYPE_REG },
		{ "BLKSIZ", DWC_MMC_BLKSIZ_REG },
		{ "BYTCNT", DWC_MMC_BYTCNT_REG },
		{ "INTMASK", DWC_MMC_INTMASK_REG },
		{ "MINTSTS", DWC_MMC_MINTSTS_REG },
		{ "RINTSTS", DWC_MMC_RINTSTS_REG },
		{ "STATUS", DWC_MMC_STATUS_REG },
		{ "CDETECT", DWC_MMC_CDETECT_REG },
		{ "WRTPRT", DWC_MMC_WRTPRT_REG },
		{ "USRID", DWC_MMC_USRID_REG },
		{ "VERID", DWC_MMC_VERID_REG },
		{ "RST", DWC_MMC_RST_REG },
		{ "BACK_END_POWER", DWC_MMC_BACK_END_POWER_REG },
	};
	device_t self = device_find_by_driver_unit("dwcmmc", 0);
	if (self == NULL)
		return;
	struct dwc_mmc_softc *sc = device_private(self);
	int i;

	for (i = 0; i < __arraycount(regs); i++) {
		device_printf(sc->sc_dev, "%s: %#x\n", regs[i].name,
		    MMC_READ(sc, regs[i].reg));
	}
}
