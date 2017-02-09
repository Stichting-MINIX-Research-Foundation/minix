/* $NetBSD: mfi.c,v 1.57 2015/04/04 15:10:47 christos Exp $ */
/* $OpenBSD: mfi.c,v 1.66 2006/11/28 23:59:45 dlg Exp $ */

/*
 * Copyright (c) 2012 Manuel Bouyer.
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
 */

/*
 * Copyright (c) 2006 Marco Peereboom <marco@peereboom.us>
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

 /*-
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *            Copyright 1994-2009 The FreeBSD Project.
 *            All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FREEBSD PROJECT OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY,OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION)HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mfi.c,v 1.57 2015/04/04 15:10:47 christos Exp $");

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/cpu.h>
#include <sys/conf.h>
#include <sys/kauth.h>

#include <uvm/uvm_param.h>

#include <sys/bus.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_spc.h>
#include <dev/scsipi/scsipi_disk.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/mfireg.h>
#include <dev/ic/mfivar.h>
#include <dev/ic/mfiio.h>

#if NBIO > 0
#include <dev/biovar.h>
#endif /* NBIO > 0 */

#ifdef MFI_DEBUG
uint32_t	mfi_debug = 0
/*		    | MFI_D_CMD  */
/*		    | MFI_D_INTR */
/*		    | MFI_D_MISC */
/*		    | MFI_D_DMA */
/*		    | MFI_D_IOCTL */
/*		    | MFI_D_RW */
/*		    | MFI_D_MEM */
/*		    | MFI_D_CCB */
/*		    | MFI_D_SYNC */
		;
#endif

static void		mfi_scsipi_request(struct scsipi_channel *,
				scsipi_adapter_req_t, void *);
static void		mfiminphys(struct buf *bp);

static struct mfi_ccb	*mfi_get_ccb(struct mfi_softc *);
static void		mfi_put_ccb(struct mfi_ccb *);
static int		mfi_init_ccb(struct mfi_softc *);

static struct mfi_mem	*mfi_allocmem(struct mfi_softc *, size_t);
static void		mfi_freemem(struct mfi_softc *, struct mfi_mem **);

static int		mfi_transition_firmware(struct mfi_softc *);
static int		mfi_initialize_firmware(struct mfi_softc *);
static int		mfi_get_info(struct mfi_softc *);
static int		mfi_get_bbu(struct mfi_softc *,
			    struct mfi_bbu_status *);
/* return codes for mfi_get_bbu */
#define MFI_BBU_GOOD	0
#define MFI_BBU_BAD	1
#define MFI_BBU_UNKNOWN	2
static uint32_t		mfi_read(struct mfi_softc *, bus_size_t);
static void		mfi_write(struct mfi_softc *, bus_size_t, uint32_t);
static int		mfi_poll(struct mfi_ccb *);
static int		mfi_create_sgl(struct mfi_ccb *, int);

/* commands */
static int		mfi_scsi_ld(struct mfi_ccb *, struct scsipi_xfer *);
static int		mfi_scsi_ld_io(struct mfi_ccb *, struct scsipi_xfer *,
				uint64_t, uint32_t);
static void		mfi_scsi_ld_done(struct mfi_ccb *);
static void		mfi_scsi_xs_done(struct mfi_ccb *, int, int);
static int		mfi_mgmt_internal(struct mfi_softc *, uint32_t,
			    uint32_t, uint32_t, void *, uint8_t *, bool);
static int		mfi_mgmt(struct mfi_ccb *,struct scsipi_xfer *,
			    uint32_t, uint32_t, uint32_t, void *, uint8_t *);
static void		mfi_mgmt_done(struct mfi_ccb *);

#if NBIO > 0
static int		mfi_ioctl(device_t, u_long, void *);
static int		mfi_ioctl_inq(struct mfi_softc *, struct bioc_inq *);
static int		mfi_ioctl_vol(struct mfi_softc *, struct bioc_vol *);
static int		mfi_ioctl_disk(struct mfi_softc *, struct bioc_disk *);
static int		mfi_ioctl_alarm(struct mfi_softc *,
				struct bioc_alarm *);
static int		mfi_ioctl_blink(struct mfi_softc *sc,
				struct bioc_blink *);
static int		mfi_ioctl_setstate(struct mfi_softc *,
				struct bioc_setstate *);
static int		mfi_bio_hs(struct mfi_softc *, int, int, void *);
static int		mfi_create_sensors(struct mfi_softc *);
static int		mfi_destroy_sensors(struct mfi_softc *);
static void		mfi_sensor_refresh(struct sysmon_envsys *,
				envsys_data_t *);
#endif /* NBIO > 0 */
static bool		mfi_shutdown(device_t, int);
static bool		mfi_suspend(device_t, const pmf_qual_t *);
static bool		mfi_resume(device_t, const pmf_qual_t *);

static dev_type_open(mfifopen);
static dev_type_close(mfifclose);
static dev_type_ioctl(mfifioctl);
const struct cdevsw mfi_cdevsw = {
	.d_open = mfifopen,
	.d_close = mfifclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = mfifioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

extern struct cfdriver mfi_cd;

static uint32_t 	mfi_xscale_fw_state(struct mfi_softc *sc);
static void 		mfi_xscale_intr_ena(struct mfi_softc *sc);
static void 		mfi_xscale_intr_dis(struct mfi_softc *sc);
static int 		mfi_xscale_intr(struct mfi_softc *sc);
static void 		mfi_xscale_post(struct mfi_softc *sc, struct mfi_ccb *ccb);

static const struct mfi_iop_ops mfi_iop_xscale = {
	mfi_xscale_fw_state,
	mfi_xscale_intr_dis,
	mfi_xscale_intr_ena,
	mfi_xscale_intr,
	mfi_xscale_post,
	mfi_scsi_ld_io,
};

static uint32_t 	mfi_ppc_fw_state(struct mfi_softc *sc);
static void 		mfi_ppc_intr_ena(struct mfi_softc *sc);
static void 		mfi_ppc_intr_dis(struct mfi_softc *sc);
static int 		mfi_ppc_intr(struct mfi_softc *sc);
static void 		mfi_ppc_post(struct mfi_softc *sc, struct mfi_ccb *ccb);

static const struct mfi_iop_ops mfi_iop_ppc = {
	mfi_ppc_fw_state,
	mfi_ppc_intr_dis,
	mfi_ppc_intr_ena,
	mfi_ppc_intr,
	mfi_ppc_post,
	mfi_scsi_ld_io,
};

uint32_t	mfi_gen2_fw_state(struct mfi_softc *sc);
void		mfi_gen2_intr_ena(struct mfi_softc *sc);
void		mfi_gen2_intr_dis(struct mfi_softc *sc);
int		mfi_gen2_intr(struct mfi_softc *sc);
void		mfi_gen2_post(struct mfi_softc *sc, struct mfi_ccb *ccb);

static const struct mfi_iop_ops mfi_iop_gen2 = {
	mfi_gen2_fw_state,
	mfi_gen2_intr_dis,
	mfi_gen2_intr_ena,
	mfi_gen2_intr,
	mfi_gen2_post,
	mfi_scsi_ld_io,
};

u_int32_t	mfi_skinny_fw_state(struct mfi_softc *);
void		mfi_skinny_intr_dis(struct mfi_softc *);
void		mfi_skinny_intr_ena(struct mfi_softc *);
int		mfi_skinny_intr(struct mfi_softc *);
void		mfi_skinny_post(struct mfi_softc *, struct mfi_ccb *);

static const struct mfi_iop_ops mfi_iop_skinny = {
	mfi_skinny_fw_state,
	mfi_skinny_intr_dis,
	mfi_skinny_intr_ena,
	mfi_skinny_intr,
	mfi_skinny_post,
	mfi_scsi_ld_io,
};

static int	mfi_tbolt_init_desc_pool(struct mfi_softc *);
static int	mfi_tbolt_init_MFI_queue(struct mfi_softc *);
static void	mfi_tbolt_build_mpt_ccb(struct mfi_ccb *);
int		mfi_tbolt_scsi_ld_io(struct mfi_ccb *, struct scsipi_xfer *,
		    uint64_t, uint32_t);
static void	mfi_tbolt_scsi_ld_done(struct mfi_ccb *);
static int	mfi_tbolt_create_sgl(struct mfi_ccb *, int);
void		mfi_tbolt_sync_map_info(struct work *, void *);
static void	mfi_sync_map_complete(struct mfi_ccb *);

u_int32_t	mfi_tbolt_fw_state(struct mfi_softc *);
void		mfi_tbolt_intr_dis(struct mfi_softc *);
void		mfi_tbolt_intr_ena(struct mfi_softc *);
int		mfi_tbolt_intr(struct mfi_softc *sc);
void		mfi_tbolt_post(struct mfi_softc *, struct mfi_ccb *);

static const struct mfi_iop_ops mfi_iop_tbolt = {
	mfi_tbolt_fw_state,
	mfi_tbolt_intr_dis,
	mfi_tbolt_intr_ena,
	mfi_tbolt_intr,
	mfi_tbolt_post,
	mfi_tbolt_scsi_ld_io,
};

#define mfi_fw_state(_s) 	((_s)->sc_iop->mio_fw_state(_s))
#define mfi_intr_enable(_s) 	((_s)->sc_iop->mio_intr_ena(_s))
#define mfi_intr_disable(_s) 	((_s)->sc_iop->mio_intr_dis(_s))
#define mfi_my_intr(_s) 	((_s)->sc_iop->mio_intr(_s))
#define mfi_post(_s, _c) 	((_s)->sc_iop->mio_post((_s), (_c)))

static struct mfi_ccb *
mfi_get_ccb(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	int			s;

	s = splbio();
	ccb = TAILQ_FIRST(&sc->sc_ccb_freeq);
	if (ccb) {
		TAILQ_REMOVE(&sc->sc_ccb_freeq, ccb, ccb_link);
		ccb->ccb_state = MFI_CCB_READY;
	}
	splx(s);

	DNPRINTF(MFI_D_CCB, "%s: mfi_get_ccb: %p\n", DEVNAME(sc), ccb);
	if (__predict_false(ccb == NULL && sc->sc_running))
		aprint_error_dev(sc->sc_dev, "out of ccb\n");

	return ccb;
}

static void
mfi_put_ccb(struct mfi_ccb *ccb)
{
	struct mfi_softc	*sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr = &ccb->ccb_frame->mfr_header;
	int			s;

	DNPRINTF(MFI_D_CCB, "%s: mfi_put_ccb: %p\n", DEVNAME(sc), ccb);

	hdr->mfh_cmd_status = 0x0;
	hdr->mfh_flags = 0x0;
	ccb->ccb_state = MFI_CCB_FREE;
	ccb->ccb_xs = NULL;
	ccb->ccb_flags = 0;
	ccb->ccb_done = NULL;
	ccb->ccb_direction = 0;
	ccb->ccb_frame_size = 0;
	ccb->ccb_extra_frames = 0;
	ccb->ccb_sgl = NULL;
	ccb->ccb_data = NULL;
	ccb->ccb_len = 0;
	if (sc->sc_ioptype == MFI_IOP_TBOLT) {
		/* erase tb_request_desc but preserve SMID */
		int index = ccb->ccb_tb_request_desc.header.SMID;
		ccb->ccb_tb_request_desc.words = 0;
		ccb->ccb_tb_request_desc.header.SMID = index;
	}
	s = splbio();
	TAILQ_INSERT_TAIL(&sc->sc_ccb_freeq, ccb, ccb_link);
	splx(s);
}

static int
mfi_destroy_ccb(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	uint32_t		i;

	DNPRINTF(MFI_D_CCB, "%s: mfi_destroy_ccb\n", DEVNAME(sc));


	for (i = 0; (ccb = mfi_get_ccb(sc)) != NULL; i++) {
		/* create a dma map for transfer */
		bus_dmamap_destroy(sc->sc_datadmat, ccb->ccb_dmamap);
	}

	if (i < sc->sc_max_cmds)
		return EBUSY;

	free(sc->sc_ccb, M_DEVBUF);

	return 0;
}

static int
mfi_init_ccb(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	uint32_t		i;
	int			error;
	bus_addr_t		io_req_base_phys;
	uint8_t			*io_req_base;
	int offset;

	DNPRINTF(MFI_D_CCB, "%s: mfi_init_ccb\n", DEVNAME(sc));

	sc->sc_ccb = malloc(sizeof(struct mfi_ccb) * sc->sc_max_cmds,
	    M_DEVBUF, M_WAITOK|M_ZERO);
	if (sc->sc_ioptype == MFI_IOP_TBOLT) {
		/*
		 * The first 256 bytes (SMID 0) is not used.
		 * Don't add to the cmd list.
		 */
		io_req_base = (uint8_t *)MFIMEM_KVA(sc->sc_tbolt_reqmsgpool) +
		    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE;
		io_req_base_phys = MFIMEM_DVA(sc->sc_tbolt_reqmsgpool) +
		    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE;
	} else {
		io_req_base = NULL;	/* XXX: gcc */
		io_req_base_phys = 0;	/* XXX: gcc */
	}

	for (i = 0; i < sc->sc_max_cmds; i++) {
		ccb = &sc->sc_ccb[i];

		ccb->ccb_sc = sc;

		/* select i'th frame */
		ccb->ccb_frame = (union mfi_frame *)
		    ((char*)MFIMEM_KVA(sc->sc_frames) + sc->sc_frames_size * i);
		ccb->ccb_pframe =
		    MFIMEM_DVA(sc->sc_frames) + sc->sc_frames_size * i;
		ccb->ccb_frame->mfr_header.mfh_context = i;

		/* select i'th sense */
		ccb->ccb_sense = (struct mfi_sense *)
		    ((char*)MFIMEM_KVA(sc->sc_sense) + MFI_SENSE_SIZE * i);
		ccb->ccb_psense =
		    (MFIMEM_DVA(sc->sc_sense) + MFI_SENSE_SIZE * i);

		/* create a dma map for transfer */
		error = bus_dmamap_create(sc->sc_datadmat,
		    MAXPHYS, sc->sc_max_sgl, MAXPHYS, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->ccb_dmamap);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "cannot create ccb dmamap (%d)\n", error);
			goto destroy;
		}
		if (sc->sc_ioptype == MFI_IOP_TBOLT) {
			offset = MEGASAS_THUNDERBOLT_NEW_MSG_SIZE * i;
			ccb->ccb_tb_io_request =
			    (struct mfi_mpi2_request_raid_scsi_io *)
			    (io_req_base + offset);
			ccb->ccb_tb_pio_request =
			    io_req_base_phys + offset;
			offset = MEGASAS_MAX_SZ_CHAIN_FRAME * i;
			ccb->ccb_tb_sg_frame =
			    (mpi2_sge_io_union *)(sc->sc_reply_pool_limit +
			    offset);
			ccb->ccb_tb_psg_frame = sc->sc_sg_frame_busaddr +
			    offset;
			/* SMID 0 is reserved. Set SMID/index from 1 */
			ccb->ccb_tb_request_desc.header.SMID = i + 1;
		}

		DNPRINTF(MFI_D_CCB,
		    "ccb(%d): %p frame: %#lx (%#lx) sense: %#lx (%#lx) map: %#lx\n",
		    ccb->ccb_frame->mfr_header.mfh_context, ccb,
		    (u_long)ccb->ccb_frame, (u_long)ccb->ccb_pframe,
		    (u_long)ccb->ccb_sense, (u_long)ccb->ccb_psense,
		    (u_long)ccb->ccb_dmamap);

		/* add ccb to queue */
		mfi_put_ccb(ccb);
	}

	return 0;
destroy:
	/* free dma maps and ccb memory */
	while (i) {
		i--;
		ccb = &sc->sc_ccb[i];
		bus_dmamap_destroy(sc->sc_datadmat, ccb->ccb_dmamap);
	}

	free(sc->sc_ccb, M_DEVBUF);

	return 1;
}

static uint32_t
mfi_read(struct mfi_softc *sc, bus_size_t r)
{
	uint32_t rv;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);

	DNPRINTF(MFI_D_RW, "%s: mr 0x%lx 0x08%x ", DEVNAME(sc), (u_long)r, rv);
	return rv;
}

static void
mfi_write(struct mfi_softc *sc, bus_size_t r, uint32_t v)
{
	DNPRINTF(MFI_D_RW, "%s: mw 0x%lx 0x%08x", DEVNAME(sc), (u_long)r, v);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

static struct mfi_mem *
mfi_allocmem(struct mfi_softc *sc, size_t size)
{
	struct mfi_mem		*mm;
	int			nsegs;

	DNPRINTF(MFI_D_MEM, "%s: mfi_allocmem: %ld\n", DEVNAME(sc),
	    (long)size);

	mm = malloc(sizeof(struct mfi_mem), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (mm == NULL)
		return NULL;

	mm->am_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &mm->am_map) != 0)
		goto amfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &mm->am_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &mm->am_seg, nsegs, size, &mm->am_kva,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, mm->am_map, mm->am_kva, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	DNPRINTF(MFI_D_MEM, "  kva: %p  dva: %p  map: %p\n",
	    mm->am_kva, (void *)mm->am_map->dm_segs[0].ds_addr, mm->am_map);

	memset(mm->am_kva, 0, size);
	return mm;

unmap:
	bus_dmamem_unmap(sc->sc_dmat, mm->am_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &mm->am_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, mm->am_map);
amfree:
	free(mm, M_DEVBUF);

	return NULL;
}

static void
mfi_freemem(struct mfi_softc *sc, struct mfi_mem **mmp)
{
	struct mfi_mem *mm = *mmp;

	if (mm == NULL)
		return;

	*mmp = NULL;

	DNPRINTF(MFI_D_MEM, "%s: mfi_freemem: %p\n", DEVNAME(sc), mm);

	bus_dmamap_unload(sc->sc_dmat, mm->am_map);
	bus_dmamem_unmap(sc->sc_dmat, mm->am_kva, mm->am_size);
	bus_dmamem_free(sc->sc_dmat, &mm->am_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mm->am_map);
	free(mm, M_DEVBUF);
}

static int
mfi_transition_firmware(struct mfi_softc *sc)
{
	uint32_t		fw_state, cur_state;
	int			max_wait, i;

	fw_state = mfi_fw_state(sc) & MFI_STATE_MASK;

	DNPRINTF(MFI_D_CMD, "%s: mfi_transition_firmware: %#x\n", DEVNAME(sc),
	    fw_state);

	while (fw_state != MFI_STATE_READY) {
		DNPRINTF(MFI_D_MISC,
		    "%s: waiting for firmware to become ready\n",
		    DEVNAME(sc));
		cur_state = fw_state;
		switch (fw_state) {
		case MFI_STATE_FAULT:
			aprint_error_dev(sc->sc_dev, "firmware fault\n");
			return 1;
		case MFI_STATE_WAIT_HANDSHAKE:
			if (sc->sc_ioptype == MFI_IOP_SKINNY ||
			    sc->sc_ioptype == MFI_IOP_TBOLT)
				mfi_write(sc, MFI_SKINNY_IDB, MFI_INIT_CLEAR_HANDSHAKE);
			else
				mfi_write(sc, MFI_IDB, MFI_INIT_CLEAR_HANDSHAKE);
			max_wait = 2;
			break;
		case MFI_STATE_OPERATIONAL:
			if (sc->sc_ioptype == MFI_IOP_SKINNY ||
			    sc->sc_ioptype == MFI_IOP_TBOLT)
				mfi_write(sc, MFI_SKINNY_IDB, MFI_INIT_READY);
			else
				mfi_write(sc, MFI_IDB, MFI_INIT_READY);
			max_wait = 10;
			break;
		case MFI_STATE_UNDEFINED:
		case MFI_STATE_BB_INIT:
			max_wait = 2;
			break;
		case MFI_STATE_FW_INIT:
		case MFI_STATE_DEVICE_SCAN:
		case MFI_STATE_FLUSH_CACHE:
			max_wait = 20;
			break;
		case MFI_STATE_BOOT_MESSAGE_PENDING:
			if (sc->sc_ioptype == MFI_IOP_SKINNY ||
			    sc->sc_ioptype == MFI_IOP_TBOLT) {
				mfi_write(sc, MFI_SKINNY_IDB, MFI_INIT_HOTPLUG);
			} else {
				mfi_write(sc, MFI_IDB, MFI_INIT_HOTPLUG);
			}
			max_wait = 180;
			break;
		default:
			aprint_error_dev(sc->sc_dev,
			    "unknown firmware state %d\n", fw_state);
			return 1;
		}
		for (i = 0; i < (max_wait * 10); i++) {
			fw_state = mfi_fw_state(sc) & MFI_STATE_MASK;
			if (fw_state == cur_state)
				DELAY(100000);
			else
				break;
		}
		if (fw_state == cur_state) {
			aprint_error_dev(sc->sc_dev,
			    "firmware stuck in state %#x\n", fw_state);
			return 1;
		}
	}

	return 0;
}

static int
mfi_initialize_firmware(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	struct mfi_init_frame	*init;
	struct mfi_init_qinfo	*qinfo;

	DNPRINTF(MFI_D_MISC, "%s: mfi_initialize_firmware\n", DEVNAME(sc));

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return 1;

	init = &ccb->ccb_frame->mfr_init;
	qinfo = (struct mfi_init_qinfo *)((uint8_t *)init + MFI_FRAME_SIZE);

	memset(qinfo, 0, sizeof *qinfo);
	qinfo->miq_rq_entries = sc->sc_max_cmds + 1;
	qinfo->miq_rq_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_reply_q));
	qinfo->miq_pi_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_producer));
	qinfo->miq_ci_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_consumer));

	init->mif_header.mfh_cmd = MFI_CMD_INIT;
	init->mif_header.mfh_data_len = sizeof *qinfo;
	init->mif_qinfo_new_addr_lo = htole32(ccb->ccb_pframe + MFI_FRAME_SIZE);

	DNPRINTF(MFI_D_MISC, "%s: entries: %#x rq: %#x pi: %#x ci: %#x\n",
	    DEVNAME(sc),
	    qinfo->miq_rq_entries, qinfo->miq_rq_addr_lo,
	    qinfo->miq_pi_addr_lo, qinfo->miq_ci_addr_lo);

	if (mfi_poll(ccb)) {
		aprint_error_dev(sc->sc_dev,
		    "mfi_initialize_firmware failed\n");
		return 1;
	}

	mfi_put_ccb(ccb);

	return 0;
}

static int
mfi_get_info(struct mfi_softc *sc)
{
#ifdef MFI_DEBUG
	int i;
#endif
	DNPRINTF(MFI_D_MISC, "%s: mfi_get_info\n", DEVNAME(sc));

	if (mfi_mgmt_internal(sc, MR_DCMD_CTRL_GET_INFO, MFI_DATA_IN,
	    sizeof(sc->sc_info), &sc->sc_info, NULL, cold ? true : false))
		return 1;

#ifdef MFI_DEBUG

	for (i = 0; i < sc->sc_info.mci_image_component_count; i++) {
		printf("%s: active FW %s Version %s date %s time %s\n",
		    DEVNAME(sc),
		    sc->sc_info.mci_image_component[i].mic_name,
		    sc->sc_info.mci_image_component[i].mic_version,
		    sc->sc_info.mci_image_component[i].mic_build_date,
		    sc->sc_info.mci_image_component[i].mic_build_time);
	}

	for (i = 0; i < sc->sc_info.mci_pending_image_component_count; i++) {
		printf("%s: pending FW %s Version %s date %s time %s\n",
		    DEVNAME(sc),
		    sc->sc_info.mci_pending_image_component[i].mic_name,
		    sc->sc_info.mci_pending_image_component[i].mic_version,
		    sc->sc_info.mci_pending_image_component[i].mic_build_date,
		    sc->sc_info.mci_pending_image_component[i].mic_build_time);
	}

	printf("%s: max_arms %d max_spans %d max_arrs %d max_lds %d name %s\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_max_arms,
	    sc->sc_info.mci_max_spans,
	    sc->sc_info.mci_max_arrays,
	    sc->sc_info.mci_max_lds,
	    sc->sc_info.mci_product_name);

	printf("%s: serial %s present %#x fw time %d max_cmds %d max_sg %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_serial_number,
	    sc->sc_info.mci_hw_present,
	    sc->sc_info.mci_current_fw_time,
	    sc->sc_info.mci_max_cmds,
	    sc->sc_info.mci_max_sg_elements);

	printf("%s: max_rq %d lds_pres %d lds_deg %d lds_off %d pd_pres %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_max_request_size,
	    sc->sc_info.mci_lds_present,
	    sc->sc_info.mci_lds_degraded,
	    sc->sc_info.mci_lds_offline,
	    sc->sc_info.mci_pd_present);

	printf("%s: pd_dsk_prs %d pd_dsk_pred_fail %d pd_dsk_fail %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_pd_disks_present,
	    sc->sc_info.mci_pd_disks_pred_failure,
	    sc->sc_info.mci_pd_disks_failed);

	printf("%s: nvram %d mem %d flash %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_nvram_size,
	    sc->sc_info.mci_memory_size,
	    sc->sc_info.mci_flash_size);

	printf("%s: ram_cor %d ram_uncor %d clus_all %d clus_act %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_ram_correctable_errors,
	    sc->sc_info.mci_ram_uncorrectable_errors,
	    sc->sc_info.mci_cluster_allowed,
	    sc->sc_info.mci_cluster_active);

	printf("%s: max_strps_io %d raid_lvl %#x adapt_ops %#x ld_ops %#x\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_max_strips_per_io,
	    sc->sc_info.mci_raid_levels,
	    sc->sc_info.mci_adapter_ops,
	    sc->sc_info.mci_ld_ops);

	printf("%s: strp_sz_min %d strp_sz_max %d pd_ops %#x pd_mix %#x\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_stripe_sz_ops.min,
	    sc->sc_info.mci_stripe_sz_ops.max,
	    sc->sc_info.mci_pd_ops,
	    sc->sc_info.mci_pd_mix_support);

	printf("%s: ecc_bucket %d pckg_prop %s\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_ecc_bucket_count,
	    sc->sc_info.mci_package_version);

	printf("%s: sq_nm %d prd_fail_poll %d intr_thrtl %d intr_thrtl_to %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_seq_num,
	    sc->sc_info.mci_properties.mcp_pred_fail_poll_interval,
	    sc->sc_info.mci_properties.mcp_intr_throttle_cnt,
	    sc->sc_info.mci_properties.mcp_intr_throttle_timeout);

	printf("%s: rbld_rate %d patr_rd_rate %d bgi_rate %d cc_rate %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_rebuild_rate,
	    sc->sc_info.mci_properties.mcp_patrol_read_rate,
	    sc->sc_info.mci_properties.mcp_bgi_rate,
	    sc->sc_info.mci_properties.mcp_cc_rate);

	printf("%s: rc_rate %d ch_flsh %d spin_cnt %d spin_dly %d clus_en %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_recon_rate,
	    sc->sc_info.mci_properties.mcp_cache_flush_interval,
	    sc->sc_info.mci_properties.mcp_spinup_drv_cnt,
	    sc->sc_info.mci_properties.mcp_spinup_delay,
	    sc->sc_info.mci_properties.mcp_cluster_enable);

	printf("%s: coerc %d alarm %d dis_auto_rbld %d dis_bat_wrn %d ecc %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_coercion_mode,
	    sc->sc_info.mci_properties.mcp_alarm_enable,
	    sc->sc_info.mci_properties.mcp_disable_auto_rebuild,
	    sc->sc_info.mci_properties.mcp_disable_battery_warn,
	    sc->sc_info.mci_properties.mcp_ecc_bucket_size);

	printf("%s: ecc_leak %d rest_hs %d exp_encl_dev %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_ecc_bucket_leak_rate,
	    sc->sc_info.mci_properties.mcp_restore_hotspare_on_insertion,
	    sc->sc_info.mci_properties.mcp_expose_encl_devices);

	printf("%s: vendor %#x device %#x subvendor %#x subdevice %#x\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_pci.mip_vendor,
	    sc->sc_info.mci_pci.mip_device,
	    sc->sc_info.mci_pci.mip_subvendor,
	    sc->sc_info.mci_pci.mip_subdevice);

	printf("%s: type %#x port_count %d port_addr ",
	    DEVNAME(sc),
	    sc->sc_info.mci_host.mih_type,
	    sc->sc_info.mci_host.mih_port_count);

	for (i = 0; i < 8; i++)
		printf("%.0" PRIx64 " ", sc->sc_info.mci_host.mih_port_addr[i]);
	printf("\n");

	printf("%s: type %.x port_count %d port_addr ",
	    DEVNAME(sc),
	    sc->sc_info.mci_device.mid_type,
	    sc->sc_info.mci_device.mid_port_count);

	for (i = 0; i < 8; i++) {
		printf("%.0" PRIx64 " ",
		    sc->sc_info.mci_device.mid_port_addr[i]);
	}
	printf("\n");
#endif /* MFI_DEBUG */

	return 0;
}

static int
mfi_get_bbu(struct mfi_softc *sc, struct mfi_bbu_status *stat)
{
	DNPRINTF(MFI_D_MISC, "%s: mfi_get_bbu\n", DEVNAME(sc));

	if (mfi_mgmt_internal(sc, MR_DCMD_BBU_GET_STATUS, MFI_DATA_IN,
	    sizeof(*stat), stat, NULL, cold ? true : false))
		return MFI_BBU_UNKNOWN;
#ifdef MFI_DEBUG
	printf("bbu type %d, voltage %d, current %d, temperature %d, "
	    "status 0x%x\n", stat->battery_type, stat->voltage, stat->current,
	    stat->temperature, stat->fw_status);
	printf("details: ");
	switch(stat->battery_type) {
	case MFI_BBU_TYPE_IBBU:
		printf("guage %d relative charge %d charger state %d "
		    "charger ctrl %d\n", stat->detail.ibbu.gas_guage_status,
		    stat->detail.ibbu.relative_charge ,
		    stat->detail.ibbu.charger_system_state ,
		    stat->detail.ibbu.charger_system_ctrl);
		printf("\tcurrent %d abs charge %d max error %d\n",
		    stat->detail.ibbu.charging_current ,
		    stat->detail.ibbu.absolute_charge ,
		    stat->detail.ibbu.max_error);
		break;
	case MFI_BBU_TYPE_BBU:
		printf("guage %d relative charge %d charger state %d\n",
		    stat->detail.ibbu.gas_guage_status,
		    stat->detail.bbu.relative_charge ,
		    stat->detail.bbu.charger_status );
		printf("\trem capacity %d fyll capacity %d SOH %d\n",
		    stat->detail.bbu.remaining_capacity ,
		    stat->detail.bbu.full_charge_capacity ,
		    stat->detail.bbu.is_SOH_good);
	default:
		printf("\n");
	}
#endif
	switch(stat->battery_type) {
	case MFI_BBU_TYPE_BBU:
		return (stat->detail.bbu.is_SOH_good ? 
		    MFI_BBU_GOOD : MFI_BBU_BAD);
	case MFI_BBU_TYPE_NONE:
		return MFI_BBU_UNKNOWN;
	default:
		if (stat->fw_status &
		    (MFI_BBU_STATE_PACK_MISSING |
		     MFI_BBU_STATE_VOLTAGE_LOW |
		     MFI_BBU_STATE_TEMPERATURE_HIGH |
		     MFI_BBU_STATE_LEARN_CYC_FAIL |
		     MFI_BBU_STATE_LEARN_CYC_TIMEOUT |
		     MFI_BBU_STATE_I2C_ERR_DETECT))
			return MFI_BBU_BAD;
		return MFI_BBU_GOOD;
	}
}

static void
mfiminphys(struct buf *bp)
{
	DNPRINTF(MFI_D_MISC, "mfiminphys: %d\n", bp->b_bcount);

	/* XXX currently using MFI_MAXFER = MAXPHYS */
	if (bp->b_bcount > MFI_MAXFER)
		bp->b_bcount = MFI_MAXFER;
	minphys(bp);
}

int
mfi_rescan(device_t self, const char *ifattr, const int *locators)
{
	struct mfi_softc *sc = device_private(self);

	if (sc->sc_child != NULL)
		return 0;

	sc->sc_child = config_found_sm_loc(self, ifattr, locators, &sc->sc_chan,
	    scsiprint, NULL);

	return 0;
}

void
mfi_childdetached(device_t self, device_t child)
{
	struct mfi_softc *sc = device_private(self);

	KASSERT(self == sc->sc_dev);
	KASSERT(child == sc->sc_child);

	if (child == sc->sc_child)
		sc->sc_child = NULL;
}

int
mfi_detach(struct mfi_softc *sc, int flags)
{
	int			error;

	DNPRINTF(MFI_D_MISC, "%s: mfi_detach\n", DEVNAME(sc));

	if ((error = config_detach_children(sc->sc_dev, flags)) != 0)
		return error;

#if NBIO > 0
	mfi_destroy_sensors(sc);
	bio_unregister(sc->sc_dev);
#endif /* NBIO > 0 */

	mfi_intr_disable(sc);
	mfi_shutdown(sc->sc_dev, 0);

	if (sc->sc_ioptype == MFI_IOP_TBOLT) {
		workqueue_destroy(sc->sc_ldsync_wq);
		mfi_put_ccb(sc->sc_ldsync_ccb);
		mfi_freemem(sc, &sc->sc_tbolt_reqmsgpool);
		mfi_freemem(sc, &sc->sc_tbolt_ioc_init);
		mfi_freemem(sc, &sc->sc_tbolt_verbuf);
	}

	if ((error = mfi_destroy_ccb(sc)) != 0)
		return error;

	mfi_freemem(sc, &sc->sc_sense);

	mfi_freemem(sc, &sc->sc_frames);

	mfi_freemem(sc, &sc->sc_pcq);

	return 0;
}

static bool
mfi_shutdown(device_t dev, int how)
{
	struct mfi_softc	*sc = device_private(dev);
	uint8_t			mbox[MFI_MBOX_SIZE];
	int s = splbio();
	DNPRINTF(MFI_D_MISC, "%s: mfi_shutdown\n", DEVNAME(sc));
	if (sc->sc_running) {
		mbox[0] = MR_FLUSH_CTRL_CACHE | MR_FLUSH_DISK_CACHE;
		if (mfi_mgmt_internal(sc, MR_DCMD_CTRL_CACHE_FLUSH,
		    MFI_DATA_NONE, 0, NULL, mbox, true)) {
			aprint_error_dev(dev, "shutdown: cache flush failed\n");
			goto fail;
		}

		mbox[0] = 0;
		if (mfi_mgmt_internal(sc, MR_DCMD_CTRL_SHUTDOWN,
		    MFI_DATA_NONE, 0, NULL, mbox, true)) {
			aprint_error_dev(dev, "shutdown: "
			    "firmware shutdown failed\n");
			goto fail;
		}
		sc->sc_running = false;
	}
	splx(s);
	return true;
fail:
	splx(s);
	return false;
}

static bool
mfi_suspend(device_t dev, const pmf_qual_t *q)
{
	/* XXX to be implemented */
	return false;
}

static bool
mfi_resume(device_t dev, const pmf_qual_t *q)
{
	/* XXX to be implemented */
	return false;
}

int
mfi_attach(struct mfi_softc *sc, enum mfi_iop iop)
{
	struct scsipi_adapter *adapt = &sc->sc_adapt;
	struct scsipi_channel *chan = &sc->sc_chan;
	uint32_t		status, frames, max_sgl;
	int			i;

	DNPRINTF(MFI_D_MISC, "%s: mfi_attach\n", DEVNAME(sc));

	sc->sc_ioptype = iop;

	switch (iop) {
	case MFI_IOP_XSCALE:
		sc->sc_iop = &mfi_iop_xscale;
		break;
	case MFI_IOP_PPC:
		sc->sc_iop = &mfi_iop_ppc;
		break;
	case MFI_IOP_GEN2:
		sc->sc_iop = &mfi_iop_gen2;
		break;
	case MFI_IOP_SKINNY:
		sc->sc_iop = &mfi_iop_skinny;
		break;
	case MFI_IOP_TBOLT:
		sc->sc_iop = &mfi_iop_tbolt;
		break;
	default:
		 panic("%s: unknown iop %d", DEVNAME(sc), iop);
	}

	if (mfi_transition_firmware(sc))
		return 1;

	TAILQ_INIT(&sc->sc_ccb_freeq);

	status = mfi_fw_state(sc);
	sc->sc_max_cmds = status & MFI_STATE_MAXCMD_MASK;
	max_sgl = (status & MFI_STATE_MAXSGL_MASK) >> 16;
	if (sc->sc_ioptype == MFI_IOP_TBOLT) {
		sc->sc_max_sgl = min(max_sgl, (128 * 1024) / PAGE_SIZE + 1);
		sc->sc_sgl_size = sizeof(struct mfi_sg_ieee);
	} else if (sc->sc_64bit_dma) {
		sc->sc_max_sgl = min(max_sgl, (128 * 1024) / PAGE_SIZE + 1);
		sc->sc_sgl_size = sizeof(struct mfi_sg64);
	} else {
		sc->sc_max_sgl = max_sgl;
		sc->sc_sgl_size = sizeof(struct mfi_sg32);
	}
	DNPRINTF(MFI_D_MISC, "%s: max commands: %u, max sgl: %u\n",
	    DEVNAME(sc), sc->sc_max_cmds, sc->sc_max_sgl);

	if (sc->sc_ioptype == MFI_IOP_TBOLT) {
		uint32_t tb_mem_size;
		/* for Alignment */
		tb_mem_size = MEGASAS_THUNDERBOLT_MSG_ALLIGNMENT;

		tb_mem_size +=
		    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE * (sc->sc_max_cmds + 1);
		sc->sc_reply_pool_size =
		    ((sc->sc_max_cmds + 1 + 15) / 16) * 16;
		tb_mem_size +=
		    MEGASAS_THUNDERBOLT_REPLY_SIZE * sc->sc_reply_pool_size;

		/* this is for SGL's */
		tb_mem_size += MEGASAS_MAX_SZ_CHAIN_FRAME * sc->sc_max_cmds;
		sc->sc_tbolt_reqmsgpool = mfi_allocmem(sc, tb_mem_size);
		if (sc->sc_tbolt_reqmsgpool == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "unable to allocate thunderbolt "
			    "request message pool\n");
			goto nopcq;
		}
		if (mfi_tbolt_init_desc_pool(sc)) {
			aprint_error_dev(sc->sc_dev,
			    "Thunderbolt pool preparation error\n");
			goto nopcq;
		}

		/*
		 * Allocate DMA memory mapping for MPI2 IOC Init descriptor,
		 * we are taking it diffrent from what we have allocated for
		 * Request and reply descriptors to avoid confusion later
		 */
		sc->sc_tbolt_ioc_init = mfi_allocmem(sc,
		    sizeof(struct mpi2_ioc_init_request));
		if (sc->sc_tbolt_ioc_init == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "unable to allocate thunderbolt IOC init memory");
			goto nopcq;
		}

		sc->sc_tbolt_verbuf = mfi_allocmem(sc,
		    MEGASAS_MAX_NAME*sizeof(bus_addr_t));
		if (sc->sc_tbolt_verbuf == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "unable to allocate thunderbolt version buffer\n");
			goto nopcq;
		}

	}
	/* consumer/producer and reply queue memory */
	sc->sc_pcq = mfi_allocmem(sc, (sizeof(uint32_t) * sc->sc_max_cmds) +
	    sizeof(struct mfi_prod_cons));
	if (sc->sc_pcq == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate reply queue memory\n");
		goto nopcq;
	}
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_pcq), 0,
	    sizeof(uint32_t) * sc->sc_max_cmds + sizeof(struct mfi_prod_cons),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* frame memory */
	frames = (sc->sc_sgl_size * sc->sc_max_sgl + MFI_FRAME_SIZE - 1) /
	    MFI_FRAME_SIZE + 1;
	sc->sc_frames_size = frames * MFI_FRAME_SIZE;
	sc->sc_frames = mfi_allocmem(sc, sc->sc_frames_size * sc->sc_max_cmds);
	if (sc->sc_frames == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate frame memory\n");
		goto noframe;
	}
	/* XXX hack, fix this */
	if (MFIMEM_DVA(sc->sc_frames) & 0x3f) {
		aprint_error_dev(sc->sc_dev,
		    "improper frame alignment (%#llx) FIXME\n",
		    (long long int)MFIMEM_DVA(sc->sc_frames));
		goto noframe;
	}

	/* sense memory */
	sc->sc_sense = mfi_allocmem(sc, sc->sc_max_cmds * MFI_SENSE_SIZE);
	if (sc->sc_sense == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate sense memory\n");
		goto nosense;
	}

	/* now that we have all memory bits go initialize ccbs */
	if (mfi_init_ccb(sc)) {
		aprint_error_dev(sc->sc_dev, "could not init ccb list\n");
		goto noinit;
	}

	/* kickstart firmware with all addresses and pointers */
	if (sc->sc_ioptype == MFI_IOP_TBOLT) {
		if (mfi_tbolt_init_MFI_queue(sc)) {
			aprint_error_dev(sc->sc_dev,
			    "could not initialize firmware\n");
			goto noinit;
		}
	} else {
		if (mfi_initialize_firmware(sc)) {
			aprint_error_dev(sc->sc_dev,
			    "could not initialize firmware\n");
			goto noinit;
		}
	}
	sc->sc_running = true;

	if (mfi_get_info(sc)) {
		aprint_error_dev(sc->sc_dev,
		    "could not retrieve controller information\n");
		goto noinit;
	}
	aprint_normal_dev(sc->sc_dev,
	    "%s version %s\n",
	    sc->sc_info.mci_product_name,
	    sc->sc_info.mci_package_version);


	aprint_normal_dev(sc->sc_dev, "logical drives %d, %dMB RAM, ",
	    sc->sc_info.mci_lds_present,
	    sc->sc_info.mci_memory_size);
	sc->sc_bbuok = false;
	if (sc->sc_info.mci_hw_present & MFI_INFO_HW_BBU) {
		struct mfi_bbu_status	bbu_stat;
		int mfi_bbu_status = mfi_get_bbu(sc, &bbu_stat);
		aprint_normal("BBU type ");
		switch (bbu_stat.battery_type) {
		case MFI_BBU_TYPE_BBU:
			aprint_normal("BBU");
			break;
		case MFI_BBU_TYPE_IBBU:
			aprint_normal("IBBU");
			break;
		default:
			aprint_normal("unknown type %d", bbu_stat.battery_type);
		}
		aprint_normal(", status ");
		switch(mfi_bbu_status) {
		case MFI_BBU_GOOD:
			aprint_normal("good\n");
			sc->sc_bbuok = true;
			break;
		case MFI_BBU_BAD:
			aprint_normal("bad\n");
			break;
		case MFI_BBU_UNKNOWN:
			aprint_normal("unknown\n");
			break;
		default:
			panic("mfi_bbu_status");
		}
	} else {
		aprint_normal("BBU not present\n");
	}

	sc->sc_ld_cnt = sc->sc_info.mci_lds_present;
	sc->sc_max_ld = sc->sc_ld_cnt;
	for (i = 0; i < sc->sc_ld_cnt; i++)
		sc->sc_ld[i].ld_present = 1;

	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	/* keep a few commands for management */
	if (sc->sc_max_cmds > 4)
		adapt->adapt_openings = sc->sc_max_cmds - 4;
	else
		adapt->adapt_openings = sc->sc_max_cmds;
	adapt->adapt_max_periph = adapt->adapt_openings;
	adapt->adapt_request = mfi_scsipi_request;
	adapt->adapt_minphys = mfiminphys;

	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_sas_bustype;
	chan->chan_channel = 0;
	chan->chan_flags = 0;
	chan->chan_nluns = 8;
	chan->chan_ntargets = MFI_MAX_LD;
	chan->chan_id = MFI_MAX_LD;

	mfi_rescan(sc->sc_dev, "scsi", NULL);

	/* enable interrupts */
	mfi_intr_enable(sc);

#if NBIO > 0
	if (bio_register(sc->sc_dev, mfi_ioctl) != 0)
		panic("%s: controller registration failed", DEVNAME(sc));
	if (mfi_create_sensors(sc) != 0)
		aprint_error_dev(sc->sc_dev, "unable to create sensors\n");
#endif /* NBIO > 0 */
	if (!pmf_device_register1(sc->sc_dev, mfi_suspend, mfi_resume,
	    mfi_shutdown)) {
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");
	}

	return 0;
noinit:
	mfi_freemem(sc, &sc->sc_sense);
nosense:
	mfi_freemem(sc, &sc->sc_frames);
noframe:
	mfi_freemem(sc, &sc->sc_pcq);
nopcq:
	if (sc->sc_ioptype == MFI_IOP_TBOLT) {
		if (sc->sc_tbolt_reqmsgpool)
			mfi_freemem(sc, &sc->sc_tbolt_reqmsgpool);
		if (sc->sc_tbolt_verbuf)
			mfi_freemem(sc, &sc->sc_tbolt_verbuf);
	}
	return 1;
}

static int
mfi_poll(struct mfi_ccb *ccb)
{
	struct mfi_softc *sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr;
	int			to = 0;
	int			rv = 0;

	DNPRINTF(MFI_D_CMD, "%s: mfi_poll\n", DEVNAME(sc));

	hdr = &ccb->ccb_frame->mfr_header;
	hdr->mfh_cmd_status = 0xff;
	if (!sc->sc_MFA_enabled)
		hdr->mfh_flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;

	/* no callback, caller is supposed to do the cleanup */
	ccb->ccb_done = NULL;

	mfi_post(sc, ccb);
	if (sc->sc_MFA_enabled) {
		/*
		 * depending on the command type, result may be posted
		 * to *hdr, or not. In addition it seems there's
		 * no way to avoid posting the SMID to the reply queue.
		 * So pool using the interrupt routine.
		 */
		 while (ccb->ccb_state != MFI_CCB_DONE) {
			delay(1000);
			if (to++ > 5000) { /* XXX 5 seconds busywait sucks */
				rv = 1;
				break;
			}
			mfi_tbolt_intrh(sc);
		 }
	} else {
		bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_frames),
		    ccb->ccb_pframe - MFIMEM_DVA(sc->sc_frames),
		    sc->sc_frames_size, BUS_DMASYNC_POSTREAD);

		while (hdr->mfh_cmd_status == 0xff) {
			delay(1000);
			if (to++ > 5000) { /* XXX 5 seconds busywait sucks */
				rv = 1;
				break;
			}
			bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_frames),
			    ccb->ccb_pframe - MFIMEM_DVA(sc->sc_frames),
			    sc->sc_frames_size, BUS_DMASYNC_POSTREAD);
		}
	}
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_frames),
	    ccb->ccb_pframe - MFIMEM_DVA(sc->sc_frames),
	    sc->sc_frames_size, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	if (ccb->ccb_data != NULL) {
		DNPRINTF(MFI_D_INTR, "%s: mfi_mgmt_done sync\n",
		    DEVNAME(sc));
		bus_dmamap_sync(sc->sc_datadmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize,
		    (ccb->ccb_direction & MFI_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_datadmat, ccb->ccb_dmamap);
	}

	if (rv != 0) {
		aprint_error_dev(sc->sc_dev, "timeout on ccb %d\n",
		    hdr->mfh_context);
		ccb->ccb_flags |= MFI_CCB_F_ERR;
		return 1;
	}

	return 0;
}

int
mfi_intr(void *arg)
{
	struct mfi_softc	*sc = arg;
	struct mfi_prod_cons	*pcq;
	struct mfi_ccb		*ccb;
	uint32_t		producer, consumer, ctx;
	int			claimed = 0;

	if (!mfi_my_intr(sc))
		return 0;

	pcq = MFIMEM_KVA(sc->sc_pcq);

	DNPRINTF(MFI_D_INTR, "%s: mfi_intr %#lx %#lx\n", DEVNAME(sc),
	    (u_long)sc, (u_long)pcq);

	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_pcq), 0,
	    sizeof(uint32_t) * sc->sc_max_cmds + sizeof(struct mfi_prod_cons),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	producer = pcq->mpc_producer;
	consumer = pcq->mpc_consumer;

	while (consumer != producer) {
		DNPRINTF(MFI_D_INTR, "%s: mfi_intr pi %#x ci %#x\n",
		    DEVNAME(sc), producer, consumer);

		ctx = pcq->mpc_reply_q[consumer];
		pcq->mpc_reply_q[consumer] = MFI_INVALID_CTX;
		if (ctx == MFI_INVALID_CTX)
			aprint_error_dev(sc->sc_dev,
			    "invalid context, p: %d c: %d\n",
			    producer, consumer);
		else {
			/* XXX remove from queue and call scsi_done */
			ccb = &sc->sc_ccb[ctx];
			DNPRINTF(MFI_D_INTR, "%s: mfi_intr context %#x\n",
			    DEVNAME(sc), ctx);
			bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_frames),
			    ccb->ccb_pframe - MFIMEM_DVA(sc->sc_frames),
			    sc->sc_frames_size,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			ccb->ccb_done(ccb);

			claimed = 1;
		}
		consumer++;
		if (consumer == (sc->sc_max_cmds + 1))
			consumer = 0;
	}

	pcq->mpc_consumer = consumer;
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_pcq), 0,
	    sizeof(uint32_t) * sc->sc_max_cmds + sizeof(struct mfi_prod_cons),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return claimed;
}

static int
mfi_scsi_ld_io(struct mfi_ccb *ccb, struct scsipi_xfer *xs, uint64_t blockno,
    uint32_t blockcnt)
{
	struct scsipi_periph *periph = xs->xs_periph;
	struct mfi_io_frame   *io;

	DNPRINTF(MFI_D_CMD, "%s: mfi_scsi_ld_io: %d\n",
	    device_xname(periph->periph_channel->chan_adapter->adapt_dev),
	    periph->periph_target);

	if (!xs->data)
		return 1;

	io = &ccb->ccb_frame->mfr_io;
	if (xs->xs_control & XS_CTL_DATA_IN) {
		io->mif_header.mfh_cmd = MFI_CMD_LD_READ;
		ccb->ccb_direction = MFI_DATA_IN;
	} else {
		io->mif_header.mfh_cmd = MFI_CMD_LD_WRITE;
		ccb->ccb_direction = MFI_DATA_OUT;
	}
	io->mif_header.mfh_target_id = periph->periph_target;
	io->mif_header.mfh_timeout = 0;
	io->mif_header.mfh_flags = 0;
	io->mif_header.mfh_sense_len = MFI_SENSE_SIZE;
	io->mif_header.mfh_data_len= blockcnt;
	io->mif_lba_hi = (blockno >> 32);
	io->mif_lba_lo = (blockno & 0xffffffff);
	io->mif_sense_addr_lo = htole32(ccb->ccb_psense);
	io->mif_sense_addr_hi = 0;

	ccb->ccb_done = mfi_scsi_ld_done;
	ccb->ccb_xs = xs;
	ccb->ccb_frame_size = MFI_IO_FRAME_SIZE;
	ccb->ccb_sgl = &io->mif_sgl;
	ccb->ccb_data = xs->data;
	ccb->ccb_len = xs->datalen;

	if (mfi_create_sgl(ccb, (xs->xs_control & XS_CTL_NOSLEEP) ?
	    BUS_DMA_NOWAIT : BUS_DMA_WAITOK))
		return 1;

	return 0;
}

static void
mfi_scsi_ld_done(struct mfi_ccb *ccb)
{
	struct mfi_frame_header	*hdr = &ccb->ccb_frame->mfr_header;
	mfi_scsi_xs_done(ccb, hdr->mfh_cmd_status, hdr->mfh_scsi_status);
}

static void
mfi_scsi_xs_done(struct mfi_ccb *ccb, int status, int scsi_status)
{
	struct scsipi_xfer	*xs = ccb->ccb_xs;
	struct mfi_softc	*sc = ccb->ccb_sc;

	DNPRINTF(MFI_D_INTR, "%s: mfi_scsi_xs_done %#lx %#lx\n",
	    DEVNAME(sc), (u_long)ccb, (u_long)ccb->ccb_frame);

	if (xs->data != NULL) {
		DNPRINTF(MFI_D_INTR, "%s: mfi_scsi_xs_done sync\n",
		    DEVNAME(sc));
		bus_dmamap_sync(sc->sc_datadmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_datadmat, ccb->ccb_dmamap);
	}

	if (status != MFI_STAT_OK) {
		xs->error = XS_DRIVER_STUFFUP;
		DNPRINTF(MFI_D_INTR, "%s: mfi_scsi_xs_done stuffup %#x\n",
		    DEVNAME(sc), status);

		if (scsi_status != 0) {
			bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_sense),
			    ccb->ccb_psense - MFIMEM_DVA(sc->sc_sense),
			    MFI_SENSE_SIZE, BUS_DMASYNC_POSTREAD);
			DNPRINTF(MFI_D_INTR,
			    "%s: mfi_scsi_xs_done sense %#x %lx %lx\n",
			    DEVNAME(sc), scsi_status,
			    (u_long)&xs->sense, (u_long)ccb->ccb_sense);
			memset(&xs->sense, 0, sizeof(xs->sense));
			memcpy(&xs->sense, ccb->ccb_sense,
			    sizeof(struct scsi_sense_data));
			xs->error = XS_SENSE;
		}
	} else {
		xs->error = XS_NOERROR;
		xs->status = SCSI_OK;
		xs->resid = 0;
	}

	mfi_put_ccb(ccb);
	scsipi_done(xs);
}

static int
mfi_scsi_ld(struct mfi_ccb *ccb, struct scsipi_xfer *xs)
{
	struct mfi_pass_frame	*pf;
	struct scsipi_periph *periph = xs->xs_periph;

	DNPRINTF(MFI_D_CMD, "%s: mfi_scsi_ld: %d\n",
	    device_xname(periph->periph_channel->chan_adapter->adapt_dev),
	    periph->periph_target);

	pf = &ccb->ccb_frame->mfr_pass;
	pf->mpf_header.mfh_cmd = MFI_CMD_LD_SCSI_IO;
	pf->mpf_header.mfh_target_id = periph->periph_target;
	pf->mpf_header.mfh_lun_id = 0;
	pf->mpf_header.mfh_cdb_len = xs->cmdlen;
	pf->mpf_header.mfh_timeout = 0;
	pf->mpf_header.mfh_data_len= xs->datalen; /* XXX */
	pf->mpf_header.mfh_sense_len = MFI_SENSE_SIZE;

	pf->mpf_sense_addr_hi = 0;
	pf->mpf_sense_addr_lo = htole32(ccb->ccb_psense);

	memset(pf->mpf_cdb, 0, 16);
	memcpy(pf->mpf_cdb, &xs->cmdstore, xs->cmdlen);

	ccb->ccb_done = mfi_scsi_ld_done;
	ccb->ccb_xs = xs;
	ccb->ccb_frame_size = MFI_PASS_FRAME_SIZE;
	ccb->ccb_sgl = &pf->mpf_sgl;

	if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT))
		ccb->ccb_direction = (xs->xs_control & XS_CTL_DATA_IN) ?
		    MFI_DATA_IN : MFI_DATA_OUT;
	else
		ccb->ccb_direction = MFI_DATA_NONE;

	if (xs->data) {
		ccb->ccb_data = xs->data;
		ccb->ccb_len = xs->datalen;

		if (mfi_create_sgl(ccb, (xs->xs_control & XS_CTL_NOSLEEP) ?
		    BUS_DMA_NOWAIT : BUS_DMA_WAITOK))
			return 1;
	}

	return 0;
}

static void
mfi_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_periph	*periph;
	struct scsipi_xfer	*xs;
	struct scsipi_adapter	*adapt = chan->chan_adapter;
	struct mfi_softc	*sc = device_private(adapt->adapt_dev);
	struct mfi_ccb		*ccb;
	struct scsi_rw_6	*rw;
	struct scsipi_rw_10	*rwb;
	struct scsipi_rw_12	*rw12;
	struct scsipi_rw_16	*rw16;
	uint64_t		blockno;
	uint32_t		blockcnt;
	uint8_t			target;
	uint8_t			mbox[MFI_MBOX_SIZE];
	int			s;

	switch (req) {
	case ADAPTER_REQ_GROW_RESOURCES:
		/* Not supported. */
		return;
	case ADAPTER_REQ_SET_XFER_MODE:
	{
		struct scsipi_xfer_mode *xm = arg;
		xm->xm_mode = PERIPH_CAP_TQING;
		xm->xm_period = 0;
		xm->xm_offset = 0;
		scsipi_async_event(&sc->sc_chan, ASYNC_EVENT_XFER_MODE, xm);
		return;
	}
	case ADAPTER_REQ_RUN_XFER:
		break;
	}

	xs = arg;

	periph = xs->xs_periph;
	target = periph->periph_target;

	DNPRINTF(MFI_D_CMD, "%s: mfi_scsipi_request req %d opcode: %#x "
	    "target %d lun %d\n", DEVNAME(sc), req, xs->cmd->opcode,
	    periph->periph_target, periph->periph_lun);

	s = splbio();
	if (target >= MFI_MAX_LD || !sc->sc_ld[target].ld_present ||
	    periph->periph_lun != 0) {
		DNPRINTF(MFI_D_CMD, "%s: invalid target %d\n",
		    DEVNAME(sc), target);
		xs->error = XS_SELTIMEOUT;
		scsipi_done(xs);
		splx(s);
		return;
	}
	if ((xs->cmd->opcode == SCSI_SYNCHRONIZE_CACHE_10 ||
	    xs->cmd->opcode == SCSI_SYNCHRONIZE_CACHE_16) && sc->sc_bbuok) {
		/* the cache is stable storage, don't flush */
		xs->error = XS_NOERROR;
		xs->status = SCSI_OK;
		xs->resid = 0;
		scsipi_done(xs);
		splx(s);
		return;
	}

	if ((ccb = mfi_get_ccb(sc)) == NULL) {
		DNPRINTF(MFI_D_CMD, "%s: mfi_scsipi_request no ccb\n", DEVNAME(sc));
		xs->error = XS_RESOURCE_SHORTAGE;
		scsipi_done(xs);
		splx(s);
		return;
	}

	switch (xs->cmd->opcode) {
	/* IO path */
	case READ_16:
	case WRITE_16:
		rw16 = (struct scsipi_rw_16 *)xs->cmd;
		blockno = _8btol(rw16->addr);
		blockcnt = _4btol(rw16->length);
		if (sc->sc_iop->mio_ld_io(ccb, xs, blockno, blockcnt)) {
			goto stuffup;
		}
		break;

	case READ_12:
	case WRITE_12:
		rw12 = (struct scsipi_rw_12 *)xs->cmd;
		blockno = _4btol(rw12->addr);
		blockcnt = _4btol(rw12->length);
		if (sc->sc_iop->mio_ld_io(ccb, xs, blockno, blockcnt)) {
			goto stuffup;
		}
		break;

	case READ_10:
	case WRITE_10:
		rwb = (struct scsipi_rw_10 *)xs->cmd;
		blockno = _4btol(rwb->addr);
		blockcnt = _2btol(rwb->length);
		if (sc->sc_iop->mio_ld_io(ccb, xs, blockno, blockcnt)) {
			goto stuffup;
		}
		break;

	case SCSI_READ_6_COMMAND:
	case SCSI_WRITE_6_COMMAND:
		rw = (struct scsi_rw_6 *)xs->cmd;
		blockno = _3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff);
		blockcnt = rw->length ? rw->length : 0x100;
		if (sc->sc_iop->mio_ld_io(ccb, xs, blockno, blockcnt)) {
			goto stuffup;
		}
		break;

	case SCSI_SYNCHRONIZE_CACHE_10:
	case SCSI_SYNCHRONIZE_CACHE_16:
		mbox[0] = MR_FLUSH_CTRL_CACHE | MR_FLUSH_DISK_CACHE;
		if (mfi_mgmt(ccb, xs,
		    MR_DCMD_CTRL_CACHE_FLUSH, MFI_DATA_NONE, 0, NULL, mbox)) {
			goto stuffup;
		}
		break;

	/* hand it of to the firmware and let it deal with it */
	case SCSI_TEST_UNIT_READY:
		/* save off sd? after autoconf */
		if (!cold)	/* XXX bogus */
			strlcpy(sc->sc_ld[target].ld_dev, device_xname(sc->sc_dev),
			    sizeof(sc->sc_ld[target].ld_dev));
		/* FALLTHROUGH */

	default:
		if (mfi_scsi_ld(ccb, xs)) {
			goto stuffup;
		}
		break;
	}

	DNPRINTF(MFI_D_CMD, "%s: start io %d\n", DEVNAME(sc), target);

	if (xs->xs_control & XS_CTL_POLL) {
		if (mfi_poll(ccb)) {
			/* XXX check for sense in ccb->ccb_sense? */
			aprint_error_dev(sc->sc_dev,
			    "mfi_scsipi_request poll failed\n");
			memset(&xs->sense, 0, sizeof(xs->sense));
			xs->sense.scsi_sense.response_code =
			    SSD_RCODE_VALID | SSD_RCODE_CURRENT;
			xs->sense.scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
			xs->sense.scsi_sense.asc = 0x20; /* invalid opcode */
			xs->error = XS_SENSE;
			xs->status = SCSI_CHECK;
		} else {
			DNPRINTF(MFI_D_DMA,
			    "%s: mfi_scsipi_request poll complete %d\n",
			    DEVNAME(sc), ccb->ccb_dmamap->dm_nsegs);
			xs->error = XS_NOERROR;
			xs->status = SCSI_OK;
			xs->resid = 0;
		}
		mfi_put_ccb(ccb);
		scsipi_done(xs);
		splx(s);
		return;
	}

	mfi_post(sc, ccb);

	DNPRINTF(MFI_D_DMA, "%s: mfi_scsipi_request queued %d\n", DEVNAME(sc),
	    ccb->ccb_dmamap->dm_nsegs);

	splx(s);
	return;

stuffup:
	mfi_put_ccb(ccb);
	xs->error = XS_DRIVER_STUFFUP;
	scsipi_done(xs);
	splx(s);
}

static int
mfi_create_sgl(struct mfi_ccb *ccb, int flags)
{
	struct mfi_softc	*sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr;
	bus_dma_segment_t	*sgd;
	union mfi_sgl		*sgl;
	int			error, i;

	DNPRINTF(MFI_D_DMA, "%s: mfi_create_sgl %#lx\n", DEVNAME(sc),
	    (u_long)ccb->ccb_data);

	if (!ccb->ccb_data)
		return 1;

	KASSERT(flags == BUS_DMA_NOWAIT || !cpu_intr_p());
	error = bus_dmamap_load(sc->sc_datadmat, ccb->ccb_dmamap,
	    ccb->ccb_data, ccb->ccb_len, NULL, flags);
	if (error) {
		if (error == EFBIG) {
			aprint_error_dev(sc->sc_dev, "more than %d dma segs\n",
			    sc->sc_max_sgl);
		} else {
			aprint_error_dev(sc->sc_dev,
			    "error %d loading dma map\n", error);
		}
		return 1;
	}

	hdr = &ccb->ccb_frame->mfr_header;
	sgl = ccb->ccb_sgl;
	sgd = ccb->ccb_dmamap->dm_segs;
	for (i = 0; i < ccb->ccb_dmamap->dm_nsegs; i++) {
		if (sc->sc_ioptype == MFI_IOP_TBOLT &&
		    (hdr->mfh_cmd == MFI_CMD_PD_SCSI_IO ||
		     hdr->mfh_cmd == MFI_CMD_LD_READ ||
		     hdr->mfh_cmd == MFI_CMD_LD_WRITE)) {
			sgl->sg_ieee[i].addr = htole64(sgd[i].ds_addr);
			sgl->sg_ieee[i].len = htole32(sgd[i].ds_len);
			sgl->sg_ieee[i].flags = 0;
			DNPRINTF(MFI_D_DMA, "%s: addr: %#" PRIx64 " len: %#"
			    PRIx32 "\n",
			    DEVNAME(sc), sgl->sg64[i].addr, sgl->sg64[i].len);
			hdr->mfh_flags |= MFI_FRAME_IEEE_SGL | MFI_FRAME_SGL64;
		} else if (sc->sc_64bit_dma) {
			sgl->sg64[i].addr = htole64(sgd[i].ds_addr);
			sgl->sg64[i].len = htole32(sgd[i].ds_len);
			DNPRINTF(MFI_D_DMA, "%s: addr: %#" PRIx64 " len: %#"
			    PRIx32 "\n",
			    DEVNAME(sc), sgl->sg64[i].addr, sgl->sg64[i].len);
			hdr->mfh_flags |= MFI_FRAME_SGL64;
		} else {
			sgl->sg32[i].addr = htole32(sgd[i].ds_addr);
			sgl->sg32[i].len = htole32(sgd[i].ds_len);
			DNPRINTF(MFI_D_DMA, "%s: addr: %#x  len: %#x\n",
			    DEVNAME(sc), sgl->sg32[i].addr, sgl->sg32[i].len);
			hdr->mfh_flags |= MFI_FRAME_SGL32;
		}
	}

	if (ccb->ccb_direction == MFI_DATA_IN) {
		hdr->mfh_flags |= MFI_FRAME_DIR_READ;
		bus_dmamap_sync(sc->sc_datadmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
	} else {
		hdr->mfh_flags |= MFI_FRAME_DIR_WRITE;
		bus_dmamap_sync(sc->sc_datadmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);
	}

	hdr->mfh_sg_count = ccb->ccb_dmamap->dm_nsegs;
	ccb->ccb_frame_size += sc->sc_sgl_size * ccb->ccb_dmamap->dm_nsegs;
	ccb->ccb_extra_frames = (ccb->ccb_frame_size - 1) / MFI_FRAME_SIZE;

	DNPRINTF(MFI_D_DMA, "%s: sg_count: %d  frame_size: %d  frames_size: %d"
	    "  dm_nsegs: %d  extra_frames: %d\n",
	    DEVNAME(sc),
	    hdr->mfh_sg_count,
	    ccb->ccb_frame_size,
	    sc->sc_frames_size,
	    ccb->ccb_dmamap->dm_nsegs,
	    ccb->ccb_extra_frames);

	return 0;
}

static int
mfi_mgmt_internal(struct mfi_softc *sc, uint32_t opc, uint32_t dir,
    uint32_t len, void *buf, uint8_t *mbox, bool poll)
{
	struct mfi_ccb		*ccb;
	int			rv = 1;

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return rv;
	rv = mfi_mgmt(ccb, NULL, opc, dir, len, buf, mbox);
	if (rv)
		return rv;

	if (poll) {
		rv = 1;
		if (mfi_poll(ccb))
			goto done;
	} else {
		mfi_post(sc, ccb);

		DNPRINTF(MFI_D_MISC, "%s: mfi_mgmt_internal sleeping\n",
		    DEVNAME(sc));
		while (ccb->ccb_state != MFI_CCB_DONE)
			tsleep(ccb, PRIBIO, "mfi_mgmt", 0);

		if (ccb->ccb_flags & MFI_CCB_F_ERR)
			goto done;
	}
	rv = 0;

done:
	mfi_put_ccb(ccb);
	return rv;
}

static int
mfi_mgmt(struct mfi_ccb *ccb, struct scsipi_xfer *xs,
    uint32_t opc, uint32_t dir, uint32_t len, void *buf, uint8_t *mbox)
{
	struct mfi_dcmd_frame	*dcmd;

	DNPRINTF(MFI_D_MISC, "%s: mfi_mgmt %#x\n", DEVNAME(ccb->ccb_sc), opc);

	dcmd = &ccb->ccb_frame->mfr_dcmd;
	memset(dcmd->mdf_mbox, 0, MFI_MBOX_SIZE);
	dcmd->mdf_header.mfh_cmd = MFI_CMD_DCMD;
	dcmd->mdf_header.mfh_timeout = 0;

	dcmd->mdf_opcode = opc;
	dcmd->mdf_header.mfh_data_len = 0;
	ccb->ccb_direction = dir;
	ccb->ccb_xs = xs;
	ccb->ccb_done = mfi_mgmt_done;

	ccb->ccb_frame_size = MFI_DCMD_FRAME_SIZE;

	/* handle special opcodes */
	if (mbox)
		memcpy(dcmd->mdf_mbox, mbox, MFI_MBOX_SIZE);

	if (dir != MFI_DATA_NONE) {
		dcmd->mdf_header.mfh_data_len = len;
		ccb->ccb_data = buf;
		ccb->ccb_len = len;
		ccb->ccb_sgl = &dcmd->mdf_sgl;

		if (mfi_create_sgl(ccb, BUS_DMA_WAITOK))
			return 1;
	}
	return 0;
}

static void
mfi_mgmt_done(struct mfi_ccb *ccb)
{
	struct scsipi_xfer	*xs = ccb->ccb_xs;
	struct mfi_softc	*sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr = &ccb->ccb_frame->mfr_header;

	DNPRINTF(MFI_D_INTR, "%s: mfi_mgmt_done %#lx %#lx\n",
	    DEVNAME(sc), (u_long)ccb, (u_long)ccb->ccb_frame);

	if (ccb->ccb_data != NULL) {
		DNPRINTF(MFI_D_INTR, "%s: mfi_mgmt_done sync\n",
		    DEVNAME(sc));
		bus_dmamap_sync(sc->sc_datadmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize,
		    (ccb->ccb_direction & MFI_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_datadmat, ccb->ccb_dmamap);
	}

	if (hdr->mfh_cmd_status != MFI_STAT_OK)
		ccb->ccb_flags |= MFI_CCB_F_ERR;

	ccb->ccb_state = MFI_CCB_DONE;
	if (xs) {
		if (hdr->mfh_cmd_status != MFI_STAT_OK) {
			xs->error = XS_DRIVER_STUFFUP;
		} else {
			xs->error = XS_NOERROR;
			xs->status = SCSI_OK;
			xs->resid = 0;
		}
		mfi_put_ccb(ccb);
		scsipi_done(xs);
	} else
		wakeup(ccb);
}

#if NBIO > 0
int
mfi_ioctl(device_t dev, u_long cmd, void *addr)
{
	struct mfi_softc *sc = device_private(dev);
	int error = 0;
	int s;

	KERNEL_LOCK(1, curlwp);
	s = splbio();

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl ", DEVNAME(sc));

	switch (cmd) {
	case BIOCINQ:
		DNPRINTF(MFI_D_IOCTL, "inq\n");
		error = mfi_ioctl_inq(sc, (struct bioc_inq *)addr);
		break;

	case BIOCVOL:
		DNPRINTF(MFI_D_IOCTL, "vol\n");
		error = mfi_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK:
		DNPRINTF(MFI_D_IOCTL, "disk\n");
		error = mfi_ioctl_disk(sc, (struct bioc_disk *)addr);
		break;

	case BIOCALARM:
		DNPRINTF(MFI_D_IOCTL, "alarm\n");
		error = mfi_ioctl_alarm(sc, (struct bioc_alarm *)addr);
		break;

	case BIOCBLINK:
		DNPRINTF(MFI_D_IOCTL, "blink\n");
		error = mfi_ioctl_blink(sc, (struct bioc_blink *)addr);
		break;

	case BIOCSETSTATE:
		DNPRINTF(MFI_D_IOCTL, "setstate\n");
		error = mfi_ioctl_setstate(sc, (struct bioc_setstate *)addr);
		break;

	default:
		DNPRINTF(MFI_D_IOCTL, " invalid ioctl\n");
		error = EINVAL;
	}
	splx(s);
	KERNEL_UNLOCK_ONE(curlwp);

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl return %x\n", DEVNAME(sc), error);
	return error;
}

static int
mfi_ioctl_inq(struct mfi_softc *sc, struct bioc_inq *bi)
{
	struct mfi_conf		*cfg;
	int			rv = EINVAL;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_inq\n", DEVNAME(sc));

	if (mfi_get_info(sc)) {
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_inq failed\n",
		    DEVNAME(sc));
		return EIO;
	}

	/* get figures */
	cfg = malloc(sizeof *cfg, M_DEVBUF, M_WAITOK);
	if (mfi_mgmt_internal(sc, MD_DCMD_CONF_GET, MFI_DATA_IN,
	    sizeof *cfg, cfg, NULL, false))
		goto freeme;

	strlcpy(bi->bi_dev, DEVNAME(sc), sizeof(bi->bi_dev));
	bi->bi_novol = cfg->mfc_no_ld + cfg->mfc_no_hs;
	bi->bi_nodisk = sc->sc_info.mci_pd_disks_present;

	rv = 0;
freeme:
	free(cfg, M_DEVBUF);
	return rv;
}

static int
mfi_ioctl_vol(struct mfi_softc *sc, struct bioc_vol *bv)
{
	int			i, per, rv = EINVAL;
	uint8_t			mbox[MFI_MBOX_SIZE];

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_vol %#x\n",
	    DEVNAME(sc), bv->bv_volid);

	if (mfi_mgmt_internal(sc, MR_DCMD_LD_GET_LIST, MFI_DATA_IN,
	    sizeof(sc->sc_ld_list), &sc->sc_ld_list, NULL, false))
		goto done;

	i = bv->bv_volid;
	mbox[0] = sc->sc_ld_list.mll_list[i].mll_ld.mld_target;
	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_vol target %#x\n",
	    DEVNAME(sc), mbox[0]);

	if (mfi_mgmt_internal(sc, MR_DCMD_LD_GET_INFO, MFI_DATA_IN,
	    sizeof(sc->sc_ld_details), &sc->sc_ld_details, mbox, false))
		goto done;

	if (bv->bv_volid >= sc->sc_ld_list.mll_no_ld) {
		/* go do hotspares */
		rv = mfi_bio_hs(sc, bv->bv_volid, MFI_MGMT_VD, bv);
		goto done;
	}

	strlcpy(bv->bv_dev, sc->sc_ld[i].ld_dev, sizeof(bv->bv_dev));

	switch(sc->sc_ld_list.mll_list[i].mll_state) {
	case MFI_LD_OFFLINE:
		bv->bv_status = BIOC_SVOFFLINE;
		break;

	case MFI_LD_PART_DEGRADED:
	case MFI_LD_DEGRADED:
		bv->bv_status = BIOC_SVDEGRADED;
		break;

	case MFI_LD_ONLINE:
		bv->bv_status = BIOC_SVONLINE;
		break;

	default:
		bv->bv_status = BIOC_SVINVALID;
		DNPRINTF(MFI_D_IOCTL, "%s: invalid logical disk state %#x\n",
		    DEVNAME(sc),
		    sc->sc_ld_list.mll_list[i].mll_state);
	}

	/* additional status can modify MFI status */
	switch (sc->sc_ld_details.mld_progress.mlp_in_prog) {
	case MFI_LD_PROG_CC:
	case MFI_LD_PROG_BGI:
		bv->bv_status = BIOC_SVSCRUB;
		per = (int)sc->sc_ld_details.mld_progress.mlp_cc.mp_progress;
		bv->bv_percent = (per * 100) / 0xffff;
		bv->bv_seconds =
		    sc->sc_ld_details.mld_progress.mlp_cc.mp_elapsed_seconds;
		break;

	case MFI_LD_PROG_FGI:
	case MFI_LD_PROG_RECONSTRUCT:
		/* nothing yet */
		break;
	}

	/*
	 * The RAID levels are determined per the SNIA DDF spec, this is only
	 * a subset that is valid for the MFI contrller.
	 */
	bv->bv_level = sc->sc_ld_details.mld_cfg.mlc_parm.mpa_pri_raid;
	if (sc->sc_ld_details.mld_cfg.mlc_parm.mpa_sec_raid ==
	    MFI_DDF_SRL_SPANNED)
		bv->bv_level *= 10;

	bv->bv_nodisk = sc->sc_ld_details.mld_cfg.mlc_parm.mpa_no_drv_per_span *
	    sc->sc_ld_details.mld_cfg.mlc_parm.mpa_span_depth;

	bv->bv_size = sc->sc_ld_details.mld_size * 512; /* bytes per block */

	rv = 0;
done:
	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_vol done %x\n",
	    DEVNAME(sc), rv);
	return rv;
}

static int
mfi_ioctl_disk(struct mfi_softc *sc, struct bioc_disk *bd)
{
	struct mfi_conf		*cfg;
	struct mfi_array	*ar;
	struct mfi_ld_cfg	*ld;
	struct mfi_pd_details	*pd;
	struct scsipi_inquiry_data *inqbuf;
	char			vend[8+16+4+1];
	int			i, rv = EINVAL;
	int			arr, vol, disk;
	uint32_t		size;
	uint8_t			mbox[MFI_MBOX_SIZE];

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_disk %#x\n",
	    DEVNAME(sc), bd->bd_diskid);

	pd = malloc(sizeof *pd, M_DEVBUF, M_WAITOK | M_ZERO);

	/* send single element command to retrieve size for full structure */
	cfg = malloc(sizeof *cfg, M_DEVBUF, M_WAITOK);
	if (mfi_mgmt_internal(sc, MD_DCMD_CONF_GET, MFI_DATA_IN,
	    sizeof *cfg, cfg, NULL, false))
		goto freeme;

	size = cfg->mfc_size;
	free(cfg, M_DEVBUF);

	/* memory for read config */
	cfg = malloc(size, M_DEVBUF, M_WAITOK|M_ZERO);
	if (mfi_mgmt_internal(sc, MD_DCMD_CONF_GET, MFI_DATA_IN,
	    size, cfg, NULL, false))
		goto freeme;

	ar = cfg->mfc_array;

	/* calculate offset to ld structure */
	ld = (struct mfi_ld_cfg *)(
	    ((uint8_t *)cfg) + offsetof(struct mfi_conf, mfc_array) +
	    cfg->mfc_array_size * cfg->mfc_no_array);

	vol = bd->bd_volid;

	if (vol >= cfg->mfc_no_ld) {
		/* do hotspares */
		rv = mfi_bio_hs(sc, bd->bd_volid, MFI_MGMT_SD, bd);
		goto freeme;
	}

	/* find corresponding array for ld */
	for (i = 0, arr = 0; i < vol; i++)
		arr += ld[i].mlc_parm.mpa_span_depth;

	/* offset disk into pd list */
	disk = bd->bd_diskid % ld[vol].mlc_parm.mpa_no_drv_per_span;

	/* offset array index into the next spans */
	arr += bd->bd_diskid / ld[vol].mlc_parm.mpa_no_drv_per_span;

	bd->bd_target = ar[arr].pd[disk].mar_enc_slot;
	switch (ar[arr].pd[disk].mar_pd_state){
	case MFI_PD_UNCONFIG_GOOD:
		bd->bd_status = BIOC_SDUNUSED;
		break;

	case MFI_PD_HOTSPARE: /* XXX dedicated hotspare part of array? */
		bd->bd_status = BIOC_SDHOTSPARE;
		break;

	case MFI_PD_OFFLINE:
		bd->bd_status = BIOC_SDOFFLINE;
		break;

	case MFI_PD_FAILED:
		bd->bd_status = BIOC_SDFAILED;
		break;

	case MFI_PD_REBUILD:
		bd->bd_status = BIOC_SDREBUILD;
		break;

	case MFI_PD_ONLINE:
		bd->bd_status = BIOC_SDONLINE;
		break;

	case MFI_PD_UNCONFIG_BAD: /* XXX define new state in bio */
	default:
		bd->bd_status = BIOC_SDINVALID;
		break;

	}

	/* get the remaining fields */
	*((uint16_t *)&mbox) = ar[arr].pd[disk].mar_pd.mfp_id;
	memset(pd, 0, sizeof(*pd));
	if (mfi_mgmt_internal(sc, MR_DCMD_PD_GET_INFO, MFI_DATA_IN,
	    sizeof *pd, pd, mbox, false))
		goto freeme;

	bd->bd_size = pd->mpd_size * 512; /* bytes per block */

	/* if pd->mpd_enc_idx is 0 then it is not in an enclosure */
	bd->bd_channel = pd->mpd_enc_idx;

	inqbuf = (struct scsipi_inquiry_data *)&pd->mpd_inq_data;
	memcpy(vend, inqbuf->vendor, sizeof vend - 1);
	vend[sizeof vend - 1] = '\0';
	strlcpy(bd->bd_vendor, vend, sizeof(bd->bd_vendor));

	/* XXX find a way to retrieve serial nr from drive */
	/* XXX find a way to get bd_procdev */

	rv = 0;
freeme:
	free(pd, M_DEVBUF);
	free(cfg, M_DEVBUF);

	return rv;
}

static int
mfi_ioctl_alarm(struct mfi_softc *sc, struct bioc_alarm *ba)
{
	uint32_t		opc, dir = MFI_DATA_NONE;
	int			rv = 0;
	int8_t			ret;

	switch(ba->ba_opcode) {
	case BIOC_SADISABLE:
		opc = MR_DCMD_SPEAKER_DISABLE;
		break;

	case BIOC_SAENABLE:
		opc = MR_DCMD_SPEAKER_ENABLE;
		break;

	case BIOC_SASILENCE:
		opc = MR_DCMD_SPEAKER_SILENCE;
		break;

	case BIOC_GASTATUS:
		opc = MR_DCMD_SPEAKER_GET;
		dir = MFI_DATA_IN;
		break;

	case BIOC_SATEST:
		opc = MR_DCMD_SPEAKER_TEST;
		break;

	default:
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_alarm biocalarm invalid "
		    "opcode %x\n", DEVNAME(sc), ba->ba_opcode);
		return EINVAL;
	}

	if (mfi_mgmt_internal(sc, opc, dir, sizeof(ret), &ret, NULL, false))
		rv = EINVAL;
	else
		if (ba->ba_opcode == BIOC_GASTATUS)
			ba->ba_status = ret;
		else
			ba->ba_status = 0;

	return rv;
}

static int
mfi_ioctl_blink(struct mfi_softc *sc, struct bioc_blink *bb)
{
	int			i, found, rv = EINVAL;
	uint8_t			mbox[MFI_MBOX_SIZE];
	uint32_t		cmd;
	struct mfi_pd_list	*pd;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_blink %x\n", DEVNAME(sc),
	    bb->bb_status);

	/* channel 0 means not in an enclosure so can't be blinked */
	if (bb->bb_channel == 0)
		return EINVAL;

	pd = malloc(MFI_PD_LIST_SIZE, M_DEVBUF, M_WAITOK);

	if (mfi_mgmt_internal(sc, MR_DCMD_PD_GET_LIST, MFI_DATA_IN,
	    MFI_PD_LIST_SIZE, pd, NULL, false))
		goto done;

	for (i = 0, found = 0; i < pd->mpl_no_pd; i++)
		if (bb->bb_channel == pd->mpl_address[i].mpa_enc_index &&
		    bb->bb_target == pd->mpl_address[i].mpa_enc_slot) {
		    	found = 1;
			break;
		}

	if (!found)
		goto done;

	memset(mbox, 0, sizeof mbox);

	*((uint16_t *)&mbox) = pd->mpl_address[i].mpa_pd_id;

	switch (bb->bb_status) {
	case BIOC_SBUNBLINK:
		cmd = MR_DCMD_PD_UNBLINK;
		break;

	case BIOC_SBBLINK:
		cmd = MR_DCMD_PD_BLINK;
		break;

	case BIOC_SBALARM:
	default:
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_blink biocblink invalid "
		    "opcode %x\n", DEVNAME(sc), bb->bb_status);
		goto done;
	}


	if (mfi_mgmt_internal(sc, cmd, MFI_DATA_NONE, 0, NULL, mbox, false))
		goto done;

	rv = 0;
done:
	free(pd, M_DEVBUF);
	return rv;
}

static int
mfi_ioctl_setstate(struct mfi_softc *sc, struct bioc_setstate *bs)
{
	struct mfi_pd_list	*pd;
	int			i, found, rv = EINVAL;
	uint8_t			mbox[MFI_MBOX_SIZE];

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_setstate %x\n", DEVNAME(sc),
	    bs->bs_status);

	pd = malloc(MFI_PD_LIST_SIZE, M_DEVBUF, M_WAITOK);

	if (mfi_mgmt_internal(sc, MR_DCMD_PD_GET_LIST, MFI_DATA_IN,
	    MFI_PD_LIST_SIZE, pd, NULL, false))
		goto done;

	for (i = 0, found = 0; i < pd->mpl_no_pd; i++)
		if (bs->bs_channel == pd->mpl_address[i].mpa_enc_index &&
		    bs->bs_target == pd->mpl_address[i].mpa_enc_slot) {
		    	found = 1;
			break;
		}

	if (!found)
		goto done;

	memset(mbox, 0, sizeof mbox);

	*((uint16_t *)&mbox) = pd->mpl_address[i].mpa_pd_id;

	switch (bs->bs_status) {
	case BIOC_SSONLINE:
		mbox[2] = MFI_PD_ONLINE;
		break;

	case BIOC_SSOFFLINE:
		mbox[2] = MFI_PD_OFFLINE;
		break;

	case BIOC_SSHOTSPARE:
		mbox[2] = MFI_PD_HOTSPARE;
		break;
/*
	case BIOC_SSREBUILD:
		break;
*/
	default:
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_setstate invalid "
		    "opcode %x\n", DEVNAME(sc), bs->bs_status);
		goto done;
	}


	if (mfi_mgmt_internal(sc, MD_DCMD_PD_SET_STATE, MFI_DATA_NONE,
	    0, NULL, mbox, false))
		goto done;

	rv = 0;
done:
	free(pd, M_DEVBUF);
	return rv;
}

static int
mfi_bio_hs(struct mfi_softc *sc, int volid, int type, void *bio_hs)
{
	struct mfi_conf		*cfg;
	struct mfi_hotspare	*hs;
	struct mfi_pd_details	*pd;
	struct bioc_disk	*sdhs;
	struct bioc_vol		*vdhs;
	struct scsipi_inquiry_data *inqbuf;
	char			vend[8+16+4+1];
	int			i, rv = EINVAL;
	uint32_t		size;
	uint8_t			mbox[MFI_MBOX_SIZE];

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_vol_hs %d\n", DEVNAME(sc), volid);

	if (!bio_hs)
		return EINVAL;

	pd = malloc(sizeof *pd, M_DEVBUF, M_WAITOK | M_ZERO);

	/* send single element command to retrieve size for full structure */
	cfg = malloc(sizeof *cfg, M_DEVBUF, M_WAITOK);
	if (mfi_mgmt_internal(sc, MD_DCMD_CONF_GET, MFI_DATA_IN,
	    sizeof *cfg, cfg, NULL, false))
		goto freeme;

	size = cfg->mfc_size;
	free(cfg, M_DEVBUF);

	/* memory for read config */
	cfg = malloc(size, M_DEVBUF, M_WAITOK|M_ZERO);
	if (mfi_mgmt_internal(sc, MD_DCMD_CONF_GET, MFI_DATA_IN,
	    size, cfg, NULL, false))
		goto freeme;

	/* calculate offset to hs structure */
	hs = (struct mfi_hotspare *)(
	    ((uint8_t *)cfg) + offsetof(struct mfi_conf, mfc_array) +
	    cfg->mfc_array_size * cfg->mfc_no_array +
	    cfg->mfc_ld_size * cfg->mfc_no_ld);

	if (volid < cfg->mfc_no_ld)
		goto freeme; /* not a hotspare */

	if (volid > (cfg->mfc_no_ld + cfg->mfc_no_hs))
		goto freeme; /* not a hotspare */

	/* offset into hotspare structure */
	i = volid - cfg->mfc_no_ld;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_vol_hs i %d volid %d no_ld %d no_hs %d "
	    "hs %p cfg %p id %02x\n", DEVNAME(sc), i, volid, cfg->mfc_no_ld,
	    cfg->mfc_no_hs, hs, cfg, hs[i].mhs_pd.mfp_id);

	/* get pd fields */
	memset(mbox, 0, sizeof mbox);
	*((uint16_t *)&mbox) = hs[i].mhs_pd.mfp_id;
	if (mfi_mgmt_internal(sc, MR_DCMD_PD_GET_INFO, MFI_DATA_IN,
	    sizeof *pd, pd, mbox, false)) {
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_vol_hs illegal PD\n",
		    DEVNAME(sc));
		goto freeme;
	}

	switch (type) {
	case MFI_MGMT_VD:
		vdhs = bio_hs;
		vdhs->bv_status = BIOC_SVONLINE;
		vdhs->bv_size = pd->mpd_size * 512; /* bytes per block */
		vdhs->bv_level = -1; /* hotspare */
		vdhs->bv_nodisk = 1;
		break;

	case MFI_MGMT_SD:
		sdhs = bio_hs;
		sdhs->bd_status = BIOC_SDHOTSPARE;
		sdhs->bd_size = pd->mpd_size * 512; /* bytes per block */
		sdhs->bd_channel = pd->mpd_enc_idx;
		sdhs->bd_target = pd->mpd_enc_slot;
		inqbuf = (struct scsipi_inquiry_data *)&pd->mpd_inq_data;
		memcpy(vend, inqbuf->vendor, sizeof(vend) - 1);
		vend[sizeof vend - 1] = '\0';
		strlcpy(sdhs->bd_vendor, vend, sizeof(sdhs->bd_vendor));
		break;

	default:
		goto freeme;
	}

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_vol_hs 6\n", DEVNAME(sc));
	rv = 0;
freeme:
	free(pd, M_DEVBUF);
	free(cfg, M_DEVBUF);

	return rv;
}

static int
mfi_destroy_sensors(struct mfi_softc *sc)
{
	if (sc->sc_sme == NULL)
		return 0;
	sysmon_envsys_unregister(sc->sc_sme);
	sc->sc_sme = NULL;
	free(sc->sc_sensor, M_DEVBUF);
	return 0;
}

static int
mfi_create_sensors(struct mfi_softc *sc)
{
	int i;
	int nsensors = sc->sc_ld_cnt + 1;
	int rv;

	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sensor = malloc(sizeof(envsys_data_t) * nsensors,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_sensor == NULL) {
		aprint_error_dev(sc->sc_dev, "can't allocate envsys_data_t\n");
		return ENOMEM;
	}

	/* BBU */
	sc->sc_sensor[0].units = ENVSYS_INDICATOR;
	sc->sc_sensor[0].state = ENVSYS_SINVALID;
	sc->sc_sensor[0].value_cur = 0;
	/* Enable monitoring for BBU state changes, if present */
	if (sc->sc_info.mci_hw_present & MFI_INFO_HW_BBU)
		sc->sc_sensor[0].flags |= ENVSYS_FMONCRITICAL;
	snprintf(sc->sc_sensor[0].desc,
	    sizeof(sc->sc_sensor[0].desc), "%s BBU", DEVNAME(sc));
	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor[0]))
		goto out;

	for (i = 1; i < nsensors; i++) {
		sc->sc_sensor[i].units = ENVSYS_DRIVE;
		sc->sc_sensor[i].state = ENVSYS_SINVALID;
		sc->sc_sensor[i].value_cur = ENVSYS_DRIVE_EMPTY;
		/* Enable monitoring for drive state changes */
		sc->sc_sensor[i].flags |= ENVSYS_FMONSTCHANGED;
		/* logical drives */
		snprintf(sc->sc_sensor[i].desc,
		    sizeof(sc->sc_sensor[i].desc), "%s:%d",
		    DEVNAME(sc), i - 1);
		if (sysmon_envsys_sensor_attach(sc->sc_sme,
						&sc->sc_sensor[i]))
			goto out;
	}

	sc->sc_sme->sme_name = DEVNAME(sc);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = mfi_sensor_refresh;
	rv = sysmon_envsys_register(sc->sc_sme);
	if (rv != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to register with sysmon (rv = %d)\n", rv);
		goto out;
	}
	return 0;

out:
	free(sc->sc_sensor, M_DEVBUF);
	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;
	return EINVAL;
}

static void
mfi_sensor_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct mfi_softc	*sc = sme->sme_cookie;
	struct bioc_vol		bv;
	int s;
	int error;

	if (edata->sensor >= sc->sc_ld_cnt + 1)
		return;

	if (edata->sensor == 0) {
		/* BBU */
		struct mfi_bbu_status	bbu_stat;
		int bbu_status;
		if ((sc->sc_info.mci_hw_present & MFI_INFO_HW_BBU) == 0)
			return;

		KERNEL_LOCK(1, curlwp);
		s = splbio();
		bbu_status = mfi_get_bbu(sc, &bbu_stat);
		splx(s);
		KERNEL_UNLOCK_ONE(curlwp);
		switch(bbu_status) {
		case MFI_BBU_GOOD:
			edata->value_cur = 1;
			edata->state = ENVSYS_SVALID;
			if (!sc->sc_bbuok)
				aprint_normal_dev(sc->sc_dev,
				    "BBU state changed to good\n");
			sc->sc_bbuok = true;
			break;
		case MFI_BBU_BAD:
			edata->value_cur = 0;
			edata->state = ENVSYS_SCRITICAL;
			if (sc->sc_bbuok)
				aprint_normal_dev(sc->sc_dev,
				    "BBU state changed to bad\n");
			sc->sc_bbuok = false;
			break;
		case MFI_BBU_UNKNOWN:
		default:
			edata->value_cur = 0;
			edata->state = ENVSYS_SINVALID;
			sc->sc_bbuok = false;
			break;
		}
		return;
	}

	memset(&bv, 0, sizeof(bv));
	bv.bv_volid = edata->sensor - 1;
	KERNEL_LOCK(1, curlwp);
	s = splbio();
	error = mfi_ioctl_vol(sc, &bv);
	splx(s);
	KERNEL_UNLOCK_ONE(curlwp);
	if (error)
		bv.bv_status = BIOC_SVINVALID;

	bio_vol_to_envsys(edata, &bv);
}

#endif /* NBIO > 0 */

static uint32_t
mfi_xscale_fw_state(struct mfi_softc *sc)
{
	return mfi_read(sc, MFI_OMSG0);
}

static void
mfi_xscale_intr_dis(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_OMSK, 0);
}

static void
mfi_xscale_intr_ena(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_OMSK, MFI_ENABLE_INTR);
}

static int
mfi_xscale_intr(struct mfi_softc *sc)
{
	uint32_t status;

	status = mfi_read(sc, MFI_OSTS);
	if (!ISSET(status, MFI_OSTS_INTR_VALID))
		return 0;

	/* write status back to acknowledge interrupt */
	mfi_write(sc, MFI_OSTS, status);
	return 1;
}

static void
mfi_xscale_post(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_frames),
	    ccb->ccb_pframe - MFIMEM_DVA(sc->sc_frames),
	    sc->sc_frames_size, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_sense),
	    ccb->ccb_psense - MFIMEM_DVA(sc->sc_sense),
	    MFI_SENSE_SIZE, BUS_DMASYNC_PREREAD);

	mfi_write(sc, MFI_IQP, (ccb->ccb_pframe >> 3) |
	    ccb->ccb_extra_frames);
	ccb->ccb_state = MFI_CCB_RUNNING;
}

static uint32_t
mfi_ppc_fw_state(struct mfi_softc *sc)
{
	return mfi_read(sc, MFI_OSP);
}

static void
mfi_ppc_intr_dis(struct mfi_softc *sc)
{
	/* Taking a wild guess --dyoung */
	mfi_write(sc, MFI_OMSK, ~(uint32_t)0x0);
	mfi_write(sc, MFI_ODC, 0xffffffff);
}

static void
mfi_ppc_intr_ena(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_ODC, 0xffffffff);
	mfi_write(sc, MFI_OMSK, ~0x80000004);
}

static int
mfi_ppc_intr(struct mfi_softc *sc)
{
	uint32_t status;

	status = mfi_read(sc, MFI_OSTS);
	if (!ISSET(status, MFI_OSTS_PPC_INTR_VALID))
		return 0;

	/* write status back to acknowledge interrupt */
	mfi_write(sc, MFI_ODC, status);
	return 1;
}

static void
mfi_ppc_post(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	mfi_write(sc, MFI_IQP, 0x1 | ccb->ccb_pframe |
	    (ccb->ccb_extra_frames << 1));
	ccb->ccb_state = MFI_CCB_RUNNING;
}

u_int32_t
mfi_gen2_fw_state(struct mfi_softc *sc)
{
	return (mfi_read(sc, MFI_OSP));
}

void
mfi_gen2_intr_dis(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_OMSK, 0xffffffff);
	mfi_write(sc, MFI_ODC, 0xffffffff);
}

void
mfi_gen2_intr_ena(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_ODC, 0xffffffff);
	mfi_write(sc, MFI_OMSK, ~MFI_OSTS_GEN2_INTR_VALID);
}

int
mfi_gen2_intr(struct mfi_softc *sc)
{
	u_int32_t status;

	status = mfi_read(sc, MFI_OSTS);
	if (!ISSET(status, MFI_OSTS_GEN2_INTR_VALID))
		return (0);

	/* write status back to acknowledge interrupt */
	mfi_write(sc, MFI_ODC, status);

	return (1);
}

void
mfi_gen2_post(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	mfi_write(sc, MFI_IQP, 0x1 | ccb->ccb_pframe |
	    (ccb->ccb_extra_frames << 1));
	ccb->ccb_state = MFI_CCB_RUNNING;
}

u_int32_t
mfi_skinny_fw_state(struct mfi_softc *sc)
{
	return (mfi_read(sc, MFI_OSP));
}

void
mfi_skinny_intr_dis(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_OMSK, 0);
}

void
mfi_skinny_intr_ena(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_OMSK, ~0x00000001);
}

int
mfi_skinny_intr(struct mfi_softc *sc)
{
	u_int32_t status;

	status = mfi_read(sc, MFI_OSTS);
	if (!ISSET(status, MFI_OSTS_SKINNY_INTR_VALID))
		return (0);

	/* write status back to acknowledge interrupt */
	mfi_write(sc, MFI_OSTS, status);

	return (1);
}

void
mfi_skinny_post(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	mfi_write(sc, MFI_IQPL, 0x1 | ccb->ccb_pframe |
	    (ccb->ccb_extra_frames << 1));
	mfi_write(sc, MFI_IQPH, 0x00000000);
	ccb->ccb_state = MFI_CCB_RUNNING;
}

#define MFI_FUSION_ENABLE_INTERRUPT_MASK	(0x00000008)

void
mfi_tbolt_intr_ena(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_OMSK, ~MFI_FUSION_ENABLE_INTERRUPT_MASK);
	mfi_read(sc, MFI_OMSK);
}

void
mfi_tbolt_intr_dis(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_OMSK, 0xFFFFFFFF);
	mfi_read(sc, MFI_OMSK);
}

int
mfi_tbolt_intr(struct mfi_softc *sc)
{
	int32_t status;

	status = mfi_read(sc, MFI_OSTS);

	if (ISSET(status, 0x1)) {
		mfi_write(sc, MFI_OSTS, status);
		mfi_read(sc, MFI_OSTS);
		if (ISSET(status, MFI_STATE_CHANGE_INTERRUPT))
			return 0;
		return 1;
	}
	if (!ISSET(status, MFI_FUSION_ENABLE_INTERRUPT_MASK))
		return 0;
	mfi_read(sc, MFI_OSTS);
	return 1;
}

u_int32_t
mfi_tbolt_fw_state(struct mfi_softc *sc)
{
	return mfi_read(sc, MFI_OSP);
}

void
mfi_tbolt_post(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	if (sc->sc_MFA_enabled) {
		if ((ccb->ccb_flags & MFI_CCB_F_TBOLT) == 0)
			mfi_tbolt_build_mpt_ccb(ccb);
		mfi_write(sc, MFI_IQPL,
		    ccb->ccb_tb_request_desc.words & 0xFFFFFFFF);
		mfi_write(sc, MFI_IQPH, 
		    ccb->ccb_tb_request_desc.words >> 32);
		ccb->ccb_state = MFI_CCB_RUNNING;
		return;
	}
	uint64_t bus_add = ccb->ccb_pframe;
	bus_add |= (MFI_REQ_DESCRIPT_FLAGS_MFA
	    << MFI_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
	mfi_write(sc, MFI_IQPL, bus_add);
	mfi_write(sc, MFI_IQPH, bus_add >> 32);
	ccb->ccb_state = MFI_CCB_RUNNING;
}

static void
mfi_tbolt_build_mpt_ccb(struct mfi_ccb *ccb)
{
	union mfi_mpi2_request_descriptor *req_desc = &ccb->ccb_tb_request_desc;
	struct mfi_mpi2_request_raid_scsi_io *io_req = ccb->ccb_tb_io_request;
	struct mpi25_ieee_sge_chain64 *mpi25_ieee_chain;

	io_req->Function = MPI2_FUNCTION_PASSTHRU_IO_REQUEST;
	io_req->SGLOffset0 =
	    offsetof(struct mfi_mpi2_request_raid_scsi_io, SGL) / 4;
	io_req->ChainOffset =
	    offsetof(struct mfi_mpi2_request_raid_scsi_io, SGL) / 16;

	mpi25_ieee_chain =
	    (struct mpi25_ieee_sge_chain64 *)&io_req->SGL.IeeeChain;
	mpi25_ieee_chain->Address = ccb->ccb_pframe;

	/*
	  In MFI pass thru, nextChainOffset will always be zero to
	  indicate the end of the chain.
	*/
	mpi25_ieee_chain->Flags= MPI2_IEEE_SGE_FLAGS_CHAIN_ELEMENT
		| MPI2_IEEE_SGE_FLAGS_IOCPLBNTA_ADDR;

	/* setting the length to the maximum length */
	mpi25_ieee_chain->Length = 1024;

	req_desc->header.RequestFlags = (MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO <<
	    MFI_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
	ccb->ccb_flags |= MFI_CCB_F_TBOLT;
	bus_dmamap_sync(ccb->ccb_sc->sc_dmat,
	    MFIMEM_MAP(ccb->ccb_sc->sc_tbolt_reqmsgpool), 
	    ccb->ccb_tb_pio_request -
	     MFIMEM_DVA(ccb->ccb_sc->sc_tbolt_reqmsgpool),
	    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

/*
 * Description:
 *      This function will prepare message pools for the Thunderbolt controller
 */
static int
mfi_tbolt_init_desc_pool(struct mfi_softc *sc)
{
	uint32_t     offset = 0;
	uint8_t      *addr = MFIMEM_KVA(sc->sc_tbolt_reqmsgpool);

	/* Request Decriptors alignment restrictions */
	KASSERT(((uintptr_t)addr & 0xFF) == 0);

	/* Skip request message pool */
	addr = &addr[MEGASAS_THUNDERBOLT_NEW_MSG_SIZE * (sc->sc_max_cmds + 1)];

	/* Reply Frame Pool is initialized */
	sc->sc_reply_frame_pool = (struct mfi_mpi2_reply_header *) addr;
	KASSERT(((uintptr_t)addr & 0xFF) == 0);

	offset = (uintptr_t)sc->sc_reply_frame_pool
	    - (uintptr_t)MFIMEM_KVA(sc->sc_tbolt_reqmsgpool);
	sc->sc_reply_frame_busaddr =
	    MFIMEM_DVA(sc->sc_tbolt_reqmsgpool) + offset;

	/* initializing reply address to 0xFFFFFFFF */
	memset((uint8_t *)sc->sc_reply_frame_pool, 0xFF,
	       (MEGASAS_THUNDERBOLT_REPLY_SIZE * sc->sc_reply_pool_size));

	/* Skip Reply Frame Pool */
	addr += MEGASAS_THUNDERBOLT_REPLY_SIZE * sc->sc_reply_pool_size;
	sc->sc_reply_pool_limit = (void *)addr;

	offset = MEGASAS_THUNDERBOLT_REPLY_SIZE * sc->sc_reply_pool_size;
	sc->sc_sg_frame_busaddr = sc->sc_reply_frame_busaddr + offset;

	/* initialize the last_reply_idx to 0 */
	sc->sc_last_reply_idx = 0;
	offset = (sc->sc_sg_frame_busaddr + (MEGASAS_MAX_SZ_CHAIN_FRAME *
	    sc->sc_max_cmds)) - MFIMEM_DVA(sc->sc_tbolt_reqmsgpool);
	KASSERT(offset <= sc->sc_tbolt_reqmsgpool->am_size);
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_tbolt_reqmsgpool), 0,
	    MFIMEM_MAP(sc->sc_tbolt_reqmsgpool)->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	return 0;
}

/*
 * This routine prepare and issue INIT2 frame to the Firmware
 */

static int
mfi_tbolt_init_MFI_queue(struct mfi_softc *sc)
{
	struct mpi2_ioc_init_request   *mpi2IocInit;
	struct mfi_init_frame		*mfi_init;
	struct mfi_ccb			*ccb;
	bus_addr_t			phyAddress;
	mfi_address			*mfiAddressTemp;
	int				s;
	char				*verbuf;
	char				wqbuf[10];

	/* Check if initialization is already completed */
	if (sc->sc_MFA_enabled) {
		return 1;
	}

	mpi2IocInit =
	    (struct mpi2_ioc_init_request *)MFIMEM_KVA(sc->sc_tbolt_ioc_init);

	s = splbio();
	if ((ccb = mfi_get_ccb(sc)) == NULL) {
		splx(s);
		return (EBUSY);
	}


	mfi_init = &ccb->ccb_frame->mfr_init;

	memset(mpi2IocInit, 0, sizeof(struct mpi2_ioc_init_request));
	mpi2IocInit->Function  = MPI2_FUNCTION_IOC_INIT;
	mpi2IocInit->WhoInit   = MPI2_WHOINIT_HOST_DRIVER;

	/* set MsgVersion and HeaderVersion host driver was built with */
	mpi2IocInit->MsgVersion = MPI2_VERSION;
	mpi2IocInit->HeaderVersion = MPI2_HEADER_VERSION;
	mpi2IocInit->SystemRequestFrameSize = MEGASAS_THUNDERBOLT_NEW_MSG_SIZE/4;
	mpi2IocInit->ReplyDescriptorPostQueueDepth =
	    (uint16_t)sc->sc_reply_pool_size;
	mpi2IocInit->ReplyFreeQueueDepth = 0; /* Not supported by MR. */

	/* Get physical address of reply frame pool */
	phyAddress = sc->sc_reply_frame_busaddr;
	mfiAddressTemp =
	    (mfi_address *)&mpi2IocInit->ReplyDescriptorPostQueueAddress;
	mfiAddressTemp->u.addressLow = (uint32_t)phyAddress;
	mfiAddressTemp->u.addressHigh = (uint32_t)((uint64_t)phyAddress >> 32);

	/* Get physical address of request message pool */
	phyAddress =  MFIMEM_DVA(sc->sc_tbolt_reqmsgpool);
	mfiAddressTemp = (mfi_address *)&mpi2IocInit->SystemRequestFrameBaseAddress;
	mfiAddressTemp->u.addressLow = (uint32_t)phyAddress;
	mfiAddressTemp->u.addressHigh = (uint32_t)((uint64_t)phyAddress >> 32);

	mpi2IocInit->ReplyFreeQueueAddress =  0; /* Not supported by MR. */
	mpi2IocInit->TimeStamp = time_uptime;

	verbuf = MFIMEM_KVA(sc->sc_tbolt_verbuf);
	snprintf(verbuf, strlen(MEGASAS_VERSION) + 2, "%s\n",
                MEGASAS_VERSION);
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_tbolt_verbuf), 0,
	    MFIMEM_MAP(sc->sc_tbolt_verbuf)->dm_mapsize, BUS_DMASYNC_PREWRITE);
	mfi_init->driver_ver_lo = htole32(MFIMEM_DVA(sc->sc_tbolt_verbuf));
	mfi_init->driver_ver_hi =
		    htole32((uint64_t)MFIMEM_DVA(sc->sc_tbolt_verbuf) >> 32);

	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_tbolt_ioc_init), 0,
	    MFIMEM_MAP(sc->sc_tbolt_ioc_init)->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	/* Get the physical address of the mpi2 ioc init command */
	phyAddress =  MFIMEM_DVA(sc->sc_tbolt_ioc_init);
	mfi_init->mif_qinfo_new_addr_lo = htole32(phyAddress);
	mfi_init->mif_qinfo_new_addr_hi = htole32((uint64_t)phyAddress >> 32);

	mfi_init->mif_header.mfh_cmd = MFI_CMD_INIT;
	mfi_init->mif_header.mfh_data_len = sizeof(struct mpi2_ioc_init_request);
	if (mfi_poll(ccb) != 0) {
		aprint_error_dev(sc->sc_dev, "failed to send IOC init2 "
		    "command at 0x%" PRIx64 "\n",
		    (uint64_t)ccb->ccb_pframe);
		splx(s);
		return 1;
	}
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_tbolt_verbuf), 0,
	    MFIMEM_MAP(sc->sc_tbolt_verbuf)->dm_mapsize, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_tbolt_ioc_init), 0,
	    MFIMEM_MAP(sc->sc_tbolt_ioc_init)->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	mfi_put_ccb(ccb);
	splx(s);

	if (mfi_init->mif_header.mfh_cmd_status == 0) {
		sc->sc_MFA_enabled = 1;
	}
	else {
		aprint_error_dev(sc->sc_dev, "Init command Failed %x\n",
		    mfi_init->mif_header.mfh_cmd_status);
		return 1;
	}

	snprintf(wqbuf, sizeof(wqbuf), "%swq", DEVNAME(sc));
	if (workqueue_create(&sc->sc_ldsync_wq, wqbuf, mfi_tbolt_sync_map_info,
	    sc, PRIBIO, IPL_BIO, 0) != 0) {
		aprint_error_dev(sc->sc_dev, "workqueue_create failed\n");
		return 1;
	}
	workqueue_enqueue(sc->sc_ldsync_wq, &sc->sc_ldsync_wk, NULL);
	return 0;
}

int
mfi_tbolt_intrh(void *arg)
{
	struct mfi_softc	*sc = arg;
	struct mfi_ccb		*ccb;
	union mfi_mpi2_reply_descriptor *desc;
	int smid, num_completed;

	if (!mfi_tbolt_intr(sc))
		return 0;

	DNPRINTF(MFI_D_INTR, "%s: mfi_tbolt_intrh %#lx %#lx\n", DEVNAME(sc),
	    (u_long)sc, (u_long)sc->sc_last_reply_idx);

	KASSERT(sc->sc_last_reply_idx < sc->sc_reply_pool_size);

	desc = (union mfi_mpi2_reply_descriptor *)
	    ((uintptr_t)sc->sc_reply_frame_pool +
	     sc->sc_last_reply_idx * MEGASAS_THUNDERBOLT_REPLY_SIZE);

	bus_dmamap_sync(sc->sc_dmat,
	    MFIMEM_MAP(sc->sc_tbolt_reqmsgpool), 
	    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE * (sc->sc_max_cmds + 1),
	    MEGASAS_THUNDERBOLT_REPLY_SIZE * sc->sc_reply_pool_size,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	num_completed = 0;
	while ((desc->header.ReplyFlags & MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK) !=
	    MPI2_RPY_DESCRIPT_FLAGS_UNUSED) {
		smid = desc->header.SMID;
		KASSERT(smid > 0 && smid <= sc->sc_max_cmds);
		ccb = &sc->sc_ccb[smid - 1];
		DNPRINTF(MFI_D_INTR,
		    "%s: mfi_tbolt_intr SMID %#x reply_idx %#x "
		    "desc %#" PRIx64 " ccb %p\n", DEVNAME(sc), smid,
		    sc->sc_last_reply_idx, desc->words, ccb);
		KASSERT(ccb->ccb_state == MFI_CCB_RUNNING);
		if (ccb->ccb_flags & MFI_CCB_F_TBOLT_IO &&
		    ccb->ccb_tb_io_request->ChainOffset != 0) {
			bus_dmamap_sync(sc->sc_dmat,
			    MFIMEM_MAP(sc->sc_tbolt_reqmsgpool), 
			    ccb->ccb_tb_psg_frame -
				MFIMEM_DVA(sc->sc_tbolt_reqmsgpool),
			    MEGASAS_MAX_SZ_CHAIN_FRAME,  BUS_DMASYNC_POSTREAD);
		}
		if (ccb->ccb_flags & MFI_CCB_F_TBOLT_IO) {
			bus_dmamap_sync(sc->sc_dmat,
			    MFIMEM_MAP(sc->sc_tbolt_reqmsgpool), 
			    ccb->ccb_tb_pio_request -
				MFIMEM_DVA(sc->sc_tbolt_reqmsgpool),
			    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		}
		if (ccb->ccb_done)
			ccb->ccb_done(ccb);
		else
			ccb->ccb_state = MFI_CCB_DONE;
		sc->sc_last_reply_idx++;
		if (sc->sc_last_reply_idx >= sc->sc_reply_pool_size) {
			sc->sc_last_reply_idx = 0;
		}
		desc->words = ~0x0;
		/* Get the next reply descriptor */
		desc = (union mfi_mpi2_reply_descriptor *)
		    ((uintptr_t)sc->sc_reply_frame_pool +
		     sc->sc_last_reply_idx * MEGASAS_THUNDERBOLT_REPLY_SIZE);
		num_completed++;
	}
	if (num_completed == 0)
		return 0;

	bus_dmamap_sync(sc->sc_dmat,
	    MFIMEM_MAP(sc->sc_tbolt_reqmsgpool), 
	    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE * (sc->sc_max_cmds + 1),
	    MEGASAS_THUNDERBOLT_REPLY_SIZE * sc->sc_reply_pool_size,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	mfi_write(sc, MFI_RPI, sc->sc_last_reply_idx);
	return 1;
}


int
mfi_tbolt_scsi_ld_io(struct mfi_ccb *ccb, struct scsipi_xfer *xs,
    uint64_t blockno, uint32_t blockcnt)
{
	struct scsipi_periph *periph = xs->xs_periph;
	struct mfi_mpi2_request_raid_scsi_io    *io_req;
	int sge_count;

	DNPRINTF(MFI_D_CMD, "%s: mfi_tbolt_scsi_ld_io: %d\n",
	    device_xname(periph->periph_channel->chan_adapter->adapt_dev),
	    periph->periph_target);

	if (!xs->data)
		return 1;

	ccb->ccb_done = mfi_tbolt_scsi_ld_done;
	ccb->ccb_xs = xs;
	ccb->ccb_data = xs->data;
	ccb->ccb_len = xs->datalen;

	io_req = ccb->ccb_tb_io_request;

	/* Just the CDB length,rest of the Flags are zero */
	io_req->IoFlags = xs->cmdlen;
	memset(io_req->CDB.CDB32, 0, 32);
	memcpy(io_req->CDB.CDB32, &xs->cmdstore, xs->cmdlen);

	io_req->RaidContext.TargetID = periph->periph_target;
	io_req->RaidContext.Status = 0;
	io_req->RaidContext.exStatus = 0;
	io_req->RaidContext.timeoutValue = MFI_FUSION_FP_DEFAULT_TIMEOUT;
	io_req->Function = MPI2_FUNCTION_LD_IO_REQUEST;
	io_req->DevHandle = periph->periph_target;

	ccb->ccb_tb_request_desc.header.RequestFlags =
	    (MFI_REQ_DESCRIPT_FLAGS_LD_IO << MFI_REQ_DESCRIPT_FLAGS_TYPE_SHIFT);
	io_req->DataLength = blockcnt * MFI_SECTOR_LEN;

	if (xs->xs_control & XS_CTL_DATA_IN) {
		io_req->Control = MPI2_SCSIIO_CONTROL_READ;
		ccb->ccb_direction = MFI_DATA_IN;
	} else {
		io_req->Control = MPI2_SCSIIO_CONTROL_WRITE;
		ccb->ccb_direction = MFI_DATA_OUT;
	}

	sge_count = mfi_tbolt_create_sgl(ccb,
	    (xs->xs_control & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK
	    );
	if (sge_count < 0)
		return 1;
	KASSERT(sge_count <= ccb->ccb_sc->sc_max_sgl);
	io_req->RaidContext.numSGE = sge_count;
	io_req->SGLFlags = MPI2_SGE_FLAGS_64_BIT_ADDRESSING;
	io_req->SGLOffset0 =
	    offsetof(struct mfi_mpi2_request_raid_scsi_io, SGL) / 4;

	io_req->SenseBufferLowAddress = htole32(ccb->ccb_psense);
	io_req->SenseBufferLength = MFI_SENSE_SIZE;

	ccb->ccb_flags |= MFI_CCB_F_TBOLT | MFI_CCB_F_TBOLT_IO;
	bus_dmamap_sync(ccb->ccb_sc->sc_dmat,
	    MFIMEM_MAP(ccb->ccb_sc->sc_tbolt_reqmsgpool), 
	    ccb->ccb_tb_pio_request -
	     MFIMEM_DVA(ccb->ccb_sc->sc_tbolt_reqmsgpool),
	    MEGASAS_THUNDERBOLT_NEW_MSG_SIZE,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return 0;
}


static void
mfi_tbolt_scsi_ld_done(struct mfi_ccb *ccb)
{
	struct mfi_mpi2_request_raid_scsi_io *io_req = ccb->ccb_tb_io_request;
	mfi_scsi_xs_done(ccb, io_req->RaidContext.Status,
	    io_req->RaidContext.exStatus);
}

static int
mfi_tbolt_create_sgl(struct mfi_ccb *ccb, int flags)
{
	struct mfi_softc	*sc = ccb->ccb_sc;
	bus_dma_segment_t	*sgd;
	int			error, i, sge_idx, sge_count;
	struct mfi_mpi2_request_raid_scsi_io *io_req;
	struct mpi25_ieee_sge_chain64 *sgl_ptr;

	DNPRINTF(MFI_D_DMA, "%s: mfi_tbolt_create_sgl %#lx\n", DEVNAME(sc),
	    (u_long)ccb->ccb_data);

	if (!ccb->ccb_data)
		return -1;

	KASSERT(flags == BUS_DMA_NOWAIT || !cpu_intr_p());
	error = bus_dmamap_load(sc->sc_datadmat, ccb->ccb_dmamap,
	    ccb->ccb_data, ccb->ccb_len, NULL, flags);
	if (error) {
		if (error == EFBIG)
			aprint_error_dev(sc->sc_dev, "more than %d dma segs\n",
			    sc->sc_max_sgl);
		else
			aprint_error_dev(sc->sc_dev,
			    "error %d loading dma map\n", error);
		return -1;
	}

	io_req = ccb->ccb_tb_io_request;
	sgl_ptr = &io_req->SGL.IeeeChain.Chain64;
	sge_count = ccb->ccb_dmamap->dm_nsegs;
	sgd = ccb->ccb_dmamap->dm_segs;
	KASSERT(sge_count <= sc->sc_max_sgl);
	KASSERT(sge_count <=
	    (MEGASAS_THUNDERBOLT_MAX_SGE_IN_MAINMSG - 1 +
	     MEGASAS_THUNDERBOLT_MAX_SGE_IN_CHAINMSG));

	if (sge_count > MEGASAS_THUNDERBOLT_MAX_SGE_IN_MAINMSG) {
		/* One element to store the chain info */
		sge_idx = MEGASAS_THUNDERBOLT_MAX_SGE_IN_MAINMSG - 1;
		DNPRINTF(MFI_D_DMA,
		    "mfi sge_idx %d sge_count %d io_req paddr 0x%" PRIx64 "\n",
		    sge_idx, sge_count, ccb->ccb_tb_pio_request);
	} else {
		sge_idx = sge_count;
	}

	for (i = 0; i < sge_idx; i++) {
		sgl_ptr->Address = htole64(sgd[i].ds_addr);
		sgl_ptr->Length = htole32(sgd[i].ds_len);
		sgl_ptr->Flags = 0;
		if (sge_idx < sge_count) {
			DNPRINTF(MFI_D_DMA,
			    "sgl %p %d 0x%" PRIx64 " len 0x%" PRIx32
			    " flags 0x%x\n", sgl_ptr, i,
			    sgl_ptr->Address, sgl_ptr->Length,
			    sgl_ptr->Flags);
		}
		sgl_ptr++;
	}
	io_req->ChainOffset = 0;
	if (sge_idx < sge_count) {
		struct mpi25_ieee_sge_chain64 *sg_chain;
		io_req->ChainOffset = MEGASAS_THUNDERBOLT_CHAIN_OFF_MAINMSG;
		sg_chain = sgl_ptr;
		/* Prepare chain element */
		sg_chain->NextChainOffset = 0;
		sg_chain->Flags = (MPI2_IEEE_SGE_FLAGS_CHAIN_ELEMENT |
		    MPI2_IEEE_SGE_FLAGS_IOCPLBNTA_ADDR);
		sg_chain->Length =  (sizeof(mpi2_sge_io_union) *
		    (sge_count - sge_idx));
		sg_chain->Address = ccb->ccb_tb_psg_frame;
		DNPRINTF(MFI_D_DMA,
		    "sgl %p chain 0x%" PRIx64 " len 0x%" PRIx32
		    " flags 0x%x\n", sg_chain, sg_chain->Address,
		    sg_chain->Length, sg_chain->Flags);
		sgl_ptr = &ccb->ccb_tb_sg_frame->IeeeChain.Chain64;
		for (; i < sge_count; i++) {
			sgl_ptr->Address = htole64(sgd[i].ds_addr);
			sgl_ptr->Length = htole32(sgd[i].ds_len);
			sgl_ptr->Flags = 0;
			DNPRINTF(MFI_D_DMA,
			    "sgl %p %d 0x%" PRIx64 " len 0x%" PRIx32
			    " flags 0x%x\n", sgl_ptr, i, sgl_ptr->Address,
			    sgl_ptr->Length, sgl_ptr->Flags);
			sgl_ptr++;
		}
		bus_dmamap_sync(sc->sc_dmat,
		    MFIMEM_MAP(sc->sc_tbolt_reqmsgpool), 
		    ccb->ccb_tb_psg_frame - MFIMEM_DVA(sc->sc_tbolt_reqmsgpool),
		    MEGASAS_MAX_SZ_CHAIN_FRAME,  BUS_DMASYNC_PREREAD);
	}

	if (ccb->ccb_direction == MFI_DATA_IN) {
		bus_dmamap_sync(sc->sc_datadmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
	} else {
		bus_dmamap_sync(sc->sc_datadmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);
	}
	return sge_count;
}

/*
 * The ThunderBolt HW has an option for the driver to directly
 * access the underlying disks and operate on the RAID.  To
 * do this there needs to be a capability to keep the RAID controller
 * and driver in sync.  The FreeBSD driver does not take advantage
 * of this feature since it adds a lot of complexity and slows down
 * performance.  Performance is gained by using the controller's
 * cache etc.
 *
 * Even though this driver doesn't access the disks directly, an
 * AEN like command is used to inform the RAID firmware to "sync"
 * with all LD's via the MFI_DCMD_LD_MAP_GET_INFO command.  This
 * command in write mode will return when the RAID firmware has
 * detected a change to the RAID state.  Examples of this type
 * of change are removing a disk.  Once the command returns then
 * the driver needs to acknowledge this and "sync" all LD's again.
 * This repeats until we shutdown.  Then we need to cancel this
 * pending command.
 *
 * If this is not done right the RAID firmware will not remove a
 * pulled drive and the RAID won't go degraded etc.  Effectively,
 * stopping any RAID mangement to functions.
 *
 * Doing another LD sync, requires the use of an event since the
 * driver needs to do a mfi_wait_command and can't do that in an
 * interrupt thread.
 *
 * The driver could get the RAID state via the MFI_DCMD_LD_MAP_GET_INFO
 * That requires a bunch of structure and it is simplier to just do
 * the MFI_DCMD_LD_GET_LIST versus walking the RAID map.
 */

void
mfi_tbolt_sync_map_info(struct work *w, void *v)
{
	struct mfi_softc *sc = v;
	int i;
	struct mfi_ccb *ccb = NULL;
	uint8_t mbox[MFI_MBOX_SIZE];
	struct mfi_ld *ld_sync = NULL;
	size_t ld_size;
	int s;

	DNPRINTF(MFI_D_SYNC, "%s: mfi_tbolt_sync_map_info\n", DEVNAME(sc));
again:
	s = splbio();
	if (sc->sc_ldsync_ccb != NULL) {
		splx(s);
		return;
	}

	if (mfi_mgmt_internal(sc, MR_DCMD_LD_GET_LIST, MFI_DATA_IN,
	    sizeof(sc->sc_ld_list), &sc->sc_ld_list, NULL, false)) {
		aprint_error_dev(sc->sc_dev, "MR_DCMD_LD_GET_LIST failed\n");
		goto err;
	}

	ld_size = sizeof(*ld_sync) * sc->sc_ld_list.mll_no_ld;
	
	ld_sync = malloc(ld_size, M_DEVBUF, M_WAITOK | M_ZERO);
	if (ld_sync == NULL) {
		aprint_error_dev(sc->sc_dev, "Failed to allocate sync\n");
		goto err;
	}
	for (i = 0; i < sc->sc_ld_list.mll_no_ld; i++) {
		ld_sync[i] = sc->sc_ld_list.mll_list[i].mll_ld;
	}

	if ((ccb = mfi_get_ccb(sc)) == NULL) {
		aprint_error_dev(sc->sc_dev, "Failed to get sync command\n");
		goto err;
	}
	sc->sc_ldsync_ccb = ccb;
	
	memset(mbox, 0, MFI_MBOX_SIZE);
	mbox[0] = sc->sc_ld_list.mll_no_ld;
	mbox[1] = MFI_DCMD_MBOX_PEND_FLAG;
	if (mfi_mgmt(ccb, NULL, MR_DCMD_LD_MAP_GET_INFO, MFI_DATA_OUT,
	    ld_size, ld_sync, mbox)) {
		aprint_error_dev(sc->sc_dev, "Failed to create sync command\n");
		goto err;
	}
	/*
	 * we won't sleep on this command, so we have to override
	 * the callback set up by mfi_mgmt()
	 */
	ccb->ccb_done = mfi_sync_map_complete;

	mfi_post(sc, ccb);
	splx(s);
	return;

err:
	if (ld_sync)
		free(ld_sync, M_DEVBUF);
	if (ccb)
		mfi_put_ccb(ccb);
	sc->sc_ldsync_ccb = NULL;
	splx(s);
	kpause("ldsyncp", 0, hz, NULL);
	goto again;
}

static void
mfi_sync_map_complete(struct mfi_ccb *ccb)
{
	struct mfi_softc *sc = ccb->ccb_sc;
	bool aborted = !sc->sc_running;

	DNPRINTF(MFI_D_SYNC, "%s: mfi_sync_map_complete\n",
	    DEVNAME(ccb->ccb_sc));
	KASSERT(sc->sc_ldsync_ccb == ccb);
	mfi_mgmt_done(ccb);
	free(ccb->ccb_data, M_DEVBUF);
	if (ccb->ccb_flags & MFI_CCB_F_ERR) {
		aprint_error_dev(sc->sc_dev, "sync command failed\n");
		aborted = true;
	}
	mfi_put_ccb(ccb);
	sc->sc_ldsync_ccb = NULL;

	/* set it up again so the driver can catch more events */
	if (!aborted) {
		workqueue_enqueue(sc->sc_ldsync_wq, &sc->sc_ldsync_wk, NULL);
	}
}

static int
mfifopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct mfi_softc *sc;

	if ((sc = device_lookup_private(&mfi_cd, minor(dev))) == NULL)
		return (ENXIO);
	return (0);
}

static int
mfifclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	return (0);
}

static int
mfifioctl(dev_t dev, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct mfi_softc *sc;
	struct mfi_ioc_packet *ioc = data;
	uint8_t *udata;
	struct mfi_ccb *ccb = NULL;
	int ctx, i, s, error;
	union mfi_sense_ptr sense_ptr;

	switch(cmd) {
	case MFI_CMD:
		sc = device_lookup_private(&mfi_cd, ioc->mfi_adapter_no);
		break;
	default:
		return ENOTTY;
	}
	if (sc == NULL)
		return (ENXIO);
	if (sc->sc_opened)
		return (EBUSY);

	switch(cmd) {
	case MFI_CMD:
		error = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL, data);
		if (error)
			return error;
		if (ioc->mfi_sge_count > MAX_IOCTL_SGE)
			return EINVAL;
		s = splbio();
		if ((ccb = mfi_get_ccb(sc)) == NULL)
			return ENOMEM;
		ccb->ccb_data = NULL;
		ctx = ccb->ccb_frame->mfr_header.mfh_context;
		memcpy(ccb->ccb_frame, ioc->mfi_frame.raw,
		   sizeof(*ccb->ccb_frame));
		ccb->ccb_frame->mfr_header.mfh_context = ctx;
		ccb->ccb_frame->mfr_header.mfh_scsi_status = 0;
		ccb->ccb_frame->mfr_header.mfh_pad0 = 0;
		ccb->ccb_frame_size =
		    (sizeof(union mfi_sgl) * ioc->mfi_sge_count) +
		    ioc->mfi_sgl_off;
		if (ioc->mfi_sge_count > 0) {
			ccb->ccb_sgl = (union mfi_sgl *)
			    &ccb->ccb_frame->mfr_bytes[ioc->mfi_sgl_off];
		}
		if (ccb->ccb_frame->mfr_header.mfh_flags & MFI_FRAME_DIR_READ)
			ccb->ccb_direction = MFI_DATA_IN;
		if (ccb->ccb_frame->mfr_header.mfh_flags & MFI_FRAME_DIR_WRITE)
			ccb->ccb_direction = MFI_DATA_OUT;
		ccb->ccb_len = ccb->ccb_frame->mfr_header.mfh_data_len;
		if (ccb->ccb_len > MAXPHYS) {
			error = ENOMEM;
			goto out;
		}
		if (ccb->ccb_len &&
		    (ccb->ccb_direction & (MFI_DATA_IN | MFI_DATA_OUT)) != 0) {
			udata = malloc(ccb->ccb_len, M_DEVBUF, M_WAITOK|M_ZERO);
			if (udata == NULL) {
				error = ENOMEM;
				goto out;
			}
			ccb->ccb_data = udata;
			if (ccb->ccb_direction & MFI_DATA_OUT) {
				for (i = 0; i < ioc->mfi_sge_count; i++) {
					error = copyin(ioc->mfi_sgl[i].iov_base,
					    udata, ioc->mfi_sgl[i].iov_len);
					if (error)
						goto out;
					udata = &udata[
					    ioc->mfi_sgl[i].iov_len];
				}
			}
			if (mfi_create_sgl(ccb, BUS_DMA_WAITOK)) {
				error = EIO;
				goto out;
			}
		}
		if (ccb->ccb_frame->mfr_header.mfh_cmd == MFI_CMD_PD_SCSI_IO) {
			ccb->ccb_frame->mfr_io.mif_sense_addr_lo =
			    htole32(ccb->ccb_psense);
			ccb->ccb_frame->mfr_io.mif_sense_addr_hi = 0;
		}
		ccb->ccb_done = mfi_mgmt_done;
		mfi_post(sc, ccb);
		while (ccb->ccb_state != MFI_CCB_DONE)
			tsleep(ccb, PRIBIO, "mfi_fioc", 0);

		if (ccb->ccb_direction & MFI_DATA_IN) {
			udata = ccb->ccb_data;
			for (i = 0; i < ioc->mfi_sge_count; i++) {
				error = copyout(udata,
				    ioc->mfi_sgl[i].iov_base,
				    ioc->mfi_sgl[i].iov_len);
				if (error)
					goto out;
				udata = &udata[
				    ioc->mfi_sgl[i].iov_len];
			}
		}
		if (ioc->mfi_sense_len) {
			memcpy(&sense_ptr.sense_ptr_data[0],
			&ioc->mfi_frame.raw[ioc->mfi_sense_off],
			sizeof(sense_ptr.sense_ptr_data));
			error = copyout(ccb->ccb_sense,
			    sense_ptr.user_space,
			    sizeof(sense_ptr.sense_ptr_data));
			if (error)
				goto out;
		}
		memcpy(ioc->mfi_frame.raw, ccb->ccb_frame,
		   sizeof(*ccb->ccb_frame));
		break;
	default:
		printf("mfifioctl unhandled cmd 0x%lx\n", cmd);
		return ENOTTY;
	}

out:
	if (ccb->ccb_data)
		free(ccb->ccb_data, M_DEVBUF);
	if (ccb)
		mfi_put_ccb(ccb);
	splx(s);
	return error;
}
