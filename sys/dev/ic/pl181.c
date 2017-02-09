/* $NetBSD: pl181.c,v 1.1 2015/01/27 16:33:26 jmcneill Exp $ */

/*-
 * Copyright (c) 2015 Jared D. McNeill <jmcneill@invisible.ca>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pl181.c,v 1.1 2015/01/27 16:33:26 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <dev/sdmmc/sdmmcvar.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmc_ioreg.h>

#include <dev/ic/pl181reg.h>
#include <dev/ic/pl181var.h>

static int	plmmc_host_reset(sdmmc_chipset_handle_t);
static uint32_t	plmmc_host_ocr(sdmmc_chipset_handle_t);
static int	plmmc_host_maxblklen(sdmmc_chipset_handle_t);
static int	plmmc_card_detect(sdmmc_chipset_handle_t);
static int	plmmc_write_protect(sdmmc_chipset_handle_t);
static int	plmmc_bus_power(sdmmc_chipset_handle_t, uint32_t);
static int	plmmc_bus_clock(sdmmc_chipset_handle_t, int);
static int	plmmc_bus_width(sdmmc_chipset_handle_t, int);
static int	plmmc_bus_rod(sdmmc_chipset_handle_t, int);
static void	plmmc_exec_command(sdmmc_chipset_handle_t,
				     struct sdmmc_command *);
static void	plmmc_card_enable_intr(sdmmc_chipset_handle_t, int);
static void	plmmc_card_intr_ack(sdmmc_chipset_handle_t);

static int	plmmc_wait_status(struct plmmc_softc *, uint32_t, int);
static int	plmmc_pio_wait(struct plmmc_softc *,
				 struct sdmmc_command *);
static int	plmmc_pio_transfer(struct plmmc_softc *,
				     struct sdmmc_command *);

static struct sdmmc_chip_functions plmmc_chip_functions = {
	.host_reset = plmmc_host_reset,
	.host_ocr = plmmc_host_ocr,
	.host_maxblklen = plmmc_host_maxblklen,
	.card_detect = plmmc_card_detect,
	.write_protect = plmmc_write_protect,
	.bus_power = plmmc_bus_power,
	.bus_clock = plmmc_bus_clock,
	.bus_width = plmmc_bus_width,
	.bus_rod = plmmc_bus_rod,
	.exec_command = plmmc_exec_command,
	.card_enable_intr = plmmc_card_enable_intr,
	.card_intr_ack = plmmc_card_intr_ack,
};

#define MMCI_WRITE(sc, reg, val) \
	bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))
#define MMCI_READ(sc, reg) \
	bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (reg))

void
plmmc_init(struct plmmc_softc *sc)
{
	struct sdmmcbus_attach_args saa;

	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_BIO);
	cv_init(&sc->sc_intr_cv, "plmmcirq");

#ifdef PLMMC_DEBUG
	device_printf(sc->sc_dev, "PeriphID %#x %#x %#x %#x\n",
	    MMCI_READ(sc, MMCI_PERIPH_ID0_REG),
	    MMCI_READ(sc, MMCI_PERIPH_ID1_REG),
	    MMCI_READ(sc, MMCI_PERIPH_ID2_REG),
	    MMCI_READ(sc, MMCI_PERIPH_ID3_REG));
	device_printf(sc->sc_dev, "PCellID %#x %#x %#x %#x\n",
	    MMCI_READ(sc, MMCI_PCELL_ID0_REG),
	    MMCI_READ(sc, MMCI_PCELL_ID1_REG),
	    MMCI_READ(sc, MMCI_PCELL_ID2_REG),
	    MMCI_READ(sc, MMCI_PCELL_ID3_REG));
#endif

	plmmc_bus_clock(sc, 400);
	MMCI_WRITE(sc, MMCI_POWER_REG, 0);
	delay(10000);
	MMCI_WRITE(sc, MMCI_POWER_REG, MMCI_POWER_CTRL_POWERUP);
	delay(10000);
	MMCI_WRITE(sc, MMCI_POWER_REG, MMCI_POWER_CTRL_POWERON);
	plmmc_host_reset(sc);

	memset(&saa, 0, sizeof(saa));
	saa.saa_busname = "sdmmc";
	saa.saa_sct = &plmmc_chip_functions;
	saa.saa_sch = sc;
	saa.saa_clkmin = 400;
	saa.saa_clkmax = sc->sc_clock_freq / 1000;
	saa.saa_caps = 0;

	sc->sc_sdmmc_dev = config_found(sc->sc_dev, &saa, NULL);
}

int
plmmc_intr(void *priv)
{
	struct plmmc_softc *sc = priv;
	uint32_t status;

	mutex_enter(&sc->sc_intr_lock);
	status = MMCI_READ(sc, MMCI_STATUS_REG);
#ifdef PLMMC_DEBUG
	printf("%s: MMCI_STATUS_REG = %#x\n", __func__, status);
#endif
	if (!status) {
		mutex_exit(&sc->sc_intr_lock);
		return 0;
	}

	sc->sc_intr_status |= status;
	cv_broadcast(&sc->sc_intr_cv);

	mutex_exit(&sc->sc_intr_lock);

	return 1;
}

static int
plmmc_wait_status(struct plmmc_softc *sc, uint32_t mask, int timeout)
{
	int retry, error;

	KASSERT(mutex_owned(&sc->sc_intr_lock));

	if (sc->sc_intr_status & mask)
		return 0;

	retry = timeout / hz;
	if (sc->sc_ih == NULL)
		retry *= 1000;

	while (retry > 0) {
		if (sc->sc_ih == NULL) {
			sc->sc_intr_status |= MMCI_READ(sc, MMCI_STATUS_REG);
			if (sc->sc_intr_status & mask)
				return 0;
			delay(10000);
		} else {
			error = cv_timedwait(&sc->sc_intr_cv,
			    &sc->sc_intr_lock, hz);
			if (error && error != EWOULDBLOCK) {
				device_printf(sc->sc_dev,
				    "cv_timedwait returned %d\n", error);
				return error;
			}
			if (sc->sc_intr_status & mask)
				return 0;
		}
		--retry;
	}

	device_printf(sc->sc_dev, "%s timeout, MMCI_STATUS_REG = %#x\n",
	    __func__, MMCI_READ(sc, MMCI_STATUS_REG));

	return ETIMEDOUT;
}

static int
plmmc_pio_wait(struct plmmc_softc *sc, struct sdmmc_command *cmd)
{
	uint32_t bit = (cmd->c_flags & SCF_CMD_READ) ?
	    MMCI_INT_RX_DATA_AVAIL : MMCI_INT_TX_FIFO_EMPTY;

	MMCI_WRITE(sc, MMCI_CLEAR_REG, bit);
	const int error = plmmc_wait_status(sc,
		bit | MMCI_INT_DATA_END | MMCI_INT_DATA_BLOCK_END, hz*2);
	sc->sc_intr_status &= ~bit;

	return error;
}

static int
plmmc_pio_transfer(struct plmmc_softc *sc, struct sdmmc_command *cmd)
{
	uint32_t *datap = (uint32_t *)cmd->c_data;
	int i;

	cmd->c_resid = cmd->c_datalen;
	for (i = 0; i < (cmd->c_datalen >> 2); i++) {
		if (plmmc_pio_wait(sc, cmd))
			return ETIMEDOUT;
		if (cmd->c_flags & SCF_CMD_READ) {
			datap[i] = MMCI_READ(sc, MMCI_FIFO_REG);
		} else {
			MMCI_WRITE(sc, MMCI_FIFO_REG, datap[i]);
		}
		cmd->c_resid -= 4;
	}

	return 0;
}
				     
static int
plmmc_host_reset(sdmmc_chipset_handle_t sch)
{
	struct plmmc_softc *sc = sch;

	MMCI_WRITE(sc, MMCI_MASK0_REG, 0);
	MMCI_WRITE(sc, MMCI_MASK1_REG, 0);
	MMCI_WRITE(sc, MMCI_CLEAR_REG, 0xffffffff);

	return 0;
}

static uint32_t
plmmc_host_ocr(sdmmc_chipset_handle_t sch)
{
	return MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V;
}

static int
plmmc_host_maxblklen(sdmmc_chipset_handle_t sch)
{
	return 2048;
}

static int
plmmc_card_detect(sdmmc_chipset_handle_t sch)
{
	return 1;
}

static int
plmmc_write_protect(sdmmc_chipset_handle_t sch)
{
	return 0;
}

static int
plmmc_bus_power(sdmmc_chipset_handle_t sch, uint32_t ocr)
{
	return 0;
}

static int
plmmc_bus_clock(sdmmc_chipset_handle_t sch, int freq)
{
	struct plmmc_softc *sc = sch;
	u_int pll_freq, clk_div;
	uint32_t clock;

	clock = MMCI_CLOCK_PWRSAVE;
	if (freq) {
		pll_freq = sc->sc_clock_freq / 1000;
		clk_div = (howmany(pll_freq, freq) >> 1) - 1;
		clock |= __SHIFTIN(clk_div, MMCI_CLOCK_CLKDIV);
		clock |= MMCI_CLOCK_ENABLE;
	}
	MMCI_WRITE(sc, MMCI_CLOCK_REG, clock);

	return 0;
}

static int
plmmc_bus_width(sdmmc_chipset_handle_t sch, int width)
{
	return 0;
}

static int
plmmc_bus_rod(sdmmc_chipset_handle_t sch, int on)
{
	struct plmmc_softc *sc = sch;
	uint32_t power;


	power = MMCI_READ(sc, MMCI_POWER_REG);
	if (on) {
		power |= MMCI_POWER_ROD;
	} else {
		power &= ~MMCI_POWER_ROD;
	}
	MMCI_WRITE(sc, MMCI_POWER_REG, power);

	return 0;
}

static void
plmmc_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct plmmc_softc *sc = sch;
	uint32_t cmdval = MMCI_COMMAND_ENABLE;

#ifdef PLMMC_DEBUG
	device_printf(sc->sc_dev, "opcode %d flags %#x datalen %d\n",
	    cmd->c_opcode, cmd->c_flags, cmd->c_datalen);
#endif

	mutex_enter(&sc->sc_intr_lock);

	MMCI_WRITE(sc, MMCI_COMMAND_REG, 0);
	MMCI_WRITE(sc, MMCI_MASK0_REG, 0);
	MMCI_WRITE(sc, MMCI_CLEAR_REG, 0xffffffff);
	MMCI_WRITE(sc, MMCI_MASK0_REG,
	    MMCI_INT_CMD_TIMEOUT | MMCI_INT_DATA_TIMEOUT |
	    MMCI_INT_RX_DATA_AVAIL | MMCI_INT_TX_FIFO_EMPTY |
	    MMCI_INT_DATA_END | MMCI_INT_DATA_BLOCK_END |
	    MMCI_INT_CMD_RESP_END | MMCI_INT_CMD_SENT);

	sc->sc_intr_status = 0;

	if (cmd->c_flags & SCF_RSP_PRESENT)
		cmdval |= MMCI_COMMAND_RESPONSE;
	if (cmd->c_flags & SCF_RSP_136)
		cmdval |= MMCI_COMMAND_LONGRSP;

	if (cmd->c_datalen > 0) {
		unsigned int nblks = cmd->c_datalen / cmd->c_blklen;
		if (nblks == 0 || (cmd->c_datalen % cmd->c_blklen) != 0)
			++nblks;

		const uint32_t dir = (cmd->c_flags & SCF_CMD_READ) ? 1 : 0;
		const uint32_t blksize = ffs(cmd->c_blklen) - 1;

		MMCI_WRITE(sc, MMCI_DATA_TIMER_REG, 0xffffffff);
		MMCI_WRITE(sc, MMCI_DATA_LENGTH_REG, nblks * cmd->c_blklen);
		MMCI_WRITE(sc, MMCI_DATA_CTRL_REG,
		    __SHIFTIN(dir, MMCI_DATA_CTRL_DIRECTION) |
		    __SHIFTIN(blksize, MMCI_DATA_CTRL_BLOCKSIZE) |
		    MMCI_DATA_CTRL_ENABLE);
	}

	MMCI_WRITE(sc, MMCI_ARGUMENT_REG, cmd->c_arg);
	MMCI_WRITE(sc, MMCI_COMMAND_REG, cmdval | cmd->c_opcode);

	if (cmd->c_datalen > 0) {
		cmd->c_error = plmmc_pio_transfer(sc, cmd);
		if (cmd->c_error) {
			device_printf(sc->sc_dev,
			    "error (%d) waiting for xfer\n", cmd->c_error);
			goto done;
		}
	}

	if (cmd->c_flags & SCF_RSP_PRESENT) {
		cmd->c_error = plmmc_wait_status(sc,
		    MMCI_INT_CMD_RESP_END|MMCI_INT_CMD_TIMEOUT, hz * 2);
		if (cmd->c_error == 0 &&
		    (sc->sc_intr_status & MMCI_INT_CMD_TIMEOUT)) {
			cmd->c_error = ETIMEDOUT;
		}
		if (cmd->c_error) {
#ifdef PLMMC_DEBUG
			device_printf(sc->sc_dev,
			    "error (%d) waiting for resp\n", cmd->c_error);
#endif
			goto done;
		}

		if (cmd->c_flags & SCF_RSP_136) {
			cmd->c_resp[3] = MMCI_READ(sc, MMCI_RESP0_REG);
			cmd->c_resp[2] = MMCI_READ(sc, MMCI_RESP1_REG);
			cmd->c_resp[1] = MMCI_READ(sc, MMCI_RESP2_REG);
			cmd->c_resp[0] = MMCI_READ(sc, MMCI_RESP3_REG);
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
			cmd->c_resp[0] = MMCI_READ(sc, MMCI_RESP0_REG);
		}
	}

done:
	cmd->c_flags |= SCF_ITSDONE;
	MMCI_WRITE(sc, MMCI_COMMAND_REG, 0);
	MMCI_WRITE(sc, MMCI_MASK0_REG, 0);
	MMCI_WRITE(sc, MMCI_CLEAR_REG, 0xffffffff);
	MMCI_WRITE(sc, MMCI_DATA_CNT_REG, 0);

#ifdef PLMMC_DEBUG
	device_printf(sc->sc_dev, "MMCI_STATUS_REG = %#x\n",
	    MMCI_READ(sc, MMCI_STATUS_REG));
#endif
	mutex_exit(&sc->sc_intr_lock);
}

static void
plmmc_card_enable_intr(sdmmc_chipset_handle_t sch, int enable)
{
}

static void
plmmc_card_intr_ack(sdmmc_chipset_handle_t sch)
{
}
