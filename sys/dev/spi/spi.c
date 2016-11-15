/* $NetBSD: spi.c,v 1.8 2013/02/15 17:44:40 rkujawa Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: spi.c,v 1.8 2013/02/15 17:44:40 rkujawa Exp $");

#include "locators.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/errno.h>

#include <dev/spi/spivar.h>

struct spi_softc {
	struct spi_controller	sc_controller;
	int			sc_mode;
	int			sc_speed;
	int			sc_nslaves;
	struct spi_handle	*sc_slaves;
};

/*
 * SPI slave device.  We have one of these per slave.
 */
struct spi_handle {
	struct spi_softc	*sh_sc;
	struct spi_controller	*sh_controller;
	int			sh_slave;
};

/*
 * API for bus drivers.
 */

int
spibus_print(void *aux, const char *pnp)
{

	if (pnp != NULL)
		aprint_normal("spi at %s", pnp);

	return (UNCONF);
}


static int
spi_match(device_t parent, cfdata_t cf, void *aux)
{

	return 1;
}

static int
spi_print(void *aux, const char *pnp)
{
	struct spi_attach_args *sa = aux;

	if (sa->sa_handle->sh_slave != -1)
		aprint_normal(" slave %d", sa->sa_handle->sh_slave);

	return (UNCONF);
}

static int
spi_search(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct spi_softc *sc = device_private(parent);
	struct spi_attach_args sa;
	int addr;

	addr = cf->cf_loc[SPICF_SLAVE];
	if ((addr < 0) || (addr >= sc->sc_controller.sct_nslaves)) {
		return -1;
	}

	sa.sa_handle = &sc->sc_slaves[addr];

	if (config_match(parent, cf, &sa) > 0)
		config_attach(parent, cf, &sa, spi_print);

	return 0;
}

/*
 * API for device drivers.
 *
 * We provide wrapper routines to decouple the ABI for the SPI
 * device drivers from the ABI for the SPI bus drivers.
 */
static void
spi_attach(device_t parent, device_t self, void *aux)
{
	struct spi_softc *sc = device_private(self);
	struct spibus_attach_args *sba = aux;
	int i;

	aprint_naive(": SPI bus\n");
	aprint_normal(": SPI bus\n");

	sc->sc_controller = *sba->sba_controller;
	sc->sc_nslaves = sba->sba_controller->sct_nslaves;
	/* allocate slave structures */
	sc->sc_slaves = malloc(sizeof (struct spi_handle) * sc->sc_nslaves,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	sc->sc_speed = 0;
	sc->sc_mode = -1;

	/*
	 * Initialize slave handles
	 */
	for (i = 0; i < sc->sc_nslaves; i++) {
		sc->sc_slaves[i].sh_slave = i;
		sc->sc_slaves[i].sh_sc = sc;
		sc->sc_slaves[i].sh_controller = &sc->sc_controller;
	}

	/*
	 * Locate and attach child devices
	 */
	config_search_ia(spi_search, self, "spi", NULL);
}

CFATTACH_DECL_NEW(spi, sizeof(struct spi_softc),
    spi_match, spi_attach, NULL, NULL);

/*
 * Configure.  This should be the first thing that the SPI driver
 * should do, to configure which mode (e.g. SPI_MODE_0, which is the
 * same as Philips Microwire mode), and speed.  If the bus driver
 * cannot run fast enough, then it should just configure the fastest
 * mode that it can support.  If the bus driver cannot run slow
 * enough, then the device is incompatible and an error should be
 * returned.
 */
int
spi_configure(struct spi_handle *sh, int mode, int speed)
{
	int			s, rv;
	struct spi_softc	*sc = sh->sh_sc;
	struct spi_controller	*tag = sh->sh_controller;

	/* ensure that request is compatible with other devices on the bus */
	if ((sc->sc_mode >= 0) && (sc->sc_mode != mode))
		return EINVAL;

	s = splbio();
	/* pick lowest configured speed */
	if (speed == 0)
		speed = sc->sc_speed;
	if (sc->sc_speed)
		speed = min(sc->sc_speed, speed);

	rv = (*tag->sct_configure)(tag->sct_cookie, sh->sh_slave,
	    mode, speed);

	if (rv == 0) {
		sc->sc_mode = mode;
		sc->sc_speed = speed;
	}
	splx(s);
	return rv;
}

void
spi_transfer_init(struct spi_transfer *st)
{

	mutex_init(&st->st_lock, MUTEX_DEFAULT, IPL_BIO);
	cv_init(&st->st_cv, "spicv");

	st->st_flags = 0;
	st->st_errno = 0;
	st->st_done = NULL;
	st->st_chunks = NULL;
	st->st_private = NULL;
	st->st_slave = -1;
}

void
spi_chunk_init(struct spi_chunk *chunk, int cnt, const uint8_t *wptr,
    uint8_t *rptr)
{

	chunk->chunk_write = chunk->chunk_wptr = wptr;
	chunk->chunk_read = chunk->chunk_rptr = rptr;
	chunk->chunk_rresid = chunk->chunk_wresid = chunk->chunk_count = cnt;
	chunk->chunk_next = NULL;
}

void
spi_transfer_add(struct spi_transfer *st, struct spi_chunk *chunk)
{
	struct spi_chunk **cpp;

	/* this is an O(n) insert -- perhaps we should use a simpleq? */
	for (cpp = &st->st_chunks; *cpp; cpp = &(*cpp)->chunk_next);
	*cpp = chunk;
}

int
spi_transfer(struct spi_handle *sh, struct spi_transfer *st)
{
	struct spi_controller	*tag = sh->sh_controller;
	struct spi_chunk	*chunk;

	/*
	 * Initialize "resid" counters and pointers, so that callers
	 * and bus drivers don't have to.
	 */
	for (chunk = st->st_chunks; chunk; chunk = chunk->chunk_next) {
		chunk->chunk_wresid = chunk->chunk_rresid = chunk->chunk_count;
		chunk->chunk_wptr = chunk->chunk_write;
		chunk->chunk_rptr = chunk->chunk_read;
	}

	/*
	 * Match slave to handle's slave.
	 */
	st->st_slave = sh->sh_slave;

	return (*tag->sct_transfer)(tag->sct_cookie, st);
}

void
spi_wait(struct spi_transfer *st)
{

	mutex_enter(&st->st_lock);
	while (!(st->st_flags & SPI_F_DONE)) {
		cv_wait(&st->st_cv, &st->st_lock);
	}
	mutex_exit(&st->st_lock);
	cv_destroy(&st->st_cv);
	mutex_destroy(&st->st_lock);
}

void
spi_done(struct spi_transfer *st, int err)
{

	mutex_enter(&st->st_lock);
	if ((st->st_errno = err) != 0) {
		st->st_flags |= SPI_F_ERROR;
	}
	st->st_flags |= SPI_F_DONE;
	if (st->st_done != NULL) {
		(*st->st_done)(st);
	} else {
		cv_broadcast(&st->st_cv);
	}
	mutex_exit(&st->st_lock);
}

/*
 * Some convenience routines.  These routines block until the work
 * is done.
 *
 * spi_recv - receives data from the bus
 *
 * spi_send - sends data to the bus
 *
 * spi_send_recv - sends data to the bus, and then receives.  Note that this is
 * done synchronously, i.e. send a command and get the response.  This is
 * not full duplex.  If you wnat full duplex, you can't use these convenience
 * wrappers.
 */
int
spi_recv(struct spi_handle *sh, int cnt, uint8_t *data)
{
	struct spi_transfer	trans;
	struct spi_chunk	chunk;

	spi_transfer_init(&trans);
	spi_chunk_init(&chunk, cnt, NULL, data);
	spi_transfer_add(&trans, &chunk);

	/* enqueue it and wait for it to complete */
	spi_transfer(sh, &trans);
	spi_wait(&trans);

	if (trans.st_flags & SPI_F_ERROR)
		return trans.st_errno;

	return 0;
}

int
spi_send(struct spi_handle *sh, int cnt, const uint8_t *data)
{
	struct spi_transfer	trans;
	struct spi_chunk	chunk;

	spi_transfer_init(&trans);
	spi_chunk_init(&chunk, cnt, data, NULL);
	spi_transfer_add(&trans, &chunk);

	/* enqueue it and wait for it to complete */
	spi_transfer(sh, &trans);
	spi_wait(&trans);

	if (trans.st_flags & SPI_F_ERROR)
		return trans.st_errno;

	return 0;
}

int
spi_send_recv(struct spi_handle *sh, int scnt, const uint8_t *snd,
    int rcnt, uint8_t *rcv)
{
	struct spi_transfer	trans;
	struct spi_chunk	chunk1, chunk2;

	spi_transfer_init(&trans);
	spi_chunk_init(&chunk1, scnt, snd, NULL);
	spi_chunk_init(&chunk2, rcnt, NULL, rcv);
	spi_transfer_add(&trans, &chunk1);
	spi_transfer_add(&trans, &chunk2);

	/* enqueue it and wait for it to complete */
	spi_transfer(sh, &trans);
	spi_wait(&trans);

	if (trans.st_flags & SPI_F_ERROR)
		return trans.st_errno;

	return 0;
}
