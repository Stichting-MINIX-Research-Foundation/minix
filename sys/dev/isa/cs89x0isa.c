/* $NetBSD: cs89x0isa.c,v 1.17 2015/04/13 16:33:24 riastradh Exp $ */

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

/* isa DMA routines for cs89x0 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cs89x0isa.c,v 1.17 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/cs89x0reg.h>
#include <dev/ic/cs89x0var.h>
#include <dev/isa/cs89x0isavar.h>

#define DMA_STATUS_BITS 0x0007	/* bit masks for checking DMA status */
#define DMA_STATUS_OK 0x0004

void
cs_isa_dma_attach(struct cs_softc *sc)
{
	struct cs_softc_isa *isc = (struct cs_softc_isa *)sc;

	if (isc->sc_drq == ISA_UNKNOWN_DRQ)
		printf("%s: DMA channel unspecified, not using DMA\n",
		    device_xname(sc->sc_dev));
	else if (isc->sc_drq < 5 || isc->sc_drq > 7)
		printf("%s: invalid DMA channel, not using DMA\n",
		    device_xname(sc->sc_dev));
	else {
		bus_size_t maxsize;
		bus_addr_t dma_addr;

		maxsize = isa_dmamaxsize(isc->sc_ic, isc->sc_drq);
		if (maxsize < CS8900_DMASIZE) {
			printf("%s: max DMA size %lu is"
			    " less than required %d\n",
			    device_xname(sc->sc_dev), (u_long)maxsize,
			    CS8900_DMASIZE);
			goto after_dma_block;
		}

		if (isa_drq_alloc(isc->sc_ic, isc->sc_drq) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to reserve drq %d\n",
			    isc->sc_drq);
			goto after_dma_block;
		}

		if (isa_dmamap_create(isc->sc_ic, isc->sc_drq,
		    CS8900_DMASIZE, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create ISA DMA map\n");
			goto after_dma_block;
		}
		if (isa_dmamem_alloc(isc->sc_ic, isc->sc_drq,
		    CS8900_DMASIZE, &dma_addr, BUS_DMA_NOWAIT) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to allocate DMA buffer\n");
			goto after_dma_block;
		}
		if (isa_dmamem_map(isc->sc_ic, isc->sc_drq, dma_addr,
		    CS8900_DMASIZE, (void **)&isc->sc_dmabase,
		       BUS_DMA_NOWAIT | BUS_DMA_COHERENT /* XXX */ ) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to map DMA buffer\n");
			isa_dmamem_free(isc->sc_ic, isc->sc_drq, dma_addr,
			    CS8900_DMASIZE);
			goto after_dma_block;
		}

		isc->sc_dmasize = CS8900_DMASIZE;
		sc->sc_cfgflags |= CFGFLG_DMA_MODE;
		isc->sc_dmaaddr = dma_addr;
after_dma_block:
		;
	}
}

void cs_isa_dma_chipinit(struct cs_softc *sc)
{
	struct cs_softc_isa *isc = (struct cs_softc_isa *)sc;

	if (sc->sc_cfgflags & CFGFLG_DMA_MODE) {
		/*
		 * First we program the DMA controller and ensure the memory
		 * buffer is valid. If it isn't then we just go on without
		 * DMA.
		 */
		if (isa_dmastart(isc->sc_ic, isc->sc_drq, isc->sc_dmabase,
		    isc->sc_dmasize, NULL, DMAMODE_READ | DMAMODE_LOOPDEMAND,
		    BUS_DMA_NOWAIT)) {
			/* XXX XXX XXX */
			panic("%s: unable to start DMA",
			    device_xname(sc->sc_dev));
		}
		isc->sc_dmacur = isc->sc_dmabase;

		/* interrupt when a DMA'd frame is received */
		CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG,
		    RX_CFG_ALL_IE | RX_CFG_RX_DMA_ONLY);

		/*
		 * set the DMA burst bit so we don't tie up the bus for too
		 * long.
		 */
		if (isc->sc_dmasize == 16384) {
			CS_WRITE_PACKET_PAGE(sc, PKTPG_BUS_CTL,
			    ((CS_READ_PACKET_PAGE(sc, PKTPG_BUS_CTL) &
			     ~BUS_CTL_DMA_SIZE) | BUS_CTL_DMA_BURST));
		} else { /* use 64K */
			CS_WRITE_PACKET_PAGE(sc, PKTPG_BUS_CTL,
			    CS_READ_PACKET_PAGE(sc, PKTPG_BUS_CTL) |
			     BUS_CTL_DMA_SIZE | BUS_CTL_DMA_BURST);
		}

		CS_WRITE_PACKET_PAGE(sc, PKTPG_DMA_CHANNEL, isc->sc_drq - 5);
	}
}

void cs_process_rx_dma(struct cs_softc *sc)
{
	struct cs_softc_isa *isc = (struct cs_softc_isa *)sc;
	struct ifnet *ifp;
	u_int16_t num_dma_frames;
	u_int16_t pkt_length;
	u_int16_t status;
	u_int to_copy;
	char *dma_mem_ptr;
	struct mbuf *m;
	u_char *pBuff;
	int pad;

	/* initialise the pointers */
	ifp = &sc->sc_ethercom.ec_if;

	/* Read the number of frames DMAed. */
	num_dma_frames = CS_READ_PACKET_PAGE(sc, PKTPG_DMA_FRAME_COUNT);
	num_dma_frames &= (u_int16_t) (0x0fff);

	/*
	 * Loop till number of DMA frames ready to read is zero. After
	 * reading the frame out of memory we must check if any have been
	 * received while we were processing
	 */
	while (num_dma_frames != 0) {
		dma_mem_ptr = isc->sc_dmacur;

		/*
		 * process all of the DMA frames in memory
		 *
		 * This loop relies on the dma_mem_ptr variable being set to the
		 * next frames start address.
		 */
		for (; num_dma_frames > 0; num_dma_frames--) {

			/*
			 * Get the length and status of the packet. Only the
			 * status is guaranteed to be at dma_mem_ptr, ie need
			 * to check for wraparound before reading the length
			 */
			status = *((u_int16_t *) dma_mem_ptr);
			dma_mem_ptr += 2;
			if (dma_mem_ptr > (isc->sc_dmabase + isc->sc_dmasize)) {
				dma_mem_ptr = isc->sc_dmabase;
			}
			pkt_length = *((u_int16_t *) dma_mem_ptr);
			dma_mem_ptr += 2;

			/* Do some sanity checks on the length and status. */
			if ((pkt_length > ETHER_MAX_LEN) ||
			    ((status & DMA_STATUS_BITS) != DMA_STATUS_OK)) {
				/*
				 * the SCO driver kills the adapter in this
				 * situation
				 */
				/*
				 * should increment the error count and reset
				 * the DMA operation.
				 */
				printf("%s: cs_process_rx_dma: "
				    "DMA buffer out of sync about to reset\n",
				    device_xname(sc->sc_dev));
				ifp->if_ierrors++;

				/* skip the rest of the DMA buffer */
				isa_dmaabort(isc->sc_ic, isc->sc_drq);

				/* now reset the chip and reinitialise */
				cs_init(&sc->sc_ethercom.ec_if);
				return;
			}
			/* Check the status of the received packet. */
			if (status & RX_EVENT_RX_OK) {
				/* get a new mbuf */
				MGETHDR(m, M_DONTWAIT, MT_DATA);
				if (m == 0) {
					printf("%s: cs_process_rx_dma: "
					    "unable to allocate mbuf\n",
					    device_xname(sc->sc_dev));
					ifp->if_ierrors++;
					/*
					 * couldn't allocate an mbuf so
					 * things are not good, may as well
					 * drop all the packets I think.
					 */
					CS_READ_PACKET_PAGE(sc,
					    PKTPG_DMA_FRAME_COUNT);

					/* now reset DMA operation */
					isa_dmaabort(isc->sc_ic, isc->sc_drq);

					/*
					 * now reset the chip and
					 * reinitialise
					 */
					cs_init(&sc->sc_ethercom.ec_if);
					return;
				}
				/*
				 * save processing by always using a mbuf
				 * cluster, guaranteed to fit packet
				 */
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					/* couldn't allocate an mbuf cluster */
					printf("%s: cs_process_rx_dma: "
					    "unable to allocate a cluster\n",
					    device_xname(sc->sc_dev));
					m_freem(m);

					/* skip the frame */
					CS_READ_PACKET_PAGE(sc, PKTPG_DMA_FRAME_COUNT);
					isa_dmaabort(isc->sc_ic, isc->sc_drq);

					/*
					 * now reset the chip and
					 * reinitialise
					 */
					cs_init(&sc->sc_ethercom.ec_if);
					return;
				}
				m->m_pkthdr.rcvif = ifp;
				m->m_pkthdr.len = pkt_length;
				m->m_len = pkt_length;

				/*
				 * align ip header on word boundary for
				 * ipintr
				 */
				pad = ALIGN(sizeof(struct ether_header)) -
				    sizeof(struct ether_header);
				m->m_data += pad;

				/*
				 * set up the buffer pointer to point to the
				 * data area
				 */
				pBuff = mtod(m, char *);

				/*
				 * Read the frame into free_pktbuf
				 * The buffer is circular buffer, either
				 * 16K or 64K in length.
				 *
				 * need to check where the end of the buffer
				 * is and go back to the start.
				 */
				if ((dma_mem_ptr + pkt_length) <
				    (isc->sc_dmabase + isc->sc_dmasize)) {
					/*
					 * No wrap around. Copy the frame
					 * header
					 */
					memcpy(pBuff, dma_mem_ptr, pkt_length);
					dma_mem_ptr += pkt_length;
				} else {
					to_copy = (u_int)
					    ((isc->sc_dmabase + isc->sc_dmasize) -
					    dma_mem_ptr);

					/* Copy the first half of the frame. */
					memcpy(pBuff, dma_mem_ptr, to_copy);
					pBuff += to_copy;

					/*
		                         * Rest of the frame is to be read
		                         * from the first byte of the DMA
		                         * memory.
		                         */
					/*
					 * Get the number of bytes leftout in
					 * the frame.
					 */
					to_copy = pkt_length - to_copy;

					dma_mem_ptr = isc->sc_dmabase;

					/* Copy rest of the frame. */
					memcpy(pBuff, dma_mem_ptr, to_copy);
					dma_mem_ptr += to_copy;
				}

				cs_ether_input(sc, m);
			}
			/* (status & RX_OK) */
			else {
				/* the frame was not received OK */
				/* Increment the input error count */
				ifp->if_ierrors++;

				/*
				 * If debugging is enabled then log error
				 * messages if we got any.
				 */
				if ((ifp->if_flags & IFF_DEBUG) &&
				    status != REG_NUM_RX_EVENT)
					cs_print_rx_errors(sc, status);
			}
			/*
			 * now update the current frame pointer. the
			 * dma_mem_ptr should point to the next packet to be
			 * received, without the alignment considerations.
			 *
			 * The cs8900 pads all frames to start at the next 32bit
			 * aligned addres. hence we need to pad our offset
			 * pointer.
			 */
			dma_mem_ptr += 3;
			dma_mem_ptr = (char *)
			    ((long) dma_mem_ptr & 0xfffffffc);
			if (dma_mem_ptr < (isc->sc_dmabase + isc->sc_dmasize)) {
				isc->sc_dmacur = dma_mem_ptr;
			} else {
				dma_mem_ptr = isc->sc_dmacur = isc->sc_dmabase;
			}
		} /* for all frames */
		/* Read the number of frames DMAed again. */
		num_dma_frames = CS_READ_PACKET_PAGE(sc, PKTPG_DMA_FRAME_COUNT);
		num_dma_frames &= (u_int16_t) (0x0fff);
	} /* while there are frames left */
}
