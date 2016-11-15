/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

Developed by Semihalf

********************************************************************************
Marvell BSD License

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File under the following licensing terms.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    *   Redistributions of source code must retain the above copyright notice,
            this list of conditions and the following disclaimer.

    *   Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    *   Neither the name of Marvell nor the names of its contributors may be
        used to endorse or promote products derived from this software without
        specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*
 * Transfer mechanism extracted from arspi.c corresponding with the lines 
 * 254-262 in this file.
 */

#include <sys/param.h>
#include <sys/device.h>

#include <dev/spi/spivar.h>

#include <dev/marvell/mvspireg.h>
#include <dev/marvell/marvellvar.h>

#include "locators.h"

extern uint32_t mvTclk;

struct mvspi_softc {
	struct device		sc_dev;
	struct spi_controller	sc_spi;
	void			*sc_ih;
	bool			sc_interrupts;

	struct spi_transfer	*sc_transfer;
	struct spi_chunk	*sc_wchunk;	/* For partial writes */
	struct spi_transq	sc_transq;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	bus_size_t		sc_size;
};

int mvspi_match(struct device *, struct cfdata *, void *);
void mvspi_attach(struct device *, struct device *, void *);
/* SPI service routines */
int mvspi_configure(void *, int, int, int);
int mvspi_transfer(void *, struct spi_transfer *);
/* Internal support */
void mvspi_sched(struct mvspi_softc *);
void mvspi_assert(struct mvspi_softc *sc);
void mvspi_deassert(struct mvspi_softc *sc);

#define	GETREG(sc, x)					\
	bus_space_read_4(sc->sc_st, sc->sc_sh, x)
#define	PUTREG(sc, x, v)				\
	bus_space_write_4(sc->sc_st, sc->sc_sh, x, v)

/* Attach structure */
CFATTACH_DECL_NEW(mvspi_mbus, sizeof(struct mvspi_softc),
    mvspi_match, mvspi_attach, NULL, NULL);

int
mvspi_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, cf->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT ||
	    mva->mva_irq == MVA_IRQ_DEFAULT)
		return 0;

	mva->mva_size = MVSPI_SIZE;
	return 1;
}

void
mvspi_attach(struct device *parent, struct device *self, void *aux)
{
	struct mvspi_softc *sc =  device_private(self);
  	struct marvell_attach_args *mva = aux;
	struct spibus_attach_args sba;
	int ctl;

	aprint_normal(": Marvell SPI controller\n");

	/*
	 * Map registers.
	 */
	sc->sc_st = mva->mva_iot;
	sc->sc_size = mva->mva_size;
	
	if (bus_space_subregion(sc->sc_st, mva->mva_ioh, mva->mva_offset,
	    mva->mva_size, &sc->sc_sh)) {
		aprint_error_dev(self, "Cannot map registers\n");
		return;
	}

	/*
	 * Initialize hardware.
	 */
	ctl = GETREG(sc, MVSPI_INTCONF_REG);

	ctl &= MVSPI_DIRHS_MASK;
	ctl &= MVSPI_1BYTE_MASK;

	PUTREG(sc, MVSPI_INTCONF_REG, ctl),

	/*
	 * Initialize SPI controller.
	 */
	sc->sc_spi.sct_cookie = sc;
	sc->sc_spi.sct_configure = mvspi_configure;
	sc->sc_spi.sct_transfer = mvspi_transfer;
	sc->sc_spi.sct_nslaves = 1;

	/*
	 * Initialize the queue.
	 */
	spi_transq_init(&sc->sc_transq);

	/*
	 * Initialize and attach bus attach.
	 */
	sba.sba_controller = &sc->sc_spi;
	(void) config_found_ia(self, "spibus", &sba, spibus_print);
}
    
int
mvspi_configure(void *cookie, int slave, int mode, int speed)
{
	struct mvspi_softc *sc = cookie;
	uint32_t ctl = 0, spr, sppr;
	uint32_t divider;
	uint32_t best_spr = 0, best_sppr = 0;
	uint32_t best_sppr0, best_spprhi;
	uint8_t exact_match = 0;
	uint32_t min_baud_offset = 0xFFFFFFFF;
	
	if (slave < 0 || slave > 7)
		return EINVAL;

	switch(mode) {
		case SPI_MODE_0:
			ctl &= ~(MVSPI_CPOL_MASK);
			/* In boards documentation, CPHA is inverted */
			ctl &= MVSPI_CPHA_MASK;
			break;
		case SPI_MODE_1:
			ctl |= MVSPI_CPOL_MASK;
			ctl &= MVSPI_CPHA_MASK;
			break;
		case SPI_MODE_2:
			ctl &= ~(MVSPI_CPOL_MASK);
			ctl |= ~(MVSPI_CPHA_MASK);
			break;
		case SPI_MODE_3:
			ctl |= MVSPI_CPOL_MASK;
			ctl |= ~(MVSPI_CPHA_MASK);
			break;
		default:
			return EINVAL;
	}

	/* Find the best prescale configuration - less or equal:
	 * SPI actual frecuency = core_clk / (SPR * (2 ^ SPPR))
	 * Try to find the minimal SPR and SPPR values that offer
	 * the best prescale config.
	 *
	 */
	for (spr = 1; spr <= MVSPI_SPR_MAXVALUE; spr++) {
		for (sppr = 0; sppr <= MVSPI_SPPR_MAXVALUE; sppr++) {
			divider = spr * (1 << sppr);
			/* Check for higher - irrelevant */
			if ((mvTclk / divider) > speed)
				continue;

			/* Check for exact fit */
			if ((mvTclk / divider) == speed) {
				best_spr = spr;
				best_sppr = sppr;
				exact_match = 1;
				break;
			}

			/* Check if this is better than the previous one */
			if ((speed - (mvTclk / divider)) < min_baud_offset) {
				min_baud_offset = (speed - (mvTclk / divider));
				best_spr = spr;
				best_sppr = sppr;
			}
		}

		if (exact_match == 1)
			break;
	}

	if (best_spr == 0) {
		printf("%s ERROR: SPI baud rate prescale error!\n", __func__);
		return -1;
	}

	ctl &= ~(MVSPI_SPR_MASK);
	ctl &= ~(MVSPI_SPPR_MASK);
	ctl |= best_spr;

	best_spprhi = best_sppr & MVSPI_SPPRHI_MASK;
	best_spprhi = best_spprhi << 5;

	ctl |= best_spprhi;

	best_sppr0 = best_sppr & MVSPI_SPPR0_MASK;
	best_sppr0 = best_sppr0 << 4;

	ctl |= best_sppr0;

	PUTREG(sc, MVSPI_INTCONF_REG, ctl);

	return 0;
}

int
mvspi_transfer(void *cookie, struct spi_transfer *st)
{
	struct mvspi_softc *sc = cookie;
	int s;

	s = splbio();
	spi_transq_enqueue(&sc->sc_transq, st);
	if (sc->sc_transfer == NULL) {
		mvspi_sched(sc);
	}
	splx(s);
	return 0;
}

void
mvspi_assert(struct mvspi_softc *sc)
{
	int ctl;
	
	if (sc->sc_transfer->st_slave < 0 && sc->sc_transfer->st_slave > 7) {
		printf("%s ERROR: Slave number %d not valid!\n",  __func__, sc->sc_transfer->st_slave);
		return;
	} else
		/* Enable appropriate CSn according to its slave number */
		PUTREG(sc, MVSPI_CTRL_REG, (sc->sc_transfer->st_slave << 2));

	/* Enable CSnAct */
	ctl = GETREG(sc, MVSPI_CTRL_REG);
	ctl |= MVSPI_CSNACT_MASK;
	PUTREG(sc, MVSPI_CTRL_REG, ctl);
}

void
mvspi_deassert(struct mvspi_softc *sc)
{
	int ctl = GETREG(sc, MVSPI_CTRL_REG);
	ctl &= ~(MVSPI_CSNACT_MASK);
	PUTREG(sc, MVSPI_CTRL_REG, ctl);
}

void
mvspi_sched(struct mvspi_softc *sc)
{
	struct spi_transfer *st;
	struct spi_chunk *chunk;
	int i, j, ctl;
	uint8_t byte;
	int ready = FALSE;

	for (;;) {
		if ((st = sc->sc_transfer) == NULL) {
			if ((st = spi_transq_first(&sc->sc_transq)) == NULL) {
				/* No work left to do */
				break;
			}
			spi_transq_dequeue(&sc->sc_transq);
			sc->sc_transfer = st;
		}

		chunk = st->st_chunks;

		mvspi_assert(sc);

		do {
			for (i = chunk->chunk_wresid; i > 0; i--) {
				/* First clear the ready bit */
				ctl = GETREG(sc, MVSPI_CTRL_REG);
				ctl &= ~(MVSPI_CR_SMEMRDY);
				PUTREG(sc, MVSPI_CTRL_REG, ctl);

				if (chunk->chunk_wptr){
					byte = *chunk->chunk_wptr;
					chunk->chunk_wptr++;
				} else
					byte = MVSPI_DUMMY_BYTE;

				/* Transmit data */
				PUTREG(sc, MVSPI_DATAOUT_REG, byte);

				/* Wait with timeout for memory ready */
				for (j = 0; j < MVSPI_WAIT_RDY_MAX_LOOP; j++) {
					if (GETREG(sc, MVSPI_CTRL_REG) &
						MVSPI_CR_SMEMRDY) {
						ready = TRUE;
						break;
					}

				}

				if (!ready) {
					mvspi_deassert(sc);
					spi_done(st, EBUSY);
					return;
				}

				/* Check that the RX data is needed */
				if (chunk->chunk_rptr) {
					*chunk->chunk_rptr =
						GETREG(sc, MVSPI_DATAIN_REG);
					chunk->chunk_rptr++;

				}

			}

			chunk = chunk->chunk_next;

		} while (chunk != NULL);

		mvspi_deassert(sc);

		spi_done(st, 0);
		sc->sc_transfer = NULL;


		break;
	}
}
