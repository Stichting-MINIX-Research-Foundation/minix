/*	$NetBSD: siop.c,v 1.100 2013/11/02 13:59:14 gson Exp $	*/

/*
 * Copyright (c) 2000 Manuel Bouyer.
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
__KERNEL_RCSID(0, "$NetBSD: siop.c,v 1.100 2013/11/02 13:59:14 gson Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>

#include <machine/endian.h>
#include <sys/bus.h>

#include <dev/microcode/siop/siop.out>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsipi_all.h>

#include <dev/scsipi/scsiconf.h>

#include <dev/ic/siopreg.h>
#include <dev/ic/siopvar_common.h>
#include <dev/ic/siopvar.h>

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
#define SIOP_NCMDPB (PAGE_SIZE / sizeof(struct siop_xfer))

/* Number of scheduler slot (needs to match script) */
#define SIOP_NSLOTS 40

void	siop_reset(struct siop_softc *);
void	siop_handle_reset(struct siop_softc *);
int	siop_handle_qtag_reject(struct siop_cmd *);
void	siop_scsicmd_end(struct siop_cmd *);
void	siop_unqueue(struct siop_softc *, int, int);
static void	siop_start(struct siop_softc *, struct siop_cmd *);
void	siop_timeout(void *);
int	siop_scsicmd(struct scsipi_xfer *);
void	siop_scsipi_request(struct scsipi_channel *,
			scsipi_adapter_req_t, void *);
void	siop_dump_script(struct siop_softc *);
void	siop_morecbd(struct siop_softc *);
struct siop_lunsw *siop_get_lunsw(struct siop_softc *);
void	siop_add_reselsw(struct siop_softc *, int);
void	siop_update_scntl3(struct siop_softc *,
			struct siop_common_target *);

#ifdef SIOP_STATS
static int siop_stat_intr = 0;
static int siop_stat_intr_shortxfer = 0;
static int siop_stat_intr_sdp = 0;
static int siop_stat_intr_saveoffset = 0;
static int siop_stat_intr_done = 0;
static int siop_stat_intr_xferdisc = 0;
static int siop_stat_intr_lunresel = 0;
static int siop_stat_intr_qfull = 0;
void siop_printstats(void);
#define INCSTAT(x) x++
#else
#define INCSTAT(x)
#endif

static inline void siop_script_sync(struct siop_softc *, int);
static inline void
siop_script_sync(struct siop_softc *sc, int ops)
{

	if ((sc->sc_c.features & SF_CHIP_RAM) == 0)
		bus_dmamap_sync(sc->sc_c.sc_dmat, sc->sc_c.sc_scriptdma, 0,
		    PAGE_SIZE, ops);
}

static inline uint32_t siop_script_read(struct siop_softc *, u_int);
static inline uint32_t
siop_script_read(struct siop_softc *sc, u_int offset)
{

	if (sc->sc_c.features & SF_CHIP_RAM) {
		return bus_space_read_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
		    offset * 4);
	} else {
		return siop_ctoh32(&sc->sc_c, sc->sc_c.sc_script[offset]);
	}
}

static inline void siop_script_write(struct siop_softc *, u_int,
	uint32_t);
static inline void
siop_script_write(struct siop_softc *sc, u_int offset, uint32_t val)
{

	if (sc->sc_c.features & SF_CHIP_RAM) {
		bus_space_write_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
		    offset * 4, val);
	} else {
		sc->sc_c.sc_script[offset] = siop_htoc32(&sc->sc_c, val);
	}
}

void
siop_attach(struct siop_softc *sc)
{

	if (siop_common_attach(&sc->sc_c) != 0)
		return;

	TAILQ_INIT(&sc->free_list);
	TAILQ_INIT(&sc->cmds);
	TAILQ_INIT(&sc->lunsw_list);
	sc->sc_currschedslot = 0;
#ifdef SIOP_DEBUG
	printf("%s: script size = %d, PHY addr=0x%x, VIRT=%p\n",
	    device_xname(sc->sc_c.sc_dev), (int)sizeof(siop_script),
	    (uint32_t)sc->sc_c.sc_scriptaddr, sc->sc_c.sc_script);
#endif

	sc->sc_c.sc_adapt.adapt_max_periph = SIOP_NTAG - 1;
	sc->sc_c.sc_adapt.adapt_request = siop_scsipi_request;

	/* Do a bus reset, so that devices fall back to narrow/async */
	siop_resetbus(&sc->sc_c);
	/*
	 * siop_reset() will reset the chip, thus clearing pending interrupts
	 */
	siop_reset(sc);
#ifdef SIOP_DUMP_SCRIPT
	siop_dump_script(sc);
#endif

	config_found(sc->sc_c.sc_dev, &sc->sc_c.sc_chan, scsiprint);
}

void
siop_reset(struct siop_softc *sc)
{
	int i, j;
	struct siop_lunsw *lunsw;

	siop_common_reset(&sc->sc_c);

	/* copy and patch the script */
	if (sc->sc_c.features & SF_CHIP_RAM) {
		bus_space_write_region_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh, 0,
		    siop_script, __arraycount(siop_script));
		for (j = 0; j < __arraycount(E_abs_msgin_Used); j++) {
			bus_space_write_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
			    E_abs_msgin_Used[j] * 4,
			    sc->sc_c.sc_scriptaddr + Ent_msgin_space);
		}
		if (sc->sc_c.features & SF_CHIP_LED0) {
			bus_space_write_region_4(sc->sc_c.sc_ramt,
			    sc->sc_c.sc_ramh,
			    Ent_led_on1, siop_led_on,
			    __arraycount(siop_led_on));
			bus_space_write_region_4(sc->sc_c.sc_ramt,
			    sc->sc_c.sc_ramh,
			    Ent_led_on2, siop_led_on,
			    __arraycount(siop_led_on));
			bus_space_write_region_4(sc->sc_c.sc_ramt,
			    sc->sc_c.sc_ramh,
			    Ent_led_off, siop_led_off,
			    __arraycount(siop_led_off));
		}
	} else {
		for (j = 0; j < __arraycount(siop_script); j++) {
			sc->sc_c.sc_script[j] =
			    siop_htoc32(&sc->sc_c, siop_script[j]);
		}
		for (j = 0; j < __arraycount(E_abs_msgin_Used); j++) {
			sc->sc_c.sc_script[E_abs_msgin_Used[j]] =
			    siop_htoc32(&sc->sc_c,
				sc->sc_c.sc_scriptaddr + Ent_msgin_space);
		}
		if (sc->sc_c.features & SF_CHIP_LED0) {
			for (j = 0; j < __arraycount(siop_led_on); j++)
				sc->sc_c.sc_script[
				    Ent_led_on1 / sizeof(siop_led_on[0]) + j
				    ] = siop_htoc32(&sc->sc_c, siop_led_on[j]);
			for (j = 0; j < __arraycount(siop_led_on); j++)
				sc->sc_c.sc_script[
				    Ent_led_on2 / sizeof(siop_led_on[0]) + j
				    ] = siop_htoc32(&sc->sc_c, siop_led_on[j]);
			for (j = 0; j < __arraycount(siop_led_off); j++)
				sc->sc_c.sc_script[
				    Ent_led_off / sizeof(siop_led_off[0]) + j
				    ] = siop_htoc32(&sc->sc_c, siop_led_off[j]);
		}
	}
	sc->script_free_lo = __arraycount(siop_script);
	sc->script_free_hi = sc->sc_c.ram_size / 4;
	sc->sc_ntargets = 0;

	/* free used and unused lun switches */
	while((lunsw = TAILQ_FIRST(&sc->lunsw_list)) != NULL) {
#ifdef SIOP_DEBUG
		printf("%s: free lunsw at offset %d\n",
		    device_xname(sc->sc_c.sc_dev), lunsw->lunsw_off);
#endif
		TAILQ_REMOVE(&sc->lunsw_list, lunsw, next);
		free(lunsw, M_DEVBUF);
	}
	TAILQ_INIT(&sc->lunsw_list);
	/* restore reselect switch */
	for (i = 0; i < sc->sc_c.sc_chan.chan_ntargets; i++) {
		struct siop_target *target;
		if (sc->sc_c.targets[i] == NULL)
			continue;
#ifdef SIOP_DEBUG
		printf("%s: restore sw for target %d\n",
		    device_xname(sc->sc_c.sc_dev), i);
#endif
		target = (struct siop_target *)sc->sc_c.targets[i];
		free(target->lunsw, M_DEVBUF);
		target->lunsw = siop_get_lunsw(sc);
		if (target->lunsw == NULL) {
			aprint_error_dev(sc->sc_c.sc_dev,
			    "can't alloc lunsw for target %d\n", i);
			break;
		}
		siop_add_reselsw(sc, i);
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
	    siop_cmd->cmd_c.dsa,					\
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
siop_intr(void *v)
{
	struct siop_softc *sc = v;
	struct siop_target *siop_target;
	struct siop_cmd *siop_cmd;
	struct siop_lun *siop_lun;
	struct scsipi_xfer *xs;
	int istat, sist, sstat1, dstat = 0; /* XXX: gcc */
	uint32_t irqcode;
	int need_reset = 0;
	int offset, target, lun, tag;
	bus_addr_t dsa;
	struct siop_cbd *cbdp;
	int freetarget = 0;
	int restart = 0;

	istat = bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_ISTAT);
	if ((istat & (ISTAT_INTF | ISTAT_DIP | ISTAT_SIP)) == 0)
		return 0;
	INCSTAT(siop_stat_intr);
	if (istat & ISTAT_INTF) {
		printf("INTRF\n");
		bus_space_write_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
		    SIOP_ISTAT, ISTAT_INTF);
	}
	if ((istat &(ISTAT_DIP | ISTAT_SIP | ISTAT_ABRT)) ==
	    (ISTAT_DIP | ISTAT_ABRT)) {
		/* clear abort */
		bus_space_write_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
		    SIOP_ISTAT, 0);
	}
	/* use DSA to find the current siop_cmd */
	siop_cmd = NULL;
	dsa = bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSA);
	TAILQ_FOREACH(cbdp, &sc->cmds, next) {
		if (dsa >= cbdp->xferdma->dm_segs[0].ds_addr &&
		    dsa < cbdp->xferdma->dm_segs[0].ds_addr + PAGE_SIZE) {
			dsa -= cbdp->xferdma->dm_segs[0].ds_addr;
			siop_cmd = &cbdp->cmds[dsa / sizeof(struct siop_xfer)];
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			break;
		}
	}
	if (siop_cmd) {
		xs = siop_cmd->cmd_c.xs;
		siop_target = (struct siop_target *)siop_cmd->cmd_c.siop_target;
		target = siop_cmd->cmd_c.xs->xs_periph->periph_target;
		lun = siop_cmd->cmd_c.xs->xs_periph->periph_lun;
		tag = siop_cmd->cmd_c.tag;
		siop_lun = siop_target->siop_lun[lun];
#ifdef DIAGNOSTIC
		if (siop_cmd->cmd_c.status != CMDST_ACTIVE) {
			printf("siop_cmd (lun %d) for DSA 0x%x "
			    "not active (%d)\n", lun, (u_int)dsa,
			    siop_cmd->cmd_c.status);
			xs = NULL;
			siop_target = NULL;
			target = -1;
			lun = -1;
			tag = -1;
			siop_lun = NULL;
			siop_cmd = NULL;
		} else if (siop_lun->siop_tag[tag].active != siop_cmd) {
			printf("siop_cmd (lun %d tag %d) not in siop_lun "
			    "active (%p != %p)\n", lun, tag, siop_cmd,
			    siop_lun->siop_tag[tag].active);
		}
#endif
	} else {
		xs = NULL;
		siop_target = NULL;
		target = -1;
		lun = -1;
		tag = -1;
		siop_lun = NULL;
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
		printf("DMA IRQ:");
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
		if (siop_cmd)
			printf("last msg_in=0x%x status=0x%x\n",
			    siop_cmd->cmd_tables->msg_in[0],
			    siop_ctoh32(&sc->sc_c,
				siop_cmd->cmd_tables->status));
		else
			aprint_error_dev(sc->sc_c.sc_dev,
			    "current DSA invalid\n");
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
		    "DSA=0x%x DSP=0x%lx\n", sist,
		    bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			SIOP_SSTAT1),
		    bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh, SIOP_DSA),
		    (u_long)(bus_space_read_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			SIOP_DSP) -
		    sc->sc_c.sc_scriptaddr));
#endif
		if (sist & SIST0_RST) {
			siop_handle_reset(sc);
			/* no table to flush here */
			return 1;
		}
		if (sist & SIST0_SGE) {
			if (siop_cmd)
				scsipi_printaddr(xs->xs_periph);
			else
				printf("%s:", device_xname(sc->sc_c.sc_dev));
			printf("scsi gross error\n");
			goto reset;
		}
		if ((sist & SIST0_MA) && need_reset == 0) {
			if (siop_cmd) {
				int scratcha0;
				dstat = bus_space_read_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_DSTAT);
				/*
				 * first restore DSA, in case we were in a S/G
				 * operation.
				 */
				bus_space_write_4(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh,
				    SIOP_DSA, siop_cmd->cmd_c.dsa);
				scratcha0 = bus_space_read_1(sc->sc_c.sc_rt,
				    sc->sc_c.sc_rh, SIOP_SCRATCHA);
				switch (sstat1 & SSTAT1_PHASE_MASK) {
				case SSTAT1_PHASE_STATUS:
				/*
				 * previous phase may be aborted for any reason
				 * ( for example, the target has less data to
				 * transfer than requested). Compute resid and
				 * just go to status, the command should
				 * terminate.
				 */
					INCSTAT(siop_stat_intr_shortxfer);
					if (scratcha0 & A_flag_data)
						siop_ma(&siop_cmd->cmd_c);
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
					INCSTAT(siop_stat_intr_xferdisc);
					if (scratcha0 & A_flag_data)
						siop_ma(&siop_cmd->cmd_c);
					else if ((dstat & DSTAT_DFE) == 0)
						siop_clearfifo(&sc->sc_c);
					bus_space_write_1(sc->sc_c.sc_rt,
					    sc->sc_c.sc_rh, SIOP_SCRATCHA,
					    scratcha0 & ~A_flag_data);
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
			if (siop_cmd)
				scsipi_printaddr(xs->xs_periph);
			else
				printf("%s:", device_xname(sc->sc_c.sc_dev));
			printf("parity error\n");
			goto reset;
		}
		if ((sist & (SIST1_STO << 8)) && need_reset == 0) {
			/* selection time out, assume there's no device here */
			if (siop_cmd) {
				siop_cmd->cmd_c.status = CMDST_DONE;
				xs->error = XS_SELTIMEOUT;
				freetarget = 1;
				goto end;
			} else {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "selection timeout without "
				    "command\n");
				need_reset = 1;
			}
		}
		if (sist & SIST0_UDC) {
			/*
			 * unexpected disconnect. Usually the target signals
			 * a fatal condition this way. Attempt to get sense.
			 */
			 if (siop_cmd) {
				siop_cmd->cmd_tables->status =
				    siop_htoc32(&sc->sc_c, SCSI_CHECK);
				goto end;
			}
			aprint_error_dev(sc->sc_c.sc_dev,
			    "unexpected disconnect without "
			    "command\n");
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
		if (siop_cmd) {
			siop_cmd->cmd_c.status = CMDST_DONE;
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
			if (siop_cmd == NULL) {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "script interrupt (0x%x) with "
				    "invalid DSA !!!\n",
				    irqcode);
				goto reset;
			}
			if (siop_cmd->cmd_c.status != CMDST_ACTIVE) {
				aprint_error_dev(sc->sc_c.sc_dev,
				     "command with invalid status "
				    "(IRQ code 0x%x current status %d) !\n",
				    irqcode, siop_cmd->cmd_c.status);
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
		case A_int_reseltarg:
			aprint_error_dev(sc->sc_c.sc_dev,
			    "reselect with invalid target\n");
			goto reset;
		case A_int_resellun:
			INCSTAT(siop_stat_intr_lunresel);
			target = bus_space_read_1(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_SCRATCHA) & 0xf;
			lun = bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_SCRATCHA + 1);
			tag = bus_space_read_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_SCRATCHA + 2);
			siop_target =
			    (struct siop_target *)sc->sc_c.targets[target];
			if (siop_target == NULL) {
				printf("%s: reselect with invalid target %d\n",
				    device_xname(sc->sc_c.sc_dev), target);
				goto reset;
			}
			siop_lun = siop_target->siop_lun[lun];
			if (siop_lun == NULL) {
				printf("%s: target %d reselect with invalid "
				    "lun %d\n", device_xname(sc->sc_c.sc_dev),
				    target, lun);
				goto reset;
			}
			if (siop_lun->siop_tag[tag].active == NULL) {
				printf("%s: target %d lun %d tag %d reselect "
				    "without command\n",
				    device_xname(sc->sc_c.sc_dev),
				    target, lun, tag);
				goto reset;
			}
			siop_cmd = siop_lun->siop_tag[tag].active;
			bus_space_write_4(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
			    SIOP_DSP, siop_cmd->cmd_c.dsa +
			    sizeof(struct siop_common_xfer) +
			    Ent_ldsa_reload_dsa);
			siop_table_sync(siop_cmd, BUS_DMASYNC_PREWRITE);
			return 1;
		case A_int_reseltag:
			printf("%s: reselect with invalid tag\n",
			    device_xname(sc->sc_c.sc_dev));
			goto reset;
		case A_int_msgin:
		{
			int msgin = bus_space_read_1(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_SFBR);

			if (msgin == MSG_MESSAGE_REJECT) {
				int msg, extmsg;
				if (siop_cmd->cmd_tables->msg_out[0] & 0x80) {
					/*
					 * message was part of a identify +
					 * something else. Identify shouldn't
					 * have been rejected.
					 */
					msg =
					    siop_cmd->cmd_tables->msg_out[1];
					extmsg =
					    siop_cmd->cmd_tables->msg_out[3];
				} else {
					msg = siop_cmd->cmd_tables->msg_out[0];
					extmsg =
					    siop_cmd->cmd_tables->msg_out[2];
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
					if ((siop_target->target_c.flags &
					   TARF_SYNC) == 0) {
						siop_target->target_c.status =
						    TARST_OK;
						siop_update_xfer_mode(&sc->sc_c,
						    target);
						/* no table to flush here */
						CALL_SCRIPT(Ent_msgin_ack);
						return 1;
					}
					siop_target->target_c.status =
					    TARST_SYNC_NEG;
					siop_sdtr_msg(&siop_cmd->cmd_c, 0,
					    sc->sc_c.st_minsync,
					    sc->sc_c.maxoff);
					siop_table_sync(siop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return 1;
				} else if (msg == MSG_EXTENDED &&
				    extmsg == MSG_EXT_SDTR) {
					/* sync rejected */
					siop_target->target_c.offset = 0;
					siop_target->target_c.period = 0;
					siop_target->target_c.status = TARST_OK;
					siop_update_xfer_mode(&sc->sc_c,
					    target);
					/* no table to flush here */
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				} else if (msg == MSG_SIMPLE_Q_TAG ||
				    msg == MSG_HEAD_OF_Q_TAG ||
				    msg == MSG_ORDERED_Q_TAG) {
					if (siop_handle_qtag_reject(
					    siop_cmd) == -1)
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
				siop_cmd->cmd_tables->t_extmsgdata.count =
				    siop_htoc32(&sc->sc_c, 1);
				siop_table_sync(siop_cmd,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
				CALL_SCRIPT(Ent_get_extmsgdata);
				return 1;
			}
			if (xs)
				scsipi_printaddr(xs->xs_periph);
			else
				printf("%s: ", device_xname(sc->sc_c.sc_dev));
			printf("unhandled message 0x%x\n",
			    siop_cmd->cmd_tables->msg_in[0]);
			siop_cmd->cmd_tables->msg_out[0] = MSG_MESSAGE_REJECT;
			siop_cmd->cmd_tables->t_msgout.count =
				siop_htoc32(&sc->sc_c, 1);
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_send_msgout);
			return 1;
		}
		case A_int_extmsgin:
#ifdef SIOP_DEBUG_INTR
			printf("extended message: msg 0x%x len %d\n",
			    siop_cmd->cmd_tables->msg_in[2],
			    siop_cmd->cmd_tables->msg_in[1]);
#endif
			if (siop_cmd->cmd_tables->msg_in[1] >
			    sizeof(siop_cmd->cmd_tables->msg_in) - 2)
				aprint_error_dev(sc->sc_c.sc_dev,
				    "extended message too big (%d)\n",
				    siop_cmd->cmd_tables->msg_in[1]);
			siop_cmd->cmd_tables->t_extmsgdata.count =
			    siop_htoc32(&sc->sc_c,
				siop_cmd->cmd_tables->msg_in[1] - 1);
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_get_extmsgdata);
			return 1;
		case A_int_extmsgdata:
#ifdef SIOP_DEBUG_INTR
			{
			int i;
			printf("extended message: 0x%x, data:",
			    siop_cmd->cmd_tables->msg_in[2]);
			for (i = 3; i < 2 + siop_cmd->cmd_tables->msg_in[1];
			    i++)
				printf(" 0x%x",
				    siop_cmd->cmd_tables->msg_in[i]);
			printf("\n");
			}
#endif
			if (siop_cmd->cmd_tables->msg_in[0] ==
			    MSG_IGN_WIDE_RESIDUE) {
			/* we got the second byte of MSG_IGN_WIDE_RESIDUE */
				if (siop_cmd->cmd_tables->msg_in[3] != 1)
					printf("MSG_IGN_WIDE_RESIDUE: "
					    "bad len %d\n",
					    siop_cmd->cmd_tables->msg_in[3]);
				switch (siop_iwr(&siop_cmd->cmd_c)) {
				case SIOP_NEG_MSGOUT:
					siop_table_sync(siop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return(1);
				case SIOP_NEG_ACK:
					CALL_SCRIPT(Ent_msgin_ack);
					return(1);
				default:
					panic("invalid retval from "
					    "siop_iwr()");
				}
				return(1);
			}
			if (siop_cmd->cmd_tables->msg_in[2] == MSG_EXT_WDTR) {
				switch (siop_wdtr_neg(&siop_cmd->cmd_c)) {
				case SIOP_NEG_MSGOUT:
					siop_update_scntl3(sc,
					    siop_cmd->cmd_c.siop_target);
					siop_table_sync(siop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return(1);
				case SIOP_NEG_ACK:
					siop_update_scntl3(sc,
					    siop_cmd->cmd_c.siop_target);
					CALL_SCRIPT(Ent_msgin_ack);
					return(1);
				default:
					panic("invalid retval from "
					    "siop_wdtr_neg()");
				}
				return(1);
			}
			if (siop_cmd->cmd_tables->msg_in[2] == MSG_EXT_SDTR) {
				switch (siop_sdtr_neg(&siop_cmd->cmd_c)) {
				case SIOP_NEG_MSGOUT:
					siop_update_scntl3(sc,
					    siop_cmd->cmd_c.siop_target);
					siop_table_sync(siop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return(1);
				case SIOP_NEG_ACK:
					siop_update_scntl3(sc,
					    siop_cmd->cmd_c.siop_target);
					CALL_SCRIPT(Ent_msgin_ack);
					return(1);
				default:
					panic("invalid retval from "
					    "siop_sdtr_neg()");
				}
				return(1);
			}
			/* send a message reject */
			siop_cmd->cmd_tables->msg_out[0] = MSG_MESSAGE_REJECT;
			siop_cmd->cmd_tables->t_msgout.count =
			    siop_htoc32(&sc->sc_c, 1);
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_send_msgout);
			return 1;
		case A_int_disc:
			INCSTAT(siop_stat_intr_sdp);
			offset = bus_space_read_1(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_SCRATCHA + 1);
#ifdef SIOP_DEBUG_DR
			printf("disconnect offset %d\n", offset);
#endif
			siop_sdp(&siop_cmd->cmd_c, offset);
			/* we start again with no offset */
			siop_cmd->saved_offset = SIOP_NOOFFSET;
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_script_sched);
			return 1;
		case A_int_saveoffset:
			INCSTAT(siop_stat_intr_saveoffset);
			offset = bus_space_read_1(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_SCRATCHA + 1);
#ifdef SIOP_DEBUG_DR
			printf("saveoffset offset %d\n", offset);
#endif
			siop_cmd->saved_offset = offset;
			CALL_SCRIPT(Ent_script_sched);
			return 1;
		case A_int_resfail:
			printf("reselect failed\n");
			CALL_SCRIPT(Ent_script_sched);
			return  1;
		case A_int_done:
			if (xs == NULL) {
				printf("%s: done without command, DSA=0x%lx\n",
				    device_xname(sc->sc_c.sc_dev),
				    (u_long)siop_cmd->cmd_c.dsa);
				siop_cmd->cmd_c.status = CMDST_FREE;
				CALL_SCRIPT(Ent_script_sched);
				return 1;
			}
#ifdef SIOP_DEBUG_INTR
			printf("done, DSA=0x%lx target id 0x%x last msg "
			    "in=0x%x status=0x%x\n",
			    (u_long)siop_cmd->cmd_c.dsa,
			    siop_ctoh32(&sc->sc_c, siop_cmd->cmd_tables->id),
			    siop_cmd->cmd_tables->msg_in[0],
			    siop_ctoh32(&sc->sc_c,
				siop_cmd->cmd_tables->status));
#endif
			INCSTAT(siop_stat_intr_done);
			/* update resid.  */
			offset = bus_space_read_1(sc->sc_c.sc_rt,
			    sc->sc_c.sc_rh, SIOP_SCRATCHA + 1);
			/*
			 * if we got a disconnect between the last data phase
			 * and the status phase, offset will be 0. In this
			 * case, siop_cmd->saved_offset will have the proper
			 * value if it got updated by the controller
			 */
			if (offset == 0 &&
			    siop_cmd->saved_offset != SIOP_NOOFFSET)
				offset = siop_cmd->saved_offset;
			siop_update_resid(&siop_cmd->cmd_c, offset);
			siop_cmd->cmd_c.status = CMDST_DONE;
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
	printf("siop_intr: I shouldn't be there !\n");
	return 1;

end:
	/*
	 * restart the script now if command completed properly
	 * Otherwise wait for siop_scsicmd_end(), we may need to cleanup the
	 * queue
	 */
	xs->status = siop_ctoh32(&sc->sc_c, siop_cmd->cmd_tables->status);
	if (xs->status == SCSI_OK)
		CALL_SCRIPT(Ent_script_sched);
	else
		restart = 1;
	siop_lun->siop_tag[tag].active = NULL;
	siop_scsicmd_end(siop_cmd);
	if (freetarget && siop_target->target_c.status == TARST_PROBING)
		siop_del_dev(sc, target, lun);
	if (restart)
		CALL_SCRIPT(Ent_script_sched);
	if (sc->sc_flags & SCF_CHAN_NOSLOT) {
		/* a command terminated, so we have free slots now */
		sc->sc_flags &= ~SCF_CHAN_NOSLOT;
		scsipi_channel_thaw(&sc->sc_c.sc_chan, 1);
	}

	return 1;
}

void
siop_scsicmd_end(struct siop_cmd *siop_cmd)
{
	struct scsipi_xfer *xs = siop_cmd->cmd_c.xs;
	struct siop_softc *sc = (struct siop_softc *)siop_cmd->cmd_c.siop_sc;

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
		siop_unqueue(sc, xs->xs_periph->periph_target,
		    xs->xs_periph->periph_lun);
		break;
	case SCSI_QUEUE_FULL:
		INCSTAT(siop_stat_intr_qfull);
#ifdef SIOP_DEBUG
		printf("%s:%d:%d: queue full (tag %d)\n",
		    device_xname(sc->sc_c.sc_dev),
		    xs->xs_periph->periph_target,
		    xs->xs_periph->periph_lun, siop_cmd->cmd_c.tag);
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
		bus_dmamap_sync(sc->sc_c.sc_dmat, siop_cmd->cmd_c.dmamap_data,
		    0, siop_cmd->cmd_c.dmamap_data->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_c.sc_dmat,
		    siop_cmd->cmd_c.dmamap_data);
	}
	bus_dmamap_unload(sc->sc_c.sc_dmat, siop_cmd->cmd_c.dmamap_cmd);
	if ((xs->xs_control & XS_CTL_POLL) == 0)
		callout_stop(&xs->xs_callout);
	siop_cmd->cmd_c.status = CMDST_FREE;
	TAILQ_INSERT_TAIL(&sc->free_list, siop_cmd, next);
#if 0
	if (xs->resid != 0)
		printf("resid %d datalen %d\n", xs->resid, xs->datalen);
#endif
	scsipi_done(xs);
}

void
siop_unqueue(struct siop_softc *sc, int target, int lun)
{
	int slot, tag;
	struct siop_cmd *siop_cmd;
	struct siop_lun *siop_lun =
	    ((struct siop_target *)sc->sc_c.targets[target])->siop_lun[lun];

	/* first make sure to read valid data */
	siop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (tag = 1; tag < SIOP_NTAG; tag++) {
		/* look for commands in the scheduler, not yet started */
		if (siop_lun->siop_tag[tag].active == NULL)
			continue;
		siop_cmd = siop_lun->siop_tag[tag].active;
		for (slot = 0; slot <= sc->sc_currschedslot; slot++) {
			if (siop_script_read(sc,
			    (Ent_script_sched_slot0 / 4) + slot * 2 + 1) ==
			    siop_cmd->cmd_c.dsa +
			    sizeof(struct siop_common_xfer) +
			    Ent_ldsa_select)
				break;
		}
		if (slot >  sc->sc_currschedslot)
			continue; /* didn't find it */
		if (siop_script_read(sc,
		    (Ent_script_sched_slot0 / 4) + slot * 2) == 0x80000000)
			continue; /* already started */
		/* clear the slot */
		siop_script_write(sc, (Ent_script_sched_slot0 / 4) + slot * 2,
		    0x80000000);
		/* ask to requeue */
		siop_cmd->cmd_c.xs->error = XS_REQUEUE;
		siop_cmd->cmd_c.xs->status = SCSI_SIOP_NOCHECK;
		siop_lun->siop_tag[tag].active = NULL;
		siop_scsicmd_end(siop_cmd);
	}
	/* update sc_currschedslot */
	sc->sc_currschedslot = 0;
	for (slot = SIOP_NSLOTS - 1; slot >= 0; slot--) {
		if (siop_script_read(sc,
		    (Ent_script_sched_slot0 / 4) + slot * 2) != 0x80000000)
			sc->sc_currschedslot = slot;
	}
}

/*
 * handle a rejected queue tag message: the command will run untagged,
 * has to adjust the reselect script.
 */
int
siop_handle_qtag_reject(struct siop_cmd *siop_cmd)
{
	struct siop_softc *sc = (struct siop_softc *)siop_cmd->cmd_c.siop_sc;
	int target = siop_cmd->cmd_c.xs->xs_periph->periph_target;
	int lun = siop_cmd->cmd_c.xs->xs_periph->periph_lun;
	int tag = siop_cmd->cmd_tables->msg_out[2];
	struct siop_lun *siop_lun =
	    ((struct siop_target*)sc->sc_c.targets[target])->siop_lun[lun];

#ifdef SIOP_DEBUG
	printf("%s:%d:%d: tag message %d (%d) rejected (status %d)\n",
	    device_xname(sc->sc_c.sc_dev), target, lun, tag,
	    siop_cmd->cmd_c.tag,
	    siop_cmd->cmd_c.status);
#endif

	if (siop_lun->siop_tag[0].active != NULL) {
		printf("%s: untagged command already running for target %d "
		    "lun %d (status %d)\n", device_xname(sc->sc_c.sc_dev),
		    target, lun, siop_lun->siop_tag[0].active->cmd_c.status);
		return -1;
	}
	/* clear tag slot */
	siop_lun->siop_tag[tag].active = NULL;
	/* add command to non-tagged slot */
	siop_lun->siop_tag[0].active = siop_cmd;
	siop_cmd->cmd_c.tag = 0;
	/* adjust reselect script if there is one */
	if (siop_lun->siop_tag[0].reseloff > 0) {
		siop_script_write(sc,
		    siop_lun->siop_tag[0].reseloff + 1,
		    siop_cmd->cmd_c.dsa + sizeof(struct siop_common_xfer) +
		    Ent_ldsa_reload_dsa);
		siop_table_sync(siop_cmd, BUS_DMASYNC_PREWRITE);
	}
	return 0;
}

/*
 * handle a bus reset: reset chip, unqueue all active commands, free all
 * target struct and report lossage to upper layer.
 * As the upper layer may requeue immediatly we have to first store
 * all active commands in a temporary queue.
 */
void
siop_handle_reset(struct siop_softc *sc)
{
	struct siop_cmd *siop_cmd;
	struct siop_lun *siop_lun;
	int target, lun, tag;

	/*
	 * scsi bus reset. reset the chip and restart
	 * the queue. Need to clean up all active commands
	 */
	printf("%s: scsi bus reset\n", device_xname(sc->sc_c.sc_dev));
	/* stop, reset and restart the chip */
	siop_reset(sc);
	if (sc->sc_flags & SCF_CHAN_NOSLOT) {
		/* chip has been reset, all slots are free now */
		sc->sc_flags &= ~SCF_CHAN_NOSLOT;
		scsipi_channel_thaw(&sc->sc_c.sc_chan, 1);
	}
	/*
	 * Process all commands: first commands being executed
	 */
	for (target = 0; target < sc->sc_c.sc_chan.chan_ntargets;
	    target++) {
		if (sc->sc_c.targets[target] == NULL)
			continue;
		for (lun = 0; lun < 8; lun++) {
			struct siop_target *siop_target =
			    (struct siop_target *)sc->sc_c.targets[target];
			siop_lun = siop_target->siop_lun[lun];
			if (siop_lun == NULL)
				continue;
			for (tag = 0; tag <
			    ((sc->sc_c.targets[target]->flags & TARF_TAG) ?
			    SIOP_NTAG : 1);
			    tag++) {
				siop_cmd = siop_lun->siop_tag[tag].active;
				if (siop_cmd == NULL)
					continue;
				scsipi_printaddr(siop_cmd->cmd_c.xs->xs_periph);
				printf("command with tag id %d reset\n", tag);
				siop_cmd->cmd_c.xs->error =
				    (siop_cmd->cmd_c.flags & CMDFL_TIMEOUT) ?
				    XS_TIMEOUT : XS_RESET;
				siop_cmd->cmd_c.xs->status = SCSI_SIOP_NOCHECK;
				siop_lun->siop_tag[tag].active = NULL;
				siop_cmd->cmd_c.status = CMDST_DONE;
				siop_scsicmd_end(siop_cmd);
			}
		}
		sc->sc_c.targets[target]->status = TARST_ASYNC;
		sc->sc_c.targets[target]->flags &= ~TARF_ISWIDE;
		sc->sc_c.targets[target]->period =
		    sc->sc_c.targets[target]->offset = 0;
		siop_update_xfer_mode(&sc->sc_c, target);
	}

	scsipi_async_event(&sc->sc_c.sc_chan, ASYNC_EVENT_RESET, NULL);
}

void
siop_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct siop_softc *sc = device_private(chan->chan_adapter->adapt_dev);
	struct siop_cmd *siop_cmd;
	struct siop_target *siop_target;
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
#ifdef SIOP_DEBUG_SCHED
		printf("starting cmd for %d:%d\n", target, lun);
#endif
		siop_cmd = TAILQ_FIRST(&sc->free_list);
		if (siop_cmd == NULL) {
			xs->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(xs);
			splx(s);
			return;
		}
		TAILQ_REMOVE(&sc->free_list, siop_cmd, next);
#ifdef DIAGNOSTIC
		if (siop_cmd->cmd_c.status != CMDST_FREE)
			panic("siop_scsicmd: new cmd not free");
#endif
		siop_target = (struct siop_target *)sc->sc_c.targets[target];
		if (siop_target == NULL) {
#ifdef SIOP_DEBUG
			printf("%s: alloc siop_target for target %d\n",
			    device_xname(sc->sc_c.sc_dev), target);
#endif
			sc->sc_c.targets[target] =
			    malloc(sizeof(struct siop_target),
				M_DEVBUF, M_NOWAIT|M_ZERO);
			if (sc->sc_c.targets[target] == NULL) {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "can't malloc memory for "
				    "target %d\n", target);
				xs->error = XS_RESOURCE_SHORTAGE;
				scsipi_done(xs);
				TAILQ_INSERT_TAIL(&sc->free_list,
				    siop_cmd, next);
				splx(s);
				return;
			}
			siop_target =
			    (struct siop_target *)sc->sc_c.targets[target];
			siop_target->target_c.status = TARST_PROBING;
			siop_target->target_c.flags = 0;
			siop_target->target_c.id =
			    sc->sc_c.clock_div << 24; /* scntl3 */
			siop_target->target_c.id |=  target << 16; /* id */
			/* siop_target->target_c.id |= 0x0 << 8; scxfer is 0 */

			/* get a lun switch script */
			siop_target->lunsw = siop_get_lunsw(sc);
			if (siop_target->lunsw == NULL) {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "can't alloc lunsw for target %d\n",
				    target);
				xs->error = XS_RESOURCE_SHORTAGE;
				scsipi_done(xs);
				TAILQ_INSERT_TAIL(&sc->free_list,
				    siop_cmd, next);
				splx(s);
				return;
			}
			for (i=0; i < 8; i++)
				siop_target->siop_lun[i] = NULL;
			siop_add_reselsw(sc, target);
		}
		if (siop_target->siop_lun[lun] == NULL) {
			siop_target->siop_lun[lun] =
			    malloc(sizeof(struct siop_lun), M_DEVBUF,
			    M_NOWAIT|M_ZERO);
			if (siop_target->siop_lun[lun] == NULL) {
				aprint_error_dev(sc->sc_c.sc_dev,
				    "can't alloc siop_lun for "
				    "target %d lun %d\n",
				    target, lun);
				xs->error = XS_RESOURCE_SHORTAGE;
				scsipi_done(xs);
				TAILQ_INSERT_TAIL(&sc->free_list,
				    siop_cmd, next);
				splx(s);
				return;
			}
		}
		siop_cmd->cmd_c.siop_target = sc->sc_c.targets[target];
		siop_cmd->cmd_c.xs = xs;
		siop_cmd->cmd_c.flags = 0;
		siop_cmd->cmd_c.status = CMDST_READY;

		/* load the DMA maps */
		error = bus_dmamap_load(sc->sc_c.sc_dmat,
		    siop_cmd->cmd_c.dmamap_cmd,
		    xs->cmd, xs->cmdlen, NULL, BUS_DMA_NOWAIT);
		if (error) {
			aprint_error_dev(sc->sc_c.sc_dev,
			    "unable to load cmd DMA map: %d\n",
			    error);
			xs->error = (error == EAGAIN) ?
			    XS_RESOURCE_SHORTAGE : XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			siop_cmd->cmd_c.status = CMDST_FREE;
			TAILQ_INSERT_TAIL(&sc->free_list, siop_cmd, next);
			splx(s);
			return;
		}
		if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
			error = bus_dmamap_load(sc->sc_c.sc_dmat,
			    siop_cmd->cmd_c.dmamap_data, xs->data, xs->datalen,
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
				    siop_cmd->cmd_c.dmamap_cmd);
				siop_cmd->cmd_c.status = CMDST_FREE;
				TAILQ_INSERT_TAIL(&sc->free_list,
				    siop_cmd, next);
				splx(s);
				return;
			}
			bus_dmamap_sync(sc->sc_c.sc_dmat,
			    siop_cmd->cmd_c.dmamap_data, 0,
			    siop_cmd->cmd_c.dmamap_data->dm_mapsize,
			    (xs->xs_control & XS_CTL_DATA_IN) ?
			    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
		}
		bus_dmamap_sync(sc->sc_c.sc_dmat, siop_cmd->cmd_c.dmamap_cmd, 0,
		    siop_cmd->cmd_c.dmamap_cmd->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		if (xs->xs_tag_type) {
			/* use tag_id + 1, tag 0 is reserved for untagged cmds*/
			siop_cmd->cmd_c.tag = xs->xs_tag_id + 1;
		} else {
			siop_cmd->cmd_c.tag = 0;
		}
		siop_setuptables(&siop_cmd->cmd_c);
		siop_cmd->saved_offset = SIOP_NOOFFSET;
		siop_table_sync(siop_cmd,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		siop_start(sc, siop_cmd);
		if (xs->xs_control & XS_CTL_POLL) {
			/* poll for command completion */
			while ((xs->xs_status & XS_STS_DONE) == 0) {
				delay(1000);
				siop_intr(sc);
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
		siop_morecbd(sc);
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
	    {
		struct scsipi_xfer_mode *xm = arg;
		if (sc->sc_c.targets[xm->xm_target] == NULL)
			return;
		s = splbio();
		if (xm->xm_mode & PERIPH_CAP_TQING)
			sc->sc_c.targets[xm->xm_target]->flags |= TARF_TAG;
		if ((xm->xm_mode & PERIPH_CAP_WIDE16) &&
		    (sc->sc_c.features & SF_BUS_WIDE))
			sc->sc_c.targets[xm->xm_target]->flags |= TARF_WIDE;
		if (xm->xm_mode & PERIPH_CAP_SYNC)
			sc->sc_c.targets[xm->xm_target]->flags |= TARF_SYNC;
		if ((xm->xm_mode & (PERIPH_CAP_SYNC | PERIPH_CAP_WIDE16)) ||
		    sc->sc_c.targets[xm->xm_target]->status == TARST_PROBING)
			sc->sc_c.targets[xm->xm_target]->status =
			    TARST_ASYNC;

		for (lun = 0; lun < sc->sc_c.sc_chan.chan_nluns; lun++) {
			if (scsipi_lookup_periph(chan,
			    xm->xm_target, lun) != NULL) {
				/* allocate a lun sw entry for this device */
				siop_add_dev(sc, xm->xm_target, lun);
			}
		}

		splx(s);
	    }
	}
}

static void
siop_start(struct siop_softc *sc, struct siop_cmd *siop_cmd)
{
	struct siop_lun *siop_lun;
	struct siop_xfer *siop_xfer;
	uint32_t dsa;
	int timeout;
	int target, lun, slot;

	/*
	 * first make sure to read valid data
	 */
	siop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * The queue management here is a bit tricky: the script always looks
	 * at the slot from first to last, so if we always use the first
	 * free slot commands can stay at the tail of the queue ~forever.
	 * The algorithm used here is to restart from the head when we know
	 * that the queue is empty, and only add commands after the last one.
	 * When we're at the end of the queue wait for the script to clear it.
	 * The best thing to do here would be to implement a circular queue,
	 * but using only 53c720 features this can be "interesting".
	 * A mid-way solution could be to implement 2 queues and swap orders.
	 */
	slot = sc->sc_currschedslot;
	/*
	 * If the instruction is 0x80000000 (JUMP foo, IF FALSE) the slot is
	 * free. As this is the last used slot, all previous slots are free,
	 * we can restart from 0.
	 */
	if (siop_script_read(sc, (Ent_script_sched_slot0 / 4) + slot * 2) ==
	    0x80000000) {
		slot = sc->sc_currschedslot = 0;
	} else {
		slot++;
	}
	target = siop_cmd->cmd_c.xs->xs_periph->periph_target;
	lun = siop_cmd->cmd_c.xs->xs_periph->periph_lun;
	siop_lun =
	    ((struct siop_target*)sc->sc_c.targets[target])->siop_lun[lun];
	/* if non-tagged command active, panic: this shouldn't happen */
	if (siop_lun->siop_tag[0].active != NULL) {
		panic("siop_start: tagged cmd while untagged running");
	}
#ifdef DIAGNOSTIC
	/* sanity check the tag if needed */
	if (siop_cmd->cmd_c.flags & CMDFL_TAG) {
		if (siop_lun->siop_tag[siop_cmd->cmd_c.tag].active != NULL)
			panic("siop_start: tag not free");
		if (siop_cmd->cmd_c.tag >= SIOP_NTAG) {
			scsipi_printaddr(siop_cmd->cmd_c.xs->xs_periph);
			printf(": tag id %d\n", siop_cmd->cmd_c.tag);
			panic("siop_start: invalid tag id");
		}
	}
#endif
	/*
	 * find a free scheduler slot and load it.
	 */
	for (; slot < SIOP_NSLOTS; slot++) {
		/*
		 * If cmd if 0x80000000 the slot is free
		 */
		if (siop_script_read(sc,
		    (Ent_script_sched_slot0 / 4) + slot * 2) ==
		    0x80000000)
			break;
	}
	if (slot == SIOP_NSLOTS) {
		/*
		 * no more free slot, no need to continue. freeze the queue
		 * and requeue this command.
		 */
		scsipi_channel_freeze(&sc->sc_c.sc_chan, 1);
		sc->sc_flags |= SCF_CHAN_NOSLOT;
		siop_cmd->cmd_c.xs->error = XS_REQUEUE;
		siop_cmd->cmd_c.xs->status = SCSI_SIOP_NOCHECK;
		siop_scsicmd_end(siop_cmd);
		return;
	}
#ifdef SIOP_DEBUG_SCHED
	printf("using slot %d for DSA 0x%lx\n", slot,
	    (u_long)siop_cmd->cmd_c.dsa);
#endif
	/* mark command as active */
	if (siop_cmd->cmd_c.status == CMDST_READY)
		siop_cmd->cmd_c.status = CMDST_ACTIVE;
	else
		panic("siop_start: bad status");
	siop_lun->siop_tag[siop_cmd->cmd_c.tag].active = siop_cmd;
	/* patch scripts with DSA addr */
	dsa = siop_cmd->cmd_c.dsa;
	/* first reselect switch, if we have an entry */
	if (siop_lun->siop_tag[siop_cmd->cmd_c.tag].reseloff > 0)
		siop_script_write(sc,
		    siop_lun->siop_tag[siop_cmd->cmd_c.tag].reseloff + 1,
		    dsa + sizeof(struct siop_common_xfer) +
		    Ent_ldsa_reload_dsa);
	/* CMD script: MOVE MEMORY addr */
	siop_xfer = (struct siop_xfer*)siop_cmd->cmd_tables;
	siop_xfer->resel[E_ldsa_abs_slot_Used[0]] =
	    siop_htoc32(&sc->sc_c, sc->sc_c.sc_scriptaddr +
	    Ent_script_sched_slot0 + slot * 8);
	siop_table_sync(siop_cmd, BUS_DMASYNC_PREWRITE);
	/* scheduler slot: JUMP ldsa_select */
	siop_script_write(sc,
	    (Ent_script_sched_slot0 / 4) + slot * 2 + 1,
	    dsa + sizeof(struct siop_common_xfer) + Ent_ldsa_select);
	/* handle timeout */
	if ((siop_cmd->cmd_c.xs->xs_control & XS_CTL_POLL) == 0) {
		/* start exire timer */
		timeout = mstohz(siop_cmd->cmd_c.xs->timeout);
		if (timeout == 0)
			timeout = 1;
		callout_reset( &siop_cmd->cmd_c.xs->xs_callout,
		    timeout, siop_timeout, siop_cmd);
	}
	/*
	 * Change JUMP cmd so that this slot will be handled
	 */
	siop_script_write(sc, (Ent_script_sched_slot0 / 4) + slot * 2,
	    0x80080000);
	sc->sc_currschedslot = slot;

	/* make sure SCRIPT processor will read valid data */
	siop_script_sync(sc,BUS_DMASYNC_PREREAD |  BUS_DMASYNC_PREWRITE);
	/* Signal script it has some work to do */
	bus_space_write_1(sc->sc_c.sc_rt, sc->sc_c.sc_rh,
	    SIOP_ISTAT, ISTAT_SIGP);
	/* and wait for IRQ */
}

void
siop_timeout(void *v)
{
	struct siop_cmd *siop_cmd = v;
	struct siop_softc *sc = (struct siop_softc *)siop_cmd->cmd_c.siop_sc;
	int s;

	scsipi_printaddr(siop_cmd->cmd_c.xs->xs_periph);
	printf("command timeout, CDB: ");
	scsipi_print_cdb(siop_cmd->cmd_c.xs->cmd);
	printf("\n");

	s = splbio();
	/* reset the scsi bus */
	siop_resetbus(&sc->sc_c);

	/* deactivate callout */
	callout_stop(&siop_cmd->cmd_c.xs->xs_callout);
	/* mark command as being timed out; siop_intr will handle it */
	/*
	 * mark command has being timed out and just return;
	 * the bus reset will generate an interrupt,
	 * it will be handled in siop_intr()
	 */
	siop_cmd->cmd_c.flags |= CMDFL_TIMEOUT;
	splx(s);
}

void
siop_dump_script(struct siop_softc *sc)
{
	int i;

	for (i = 0; i < PAGE_SIZE / 4; i += 2) {
		printf("0x%04x: 0x%08x 0x%08x", i * 4,
		    siop_script_read(sc, i),
		    siop_script_read(sc, i + 1));
		if ((siop_script_read(sc, i) & 0xe0000000) == 0xc0000000) {
			i++;
			printf(" 0x%08x", siop_script_read(sc, i + 1));
		}
		printf("\n");
	}
}

void
siop_morecbd(struct siop_softc *sc)
{
	int error, off, i, j, s;
	bus_dma_segment_t seg;
	int rseg;
	struct siop_cbd *newcbd;
	struct siop_xfer *xfer;
	bus_addr_t dsa;
	uint32_t *scr;

	/* allocate a new list head */
	newcbd = malloc(sizeof(struct siop_cbd), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (newcbd == NULL) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "can't allocate memory for command descriptors head\n");
		return;
	}

	/* allocate cmd list */
	newcbd->cmds = malloc(sizeof(struct siop_cmd) * SIOP_NCMDPB,
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (newcbd->cmds == NULL) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "can't allocate memory for command descriptors\n");
		goto bad3;
	}
	error = bus_dmamem_alloc(sc->sc_c.sc_dmat, PAGE_SIZE, PAGE_SIZE,
	    0, &seg, 1, &rseg, BUS_DMA_NOWAIT);
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
		    "unable to create cbd DMA map, error = %d\n",
		    error);
		goto bad1;
	}
	error = bus_dmamap_load(sc->sc_c.sc_dmat, newcbd->xferdma,
	    newcbd->xfers, PAGE_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_c.sc_dev,
		    "unable to load cbd DMA map, error = %d\n",
		    error);
		goto bad0;
	}
#ifdef SIOP_DEBUG
	printf("%s: alloc newcdb at PHY addr 0x%lx\n",
	    device_xname(sc->sc_c.sc_dev),
	    (unsigned long)newcbd->xferdma->dm_segs[0].ds_addr);
#endif
	off = (sc->sc_c.features & SF_CHIP_BE) ? 3 : 0;
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
		newcbd->cmds[i].siop_cbdp = newcbd;
		xfer = &newcbd->xfers[i];
		newcbd->cmds[i].cmd_tables = (struct siop_common_xfer *)xfer;
		memset(newcbd->cmds[i].cmd_tables, 0, sizeof(struct siop_xfer));
		dsa = newcbd->xferdma->dm_segs[0].ds_addr +
		    i * sizeof(struct siop_xfer);
		newcbd->cmds[i].cmd_c.dsa = dsa;
		newcbd->cmds[i].cmd_c.status = CMDST_FREE;
		xfer->siop_tables.t_msgout.count= siop_htoc32(&sc->sc_c, 1);
		xfer->siop_tables.t_msgout.addr = siop_htoc32(&sc->sc_c, dsa);
		xfer->siop_tables.t_msgin.count= siop_htoc32(&sc->sc_c, 1);
		xfer->siop_tables.t_msgin.addr = siop_htoc32(&sc->sc_c,
		    dsa + offsetof(struct siop_common_xfer, msg_in));
		xfer->siop_tables.t_extmsgin.count= siop_htoc32(&sc->sc_c, 2);
		xfer->siop_tables.t_extmsgin.addr = siop_htoc32(&sc->sc_c,
		    dsa + offsetof(struct siop_common_xfer, msg_in) + 1);
		xfer->siop_tables.t_extmsgdata.addr = siop_htoc32(&sc->sc_c,
		    dsa + offsetof(struct siop_common_xfer, msg_in) + 3);
		xfer->siop_tables.t_status.count= siop_htoc32(&sc->sc_c, 1);
		xfer->siop_tables.t_status.addr = siop_htoc32(&sc->sc_c,
		    dsa + offsetof(struct siop_common_xfer, status) + off);
		/* The select/reselect script */
		scr = &xfer->resel[0];
		for (j = 0; j < __arraycount(load_dsa); j++)
			scr[j] = siop_htoc32(&sc->sc_c, load_dsa[j]);
		/*
		 * 0x78000000 is a 'move data8 to reg'. data8 is the second
		 * octet, reg offset is the third.
		 */
		scr[Ent_rdsa0 / 4] = siop_htoc32(&sc->sc_c,
		    0x78100000 | ((dsa & 0x000000ff) <<  8));
		scr[Ent_rdsa1 / 4] = siop_htoc32(&sc->sc_c,
		    0x78110000 | ( dsa & 0x0000ff00       ));
		scr[Ent_rdsa2 / 4] = siop_htoc32(&sc->sc_c,
		    0x78120000 | ((dsa & 0x00ff0000) >>  8));
		scr[Ent_rdsa3 / 4] = siop_htoc32(&sc->sc_c,
		    0x78130000 | ((dsa & 0xff000000) >> 16));
		scr[E_ldsa_abs_reselected_Used[0]] = siop_htoc32(&sc->sc_c,
		    sc->sc_c.sc_scriptaddr + Ent_reselected);
		scr[E_ldsa_abs_reselect_Used[0]] = siop_htoc32(&sc->sc_c,
		    sc->sc_c.sc_scriptaddr + Ent_reselect);
		scr[E_ldsa_abs_selected_Used[0]] = siop_htoc32(&sc->sc_c,
		    sc->sc_c.sc_scriptaddr + Ent_selected);
		scr[E_ldsa_abs_data_Used[0]] = siop_htoc32(&sc->sc_c,
		    dsa + sizeof(struct siop_common_xfer) + Ent_ldsa_data);
		/* JUMP foo, IF FALSE - used by MOVE MEMORY to clear the slot */
		scr[Ent_ldsa_data / 4] = siop_htoc32(&sc->sc_c, 0x80000000);
		s = splbio();
		TAILQ_INSERT_TAIL(&sc->free_list, &newcbd->cmds[i], next);
		splx(s);
#ifdef SIOP_DEBUG
		printf("tables[%d]: in=0x%x out=0x%x status=0x%x\n", i,
		    siop_ctoh32(&sc->sc_c,
			newcbd->cmds[i].cmd_tables->t_msgin.addr),
		    siop_ctoh32(&sc->sc_c,
			newcbd->cmds[i].cmd_tables->t_msgout.addr),
		    siop_ctoh32(&sc->sc_c,
			newcbd->cmds[i].cmd_tables->t_status.addr));
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

struct siop_lunsw *
siop_get_lunsw(struct siop_softc *sc)
{
	struct siop_lunsw *lunsw;
	int i;

	if (sc->script_free_lo + __arraycount(lun_switch) >= sc->script_free_hi)
		return NULL;
	lunsw = TAILQ_FIRST(&sc->lunsw_list);
	if (lunsw != NULL) {
#ifdef SIOP_DEBUG
		printf("siop_get_lunsw got lunsw at offset %d\n",
		    lunsw->lunsw_off);
#endif
		TAILQ_REMOVE(&sc->lunsw_list, lunsw, next);
		return lunsw;
	}
	lunsw = malloc(sizeof(struct siop_lunsw), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (lunsw == NULL)
		return NULL;
#ifdef SIOP_DEBUG
	printf("allocating lunsw at offset %d\n", sc->script_free_lo);
#endif
	if (sc->sc_c.features & SF_CHIP_RAM) {
		bus_space_write_region_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
		    sc->script_free_lo * 4, lun_switch,
		    __arraycount(lun_switch));
		bus_space_write_4(sc->sc_c.sc_ramt, sc->sc_c.sc_ramh,
		    (sc->script_free_lo + E_abs_lunsw_return_Used[0]) * 4,
		    sc->sc_c.sc_scriptaddr + Ent_lunsw_return);
	} else {
		for (i = 0; i < __arraycount(lun_switch); i++)
			sc->sc_c.sc_script[sc->script_free_lo + i] =
			    siop_htoc32(&sc->sc_c, lun_switch[i]);
		sc->sc_c.sc_script[
		    sc->script_free_lo + E_abs_lunsw_return_Used[0]] =
		    siop_htoc32(&sc->sc_c,
			sc->sc_c.sc_scriptaddr + Ent_lunsw_return);
	}
	lunsw->lunsw_off = sc->script_free_lo;
	lunsw->lunsw_size = __arraycount(lun_switch);
	sc->script_free_lo += lunsw->lunsw_size;
	siop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	return lunsw;
}

void
siop_add_reselsw(struct siop_softc *sc, int target)
{
	int i, j;
	struct siop_target *siop_target;
	struct siop_lun *siop_lun;

	siop_target = (struct siop_target *)sc->sc_c.targets[target];
	/*
	 * add an entry to resel switch
	 */
	siop_script_sync(sc, BUS_DMASYNC_POSTWRITE);
	for (i = 0; i < 15; i++) {
		siop_target->reseloff = Ent_resel_targ0 / 4 + i * 2;
		if ((siop_script_read(sc, siop_target->reseloff) & 0xff)
		    == 0xff) { /* it's free */
#ifdef SIOP_DEBUG
			printf("siop: target %d slot %d offset %d\n",
			    target, i, siop_target->reseloff);
#endif
			/* JUMP abs_foo, IF target | 0x80; */
			siop_script_write(sc, siop_target->reseloff,
			    0x800c0080 | target);
			siop_script_write(sc, siop_target->reseloff + 1,
			    sc->sc_c.sc_scriptaddr +
			    siop_target->lunsw->lunsw_off * 4 +
			    Ent_lun_switch_entry);
			break;
		}
	}
	if (i == 15) /* no free slot, shouldn't happen */
		panic("siop: resel switch full");

	sc->sc_ntargets++;
	for (i = 0; i < 8; i++) {
		siop_lun = siop_target->siop_lun[i];
		if (siop_lun == NULL)
			continue;
		if (siop_lun->reseloff > 0) {
			siop_lun->reseloff = 0;
			for (j = 0; j < SIOP_NTAG; j++)
				siop_lun->siop_tag[j].reseloff = 0;
			siop_add_dev(sc, target, i);
		}
	}
	siop_update_scntl3(sc, sc->sc_c.targets[target]);
	siop_script_sync(sc, BUS_DMASYNC_PREWRITE);
}

void
siop_update_scntl3(struct siop_softc *sc,
    struct siop_common_target *_siop_target)
{
	struct siop_target *siop_target = (struct siop_target *)_siop_target;

	/* MOVE target->id >> 24 TO SCNTL3 */
	siop_script_write(sc,
	    siop_target->lunsw->lunsw_off + (Ent_restore_scntl3 / 4),
	    0x78030000 | ((siop_target->target_c.id >> 16) & 0x0000ff00));
	/* MOVE target->id >> 8 TO SXFER */
	siop_script_write(sc,
	    siop_target->lunsw->lunsw_off + (Ent_restore_scntl3 / 4) + 2,
	    0x78050000 | (siop_target->target_c.id & 0x0000ff00));
	siop_script_sync(sc, BUS_DMASYNC_PREWRITE);
}

void
siop_add_dev(struct siop_softc *sc, int target, int lun)
{
	struct siop_lunsw *lunsw;
	struct siop_target *siop_target =
	    (struct siop_target *)sc->sc_c.targets[target];
	struct siop_lun *siop_lun = siop_target->siop_lun[lun];
	int i, ntargets;

	if (siop_lun->reseloff > 0)
		return;
	lunsw = siop_target->lunsw;
	if ((lunsw->lunsw_off + lunsw->lunsw_size) < sc->script_free_lo) {
		/*
		 * can't extend this slot. Probably not worth trying to deal
		 * with this case
		 */
#ifdef SIOP_DEBUG
		aprint_error_dev(sc->sc_c.sc_dev,
		    "%d:%d: can't allocate a lun sw slot\n", target, lun);
#endif
		return;
	}
	/* count how many free targets we still have to probe */
	ntargets =  sc->sc_c.sc_chan.chan_ntargets - 1 - sc->sc_ntargets;

	/*
	 * we need 8 bytes for the lun sw additional entry, and
	 * eventually sizeof(tag_switch) for the tag switch entry.
	 * Keep enough free space for the free targets that could be
	 * probed later.
	 */
	if (sc->script_free_lo + 2 +
	    (ntargets * __arraycount(lun_switch)) >=
	    ((siop_target->target_c.flags & TARF_TAG) ?
	    sc->script_free_hi - __arraycount(tag_switch) :
	    sc->script_free_hi)) {
		/*
		 * not enough space, probably not worth dealing with it.
		 * We can hold 13 tagged-queuing capable devices in the 4k RAM.
		 */
#ifdef SIOP_DEBUG
		aprint_error_dev(sc->sc_c.sc_dev,
		    "%d:%d: not enough memory for a lun sw slot\n",
		    target, lun);
#endif
		return;
	}
#ifdef SIOP_DEBUG
	printf("%s:%d:%d: allocate lun sw entry\n",
	    device_xname(sc->sc_c.sc_dev), target, lun);
#endif
	/* INT int_resellun */
	siop_script_write(sc, sc->script_free_lo, 0x98080000);
	siop_script_write(sc, sc->script_free_lo + 1, A_int_resellun);
	/* Now the slot entry: JUMP abs_foo, IF lun */
	siop_script_write(sc, sc->script_free_lo - 2,
	    0x800c0000 | lun);
	siop_script_write(sc, sc->script_free_lo - 1, 0);
	siop_lun->reseloff = sc->script_free_lo - 2;
	lunsw->lunsw_size += 2;
	sc->script_free_lo += 2;
	if (siop_target->target_c.flags & TARF_TAG) {
		/* we need a tag switch */
		sc->script_free_hi -= __arraycount(tag_switch);
		if (sc->sc_c.features & SF_CHIP_RAM) {
			bus_space_write_region_4(sc->sc_c.sc_ramt,
			    sc->sc_c.sc_ramh,
			    sc->script_free_hi * 4, tag_switch,
			    __arraycount(tag_switch));
		} else {
			for(i = 0; i < __arraycount(tag_switch); i++) {
				sc->sc_c.sc_script[sc->script_free_hi + i] =
				    siop_htoc32(&sc->sc_c, tag_switch[i]);
			}
		}
		siop_script_write(sc,
		    siop_lun->reseloff + 1,
		    sc->sc_c.sc_scriptaddr + sc->script_free_hi * 4 +
		    Ent_tag_switch_entry);

		for (i = 0; i < SIOP_NTAG; i++) {
			siop_lun->siop_tag[i].reseloff =
			    sc->script_free_hi + (Ent_resel_tag0 / 4) + i * 2;
		}
	} else {
		/* non-tag case; just work with the lun switch */
		siop_lun->siop_tag[0].reseloff =
		    siop_target->siop_lun[lun]->reseloff;
	}
	siop_script_sync(sc, BUS_DMASYNC_PREWRITE);
}

void
siop_del_dev(struct siop_softc *sc, int target, int lun)
{
	int i;
	struct siop_target *siop_target;

#ifdef SIOP_DEBUG
	printf("%s:%d:%d: free lun sw entry\n",
	    device_xname(sc->sc_c.sc_dev), target, lun);
#endif
	if (sc->sc_c.targets[target] == NULL)
		return;
	siop_target = (struct siop_target *)sc->sc_c.targets[target];
	free(siop_target->siop_lun[lun], M_DEVBUF);
	siop_target->siop_lun[lun] = NULL;
	/* XXX compact sw entry too ? */
	/* check if we can free the whole target */
	for (i = 0; i < 8; i++) {
		if (siop_target->siop_lun[i] != NULL)
			return;
	}
#ifdef SIOP_DEBUG
	printf("%s: free siop_target for target %d lun %d lunsw offset %d\n",
	    device_xname(sc->sc_c.sc_dev), target, lun,
	    siop_target->lunsw->lunsw_off);
#endif
	/*
	 * nothing here, free the target struct and resel
	 * switch entry
	 */
	siop_script_write(sc, siop_target->reseloff, 0x800c00ff);
	siop_script_sync(sc, BUS_DMASYNC_PREWRITE);
	TAILQ_INSERT_TAIL(&sc->lunsw_list, siop_target->lunsw, next);
	free(sc->sc_c.targets[target], M_DEVBUF);
	sc->sc_c.targets[target] = NULL;
	sc->sc_ntargets--;
}

#ifdef SIOP_STATS
void
siop_printstats(void)
{

	printf("siop_stat_intr %d\n", siop_stat_intr);
	printf("siop_stat_intr_shortxfer %d\n", siop_stat_intr_shortxfer);
	printf("siop_stat_intr_xferdisc %d\n", siop_stat_intr_xferdisc);
	printf("siop_stat_intr_sdp %d\n", siop_stat_intr_sdp);
	printf("siop_stat_intr_saveoffset %d\n", siop_stat_intr_saveoffset);
	printf("siop_stat_intr_done %d\n", siop_stat_intr_done);
	printf("siop_stat_intr_lunresel %d\n", siop_stat_intr_lunresel);
	printf("siop_stat_intr_qfull %d\n", siop_stat_intr_qfull);
}
#endif
