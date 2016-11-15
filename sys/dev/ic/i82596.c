/* $NetBSD: i82596.c,v 1.32 2015/02/21 11:39:05 martin Exp $ */

/*
 * Copyright (c) 2003 Jochen Kunz.
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
 * 3. The name of Jochen Kunz may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOCHEN KUNZ
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JOCHEN KUNZ
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the Intel i82596CA and i82596DX/SX 10MBit/s Ethernet chips.
 *
 * It operates the i82596 in 32-Bit Linear Mode, opposed to the old i82586
 * ie(4) driver (src/sys/dev/ic/i82586.c), that degrades the i82596 to
 * i82586 compatibility mode.
 *
 * Documentation about these chips can be found at
 *
 *	http://developer.intel.com/design/network/datashts/290218.htm
 *	http://developer.intel.com/design/network/datashts/290219.htm
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i82596.c,v 1.32 2015/02/21 11:39:05 martin Exp $");

/* autoconfig and device stuff */
#include <sys/param.h>
#include <sys/device.h>
#include <sys/conf.h>
#include "locators.h"
#include "ioconf.h"

/* bus_space / bus_dma etc. */
#include <sys/bus.h>
#include <sys/intr.h>

/* general system data and functions */
#include <sys/systm.h>
#include <sys/ioctl.h>

/* tsleep / sleep / wakeup */
#include <sys/proc.h>
/* hz for above */
#include <sys/kernel.h>

/* network stuff */
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <net/bpf.h>

#include <dev/ic/i82596reg.h>
#include <dev/ic/i82596var.h>

/* Supported chip variants */
const char *i82596_typenames[] = { "unknown", "DX/SX", "CA" };

/* media change and status callback */
static int iee_mediachange(struct ifnet *);
static void iee_mediastatus(struct ifnet *, struct ifmediareq *);

/* interface routines to upper protocols */
static void iee_start(struct ifnet *);			/* initiate output */
static int iee_ioctl(struct ifnet *, u_long, void *);	/* ioctl routine */
static int iee_init(struct ifnet *);			/* init routine */
static void iee_stop(struct ifnet *, int);		/* stop routine */
static void iee_watchdog(struct ifnet *);		/* timer routine */

/* internal helper functions */
static void iee_cb_setup(struct iee_softc *, uint32_t);

/*
 * Things a MD frontend has to provide:
 *
 * The functions via function pointers in the softc:
 *	int (*sc_iee_cmd)(struct iee_softc *sc, uint32_t cmd);
 *	int (*sc_iee_reset)(struct iee_softc *sc);
 *	void (*sc_mediastatus)(struct ifnet *, struct ifmediareq *);
 *	int (*sc_mediachange)(struct ifnet *);
 *
 * sc_iee_cmd(): send a command to the i82596 by writing the cmd parameter
 *	to the SCP cmd word and issuing a Channel Attention.
 * sc_iee_reset(): initiate a reset, supply the address of the SCP to the
 *	chip, wait for the chip to initialize and ACK interrupts that
 *	this may have caused by calling (sc->sc_iee_cmd)(sc, IEE_SCB_ACK);
 * This functions must carefully bus_dmamap_sync() all data they have touched!
 *
 * sc_mediastatus() and sc_mediachange() are just MD hooks to the according
 * MI functions. The MD frontend may set this pointers to NULL when they
 * are not needed.
 * 
 * sc->sc_type has to be set to I82596_UNKNOWN or I82596_DX or I82596_CA.
 * This is for printing out the correct chip type at attach time only. The
 * MI backend doesn't distinguish different chip types when programming
 * the chip.
 * 
 * IEE_NEED_SWAP in sc->sc_flags has to be cleared on little endian hardware
 * and set on big endian hardware, when endianess conversion is not done
 * by the bus attachment but done by i82596 chip itself.
 * Usually you need to set IEE_NEED_SWAP on big endian machines
 * where the hardware (the LE/~BE pin) is configured as BE mode.
 * 
 * If the chip is configured as BE mode, all 8 bit (byte) and 16 bit (word)
 * entities can be written in big endian. But Rev A chip doesn't support
 * 32 bit (dword) entities with big endian byte ordering, so we have to
 * treat all 32 bit (dword) entities as two 16 bit big endian entities.
 * Rev B and C chips support big endian byte ordering for 32 bit entities,
 * and this new feature is enabled by IEE_SYSBUS_BE in the sysbus byte.
 *
 * With the IEE_SYSBUS_BE feature, all 32 bit address ponters are
 * treated as true 32 bit entities but the SCB absolute address and
 * statistical counters are still treated as two 16 bit big endian entities,
 * so we have to always swap high and low words for these entities.
 * IEE_SWAP32() should be used for the SCB address and statistical counters,
 * and IEE_SWAPA32() should be used for other 32 bit pointers in the shmem.
 *
 * IEE_REV_A flag must be set in sc->sc_flags if the IEE_SYSBUS_BE feature
 * is disabled even on big endian machines for the old Rev A chip in backend.
 * 
 * sc->sc_cl_align must be set to 1 or to the cache line size. When set to
 * 1 no special alignment of DMA descriptors is done. If sc->sc_cl_align != 1
 * it forces alignment of the data structures in the shared memory to a multiple
 * of sc->sc_cl_align. This is needed on some hppa machines that have non DMA
 * I/O coherent caches and are unable to map the shared memory uncachable.
 * (At least pre PA7100LC CPUs are unable to map memory uncachable.)
 * 
 * The MD frontend also has to set sc->sc_cl_align and sc->sc_sysbus
 * to allocate and setup shared DMA memory in MI iee_attach().
 * All communication with the chip is done via this shared memory.
 * This memory is mapped with BUS_DMA_COHERENT so it will be uncached
 * if possible for archs with non DMA I/O coherent caches.
 * The base of the memory needs to be aligned to an even address
 * if sc->sc_cl_align == 1 and aligned to a cache line if sc->sc_cl_align != 1.
 * Each descriptor offsets are calculated in iee_attach() to handle this.
 * 
 * An interrupt with iee_intr() as handler must be established.
 * 
 * Call void iee_attach(struct iee_softc *sc, uint8_t *ether_address,
 * int *media, int nmedia, int defmedia); when everything is set up. First
 * parameter is a pointer to the MI softc, ether_address is an array that
 * contains the ethernet address. media is an array of the media types
 * provided by the hardware. The members of this array are supplied to
 * ifmedia_add() in sequence. nmedia is the count of elements in media.
 * defmedia is the default media that is set via ifmedia_set().
 * nmedia and defmedia are ignored when media == NULL.
 * 
 * The MD backend may call iee_detach() to detach the device.
 * 
 * See sys/arch/hppa/gsc/if_iee_gsc.c for an example.
 */


/*
 * How frame reception is done:
 * Each Receive Frame Descriptor has one associated Receive Buffer Descriptor.
 * Each RBD points to the data area of an mbuf cluster. The RFDs are linked
 * together in a circular list. sc->sc_rx_done is the count of RFDs in the
 * list already processed / the number of the RFD that has to be checked for
 * a new frame first at the next RX interrupt. Upon successful reception of
 * a frame the mbuf cluster is handled to upper protocol layers, a new mbuf
 * cluster is allocated and the RFD / RBD are reinitialized accordingly.
 * 
 * When a RFD list overrun occurred the whole RFD and RBD lists are
 * reinitialized and frame reception is started again.
 */
int
iee_intr(void *intarg)
{
	struct iee_softc *sc = intarg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct iee_rfd *rfd;
	struct iee_rbd *rbd;
	bus_dmamap_t rx_map;
	struct mbuf *rx_mbuf;
	struct mbuf *new_mbuf;
	int scb_status;
	int scb_cmd;
	int n, col;
	uint16_t status, count, cmd;

	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		(sc->sc_iee_cmd)(sc, IEE_SCB_ACK);
		return 1;
	}
	IEE_SCBSYNC(sc, BUS_DMASYNC_POSTREAD);
	scb_status = SC_SCB(sc)->scb_status;
	scb_cmd = SC_SCB(sc)->scb_cmd;
	for (;;) {
		rfd = SC_RFD(sc, sc->sc_rx_done);
		IEE_RFDSYNC(sc, sc->sc_rx_done,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		status = rfd->rfd_status;
		if ((status & IEE_RFD_C) == 0) {
			IEE_RFDSYNC(sc, sc->sc_rx_done, BUS_DMASYNC_PREREAD);
			break;
		}
		rfd->rfd_status = 0;
		IEE_RFDSYNC(sc, sc->sc_rx_done,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* At least one packet was received. */
		rx_map = sc->sc_rx_map[sc->sc_rx_done];
		rx_mbuf = sc->sc_rx_mbuf[sc->sc_rx_done];
		IEE_RBDSYNC(sc, (sc->sc_rx_done + IEE_NRFD - 1) % IEE_NRFD,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		SC_RBD(sc, (sc->sc_rx_done + IEE_NRFD - 1) % IEE_NRFD)->rbd_size
		    &= ~IEE_RBD_EL;
		IEE_RBDSYNC(sc, (sc->sc_rx_done + IEE_NRFD - 1) % IEE_NRFD,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		rbd = SC_RBD(sc, sc->sc_rx_done);
		IEE_RBDSYNC(sc, sc->sc_rx_done,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		count = rbd->rbd_count;
		if ((status & IEE_RFD_OK) == 0
		    || (count & IEE_RBD_EOF) == 0
		    || (count & IEE_RBD_F) == 0){
			/* Receive error, skip frame and reuse buffer. */
			rbd->rbd_count = 0;
			rbd->rbd_size = IEE_RBD_EL | rx_map->dm_segs[0].ds_len;
			IEE_RBDSYNC(sc, sc->sc_rx_done,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			printf("%s: iee_intr: receive error %d, rfd_status="
			    "0x%.4x, rfd_count=0x%.4x\n",
			    device_xname(sc->sc_dev),
			    ++sc->sc_rx_err, status, count);
			sc->sc_rx_done = (sc->sc_rx_done + 1) % IEE_NRFD;
			continue;
		}
		bus_dmamap_sync(sc->sc_dmat, rx_map, 0, rx_map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		rx_mbuf->m_pkthdr.len = rx_mbuf->m_len =
		    count & IEE_RBD_COUNT;
		rx_mbuf->m_pkthdr.rcvif = ifp;
		MGETHDR(new_mbuf, M_DONTWAIT, MT_DATA);
		if (new_mbuf == NULL) {
			printf("%s: iee_intr: can't allocate mbuf\n",
			    device_xname(sc->sc_dev));
			break;
		}
		MCLAIM(new_mbuf, &sc->sc_ethercom.ec_rx_mowner);
		MCLGET(new_mbuf, M_DONTWAIT);
		if ((new_mbuf->m_flags & M_EXT) == 0) {
			printf("%s: iee_intr: can't alloc mbuf cluster\n",
			    device_xname(sc->sc_dev));
			m_freem(new_mbuf);
			break;
		}
		bus_dmamap_unload(sc->sc_dmat, rx_map);
		new_mbuf->m_len = new_mbuf->m_pkthdr.len = MCLBYTES - 2;
		new_mbuf->m_data += 2;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, rx_map,
		    new_mbuf, BUS_DMA_READ | BUS_DMA_NOWAIT) != 0)
			panic("%s: iee_intr: can't load RX DMA map\n",
			    device_xname(sc->sc_dev));
		bus_dmamap_sync(sc->sc_dmat, rx_map, 0,
		    rx_map->dm_mapsize, BUS_DMASYNC_PREREAD);
		bpf_mtap(ifp, rx_mbuf);
		(*ifp->if_input)(ifp, rx_mbuf);
		ifp->if_ipackets++;
		sc->sc_rx_mbuf[sc->sc_rx_done] = new_mbuf;
		rbd->rbd_count = 0;
		rbd->rbd_size = IEE_RBD_EL | rx_map->dm_segs[0].ds_len;
		rbd->rbd_rb_addr = IEE_SWAPA32(rx_map->dm_segs[0].ds_addr);
		IEE_RBDSYNC(sc, sc->sc_rx_done,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		sc->sc_rx_done = (sc->sc_rx_done + 1) % IEE_NRFD;
	}
	if ((scb_status & IEE_SCB_RUS) == IEE_SCB_RUS_NR1
	    || (scb_status & IEE_SCB_RUS) == IEE_SCB_RUS_NR2
	    || (scb_status & IEE_SCB_RUS) == IEE_SCB_RUS_NR3) {
		/* Receive Overrun, reinit receive ring buffer. */
		for (n = 0 ; n < IEE_NRFD ; n++) {
			rfd = SC_RFD(sc, n);
			rbd = SC_RBD(sc, n);
			rfd->rfd_cmd = IEE_RFD_SF;
			rfd->rfd_link_addr =
			    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_rfd_off
			    + sc->sc_rfd_sz * ((n + 1) % IEE_NRFD)));
			rbd->rbd_next_rbd =
			    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_rbd_off
			    + sc->sc_rbd_sz * ((n + 1) % IEE_NRFD)));
			rbd->rbd_size = IEE_RBD_EL |
			    sc->sc_rx_map[n]->dm_segs[0].ds_len;
			rbd->rbd_rb_addr =
			    IEE_SWAPA32(sc->sc_rx_map[n]->dm_segs[0].ds_addr);
		}
		SC_RFD(sc, 0)->rfd_rbd_addr =
		    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_rbd_off));
		sc->sc_rx_done = 0;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_shmem_map, sc->sc_rfd_off,
		    sc->sc_rfd_sz * IEE_NRFD + sc->sc_rbd_sz * IEE_NRFD,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		(sc->sc_iee_cmd)(sc, IEE_SCB_RUC_ST);
		printf("%s: iee_intr: receive ring buffer overrun\n",
		    device_xname(sc->sc_dev));
	}

	if (sc->sc_next_cb != 0) {
		IEE_CBSYNC(sc, sc->sc_next_cb - 1,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		status = SC_CB(sc, sc->sc_next_cb - 1)->cb_status;
		IEE_CBSYNC(sc, sc->sc_next_cb - 1,
		    BUS_DMASYNC_PREREAD);
		if ((status & IEE_CB_C) != 0) {
			/* CMD list finished */
			ifp->if_timer = 0;
			if (sc->sc_next_tbd != 0) {
				/* A TX CMD list finished, cleanup */
				for (n = 0 ; n < sc->sc_next_cb ; n++) {
					m_freem(sc->sc_tx_mbuf[n]);
					sc->sc_tx_mbuf[n] = NULL;
					bus_dmamap_unload(sc->sc_dmat,
					    sc->sc_tx_map[n]);
					IEE_CBSYNC(sc, n,
				    	    BUS_DMASYNC_POSTREAD|
					    BUS_DMASYNC_POSTWRITE);
					status = SC_CB(sc, n)->cb_status;
					IEE_CBSYNC(sc, n,
				    	    BUS_DMASYNC_PREREAD);
					if ((status & IEE_CB_COL) != 0 &&
					    (status & IEE_CB_MAXCOL) == 0)
						col = 16;
					else
						col = status
						    & IEE_CB_MAXCOL;
					sc->sc_tx_col += col;
					if ((status & IEE_CB_OK) != 0) {
						ifp->if_opackets++;
						ifp->if_collisions += col;
					}
				}
				sc->sc_next_tbd = 0;
				ifp->if_flags &= ~IFF_OACTIVE;
			}
			for (n = 0 ; n < sc->sc_next_cb; n++) {
				/*
				 * Check if a CMD failed, but ignore TX errors.
				 */
				IEE_CBSYNC(sc, n,
				    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
				cmd = SC_CB(sc, n)->cb_cmd;
				status = SC_CB(sc, n)->cb_status;
				IEE_CBSYNC(sc, n, BUS_DMASYNC_PREREAD);
				if ((cmd & IEE_CB_CMD) != IEE_CB_CMD_TR &&
				    (status & IEE_CB_OK) == 0)
					printf("%s: iee_intr: scb_status=0x%x "
					    "scb_cmd=0x%x failed command %d: "
					    "cb_status[%d]=0x%.4x "
					    "cb_cmd[%d]=0x%.4x\n",
					    device_xname(sc->sc_dev),
					    scb_status, scb_cmd,
					    ++sc->sc_cmd_err,
					    n, status, n, cmd);
			}
			sc->sc_next_cb = 0;
			if ((sc->sc_flags & IEE_WANT_MCAST) != 0) {
				iee_cb_setup(sc, IEE_CB_CMD_MCS |
				    IEE_CB_S | IEE_CB_EL | IEE_CB_I);
				(sc->sc_iee_cmd)(sc, IEE_SCB_CUC_EXE);
			} else
				/* Try to get deferred packets going. */
				iee_start(ifp);
		}
	}
	if (IEE_SWAP32(SC_SCB(sc)->scb_crc_err) != sc->sc_crc_err) {
		sc->sc_crc_err = IEE_SWAP32(SC_SCB(sc)->scb_crc_err);
		printf("%s: iee_intr: crc_err=%d\n", device_xname(sc->sc_dev),
		    sc->sc_crc_err);
	}
	if (IEE_SWAP32(SC_SCB(sc)->scb_align_err) != sc->sc_align_err) {
		sc->sc_align_err = IEE_SWAP32(SC_SCB(sc)->scb_align_err);
		printf("%s: iee_intr: align_err=%d\n", device_xname(sc->sc_dev),
		    sc->sc_align_err);
	}
	if (IEE_SWAP32(SC_SCB(sc)->scb_resource_err) != sc->sc_resource_err) {
		sc->sc_resource_err = IEE_SWAP32(SC_SCB(sc)->scb_resource_err);
		printf("%s: iee_intr: resource_err=%d\n",
		    device_xname(sc->sc_dev), sc->sc_resource_err);
	}
	if (IEE_SWAP32(SC_SCB(sc)->scb_overrun_err) != sc->sc_overrun_err) {
		sc->sc_overrun_err = IEE_SWAP32(SC_SCB(sc)->scb_overrun_err);
		printf("%s: iee_intr: overrun_err=%d\n",
		    device_xname(sc->sc_dev), sc->sc_overrun_err);
	}
	if (IEE_SWAP32(SC_SCB(sc)->scb_rcvcdt_err) != sc->sc_rcvcdt_err) {
		sc->sc_rcvcdt_err = IEE_SWAP32(SC_SCB(sc)->scb_rcvcdt_err);
		printf("%s: iee_intr: rcvcdt_err=%d\n",
		    device_xname(sc->sc_dev), sc->sc_rcvcdt_err);
	}
	if (IEE_SWAP32(SC_SCB(sc)->scb_short_fr_err) != sc->sc_short_fr_err) {
		sc->sc_short_fr_err = IEE_SWAP32(SC_SCB(sc)->scb_short_fr_err);
		printf("%s: iee_intr: short_fr_err=%d\n",
		    device_xname(sc->sc_dev), sc->sc_short_fr_err);
	}
	IEE_SCBSYNC(sc, BUS_DMASYNC_PREREAD);
	(sc->sc_iee_cmd)(sc, IEE_SCB_ACK);
	return 1;
}



/*
 * How Command Block List Processing is done.
 * 
 * A running CBL is never manipulated. If there is a CBL already running,
 * further CMDs are deferred until the current list is done. A new list is
 * setup when the old one has finished.
 * This eases programming. To manipulate a running CBL it is necessary to
 * suspend the Command Unit to avoid race conditions. After a suspend
 * is sent we have to wait for an interrupt that ACKs the suspend. Then
 * we can manipulate the CBL and resume operation. I am not sure that this
 * is more effective than the current, much simpler approach. => KISS
 * See i82596CA data sheet page 26.
 * 
 * A CBL is running or on the way to be set up when (sc->sc_next_cb != 0).
 * 
 * A CBL may consist of TX CMDs, and _only_ TX CMDs.
 * A TX CBL is running or on the way to be set up when
 * ((sc->sc_next_cb != 0) && (sc->sc_next_tbd != 0)).
 * 
 * A CBL may consist of other non-TX CMDs like IAS or CONF, and _only_
 * non-TX CMDs.
 * 
 * This comes mostly through the way how an Ethernet driver works and
 * because running CBLs are not manipulated when they are on the way. If
 * if_start() is called there will be TX CMDs enqueued so we have a running
 * CBL and other CMDs from e.g. if_ioctl() will be deferred and vice versa.
 * 
 * The Multicast Setup Command is special. A MCS needs more space than
 * a single CB has. Actual space requirement depends on the length of the
 * multicast list. So we always defer MCS until other CBLs are finished,
 * then we setup a CONF CMD in the first CB. The CONF CMD is needed to
 * turn ALLMULTI on the hardware on or off. The MCS is the 2nd CB and may
 * use all the remaining space in the CBL and the Transmit Buffer Descriptor
 * List. (Therefore CBL and TBDL must be continuous in physical and virtual
 * memory. This is guaranteed through the definitions of the list offsets
 * in i82596reg.h and because it is only a single DMA segment used for all
 * lists.) When ALLMULTI is enabled via the CONF CMD, the MCS is run with
 * a multicast list length of 0, thus disabling the multicast filter.
 * A deferred MCS is signaled via ((sc->sc_flags & IEE_WANT_MCAST) != 0)
 */
void
iee_cb_setup(struct iee_softc *sc, uint32_t cmd)
{
	struct iee_cb *cb = SC_CB(sc, sc->sc_next_cb);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multistep step;
	struct ether_multi *enm;

	memset(cb, 0, sc->sc_cb_sz);
	cb->cb_cmd = cmd;
	switch (cmd & IEE_CB_CMD) {
	case IEE_CB_CMD_NOP:	/* NOP CMD */
		break;
	case IEE_CB_CMD_IAS:	/* Individual Address Setup */
		memcpy(__UNVOLATILE(cb->cb_ind_addr), CLLADDR(ifp->if_sadl),
		    ETHER_ADDR_LEN);
		break;
	case IEE_CB_CMD_CONF:	/* Configure */
		memcpy(__UNVOLATILE(cb->cb_cf), sc->sc_cf, sc->sc_cf[0]
		    & IEE_CF_0_CNT_M);
		break;
	case IEE_CB_CMD_MCS:	/* Multicast Setup */
		if (sc->sc_next_cb != 0) {
			sc->sc_flags |= IEE_WANT_MCAST;
			return;
		}
		sc->sc_flags &= ~IEE_WANT_MCAST;
		if ((sc->sc_cf[8] & IEE_CF_8_PRM) != 0) {
			/* Need no multicast filter in promisc mode. */
			iee_cb_setup(sc, IEE_CB_CMD_CONF | IEE_CB_S | IEE_CB_EL
			    | IEE_CB_I);
			return;
		}
		/* Leave room for a CONF CMD to en/dis-able ALLMULTI mode */
		cb = SC_CB(sc, sc->sc_next_cb + 1);
		cb->cb_cmd = cmd;
		cb->cb_mcast.mc_size = 0;
		ETHER_FIRST_MULTI(step, &sc->sc_ethercom, enm);
		while (enm != NULL) {
			if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
			    ETHER_ADDR_LEN) != 0 || cb->cb_mcast.mc_size
			    * ETHER_ADDR_LEN + 2 * sc->sc_cb_sz >
			    sc->sc_cb_sz * IEE_NCB +
			    sc->sc_tbd_sz * IEE_NTBD * IEE_NCB) {
				cb->cb_mcast.mc_size = 0;
				break;
			}
			memcpy(__UNVOLATILE(&cb->cb_mcast.mc_addrs[
			    cb->cb_mcast.mc_size]),
			    enm->enm_addrlo, ETHER_ADDR_LEN);
			ETHER_NEXT_MULTI(step, enm);
			cb->cb_mcast.mc_size += ETHER_ADDR_LEN;
		}
		if (cb->cb_mcast.mc_size == 0) {
			/* Can't do exact mcast filtering, do ALLMULTI mode. */
			ifp->if_flags |= IFF_ALLMULTI;
			sc->sc_cf[11] &= ~IEE_CF_11_MCALL;
		} else {
			/* disable ALLMULTI and load mcast list */
			ifp->if_flags &= ~IFF_ALLMULTI;
			sc->sc_cf[11] |= IEE_CF_11_MCALL;
			/* Mcast setup may need more than sc->sc_cb_sz bytes. */
			bus_dmamap_sync(sc->sc_dmat, sc->sc_shmem_map,
			    sc->sc_cb_off,
			    sc->sc_cb_sz * IEE_NCB +
			    sc->sc_tbd_sz * IEE_NTBD * IEE_NCB,
			    BUS_DMASYNC_PREWRITE);
		}
		iee_cb_setup(sc, IEE_CB_CMD_CONF);
		break;
	case IEE_CB_CMD_TR:	/* Transmit */
		cb->cb_transmit.tx_tbd_addr =
		    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_tbd_off
		    + sc->sc_tbd_sz * sc->sc_next_tbd));
		cb->cb_cmd |= IEE_CB_SF; /* Always use Flexible Mode. */
		break;
	case IEE_CB_CMD_TDR:	/* Time Domain Reflectometry */
		break;
	case IEE_CB_CMD_DUMP:	/* Dump */
		break;
	case IEE_CB_CMD_DIAG:	/* Diagnose */
		break;
	default:
		/* can't happen */
		break;
	}
	cb->cb_link_addr = IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_cb_off +
	    sc->sc_cb_sz * (sc->sc_next_cb + 1)));
	IEE_CBSYNC(sc, sc->sc_next_cb,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_next_cb++;
	ifp->if_timer = 5;
}



void
iee_attach(struct iee_softc *sc, uint8_t *eth_addr, int *media, int nmedia,
    int defmedia)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int n;

	KASSERT(sc->sc_cl_align > 0 && powerof2(sc->sc_cl_align));

	/*
	 * Calculate DMA descriptor offsets and sizes in shmem
	 * which should be cache line aligned.
	 */
	sc->sc_scp_off  = 0;
	sc->sc_scp_sz   = roundup2(sizeof(struct iee_scp), sc->sc_cl_align);
	sc->sc_iscp_off = sc->sc_scp_sz;
	sc->sc_iscp_sz  = roundup2(sizeof(struct iee_iscp), sc->sc_cl_align);
	sc->sc_scb_off  = sc->sc_iscp_off + sc->sc_iscp_sz;
	sc->sc_scb_sz   = roundup2(sizeof(struct iee_scb), sc->sc_cl_align);
	sc->sc_rfd_off  = sc->sc_scb_off + sc->sc_scb_sz;
	sc->sc_rfd_sz   = roundup2(sizeof(struct iee_rfd), sc->sc_cl_align);
	sc->sc_rbd_off  = sc->sc_rfd_off + sc->sc_rfd_sz * IEE_NRFD;
	sc->sc_rbd_sz   = roundup2(sizeof(struct iee_rbd), sc->sc_cl_align);
	sc->sc_cb_off   = sc->sc_rbd_off + sc->sc_rbd_sz * IEE_NRFD;
	sc->sc_cb_sz    = roundup2(sizeof(struct iee_cb), sc->sc_cl_align);
	sc->sc_tbd_off  = sc->sc_cb_off + sc->sc_cb_sz * IEE_NCB;
	sc->sc_tbd_sz   = roundup2(sizeof(struct iee_tbd), sc->sc_cl_align);
	sc->sc_shmem_sz = sc->sc_tbd_off + sc->sc_tbd_sz * IEE_NTBD * IEE_NCB;

	/* allocate memory for shared DMA descriptors */
	if (bus_dmamem_alloc(sc->sc_dmat, sc->sc_shmem_sz, PAGE_SIZE, 0,
	    &sc->sc_dma_segs, 1, &sc->sc_dma_rsegs, BUS_DMA_NOWAIT) != 0) {
		aprint_error(": can't allocate %d bytes of DMA memory\n",
		    sc->sc_shmem_sz);
		return;
	}
	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_dma_segs, sc->sc_dma_rsegs,
	    sc->sc_shmem_sz, (void **)&sc->sc_shmem_addr,
	    BUS_DMA_COHERENT | BUS_DMA_NOWAIT) != 0) {
		aprint_error(": can't map DMA memory\n");
		bus_dmamem_free(sc->sc_dmat, &sc->sc_dma_segs,
		    sc->sc_dma_rsegs);
		return;
	}
	if (bus_dmamap_create(sc->sc_dmat, sc->sc_shmem_sz, sc->sc_dma_rsegs,
	    sc->sc_shmem_sz, 0, BUS_DMA_NOWAIT, &sc->sc_shmem_map) != 0) {
		aprint_error(": can't create DMA map\n");
		bus_dmamem_unmap(sc->sc_dmat, sc->sc_shmem_addr,
		    sc->sc_shmem_sz);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_dma_segs,
		    sc->sc_dma_rsegs);
		return;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_shmem_map, sc->sc_shmem_addr,
	    sc->sc_shmem_sz, NULL, BUS_DMA_NOWAIT) != 0) {
		aprint_error(": can't load DMA map\n");
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_shmem_map);
		bus_dmamem_unmap(sc->sc_dmat, sc->sc_shmem_addr,
		    sc->sc_shmem_sz);
		bus_dmamem_free(sc->sc_dmat, &sc->sc_dma_segs,
		    sc->sc_dma_rsegs);
		return;
	}
	memset(sc->sc_shmem_addr, 0, sc->sc_shmem_sz);

	/* Set pointer to Intermediate System Configuration Pointer. */
	/* Phys. addr. in big endian order. (Big endian as defined by Intel.) */
	SC_SCP(sc)->scp_iscp_addr = IEE_SWAP32(IEE_PHYS_SHMEM(sc->sc_iscp_off));
	SC_SCP(sc)->scp_sysbus = sc->sc_sysbus;
	/* Set pointer to System Control Block. */
	/* Phys. addr. in big endian order. (Big endian as defined by Intel.) */
	SC_ISCP(sc)->iscp_scb_addr = IEE_SWAP32(IEE_PHYS_SHMEM(sc->sc_scb_off));
	/* Set pointer to Receive Frame Area. (physical address) */
	SC_SCB(sc)->scb_rfa_addr = IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_rfd_off));
	/* Set pointer to Command Block. (physical address) */
	SC_SCB(sc)->scb_cmd_blk_addr =
	    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_cb_off));

	bus_dmamap_sync(sc->sc_dmat, sc->sc_shmem_map, 0, sc->sc_shmem_sz,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	ifmedia_init(&sc->sc_ifmedia, 0, iee_mediachange, iee_mediastatus);
	if (media != NULL) {
		for (n = 0 ; n < nmedia ; n++)
			ifmedia_add(&sc->sc_ifmedia, media[n], 0, NULL);
		ifmedia_set(&sc->sc_ifmedia, defmedia);
	} else {
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_ifmedia, IFM_ETHER | IFM_NONE);
	}

	ifp->if_softc = sc;
	strcpy(ifp->if_xname, device_xname(sc->sc_dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = iee_start;	/* initiate output routine */
	ifp->if_ioctl = iee_ioctl;	/* ioctl routine */
	ifp->if_init = iee_init;	/* init routine */
	ifp->if_stop = iee_stop;	/* stop routine */
	ifp->if_watchdog = iee_watchdog;	/* timer routine */
	IFQ_SET_READY(&ifp->if_snd);
	/* iee supports IEEE 802.1Q Virtual LANs, see vlan(4). */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	if_attach(ifp);
	ether_ifattach(ifp, eth_addr);

	aprint_normal(": Intel 82596%s address %s\n",
	    i82596_typenames[sc->sc_type], ether_sprintf(eth_addr));

	for (n = 0 ; n < IEE_NCB ; n++)
		sc->sc_tx_map[n] = NULL;
	for (n = 0 ; n < IEE_NRFD ; n++) {
		sc->sc_rx_mbuf[n] = NULL;
		sc->sc_rx_map[n] = NULL;
	}
	sc->sc_tx_timeout = 0;
	sc->sc_setup_timeout = 0;
	(sc->sc_iee_reset)(sc);
}



void
iee_detach(struct iee_softc *sc, int flags)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	if ((ifp->if_flags & IFF_RUNNING) != 0)
		iee_stop(ifp, 1);
	ether_ifdetach(ifp);
	if_detach(ifp);
	bus_dmamap_unload(sc->sc_dmat, sc->sc_shmem_map);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_shmem_map);
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_shmem_addr, sc->sc_shmem_sz);
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dma_segs, sc->sc_dma_rsegs);
}



/* media change and status callback */
int
iee_mediachange(struct ifnet *ifp)
{
	struct iee_softc *sc = ifp->if_softc;

	if (sc->sc_mediachange != NULL)
		return (sc->sc_mediachange)(ifp);
	return 0;
}



void
iee_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmreq)
{
	struct iee_softc *sc = ifp->if_softc;

	if (sc->sc_mediastatus != NULL)
		(sc->sc_mediastatus)(ifp, ifmreq);
}



/* initiate output routine */
void
iee_start(struct ifnet *ifp)
{
	struct iee_softc *sc = ifp->if_softc;
	struct mbuf *m = NULL;
	struct iee_tbd *tbd;
	int t;
	int n;

	if (sc->sc_next_cb != 0)
		/* There is already a CMD running. Defer packet enqueuing. */
		return;
	for (t = 0 ; t < IEE_NCB ; t++) {
		IFQ_DEQUEUE(&ifp->if_snd, sc->sc_tx_mbuf[t]);
		if (sc->sc_tx_mbuf[t] == NULL)
			break;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, sc->sc_tx_map[t],
		    sc->sc_tx_mbuf[t], BUS_DMA_WRITE | BUS_DMA_NOWAIT) != 0) {
			/*
			 * The packet needs more TBD than we support.
			 * Copy the packet into a mbuf cluster to get it out.
			 */
			printf("%s: iee_start: failed to load DMA map\n",
			    device_xname(sc->sc_dev));
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: iee_start: can't allocate mbuf\n",
				    device_xname(sc->sc_dev));
				m_freem(sc->sc_tx_mbuf[t]);
				t--;
				continue;
			}
			MCLAIM(m, &sc->sc_ethercom.ec_rx_mowner);
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				printf("%s: iee_start: can't allocate mbuf "
				    "cluster\n", device_xname(sc->sc_dev));
				m_freem(sc->sc_tx_mbuf[t]);
				m_freem(m);
				t--;
				continue;
			}
			m_copydata(sc->sc_tx_mbuf[t], 0,
			    sc->sc_tx_mbuf[t]->m_pkthdr.len, mtod(m, void *));
			m->m_pkthdr.len = sc->sc_tx_mbuf[t]->m_pkthdr.len;
			m->m_len = sc->sc_tx_mbuf[t]->m_pkthdr.len;
			m_freem(sc->sc_tx_mbuf[t]);
			sc->sc_tx_mbuf[t] = m;
			if (bus_dmamap_load_mbuf(sc->sc_dmat, sc->sc_tx_map[t],
		    	    m, BUS_DMA_WRITE | BUS_DMA_NOWAIT) != 0) {
				printf("%s: iee_start: can't load TX DMA map\n",
				    device_xname(sc->sc_dev));
				m_freem(sc->sc_tx_mbuf[t]);
				t--;
				continue;
			}
		}
		for (n = 0 ; n < sc->sc_tx_map[t]->dm_nsegs ; n++) {
			tbd = SC_TBD(sc, sc->sc_next_tbd + n);
			tbd->tbd_tb_addr =
			    IEE_SWAPA32(sc->sc_tx_map[t]->dm_segs[n].ds_addr);
			tbd->tbd_size =
			    sc->sc_tx_map[t]->dm_segs[n].ds_len;
			tbd->tbd_link_addr =
			    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_tbd_off +
			    sc->sc_tbd_sz * (sc->sc_next_tbd + n + 1)));
		}
		SC_TBD(sc, sc->sc_next_tbd + n - 1)->tbd_size |= IEE_CB_EL;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_shmem_map,
		    sc->sc_tbd_off + sc->sc_next_tbd * sc->sc_tbd_sz,
		    sc->sc_tbd_sz * sc->sc_tx_map[t]->dm_nsegs,
		    BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_tx_map[t], 0,
		    sc->sc_tx_map[t]->dm_mapsize, BUS_DMASYNC_PREWRITE);
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			iee_cb_setup(sc, IEE_CB_CMD_TR | IEE_CB_S | IEE_CB_EL
			    | IEE_CB_I);
		else
			iee_cb_setup(sc, IEE_CB_CMD_TR);
		sc->sc_next_tbd += n;
		/* Pass packet to bpf if someone listens. */
		bpf_mtap(ifp, sc->sc_tx_mbuf[t]);
	}
	if (t == 0)
		/* No packets got set up for TX. */
		return;
	if (t == IEE_NCB)
		ifp->if_flags |= IFF_OACTIVE;
	(sc->sc_iee_cmd)(sc, IEE_SCB_CUC_EXE);
}



/* ioctl routine */
int
iee_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct iee_softc *sc = ifp->if_softc;
	int s;
	int err;

	s = splnet();
	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		err = ifmedia_ioctl(ifp, (struct ifreq *) data,
		    &sc->sc_ifmedia, cmd);
		break;

	default:
		err = ether_ioctl(ifp, cmd, data);
		if (err == ENETRESET) {
			/*
			 * Multicast list as changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING) {
				iee_cb_setup(sc, IEE_CB_CMD_MCS | IEE_CB_S |
				    IEE_CB_EL | IEE_CB_I);
				if ((sc->sc_flags & IEE_WANT_MCAST) == 0)
					(*sc->sc_iee_cmd)(sc, IEE_SCB_CUC_EXE);
			}
			err = 0;
		}
		break;
	}
	splx(s);
	return err;
}



/* init routine */
int
iee_init(struct ifnet *ifp)
{
	struct iee_softc *sc = ifp->if_softc;
	int r;
	int t;
	int n;
	int err;

	sc->sc_next_cb = 0;
	sc->sc_next_tbd = 0;
	sc->sc_flags &= ~IEE_WANT_MCAST;
	sc->sc_rx_done = 0;
	SC_SCB(sc)->scb_crc_err = 0;
	SC_SCB(sc)->scb_align_err = 0;
	SC_SCB(sc)->scb_resource_err = 0;
	SC_SCB(sc)->scb_overrun_err = 0;
	SC_SCB(sc)->scb_rcvcdt_err = 0;
	SC_SCB(sc)->scb_short_fr_err = 0;
	sc->sc_crc_err = 0;
	sc->sc_align_err = 0;
	sc->sc_resource_err = 0;
	sc->sc_overrun_err = 0;
	sc->sc_rcvcdt_err = 0;
	sc->sc_short_fr_err = 0;
	sc->sc_tx_col = 0;
	sc->sc_rx_err = 0;
	sc->sc_cmd_err = 0;
	/* Create Transmit DMA maps. */
	for (t = 0 ; t < IEE_NCB ; t++) {
		if (sc->sc_tx_map[t] == NULL && bus_dmamap_create(sc->sc_dmat,
		    MCLBYTES, IEE_NTBD, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &sc->sc_tx_map[t]) != 0) {
			printf("%s: iee_init: can't create TX DMA map\n",
			    device_xname(sc->sc_dev));
			for (n = 0 ; n < t ; n++)
				bus_dmamap_destroy(sc->sc_dmat,
				    sc->sc_tx_map[n]);
			return ENOBUFS;
		}
	}
	/* Initialize Receive Frame and Receive Buffer Descriptors */
	err = 0;
	memset(SC_RFD(sc, 0), 0, sc->sc_rfd_sz * IEE_NRFD);
	memset(SC_RBD(sc, 0), 0, sc->sc_rbd_sz * IEE_NRFD);
	for (r = 0 ; r < IEE_NRFD ; r++) {
		SC_RFD(sc, r)->rfd_cmd = IEE_RFD_SF;
		SC_RFD(sc, r)->rfd_link_addr =
		    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_rfd_off
		    + sc->sc_rfd_sz * ((r + 1) % IEE_NRFD)));

		SC_RBD(sc, r)->rbd_next_rbd =
		    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_rbd_off
		    + sc->sc_rbd_sz * ((r + 1) % IEE_NRFD)));
		if (sc->sc_rx_mbuf[r] == NULL) {
			MGETHDR(sc->sc_rx_mbuf[r], M_DONTWAIT, MT_DATA);
			if (sc->sc_rx_mbuf[r] == NULL) {
				printf("%s: iee_init: can't allocate mbuf\n",
				    device_xname(sc->sc_dev));
				err = 1;
				break;
			}
			MCLAIM(sc->sc_rx_mbuf[r],&sc->sc_ethercom.ec_rx_mowner);
			MCLGET(sc->sc_rx_mbuf[r], M_DONTWAIT);
			if ((sc->sc_rx_mbuf[r]->m_flags & M_EXT) == 0) {
				printf("%s: iee_init: can't allocate mbuf"
				    " cluster\n", device_xname(sc->sc_dev));
				m_freem(sc->sc_rx_mbuf[r]);
				err = 1;
				break;
			}
			sc->sc_rx_mbuf[r]->m_len =
			    sc->sc_rx_mbuf[r]->m_pkthdr.len = MCLBYTES - 2;
			sc->sc_rx_mbuf[r]->m_data += 2;
		}
		if (sc->sc_rx_map[r] == NULL && bus_dmamap_create(sc->sc_dmat,
		    MCLBYTES, 1, MCLBYTES , 0, BUS_DMA_NOWAIT,
		    &sc->sc_rx_map[r]) != 0) {
				printf("%s: iee_init: can't create RX "
				    "DMA map\n", device_xname(sc->sc_dev));
				m_freem(sc->sc_rx_mbuf[r]);
				err = 1;
				break;
			}
		if (bus_dmamap_load_mbuf(sc->sc_dmat, sc->sc_rx_map[r],
		    sc->sc_rx_mbuf[r], BUS_DMA_READ | BUS_DMA_NOWAIT) != 0) {
			printf("%s: iee_init: can't load RX DMA map\n",
			    device_xname(sc->sc_dev));
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_rx_map[r]);
			m_freem(sc->sc_rx_mbuf[r]);
			err = 1;
			break;
		}
		bus_dmamap_sync(sc->sc_dmat, sc->sc_rx_map[r], 0,
		    sc->sc_rx_map[r]->dm_mapsize, BUS_DMASYNC_PREREAD);
		SC_RBD(sc, r)->rbd_size = sc->sc_rx_map[r]->dm_segs[0].ds_len;
		SC_RBD(sc, r)->rbd_rb_addr =
		    IEE_SWAPA32(sc->sc_rx_map[r]->dm_segs[0].ds_addr);
	}
	SC_RFD(sc, 0)->rfd_rbd_addr =
	    IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_rbd_off));
	if (err != 0) {
		for (n = 0 ; n < r; n++) {
			m_freem(sc->sc_rx_mbuf[n]);
			sc->sc_rx_mbuf[n] = NULL;
			bus_dmamap_unload(sc->sc_dmat, sc->sc_rx_map[n]);
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_rx_map[n]);
			sc->sc_rx_map[n] = NULL;
		}
		for (n = 0 ; n < t ; n++) {
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_tx_map[n]);
			sc->sc_tx_map[n] = NULL;
		}
		return ENOBUFS;
	}

	(sc->sc_iee_reset)(sc);
	iee_cb_setup(sc, IEE_CB_CMD_IAS);
	sc->sc_cf[0] = IEE_CF_0_DEF | IEE_CF_0_PREF;
	sc->sc_cf[1] = IEE_CF_1_DEF;
	sc->sc_cf[2] = IEE_CF_2_DEF;
	sc->sc_cf[3] = IEE_CF_3_ADDRLEN_DEF | IEE_CF_3_NSAI
	    | IEE_CF_3_PREAMLEN_DEF;
	sc->sc_cf[4] = IEE_CF_4_DEF;
	sc->sc_cf[5] = IEE_CF_5_DEF;
	sc->sc_cf[6] = IEE_CF_6_DEF;
	sc->sc_cf[7] = IEE_CF_7_DEF;
	sc->sc_cf[8] = IEE_CF_8_DEF;
	sc->sc_cf[9] = IEE_CF_9_DEF;
	sc->sc_cf[10] = IEE_CF_10_DEF;
	sc->sc_cf[11] = IEE_CF_11_DEF & ~IEE_CF_11_LNGFLD;
	sc->sc_cf[12] = IEE_CF_12_DEF;
	sc->sc_cf[13] = IEE_CF_13_DEF;
	iee_cb_setup(sc, IEE_CB_CMD_CONF | IEE_CB_S | IEE_CB_EL);
	SC_SCB(sc)->scb_rfa_addr = IEE_SWAPA32(IEE_PHYS_SHMEM(sc->sc_rfd_off));
	bus_dmamap_sync(sc->sc_dmat, sc->sc_shmem_map, 0, sc->sc_shmem_sz,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	(sc->sc_iee_cmd)(sc, IEE_SCB_CUC_EXE | IEE_SCB_RUC_ST);
	/* Issue a Channel Attention to ACK interrupts we may have caused. */
	(sc->sc_iee_cmd)(sc, IEE_SCB_ACK);

	/* Mark the interface as running and ready to RX/TX packets. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	return 0;
}



/* stop routine */
void
iee_stop(struct ifnet *ifp, int disable)
{
	struct iee_softc *sc = ifp->if_softc;
	int n;

	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_flags |= IFF_OACTIVE;
	ifp->if_timer = 0;
	/* Reset the chip to get it quiet. */
	(sc->sc_iee_reset)(ifp->if_softc);
	/* Issue a Channel Attention to ACK interrupts we may have caused. */
	(sc->sc_iee_cmd)(ifp->if_softc, IEE_SCB_ACK);
	/* Release any dynamically allocated resources. */
	for (n = 0 ; n < IEE_NCB ; n++) {
		if (sc->sc_tx_map[n] != NULL)
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_tx_map[n]);
		sc->sc_tx_map[n] = NULL;
	}
	for (n = 0 ; n < IEE_NRFD ; n++) {
		if (sc->sc_rx_mbuf[n] != NULL)
			m_freem(sc->sc_rx_mbuf[n]);
		sc->sc_rx_mbuf[n] = NULL;
		if (sc->sc_rx_map[n] != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->sc_rx_map[n]);
			bus_dmamap_destroy(sc->sc_dmat, sc->sc_rx_map[n]);
		}
		sc->sc_rx_map[n] = NULL;
	}
}



/* timer routine */
void
iee_watchdog(struct ifnet *ifp)
{
	struct iee_softc *sc = ifp->if_softc;

	(sc->sc_iee_reset)(sc);
	if (sc->sc_next_tbd != 0)
		printf("%s: iee_watchdog: transmit timeout %d\n",
		    device_xname(sc->sc_dev), ++sc->sc_tx_timeout);
	else
		printf("%s: iee_watchdog: setup timeout %d\n",
		    device_xname(sc->sc_dev), ++sc->sc_setup_timeout);
	iee_init(ifp);
}
