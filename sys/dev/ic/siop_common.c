/*	$NetBSD: siop_common.c,v 1.54 2013/09/15 13:56:27 martin Exp $	*/

/*
 * Copyright (c) 2000, 2002 Manuel Bouyer.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* SYM53c7/8xx PCI-SCSI I/O Processors driver */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: siop_common.c,v 1.54 2013/09/15 13:56:27 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/scsiio.h>

#include <machine/endian.h>
#include <sys/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsipi_all.h>

#include <dev/scsipi/scsiconf.h>

#include <dev/ic/siopreg.h>
#include <dev/ic/siopvar_common.h>

#include "opt_siop.h"

#undef DEBUG
#undef DEBUG_DR
#undef DEBUG_NEG

int
siop_common_attach(struct siop_common_softc *sc)
{
	int error, i;
	bus_dma_segment_t seg;
	int rseg;

	/*
	 * Allocate DMA-safe memory for the script and map it.
	 */
	if ((sc->features & SF_CHIP_RAM) == 0) {
		error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE,
		    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "unable to allocate script DMA memory, "
			    "error = %d\n", error);
			return error;
		}
		error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, PAGE_SIZE,
		    (void **)&sc->sc_script,
		    BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "unable to map script DMA memory, "
			    "error = %d\n", error);
			return error;
		}
		error = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1,
		    PAGE_SIZE, 0, BUS_DMA_NOWAIT, &sc->sc_scriptdma);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create script DMA map, "
			    "error = %d\n", error);
			return error;
		}
		error = bus_dmamap_load(sc->sc_dmat, sc->sc_scriptdma,
		    sc->sc_script, PAGE_SIZE, NULL, BUS_DMA_NOWAIT);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "unable to load script DMA map, "
			    "error = %d\n", error);
			return error;
		}
		sc->sc_scriptaddr =
		    sc->sc_scriptdma->dm_segs[0].ds_addr;
		sc->ram_size = PAGE_SIZE;
	}

	sc->sc_adapt.adapt_dev = sc->sc_dev;
	sc->sc_adapt.adapt_nchannels = 1;
	sc->sc_adapt.adapt_openings = 0;
	sc->sc_adapt.adapt_ioctl = siop_ioctl;
	sc->sc_adapt.adapt_minphys = minphys;

	memset(&sc->sc_chan, 0, sizeof(sc->sc_chan));
	sc->sc_chan.chan_adapter = &sc->sc_adapt;
	sc->sc_chan.chan_bustype = &scsi_bustype;
	sc->sc_chan.chan_channel = 0;
	sc->sc_chan.chan_flags = SCSIPI_CHAN_CANGROW;
	sc->sc_chan.chan_ntargets =
	    (sc->features & SF_BUS_WIDE) ? 16 : 8;
	sc->sc_chan.chan_nluns = 8;
	sc->sc_chan.chan_id =
	    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCID);
	if (sc->sc_chan.chan_id == 0 ||
	    sc->sc_chan.chan_id >= sc->sc_chan.chan_ntargets)
		sc->sc_chan.chan_id = SIOP_DEFAULT_TARGET;

	for (i = 0; i < 16; i++)
		sc->targets[i] = NULL;

	/* find min/max sync period for this chip */
	sc->st_maxsync = 0;
	sc->dt_maxsync = 0;
	sc->st_minsync = 255;
	sc->dt_minsync = 255;
	for (i = 0; i < __arraycount(scf_period); i++) {
		if (sc->clock_period != scf_period[i].clock)
			continue;
		if (sc->st_maxsync < scf_period[i].period)
			sc->st_maxsync = scf_period[i].period;
		if (sc->st_minsync > scf_period[i].period)
			sc->st_minsync = scf_period[i].period;
	}
	if (sc->st_maxsync == 255 || sc->st_minsync == 0)
		panic("siop: can't find my sync parameters");
	for (i = 0; i < __arraycount(dt_scf_period); i++) {
		if (sc->clock_period != dt_scf_period[i].clock)
			continue;
		if (sc->dt_maxsync < dt_scf_period[i].period)
			sc->dt_maxsync = dt_scf_period[i].period;
		if (sc->dt_minsync > dt_scf_period[i].period)
			sc->dt_minsync = dt_scf_period[i].period;
	}
	if (sc->dt_maxsync == 255 || sc->dt_minsync == 0)
		panic("siop: can't find my sync parameters");
	return 0;
}

void
siop_common_reset(struct siop_common_softc *sc)
{
	u_int32_t stest1, stest3;

	/* reset the chip */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, ISTAT_SRST);
	delay(1000);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, 0);

	/* init registers */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL0,
	    SCNTL0_ARB_MASK | SCNTL0_EPC | SCNTL0_AAP);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1, 0);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3, sc->clock_div);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SXFER, 0);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_DIEN, 0xff);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SIEN0,
	    0xff & ~(SIEN0_CMP | SIEN0_SEL | SIEN0_RSL));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SIEN1,
	    0xff & ~(SIEN1_HTH | SIEN1_GEN));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2, 0);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3, STEST3_TE);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STIME0,
	    (0xb << STIME0_SEL_SHIFT));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCID,
	    sc->sc_chan.chan_id | SCID_RRE);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_RESPID0,
	    1 << sc->sc_chan.chan_id);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_DCNTL,
	    (sc->features & SF_CHIP_PF) ? DCNTL_COM | DCNTL_PFEN : DCNTL_COM);
	if (sc->features & SF_CHIP_AAIP)
		bus_space_write_1(sc->sc_rt, sc->sc_rh,
		    SIOP_AIPCNTL1, AIPCNTL1_DIS);

	/* enable clock doubler or quadruler if appropriate */
	if (sc->features & (SF_CHIP_DBLR | SF_CHIP_QUAD)) {
		stest3 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1,
		    STEST1_DBLEN);
		if (sc->features & SF_CHIP_QUAD) {
			/* wait for PPL to lock */
			while ((bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_STEST4) & STEST4_LOCK) == 0)
				delay(10);
		} else {
			/* data sheet says 20us - more won't hurt */
			delay(100);
		}
		/* halt scsi clock, select doubler/quad, restart clock */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3,
		    stest3 | STEST3_HSC);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1,
		    STEST1_DBLEN | STEST1_DBLSEL);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3, stest3);
	} else {
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1, 0);
	}

	if (sc->features & SF_CHIP_USEPCIC) {
		stest1 = bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_STEST1);
		stest1 |= STEST1_SCLK;
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1, stest1);
	}

	if (sc->features & SF_CHIP_FIFO)
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST5,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST5) |
		    CTEST5_DFS);
	if (sc->features & SF_CHIP_LED0) {
		/* Set GPIO0 as output if software LED control is required */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_GPCNTL,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_GPCNTL) & 0xfe);
	}
	if (sc->features & SF_BUS_ULTRA3) {
		/* reset SCNTL4 */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL4, 0);
	}
	sc->mode = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_STEST4) &
	    STEST4_MODE_MASK;

	/*
	 * initialise the RAM. Without this we may get scsi gross errors on
	 * the 1010
	 */
	if (sc->features & SF_CHIP_RAM)
		bus_space_set_region_4(sc->sc_ramt, sc->sc_ramh,
			0, 0, sc->ram_size / 4);
	sc->sc_reset(sc);
}

/* prepare tables before sending a cmd */
void
siop_setuptables(struct siop_common_cmd *siop_cmd)
{
	int i;
	struct siop_common_softc *sc = siop_cmd->siop_sc;
	struct scsipi_xfer *xs = siop_cmd->xs;
	int target = xs->xs_periph->periph_target;
	int lun = xs->xs_periph->periph_lun;
	int msgoffset = 1;

	siop_cmd->siop_tables->id = siop_htoc32(sc, sc->targets[target]->id);
	memset(siop_cmd->siop_tables->msg_out, 0,
	    sizeof(siop_cmd->siop_tables->msg_out));
	/* request sense doesn't disconnect */
	if (xs->xs_control & XS_CTL_REQSENSE)
		siop_cmd->siop_tables->msg_out[0] = MSG_IDENTIFY(lun, 0);
	else if ((sc->features & SF_CHIP_GEBUG) &&
	    (sc->targets[target]->flags & TARF_ISWIDE) == 0)
		/*
		 * 1010 bug: it seems that the 1010 has problems with reselect
		 * when not in wide mode (generate false SCSI gross error).
		 * The FreeBSD sym driver has comments about it but their
		 * workaround (disable SCSI gross error reporting) doesn't
		 * work with my adapter. So disable disconnect when not
		 * wide.
		 */
		siop_cmd->siop_tables->msg_out[0] = MSG_IDENTIFY(lun, 0);
	else
		siop_cmd->siop_tables->msg_out[0] = MSG_IDENTIFY(lun, 1);
	if (xs->xs_tag_type != 0) {
		if ((sc->targets[target]->flags & TARF_TAG) == 0) {
			scsipi_printaddr(xs->xs_periph);
			printf(": tagged command type %d id %d\n",
			    siop_cmd->xs->xs_tag_type, siop_cmd->xs->xs_tag_id);
			panic("tagged command for non-tagging device");
		}
		siop_cmd->flags |= CMDFL_TAG;
		siop_cmd->siop_tables->msg_out[1] = siop_cmd->xs->xs_tag_type;
		/*
		 * use siop_cmd->tag not xs->xs_tag_id, caller may want a
		 * different one
		 */
		siop_cmd->siop_tables->msg_out[2] = siop_cmd->tag;
		msgoffset = 3;
	}
	siop_cmd->siop_tables->t_msgout.count = siop_htoc32(sc, msgoffset);
	if (sc->targets[target]->status == TARST_ASYNC) {
		if ((sc->targets[target]->flags & TARF_DT) &&
		    (sc->mode == STEST4_MODE_LVD)) {
			sc->targets[target]->status = TARST_PPR_NEG;
			siop_ppr_msg(siop_cmd, msgoffset, sc->dt_minsync,
			    sc->maxoff);
		} else if (sc->targets[target]->flags & TARF_WIDE) {
			sc->targets[target]->status = TARST_WIDE_NEG;
			siop_wdtr_msg(siop_cmd, msgoffset,
			    MSG_EXT_WDTR_BUS_16_BIT);
		} else if (sc->targets[target]->flags & TARF_SYNC) {
			sc->targets[target]->status = TARST_SYNC_NEG;
			siop_sdtr_msg(siop_cmd, msgoffset, sc->st_minsync,
			(sc->maxoff > 31) ? 31 :  sc->maxoff);
		} else {
			sc->targets[target]->status = TARST_OK;
			siop_update_xfer_mode(sc, target);
		}
	}
	siop_cmd->siop_tables->status =
	    siop_htoc32(sc, SCSI_SIOP_NOSTATUS); /* set invalid status */

	siop_cmd->siop_tables->cmd.count =
	    siop_htoc32(sc, siop_cmd->dmamap_cmd->dm_segs[0].ds_len);
	siop_cmd->siop_tables->cmd.addr =
	    siop_htoc32(sc, siop_cmd->dmamap_cmd->dm_segs[0].ds_addr);
	if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
		for (i = 0; i < siop_cmd->dmamap_data->dm_nsegs; i++) {
			siop_cmd->siop_tables->data[i].count =
			    siop_htoc32(sc,
				siop_cmd->dmamap_data->dm_segs[i].ds_len);
			siop_cmd->siop_tables->data[i].addr =
			    siop_htoc32(sc,
				siop_cmd->dmamap_data->dm_segs[i].ds_addr);
		}
	}
}

int
siop_wdtr_neg(struct siop_common_cmd *siop_cmd)
{
	struct siop_common_softc *sc = siop_cmd->siop_sc;
	struct siop_common_target *siop_target = siop_cmd->siop_target;
	int target = siop_cmd->xs->xs_periph->periph_target;
	struct siop_common_xfer *tables = siop_cmd->siop_tables;

	if (siop_target->status == TARST_WIDE_NEG) {
		/* we initiated wide negotiation */
		switch (tables->msg_in[3]) {
		case MSG_EXT_WDTR_BUS_8_BIT:
			siop_target->flags &= ~TARF_ISWIDE;
			sc->targets[target]->id &= ~(SCNTL3_EWS << 24);
			break;
		case MSG_EXT_WDTR_BUS_16_BIT:
			if (siop_target->flags & TARF_WIDE) {
				siop_target->flags |= TARF_ISWIDE;
				sc->targets[target]->id |= (SCNTL3_EWS << 24);
				break;
			}
		/* FALLTHROUGH */
		default:
			/*
			 * hum, we got more than what we can handle, shouldn't
			 * happen. Reject, and stay async
			 */
			siop_target->flags &= ~TARF_ISWIDE;
			siop_target->status = TARST_OK;
			siop_target->offset = siop_target->period = 0;
			siop_update_xfer_mode(sc, target);
			printf("%s: rejecting invalid wide negotiation from "
			    "target %d (%d)\n", device_xname(sc->sc_dev),
			    target,
			    tables->msg_in[3]);
			tables->t_msgout.count = siop_htoc32(sc, 1);
			tables->msg_out[0] = MSG_MESSAGE_REJECT;
			return SIOP_NEG_MSGOUT;
		}
		tables->id = siop_htoc32(sc, sc->targets[target]->id);
		bus_space_write_1(sc->sc_rt, sc->sc_rh,
		    SIOP_SCNTL3,
		    (sc->targets[target]->id >> 24) & 0xff);
		/* we now need to do sync */
		if (siop_target->flags & TARF_SYNC) {
			siop_target->status = TARST_SYNC_NEG;
			siop_sdtr_msg(siop_cmd, 0, sc->st_minsync,
			    (sc->maxoff > 31) ? 31 : sc->maxoff);
			return SIOP_NEG_MSGOUT;
		} else {
			siop_target->status = TARST_OK;
			siop_update_xfer_mode(sc, target);
			return SIOP_NEG_ACK;
		}
	} else {
		/* target initiated wide negotiation */
		if (tables->msg_in[3] >= MSG_EXT_WDTR_BUS_16_BIT
		    && (siop_target->flags & TARF_WIDE)) {
			siop_target->flags |= TARF_ISWIDE;
			sc->targets[target]->id |= SCNTL3_EWS << 24;
		} else {
			siop_target->flags &= ~TARF_ISWIDE;
			sc->targets[target]->id &= ~(SCNTL3_EWS << 24);
		}
		tables->id = siop_htoc32(sc, sc->targets[target]->id);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
		    (sc->targets[target]->id >> 24) & 0xff);
		/*
		 * we did reset wide parameters, so fall back to async,
		 * but don't schedule a sync neg, target should initiate it
		 */
		siop_target->status = TARST_OK;
		siop_target->offset = siop_target->period = 0;
		siop_update_xfer_mode(sc, target);
		siop_wdtr_msg(siop_cmd, 0, (siop_target->flags & TARF_ISWIDE) ?
		    MSG_EXT_WDTR_BUS_16_BIT : MSG_EXT_WDTR_BUS_8_BIT);
		return SIOP_NEG_MSGOUT;
	}
}

int
siop_ppr_neg(struct siop_common_cmd *siop_cmd)
{
	struct siop_common_softc *sc = siop_cmd->siop_sc;
	struct siop_common_target *siop_target = siop_cmd->siop_target;
	int target = siop_cmd->xs->xs_periph->periph_target;
	struct siop_common_xfer *tables = siop_cmd->siop_tables;
	int sync, offset, options, scf = 0;
	int i;

#ifdef DEBUG_NEG
	printf("%s: answer on ppr negotiation:", device_xname(sc->sc_dev));
	for (i = 0; i < 8; i++)
		printf(" 0x%x", tables->msg_in[i]);
	printf("\n");
#endif

	if (siop_target->status == TARST_PPR_NEG) {
		/* we initiated PPR negotiation */
		sync = tables->msg_in[3];
		offset = tables->msg_in[5];
		options = tables->msg_in[7];
		if (options != MSG_EXT_PPR_DT) {
			/* should't happen */
			printf("%s: ppr negotiation for target %d: "
			    "no DT option\n", device_xname(sc->sc_dev), target);
			siop_target->status = TARST_ASYNC;
			siop_target->flags &= ~(TARF_DT | TARF_ISDT);
			siop_target->offset = 0;
			siop_target->period = 0;
			goto reject;
		}

		if (offset > sc->maxoff || sync < sc->dt_minsync ||
		    sync > sc->dt_maxsync) {
			printf("%s: ppr negotiation for target %d: "
			    "offset (%d) or sync (%d) out of range\n",
			    device_xname(sc->sc_dev), target, offset, sync);
			/* should not happen */
			siop_target->offset = 0;
			siop_target->period = 0;
			goto reject;
		} else {
			for (i = 0; i < __arraycount(dt_scf_period); i++) {
				if (sc->clock_period != dt_scf_period[i].clock)
					continue;
				if (dt_scf_period[i].period == sync) {
					/* ok, found it. we now are sync. */
					siop_target->offset = offset;
					siop_target->period = sync;
					scf = dt_scf_period[i].scf;
					siop_target->flags |= TARF_ISDT;
				}
			}
			if ((siop_target->flags & TARF_ISDT) == 0) {
				printf("%s: ppr negotiation for target %d: "
				    "sync (%d) incompatible with adapter\n",
				    device_xname(sc->sc_dev), target, sync);
				/*
				 * we didn't find it in our table, do async
				 * send reject msg, start SDTR/WDTR neg
				 */
				siop_target->status = TARST_ASYNC;
				siop_target->flags &= ~(TARF_DT | TARF_ISDT);
				siop_target->offset = 0;
				siop_target->period = 0;
				goto reject;
			}
		}
		if (tables->msg_in[6] != 1) {
			printf("%s: ppr negotiation for target %d: "
			    "transfer width (%d) incompatible with dt\n",
			    device_xname(sc->sc_dev),
			    target, tables->msg_in[6]);
			/* DT mode can only be done with wide transfers */
			siop_target->status = TARST_ASYNC;
			goto reject;
		}
		siop_target->flags |= TARF_ISWIDE;
		sc->targets[target]->id |= (SCNTL3_EWS << 24);
		sc->targets[target]->id &= ~(SCNTL3_SCF_MASK << 24);
		sc->targets[target]->id |= scf << (24 + SCNTL3_SCF_SHIFT);
		sc->targets[target]->id &= ~(SXFER_MO_MASK << 8);
		sc->targets[target]->id |=
		    (siop_target->offset & SXFER_MO_MASK) << 8;
		sc->targets[target]->id &= ~0xff;
		sc->targets[target]->id |= SCNTL4_U3EN;
		siop_target->status = TARST_OK;
		siop_update_xfer_mode(sc, target);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
		    (sc->targets[target]->id >> 24) & 0xff);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SXFER,
		    (sc->targets[target]->id >> 8) & 0xff);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL4,
		    sc->targets[target]->id & 0xff);
		return SIOP_NEG_ACK;
	} else {
		/* target initiated PPR negotiation, shouldn't happen */
		printf("%s: rejecting invalid PPR negotiation from "
		    "target %d\n", device_xname(sc->sc_dev), target);
reject:
		tables->t_msgout.count = siop_htoc32(sc, 1);
		tables->msg_out[0] = MSG_MESSAGE_REJECT;
		return SIOP_NEG_MSGOUT;
	}
}

int
siop_sdtr_neg(struct siop_common_cmd *siop_cmd)
{
	struct siop_common_softc *sc = siop_cmd->siop_sc;
	struct siop_common_target *siop_target = siop_cmd->siop_target;
	int target = siop_cmd->xs->xs_periph->periph_target;
	int sync, maxoffset, offset, i;
	int send_msgout = 0;
	struct siop_common_xfer *tables = siop_cmd->siop_tables;

	/* limit to Ultra/2 parameters, need PPR for Ultra/3 */
	maxoffset = (sc->maxoff > 31) ? 31 : sc->maxoff;

	sync = tables->msg_in[3];
	offset = tables->msg_in[4];

	if (siop_target->status == TARST_SYNC_NEG) {
		/* we initiated sync negotiation */
		siop_target->status = TARST_OK;
#ifdef DEBUG
		printf("sdtr: sync %d offset %d\n", sync, offset);
#endif
		if (offset > maxoffset || sync < sc->st_minsync ||
			sync > sc->st_maxsync)
			goto reject;
		for (i = 0; i < __arraycount(scf_period); i++) {
			if (sc->clock_period != scf_period[i].clock)
				continue;
			if (scf_period[i].period == sync) {
				/* ok, found it. we now are sync. */
				siop_target->offset = offset;
				siop_target->period = sync;
				sc->targets[target]->id &=
				    ~(SCNTL3_SCF_MASK << 24);
				sc->targets[target]->id |= scf_period[i].scf
				    << (24 + SCNTL3_SCF_SHIFT);
				if (sync < 25 && /* Ultra */
				    (sc->features & SF_BUS_ULTRA3) == 0)
					sc->targets[target]->id |=
					    SCNTL3_ULTRA << 24;
				else
					sc->targets[target]->id &=
					    ~(SCNTL3_ULTRA << 24);
				sc->targets[target]->id &=
				    ~(SXFER_MO_MASK << 8);
				sc->targets[target]->id |=
				    (offset & SXFER_MO_MASK) << 8;
				sc->targets[target]->id &= ~0xff; /* scntl4 */
				goto end;
			}
		}
		/*
		 * we didn't find it in our table, do async and send reject
		 * msg
		 */
reject:
		send_msgout = 1;
		tables->t_msgout.count = siop_htoc32(sc, 1);
		tables->msg_out[0] = MSG_MESSAGE_REJECT;
		sc->targets[target]->id &= ~(SCNTL3_SCF_MASK << 24);
		sc->targets[target]->id &= ~(SCNTL3_ULTRA << 24);
		sc->targets[target]->id &= ~(SXFER_MO_MASK << 8);
		sc->targets[target]->id &= ~0xff; /* scntl4 */
		siop_target->offset = siop_target->period = 0;
	} else { /* target initiated sync neg */
#ifdef DEBUG
		printf("sdtr (target): sync %d offset %d\n", sync, offset);
#endif
		if (offset == 0 || sync > sc->st_maxsync) { /* async */
			goto async;
		}
		if (offset > maxoffset)
			offset = maxoffset;
		if (sync < sc->st_minsync)
			sync = sc->st_minsync;
		/* look for sync period */
		for (i = 0; i < __arraycount(scf_period); i++) {
			if (sc->clock_period != scf_period[i].clock)
				continue;
			if (scf_period[i].period == sync) {
				/* ok, found it. we now are sync. */
				siop_target->offset = offset;
				siop_target->period = sync;
				sc->targets[target]->id &=
				    ~(SCNTL3_SCF_MASK << 24);
				sc->targets[target]->id |= scf_period[i].scf
				    << (24 + SCNTL3_SCF_SHIFT);
				if (sync < 25 && /* Ultra */
				    (sc->features & SF_BUS_ULTRA3) == 0)
					sc->targets[target]->id |=
					    SCNTL3_ULTRA << 24;
				else
					sc->targets[target]->id &=
					    ~(SCNTL3_ULTRA << 24);
				sc->targets[target]->id &=
				    ~(SXFER_MO_MASK << 8);
				sc->targets[target]->id |=
				    (offset & SXFER_MO_MASK) << 8;
				sc->targets[target]->id &= ~0xff; /* scntl4 */
				siop_sdtr_msg(siop_cmd, 0, sync, offset);
				send_msgout = 1;
				goto end;
			}
		}
async:
		siop_target->offset = siop_target->period = 0;
		sc->targets[target]->id &= ~(SCNTL3_SCF_MASK << 24);
		sc->targets[target]->id &= ~(SCNTL3_ULTRA << 24);
		sc->targets[target]->id &= ~(SXFER_MO_MASK << 8);
		sc->targets[target]->id &= ~0xff; /* scntl4 */
		siop_sdtr_msg(siop_cmd, 0, 0, 0);
		send_msgout = 1;
	}
end:
	if (siop_target->status == TARST_OK)
		siop_update_xfer_mode(sc, target);
#ifdef DEBUG
	printf("id now 0x%x\n", sc->targets[target]->id);
#endif
	tables->id = siop_htoc32(sc, sc->targets[target]->id);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
	    (sc->targets[target]->id >> 24) & 0xff);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SXFER,
	    (sc->targets[target]->id >> 8) & 0xff);
	if (send_msgout) {
		return SIOP_NEG_MSGOUT;
	} else {
		return SIOP_NEG_ACK;
	}
}

void
siop_sdtr_msg(struct siop_common_cmd *siop_cmd, int offset, int ssync, int soff)
{

	siop_cmd->siop_tables->msg_out[offset + 0] = MSG_EXTENDED;
	siop_cmd->siop_tables->msg_out[offset + 1] = MSG_EXT_SDTR_LEN;
	siop_cmd->siop_tables->msg_out[offset + 2] = MSG_EXT_SDTR;
	siop_cmd->siop_tables->msg_out[offset + 3] = ssync;
	siop_cmd->siop_tables->msg_out[offset + 4] = soff;
	siop_cmd->siop_tables->t_msgout.count =
	    siop_htoc32(siop_cmd->siop_sc, offset + MSG_EXT_SDTR_LEN + 2);
}

void
siop_wdtr_msg(struct siop_common_cmd *siop_cmd, int offset, int wide)
{

	siop_cmd->siop_tables->msg_out[offset + 0] = MSG_EXTENDED;
	siop_cmd->siop_tables->msg_out[offset + 1] = MSG_EXT_WDTR_LEN;
	siop_cmd->siop_tables->msg_out[offset + 2] = MSG_EXT_WDTR;
	siop_cmd->siop_tables->msg_out[offset + 3] = wide;
	siop_cmd->siop_tables->t_msgout.count =
	    siop_htoc32(siop_cmd->siop_sc, offset + MSG_EXT_WDTR_LEN + 2);
}

void
siop_ppr_msg(struct siop_common_cmd *siop_cmd, int offset, int ssync, int soff)
{

	siop_cmd->siop_tables->msg_out[offset + 0] = MSG_EXTENDED;
	siop_cmd->siop_tables->msg_out[offset + 1] = MSG_EXT_PPR_LEN;
	siop_cmd->siop_tables->msg_out[offset + 2] = MSG_EXT_PPR;
	siop_cmd->siop_tables->msg_out[offset + 3] = ssync;
	siop_cmd->siop_tables->msg_out[offset + 4] = 0; /* reserved */
	siop_cmd->siop_tables->msg_out[offset + 5] = soff;
	siop_cmd->siop_tables->msg_out[offset + 6] = 1; /* wide */
	siop_cmd->siop_tables->msg_out[offset + 7] = MSG_EXT_PPR_DT;
	siop_cmd->siop_tables->t_msgout.count =
	    siop_htoc32(siop_cmd->siop_sc, offset + MSG_EXT_PPR_LEN + 2);
}

void
siop_minphys(struct buf *bp)
{

	minphys(bp);
}

int
siop_ioctl(struct scsipi_channel *chan, u_long cmd, void *arg,
    int flag, struct proc *p)
{
	struct siop_common_softc *sc;

	sc = device_private(chan->chan_adapter->adapt_dev);

	switch (cmd) {
	case SCBUSIORESET:
		/*
		 * abort the script. This will trigger an interrupt, which will
		 * trigger a bus reset.
		 * We can't safely trigger the reset here as we can't access
		 * the required register while the script is running.
		 */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, ISTAT_ABRT);
		return (0);
	default:
		return (ENOTTY);
	}
}

void
siop_ma(struct siop_common_cmd *siop_cmd)
{
	int offset, dbc, sstat;
	struct siop_common_softc *sc = siop_cmd->siop_sc;
#ifdef DEBUG_DR
	scr_table_t *table; /* table with partial xfer */
#endif

	/*
	 * compute how much of the current table didn't get handled when
	 * a phase mismatch occurs
	 */
	if ((siop_cmd->xs->xs_control & (XS_CTL_DATA_OUT | XS_CTL_DATA_IN))
	    == 0)
	    return; /* no valid data transfer */

	offset = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCRATCHA + 1);
	if (offset >= SIOP_NSG) {
		aprint_error_dev(sc->sc_dev, "bad offset in siop_sdp (%d)\n",
		    offset);
		return;
	}
#ifdef DEBUG_DR
	table = &siop_cmd->siop_tables->data[offset];
	printf("siop_ma: offset %d count=%d addr=0x%x ", offset,
	    table->count, table->addr);
#endif
	dbc = bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DBC) & 0x00ffffff;
	if (siop_cmd->xs->xs_control & XS_CTL_DATA_OUT) {
		if (sc->features & SF_CHIP_DFBC) {
			dbc +=
			    bus_space_read_2(sc->sc_rt, sc->sc_rh, SIOP_DFBC);
		} else {
			/* need to account stale data in FIFO */
			int dfifo =
			    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_DFIFO);
			if (sc->features & SF_CHIP_FIFO) {
				dfifo |= (bus_space_read_1(sc->sc_rt, sc->sc_rh,
				    SIOP_CTEST5) & CTEST5_BOMASK) << 8;
				dbc += (dfifo - (dbc & 0x3ff)) & 0x3ff;
			} else {
				dbc += (dfifo - (dbc & 0x7f)) & 0x7f;
			}
		}
		sstat = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT0);
		if (sstat & SSTAT0_OLF)
			dbc++;
		if ((sstat & SSTAT0_ORF) && (sc->features & SF_CHIP_DFBC) == 0)
			dbc++;
		if (siop_cmd->siop_target->flags & TARF_ISWIDE) {
			sstat = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SSTAT2);
			if (sstat & SSTAT2_OLF1)
				dbc++;
			if ((sstat & SSTAT2_ORF1) &&
			    (sc->features & SF_CHIP_DFBC) == 0)
				dbc++;
		}
		/* clear the FIFO */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3) |
		    CTEST3_CLF);
	}
	siop_cmd->flags |= CMDFL_RESID;
	siop_cmd->resid = dbc;
}

void
siop_sdp(struct siop_common_cmd *siop_cmd, int offset)
{
	struct siop_common_softc *sc = siop_cmd->siop_sc;
	scr_table_t *table;

	if ((siop_cmd->xs->xs_control & (XS_CTL_DATA_OUT | XS_CTL_DATA_IN))
	    == 0)
	    return; /* no data pointers to save */

	/*
	 * offset == SIOP_NSG may be a valid condition if we get a Save data
	 * pointer when the xfer is done. Just ignore the Save data pointer
	 * in this case
	 */
	if (offset == SIOP_NSG)
		return;
#ifdef DIAGNOSTIC
	if (offset > SIOP_NSG) {
		scsipi_printaddr(siop_cmd->xs->xs_periph);
		printf(": offset %d > %d\n", offset, SIOP_NSG);
		panic("siop_sdp: offset");
	}
#endif
	/*
	 * Save data pointer. We do this by adjusting the tables to point
	 * at the begginning of the data not yet transfered.
	 * offset points to the first table with untransfered data.
	 */

	/*
	 * before doing that we decrease resid from the ammount of data which
	 * has been transfered.
	 */
	siop_update_resid(siop_cmd, offset);

	/*
	 * First let see if we have a resid from a phase mismatch. If so,
	 * we have to adjst the table at offset to remove transfered data.
	 */
	if (siop_cmd->flags & CMDFL_RESID) {
		siop_cmd->flags &= ~CMDFL_RESID;
		table = &siop_cmd->siop_tables->data[offset];
		/* "cut" already transfered data from this table */
		table->addr =
		    siop_htoc32(sc, siop_ctoh32(sc, table->addr) +
		    siop_ctoh32(sc, table->count) - siop_cmd->resid);
		table->count = siop_htoc32(sc, siop_cmd->resid);
	}

	/*
	 * now we can remove entries which have been transfered.
	 * We just move the entries with data left at the beggining of the
	 * tables
	 */
	memmove(&siop_cmd->siop_tables->data[0],
	    &siop_cmd->siop_tables->data[offset],
	    (SIOP_NSG - offset) * sizeof(scr_table_t));
}

void
siop_update_resid(struct siop_common_cmd *siop_cmd, int offset)
{
	struct siop_common_softc *sc = siop_cmd->siop_sc;
	scr_table_t *table;
	int i;

	if ((siop_cmd->xs->xs_control & (XS_CTL_DATA_OUT | XS_CTL_DATA_IN))
	    == 0)
	    return; /* no data to transfer */

	/*
	 * update resid. First account for the table entries which have
	 * been fully completed.
	 */
	for (i = 0; i < offset; i++)
		siop_cmd->xs->resid -=
		    siop_ctoh32(sc, siop_cmd->siop_tables->data[i].count);
	/*
	 * if CMDFL_RESID is set, the last table (pointed by offset) is a
	 * partial transfers. If not, offset points to the entry folloing
	 * the last full transfer.
	 */
	if (siop_cmd->flags & CMDFL_RESID) {
		table = &siop_cmd->siop_tables->data[offset];
		siop_cmd->xs->resid -=
		    siop_ctoh32(sc, table->count) - siop_cmd->resid;
	}
}

int
siop_iwr(struct siop_common_cmd *siop_cmd)
{
	int offset;
	scr_table_t *table; /* table with IWR */
	struct siop_common_softc *sc = siop_cmd->siop_sc;

	/* handle ignore wide residue messages */

	/* if target isn't wide, reject */
	if ((siop_cmd->siop_target->flags & TARF_ISWIDE) == 0) {
		siop_cmd->siop_tables->t_msgout.count = siop_htoc32(sc, 1);
		siop_cmd->siop_tables->msg_out[0] = MSG_MESSAGE_REJECT;
		return SIOP_NEG_MSGOUT;
	}
	/* get index of current command in table */
	offset = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCRATCHA + 1);
	/*
	 * if the current table did complete, we're now pointing at the
	 * next one. Go back one if we didn't see a phase mismatch.
	 */
	if ((siop_cmd->flags & CMDFL_RESID) == 0)
		offset--;
	table = &siop_cmd->siop_tables->data[offset];

	if ((siop_cmd->flags & CMDFL_RESID) == 0) {
		if (siop_ctoh32(sc, table->count) & 1) {
			/* we really got the number of bytes we expected */
			return SIOP_NEG_ACK;
		} else {
			/*
			 * now we really had a short xfer, by one byte.
			 * handle it just as if we had a phase mistmatch
			 * (there is a resid of one for this table).
			 * Update scratcha1 to reflect the fact that
			 * this xfer isn't complete.
			 */
			 siop_cmd->flags |= CMDFL_RESID;
			 siop_cmd->resid = 1;
			 bus_space_write_1(sc->sc_rt, sc->sc_rh,
			     SIOP_SCRATCHA + 1, offset);
			 return SIOP_NEG_ACK;
		}
	} else {
		/*
		 * we already have a short xfer for this table; it's
		 * just one byte less than we though it was
		 */
		siop_cmd->resid--;
		return SIOP_NEG_ACK;
	}
}

void
siop_clearfifo(struct siop_common_softc *sc)
{
	int timeout = 0;
	int ctest3 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3);

#ifdef DEBUG_INTR
	printf("DMA fifo not empty !\n");
#endif
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
	    ctest3 | CTEST3_CLF);
	while ((bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3) &
	    CTEST3_CLF) != 0) {
		delay(1);
		if (++timeout > 1000) {
			printf("clear fifo failed\n");
			bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
			    bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_CTEST3) & ~CTEST3_CLF);
			return;
		}
	}
}

int
siop_modechange(struct siop_common_softc *sc)
{
	int retry;
	int sist1, stest2;

	for (retry = 0; retry < 5; retry++) {
		/*
		 * datasheet says to wait 100ms and re-read SIST1,
		 * to check that DIFFSENSE is stable.
		 * We may delay() 5 times for  100ms at interrupt time;
		 * hopefully this will not happen often.
		 */
		delay(100000);
		(void)bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SIST0);
		sist1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SIST1);
		if (sist1 & SIEN1_SBMC)
			continue; /* we got an irq again */
		sc->mode = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_STEST4) &
		    STEST4_MODE_MASK;
		stest2 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2);
		switch(sc->mode) {
		case STEST4_MODE_DIF:
			printf("%s: switching to differential mode\n",
			    device_xname(sc->sc_dev));
			bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2,
			    stest2 | STEST2_DIF);
			break;
		case STEST4_MODE_SE:
			printf("%s: switching to single-ended mode\n",
			    device_xname(sc->sc_dev));
			bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2,
			    stest2 & ~STEST2_DIF);
			break;
		case STEST4_MODE_LVD:
			printf("%s: switching to LVD mode\n",
			    device_xname(sc->sc_dev));
			bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2,
			    stest2 & ~STEST2_DIF);
			break;
		default:
			aprint_error_dev(sc->sc_dev, "invalid SCSI mode 0x%x\n",
			    sc->mode);
			return 0;
		}
		return 1;
	}
	printf("%s: timeout waiting for DIFFSENSE to stabilise\n",
	    device_xname(sc->sc_dev));
	return 0;
}

void
siop_resetbus(struct siop_common_softc *sc)
{
	int scntl1;

	scntl1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1,
	    scntl1 | SCNTL1_RST);
	/* minimum 25 us, more time won't hurt */
	delay(100);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1, scntl1);
}

void
siop_update_xfer_mode(struct siop_common_softc *sc, int target)
{
	struct siop_common_target *siop_target = sc->targets[target];
	struct scsipi_xfer_mode xm;

	xm.xm_target = target;
	xm.xm_mode = 0;
	xm.xm_period = 0;
	xm.xm_offset = 0;

	if (siop_target->flags & TARF_ISWIDE)
		xm.xm_mode |= PERIPH_CAP_WIDE16;
	if (siop_target->period) {
		xm.xm_period = siop_target->period;
		xm.xm_offset = siop_target->offset;
		xm.xm_mode |= PERIPH_CAP_SYNC;
	}
	if (siop_target->flags & TARF_TAG) {
	/* 1010 workaround: can't do disconnect if not wide, so can't do tag */
		if ((sc->features & SF_CHIP_GEBUG) == 0 ||
		    (sc->targets[target]->flags & TARF_ISWIDE))
			xm.xm_mode |= PERIPH_CAP_TQING;
	}

	scsipi_async_event(&sc->sc_chan, ASYNC_EVENT_XFER_MODE, &xm);
}
