/*	$NetBSD: mlyvar.h,v 1.6 2012/10/27 17:18:35 chs Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran, Thor Lancelot Simon, and Eric Haszlakiewicz.
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

/*-
 * Copyright (c) 2000, 2001 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from FreeBSD: mlyvar.h,v 1.3 2001/07/14 00:12:22 msmith Exp
 */

#ifndef _PCI_MLYVAR_H_
#define	_PCI_MLYVAR_H_

/*
 * The firmware interface allows for a 16-bit command identifier.  We cap
 * ourselves at a reasonable limit.  Note that we reserve a small number of
 * CCBs for control operations.
 */
#define	MLY_MAX_CCBS	256
#define	MLY_CCBS_RESV	4

/*
 * The firmware interface allows for a 16-bit s/g list length.  We limit
 * ourselves to a reasonable maximum.
 */
#define	MLY_MAX_SEGS	17
#define	MLY_SGL_SIZE	(MLY_MAX_SEGS * sizeof(struct mly_sg_entry))

#define	MLY_MAX_XFER	((MLY_MAX_SEGS - 1) * PAGE_SIZE)

/*
 * The interval at which we poke the controller for status updates (in
 * seconds).
 */
#define	MLY_PERIODIC_INTERVAL	5

/*
 * Command slot regulation.  We can't use slot 0 due to the memory mailbox
 * implementation.
 */
#define	MLY_SLOT_START		1
#define	MLY_SLOT_MAX		(MLY_SLOT_START + MLY_MAX_CCBS)

/*
 * Per-device structure, used to save persistent state on devices.
 *
 * Note that this isn't really Bus/Target/Lun since we don't support lun !=
 * 0 at this time.
 */
struct mly_btl {
	int	mb_flags;
	int	mb_state;		/* See 8.1 */
	int	mb_type;		/* See 8.2 */

	/* Physical devices only. */
	int	mb_speed;		/* Interface transfer rate */
	int	mb_width;		/* Interface width */
};
#define	MLY_BTL_PHYSICAL	0x01	/* physical device */
#define	MLY_BTL_LOGICAL		0x02	/* logical device */
#define	MLY_BTL_PROTECTED	0x04	/* I/O not allowed */
#define	MLY_BTL_TQING		0x08	/* tagged queueing */
#define	MLY_BTL_SCANNING	0x10	/* scan in progress */
#define	MLY_BTL_RESCAN		0x20	/* need to re-scan */

/*
 * Per-command context.
 */
struct mly_softc;

struct mly_ccb {
	union {
		SLIST_ENTRY(mly_ccb)	slist;
		SIMPLEQ_ENTRY(mly_ccb)	simpleq;
	} mc_link;			/* list linkage */

	u_int		mc_slot;	/* command slot we occupy */
	u_int		mc_flags;	/* status flags */
	u_int		mc_status;	/* command completion status */
	u_int		mc_sense;	/* sense data length */
	int32_t		mc_resid;	/* I/O residual count */

	union mly_cmd_packet *mc_packet;/* our controller command */
	bus_addr_t	mc_packetphys;	/* physical address of the mapped packet */

	void		*mc_data;	/* data buffer */
	size_t		mc_length;	/* data length */
	bus_dmamap_t	mc_datamap;	/* DMA map for data */
	u_int		mc_sgoff;	/* S/G list offset */

	void		(*mc_complete)(struct mly_softc *, struct mly_ccb *);
	void		*mc_private;
};
#define	MLY_CCB_DATAIN		0x01
#define	MLY_CCB_DATAOUT		0x02
#define	MLY_CCB_MAPPED		0x04
#define	MLY_CCB_COMPLETE	0x08

/*
 * Per-controller context.
 */
struct mly_softc {
	/* Generic device info. */
	device_t		mly_dv;
	bus_space_handle_t	mly_ioh;
	bus_space_tag_t		mly_iot;
	bus_dma_tag_t		mly_dmat;
	void			*mly_ih;

	/* Scatter-gather lists. */
	struct mly_sg_entry	*mly_sg;
	bus_addr_t		mly_sg_busaddr;
	bus_dma_tag_t		mly_sg_dmat;
	bus_dmamap_t		mly_sg_dmamap;
	bus_dma_segment_t	mly_sg_seg;

	/* Memory mailbox. */
	struct mly_mmbox	*mly_mmbox;
	bus_addr_t		mly_mmbox_busaddr;
	bus_dma_tag_t		mly_mmbox_dmat;
	bus_dmamap_t		mly_mmbox_dmamap;
	bus_dma_segment_t	mly_mmbox_seg;
	u_int			mly_mmbox_cmd_idx;
	u_int			mly_mmbox_sts_idx;

	/* Command packets. */
	union mly_cmd_packet	*mly_pkt;
	bus_addr_t		mly_pkt_busaddr;
	bus_dma_tag_t		mly_pkt_dmat;
	bus_dmamap_t		mly_pkt_dmamap;
	bus_dma_segment_t	mly_pkt_seg;

	/* Command management. */
	struct mly_ccb		*mly_ccbs;
	SLIST_HEAD(,mly_ccb)	mly_ccb_free;
	SIMPLEQ_HEAD(,mly_ccb)	mly_ccb_queue;
	u_int			mly_ncmds;

	/* Controller hardware interface. */
	u_int			mly_hwif;
	u_int			mly_doorbell_true;
	u_int			mly_cmd_mailbox;
	u_int			mly_status_mailbox;
	u_int			mly_idbr;
	u_int			mly_odbr;
	u_int			mly_error_status;
	u_int			mly_interrupt_status;
	u_int			mly_interrupt_mask;

	/* Controller features, limits and status. */
	u_int			mly_state;
	struct mly_ioctl_getcontrollerinfo *mly_controllerinfo;
	struct mly_param_controller *mly_controllerparam;
	struct mly_btl		mly_btl[MLY_MAX_CHANNELS][MLY_MAX_TARGETS];

	/* Health monitoring. */
	u_int			mly_event_change;
	u_int			mly_event_counter;
	u_int			mly_event_waiting;
	struct lwp		*mly_thread;

	/* SCSI mid-layer connection. */
	struct scsipi_adapter	mly_adapt;
	struct scsipi_channel	mly_chans[MLY_MAX_CHANNELS];
	u_int			mly_nchans;
};
#define	MLY_HWIF_I960RX		0
#define	MLY_HWIF_STRONGARM	1

#define	MLY_STATE_OPEN		0x01
#define	MLY_STATE_MMBOX_ACTIVE	0x02
#define	MLY_STATE_INITOK	0x04

/*
 * Register access helpers.
 */

static __inline u_int8_t	mly_inb(struct mly_softc *, int);
static __inline u_int16_t	mly_inw(struct mly_softc *, int);
static __inline u_int32_t	mly_inl(struct mly_softc *, int);
static __inline void		mly_outb(struct mly_softc *, int, u_int8_t);
static __inline void		mly_outw(struct mly_softc *, int, u_int16_t);
static __inline void		mly_outl(struct mly_softc *, int, u_int32_t);
static __inline int		mly_idbr_true(struct mly_softc *, u_int8_t);
static __inline int		mly_odbr_true(struct mly_softc *, u_int8_t);
static __inline int		mly_error_valid(struct mly_softc *);

static __inline u_int8_t
mly_inb(struct mly_softc *mly, int off)
{

	bus_space_barrier(mly->mly_iot, mly->mly_ioh, off, 1,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_1(mly->mly_iot, mly->mly_ioh, off));
}

static __inline u_int16_t
mly_inw(struct mly_softc *mly, int off)
{

	bus_space_barrier(mly->mly_iot, mly->mly_ioh, off, 2,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_2(mly->mly_iot, mly->mly_ioh, off));
}

static __inline u_int32_t
mly_inl(struct mly_softc *mly, int off)
{

	bus_space_barrier(mly->mly_iot, mly->mly_ioh, off, 4,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(mly->mly_iot, mly->mly_ioh, off));
}

static __inline void
mly_outb(struct mly_softc *mly, int off, u_int8_t val)
{

	bus_space_write_1(mly->mly_iot, mly->mly_ioh, off, val);
	bus_space_barrier(mly->mly_iot, mly->mly_ioh, off, 1,
	    BUS_SPACE_BARRIER_WRITE);
}

static __inline void
mly_outw(struct mly_softc *mly, int off, u_int16_t val)
{

	bus_space_write_2(mly->mly_iot, mly->mly_ioh, off, val);
	bus_space_barrier(mly->mly_iot, mly->mly_ioh, off, 2,
	    BUS_SPACE_BARRIER_WRITE);
}

static __inline void
mly_outl(struct mly_softc *mly, int off, u_int32_t val)
{

	bus_space_write_4(mly->mly_iot, mly->mly_ioh, off, val);
	bus_space_barrier(mly->mly_iot, mly->mly_ioh, off, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

static __inline int
mly_idbr_true(struct mly_softc *mly, u_int8_t mask)
{
	u_int8_t val;

	val = mly_inb(mly, mly->mly_idbr) ^ mly->mly_doorbell_true;
	return ((val & mask) == mask);
}

static __inline int
mly_odbr_true(struct mly_softc *mly, u_int8_t mask)
{

	return ((mly_inb(mly, mly->mly_odbr) & mask) == mask);
}

static __inline int
mly_error_valid(struct mly_softc *mly)
{
	u_int8_t val;

	val = mly_inb(mly, mly->mly_error_status) ^ mly->mly_doorbell_true;
	return ((val & MLY_MSG_EMPTY) == 0);
}

/*
 * Bus/target/logical ID-related macros.
 */

#define	MLY_LOGDEV_ID(mly, bus, target)					\
    (((bus) - (mly)->mly_controllerinfo->physical_channels_present) *	\
    MLY_MAX_TARGETS + (target))

#define	MLY_LOGDEV_BUS(mly, logdev)					\
    (((logdev) / MLY_MAX_TARGETS) +					\
    (mly)->mly_controllerinfo->physical_channels_present)

#define	MLY_LOGDEV_TARGET(mly, logdev)					\
    ((logdev) % MLY_MAX_TARGETS)

#define	MLY_BUS_IS_VIRTUAL(mly, bus)					\
    ((bus) >= (mly)->mly_controllerinfo->physical_channels_present)

#define	MLY_BUS_IS_VALID(mly, bus)					\
    (((bus) < (mly)->mly_nchans))

#endif	/* !defined _PCI_MLYVAR_H_ */
