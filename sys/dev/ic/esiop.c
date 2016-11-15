/*	$NetBSD: esiop.c,v 1.57 2013/11/02 13:59:14 gson Exp $	*/

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

/* SYM53c7/8xx PCI-SCSI I/O Processors driver */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: esiop.c,v 1.57 2013/11/02 13:59:14 gson Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>

#include <machine/endian.h>
#include <sys/bus.h>

#include <dev/microcode/siop/esiop.out>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsipi_all.h>

#include <dev/scsipi/scsiconf.h>

#include <dev/ic/siopreg.h>
#include <dev/ic/siopvar_common.h>
#include <dev/ic/esiopvar.h>

#include "opt_siop.h"

/*
#define SIOP_DEBUG
#define SIOP_DEBUG_DR
#define SIOP_DEBUG_INTR
#define SIOP_DEBUG_SCHED
#define SIOP_DUMP_SCRIPT
*/

#define SIOP_STATS

#ifndef SIOP_DEFAULT_TARGET
#define SIOP_DEFAULT_TARGET 7
#endif

/* number of cmd descriptors per block */
#define SIOP_NCMDPB (PAGE_SIZE / sizeof(struct esiop_xfer))

void	esiop_reset(struct esiop_softc *);
void	esiop_checkdone(struct esiop_softc *);
void	esiop_handle_reset(struct esiop_softc *);
void	esiop_scsicmd_end(struct esiop_cmd *, int);
void	esiop_unqueue(struct esiop_softc *, int, int);
int	esiop_handle_qtag_reject(struct esiop_cmd *);
static void	esiop_start(struct esiop_softc *, struct esiop_cmd *);
void	esiop_timeout(void *);
void	esiop_scsipi_request(struct scsipi_channel *,
			scsipi_adapter_req_t, void *);
void	esiop_dump_script(struct esiop_softc *);
void	esiop_morecbd(struct esiop_softc *);
void	esiop_moretagtbl(struct esiop_softc *);
void	siop_add_reselsw(struct esiop_softc *, int);
void	esiop_target_register(struct esiop_softc *, uint32_t);

void    esiop_update_scntl3(struct esiop_softc *, struct siop_common_target *);

#ifdef SIOP_STATS
static int esiop_stat_intr = 0;
static int esiop_stat_intr_shortxfer = 0;
static int esiop_stat_intr_sdp = 0;
static int esiop_stat_intr_done = 0;
static int esiop_stat_intr_xferdisc = 0;
static int esiop_stat_intr_lunresel = 0;
static int esiop_stat_intr_qfull = 0;
void esiop_printstats(void);
#define INCSTAT(x) x++
#else
#define INCSTAT(x)
#endif

static inline void esiop_script_sync(struct esiop_softc *, int);
static inline void
esiop_script_sync(struct esiop_softc *sc, int ops)
{

	if ((sc->sc_c.features & SF_CHIP_RAM) == 0)
		bus_dmamap_sync(sc->sc_c.sc_dmat, sc->sc_c.sc_scriptdma, 0,
		    PAGE_SIZE, ops);
}

static inline uint32_t esiop_script_read(struct esiop_softc *, u_int);
static inline uint32_t
esiop_script_read(struct esiop_softc *sc, u_int offset)
{

	if (sc->sc_c.features & SF_CHIP_RAM) {
		return bus_space_read_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
		    offset * 4);
	} else {
		return le32toh(sc->sc_c.sc_script[offset]);
	}
}

static inline void esiop_script_write(struct esiop_softc *, u_int,
	uint32_t);
static inline void
esiop_script_write(struct esiop_softc *sc, u_int offset, uint32_t val)
{

	if (sc->sc_c.features & SF_CHIP_RAM) {
		bus_space_write_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
		    offset * 4, val);
	} else {
		sc->sc_c.sc_script[offset] = htole32(val);
	}
}

void
esiop_attach(struct esiop_softc *sc)
{
	struct esiop_dsatbl *tagtbl_donering;

	if (siop_common_attach(&sc->sc_c) != 0 )
		return;

	TAILQ_INIT(&sc->free_list);
	TAILQ_INIT(&sc->cmds);
	TAILQ_INIT(&sc->free_tagtbl);
	TAILQ_INIT(&sc->tag_tblblk);
	sc->sc_currschedslot = 0;
#ifdef SIOP_DEBUG
	aprint_debug_dev(sc->sc_c.sc_dev,
	    "script size = %d, PHY addr=0x%x, VIRT=%p\n",
	    (int)sizeof(esiop_script),
	    (uint32_t)sc->sc_c.sc_scriptaddr, sc->sc_c.sc_script);
#endif

	sc->sc_c.sc_adapt.adapt_max_periph = ESIOP_NTAG;
	sc->sc_c.sc_adapt.adapt_request = esiop_scsipi_request;

	/*
	 * get space for the CMD done slot. For this we use a tag table entry.
	 * It's the same size and allows us to not waste 3/4 of a page
	 */
#ifdef DIAGNOSTIC
	if (ESIOP_NTAG != A_ndone_slots) {
		aprint_error_dev(sc->sc_c.sc_dev,
		     "size of tag DSA table different from the done ring\n");
		return;
	}
#endif
	esiop_moretagtbl(sc);
	tagtbl_donering = TAILQ_FIRST(&sc->free_tagtbl);
	if (tagtbl_donering == NULL) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "no memory for command done ring\n");
		return;
	}
	TAILQ_REMOVE(&sc->free_tagtbl, tagtbl_donering, next);
	sc->sc_done_map = tagtbl_donering->tblblk->blkmap;
	sc->sc_done_offset = tagtbl_donering->tbl_offset;
	sc->sc_done_slot = &tagtbl_donering->tbl[0];

	/* Do a bus reset, so that devices fall back to narrow/async */
	siop_resetbus(&sc->sc_c);
	/*
	 * siop_reset() will reset the chip, thus clearing pending interrupts
	 */
	esiop_reset(sc);
#ifdef SIOP_DUMP_SCRIPT
	esiop_dump_script(sc);
#endif

	config_found(sc->sc_c.sc_dev, &sc->sc_c.sc_chan, scsiprint);
}

void
esiop_reset(struct esiop_softc *sc)
{
	int i, j;
	uint32_t addr;
	uint32_t msgin_addr, sem_addr;

	siop_common_reset(&sc->sc_c);

	/*
	 * we copy the script at the beggining of RAM. Then there is 4 bytes
	 * for messages in, and 4 bytes for semaphore
	 */
	sc->sc_free_offset = __arraycount(esiop_script);
	msgin_addr =
	    sc->sc_free_offset * sizeof(uint32_t) + sc->sc_c.sc_scriptaddr;
	sc->sc_free_offset += 1;
	sc->sc_semoffset = sc->sc_free_offset;
	sem_addr =
	    sc->sc_semoffset * sizeof(uint32_t) + sc->sc_c.sc_scriptaddr;
	sc->sc_free_offset += 1;
	/* then we have the scheduler ring */
	sc->sc_shedoffset = sc->sc_free_offset;
	sc->sc_free_offset += A_ncmd_slots * CMD_SLOTSIZE;
	/* then the targets DSA table */
	sc->sc_target_table_offset = sc->sc_free_offset;
	sc->sc_free_offset += sc->sc_c.sc_chan.chan_ntargets;
	/* copy and patch the script */
	if (sc->sc_c.features & SF_CHIP_RAM) {
		bus_space_write_region_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh, 0,
		    esiop_script,
		    __arraycount(esiop_script));
		for (j = 0; j < __arraycount(E_tlq_offset_Used); j++) {
			bus_space_write_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
			    E_tlq_offset_Used[j] * 4,
			    sizeof(struct siop_common_xfer));
		}
		for (j = 0; j < __arraycount(E_saved_offset_offset_Used); j++) {
			bus_space_write_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
			    E_saved_offset_offset_Used[j] * 4,
			    sizeof(struct siop_common_xfer) + 4);
		}
		for (j = 0; j < __arraycount(E_abs_msgin2_Used); j++) {
			bus_space_write_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
			    E_abs_msgin2_Used[j] * 4, msgin_addr);
		}
		for (j = 0; j < __arraycount(E_abs_sem_Used); j++) {
			bus_space_write_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
			    E_abs_sem_Used[j] * 4, sem_addr);
		}

		if (sc->sc_c.features & SF_CHIP_LED0) {
			bus_space_write_region_4(sc->sc_c.sc_ramt,
			    sc->sc_c.sc_ramh,
			    Ent_led_on1, esiop_led_on,
			    __arraycount(esiop_led_on));
			bus_space_write_region_4(sc->sc_c.sc_ramt,
			    sc->sc_c.sc_ramh,
			    Ent_led_on2, esiop_led_on,
			    __arraycount(esiop_led_on));
			bus_space_write_region_4(sc->sc_c.sc_ramt,
			    sc->sc_c.sc_ramh,
			    Ent_led_off, esiop_led_off,
			    __arraycount(esiop_led_off));
		}
	} else {
		for (j = 0; j < __arraycount(esiop_script); j++) {
			sc->sc_c.sc_script[j] = htole32(esiop_script[j]);
		}
		for (j = 0; j < __arraycount(E_tlq_offset_Used); j++) {
			sc->sc_c.sc_script[E_tlq_offset_Used[j]] =
			    htole32(sizeof(struct siop_common_xfer));
		}
		for (j = 0; j < __arraycount(E_saved_offset_offset_Used); j++) {
			sc->sc_c.sc_script[E_saved_offset_offset_Used[j]] =
			    htole32(sizeof(struct siop_common_xfer) + 4);
		}
		for (j = 0; j < __arraycount(E_abs_msgin2_Used); j++) {
			sc->sc_c.sc_script[E_abs_msgin2_Used[j]] =
			    htole32(msgin_addr);
		}
		for (j = 0; j < __arraycount(E_abs_sem_Used); j++) {
			sc->sc_c.sc_script[E_abs_sem_Used[j]] =
			    htole32(sem_addr);
		}

		if (sc->sc_c.features & SF_CHIP_LED0) {
			for (j = 0; j < __arraycount(esiop_led_on); j++)
				sc->sc_c.sc_script[
				    Ent_led_on1 / sizeof(esiop_led_on[0]) + j
				    ] = htole32(esiop_led_on[j]);
			for (j = 0; j < __arraycount(esiop_led_on); j++)
				sc->sc_c.sc_script[
				    Ent_led_on2 / sizeof(esiop_led_on[0]) + j
				    ] = htole32(esiop_led_on[j]);
			for (j = 0; j < __arraycount(esiop_led_off); j++)
				sc->sc_c.sc_script[
				    Ent_led_off / sizeof(esiop_led_off[0]) + j
				    ] = htole32(esiop_led_off[j]);
		}
	}
	/* get base of scheduler ring */
	addr = sc->sc_c.sc_scriptaddr + sc->sc_shedoffset * sizeof(uint32_t);
	/* init scheduler */
	for (i = 0; i < A_ncmd_slots; i++) {
		esiop_script_write(sc,
		    sc->sc_shedoffset + i * CMD_SLOTSIZE, A_f_cmd_free);
	}
	sc->sc_currschedslot = 0;
	bus_space_write_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_SCRATCHE, 0);
	bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_SCRATCHD, addr);
	/*
	 * 0x78000000 is a 'move data8 to reg'. data8 is the second
	 * octet, reg offset is the third.
	 */
	esiop_script_write(sc, Ent_cmdr0 / 4,
	    0x78640000 | ((addr & 0x000000ff) <<  8));
	esiop_script_write(sc, Ent_cmdr1 / 4,
	    0x78650000 | ((addr & 0x0000ff00)      ));
	esiop_script_write(sc, Ent_cmdr2 / 4,
	    0x78660000 | ((addr & 0x00ff0000) >>  8));
	esiop_script_write(sc, Ent_cmdr3 / 4,
	    0x78670000 | ((addr & 0xff000000) >> 16));
	/* done ring */
	for (i = 0; i < A_ndone_slots; i++)
		sc->sc_done_slot[i] = 0;
	bus_dmamap_sync(sc->sc_c.sc_dmat, sc->sc_done_map,
	    sc->sc_done_offset, A_ndone_slots * sizeof(uint32_t),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	addr = sc->sc_done_map->dm_segs[0].ds_addr + sc->sc_done_offset;
	sc->sc_currdoneslot = 0;
	bus_space_write_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_SCRATCHE + 2, 0);
	bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_SCRATCHF, addr);
	esiop_script_write(sc, Ent_doner0 / 4,
	    0x786c0000 | ((addr & 0x000000ff) <<  8));
	esiop_script_write(sc, Ent_doner1 / 4,
	    0x786d0000 | ((addr & 0x0000ff00)      ));
	esiop_script_write(sc, Ent_doner2 / 4,
	    0x786e0000 | ((addr & 0x00ff0000) >>  8));
	esiop_script_write(sc, Ent_doner3 / 4,
	    0x786f0000 | ((addr & 0xff000000) >> 16));

	/* set flags */
	bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_SCRATCHC, 0);
	/* write pointer of base of target DSA table */
	addr = (sc->sc_target_table_offset * sizeof(uint32_t)) +
	    sc->sc_c.sc_scriptaddr;
	esiop_script_write(sc, (Ent_load_targtable / 4) + 0,
	    esiop_script_read(sc,(Ent_load_targtable / 4) + 0) |
	    ((addr & 0x000000ff) <<  8));
	esiop_script_write(sc, (Ent_load_targtable / 4) + 2,
	    esiop_script_read(sc,(Ent_load_targtable / 4) + 2) |
	    ((addr & 0x0000ff00)      ));
	esiop_script_write(sc, (Ent_load_targtable / 4) + 4,
	    esiop_script_read(sc,(Ent_load_targtable / 4) + 4) |
	    ((addr & 0x00ff0000) >>  8));
	esiop_script_write(sc, (Ent_load_targtable / 4) + 6,
	    esiop_script_read(sc,(Ent_load_targtable / 4) + 6) |
	    ((addr & 0xff000000) >> 16));
#ifdef SIOP_DEBUG
	printf("%s: target table offset %d free offset %d\n",
	    device_xname(sc->sc_c.sc_dev), sc->sc_target_table_offset,
	    sc->sc_free_offset);
#endif

	/* register existing targets */
	for (i = 0; i < sc->sc_c.sc_chan.chan_ntargets; i++) {
		if (sc->sc_c.targets[i])
			esiop_target_register(sc, i);
	}
	/* start script */
	if ((sc->sc_c.features & SF_CHIP_RAM) == 0) {
		bus_dmamap_sync(sc->sc_c.sc_dmat, sc->sc_c.sc_scriptdma, 0,
		    PAGE_SIZE, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSP,
	    sc->sc_c.sc_scriptaddr + Ent_reselect);
}

#if 0
#define CALL_SCRIPT(ent) do {						\
	printf ("start script DSA 0x%lx DSP 0x%lx\n",			\
	    esiop_cmd->cmd_c.dsa,					\
	    sc->sc_c.sc_scriptaddr + ent);				\
	bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,		\
	    SIOP_DSP, sc->sc_c.sc_scriptaddr + ent);			\
} while (/* CONSTCOND */0)
#else
#define CALL_SCRIPT(ent) do {						\
	bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,		\
	    SIOP_DSP, sc->sc_c.sc_scriptaddr + ent);			\
} while (/* CONSTCOND */0)
#endif

int
esiop_intr(void *v)
{
	struct esiop_softc *sc = v;
	struct esiop_target *esiop_target;
	struct esiop_cmd *esiop_cmd;
	struct esiop_lun *esiop_lun;
	struct scsipi_xfer *xs;
	int istat, sist, sstat1, dstat = 0; /* XXX: gcc */
	uint32_t irqcode;
	int need_reset = 0;
	int offset, target, lun, tag;
	uint32_t tflags;
	uint32_t addr;
	int freetarget = 0;
	int slot;
	int retval = 0;

again:
	istat = bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_ISTAT);
	if ((istat & (ISTAT_INTF | ISTAT_DIP | ISTAT_SIP)) == 0) {
		return retval;
	}
	retval = 1;
	INCSTAT(esiop_stat_intr);
	esiop_checkdone(sc);
	if (istat & ISTAT_INTF) {
		bus_space_write_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
		    SIOP_ISTAT, ISTAT_INTF);
		goto again;
	}

	if ((istat &(ISTAT_DIP | ISTAT_SIP | ISTAT_ABRT)) ==
	    (ISTAT_DIP | ISTAT_ABRT)) {
		/* clear abort */
		bus_space_write_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
		    SIOP_ISTAT, 0);
	}

	/* get CMD from T/L/Q */
	tflags = bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
	    SIOP_SCRATCHC);
#ifdef SIOP_DEBUG_INTR
		printf("interrupt, istat=0x%x tflags=0x%x "
		    "DSA=0x%x DSP=0x%lx\n", istat, tflags,
		    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSA),
		    (u_long)(bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			SIOP_DSP) -
		    sc->sc_c.sc_scriptaddr));
#endif
	target = (tflags & A_f_c_target) ? ((tflags >> 8) & 0xff) : -1;
	if (target > sc->sc_c.sc_chan.chan_ntargets) target = -1;
	lun = (tflags & A_f_c_lun) ? ((tflags >> 16) & 0xff) : -1;
	if (lun > sc->sc_c.sc_chan.chan_nluns) lun = -1;
	tag = (tflags & A_f_c_tag) ? ((tflags >> 24) & 0xff) : -1;

	if (target >= 0 && lun >= 0) {
		esiop_target = (struct esiop_target *)sc->sc_c.targets[target];
		if (esiop_target == NULL) {
			printf("esiop_target (target %d) not valid\n", target);
			goto none;
		}
		esiop_lun = esiop_target->esiop_lun[lun];
		if (esiop_lun == NULL) {
			printf("esiop_lun (target %d lun %d) not valid\n",
			    target, lun);
			goto none;
		}
		esiop_cmd =
		    (tag >= 0) ? esiop_lun->tactive[tag] : esiop_lun->active;
		if (esiop_cmd == NULL) {
			printf("esiop_cmd (target %d lun %d tag %d)"
			    " not valid\n",
			    target, lun, tag);
			goto none;
		}
		xs = esiop_cmd->cmd_c.xs;
#ifdef DIAGNOSTIC
		if (esiop_cmd->cmd_c.status != CMDST_ACTIVE) {
			printf("esiop_cmd (target %d lun %d) "
			    "not active (%d)\n", target, lun,
			    esiop_cmd->cmd_c.status);
			goto none;
		}
#endif
		esiop_table_sync(esiop_cmd,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	} else {
none:
		xs = NULL;
		esiop_target = NULL;
		esiop_lun = NULL;
		esiop_cmd = NULL;
	}
	if (istat & ISTAT_DIP) {
		dstat = bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
		    SIOP_DSTAT);
		if (dstat & DSTAT_ABRT) {
			/* was probably generated by a bus reset IOCTL */
			if ((dstat & DSTAT_DFE) == 0)
				siop_clearfifo(&sc->sc_c);
			goto reset;
		}
		if (dstat & DSTAT_SSI) {
			printf("single step dsp 0x%08x dsa 0x08%x\n",
			    (int)(bus_space_read_4(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_DSP) -
			    sc->sc_c.sc_scriptaddr),
			    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
				SIOP_DSA));
			if ((dstat & ~(DSTAT_DFE | DSTAT_SSI)) == 0 &&
			    (istat & ISTAT_SIP) == 0) {
				bus_space_write_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_DCNTL,
				    bus_space_read_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_DCNTL) | DCNTL_STD);
			}
			return 1;
		}

		if (dstat & ~(DSTAT_SIR | DSTAT_DFE | DSTAT_SSI)) {
		printf("%s: DMA IRQ:", device_xname(sc->sc_c.sc_dev));
		if (dstat & DSTAT_IID)
			printf(" Illegal instruction");
		if (dstat & DSTAT_BF)
			printf(" bus fault");
		if (dstat & DSTAT_MDPE)
			printf(" parity");
		if (dstat & DSTAT_DFE)
			printf(" DMA fifo empty");
		else
			siop_clearfifo(&sc->sc_c);
		printf(", DSP=0x%x DSA=0x%x: ",
		    (int)(bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			SIOP_DSP) - sc->sc_c.sc_scriptaddr),
		    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSA));
		if (esiop_cmd)
			printf("T/L/Q=%d/%d/%d last msg_in=0x%x status=0x%x\n",
			    target, lun, tag, esiop_cmd->cmd_tables->msg_in[0],
			    le32toh(esiop_cmd->cmd_tables->status));
		else
			printf(" current T/L/Q invalid\n");
		need_reset = 1;
		}
	}
	if (istat & ISTAT_SIP) {
		if (istat & ISTAT_DIP)
			delay(10);
		/*
		 * Can't read sist0 & sist1 independently, or we have to
		 * insert delay
		 */
		sist = bus_space_read_2(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
		    SIOP_SIST0);
		sstat1 = bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
		    SIOP_SSTAT1);
#ifdef SIOP_DEBUG_INTR
		printf("scsi interrupt, sist=0x%x sstat1=0x%x "
		    "DSA=0x%x DSP=0x%lx\n", sist, sstat1,
		    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSA),
		    (u_long)(bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			SIOP_DSP) -
		    sc->sc_c.sc_scriptaddr));
#endif
		if (sist & SIST0_RST) {
			esiop_handle_reset(sc);
			/* no table to flush here */
			return 1;
		}
		if (sist & SIST0_SGE) {
			if (esiop_cmd)
				scsipi_printaddr(xs->xs_periph);
			else
				printf("%s:", device_xname(sc->sc_c.sc_dev));
			printf("scsi gross error\n");
			if (esiop_target)
				esiop_target->target_c.flags &= ~TARF_DT;
#ifdef SIOP_DEBUG
			printf("DSA=0x%x DSP=0x%lx\n",
			    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_DSA),
			    (u_long)(bus_space_read_4(sc->sc_c.sc_rt,
				sc->sc_c.sc_rh, SIOP_DSP) -
			    sc->sc_c.sc_scriptaddr));
			printf("SDID 0x%x SCNTL3 0x%x SXFER 0x%x SCNTL4 0x%x\n",
			    bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_SDID),
			    bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_SCNTL3),
			    bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_SXFER),
			    bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_SCNTL4));

#endif
			goto reset;
		}
		if ((sist & SIST0_MA) && need_reset == 0) {
			if (esiop_cmd) {
				int scratchc0;
				dstat = bus_space_read_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_DSTAT);
				/*
				 * first restore DSA, in case we were in a S/G
				 * operation.
				 */
				bus_space_write_4(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh,
				    SIOP_DSA, esiop_cmd->cmd_c.dsa);
				scratchc0 = bus_space_read_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_SCRATCHC);
				switch (sstat1 & SSTAT1_PHASE_MASK) {
				case SSTAT1_PHASE_STATUS:
				/*
				 * previous phase may be aborted for any reason
				 * ( for example, the target has less data to
				 * transfer than requested). Compute resid and
				 * just go to status, the command should
				 * terminate.
				 */
					INCSTAT(esiop_stat_intr_shortxfer);
					if (scratchc0 & A_f_c_data)
						siop_ma(&esiop_cmd->cmd_c);
					else if ((dstat & DSTAT_DFE) == 0)
						siop_clearfifo(&sc->sc_c);
					CALL_SCRIPT(Ent_status);
					return 1;
				case SSTAT1_PHASE_MSGIN:
				/*
				 * target may be ready to disconnect
				 * Compute resid which would be used later
				 * if a save data pointer is needed.
				 */
					INCSTAT(esiop_stat_intr_xferdisc);
					if (scratchc0 & A_f_c_data)
						siop_ma(&esiop_cmd->cmd_c);
					else if ((dstat & DSTAT_DFE) == 0)
						siop_clearfifo(&sc->sc_c);
					bus_space_write_1(sc->sc_c.sc_rt,
					    sc->sc_c.sc_rh, SIOP_SCRATCHC,
					    scratchc0 & ~A_f_c_data);
					CALL_SCRIPT(Ent_msgin);
					return 1;
				}
				aprint_error_dev(sc->sc_c.sc_dev,
				    "unexpected phase mismatch %d\n",
				    sstat1 & SSTAT1_PHASE_MASK);
			} else {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "phase mismatch without command\n");
			}
			need_reset = 1;
		}
		if (sist & SIST0_PAR) {
			/* parity error, reset */
			if (esiop_cmd)
				scsipi_printaddr(xs->xs_periph);
			else
				printf("%s:", device_xname(sc->sc_c.sc_dev));
			printf("parity error\n");
			if (esiop_target)
				esiop_target->target_c.flags &= ~TARF_DT;
			goto reset;
		}
		if ((sist & (SIST1_STO << 8)) && need_reset == 0) {
			/*
			 * selection time out, assume there's no device here
			 * We also have to update the ring pointer ourselve
			 */
			slot = bus_space_read_1(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_SCRATCHE);
			esiop_script_sync(sc,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
#ifdef SIOP_DEBUG_SCHED
			printf("sel timeout target %d, slot %d\n",
			    target, slot);
#endif
			/*
			 * mark this slot as free, and advance to next slot
			 */
			esiop_script_write(sc,
			    sc->sc_shedoffset + slot * CMD_SLOTSIZE,
			    A_f_cmd_free);
			addr = bus_space_read_4(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_SCRATCHD);
			if (slot < (A_ncmd_slots - 1)) {
				bus_space_write_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_SCRATCHE, slot + 1);
				addr = addr + sizeof(struct esiop_slot);
			} else {
				bus_space_write_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_SCRATCHE, 0);
				addr = sc->sc_c.sc_scriptaddr +
				    sc->sc_shedoffset * sizeof(uint32_t);
			}
			bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_SCRATCHD, addr);
			esiop_script_sync(sc,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			if (esiop_cmd) {
				esiop_cmd->cmd_c.status = CMDST_DONE;
				xs->error = XS_SELTIMEOUT;
				freetarget = 1;
				goto end;
			} else {
				printf("%s: selection timeout without "
				    "command, target %d (sdid 0x%x), "
				    "slot %d\n",
				    device_xname(sc->sc_c.sc_dev), target,
				    bus_space_read_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_SDID), slot);
				need_reset = 1;
			}
		}
		if (sist & SIST0_UDC) {
			/*
			 * unexpected disconnect. Usually the target signals
			 * a fatal condition this way. Attempt to get sense.
			 */
			 if (esiop_cmd) {
				esiop_cmd->cmd_tables->status =
				    htole32(SCSI_CHECK);
				goto end;
			}
			aprint_error_dev(sc->sc_c.sc_dev,
			    "unexpected disconnect without command\n");
			goto reset;
		}
		if (sist & (SIST1_SBMC << 8)) {
			/* SCSI bus mode change */
			if (siop_modechange(&sc->sc_c) == 0 || need_reset == 1)
				goto reset;
			if ((istat & ISTAT_DIP) && (dstat & DSTAT_SIR)) {
				/*
				 * we have a script interrupt, it will
				 * restart the script.
				 */
				goto scintr;
			}
			/*
			 * else we have to restart it ourselve, at the
			 * interrupted instruction.
			 */
			bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_DSP,
			    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_DSP) - 8);
			return 1;
		}
		/* Else it's an unhandled exception (for now). */
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unhandled scsi interrupt, sist=0x%x sstat1=0x%x "
		    "DSA=0x%x DSP=0x%x\n", sist,
		    bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			SIOP_SSTAT1),
		    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSA),
		    (int)(bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			SIOP_DSP) - sc->sc_c.sc_scriptaddr));
		if (esiop_cmd) {
			esiop_cmd->cmd_c.status = CMDST_DONE;
			xs->error = XS_SELTIMEOUT;
			goto end;
		}
		need_reset = 1;
	}
	if (need_reset) {
reset:
		/* fatal error, reset the bus */
		siop_resetbus(&sc->sc_c);
		/* no table to flush here */
		return 1;
	}

scintr:
	if ((istat & ISTAT_DIP) && (dstat & DSTAT_SIR)) { /* script interrupt */
		irqcode = bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
		    SIOP_DSPS);
#ifdef SIOP_DEBUG_INTR
		printf("script interrupt 0x%x\n", irqcode);
#endif
		/*
		 * no command, or an inactive command is only valid for a
		 * reselect interrupt
		 */
		if ((irqcode & 0x80) == 0) {
			if (esiop_cmd == NULL) {
				aprint_error_dev(sc->sc_c.sc_dev,
			"script interrupt (0x%x) with invalid DSA !!!\n",
				    irqcode);
				goto reset;
			}
			if (esiop_cmd->cmd_c.status != CMDST_ACTIVE) {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "command with invalid status "
				    "(IRQ code 0x%x current status %d) !\n",
				    irqcode, esiop_cmd->cmd_c.status);
				xs = NULL;
			}
		}
		switch(irqcode) {
		case A_int_err:
			printf("error, DSP=0x%x\n",
			    (int)(bus_space_read_4(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_DSP) -
			    sc->sc_c.sc_scriptaddr));
			if (xs) {
				xs->error = XS_SELTIMEOUT;
				goto end;
			} else {
				goto reset;
			}
		case A_int_msgin:
		{
			int msgin = bus_space_read_1(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_SFBR);
			if (msgin == MSG_MESSAGE_REJECT) {
				int msg, extmsg;
				if (esiop_cmd->cmd_tables->msg_out[0] & 0x80) {
					/*
					 * message was part of a identify +
					 * something else. Identify shouldn't
					 * have been rejected.
					 */
					msg =
					    esiop_cmd->cmd_tables->msg_out[1];
					extmsg =
					    esiop_cmd->cmd_tables->msg_out[3];
				} else {
					msg =
					    esiop_cmd->cmd_tables->msg_out[0];
					extmsg =
					    esiop_cmd->cmd_tables->msg_out[2];
				}
				if (msg == MSG_MESSAGE_REJECT) {
					/* MSG_REJECT  for a MSG_REJECT  !*/
					if (xs)
						scsipi_printaddr(xs->xs_periph);
					else
						printf("%s: ", device_xname(
						    sc->sc_c.sc_dev));
					printf("our reject message was "
					    "rejected\n");
					goto reset;
				}
				if (msg == MSG_EXTENDED &&
				    extmsg == MSG_EXT_WDTR) {
					/* WDTR rejected, initiate sync */
					if ((esiop_target->target_c.flags &
					   TARF_SYNC) == 0) {
						esiop_target->target_c.status =
						    TARST_OK;
						siop_update_xfer_mode(&sc->sc_c,
						    target);
						/* no table to flush here */
						CALL_SCRIPT(Ent_msgin_ack);
						return 1;
					}
					esiop_target->target_c.status =
					    TARST_SYNC_NEG;
					siop_sdtr_msg(&esiop_cmd->cmd_c, 0,
					    sc->sc_c.st_minsync,
					    sc->sc_c.maxoff);
					esiop_table_sync(esiop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return 1;
				} else if (msg == MSG_EXTENDED &&
				    extmsg == MSG_EXT_SDTR) {
					/* sync rejected */
					esiop_target->target_c.offset = 0;
					esiop_target->target_c.period = 0;
					esiop_target->target_c.status =
					    TARST_OK;
					siop_update_xfer_mode(&sc->sc_c,
					    target);
					/* no table to flush here */
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				} else if (msg == MSG_EXTENDED &&
				    extmsg == MSG_EXT_PPR) {
					/* PPR rejected */
					esiop_target->target_c.offset = 0;
					esiop_target->target_c.period = 0;
					esiop_target->target_c.status =
					    TARST_OK;
					siop_update_xfer_mode(&sc->sc_c,
					    target);
					/* no table to flush here */
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				} else if (msg == MSG_SIMPLE_Q_TAG ||
				    msg == MSG_HEAD_OF_Q_TAG ||
				    msg == MSG_ORDERED_Q_TAG) {
					if (esiop_handle_qtag_reject(
					    esiop_cmd) == -1)
						goto reset;
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				}
				if (xs)
					scsipi_printaddr(xs->xs_periph);
				else
					printf("%s: ",
					    device_xname(sc->sc_c.sc_dev));
				if (msg == MSG_EXTENDED) {
					printf("scsi message reject, extended "
					    "message sent was 0x%x\n", extmsg);
				} else {
					printf("scsi message reject, message "
					    "sent was 0x%x\n", msg);
				}
				/* no table to flush here */
				CALL_SCRIPT(Ent_msgin_ack);
				return 1;
			}
			if (msgin == MSG_IGN_WIDE_RESIDUE) {
			/* use the extmsgdata table to get the second byte */
				esiop_cmd->cmd_tables->t_extmsgdata.count =
				    htole32(1);
				esiop_table_sync(esiop_cmd,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
				CALL_SCRIPT(Ent_get_extmsgdata);
				return 1;
			}
			if (xs)
				scsipi_printaddr(xs->xs_periph);
			else
				printf("%s: ", device_xname(sc->sc_c.sc_dev));
			printf("unhandled message 0x%x\n", msgin);
			esiop_cmd->cmd_tables->msg_out[0] = MSG_MESSAGE_REJECT;
			esiop_cmd->cmd_tables->t_msgout.count= htole32(1);
			esiop_table_sync(esiop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_send_msgout);
			return 1;
		}
		case A_int_extmsgin:
#ifdef SIOP_DEBUG_INTR
			printf("extended message: msg 0x%x len %d\n",
			    esiop_cmd->cmd_tables->msg_in[2],
			    esiop_cmd->cmd_tables->msg_in[1]);
#endif
			if (esiop_cmd->cmd_tables->msg_in[1] >
			    sizeof(esiop_cmd->cmd_tables->msg_in) - 2)
				aprint_error_dev(sc->sc_c.sc_dev,
				    "extended message too big (%d)\n",
				    esiop_cmd->cmd_tables->msg_in[1]);
			esiop_cmd->cmd_tables->t_extmsgdata.count =
			    htole32(esiop_cmd->cmd_tables->msg_in[1] - 1);
			esiop_table_sync(esiop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_get_extmsgdata);
			return 1;
		case A_int_extmsgdata:
#ifdef SIOP_DEBUG_INTR
			{
			int i;
			printf("extended message: 0x%x, data:",
			    esiop_cmd->cmd_tables->msg_in[2]);
			for (i = 3; i < 2 + esiop_cmd->cmd_tables->msg_in[1];
			    i++)
				printf(" 0x%x",
				    esiop_cmd->cmd_tables->msg_in[i]);
			printf("\n");
			}
#endif
			if (esiop_cmd->cmd_tables->msg_in[0] ==
			    MSG_IGN_WIDE_RESIDUE) {
			/* we got the second byte of MSG_IGN_WIDE_RESIDUE */
				if (esiop_cmd->cmd_tables->msg_in[3] != 1)
					printf("MSG_IGN_WIDE_RESIDUE: "
					     "bad len %d\n",
					     esiop_cmd->cmd_tables->msg_in[3]);
				switch (siop_iwr(&esiop_cmd->cmd_c)) {
				case SIOP_NEG_MSGOUT:
					esiop_table_sync(esiop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return 1;
				case SIOP_NEG_ACK:
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				default:
					panic("invalid retval from "
					    "siop_iwr()");
				}
				return 1;
			}
			if (esiop_cmd->cmd_tables->msg_in[2] == MSG_EXT_PPR) {
				switch (siop_ppr_neg(&esiop_cmd->cmd_c)) {
				case SIOP_NEG_MSGOUT:
					esiop_update_scntl3(sc,
					    esiop_cmd->cmd_c.siop_target);
					esiop_table_sync(esiop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return 1;
				case SIOP_NEG_ACK:
					esiop_update_scntl3(sc,
					    esiop_cmd->cmd_c.siop_target);
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				default:
					panic("invalid retval from "
					    "siop_ppr_neg()");
				}
				return 1;
			}
			if (esiop_cmd->cmd_tables->msg_in[2] == MSG_EXT_WDTR) {
				switch (siop_wdtr_neg(&esiop_cmd->cmd_c)) {
				case SIOP_NEG_MSGOUT:
					esiop_update_scntl3(sc,
					    esiop_cmd->cmd_c.siop_target);
					esiop_table_sync(esiop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return 1;
				case SIOP_NEG_ACK:
					esiop_update_scntl3(sc,
					    esiop_cmd->cmd_c.siop_target);
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				default:
					panic("invalid retval from "
					    "siop_wdtr_neg()");
				}
				return 1;
			}
			if (esiop_cmd->cmd_tables->msg_in[2] == MSG_EXT_SDTR) {
				switch (siop_sdtr_neg(&esiop_cmd->cmd_c)) {
				case SIOP_NEG_MSGOUT:
					esiop_update_scntl3(sc,
					    esiop_cmd->cmd_c.siop_target);
					esiop_table_sync(esiop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return 1;
				case SIOP_NEG_ACK:
					esiop_update_scntl3(sc,
					    esiop_cmd->cmd_c.siop_target);
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				default:
					panic("invalid retval from "
					    "siop_sdtr_neg()");
				}
				return 1;
			}
			/* send a message reject */
			esiop_cmd->cmd_tables->msg_out[0] = MSG_MESSAGE_REJECT;
			esiop_cmd->cmd_tables->t_msgout.count = htole32(1);
			esiop_table_sync(esiop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_send_msgout);
			return 1;
		case A_int_disc:
			INCSTAT(esiop_stat_intr_sdp);
			offset = bus_space_read_1(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_SCRATCHA + 1);
#ifdef SIOP_DEBUG_DR
			printf("disconnect offset %d\n", offset);
#endif
			siop_sdp(&esiop_cmd->cmd_c, offset);
			/* we start again with no offset */
			ESIOP_XFER(esiop_cmd, saved_offset) =
			    htole32(SIOP_NOOFFSET);
			esiop_table_sync(esiop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_script_sched);
			return 1;
		case A_int_resfail:
			printf("reselect failed\n");
			CALL_SCRIPT(Ent_script_sched);
			return 1;
		case A_int_done:
			if (xs == NULL) {
				printf("%s: done without command\n",
				    device_xname(sc->sc_c.sc_dev));
				CALL_SCRIPT(Ent_script_sched);
				return 1;
			}
#ifdef SIOP_DEBUG_INTR
			printf("done, DSA=0x%lx target id 0x%x last msg "
			    "in=0x%x status=0x%x\n",
			    (u_long)esiop_cmd->cmd_c.dsa,
			    le32toh(esiop_cmd->cmd_tables->id),
			    esiop_cmd->cmd_tables->msg_in[0],
			    le32toh(esiop_cmd->cmd_tables->status));
#endif
			INCSTAT(esiop_stat_intr_done);
			esiop_cmd->cmd_c.status = CMDST_DONE;
			goto end;
		default:
			printf("unknown irqcode %x\n", irqcode);
			if (xs) {
				xs->error = XS_SELTIMEOUT;
				goto end;
			}
			goto reset;
		}
		return 1;
	}
	/*
	 * We just should't get there, but on some KVM virtual hosts,
	 * we do - see PR 48277.
	 */
	printf("esiop_intr: I shouldn't be there !\n");
	return 1;

end:
	/*
	 * restart the script now if command completed properly
	 * Otherwise wait for siop_scsicmd_end(), we may need to cleanup the
	 * queue
	 */
	xs->status = le32toh(esiop_cmd->cmd_tables->status);
#ifdef SIOP_DEBUG_INTR
	printf("esiop_intr end: status %d\n", xs->status);
#endif
	if (tag >= 0)
		esiop_lun->tactive[tag] = NULL;
	else
		esiop_lun->active = NULL;
	offset = bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
	    SIOP_SCRATCHA + 1);
	/*
	 * if we got a disconnect between the last data phase
	 * and the status phase, offset will be 0. In this
	 * case, cmd_tables->saved_offset will have the proper value
	 * if it got updated by the controller
	 */
	if (offset == 0 &&
	    ESIOP_XFER(esiop_cmd, saved_offset) != htole32(SIOP_NOOFFSET))
		offset =
		    (le32toh(ESIOP_XFER(esiop_cmd, saved_offset)) >> 8) & 0xff;

	esiop_scsicmd_end(esiop_cmd, offset);
	if (freetarget && esiop_target->target_c.status == TARST_PROBING)
		esiop_del_dev(sc, target, lun);
	CALL_SCRIPT(Ent_script_sched);
	return 1;
}

void
esiop_scsicmd_end(struct esiop_cmd *esiop_cmd, int offset)
{
	struct scsipi_xfer *xs = esiop_cmd->cmd_c.xs;
	struct esiop_softc *sc = (struct esiop_softc *)esiop_cmd->cmd_c.siop_sc;

	siop_update_resid(&esiop_cmd->cmd_c, offset);

	switch(xs->status) {
	case SCSI_OK:
		xs->error = XS_NOERROR;
		break;
	case SCSI_BUSY:
		xs->error = XS_BUSY;
		break;
	case SCSI_CHECK:
		xs->error = XS_BUSY;
		/* remove commands in the queue and scheduler */
		esiop_unqueue(sc, xs->xs_periph->periph_target,
		    xs->xs_periph->periph_lun);
		break;
	case SCSI_QUEUE_FULL:
		INCSTAT(esiop_stat_intr_qfull);
#ifdef SIOP_DEBUG
		printf("%s:%d:%d: queue full (tag %d)\n",
		    device_xname(sc->sc_c.sc_dev),
		    xs->xs_periph->periph_target,
		    xs->xs_periph->periph_lun, esiop_cmd->cmd_c.tag);
#endif
		xs->error = XS_BUSY;
		break;
	case SCSI_SIOP_NOCHECK:
		/*
		 * don't check status, xs->error is already valid
		 */
		break;
	case SCSI_SIOP_NOSTATUS:
		/*
		 * the status byte was not updated, cmd was
		 * aborted
		 */
		xs->error = XS_SELTIMEOUT;
		break;
	default:
		scsipi_printaddr(xs->xs_periph);
		printf("invalid status code %d\n", xs->status);
		xs->error = XS_DRIVER_STUFFUP;
	}
	if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
		bus_dmamap_sync(sc->sc_c.sc_dmat,
		    esiop_cmd->cmd_c.dmamap_data, 0,
		    esiop_cmd->cmd_c.dmamap_data->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_c.sc_dmat,
		    esiop_cmd->cmd_c.dmamap_data);
	}
	bus_dmamap_unload(sc->sc_c.sc_dmat, esiop_cmd->cmd_c.dmamap_cmd);
	if ((xs->xs_control & XS_CTL_POLL) == 0)
		callout_stop(&xs->xs_callout);
	esiop_cmd->cmd_c.status = CMDST_FREE;
	TAILQ_INSERT_TAIL(&sc->free_list, esiop_cmd, next);
#if 0
	if (xs->resid != 0)
		printf("resid %d datalen %d\n", xs->resid, xs->datalen);
#endif
	scsipi_done (xs);
}

void
esiop_checkdone(struct esiop_softc *sc)
{
	int target, lun, tag;
	struct esiop_target *esiop_target;
	struct esiop_lun *esiop_lun;
	struct esiop_cmd *esiop_cmd;
	uint32_t slot;
	int needsync = 0;
	int status;
	uint32_t sem, offset;

	esiop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	sem = esiop_script_read(sc, sc->sc_semoffset);
	esiop_script_write(sc, sc->sc_semoffset, sem & ~A_sem_done);
	if ((sc->sc_flags & SCF_CHAN_NOSLOT) && (sem & A_sem_start)) {
		/*
		 * at last one command have been started,
		 * so we should have free slots now
		 */
		sc->sc_flags &= ~SCF_CHAN_NOSLOT;
		scsipi_channel_thaw(&sc->sc_c.sc_chan, 1);
	}
	esiop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if ((sem & A_sem_done) == 0) {
		/* no pending done command */
		return;
	}

	bus_dmamap_sync(sc->sc_c.sc_dmat, sc->sc_done_map,
	    sc->sc_done_offset, A_ndone_slots * sizeof(uint32_t),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
next:
	if (sc->sc_done_slot[sc->sc_currdoneslot] == 0) {
		if (needsync)
			bus_dmamap_sync(sc->sc_c.sc_dmat, sc->sc_done_map,
			    sc->sc_done_offset,
			    A_ndone_slots * sizeof(uint32_t),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		return;
	}

	needsync = 1;

	slot = htole32(sc->sc_done_slot[sc->sc_currdoneslot]);
	sc->sc_done_slot[sc->sc_currdoneslot] = 0;
	sc->sc_currdoneslot += 1;
	if (sc->sc_currdoneslot == A_ndone_slots)
		sc->sc_currdoneslot = 0;

	target =  (slot & A_f_c_target) ? (slot >> 8) & 0xff : -1;
	lun =  (slot & A_f_c_lun) ? (slot >> 16) & 0xff : -1;
	tag =  (slot & A_f_c_tag) ? (slot >> 24) & 0xff : -1;

	esiop_target = (target >= 0) ?
	    (struct esiop_target *)sc->sc_c.targets[target] : NULL;
	if (esiop_target == NULL) {
		printf("esiop_target (target %d) not valid\n", target);
		goto next;
	}
	esiop_lun = (lun >= 0) ? esiop_target->esiop_lun[lun] : NULL;
	if (esiop_lun == NULL) {
		printf("esiop_lun (target %d lun %d) not valid\n",
		    target, lun);
		goto next;
	}
	esiop_cmd = (tag >= 0) ? esiop_lun->tactive[tag] : esiop_lun->active;
	if (esiop_cmd == NULL) {
		printf("esiop_cmd (target %d lun %d tag %d) not valid\n",
		    target, lun, tag);
			goto next;
	}

	esiop_table_sync(esiop_cmd,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	status = le32toh(esiop_cmd->cmd_tables->status);
#ifdef DIAGNOSTIC
	if (status != SCSI_OK) {
		printf("command for T/L/Q %d/%d/%d status %d\n",
		    target, lun, tag, status);
		goto next;
	}

#endif
	/* Ok, this command has been handled */
	esiop_cmd->cmd_c.xs->status = status;
	if (tag >= 0)
		esiop_lun->tactive[tag] = NULL;
	else
		esiop_lun->active = NULL;
	/*
	 * scratcha was eventually saved in saved_offset by script.
	 * fetch offset from it
	 */
	offset = 0;
	if (ESIOP_XFER(esiop_cmd, saved_offset) != htole32(SIOP_NOOFFSET))
		offset =
		    (le32toh(ESIOP_XFER(esiop_cmd, saved_offset)) >> 8) & 0xff;
	esiop_scsicmd_end(esiop_cmd, offset);
	goto next;
}

void
esiop_unqueue(struct esiop_softc *sc, int target, int lun)
{
	int slot, tag;
	uint32_t slotdsa;
	struct esiop_cmd *esiop_cmd;
	struct esiop_lun *esiop_lun =
	    ((struct esiop_target *)sc->sc_c.targets[target])->esiop_lun[lun];

	/* first make sure to read valid data */
	esiop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (tag = 0; tag < ESIOP_NTAG; tag++) {
		/* look for commands in the scheduler, not yet started */
		if (esiop_lun->tactive[tag] == NULL)
			continue;
		esiop_cmd = esiop_lun->tactive[tag];
		for (slot = 0; slot < A_ncmd_slots; slot++) {
			slotdsa = esiop_script_read(sc,
			    sc->sc_shedoffset + slot * CMD_SLOTSIZE);
			/* if the slot has any flag, it won't match the DSA */
			if (slotdsa == esiop_cmd->cmd_c.dsa) { /* found it */
				/* Mark this slot as ignore */
				esiop_script_write(sc,
				    sc->sc_shedoffset + slot * CMD_SLOTSIZE,
				    esiop_cmd->cmd_c.dsa | A_f_cmd_ignore);
				/* ask to requeue */
				esiop_cmd->cmd_c.xs->error = XS_REQUEUE;
				esiop_cmd->cmd_c.xs->status = SCSI_SIOP_NOCHECK;
				esiop_lun->tactive[tag] = NULL;
				esiop_scsicmd_end(esiop_cmd, 0);
				break;
			}
		}
	}
	esiop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

/*
 * handle a rejected queue tag message: the command will run untagged,
 * has to adjust the reselect script.
 */


int
esiop_handle_qtag_reject(struct esiop_cmd *esiop_cmd)
{
	struct esiop_softc *sc = (struct esiop_softc *)esiop_cmd->cmd_c.siop_sc;
	int target = esiop_cmd->cmd_c.xs->xs_periph->periph_target;
	int lun = esiop_cmd->cmd_c.xs->xs_periph->periph_lun;
	int tag = esiop_cmd->cmd_tables->msg_out[2];
	struct esiop_target *esiop_target =
	    (struct esiop_target*)sc->sc_c.targets[target];
	struct esiop_lun *esiop_lun = esiop_target->esiop_lun[lun];

#ifdef SIOP_DEBUG
	printf("%s:%d:%d: tag message %d (%d) rejected (status %d)\n",
	    device_xname(sc->sc_c.sc_dev), target, lun, tag,
	    esiop_cmd->cmd_c.tag, esiop_cmd->cmd_c.status);
#endif

	if (esiop_lun->active != NULL) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "untagged command already running for target %d "
		    "lun %d (status %d)\n",
		    target, lun, esiop_lun->active->cmd_c.status);
		return -1;
	}
	/* clear tag slot */
	esiop_lun->tactive[tag] = NULL;
	/* add command to non-tagged slot */
	esiop_lun->active = esiop_cmd;
	esiop_cmd->cmd_c.flags &= ~CMDFL_TAG;
	esiop_cmd->cmd_c.tag = -1;
	/* update DSA table */
	esiop_script_write(sc, esiop_target->lun_table_offset +
	    lun * 2 + A_target_luntbl / sizeof(uint32_t),
	    esiop_cmd->cmd_c.dsa);
	esiop_script_sync(sc, BUS_DMASYNC_PREREAD |  BUS_DMASYNC_PREWRITE);
	return 0;
}

/*
 * handle a bus reset: reset chip, unqueue all active commands, free all
 * target struct and report lossage to upper layer.
 * As the upper layer may requeue immediatly we have to first store
 * all active commands in a temporary queue.
 */
void
esiop_handle_reset(struct esiop_softc *sc)
{
	struct esiop_cmd *esiop_cmd;
	struct esiop_lun *esiop_lun;
	int target, lun, tag;
	/*
	 * scsi bus reset. reset the chip and restart
	 * the queue. Need to clean up all active commands
	 */
	printf("%s: scsi bus reset\n", device_xname(sc->sc_c.sc_dev));
	/* stop, reset and restart the chip */
	esiop_reset(sc);

	if (sc->sc_flags & SCF_CHAN_NOSLOT) {
		/* chip has been reset, all slots are free now */
		sc->sc_flags &= ~SCF_CHAN_NOSLOT;
		scsipi_channel_thaw(&sc->sc_c.sc_chan, 1);
	}
	/*
	 * Process all commands: first commands completes, then commands
	 * being executed
	 */
	esiop_checkdone(sc);
	for (target = 0; target < sc->sc_c.sc_chan.chan_ntargets; target++) {
		struct esiop_target *esiop_target =
		    (struct esiop_target *)sc->sc_c.targets[target];
		if (esiop_target == NULL)
			continue;
		for (lun = 0; lun < 8; lun++) {
			esiop_lun = esiop_target->esiop_lun[lun];
			if (esiop_lun == NULL)
				continue;
			for (tag = -1; tag <
			    ((sc->sc_c.targets[target]->flags & TARF_TAG) ?
			    ESIOP_NTAG : 0);
			    tag++) {
				if (tag >= 0)
					esiop_cmd = esiop_lun->tactive[tag];
				else
					esiop_cmd = esiop_lun->active;
				if (esiop_cmd == NULL)
					continue;
				scsipi_printaddr(
				    esiop_cmd->cmd_c.xs->xs_periph);
				printf("command with tag id %d reset\n", tag);
				esiop_cmd->cmd_c.xs->error =
				    (esiop_cmd->cmd_c.flags & CMDFL_TIMEOUT) ?
				    XS_TIMEOUT : XS_RESET;
				esiop_cmd->cmd_c.xs->status = SCSI_SIOP_NOCHECK;
				if (tag >= 0)
					esiop_lun->tactive[tag] = NULL;
				else
					esiop_lun->active = NULL;
				esiop_cmd->cmd_c.status = CMDST_DONE;
				esiop_scsicmd_end(esiop_cmd, 0);
			}
		}
		sc->sc_c.targets[target]->status = TARST_ASYNC;
		sc->sc_c.targets[target]->flags &= ~(TARF_ISWIDE | TARF_ISDT);
		sc->sc_c.targets[target]->period =
		    sc->sc_c.targets[target]->offset = 0;
		siop_update_xfer_mode(&sc->sc_c, target);
	}

	scsipi_async_event(&sc->sc_c.sc_chan, ASYNC_EVENT_RESET, NULL);
}

void
esiop_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct esiop_softc *sc = device_private(chan->chan_adapter->adapt_dev);
	struct esiop_cmd *esiop_cmd;
	struct esiop_target *esiop_target;
	int s, error, i;
	int target;
	int lun;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		target = periph->periph_target;
		lun = periph->periph_lun;

		s = splbio();
		/*
		 * first check if there are pending complete commands.
		 * this can free us some resources (in the rings for example).
		 * we have to lock it to avoid recursion.
		 */
		if ((sc->sc_flags & SCF_CHAN_ADAPTREQ) == 0) {
			sc->sc_flags |= SCF_CHAN_ADAPTREQ;
			esiop_checkdone(sc);
			sc->sc_flags &= ~SCF_CHAN_ADAPTREQ;
		}
#ifdef SIOP_DEBUG_SCHED
		printf("starting cmd for %d:%d tag %d(%d)\n", target, lun,
		    xs->xs_tag_type, xs->xs_tag_id);
#endif
		esiop_cmd = TAILQ_FIRST(&sc->free_list);
		if (esiop_cmd == NULL) {
			xs->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(xs);
			splx(s);
			return;
		}
		TAILQ_REMOVE(&sc->free_list, esiop_cmd, next);
#ifdef DIAGNOSTIC
		if (esiop_cmd->cmd_c.status != CMDST_FREE)
			panic("siop_scsicmd: new cmd not free");
#endif
		esiop_target = (struct esiop_target*)sc->sc_c.targets[target];
		if (esiop_target == NULL) {
#ifdef SIOP_DEBUG
			printf("%s: alloc siop_target for target %d\n",
				device_xname(sc->sc_c.sc_dev), target);
#endif
			sc->sc_c.targets[target] =
			    malloc(sizeof(struct esiop_target),
				M_DEVBUF, M_NOWAIT | M_ZERO);
			if (sc->sc_c.targets[target] == NULL) {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "can't malloc memory for "
				    "target %d\n",
				    target);
				xs->error = XS_RESOURCE_SHORTAGE;
				scsipi_done(xs);
				TAILQ_INSERT_TAIL(&sc->free_list,
				    esiop_cmd, next);
				splx(s);
				return;
			}
			esiop_target =
			    (struct esiop_target*)sc->sc_c.targets[target];
			esiop_target->target_c.status = TARST_PROBING;
			esiop_target->target_c.flags = 0;
			esiop_target->target_c.id =
			    sc->sc_c.clock_div << 24; /* scntl3 */
			esiop_target->target_c.id |=  target << 16; /* id */
			/* esiop_target->target_c.id |= 0x0 << 8; scxfer is 0 */

			for (i=0; i < 8; i++)
				esiop_target->esiop_lun[i] = NULL;
			esiop_target_register(sc, target);
		}
		if (esiop_target->esiop_lun[lun] == NULL) {
			esiop_target->esiop_lun[lun] =
			    malloc(sizeof(struct esiop_lun), M_DEVBUF,
			    M_NOWAIT|M_ZERO);
			if (esiop_target->esiop_lun[lun] == NULL) {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "can't alloc esiop_lun for "
				    "target %d lun %d\n",
				    target, lun);
				xs->error = XS_RESOURCE_SHORTAGE;
				scsipi_done(xs);
				TAILQ_INSERT_TAIL(&sc->free_list,
				    esiop_cmd, next);
				splx(s);
				return;
			}
		}
		esiop_cmd->cmd_c.siop_target = sc->sc_c.targets[target];
		esiop_cmd->cmd_c.xs = xs;
		esiop_cmd->cmd_c.flags = 0;
		esiop_cmd->cmd_c.status = CMDST_READY;

		/* load the DMA maps */
		error = bus_dmamap_load(sc->sc_c.sc_dmat,
		    esiop_cmd->cmd_c.dmamap_cmd,
		    xs->cmd, xs->cmdlen, NULL, BUS_DMA_NOWAIT);
		if (error) {
			aprint_error_dev(sc->sc_c.sc_dev,
			    "unable to load cmd DMA map: %d\n",
			    error);
			xs->error = (error == EAGAIN) ?
			    XS_RESOURCE_SHORTAGE : XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			esiop_cmd->cmd_c.status = CMDST_FREE;
			TAILQ_INSERT_TAIL(&sc->free_list, esiop_cmd, next);
			splx(s);
			return;
		}
		if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
			error = bus_dmamap_load(sc->sc_c.sc_dmat,
			    esiop_cmd->cmd_c.dmamap_data, xs->data, xs->datalen,
			    NULL, BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
			    ((xs->xs_control & XS_CTL_DATA_IN) ?
			     BUS_DMA_READ : BUS_DMA_WRITE));
			if (error) {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "unable to load data DMA map: %d\n",
				    error);
				xs->error = (error == EAGAIN) ?
				    XS_RESOURCE_SHORTAGE : XS_DRIVER_STUFFUP;
				scsipi_done(xs);
				bus_dmamap_unload(sc->sc_c.sc_dmat,
				    esiop_cmd->cmd_c.dmamap_cmd);
				esiop_cmd->cmd_c.status = CMDST_FREE;
				TAILQ_INSERT_TAIL(&sc->free_list,
				    esiop_cmd, next);
				splx(s);
				return;
			}
			bus_dmamap_sync(sc->sc_c.sc_dmat,
			    esiop_cmd->cmd_c.dmamap_data, 0,
			    esiop_cmd->cmd_c.dmamap_data->dm_mapsize,
			    (xs->xs_control & XS_CTL_DATA_IN) ?
			    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
		}
		bus_dmamap_sync(sc->sc_c.sc_dmat, esiop_cmd->cmd_c.dmamap_cmd,
		    0, esiop_cmd->cmd_c.dmamap_cmd->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		if (xs->xs_tag_type)
			esiop_cmd->cmd_c.tag = xs->xs_tag_id;
		else
			esiop_cmd->cmd_c.tag = -1;
		siop_setuptables(&esiop_cmd->cmd_c);
		ESIOP_XFER(esiop_cmd, saved_offset) = htole32(SIOP_NOOFFSET);
		ESIOP_XFER(esiop_cmd, tlq) = htole32(A_f_c_target | A_f_c_lun);
		ESIOP_XFER(esiop_cmd, tlq) |=
		    htole32((target << 8) | (lun << 16));
		if (esiop_cmd->cmd_c.flags & CMDFL_TAG) {
			ESIOP_XFER(esiop_cmd, tlq) |= htole32(A_f_c_tag);
			ESIOP_XFER(esiop_cmd, tlq) |=
			    htole32(esiop_cmd->cmd_c.tag << 24);
		}

		esiop_table_sync(esiop_cmd,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		esiop_start(sc, esiop_cmd);
		if (xs->xs_control & XS_CTL_POLL) {
			/* poll for command completion */
			while ((xs->xs_status & XS_STS_DONE) == 0) {
				delay(1000);
				esiop_intr(sc);
			}
		}
		splx(s);
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
#ifdef SIOP_DEBUG
		printf("%s grow resources (%d)\n",
		    device_xname(sc->sc_c.sc_dev),
		    sc->sc_c.sc_adapt.adapt_openings);
#endif
		esiop_morecbd(sc);
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
	    {
		struct scsipi_xfer_mode *xm = arg;
		if (sc->sc_c.targets[xm->xm_target] == NULL)
			return;
		s = splbio();
		if (xm->xm_mode & PERIPH_CAP_TQING) {
			sc->sc_c.targets[xm->xm_target]->flags |= TARF_TAG;
			/* allocate tag tables for this device */
			for (lun = 0;
			    lun < sc->sc_c.sc_chan.chan_nluns; lun++) {
				if (scsipi_lookup_periph(chan,
				    xm->xm_target, lun) != NULL)
					esiop_add_dev(sc, xm->xm_target, lun);
			}
		}
		if ((xm->xm_mode & PERIPH_CAP_WIDE16) &&
		    (sc->sc_c.features & SF_BUS_WIDE))
			sc->sc_c.targets[xm->xm_target]->flags |= TARF_WIDE;
		if (xm->xm_mode & PERIPH_CAP_SYNC)
			sc->sc_c.targets[xm->xm_target]->flags |= TARF_SYNC;
		if ((xm->xm_mode & PERIPH_CAP_DT) &&
		    (sc->sc_c.features & SF_CHIP_DT))
			sc->sc_c.targets[xm->xm_target]->flags |= TARF_DT;
		if ((xm->xm_mode &
		    (PERIPH_CAP_SYNC | PERIPH_CAP_WIDE16 | PERIPH_CAP_DT)) ||
		    sc->sc_c.targets[xm->xm_target]->status == TARST_PROBING)
			sc->sc_c.targets[xm->xm_target]->status = TARST_ASYNC;

		splx(s);
	    }
	}
}

static void
esiop_start(struct esiop_softc *sc, struct esiop_cmd *esiop_cmd)
{
	struct esiop_lun *esiop_lun;
	struct esiop_target *esiop_target;
	int timeout;
	int target, lun, slot;

	/*
	 * first make sure to read valid data
	 */
	esiop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * We use a circular queue here. sc->sc_currschedslot points to a
	 * free slot, unless we have filled the queue. Check this.
	 */
	slot = sc->sc_currschedslot;
	if ((esiop_script_read(sc, sc->sc_shedoffset + slot * CMD_SLOTSIZE) &
	    A_f_cmd_free) == 0) {
		/*
		 * no more free slot, no need to continue. freeze the queue
		 * and requeue this command.
		 */
		scsipi_channel_freeze(&sc->sc_c.sc_chan, 1);
		sc->sc_flags |= SCF_CHAN_NOSLOT;
		esiop_script_write(sc, sc->sc_semoffset,
		    esiop_script_read(sc, sc->sc_semoffset) & ~A_sem_start);
		esiop_script_sync(sc,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		esiop_cmd->cmd_c.xs->error = XS_REQUEUE;
		esiop_cmd->cmd_c.xs->status = SCSI_SIOP_NOCHECK;
		esiop_scsicmd_end(esiop_cmd, 0);
		return;
	}
	/* OK, we can use this slot */

	target = esiop_cmd->cmd_c.xs->xs_periph->periph_target;
	lun = esiop_cmd->cmd_c.xs->xs_periph->periph_lun;
	esiop_target = (struct esiop_target*)sc->sc_c.targets[target];
	esiop_lun = esiop_target->esiop_lun[lun];
	/* if non-tagged command active, panic: this shouldn't happen */
	if (esiop_lun->active != NULL) {
		panic("esiop_start: tagged cmd while untagged running");
	}
#ifdef DIAGNOSTIC
	/* sanity check the tag if needed */
	if (esiop_cmd->cmd_c.flags & CMDFL_TAG) {
		if (esiop_cmd->cmd_c.tag >= ESIOP_NTAG ||
		    esiop_cmd->cmd_c.tag < 0) {
			scsipi_printaddr(esiop_cmd->cmd_c.xs->xs_periph);
			printf(": tag id %d\n", esiop_cmd->cmd_c.tag);
			panic("esiop_start: invalid tag id");
		}
		if (esiop_lun->tactive[esiop_cmd->cmd_c.tag] != NULL)
			panic("esiop_start: tag not free");
	}
#endif
#ifdef SIOP_DEBUG_SCHED
	printf("using slot %d for DSA 0x%lx\n", slot,
	    (u_long)esiop_cmd->cmd_c.dsa);
#endif
	/* mark command as active */
	if (esiop_cmd->cmd_c.status == CMDST_READY)
		esiop_cmd->cmd_c.status = CMDST_ACTIVE;
	else
		panic("esiop_start: bad status");
	/* DSA table for reselect */
	if (esiop_cmd->cmd_c.flags & CMDFL_TAG) {
		esiop_lun->tactive[esiop_cmd->cmd_c.tag] = esiop_cmd;
		/* DSA table for reselect */
		esiop_lun->lun_tagtbl->tbl[esiop_cmd->cmd_c.tag] =
		    htole32(esiop_cmd->cmd_c.dsa);
		bus_dmamap_sync(sc->sc_c.sc_dmat,
		    esiop_lun->lun_tagtbl->tblblk->blkmap,
		    esiop_lun->lun_tagtbl->tbl_offset,
		    sizeof(uint32_t) * ESIOP_NTAG, BUS_DMASYNC_PREWRITE);
	} else {
		esiop_lun->active = esiop_cmd;
		esiop_script_write(sc,
		    esiop_target->lun_table_offset +
		    lun * 2 + A_target_luntbl / sizeof(uint32_t),
		    esiop_cmd->cmd_c.dsa);
	}
	/* scheduler slot: DSA */
	esiop_script_write(sc, sc->sc_shedoffset + slot * CMD_SLOTSIZE,
	    esiop_cmd->cmd_c.dsa);
	/* make sure SCRIPT processor will read valid data */
	esiop_script_sync(sc, BUS_DMASYNC_PREREAD |  BUS_DMASYNC_PREWRITE);
	/* handle timeout */
	if ((esiop_cmd->cmd_c.xs->xs_control & XS_CTL_POLL) == 0) {
		/* start exire timer */
		timeout = mstohz(esiop_cmd->cmd_c.xs->timeout);
		if (timeout == 0)
			timeout = 1;
		callout_reset( &esiop_cmd->cmd_c.xs->xs_callout,
		    timeout, esiop_timeout, esiop_cmd);
	}
	/* Signal script it has some work to do */
	bus_space_write_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
	    SIOP_ISTAT, ISTAT_SIGP);
	/* update the current slot, and wait for IRQ */
	sc->sc_currschedslot++;
	if (sc->sc_currschedslot >= A_ncmd_slots)
		sc->sc_currschedslot = 0;
}

void
esiop_timeout(void *v)
{
	struct esiop_cmd *esiop_cmd = v;
	struct esiop_softc *sc =
	    (struct esiop_softc *)esiop_cmd->cmd_c.siop_sc;
	int s;
#ifdef SIOP_DEBUG
	int slot, slotdsa;
#endif

	s = splbio();
	esiop_table_sync(esiop_cmd,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	scsipi_printaddr(esiop_cmd->cmd_c.xs->xs_periph);
#ifdef SIOP_DEBUG
	printf("command timeout (status %d)\n",
	    le32toh(esiop_cmd->cmd_tables->status));

	esiop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	for (slot = 0; slot < A_ncmd_slots; slot++) {
		slotdsa = esiop_script_read(sc,
		    sc->sc_shedoffset + slot * CMD_SLOTSIZE);
		if ((slotdsa & 0x01) == 0)
			printf("slot %d not free (0x%x)\n", slot, slotdsa);
	}
	printf("istat 0x%x ",
	    bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_ISTAT));
	printf("DSP 0x%lx DSA 0x%x\n",
	    (u_long)(bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSP)
	    - sc->sc_c.sc_scriptaddr),
	    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSA));
	(void)bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_CTEST2);
	printf("istat 0x%x\n",
	    bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_ISTAT));
#else
	printf("command timeout, CDB: ");
	scsipi_print_cdb(esiop_cmd->cmd_c.xs->cmd);
	printf("\n");
#endif
	/* reset the scsi bus */
	siop_resetbus(&sc->sc_c);

	/* deactivate callout */
	callout_stop(&esiop_cmd->cmd_c.xs->xs_callout);
	/*
	 * mark command has being timed out and just return;
	 * the bus reset will generate an interrupt,
	 * it will be handled in siop_intr()
	 */
	esiop_cmd->cmd_c.flags |= CMDFL_TIMEOUT;
	splx(s);
}

void
esiop_dump_script(struct esiop_softc *sc)
{
	int i;

	for (i = 0; i < PAGE_SIZE / 4; i += 2) {
		printf("0x%04x: 0x%08x 0x%08x", i * 4,
		    esiop_script_read(sc, i),
		    esiop_script_read(sc, i + 1));
		if ((esiop_script_read(sc, i) & 0xe0000000) == 0xc0000000) {
			i++;
			printf(" 0x%08x", esiop_script_read(sc, i + 1));
		}
		printf("\n");
	}
}

void
esiop_morecbd(struct esiop_softc *sc)
{
	int error, i, s;
	bus_dma_segment_t seg;
	int rseg;
	struct esiop_cbd *newcbd;
	struct esiop_xfer *xfer;
	bus_addr_t dsa;

	/* allocate a new list head */
	newcbd = malloc(sizeof(struct esiop_cbd), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (newcbd == NULL) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "can't allocate memory for command descriptors "
		    "head\n");
		return;
	}

	/* allocate cmd list */
	newcbd->cmds = malloc(sizeof(struct esiop_cmd) * SIOP_NCMDPB,
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (newcbd->cmds == NULL) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "can't allocate memory for command descriptors\n");
		goto bad3;
	}
	error = bus_dmamem_alloc(sc->sc_c.sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to allocate cbd DMA memory, error = %d\n",
		    error);
		goto bad2;
	}
	error = bus_dmamem_map(sc->sc_c.sc_dmat, &seg, rseg, PAGE_SIZE,
	    (void **)&newcbd->xfers, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to map cbd DMA memory, error = %d\n",
		    error);
		goto bad2;
	}
	error = bus_dmamap_create(sc->sc_c.sc_dmat, PAGE_SIZE, 1, PAGE_SIZE, 0,
	    BUS_DMA_NOWAIT, &newcbd->xferdma);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to create cbd DMA map, error = %d\n", error);
		goto bad1;
	}
	error = bus_dmamap_load(sc->sc_c.sc_dmat, newcbd->xferdma,
	    newcbd->xfers, PAGE_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to load cbd DMA map, error = %d\n", error);
		goto bad0;
	}
#ifdef SIOP_DEBUG
	aprint_debug_dev(sc->sc_c.sc_dev, "alloc newcdb at PHY addr 0x%lx\n",
	    (unsigned long)newcbd->xferdma->dm_segs[0].ds_addr);
#endif
	for (i = 0; i < SIOP_NCMDPB; i++) {
		error = bus_dmamap_create(sc->sc_c.sc_dmat, MAXPHYS, SIOP_NSG,
		    MAXPHYS, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &newcbd->cmds[i].cmd_c.dmamap_data);
		if (error) {
			aprint_error_dev(sc->sc_c.sc_dev,
			    "unable to create data DMA map for cbd: "
			    "error %d\n", error);
			goto bad0;
		}
		error = bus_dmamap_create(sc->sc_c.sc_dmat,
		    sizeof(struct scsipi_generic), 1,
		    sizeof(struct scsipi_generic), 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &newcbd->cmds[i].cmd_c.dmamap_cmd);
		if (error) {
			aprint_error_dev(sc->sc_c.sc_dev,
			    "unable to create cmd DMA map for cbd %d\n", error);
			goto bad0;
		}
		newcbd->cmds[i].cmd_c.siop_sc = &sc->sc_c;
		newcbd->cmds[i].esiop_cbdp = newcbd;
		xfer = &newcbd->xfers[i];
		newcbd->cmds[i].cmd_tables = (struct siop_common_xfer *)xfer;
		memset(newcbd->cmds[i].cmd_tables, 0,
		    sizeof(struct esiop_xfer));
		dsa = newcbd->xferdma->dm_segs[0].ds_addr +
		    i * sizeof(struct esiop_xfer);
		newcbd->cmds[i].cmd_c.dsa = dsa;
		newcbd->cmds[i].cmd_c.status = CMDST_FREE;
		xfer->siop_tables.t_msgout.count= htole32(1);
		xfer->siop_tables.t_msgout.addr = htole32(dsa);
		xfer->siop_tables.t_msgin.count= htole32(1);
		xfer->siop_tables.t_msgin.addr = htole32(dsa +
		    offsetof(struct siop_common_xfer, msg_in));
		xfer->siop_tables.t_extmsgin.count= htole32(2);
		xfer->siop_tables.t_extmsgin.addr = htole32(dsa +
		    offsetof(struct siop_common_xfer, msg_in) + 1);
		xfer->siop_tables.t_extmsgdata.addr = htole32(dsa +
		    offsetof(struct siop_common_xfer, msg_in) + 3);
		xfer->siop_tables.t_status.count= htole32(1);
		xfer->siop_tables.t_status.addr = htole32(dsa +
		    offsetof(struct siop_common_xfer, status));

		s = splbio();
		TAILQ_INSERT_TAIL(&sc->free_list, &newcbd->cmds[i], next);
		splx(s);
#ifdef SIOP_DEBUG
		printf("tables[%d]: in=0x%x out=0x%x status=0x%x\n", i,
		    le32toh(newcbd->cmds[i].cmd_tables->t_msgin.addr),
		    le32toh(newcbd->cmds[i].cmd_tables->t_msgout.addr),
		    le32toh(newcbd->cmds[i].cmd_tables->t_status.addr));
#endif
	}
	s = splbio();
	TAILQ_INSERT_TAIL(&sc->cmds, newcbd, next);
	sc->sc_c.sc_adapt.adapt_openings += SIOP_NCMDPB;
	splx(s);
	return;
bad0:
	bus_dmamap_unload(sc->sc_c.sc_dmat, newcbd->xferdma);
	bus_dmamap_destroy(sc->sc_c.sc_dmat, newcbd->xferdma);
bad1:
	bus_dmamem_free(sc->sc_c.sc_dmat, &seg, rseg);
bad2:
	free(newcbd->cmds, M_DEVBUF);
bad3:
	free(newcbd, M_DEVBUF);
}

void
esiop_moretagtbl(struct esiop_softc *sc)
{
	int error, i, j, s;
	bus_dma_segment_t seg;
	int rseg;
	struct esiop_dsatblblk *newtblblk;
	struct esiop_dsatbl *newtbls;
	uint32_t *tbls;

	/* allocate a new list head */
	newtblblk = malloc(sizeof(struct esiop_dsatblblk),
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (newtblblk == NULL) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "can't allocate memory for tag DSA table block\n");
		return;
	}

	/* allocate tbl list */
	newtbls = malloc(sizeof(struct esiop_dsatbl) * ESIOP_NTPB,
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (newtbls == NULL) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "can't allocate memory for command descriptors\n");
		goto bad3;
	}
	error = bus_dmamem_alloc(sc->sc_c.sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to allocate tbl DMA memory, error = %d\n", error);
		goto bad2;
	}
	error = bus_dmamem_map(sc->sc_c.sc_dmat, &seg, rseg, PAGE_SIZE,
	    (void *)&tbls, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to map tbls DMA memory, error = %d\n", error);
		goto bad2;
	}
	error = bus_dmamap_create(sc->sc_c.sc_dmat, PAGE_SIZE, 1, PAGE_SIZE, 0,
	    BUS_DMA_NOWAIT, &newtblblk->blkmap);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to create tbl DMA map, error = %d\n", error);
		goto bad1;
	}
	error = bus_dmamap_load(sc->sc_c.sc_dmat, newtblblk->blkmap,
	    tbls, PAGE_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to load tbl DMA map, error = %d\n", error);
		goto bad0;
	}
#ifdef SIOP_DEBUG
	printf("%s: alloc new tag DSA table at PHY addr 0x%lx\n",
	    device_xname(sc->sc_c.sc_dev),
	    (unsigned long)newtblblk->blkmap->dm_segs[0].ds_addr);
#endif
	for (i = 0; i < ESIOP_NTPB; i++) {
		newtbls[i].tblblk = newtblblk;
		newtbls[i].tbl = &tbls[i * ESIOP_NTAG];
		newtbls[i].tbl_offset = i * ESIOP_NTAG * sizeof(uint32_t);
		newtbls[i].tbl_dsa = newtblblk->blkmap->dm_segs[0].ds_addr +
		    newtbls[i].tbl_offset;
		for (j = 0; j < ESIOP_NTAG; j++)
			newtbls[i].tbl[j] = j;
		s = splbio();
		TAILQ_INSERT_TAIL(&sc->free_tagtbl, &newtbls[i], next);
		splx(s);
	}
	s = splbio();
	TAILQ_INSERT_TAIL(&sc->tag_tblblk, newtblblk, next);
	splx(s);
	return;
bad0:
	bus_dmamap_unload(sc->sc_c.sc_dmat, newtblblk->blkmap);
	bus_dmamap_destroy(sc->sc_c.sc_dmat, newtblblk->blkmap);
bad1:
	bus_dmamem_free(sc->sc_c.sc_dmat, &seg, rseg);
bad2:
	free(newtbls, M_DEVBUF);
bad3:
	free(newtblblk, M_DEVBUF);
}

void
esiop_update_scntl3(struct esiop_softc *sc,
    struct siop_common_target *_siop_target)
{
	struct esiop_target *esiop_target = (struct esiop_target *)_siop_target;

	esiop_script_write(sc, esiop_target->lun_table_offset,
	    esiop_target->target_c.id);
	esiop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

void
esiop_add_dev(struct esiop_softc *sc, int target, int lun)
{
	struct esiop_target *esiop_target =
	    (struct esiop_target *)sc->sc_c.targets[target];
	struct esiop_lun *esiop_lun = esiop_target->esiop_lun[lun];

	if (esiop_lun->lun_tagtbl != NULL)
		return; /* already allocated */

	/* we need a tag DSA table */
	esiop_lun->lun_tagtbl= TAILQ_FIRST(&sc->free_tagtbl);
	if (esiop_lun->lun_tagtbl == NULL) {
		esiop_moretagtbl(sc);
		esiop_lun->lun_tagtbl= TAILQ_FIRST(&sc->free_tagtbl);
		if (esiop_lun->lun_tagtbl == NULL) {
			/* no resources, run untagged */
			esiop_target->target_c.flags &= ~TARF_TAG;
			return;
		}
	}
	TAILQ_REMOVE(&sc->free_tagtbl, esiop_lun->lun_tagtbl, next);
	/* Update LUN DSA table */
	esiop_script_write(sc, esiop_target->lun_table_offset +
	   lun * 2 + A_target_luntbl_tag / sizeof(uint32_t),
	    esiop_lun->lun_tagtbl->tbl_dsa);
	esiop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

void
esiop_del_dev(struct esiop_softc *sc, int target, int lun)
{
	struct esiop_target *esiop_target;

#ifdef SIOP_DEBUG
		printf("%s:%d:%d: free lun sw entry\n",
		    device_xname(sc->sc_c.sc_dev), target, lun);
#endif
	if (sc->sc_c.targets[target] == NULL)
		return;
	esiop_target = (struct esiop_target *)sc->sc_c.targets[target];
	free(esiop_target->esiop_lun[lun], M_DEVBUF);
	esiop_target->esiop_lun[lun] = NULL;
}

void
esiop_target_register(struct esiop_softc *sc, uint32_t target)
{
	struct esiop_target *esiop_target =
	    (struct esiop_target *)sc->sc_c.targets[target];
	struct esiop_lun *esiop_lun;
	int lun;

	/* get a DSA table for this target */
	esiop_target->lun_table_offset = sc->sc_free_offset;
	sc->sc_free_offset += sc->sc_c.sc_chan.chan_nluns * 2 + 2;
#ifdef SIOP_DEBUG
	printf("%s: lun table for target %d offset %d free offset %d\n",
	    device_xname(sc->sc_c.sc_dev), target,
	    esiop_target->lun_table_offset,
	    sc->sc_free_offset);
#endif
	/* first 32 bytes are ID (for select) */
	esiop_script_write(sc, esiop_target->lun_table_offset,
	    esiop_target->target_c.id);
	/* Record this table in the target DSA table */
	esiop_script_write(sc,
	    sc->sc_target_table_offset + target,
	    (esiop_target->lun_table_offset * sizeof(uint32_t)) +
	    sc->sc_c.sc_scriptaddr);
	/* if we have a tag table, register it */
	for (lun = 0; lun < sc->sc_c.sc_chan.chan_nluns; lun++) {
		esiop_lun = esiop_target->esiop_lun[lun];
		if (esiop_lun == NULL)
			continue;
		if (esiop_lun->lun_tagtbl)
			esiop_script_write(sc, esiop_target->lun_table_offset +
			   lun * 2 + A_target_luntbl_tag / sizeof(uint32_t),
			    esiop_lun->lun_tagtbl->tbl_dsa);
	}
	esiop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

#ifdef SIOP_STATS
void
esiop_printstats(void)
{

	printf("esiop_stat_intr %d\n", esiop_stat_intr);
	printf("esiop_stat_intr_shortxfer %d\n", esiop_stat_intr_shortxfer);
	printf("esiop_stat_intr_xferdisc %d\n", esiop_stat_intr_xferdisc);
	printf("esiop_stat_intr_sdp %d\n", esiop_stat_intr_sdp);
	printf("esiop_stat_intr_done %d\n", esiop_stat_intr_done);
	printf("esiop_stat_intr_lunresel %d\n", esiop_stat_intr_lunresel);
	printf("esiop_stat_intr_qfull %d\n", esiop_stat_intr_qfull);
}
#endif
