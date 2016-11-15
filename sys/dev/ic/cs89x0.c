/*	$NetBSD: cs89x0.c,v 1.35 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright (c) 2004 Christopher Gilbert
 * All rights reserved.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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

/*
**++
**  FACILITY
**
**     Device Driver for the Crystal CS8900 ISA Ethernet Controller.
**
**  ABSTRACT
**
**     This module provides standard ethernet access for INET protocols
**     only.
**
**  AUTHORS
**
**     Peter Dettori     SEA - Software Engineering.
**
**  CREATION DATE:
**
**     13-Feb-1997.
**
**  MODIFICATION HISTORY (Digital):
**
**     Revision 1.27  1998/01/20  17:59:40  cgd
**     update for moved headers
**
**     Revision 1.26  1998/01/12  19:29:36  cgd
**     use arm32/isa versions of isadma code.
**
**     Revision 1.25  1997/12/12  01:35:27  cgd
**     convert to use new arp code (from Brini)
**
**     Revision 1.24  1997/12/10  22:31:56  cgd
**     trim some fat (get rid of ability to explicitly supply enet addr, since
**     it was never used and added a bunch of code which really doesn't belong in
**     an enet driver), and clean up slightly.
**
**     Revision 1.23  1997/10/06  16:42:12  cgd
**     copyright notices
**
**     Revision 1.22  1997/06/20  19:38:01  chaiken
**     fixes some smartcard problems
**
**     Revision 1.21  1997/06/10 02:56:20  grohn
**     Added call to ledNetActive
**
**     Revision 1.20  1997/06/05 00:47:06  dettori
**     Changed cs_process_rx_dma to reset and re-initialise the
**     ethernet chip when DMA gets out of sync, or mbufs
**     can't be allocated.
**
**     Revision 1.19  1997/06/03 03:09:58  dettori
**     Turn off sc_txbusy flag when a transmit underrun
**     occurs.
**
**     Revision 1.18  1997/06/02 00:04:35  dettori
**     redefined the transmit table to get around the nfs_timer bug while we are
**     looking into it further.
**
**     Also changed interrupts from EDGE to LEVEL.
**
**     Revision 1.17  1997/05/27 23:31:01  dettori
**     Pulled out changes to DMAMODE defines.
**
**     Revision 1.16  1997/05/23 04:25:16  cgd
**     reformat log so it fits in 80cols
**
**     Revision 1.15  1997/05/23  04:22:18  cgd
**     remove the existing copyright notice (which Peter Dettori indicated
**     was incorrect, copied from an existing NetBSD file only so that the
**     file would have a copyright notice on it, and which he'd intended to
**     replace).  Replace it with a Digital copyright notice, cloned from
**     ess.c.  It's not really correct either (it indicates that the source
**     is Digital confidential!), but is better than nothing and more
**     correct than what was there before.
**
**     Revision 1.14  1997/05/23  04:12:50  cgd
**     use an adaptive transmit start algorithm: start by telling the chip
**     to start transmitting after 381 bytes have been fed to it.  if that
**     gets transmit underruns, ramp down to 1021 bytes then "whole
**     packet."  If successful at a given level for a while, try the next
**     more agressive level.  This code doesn't ever try to start
**     transmitting after 5 bytes have been sent to the NIC, because
**     that underruns rather regularly.  The back-off and ramp-up mechanism
**     could probably be tuned a little bit, but this works well enough to
**     support > 1MB/s transmit rates on a clear ethernet (which is about
**     20-25% better than the driver had previously been getting).
**
**     Revision 1.13  1997/05/22  21:06:54  cgd
**     redo cs_copy_tx_frame() from scratch.  It had a fatal flaw: it was blindly
**     casting from u_int8_t * to u_int16_t * without worrying about alignment
**     issues.  This would cause bogus data to be spit out for mbufs with
**     misaligned data.  For instance, it caused the following bits to appear
**     on the wire:
**     	... etBND 1S2C .SHA(K) R ...
**     	    11112222333344445555
**     which should have appeared as:
**     	... NetBSD 1.2C (SHARK) ...
**     	    11112222333344445555
**     Note the apparent 'rotate' of the bytes in the word, which was due to
**     incorrect unaligned accesses.  This data corruption was the cause of
**     incoming telnet/rlogin hangs.
**
**     Revision 1.12  1997/05/22  01:55:32  cgd
**     reformat log so it fits in 80cols
**
**     Revision 1.11  1997/05/22  01:50:27  cgd
**     * enable input packet address checking in the BPF+IFF_PROMISCUOUS case,
**       so packets aimed at other hosts don't get sent to ether_input().
**     * Add a static const char *rcsid initialized with an RCS Id tag, so that
**       you can easily tell (`strings`) what version of the driver is in your
**       kernel binary.
**     * get rid of ether_cmp().  It was inconsistently used, not necessarily
**       safe, and not really a performance win anyway.  (It was only used when
**       setting up the multicast logical address filter, which is an
**       infrequent event.  It could have been used in the IFF_PROMISCUOUS
**       address check above, but the benefit of it vs. memcmp would be
**       inconsequential, there.)  Use memcmp() instead.
**     * restructure csStartOuput to avoid the following bugs in the case where
**       txWait was being set:
**         * it would accidentally drop the outgoing packet if told to wait
**           but the outgoing packet queue was empty.
**         * it would bpf_mtap() the outgoing packet multiple times (once for
**           each time it was told to wait), and would also recalculate
**           the length of the outgoing packet each time it was told to
**           wait.
**       While there, rename txWait to txLoop, since with the new structure of
**       the code, the latter name makes more sense.
**
**     Revision 1.10  1997/05/19  02:03:20  cgd
**     Set RX_CTL in cs_set_ladr_filt(), rather than cs_initChip().  cs_initChip()
**     is the only caller of cs_set_ladr_filt(), and always calls it, so this
**     ends up being logically the same.  In cs_set_ladr_filt(), if IFF_PROMISC
**     is set, enable promiscuous mode (and set IFF_ALLMULTI), otherwise behave
**     as before.
**
**     Revision 1.9  1997/05/19  01:45:37  cgd
**     create a new function, cs_ether_input(), which does received-packet
**     BPF and ether_input processing.  This code used to be in three places,
**     and centralizing it will make adding IFF_PROMISC support much easier.
**     Also, in cs_copy_tx_frame(), put it some (currently disabled) code to
**     do copies with bus_space_write_region_2().  It's more correct, and
**     potentially more efficient.  That function needs to be gutted (to
**     deal properly with alignment issues, which it currently does wrong),
**     however, and the change doesn't gain much, so there's no point in
**     enabling it now.
**
**     Revision 1.8  1997/05/19  01:17:10  cgd
**     fix a comment re: the setting of the TxConfig register.  Clean up
**     interface counter maintenance (make it use standard idiom).
**
**--
*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cs89x0.c,v 1.35 2015/04/13 16:33:24 riastradh Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/cs89x0reg.h>
#include <dev/ic/cs89x0var.h>

#ifdef SHARK
#include <shark/shark/sequoia.h>
#endif

/*
 * MACRO DEFINITIONS
 */
#define CS_OUTPUT_LOOP_MAX 100	/* max times round notorious tx loop */

/*
 * FUNCTION PROTOTYPES
 */
static void	cs_get_default_media(struct cs_softc *);
static int	cs_get_params(struct cs_softc *);
static int	cs_get_enaddr(struct cs_softc *);
static int	cs_reset_chip(struct cs_softc *);
static void	cs_reset(struct cs_softc *);
static int	cs_ioctl(struct ifnet *, u_long, void *);
static void	cs_initChip(struct cs_softc *);
static void	cs_buffer_event(struct cs_softc *, u_int16_t);
static void	cs_transmit_event(struct cs_softc *, u_int16_t);
static void	cs_receive_event(struct cs_softc *, u_int16_t);
static void	cs_process_receive(struct cs_softc *);
static void	cs_process_rx_early(struct cs_softc *);
static void	cs_start_output(struct ifnet *);
static void	cs_copy_tx_frame(struct cs_softc *, struct mbuf *);
static void	cs_set_ladr_filt(struct cs_softc *, struct ethercom *);
static u_int16_t cs_hash_index(char *);
static void	cs_counter_event(struct cs_softc *, u_int16_t);

static int	cs_mediachange(struct ifnet *);
static void	cs_mediastatus(struct ifnet *, struct ifmediareq *);

static bool cs_shutdown(device_t, int);
static int cs_enable(struct cs_softc *);
static void cs_disable(struct cs_softc *);
static void cs_stop(struct ifnet *, int);
static int cs_scan_eeprom(struct cs_softc *);
static int cs_read_pktpg_from_eeprom(struct cs_softc *, int, u_int16_t *);


/*
 * GLOBAL DECLARATIONS
 */

/*
 * Xmit-early table.
 *
 * To get better performance, we tell the chip to start packet
 * transmission before the whole packet is copied to the chip.
 * However, this can fail under load.  When it fails, we back off
 * to a safer setting for a little while.
 *
 * txcmd is the value of txcmd used to indicate when to start transmission.
 * better is the next 'better' state in the table.
 * better_count is the number of output packets before transition to the
 *   better state.
 * worse is the next 'worse' state in the table.
 *
 * Transition to the next worse state happens automatically when a
 * transmittion underrun occurs.
 */
struct cs_xmit_early {
	u_int16_t       txcmd;
	int             better;
	int             better_count;
	int             worse;
} cs_xmit_early_table[3] = {
	{ TX_CMD_START_381,	0,	INT_MAX,	1, },
	{ TX_CMD_START_1021,	0,	50000,		2, },
	{ TX_CMD_START_ALL,	1,	5000,		2, },
};

int cs_default_media[] = {
	IFM_ETHER|IFM_10_2,
	IFM_ETHER|IFM_10_5,
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_T|IFM_FDX,
};
int cs_default_nmedia = sizeof(cs_default_media) / sizeof(cs_default_media[0]);

int
cs_attach(struct cs_softc *sc, u_int8_t *enaddr, int *media,
	  int nmedia, int defmedia)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	const char *chipname, *medname;
	u_int16_t reg;
	int i;

	/* Start out in IO mode */
	sc->sc_memorymode = FALSE;

	/* make sure we're right */
	for (i = 0; i < 10000; i++) {
		reg = CS_READ_PACKET_PAGE(sc, PKTPG_EISA_NUM);
		if (reg == EISA_NUM_CRYSTAL) {
			break;
		}
	}
	if (i == 10000) {
		aprint_error_dev(sc->sc_dev, "wrong id(0x%x)\n", reg);
		return 1; /* XXX should panic? */
	}

	reg = CS_READ_PACKET_PAGE(sc, PKTPG_PRODUCT_ID);
	sc->sc_prodid = reg & PROD_ID_MASK;
	sc->sc_prodrev = (reg & PROD_REV_MASK) >> 8;

	switch (sc->sc_prodid) {
	case PROD_ID_CS8900:
		chipname = "CS8900";
		break;
	case PROD_ID_CS8920:
		chipname = "CS8920";
		break;
	case PROD_ID_CS8920M:
		chipname = "CS8920M";
		break;
	default:
		panic("cs_attach: impossible");
	}

	/*
	 * the first thing to do is check that the mbuf cluster size is
	 * greater than the MTU for an ethernet frame. The code depends on
	 * this and to port this to a OS where this was not the case would
	 * not be straightforward.
	 *
	 * we need 1 byte spare because our
	 * packet read loop can overrun.
	 * and we may need pad bytes to align ip header.
	 */
	if (MCLBYTES < ETHER_MAX_LEN + 1 +
		ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header)) {
		printf("%s: MCLBYTES too small for Ethernet frame\n",
		    device_xname(sc->sc_dev));
		return 1;
	}

	/* Start out not transmitting */
	sc->sc_txbusy = FALSE;

	/* Set up early transmit threshhold */
	sc->sc_xe_ent = 0;
	sc->sc_xe_togo = cs_xmit_early_table[sc->sc_xe_ent].better_count;

	/* Initialize ifnet structure. */
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = cs_start_output;
	ifp->if_init = cs_init;
	ifp->if_ioctl = cs_ioctl;
	ifp->if_stop = cs_stop;
	ifp->if_watchdog = NULL;	/* no watchdog at this stage */
	ifp->if_flags = IFF_SIMPLEX | IFF_NOTRAILERS |
	    IFF_BROADCAST | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize ifmedia structures. */
	ifmedia_init(&sc->sc_media, 0, cs_mediachange, cs_mediastatus);

	if (media != NULL) {
		for (i = 0; i < nmedia; i++)
			ifmedia_add(&sc->sc_media, media[i], 0, NULL);
		ifmedia_set(&sc->sc_media, defmedia);
	} else {
		for (i = 0; i < cs_default_nmedia; i++)
			ifmedia_add(&sc->sc_media, cs_default_media[i],
			    0, NULL);
		cs_get_default_media(sc);
	}

	if (sc->sc_cfgflags & CFGFLG_PARSE_EEPROM) {
		if (cs_scan_eeprom(sc) == CS_ERROR) {
			/* failed to scan the eeprom, pretend there isn't an eeprom */
			aprint_error_dev(sc->sc_dev, "unable to scan EEPROM\n");
			sc->sc_cfgflags |= CFGFLG_NOT_EEPROM;
		}
	}

	if ((sc->sc_cfgflags & CFGFLG_NOT_EEPROM) == 0) {
		/* Get parameters from the EEPROM */
		if (cs_get_params(sc) == CS_ERROR) {
			aprint_error_dev(sc->sc_dev,
			    "unable to get settings from EEPROM\n");
			return 1;
		}
	}

	if (enaddr != NULL)
		memcpy(sc->sc_enaddr, enaddr, sizeof(sc->sc_enaddr));
	else if ((sc->sc_cfgflags & CFGFLG_NOT_EEPROM) == 0) {
		/* Get and store the Ethernet address */
		if (cs_get_enaddr(sc) == CS_ERROR) {
			aprint_error_dev(sc->sc_dev,
			    "unable to read Ethernet address\n");
			return 1;
		}
	} else {
#if 1
		int j;
		uint v;

		for (j = 0; j < 6; j += 2) {
			v = CS_READ_PACKET_PAGE(sc, PKTPG_IND_ADDR + j);
			sc->sc_enaddr[j + 0] = v;
			sc->sc_enaddr[j + 1] = v >> 8;
		}
#else
		printf("%s: no Ethernet address!\n", device_xname(sc->sc_dev));
		return 1;
#endif
	}

	switch (IFM_SUBTYPE(sc->sc_media.ifm_cur->ifm_media)) {
	case IFM_10_2:
		medname = "BNC";
		break;
	case IFM_10_5:
		medname = "AUI";
		break;
	case IFM_10_T:
		if (sc->sc_media.ifm_cur->ifm_media & IFM_FDX)
			medname = "UTP <full-duplex>";
		else
			medname = "UTP";
		break;
	default:
		panic("cs_attach: impossible");
	}
	printf("%s: %s rev. %c, address %s, media %s\n",
	    device_xname(sc->sc_dev),
	    chipname, sc->sc_prodrev + 'A', ether_sprintf(sc->sc_enaddr),
	    medname);

	if (sc->sc_dma_attach)
		(*sc->sc_dma_attach)(sc);

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
			  RND_TYPE_NET, RND_FLAG_DEFAULT);
	sc->sc_cfgflags |= CFGFLG_ATTACHED;

	if (pmf_device_register1(sc->sc_dev, NULL, NULL, cs_shutdown))
		pmf_class_network_register(sc->sc_dev, ifp);
	else
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish power handler\n");

	/* Reset the chip */
	if (cs_reset_chip(sc) == CS_ERROR) {
		aprint_error_dev(sc->sc_dev, "reset failed\n");
		cs_detach(sc);
		return 1;
	}

	return 0;
}

int
cs_detach(struct cs_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	if (sc->sc_cfgflags & CFGFLG_ATTACHED) {
		rnd_detach_source(&sc->rnd_source);
		ether_ifdetach(ifp);
		if_detach(ifp);
		sc->sc_cfgflags &= ~CFGFLG_ATTACHED;
	}

#if 0
	/*
	 * XXX not necessary
	 */
	if (sc->sc_cfgflags & CFGFLG_DMA_MODE) {
		isa_dmamem_unmap(sc->sc_ic, sc->sc_drq, sc->sc_dmabase, sc->sc_dmasize);
		isa_dmamem_free(sc->sc_ic, sc->sc_drq, sc->sc_dmaaddr, sc->sc_dmasize);
		isa_dmamap_destroy(sc->sc_ic, sc->sc_drq);
		sc->sc_cfgflags &= ~CFGFLG_DMA_MODE;
	}
#endif

	pmf_device_deregister(sc->sc_dev);

	return 0;
}

bool
cs_shutdown(device_t self, int howto)
{
	struct cs_softc *sc;

	sc = device_private(self);
	cs_reset(sc);

	return true;
}

void
cs_get_default_media(struct cs_softc *sc)
{
	u_int16_t adp_cfg, xmit_ctl;

	if (cs_verify_eeprom(sc) == CS_ERROR) {
		aprint_error_dev(sc->sc_dev,
		    "cs_get_default_media: EEPROM missing or bad\n");
		goto fakeit;
	}

	if (cs_read_eeprom(sc, EEPROM_ADPTR_CFG, &adp_cfg) == CS_ERROR) {
		aprint_error_dev(sc->sc_dev,
		    "unable to read adapter config from EEPROM\n");
		goto fakeit;
	}

	if (cs_read_eeprom(sc, EEPROM_XMIT_CTL, &xmit_ctl) == CS_ERROR) {
		aprint_error_dev(sc->sc_dev,
		    "unable to read transmit control from EEPROM\n");
		goto fakeit;
	}

	switch (adp_cfg & ADPTR_CFG_MEDIA) {
	case ADPTR_CFG_AUI:
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_10_5);
		break;
	case ADPTR_CFG_10BASE2:
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_10_2);
		break;
	case ADPTR_CFG_10BASET:
	default:
		if (xmit_ctl & XMIT_CTL_FDX)
			ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_10_T|IFM_FDX);
		else
			ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_10_T);
		break;
	}
	return;

 fakeit:
	aprint_error_dev(sc->sc_dev,
	    "WARNING: default media setting may be inaccurate\n");
	/* XXX Arbitrary... */
	ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_10_T);
}

/*
 * cs_scan_eeprom
 *
 * Attempt to take a complete copy of the eeprom into main memory.
 * this will allow faster parsing of the eeprom data.
 *
 * Only tested against a 8920M's eeprom, but the data sheet for the
 * 8920A indicates that is uses the same layout.
 */
int
cs_scan_eeprom(struct cs_softc *sc)
{
	u_int16_t result;
	int	i;
	int	eeprom_size;
	u_int8_t checksum = 0;

	if (cs_verify_eeprom(sc) == CS_ERROR) {
		aprint_error_dev(sc->sc_dev,
		    "cs_scan_params: EEPROM missing or bad\n");
		return (CS_ERROR);
	}

	/*
	 * read the 0th word from the eeprom, it will tell us the length
	 * and if the eeprom is valid
	 */
	cs_read_eeprom(sc, 0, &result);

	/* check the eeprom signature */
	if ((result & 0xE000) != 0xA000) {
		/* empty eeprom */
		return (CS_ERROR);
	}

	/*
	 * take the eeprom size (note the read value doesn't include the header
	 * word)
	 */
	eeprom_size = (result & 0xff) + 2;

	sc->eeprom_data = malloc(eeprom_size, M_DEVBUF, M_WAITOK);
	if (sc->eeprom_data == NULL) {
		/* no memory, treat this as if there's no eeprom */
		return (CS_ERROR);
	}

	sc->eeprom_size = eeprom_size;

	/* read the eeprom into the buffer, also calculate the checksum  */
	for (i = 0; i < (eeprom_size >> 1); i++) {
		cs_read_eeprom(sc, i, &(sc->eeprom_data[i]));
		checksum += (sc->eeprom_data[i] & 0xff00) >> 8;
		checksum += (sc->eeprom_data[i] & 0x00ff);
	}

	/*
	 * validate checksum calculation, the sum of all the bytes should be 0,
	 * as the high byte of the last word is the 2's complement of the
	 * sum to that point.
	 */
	if (checksum != 0) {
		aprint_error_dev(sc->sc_dev, "eeprom checksum failure\n");
		return (CS_ERROR);
	}

	return (CS_OK);
}

static int
cs_read_pktpg_from_eeprom(struct cs_softc *sc, int pktpg, u_int16_t *pValue)
{
	int x, maxword;

	/* Check that we have eeprom data */
	if ((sc->eeprom_data == NULL) || (sc->eeprom_size < 2))
		return (CS_ERROR);

	/*
	 * We only want to read the data words, the last word contains the
	 * checksum
	 */
	maxword = (sc->eeprom_size - 2) >> 1;

	/* start 1 word in, as the first word is the length and signature */
	x = 1;

	while ( x < (maxword)) {
		u_int16_t header;
		int group_size;
		int offset;
		int offset_max;

		/* read in the group header word */
		header = sc->eeprom_data[x];
		x++;	/* skip group header */

		/*
		 * size of group in words is in the top 4 bits, note that it
		 * is one less than the number of words
		 */
		group_size = header & 0xF000;

		/*
		 * CS8900 Data sheet says this should be 0x01ff,
		 * but my cs8920 eeprom has higher offsets,
		 * perhaps the 8920 allows higher offsets, otherwise
		 * it's writing to places that it shouldn't
		 */
		/* work out the offsets this group covers */
		offset = header & 0x0FFF;
		offset_max = offset + (group_size << 1);

		/* check if the pkgpg we're after is in this group */
		if ((offset <= pktpg) && (pktpg <= offset_max)) {
			/* the pkgpg value we want is in here */
			int eeprom_location;

			eeprom_location = ((pktpg - offset) >> 1) ;

			*pValue = sc->eeprom_data[x + eeprom_location];
			return (CS_OK);
		} else {
			/* skip this group (+ 1 for first entry) */
			x += group_size + 1;
		}
	}

	/*
	 * if we've fallen out here then we don't have a value in the EEPROM
	 * for this pktpg so return an error
	 */
	return (CS_ERROR);
}

int
cs_get_params(struct cs_softc *sc)
{
	u_int16_t isaConfig;
	u_int16_t adapterConfig;

	if (cs_verify_eeprom(sc) == CS_ERROR) {
		aprint_error_dev(sc->sc_dev,
		    "cs_get_params: EEPROM missing or bad\n");
		return (CS_ERROR);
	}

	if (sc->sc_cfgflags & CFGFLG_PARSE_EEPROM) {
		/* Get ISA configuration from the EEPROM */
		if (cs_read_pktpg_from_eeprom(sc, PKTPG_BUS_CTL, &isaConfig)
			       	== CS_ERROR) {
			/* eeprom doesn't have this value, use data sheet default */
			isaConfig = 0x0017;
		}

		/* Get adapter configuration from the EEPROM */
		if (cs_read_pktpg_from_eeprom(sc, PKTPG_SELF_CTL, &adapterConfig)
				== CS_ERROR) {
			/* eeprom doesn't have this value, use data sheet default */
			adapterConfig = 0x0015;
		}

		/* Copy the USE_SA flag */
		if (isaConfig & BUS_CTL_USE_SA)
			sc->sc_cfgflags |= CFGFLG_USE_SA;

		/* Copy the IO Channel Ready flag */
		if (isaConfig & BUS_CTL_IOCHRDY)
			sc->sc_cfgflags |= CFGFLG_IOCHRDY;

		/* Copy the DC/DC Polarity flag */
		if (adapterConfig & SELF_CTL_HCB1)
			sc->sc_cfgflags |= CFGFLG_DCDC_POL;
	} else {
		/* Get ISA configuration from the EEPROM */
		if (cs_read_eeprom(sc, EEPROM_ISA_CFG, &isaConfig) == CS_ERROR)
			goto eeprom_bad;

		/* Get adapter configuration from the EEPROM */
		if (cs_read_eeprom(sc, EEPROM_ADPTR_CFG, &adapterConfig) == CS_ERROR)
			goto eeprom_bad;

		/* Copy the USE_SA flag */
		if (isaConfig & ISA_CFG_USE_SA)
			sc->sc_cfgflags |= CFGFLG_USE_SA;

		/* Copy the IO Channel Ready flag */
		if (isaConfig & ISA_CFG_IOCHRDY)
			sc->sc_cfgflags |= CFGFLG_IOCHRDY;

		/* Copy the DC/DC Polarity flag */
		if (adapterConfig & ADPTR_CFG_DCDC_POL)
			sc->sc_cfgflags |= CFGFLG_DCDC_POL;
	}

	return (CS_OK);
eeprom_bad:
	aprint_error_dev(sc->sc_dev,
	    "cs_get_params: unable to read from EEPROM\n");
	return (CS_ERROR);
}

int
cs_get_enaddr(struct cs_softc *sc)
{
	uint16_t myea[ETHER_ADDR_LEN / sizeof(uint16_t)];
	int i;

	if (cs_verify_eeprom(sc) == CS_ERROR) {
		aprint_error_dev(sc->sc_dev,
		    "cs_get_enaddr: EEPROM missing or bad\n");
		return (CS_ERROR);
	}

	/* Get Ethernet address from the EEPROM */
	if (sc->sc_cfgflags & CFGFLG_PARSE_EEPROM) {
		if (cs_read_pktpg_from_eeprom(sc, PKTPG_IND_ADDR, &myea[0])
				== CS_ERROR)
			goto eeprom_bad;
		if (cs_read_pktpg_from_eeprom(sc, PKTPG_IND_ADDR + 2, &myea[1])
				== CS_ERROR)
			goto eeprom_bad;
		if (cs_read_pktpg_from_eeprom(sc, PKTPG_IND_ADDR + 4, &myea[2])
				== CS_ERROR)
			goto eeprom_bad;
	} else {
		if (cs_read_eeprom(sc, EEPROM_IND_ADDR_H, &myea[0]) == CS_ERROR)
			goto eeprom_bad;
		if (cs_read_eeprom(sc, EEPROM_IND_ADDR_M, &myea[1]) == CS_ERROR)
			goto eeprom_bad;
		if (cs_read_eeprom(sc, EEPROM_IND_ADDR_L, &myea[2]) == CS_ERROR)
			goto eeprom_bad;
	}

	for (i = 0; i < __arraycount(myea); i++) {
		sc->sc_enaddr[i * 2 + 0] = myea[i];
		sc->sc_enaddr[i * 2 + 1] = myea[i] >> 8;
	}

	return (CS_OK);

 eeprom_bad:
	aprint_error_dev(sc->sc_dev,
	    "cs_get_enaddr: unable to read from EEPROM\n");
	return (CS_ERROR);
}

int
cs_reset_chip(struct cs_softc *sc)
{
	int intState;
	int x;

	/* Disable interrupts at the CPU so reset command is atomic */
	intState = splnet();

	/*
	 * We are now resetting the chip
	 *
	 * A spurious interrupt is generated by the chip when it is reset. This
	 * variable informs the interrupt handler to ignore this interrupt.
	 */
	sc->sc_resetting = TRUE;

	/* Issue a reset command to the chip */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_SELF_CTL, SELF_CTL_RESET);

	/* Re-enable interrupts at the CPU */
	splx(intState);

	/* The chip is always in IO mode after a reset */
	sc->sc_memorymode = FALSE;

	/* If transmission was in progress, it is not now */
	sc->sc_txbusy = FALSE;

	/*
	 * there was a delay(125); here, but it seems uneccesary 125 usec is
	 * 1/8000 of a second, not 1/8 of a second. the data sheet advises
	 * 1/10 of a second here, but the SI_BUSY and INIT_DONE loops below
	 * should be sufficient.
	 */

	/* Transition SBHE to switch chip from 8-bit to 16-bit */
	IO_READ_1(sc, PORT_PKTPG_PTR + 0);
	IO_READ_1(sc, PORT_PKTPG_PTR + 1);
	IO_READ_1(sc, PORT_PKTPG_PTR + 0);
	IO_READ_1(sc, PORT_PKTPG_PTR + 1);

	/* Wait until the EEPROM is not busy */
	for (x = 0; x < MAXLOOP; x++) {
		if (!(CS_READ_PACKET_PAGE(sc, PKTPG_SELF_ST) & SELF_ST_SI_BUSY))
			break;
	}

	if (x == MAXLOOP)
		return CS_ERROR;

	/* Wait until initialization is done */
	for (x = 0; x < MAXLOOP; x++) {
		if (CS_READ_PACKET_PAGE(sc, PKTPG_SELF_ST) & SELF_ST_INIT_DONE)
			break;
	}

	if (x == MAXLOOP)
		return CS_ERROR;

	/* Reset is no longer in progress */
	sc->sc_resetting = FALSE;

	return CS_OK;
}

int
cs_verify_eeprom(struct cs_softc *sc)
{
	u_int16_t self_status;

	/* Verify that the EEPROM is present and OK */
	self_status = CS_READ_PACKET_PAGE_IO(sc, PKTPG_SELF_ST);
	if (((self_status & SELF_ST_EEP_PRES) &&
	     (self_status & SELF_ST_EEP_OK)) == 0)
		return (CS_ERROR);

	return (CS_OK);
}

int
cs_read_eeprom(struct cs_softc *sc, int offset, u_int16_t *pValue)
{
	int x;

	/* Ensure that the EEPROM is not busy */
	for (x = 0; x < MAXLOOP; x++) {
		if (!(CS_READ_PACKET_PAGE_IO(sc, PKTPG_SELF_ST) &
		      SELF_ST_SI_BUSY))
			break;
	}

	if (x == MAXLOOP)
		return (CS_ERROR);

	/* Issue the command to read the offset within the EEPROM */
	CS_WRITE_PACKET_PAGE_IO(sc, PKTPG_EEPROM_CMD,
	    offset | EEPROM_CMD_READ);

	/* Wait until the command is completed */
	for (x = 0; x < MAXLOOP; x++) {
		if (!(CS_READ_PACKET_PAGE_IO(sc, PKTPG_SELF_ST) &
		      SELF_ST_SI_BUSY))
			break;
	}

	if (x == MAXLOOP)
		return (CS_ERROR);

	/* Get the EEPROM data from the EEPROM Data register */
	*pValue = CS_READ_PACKET_PAGE_IO(sc, PKTPG_EEPROM_DATA);

	return (CS_OK);
}

void
cs_initChip(struct cs_softc *sc)
{
	u_int16_t busCtl;
	u_int16_t selfCtl;
	u_int16_t v;
	u_int16_t isaId;
	int i;
	int media = IFM_SUBTYPE(sc->sc_media.ifm_cur->ifm_media);

	/* Disable reception and transmission of frames */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_LINE_CTL,
	    CS_READ_PACKET_PAGE(sc, PKTPG_LINE_CTL) &
	    ~LINE_CTL_RX_ON & ~LINE_CTL_TX_ON);

	/* Disable interrupt at the chip */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_BUS_CTL,
	    CS_READ_PACKET_PAGE(sc, PKTPG_BUS_CTL) & ~BUS_CTL_INT_ENBL);

	/* If IOCHRDY is enabled then clear the bit in the busCtl register */
	busCtl = CS_READ_PACKET_PAGE(sc, PKTPG_BUS_CTL);
	if (sc->sc_cfgflags & CFGFLG_IOCHRDY) {
		CS_WRITE_PACKET_PAGE(sc, PKTPG_BUS_CTL,
		    busCtl & ~BUS_CTL_IOCHRDY);
	} else {
		CS_WRITE_PACKET_PAGE(sc, PKTPG_BUS_CTL,
		    busCtl | BUS_CTL_IOCHRDY);
	}

	/* Set the Line Control register to match the media type */
	if (media == IFM_10_T)
		CS_WRITE_PACKET_PAGE(sc, PKTPG_LINE_CTL, LINE_CTL_10BASET);
	else
		CS_WRITE_PACKET_PAGE(sc, PKTPG_LINE_CTL, LINE_CTL_AUI_ONLY);

	/*
	 * Set the BSTATUS/HC1 pin to be used as HC1.  HC1 is used to
	 * enable the DC/DC converter
	 */
	selfCtl = SELF_CTL_HC1E;

	/* If the media type is 10Base2 */
	if (media == IFM_10_2) {
		/*
		 * Enable the DC/DC converter if it has a low enable.
		 */
		if ((sc->sc_cfgflags & CFGFLG_DCDC_POL) == 0)
			/*
			 * Set the HCB1 bit, which causes the HC1 pin to go
			 * low.
			 */
			selfCtl |= SELF_CTL_HCB1;
	} else { /* Media type is 10BaseT or AUI */
		/*
		 * Disable the DC/DC converter if it has a high enable.
		 */
		if ((sc->sc_cfgflags & CFGFLG_DCDC_POL) != 0) {
			/*
			 * Set the HCB1 bit, which causes the HC1 pin to go
			 * low.
			 */
			selfCtl |= SELF_CTL_HCB1;
		}
	}
	CS_WRITE_PACKET_PAGE(sc, PKTPG_SELF_CTL, selfCtl);

	/* enable normal link pulse */
	if (sc->sc_prodid == PROD_ID_CS8920 || sc->sc_prodid == PROD_ID_CS8920M)
		CS_WRITE_PACKET_PAGE(sc, PKTPG_AUTONEG_CTL, AUTOCTL_NLP_ENABLE);

	/* Enable full-duplex, if appropriate */
	if (sc->sc_media.ifm_cur->ifm_media & IFM_FDX)
		CS_WRITE_PACKET_PAGE(sc, PKTPG_TEST_CTL, TEST_CTL_FDX);

	/* RX_CTL set in cs_set_ladr_filt(), below */

	/* enable all transmission interrupts */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_TX_CFG, TX_CFG_ALL_IE);

	/* Accept all receive interrupts */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG, RX_CFG_ALL_IE);

	/*
	 * Configure Operational Modes
	 *
	 * I have turned off the BUF_CFG_RX_MISS_IE, to speed things up, this is
	 * a better way to do it because the card has a counter which can be
	 * read to update the RX_MISS counter. This saves many interrupts.
	 *
	 * I have turned on the tx and rx overflow interrupts to counter using
	 * the receive miss interrupt. This is a better estimate of errors
	 * and requires lower system overhead.
	 */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_BUF_CFG, BUF_CFG_TX_UNDR_IE |
			  BUF_CFG_RX_DMA_IE);

	if (sc->sc_dma_chipinit)
		(*sc->sc_dma_chipinit)(sc);

	/* If memory mode is enabled */
	if (sc->sc_cfgflags & CFGFLG_MEM_MODE) {
		/* If external logic is present for address decoding */
		if (CS_READ_PACKET_PAGE(sc, PKTPG_SELF_ST) & SELF_ST_EL_PRES) {
			/*
			 * Program the external logic to decode address bits
			 * SA20-SA23
			 */
			CS_WRITE_PACKET_PAGE(sc, PKTPG_EEPROM_CMD,
			    ((sc->sc_pktpgaddr & 0xffffff) >> 20) |
			    EEPROM_CMD_ELSEL);
		}

		/*
		 * Write the packet page base physical address to the memory
		 * base register.
		 */
		CS_WRITE_PACKET_PAGE(sc, PKTPG_MEM_BASE + 0,
		    sc->sc_pktpgaddr & 0xFFFF);
		CS_WRITE_PACKET_PAGE(sc, PKTPG_MEM_BASE + 2,
		    sc->sc_pktpgaddr >> 16);
		busCtl = BUS_CTL_MEM_MODE;

		/* tell the chip to read the addresses off the SA pins */
		if (sc->sc_cfgflags & CFGFLG_USE_SA) {
			busCtl |= BUS_CTL_USE_SA;
		}
		CS_WRITE_PACKET_PAGE(sc, PKTPG_BUS_CTL,
		    CS_READ_PACKET_PAGE(sc, PKTPG_BUS_CTL) | busCtl);

		/* We are in memory mode now! */
		sc->sc_memorymode = TRUE;

		/*
		 * wait here (10ms) for the chip to swap over. this is the
		 * maximum time that this could take.
		 */
		delay(10000);

		/* Verify that we can read from the chip */
		isaId = CS_READ_PACKET_PAGE(sc, PKTPG_EISA_NUM);

		/*
		 * As a last minute sanity check before actually using mapped
		 * memory we verify that we can read the isa number from the
		 * chip in memory mode.
		 */
		if (isaId != EISA_NUM_CRYSTAL) {
			aprint_error_dev(sc->sc_dev,
			    "failed to enable memory mode\n");
			sc->sc_memorymode = FALSE;
		} else {
			/*
			 * we are in memory mode so if we aren't using DMA,
			 * then program the chip to interrupt early.
			 */
			if ((sc->sc_cfgflags & CFGFLG_DMA_MODE) == 0) {
				CS_WRITE_PACKET_PAGE(sc, PKTPG_BUF_CFG,
				    BUF_CFG_RX_DEST_IE |
				    BUF_CFG_RX_MISS_OVER_IE |
				    BUF_CFG_TX_COL_OVER_IE);
			}
		}

	}

	/* Put Ethernet address into the Individual Address register */
	for (i = 0; i < 6; i += 2) {
		v = sc->sc_enaddr[i + 0] | (sc->sc_enaddr[i + 1]) << 8;
		CS_WRITE_PACKET_PAGE(sc, PKTPG_IND_ADDR + i, v);
	}

	if (sc->sc_irq != -1) {
		/* Set the interrupt level in the chip */
		if (sc->sc_prodid == PROD_ID_CS8900) {
			if (sc->sc_irq == 5) {
				CS_WRITE_PACKET_PAGE(sc, PKTPG_INT_NUM, 3);
			} else {
				CS_WRITE_PACKET_PAGE(sc, PKTPG_INT_NUM, (sc->sc_irq) - 10);
			}
		}
		else { /* CS8920 */
			CS_WRITE_PACKET_PAGE(sc, PKTPG_8920_INT_NUM, sc->sc_irq);
		}
	}

	/* write the multicast mask to the address filter register */
	cs_set_ladr_filt(sc, &sc->sc_ethercom);

	/* Enable reception and transmission of frames */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_LINE_CTL,
	    CS_READ_PACKET_PAGE(sc, PKTPG_LINE_CTL) |
	    LINE_CTL_RX_ON | LINE_CTL_TX_ON);

	/* Enable interrupt at the chip */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_BUS_CTL,
	    CS_READ_PACKET_PAGE(sc, PKTPG_BUS_CTL) | BUS_CTL_INT_ENBL);
}

int
cs_init(struct ifnet *ifp)
{
	int intState;
	int error = CS_OK;
	struct cs_softc *sc = ifp->if_softc;

	if (cs_enable(sc))
		goto out;

	cs_stop(ifp, 0);

	intState = splnet();

#if 0
	/* Mark the interface as down */
	sc->sc_ethercom.ec_if.if_flags &= ~(IFF_UP | IFF_RUNNING);
#endif

#ifdef CS_DEBUG
	/* Enable debugging */
	sc->sc_ethercom.ec_if.if_flags |= IFF_DEBUG;
#endif

	/* Reset the chip */
	if ((error = cs_reset_chip(sc)) == CS_OK) {
		/* Initialize the chip */
		cs_initChip(sc);

		/* Mark the interface as running */
		sc->sc_ethercom.ec_if.if_flags |= IFF_RUNNING;
		sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;
		sc->sc_ethercom.ec_if.if_timer = 0;

		/* Assume we have carrier until we are told otherwise. */
		sc->sc_carrier = 1;
	} else {
		aprint_error_dev(sc->sc_dev, "unable to reset chip\n");
	}

	splx(intState);
out:
	if (error == CS_OK)
		return 0;
	return EIO;
}

void
cs_set_ladr_filt(struct cs_softc *sc, struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int16_t af[4];
	u_int16_t port, mask, index;

	/*
         * Set up multicast address filter by passing all multicast addresses
         * through a crc generator, and then using the high order 6 bits as an
         * index into the 64 bit logical address filter.  The high order bit
         * selects the word, while the rest of the bits select the bit within
         * the word.
         */
	if (ifp->if_flags & IFF_PROMISC) {
		/* accept all valid frames. */
		CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CTL,
		    RX_CTL_PROMISC_A | RX_CTL_RX_OK_A |
		    RX_CTL_IND_A | RX_CTL_BCAST_A | RX_CTL_MCAST_A);
		ifp->if_flags |= IFF_ALLMULTI;
		return;
	}

	/*
	 * accept frames if a. crc valid, b. individual address match c.
	 * broadcast address,and d. multicast addresses matched in the hash
	 * filter
	 */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CTL,
	    RX_CTL_RX_OK_A | RX_CTL_IND_A | RX_CTL_BCAST_A | RX_CTL_MCAST_A);


	/*
	 * start off with all multicast flag clear, set it if we need to
	 * later, otherwise we will leave it.
	 */
	ifp->if_flags &= ~IFF_ALLMULTI;
	af[0] = af[1] = af[2] = af[3] = 0x0000;

	/*
	 * Loop through all the multicast addresses unless we get a range of
	 * addresses, in which case we will just accept all packets.
	 * Justification for this is given in the next comment.
	 */
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof enm->enm_addrlo)) {
			/*
	                 * We must listen to a range of multicast addresses.
	                 * For now, just accept all multicasts, rather than
	                 * trying to set only those filter bits needed to match
	                 * the range.  (At this time, the only use of address
	                 * ranges is for IP multicast routing, for which the
	                 * range is big enough to require all bits set.)
	                 */
			ifp->if_flags |= IFF_ALLMULTI;
			af[0] = af[1] = af[2] = af[3] = 0xffff;
			break;
		} else {
			/*
	                 * we have got an individual address so just set that
	                 * bit.
	                 */
			index = cs_hash_index(enm->enm_addrlo);

			/* Set the bit the Logical address filter. */
			port = (u_int16_t) (index >> 4);
			mask = (u_int16_t) (1 << (index & 0xf));
			af[port] |= mask;

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	/* now program the chip with the addresses */
	CS_WRITE_PACKET_PAGE(sc, PKTPG_LOG_ADDR + 0, af[0]);
	CS_WRITE_PACKET_PAGE(sc, PKTPG_LOG_ADDR + 2, af[1]);
	CS_WRITE_PACKET_PAGE(sc, PKTPG_LOG_ADDR + 4, af[2]);
	CS_WRITE_PACKET_PAGE(sc, PKTPG_LOG_ADDR + 6, af[3]);
	return;
}

u_int16_t
cs_hash_index(char *addr)
{
	uint32_t crc;
	uint16_t hash_code;

	crc = ether_crc32_le(addr, ETHER_ADDR_LEN);

	hash_code = crc >> 26;
	return (hash_code);
}

void
cs_reset(struct cs_softc *sc)
{

	/* Mark the interface as down */
	sc->sc_ethercom.ec_if.if_flags &= ~IFF_RUNNING;

	/* Reset the chip */
	cs_reset_chip(sc);
}

int
cs_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct cs_softc *sc = ifp->if_softc;
	struct ifreq *ifr = data;
	int state;
	int result;

	state = splnet();

	result = 0;		/* only set if something goes wrong */

	switch (cmd) {
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		result = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		result = ether_ioctl(ifp, cmd, data);
		if (result == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING) {
				/*
				 * Multicast list has changed.  Set the
				 * hardware filter accordingly.
				 */
				cs_set_ladr_filt(sc, &sc->sc_ethercom);
			}
			result = 0;
		}
		break;
	}

	splx(state);

	return result;
}

int
cs_mediachange(struct ifnet *ifp)
{

	/*
	 * Current media is already set up.  Just reset the interface
	 * to let the new value take hold.
	 */
	cs_init(ifp);
	return (0);
}

void
cs_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct cs_softc *sc = ifp->if_softc;

	/*
	 * The currently selected media is always the active media.
	 */
	ifmr->ifm_active = sc->sc_media.ifm_cur->ifm_media;

	if (ifp->if_flags & IFF_UP) {
		/* Interface up, status is valid. */
		ifmr->ifm_status = IFM_AVALID |
		    (sc->sc_carrier ? IFM_ACTIVE : 0);
	}
		else ifmr->ifm_status = 0;
}

int
cs_intr(void *arg)
{
	struct cs_softc *sc = arg;
	u_int16_t Event;
	u_int16_t rndEvent;

/*printf("cs_intr %p\n", sc);*/
	/* Ignore any interrupts that happen while the chip is being reset */
	if (sc->sc_resetting) {
		printf("%s: cs_intr: reset in progress\n",
		    device_xname(sc->sc_dev));
		return 1;
	}

	/* Read an event from the Interrupt Status Queue */
	if (sc->sc_memorymode)
		Event = CS_READ_PACKET_PAGE(sc, PKTPG_ISQ);
	else
		Event = CS_READ_PORT(sc, PORT_ISQ);

	if ((Event & REG_NUM_MASK) == 0 || Event == 0xffff)
		return 0;	/* not ours */

	rndEvent = Event;

	/* Process all the events in the Interrupt Status Queue */
	while ((Event & REG_NUM_MASK) != 0 && Event != 0xffff) {
		/* Dispatch to an event handler based on the register number */
		switch (Event & REG_NUM_MASK) {
		case REG_NUM_RX_EVENT:
			cs_receive_event(sc, Event);
			break;
		case REG_NUM_TX_EVENT:
			cs_transmit_event(sc, Event);
			break;
		case REG_NUM_BUF_EVENT:
			cs_buffer_event(sc, Event);
			break;
		case REG_NUM_TX_COL:
		case REG_NUM_RX_MISS:
			cs_counter_event(sc, Event);
			break;
		default:
			printf("%s: unknown interrupt event 0x%x\n",
			    device_xname(sc->sc_dev), Event);
			break;
		}

		/* Read another event from the Interrupt Status Queue */
		if (sc->sc_memorymode)
			Event = CS_READ_PACKET_PAGE(sc, PKTPG_ISQ);
		else
			Event = CS_READ_PORT(sc, PORT_ISQ);
	}

	/* have handled the interrupt */
	rnd_add_uint32(&sc->rnd_source, rndEvent);
	return 1;
}

void
cs_counter_event(struct cs_softc *sc, u_int16_t cntEvent)
{
	struct ifnet *ifp;
	u_int16_t errorCount;

	ifp = &sc->sc_ethercom.ec_if;

	switch (cntEvent & REG_NUM_MASK) {
	case REG_NUM_TX_COL:
		/*
		 * the count should be read before an overflow occurs.
		 */
		errorCount = CS_READ_PACKET_PAGE(sc, PKTPG_TX_COL);
		/*
		 * the tramsit event routine always checks the number of
		 * collisions for any packet so we don't increment any
		 * counters here, as they should already have been
		 * considered.
		 */
		break;
	case REG_NUM_RX_MISS:
		/*
		 * the count should be read before an overflow occurs.
		 */
		errorCount = CS_READ_PACKET_PAGE(sc, PKTPG_RX_MISS);
		/*
		 * Increment the input error count, the first 6bits are the
		 * register id.
		 */
		ifp->if_ierrors += ((errorCount & 0xffC0) >> 6);
		break;
	default:
		/* do nothing */
		break;
	}
}

void
cs_buffer_event(struct cs_softc *sc, u_int16_t bufEvent)
{

	/*
	 * multiple events can be in the buffer event register at one time so
	 * a standard switch statement will not suffice, here every event
	 * must be checked.
	 */

	/*
	 * if 128 bits have been rxed by the time we get here, the dest event
	 * will be cleared and 128 event will be set.
	 */
	if ((bufEvent & (BUF_EVENT_RX_DEST | BUF_EVENT_RX_128)) != 0) {
		cs_process_rx_early(sc);
	}

	if (bufEvent & BUF_EVENT_RX_DMA) {
		/* process the receive data */
		if (sc->sc_dma_process_rx)
			(*sc->sc_dma_process_rx)(sc);
		else
			/* should panic? */
			aprint_error_dev(sc->sc_dev, "unexpected DMA event\n");
	}

	if (bufEvent & BUF_EVENT_TX_UNDR) {
#if 0
		/*
		 * This can happen occasionally, and it's not worth worrying
		 * about.
		 */
		printf("%s: transmit underrun (%d -> %d)\n",
		    device_xname(sc->sc_dev), sc->sc_xe_ent,
		    cs_xmit_early_table[sc->sc_xe_ent].worse);
#endif
		sc->sc_xe_ent = cs_xmit_early_table[sc->sc_xe_ent].worse;
		sc->sc_xe_togo =
		    cs_xmit_early_table[sc->sc_xe_ent].better_count;

		/* had an underrun, transmit is finished */
		sc->sc_txbusy = FALSE;
	}

	if (bufEvent & BUF_EVENT_SW_INT) {
		printf("%s: software initiated interrupt\n",
		    device_xname(sc->sc_dev));
	}
}

void
cs_transmit_event(struct cs_softc *sc, u_int16_t txEvent)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/* If there were any errors transmitting this frame */
	if (txEvent & (TX_EVENT_LOSS_CRS | TX_EVENT_SQE_ERR | TX_EVENT_OUT_WIN |
		       TX_EVENT_JABBER | TX_EVENT_16_COLL)) {
		/* Increment the output error count */
		ifp->if_oerrors++;

		/* Note carrier loss. */
		if (txEvent & TX_EVENT_LOSS_CRS)
			sc->sc_carrier = 0;

		/* If debugging is enabled then log error messages */
		if (ifp->if_flags & IFF_DEBUG) {
			if (txEvent & TX_EVENT_LOSS_CRS) {
				aprint_error_dev(sc->sc_dev, "lost carrier\n");
			}
			if (txEvent & TX_EVENT_SQE_ERR) {
				aprint_error_dev(sc->sc_dev, "SQE error\n");
			}
			if (txEvent & TX_EVENT_OUT_WIN) {
				aprint_error_dev(sc->sc_dev,
				    "out-of-window collision\n");
			}
			if (txEvent & TX_EVENT_JABBER) {
				aprint_error_dev(sc->sc_dev, "jabber\n");
			}
			if (txEvent & TX_EVENT_16_COLL) {
				aprint_error_dev(sc->sc_dev, "16 collisions\n");
			}
		}
	}
	else {
		/* Transmission successful, carrier is up. */
		sc->sc_carrier = 1;
#ifdef SHARK
		ledNetActive();
#endif
	}

	/* Add the number of collisions for this frame */
	if (txEvent & TX_EVENT_16_COLL) {
		ifp->if_collisions += 16;
	} else {
		ifp->if_collisions += ((txEvent & TX_EVENT_COLL_MASK) >> 11);
	}

	ifp->if_opackets++;

	/* Transmission is no longer in progress */
	sc->sc_txbusy = FALSE;

	/* If there is more to transmit */
	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0) {
		/* Start the next transmission */
		cs_start_output(ifp);
	}
}

void
cs_print_rx_errors(struct cs_softc *sc, u_int16_t rxEvent)
{

	if (rxEvent & RX_EVENT_RUNT)
		aprint_error_dev(sc->sc_dev, "runt\n");

	if (rxEvent & RX_EVENT_X_DATA)
		aprint_error_dev(sc->sc_dev, "extra data\n");

	if (rxEvent & RX_EVENT_CRC_ERR) {
		if (rxEvent & RX_EVENT_DRIBBLE)
			aprint_error_dev(sc->sc_dev, "alignment error\n");
		else
			aprint_error_dev(sc->sc_dev, "CRC error\n");
	} else {
		if (rxEvent & RX_EVENT_DRIBBLE)
			aprint_error_dev(sc->sc_dev, "dribble bits\n");
	}
}

void
cs_receive_event(struct cs_softc *sc, u_int16_t rxEvent)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/* If the frame was not received OK */
	if (!(rxEvent & RX_EVENT_RX_OK)) {
		/* Increment the input error count */
		ifp->if_ierrors++;

		/*
		 * If debugging is enabled then log error messages.
		 */
		if (ifp->if_flags & IFF_DEBUG) {
			if (rxEvent != REG_NUM_RX_EVENT) {
				cs_print_rx_errors(sc, rxEvent);

				/*
				 * Must read the length of all received
				 * frames
				 */
				CS_READ_PACKET_PAGE(sc, PKTPG_RX_LENGTH);

				/* Skip the received frame */
				CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG,
					CS_READ_PACKET_PAGE(sc, PKTPG_RX_CFG) |
						  RX_CFG_SKIP);
			} else {
				aprint_error_dev(sc->sc_dev, "implied skip\n");
			}
		}
	} else {
		/*
		 * process the received frame and pass it up to the upper
		 * layers.
		 */
		cs_process_receive(sc);
	}
}

void
cs_ether_input(struct cs_softc *sc, struct mbuf *m)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	ifp->if_ipackets++;

	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	bpf_mtap(ifp, m);

	/* Pass the packet up. */
	(*ifp->if_input)(ifp, m);
}

void
cs_process_receive(struct cs_softc *sc)
{
	struct ifnet *ifp;
	struct mbuf *m;
	int totlen;
	u_int16_t *pBuff, *pBuffLimit;
	int pad;
	unsigned int frameOffset = 0;	/* XXX: gcc */

#ifdef SHARK
	ledNetActive();
#endif

	ifp = &sc->sc_ethercom.ec_if;

	/* Received a packet; carrier is up. */
	sc->sc_carrier = 1;

	if (sc->sc_memorymode) {
		/* Initialize the frame offset */
		frameOffset = PKTPG_RX_LENGTH;

		/* Get the length of the received frame */
		totlen = CS_READ_PACKET_PAGE(sc, frameOffset);
		frameOffset += 2;
	}
	else {
		/* drop status */
		CS_READ_PORT(sc, PORT_RXTX_DATA);

		/* Get the length of the received frame */
		totlen = CS_READ_PORT(sc, PORT_RXTX_DATA);
	}

	if (totlen > ETHER_MAX_LEN) {
		aprint_error_dev(sc->sc_dev, "invalid packet length %d\n",
		    totlen);

		/* skip the received frame */
		CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG,
			CS_READ_PACKET_PAGE(sc, PKTPG_RX_CFG) | RX_CFG_SKIP);
		return;
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0) {
		aprint_error_dev(sc->sc_dev,
		    "cs_process_receive: unable to allocate mbuf\n");
		ifp->if_ierrors++;
		/*
		 * couldn't allocate an mbuf so things are not good, may as
		 * well drop the packet I think.
		 *
		 * have already read the length so we should be right to skip
		 * the packet.
		 */
		CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG,
		    CS_READ_PACKET_PAGE(sc, PKTPG_RX_CFG) | RX_CFG_SKIP);
		return;
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;

	/* number of bytes to align ip header on word boundary for ipintr */
	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);

	/*
	 * alloc mbuf cluster if we need.
	 * we need 1 byte spare because following
	 * packet read loop can overrun.
	 */
	if (totlen + pad + 1 > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			/* couldn't allocate an mbuf cluster */
			aprint_error_dev(sc->sc_dev,
			    "cs_process_receive: "
			    "unable to allocate a cluster\n");
			m_freem(m);

			/* skip the received frame */
			CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG,
				CS_READ_PACKET_PAGE(sc, PKTPG_RX_CFG) | RX_CFG_SKIP);
			return;
		}
	}

	/* align ip header on word boundary for ipintr */
	m->m_data += pad;

	m->m_len = totlen;
	pBuff = mtod(m, u_int16_t *);

	/* now read the data from the chip */
	if (sc->sc_memorymode) {
		pBuffLimit = pBuff + (totlen + 1) / 2;	/* don't want to go over */
		while (pBuff < pBuffLimit) {
			*pBuff++ = CS_READ_PACKET_PAGE(sc, frameOffset);
			frameOffset += 2;
		}
	}
	else {
		IO_READ_MULTI_2(sc, PORT_RXTX_DATA, pBuff, (totlen + 1)>>1);
	}

	cs_ether_input(sc, m);
}

void
cs_process_rx_early(struct cs_softc *sc)
{
	struct ifnet *ifp;
	struct mbuf *m;
	u_int16_t frameCount, oldFrameCount;
	u_int16_t rxEvent;
	u_int16_t *pBuff;
	int pad;
	unsigned int frameOffset;


	ifp = &sc->sc_ethercom.ec_if;

	/* Initialize the frame offset */
	frameOffset = PKTPG_RX_FRAME;
	frameCount = 0;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0) {
		aprint_error_dev(sc->sc_dev,
		    "cs_process_rx_early: unable to allocate mbuf\n");
		ifp->if_ierrors++;
		/*
		 * couldn't allocate an mbuf so things are not good, may as
		 * well drop the packet I think.
		 *
		 * have already read the length so we should be right to skip
		 * the packet.
		 */
		CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG,
		    CS_READ_PACKET_PAGE(sc, PKTPG_RX_CFG) | RX_CFG_SKIP);
		return;
	}
	m->m_pkthdr.rcvif = ifp;
	/*
	 * save processing by always using a mbuf cluster, guaranteed to fit
	 * packet
	 */
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		/* couldn't allocate an mbuf cluster */
		aprint_error_dev(sc->sc_dev,
		    "cs_process_rx_early: unable to allocate a cluster\n");
		m_freem(m);
		/* skip the frame */
		CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG,
		    CS_READ_PACKET_PAGE(sc, PKTPG_RX_CFG) | RX_CFG_SKIP);
		return;
	}

	/* align ip header on word boundary for ipintr */
	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);
	m->m_data += pad;

	/* set up the buffer pointer to point to the data area */
	pBuff = mtod(m, u_int16_t *);

	/*
	 * now read the frame byte counter until we have finished reading the
	 * frame
	 */
	oldFrameCount = 0;
	frameCount = CS_READ_PACKET_PAGE(sc, PKTPG_FRAME_BYTE_COUNT);
	while ((frameCount != 0) && (frameCount < MCLBYTES)) {
		for (; oldFrameCount < frameCount; oldFrameCount += 2) {
			*pBuff++ = CS_READ_PACKET_PAGE(sc, frameOffset);
			frameOffset += 2;
		}

		/* read the new count from the chip */
		frameCount = CS_READ_PACKET_PAGE(sc, PKTPG_FRAME_BYTE_COUNT);
	}

	/* update the mbuf counts */
	m->m_len = oldFrameCount;
	m->m_pkthdr.len = oldFrameCount;

	/* now check the Rx Event register */
	rxEvent = CS_READ_PACKET_PAGE(sc, PKTPG_RX_EVENT);

	if ((rxEvent & RX_EVENT_RX_OK) != 0) {
		/*
		 * do an implied skip, it seems to be more reliable than a
		 * forced skip.
		 */
		rxEvent = CS_READ_PACKET_PAGE(sc, PKTPG_RX_STATUS);
		rxEvent = CS_READ_PACKET_PAGE(sc, PKTPG_RX_LENGTH);

		/*
		 * now read the RX_EVENT register to perform an implied skip.
		 */
		rxEvent = CS_READ_PACKET_PAGE(sc, PKTPG_RX_EVENT);

		cs_ether_input(sc, m);
	} else {
		m_freem(m);
		ifp->if_ierrors++;
	}
}

void
cs_start_output(struct ifnet *ifp)
{
	struct cs_softc *sc;
	struct mbuf *pMbuf;
	struct mbuf *pMbufChain;
	u_int16_t BusStatus;
	u_int16_t Length;
	int txLoop = 0;
	int dropout = 0;

	sc = ifp->if_softc;

	/* check that the interface is up and running */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING) {
		return;
	}

	/* Don't interrupt a transmission in progress */
	if (sc->sc_txbusy) {
		return;
	}

	/* this loop will only run through once if transmission is successful */
	/*
	 * While there are packets to transmit and a transmit is not in
	 * progress
	 */
	while (sc->sc_txbusy == 0 && dropout == 0) {
		IFQ_DEQUEUE(&ifp->if_snd, pMbufChain);
		if (pMbufChain == NULL)
			break;

		/*
	         * If BPF is listening on this interface, let it see the packet
	         * before we commit it to the wire.
	         */
		bpf_mtap(ifp, pMbufChain);

		/* Find the total length of the data to transmit */
		Length = 0;
		for (pMbuf = pMbufChain; pMbuf != NULL; pMbuf = pMbuf->m_next)
			Length += pMbuf->m_len;

		do {
			/*
			 * Request that the transmit be started after all
			 * data has been copied
			 *
			 * In IO mode must write to the IO port not the packet
			 * page address
			 *
			 * If this is changed to start transmission after a
			 * small amount of data has been copied you tend to
			 * get packet missed errors i think because the ISA
			 * bus is too slow. Or possibly the copy routine is
			 * not streamlined enough.
			 */
			if (sc->sc_memorymode) {
				CS_WRITE_PACKET_PAGE(sc, PKTPG_TX_CMD,
					cs_xmit_early_table[sc->sc_xe_ent].txcmd);
				CS_WRITE_PACKET_PAGE(sc, PKTPG_TX_LENGTH, Length);
			}
			else {
				CS_WRITE_PORT(sc, PORT_TX_CMD,
					cs_xmit_early_table[sc->sc_xe_ent].txcmd);
				CS_WRITE_PORT(sc, PORT_TX_LENGTH, Length);
			}

			/*
			 * Adjust early-transmit machinery.
			 */
			if (--sc->sc_xe_togo == 0) {
				sc->sc_xe_ent =
				    cs_xmit_early_table[sc->sc_xe_ent].better;
				sc->sc_xe_togo =
			    cs_xmit_early_table[sc->sc_xe_ent].better_count;
			}
			/*
			 * Read the BusStatus register which indicates
			 * success of the request
			 */
			BusStatus = CS_READ_PACKET_PAGE(sc, PKTPG_BUS_ST);

			/*
			 * If there was an error in the transmit bid free the
			 * mbuf and go on. This is presuming that mbuf is
			 * corrupt.
			 */
			if (BusStatus & BUS_ST_TX_BID_ERR) {
				aprint_error_dev(sc->sc_dev,
				    "transmit bid error (too big)");

				/* Discard the bad mbuf chain */
				m_freem(pMbufChain);
				sc->sc_ethercom.ec_if.if_oerrors++;

				/* Loop up to transmit the next chain */
				txLoop = 0;
			} else {
				if (BusStatus & BUS_ST_RDY4TXNOW) {
					/*
					 * The chip is ready for transmission
					 * now
					 */
					/*
					 * Copy the frame to the chip to
					 * start transmission
					 */
					cs_copy_tx_frame(sc, pMbufChain);

					/* Free the mbuf chain */
					m_freem(pMbufChain);

					/* Transmission is now in progress */
					sc->sc_txbusy = TRUE;
					txLoop = 0;
				} else {
					/*
					 * if we get here we want to try
					 * again with the same mbuf, until
					 * the chip lets us transmit.
					 */
					txLoop++;
					if (txLoop > CS_OUTPUT_LOOP_MAX) {
						/* Free the mbuf chain */
						m_freem(pMbufChain);
						/*
						 * Transmission is not in
						 * progress
						 */
						sc->sc_txbusy = FALSE;
						/*
						 * Increment the output error
						 * count
						 */
						ifp->if_oerrors++;
						/*
						 * exit the routine and drop
						 * the packet.
						 */
						txLoop = 0;
						dropout = 1;
					}
				}
			}
		} while (txLoop);
	}
}

void
cs_copy_tx_frame(struct cs_softc *sc, struct mbuf *m0)
{
	struct mbuf *m;
	int len, leftover, frameoff;
	u_int16_t dbuf;
	u_int8_t *p;
#ifdef DIAGNOSTIC
	u_int8_t *lim;
#endif

	/* Initialize frame pointer and data port address */
	frameoff = PKTPG_TX_FRAME;

	/* start out with no leftover data */
	leftover = 0;
	dbuf = 0;

	/* Process the chain of mbufs */
	for (m = m0; m != NULL; m = m->m_next) {
		/*
		 * Process all of the data in a single mbuf.
		 */
		p = mtod(m, u_int8_t *);
		len = m->m_len;
#ifdef DIAGNOSTIC
		lim = p + len;
#endif

		while (len > 0) {
			if (leftover) {
				/*
				 * Data left over (from mbuf or realignment).
				 * Buffer the next byte, and write it and
				 * the leftover data out.
				 */
				dbuf |= *p++ << 8;
				len--;
				if (sc->sc_memorymode) {
					CS_WRITE_PACKET_PAGE(sc, frameoff, dbuf);
					frameoff += 2;
				}
				else {
					CS_WRITE_PORT(sc, PORT_RXTX_DATA, dbuf);
				}
				leftover = 0;
			} else if ((long) p & 1) {
				/*
				 * Misaligned data.  Buffer the next byte.
				 */
				dbuf = *p++;
				len--;
				leftover = 1;
			} else {
				/*
				 * Aligned data.  This is the case we like.
				 *
				 * Write-region out as much as we can, then
				 * buffer the remaining byte (if any).
				 */
				leftover = len & 1;
				len &= ~1;
				if (sc->sc_memorymode) {
					MEM_WRITE_REGION_2(sc, frameoff,
						(u_int16_t *) p, len >> 1);
					frameoff += len;
				}
				else {
					IO_WRITE_MULTI_2(sc,
						PORT_RXTX_DATA, (u_int16_t *)p, len >> 1);
				}
				p += len;

				if (leftover)
					dbuf = *p++;
				len = 0;
			}
		}
		if (len < 0)
			panic("cs_copy_tx_frame: negative len");
#ifdef DIAGNOSTIC
		if (p != lim)
			panic("cs_copy_tx_frame: p != lim");
#endif
	}
	if (leftover) {
		if (sc->sc_memorymode) {
			CS_WRITE_PACKET_PAGE(sc, frameoff, dbuf);
		}
		else {
			CS_WRITE_PORT(sc, PORT_RXTX_DATA, dbuf);
		}
	}
}

static int
cs_enable(struct cs_softc *sc)
{

	if (CS_IS_ENABLED(sc) == 0) {
		if (sc->sc_enable != NULL) {
			int error;

			error = (*sc->sc_enable)(sc);
			if (error)
				return (error);
		}
		sc->sc_cfgflags |= CFGFLG_ENABLED;
	}

	return (0);
}

static void
cs_disable(struct cs_softc *sc)
{

	if (CS_IS_ENABLED(sc)) {
		if (sc->sc_disable != NULL)
			(*sc->sc_disable)(sc);

		sc->sc_cfgflags &= ~CFGFLG_ENABLED;
	}
}

static void
cs_stop(struct ifnet *ifp, int disable)
{
	struct cs_softc *sc = ifp->if_softc;

	CS_WRITE_PACKET_PAGE(sc, PKTPG_RX_CFG, 0);
	CS_WRITE_PACKET_PAGE(sc, PKTPG_TX_CFG, 0);
	CS_WRITE_PACKET_PAGE(sc, PKTPG_BUF_CFG, 0);
	CS_WRITE_PACKET_PAGE(sc, PKTPG_BUS_CTL, 0);

	if (disable) {
		cs_disable(sc);
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

int
cs_activate(device_t self, enum devact act)
{
	struct cs_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ethercom.ec_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}
