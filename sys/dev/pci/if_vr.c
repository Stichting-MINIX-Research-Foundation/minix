/*	$NetBSD: if_vr.c,v 1.115 2015/04/13 16:33:25 riastradh Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$FreeBSD: if_vr.c,v 1.7 1999/01/10 18:51:49 wpaul Exp $
 */

/*
 * VIA Rhine fast ethernet PCI NIC driver
 *
 * Supports various network adapters based on the VIA Rhine
 * and Rhine II PCI controllers, including the D-Link DFE530TX.
 * Datasheets are available at http://www.via.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The VIA Rhine controllers are similar in some respects to the
 * the DEC tulip chips, except less complicated. The controller
 * uses an MII bus and an external physical layer interface. The
 * receiver has a one entry perfect filter and a 64-bit hash table
 * multicast filter. Transmit and receive descriptors are similar
 * to the tulip.
 *
 * The Rhine has a serious flaw in its transmit DMA mechanism:
 * transmit buffers must be longword aligned. Unfortunately,
 * the kernel doesn't guarantee that mbufs will be filled in starting
 * at longword boundaries, so we have to do a buffer copy before
 * transmission.
 *
 * Apparently, the receive DMA mechanism also has the same flaw.  This
 * means that on systems with struct alignment requirements, incoming
 * frames must be copied to a new buffer which shifts the data forward
 * 2 bytes so that the payload is aligned on a 4-byte boundary.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_vr.c,v 1.115 2015/04/13 16:33:25 riastradh Exp $");



#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/endian.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_vrreg.h>

#define	VR_USEIOSPACE

/*
 * Various supported device vendors/types and their names.
 */
static const struct vr_type {
	pci_vendor_id_t		vr_vid;
	pci_product_id_t	vr_did;
} vr_devs[] = {
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT3043 },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT6102 },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT6105 },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT6105M },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT86C100A }
};

/*
 * Transmit descriptor list size.
 */
#define	VR_NTXDESC		64
#define	VR_NTXDESC_MASK		(VR_NTXDESC - 1)
#define	VR_NEXTTX(x)		(((x) + 1) & VR_NTXDESC_MASK)

/*
 * Receive descriptor list size.
 */
#define	VR_NRXDESC		64
#define	VR_NRXDESC_MASK		(VR_NRXDESC - 1)
#define	VR_NEXTRX(x)		(((x) + 1) & VR_NRXDESC_MASK)

/*
 * Control data structres that are DMA'd to the Rhine chip.  We allocate
 * them in a single clump that maps to a single DMA segment to make several
 * things easier.
 *
 * Note that since we always copy outgoing packets to aligned transmit
 * buffers, we can reduce the transmit descriptors to one per packet.
 */
struct vr_control_data {
	struct vr_desc		vr_txdescs[VR_NTXDESC];
	struct vr_desc		vr_rxdescs[VR_NRXDESC];
};

#define	VR_CDOFF(x)		offsetof(struct vr_control_data, x)
#define	VR_CDTXOFF(x)		VR_CDOFF(vr_txdescs[(x)])
#define	VR_CDRXOFF(x)		VR_CDOFF(vr_rxdescs[(x)])

/*
 * Software state of transmit and receive descriptors.
 */
struct vr_descsoft {
	struct mbuf		*ds_mbuf;	/* head of mbuf chain */
	bus_dmamap_t		ds_dmamap;	/* our DMA map */
};

struct vr_softc {
	device_t		vr_dev;
	void			*vr_ih;		/* interrupt cookie */
	bus_space_tag_t		vr_bst;		/* bus space tag */
	bus_space_handle_t	vr_bsh;		/* bus space handle */
	bus_dma_tag_t		vr_dmat;	/* bus DMA tag */
	pci_chipset_tag_t	vr_pc;		/* PCI chipset info */
	pcitag_t		vr_tag;		/* PCI tag */
	struct ethercom		vr_ec;		/* Ethernet common info */
	uint8_t 		vr_enaddr[ETHER_ADDR_LEN];
	struct mii_data		vr_mii;		/* MII/media info */

	pcireg_t		vr_id;		/* vendor/product ID */
	uint8_t			vr_revid;	/* Rhine chip revision */

	callout_t		vr_tick_ch;	/* tick callout */

	bus_dmamap_t		vr_cddmamap;	/* control data DMA map */
#define	vr_cddma	vr_cddmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct vr_descsoft	vr_txsoft[VR_NTXDESC];
	struct vr_descsoft	vr_rxsoft[VR_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct vr_control_data	*vr_control_data;

	int	vr_txpending;		/* number of TX requests pending */
	int	vr_txdirty;		/* first dirty TX descriptor */
	int	vr_txlast;		/* last used TX descriptor */

	int	vr_rxptr;		/* next ready RX descriptor */

	uint32_t	vr_save_iobase;
	uint32_t	vr_save_membase;
	uint32_t	vr_save_irq;

	bool		vr_link;
	int		vr_flags;
#define VR_F_RESTART	0x1		/* restart on next tick */
	int		vr_if_flags;

	krndsource_t rnd_source;	/* random source */
};

#define	VR_CDTXADDR(sc, x)	((sc)->vr_cddma + VR_CDTXOFF((x)))
#define	VR_CDRXADDR(sc, x)	((sc)->vr_cddma + VR_CDRXOFF((x)))

#define	VR_CDTX(sc, x)		(&(sc)->vr_control_data->vr_txdescs[(x)])
#define	VR_CDRX(sc, x)		(&(sc)->vr_control_data->vr_rxdescs[(x)])

#define	VR_DSTX(sc, x)		(&(sc)->vr_txsoft[(x)])
#define	VR_DSRX(sc, x)		(&(sc)->vr_rxsoft[(x)])

#define	VR_CDTXSYNC(sc, x, ops)						\
	bus_dmamap_sync((sc)->vr_dmat, (sc)->vr_cddmamap,		\
	    VR_CDTXOFF((x)), sizeof(struct vr_desc), (ops))

#define	VR_CDRXSYNC(sc, x, ops)						\
	bus_dmamap_sync((sc)->vr_dmat, (sc)->vr_cddmamap,		\
	    VR_CDRXOFF((x)), sizeof(struct vr_desc), (ops))

/*
 * Note we rely on MCLBYTES being a power of two below.
 */
#define	VR_INIT_RXDESC(sc, i)						\
do {									\
	struct vr_desc *__d = VR_CDRX((sc), (i));			\
	struct vr_descsoft *__ds = VR_DSRX((sc), (i));			\
									\
	__d->vr_next = htole32(VR_CDRXADDR((sc), VR_NEXTRX((i))));	\
	__d->vr_data = htole32(__ds->ds_dmamap->dm_segs[0].ds_addr);	\
	__d->vr_ctl = htole32(VR_RXCTL_CHAIN | VR_RXCTL_RX_INTR |	\
	    ((MCLBYTES - 1) & VR_RXCTL_BUFLEN));			\
	__d->vr_status = htole32(VR_RXSTAT_FIRSTFRAG |			\
	    VR_RXSTAT_LASTFRAG | VR_RXSTAT_OWN);			\
	VR_CDRXSYNC((sc), (i), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
} while (/* CONSTCOND */ 0)

/*
 * register space access macros
 */
#define	CSR_WRITE_4(sc, reg, val)					\
	bus_space_write_4(sc->vr_bst, sc->vr_bsh, reg, val)
#define	CSR_WRITE_2(sc, reg, val)					\
	bus_space_write_2(sc->vr_bst, sc->vr_bsh, reg, val)
#define	CSR_WRITE_1(sc, reg, val)					\
	bus_space_write_1(sc->vr_bst, sc->vr_bsh, reg, val)

#define	CSR_READ_4(sc, reg)						\
	bus_space_read_4(sc->vr_bst, sc->vr_bsh, reg)
#define	CSR_READ_2(sc, reg)						\
	bus_space_read_2(sc->vr_bst, sc->vr_bsh, reg)
#define	CSR_READ_1(sc, reg)						\
	bus_space_read_1(sc->vr_bst, sc->vr_bsh, reg)

#define	VR_TIMEOUT		1000

static int	vr_add_rxbuf(struct vr_softc *, int);

static void	vr_rxeof(struct vr_softc *);
static void	vr_rxeoc(struct vr_softc *);
static void	vr_txeof(struct vr_softc *);
static int	vr_intr(void *);
static void	vr_start(struct ifnet *);
static int	vr_ioctl(struct ifnet *, u_long, void *);
static int	vr_init(struct ifnet *);
static void	vr_stop(struct ifnet *, int);
static void	vr_rxdrain(struct vr_softc *);
static void	vr_watchdog(struct ifnet *);
static void	vr_tick(void *);

static int	vr_mii_readreg(device_t, int, int);
static void	vr_mii_writereg(device_t, int, int, int);
static void	vr_mii_statchg(struct ifnet *);

static void	vr_setmulti(struct vr_softc *);
static void	vr_reset(struct vr_softc *);
static int	vr_restore_state(pci_chipset_tag_t, pcitag_t, device_t,
    pcireg_t);
static bool	vr_resume(device_t, const pmf_qual_t *);

int	vr_copy_small = 0;

#define	VR_SETBIT(sc, reg, x)				\
	CSR_WRITE_1(sc, reg,				\
	    CSR_READ_1(sc, reg) | (x))

#define	VR_CLRBIT(sc, reg, x)				\
	CSR_WRITE_1(sc, reg,				\
	    CSR_READ_1(sc, reg) & ~(x))

#define	VR_SETBIT16(sc, reg, x)				\
	CSR_WRITE_2(sc, reg,				\
	    CSR_READ_2(sc, reg) | (x))

#define	VR_CLRBIT16(sc, reg, x)				\
	CSR_WRITE_2(sc, reg,				\
	    CSR_READ_2(sc, reg) & ~(x))

#define	VR_SETBIT32(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
	    CSR_READ_4(sc, reg) | (x))

#define	VR_CLRBIT32(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
	    CSR_READ_4(sc, reg) & ~(x))

/*
 * MII bit-bang glue.
 */
static uint32_t vr_mii_bitbang_read(device_t);
static void	vr_mii_bitbang_write(device_t, uint32_t);

static const struct mii_bitbang_ops vr_mii_bitbang_ops = {
	vr_mii_bitbang_read,
	vr_mii_bitbang_write,
	{
		VR_MIICMD_DATAOUT,	/* MII_BIT_MDO */
		VR_MIICMD_DATAIN,	/* MII_BIT_MDI */
		VR_MIICMD_CLK,		/* MII_BIT_MDC */
		VR_MIICMD_DIR,		/* MII_BIT_DIR_HOST_PHY */
		0,			/* MII_BIT_DIR_PHY_HOST */
	}
};

static uint32_t
vr_mii_bitbang_read(device_t self)
{
	struct vr_softc *sc = device_private(self);

	return (CSR_READ_1(sc, VR_MIICMD));
}

static void
vr_mii_bitbang_write(device_t self, uint32_t val)
{
	struct vr_softc *sc = device_private(self);

	CSR_WRITE_1(sc, VR_MIICMD, (val & 0xff) | VR_MIICMD_DIRECTPGM);
}

/*
 * Read an PHY register through the MII.
 */
static int
vr_mii_readreg(device_t self, int phy, int reg)
{
	struct vr_softc *sc = device_private(self);

	CSR_WRITE_1(sc, VR_MIICMD, VR_MIICMD_DIRECTPGM);
	return (mii_bitbang_readreg(self, &vr_mii_bitbang_ops, phy, reg));
}

/*
 * Write to a PHY register through the MII.
 */
static void
vr_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct vr_softc *sc = device_private(self);

	CSR_WRITE_1(sc, VR_MIICMD, VR_MIICMD_DIRECTPGM);
	mii_bitbang_writereg(self, &vr_mii_bitbang_ops, phy, reg, val);
}

static void
vr_mii_statchg(struct ifnet *ifp)
{
	struct vr_softc *sc = ifp->if_softc;
	int i;

	/*
	 * In order to fiddle with the 'full-duplex' bit in the netconfig
	 * register, we first have to put the transmit and/or receive logic
	 * in the idle state.
	 */
	if ((sc->vr_mii.mii_media_status & IFM_ACTIVE) &&
	    IFM_SUBTYPE(sc->vr_mii.mii_media_active) != IFM_NONE) {
		sc->vr_link = true;

		if (CSR_READ_2(sc, VR_COMMAND) & (VR_CMD_TX_ON|VR_CMD_RX_ON))
			VR_CLRBIT16(sc, VR_COMMAND,
			    (VR_CMD_TX_ON|VR_CMD_RX_ON));

		if (sc->vr_mii.mii_media_active & IFM_FDX)
			VR_SETBIT16(sc, VR_COMMAND, VR_CMD_FULLDUPLEX);
		else
			VR_CLRBIT16(sc, VR_COMMAND, VR_CMD_FULLDUPLEX);

		VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_ON|VR_CMD_RX_ON);
	} else {
		sc->vr_link = false;
		VR_CLRBIT16(sc, VR_COMMAND, VR_CMD_TX_ON|VR_CMD_RX_ON);
		for (i = VR_TIMEOUT; i > 0; i--) {
			delay(10);
			if (!(CSR_READ_2(sc, VR_COMMAND) &
			    (VR_CMD_TX_ON|VR_CMD_RX_ON)))
				break;
		}
		if (i == 0) {
#ifdef VR_DEBUG
			printf("%s: rx shutdown error!\n",
			    device_xname(sc->vr_dev));
#endif
			sc->vr_flags |= VR_F_RESTART;
		}
	}
}

#define	vr_calchash(addr) \
	(ether_crc32_be((addr), ETHER_ADDR_LEN) >> 26)

/*
 * Program the 64-bit multicast hash filter.
 */
static void
vr_setmulti(struct vr_softc *sc)
{
	struct ifnet *ifp;
	int h = 0;
	uint32_t hashes[2] = { 0, 0 };
	struct ether_multistep step;
	struct ether_multi *enm;
	int mcnt = 0;
	uint8_t rxfilt;

	ifp = &sc->vr_ec.ec_if;

	rxfilt = CSR_READ_1(sc, VR_RXCFG);

	if (ifp->if_flags & IFF_PROMISC) {
allmulti:
		ifp->if_flags |= IFF_ALLMULTI;
		rxfilt |= VR_RXCFG_RX_MULTI;
		CSR_WRITE_1(sc, VR_RXCFG, rxfilt);
		CSR_WRITE_4(sc, VR_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, VR_MAR1, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, VR_MAR0, 0);
	CSR_WRITE_4(sc, VR_MAR1, 0);

	/* now program new ones */
	ETHER_FIRST_MULTI(step, &sc->vr_ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = vr_calchash(enm->enm_addrlo);

		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		ETHER_NEXT_MULTI(step, enm);
		mcnt++;
	}

	ifp->if_flags &= ~IFF_ALLMULTI;

	if (mcnt)
		rxfilt |= VR_RXCFG_RX_MULTI;
	else
		rxfilt &= ~VR_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, VR_MAR0, hashes[0]);
	CSR_WRITE_4(sc, VR_MAR1, hashes[1]);
	CSR_WRITE_1(sc, VR_RXCFG, rxfilt);
}

static void
vr_reset(struct vr_softc *sc)
{
	int i;

	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RESET);

	for (i = 0; i < VR_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_2(sc, VR_COMMAND) & VR_CMD_RESET))
			break;
	}
	if (i == VR_TIMEOUT) {
		if (sc->vr_revid < REV_ID_VT3065_A) {
			printf("%s: reset never completed!\n",
			    device_xname(sc->vr_dev));
		} else {
			/* Use newer force reset command */
			printf("%s: using force reset command.\n",
			    device_xname(sc->vr_dev));
			VR_SETBIT(sc, VR_MISC_CR1, VR_MISCCR1_FORSRST);
		}
	}

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 * Note: the length fields are only 11 bits wide, which means the
 * largest size we can specify is 2047. This is important because
 * MCLBYTES is 2048, so we have to subtract one otherwise we'll
 * overflow the field and make a mess.
 */
static int
vr_add_rxbuf(struct vr_softc *sc, int i)
{
	struct vr_descsoft *ds = VR_DSRX(sc, i);
	struct mbuf *m_new;
	int error;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL)
		return (ENOBUFS);

	MCLGET(m_new, M_DONTWAIT);
	if ((m_new->m_flags & M_EXT) == 0) {
		m_freem(m_new);
		return (ENOBUFS);
	}

	if (ds->ds_mbuf != NULL)
		bus_dmamap_unload(sc->vr_dmat, ds->ds_dmamap);

	ds->ds_mbuf = m_new;

	error = bus_dmamap_load(sc->vr_dmat, ds->ds_dmamap,
	    m_new->m_ext.ext_buf, m_new->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		aprint_error_dev(sc->vr_dev, "unable to load rx DMA map %d, error = %d\n",
		    i, error);
		panic("vr_add_rxbuf");		/* XXX */
	}

	bus_dmamap_sync(sc->vr_dmat, ds->ds_dmamap, 0,
	    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	VR_INIT_RXDESC(sc, i);

	return (0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
vr_rxeof(struct vr_softc *sc)
{
	struct mbuf *m;
	struct ifnet *ifp;
	struct vr_desc *d;
	struct vr_descsoft *ds;
	int i, total_len;
	uint32_t rxstat;

	ifp = &sc->vr_ec.ec_if;

	for (i = sc->vr_rxptr;; i = VR_NEXTRX(i)) {
		d = VR_CDRX(sc, i);
		ds = VR_DSRX(sc, i);

		VR_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		rxstat = le32toh(d->vr_status);

		if (rxstat & VR_RXSTAT_OWN) {
			/*
			 * We have processed all of the receive buffers.
			 */
			break;
		}

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
		 * comes up in the ring.
		 */
		if (rxstat & VR_RXSTAT_RXERR) {
			const char *errstr;

			ifp->if_ierrors++;
			switch (rxstat & 0x000000FF) {
			case VR_RXSTAT_CRCERR:
				errstr = "crc error";
				break;
			case VR_RXSTAT_FRAMEALIGNERR:
				errstr = "frame alignment error";
				break;
			case VR_RXSTAT_FIFOOFLOW:
				errstr = "FIFO overflow";
				break;
			case VR_RXSTAT_GIANT:
				errstr = "received giant packet";
				break;
			case VR_RXSTAT_RUNT:
				errstr = "received runt packet";
				break;
			case VR_RXSTAT_BUSERR:
				errstr = "system bus error";
				break;
			case VR_RXSTAT_BUFFERR:
				errstr = "rx buffer error";
				break;
			default:
				errstr = "unknown rx error";
				break;
			}
			printf("%s: receive error: %s\n", device_xname(sc->vr_dev),
			    errstr);

			VR_INIT_RXDESC(sc, i);

			continue;
		} else if (!(rxstat & VR_RXSTAT_FIRSTFRAG) ||
		           !(rxstat & VR_RXSTAT_LASTFRAG)) {
			/*
			 * This driver expects to receive whole packets every
			 * time.  In case we receive a fragment that is not
			 * a complete packet, we discard it.
			 */
			ifp->if_ierrors++;

			printf("%s: receive error: incomplete frame; "
			       "size = %d, status = 0x%x\n",
			       device_xname(sc->vr_dev),
			       VR_RXBYTES(le32toh(d->vr_status)), rxstat);

			VR_INIT_RXDESC(sc, i);

			continue;
		}

		bus_dmamap_sync(sc->vr_dmat, ds->ds_dmamap, 0,
		    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		/* No errors; receive the packet. */
		total_len = VR_RXBYTES(le32toh(d->vr_status));
#ifdef DIAGNOSTIC
		if (total_len == 0) {
			/*
			 * If we receive a zero-length packet, we probably
			 * missed to handle an error condition above.
			 * Discard it to avoid a later crash.
			 */
			ifp->if_ierrors++;

			printf("%s: receive error: zero-length packet; "
			       "status = 0x%x\n",
			       device_xname(sc->vr_dev), rxstat);

			VR_INIT_RXDESC(sc, i);

			continue;
		}
#endif

		/*
		 * The Rhine chip includes the CRC with every packet.
		 * Trim it off here.
		 */
		total_len -= ETHER_CRC_LEN;

#ifdef __NO_STRICT_ALIGNMENT
		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when we receive lots
		 * of small packets.
		 *
		 * Otherwise, we add a new buffer to the receive
		 * chain.  If this fails, we drop the packet and
		 * recycle the old buffer.
		 */
		if (vr_copy_small != 0 && total_len <= MHLEN) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			memcpy(mtod(m, void *),
			    mtod(ds->ds_mbuf, void *), total_len);
			VR_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->vr_dmat, ds->ds_dmamap, 0,
			    ds->ds_dmamap->dm_mapsize,
			    BUS_DMASYNC_PREREAD);
		} else {
			m = ds->ds_mbuf;
			if (vr_add_rxbuf(sc, i) == ENOBUFS) {
 dropit:
				ifp->if_ierrors++;
				VR_INIT_RXDESC(sc, i);
				bus_dmamap_sync(sc->vr_dmat,
				    ds->ds_dmamap, 0,
				    ds->ds_dmamap->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}
#else
		/*
		 * The Rhine's packet buffers must be 4-byte aligned.
		 * But this means that the data after the Ethernet header
		 * is misaligned.  We must allocate a new buffer and
		 * copy the data, shifted forward 2 bytes.
		 */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
 dropit:
			ifp->if_ierrors++;
			VR_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->vr_dmat, ds->ds_dmamap, 0,
			    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			continue;
		}
		if (total_len > (MHLEN - 2)) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				goto dropit;
			}
		}
		m->m_data += 2;

		/*
		 * Note that we use clusters for incoming frames, so the
		 * buffer is virtually contiguous.
		 */
		memcpy(mtod(m, void *), mtod(ds->ds_mbuf, void *),
		    total_len);

		/* Allow the receive descriptor to continue using its mbuf. */
		VR_INIT_RXDESC(sc, i);
		bus_dmamap_sync(sc->vr_dmat, ds->ds_dmamap, 0,
		    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
#endif /* __NO_STRICT_ALIGNMENT */

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = total_len;
		/*
		 * Handle BPF listeners. Let the BPF user see the packet, but
		 * don't pass it up to the ether_input() layer unless it's
		 * a broadcast packet, multicast packet, matches our ethernet
		 * address or the interface is in promiscuous mode.
		 */
		bpf_mtap(ifp, m);
		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->vr_rxptr = i;
}

void
vr_rxeoc(struct vr_softc *sc)
{
	struct ifnet *ifp;
	int i;

	ifp = &sc->vr_ec.ec_if;

	ifp->if_ierrors++;

	VR_CLRBIT16(sc, VR_COMMAND, VR_CMD_RX_ON);
	for (i = 0; i < VR_TIMEOUT; i++) {
		DELAY(10);
		if ((CSR_READ_2(sc, VR_COMMAND) & VR_CMD_RX_ON) == 0)
			break;
	}
	if (i == VR_TIMEOUT) {
		/* XXX need reset? */
		printf("%s: RX shutdown never complete\n",
		    device_xname(sc->vr_dev));
	}

	vr_rxeof(sc);

	CSR_WRITE_4(sc, VR_RXADDR, VR_CDRXADDR(sc, sc->vr_rxptr));
	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RX_ON);
	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RX_GO);
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void
vr_txeof(struct vr_softc *sc)
{
	struct ifnet *ifp = &sc->vr_ec.ec_if;
	struct vr_desc *d;
	struct vr_descsoft *ds;
	uint32_t txstat;
	int i, j;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	for (i = sc->vr_txdirty; sc->vr_txpending != 0;
	     i = VR_NEXTTX(i), sc->vr_txpending--) {
		d = VR_CDTX(sc, i);
		ds = VR_DSTX(sc, i);

		VR_CDTXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		txstat = le32toh(d->vr_status);

		if (txstat & (VR_TXSTAT_ABRT | VR_TXSTAT_UDF)) {
			VR_CLRBIT16(sc, VR_COMMAND, VR_CMD_TX_ON);
			for (j = 0; j < VR_TIMEOUT; j++) {
				DELAY(10);
				if ((CSR_READ_2(sc, VR_COMMAND) &
				    VR_CMD_TX_ON) == 0)
					break;
			}
			if (j == VR_TIMEOUT) {
				/* XXX need reset? */
				printf("%s: TX shutdown never complete\n",
				    device_xname(sc->vr_dev));
			}
			d->vr_status = htole32(VR_TXSTAT_OWN);
			CSR_WRITE_4(sc, VR_TXADDR, VR_CDTXADDR(sc, i));
			break;
		}

		if (txstat & VR_TXSTAT_OWN)
			break;

		bus_dmamap_sync(sc->vr_dmat, ds->ds_dmamap,
		    0, ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->vr_dmat, ds->ds_dmamap);
		m_freem(ds->ds_mbuf);
		ds->ds_mbuf = NULL;

		if (txstat & VR_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & VR_TXSTAT_DEFER)
				ifp->if_collisions++;
			if (txstat & VR_TXSTAT_LATECOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions += (txstat & VR_TXSTAT_COLLCNT) >> 3;
		ifp->if_opackets++;
	}

	/* Update the dirty transmit buffer pointer. */
	sc->vr_txdirty = i;

	/*
	 * Cancel the watchdog timer if there are no pending
	 * transmissions.
	 */
	if (sc->vr_txpending == 0)
		ifp->if_timer = 0;
}

static int
vr_intr(void *arg)
{
	struct vr_softc *sc;
	struct ifnet *ifp;
	uint16_t status;
	int handled = 0, dotx = 0;

	sc = arg;
	ifp = &sc->vr_ec.ec_if;

	/* Suppress unwanted interrupts. */
	if ((ifp->if_flags & IFF_UP) == 0) {
		vr_stop(ifp, 1);
		return (0);
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, VR_IMR, 0x0000);

	for (;;) {
		status = CSR_READ_2(sc, VR_ISR);
		if (status)
			CSR_WRITE_2(sc, VR_ISR, status);

		if ((status & VR_INTRS) == 0)
			break;

		handled = 1;

		rnd_add_uint32(&sc->rnd_source, status);

		if (status & VR_ISR_RX_OK)
			vr_rxeof(sc);

		if (status & VR_ISR_RX_DROPPED) {
			printf("%s: rx packet lost\n", device_xname(sc->vr_dev));
			ifp->if_ierrors++;
		}

		if (status &
		    (VR_ISR_RX_ERR | VR_ISR_RX_NOBUF | VR_ISR_RX_OFLOW))
			vr_rxeoc(sc);


		if (status & (VR_ISR_BUSERR | VR_ISR_TX_UNDERRUN)) {
			if (status & VR_ISR_BUSERR)
				printf("%s: PCI bus error\n",
				    device_xname(sc->vr_dev));
			if (status & VR_ISR_TX_UNDERRUN)
				printf("%s: transmit underrun\n",
				    device_xname(sc->vr_dev));
			/* vr_init() calls vr_start() */
			dotx = 0;
			(void)vr_init(ifp);

		}

		if (status & VR_ISR_TX_OK) {
			dotx = 1;
			vr_txeof(sc);
		}

		if (status &
		    (VR_ISR_TX_ABRT | VR_ISR_TX_ABRT2 | VR_ISR_TX_UDFI)) {
			if (status & (VR_ISR_TX_ABRT | VR_ISR_TX_ABRT2))
				printf("%s: transmit aborted\n",
				    device_xname(sc->vr_dev));
			if (status & VR_ISR_TX_UDFI)
				printf("%s: transmit underflow\n",
				    device_xname(sc->vr_dev));
			ifp->if_oerrors++;
			dotx = 1;
			vr_txeof(sc);
			if (sc->vr_txpending) {
				VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_ON);
				VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_GO);
			}
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, VR_IMR, VR_INTRS);

	if (dotx)
		vr_start(ifp);

	return (handled);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */
static void
vr_start(struct ifnet *ifp)
{
	struct vr_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct vr_desc *d;
	struct vr_descsoft *ds;
	int error, firsttx, nexttx, opending;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;
	if (sc->vr_link == false)
		return;

	/*
	 * Remember the previous txpending and the first transmit
	 * descriptor we use.
	 */
	opending = sc->vr_txpending;
	firsttx = VR_NEXTTX(sc->vr_txlast);

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while (sc->vr_txpending < VR_NTXDESC) {
		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		m = NULL;

		/*
		 * Get the next available transmit descriptor.
		 */
		nexttx = VR_NEXTTX(sc->vr_txlast);
		d = VR_CDTX(sc, nexttx);
		ds = VR_DSTX(sc, nexttx);

		/*
		 * Load the DMA map.  If this fails, the packet didn't
		 * fit in one DMA segment, and we need to copy.  Note,
		 * the packet must also be aligned.
		 * if the packet is too small, copy it too, so we're sure
		 * we have enough room for the pad buffer.
		 */
		if ((mtod(m0, uintptr_t) & 3) != 0 ||
		    m0->m_pkthdr.len < VR_MIN_FRAMELEN ||
		    bus_dmamap_load_mbuf(sc->vr_dmat, ds->ds_dmamap, m0,
		     BUS_DMA_WRITE|BUS_DMA_NOWAIT) != 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: unable to allocate Tx mbuf\n",
				    device_xname(sc->vr_dev));
				break;
			}
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					printf("%s: unable to allocate Tx "
					    "cluster\n", device_xname(sc->vr_dev));
					m_freem(m);
					break;
				}
			}
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, void *));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			/*
			 * The Rhine doesn't auto-pad, so we have to do this
			 * ourselves.
			 */
			if (m0->m_pkthdr.len < VR_MIN_FRAMELEN) {
				memset(mtod(m, char *) + m0->m_pkthdr.len,
				    0, VR_MIN_FRAMELEN - m0->m_pkthdr.len);
				m->m_pkthdr.len = m->m_len = VR_MIN_FRAMELEN;
			}
			error = bus_dmamap_load_mbuf(sc->vr_dmat,
			    ds->ds_dmamap, m, BUS_DMA_WRITE|BUS_DMA_NOWAIT);
			if (error) {
				m_freem(m);
				printf("%s: unable to load Tx buffer, "
				    "error = %d\n", device_xname(sc->vr_dev), error);
				break;
			}
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m != NULL) {
			m_freem(m0);
			m0 = m;
		}

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->vr_dmat, ds->ds_dmamap, 0,
		    ds->ds_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);

		/*
		 * Store a pointer to the packet so we can free it later.
		 */
		ds->ds_mbuf = m0;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		bpf_mtap(ifp, m0);

		/*
		 * Fill in the transmit descriptor.
		 */
		d->vr_data = htole32(ds->ds_dmamap->dm_segs[0].ds_addr);
		d->vr_ctl = htole32(m0->m_pkthdr.len);
		d->vr_ctl |= htole32(VR_TXCTL_FIRSTFRAG | VR_TXCTL_LASTFRAG);

		/*
		 * If this is the first descriptor we're enqueuing,
		 * don't give it to the Rhine yet.  That could cause
		 * a race condition.  We'll do it below.
		 */
		if (nexttx == firsttx)
			d->vr_status = 0;
		else
			d->vr_status = htole32(VR_TXSTAT_OWN);

		VR_CDTXSYNC(sc, nexttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Advance the tx pointer. */
		sc->vr_txpending++;
		sc->vr_txlast = nexttx;
	}

	if (sc->vr_txpending == VR_NTXDESC) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->vr_txpending != opending) {
		/*
		 * We enqueued packets.  If the transmitter was idle,
		 * reset the txdirty pointer.
		 */
		if (opending == 0)
			sc->vr_txdirty = firsttx;

		/*
		 * Cause a transmit interrupt to happen on the
		 * last packet we enqueued.
		 */
		VR_CDTX(sc, sc->vr_txlast)->vr_ctl |= htole32(VR_TXCTL_FINT);
		VR_CDTXSYNC(sc, sc->vr_txlast,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * The entire packet chain is set up.  Give the
		 * first descriptor to the Rhine now.
		 */
		VR_CDTX(sc, firsttx)->vr_status = htole32(VR_TXSTAT_OWN);
		VR_CDTXSYNC(sc, firsttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Start the transmitter. */
		VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_GO);

		/* Set the watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * Initialize the interface.  Must be called at splnet.
 */
static int
vr_init(struct ifnet *ifp)
{
	struct vr_softc *sc = ifp->if_softc;
	struct vr_desc *d;
	struct vr_descsoft *ds;
	int i, error = 0;

	/* Cancel pending I/O. */
	vr_stop(ifp, 0);

	/* Reset the Rhine to a known state. */
	vr_reset(sc);

	/* set DMA length in BCR0 and BCR1 */
	VR_CLRBIT(sc, VR_BCR0, VR_BCR0_DMA_LENGTH);
	VR_SETBIT(sc, VR_BCR0, VR_BCR0_DMA_STORENFWD);

	VR_CLRBIT(sc, VR_BCR0, VR_BCR0_RX_THRESH);
	VR_SETBIT(sc, VR_BCR0, VR_BCR0_RXTH_128BYTES);

	VR_CLRBIT(sc, VR_BCR1, VR_BCR1_TX_THRESH);
	VR_SETBIT(sc, VR_BCR1, VR_BCR1_TXTH_STORENFWD);

	/* set DMA threshold length in RXCFG and TXCFG */
	VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_THRESH);
	VR_SETBIT(sc, VR_RXCFG, VR_RXTHRESH_128BYTES);

	VR_CLRBIT(sc, VR_TXCFG, VR_TXCFG_TX_THRESH);
	VR_SETBIT(sc, VR_TXCFG, VR_TXTHRESH_STORENFWD);

	/*
	 * Initialize the transmit descriptor ring.  txlast is initialized
	 * to the end of the list so that it will wrap around to the first
	 * descriptor when the first packet is transmitted.
	 */
	for (i = 0; i < VR_NTXDESC; i++) {
		d = VR_CDTX(sc, i);
		memset(d, 0, sizeof(struct vr_desc));
		d->vr_next = htole32(VR_CDTXADDR(sc, VR_NEXTTX(i)));
		VR_CDTXSYNC(sc, i, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}
	sc->vr_txpending = 0;
	sc->vr_txdirty = 0;
	sc->vr_txlast = VR_NTXDESC - 1;

	/*
	 * Initialize the receive descriptor ring.
	 */
	for (i = 0; i < VR_NRXDESC; i++) {
		ds = VR_DSRX(sc, i);
		if (ds->ds_mbuf == NULL) {
			if ((error = vr_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    device_xname(sc->vr_dev), i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				vr_rxdrain(sc);
				goto out;
			}
		} else
			VR_INIT_RXDESC(sc, i);
	}
	sc->vr_rxptr = 0;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		VR_SETBIT(sc, VR_RXCFG, VR_RXCFG_RX_PROMISC);
	else
		VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_PROMISC);

	/* Set capture broadcast bit to capture broadcast frames. */
	if (ifp->if_flags & IFF_BROADCAST)
		VR_SETBIT(sc, VR_RXCFG, VR_RXCFG_RX_BROAD);
	else
		VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_BROAD);

	/* Program the multicast filter, if necessary. */
	vr_setmulti(sc);

	/* Give the transmit and receive rings to the Rhine. */
	CSR_WRITE_4(sc, VR_RXADDR, VR_CDRXADDR(sc, sc->vr_rxptr));
	CSR_WRITE_4(sc, VR_TXADDR, VR_CDTXADDR(sc, VR_NEXTTX(sc->vr_txlast)));

	/* Set current media. */
	sc->vr_link = true;
	if ((error = ether_mediachange(ifp)) != 0)
		goto out;

	/* Enable receiver and transmitter. */
	CSR_WRITE_2(sc, VR_COMMAND, VR_CMD_TX_NOPOLL|VR_CMD_START|
				    VR_CMD_TX_ON|VR_CMD_RX_ON|
				    VR_CMD_RX_GO);

	/* Enable interrupts. */
	CSR_WRITE_2(sc, VR_ISR, 0xFFFF);
	CSR_WRITE_2(sc, VR_IMR, VR_INTRS);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Start one second timer. */
	callout_reset(&sc->vr_tick_ch, hz, vr_tick, sc);

	/* Attempt to start output on the interface. */
	vr_start(ifp);

 out:
	if (error)
		printf("%s: interface not running\n", device_xname(sc->vr_dev));
	return (error);
}

static int
vr_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct vr_softc *sc = ifp->if_softc;
	int s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, command, data)) != 0)
			break;

		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_RUNNING:
			vr_stop(ifp, 1);
			break;
		case IFF_UP:
			vr_init(ifp);
			break;
		case IFF_UP | IFF_RUNNING:
			if ((ifp->if_flags ^ sc->vr_if_flags) == IFF_PROMISC)
				vr_setmulti(sc);
			else
				vr_init(ifp);
			break;
		}
		sc->vr_if_flags = ifp->if_flags;
		break;
	default:
		if ((error = ether_ioctl(ifp, command, data)) != ENETRESET)
			break;
		error = 0;
		if (command == SIOCADDMULTI || command == SIOCDELMULTI)
			vr_setmulti(sc);
	}
	splx(s);

	return error;
}

static void
vr_watchdog(struct ifnet *ifp)
{
	struct vr_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", device_xname(sc->vr_dev));
	ifp->if_oerrors++;

	(void) vr_init(ifp);
}

/*
 * One second timer, used to tick MII.
 */
static void
vr_tick(void *arg)
{
	struct vr_softc *sc = arg;
	int s;

	s = splnet();
	if (sc->vr_flags & VR_F_RESTART) {
		printf("%s: restarting\n", device_xname(sc->vr_dev));
		vr_init(&sc->vr_ec.ec_if);
		sc->vr_flags &= ~VR_F_RESTART;
	}
	mii_tick(&sc->vr_mii);
	splx(s);

	callout_reset(&sc->vr_tick_ch, hz, vr_tick, sc);
}

/*
 * Drain the receive queue.
 */
static void
vr_rxdrain(struct vr_softc *sc)
{
	struct vr_descsoft *ds;
	int i;

	for (i = 0; i < VR_NRXDESC; i++) {
		ds = VR_DSRX(sc, i);
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->vr_dmat, ds->ds_dmamap);
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * transmit lists.
 */
static void
vr_stop(struct ifnet *ifp, int disable)
{
	struct vr_softc *sc = ifp->if_softc;
	struct vr_descsoft *ds;
	int i;

	/* Cancel one second timer. */
	callout_stop(&sc->vr_tick_ch);

	/* Down the MII. */
	mii_down(&sc->vr_mii);

	ifp = &sc->vr_ec.ec_if;
	ifp->if_timer = 0;

	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_STOP);
	VR_CLRBIT16(sc, VR_COMMAND, (VR_CMD_RX_ON|VR_CMD_TX_ON));
	CSR_WRITE_2(sc, VR_IMR, 0x0000);
	CSR_WRITE_4(sc, VR_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, VR_RXADDR, 0x00000000);

	/*
	 * Release any queued transmit buffers.
	 */
	for (i = 0; i < VR_NTXDESC; i++) {
		ds = VR_DSTX(sc, i);
		if (ds->ds_mbuf != NULL) {
			bus_dmamap_unload(sc->vr_dmat, ds->ds_dmamap);
			m_freem(ds->ds_mbuf);
			ds->ds_mbuf = NULL;
		}
	}

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (disable)
		vr_rxdrain(sc);
}

static int	vr_probe(device_t, cfdata_t, void *);
static void	vr_attach(device_t, device_t, void *);
static bool	vr_shutdown(device_t, int);

CFATTACH_DECL_NEW(vr, sizeof (struct vr_softc),
    vr_probe, vr_attach, NULL, NULL);

static const struct vr_type *
vr_lookup(struct pci_attach_args *pa)
{
	const struct vr_type *vrt;
	int i;

	for (i = 0; i < __arraycount(vr_devs); i++) {
		vrt = &vr_devs[i];
		if (PCI_VENDOR(pa->pa_id) == vrt->vr_vid &&
		    PCI_PRODUCT(pa->pa_id) == vrt->vr_did)
			return (vrt);
	}
	return (NULL);
}

static int
vr_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (vr_lookup(pa) != NULL)
		return (1);

	return (0);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static bool
vr_shutdown(device_t self, int howto)
{
	struct vr_softc *sc = device_private(self);

	vr_stop(&sc->vr_ec.ec_if, 1);

	return true;
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static void
vr_attach(device_t parent, device_t self, void *aux)
{
	struct vr_softc *sc = device_private(self);
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;
	bus_dma_segment_t seg;
	uint32_t reg;
	struct ifnet *ifp;
	uint8_t eaddr[ETHER_ADDR_LEN], mac;
	int i, rseg, error;
	char intrbuf[PCI_INTRSTR_LEN];

#define	PCI_CONF_WRITE(r, v)	pci_conf_write(sc->vr_pc, sc->vr_tag, (r), (v))
#define	PCI_CONF_READ(r)	pci_conf_read(sc->vr_pc, sc->vr_tag, (r))

	sc->vr_dev = self;
	sc->vr_pc = pa->pa_pc;
	sc->vr_tag = pa->pa_tag;
	sc->vr_id = pa->pa_id;
	callout_init(&sc->vr_tick_ch, 0);

	pci_aprint_devinfo(pa, NULL);

	/*
	 * Handle power management nonsense.
	 */

	sc->vr_save_iobase = PCI_CONF_READ(VR_PCI_LOIO);
	sc->vr_save_membase = PCI_CONF_READ(VR_PCI_LOMEM);
	sc->vr_save_irq = PCI_CONF_READ(PCI_INTERRUPT_REG);

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    vr_restore_state)) && error != EOPNOTSUPP) {
		aprint_error_dev(self, "cannot activate %d\n",
		    error);
		return;
	}

	/* Make sure bus mastering is enabled. */
	reg = PCI_CONF_READ(PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	PCI_CONF_WRITE(PCI_COMMAND_STATUS_REG, reg);

	/* Get revision */
	sc->vr_revid = PCI_REVISION(pa->pa_class);

	/*
	 * Map control/status registers.
	 */
	{
		bus_space_tag_t iot, memt;
		bus_space_handle_t ioh, memh;
		int ioh_valid, memh_valid;
		pci_intr_handle_t intrhandle;
		const char *intrstr;

		ioh_valid = (pci_mapreg_map(pa, VR_PCI_LOIO,
			PCI_MAPREG_TYPE_IO, 0,
			&iot, &ioh, NULL, NULL) == 0);
		memh_valid = (pci_mapreg_map(pa, VR_PCI_LOMEM,
			PCI_MAPREG_TYPE_MEM |
			PCI_MAPREG_MEM_TYPE_32BIT,
			0, &memt, &memh, NULL, NULL) == 0);
#if defined(VR_USEIOSPACE)
		if (ioh_valid) {
			sc->vr_bst = iot;
			sc->vr_bsh = ioh;
		} else if (memh_valid) {
			sc->vr_bst = memt;
			sc->vr_bsh = memh;
		}
#else
		if (memh_valid) {
			sc->vr_bst = memt;
			sc->vr_bsh = memh;
		} else if (ioh_valid) {
			sc->vr_bst = iot;
			sc->vr_bsh = ioh;
		}
#endif
		else {
			printf(": unable to map device registers\n");
			return;
		}

		/* Allocate interrupt */
		if (pci_intr_map(pa, &intrhandle)) {
			aprint_error_dev(self, "couldn't map interrupt\n");
			return;
		}
		intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
		sc->vr_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
						vr_intr, sc);
		if (sc->vr_ih == NULL) {
			aprint_error_dev(self, "couldn't establish interrupt");
			if (intrstr != NULL)
				aprint_error(" at %s", intrstr);
			aprint_error("\n");
		}
		aprint_normal_dev(self, "interrupting at %s\n", intrstr);
	}

	/*
	 * Windows may put the chip in suspend mode when it
	 * shuts down. Be sure to kick it in the head to wake it
	 * up again.
	 *
	 * Don't touch this register on VT3043 since it causes
	 * kernel MCHK trap on macppc.
	 * (Note some VT86C100A chip returns a product ID of VT3043)
	 */
	if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_VIATECH_VT3043)
		VR_CLRBIT(sc, VR_STICKHW, (VR_STICKHW_DS0|VR_STICKHW_DS1));

	/* Reset the adapter. */
	vr_reset(sc);

	/*
	 * Get station address. The way the Rhine chips work,
	 * you're not allowed to directly access the EEPROM once
	 * they've been programmed a special way. Consequently,
	 * we need to read the node address from the PAR0 and PAR1
	 * registers.
	 *
	 * XXXSCW: On the Rhine III, setting VR_EECSR_LOAD forces a reload
	 *         of the *whole* EEPROM, not just the MAC address. This is
	 *         pretty pointless since the chip does this automatically
	 *         at powerup/reset.
	 *         I suspect the same thing applies to the other Rhine
	 *         variants, but in the absence of a data sheet for those
	 *         (and the lack of anyone else noticing the problems this
	 *         causes) I'm going to retain the old behaviour for the
	 *         other parts.
	 *         In some cases, the chip really does startup without having
	 *         read the EEPROM (kern/34812). To handle this case, we force
	 *         a reload if we see an all-zeroes MAC address.
	 */
	for (mac = 0, i = 0; i < ETHER_ADDR_LEN; i++)
		mac |= (eaddr[i] = CSR_READ_1(sc, VR_PAR0 + i));

	if (mac == 0 || (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_VIATECH_VT6105 &&
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_VIATECH_VT6102)) {
		VR_SETBIT(sc, VR_EECSR, VR_EECSR_LOAD);
		DELAY(200);
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			eaddr[i] = CSR_READ_1(sc, VR_PAR0 + i);
	}

	/*
	 * A Rhine chip was detected. Inform the world.
	 */
	aprint_normal("%s: Ethernet address: %s\n",
		device_xname(self), ether_sprintf(eaddr));

	memcpy(sc->vr_enaddr, eaddr, ETHER_ADDR_LEN);

	sc->vr_dmat = pa->pa_dmat;

	/*
	 * Allocate the control data structures, and create and load
	 * the DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->vr_dmat,
	    sizeof(struct vr_control_data), PAGE_SIZE, 0, &seg, 1, &rseg,
	    0)) != 0) {
		aprint_error_dev(self, "unable to allocate control data, error = %d\n", error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->vr_dmat, &seg, rseg,
	    sizeof(struct vr_control_data), (void **)&sc->vr_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(self, "unable to map control data, error = %d\n", error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->vr_dmat,
	    sizeof(struct vr_control_data), 1,
	    sizeof(struct vr_control_data), 0, 0,
	    &sc->vr_cddmamap)) != 0) {
		aprint_error_dev(self, "unable to create control data DMA map, "
		    "error = %d\n", error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->vr_dmat, sc->vr_cddmamap,
	    sc->vr_control_data, sizeof(struct vr_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(self, "unable to load control data DMA map, error = %d\n",
		    error);
		goto fail_3;
	}

	/*
	 * Create the transmit buffer DMA maps.
	 */
	for (i = 0; i < VR_NTXDESC; i++) {
		if ((error = bus_dmamap_create(sc->vr_dmat, MCLBYTES,
		    1, MCLBYTES, 0, 0,
		    &VR_DSTX(sc, i)->ds_dmamap)) != 0) {
			aprint_error_dev(self, "unable to create tx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < VR_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->vr_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0,
		    &VR_DSRX(sc, i)->ds_dmamap)) != 0) {
			aprint_error_dev(self, "unable to create rx DMA map %d, "
			    "error = %d\n", i, error);
			goto fail_5;
		}
		VR_DSRX(sc, i)->ds_mbuf = NULL;
	}

	ifp = &sc->vr_ec.ec_if;
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = vr_ioctl;
	ifp->if_start = vr_start;
	ifp->if_watchdog = vr_watchdog;
	ifp->if_init = vr_init;
	ifp->if_stop = vr_stop;
	IFQ_SET_READY(&ifp->if_snd);

	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);

	/*
	 * Initialize MII/media info.
	 */
	sc->vr_mii.mii_ifp = ifp;
	sc->vr_mii.mii_readreg = vr_mii_readreg;
	sc->vr_mii.mii_writereg = vr_mii_writereg;
	sc->vr_mii.mii_statchg = vr_mii_statchg;

	sc->vr_ec.ec_mii = &sc->vr_mii;
	ifmedia_init(&sc->vr_mii.mii_media, IFM_IMASK, ether_mediachange,
		ether_mediastatus);
	mii_attach(self, &sc->vr_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, MIIF_FORCEANEG);
	if (LIST_FIRST(&sc->vr_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->vr_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->vr_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->vr_mii.mii_media, IFM_ETHER|IFM_AUTO);

	sc->vr_ec.ec_capabilities |= ETHERCAP_VLAN_MTU;

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, sc->vr_enaddr);

	rnd_attach_source(&sc->rnd_source, device_xname(self),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	if (pmf_device_register1(self, NULL, vr_resume, vr_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

 fail_5:
	for (i = 0; i < VR_NRXDESC; i++) {
		if (sc->vr_rxsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->vr_dmat,
			    sc->vr_rxsoft[i].ds_dmamap);
	}
 fail_4:
	for (i = 0; i < VR_NTXDESC; i++) {
		if (sc->vr_txsoft[i].ds_dmamap != NULL)
			bus_dmamap_destroy(sc->vr_dmat,
			    sc->vr_txsoft[i].ds_dmamap);
	}
	bus_dmamap_unload(sc->vr_dmat, sc->vr_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->vr_dmat, sc->vr_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->vr_dmat, (void *)sc->vr_control_data,
	    sizeof(struct vr_control_data));
 fail_1:
	bus_dmamem_free(sc->vr_dmat, &seg, rseg);
 fail_0:
	return;
}

static int
vr_restore_state(pci_chipset_tag_t pc, pcitag_t tag, device_t self,
    pcireg_t state)
{
	struct vr_softc *sc = device_private(self);
	int error;

	if (state == PCI_PMCSR_STATE_D0)
		return 0;
	if ((error = pci_set_powerstate(pc, tag, PCI_PMCSR_STATE_D0)))
		return error;

	/* Restore PCI config data. */
	PCI_CONF_WRITE(VR_PCI_LOIO, sc->vr_save_iobase);
	PCI_CONF_WRITE(VR_PCI_LOMEM, sc->vr_save_membase);
	PCI_CONF_WRITE(PCI_INTERRUPT_REG, sc->vr_save_irq);
	return 0;
}

static bool
vr_resume(device_t self, const pmf_qual_t *qual)
{
	struct vr_softc *sc = device_private(self);

	if (PCI_PRODUCT(sc->vr_id) != PCI_PRODUCT_VIATECH_VT3043)
		VR_CLRBIT(sc, VR_STICKHW, (VR_STICKHW_DS0|VR_STICKHW_DS1));

	return true;
}
