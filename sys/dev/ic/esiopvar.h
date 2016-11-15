/*	$NetBSD: esiopvar.h,v 1.21 2012/08/24 09:01:22 msaitoh Exp $	*/

/*
 * Copyright (c) 2002 Manuel Bouyer.
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

/* structure and definitions for the siop driver */

/* Number of tag */
#define ESIOP_NTAG 256

/*
 * description of a command scheduler slot. The script uses a ring of
 * A_ncmd_slots of this.
 */
struct esiop_slot {
	uint32_t dsa; /* DSA of the xfer. The first 2 bits holds flags */
} __packed;

#define CMD_SLOTSIZE (sizeof(struct esiop_slot) / sizeof(uint32_t))

/*
 * xfer description of the script: tables and reselect script
 * In struct siop_common_cmd siop_xfer will point to this.
 * If you change this don't forget to update o_cmd_* and cmd_slot_size in script
 */
struct esiop_xfer {
	struct siop_common_xfer siop_tables;
	uint32_t tlq; /* target/lun/tag loaded in scratchC by script */
	uint32_t saved_offset;/* contains scratchA if script saved an offset */
} __packed;

#define ESIOP_XFER(cmd, m) (((struct esiop_xfer *)((cmd)->cmd_tables))->m)

/*
 * This describes a command handled by the SCSI controller
 * These are chained in either a free list or an active list
 * We have one queue per target
 */

struct esiop_cmd {
	TAILQ_ENTRY (esiop_cmd) next;
	struct siop_common_cmd cmd_c;
	struct esiop_cbd *esiop_cbdp; /* pointer to our siop_cbd */
};
#define cmd_tables cmd_c.siop_tables

/* command block descriptors: an array of siop_cmd + an array of siop_xfer */
struct esiop_cbd {
	TAILQ_ENTRY (esiop_cbd) next;
	struct esiop_cmd *cmds;
	struct esiop_xfer *xfers;
	bus_dmamap_t xferdma; /* DMA map for this block of xfers */
};

TAILQ_HEAD(cmd_list, esiop_cmd);
TAILQ_HEAD(cbd_list, esiop_cbd);

/* DSA table descriptor for tags. Free tables are in a list */
struct esiop_dsatbl {
	TAILQ_ENTRY (esiop_dsatbl) next;
	uint32_t *tbl; /* the table itself */
	uint32_t tbl_dsa; /* DSA of base of this table */
	bus_addr_t tbl_offset; /* offset of this table in the map */
	struct esiop_dsatblblk *tblblk; /* pointer back to our block */
};

/* DSA table block descriptor. */
struct esiop_dsatblblk {
	TAILQ_ENTRY (esiop_dsatblblk) next;
	bus_dmamap_t blkmap; /* DMA map of this block */
};

TAILQ_HEAD(tbl_list, esiop_dsatbl);
TAILQ_HEAD(tblblk_list, esiop_dsatblblk);

/* Number of table per block */
#define ESIOP_NTPB ((PAGE_SIZE) / (sizeof(uint32_t) * ESIOP_NTAG))

/* per lun struct */
struct esiop_lun {
	struct esiop_cmd *active; /* active non-tagged command */
	struct esiop_cmd *tactive[ESIOP_NTAG]; /* active tagged commands */
	int lun_flags; /* per-lun flags */
	struct esiop_dsatbl *lun_tagtbl; /* the tag DSA table */
};

/*
 * per target struct; siop_common_cmd->target and siop_common_softc->targets[]
 * will point to this
 */
struct esiop_target {
	struct siop_common_target target_c;
	struct esiop_lun *esiop_lun[8]; /* per-lun state */
	uint32_t lun_table_offset; /* pointer to our DSA table */
};

static __inline void esiop_table_sync(struct esiop_cmd *, int);
static __inline void
esiop_table_sync(struct esiop_cmd *esiop_cmd, int ops)
{
	struct siop_common_softc *sc  = esiop_cmd->cmd_c.siop_sc;
	bus_addr_t offset;

	offset = esiop_cmd->cmd_c.dsa -
	    esiop_cmd->esiop_cbdp->xferdma->dm_segs[0].ds_addr;
	bus_dmamap_sync(sc->sc_dmat, esiop_cmd->esiop_cbdp->xferdma, offset,
	    sizeof(struct esiop_xfer), ops);
}



/* Driver internal state */
struct esiop_softc {
	struct siop_common_softc sc_c;
	uint32_t sc_semoffset;		/* semaphore */
	uint32_t sc_shedoffset;		/* base of scheduler ring */
	int sc_currschedslot;		/* current scheduler slot */
	struct cbd_list cmds;		/* list of command block descriptors */
	struct cmd_list free_list;	/* cmd descr free list */
	struct tbl_list free_tagtbl;	/* list of free tag DSA tables */
	struct tblblk_list tag_tblblk;	/* tag DSA table blocks */
	uint32_t sc_flags;
	uint32_t sc_free_offset;	/* pointer to free RAM */
	uint32_t sc_target_table_offset;/* pointer to target DSA table */
	int sc_currdoneslot;		/* current done slot */
	bus_dmamap_t sc_done_map;	/* dma map for done ring (shared) */
	bus_addr_t sc_done_offset;	/* offset of ring in sc_done_map */
	uint32_t *sc_done_slot;		/* The done ring itself */
};

/* defs for sc_flags */
#define SCF_CHAN_NOSLOT	0x0001		/* channel out of scheduler slot */
#define SCF_CHAN_ADAPTREQ 0x0002	/* esiop_scsipi_request() is running */

void    esiop_attach(struct esiop_softc *);
int	esiop_intr(void *);
void	esiop_add_dev(struct esiop_softc *, int, int);
void	esiop_del_dev(struct esiop_softc *, int, int);
