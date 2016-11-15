/*	$NetBSD: ld_aac.c,v 1.28 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ld_aac.c,v 1.28 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/endian.h>
#include <sys/dkio.h>
#include <sys/disk.h>

#include <sys/bus.h>

#include <dev/ldvar.h>

#include <dev/ic/aacreg.h>
#include <dev/ic/aacvar.h>

struct ld_aac_softc {
	struct	ld_softc sc_ld;
	int	sc_hwunit;
};

static void	ld_aac_attach(device_t, device_t, void *);
static void	ld_aac_intr(struct aac_ccb *);
static int	ld_aac_dobio(struct ld_aac_softc *, void *, int, daddr_t, int,
			     struct buf *);
static int	ld_aac_dump(struct ld_softc *, void *, int, int);
static int	ld_aac_match(device_t, cfdata_t, void *);
static int	ld_aac_start(struct ld_softc *, struct buf *);

CFATTACH_DECL_NEW(ld_aac, sizeof(struct ld_aac_softc),
    ld_aac_match, ld_aac_attach, NULL, NULL);

static int
ld_aac_match(device_t parent, cfdata_t match, void *aux)
{

	return (1);
}

static void
ld_aac_attach(device_t parent, device_t self, void *aux)
{
	struct aac_attach_args *aaca = aux;
	struct ld_aac_softc *sc = device_private(self);
	struct ld_softc *ld = &sc->sc_ld;
	struct aac_softc *aac = device_private(parent);
	struct aac_drive *hdr = &aac->sc_hdr[aaca->aaca_unit];

	ld->sc_dv = self;

	sc->sc_hwunit = aaca->aaca_unit;
	ld->sc_flags = LDF_ENABLED;
	ld->sc_maxxfer = AAC_MAX_XFER(aac);
	ld->sc_secperunit = hdr->hd_size;
	ld->sc_secsize = AAC_SECTOR_SIZE;
	ld->sc_maxqueuecnt =
	    (aac->sc_max_fibs - AAC_NCCBS_RESERVE) / aac->sc_nunits;
	ld->sc_start = ld_aac_start;
	ld->sc_dump = ld_aac_dump;

	aprint_normal(": %s\n",
	    aac_describe_code(aac_container_types, hdr->hd_devtype));
	ldattach(ld);
}

static int
ld_aac_dobio(struct ld_aac_softc *sc, void *data, int datasize, daddr_t blkno,
	     int dowrite, struct buf *bp)
{
	struct aac_blockread_response *brr;
	struct aac_blockwrite_response *bwr;
	struct aac_ccb *ac;
	struct aac_softc *aac;
	struct aac_fib *fib;
	bus_dmamap_t xfer;
	u_int32_t status;
	u_int16_t size;
	int s, rv, i;

	aac = device_private(device_parent(sc->sc_ld.sc_dv));

	/*
	 * Allocate a command control block and map the data transfer.
	 */
	ac = aac_ccb_alloc(aac, (dowrite ? AAC_CCB_DATA_OUT : AAC_CCB_DATA_IN));
	if (ac == NULL)
		return EBUSY;
	ac->ac_data = data;
	ac->ac_datalen = datasize;

	if ((rv = aac_ccb_map(aac, ac)) != 0) {
		aac_ccb_free(aac, ac);
		return (rv);
	}

	/*
	 * Build the command.
	 */
	fib = ac->ac_fib;

        fib->Header.XferState = htole32(AAC_FIBSTATE_HOSTOWNED |
	    AAC_FIBSTATE_INITIALISED | AAC_FIBSTATE_FROMHOST |
	    AAC_FIBSTATE_REXPECTED | AAC_FIBSTATE_NORM |
	    AAC_FIBSTATE_ASYNC | AAC_FIBSTATE_FAST_RESPONSE );

	if (aac->sc_quirks & AAC_QUIRK_RAW_IO) {
		struct aac_raw_io *raw;
		struct aac_sg_entryraw *sge;
		struct aac_sg_tableraw *sgt;

		raw = (struct aac_raw_io *)&fib->data[0];
		fib->Header.Command = htole16(RawIo);
		raw->BlockNumber = htole64(blkno);
		raw->ByteCount = htole32(datasize);
		raw->ContainerId = htole16(sc->sc_hwunit);
		raw->BpTotal = 0;
		raw->BpComplete = 0;
		size = sizeof(struct aac_raw_io);
		sgt = &raw->SgMapRaw;
		raw->Flags = (dowrite ? 0 : 1);

		xfer = ac->ac_dmamap_xfer;
		sgt->SgCount = xfer->dm_nsegs;
		sge = sgt->SgEntryRaw;

		for (i = 0; i < xfer->dm_nsegs; i++, sge++) {
			sge->SgAddress = htole64(xfer->dm_segs[i].ds_addr);
			sge->SgByteCount = htole32(xfer->dm_segs[i].ds_len);
			sge->Next = 0;
			sge->Prev = 0;
			sge->Flags = 0;
		}
		size += xfer->dm_nsegs * sizeof(struct aac_sg_entryraw);
		size = sizeof(fib->Header) + size;
		fib->Header.Size = htole16(size);
	} else if ((aac->sc_quirks & AAC_QUIRK_SG_64BIT) == 0) {
		struct aac_blockread *br;
		struct aac_blockwrite *bw;
		struct aac_sg_entry *sge;
		struct aac_sg_table *sgt;

		fib->Header.Command = htole16(ContainerCommand);
		if (dowrite) {
			bw = (struct aac_blockwrite *)&fib->data[0];
			bw->Command = htole32(VM_CtBlockWrite);
			bw->ContainerId = htole32(sc->sc_hwunit);
			bw->BlockNumber = htole32(blkno);
			bw->ByteCount = htole32(datasize);
			bw->Stable = htole32(CUNSTABLE);
			/* CSTABLE sometimes?  FUA? */

			size = sizeof(struct aac_blockwrite);
			sgt = &bw->SgMap;
		} else {
			br = (struct aac_blockread *)&fib->data[0];
			br->Command = htole32(VM_CtBlockRead);
			br->ContainerId = htole32(sc->sc_hwunit);
			br->BlockNumber = htole32(blkno);
			br->ByteCount = htole32(datasize);

			size = sizeof(struct aac_blockread);
			sgt = &br->SgMap;
		}

		xfer = ac->ac_dmamap_xfer;
		sgt->SgCount = xfer->dm_nsegs;
		sge = sgt->SgEntry;

		for (i = 0; i < xfer->dm_nsegs; i++, sge++) {
			sge->SgAddress = htole32(xfer->dm_segs[i].ds_addr);
			sge->SgByteCount = htole32(xfer->dm_segs[i].ds_len);
			AAC_DPRINTF(AAC_D_IO,
			    ("#%d va %p pa %" PRIxPADDR " len %zx\n",
			    i, data, xfer->dm_segs[i].ds_addr,
			    xfer->dm_segs[i].ds_len));
		}

		size += xfer->dm_nsegs * sizeof(struct aac_sg_entry);
		size = sizeof(fib->Header) + size;
		fib->Header.Size = htole16(size);
	} else {
		struct aac_blockread64 *br;
		struct aac_blockwrite64 *bw;
		struct aac_sg_entry64 *sge;
		struct aac_sg_table64 *sgt;

		fib->Header.Command = htole16(ContainerCommand64);
		if (dowrite) {
			bw = (struct aac_blockwrite64 *)&fib->data[0];
			bw->Command = htole32(VM_CtHostWrite64);
			bw->BlockNumber = htole32(blkno);
			bw->ContainerId = htole16(sc->sc_hwunit);
			bw->SectorCount = htole16(datasize / AAC_BLOCK_SIZE);
			bw->Pad = 0;
			bw->Flags = 0;

			size = sizeof(struct aac_blockwrite64);
			sgt = &bw->SgMap64;
		} else {
			br = (struct aac_blockread64 *)&fib->data[0];
			br->Command = htole32(VM_CtHostRead64);
			br->BlockNumber = htole32(blkno);
			br->ContainerId = htole16(sc->sc_hwunit);
			br->SectorCount = htole16(datasize / AAC_BLOCK_SIZE);
			br->Pad = 0;
			br->Flags = 0;

			size = sizeof(struct aac_blockread64);
			sgt = &br->SgMap64;
		}

		xfer = ac->ac_dmamap_xfer;
		sgt->SgCount = xfer->dm_nsegs;
		sge = sgt->SgEntry64;

		for (i = 0; i < xfer->dm_nsegs; i++, sge++) {
			/*
			 * XXX - This is probably an alignment issue on non-x86
			 * platforms since this is a packed array of 64/32-bit
			 * tuples, so every other SgAddress is 32-bit, but not
			 * 64-bit aligned.
			 */
			sge->SgAddress = htole64(xfer->dm_segs[i].ds_addr);
			sge->SgByteCount = htole32(xfer->dm_segs[i].ds_len);
			AAC_DPRINTF(AAC_D_IO,
			    ("#%d va %p pa %" PRIxPADDR " len %zx\n",
			    i, data, xfer->dm_segs[i].ds_addr,
			    xfer->dm_segs[i].ds_len));
		}
		size += xfer->dm_nsegs * sizeof(struct aac_sg_entry64);
		size = sizeof(fib->Header) + size;
		fib->Header.Size = htole16(size);
	}

	if (bp == NULL) {
		/*
		 * Polled commands must not sit on the software queue.  Wait
		 * up to 30 seconds for the command to complete.
		 */
		s = splbio();
		rv = aac_ccb_poll(aac, ac, 30000);
		aac_ccb_unmap(aac, ac);
		aac_ccb_free(aac, ac);
		splx(s);

		if (rv == 0) {
			if (dowrite) {
				bwr = (struct aac_blockwrite_response *)
				    &ac->ac_fib->data[0];
				status = le32toh(bwr->Status);
			} else {
				brr = (struct aac_blockread_response *)
				    &ac->ac_fib->data[0];
				status = le32toh(brr->Status);
			}

			if (status != ST_OK) {
				aprint_error_dev(sc->sc_ld.sc_dv,
				    "I/O error: %s\n",
				    aac_describe_code(aac_command_status_table,
				    status));
				rv = EIO;
			}
		}
	} else {
		ac->ac_device = sc->sc_ld.sc_dv;
		ac->ac_context = bp;
		ac->ac_intr = ld_aac_intr;
		aac_ccb_enqueue(aac, ac);
		rv = 0;
	}

	return (rv);
}

static int
ld_aac_start(struct ld_softc *ld, struct buf *bp)
{

	return (ld_aac_dobio((struct ld_aac_softc *)ld, bp->b_data,
	    bp->b_bcount, bp->b_rawblkno, (bp->b_flags & B_READ) == 0, bp));
}

static void
ld_aac_intr(struct aac_ccb *ac)
{
	struct aac_blockread_response *brr;
	struct aac_blockwrite_response *bwr;
	struct ld_aac_softc *sc;
	struct aac_softc *aac;
	struct buf *bp;
	u_int32_t status;

	bp = ac->ac_context;
	sc = device_private(ac->ac_device);
	aac = device_private(device_parent(ac->ac_device));

	if ((bp->b_flags & B_READ) != 0) {
		brr = (struct aac_blockread_response *)&ac->ac_fib->data[0];
		status = le32toh(brr->Status);
	} else {
		bwr = (struct aac_blockwrite_response *)&ac->ac_fib->data[0];
		status = le32toh(bwr->Status);
	}

	aac_ccb_unmap(aac, ac);
	aac_ccb_free(aac, ac);

	if (status != ST_OK) {
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;

		aprint_error_dev(sc->sc_ld.sc_dv, "I/O error: %s\n",
		    aac_describe_code(aac_command_status_table, status));
	} else
		bp->b_resid = 0;

	lddone(&sc->sc_ld, bp);
}

static int
ld_aac_dump(struct ld_softc *ld, void *data, int blkno, int blkcnt)
{

	return (ld_aac_dobio((struct ld_aac_softc *)ld, data,
	    blkcnt * ld->sc_secsize, blkno, 1, NULL));
}
