/*	$NetBSD: dpt.c,v 1.72 2014/10/18 08:33:27 snj Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran, Charles M. Hannum and by Jason R. Thorpe of the Numerical
 * Aerospace Simulation Facility, NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation
 * All rights reserved.
 *
 * TERMS AND CONDITIONS OF USE
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Adaptec and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose, are disclaimed. In no
 * event shall Adaptec be liable for any direct, indirect, incidental, special,
 * exemplary or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruptions) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this driver software, even
 * if advised of the possibility of such damage.
 */

/*
 * Portions of this code fall under the following copyright:
 *
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dpt.c,v 1.72 2014/10/18 08:33:27 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#include <sys/bus.h>
#ifdef i386
#include <machine/pio.h>
#include <machine/cputypes.h>
#endif

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/dptreg.h>
#include <dev/ic/dptvar.h>

#include <dev/i2o/dptivar.h>

#ifdef DEBUG
#define	DPRINTF(x)		printf x
#else
#define	DPRINTF(x)
#endif

#define dpt_inb(x, o)		\
    bus_space_read_1((x)->sc_iot, (x)->sc_ioh, (o))
#define dpt_outb(x, o, d)	\
    bus_space_write_1((x)->sc_iot, (x)->sc_ioh, (o), (d))

static const char * const dpt_cname[] = {
	"3334", "SmartRAID IV",
	"3332", "SmartRAID IV",
	"2144", "SmartCache IV",
	"2044", "SmartCache IV",
	"2142", "SmartCache IV",
	"2042", "SmartCache IV",
	"2041", "SmartCache IV",
	"3224", "SmartRAID III",
	"3222", "SmartRAID III",
	"3021", "SmartRAID III",
	"2124", "SmartCache III",
	"2024", "SmartCache III",
	"2122", "SmartCache III",
	"2022", "SmartCache III",
	"2021", "SmartCache III",
	"2012", "SmartCache Plus",
	"2011", "SmartCache Plus",
	NULL,   "<unknown>",
};

static void	*dpt_sdh;

dev_type_open(dptopen);
dev_type_ioctl(dptioctl);

const struct cdevsw dpt_cdevsw = {
	.d_open = dptopen,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = dptioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER,
};

extern struct cfdriver dpt_cd;

static struct dpt_sig dpt_sig = {
	{ 'd', 'P', 't', 'S', 'i', 'G'},
	SIG_VERSION,
#if defined(i386)
	PROC_INTEL,
#elif defined(powerpc)
	PROC_POWERPC,
#elif defined(alpha)
	PROC_ALPHA,
#elif defined(__mips__)
	PROC_MIPS,
#elif defined(sparc64)
	PROC_ULTRASPARC,
#else
	0xff,
#endif
#if defined(i386)
	PROC_386 | PROC_486 | PROC_PENTIUM | PROC_SEXIUM,
#else
	0,
#endif
	FT_HBADRVR,
	0,
	OEM_DPT,
	OS_FREE_BSD,	/* XXX */
	CAP_ABOVE16MB,
	DEV_ALL,
	ADF_ALL_EATA,
	0,
	0,
	DPT_VERSION,
	DPT_REVISION,
	DPT_SUBREVISION,
	DPT_MONTH,
	DPT_DAY,
	DPT_YEAR,
	""		/* Will be filled later */
};

static void	dpt_ccb_abort(struct dpt_softc *, struct dpt_ccb *);
static void	dpt_ccb_done(struct dpt_softc *, struct dpt_ccb *);
static int	dpt_ccb_map(struct dpt_softc *, struct dpt_ccb *);
static int	dpt_ccb_poll(struct dpt_softc *, struct dpt_ccb *);
static void	dpt_ccb_unmap(struct dpt_softc *, struct dpt_ccb *);
static int	dpt_cmd(struct dpt_softc *, struct dpt_ccb *, int, int);
static void	dpt_ctlrinfo(struct dpt_softc *, struct dpt_eata_ctlrinfo *);
static void	dpt_hba_inquire(struct dpt_softc *, struct eata_inquiry_data **);
static void	dpt_minphys(struct buf *);
static int	dpt_passthrough(struct dpt_softc *, struct eata_ucp *,
				struct lwp *);
static void	dpt_scsipi_request(struct scsipi_channel *,
				   scsipi_adapter_req_t, void *);
static void	dpt_shutdown(void *);
static void	dpt_sysinfo(struct dpt_softc *, struct dpt_sysinfo *);
static int	dpt_wait(struct dpt_softc *, u_int8_t, u_int8_t, int);

static inline struct dpt_ccb	*dpt_ccb_alloc(struct dpt_softc *);
static inline void	dpt_ccb_free(struct dpt_softc *, struct dpt_ccb *);

static inline struct dpt_ccb *
dpt_ccb_alloc(struct dpt_softc *sc)
{
	struct dpt_ccb *ccb;
	int s;

	s = splbio();
	ccb = SLIST_FIRST(&sc->sc_ccb_free);
	SLIST_REMOVE_HEAD(&sc->sc_ccb_free, ccb_chain);
	splx(s);

	return (ccb);
}

static inline void
dpt_ccb_free(struct dpt_softc *sc, struct dpt_ccb *ccb)
{
	int s;

	ccb->ccb_flg = 0;
	ccb->ccb_savesp = NULL;
	s = splbio();
	SLIST_INSERT_HEAD(&sc->sc_ccb_free, ccb, ccb_chain);
	splx(s);
}

/*
 * Handle an interrupt from the HBA.
 */
int
dpt_intr(void *cookie)
{
	struct dpt_softc *sc;
	struct dpt_ccb *ccb;
	struct eata_sp *sp;
	int forus;

	sc = cookie;
	sp = sc->sc_stp;
	forus = 0;

	for (;;) {
		/*
		 * HBA might have interrupted while we were dealing with the
		 * last completed command, since we ACK before we deal; keep
		 * polling.
		 */
		if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_INTR) == 0)
			break;
		forus = 1;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, sc->sc_stpoff,
		    sizeof(struct eata_sp), BUS_DMASYNC_POSTREAD);

		/* Might have looped before HBA can reset HBA_AUX_INTR. */
		if (sp->sp_ccbid == -1) {
			DELAY(50);

			if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_INTR) == 0)
				return (0);

			printf("%s: no status\n", device_xname(sc->sc_dev));

			/* Re-sync DMA map */
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
			    sc->sc_stpoff, sizeof(struct eata_sp),
			    BUS_DMASYNC_POSTREAD);
		}

		/* Make sure CCB ID from status packet is realistic. */
		if ((u_int)sp->sp_ccbid >= sc->sc_nccbs) {
			printf("%s: bogus status (returned CCB id %d)\n",
			    device_xname(sc->sc_dev), sp->sp_ccbid);

			/* Ack the interrupt */
			sp->sp_ccbid = -1;
			(void)dpt_inb(sc, HA_STATUS);
			continue;
		}

		/* Sync up DMA map and cache cmd status. */
		ccb = sc->sc_ccbs + sp->sp_ccbid;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, CCB_OFF(sc, ccb),
		    sizeof(struct dpt_ccb), BUS_DMASYNC_POSTWRITE);

		ccb->ccb_hba_status = sp->sp_hba_status & 0x7f;
		ccb->ccb_scsi_status = sp->sp_scsi_status;
		if (ccb->ccb_savesp != NULL)
			memcpy(ccb->ccb_savesp, sp, sizeof(*sp));

		/*
		 * Ack the interrupt and process the CCB.  If this
		 * is a private CCB it's up to dpt_ccb_poll() to
		 * notice.
		 */
		sp->sp_ccbid = -1;
		ccb->ccb_flg |= CCB_INTR;
		(void)dpt_inb(sc, HA_STATUS);
		if ((ccb->ccb_flg & CCB_PRIVATE) == 0)
			dpt_ccb_done(sc, ccb);
		else if ((ccb->ccb_flg & CCB_WAIT) != 0)
			wakeup(ccb);
	}

	return (forus);
}

/*
 * Initialize and attach the HBA.  This is the entry point from bus
 * specific probe-and-attach code.
 */
void
dpt_init(struct dpt_softc *sc, const char *intrstr)
{
	struct scsipi_adapter *adapt;
	struct scsipi_channel *chan;
	struct eata_inquiry_data *ei;
	int i, j, rv, rseg, maxchannel, maxtarget, mapsize;
	bus_dma_segment_t seg;
	struct eata_cfg *ec;
	struct dpt_ccb *ccb;
	char model[__arraycount(ei->ei_model) + __arraycount(ei->ei_suffix) + 1];
	char vendor[__arraycount(ei->ei_vendor) + 1];

	ec = &sc->sc_ec;
	snprintf(dpt_sig.dsDescription, sizeof(dpt_sig.dsDescription),
	    "NetBSD %s DPT driver", osrelease);
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	/*
	 * Allocate the CCB/status packet/scratch DMA map and load.
	 */
	sc->sc_nccbs =
	    min(be16toh(*(int16_t *)ec->ec_queuedepth), DPT_MAX_CCBS);
	sc->sc_stpoff = sc->sc_nccbs * sizeof(struct dpt_ccb);
	sc->sc_scroff = sc->sc_stpoff + sizeof(struct eata_sp);
	mapsize = sc->sc_nccbs * sizeof(struct dpt_ccb) +
	    DPT_SCRATCH_SIZE + sizeof(struct eata_sp);

	if ((rv = bus_dmamem_alloc(sc->sc_dmat, mapsize,
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate CCBs, rv = %d\n", rv);
		return;
	}

	if ((rv = bus_dmamem_map(sc->sc_dmat, &seg, rseg, mapsize,
	    (void **)&sc->sc_ccbs, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map CCBs, rv = %d\n",
		    rv);
		return;
	}

	if ((rv = bus_dmamap_create(sc->sc_dmat, mapsize,
	    mapsize, 1, 0, BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create CCB DMA map, rv = %d\n", rv);
		return;
	}

	if ((rv = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap,
	    sc->sc_ccbs, mapsize, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load CCB DMA map, rv = %d\n", rv);
		return;
	}

	sc->sc_stp = (struct eata_sp *)((char *)sc->sc_ccbs + sc->sc_stpoff);
	sc->sc_stppa = sc->sc_dmamap->dm_segs[0].ds_addr + sc->sc_stpoff;
	sc->sc_scr = (char *)sc->sc_ccbs + sc->sc_scroff;
	sc->sc_scrpa = sc->sc_dmamap->dm_segs[0].ds_addr + sc->sc_scroff;
	sc->sc_stp->sp_ccbid = -1;

	/*
	 * Create the CCBs.
	 */
	SLIST_INIT(&sc->sc_ccb_free);
	memset(sc->sc_ccbs, 0, sizeof(struct dpt_ccb) * sc->sc_nccbs);

	for (i = 0, ccb = sc->sc_ccbs; i < sc->sc_nccbs; i++, ccb++) {
		rv = bus_dmamap_create(sc->sc_dmat, DPT_MAX_XFER,
		    DPT_SG_SIZE, DPT_MAX_XFER, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap_xfer);
		if (rv) {
			aprint_error_dev(sc->sc_dev, "can't create ccb dmamap (%d)\n", rv);
			break;
		}

		ccb->ccb_id = i;
		ccb->ccb_ccbpa = sc->sc_dmamap->dm_segs[0].ds_addr +
		    CCB_OFF(sc, ccb);
		SLIST_INSERT_HEAD(&sc->sc_ccb_free, ccb, ccb_chain);
	}

	if (i == 0) {
		aprint_error_dev(sc->sc_dev, "unable to create CCBs\n");
		return;
	} else if (i != sc->sc_nccbs) {
		aprint_error_dev(sc->sc_dev, "%d/%d CCBs created!\n",
		    i, sc->sc_nccbs);
		sc->sc_nccbs = i;
	}

	/* Set shutdownhook before we start any device activity. */
	if (dpt_sdh == NULL)
		dpt_sdh = shutdownhook_establish(dpt_shutdown, NULL);

	/* Get the inquiry data from the HBA. */
	dpt_hba_inquire(sc, &ei);

	/*
	 * dpt0 at pci0 dev 12 function 0: DPT SmartRAID III (PM3224A/9X-R)
	 * dpt0: interrupting at irq 10
	 * dpt0: 64 queued commands, 1 channel(s), adapter on ID(s) 7
	 */
	for (i = 0; i < __arraycount(ei->ei_vendor) && ei->ei_vendor[i] != ' ';
	    i++)
		vendor[i] = ei->ei_vendor[i];
	vendor[i] = '\0';

	for (i = 0; i < __arraycount(ei->ei_model) && ei->ei_model[i] != ' ';
	    i++)
		model[i] = ei->ei_model[i];
	for (j = 0; j < __arraycount(ei->ei_suffix) && ei->ei_suffix[j] != ' ';
	    i++, j++)
		model[i] = ei->ei_suffix[j];
	model[i] = '\0';

	/* Find the marketing name for the board. */
	for (i = 0; dpt_cname[i] != NULL; i += 2)
		if (memcmp(ei->ei_model + 2, dpt_cname[i], 4) == 0)
			break;

	aprint_normal("%s %s (%s)\n", vendor, dpt_cname[i + 1], model);

	if (intrstr != NULL)
		aprint_normal_dev(sc->sc_dev, "interrupting at %s\n",
		    intrstr);

	maxchannel = (ec->ec_feat3 & EC_F3_MAX_CHANNEL_MASK) >>
	    EC_F3_MAX_CHANNEL_SHIFT;
	maxtarget = (ec->ec_feat3 & EC_F3_MAX_TARGET_MASK) >>
	    EC_F3_MAX_TARGET_SHIFT;

	aprint_normal_dev(sc->sc_dev, "%d queued commands, %d channel(s), adapter on ID(s)",
	    sc->sc_nccbs, maxchannel + 1);

	for (i = 0; i <= maxchannel; i++) {
		sc->sc_hbaid[i] = ec->ec_hba[3 - i];
		aprint_normal(" %d", sc->sc_hbaid[i]);
	}
	aprint_normal("\n");

	/*
	 * Reset the SCSI controller chip(s) and bus.  XXX Do we need to do
	 * this for each bus?
	 */
	if (dpt_cmd(sc, NULL, CP_IMMEDIATE, CPI_BUS_RESET))
		panic("%s: dpt_cmd failed", device_xname(sc->sc_dev));

	/* Fill in the scsipi_adapter. */
	adapt = &sc->sc_adapt;
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = maxchannel + 1;
	adapt->adapt_openings = sc->sc_nccbs - 1;
	adapt->adapt_max_periph = sc->sc_nccbs - 1;
	adapt->adapt_request = dpt_scsipi_request;
	adapt->adapt_minphys = dpt_minphys;

	for (i = 0; i <= maxchannel; i++) {
		/* Fill in the scsipi_channel. */
		chan = &sc->sc_chans[i];
		memset(chan, 0, sizeof(*chan));
		chan->chan_adapter = adapt;
		chan->chan_bustype = &scsi_bustype;
		chan->chan_channel = i;
		chan->chan_ntargets = maxtarget + 1;
		chan->chan_nluns = ec->ec_maxlun + 1;
		chan->chan_id = sc->sc_hbaid[i];
		config_found(sc->sc_dev, chan, scsiprint);
	}
}

/*
 * Read the EATA configuration from the HBA and perform some sanity checks.
 */
int
dpt_readcfg(struct dpt_softc *sc)
{
	struct eata_cfg *ec;
	int i, j, stat;
	u_int16_t *p;

	ec = &sc->sc_ec;

	/* Older firmware may puke if we talk to it too soon after reset. */
	dpt_outb(sc, HA_COMMAND, CP_RESET);
	DELAY(750000);

	for (i = 1000; i; i--) {
		if ((dpt_inb(sc, HA_STATUS) & HA_ST_READY) != 0)
			break;
		DELAY(2000);
	}

	if (i == 0) {
		printf("%s: HBA not ready after reset (hba status:%02x)\n",
		    device_xname(sc->sc_dev), dpt_inb(sc, HA_STATUS));
		return (-1);
	}

	while((((stat = dpt_inb(sc, HA_STATUS))
	    != (HA_ST_READY|HA_ST_SEEK_COMPLETE))
	    && (stat != (HA_ST_READY|HA_ST_SEEK_COMPLETE|HA_ST_ERROR))
	    && (stat != (HA_ST_READY|HA_ST_SEEK_COMPLETE|HA_ST_ERROR|HA_ST_DRQ)))
	    || (dpt_wait(sc, HA_ST_BUSY, 0, 2000))) {
		/* RAID drives still spinning up? */
		if(dpt_inb(sc, HA_ERROR) != 'D' ||
		   dpt_inb(sc, HA_ERROR + 1) != 'P' ||
		   dpt_inb(sc, HA_ERROR + 2) != 'T') {
			printf("%s: HBA not ready\n", device_xname(sc->sc_dev));
			return (-1);
		}
	}

	/*
	 * Issue the read-config command and wait for the data to appear.
	 *
	 * Apparently certian firmware revisions won't DMA later on if we
	 * request the config data using PIO, but it makes it a lot easier
	 * as no DMA setup is required.
	 */
	dpt_outb(sc, HA_COMMAND, CP_PIO_GETCFG);
	memset(ec, 0, sizeof(*ec));
	i = ((int)(uintptr_t)&((struct eata_cfg *)0)->ec_cfglen +
	    sizeof(ec->ec_cfglen)) >> 1;
	p = (u_int16_t *)ec;

	if (dpt_wait(sc, 0xFF, HA_ST_DATA_RDY, 2000)) {
		printf("%s: cfg data didn't appear (hba status:%02x)\n",
		    device_xname(sc->sc_dev), dpt_inb(sc, HA_STATUS));
		return (-1);
	}

	/* Begin reading. */
	while (i--)
		*p++ = bus_space_read_stream_2(sc->sc_iot, sc->sc_ioh, HA_DATA);

	if ((i = ec->ec_cfglen) > (sizeof(struct eata_cfg)
	    - (int)(uintptr_t)(&(((struct eata_cfg *)0L)->ec_cfglen))
	    - sizeof(ec->ec_cfglen)))
		i = sizeof(struct eata_cfg)
		  - (int)(uintptr_t)(&(((struct eata_cfg *)0L)->ec_cfglen))
		  - sizeof(ec->ec_cfglen);

	j = i + (int)(uintptr_t)(&(((struct eata_cfg *)0L)->ec_cfglen)) +
	    sizeof(ec->ec_cfglen);
	i >>= 1;

	while (i--)
		*p++ = bus_space_read_stream_2(sc->sc_iot, sc->sc_ioh, HA_DATA);

	/* Flush until we have read 512 bytes. */
	i = (512 - j + 1) >> 1;
	while (i--)
		(void)bus_space_read_stream_2(sc->sc_iot, sc->sc_ioh, HA_DATA);

	/* Defaults for older firmware... */
	if (p <= (u_short *)&ec->ec_hba[DPT_MAX_CHANNELS - 1])
		ec->ec_hba[DPT_MAX_CHANNELS - 1] = 7;

	if ((dpt_inb(sc, HA_STATUS) & HA_ST_ERROR) != 0) {
		aprint_error_dev(sc->sc_dev, "HBA error\n");
		return (-1);
	}

	if (memcmp(ec->ec_eatasig, "EATA", 4) != 0) {
		aprint_error_dev(sc->sc_dev, "EATA signature mismatch\n");
		return (-1);
	}

	if ((ec->ec_feat0 & EC_F0_HBA_VALID) == 0) {
		aprint_error_dev(sc->sc_dev, "ec_hba field invalid\n");
		return (-1);
	}

	if ((ec->ec_feat0 & EC_F0_DMA_SUPPORTED) == 0) {
		aprint_error_dev(sc->sc_dev, "DMA not supported\n");
		return (-1);
	}

	return (0);
}

/*
 * Our `shutdownhook' to cleanly shut down the HBA.  The HBA must flush all
 * data from its cache and mark array groups as clean.
 *
 * XXX This doesn't always work (i.e., the HBA may still be flushing after
 * we tell root that it's safe to power off).
 */
static void
dpt_shutdown(void *cookie)
{
	extern struct cfdriver dpt_cd;
	struct dpt_softc *sc;
	int i;

	printf("shutting down dpt devices...");

	for (i = 0; i < dpt_cd.cd_ndevs; i++) {
		if ((sc = device_lookup_private(&dpt_cd, i)) == NULL)
			continue;
		dpt_cmd(sc, NULL, CP_IMMEDIATE, CPI_POWEROFF_WARN);
	}

	delay(10000*1000);
	printf(" done\n");
}

/*
 * Send an EATA command to the HBA.
 */
static int
dpt_cmd(struct dpt_softc *sc, struct dpt_ccb *ccb, int eatacmd, int icmd)
{
	u_int32_t pa;
	int i, s;

	s = splbio();

	for (i = 20000; i != 0; i--) {
		if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_BUSY) == 0)
			break;
		DELAY(50);
	}
	if (i == 0) {
		splx(s);
		return (-1);
	}

	pa = (ccb != NULL ? ccb->ccb_ccbpa : 0);
	dpt_outb(sc, HA_DMA_BASE + 0, (pa      ) & 0xff);
	dpt_outb(sc, HA_DMA_BASE + 1, (pa >>  8) & 0xff);
	dpt_outb(sc, HA_DMA_BASE + 2, (pa >> 16) & 0xff);
	dpt_outb(sc, HA_DMA_BASE + 3, (pa >> 24) & 0xff);

	if (eatacmd == CP_IMMEDIATE)
		dpt_outb(sc, HA_ICMD, icmd);

	dpt_outb(sc, HA_COMMAND, eatacmd);

	splx(s);
	return (0);
}

/*
 * Wait for the HBA status register to reach a specific state.
 */
static int
dpt_wait(struct dpt_softc *sc, u_int8_t mask, u_int8_t state, int ms)
{

	for (ms *= 10; ms != 0; ms--) {
		if ((dpt_inb(sc, HA_STATUS) & mask) == state)
			return (0);
		DELAY(100);
	}

	return (-1);
}

/*
 * Spin waiting for a command to finish.  The timeout value from the CCB is
 * used.  The CCB must be marked with CCB_PRIVATE, otherwise it'll will get
 * recycled before we get a look at it.
 */
static int
dpt_ccb_poll(struct dpt_softc *sc, struct dpt_ccb *ccb)
{
	int i, s;

#ifdef DEBUG
	if ((ccb->ccb_flg & CCB_PRIVATE) == 0)
		panic("dpt_ccb_poll: called for non-CCB_PRIVATE request");
#endif

	s = splbio();

	if ((ccb->ccb_flg & CCB_INTR) != 0) {
		splx(s);
		return (0);
	}

	for (i = ccb->ccb_timeout * 20; i != 0; i--) {
		if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_INTR) != 0)
			dpt_intr(sc);
		if ((ccb->ccb_flg & CCB_INTR) != 0)
			break;
		DELAY(50);
	}

	splx(s);
	return (i == 0);
}

/*
 * We have a command which has been processed by the HBA, so now we look to
 * see how the operation went.  CCBs marked CCB_PRIVATE are not passed here
 * by dpt_intr().
 */
static void
dpt_ccb_done(struct dpt_softc *sc, struct dpt_ccb *ccb)
{
	struct scsipi_xfer *xs;

	xs = ccb->ccb_xs;

	SC_DEBUG(xs->xs_periph, SCSIPI_DB2, ("dpt_ccb_done\n"));

	/*
	 * If we were a data transfer, unload the map that described the
	 * data buffer.
	 */
	if (xs->datalen != 0)
		dpt_ccb_unmap(sc, ccb);

	if (xs->error == XS_NOERROR) {
		if (ccb->ccb_hba_status != SP_HBA_NO_ERROR) {
			switch (ccb->ccb_hba_status) {
			case SP_HBA_ERROR_SEL_TO:
				xs->error = XS_SELTIMEOUT;
				break;
			case SP_HBA_ERROR_RESET:
				xs->error = XS_RESET;
				break;
			default:
				printf("%s: HBA status %x\n",
				    device_xname(sc->sc_dev), ccb->ccb_hba_status);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else if (ccb->ccb_scsi_status != SCSI_OK) {
			switch (ccb->ccb_scsi_status) {
			case SCSI_CHECK:
				memcpy(&xs->sense.scsi_sense, &ccb->ccb_sense,
				    sizeof(xs->sense.scsi_sense));
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
			case SCSI_QUEUE_FULL:
				xs->error = XS_BUSY;
				break;
			default:
				scsipi_printaddr(xs->xs_periph);
				printf("SCSI status %x\n",
				    ccb->ccb_scsi_status);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else
			xs->resid = 0;

		xs->status = ccb->ccb_scsi_status;
	}

	/* Free up the CCB and mark the command as done. */
	dpt_ccb_free(sc, ccb);
	scsipi_done(xs);
}

/*
 * Specified CCB has timed out, abort it.
 */
static void
dpt_ccb_abort(struct dpt_softc *sc, struct dpt_ccb *ccb)
{
	struct scsipi_periph *periph;
	struct scsipi_xfer *xs;
	int s;

	xs = ccb->ccb_xs;
	periph = xs->xs_periph;

	scsipi_printaddr(periph);
	printf("timed out (status:%02x aux status:%02x)",
	    dpt_inb(sc, HA_STATUS), dpt_inb(sc, HA_AUX_STATUS));

	s = splbio();

	if ((ccb->ccb_flg & CCB_ABORT) != 0) {
		/* Abort timed out, reset the HBA */
		printf(" AGAIN, resetting HBA\n");
		dpt_outb(sc, HA_COMMAND, CP_RESET);
		DELAY(750000);
	} else {
		/* Abort the operation that has timed out */
		printf("\n");
		xs->error = XS_TIMEOUT;
		ccb->ccb_timeout = DPT_ABORT_TIMEOUT;
		ccb->ccb_flg |= CCB_ABORT;
		/* Start the abort */
		if (dpt_cmd(sc, ccb, CP_IMMEDIATE, CPI_SPEC_ABORT))
			aprint_error_dev(sc->sc_dev, "dpt_cmd failed\n");
	}

	splx(s);
}

/*
 * Map a data transfer.
 */
static int
dpt_ccb_map(struct dpt_softc *sc, struct dpt_ccb *ccb)
{
	struct scsipi_xfer *xs;
	bus_dmamap_t xfer;
	bus_dma_segment_t *ds;
	struct eata_sg *sg;
	struct eata_cp *cp;
	int rv, i;

	xs = ccb->ccb_xs;
	xfer = ccb->ccb_dmamap_xfer;
	cp = &ccb->ccb_eata_cp;

	rv = bus_dmamap_load(sc->sc_dmat, xfer, xs->data, xs->datalen, NULL,
	    ((xs->xs_control & XS_CTL_NOSLEEP) != 0 ?
	    BUS_DMA_NOWAIT : BUS_DMA_WAITOK) | BUS_DMA_STREAMING |
	    ((xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMA_READ : BUS_DMA_WRITE));

	switch (rv) {
	case 0:
		break;
	case ENOMEM:
	case EAGAIN:
		xs->error = XS_RESOURCE_SHORTAGE;
		break;
	default:
		xs->error = XS_DRIVER_STUFFUP;
		printf("%s: error %d loading map\n", device_xname(sc->sc_dev), rv);
		break;
	}

	if (xs->error != XS_NOERROR) {
		dpt_ccb_free(sc, ccb);
		scsipi_done(xs);
		return (-1);
	}

	bus_dmamap_sync(sc->sc_dmat, xfer, 0, xfer->dm_mapsize,
	    (xs->xs_control & XS_CTL_DATA_IN) != 0 ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	/* Don't bother using scatter/gather for just 1 seg */
	if (xfer->dm_nsegs == 1) {
		cp->cp_dataaddr = htobe32(xfer->dm_segs[0].ds_addr);
		cp->cp_datalen = htobe32(xfer->dm_segs[0].ds_len);
	} else {
		/*
		 * Load the hardware scatter/gather map with
		 * the contents of the DMA map.
		 */
		sg = ccb->ccb_sg;
		ds = xfer->dm_segs;
		for (i = 0; i < xfer->dm_nsegs; i++, sg++, ds++) {
 			sg->sg_addr = htobe32(ds->ds_addr);
 			sg->sg_len =  htobe32(ds->ds_len);
 		}
	 	cp->cp_dataaddr = htobe32(CCB_OFF(sc, ccb) +
		    sc->sc_dmamap->dm_segs[0].ds_addr +
		    offsetof(struct dpt_ccb, ccb_sg));
		cp->cp_datalen = htobe32(i * sizeof(struct eata_sg));
		cp->cp_ctl0 |= CP_C0_SCATTER;
	}

	return (0);
}

/*
 * Unmap a transfer.
 */
static void
dpt_ccb_unmap(struct dpt_softc *sc, struct dpt_ccb *ccb)
{

	bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap_xfer, 0,
	    ccb->ccb_dmamap_xfer->dm_mapsize,
	    (ccb->ccb_eata_cp.cp_ctl0 & CP_C0_DATA_IN) != 0 ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap_xfer);
}

/*
 * Adjust the size of each I/O before it passes to the SCSI layer.
 */
static void
dpt_minphys(struct buf *bp)
{

	if (bp->b_bcount > DPT_MAX_XFER)
		bp->b_bcount = DPT_MAX_XFER;
	minphys(bp);
}

/*
 * Start a SCSI command.
 */
static void
dpt_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
		   void *arg)
{
	struct dpt_softc *sc;
	struct scsipi_xfer *xs;
	int flags;
	struct scsipi_periph *periph;
	struct dpt_ccb *ccb;
	struct eata_cp *cp;

	sc = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		flags = xs->xs_control;

#ifdef DIAGNOSTIC
		/* Cmds must be no more than 12 bytes for us. */
		if (xs->cmdlen > 12) {
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			break;
		}
#endif
		/*
		 * XXX We can't reset devices just yet.  Apparently some
		 * older firmware revisions don't even support it.
		 */
		if ((flags & XS_CTL_RESET) != 0) {
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			break;
		}

		/*
		 * Get a CCB and fill it.
		 */
		ccb = dpt_ccb_alloc(sc);
		ccb->ccb_xs = xs;
		ccb->ccb_timeout = xs->timeout;

		cp = &ccb->ccb_eata_cp;
		memcpy(&cp->cp_cdb_cmd, xs->cmd, xs->cmdlen);
		cp->cp_ccbid = ccb->ccb_id;
		cp->cp_senselen = sizeof(ccb->ccb_sense);
		cp->cp_stataddr = htobe32(sc->sc_stppa);
		cp->cp_ctl0 = CP_C0_AUTO_SENSE;
		cp->cp_ctl1 = 0;
		cp->cp_ctl2 = 0;
		cp->cp_ctl3 = periph->periph_target << CP_C3_ID_SHIFT;
		cp->cp_ctl3 |= chan->chan_channel << CP_C3_CHANNEL_SHIFT;
		cp->cp_ctl4 = periph->periph_lun << CP_C4_LUN_SHIFT;
		cp->cp_ctl4 |= CP_C4_DIS_PRI | CP_C4_IDENTIFY;

		if ((flags & XS_CTL_DATA_IN) != 0)
			cp->cp_ctl0 |= CP_C0_DATA_IN;
		if ((flags & XS_CTL_DATA_OUT) != 0)
			cp->cp_ctl0 |= CP_C0_DATA_OUT;
		if (sc->sc_hbaid[chan->chan_channel] == periph->periph_target)
			cp->cp_ctl0 |= CP_C0_INTERPRET;

		/* Synchronous xfers musn't write-back through the cache. */
		if (xs->bp != NULL)
			if ((xs->bp->b_flags & (B_ASYNC | B_READ)) == 0)
				cp->cp_ctl2 |= CP_C2_NO_CACHE;

		cp->cp_senseaddr =
		    htobe32(sc->sc_dmamap->dm_segs[0].ds_addr +
		    CCB_OFF(sc, ccb) + offsetof(struct dpt_ccb, ccb_sense));

		if (xs->datalen != 0) {
			if (dpt_ccb_map(sc, ccb))
				break;
		} else {
			cp->cp_dataaddr = 0;
			cp->cp_datalen = 0;
		}

		/* Sync up CCB and status packet. */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    CCB_OFF(sc, ccb), sizeof(struct dpt_ccb),
		    BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, sc->sc_stpoff,
		    sizeof(struct eata_sp), BUS_DMASYNC_PREREAD);

		/*
		 * Start the command.
		 */
		if ((xs->xs_control & XS_CTL_POLL) != 0)
			ccb->ccb_flg |= CCB_PRIVATE;

		if (dpt_cmd(sc, ccb, CP_DMA_CMD, 0)) {
			aprint_error_dev(sc->sc_dev, "dpt_cmd failed\n");
			xs->error = XS_DRIVER_STUFFUP;
			if (xs->datalen != 0)
				dpt_ccb_unmap(sc, ccb);
			dpt_ccb_free(sc, ccb);
			break;
		}

		if ((xs->xs_control & XS_CTL_POLL) == 0)
			break;

		if (dpt_ccb_poll(sc, ccb)) {
			dpt_ccb_abort(sc, ccb);
			/* Wait for abort to complete... */
			if (dpt_ccb_poll(sc, ccb))
				dpt_ccb_abort(sc, ccb);
		}

		dpt_ccb_done(sc, ccb);
		break;

	case ADAPTER_REQ_GROW_RESOURCES:
		/*
		 * Not supported, since we allocate the maximum number of
		 * CCBs up front.
		 */
		break;

	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * This will be handled by the HBA itself, and we can't
		 * modify that (ditto for tagged queueing).
		 */
		break;
	}
}

/*
 * Get inquiry data from the adapter.
 */
static void
dpt_hba_inquire(struct dpt_softc *sc, struct eata_inquiry_data **ei)
{
	struct dpt_ccb *ccb;
	struct eata_cp *cp;

	*ei = (struct eata_inquiry_data *)sc->sc_scr;

	/* Get a CCB and mark as private */
	ccb = dpt_ccb_alloc(sc);
	ccb->ccb_flg |= CCB_PRIVATE;
	ccb->ccb_timeout = 200;

	/* Put all the arguments into the CCB. */
	cp = &ccb->ccb_eata_cp;
	cp->cp_ccbid = ccb->ccb_id;
	cp->cp_senselen = sizeof(ccb->ccb_sense);
	cp->cp_senseaddr = 0;
	cp->cp_stataddr = htobe32(sc->sc_stppa);
	cp->cp_dataaddr = htobe32(sc->sc_scrpa);
	cp->cp_datalen = htobe32(sizeof(struct eata_inquiry_data));
	cp->cp_ctl0 = CP_C0_DATA_IN | CP_C0_INTERPRET;
	cp->cp_ctl1 = 0;
	cp->cp_ctl2 = 0;
	cp->cp_ctl3 = sc->sc_hbaid[0] << CP_C3_ID_SHIFT;
	cp->cp_ctl4 = CP_C4_DIS_PRI | CP_C4_IDENTIFY;

	/* Put together the SCSI inquiry command. */
	memset(&cp->cp_cdb_cmd, 0, 12);
	cp->cp_cdb_cmd = INQUIRY;
	cp->cp_cdb_len = sizeof(struct eata_inquiry_data);

	/* Sync up CCB, status packet and scratch area. */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, CCB_OFF(sc, ccb),
	    sizeof(struct dpt_ccb), BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, sc->sc_stpoff,
	    sizeof(struct eata_sp), BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, sc->sc_scroff,
	    sizeof(struct eata_inquiry_data), BUS_DMASYNC_PREREAD);

	/* Start the command and poll on completion. */
	if (dpt_cmd(sc, ccb, CP_DMA_CMD, 0))
		panic("%s: dpt_cmd failed", device_xname(sc->sc_dev));

	if (dpt_ccb_poll(sc, ccb))
		panic("%s: inquiry timed out", device_xname(sc->sc_dev));

	if (ccb->ccb_hba_status != SP_HBA_NO_ERROR ||
	    ccb->ccb_scsi_status != SCSI_OK)
		panic("%s: inquiry failed (hba:%02x scsi:%02x)",
		    device_xname(sc->sc_dev), ccb->ccb_hba_status,
		    ccb->ccb_scsi_status);

	/* Sync up the DMA map and free CCB, returning. */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, sc->sc_scroff,
	    sizeof(struct eata_inquiry_data), BUS_DMASYNC_POSTREAD);
	dpt_ccb_free(sc, ccb);
}

int
dptopen(dev_t dev, int flag, int mode, struct lwp *l)
{

	if (device_lookup(&dpt_cd, minor(dev)) == NULL)
		return (ENXIO);

	return (0);
}

int
dptioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct dpt_softc *sc;
	int rv;

	sc = device_lookup_private(&dpt_cd, minor(dev));

	switch (cmd & 0xffff) {
	case DPT_SIGNATURE:
		memcpy(data, &dpt_sig, min(IOCPARM_LEN(cmd), sizeof(dpt_sig)));
		break;

	case DPT_CTRLINFO:
		dpt_ctlrinfo(sc, (struct dpt_eata_ctlrinfo *)data);
		break;

	case DPT_SYSINFO:
		dpt_sysinfo(sc, (struct dpt_sysinfo *)data);
		break;

	case DPT_BLINKLED:
		/*
		 * XXX Don't know how to get this from EATA boards.  I think
		 * it involves waiting for a "DPT" sequence from HA_ERROR
		 * and then reading one of the HA_ICMD registers.
		 */
		*(int *)data = 0;
		break;

	case DPT_EATAUSRCMD:
		rv = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL, data);
		if (rv)
			return (rv);

		if (IOCPARM_LEN(cmd) < sizeof(struct eata_ucp)) {
			DPRINTF(("%s: ucp %lu vs %lu bytes\n",
			    device_xname(sc->sc_dev), IOCPARM_LEN(cmd),
			    (unsigned long int)sizeof(struct eata_ucp)));
			return (EINVAL);
		}

		mutex_enter(&sc->sc_lock);
		rv = dpt_passthrough(sc, (struct eata_ucp *)data, l);
		mutex_exit(&sc->sc_lock);

		return (rv);

	default:
		DPRINTF(("%s: unknown ioctl %lx\n", device_xname(sc->sc_dev), cmd));
		return (ENOTTY);
	}

	return (0);
}

void
dpt_ctlrinfo(struct dpt_softc *sc, struct dpt_eata_ctlrinfo *info)
{

	memset(info, 0, sizeof(*info));
	info->id = sc->sc_hbaid[0];
	info->vect = sc->sc_isairq;
	info->base = sc->sc_isaport;
	info->qdepth = sc->sc_nccbs;
	info->sgsize = DPT_SG_SIZE * sizeof(struct eata_sg);
	info->heads = 16;
	info->sectors = 63;
	info->do_drive32 = 1;
	info->primary = 1;
	info->cpLength = sizeof(struct eata_cp);
	info->spLength = sizeof(struct eata_sp);
	info->drqNum = sc->sc_isadrq;
}

void
dpt_sysinfo(struct dpt_softc *sc, struct dpt_sysinfo *info)
{
#ifdef i386
	int i, j;
#endif

	memset(info, 0, sizeof(*info));

#ifdef i386
	outb (0x70, 0x12);
	i = inb(0x71);
	j = i >> 4;
	if (i == 0x0f) {
		outb (0x70, 0x19);
		j = inb (0x71);
	}
	info->drive0CMOS = j;

	j = i & 0x0f;
	if (i == 0x0f) {
		outb (0x70, 0x1a);
		j = inb (0x71);
	}
	info->drive1CMOS = j;
	info->processorFamily = dpt_sig.dsProcessorFamily;

	/*
	 * Get the conventional memory size from CMOS.
	 */
	outb(0x70, 0x16);
	j = inb(0x71);
	j <<= 8;
	outb(0x70, 0x15);
	j |= inb(0x71);
	info->conventionalMemSize = j;

	/*
	 * Get the extended memory size from CMOS.
	 */
	outb(0x70, 0x31);
	j = inb(0x71);
	j <<= 8;
	outb(0x70, 0x30);
	j |= inb(0x71);
	info->extendedMemSize = j;

	switch (cpu_class) {
	case CPUCLASS_386:
		info->processorType = PROC_386;
		break;
	case CPUCLASS_486:
		info->processorType = PROC_486;
		break;
	case CPUCLASS_586:
		info->processorType = PROC_PENTIUM;
		break;
	case CPUCLASS_686:
	default:
		info->processorType = PROC_SEXIUM;
		break;
	}

	info->flags = SI_CMOS_Valid | SI_BusTypeValid |
	    SI_MemorySizeValid | SI_NO_SmartROM;
#else
	info->flags = SI_BusTypeValid | SI_NO_SmartROM;
#endif

	info->busType = sc->sc_bustype;
}

int
dpt_passthrough(struct dpt_softc *sc, struct eata_ucp *ucp, struct lwp *l)
{
	struct dpt_ccb *ccb;
	struct eata_sp sp;
	struct eata_cp *cp;
	struct eata_sg *sg;
	bus_dmamap_t xfer = 0; /* XXX: gcc */
	bus_dma_segment_t *ds;
	int datain = 0, s, rv = 0, i, uslen; /* XXX: gcc */

	/*
	 * Get a CCB and fill.
	 */
	ccb = dpt_ccb_alloc(sc);
	ccb->ccb_flg |= CCB_PRIVATE | CCB_WAIT;
	ccb->ccb_timeout = 0;
	ccb->ccb_savesp = &sp;

	cp = &ccb->ccb_eata_cp;
	memcpy(cp, ucp->ucp_cp, sizeof(ucp->ucp_cp));
	uslen = cp->cp_senselen;
	cp->cp_ccbid = ccb->ccb_id;
	cp->cp_senselen = sizeof(ccb->ccb_sense);
	cp->cp_senseaddr = htobe32(sc->sc_dmamap->dm_segs[0].ds_addr +
	    CCB_OFF(sc, ccb) + offsetof(struct dpt_ccb, ccb_sense));
	cp->cp_stataddr = htobe32(sc->sc_stppa);

	/*
	 * Map data transfers.
	 */
	if (ucp->ucp_dataaddr && ucp->ucp_datalen) {
		xfer = ccb->ccb_dmamap_xfer;
		datain = ((cp->cp_ctl0 & CP_C0_DATA_IN) != 0);

		if (ucp->ucp_datalen > DPT_MAX_XFER) {
			DPRINTF(("%s: xfer too big\n", device_xname(sc->sc_dev)));
			dpt_ccb_free(sc, ccb);
			return (EFBIG);
		}
		rv = bus_dmamap_load(sc->sc_dmat, xfer,
		    ucp->ucp_dataaddr, ucp->ucp_datalen, l->l_proc,
		    BUS_DMA_WAITOK | BUS_DMA_STREAMING |
		    (datain ? BUS_DMA_READ : BUS_DMA_WRITE));
		if (rv != 0) {
			DPRINTF(("%s: map failed; %d\n", device_xname(sc->sc_dev),
			    rv));
			dpt_ccb_free(sc, ccb);
			return (rv);
		}

		bus_dmamap_sync(sc->sc_dmat, xfer, 0, xfer->dm_mapsize,
		    (datain ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE));

		sg = ccb->ccb_sg;
		ds = xfer->dm_segs;
		for (i = 0; i < xfer->dm_nsegs; i++, sg++, ds++) {
	 		sg->sg_addr = htobe32(ds->ds_addr);
	 		sg->sg_len = htobe32(ds->ds_len);
 		}
		cp->cp_dataaddr = htobe32(CCB_OFF(sc, ccb) +
		    sc->sc_dmamap->dm_segs[0].ds_addr +
		    offsetof(struct dpt_ccb, ccb_sg));
		cp->cp_datalen = htobe32(i * sizeof(struct eata_sg));
		cp->cp_ctl0 |= CP_C0_SCATTER;
	} else {
		cp->cp_dataaddr = 0;
		cp->cp_datalen = 0;
	}

	/*
	 * Start the command and sleep on completion.
	 */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, CCB_OFF(sc, ccb),
	    sizeof(struct dpt_ccb), BUS_DMASYNC_PREWRITE);
	s = splbio();
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, sc->sc_stpoff,
	    sizeof(struct eata_sp), BUS_DMASYNC_PREREAD);
	if (dpt_cmd(sc, ccb, CP_DMA_CMD, 0))
		panic("%s: dpt_cmd failed", device_xname(sc->sc_dev));
	tsleep(ccb, PWAIT, "dptucmd", 0);
	splx(s);

	/*
	 * Sync up the DMA map and copy out results.
	 */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, CCB_OFF(sc, ccb),
	    sizeof(struct dpt_ccb), BUS_DMASYNC_POSTWRITE);

	if (cp->cp_datalen != 0) {
		bus_dmamap_sync(sc->sc_dmat, xfer, 0, xfer->dm_mapsize,
		    (datain ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE));
		bus_dmamap_unload(sc->sc_dmat, xfer);
	}

	if (ucp->ucp_stataddr != NULL) {
		rv = copyout(&sp, ucp->ucp_stataddr, sizeof(sp));
		if (rv != 0) {
			DPRINTF(("%s: sp copyout() failed\n",
			    device_xname(sc->sc_dev)));
		}
	}
	if (rv == 0 && ucp->ucp_senseaddr != NULL) {
		i = min(uslen, sizeof(ccb->ccb_sense));
		rv = copyout(&ccb->ccb_sense, ucp->ucp_senseaddr, i);
		if (rv != 0) {
			DPRINTF(("%s: sense copyout() failed\n",
			    device_xname(sc->sc_dev)));
		}
	}

	ucp->ucp_hstatus = (u_int8_t)ccb->ccb_hba_status;
	ucp->ucp_tstatus = (u_int8_t)ccb->ccb_scsi_status;
	dpt_ccb_free(sc, ccb);
	return (rv);
}
