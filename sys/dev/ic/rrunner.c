/*	$NetBSD: rrunner.c,v 1.79 2015/08/30 04:27:03 dholland Exp $	*/

/*
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code contributed to The NetBSD Foundation by Kevin M. Lahey
 * of the Numerical Aerospace Simulation Facility, NASA Ames Research
 * Center.
 *
 * Partially based on a HIPPI driver written by Essential Communications
 * Corporation.  Thanks to Jason Thorpe, Matt Jacob, and Fred Templin
 * for invaluable advice and encouragement!
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
__KERNEL_RCSID(0, "$NetBSD: rrunner.c,v 1.79 2015/08/30 04:27:03 dholland Exp $");

#include "opt_inet.h"

#include "esh.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <net/if_hippi.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif


#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/rrunnerreg.h>
#include <dev/ic/rrunnervar.h>

/*
#define ESH_PRINTF
*/

/* Autoconfig definition of driver back-end */
extern struct cfdriver esh_cd;

struct esh_softc *esh_softc_debug[22];  /* for gdb */

#ifdef DIAGNOSTIC
u_int32_t max_write_len;
#endif

/* Network device driver and initialization framework routines */

void eshinit(struct esh_softc *);
int  eshioctl(struct ifnet *, u_long, void *);
void eshreset(struct esh_softc *);
void eshstart(struct ifnet *);
static int eshstatus(struct esh_softc *);
void eshstop(struct esh_softc *);
void eshwatchdog(struct ifnet *);

/* Routines to support FP operation */

dev_type_open(esh_fpopen);
dev_type_close(esh_fpclose);
dev_type_read(esh_fpread);
dev_type_write(esh_fpwrite);
#ifdef MORE_DONE
dev_type_mmap(esh_fpmmap);
#endif
dev_type_strategy(esh_fpstrategy);

const struct cdevsw esh_cdevsw = {
	.d_open = esh_fpopen,
	.d_close = esh_fpclose,
	.d_read = esh_fpread,
	.d_write = esh_fpwrite,
	.d_ioctl = nullioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nullpoll,
#ifdef MORE_DONE
	.d_mmap = esh_fpmmap,
#else
	.d_mmap = nommap,
#endif
	.d_kqfilter = nullkqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

/* General routines, not externally visable */

static struct mbuf *esh_adjust_mbufs(struct esh_softc *, struct mbuf *m);
static void esh_dma_sync(struct esh_softc *, void *,
			      int, int, int, int, int, int);
static void esh_fill_snap_ring(struct esh_softc *);
static void esh_init_snap_ring(struct esh_softc *);
static void esh_close_snap_ring(struct esh_softc *);
static void esh_read_snap_ring(struct esh_softc *, u_int16_t, int);
static void esh_fill_fp_ring(struct esh_softc *, struct esh_fp_ring_ctl *);
static void esh_flush_fp_ring(struct esh_softc *,
				   struct esh_fp_ring_ctl *,
				   struct esh_dmainfo *);
static void esh_init_fp_rings(struct esh_softc *);
static void esh_read_fp_ring(struct esh_softc *, u_int16_t, int, int);
static void esh_reset_runcode(struct esh_softc *);
static void esh_send(struct esh_softc *);
static void esh_send_cmd(struct esh_softc *, u_int8_t, u_int8_t, u_int8_t);
static u_int32_t esh_read_eeprom(struct esh_softc *, u_int32_t);
static void esh_write_addr(bus_space_tag_t, bus_space_handle_t,
				bus_addr_t, bus_addr_t);
static int esh_write_eeprom(struct esh_softc *, u_int32_t, u_int32_t);
static void eshstart_cleanup(struct esh_softc *, u_int16_t, int);

static struct esh_dmainfo *esh_new_dmainfo(struct esh_softc *);
static void esh_free_dmainfo(struct esh_softc *, struct esh_dmainfo *);
static int esh_generic_ioctl(struct esh_softc *, u_long, void *, u_long,
				  struct lwp *);

#ifdef ESH_PRINTF
static int esh_check(struct esh_softc *);
#endif

#define ESHUNIT(x)	((minor(x) & 0xff00) >> 8)
#define ESHULP(x)	(minor(x) & 0x00ff)


/*
 * Back-end attach and configure.  Allocate DMA space and initialize
 * all structures.
 */

void
eshconfig(struct esh_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int32_t misc_host_ctl;
	u_int32_t misc_local_ctl;
	u_int32_t header_format;
	u_int32_t ula_tmp;
	bus_size_t size;
	int rseg;
	int error;
	int i;

	esh_softc_debug[device_unit(sc->sc_dev)] = sc;
	sc->sc_flags = 0;

	TAILQ_INIT(&sc->sc_dmainfo_freelist);
	sc->sc_dmainfo_freelist_count = 0;

	/*
	 * Allocate and divvy up some host side memory that can hold
	 * data structures that will be DMA'ed over to the NIC
	 */

	sc->sc_dma_size = sizeof(struct rr_gen_info) +
		sizeof(struct rr_ring_ctl) * RR_ULP_COUNT +
		sizeof(struct rr_descr) * RR_SEND_RING_SIZE +
		sizeof(struct rr_descr) * RR_SNAP_RECV_RING_SIZE +
		sizeof(struct rr_event) * RR_EVENT_RING_SIZE;

	error = bus_dmamem_alloc(sc->sc_dmat, sc->sc_dma_size,
				 0, RR_DMA_BOUNDARY, &sc->sc_dmaseg, 1,
				 &rseg, BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't allocate space for host-side"
		       "data structures\n");
		return;
	}
	if (rseg > 1) {
		aprint_error_dev(sc->sc_dev, "contiguous memory not available\n");
		goto bad_dmamem_map;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->sc_dmaseg, rseg,
			       sc->sc_dma_size, (void **)&sc->sc_dma_addr,
			       BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error) {
		aprint_error_dev(sc->sc_dev, 
		       "couldn't map memory for host-side structures\n");
		goto bad_dmamem_map;
	}

	if (bus_dmamap_create(sc->sc_dmat, sc->sc_dma_size,
			      1, sc->sc_dma_size, RR_DMA_BOUNDARY,
			      BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT,
			      &sc->sc_dma)) {
		aprint_error_dev(sc->sc_dev, "couldn't create DMA map\n");
		goto bad_dmamap_create;
	}

	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dma, sc->sc_dma_addr,
			    sc->sc_dma_size, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev, "couldn't load DMA map\n");
		goto bad_dmamap_load;
	}

	memset(sc->sc_dma_addr, 0, sc->sc_dma_size);

	sc->sc_gen_info_dma = sc->sc_dma->dm_segs->ds_addr;
	sc->sc_gen_info = (struct rr_gen_info *) sc->sc_dma_addr;
	size = sizeof(struct rr_gen_info);

	sc->sc_recv_ring_table_dma = sc->sc_dma->dm_segs->ds_addr + size;
	sc->sc_recv_ring_table =
		(struct rr_ring_ctl *) (sc->sc_dma_addr + size);
	size += sizeof(struct rr_ring_ctl) * RR_ULP_COUNT;

	sc->sc_send_ring_dma = sc->sc_dma->dm_segs->ds_addr + size;
	sc->sc_send_ring = (struct rr_descr *) (sc->sc_dma_addr + size);
	sc->sc2_send_ring = (struct rr2_descr *) (sc->sc_dma_addr + size);
	size += sizeof(struct rr_descr) * RR_SEND_RING_SIZE;

	sc->sc_snap_recv_ring_dma = sc->sc_dma->dm_segs->ds_addr + size;
	sc->sc_snap_recv_ring = (struct rr_descr *) (sc->sc_dma_addr + size);
	sc->sc2_snap_recv_ring = (struct rr2_descr *) (sc->sc_dma_addr + size);
	size += sizeof(struct rr_descr) * RR_SNAP_RECV_RING_SIZE;

	sc->sc_event_ring_dma = sc->sc_dma->dm_segs->ds_addr + size;
	sc->sc_event_ring = (struct rr_event *) (sc->sc_dma_addr + size);
	size += sizeof(struct rr_event) * RR_EVENT_RING_SIZE;

#ifdef DIAGNOSTIC
	if (size > sc->sc_dmaseg.ds_len) {
		aprint_error_dev(sc->sc_dev, "bogus size calculation\n");
		goto bad_other;
	}
#endif

	/*
	 * Allocate DMA maps for transfers.  We do this here and now
	 * so we won't have to wait for them in the middle of sending
	 * or receiving something.
	 */

	if (bus_dmamap_create(sc->sc_dmat, ESH_MAX_NSEGS * RR_DMA_MAX,
			      ESH_MAX_NSEGS, RR_DMA_MAX, RR_DMA_BOUNDARY,
			      BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT,
			      &sc->sc_send.ec_dma)) {
		aprint_error_dev(sc->sc_dev, "failed bus_dmamap_create\n");
			goto bad_other;
	}
	sc->sc_send.ec_offset = 0;
	sc->sc_send.ec_descr = sc->sc_send_ring;
    	TAILQ_INIT(&sc->sc_send.ec_di_queue);
	bufq_alloc(&sc->sc_send.ec_buf_queue, "fcfs", 0);

	for (i = 0; i < RR_MAX_SNAP_RECV_RING_SIZE; i++)
		if (bus_dmamap_create(sc->sc_dmat, RR_DMA_MAX, 1, RR_DMA_MAX,
				      RR_DMA_BOUNDARY,
				      BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT,
				      &sc->sc_snap_recv.ec_dma[i])) {
			aprint_error_dev(sc->sc_dev, "failed bus_dmamap_create\n");
			for (i--; i >= 0; i--)
				bus_dmamap_destroy(sc->sc_dmat,
						   sc->sc_snap_recv.ec_dma[i]);
			goto bad_ring_dmamap_create;
		}

	/*
	 * If this is a coldboot, the NIC RunCode should be operational.
	 * If it is a warmboot, it may or may not be operational.
	 * Just to be sure, we'll stop the RunCode and reset everything.
	 */

	/* Halt the processor (preserve NO_SWAP, if set) */

	misc_host_ctl = bus_space_read_4(iot, ioh, RR_MISC_HOST_CTL);
	bus_space_write_4(iot, ioh, RR_MISC_HOST_CTL,
			  (misc_host_ctl & RR_MH_NO_SWAP) | RR_MH_HALT_PROC);

	/* Make the EEPROM readable */

	misc_local_ctl = bus_space_read_4(iot, ioh, RR_MISC_LOCAL_CTL);
	bus_space_write_4(iot, ioh, RR_MISC_LOCAL_CTL,
	    misc_local_ctl & ~(RR_LC_FAST_PROM | RR_LC_ADD_SRAM |
			       RR_LC_PARITY_ON));

	/* Extract interesting information from the EEPROM: */

	header_format = esh_read_eeprom(sc, RR_EE_HEADER_FORMAT);
	if (header_format != RR_EE_HEADER_FORMAT_MAGIC) {
		aprint_error_dev(sc->sc_dev, "bogus EEPROM header format value %x\n",
		       header_format);
		goto bad_other;
	}

	/*
	 * As it is now, the runcode version in the EEPROM doesn't
	 * reflect the actual runcode version number.  That is only
	 * available once the runcode starts up.  We should probably
	 * change the firmware update code to modify this value,
	 * but Essential itself doesn't do it right now.
	 */

	sc->sc_sram_size = 4 * esh_read_eeprom(sc, RR_EE_SRAM_SIZE);
	sc->sc_runcode_start = esh_read_eeprom(sc, RR_EE_RUNCODE_START);
	sc->sc_runcode_version = esh_read_eeprom(sc, RR_EE_RUNCODE_VERSION);

	sc->sc_pci_latency = esh_read_eeprom(sc, RR_EE_PCI_LATENCY);
	sc->sc_pci_lat_gnt = esh_read_eeprom(sc, RR_EE_PCI_LAT_GNT);

	/* General tuning values */

	sc->sc_tune.rt_mode_and_status =
		esh_read_eeprom(sc, RR_EE_MODE_AND_STATUS);
	sc->sc_tune.rt_conn_retry_count =
		esh_read_eeprom(sc, RR_EE_CONN_RETRY_COUNT);
	sc->sc_tune.rt_conn_retry_timer =
		esh_read_eeprom(sc, RR_EE_CONN_RETRY_TIMER);
	sc->sc_tune.rt_conn_timeout =
		esh_read_eeprom(sc, RR_EE_CONN_TIMEOUT);
	sc->sc_tune.rt_interrupt_timer =
		esh_read_eeprom(sc, RR_EE_INTERRUPT_TIMER);
	sc->sc_tune.rt_tx_timeout =
		esh_read_eeprom(sc, RR_EE_TX_TIMEOUT);
	sc->sc_tune.rt_rx_timeout =
		esh_read_eeprom(sc, RR_EE_RX_TIMEOUT);
	sc->sc_tune.rt_stats_timer =
		esh_read_eeprom(sc, RR_EE_STATS_TIMER);
	sc->sc_tune.rt_stats_timer = ESH_STATS_TIMER_DEFAULT;

	/* DMA tuning values */

	sc->sc_tune.rt_pci_state =
		esh_read_eeprom(sc, RR_EE_PCI_STATE);
	sc->sc_tune.rt_dma_write_state =
		esh_read_eeprom(sc, RR_EE_DMA_WRITE_STATE);
	sc->sc_tune.rt_dma_read_state =
		esh_read_eeprom(sc, RR_EE_DMA_READ_STATE);
	sc->sc_tune.rt_driver_param =
		esh_read_eeprom(sc, RR_EE_DRIVER_PARAM);

	/*
	 * Snag the ULA.  The first two bytes are reserved.
	 * We don't really use it immediately, but it would be good to
	 * have for building IPv6 addresses, etc.
	 */

	ula_tmp = esh_read_eeprom(sc, RR_EE_ULA_HI);
	sc->sc_ula[0] = (ula_tmp >> 8) & 0xff;
	sc->sc_ula[1] = ula_tmp & 0xff;

	ula_tmp = esh_read_eeprom(sc, RR_EE_ULA_LO);
	sc->sc_ula[2] = (ula_tmp >> 24) & 0xff;
	sc->sc_ula[3] = (ula_tmp >> 16) & 0xff;
	sc->sc_ula[4] = (ula_tmp >> 8) & 0xff;
	sc->sc_ula[5] = ula_tmp & 0xff;

	/* Reset EEPROM readability */

	bus_space_write_4(iot, ioh, RR_MISC_LOCAL_CTL, misc_local_ctl);

	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = eshstart;
	ifp->if_ioctl = eshioctl;
	ifp->if_watchdog = eshwatchdog;
	ifp->if_flags = IFF_SIMPLEX | IFF_NOTRAILERS | IFF_NOARP;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	hippi_ifattach(ifp, sc->sc_ula);

	sc->sc_misaligned_bufs = sc->sc_bad_lens = 0;
	sc->sc_fp_rings = 0;

	return;

bad_ring_dmamap_create:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_send.ec_dma);
bad_other:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dma);
bad_dmamap_load:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dma);
bad_dmamap_create:
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_dma_addr, sc->sc_dma_size);
bad_dmamem_map:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dmaseg, rseg);
	return;
}


/*
 * Bring device up.
 *
 * Assume that the on-board processor has already been stopped,
 * the rings have been cleared of valid buffers, and everything
 * is pretty much as it was when the system started.
 *
 * Stop the processor (just for good measure), clear the SRAM,
 * reload the boot code, and start it all up again, with the PC
 * pointing at the boot code.  Once the boot code has had a chance
 * to come up, adjust all of the appropriate parameters, and send
 * the 'start firmware' command.
 *
 * The NIC won't actually be up until it gets an interrupt with an
 * event indicating the RunCode is up.
 */

void
eshinit(struct esh_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct rr_ring_ctl *ring;
	u_int32_t misc_host_ctl;
	u_int32_t misc_local_ctl;
	u_int32_t value;
	u_int32_t mode;

	/* If we're already doing an init, don't try again simultaniously */

	if ((sc->sc_flags & ESH_FL_INITIALIZING) != 0)
		return;
	sc->sc_flags = ESH_FL_INITIALIZING;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma, 0, sc->sc_dma_size,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* Halt the processor (preserve NO_SWAP, if set) */

	misc_host_ctl = bus_space_read_4(iot, ioh, RR_MISC_HOST_CTL);
	bus_space_write_4(iot, ioh, RR_MISC_HOST_CTL,
			  (misc_host_ctl & RR_MH_NO_SWAP)
			  | RR_MH_HALT_PROC | RR_MH_CLEAR_INT);

	/* Make the EEPROM readable */

	misc_local_ctl = bus_space_read_4(iot, ioh, RR_MISC_LOCAL_CTL);
	bus_space_write_4(iot, ioh, RR_MISC_LOCAL_CTL,
			  misc_local_ctl & ~(RR_LC_FAST_PROM |
					     RR_LC_ADD_SRAM |
					     RR_LC_PARITY_ON));

	/* Reset DMA */

	bus_space_write_4(iot, ioh, RR_RX_STATE, RR_RS_RESET);
	bus_space_write_4(iot, ioh, RR_TX_STATE, 0);
	bus_space_write_4(iot, ioh, RR_DMA_READ_STATE, RR_DR_RESET);
	bus_space_write_4(iot, ioh, RR_DMA_WRITE_STATE, RR_DW_RESET);
	bus_space_write_4(iot, ioh, RR_PCI_STATE, 0);
	bus_space_write_4(iot, ioh, RR_TIMER, 0);
	bus_space_write_4(iot, ioh, RR_TIMER_REF, 0);

	/*
	 * Reset the assist register that the documentation suggests
	 * resetting.  Too bad that the docs don't mention anything
	 * else about the register!
	 */

	bus_space_write_4(iot, ioh, 0x15C, 1);

	/* Clear BIST, set the PC to the start of the code and let 'er rip */

	value = bus_space_read_4(iot, ioh, RR_PCI_BIST);
	bus_space_write_4(iot, ioh, RR_PCI_BIST, (value & ~0xff) | 8);

	sc->sc_bist_write(sc, 0);
	esh_reset_runcode(sc);

	bus_space_write_4(iot, ioh, RR_PROC_PC, sc->sc_runcode_start);
	bus_space_write_4(iot, ioh, RR_PROC_BREAKPT, 0x00000001);

	misc_host_ctl &= ~RR_MH_HALT_PROC;
	bus_space_write_4(iot, ioh, RR_MISC_HOST_CTL, misc_host_ctl);

	/* XXX: should we sleep rather than delaying for 1ms!? */

	delay(1000);  /* Need 500 us, but we'll give it more */

	value = sc->sc_bist_read(sc);
	if (value != 0) {
		aprint_error_dev(sc->sc_dev, "BIST is %d, not 0!\n",
		       value);
		goto bad_init;
	}

#ifdef ESH_PRINTF
	printf("%s:  BIST is %x\n", device_xname(sc->sc_dev), value);
	eshstatus(sc);
#endif

	/* RunCode is up.  Initialize NIC */

	esh_write_addr(iot, ioh, RR_GEN_INFO_PTR, sc->sc_gen_info_dma);
	esh_write_addr(iot, ioh, RR_RECV_RING_PTR, sc->sc_recv_ring_table_dma);

	sc->sc_event_consumer = 0;
	bus_space_write_4(iot, ioh, RR_EVENT_CONSUMER, sc->sc_event_consumer);
	sc->sc_event_producer = bus_space_read_4(iot, ioh, RR_EVENT_PRODUCER);
	sc->sc_cmd_producer = RR_INIT_CMD;
	sc->sc_cmd_consumer = 0;

	mode = bus_space_read_4(iot, ioh, RR_MODE_AND_STATUS);
	mode |= (RR_MS_WARNINGS |
		 RR_MS_ERR_TERM |
		 RR_MS_NO_RESTART |
		 RR_MS_SWAP_DATA);
	mode &= ~RR_MS_PH_MODE;
	bus_space_write_4(iot, ioh, RR_MODE_AND_STATUS, mode);

#if 0
#ifdef ESH_PRINTF
	printf("eshinit:  misc_local_ctl %x, SRAM size %d\n", misc_local_ctl,
		sc->sc_sram_size);
#endif
/*
	misc_local_ctl |= (RR_LC_FAST_PROM | RR_LC_PARITY_ON);
*/
	if (sc->sc_sram_size > 256 * 1024) {
		misc_local_ctl |= RR_LC_ADD_SRAM;
	}
#endif

#ifdef ESH_PRINTF
	printf("eshinit:  misc_local_ctl %x\n", misc_local_ctl);
#endif
	bus_space_write_4(iot, ioh, RR_MISC_LOCAL_CTL, misc_local_ctl);

	/* Set tuning parameters */

	bus_space_write_4(iot, ioh, RR_CONN_RETRY_COUNT,
			  sc->sc_tune.rt_conn_retry_count);
	bus_space_write_4(iot, ioh, RR_CONN_RETRY_TIMER,
			  sc->sc_tune.rt_conn_retry_timer);
	bus_space_write_4(iot, ioh, RR_CONN_TIMEOUT,
			  sc->sc_tune.rt_conn_timeout);
	bus_space_write_4(iot, ioh, RR_INTERRUPT_TIMER,
			  sc->sc_tune.rt_interrupt_timer);
	bus_space_write_4(iot, ioh, RR_TX_TIMEOUT,
			  sc->sc_tune.rt_tx_timeout);
	bus_space_write_4(iot, ioh, RR_RX_TIMEOUT,
			  sc->sc_tune.rt_rx_timeout);
	bus_space_write_4(iot, ioh, RR_STATS_TIMER,
			  sc->sc_tune.rt_stats_timer);
	bus_space_write_4(iot, ioh, RR_PCI_STATE,
			  sc->sc_tune.rt_pci_state);
	bus_space_write_4(iot, ioh, RR_DMA_WRITE_STATE,
			  sc->sc_tune.rt_dma_write_state);
	bus_space_write_4(iot, ioh, RR_DMA_READ_STATE,
			  sc->sc_tune.rt_dma_read_state);

	sc->sc_max_rings = bus_space_read_4(iot, ioh, RR_MAX_RECV_RINGS);

	sc->sc_runcode_version =
		bus_space_read_4(iot, ioh, RR_RUNCODE_VERSION);
	sc->sc_version = sc->sc_runcode_version >> 16;
	if (sc->sc_version != 1 && sc->sc_version != 2) {
		aprint_error_dev(sc->sc_dev, "bad version number %d in runcode\n",
		       sc->sc_version);
		goto bad_init;
	}

	if (sc->sc_version == 1) {
		sc->sc_options = 0;
	} else {
		value = bus_space_read_4(iot, ioh, RR_ULA);
		sc->sc_options = value >> 16;
	}

	if (sc->sc_options & (RR_OP_LONG_TX | RR_OP_LONG_RX)) {
		aprint_error_dev(sc->sc_dev, "unsupported firmware -- long descriptors\n");
		goto bad_init;
	}

	printf("%s: startup runcode version %d.%d.%d, options %x\n",
	       device_xname(sc->sc_dev),
	       sc->sc_version,
	       (sc->sc_runcode_version >> 8) & 0xff,
	       sc->sc_runcode_version & 0xff,
	       sc->sc_options);

	/* Initialize the general ring information */

	memset(sc->sc_recv_ring_table, 0,
	      sizeof(struct rr_ring_ctl) * RR_ULP_COUNT);

	ring = &sc->sc_gen_info->ri_event_ring_ctl;
	ring->rr_ring_addr = sc->sc_event_ring_dma;
	ring->rr_entry_size = sizeof(struct rr_event);
	ring->rr_free_bufs = RR_EVENT_RING_SIZE / 4;
	ring->rr_entries = RR_EVENT_RING_SIZE;
	ring->rr_prod_index = 0;

	ring = &sc->sc_gen_info->ri_cmd_ring_ctl;
	ring->rr_free_bufs = 8;
	ring->rr_entry_size = sizeof(union rr_cmd);
	ring->rr_prod_index = RR_INIT_CMD;

	ring = &sc->sc_gen_info->ri_send_ring_ctl;
	ring->rr_ring_addr = sc->sc_send_ring_dma;
	if (sc->sc_version == 1) {
		ring->rr_free_bufs = RR_RR_DONT_COMPLAIN;
	} else {
		ring->rr_free_bufs = 0;
	}

	ring->rr_entries = RR_SEND_RING_SIZE;
	ring->rr_entry_size = sizeof(struct rr_descr);

	ring->rr_prod_index = sc->sc_send.ec_producer =
		sc->sc_send.ec_consumer = 0;
	sc->sc_send.ec_cur_mbuf = NULL;
	sc->sc_send.ec_cur_buf = NULL;

	sc->sc_snap_recv.ec_descr = sc->sc_snap_recv_ring;
	sc->sc_snap_recv.ec_consumer = sc->sc_snap_recv.ec_producer = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma, 0, sc->sc_dma_size,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Set up the watchdog to make sure something happens! */

	sc->sc_watchdog = 0;
	ifp->if_timer = 5;

	/*
	 * Can't actually turn on interface until we see some events,
	 * so set initialized flag, but don't start sending.
	 */

	sc->sc_flags = ESH_FL_INITIALIZED;
	esh_send_cmd(sc, RR_CC_START_RUNCODE, 0, 0);
	return;

bad_init:
	sc->sc_flags = 0;
	wakeup((void *) sc);
	return;
}


/*
 * Code to handle the Framing Protocol (FP) interface to the esh.
 * This will allow us to write directly to the wire, with no
 * intervening memcpy's to slow us down.
 */

int 
esh_fpopen(dev_t dev, int oflags, int devtype,
    struct lwp *l)
{
	struct esh_softc *sc;
	struct rr_ring_ctl *ring_ctl;
	struct esh_fp_ring_ctl *recv;
	int ulp = ESHULP(dev);
	int error = 0;
	bus_size_t size;
	int rseg;
	int s;

	sc = device_lookup_private(&esh_cd, ESHUNIT(dev));
	if (sc == NULL || ulp == HIPPI_ULP_802)
		return (ENXIO);

#ifdef ESH_PRINTF
	printf("esh_fpopen:  opening board %d, ulp %d\n",
	    device_unit(sc->sc_dev), ulp);
#endif

	/* If the card is not up, initialize it. */

	s = splnet();

	if (sc->sc_fp_rings >= sc->sc_max_rings - 1) {
		splx(s);
		return (ENOSPC);
	}

	if ((sc->sc_flags & ESH_FL_INITIALIZED) == 0) {
		eshinit(sc);
		if ((sc->sc_flags & ESH_FL_INITIALIZED) == 0) {
			splx(s);
			return EIO;
		}
	}

	if ((sc->sc_flags & ESH_FL_RUNCODE_UP) == 0) {
		/*
		 * Wait for the runcode to indicate that it is up,
		 * while watching to make sure we haven't crashed.
		 */

		error = 0;
		while (error == 0 &&
		       (sc->sc_flags & ESH_FL_INITIALIZED) != 0 &&
		       (sc->sc_flags & ESH_FL_RUNCODE_UP) == 0) {
			error = tsleep((void *) sc, PCATCH | PRIBIO,
				       "eshinit", 0);
#ifdef ESH_PRINTF
			printf("esh_fpopen:  tslept\n");
#endif
		}

		if (error != 0) {
			splx(s);
			return error;
		}

		if ((sc->sc_flags & ESH_FL_RUNCODE_UP) == 0) {
			splx(s);
			return EIO;
		}
	}


#ifdef ESH_PRINTF
	printf("esh_fpopen:  card up\n");
#endif

	/* Look at the ring descriptor to see if the ULP is in use */

	ring_ctl = &sc->sc_recv_ring_table[ulp];
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma,
			(char *) ring_ctl - (char *) sc->sc_dma_addr,
			sizeof(*ring_ctl),
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	if (ring_ctl->rr_entry_size != 0) {
		splx(s);
		return (EBUSY);
	}

#ifdef ESH_PRINTF
	printf("esh_fpopen:  ring %d okay\n", ulp);
#endif

	/*
	 * Allocate the DMA space for the ring;  space for the
	 * ring control blocks has already been staticly allocated.
	 */

	recv = (struct esh_fp_ring_ctl *)
	    malloc(sizeof(*recv), M_DEVBUF, M_WAITOK|M_ZERO);
	if (recv == NULL)
		return(ENOMEM);
	TAILQ_INIT(&recv->ec_queue);

	size = RR_FP_RECV_RING_SIZE * sizeof(struct rr_descr);
	error = bus_dmamem_alloc(sc->sc_dmat, size, 0, RR_DMA_BOUNDARY,
				 &recv->ec_dmaseg, 1,
				 &rseg, BUS_DMA_WAITOK);

	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't allocate space for FP receive ring"
		       "data structures\n");
		goto bad_fp_dmamem_alloc;
	}

	if (rseg > 1) {
		aprint_error_dev(sc->sc_dev, "contiguous memory not available for "
		       "FP receive ring\n");
		goto bad_fp_dmamem_map;
	}

	error = bus_dmamem_map(sc->sc_dmat, &recv->ec_dmaseg, rseg,
			       size, (void **) &recv->ec_descr,
			       BUS_DMA_WAITOK | BUS_DMA_COHERENT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't map memory for FP receive ring\n");
		goto bad_fp_dmamem_map;
	}

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, RR_DMA_BOUNDARY,
			      BUS_DMA_ALLOCNOW | BUS_DMA_WAITOK,
			      &recv->ec_dma)) {
		aprint_error_dev(sc->sc_dev, "couldn't create DMA map for FP receive ring\n");
		goto bad_fp_dmamap_create;
	}

	if (bus_dmamap_load(sc->sc_dmat, recv->ec_dma, recv->ec_descr,
			    size, NULL, BUS_DMA_WAITOK)) {
		aprint_error_dev(sc->sc_dev, "couldn't load DMA map for FP receive ring\n");
		goto bad_fp_dmamap_load;
	}

	memset(recv->ec_descr, 0, size);

	/*
	 * Create the ring:
	 *
	 * XXX:  HTF are we gonna deal with the fact that we don't know
	 *	 if the open succeeded until we get a response from
	 *	 the event handler?  I guess we could go to sleep waiting
	 *	 for the interrupt, and get woken up by the eshintr
	 *       case handling it.
	 */

	ring_ctl->rr_ring_addr = recv->ec_dma->dm_segs->ds_addr;
	ring_ctl->rr_free_bufs = RR_FP_RECV_RING_SIZE / 4;
	ring_ctl->rr_entries = RR_FP_RECV_RING_SIZE;
	ring_ctl->rr_entry_size = sizeof(struct rr_descr);
	ring_ctl->rr_prod_index = recv->ec_producer = recv->ec_consumer = 0;
	ring_ctl->rr_mode = RR_RR_CHARACTER;
	recv->ec_ulp = ulp;
	recv->ec_index = -1;

	sc->sc_fp_recv[ulp] = recv;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma,
			(char *) ring_ctl - (char *) sc->sc_dma_addr,
			sizeof(*ring_ctl),
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->sc_dmat, recv->ec_dma, 0, size,
			BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	esh_send_cmd(sc, RR_CC_ENABLE_RING, ulp, recv->ec_producer);

#ifdef ESH_PRINTF
	printf("esh_fpopen:  sent create ring cmd\n");
#endif

	while (recv->ec_index == -1) {
		error = tsleep((void *) &recv->ec_ulp, PCATCH | PRIBIO,
			       "eshfpopen", 0);
		if (error != 0 || recv->ec_index == -1) {
			goto bad_fp_ring_create;
		}
	}
#ifdef ESH_PRINTF
	printf("esh_fpopen:  created ring\n");
#endif

	/*
	 * Ring is created.  Set up various pointers to the ring
	 * information, fill the ring, and get going...
	 */

	sc->sc_fp_rings++;
	splx(s);
	return 0;

bad_fp_ring_create:
#ifdef ESH_PRINTF
	printf("esh_fpopen:  bad ring create\n");
#endif
	sc->sc_fp_recv[ulp] = NULL;
	memset(ring_ctl, 0, sizeof(*ring_ctl));
	bus_dmamap_unload(sc->sc_dmat, recv->ec_dma);
bad_fp_dmamap_load:
	bus_dmamap_destroy(sc->sc_dmat, recv->ec_dma);
bad_fp_dmamap_create:
	bus_dmamem_unmap(sc->sc_dmat, (void *) recv->ec_descr, size);
bad_fp_dmamem_map:
	bus_dmamem_free(sc->sc_dmat, &recv->ec_dmaseg, rseg);
bad_fp_dmamem_alloc:
	free(recv, M_DEVBUF);
	if (error == 0)
		error = ENOMEM;
	splx(s);
	return (error);
}


int 
esh_fpclose(dev_t dev, int fflag, int devtype,
    struct lwp *l)
{
	struct esh_softc *sc;
	struct rr_ring_ctl *ring_ctl;
	struct esh_fp_ring_ctl *ring;
	int ulp = ESHULP(dev);
	int index;
	int error = 0;
	int s;

	sc = device_lookup_private(&esh_cd, ESHUNIT(dev));
	if (sc == NULL || ulp == HIPPI_ULP_802)
		return (ENXIO);

	s = splnet();

	ring = sc->sc_fp_recv[ulp];
	ring_ctl = &sc->sc_recv_ring_table[ulp];
	index = ring->ec_index;

#ifdef ESH_PRINTF
	printf("esh_fpclose:  closing unit %d, ulp %d\n",
	    device_unit(sc->sc_dev), ulp);
#endif
	assert(ring);
	assert(ring_ctl);

	/*
	 * Disable the ring, wait for notification, and get rid of DMA
	 * stuff and dynamically allocated memory.  Loop, waiting to
	 * learn that the ring has been disabled, or the card
	 * has been shut down.
	 */

	do {
		esh_send_cmd(sc, RR_CC_DISABLE_RING, ulp, ring->ec_producer);

		error = tsleep((void *) &ring->ec_index, PCATCH | PRIBIO,
			       "esh_fpclose", 0);
		if (error != 0 && error != EAGAIN) {
			aprint_error_dev(sc->sc_dev, "esh_fpclose:  wait on ring disable bad\n");
			ring->ec_index = -1;
			break;
		}
	} while (ring->ec_index != -1 && sc->sc_flags != 0);

	/*
	 * XXX:  Gotta unload the ring, removing old descriptors!
	 *       *Can* there be outstanding reads with a close issued!?
	 */

	bus_dmamap_unload(sc->sc_dmat, ring->ec_dma);
	bus_dmamap_destroy(sc->sc_dmat, ring->ec_dma);
	bus_dmamem_unmap(sc->sc_dmat, (void *) ring->ec_descr,
			 RR_FP_RECV_RING_SIZE * sizeof(struct rr_descr));
	bus_dmamem_free(sc->sc_dmat, &ring->ec_dmaseg, ring->ec_dma->dm_nsegs);
	free(ring, M_DEVBUF);
	memset(ring_ctl, 0, sizeof(*ring_ctl));
	sc->sc_fp_recv[ulp] = NULL;
	sc->sc_fp_recv_index[index] = NULL;

	sc->sc_fp_rings--;
	if (sc->sc_fp_rings == 0)
		sc->sc_flags &= ~ESH_FL_FP_RING_UP;

	splx(s);
	return 0;
}

int
esh_fpread(dev_t dev, struct uio *uio, int ioflag)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct iovec *iovp;
	struct esh_softc *sc;
	struct esh_fp_ring_ctl *ring;
	struct esh_dmainfo *di;
	int ulp = ESHULP(dev);
	int error;
	int i;
	int s;

#ifdef ESH_PRINTF
	printf("esh_fpread:  dev %x\n", dev);
#endif

	sc = device_lookup_private(&esh_cd, ESHUNIT(dev));
	if (sc == NULL || ulp == HIPPI_ULP_802)
		return (ENXIO);

	s = splnet();

	ring = sc->sc_fp_recv[ulp];

	if ((sc->sc_flags & ESH_FL_INITIALIZED) == 0) {
		error = ENXIO;
		goto fpread_done;
	}

	/* Check for validity */
	for (i = 0; i < uio->uio_iovcnt; i++) {
		/* Check for valid offsets and sizes */
		if (((u_long) uio->uio_iov[i].iov_base & 3) != 0 ||
		    (i < uio->uio_iovcnt - 1 &&
		     (uio->uio_iov[i].iov_len & 3) != 0)) {
			error = EFAULT;
			goto fpread_done;
		}
	}

	/* Lock down the pages */
	for (i = 0; i < uio->uio_iovcnt; i++) {
		iovp = &uio->uio_iov[i];
		error = uvm_vslock(p->p_vmspace, iovp->iov_base, iovp->iov_len,
		    VM_PROT_WRITE);
		if (error) {
			/* Unlock what we've locked so far. */
			for (--i; i >= 0; i--) {
				iovp = &uio->uio_iov[i];
				uvm_vsunlock(p->p_vmspace, iovp->iov_base,
				    iovp->iov_len);
			}
			goto fpread_done;
		}
	}

	/*
	 * Perform preliminary DMA mapping and throw the buffers
	 * onto the queue to be sent.
	 */

	di = esh_new_dmainfo(sc);
	if (di == NULL) {
		error = ENOMEM;
		goto fpread_done;
	}
	di->ed_buf = NULL;
	di->ed_error = 0;
	di->ed_read_len = 0;

#ifdef ESH_PRINTF
	printf("esh_fpread:  ulp %d, uio offset %qd, resid %d, iovcnt %d\n",
	       ulp, uio->uio_offset, uio->uio_resid, uio->uio_iovcnt);
#endif

	error = bus_dmamap_load_uio(sc->sc_dmat, di->ed_dma,
				    uio, BUS_DMA_READ|BUS_DMA_WAITOK);
	if (error) {
		aprint_error_dev(sc->sc_dev, "esh_fpread:  bus_dmamap_load_uio "
		       "failed\terror code %d\n",
		       error);
		error = ENOBUFS;
		esh_free_dmainfo(sc, di);
		goto fpread_done;
	}

	bus_dmamap_sync(sc->sc_dmat, di->ed_dma,
			0, di->ed_dma->dm_mapsize,
			BUS_DMASYNC_PREREAD);

#ifdef ESH_PRINTF
	printf("esh_fpread:  ulp %d, di %p, nsegs %d, uio len %d\n",
	       ulp, di, di->ed_dma->dm_nsegs, uio->uio_resid);
#endif

	di->ed_flags |= ESH_DI_BUSY;

	TAILQ_INSERT_TAIL(&ring->ec_queue, di, ed_list);
	esh_fill_fp_ring(sc, ring);

	while ((di->ed_flags & ESH_DI_BUSY) != 0 && error == 0) {
		error = tsleep((void *) di, PCATCH | PRIBIO, "esh_fpread", 0);
#ifdef ESH_PRINTF
		printf("esh_fpread:  ulp %d, tslept %d\n", ulp, error);
#endif
		if (error) {
			/*
			 * Remove the buffer entries from the ring;  this
			 * is gonna require a DISCARD_PKT command, and
			 * will certainly disrupt things.  This is why we
			 * can have only one outstanding read on a ring
			 * at a time.  :-(
			 */

			printf("esh_fpread:  was that a ^C!?  error %d, ulp %d\n",
			       error, ulp);
			if (error == EINTR || error == ERESTART)
				error = 0;
			if ((di->ed_flags & ESH_DI_BUSY) != 0) {
				esh_flush_fp_ring(sc, ring, di);
				error = EINTR;
				break;
			}
		}
	}

	if (error == 0 && di->ed_error != 0)
		error = EIO;

	/*
	 * How do we let the caller know how much has been read?
	 * Adjust the uio_resid stuff!?
	 */

	assert(uio->uio_resid >= di->ed_read_len);

	uio->uio_resid -= di->ed_read_len;
	for (i = 0; i < uio->uio_iovcnt; i++) {
		iovp = &uio->uio_iov[i];
		uvm_vsunlock(p->p_vmspace, iovp->iov_base, iovp->iov_len);
	}
	esh_free_dmainfo(sc, di);

fpread_done:
#ifdef ESH_PRINTF
	printf("esh_fpread:  ulp %d, error %d\n", ulp, error);
#endif
	splx(s);
	return error;
}


int
esh_fpwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct iovec *iovp;
	struct esh_softc *sc;
	struct esh_send_ring_ctl *ring;
	struct esh_dmainfo *di;
	int ulp = ESHULP(dev);
	int error;
	int len;
	int i;
	int s;

#ifdef ESH_PRINTF
	printf("esh_fpwrite:  dev %x\n", dev);
#endif

	sc = device_lookup_private(&esh_cd, ESHUNIT(dev));
	if (sc == NULL || ulp == HIPPI_ULP_802)
		return (ENXIO);

	s = splnet();

	ring = &sc->sc_send;

	if ((sc->sc_flags & ESH_FL_INITIALIZED) == 0) {
		error = ENXIO;
		goto fpwrite_done;
	}

	/* Check for validity */
	for (i = 0; i < uio->uio_iovcnt; i++) {
		if (((u_long) uio->uio_iov[i].iov_base & 3) != 0 ||
		    (i < uio->uio_iovcnt - 1 &&
		     (uio->uio_iov[i].iov_len & 3) != 0)) {
			error = EFAULT;
			goto fpwrite_done;
		}
	}

	/* Lock down the pages */
	for (i = 0; i < uio->uio_iovcnt; i++) {
		iovp = &uio->uio_iov[i];
		error = uvm_vslock(p->p_vmspace, iovp->iov_base, iovp->iov_len,
		    VM_PROT_READ);
		if (error) {
			/* Unlock what we've locked so far. */
			for (--i; i >= 0; i--) {
				iovp = &uio->uio_iov[i];
				uvm_vsunlock(p->p_vmspace, iovp->iov_base,
				    iovp->iov_len);
			}
			goto fpwrite_done;
		}
	}

	/*
	 * Perform preliminary DMA mapping and throw the buffers
	 * onto the queue to be sent.
	 */

	di = esh_new_dmainfo(sc);
	if (di == NULL) {
		error = ENOMEM;
		goto fpwrite_done;
	}
	di->ed_buf = NULL;
	di->ed_error = 0;

#ifdef ESH_PRINTF
	printf("esh_fpwrite:  uio offset %qd, resid %d, iovcnt %d\n",
	       uio->uio_offset, uio->uio_resid, uio->uio_iovcnt);
#endif

	error = bus_dmamap_load_uio(sc->sc_dmat, di->ed_dma,
				    uio, BUS_DMA_WRITE|BUS_DMA_WAITOK);
	if (error) {
		aprint_error_dev(sc->sc_dev, "esh_fpwrite:  bus_dmamap_load_uio "
		       "failed\terror code %d\n",
		       error);
		error = ENOBUFS;
		esh_free_dmainfo(sc, di);
		goto fpwrite_done;
	}

	bus_dmamap_sync(sc->sc_dmat, di->ed_dma,
			0, di->ed_dma->dm_mapsize,
			BUS_DMASYNC_PREWRITE);

#ifdef ESH_PRINTF
	printf("esh_fpwrite:  di %p, nsegs %d, uio len %d\n",
	       di, di->ed_dma->dm_nsegs, uio->uio_resid);
#endif

	len = di->ed_dma->dm_mapsize;
	di->ed_flags |= ESH_DI_BUSY;

	TAILQ_INSERT_TAIL(&ring->ec_di_queue, di, ed_list);
	eshstart(&sc->sc_if);

	while ((di->ed_flags & ESH_DI_BUSY) != 0 && error == 0) {
		error = tsleep((void *) di, PRIBIO, "esh_fpwrite", 0);
#ifdef ESH_PRINTF
		printf("esh_fpwrite:  tslept %d\n", error);
#endif
		if (error) {
			printf("esh_fpwrite:  was that a ^C!?  Shouldn't be!  Error %d\n",
			       error);
			if (error == EINTR || error == ERESTART)
				error = 0;
			if ((di->ed_flags & ESH_DI_BUSY) != 0) {
				panic("interrupted eshwrite!");
#if 0
				/* Better do *something* here! */
				esh_flush_send_ring(sc, di);
#endif
				error = EINTR;
				break;
			}
		}
	}

	if (error == 0 && di->ed_error != 0)
		error = EIO;

	/*
	 * How do we let the caller know how much has been written?
	 * Adjust the uio_resid stuff!?
	 */

	uio->uio_resid -= len;
	uio->uio_offset += len;

	for (i = 0; i < uio->uio_iovcnt; i++) {
		iovp = &uio->uio_iov[i];
		uvm_vsunlock(p->p_vmspace, iovp->iov_base, iovp->iov_len);
	}

	esh_free_dmainfo(sc, di);

fpwrite_done:
#ifdef ESH_PRINTF
	printf("esh_fpwrite:  error %d\n", error);
#endif
	splx(s);
	return error;
}

void
esh_fpstrategy(struct buf *bp)
{
	struct esh_softc *sc;
	int ulp = ESHULP(bp->b_dev);
	int error = 0;
	int s;

#ifdef ESH_PRINTF
        printf("esh_fpstrategy:  starting, bcount %ld, flags %lx, dev %x\n"
	       "\tunit %x, ulp %d\n",
		bp->b_bcount, bp->b_flags, bp->b_dev, unit, ulp);
#endif

	sc = device_lookup_private(&esh_cd, ESHUNIT(bp->b_dev));

	s = splnet();
	if (sc == NULL || ulp == HIPPI_ULP_802) {
		bp->b_error = ENXIO;
		goto done;
	}

	if (bp->b_bcount == 0)
		goto done;

#define UP_FLAGS (ESH_FL_INITIALIZED | ESH_FL_RUNCODE_UP)

	if ((sc->sc_flags & UP_FLAGS) != UP_FLAGS) {
		bp->b_error = EBUSY;
		goto done;
	}
#undef UP_FLAGS

	if (bp->b_flags & B_READ) {
		/*
		 * Perform preliminary DMA mapping and throw the buffers
		 * onto the queue to be sent.
		 */

		struct esh_fp_ring_ctl *ring = sc->sc_fp_recv[ulp];
		struct esh_dmainfo *di = esh_new_dmainfo(sc);

		if (di == NULL) {
			bp->b_error = ENOMEM;
			goto done;
		}
		di->ed_buf = bp;
		error = bus_dmamap_load(sc->sc_dmat, di->ed_dma,
					bp->b_data, bp->b_bcount,
					bp->b_proc,
					BUS_DMA_READ|BUS_DMA_WAITOK);
		if (error) {
			aprint_error_dev(sc->sc_dev, "esh_fpstrategy:  "
			       "bus_dmamap_load "
			       "failed\terror code %d\n",
			       error);
			bp->b_error = ENOBUFS;
			esh_free_dmainfo(sc, di);
			goto done;
		}

		bus_dmamap_sync(sc->sc_dmat, di->ed_dma,
				0, di->ed_dma->dm_mapsize,
				BUS_DMASYNC_PREREAD);

#ifdef ESH_PRINTF
		printf("fpstrategy:  di %p\n", di);
#endif

		TAILQ_INSERT_TAIL(&ring->ec_queue, di, ed_list);
		esh_fill_fp_ring(sc, ring);
	} else {
		/*
		 * Queue up the buffer for future sending.  If the card
		 * isn't already transmitting, give it a kick.
		 */

		struct esh_send_ring_ctl *ring = &sc->sc_send;
		bufq_put(ring->ec_buf_queue, bp);
#ifdef ESH_PRINTF
		printf("esh_fpstrategy:  ready to call eshstart to write!\n");
#endif
		eshstart(&sc->sc_if);
	}
	splx(s);
	return;

done:
	splx(s);
#ifdef ESH_PRINTF
	printf("esh_fpstrategy:  failing, bp->b_error %d!\n",
	       bp->b_error);
#endif
	biodone(bp);
}

/*
 * Handle interrupts.  This is basicly event handling code;  version two
 * firmware tries to speed things up by just telling us the location
 * of the producer and consumer indices, rather than sending us an event.
 */

int
eshintr(void *arg)
{
	struct esh_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_if;
	u_int32_t rc_offsets;
	u_int32_t misc_host_ctl;
	int rc_send_consumer = 0;	/* shut up compiler */
	int rc_snap_ring_consumer = 0;	/* ditto */
	u_int8_t fp_ring_consumer[RR_MAX_RECV_RING];
	int start_consumer;
	int ret = 0;

	int okay = 0;
	int blah = 0;
	char sbuf[100];
	char t[100];


	/* Check to see if this is our interrupt. */

	misc_host_ctl = bus_space_read_4(iot, ioh, RR_MISC_HOST_CTL);
	if ((misc_host_ctl & RR_MH_INTERRUPT) == 0)
		return 0;

	/* If we can't do anything with the interrupt, just drop it */

	if (sc->sc_flags == 0)
		return 1;

	rc_offsets = bus_space_read_4(iot, ioh, RR_EVENT_PRODUCER);
	sc->sc_event_producer = rc_offsets & 0xff;
	if (sc->sc_version == 2) {
		int i;

		sbuf[0] = '\0';
		strlcat(sbuf, "rc:  ", sizeof(sbuf));
		rc_send_consumer = (rc_offsets >> 8) & 0xff;
		rc_snap_ring_consumer = (rc_offsets >> 16) & 0xff;
		for (i = 0; i < RR_MAX_RECV_RING; i += 4) {
			rc_offsets =
				bus_space_read_4(iot, ioh,
						 RR_RUNCODE_RECV_CONS + i);
			/* XXX:  should do this right! */
			NTOHL(rc_offsets);
			*((u_int32_t *) &fp_ring_consumer[i]) = rc_offsets;
			snprintf(t, sizeof(t), "%.8x|", rc_offsets);
			strlcat(sbuf, t, sizeof(sbuf));
		}
	}
	start_consumer = sc->sc_event_consumer;

	/* Take care of synchronizing DMA with entries we read... */

	esh_dma_sync(sc, sc->sc_event_ring,
		     start_consumer, sc->sc_event_producer,
		     RR_EVENT_RING_SIZE, sizeof(struct rr_event), 0,
		     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (sc->sc_event_consumer != sc->sc_event_producer) {
		struct rr_event *event =
			&sc->sc_event_ring[sc->sc_event_consumer];

#ifdef ESH_PRINTF
		if (event->re_code != RR_EC_WATCHDOG &&
		    event->re_code != RR_EC_STATS_UPDATE &&
		    event->re_code != RR_EC_SET_CMD_CONSUMER) {
			printf("%s:  event code %x, ring %d, index %d\n",
			       device_xname(sc->sc_dev), event->re_code,
			       event->re_ring, event->re_index);
			if (okay == 0)
				printf("%s\n", sbuf);
			okay = 1;
		}
#endif
		ret = 1;   /* some action was taken by card */

		switch(event->re_code) {
		case RR_EC_RUNCODE_UP:
			printf("%s:  firmware up\n", device_xname(sc->sc_dev));
			sc->sc_flags |= ESH_FL_RUNCODE_UP;
			esh_send_cmd(sc, RR_CC_WATCHDOG, 0, 0);
			esh_send_cmd(sc, RR_CC_UPDATE_STATS, 0, 0);
#ifdef ESH_PRINTF
			eshstatus(sc);
#endif
			if ((ifp->if_flags & IFF_UP) != 0)
				esh_init_snap_ring(sc);
			if (sc->sc_fp_rings > 0)
				esh_init_fp_rings(sc);

			/*
			 * XXX:   crank up FP rings that might be
			 *        in use after a reset!
			 */
			wakeup((void *) sc);
			break;

		case RR_EC_WATCHDOG:
			/*
			 * Record the watchdog event.
			 * This is checked by eshwatchdog
			 */

			sc->sc_watchdog = 1;
			break;

		case RR_EC_SET_CMD_CONSUMER:
			sc->sc_cmd_consumer = event->re_index;
			break;

		case RR_EC_LINK_ON:
			printf("%s:  link up\n", device_xname(sc->sc_dev));
			sc->sc_flags |= ESH_FL_LINK_UP;

			esh_send_cmd(sc, RR_CC_WATCHDOG, 0, 0);
			esh_send_cmd(sc, RR_CC_UPDATE_STATS, 0, 0);
			if ((sc->sc_flags & ESH_FL_SNAP_RING_UP) != 0) {
				/*
				 * Interface is now `running', with no
				 * output active.
				 */
				ifp->if_flags |= IFF_RUNNING;
				ifp->if_flags &= ~IFF_OACTIVE;

				/* Attempt to start output, if any. */
			}
			eshstart(ifp);
			break;

		case RR_EC_LINK_OFF:
			sc->sc_flags &= ~ESH_FL_LINK_UP;
			printf("%s:  link down\n", device_xname(sc->sc_dev));
			break;

		/*
		 * These are all unexpected.  We need to handle all
		 * of them, though.
		 */

		case RR_EC_INVALID_CMD:
		case RR_EC_INTERNAL_ERROR:
		case RR2_EC_INTERNAL_ERROR:
		case RR_EC_BAD_SEND_RING:
		case RR_EC_BAD_SEND_BUF:
		case RR_EC_BAD_SEND_DESC:
		case RR_EC_RECV_RING_FLUSH:
		case RR_EC_RECV_ERROR_INFO:
		case RR_EC_BAD_RECV_BUF:
		case RR_EC_BAD_RECV_DESC:
		case RR_EC_BAD_RECV_RING:
		case RR_EC_UNIMPLEMENTED:
			aprint_error_dev(sc->sc_dev, "unexpected event %x;"
			       "shutting down interface\n",
			       event->re_code);
			ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
			sc->sc_flags = ESH_FL_CRASHED;
#ifdef ESH_PRINTF
			eshstatus(sc);
#endif
			break;

#define CALLOUT(a) case a:						\
	printf("%s:  Event " #a " received -- "				\
	       "ring %d index %d timestamp %x\n",			\
	       device_xname(sc->sc_dev), event->re_ring, event->re_index,	\
	       event->re_timestamp);					\
	break;

		CALLOUT(RR_EC_NO_RING_FOR_ULP);
		CALLOUT(RR_EC_REJECTING);  /* dropping packets */
#undef CALLOUT

			/* Send events */

		case RR_EC_PACKET_SENT:   	/* not used in firmware 2.x */
			ifp->if_opackets++;
			/* FALLTHROUGH */

		case RR_EC_SET_SND_CONSUMER:
			assert(sc->sc_version == 1);
			/* FALLTHROUGH */

		case RR_EC_SEND_RING_LOW:
			eshstart_cleanup(sc, event->re_index, 0);
			break;


		case RR_EC_CONN_REJECT:
		case RR_EC_CAMPON_TIMEOUT:
		case RR_EC_CONN_TIMEOUT:
		case RR_EC_DISCONN_ERR:
		case RR_EC_INTERNAL_PARITY:
		case RR_EC_TX_IDLE:
		case RR_EC_SEND_LINK_OFF:
			eshstart_cleanup(sc, event->re_index, event->re_code);
			break;

			/* Receive events */

		case RR_EC_RING_ENABLED:
			if (event->re_ring == HIPPI_ULP_802) {
				rc_snap_ring_consumer = 0; /* prevent read */
				sc->sc_flags |= ESH_FL_SNAP_RING_UP;
				esh_fill_snap_ring(sc);

				if (sc->sc_flags & ESH_FL_LINK_UP) {
					/*
					 * Interface is now `running', with no
					 * output active.
					 */
					ifp->if_flags |= IFF_RUNNING;
					ifp->if_flags &= ~IFF_OACTIVE;

					/* Attempt to start output, if any. */

					eshstart(ifp);
				}
#ifdef ESH_PRINTF
				if (event->re_index != 0)
					printf("ENABLE snap ring -- index %d instead of 0!\n",
					       event->re_index);
#endif
			} else {
				struct esh_fp_ring_ctl *ring =
					sc->sc_fp_recv[event->re_ring];

				sc->sc_flags |= ESH_FL_FP_RING_UP;
#ifdef ESH_PRINTF
				printf("eshintr:  FP ring %d up\n",
				       event->re_ring);
#endif

				sc->sc_fp_recv_index[event->re_index] = ring;
				ring->ec_index = event->re_index;
				wakeup((void *) &ring->ec_ulp);
			}
			break;

		case RR_EC_RING_DISABLED:
#ifdef ESH_PRINTF
			printf("eshintr:  disabling ring %d\n",
			       event->re_ring);
#endif
			if (event->re_ring == HIPPI_ULP_802) {
				struct rr_ring_ctl *ring =
					sc->sc_recv_ring_table + HIPPI_ULP_802;
				memset(ring, 0, sizeof(*ring));
				sc->sc_flags &= ~ESH_FL_CLOSING_SNAP;
				sc->sc_flags &= ~ESH_FL_SNAP_RING_UP;
				while (sc->sc_snap_recv.ec_consumer
				       != sc->sc_snap_recv.ec_producer) {
					u_int16_t offset = sc->sc_snap_recv.ec_consumer;

					bus_dmamap_unload(sc->sc_dmat,
							  sc->sc_snap_recv.ec_dma[offset]);
					m_free(sc->sc_snap_recv.ec_m[offset]);
					sc->sc_snap_recv.ec_m[offset] = NULL;
					sc->sc_snap_recv.ec_consumer =
						NEXT_RECV(sc->sc_snap_recv.ec_consumer);
				}
				sc->sc_snap_recv.ec_consumer =
					rc_snap_ring_consumer;
				sc->sc_snap_recv.ec_producer =
					rc_snap_ring_consumer;
				wakeup((void *) &sc->sc_snap_recv);
			} else {
				struct esh_fp_ring_ctl *recv =
					sc->sc_fp_recv[event->re_ring];
				assert(recv != NULL);
				recv->ec_consumer = recv->ec_producer =
					fp_ring_consumer[recv->ec_index];
				recv->ec_index = -1;
				wakeup((void *) &recv->ec_index);
			}
			break;

		case RR_EC_RING_ENABLE_ERR:
			if (event->re_ring == HIPPI_ULP_802) {
				aprint_error_dev(sc->sc_dev, "unable to enable SNAP ring!?\n\t"
				       "shutting down interface\n");
				ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
#ifdef ESH_PRINTF
				eshstatus(sc);
#endif
			} else {
				/*
				 * If we just leave the ring index as-is,
				 * the driver will figure out that
				 * we failed to open the ring.
				 */
				wakeup((void *) &(sc->sc_fp_recv[event->re_ring]->ec_ulp));
			}
			break;

		case RR_EC_PACKET_DISCARDED:
		        /*
			 * Determine the dmainfo for the current packet
			 * we just discarded and wake up the waiting
			 * process.
			 *
			 * This should never happen on the network ring!
			 */

			if (event->re_ring == HIPPI_ULP_802) {
				aprint_error_dev(sc->sc_dev, "discard on SNAP ring!?\n\t"
				       "shutting down interface\n");
				ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
				sc->sc_flags = ESH_FL_CRASHED;
			} else {
				struct esh_fp_ring_ctl *ring =
					sc->sc_fp_recv[event->re_ring];
				struct esh_dmainfo *di =
					ring->ec_cur_dmainfo;

				if (di == NULL)
					di = ring->ec_dmainfo[ring->ec_producer];
				printf("eshintr:  DISCARD:  index %d,"
				       "ring prod %d, di %p, ring[index] %p\n",
				       event->re_index, ring->ec_producer, di,
				       ring->ec_dmainfo[event->re_index]);

				if (di == NULL)
					di = ring->ec_dmainfo[event->re_index];

				if (di == NULL) {
					printf("eshintr:  DISCARD:  NULL di, skipping...\n");
					break;
				}

				di->ed_flags &=
					~(ESH_DI_READING | ESH_DI_BUSY);
				wakeup((void *) &di->ed_flags);
			}
			break;

		case RR_EC_OUT_OF_BUF:
		case RR_EC_RECV_RING_OUT:
		case RR_EC_RECV_RING_LOW:
			break;

		case RR_EC_SET_RECV_CONSUMER:
		case RR_EC_PACKET_RECVED:
			if (event->re_ring == HIPPI_ULP_802)
				esh_read_snap_ring(sc, event->re_index, 0);
			else if (sc->sc_fp_recv[event->re_ring] != NULL)
				esh_read_fp_ring(sc, event->re_index, 0,
						 event->re_ring);
			break;

		case RR_EC_RECV_IDLE:
		case RR_EC_PARITY_ERR:
		case RR_EC_LLRC_ERR:
		case RR_EC_PKT_LENGTH_ERR:
		case RR_EC_IP_HDR_CKSUM_ERR:
		case RR_EC_DATA_CKSUM_ERR:
		case RR_EC_SHORT_BURST_ERR:
		case RR_EC_RECV_LINK_OFF:
		case RR_EC_FLAG_SYNC_ERR:
		case RR_EC_FRAME_ERR:
		case RR_EC_STATE_TRANS_ERR:
		case RR_EC_NO_READY_PULSE:
			if (event->re_ring == HIPPI_ULP_802) {
				esh_read_snap_ring(sc, event->re_index,
						   event->re_code);
			} else {
				struct esh_fp_ring_ctl *r;

				r = sc->sc_fp_recv[event->re_ring];
				if (r)
					r->ec_error = event->re_code;
			}
			break;

		/*
		 * Statistics events can be ignored for now.  They might become
		 * necessary if we have to deliver stats on demand, rather than
		 * just returning the statistics block of memory.
		 */

		case RR_EC_STATS_UPDATE:
		case RR_EC_STATS_RETRIEVED:
		case RR_EC_TRACE:
			break;

		default:
			aprint_error_dev(sc->sc_dev, "Bogus event code %x, "
			       "ring %d, index %d, timestamp %x\n",
			       event->re_code,
			       event->re_ring, event->re_index,
			       event->re_timestamp);
			break;
		}

		sc->sc_event_consumer = NEXT_EVENT(sc->sc_event_consumer);
	}

	/* Do the receive and send ring processing for version 2 RunCode */

	if (sc->sc_version == 2) {
		int i;
		if (sc->sc_send.ec_consumer != rc_send_consumer) {
			eshstart_cleanup(sc, rc_send_consumer, 0);
			ret = 1;
			blah++;
		}
		if (sc->sc_snap_recv.ec_consumer != rc_snap_ring_consumer &&
		    (sc->sc_flags & ESH_FL_SNAP_RING_UP) != 0) {
			esh_read_snap_ring(sc, rc_snap_ring_consumer, 0);
			ret = 1;
			blah++;
		}
		for (i = 0; i < RR_MAX_RECV_RING; i++) {
			struct esh_fp_ring_ctl *r = sc->sc_fp_recv_index[i];

			if (r != NULL &&
			    r->ec_consumer != fp_ring_consumer[i]) {
#ifdef ESH_PRINTF
				printf("eshintr:  performed read on ring %d, index %d\n",
				       r->ec_ulp, i);
#endif
				blah++;
				esh_read_fp_ring(sc, fp_ring_consumer[i],
						 0, r->ec_ulp);
				fp_ring_consumer[i] = r->ec_consumer;
			}
		}
		if (blah != 0 && okay == 0) {
			okay = 1;
#ifdef ESH_PRINTF
			printf("%s\n", sbuf);
#endif
		}
		rc_offsets = (sc->sc_snap_recv.ec_consumer << 16) |
			(sc->sc_send.ec_consumer << 8) | sc->sc_event_consumer;
	} else {
		rc_offsets = sc->sc_event_consumer;
	}

	esh_dma_sync(sc, sc->sc_event_ring,
		     start_consumer, sc->sc_event_producer,
		     RR_EVENT_RING_SIZE, sizeof(struct rr_event), 0,
		     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* Write out new values for the FP segments... */

	if (sc->sc_version == 2) {
		int i;
		u_int32_t u;

		sbuf[0] = '\0';
		strlcat(sbuf, "drv: ", sizeof(sbuf));
		for (i = 0; i < RR_MAX_RECV_RING; i += 4) {
			/* XXX:  should do this right! */
			u = *((u_int32_t *) &fp_ring_consumer[i]);
			snprintf(t, sizeof(t), "%.8x|", u);
			strlcat(sbuf, t, sizeof(sbuf));
			NTOHL(u);
			bus_space_write_4(iot, ioh,
					  RR_DRIVER_RECV_CONS + i, u);
		}
#ifdef ESH_PRINTF
		if (okay == 1)
			printf("%s\n", sbuf);
#endif

		sbuf[0] = '\0';
		strlcat(sbuf, "rcn: ", sizeof(sbuf));
		for (i = 0; i < RR_MAX_RECV_RING; i += 4) {
			u = bus_space_read_4(iot, ioh,
					     RR_RUNCODE_RECV_CONS + i);
			/* XXX:  should do this right! */
			NTOHL(u);
			snprintf(t, sizeof(t), "%.8x|", u);
			strlcat(sbuf, t, sizeof(sbuf));
		}
#ifdef ESH_PRINTF
		if (okay == 1)
			printf("%s\n", sbuf);
#endif
	}

	/* Clear interrupt */
	bus_space_write_4(iot, ioh, RR_EVENT_CONSUMER, rc_offsets);

	return (ret);
}


/*
 * Start output on the interface.  Always called at splnet().
 * Check to see if there are any mbufs that didn't get sent the
 * last time this was called.  If there are none, get more mbufs
 * and send 'em.
 *
 * For now, we only send one packet at a time.
 */

void
eshstart(struct ifnet *ifp)
{
	struct esh_softc *sc = ifp->if_softc;
	struct esh_send_ring_ctl *send = &sc->sc_send;
	struct mbuf *m = NULL;
	int error;

	/* Don't transmit if interface is busy or not running */

#ifdef ESH_PRINTF
	printf("eshstart:  ready to look;  flags %x\n", sc->sc_flags);
#endif

#define LINK_UP_FLAGS (ESH_FL_LINK_UP | ESH_FL_INITIALIZED | ESH_FL_RUNCODE_UP)
	if ((sc->sc_flags & LINK_UP_FLAGS) != LINK_UP_FLAGS)
		return;
#undef LINK_UP_FLAGS

#ifdef ESH_PRINTF
	if (esh_check(sc))
		return;
#endif

	/* If we have sent the current packet, get another */

	while ((sc->sc_flags & ESH_FL_SNAP_RING_UP) != 0 &&
	       (m = send->ec_cur_mbuf) == NULL && send->ec_cur_buf == NULL &&
		send->ec_cur_dmainfo == NULL) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)		/* not really needed */
			break;

		if (ifp->if_bpf) {
			/*
			 * On output, the raw packet has a eight-byte CCI
			 * field prepended.  On input, there is no such field.
			 * The bpf expects the packet to look the same in both
			 * places, so we temporarily lop off the prepended CCI
			 * field here, then replace it.  Ugh.
			 *
			 * XXX:  Need to use standard mbuf manipulation
			 *       functions, first mbuf may be less than
			 *       8 bytes long.
			 */

			m->m_len -= 8;
			m->m_data += 8;
			m->m_pkthdr.len -= 8;
			bpf_mtap(ifp, m);
			m->m_len += 8;
			m->m_data -= 8;
			m->m_pkthdr.len += 8;
		}

		send->ec_len = m->m_pkthdr.len;
		m = send->ec_cur_mbuf = esh_adjust_mbufs(sc, m);
		if (m == NULL)
			continue;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, send->ec_dma,
					     m, BUS_DMA_WRITE|BUS_DMA_NOWAIT);
		if (error)
			panic("%s:  eshstart:  "
			      "bus_dmamap_load_mbuf failed err %d\n",
			      device_xname(sc->sc_dev), error);
		send->ec_offset = 0;
	}

	/*
	 * If there are no network packets to send, see if there
	 * are any FP packets to send.
	 *
	 * XXX:  Some users may disagree with these priorities;
	 *       this reduces network latency by increasing FP latency...
	 *	 Note that it also means that FP packets can get
	 *	 locked out so that they *never* get sent, if the
	 *	 network constantly fills up the pipe.  Not good!
	 */

	if ((sc->sc_flags & ESH_FL_FP_RING_UP) != 0 &&
	    send->ec_cur_mbuf == NULL && send->ec_cur_buf == NULL &&
	    send->ec_cur_dmainfo == NULL &&
	    bufq_peek(send->ec_buf_queue) != NULL) {
		struct buf *bp;

#ifdef ESH_PRINTF
		printf("eshstart:  getting a buf from send->ec_queue %p\n",
		       send->ec_queue);
#endif

		bp = send->ec_cur_buf = bufq_get(send->ec_buf_queue);
		send->ec_offset = 0;
		send->ec_len = bp->b_bcount;

		/*
		 * Determine the DMA mapping for the buffer.
		 * If this is too large, what do we do!?
		 */

		error = bus_dmamap_load(sc->sc_dmat, send->ec_dma,
					bp->b_data, bp->b_bcount,
					bp->b_proc,
					BUS_DMA_WRITE|BUS_DMA_NOWAIT);

		if (error)
			panic("%s:  eshstart:  "
			      "bus_dmamap_load failed err %d\n",
			      device_xname(sc->sc_dev), error);
	}

	/*
	 * If there are no packets from strategy to send, see if there
	 * are any FP packets to send from fpwrite.
	 */

	if ((sc->sc_flags & ESH_FL_FP_RING_UP) != 0 &&
	    send->ec_cur_mbuf == NULL && send->ec_cur_buf == NULL &&
	    send->ec_cur_dmainfo == NULL) {
		struct esh_dmainfo *di;

		di = TAILQ_FIRST(&send->ec_di_queue);
		if (di == NULL)
			return;
		TAILQ_REMOVE(&send->ec_di_queue, di, ed_list);

#ifdef ESH_PRINTF
		printf("eshstart:  getting a di from send->ec_di_queue %p\n",
		       &send->ec_di_queue);
#endif

		send->ec_cur_dmainfo = di;
		send->ec_offset = 0;
		send->ec_len = di->ed_dma->dm_mapsize;
	}

	if (send->ec_cur_mbuf == NULL && send->ec_cur_buf == NULL &&
	    send->ec_cur_dmainfo == NULL)
		return;

	assert(send->ec_len);
	assert(send->ec_dma->dm_nsegs ||
	       send->ec_cur_dmainfo->ed_dma->dm_nsegs);
	assert(send->ec_cur_mbuf || send->ec_cur_buf || send->ec_cur_dmainfo);

	esh_send(sc);
	return;
}


/*
 * Put the buffers from the send dmamap into the descriptors and
 * send 'em off...
 */

static void
esh_send(struct esh_softc *sc)
{
	struct esh_send_ring_ctl *send = &sc->sc_send;
	u_int start_producer = send->ec_producer;
	bus_dmamap_t dma;

	if (send->ec_cur_dmainfo != NULL)
		dma = send->ec_cur_dmainfo->ed_dma;
	else
		dma = send->ec_dma;

#ifdef ESH_PRINTF
	printf("esh_send:  producer %x  consumer %x  nsegs %d\n",
	       send->ec_producer, send->ec_consumer, dma->dm_nsegs);
#endif

	esh_dma_sync(sc, send->ec_descr, send->ec_producer, send->ec_consumer,
		     RR_SEND_RING_SIZE, sizeof(struct rr_descr), 1,
		     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (NEXT_SEND(send->ec_producer) != send->ec_consumer &&
	       send->ec_offset < dma->dm_nsegs) {
		int offset = send->ec_producer;

		send->ec_descr[offset].rd_buffer_addr =
			dma->dm_segs[send->ec_offset].ds_addr;
		send->ec_descr[offset].rd_length =
			dma->dm_segs[send->ec_offset].ds_len;
		send->ec_descr[offset].rd_control = 0;

		if (send->ec_offset == 0) {
			/* Start of the dmamap... */
			send->ec_descr[offset].rd_control |=
				RR_CT_PACKET_START;
		}

		if (send->ec_offset + 1 == dma->dm_nsegs) {
			send->ec_descr[offset].rd_control |= RR_CT_PACKET_END;
		}

		send->ec_offset++;
		send->ec_producer = NEXT_SEND(send->ec_producer);
	}

	/*
	 * XXX:   we could optimize the dmamap_sync to just get what we've
	 *        just set up, rather than the whole buffer...
	 */

	bus_dmamap_sync(sc->sc_dmat, dma, 0, dma->dm_mapsize,
			BUS_DMASYNC_PREWRITE);
	esh_dma_sync(sc, send->ec_descr,
		     start_producer, send->ec_consumer,
		     RR_SEND_RING_SIZE, sizeof(struct rr_descr), 1,
		     BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

#ifdef ESH_PRINTF
	if (send->ec_offset != dma->dm_nsegs)
		printf("eshstart:  couldn't fit packet in send ring!\n");
#endif

	if (sc->sc_version == 1) {
		esh_send_cmd(sc, RR_CC_SET_SEND_PRODUCER,
			     0, send->ec_producer);
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				  RR_SEND_PRODUCER, send->ec_producer);
	}
	return;
}


/*
 * Cleanup for the send routine.  When the NIC sends us an event to
 * let us know that it has consumed our buffers, we need to free the
 * buffers, and possibly send another packet.
 */

static void
eshstart_cleanup(struct esh_softc *sc, u_int16_t consumer, int error)
{
	struct esh_send_ring_ctl *send = &sc->sc_send;
	int start_consumer = send->ec_consumer;
	bus_dmamap_t dma;

	if (send->ec_cur_dmainfo != NULL)
		dma = send->ec_cur_dmainfo->ed_dma;
	else
		dma = send->ec_dma;

#ifdef ESH_PRINTF
	printf("eshstart_cleanup:  consumer %x, send->consumer %x\n",
	       consumer, send->ec_consumer);
#endif

	esh_dma_sync(sc, send->ec_descr,
		     send->ec_consumer, consumer,
		     RR_SEND_RING_SIZE, sizeof(struct rr_descr), 0,
		     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (send->ec_consumer != consumer) {
		assert(dma->dm_nsegs);
		assert(send->ec_cur_mbuf || send->ec_cur_buf ||
		       send->ec_cur_dmainfo);

		if (send->ec_descr[send->ec_consumer].rd_control &
		    RR_CT_PACKET_END) {
#ifdef ESH_PRINT
			printf("eshstart_cleanup:  dmamap_sync mapsize %d\n",
			       send->ec_dma->dm_mapsize);
#endif
			bus_dmamap_sync(sc->sc_dmat, dma, 0, dma->dm_mapsize,
					BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, dma);
			if (send->ec_cur_mbuf) {
				m_freem(send->ec_cur_mbuf);
				send->ec_cur_mbuf = NULL;
			} else if (send->ec_cur_dmainfo) {
				send->ec_cur_dmainfo->ed_flags &= ~ESH_DI_BUSY;
				send->ec_cur_dmainfo->ed_error =
					(send->ec_error ? send->ec_error : error);
				send->ec_error = 0;
				wakeup((void *) send->ec_cur_dmainfo);
				send->ec_cur_dmainfo = NULL;
			} else if (send->ec_cur_buf) {
				biodone(send->ec_cur_buf);
				send->ec_cur_buf = NULL;
			} else {
				panic("%s:  eshstart_cleanup:  "
				      "no current mbuf, buf, or dmainfo!\n",
				      device_xname(sc->sc_dev));
			}

			/*
			 * Version 1 of the firmware sent an event each
			 * time it sent out a packet.  Later versions do not
			 * (which results in a considerable speedup), so we
			 * have to keep track here.
			 */

			if (sc->sc_version != 1)
				sc->sc_if.if_opackets++;
		}
		if (error != 0)
			send->ec_error = error;

		send->ec_consumer = NEXT_SEND(send->ec_consumer);
	}

	esh_dma_sync(sc, send->ec_descr,
		     start_consumer, consumer,
		     RR_SEND_RING_SIZE, sizeof(struct rr_descr), 0,
		     BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	eshstart(&sc->sc_if);
}


/*
 * XXX:  Ouch:  The NIC can only send word-aligned buffers, and only
 *       the last buffer in the packet can have a length that is not
 *       a multiple of four!
 *
 * Here we traverse the packet, pick out the bogus mbufs, and fix 'em
 * if possible.  The fix is amazingly expensive, so we sure hope that
 * this is a rare occurance (it seems to be).
 */

static struct mbuf *
esh_adjust_mbufs(struct esh_softc *sc, struct mbuf *m)
{
	struct mbuf *m0, *n, *n0;
	u_int32_t write_len;

	write_len = m->m_pkthdr.len;
#ifdef DIAGNOSTIC
	if (write_len > max_write_len)
		max_write_len = write_len;
#endif

	for (n0 = n = m; n; n = n->m_next) {
		while (n && n->m_len == 0) {
			MFREE(n, m0);
			if (n == m)
				n = n0 = m = m0;
			else
				n = n0->m_next = m0;
		}
		if (n == NULL)
			break;

		if (mtod(n, long) & 3 || (n->m_next && n->m_len & 3)) {
			/* Gotta clean it up */
			struct mbuf *o;
			u_int32_t len;

			sc->sc_misaligned_bufs++;
			MGETHDR(o, M_DONTWAIT, MT_DATA);
			if (!o)
				goto bogosity;

			MCLGET(o, M_DONTWAIT);
			if (!(o->m_flags & M_EXT)) {
				MFREE(o, m0);
				goto bogosity;
			}

			/*
			 * XXX: Copy as much as we can into the
			 *      cluster.  For now we can't have more
			 *      than a cluster in there.  May change.
			 *      I'd prefer not to get this
			 *      down-n-dirty, but we have to be able
			 *      to do this kind of funky copy.
			 */

			len = min(MCLBYTES, write_len);
#ifdef DIAGNOSTIC
			assert(n->m_len <= len);
			assert(len <= MCLBYTES);
#endif

			m_copydata(n, 0, len, mtod(o, void *));
			o->m_pkthdr.len = len;
			m_adj(n, len);
			o->m_len = len;
			o->m_next = n;

			if (n == m)
				m = o;
			else
				n0->m_next = o;
			n = o;
		}
		n0 = n;
		write_len -= n->m_len;
	}
	return m;

bogosity:
	aprint_error_dev(sc->sc_dev, "esh_adjust_mbuf:  unable to allocate cluster for "
	       "mbuf %p, len %x\n",
	       mtod(m, void *), m->m_len);
	m_freem(m);
	return NULL;
}


/*
 * Read in the current valid entries from the ring and forward
 * them to the upper layer protocols.  It is possible that we
 * haven't received the whole packet yet, in which case we just
 * add each of the buffers into the packet until we have the whole
 * thing.
 */

static void
esh_read_snap_ring(struct esh_softc *sc, u_int16_t consumer, int error)
{
	struct ifnet *ifp = &sc->sc_if;
	struct esh_snap_ring_ctl *recv = &sc->sc_snap_recv;
	int start_consumer = recv->ec_consumer;
	u_int16_t control;

	if ((sc->sc_flags & ESH_FL_SNAP_RING_UP) == 0)
		return;

	if (error)
		recv->ec_error = error;

	esh_dma_sync(sc, recv->ec_descr,
		     start_consumer, consumer,
		     RR_SNAP_RECV_RING_SIZE,
		     sizeof(struct rr_descr), 0,
		     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (recv->ec_consumer != consumer) {
		u_int16_t offset = recv->ec_consumer;
		struct mbuf *m;

		m = recv->ec_m[offset];
		m->m_len = recv->ec_descr[offset].rd_length;
		control = recv->ec_descr[offset].rd_control;
		bus_dmamap_sync(sc->sc_dmat, recv->ec_dma[offset], 0, m->m_len,
				BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, recv->ec_dma[offset]);

#ifdef ESH_PRINTF
		printf("esh_read_snap_ring: offset %x addr %p len %x flags %x\n",
		       offset, mtod(m, void *), m->m_len, control);
#endif
		if (control & RR_CT_PACKET_START || !recv->ec_cur_mbuf) {
			if (recv->ec_cur_pkt) {
				m_freem(recv->ec_cur_pkt);
				recv->ec_cur_pkt = NULL;
				printf("%s:  possible skipped packet!\n",
				       device_xname(sc->sc_dev));
			}
			recv->ec_cur_pkt = recv->ec_cur_mbuf = m;
			/* allocated buffers all have pkthdrs... */
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len;
		} else {
			if (!recv->ec_cur_pkt)
				panic("esh_read_snap_ring:  no cur_pkt");

			recv->ec_cur_mbuf->m_next = m;
			recv->ec_cur_mbuf = m;
			recv->ec_cur_pkt->m_pkthdr.len += m->m_len;
		}

		recv->ec_m[offset] = NULL;
		recv->ec_descr[offset].rd_length = 0;
		recv->ec_descr[offset].rd_buffer_addr = 0;

		/* Note that we can START and END on the same buffer */

		if (control & RR_CT_PACKET_END) { /* XXX: RR2_ matches */
			m = recv->ec_cur_pkt;
			if (!error && !recv->ec_error) {
				/*
				 * We have a complete packet, send it up
				 * the stack...
				 */
				ifp->if_ipackets++;

				/*
				 * Check if there's a BPF listener on this
				 * interface.  If so, hand off the raw packet
				 * to BPF.
				 */
				bpf_mtap(ifp, m);
				if ((ifp->if_flags & IFF_RUNNING) == 0) {
					m_freem(m);
				} else {
					m = m_pullup(m,
					    sizeof(struct hippi_header));
					(*ifp->if_input)(ifp, m);
				}
			} else {
				ifp->if_ierrors++;
				recv->ec_error = 0;
				m_freem(m);
			}
			recv->ec_cur_pkt = recv->ec_cur_mbuf = NULL;
		}

		recv->ec_descr[offset].rd_control = 0;
		recv->ec_consumer = NEXT_RECV(recv->ec_consumer);
	}

	esh_dma_sync(sc, recv->ec_descr,
		     start_consumer, consumer,
		     RR_SNAP_RECV_RING_SIZE,
		     sizeof(struct rr_descr), 0,
		     BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	esh_fill_snap_ring(sc);
}


/*
 * Add the SNAP (IEEE 802) receive ring to the NIC.  It is possible
 * that we are doing this after resetting the card, in which case
 * the structures have already been filled in and we may need to
 * resume sending data.
 */

static void
esh_init_snap_ring(struct esh_softc *sc)
{
	struct rr_ring_ctl *ring = sc->sc_recv_ring_table + HIPPI_ULP_802;

	if ((sc->sc_flags & ESH_FL_CLOSING_SNAP) != 0) {
		aprint_error_dev(sc->sc_dev, "can't reopen SNAP ring until ring disable is completed\n");
		return;
	}

	if (ring->rr_entry_size == 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma,
				(char *) ring - (char *) sc->sc_dma_addr,
				sizeof(*ring),
				BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		ring->rr_ring_addr = sc->sc_snap_recv_ring_dma;
		ring->rr_free_bufs = RR_SNAP_RECV_RING_SIZE / 4;
		ring->rr_entries = RR_SNAP_RECV_RING_SIZE;
		ring->rr_entry_size = sizeof(struct rr_descr);
		ring->rr_prod_index = 0;
		sc->sc_snap_recv.ec_producer = 0;
		sc->sc_snap_recv.ec_consumer = 0;
		ring->rr_mode = RR_RR_IP;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma,
				(char *) ring - (char *) sc->sc_dma_addr,
				sizeof(ring),
				BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		esh_send_cmd(sc, RR_CC_ENABLE_RING, HIPPI_ULP_802,
			     sc->sc_snap_recv.ec_producer);
	} else {
		printf("%s:  snap receive ring already initialized!\n",
		       device_xname(sc->sc_dev));
	}
}

static void
esh_close_snap_ring(struct esh_softc *sc)
{
#ifdef ESH_PRINTF
	printf("esh_close_snap_ring:  starting\n");
#endif

	if ((sc->sc_flags & ESH_FL_SNAP_RING_UP) == 0)
		return;

	sc->sc_flags |= ESH_FL_CLOSING_SNAP;
	esh_send_cmd(sc, RR_CC_DISABLE_RING, HIPPI_ULP_802, 0);

	/* Disable event will trigger the rest of the cleanup. */
}

/*
 * Fill in the snap ring with more mbuf buffers so that we can
 * receive traffic.
 */

static void
esh_fill_snap_ring(struct esh_softc *sc)
{
	struct esh_snap_ring_ctl *recv = &sc->sc_snap_recv;
	int start_producer = recv->ec_producer;
	int error;

	esh_dma_sync(sc, recv->ec_descr,
		     recv->ec_producer, recv->ec_consumer,
		     RR_SNAP_RECV_RING_SIZE,
		     sizeof(struct rr_descr), 1,
		     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (NEXT_RECV(recv->ec_producer) != recv->ec_consumer) {
		int offset = recv->ec_producer;
		struct mbuf *m;

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (!m)
			break;
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			break;
		}

		error = bus_dmamap_load(sc->sc_dmat, recv->ec_dma[offset],
					mtod(m, void *), MCLBYTES,
					NULL, BUS_DMA_READ|BUS_DMA_NOWAIT);
		if (error) {
			printf("%s:  esh_fill_recv_ring:  bus_dmamap_load "
			       "failed\toffset %x, error code %d\n",
			       device_xname(sc->sc_dev), offset, error);
			m_free(m);
			break;
		}

		/*
		 * In this implementation, we should only see one segment
		 * per DMA.
		 */

		assert(recv->ec_dma[offset]->dm_nsegs == 1);

		/*
		 * Load into the descriptors.
		 */

		recv->ec_descr[offset].rd_ring =
			(sc->sc_version == 1) ? HIPPI_ULP_802 : 0;
		recv->ec_descr[offset].rd_buffer_addr =
			recv->ec_dma[offset]->dm_segs->ds_addr;
		recv->ec_descr[offset].rd_length =
			recv->ec_dma[offset]->dm_segs->ds_len;
		recv->ec_descr[offset].rd_control = 0;

		bus_dmamap_sync(sc->sc_dmat, recv->ec_dma[offset], 0, MCLBYTES,
				BUS_DMASYNC_PREREAD);

		recv->ec_m[offset] = m;

		recv->ec_producer = NEXT_RECV(recv->ec_producer);
	}

	esh_dma_sync(sc, recv->ec_descr,
		     start_producer, recv->ec_consumer,
		     RR_SNAP_RECV_RING_SIZE,
		     sizeof(struct rr_descr), 1,
		     BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (sc->sc_version == 1)
		esh_send_cmd(sc, RR_CC_SET_RECV_PRODUCER, HIPPI_ULP_802,
			     recv->ec_producer);
	else
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				  RR_SNAP_RECV_PRODUCER, recv->ec_producer);
}

static void
esh_init_fp_rings(struct esh_softc *sc)
{
	struct esh_fp_ring_ctl *recv;
	struct rr_ring_ctl *ring_ctl;
	int ulp;

	for (ulp = 0; ulp < RR_ULP_COUNT; ulp++) {
		ring_ctl = &sc->sc_recv_ring_table[ulp];
		recv = sc->sc_fp_recv[ulp];

		if (recv == NULL)
			continue;

		ring_ctl->rr_ring_addr = recv->ec_dma->dm_segs->ds_addr;
		ring_ctl->rr_free_bufs = RR_FP_RECV_RING_SIZE / 4;
		ring_ctl->rr_entries = RR_FP_RECV_RING_SIZE;
		ring_ctl->rr_entry_size = sizeof(struct rr_descr);
		ring_ctl->rr_prod_index = 0;
		ring_ctl->rr_mode = RR_RR_CHARACTER;
		recv->ec_producer = 0;
		recv->ec_consumer = 0;
		recv->ec_index = -1;

		esh_send_cmd(sc, RR_CC_ENABLE_RING, ulp, recv->ec_producer);
	}
}

static void
esh_read_fp_ring(struct esh_softc *sc, u_int16_t consumer, int error, int ulp)
{
	struct esh_fp_ring_ctl *recv = sc->sc_fp_recv[ulp];
	int start_consumer = recv->ec_consumer;
	u_int16_t control;

#ifdef ESH_PRINTF
	printf("esh_read_fp_ring:  ulp %d, consumer %d, producer %d, old consumer %d\n",
	       recv->ec_ulp, consumer, recv->ec_producer, recv->ec_consumer);
#endif
	if ((sc->sc_flags & ESH_FL_FP_RING_UP) == 0)
		return;

	if (error != 0)
		recv->ec_error = error;

	esh_dma_sync(sc, recv->ec_descr,
		     start_consumer, consumer,
		     RR_FP_RECV_RING_SIZE,
		     sizeof(struct rr_descr), 0,
		     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (recv->ec_consumer != consumer) {
		u_int16_t offset = recv->ec_consumer;

		control = recv->ec_descr[offset].rd_control;

		if (control & RR_CT_PACKET_START) {
			if (recv->ec_read_len) {
				recv->ec_error = 0;
				printf("%s:  ulp %d: possible skipped FP packet!\n",
				       device_xname(sc->sc_dev), recv->ec_ulp);
			}
			recv->ec_seen_end = 0;
			recv->ec_read_len = 0;
		}
		if (recv->ec_seen_end == 0)
			recv->ec_read_len += recv->ec_descr[offset].rd_length;

#if NOT_LAME
		recv->ec_descr[offset].rd_length = 0;
		recv->ec_descr[offset].rd_buffer_addr = 0;
#endif

#ifdef ESH_PRINTF
		printf("esh_read_fp_ring: offset %d addr %d len %d flags %x, total %d\n",
		       offset, recv->ec_descr[offset].rd_buffer_addr,
		       recv->ec_descr[offset].rd_length, control, recv->ec_read_len);
#endif
		/* Note that we can START and END on the same buffer */

		if ((control & RR_CT_PACKET_END) == RR_CT_PACKET_END) {
			if (recv->ec_dmainfo[offset] != NULL) {
				struct esh_dmainfo *di =
				    recv->ec_dmainfo[offset];

				recv->ec_dmainfo[offset] = NULL;
				bus_dmamap_sync(sc->sc_dmat, di->ed_dma,
						0, recv->ec_read_len,
						BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat, di->ed_dma);

				if (!error && !recv->ec_error) {
				/*
				 * XXX:  we oughta do this right, with full
				 *  BPF support and the rest...
				 */
					if (di->ed_buf != NULL) {
						di->ed_buf->b_resid =
							di->ed_buf->b_bcount -
							recv->ec_read_len;
					} else {
						di->ed_read_len =
							recv->ec_read_len;
					}
				} else {
					if (di->ed_buf != NULL) {
						di->ed_buf->b_resid =
							di->ed_buf->b_bcount;
						di->ed_buf->b_error = EIO;
					} else {
						di->ed_error = EIO;
						recv->ec_error = 0;
					}
				}

#ifdef ESH_PRINTF
				printf("esh_read_fp_ring:  ulp %d, read %d, resid %ld\n",
				       recv->ec_ulp, recv->ec_read_len, (di->ed_buf ? di->ed_buf->b_resid : di->ed_read_len));
#endif
				di->ed_flags &=
					~(ESH_DI_BUSY | ESH_DI_READING);
				if (di->ed_buf != NULL)
					biodone(di->ed_buf);
				else
					wakeup((void *) di);
				recv->ec_read_len = 0;
			} else {
#ifdef ESH_PRINTF
				printf("esh_read_fp_ring:  ulp %d, seen end at %d\n",
				       recv->ec_ulp, offset);
#endif
				recv->ec_seen_end = 1;
			}
		}

#if NOT_LAME
		recv->ec_descr[offset].rd_control = 0;
#endif
		recv->ec_consumer = NEXT_RECV(recv->ec_consumer);
	}

	esh_dma_sync(sc, recv->ec_descr,
		     start_consumer, consumer,
		     RR_SNAP_RECV_RING_SIZE,
		     sizeof(struct rr_descr), 0,
		     BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	esh_fill_fp_ring(sc, recv);
}


static void
esh_fill_fp_ring(struct esh_softc *sc, struct esh_fp_ring_ctl *recv)
{
	struct esh_dmainfo *di = recv->ec_cur_dmainfo;
	int start_producer = recv->ec_producer;

#ifdef ESH_PRINTF
        printf("esh_fill_fp_ring:  ulp %d, di %p, producer %d\n",
		recv->ec_ulp, di, start_producer);
#endif

	esh_dma_sync(sc, recv->ec_descr,
		     recv->ec_producer, recv->ec_consumer,
		     RR_SNAP_RECV_RING_SIZE,
		     sizeof(struct rr_descr), 1,
		     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (NEXT_RECV(recv->ec_producer) != recv->ec_consumer) {
		int offset = recv->ec_producer;

		if (di == NULL) {
			/*
			 * Must allow only one reader at a time; see
			 * esh_flush_fp_ring().
			 */

			if (offset != start_producer)
				goto fp_fill_done;

			di = TAILQ_FIRST(&recv->ec_queue);
			if (di == NULL)
				goto fp_fill_done;
			TAILQ_REMOVE(&recv->ec_queue, di, ed_list);
			recv->ec_offset = 0;
			recv->ec_cur_dmainfo = di;
			di->ed_flags |= ESH_DI_READING;
#ifdef ESH_PRINTF
			printf("\toffset %d nsegs %d\n",
			       recv->ec_offset, di->ed_dma->dm_nsegs);
#endif
		}

		/*
		 * Load into the descriptors.
		 */

		recv->ec_descr[offset].rd_ring = 0;
		recv->ec_descr[offset].rd_buffer_addr =
			di->ed_dma->dm_segs[recv->ec_offset].ds_addr;
		recv->ec_descr[offset].rd_length =
			di->ed_dma->dm_segs[recv->ec_offset].ds_len;
		recv->ec_descr[offset].rd_control = 0;
		recv->ec_dmainfo[offset] = NULL;

		if (recv->ec_offset == 0) {
			/* Start of the dmamap... */
			recv->ec_descr[offset].rd_control |=
				RR_CT_PACKET_START;
		}

		assert(recv->ec_offset < di->ed_dma->dm_nsegs);

		recv->ec_offset++;
		if (recv->ec_offset == di->ed_dma->dm_nsegs) {
			recv->ec_descr[offset].rd_control |= RR_CT_PACKET_END;
			recv->ec_dmainfo[offset] = di;
			di = NULL;
			recv->ec_offset = 0;
			recv->ec_cur_dmainfo = NULL;
		}

		recv->ec_producer = NEXT_RECV(recv->ec_producer);
	}

fp_fill_done:
	esh_dma_sync(sc, recv->ec_descr,
		     start_producer, recv->ec_consumer,
		     RR_SNAP_RECV_RING_SIZE,
		     sizeof(struct rr_descr), 1,
		     BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);


	if (sc->sc_version == 1) {
		esh_send_cmd(sc, RR_CC_SET_RECV_PRODUCER, recv->ec_ulp,
			     recv->ec_producer);
	} else {
		union {
			u_int32_t producer;
			u_int8_t indices[4];
		} v;
		int which;
		int i;
		struct esh_fp_ring_ctl *r;

		which = (recv->ec_index / 4) * 4;
#if BAD_PRODUCER
		v.producer = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
					      RR_RECVS_PRODUCER + which);
		NTOHL(v.producer);
#endif
		for (i = 0; i < 4; i++) {
			r = sc->sc_fp_recv_index[i + which];
			if (r != NULL)
				v.indices[i] = r->ec_producer;
			else
				v.indices[i] = 0;
		}
#ifdef ESH_PRINTF
		printf("esh_fill_fp_ring:  ulp %d, updating producer %d:  %.8x\n",
			recv->ec_ulp, which, v.producer);
#endif
		HTONL(v.producer);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				  RR_RECVS_PRODUCER + which, v.producer);
	}
#ifdef ESH_PRINTF
	printf("esh_fill_fp_ring:  ulp %d, final producer %d\n",
		recv->ec_ulp, recv->ec_producer);
#endif
}

/*
 * When a read is interrupted, we need to flush the buffers out of
 * the ring;  otherwise, a driver error could lock a process up,
 * with no way to exit.
 */

static void
esh_flush_fp_ring(struct esh_softc *sc, struct esh_fp_ring_ctl *recv, struct esh_dmainfo *di)
{
	int error = 0;

	/*
	 * If the read request hasn't yet made it to the top of the queue,
	 * just remove it from the queue, and return.
	 */

	if ((di->ed_flags & ESH_DI_READING) != ESH_DI_READING) {
		TAILQ_REMOVE(&recv->ec_queue, di, ed_list);
		return;
	}

#ifdef ESH_PRINTF
	printf("esh_flush_fp_ring:  di->ed_flags %x, ulp %d, producer %x\n",
	       di->ed_flags, recv->ec_ulp, recv->ec_producer);
#endif

	/* Now we gotta get tough.  Issue a discard packet command */

	esh_send_cmd(sc, RR_CC_DISCARD_PKT, recv->ec_ulp,
		     recv->ec_producer - 1);

	/* Wait for it to finish */

	while ((di->ed_flags & ESH_DI_READING) != ESH_DI_READING &&
	       error == 0) {
		error = tsleep((void *) &di->ed_flags, PRIBIO,
			       "esh_flush_fp_ring", hz);
		printf("esh_flush_fp_ring:  di->ed_flags %x, error %d\n",
		       di->ed_flags, error);
		/*
		 * What do I do if this times out or gets interrupted?
		 * Reset the card?  I could get an interrupt before
		 * giving it a chance to check.  Perhaps I oughta wait
		 * awhile?  What about not giving the user a chance
		 * to interrupt, and just expecting a quick answer?
		 * That way I could reset the card if it doesn't
		 * come back right away!
		 */
		if (error != 0) {
			eshreset(sc);
			break;
		}
	}

	/* XXX:  Do we need to clear out the dmainfo pointers */
}


int
eshioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	int error = 0;
	struct esh_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifdrv *ifd = (struct ifdrv *) data;
	u_long len;
	int s;

	s = splnet();

	while (sc->sc_flags & ESH_FL_EEPROM_BUSY) {
		error = tsleep(&sc->sc_flags, PCATCH | PRIBIO,
		    "esheeprom", 0);
		if (error != 0)
			goto ioctl_done;
	}

	switch (cmd) {

	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		if ((sc->sc_flags & ESH_FL_INITIALIZED) == 0) {
			eshinit(sc);
			if ((sc->sc_flags & ESH_FL_INITIALIZED) == 0) {
				error = EIO;
				goto ioctl_done;
			}
		}

		if ((sc->sc_flags & (ESH_FL_RUNCODE_UP | ESH_FL_SNAP_RING_UP))
		    == ESH_FL_RUNCODE_UP) {
			while (sc->sc_flags & ESH_FL_CLOSING_SNAP) {
				error = tsleep((void *) &sc->sc_snap_recv,
					       PRIBIO, "esh_closing_fp_ring",
					       hz);
				if (error != 0)
					goto ioctl_done;
			}
			esh_init_snap_ring(sc);
		}

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			/* The driver doesn't really care about IP addresses */
			break;
#endif
		default:
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */

			ifp->if_flags &= ~IFF_RUNNING;
			esh_close_snap_ring(sc);
			while (sc->sc_flags & ESH_FL_CLOSING_SNAP) {
				error = tsleep((void *) &sc->sc_snap_recv,
					       PRIBIO, "esh_closing_fp_ring",
					       hz);
				if (error != 0)
					goto ioctl_done;
			}

		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {

			if ((sc->sc_flags & ESH_FL_INITIALIZED) == 0) {
				eshinit(sc);
				if ((sc->sc_flags & ESH_FL_INITIALIZED) == 0) {
					error = EIO;
					goto ioctl_done;
				}
			}

			if ((sc->sc_flags & (ESH_FL_RUNCODE_UP | ESH_FL_SNAP_RING_UP)) == ESH_FL_RUNCODE_UP) {
				while (sc->sc_flags & ESH_FL_CLOSING_SNAP) {
					error = tsleep((void *) &sc->sc_snap_recv, PRIBIO, "esh_closing_fp_ring", hz);
					if (error != 0)
						goto ioctl_done;
				}
				esh_init_snap_ring(sc);
			}
		}
		break;

	case SIOCSDRVSPEC: /* Driver-specific configuration calls */
	        cmd = ifd->ifd_cmd;
		len = ifd->ifd_len;
		data = ifd->ifd_data;

		esh_generic_ioctl(sc, cmd, data, len, NULL);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

ioctl_done:
	splx(s);
	return (error);
}


static int
esh_generic_ioctl(struct esh_softc *sc, u_long cmd, void *data,
		  u_long len, struct lwp *l)
{
	struct ifnet *ifp = &sc->sc_if;
	struct rr_eeprom rr_eeprom;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int32_t misc_host_ctl;
	u_int32_t misc_local_ctl;
	u_int32_t address;
	u_int32_t value;
	u_int32_t offset;
	u_int32_t length;
	int error = 0;
	int i;

	/*
	 * If we have a LWP pointer, check to make sure that the
	 * user is privileged before performing any destruction operations.
	 */

	if (l != NULL) {
		switch (cmd) {
		case EIOCGTUNE:
		case EIOCGEEPROM:
		case EIOCGSTATS:
			break;

		default:
			error = kauth_authorize_network(l->l_cred,
			    KAUTH_NETWORK_INTERFACE,
			    KAUTH_REQ_NETWORK_INTERFACE_SETPRIV,
			    ifp, KAUTH_ARG(cmd), NULL);
			if (error)
				return (error);
		}
	}

	switch (cmd) {
	case EIOCGTUNE:
		if (len != sizeof(struct rr_tuning))
			error = EMSGSIZE;
		else {
			error = copyout((void *) &sc->sc_tune, data,
					sizeof(struct rr_tuning));
		}
		break;

	case EIOCSTUNE:
		if ((ifp->if_flags & IFF_UP) == 0) {
			if (len != sizeof(struct rr_tuning)) {
				error = EMSGSIZE;
			} else {
				error = copyin(data, (void *) &sc->sc_tune,
					       sizeof(struct rr_tuning));
			}
		} else {
			error = EBUSY;
		}
		break;

	case EIOCGSTATS:
		if (len != sizeof(struct rr_stats))
			error = EMSGSIZE;
		else
			error = copyout((void *) &sc->sc_gen_info->ri_stats,
					data, sizeof(struct rr_stats));
		break;

	case EIOCGEEPROM:
	case EIOCSEEPROM:
		if ((ifp->if_flags & IFF_UP) != 0) {
			error = EBUSY;
			break;
		}

		if (len != sizeof(struct rr_eeprom)) {
			error = EMSGSIZE;
			break;
		}

		error = copyin(data, (void *) &rr_eeprom, sizeof(rr_eeprom));
		if (error != 0)
			break;

		offset = rr_eeprom.ifr_offset;
		length = rr_eeprom.ifr_length;

		if (length > RR_EE_MAX_LEN * sizeof(u_int32_t)) {
			error = EFBIG;
			break;
		}

		if (offset + length > RR_EE_MAX_LEN * sizeof(u_int32_t)) {
			error = EFAULT;
			break;
		}

		if (offset % 4 || length % 4) {
			error = EIO;
			break;
		}

		/* Halt the processor (preserve NO_SWAP, if set) */

		misc_host_ctl = bus_space_read_4(iot, ioh, RR_MISC_HOST_CTL);
		bus_space_write_4(iot, ioh, RR_MISC_HOST_CTL,
				  (misc_host_ctl & RR_MH_NO_SWAP) |
				  RR_MH_HALT_PROC);

		/* Make the EEPROM accessible */

		misc_local_ctl = bus_space_read_4(iot, ioh, RR_MISC_LOCAL_CTL);
		value = misc_local_ctl &
			~(RR_LC_FAST_PROM | RR_LC_ADD_SRAM | RR_LC_PARITY_ON);
		if (cmd == EIOCSEEPROM)   /* make writable! */
			value |= RR_LC_WRITE_PROM;
		bus_space_write_4(iot, ioh, RR_MISC_LOCAL_CTL, value);

		if (cmd == EIOCSEEPROM) {
			printf("%s:  writing EEPROM\n", device_xname(sc->sc_dev));
			sc->sc_flags |= ESH_FL_EEPROM_BUSY;
		}

		/* Do that EEPROM voodoo that you do so well... */

		address = offset * RR_EE_BYTE_LEN;
		for (i = 0; i < length; i += 4) {
			if (cmd == EIOCGEEPROM) {
				value = esh_read_eeprom(sc, address);
				address += RR_EE_WORD_LEN;
				if (copyout(&value,
					    (char *) rr_eeprom.ifr_buffer + i,
					    sizeof(u_int32_t)) != 0) {
					error = EFAULT;
					break;
				}
			} else {
				if (copyin((char *) rr_eeprom.ifr_buffer + i,
					   &value, sizeof(u_int32_t)) != 0) {
					error = EFAULT;
					break;
				}
				if (esh_write_eeprom(sc, address,
						     value) != 0) {
					error = EIO;
					break;
				}

				/*
				 * Have to give up control now and
				 * then, so sleep for a clock tick.
				 * Might be good to figure out how
				 * long a tick is, so that we could
				 * intelligently chose the frequency
				 * of these pauses.
				 */

				if (i % 40 == 0) {
					tsleep(&sc->sc_flags,
					       PRIBIO, "eshweeprom", 1);
				}

				address += RR_EE_WORD_LEN;
			}
		}

		bus_space_write_4(iot, ioh, RR_MISC_LOCAL_CTL, misc_local_ctl);
		if (cmd == EIOCSEEPROM) {
			sc->sc_flags &= ~ESH_FL_EEPROM_BUSY;
			wakeup(&sc->sc_flags);
			printf("%s:  done writing EEPROM\n",
			       device_xname(sc->sc_dev));
		}
		break;

	case EIOCRESET:
		eshreset(sc);
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}


void
eshreset(struct esh_softc *sc)
{
	int s;

	s = splnet();
	eshstop(sc);
	eshinit(sc);
	splx(s);
}

/*
 * The NIC expects a watchdog command every 10 seconds.  If it doesn't
 * get the watchdog, it figures the host is dead and stops.  When it does
 * get the command, it'll generate a watchdog event to let the host know
 * that it is still alive.  We watch for this.
 */

void
eshwatchdog(struct ifnet *ifp)
{
	struct esh_softc *sc = ifp->if_softc;

	if (!sc->sc_watchdog) {
		printf("%s:  watchdog timer expired.  "
		       "Should reset interface!\n",
		       device_xname(sc->sc_dev));
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		eshstatus(sc);
#if 0
 		eshstop(sc);  /*   DON'T DO THIS, it'll clear data we
				   could use to debug it! */
#endif
	} else {
		sc->sc_watchdog = 0;

		esh_send_cmd(sc, RR_CC_WATCHDOG, 0, 0);
		ifp->if_timer = 5;
	}
}


/*
 * Stop the NIC and throw away packets that have started to be sent,
 * but didn't make it all the way.  Re-adjust the various queue
 * pointers to account for this.
 */

void
eshstop(struct esh_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int32_t misc_host_ctl;
	int i;

	if (!(sc->sc_flags & ESH_FL_INITIALIZED))
		return;

	/* Just shut it all down.  This isn't pretty, but it works */

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dma, 0, sc->sc_dma_size,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	misc_host_ctl = bus_space_read_4(iot, ioh, RR_MISC_HOST_CTL);
	bus_space_write_4(iot, ioh, RR_MISC_HOST_CTL,
			  (misc_host_ctl & RR_MH_NO_SWAP) | RR_MH_HALT_PROC);
	sc->sc_flags = 0;
	ifp->if_timer = 0;  /* turn off watchdog timer */

	while (sc->sc_snap_recv.ec_consumer
               != sc->sc_snap_recv.ec_producer) {
		u_int16_t offset = sc->sc_snap_recv.ec_consumer;

		bus_dmamap_unload(sc->sc_dmat,
				  sc->sc_snap_recv.ec_dma[offset]);
		m_free(sc->sc_snap_recv.ec_m[offset]);
		sc->sc_snap_recv.ec_m[offset] = NULL;
		sc->sc_snap_recv.ec_consumer =
			NEXT_RECV(sc->sc_snap_recv.ec_consumer);
		wakeup((void *) &sc->sc_snap_recv);
	}

	/* Handle FP rings */

	for (i = 0; i < RR_ULP_COUNT; i++) {
		struct esh_fp_ring_ctl *ring = sc->sc_fp_recv[i];
		struct esh_dmainfo *di = NULL;

		if (ring == NULL)
			continue;

		/* Get rid of outstanding buffers */

		esh_dma_sync(sc, ring->ec_descr,
			     ring->ec_consumer, ring->ec_producer,
			     RR_FP_RECV_RING_SIZE, sizeof(struct rr_descr), 0,
			     BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		while (ring->ec_consumer != ring->ec_producer) {
			di = ring->ec_dmainfo[ring->ec_consumer];
			if (di != NULL)
				break;
			ring->ec_consumer = NEXT_RECV(ring->ec_consumer);
		}
		if (di == NULL)
			di = ring->ec_cur_dmainfo;

		if (di != NULL) {
			bus_dmamap_unload(sc->sc_dmat, di->ed_dma);
			di->ed_error = EIO;
			di->ed_flags = 0;
			wakeup((void *) &di->ed_flags);	/* packet discard */
			wakeup((void *) di);		/* wait on read */
		}
		wakeup((void *) &ring->ec_ulp);		/* ring create */
		wakeup((void *) &ring->ec_index);	/* ring disable */
	}

	/* XXX:  doesn't clear bufs being sent */

	bus_dmamap_unload(sc->sc_dmat, sc->sc_send.ec_dma);
	if (sc->sc_send.ec_cur_mbuf) {
		m_freem(sc->sc_send.ec_cur_mbuf);
	} else if (sc->sc_send.ec_cur_buf) {
		struct buf *bp = sc->sc_send.ec_cur_buf;

		bp->b_resid = bp->b_bcount;
		bp->b_error = EIO;
		biodone(bp);
	} else if (sc->sc_send.ec_cur_dmainfo) {
		struct esh_dmainfo *di = sc->sc_send.ec_cur_dmainfo;

		di->ed_flags &= ~ESH_DI_BUSY;
		di->ed_error = EIO;
		wakeup((void *) di);
	}
	sc->sc_send.ec_cur_mbuf = NULL;
	sc->sc_send.ec_cur_buf = NULL;
	sc->sc_send.ec_cur_dmainfo = NULL;

	/*
	 * Clear out the index values, since they'll be useless
	 * when we restart.
	 */

	memset(sc->sc_fp_recv_index, 0,
	      sizeof(struct esh_fp_ring_ctl *) * RR_MAX_RECV_RING);

	/* Be sure to wake up any other processes waiting on driver action. */

	wakeup(sc);		/* Wait on initialization */
	wakeup(&sc->sc_flags);	/* Wait on EEPROM write */

	/*
	 * XXX:  I have to come up with a way to avoid handling interrupts
	 *       received before this shuts down the card, but processed
	 *       afterwards!
	 */
}

/*
 * Read a value from the eeprom.  This expects that the NIC has already
 * been tweaked to put it into the right state for reading from the
 * EEPROM -- the HALT bit is set in the MISC_HOST_CTL register,
 * and the FAST_PROM, ADD_SRAM, and PARITY flags have been cleared
 * in the MISC_LOCAL_CTL register.
 *
 * The EEPROM layout is a little weird.  There is a valid byte every
 * eight bytes.  Words are then smeared out over 32 bytes.
 * All addresses listed here are the actual starting addresses.
 */

static u_int32_t
esh_read_eeprom(struct esh_softc *sc, u_int32_t addr)
{
	int i;
	u_int32_t tmp;
	u_int32_t value = 0;

	/* If the offset hasn't been added, add it.  Otherwise pass through */

	if (!(addr & RR_EE_OFFSET))
		addr += RR_EE_OFFSET;

	for (i = 0; i < 4; i++, addr += RR_EE_BYTE_LEN) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				  RR_WINDOW_BASE, addr);
		tmp = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				       RR_WINDOW_DATA);
		value = (value << 8) | ((tmp >> 24) & 0xff);
	}
	return value;
}


/*
 * Write a value to the eeprom.  Just like esh_read_eeprom, this routine
 * expects that the NIC has already been tweaked to put it into the right
 * state for reading from the EEPROM.  Things are further complicated
 * in that we need to read each byte after we write it to ensure that
 * the new value has been successfully written.  It can take as long
 * as 1ms (!) to write a byte.
 */

static int
esh_write_eeprom(struct esh_softc *sc, u_int32_t addr, u_int32_t value)
{
	int i, j;
	u_int32_t shifted_value, tmp = 0;

	/* If the offset hasn't been added, add it.  Otherwise pass through */

	if (!(addr & RR_EE_OFFSET))
		addr += RR_EE_OFFSET;

	for (i = 0; i < 4; i++, addr += RR_EE_BYTE_LEN) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				  RR_WINDOW_BASE, addr);

		/*
		 * Get the byte out of value, starting with the top, and
		 * put it into the top byte of the word to write.
		 */

		shifted_value = ((value >> ((3 - i) * 8)) & 0xff) << 24;
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, RR_WINDOW_DATA,
				  shifted_value);
		for (j = 0; j < 50; j++) {
			tmp = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
					       RR_WINDOW_DATA);
			if (tmp == shifted_value)
				break;
			delay(500);  /* 50us break * 20 = 1ms */
		}
		if (tmp != shifted_value)
			return -1;
	}

	return 0;
}


/*
 * Send a command to the NIC.  If there is no room in the command ring,
 * panic.
 */

static void
esh_send_cmd(struct esh_softc *sc, u_int8_t cmd, u_int8_t ring, u_int8_t index)
{
	union rr_cmd c;

#define NEXT_CMD(i) (((i) + 0x10 - 1) & 0x0f)

	c.l = 0;
	c.b.rc_code = cmd;
	c.b.rc_ring = ring;
	c.b.rc_index = index;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			  RR_COMMAND_RING + sizeof(c) * sc->sc_cmd_producer,
			  c.l);

#ifdef ESH_PRINTF
	/* avoid annoying messages when possible */
	if (cmd != RR_CC_WATCHDOG)
		printf("esh_send_cmd:  cmd %x ring %d index %d slot %x\n",
		       cmd, ring, index, sc->sc_cmd_producer);
#endif

	sc->sc_cmd_producer = NEXT_CMD(sc->sc_cmd_producer);
}


/*
 * Write an address to the device.
 * XXX:  This belongs in bus-dependent land!
 */

static void
esh_write_addr(bus_space_tag_t iot, bus_space_handle_t ioh, bus_addr_t addr, bus_addr_t value)
{
	bus_space_write_4(iot, ioh, addr, 0);
	bus_space_write_4(iot, ioh, addr + sizeof(u_int32_t), value);
}


/* Copy the RunCode from EEPROM to SRAM.  Ughly. */

static void
esh_reset_runcode(struct esh_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int32_t value;
	u_int32_t len;
	u_int32_t i;
	u_int32_t segments;
	u_int32_t ee_addr;
	u_int32_t rc_addr;
	u_int32_t sram_addr;

	/* Zero the SRAM */

	for (i = 0; i < sc->sc_sram_size; i += 4) {
		bus_space_write_4(iot, ioh, RR_WINDOW_BASE, i);
		bus_space_write_4(iot, ioh, RR_WINDOW_DATA, 0);
	}

	/* Find the address of the segment description section */

	rc_addr = esh_read_eeprom(sc, RR_EE_RUNCODE_SEGMENTS);
	segments = esh_read_eeprom(sc, rc_addr);

	for (i = 0; i < segments; i++) {
		rc_addr += RR_EE_WORD_LEN;
		sram_addr = esh_read_eeprom(sc, rc_addr);
		rc_addr += RR_EE_WORD_LEN;
		len = esh_read_eeprom(sc, rc_addr);
		rc_addr += RR_EE_WORD_LEN;
		ee_addr = esh_read_eeprom(sc, rc_addr);

		while (len--) {
			value = esh_read_eeprom(sc, ee_addr);
			bus_space_write_4(iot, ioh, RR_WINDOW_BASE, sram_addr);
			bus_space_write_4(iot, ioh, RR_WINDOW_DATA, value);

			ee_addr += RR_EE_WORD_LEN;
			sram_addr += 4;
		}
	}
}


/*
 * Perform bus DMA syncing operations on various rings.
 * We have to worry about our relative position in the ring,
 * and whether the ring has wrapped.  All of this code should take
 * care of those worries.
 */

static void
esh_dma_sync(struct esh_softc *sc, void *mem, int start, int end, int entries, int size, int do_equal, int ops)
{
	int offset = (char *)mem - (char *)sc->sc_dma_addr;

	if (start < end) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma,
				offset + start * size,
				(end - start) * size, ops);
	} else if (do_equal || start != end) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma,
				offset,
				end * size, ops);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma,
				offset + start * size,
				(entries - start) * size, ops);
	}
}


static struct esh_dmainfo *
esh_new_dmainfo(struct esh_softc *sc)
{
	struct esh_dmainfo *di;
	int s;

	s = splnet();

	di = TAILQ_FIRST(&sc->sc_dmainfo_freelist);
	if (di != NULL) {
		TAILQ_REMOVE(&sc->sc_dmainfo_freelist, di, ed_list);
		sc->sc_dmainfo_freelist_count--;
		splx(s);
		return di;
	}

	/* None sitting around, so build one now... */

	di = (struct esh_dmainfo *) malloc(sizeof(*di), M_DEVBUF,
	    M_WAITOK|M_ZERO);
	assert(di != NULL);

	if (bus_dmamap_create(sc->sc_dmat, ESH_MAX_NSEGS * RR_DMA_MAX,
			      ESH_MAX_NSEGS, RR_DMA_MAX, RR_DMA_BOUNDARY,
			      BUS_DMA_ALLOCNOW | BUS_DMA_WAITOK,
			      &di->ed_dma)) {
		printf("%s:  failed dmainfo bus_dmamap_create\n",
		       device_xname(sc->sc_dev));
		free(di,  M_DEVBUF);
		di = NULL;
	}

	splx(s);
	return di;
}

static void
esh_free_dmainfo(struct esh_softc *sc, struct esh_dmainfo *di)
{
	int s = splnet();

	assert(di != NULL);
	di->ed_buf = NULL;
	TAILQ_INSERT_TAIL(&sc->sc_dmainfo_freelist, di, ed_list);
	sc->sc_dmainfo_freelist_count++;
#ifdef ESH_PRINTF
	printf("esh_free_dmainfo:  freelist count %d\n", sc->sc_dmainfo_freelist_count);
#endif

	splx(s);
}


/* ------------------------- debugging functions --------------------------- */

/*
 * Print out status information about the NIC and the driver.
 */

static int
eshstatus(struct esh_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;

	/* XXX:   This looks pathetic, and should be improved! */

	printf("%s:  status -- fail1 %x fail2 %x\n",
	       device_xname(sc->sc_dev),
	       bus_space_read_4(iot, ioh, RR_RUNCODE_FAIL1),
	       bus_space_read_4(iot, ioh, RR_RUNCODE_FAIL2));
	printf("\tmisc host ctl %x  misc local ctl %x\n",
	       bus_space_read_4(iot, ioh, RR_MISC_HOST_CTL),
	       bus_space_read_4(iot, ioh, RR_MISC_LOCAL_CTL));
	printf("\toperating mode %x  event producer %x\n",
	       bus_space_read_4(iot, ioh, RR_MODE_AND_STATUS),
	       bus_space_read_4(iot, ioh, RR_EVENT_PRODUCER));
	printf("\tPC %x  max rings %x\n",
	       bus_space_read_4(iot, ioh, RR_PROC_PC),
	       bus_space_read_4(iot, ioh, RR_MAX_RECV_RINGS));
	printf("\tHIPPI tx state %x  rx state %x\n",
	       bus_space_read_4(iot, ioh, RR_TX_STATE),
	       bus_space_read_4(iot, ioh, RR_RX_STATE));
	printf("\tDMA write state %x  read state %x\n",
	       bus_space_read_4(iot, ioh, RR_DMA_WRITE_STATE),
	       bus_space_read_4(iot, ioh, RR_DMA_READ_STATE));
	printf("\tDMA write addr %x%x  read addr %x%x\n",
	       bus_space_read_4(iot, ioh, RR_WRITE_HOST),
	       bus_space_read_4(iot, ioh, RR_WRITE_HOST + 4),
	       bus_space_read_4(iot, ioh, RR_READ_HOST),
	       bus_space_read_4(iot, ioh, RR_READ_HOST + 4));

	for (i = 0; i < 64; i++)
		if (sc->sc_gen_info->ri_stats.rs_stats[i])
			printf("stat %x is %x\n", i * 4,
			       sc->sc_gen_info->ri_stats.rs_stats[i]);

	return 0;
}


#ifdef ESH_PRINTF

/* Check to make sure that the NIC is still running */

static int
esh_check(struct esh_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (bus_space_read_4(iot, ioh, RR_MISC_HOST_CTL) & RR_MH_HALT_PROC) {
		printf("esh_check:  NIC stopped\n");
		eshstatus(sc);
		return 1;
	} else {
		return 0;
	}
}
#endif

