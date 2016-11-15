/*	$NetBSD: ld_twe.c,v 1.37 2015/04/13 16:33:25 riastradh Exp $	*/

/*-
 * Copyright (c) 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran; and by Jason R. Thorpe of Wasabi Systems, Inc.
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
 * 3ware "Escalade" RAID controller front-end for ld(4) driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ld_twe.c,v 1.37 2015/04/13 16:33:25 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/endian.h>
#include <sys/dkio.h>
#include <sys/disk.h>
#include <sys/proc.h>

#include <sys/bus.h>

#include <dev/ldvar.h>

#include <dev/pci/twereg.h>
#include <dev/pci/twevar.h>

struct ld_twe_softc {
	struct	ld_softc sc_ld;
	int	sc_hwunit;
};

static void	ld_twe_attach(device_t, device_t, void *);
static int	ld_twe_detach(device_t, int);
static int	ld_twe_dobio(struct ld_twe_softc *, void *, int, int, int,
			     struct buf *);
static int	ld_twe_dump(struct ld_softc *, void *, int, int);
static int	ld_twe_flush(struct ld_softc *, int);
static void	ld_twe_handler(struct twe_ccb *, int);
static int	ld_twe_match(device_t, cfdata_t, void *);
static int	ld_twe_start(struct ld_softc *, struct buf *);

static void	ld_twe_adjqparam(device_t, int);

CFATTACH_DECL_NEW(ld_twe, sizeof(struct ld_twe_softc),
    ld_twe_match, ld_twe_attach, ld_twe_detach, NULL);

static const struct twe_callbacks ld_twe_callbacks = {
	ld_twe_adjqparam,
};

static int
ld_twe_match(device_t parent, cfdata_t match, void *aux)
{

	return (1);
}

static void
ld_twe_attach(device_t parent, device_t self, void *aux)
{
	struct twe_attach_args *twea = aux;
	struct ld_twe_softc *sc = device_private(self);
	struct ld_softc *ld = &sc->sc_ld;
	struct twe_softc *twe = device_private(parent);
	struct twe_drive *td = &twe->sc_units[twea->twea_unit];
	const char *typestr, *stripestr, *statstr;
	char unktype[16], stripebuf[32], unkstat[32];
	int error;
	uint8_t status;

	ld->sc_dv = self;

	twe_register_callbacks(twe, twea->twea_unit, &ld_twe_callbacks);

	sc->sc_hwunit = twea->twea_unit;
	ld->sc_flags = LDF_ENABLED;
	ld->sc_maxxfer = twe_get_maxxfer(twe_get_maxsegs());
	ld->sc_secperunit = td->td_size;
	ld->sc_secsize = TWE_SECTOR_SIZE;
	ld->sc_maxqueuecnt = twe->sc_openings;
	ld->sc_start = ld_twe_start;
	ld->sc_dump = ld_twe_dump;
	ld->sc_flush = ld_twe_flush;

	typestr = twe_describe_code(twe_table_unittype, td->td_type);
	if (typestr == NULL) {
		snprintf(unktype, sizeof(unktype), "<0x%02x>", td->td_type);
		typestr = unktype;
	}
	switch (td->td_type) {
	case TWE_AD_CONFIG_RAID0:
	case TWE_AD_CONFIG_RAID5:
	case TWE_AD_CONFIG_RAID10:
		stripestr = twe_describe_code(twe_table_stripedepth,
		    td->td_stripe);
		if (stripestr == NULL)
			snprintf(stripebuf, sizeof(stripebuf),
			    "<stripe code 0x%02x> ", td->td_stripe);
		else
			snprintf(stripebuf, sizeof(stripebuf), "%s stripe ",
			    stripestr);
		break;
	default:
		stripebuf[0] = '\0';
	}

	error = twe_param_get_1(twe, TWE_PARAM_UNITINFO + twea->twea_unit,
	    TWE_PARAM_UNITINFO_Status, &status);
	status &= TWE_PARAM_UNITSTATUS_MASK;
	if (error) {
		snprintf(unkstat, sizeof(unkstat), "<unknown>");
		statstr = unkstat;
	} else if ((statstr =
		    twe_describe_code(twe_table_unitstate, status)) == NULL) {
		snprintf(unkstat, sizeof(unkstat), "<status code 0x%02x>",
		    status);
		statstr = unkstat;
	}

	aprint_normal(": %s%s, status: %s\n", stripebuf, typestr, statstr);
	ldattach(ld);
}

static int
ld_twe_detach(device_t self, int flags)
{
	struct ld_twe_softc *sc = device_private(self);
	struct ld_softc *ld = &sc->sc_ld;
	int rv;

	if ((rv = ldbegindetach(ld, flags)) != 0)
		return (rv);
	ldenddetach(ld);

	return (0);
}

static int
ld_twe_dobio(struct ld_twe_softc *sc, void *data, int datasize, int blkno,
	     int dowrite, struct buf *bp)
{
	struct twe_ccb *ccb;
	struct twe_cmd *tc;
	struct twe_softc *twe;
	int s, rv, flags;

	twe = device_private(device_parent(sc->sc_ld.sc_dv));

	flags = (dowrite ? TWE_CCB_DATA_OUT : TWE_CCB_DATA_IN);
	if ((ccb = twe_ccb_alloc(twe, flags)) == NULL)
		return (EAGAIN);

	ccb->ccb_data = data;
	ccb->ccb_datasize = datasize;
	tc = ccb->ccb_cmd;

	/* Build the command. */
	tc->tc_size = 3;
	tc->tc_unit = sc->sc_hwunit;
	tc->tc_count = htole16(datasize / TWE_SECTOR_SIZE);
	tc->tc_args.io.lba = htole32(blkno);

	if (dowrite)
		tc->tc_opcode = TWE_OP_WRITE | (tc->tc_size << 5);
	else
		tc->tc_opcode = TWE_OP_READ | (tc->tc_size << 5);

	/* Map the data transfer. */
	if ((rv = twe_ccb_map(twe, ccb)) != 0) {
		twe_ccb_free(twe, ccb);
		return (rv);
	}

	if (bp == NULL) {
		/*
		 * Polled commands must not sit on the software queue.  Wait
		 * up to 2 seconds for the command to complete.
		 */
		s = splbio();
		rv = twe_ccb_poll(twe, ccb, 2000);
		twe_ccb_unmap(twe, ccb);
		twe_ccb_free(twe, ccb);
		splx(s);
	} else {
		ccb->ccb_tx.tx_handler = ld_twe_handler;
		ccb->ccb_tx.tx_context = bp;
		ccb->ccb_tx.tx_dv = sc->sc_ld.sc_dv;
		twe_ccb_enqueue(twe, ccb);
		rv = 0;
	}

	return (rv);
}

static int
ld_twe_start(struct ld_softc *ld, struct buf *bp)
{

	return (ld_twe_dobio((struct ld_twe_softc *)ld, bp->b_data,
	    bp->b_bcount, bp->b_rawblkno, (bp->b_flags & B_READ) == 0, bp));
}

static void
ld_twe_handler(struct twe_ccb *ccb, int error)
{
	struct buf *bp;
	struct twe_context *tx;
	struct ld_twe_softc *sc;
	struct twe_softc *twe;

	tx = &ccb->ccb_tx;
	bp = tx->tx_context;
	sc = device_private(tx->tx_dv);
	twe = device_private(device_parent(sc->sc_ld.sc_dv));

	twe_ccb_unmap(twe, ccb);
	twe_ccb_free(twe, ccb);

	if (error) {
		bp->b_error = error;
		bp->b_resid = bp->b_bcount;
	} else
		bp->b_resid = 0;

	lddone(&sc->sc_ld, bp);
}

static int
ld_twe_dump(struct ld_softc *ld, void *data, int blkno, int blkcnt)
{

	return (ld_twe_dobio((struct ld_twe_softc *)ld, data,
	    blkcnt * ld->sc_secsize, blkno, 1, NULL));
}

static int
ld_twe_flush(struct ld_softc *ld, int flags)
{
	struct ld_twe_softc *sc = (void *) ld;
	struct twe_softc *twe = device_private(device_parent(ld->sc_dv));
	struct twe_ccb *ccb;
	struct twe_cmd *tc;
	int s, rv;

	ccb = twe_ccb_alloc_wait(twe, 0);
	KASSERT(ccb != NULL);

	ccb->ccb_data = NULL;
	ccb->ccb_datasize = 0;

	tc = ccb->ccb_cmd;
	tc->tc_size = 2;
	tc->tc_opcode = TWE_OP_FLUSH;
	tc->tc_unit = sc->sc_hwunit;
	tc->tc_count = 0;

	if (flags & LDFL_POLL) {
		/*
		 * Polled commands must not sit on the software queue.  Wait
		 * up to 2 seconds for the command to complete.
		 */
		s = splbio();
		rv = twe_ccb_poll(twe, ccb, 2000);
		twe_ccb_unmap(twe, ccb);
		twe_ccb_free(twe, ccb);
		splx(s);
	} else {
		ccb->ccb_tx.tx_handler = twe_ccb_wait_handler;
		ccb->ccb_tx.tx_context = NULL;
		ccb->ccb_tx.tx_dv = ld->sc_dv;
		twe_ccb_enqueue(twe, ccb);

		rv = 0;
		s = splbio();
		while ((ccb->ccb_flags & TWE_CCB_COMPLETE) == 0)
			if ((rv = tsleep(ccb, PRIBIO, "tweflush",
			    60 * hz)) != 0)
				break;
		twe_ccb_free(twe, ccb);
		splx(s);
	}

	return (rv);
}

static void
ld_twe_adjqparam(device_t self, int openings)
{
	struct ld_twe_softc *sc = device_private(self);
	struct ld_softc *ld = &sc->sc_ld;

	ldadjqparam(ld, openings);
}
