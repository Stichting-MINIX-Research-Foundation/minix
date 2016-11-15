/*	$NetBSD: cissvar.h,v 1.6 2013/10/12 16:52:21 christos Exp $	*/
/*	$OpenBSD: cissvar.h,v 1.15 2013/05/30 16:15:02 deraadt Exp $	*/

/*
 * Copyright (c) 2005,2006 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/mutex.h>
#include <sys/condvar.h>

#include <dev/sysmon/sysmonvar.h>
#include <sys/envsys.h>

struct ciss_ld {
	struct ciss_blink bling;	/* a copy of blink state */
	char	xname[16];		/* copy of the sdN name */
	int	ndrives;
	u_int8_t tgts[1];
};

struct ciss_softc {
	/* Generic device info. */
	device_t		sc_dev;
	kmutex_t		sc_mutex;
	kmutex_t		sc_mutex_scratch;
	bus_space_handle_t	sc_ioh;
	bus_space_tag_t		sc_iot;
	bus_dma_tag_t		sc_dmat;
	void			*sc_ih;
	void			*sc_sh;		/* shutdown hook */
	struct proc		*sc_thread;
	int			sc_flush;

	struct scsipi_channel	sc_channel;
	struct scsipi_channel	*sc_channel_raw;
	struct scsipi_adapter	sc_adapter;
	struct scsipi_adapter	*sc_adapter_raw;
	struct callout		sc_hb;

	u_int	sc_flags;
	int ccblen, maxcmd, maxsg, nbus, ndrives, maxunits;
	ciss_queue_head	sc_free_ccb, sc_ccbq, sc_ccbdone;
	kcondvar_t		sc_condvar;

	bus_dmamap_t		cmdmap;
	bus_dma_segment_t	cmdseg[1];
	void *			ccbs;
	void			*scratch;
	u_int			sc_waitflag;

	bus_space_handle_t	cfg_ioh;

	int fibrillation;
	struct ciss_config cfg;
	int cfgoff;
	u_int32_t iem;
	u_int32_t heartbeat;
	struct ciss_ld **sc_lds;

	/* scsi ioctl from sd device */
	int			(*sc_ioctl)(device_t, u_long, void *);

	struct sysmon_envsys    *sc_sme;
	envsys_data_t		*sc_sensor;
};

struct ciss_rawsoftc {
	struct ciss_softc *sc_softc;
	u_int8_t	sc_channel;
};

int	ciss_attach(struct ciss_softc *sc);
int	ciss_intr(void *v);
