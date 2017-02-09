/*	$NetBSD: gtmpsc.c,v 1.46 2014/11/15 19:18:18 christos Exp $	*/
/*
 * Copyright (c) 2009 KIYOHARA Takashi
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
/*
 * mpsc.c - Multi-Protocol Serial Controller driver, supports UART mode only
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gtmpsc.c,v 1.46 2014/11/15 19:18:18 christos Exp $");

#include "opt_kgdb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/intr.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/timepps.h>
#include <sys/tty.h>
#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <dev/cons.h>

#include <dev/marvell/gtreg.h>
#include <dev/marvell/gtvar.h>
#include <dev/marvell/gtbrgreg.h>
#include <dev/marvell/gtbrgvar.h>
#include <dev/marvell/gtsdmareg.h>
#include <dev/marvell/gtsdmavar.h>
#include <dev/marvell/gtmpscreg.h>
#include <dev/marvell/gtmpscvar.h>
#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>

#include "gtmpsc.h"
#include "ioconf.h"
#include "locators.h"

/*
 * Wait 2 characters time for RESET_DELAY
 */
#define GTMPSC_RESET_DELAY	(2*8*1000000 / GT_MPSC_DEFAULT_BAUD_RATE)


#if defined(DEBUG)
unsigned int gtmpsc_debug = 0;
# define STATIC
# define DPRINTF(x)	do { if (gtmpsc_debug) printf x ; } while (0)
#else
# define STATIC static
# define DPRINTF(x)
#endif

#define GTMPSCUNIT(x)      TTUNIT(x)
#define GTMPSCDIALOUT(x)   TTDIALOUT(x)

#define CLEANUP_AND_RETURN_RXDMA(sc, ix)				    \
	do {								    \
		gtmpsc_pollrx_t *_vrxp = &(sc)->sc_poll_sdmapage->rx[(ix)]; \
									    \
		_vrxp->rxdesc.sdma_csr =				    \
		    SDMA_CSR_RX_L	|				    \
		    SDMA_CSR_RX_F	|				    \
		    SDMA_CSR_RX_OWN	|				    \
		    SDMA_CSR_RX_EI;					    \
		_vrxp->rxdesc.sdma_cnt =				    \
		    GTMPSC_RXBUFSZ << SDMA_RX_CNT_BUFSZ_SHIFT;		    \
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_rxdma_map,	    \
		    (ix) * sizeof(gtmpsc_pollrx_t) + sizeof(sdma_desc_t),   \
		    sizeof(vrxp->rxbuf),				    \
		    BUS_DMASYNC_PREREAD);				    \
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_rxdma_map,	    \
		    (ix) * sizeof(gtmpsc_pollrx_t),			    \
		    sizeof(sdma_desc_t),				    \
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);	    \
	} while (0);


STATIC int  gtmpscmatch(device_t, cfdata_t, void *);
STATIC void gtmpscattach(device_t, device_t, void *);

STATIC void gtmpsc_softintr(void *);

STATIC void gtmpscstart(struct tty *);
STATIC int  gtmpscparam(struct tty *, struct termios *);

STATIC void gtmpsc_shutdownhook(void *);

STATIC uint32_t cflag2mpcr(tcflag_t);
STATIC __inline void gtmpsc_intr_rx(struct gtmpsc_softc *);
STATIC __inline void gtmpsc_intr_tx(struct gtmpsc_softc *);
STATIC void gtmpsc_write(struct gtmpsc_softc *);
STATIC void gtmpsc_txflush(gtmpsc_softc_t *);
STATIC void gtmpsc_rxdesc_init(struct gtmpsc_softc *);
STATIC void gtmpsc_txdesc_init(struct gtmpsc_softc *);
STATIC void gtmpscinit_stop(struct gtmpsc_softc *);
STATIC void gtmpscinit_start(struct gtmpsc_softc *);
STATIC void gtmpscshutdown(struct gtmpsc_softc *);
STATIC void gtmpsc_loadchannelregs(struct gtmpsc_softc *);

#ifdef MPSC_CONSOLE
STATIC int gtmpsccngetc(dev_t);
STATIC void gtmpsccnputc(dev_t, int);
STATIC void gtmpsccnpollc(dev_t, int);
STATIC void gtmpsccnhalt(dev_t);

STATIC int gtmpsc_hackinit(struct gtmpsc_softc *, bus_space_tag_t,
			   bus_dma_tag_t, bus_addr_t, int, int, int, tcflag_t);
#endif

#if defined(MPSC_CONSOLE) || defined(KGDB)
STATIC int  gtmpsc_common_getc(struct gtmpsc_softc *);
STATIC void gtmpsc_common_putc(struct gtmpsc_softc *, int);
STATIC void gtmpsc_common_putc_wait_complete(struct gtmpsc_softc *, int);
#endif

dev_type_open(gtmpscopen);
dev_type_close(gtmpscclose);
dev_type_read(gtmpscread);
dev_type_write(gtmpscwrite);
dev_type_ioctl(gtmpscioctl);
dev_type_stop(gtmpscstop);
dev_type_tty(gtmpsctty);
dev_type_poll(gtmpscpoll);

const struct cdevsw gtmpsc_cdevsw = {
	.d_open = gtmpscopen,
	.d_close = gtmpscclose,
	.d_read = gtmpscread,
	.d_write = gtmpscwrite,
	.d_ioctl = gtmpscioctl,
	.d_stop = gtmpscstop,
	.d_tty = gtmpsctty,
	.d_poll = gtmpscpoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

CFATTACH_DECL_NEW(gtmpsc, sizeof(struct gtmpsc_softc),
    gtmpscmatch, gtmpscattach, NULL, NULL);


STATIC uint32_t sdma_imask;		/* soft copy of SDMA IMASK reg */
STATIC struct cnm_state gtmpsc_cnm_state;

#ifdef KGDB
static int gtmpsc_kgdb_addr;
static int gtmpsc_kgdb_attached;

STATIC int      gtmpsc_kgdb_getc(void *);
STATIC void     gtmpsc_kgdb_putc(void *, int);
#endif /* KGDB */

#ifdef MPSC_CONSOLE
/*
 * hacks for console initialization
 * which happens prior to autoconfig "attach"
 *
 * XXX Assumes PAGE_SIZE is a constant!
 */
gtmpsc_softc_t gtmpsc_cn_softc;
STATIC unsigned char gtmpsc_cn_dmapage[PAGE_SIZE] __aligned(PAGE_SIZE);


static struct consdev gtmpsc_consdev = {
	NULL, NULL, gtmpsccngetc, gtmpsccnputc, gtmpsccnpollc,
	NULL, gtmpsccnhalt, NULL, NODEV, CN_NORMAL
};
#endif


#define GT_MPSC_READ(sc, o) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_mpsch, (o))
#define GT_MPSC_WRITE(sc, o, v) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_mpsch, (o), (v))
#define GT_SDMA_READ(sc, o) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_sdmah, (o))
#define GT_SDMA_WRITE(sc, o, v) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_sdmah, (o), (v))


/* ARGSUSED */
STATIC int
gtmpscmatch(device_t parent, cfdata_t match, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT)
		return 0;

	mva->mva_size = GTMPSC_SIZE;
	return 1;
}

/* ARGSUSED */
STATIC void
gtmpscattach(device_t parent, device_t self, void *aux)
{
	struct gtmpsc_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;
	bus_dma_segment_t segs;
	struct tty *tp;
	int rsegs, err, unit;
	void *kva;

	aprint_naive("\n");
	aprint_normal(": Multi-Protocol Serial Controller\n");

	if (mva->mva_unit != MVA_UNIT_DEFAULT)
		unit = mva->mva_unit;
	else
		unit = (mva->mva_offset == GTMPSC_BASE(0)) ? 0 : 1;

#ifdef MPSC_CONSOLE
	if (cn_tab == &gtmpsc_consdev &&
	    cn_tab->cn_dev == makedev(0, unit)) {
		gtmpsc_cn_softc.sc_dev = self;
		memcpy(sc, &gtmpsc_cn_softc, sizeof(struct gtmpsc_softc));
		sc->sc_flags = GTMPSC_CONSOLE;
	} else
#endif
	{
		if (bus_space_subregion(mva->mva_iot, mva->mva_ioh,
		    mva->mva_offset, mva->mva_size, &sc->sc_mpsch)) {
			aprint_error_dev(self, "Cannot map MPSC registers\n");
			return;
		}
		if (bus_space_subregion(mva->mva_iot, mva->mva_ioh,
		    GTSDMA_BASE(unit), GTSDMA_SIZE, &sc->sc_sdmah)) {
			aprint_error_dev(self, "Cannot map SDMA registers\n");
			return;
		}
		sc->sc_dev = self;
		sc->sc_unit = unit;
		sc->sc_iot = mva->mva_iot;
		sc->sc_dmat = mva->mva_dmat;

		err = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
		    &segs, 1, &rsegs, BUS_DMA_NOWAIT);
		if (err) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamem_alloc error 0x%x\n", err);
			goto fail0;
		}
		err = bus_dmamem_map(sc->sc_dmat, &segs, 1, PAGE_SIZE, &kva,
		    BUS_DMA_NOWAIT);
		if (err) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamem_map error 0x%x\n", err);
			goto fail1;
		}
		memset(kva, 0, PAGE_SIZE);	/* paranoid/superfluous */
		sc->sc_poll_sdmapage = kva;

		err = bus_dmamap_create(sc->sc_dmat, sizeof(gtmpsc_polltx_t), 1,
		   sizeof(gtmpsc_polltx_t), 0, BUS_DMA_NOWAIT,
		   &sc->sc_txdma_map);
		if (err != 0) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamap_create error 0x%x\n", err);
			goto fail2;
		}
		err = bus_dmamap_load(sc->sc_dmat, sc->sc_txdma_map,
		    sc->sc_poll_sdmapage->tx, sizeof(gtmpsc_polltx_t),
		    NULL, BUS_DMA_NOWAIT | BUS_DMA_READ | BUS_DMA_WRITE);
		if (err != 0) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamap_load tx error 0x%x\n", err);
			goto fail3;
		}
		err = bus_dmamap_create(sc->sc_dmat, sizeof(gtmpsc_pollrx_t), 1,
		   sizeof(gtmpsc_pollrx_t), 0, BUS_DMA_NOWAIT,
		   &sc->sc_rxdma_map);
		if (err != 0) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamap_create rx error 0x%x\n", err);
			goto fail4;
		}
		err = bus_dmamap_load(sc->sc_dmat, sc->sc_rxdma_map,
		    sc->sc_poll_sdmapage->rx, sizeof(gtmpsc_pollrx_t),
		    NULL, BUS_DMA_NOWAIT | BUS_DMA_READ | BUS_DMA_WRITE);
		if (err != 0) {
			aprint_error_dev(sc->sc_dev,
			    "bus_dmamap_load rx error 0x%x\n", err);
			goto fail5;
		}

		sc->sc_brg = unit;		/* XXXXX */
		sc->sc_baudrate = GT_MPSC_DEFAULT_BAUD_RATE;
	}
	aprint_normal_dev(self, "with SDMA offset 0x%04x-0x%04x\n",
	    GTSDMA_BASE(unit), GTSDMA_BASE(unit) + GTSDMA_SIZE - 1);

	sc->sc_rx_ready = 0;
	sc->sc_tx_busy = 0;
	sc->sc_tx_done = 0;
	sc->sc_tx_stopped = 0;
	sc->sc_heldchange = 0;

	gtmpsc_txdesc_init(sc);
	gtmpsc_rxdesc_init(sc);

	sc->sc_tty = tp = tty_alloc();
	tp->t_oproc = gtmpscstart;
	tp->t_param = gtmpscparam;
	tty_attach(tp);

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_HIGH);

	/*
	 * clear any pending SDMA interrupts for this unit
	 */
	(void) gt_sdma_icause(device_parent(sc->sc_dev),
	    SDMA_INTR_RXBUF(sc->sc_unit) |
	    SDMA_INTR_RXERR(sc->sc_unit) |
	    SDMA_INTR_TXBUF(sc->sc_unit) |
	    SDMA_INTR_TXEND(sc->sc_unit));

	sc->sc_si = softint_establish(SOFTINT_SERIAL, gtmpsc_softintr, sc);
	if (sc->sc_si == NULL)
		panic("mpscattach: cannot softint_establish IPL_SOFTSERIAL");

	shutdownhook_establish(gtmpsc_shutdownhook, sc);

	gtmpscinit_stop(sc);
	gtmpscinit_start(sc);

	if (sc->sc_flags & GTMPSC_CONSOLE) {
		int maj;

		/* locate the major number */
		maj = cdevsw_lookup_major(&gtmpsc_cdevsw);

		tp->t_dev = cn_tab->cn_dev =
		    makedev(maj, device_unit(sc->sc_dev));

		aprint_normal_dev(self, "console\n");
	}

#ifdef KGDB
	/*
	 * Allow kgdb to "take over" this port.  If this is
	 * the kgdb device, it has exclusive use.
	 */
	if (sc->sc_unit == gtmpsckgdbport) {
#ifdef MPSC_CONSOLE
		if (sc->sc_unit == MPSC_CONSOLE) {
			aprint_error_dev(self,
			    "(kgdb): cannot share with console\n");
			return;
		}
#endif

		sc->sc_flags |= GTMPSC_KGDB;
		aprint_normal_dev(self, "kgdb\n");

		gtmpsc_txflush(sc);

		kgdb_attach(gtmpsc_kgdb_getc, gtmpsc_kgdb_putc, NULL);
		kgdb_dev = 123;	/* unneeded, only to satisfy some tests */
		gtmpsc_kgdb_attached = 1;
		kgdb_connect(1);
	}
#endif /* KGDB */

	return;


fail5:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_rxdma_map);
fail4:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_txdma_map);
fail3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_txdma_map);
fail2:
	bus_dmamem_unmap(sc->sc_dmat, kva, PAGE_SIZE);
fail1:
	bus_dmamem_free(sc->sc_dmat, &segs, 1);
fail0:
	return;
}

/* ARGSUSED */
int
gtmpsc_intr(void *arg)
{
	struct gt_softc *gt = (struct gt_softc *)arg;
	struct gtmpsc_softc *sc;
	uint32_t icause;
	int i;

	icause = gt_sdma_icause(gt->sc_dev, sdma_imask);

	for (i = 0; i < GTMPSC_NCHAN; i++) {
		sc = device_lookup_private(&gtmpsc_cd, i);
		if (sc == NULL)
			continue;
		mutex_spin_enter(&sc->sc_lock);
		if (icause & SDMA_INTR_RXBUF(sc->sc_unit)) {
			gtmpsc_intr_rx(sc);
			icause &= ~SDMA_INTR_RXBUF(sc->sc_unit);
		}
		if (icause & SDMA_INTR_TXBUF(sc->sc_unit)) {
			gtmpsc_intr_tx(sc);
			icause &= ~SDMA_INTR_TXBUF(sc->sc_unit);
		}
		mutex_spin_exit(&sc->sc_lock);
	}

	return 1;
}

STATIC void
gtmpsc_softintr(void *arg)
{
	struct gtmpsc_softc *sc = arg;
	struct tty *tp = sc->sc_tty;
	gtmpsc_pollrx_t *vrxp;
	int code;
	u_int cc;
	u_char *get, *end, lsr;
	int (*rint)(int, struct tty *) = tp->t_linesw->l_rint;

	if (sc->sc_rx_ready) {
		sc->sc_rx_ready = 0;

		cc = sc->sc_rcvcnt;

		/* If not yet open, drop the entire buffer content here */
		if (!ISSET(tp->t_state, TS_ISOPEN))
			cc = 0;

		vrxp = &sc->sc_poll_sdmapage->rx[sc->sc_rcvrx];
		end = vrxp->rxbuf + vrxp->rxdesc.sdma_cnt;
		get = vrxp->rxbuf + sc->sc_roffset;
		while (cc > 0) {
			code = *get;
			lsr = vrxp->rxdesc.sdma_csr;

			if (ISSET(lsr,
			    SDMA_CSR_RX_PE |
			    SDMA_CSR_RX_FR |
			    SDMA_CSR_RX_OR |
			    SDMA_CSR_RX_BR)) {
				if (ISSET(lsr, SDMA_CSR_RX_OR))
					;	/* XXXXX not yet... */
				if (ISSET(lsr, SDMA_CSR_RX_BR | SDMA_CSR_RX_FR))
					SET(code, TTY_FE);
				if (ISSET(lsr, SDMA_CSR_RX_PE))
					SET(code, TTY_PE);
			}

			if ((*rint)(code, tp) == -1) {
				/*
				 * The line discipline's buffer is out of space.
				 */
				/* XXXXX not yet... */
			}
			if (++get >= end) {
				/* cleanup this descriptor, and return to DMA */
				CLEANUP_AND_RETURN_RXDMA(sc, sc->sc_rcvrx);
				sc->sc_rcvrx =
				    (sc->sc_rcvrx + 1) % GTMPSC_NTXDESC;
				vrxp = &sc->sc_poll_sdmapage->rx[sc->sc_rcvrx];
				end = vrxp->rxbuf + vrxp->rxdesc.sdma_cnt;
				get = vrxp->rxbuf + sc->sc_roffset;
			}
			cc--;
		}
	}
	if (sc->sc_tx_done) {
		sc->sc_tx_done = 0;
		CLR(tp->t_state, TS_BUSY);
		if (ISSET(tp->t_state, TS_FLUSH))
		    CLR(tp->t_state, TS_FLUSH);
		else
		    ndflush(&tp->t_outq, (int)(sc->sc_tba - tp->t_outq.c_cf));
		(*tp->t_linesw->l_start)(tp);
	}
}

int
gtmpscopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct gtmpsc_softc *sc;
	int unit = GTMPSCUNIT(dev);
	struct tty *tp;
	int s;
	int error;

	sc = device_lookup_private(&gtmpsc_cd, unit);
	if (!sc)
		return ENXIO;
#ifdef KGDB
	/*
	 * If this is the kgdb port, no other use is permitted.
	 */
	if (sc->sc_flags & GTMPSC_KGDB)
		return EBUSY;
#endif
	tp = sc->sc_tty;
	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return EBUSY;

	s = spltty();

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		struct termios t;

		tp->t_dev = dev;

		mutex_spin_enter(&sc->sc_lock);

		/* Turn on interrupts. */
		sdma_imask |= SDMA_INTR_RXBUF(sc->sc_unit);
		gt_sdma_imask(device_parent(sc->sc_dev), sdma_imask);

		/* Clear PPS capture state on first open. */
		mutex_spin_enter(&timecounter_lock);
		memset(&sc->sc_pps_state, 0, sizeof(sc->sc_pps_state));
		sc->sc_pps_state.ppscap = PPS_CAPTUREASSERT | PPS_CAPTURECLEAR;
		pps_init(&sc->sc_pps_state);
		mutex_spin_exit(&timecounter_lock);

		mutex_spin_exit(&sc->sc_lock);

		if (sc->sc_flags & GTMPSC_CONSOLE) {
			t.c_ospeed = sc->sc_baudrate;
			t.c_cflag = sc->sc_cflag;
		} else {
			t.c_ospeed = TTYDEF_SPEED;
			t.c_cflag = TTYDEF_CFLAG;
		}
		t.c_ispeed = t.c_ospeed;

		/* Make sure gtmpscparam() will do something. */
		tp->t_ospeed = 0;
		(void) gtmpscparam(tp, &t);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		mutex_spin_enter(&sc->sc_lock);

		/* Clear the input/output ring */
		sc->sc_rcvcnt = 0;
		sc->sc_roffset = 0;
		sc->sc_rcvrx = 0;
		sc->sc_rcvdrx = 0;
		sc->sc_nexttx = 0;
		sc->sc_lasttx = 0;

		/*
		 * enable SDMA receive
		 */
		GT_SDMA_WRITE(sc, SDMA_SDCM, SDMA_SDCM_ERD);

		mutex_spin_exit(&sc->sc_lock);
	}
	splx(s);
	error = ttyopen(tp, GTMPSCDIALOUT(dev), ISSET(flag, O_NONBLOCK));
	if (error)
		goto bad;

	error = (*tp->t_linesw->l_open)(dev, tp);
	if (error)
		goto bad;

	return 0;

bad:
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		/*
		 * We failed to open the device, and nobody else had it opened.
		 * Clean up the state as appropriate.
		 */
		gtmpscshutdown(sc);
	}

	return error;
}

int
gtmpscclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	int unit = GTMPSCUNIT(dev);
	struct gtmpsc_softc *sc = device_lookup_private(&gtmpsc_cd, unit);
	struct tty *tp = sc->sc_tty;

	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		gtmpscshutdown(sc);
	}

	return 0;
}

int
gtmpscread(dev_t dev, struct uio *uio, int flag)
{
	struct gtmpsc_softc *sc =
	    device_lookup_private(&gtmpsc_cd, GTMPSCUNIT(dev));
	struct tty *tp = sc->sc_tty;

	return (*tp->t_linesw->l_read)(tp, uio, flag);
}

int
gtmpscwrite(dev_t dev, struct uio *uio, int flag)
{
	struct gtmpsc_softc *sc =
	    device_lookup_private(&gtmpsc_cd, GTMPSCUNIT(dev));
	struct tty *tp = sc->sc_tty;

	return (*tp->t_linesw->l_write)(tp, uio, flag);
}

int
gtmpscioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct gtmpsc_softc *sc =
	    device_lookup_private(&gtmpsc_cd, GTMPSCUNIT(dev));
	struct tty *tp = sc->sc_tty;
	int error;

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	error = ttioctl(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	error = 0;
	switch (cmd) {
	case TIOCSFLAGS:
		error = kauth_authorize_device_tty(l->l_cred,
		    KAUTH_DEVICE_TTY_PRIVSET, tp);
		if (error)
			return error;
		break;
	default:
		/* nothing */
		break;
	}

	mutex_spin_enter(&sc->sc_lock);

	switch (cmd) {
	case PPS_IOC_CREATE:
	case PPS_IOC_DESTROY:
	case PPS_IOC_GETPARAMS:
	case PPS_IOC_SETPARAMS:
	case PPS_IOC_GETCAP:
	case PPS_IOC_FETCH:
#ifdef PPS_SYNC
	case PPS_IOC_KCBIND:
#endif
		mutex_spin_enter(&timecounter_lock);
		error = pps_ioctl(cmd, data, &sc->sc_pps_state);
		mutex_spin_exit(&timecounter_lock);
		break;

	case TIOCDCDTIMESTAMP:	/* XXX old, overloaded  API used by xntpd v3 */
		mutex_spin_enter(&timecounter_lock);
#ifndef PPS_TRAILING_EDGE
		TIMESPEC_TO_TIMEVAL((struct timeval *)data,
		    &sc->sc_pps_state.ppsinfo.assert_timestamp);
#else
		TIMESPEC_TO_TIMEVAL((struct timeval *)data,
		    &sc->sc_pps_state.ppsinfo.clear_timestamp);
#endif
		mutex_spin_exit(&timecounter_lock);
		break;

	default:
		error = EPASSTHROUGH;
		break;
	}

	mutex_spin_exit(&sc->sc_lock);

	return error;
}

void
gtmpscstop(struct tty *tp, int flag)
{
}

struct tty *
gtmpsctty(dev_t dev)
{
	struct gtmpsc_softc *sc =
	    device_lookup_private(&gtmpsc_cd, GTMPSCUNIT(dev));

	return sc->sc_tty;
}

int
gtmpscpoll(dev_t dev, int events, struct lwp *l)
{
	struct gtmpsc_softc *sc =
	    device_lookup_private(&gtmpsc_cd, GTMPSCUNIT(dev));
	struct tty *tp = sc->sc_tty;

	return (*tp->t_linesw->l_poll)(tp, events, l);
}


STATIC void
gtmpscstart(struct tty *tp)
{
	struct gtmpsc_softc *sc;
	unsigned char *tba;
	unsigned int unit;
	int s, tbc;

	unit = GTMPSCUNIT(tp->t_dev);
	sc = device_lookup_private(&gtmpsc_cd, unit);
	if (sc == NULL)
		return;

	s = spltty();
	if (ISSET(tp->t_state, TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto out;
	if (sc->sc_tx_stopped)
		goto out;
	if (!ttypull(tp))
		goto out;

	/* Grab the first contiguous region of buffer space. */
	tba = tp->t_outq.c_cf;
	tbc = ndqb(&tp->t_outq, 0);

	mutex_spin_enter(&sc->sc_lock);

	sc->sc_tba = tba;
	sc->sc_tbc = tbc;

	sdma_imask |= SDMA_INTR_TXBUF(sc->sc_unit);
	gt_sdma_imask(device_parent(sc->sc_dev), sdma_imask);
	SET(tp->t_state, TS_BUSY);
	sc->sc_tx_busy = 1;
	gtmpsc_write(sc);

	mutex_spin_exit(&sc->sc_lock);
out:
	splx(s);
}

STATIC int
gtmpscparam(struct tty *tp, struct termios *t)
{
	struct gtmpsc_softc *sc =
	    device_lookup_private(&gtmpsc_cd, GTMPSCUNIT(tp->t_dev));

	/* Check requested parameters. */
	if (compute_cdv(t->c_ospeed) < 0)
		return EINVAL;
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return EINVAL;

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag)
		return 0;

	mutex_spin_enter(&sc->sc_lock);

	/* And copy to tty. */
	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	sc->sc_baudrate = t->c_ospeed;

	if (!sc->sc_heldchange) {
		if (sc->sc_tx_busy) {
			sc->sc_heldtbc = sc->sc_tbc;
			sc->sc_tbc = 0;
			sc->sc_heldchange = 1;
		} else
			gtmpsc_loadchannelregs(sc);
	}

	mutex_spin_exit(&sc->sc_lock);

	/* Fake carrier on */
	(void) (*tp->t_linesw->l_modem)(tp, 1);

	return 0;
}

void
gtmpsc_shutdownhook(void *arg)
{
	gtmpsc_softc_t *sc = (gtmpsc_softc_t *)arg;

	gtmpsc_txflush(sc);
}

/*
 * Convert to MPCR from cflag(CS[5678] and CSTOPB).
 */
STATIC uint32_t
cflag2mpcr(tcflag_t cflag)
{
	uint32_t mpcr = 0;

	switch (ISSET(cflag, CSIZE)) {
	case CS5:
		SET(mpcr, GTMPSC_MPCR_CL_5);
		break;
	case CS6:
		SET(mpcr, GTMPSC_MPCR_CL_6);
		break;
	case CS7:
		SET(mpcr, GTMPSC_MPCR_CL_7);
		break;
	case CS8:
		SET(mpcr, GTMPSC_MPCR_CL_8);
		break;
	}
	if (ISSET(cflag, CSTOPB))
		SET(mpcr, GTMPSC_MPCR_SBL_2);

	return mpcr;
}

STATIC void
gtmpsc_intr_rx(struct gtmpsc_softc *sc)
{
	gtmpsc_pollrx_t *vrxp;
	uint32_t csr;
	int kick, ix;

	kick = 0;

	/* already handled in gtmpsc_common_getc() */
	if (sc->sc_rcvdrx == sc->sc_rcvrx)
		return;

	ix = sc->sc_rcvdrx;
	vrxp = &sc->sc_poll_sdmapage->rx[ix];
	bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
	    ix * sizeof(gtmpsc_pollrx_t),
	    sizeof(sdma_desc_t),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	csr = vrxp->rxdesc.sdma_csr;
	while (!(csr & SDMA_CSR_RX_OWN)) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxdma_map,
		    ix * sizeof(gtmpsc_pollrx_t) + sizeof(sdma_desc_t),
		    sizeof(vrxp->rxbuf),
		    BUS_DMASYNC_POSTREAD);
		vrxp->rxdesc.sdma_cnt &= SDMA_RX_CNT_BCNT_MASK;
		if (vrxp->rxdesc.sdma_csr & SDMA_CSR_RX_BR) {
			int cn_trapped = 0;

			cn_check_magic(sc->sc_tty->t_dev,
			    CNC_BREAK, gtmpsc_cnm_state);
			if (cn_trapped)
				continue;
#if defined(KGDB) && !defined(DDB)
			if (ISSET(sc->sc_flags, GTMPSC_KGDB)) {
				kgdb_connect(1);
				continue;
			}
#endif
		}

		sc->sc_rcvcnt += vrxp->rxdesc.sdma_cnt;
		kick = 1;

		ix = (ix + 1) % GTMPSC_NTXDESC;
		vrxp = &sc->sc_poll_sdmapage->rx[ix];
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_pollrx_t),
		    sizeof(sdma_desc_t),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		csr = vrxp->rxdesc.sdma_csr;
	}
	bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
	    ix * sizeof(gtmpsc_pollrx_t),
	    sizeof(sdma_desc_t),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (kick) {
		sc->sc_rcvdrx = ix;
		sc->sc_rx_ready = 1;
		softint_schedule(sc->sc_si);
	}
}

STATIC __inline void
gtmpsc_intr_tx(struct gtmpsc_softc *sc)
{
	gtmpsc_polltx_t *vtxp;
	uint32_t csr;
	int ix;

	/*
	 * If we've delayed a parameter change, do it now,
	 * and restart output.
	 */
	if (sc->sc_heldchange) {
		gtmpsc_loadchannelregs(sc);
		sc->sc_heldchange = 0;
		sc->sc_tbc = sc->sc_heldtbc;
		sc->sc_heldtbc = 0;
	}

	/* Clean-up TX descriptors and buffers */
	ix = sc->sc_lasttx;
	while (ix != sc->sc_nexttx) {
		vtxp = &sc->sc_poll_sdmapage->tx[ix];
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_polltx_t), sizeof(sdma_desc_t),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		csr = vtxp->txdesc.sdma_csr;
		if (csr & SDMA_CSR_TX_OWN) {
			bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
			    ix * sizeof(gtmpsc_polltx_t), sizeof(sdma_desc_t),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_polltx_t) + sizeof(sdma_desc_t),
		    sizeof(vtxp->txbuf), BUS_DMASYNC_POSTWRITE);
		ix = (ix + 1) % GTMPSC_NTXDESC;
	}
	sc->sc_lasttx = ix;

	/* Output the next chunk of the contiguous buffer */
	gtmpsc_write(sc);
	if (sc->sc_tbc == 0 && sc->sc_tx_busy) {
		sc->sc_tx_busy = 0;
		sc->sc_tx_done = 1;
		softint_schedule(sc->sc_si);
		sdma_imask &= ~SDMA_INTR_TXBUF(sc->sc_unit);
		gt_sdma_imask(device_parent(sc->sc_dev), sdma_imask);
	}
}

/*
 * gtmpsc_write - write a buffer into the hardware
 */
STATIC void
gtmpsc_write(struct gtmpsc_softc *sc)
{
	gtmpsc_polltx_t *vtxp;
	uint32_t sdcm, ix;
	int kick, n;

	kick = 0;
	while (sc->sc_tbc > 0 && sc->sc_nexttx != sc->sc_lasttx) {
		n = min(sc->sc_tbc, GTMPSC_TXBUFSZ);

		ix = sc->sc_nexttx;
		sc->sc_nexttx = (ix + 1) % GTMPSC_NTXDESC;

		vtxp = &sc->sc_poll_sdmapage->tx[ix];

		memcpy(vtxp->txbuf, sc->sc_tba, n);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_polltx_t) + sizeof(sdma_desc_t),
		    sizeof(vtxp->txbuf), BUS_DMASYNC_PREWRITE);

		vtxp->txdesc.sdma_cnt = (n << SDMA_TX_CNT_BCNT_SHIFT) | n;
		vtxp->txdesc.sdma_csr =
		    SDMA_CSR_TX_L	|
		    SDMA_CSR_TX_F	|
		    SDMA_CSR_TX_EI	|
		    SDMA_CSR_TX_OWN;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_polltx_t), sizeof(sdma_desc_t),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		sc->sc_tbc -= n;
		sc->sc_tba += n;
		kick = 1;
	}
	if (kick) {
		/*
		 * now kick some SDMA
		 */
		sdcm = GT_SDMA_READ(sc, SDMA_SDCM);
		if ((sdcm & SDMA_SDCM_TXD) == 0)
			GT_SDMA_WRITE(sc, SDMA_SDCM, sdcm | SDMA_SDCM_TXD);
	}
}

/*
 * gtmpsc_txflush - wait for output to drain
 */
STATIC void
gtmpsc_txflush(gtmpsc_softc_t *sc)
{
	gtmpsc_polltx_t *vtxp;
	int ix, limit = 4000000;	/* 4 seconds */

	ix = sc->sc_nexttx - 1;
	if (ix < 0)
		ix = GTMPSC_NTXDESC - 1;

	vtxp = &sc->sc_poll_sdmapage->tx[ix];
	while (limit > 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_polltx_t), sizeof(sdma_desc_t),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if ((vtxp->txdesc.sdma_csr & SDMA_CSR_TX_OWN) == 0)
			break;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_polltx_t), sizeof(sdma_desc_t),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		DELAY(1);
		limit -= 1;
	}
}

/*
 * gtmpsc_rxdesc_init - set up RX descriptor ring
 */
STATIC void
gtmpsc_rxdesc_init(struct gtmpsc_softc *sc)
{
	gtmpsc_pollrx_t *vrxp, *prxp, *first_prxp;
	sdma_desc_t *dp;
	int i;

	first_prxp = prxp =
	    (gtmpsc_pollrx_t *)sc->sc_rxdma_map->dm_segs->ds_addr;
	vrxp = sc->sc_poll_sdmapage->rx;
	for (i = 0; i < GTMPSC_NRXDESC; i++) {
		dp = &vrxp->rxdesc;
		dp->sdma_csr =
		    SDMA_CSR_RX_L|SDMA_CSR_RX_F|SDMA_CSR_RX_OWN|SDMA_CSR_RX_EI;
		dp->sdma_cnt = GTMPSC_RXBUFSZ << SDMA_RX_CNT_BUFSZ_SHIFT;
		dp->sdma_bufp = (uint32_t)&prxp->rxbuf;
		vrxp++;
		prxp++;
		dp->sdma_next = (uint32_t)&prxp->rxdesc;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxdma_map,
		    i * sizeof(gtmpsc_pollrx_t) + sizeof(sdma_desc_t),
		    sizeof(vrxp->rxbuf), BUS_DMASYNC_PREREAD);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxdma_map,
		    i * sizeof(gtmpsc_pollrx_t), sizeof(sdma_desc_t),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	dp = &vrxp->rxdesc;
	dp->sdma_csr =
	    SDMA_CSR_RX_L | SDMA_CSR_RX_F | SDMA_CSR_RX_OWN | SDMA_CSR_RX_EI;
	dp->sdma_cnt = GTMPSC_RXBUFSZ << SDMA_RX_CNT_BUFSZ_SHIFT;
	dp->sdma_bufp = (uint32_t)&prxp->rxbuf;
	dp->sdma_next = (uint32_t)&first_prxp->rxdesc;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_rxdma_map,
	    i * sizeof(gtmpsc_pollrx_t) + sizeof(sdma_desc_t),
	    sizeof(vrxp->rxbuf), BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_rxdma_map,
	    i * sizeof(gtmpsc_pollrx_t), sizeof(sdma_desc_t),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	sc->sc_rcvcnt = 0;
	sc->sc_roffset = 0;
	sc->sc_rcvrx = 0;
	sc->sc_rcvdrx = 0;
}

/*
 * gtmpsc_txdesc_init - set up TX descriptor ring
 */
STATIC void
gtmpsc_txdesc_init(struct gtmpsc_softc *sc)
{
	gtmpsc_polltx_t *vtxp, *ptxp, *first_ptxp;
	sdma_desc_t *dp;
	int i;

	first_ptxp = ptxp =
	    (gtmpsc_polltx_t *)sc->sc_txdma_map->dm_segs->ds_addr;
	vtxp = sc->sc_poll_sdmapage->tx;
	for (i = 0; i < GTMPSC_NTXDESC; i++) {
		dp = &vtxp->txdesc;
		dp->sdma_csr = 0;
		dp->sdma_cnt = 0;
		dp->sdma_bufp = (uint32_t)&ptxp->txbuf;
		vtxp++;
		ptxp++;
		dp->sdma_next = (uint32_t)&ptxp->txdesc;
	}
	dp = &vtxp->txdesc;
	dp->sdma_csr = 0;
	dp->sdma_cnt = 0;
	dp->sdma_bufp = (uint32_t)&ptxp->txbuf;
	dp->sdma_next = (uint32_t)&first_ptxp->txdesc;

	sc->sc_nexttx = 0;
	sc->sc_lasttx = 0;
}

STATIC void
gtmpscinit_stop(struct gtmpsc_softc *sc)
{
	uint32_t csr;
	int timo = 10000;	/* XXXX */

	/* Abort MPSC Rx (aborting Tx messes things up) */
	GT_MPSC_WRITE(sc, GTMPSC_CHRN(2), GTMPSC_CHR2_RXABORT);

	/* abort SDMA RX and stop TX for MPSC unit */
	GT_SDMA_WRITE(sc, SDMA_SDCM, SDMA_SDCM_AR | SDMA_SDCM_STD);

	/* poll for SDMA RX abort completion */
	for (; timo > 0; timo--) {
		csr = GT_SDMA_READ(sc, SDMA_SDCM);
		if (!(csr & (SDMA_SDCM_AR | SDMA_SDCM_AT)))
			break;
		DELAY(50);
	}
}

STATIC void
gtmpscinit_start(struct gtmpsc_softc *sc)
{

	/*
	 * Set pointers of current/first descriptor of TX to SDMA register.
	 */
	GT_SDMA_WRITE(sc, SDMA_SCTDP, sc->sc_txdma_map->dm_segs->ds_addr);
	GT_SDMA_WRITE(sc, SDMA_SFTDP, sc->sc_txdma_map->dm_segs->ds_addr);

	/*
	 * Set pointer of current descriptor of TX to SDMA register.
	 */
	GT_SDMA_WRITE(sc, SDMA_SCRDP, sc->sc_rxdma_map->dm_segs->ds_addr);

	/*
	 * initialize SDMA unit Configuration Register
	 */
	GT_SDMA_WRITE(sc, SDMA_SDC,
	    SDMA_SDC_BSZ_8x64 | SDMA_SDC_SFM|SDMA_SDC_RFT);

	gtmpsc_loadchannelregs(sc);

	/*
	 * set MPSC LO and HI port config registers for GTMPSC unit
 	 */
	GT_MPSC_WRITE(sc, GTMPSC_MMCR_LO,
	    GTMPSC_MMCR_LO_MODE_UART	|
	    GTMPSC_MMCR_LO_ET		|
	    GTMPSC_MMCR_LO_ER		|
	    GTMPSC_MMCR_LO_NLM);
	GT_MPSC_WRITE(sc, GTMPSC_MMCR_HI,
	    GTMPSC_MMCR_HI_TCDV_DEFAULT	|
	    GTMPSC_MMCR_HI_RDW		|
	    GTMPSC_MMCR_HI_RCDV_DEFAULT);

	/*
	 * tell MPSC receive the Enter Hunt
	 */
	GT_MPSC_WRITE(sc, GTMPSC_CHRN(2), GTMPSC_CHR2_EH);
}

STATIC void
gtmpscshutdown(struct gtmpsc_softc *sc)
{
	struct tty *tp;

#ifdef KGDB
	if (sc->sc_flags & GTMPSCF_KGDB != 0)
		return;
#endif
	tp = sc->sc_tty;
	mutex_spin_enter(&sc->sc_lock);
	/* Fake carrier off */
	(void) (*tp->t_linesw->l_modem)(tp, 0);
	sdma_imask &= ~SDMA_INTR_RXBUF(sc->sc_unit);
	gt_sdma_imask(device_parent(sc->sc_dev), sdma_imask);
	mutex_spin_exit(&sc->sc_lock);
}

STATIC void
gtmpsc_loadchannelregs(struct gtmpsc_softc *sc)
{

	if (sc->sc_dev != NULL)
		gt_brg_bcr(device_parent(sc->sc_dev), sc->sc_brg,
	    	    GT_MPSC_CLOCK_SOURCE | compute_cdv(sc->sc_baudrate));
	GT_MPSC_WRITE(sc, GTMPSC_CHRN(3), GTMPSC_MAXIDLE(sc->sc_baudrate));

	/*
	 * set MPSC Protocol configuration register for GTMPSC unit
	 */
	GT_MPSC_WRITE(sc, GTMPSC_MPCR, cflag2mpcr(sc->sc_cflag));
}


#ifdef MPSC_CONSOLE
/*
 * Following are all routines needed for MPSC to act as console
 */
STATIC int
gtmpsccngetc(dev_t dev)
{

	return gtmpsc_common_getc(&gtmpsc_cn_softc);
}

STATIC void
gtmpsccnputc(dev_t dev, int c)
{

	gtmpsc_common_putc(&gtmpsc_cn_softc, c);
}

STATIC void
gtmpsccnpollc(dev_t dev, int on)
{
}

STATIC void
gtmpsccnhalt(dev_t dev)
{
	gtmpsc_softc_t *sc = &gtmpsc_cn_softc;
	uint32_t csr;

	/*
	 * flush TX buffers
	 */
	gtmpsc_txflush(sc);

	/*
	 * stop MPSC unit RX
	 */
	csr = GT_MPSC_READ(sc, GTMPSC_CHRN(2));
	csr &= ~GTMPSC_CHR2_EH;
	csr |= GTMPSC_CHR2_RXABORT;
	GT_MPSC_WRITE(sc, GTMPSC_CHRN(2), csr);

	DELAY(GTMPSC_RESET_DELAY);

	/*
	 * abort SDMA RX for MPSC unit
	 */
	GT_SDMA_WRITE(sc, SDMA_SDCM, SDMA_SDCM_AR);
}

int
gtmpsccnattach(bus_space_tag_t iot, bus_dma_tag_t dmat, bus_addr_t base,
	       int unit, int brg, int speed, tcflag_t tcflag)
{
	struct gtmpsc_softc *sc = &gtmpsc_cn_softc;
	int i, res;
	const unsigned char cp[] = "\r\nMPSC Lives!\r\n";

	res = gtmpsc_hackinit(sc, iot, dmat, base, unit, brg, speed, tcflag);
	if (res != 0)
		return res;

	gtmpscinit_stop(sc);
	gtmpscinit_start(sc);

	/*
	 * enable SDMA receive
	 */
	GT_SDMA_WRITE(sc, SDMA_SDCM, SDMA_SDCM_ERD);

	for (i = 0; i < sizeof(cp); i++) {
		if (*(cp + i) == 0)
			break;
		gtmpsc_common_putc(sc, *(cp + i));
	}

	cn_tab = &gtmpsc_consdev;
	cn_init_magic(&gtmpsc_cnm_state);

	return 0;
}

/*
 * gtmpsc_hackinit - hacks required to supprt GTMPSC console
 */
STATIC int
gtmpsc_hackinit(struct gtmpsc_softc *sc, bus_space_tag_t iot,
		bus_dma_tag_t dmat, bus_addr_t base, int unit, int brg,
		int baudrate, tcflag_t tcflag)
{
	gtmpsc_poll_sdma_t *cn_dmapage =
	    (gtmpsc_poll_sdma_t *)gtmpsc_cn_dmapage;
	int error;

	DPRINTF(("hackinit\n"));

	memset(sc, 0, sizeof(struct gtmpsc_softc));
	error = bus_space_map(iot, base + GTMPSC_BASE(unit), GTMPSC_SIZE, 0,
	    &sc->sc_mpsch);
	if (error != 0)
		goto fail0;

	error = bus_space_map(iot, base + GTSDMA_BASE(unit), GTSDMA_SIZE, 0,
	    &sc->sc_sdmah);
	if (error != 0)
		goto fail1;
	error = bus_dmamap_create(dmat, sizeof(gtmpsc_polltx_t), 1,
	   sizeof(gtmpsc_polltx_t), 0, BUS_DMA_NOWAIT, &sc->sc_txdma_map);
	if (error != 0)
		goto fail2;
	error = bus_dmamap_load(dmat, sc->sc_txdma_map, cn_dmapage->tx,
	    sizeof(gtmpsc_polltx_t), NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_READ | BUS_DMA_WRITE);
	if (error != 0)
		goto fail3;
	error = bus_dmamap_create(dmat, sizeof(gtmpsc_pollrx_t), 1,
	   sizeof(gtmpsc_pollrx_t), 0, BUS_DMA_NOWAIT,
	   &sc->sc_rxdma_map);
	if (error != 0)
		goto fail4;
	error = bus_dmamap_load(dmat, sc->sc_rxdma_map, cn_dmapage->rx,
	    sizeof(gtmpsc_pollrx_t), NULL,
	    BUS_DMA_NOWAIT | BUS_DMA_READ | BUS_DMA_WRITE);
	if (error != 0)
		goto fail5;

	sc->sc_iot = iot;
	sc->sc_dmat = dmat;
	sc->sc_poll_sdmapage = cn_dmapage;
	sc->sc_brg = brg;
	sc->sc_baudrate = baudrate;
	sc->sc_cflag = tcflag;

	gtmpsc_txdesc_init(sc);
	gtmpsc_rxdesc_init(sc);

	return 0;

fail5:
	bus_dmamap_destroy(dmat, sc->sc_rxdma_map);
fail4:
	bus_dmamap_unload(dmat, sc->sc_txdma_map);
fail3:
	bus_dmamap_destroy(dmat, sc->sc_txdma_map);
fail2:
	bus_space_unmap(iot, sc->sc_sdmah, GTSDMA_SIZE);
fail1:
	bus_space_unmap(iot, sc->sc_mpsch, GTMPSC_SIZE);
fail0:
	return error;
}
#endif	/* MPSC_CONSOLE */

#ifdef KGDB
STATIC int
gtmpsc_kgdb_getc(void *arg)
{
	struct gtmpsc_softc *sc = (struct gtmpsc_softc *)arg;

	return gtmpsc_common_getc(sc);
}

STATIC void
gtmpsc_kgdb_putc(void *arg, int c)
{
	struct gtmpsc_softc *sc = (struct gtmpsc_softc *)arg;

	return gtmpsc_common_putc(sc, c);
}
#endif /* KGDB */

#if defined(MPSC_CONSOLE) || defined(KGDB)
/*
 * gtmpsc_common_getc - polled console read
 *
 *	We copy data from the DMA buffers into a buffer in the softc
 *	to reduce descriptor ownership turnaround time
 *	MPSC can crater if it wraps descriptor rings,
 *	which is asynchronous and throttled only by line speed.
 */
STATIC int
gtmpsc_common_getc(struct gtmpsc_softc *sc)
{
	gtmpsc_pollrx_t *vrxp;
	uint32_t csr;
	int ix, ch, wdog_interval = 0;

	if (!cold)
		mutex_spin_enter(&sc->sc_lock);

	ix = sc->sc_rcvdrx;
	vrxp = &sc->sc_poll_sdmapage->rx[ix];
	while (sc->sc_rcvcnt == 0) {
		/* Wait receive */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxdma_map,
		    ix * sizeof(gtmpsc_pollrx_t),
		    sizeof(sdma_desc_t),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		csr = vrxp->rxdesc.sdma_csr;
		if (csr & SDMA_CSR_RX_OWN) {
			GT_MPSC_WRITE(sc, GTMPSC_CHRN(2),
			    GTMPSC_CHR2_EH | GTMPSC_CHR2_CRD);
			if (wdog_interval++ % 32)
				gt_watchdog_service();
			bus_dmamap_sync(sc->sc_dmat, sc->sc_rxdma_map,
			    ix * sizeof(gtmpsc_pollrx_t),
			    sizeof(sdma_desc_t),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			DELAY(50);
			continue;
		}
		if (csr & SDMA_CSR_RX_ES)
			aprint_error_dev(sc->sc_dev,
			    "RX error, rxdesc csr 0x%x\n", csr);

		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxdma_map,
		    ix * sizeof(gtmpsc_pollrx_t) + sizeof(sdma_desc_t),
		    sizeof(vrxp->rxbuf),
		    BUS_DMASYNC_POSTREAD);

		vrxp->rxdesc.sdma_cnt &= SDMA_RX_CNT_BCNT_MASK;
		sc->sc_rcvcnt = vrxp->rxdesc.sdma_cnt;
		sc->sc_roffset = 0;
		sc->sc_rcvdrx = (ix + 1) % GTMPSC_NRXDESC;

		if (sc->sc_rcvcnt == 0) {
			/* cleanup this descriptor, and return to DMA */
			CLEANUP_AND_RETURN_RXDMA(sc, sc->sc_rcvrx);
			sc->sc_rcvrx = sc->sc_rcvdrx;
		}

		ix = sc->sc_rcvdrx;
		vrxp = &sc->sc_poll_sdmapage->rx[ix];
	}
	ch = vrxp->rxbuf[sc->sc_roffset++];
	sc->sc_rcvcnt--;

	if (sc->sc_roffset == vrxp->rxdesc.sdma_cnt) {
		/* cleanup this descriptor, and return to DMA */
		CLEANUP_AND_RETURN_RXDMA(sc, ix);
		sc->sc_rcvrx = (ix + 1) % GTMPSC_NRXDESC;
	}

	gt_watchdog_service();

	if (!cold)
		mutex_spin_exit(&sc->sc_lock);
	return ch;
}

STATIC void
gtmpsc_common_putc(struct gtmpsc_softc *sc, int c)
{
	gtmpsc_polltx_t *vtxp;
	int ix;
	const int nc = 1;

	/* Get a DMA descriptor */
	if (!cold)
		mutex_spin_enter(&sc->sc_lock);
	ix = sc->sc_nexttx;
	sc->sc_nexttx = (ix + 1) % GTMPSC_NTXDESC;
	if (sc->sc_nexttx == sc->sc_lasttx) {
		gtmpsc_common_putc_wait_complete(sc, sc->sc_lasttx);
		sc->sc_lasttx = (sc->sc_lasttx + 1) % GTMPSC_NTXDESC;
	}
	if (!cold)
		mutex_spin_exit(&sc->sc_lock);

	vtxp = &sc->sc_poll_sdmapage->tx[ix];
	vtxp->txbuf[0] = c;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
	    ix * sizeof(gtmpsc_polltx_t) + sizeof(sdma_desc_t),
	    sizeof(vtxp->txbuf),
	    BUS_DMASYNC_PREWRITE);

	vtxp->txdesc.sdma_cnt = (nc << SDMA_TX_CNT_BCNT_SHIFT) | nc;
	vtxp->txdesc.sdma_csr = SDMA_CSR_TX_L | SDMA_CSR_TX_F | SDMA_CSR_TX_OWN;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
	    ix * sizeof(gtmpsc_polltx_t),
	    sizeof(sdma_desc_t),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (!cold)
		mutex_spin_enter(&sc->sc_lock);
	/*
	 * now kick some SDMA
	 */
	GT_SDMA_WRITE(sc, SDMA_SDCM, SDMA_SDCM_TXD);

	while (sc->sc_lasttx != sc->sc_nexttx) {
		gtmpsc_common_putc_wait_complete(sc, sc->sc_lasttx);
		sc->sc_lasttx = (sc->sc_lasttx + 1) % GTMPSC_NTXDESC;
	}
	if (!cold)
		mutex_spin_exit(&sc->sc_lock);
}

/*
 * gtmpsc_common_putc - polled console putc
 */
STATIC void
gtmpsc_common_putc_wait_complete(struct gtmpsc_softc *sc, int ix)
{
	gtmpsc_polltx_t *vtxp = &sc->sc_poll_sdmapage->tx[ix];
	uint32_t csr;
	int wdog_interval = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
	    ix * sizeof(gtmpsc_polltx_t),
	    sizeof(sdma_desc_t),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	csr = vtxp->txdesc.sdma_csr;
	while (csr & SDMA_CSR_TX_OWN) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_polltx_t),
		    sizeof(sdma_desc_t),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		DELAY(40);
		if (wdog_interval++ % 32)
			gt_watchdog_service();
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
		    ix * sizeof(gtmpsc_polltx_t),
		    sizeof(sdma_desc_t),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		csr = vtxp->txdesc.sdma_csr;
	}
	if (csr & SDMA_CSR_TX_ES)
		aprint_error_dev(sc->sc_dev,
		    "TX error, txdesc(%d) csr 0x%x\n", ix, csr);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_txdma_map,
	    ix * sizeof(gtmpsc_polltx_t) + sizeof(sdma_desc_t),
	    sizeof(vtxp->txbuf),
	    BUS_DMASYNC_POSTWRITE);
}
#endif	/* defined(MPSC_CONSOLE) || defined(KGDB) */
