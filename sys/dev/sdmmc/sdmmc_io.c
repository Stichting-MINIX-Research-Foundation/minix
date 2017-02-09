/*	$NetBSD: sdmmc_io.c,v 1.12 2015/10/06 14:32:51 mlelstv Exp $	*/
/*	$OpenBSD: sdmmc_io.c,v 1.10 2007/09/17 01:33:33 krw Exp $	*/

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

/* Routines for SD I/O cards. */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sdmmc_io.c,v 1.12 2015/10/06 14:32:51 mlelstv Exp $");

#ifdef _KERNEL_OPT
#include "opt_sdmmc.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <dev/sdmmc/sdmmc_ioreg.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

#ifdef SDMMC_DEBUG
#define DPRINTF(s)	do { printf s; } while (0)
#else
#define DPRINTF(s)	do {} while (0)
#endif

struct sdmmc_intr_handler {
	struct sdmmc_softc *ih_softc;
	char *ih_name;
	int (*ih_fun)(void *);
	void *ih_arg;
	TAILQ_ENTRY(sdmmc_intr_handler) entry;
};

static int	sdmmc_io_rw_direct(struct sdmmc_softc *,
		    struct sdmmc_function *, int, u_char *, int);
static int	sdmmc_io_rw_extended(struct sdmmc_softc *,
		    struct sdmmc_function *, int, u_char *, int, int);
#if 0
static int	sdmmc_io_xchg(struct sdmmc_softc *, struct sdmmc_function *,
		    int, u_char *);
#endif
static void	sdmmc_io_reset(struct sdmmc_softc *);
static int	sdmmc_io_send_op_cond(struct sdmmc_softc *, uint32_t,
		    uint32_t *);

/*
 * Initialize SD I/O card functions (before memory cards).  The host
 * system and controller must support card interrupts in order to use
 * I/O functions.
 */
int
sdmmc_io_enable(struct sdmmc_softc *sc)
{
	uint32_t host_ocr;
	uint32_t card_ocr;
	int error;

	SDMMC_LOCK(sc);

	/* Set host mode to SD "combo" card. */
	SET(sc->sc_flags, SMF_SD_MODE|SMF_IO_MODE|SMF_MEM_MODE);

	/* Reset I/O functions. */
	sdmmc_io_reset(sc);

	/*
	 * Read the I/O OCR value, determine the number of I/O
	 * functions and whether memory is also present (a "combo
	 * card") by issuing CMD5.  SD memory-only and MMC cards
	 * do not respond to CMD5.
	 */
	error = sdmmc_io_send_op_cond(sc, 0, &card_ocr);
	if (error) {
		/* No SDIO card; switch to SD memory-only mode. */
		CLR(sc->sc_flags, SMF_IO_MODE);
		error = 0;
		goto out;
	}

	/* Parse the additional bits in the I/O OCR value. */
	if (!ISSET(card_ocr, SD_IO_OCR_MEM_PRESENT)) {
		/* SDIO card without memory (not a "combo card"). */
		DPRINTF(("%s: no memory present\n", SDMMCDEVNAME(sc)));
		CLR(sc->sc_flags, SMF_MEM_MODE);
	}
	sc->sc_function_count = SD_IO_OCR_NUM_FUNCTIONS(card_ocr);
	if (sc->sc_function_count == 0) {
		/* Useless SDIO card without any I/O functions. */
		DPRINTF(("%s: no I/O functions\n", SDMMCDEVNAME(sc)));
		CLR(sc->sc_flags, SMF_IO_MODE);
		error = 0;
		goto out;
	}
	card_ocr &= SD_IO_OCR_MASK;

	/* Set the lowest voltage supported by the card and host. */
	host_ocr = sdmmc_chip_host_ocr(sc->sc_sct, sc->sc_sch);
	error = sdmmc_set_bus_power(sc, host_ocr, card_ocr);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't supply voltage requested by card\n");
		goto out;
	}

	/* Reset I/O functions (again). */
	sdmmc_io_reset(sc);

	/* Send the new OCR value until all cards are ready. */
	error = sdmmc_io_send_op_cond(sc, host_ocr, NULL);
	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't send I/O OCR\n");
		goto out;
	}

out:
	SDMMC_UNLOCK(sc);

	return error;
}

/*
 * Allocate sdmmc_function structures for SD card I/O function
 * (including function 0).
 */
void
sdmmc_io_scan(struct sdmmc_softc *sc)
{
	struct sdmmc_function *sf0, *sf;
	int error;
	int i;

	SDMMC_LOCK(sc);

	sf0 = sdmmc_function_alloc(sc);
	sf0->number = 0;
	error = sdmmc_set_relative_addr(sc, sf0);
	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't set I/O RCA\n");
		SET(sf0->flags, SFF_ERROR);
		goto out;
	}
	sc->sc_fn0 = sf0;
	SIMPLEQ_INSERT_TAIL(&sc->sf_head, sf0, sf_list);

	/* Go to Data Transfer Mode, if possible. */
	sdmmc_chip_bus_rod(sc->sc_sct, sc->sc_sch, 0);

	/* Verify that the RCA has been set by selecting the card. */
	error = sdmmc_select_card(sc, sf0);
	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't select I/O RCA %d\n",
		    sf0->rca);
		SET(sf0->flags, SFF_ERROR);
		goto out;
	}

	for (i = 1; i <= sc->sc_function_count; i++) {
		sf = sdmmc_function_alloc(sc);
		sf->number = i;
		sf->rca = sf0->rca;
		SIMPLEQ_INSERT_TAIL(&sc->sf_head, sf, sf_list);
	}

out:
	SDMMC_UNLOCK(sc);
}

/*
 * Initialize SDIO card functions.
 */
int
sdmmc_io_init(struct sdmmc_softc *sc, struct sdmmc_function *sf)
{
	struct sdmmc_function *sf0 = sc->sc_fn0;
	int error = 0;
	uint8_t reg;

	SDMMC_LOCK(sc);

	if (sf->number == 0) {
		reg = sdmmc_io_read_1(sf, SD_IO_CCCR_CAPABILITY);
		if (!(reg & CCCR_CAPS_LSC) || (reg & CCCR_CAPS_4BLS)) {
			sdmmc_io_write_1(sf, SD_IO_CCCR_BUS_WIDTH,
			    CCCR_BUS_WIDTH_4);
			sf->width = 4;
		}

		error = sdmmc_read_cis(sf, &sf->cis);
		if (error) {
			aprint_error_dev(sc->sc_dev, "couldn't read CIS\n");
			SET(sf->flags, SFF_ERROR);
			goto out;
		}

		sdmmc_check_cis_quirks(sf);

#ifdef SDMMC_DEBUG
		if (sdmmcdebug)
			sdmmc_print_cis(sf);
#endif

		reg = sdmmc_io_read_1(sf, SD_IO_CCCR_HIGH_SPEED);
		if (reg & CCCR_HIGH_SPEED_SHS) {
			reg |= CCCR_HIGH_SPEED_EHS;
			sdmmc_io_write_1(sf, SD_IO_CCCR_HIGH_SPEED, reg);
			sf->csd.tran_speed = 50000;	/* 50MHz */

			/* Wait 400KHz x 8 clock */
			delay(1);
		}
		if (sc->sc_busclk > sf->csd.tran_speed)
			sc->sc_busclk = sf->csd.tran_speed;
		error =
		    sdmmc_chip_bus_clock(sc->sc_sct, sc->sc_sch, sc->sc_busclk,
			false);
		if (error)
			aprint_error_dev(sc->sc_dev,
			    "can't change bus clock\n");
	} else {
		reg = sdmmc_io_read_1(sf0, SD_IO_FBR(sf->number) + 0x000);
		sf->interface = FBR_STD_FUNC_IF_CODE(reg);
		if (sf->interface == 0x0f)
			sf->interface =
			    sdmmc_io_read_1(sf0, SD_IO_FBR(sf->number) + 0x001);
		error = sdmmc_read_cis(sf, &sf->cis);
		if (error) {
			aprint_error_dev(sc->sc_dev, "couldn't read CIS\n");
			SET(sf->flags, SFF_ERROR);
			goto out;
		}

		sdmmc_check_cis_quirks(sf);

#ifdef SDMMC_DEBUG
		if (sdmmcdebug)
			sdmmc_print_cis(sf);
#endif
	}

out:
	SDMMC_UNLOCK(sc);

	return error;
}

/*
 * Indicate whether the function is ready to operate.
 */
static int
sdmmc_io_function_ready(struct sdmmc_function *sf)
{
	struct sdmmc_softc *sc = sf->sc;
	struct sdmmc_function *sf0 = sc->sc_fn0;
	uint8_t reg;

	if (sf->number == 0)
		return 1;	/* FN0 is always ready */

	SDMMC_LOCK(sc);
	reg = sdmmc_io_read_1(sf0, SD_IO_CCCR_FN_IOREADY);
	SDMMC_UNLOCK(sc);
	return (reg & (1 << sf->number)) != 0;
}

int
sdmmc_io_function_enable(struct sdmmc_function *sf)
{
	struct sdmmc_softc *sc = sf->sc;
	struct sdmmc_function *sf0 = sc->sc_fn0;
	uint8_t reg;
	int retry;

	if (sf->number == 0)
		return 0;	/* FN0 is always enabled */

	SDMMC_LOCK(sc);
	reg = sdmmc_io_read_1(sf0, SD_IO_CCCR_FN_ENABLE);
	SET(reg, (1U << sf->number));
	sdmmc_io_write_1(sf0, SD_IO_CCCR_FN_ENABLE, reg);
	SDMMC_UNLOCK(sc);

	retry = 5;
	while (!sdmmc_io_function_ready(sf) && retry-- > 0)
		kpause("pause", false, hz, NULL);
	return (retry >= 0) ? 0 : ETIMEDOUT;
}

/*
 * Disable the I/O function.  Return zero if the function was
 * disabled successfully.
 */
void
sdmmc_io_function_disable(struct sdmmc_function *sf)
{
	struct sdmmc_softc *sc = sf->sc;
	struct sdmmc_function *sf0 = sc->sc_fn0;
	uint8_t reg;

	if (sf->number == 0)
		return;		/* FN0 is always enabled */

	SDMMC_LOCK(sc);
	reg = sdmmc_io_read_1(sf0, SD_IO_CCCR_FN_ENABLE);
	CLR(reg, (1U << sf->number));
	sdmmc_io_write_1(sf0, SD_IO_CCCR_FN_ENABLE, reg);
	SDMMC_UNLOCK(sc);
}

static int
sdmmc_io_rw_direct(struct sdmmc_softc *sc, struct sdmmc_function *sf,
    int reg, u_char *datap, int arg)
{
	struct sdmmc_command cmd;
	int error;

	/* Don't lock */

	/* Make sure the card is selected. */
	error = sdmmc_select_card(sc, sf);
	if (error)
		return error;

	arg |= ((sf == NULL ? 0 : sf->number) & SD_ARG_CMD52_FUNC_MASK) <<
	    SD_ARG_CMD52_FUNC_SHIFT;
	arg |= (reg & SD_ARG_CMD52_REG_MASK) <<
	    SD_ARG_CMD52_REG_SHIFT;
	arg |= (*datap & SD_ARG_CMD52_DATA_MASK) <<
	    SD_ARG_CMD52_DATA_SHIFT;

	memset(&cmd, 0, sizeof cmd);
	cmd.c_opcode = SD_IO_RW_DIRECT;
	cmd.c_arg = arg;
	cmd.c_flags = SCF_CMD_AC | SCF_RSP_R5;

	error = sdmmc_mmc_command(sc, &cmd);
	*datap = SD_R5_DATA(cmd.c_resp);

	return error;
}

/*
 * Useful values of `arg' to pass in are either SD_ARG_CMD53_READ or
 * SD_ARG_CMD53_WRITE.  SD_ARG_CMD53_INCREMENT may be ORed into `arg'
 * to access successive register locations instead of accessing the
 * same register many times.
 */
static int
sdmmc_io_rw_extended(struct sdmmc_softc *sc, struct sdmmc_function *sf,
    int reg, u_char *datap, int datalen, int arg)
{
	struct sdmmc_command cmd;
	int error;

	/* Don't lock */

#if 0
	/* Make sure the card is selected. */
	error = sdmmc_select_card(sc, sf);
	if (error)
		return error;
#endif

	arg |= (((sf == NULL) ? 0 : sf->number) & SD_ARG_CMD53_FUNC_MASK) <<
	    SD_ARG_CMD53_FUNC_SHIFT;
	arg |= (reg & SD_ARG_CMD53_REG_MASK) <<
	    SD_ARG_CMD53_REG_SHIFT;
	arg |= (datalen & SD_ARG_CMD53_LENGTH_MASK) <<
	    SD_ARG_CMD53_LENGTH_SHIFT;

	memset(&cmd, 0, sizeof cmd);
	cmd.c_opcode = SD_IO_RW_EXTENDED;
	cmd.c_arg = arg;
	cmd.c_flags = SCF_CMD_AC | SCF_RSP_R5;
	cmd.c_data = datap;
	cmd.c_datalen = datalen;
	cmd.c_blklen = MIN(datalen,
	    sdmmc_chip_host_maxblklen(sc->sc_sct,sc->sc_sch));
	if (!ISSET(arg, SD_ARG_CMD53_WRITE))
		cmd.c_flags |= SCF_CMD_READ;

	error = sdmmc_mmc_command(sc, &cmd);

	return error;
}

uint8_t
sdmmc_io_read_1(struct sdmmc_function *sf, int reg)
{
	uint8_t data = 0;

	/* Don't lock */

	(void)sdmmc_io_rw_direct(sf->sc, sf, reg, (u_char *)&data,
	    SD_ARG_CMD52_READ);
	return data;
}

void
sdmmc_io_write_1(struct sdmmc_function *sf, int reg, uint8_t data)
{

	/* Don't lock */

	(void)sdmmc_io_rw_direct(sf->sc, sf, reg, (u_char *)&data,
	    SD_ARG_CMD52_WRITE);
}

uint16_t
sdmmc_io_read_2(struct sdmmc_function *sf, int reg)
{
	uint16_t data = 0;

	/* Don't lock */

	(void)sdmmc_io_rw_extended(sf->sc, sf, reg, (u_char *)&data, 2,
	    SD_ARG_CMD53_READ | SD_ARG_CMD53_INCREMENT);
	return data;
}

void
sdmmc_io_write_2(struct sdmmc_function *sf, int reg, uint16_t data)
{

	/* Don't lock */

	(void)sdmmc_io_rw_extended(sf->sc, sf, reg, (u_char *)&data, 2,
	    SD_ARG_CMD53_WRITE | SD_ARG_CMD53_INCREMENT);
}

uint32_t
sdmmc_io_read_4(struct sdmmc_function *sf, int reg)
{
	uint32_t data = 0;

	/* Don't lock */

	(void)sdmmc_io_rw_extended(sf->sc, sf, reg, (u_char *)&data, 4,
	    SD_ARG_CMD53_READ | SD_ARG_CMD53_INCREMENT);
	return data;
}

void
sdmmc_io_write_4(struct sdmmc_function *sf, int reg, uint32_t data)
{

	/* Don't lock */

	(void)sdmmc_io_rw_extended(sf->sc, sf, reg, (u_char *)&data, 4,
	    SD_ARG_CMD53_WRITE | SD_ARG_CMD53_INCREMENT);
}


int
sdmmc_io_read_multi_1(struct sdmmc_function *sf, int reg, u_char *data,
    int datalen)
{
	int error;

	/* Don't lock */

	while (datalen > SD_ARG_CMD53_LENGTH_MAX) {
		error = sdmmc_io_rw_extended(sf->sc, sf, reg, data,
		    SD_ARG_CMD53_LENGTH_MAX, SD_ARG_CMD53_READ);
		if (error)
			goto error;
		data += SD_ARG_CMD53_LENGTH_MAX;
		datalen -= SD_ARG_CMD53_LENGTH_MAX;
	}

	error = sdmmc_io_rw_extended(sf->sc, sf, reg, data, datalen,
	    SD_ARG_CMD53_READ);
error:
	return error;
}

int
sdmmc_io_write_multi_1(struct sdmmc_function *sf, int reg, u_char *data,
    int datalen)
{
	int error;

	/* Don't lock */

	while (datalen > SD_ARG_CMD53_LENGTH_MAX) {
		error = sdmmc_io_rw_extended(sf->sc, sf, reg, data,
		    SD_ARG_CMD53_LENGTH_MAX, SD_ARG_CMD53_WRITE);
		if (error)
			goto error;
		data += SD_ARG_CMD53_LENGTH_MAX;
		datalen -= SD_ARG_CMD53_LENGTH_MAX;
	}

	error = sdmmc_io_rw_extended(sf->sc, sf, reg, data, datalen,
	    SD_ARG_CMD53_WRITE);
error:
	return error;
}

#if 0
static int
sdmmc_io_xchg(struct sdmmc_softc *sc, struct sdmmc_function *sf,
    int reg, u_char *datap)
{

	/* Don't lock */

	return sdmmc_io_rw_direct(sc, sf, reg, datap,
	    SD_ARG_CMD52_WRITE|SD_ARG_CMD52_EXCHANGE);
}
#endif

/*
 * Reset the I/O functions of the card.
 */
static void
sdmmc_io_reset(struct sdmmc_softc *sc)
{

	/* Don't lock */
#if 0 /* XXX command fails */
	(void)sdmmc_io_write(sc, NULL, SD_IO_REG_CCCR_CTL, CCCR_CTL_RES);
	sdmmc_delay(100000);
#endif
}

/*
 * Get or set the card's I/O OCR value (SDIO).
 */
static int
sdmmc_io_send_op_cond(struct sdmmc_softc *sc, u_int32_t ocr, u_int32_t *ocrp)
{
	struct sdmmc_command cmd;
	int error;
	int retry;

	DPRINTF(("sdmmc_io_send_op_cond: ocr = %#x\n", ocr));

	/* Don't lock */

	/*
	 * If we change the OCR value, retry the command until the OCR
	 * we receive in response has the "CARD BUSY" bit set, meaning
	 * that all cards are ready for identification.
	 */
	for (retry = 0; retry < 100; retry++) {
		memset(&cmd, 0, sizeof cmd);
		cmd.c_opcode = SD_IO_SEND_OP_COND;
		cmd.c_arg = ocr;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R4 | SCF_TOUT_OK;

		error = sdmmc_mmc_command(sc, &cmd);
		if (error)
			break;
		if (ISSET(MMC_R4(cmd.c_resp), SD_IO_OCR_MEM_READY) || ocr == 0)
			break;

		error = ETIMEDOUT;
		sdmmc_delay(10000);
	}
	if (error == 0 && ocrp != NULL)
		*ocrp = MMC_R4(cmd.c_resp);

	DPRINTF(("sdmmc_io_send_op_cond: error = %d\n", error));

	return error;
}

/*
 * Card interrupt handling
 */

void
sdmmc_intr_enable(struct sdmmc_function *sf)
{
	struct sdmmc_softc *sc = sf->sc;
	struct sdmmc_function *sf0 = sc->sc_fn0;
	uint8_t reg;

	SDMMC_LOCK(sc);
	mutex_enter(&sc->sc_intr_task_mtx);
	reg = sdmmc_io_read_1(sf0, SD_IO_CCCR_FN_INTEN);
	reg |= 1 << sf->number;
	sdmmc_io_write_1(sf0, SD_IO_CCCR_FN_INTEN, reg);
	mutex_exit(&sc->sc_intr_task_mtx);
	SDMMC_UNLOCK(sc);
}

void
sdmmc_intr_disable(struct sdmmc_function *sf)
{
	struct sdmmc_softc *sc = sf->sc;
	struct sdmmc_function *sf0 = sc->sc_fn0;
	uint8_t reg;

	SDMMC_LOCK(sc);
	mutex_enter(&sc->sc_intr_task_mtx);
	reg = sdmmc_io_read_1(sf0, SD_IO_CCCR_FN_INTEN);
	reg &= ~(1 << sf->number);
	sdmmc_io_write_1(sf0, SD_IO_CCCR_FN_INTEN, reg);
	mutex_exit(&sc->sc_intr_task_mtx);
	SDMMC_UNLOCK(sc);
}

/*
 * Establish a handler for the SDIO card interrupt.  Because the
 * interrupt may be shared with different SDIO functions, multiple
 * handlers can be established.
 */
void *
sdmmc_intr_establish(device_t dev, int (*fun)(void *), void *arg,
    const char *name)
{
	struct sdmmc_softc *sc = device_private(dev);
	struct sdmmc_intr_handler *ih;

	if (sc->sc_sct->card_enable_intr == NULL)
		return NULL;

	ih = malloc(sizeof *ih, M_DEVBUF, M_WAITOK|M_CANFAIL|M_ZERO);
	if (ih == NULL)
		return NULL;

	ih->ih_name = malloc(strlen(name) + 1, M_DEVBUF,
	    M_WAITOK|M_CANFAIL|M_ZERO);
	if (ih->ih_name == NULL) {
		free(ih, M_DEVBUF);
		return NULL;
	}
	strlcpy(ih->ih_name, name, strlen(name));
	ih->ih_softc = sc;
	ih->ih_fun = fun;
	ih->ih_arg = arg;

	mutex_enter(&sc->sc_mtx);
	if (TAILQ_EMPTY(&sc->sc_intrq)) {
		sdmmc_intr_enable(sc->sc_fn0);
		sdmmc_chip_card_enable_intr(sc->sc_sct, sc->sc_sch, 1);
	}
	TAILQ_INSERT_TAIL(&sc->sc_intrq, ih, entry);
	mutex_exit(&sc->sc_mtx);

	return ih;
}

/*
 * Disestablish the given handler.
 */
void
sdmmc_intr_disestablish(void *cookie)
{
	struct sdmmc_intr_handler *ih = cookie;
	struct sdmmc_softc *sc = ih->ih_softc;

	if (sc->sc_sct->card_enable_intr == NULL)
		return;

	mutex_enter(&sc->sc_mtx);
	TAILQ_REMOVE(&sc->sc_intrq, ih, entry);
	if (TAILQ_EMPTY(&sc->sc_intrq)) {
		sdmmc_chip_card_enable_intr(sc->sc_sct, sc->sc_sch, 0);
		sdmmc_intr_disable(sc->sc_fn0);
	}
	mutex_exit(&sc->sc_mtx);

	free(ih->ih_name, M_DEVBUF);
	free(ih, M_DEVBUF);
}

/*
 * Call established SDIO card interrupt handlers.  The host controller
 * must call this function from its own interrupt handler to handle an
 * SDIO interrupt from the card.
 */
void
sdmmc_card_intr(device_t dev)
{
	struct sdmmc_softc *sc = device_private(dev);

	if (sc->sc_sct->card_enable_intr == NULL)
		return;

	mutex_enter(&sc->sc_intr_task_mtx);
	if (!sdmmc_task_pending(&sc->sc_intr_task))
		sdmmc_add_task(sc, &sc->sc_intr_task);
	mutex_exit(&sc->sc_intr_task_mtx);
}

void
sdmmc_intr_task(void *arg)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)arg;
	struct sdmmc_intr_handler *ih;

	mutex_enter(&sc->sc_mtx);
	TAILQ_FOREACH(ih, &sc->sc_intrq, entry) {
		/* XXX examine return value and do evcount stuff*/
		(void)(*ih->ih_fun)(ih->ih_arg);
	}
	mutex_exit(&sc->sc_mtx);

	sdmmc_chip_card_intr_ack(sc->sc_sct, sc->sc_sch);
}
