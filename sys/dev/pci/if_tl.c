/*	$NetBSD: if_tl.c,v 1.102 2015/04/13 16:33:25 riastradh Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 * Texas Instruments ThunderLAN ethernet controller
 * ThunderLAN Programmer's Guide (TI Literature Number SPWU013A)
 * available from www.ti.com
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_tl.c,v 1.102 2015/04/13 16:33:25 riastradh Exp $");

#undef TLDEBUG
#define TL_PRIV_STATS
#undef TLDEBUG_RX
#undef TLDEBUG_TX
#undef TLDEBUG_ADDR

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#include <sys/device.h>

#include <net/if.h>
#if defined(SIOCSIFMEDIA)
#include <net/if_media.h>
#endif
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/rndsource.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif


#if defined(__NetBSD__)
#include <net/if_ether.h>
#if defined(INET)
#include <netinet/if_inarp.h>
#endif

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>
#include <dev/i2c/at24cxxvar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/mii/tlphyvar.h>

#include <dev/pci/if_tlregs.h>
#include <dev/pci/if_tlvar.h>
#endif /* __NetBSD__ */

/* number of transmit/receive buffers */
#ifndef TL_NBUF
#define TL_NBUF 32
#endif

static int tl_pci_match(device_t, cfdata_t, void *);
static void tl_pci_attach(device_t, device_t, void *);
static int tl_intr(void *);

static int tl_ifioctl(struct ifnet *, ioctl_cmd_t, void *);
static int tl_mediachange(struct ifnet *);
static void tl_ifwatchdog(struct ifnet *);
static bool tl_shutdown(device_t, int);

static void tl_ifstart(struct ifnet *);
static void tl_reset(tl_softc_t *);
static int  tl_init(struct ifnet *);
static void tl_stop(struct ifnet *, int);
static void tl_restart(void *);
static int  tl_add_RxBuff(tl_softc_t *, struct Rx_list *, struct mbuf *);
static void tl_read_stats(tl_softc_t *);
static void tl_ticks(void *);
static int tl_multicast_hash(uint8_t *);
static void tl_addr_filter(tl_softc_t *);

static uint32_t tl_intreg_read(tl_softc_t *, uint32_t);
static void tl_intreg_write(tl_softc_t *, uint32_t, uint32_t);
static uint8_t tl_intreg_read_byte(tl_softc_t *, uint32_t);
static void tl_intreg_write_byte(tl_softc_t *, uint32_t, uint8_t);

void	tl_mii_sync(struct tl_softc *);
void	tl_mii_sendbits(struct tl_softc *, uint32_t, int);


#if defined(TLDEBUG_RX)
static void ether_printheader(struct ether_header *);
#endif

int tl_mii_read(device_t, int, int);
void tl_mii_write(device_t, int, int, int);

void tl_statchg(struct ifnet *);

	/* I2C glue */
static int tl_i2c_acquire_bus(void *, int);
static void tl_i2c_release_bus(void *, int);
static int tl_i2c_send_start(void *, int);
static int tl_i2c_send_stop(void *, int);
static int tl_i2c_initiate_xfer(void *, i2c_addr_t, int);
static int tl_i2c_read_byte(void *, uint8_t *, int);
static int tl_i2c_write_byte(void *, uint8_t, int);

	/* I2C bit-bang glue */
static void tl_i2cbb_set_bits(void *, uint32_t);
static void tl_i2cbb_set_dir(void *, uint32_t);
static uint32_t tl_i2cbb_read(void *);
static const struct i2c_bitbang_ops tl_i2cbb_ops = {
	tl_i2cbb_set_bits,
	tl_i2cbb_set_dir,
	tl_i2cbb_read,
	{
		TL_NETSIO_EDATA,	/* SDA */
		TL_NETSIO_ECLOCK,	/* SCL */
		TL_NETSIO_ETXEN,	/* SDA is output */
		0,			/* SDA is input */
	}
};

static inline void netsio_clr(tl_softc_t *, uint8_t);
static inline void netsio_set(tl_softc_t *, uint8_t);
static inline uint8_t netsio_read(tl_softc_t *, uint8_t);

static inline void
netsio_clr(tl_softc_t *sc, uint8_t bits)
{

	tl_intreg_write_byte(sc, TL_INT_NET + TL_INT_NetSio,
	    tl_intreg_read_byte(sc, TL_INT_NET + TL_INT_NetSio) & (~bits));
}

static inline void
netsio_set(tl_softc_t *sc, uint8_t bits)
{

	tl_intreg_write_byte(sc, TL_INT_NET + TL_INT_NetSio,
	    tl_intreg_read_byte(sc, TL_INT_NET + TL_INT_NetSio) | bits);
}

static inline uint8_t
netsio_read(tl_softc_t *sc, uint8_t bits)
{

	return tl_intreg_read_byte(sc, TL_INT_NET + TL_INT_NetSio) & bits;
}

CFATTACH_DECL_NEW(tl, sizeof(tl_softc_t),
    tl_pci_match, tl_pci_attach, NULL, NULL);

static const struct tl_product_desc tl_compaq_products[] = {
	{ PCI_PRODUCT_COMPAQ_N100TX, TLPHY_MEDIA_NO_10_T,
	  "Compaq Netelligent 10/100 TX" },
	{ PCI_PRODUCT_COMPAQ_INT100TX, TLPHY_MEDIA_NO_10_T,
	  "Integrated Compaq Netelligent 10/100 TX" },
	{ PCI_PRODUCT_COMPAQ_N10T, TLPHY_MEDIA_10_5,
	  "Compaq Netelligent 10 T" },
	{ PCI_PRODUCT_COMPAQ_N10T2, TLPHY_MEDIA_10_2,
	  "Compaq Netelligent 10 T/2 UTP/Coax" },
	{ PCI_PRODUCT_COMPAQ_IntNF3P, TLPHY_MEDIA_10_2,
	  "Compaq Integrated NetFlex 3/P" },
	{ PCI_PRODUCT_COMPAQ_IntPL100TX, TLPHY_MEDIA_10_2|TLPHY_MEDIA_NO_10_T,
	  "Compaq ProLiant Integrated Netelligent 10/100 TX" },
	{ PCI_PRODUCT_COMPAQ_DPNet100TX, TLPHY_MEDIA_10_5|TLPHY_MEDIA_NO_10_T,
	  "Compaq Dual Port Netelligent 10/100 TX" },
	{ PCI_PRODUCT_COMPAQ_DP4000, TLPHY_MEDIA_10_5|TLPHY_MEDIA_NO_10_T,
	  "Compaq Deskpro 4000 5233MMX" },
	{ PCI_PRODUCT_COMPAQ_NF3P_BNC, TLPHY_MEDIA_10_2,
	  "Compaq NetFlex 3/P w/ BNC" },
	{ PCI_PRODUCT_COMPAQ_NF3P, TLPHY_MEDIA_10_5,
	  "Compaq NetFlex 3/P" },
	{ 0, 0, NULL },
};

static const struct tl_product_desc tl_ti_products[] = {
	/*
	 * Built-in Ethernet on the TI TravelMate 5000
	 * docking station; better product description?
	 */
	{ PCI_PRODUCT_TI_TLAN, 0,
	  "Texas Instruments ThunderLAN" },
	{ 0, 0, NULL },
};

struct tl_vendor_desc {
	uint32_t tv_vendor;
	const struct tl_product_desc *tv_products;
};

const struct tl_vendor_desc tl_vendors[] = {
	{ PCI_VENDOR_COMPAQ, tl_compaq_products },
	{ PCI_VENDOR_TI, tl_ti_products },
	{ 0, NULL },
};

static const struct tl_product_desc *tl_lookup_product(uint32_t);

static const struct tl_product_desc *
tl_lookup_product(uint32_t id)
{
	const struct tl_product_desc *tp;
	const struct tl_vendor_desc *tv;

	for (tv = tl_vendors; tv->tv_products != NULL; tv++)
		if (PCI_VENDOR(id) == tv->tv_vendor)
			break;

	if ((tp = tv->tv_products) == NULL)
		return NULL;

	for (; tp->tp_desc != NULL; tp++)
		if (PCI_PRODUCT(id) == tp->tp_product)
			break;

	if (tp->tp_desc == NULL)
		return NULL;

	return tp;
}

static int
tl_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (tl_lookup_product(pa->pa_id) != NULL)
		return 1;

	return 0;
}

static void
tl_pci_attach(device_t parent, device_t self, void *aux)
{
	tl_softc_t *sc = device_private(self);
	struct pci_attach_args * const pa = (struct pci_attach_args *)aux;
	const struct tl_product_desc *tp;
	struct ifnet * const ifp = &sc->tl_if;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
	int ioh_valid, memh_valid;
	int reg_io, reg_mem;
	pcireg_t reg10, reg14;
	pcireg_t csr;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	aprint_normal("\n");

	callout_init(&sc->tl_tick_ch, 0);
	callout_init(&sc->tl_restart_ch, 0);

	tp = tl_lookup_product(pa->pa_id);
	if (tp == NULL)
		panic("%s: impossible", __func__);
	sc->tl_product = tp;

	/*
	 * Map the card space. First we have to find the I/O and MEM
	 * registers. I/O is supposed to be at 0x10, MEM at 0x14,
	 * but some boards (Compaq Netflex 3/P PCI) seem to have it reversed.
	 * The ThunderLAN manual is not consistent about this either (there
	 * are both cases in code examples).
	 */
	reg10 = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x10);
	reg14 = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x14);
	if (PCI_MAPREG_TYPE(reg10) == PCI_MAPREG_TYPE_IO)
		reg_io = 0x10;
	else if (PCI_MAPREG_TYPE(reg14) == PCI_MAPREG_TYPE_IO)
		reg_io = 0x14;
	else
		reg_io = 0;
	if (PCI_MAPREG_TYPE(reg10) == PCI_MAPREG_TYPE_MEM)
		reg_mem = 0x10;
	else if (PCI_MAPREG_TYPE(reg14) == PCI_MAPREG_TYPE_MEM)
		reg_mem = 0x14;
	else
		reg_mem = 0;

	if (reg_io != 0)
		ioh_valid = (pci_mapreg_map(pa, reg_io, PCI_MAPREG_TYPE_IO,
		    0, &iot, &ioh, NULL, NULL) == 0);
	else
		ioh_valid = 0;
	if (reg_mem != 0)
		memh_valid = (pci_mapreg_map(pa, PCI_CBMA,
		    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
		    0, &memt, &memh, NULL, NULL) == 0);
	else
		memh_valid = 0;

	if (ioh_valid) {
		sc->tl_bustag = iot;
		sc->tl_bushandle = ioh;
	} else if (memh_valid) {
		sc->tl_bustag = memt;
		sc->tl_bushandle = memh;
	} else {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}
	sc->tl_dmatag = pa->pa_dmat;

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	aprint_normal_dev(self, "%s\n", tp->tp_desc);

	tl_reset(sc);

	/* fill in the i2c tag */
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = tl_i2c_acquire_bus;
	sc->sc_i2c.ic_release_bus = tl_i2c_release_bus;
	sc->sc_i2c.ic_send_start = tl_i2c_send_start;
	sc->sc_i2c.ic_send_stop = tl_i2c_send_stop;
	sc->sc_i2c.ic_initiate_xfer = tl_i2c_initiate_xfer;
	sc->sc_i2c.ic_read_byte = tl_i2c_read_byte;
	sc->sc_i2c.ic_write_byte = tl_i2c_write_byte;

#ifdef TLDEBUG
	aprint_debug_dev(self, "default values of INTreg: 0x%x\n",
	    tl_intreg_read(sc, TL_INT_Defaults));
#endif

	/* read mac addr */
	if (seeprom_bootstrap_read(&sc->sc_i2c, 0x50, 0x83, 256 /* 2kbit */,
	    sc->tl_enaddr, ETHER_ADDR_LEN)) {
		aprint_error_dev(self, "error reading Ethernet address\n");
		return;
	}
	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(sc->tl_enaddr));

	/* Map and establish interrupts */
	if (pci_intr_map(pa, &intrhandle)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
	sc->tl_if.if_softc = sc;
	sc->tl_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
	    tl_intr, sc);
	if (sc->tl_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/* init these pointers, so that tl_shutdown won't try to read them */
	sc->Rx_list = NULL;
	sc->Tx_list = NULL;

	/* allocate DMA-safe memory for control structs */
	if (bus_dmamem_alloc(sc->tl_dmatag, PAGE_SIZE, 0, PAGE_SIZE,
	    &sc->ctrl_segs, 1, &sc->ctrl_nsegs, BUS_DMA_NOWAIT) != 0 ||
	    bus_dmamem_map(sc->tl_dmatag, &sc->ctrl_segs,
	    sc->ctrl_nsegs, PAGE_SIZE, (void **)&sc->ctrl,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT) != 0) {
		aprint_error_dev(self, "can't allocate DMA memory for lists\n");
		return;
	}

	/*
	 * Initialize our media structures and probe the MII.
	 *
	 * Note that we don't care about the media instance.  We
	 * are expecting to have multiple PHYs on the 10/100 cards,
	 * and on those cards we exclude the internal PHY from providing
	 * 10baseT.  By ignoring the instance, it allows us to not have
	 * to specify it on the command line when switching media.
	 */
	sc->tl_mii.mii_ifp = ifp;
	sc->tl_mii.mii_readreg = tl_mii_read;
	sc->tl_mii.mii_writereg = tl_mii_write;
	sc->tl_mii.mii_statchg = tl_statchg;
	sc->tl_ec.ec_mii = &sc->tl_mii;
	ifmedia_init(&sc->tl_mii.mii_media, IFM_IMASK, tl_mediachange,
	    ether_mediastatus);
	mii_attach(self, &sc->tl_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->tl_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->tl_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->tl_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->tl_mii.mii_media, IFM_ETHER|IFM_AUTO);

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->tl_ec.ec_capabilities |= ETHERCAP_VLAN_MTU;

	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;
	ifp->if_ioctl = tl_ifioctl;
	ifp->if_start = tl_ifstart;
	ifp->if_watchdog = tl_ifwatchdog;
	ifp->if_init = tl_init;
	ifp->if_stop = tl_stop;
	ifp->if_timer = 0;
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	ether_ifattach(&(sc)->tl_if, (sc)->tl_enaddr);

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot.
	 * Not doing reboot before the driver initializes.
	 */
	if (pmf_device_register1(self, NULL, NULL, tl_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	rnd_attach_source(&sc->rnd_source, device_xname(self),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);
}

static void
tl_reset(tl_softc_t *sc)
{
	int i;

	/* read stats */
	if (sc->tl_if.if_flags & IFF_RUNNING) {
		callout_stop(&sc->tl_tick_ch);
		tl_read_stats(sc);
	}
	/* Reset adapter */
	TL_HR_WRITE(sc, TL_HOST_CMD,
	    TL_HR_READ(sc, TL_HOST_CMD) | HOST_CMD_Ad_Rst);
	DELAY(100000);
	/* Disable interrupts */
	TL_HR_WRITE(sc, TL_HOST_CMD, HOST_CMD_IntOff);
	/* setup aregs & hash */
	for (i = TL_INT_Areg0; i <= TL_INT_HASH2; i = i + 4)
		tl_intreg_write(sc, i, 0);
#ifdef TLDEBUG_ADDR
	printf("Areg & hash registers: \n");
	for (i = TL_INT_Areg0; i <= TL_INT_HASH2; i = i + 4)
		printf("    reg %x: %x\n", i, tl_intreg_read(sc, i));
#endif
	/* Setup NetConfig */
	tl_intreg_write(sc, TL_INT_NetConfig,
	    TL_NETCONFIG_1F | TL_NETCONFIG_1chn | TL_NETCONFIG_PHY_EN);
	/* Bsize: accept default */
	/* TX commit in Acommit: accept default */
	/* Load Ld_tmr and Ld_thr */
	/* Ld_tmr = 3 */
	TL_HR_WRITE(sc, TL_HOST_CMD, 0x3 | HOST_CMD_LdTmr);
	/* Ld_thr = 0 */
	TL_HR_WRITE(sc, TL_HOST_CMD, 0x0 | HOST_CMD_LdThr);
	/* Unreset MII */
	netsio_set(sc, TL_NETSIO_NMRST);
	DELAY(100000);
	sc->tl_mii.mii_media_status &= ~IFM_ACTIVE;
}

static bool
tl_shutdown(device_t self, int howto)
{
	tl_softc_t *sc = device_private(self);
	struct ifnet *ifp = &sc->tl_if;

	tl_stop(ifp, 1);

	return true;
}

static void
tl_stop(struct ifnet *ifp, int disable)
{
	tl_softc_t *sc = ifp->if_softc;
	struct Tx_list *Tx;
	int i;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
	/* disable interrupts */
	TL_HR_WRITE(sc, TL_HOST_CMD, HOST_CMD_IntOff);
	/* stop TX and RX channels */
	TL_HR_WRITE(sc, TL_HOST_CMD,
	    HOST_CMD_STOP | HOST_CMD_RT | HOST_CMD_Nes);
	TL_HR_WRITE(sc, TL_HOST_CMD, HOST_CMD_STOP);
	DELAY(100000);

	/* stop statistics reading loop, read stats */
	callout_stop(&sc->tl_tick_ch);
	tl_read_stats(sc);

	/* Down the MII. */
	mii_down(&sc->tl_mii);

	/* deallocate memory allocations */
	if (sc->Rx_list) {
		for (i = 0; i< TL_NBUF; i++) {
			if (sc->Rx_list[i].m) {
				bus_dmamap_unload(sc->tl_dmatag,
				    sc->Rx_list[i].m_dmamap);
				m_freem(sc->Rx_list[i].m);
			}
			bus_dmamap_destroy(sc->tl_dmatag,
			    sc->Rx_list[i].m_dmamap);
			sc->Rx_list[i].m = NULL;
		}
		free(sc->Rx_list, M_DEVBUF);
		sc->Rx_list = NULL;
		bus_dmamap_unload(sc->tl_dmatag, sc->Rx_dmamap);
		bus_dmamap_destroy(sc->tl_dmatag, sc->Rx_dmamap);
		sc->hw_Rx_list = NULL;
		while ((Tx = sc->active_Tx) != NULL) {
			Tx->hw_list->stat = 0;
			bus_dmamap_unload(sc->tl_dmatag, Tx->m_dmamap);
			bus_dmamap_destroy(sc->tl_dmatag, Tx->m_dmamap);
			m_freem(Tx->m);
			sc->active_Tx = Tx->next;
			Tx->next = sc->Free_Tx;
			sc->Free_Tx = Tx;
		}
		sc->last_Tx = NULL;
		free(sc->Tx_list, M_DEVBUF);
		sc->Tx_list = NULL;
		bus_dmamap_unload(sc->tl_dmatag, sc->Tx_dmamap);
		bus_dmamap_destroy(sc->tl_dmatag, sc->Tx_dmamap);
		sc->hw_Tx_list = NULL;
	}
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
	sc->tl_mii.mii_media_status &= ~IFM_ACTIVE;
}

static void
tl_restart(void *v)
{

	tl_init(v);
}

static int
tl_init(struct ifnet *ifp)
{
	tl_softc_t *sc = ifp->if_softc;
	int i, s, error;
	bus_size_t boundary;
	prop_number_t prop_boundary;
	const char *errstring;
	char *nullbuf;

	s = splnet();
	/* cancel any pending IO */
	tl_stop(ifp, 1);
	tl_reset(sc);
	if ((sc->tl_if.if_flags & IFF_UP) == 0) {
		splx(s);
		return 0;
	}
	/* Set various register to reasonable value */
	/* setup NetCmd in promisc mode if needed */
	i = (ifp->if_flags & IFF_PROMISC) ? TL_NETCOMMAND_CAF : 0;
	tl_intreg_write_byte(sc, TL_INT_NET + TL_INT_NetCmd,
	    TL_NETCOMMAND_NRESET | TL_NETCOMMAND_NWRAP | i);
	/* Max receive size : MCLBYTES */
	tl_intreg_write_byte(sc, TL_INT_MISC + TL_MISC_MaxRxL, MCLBYTES & 0xff);
	tl_intreg_write_byte(sc, TL_INT_MISC + TL_MISC_MaxRxH,
	    (MCLBYTES >> 8) & 0xff);

	/* init MAC addr */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		tl_intreg_write_byte(sc, TL_INT_Areg0 + i , sc->tl_enaddr[i]);
	/* add multicast filters */
	tl_addr_filter(sc);
#ifdef TLDEBUG_ADDR
	printf("Wrote Mac addr, Areg & hash registers are now: \n");
	for (i = TL_INT_Areg0; i <= TL_INT_HASH2; i = i + 4)
		printf("    reg %x: %x\n", i, tl_intreg_read(sc, i));
#endif

	/* Pre-allocate receivers mbuf, make the lists */
	sc->Rx_list = malloc(sizeof(struct Rx_list) * TL_NBUF, M_DEVBUF,
	    M_NOWAIT|M_ZERO);
	sc->Tx_list = malloc(sizeof(struct Tx_list) * TL_NBUF, M_DEVBUF,
	    M_NOWAIT|M_ZERO);
	if (sc->Rx_list == NULL || sc->Tx_list == NULL) {
		errstring = "out of memory for lists";
		error = ENOMEM;
		goto bad;
	}

	/*
	 * Some boards (Set Engineering GFE) do not permit DMA transfers
	 * across page boundaries.
	 */
	prop_boundary = prop_dictionary_get(device_properties(sc->sc_dev),
	    "tl-dma-page-boundary");
	if (prop_boundary != NULL) {
		KASSERT(prop_object_type(prop_boundary) == PROP_TYPE_NUMBER);
		boundary = (bus_size_t)prop_number_integer_value(prop_boundary);
	} else {
		boundary = 0;
	}

	error = bus_dmamap_create(sc->tl_dmatag,
	    sizeof(struct tl_Rx_list) * TL_NBUF, 1,
	    sizeof(struct tl_Rx_list) * TL_NBUF, 0, BUS_DMA_WAITOK,
	    &sc->Rx_dmamap);
	if (error == 0)
		error = bus_dmamap_create(sc->tl_dmatag,
		    sizeof(struct tl_Tx_list) * TL_NBUF, 1,
		    sizeof(struct tl_Tx_list) * TL_NBUF, boundary,
		    BUS_DMA_WAITOK, &sc->Tx_dmamap);
	if (error == 0)
		error = bus_dmamap_create(sc->tl_dmatag, ETHER_MIN_TX, 1,
		    ETHER_MIN_TX, boundary, BUS_DMA_WAITOK,
		    &sc->null_dmamap);
	if (error) {
		errstring = "can't allocate DMA maps for lists";
		goto bad;
	}
	memset(sc->ctrl, 0, PAGE_SIZE);
	sc->hw_Rx_list = (void *)sc->ctrl;
	sc->hw_Tx_list =
	    (void *)(sc->ctrl + sizeof(struct tl_Rx_list) * TL_NBUF);
	nullbuf = sc->ctrl + sizeof(struct tl_Rx_list) * TL_NBUF +
	    sizeof(struct tl_Tx_list) * TL_NBUF;
	error = bus_dmamap_load(sc->tl_dmatag, sc->Rx_dmamap,
	    sc->hw_Rx_list, sizeof(struct tl_Rx_list) * TL_NBUF, NULL,
	    BUS_DMA_WAITOK);
	if (error == 0)
		error = bus_dmamap_load(sc->tl_dmatag, sc->Tx_dmamap,
		    sc->hw_Tx_list, sizeof(struct tl_Tx_list) * TL_NBUF, NULL,
		    BUS_DMA_WAITOK);
	if (error == 0)
		error = bus_dmamap_load(sc->tl_dmatag, sc->null_dmamap,
		    nullbuf, ETHER_MIN_TX, NULL, BUS_DMA_WAITOK);
	if (error) {
		errstring = "can't DMA map DMA memory for lists";
		goto bad;
	}
	for (i = 0; i < TL_NBUF; i++) {
		error = bus_dmamap_create(sc->tl_dmatag, MCLBYTES,
		    1, MCLBYTES, boundary, BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
		    &sc->Rx_list[i].m_dmamap);
		if (error == 0) {
			error = bus_dmamap_create(sc->tl_dmatag, MCLBYTES,
			    TL_NSEG, MCLBYTES, boundary,
			    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
			    &sc->Tx_list[i].m_dmamap);
		}
		if (error) {
			errstring = "can't allocate DMA maps for mbufs";
			goto bad;
		}
		sc->Rx_list[i].hw_list = &sc->hw_Rx_list[i];
		sc->Rx_list[i].hw_listaddr = sc->Rx_dmamap->dm_segs[0].ds_addr
		    + sizeof(struct tl_Rx_list) * i;
		sc->Tx_list[i].hw_list = &sc->hw_Tx_list[i];
		sc->Tx_list[i].hw_listaddr = sc->Tx_dmamap->dm_segs[0].ds_addr
		    + sizeof(struct tl_Tx_list) * i;
		if (tl_add_RxBuff(sc, &sc->Rx_list[i], NULL) == 0) {
			errstring = "out of mbuf for receive list";
			error = ENOMEM;
			goto bad;
		}
		if (i > 0) { /* chain the list */
			sc->Rx_list[i - 1].next = &sc->Rx_list[i];
			sc->hw_Rx_list[i - 1].fwd =
			    htole32(sc->Rx_list[i].hw_listaddr);
			sc->Tx_list[i - 1].next = &sc->Tx_list[i];
		}
	}
	sc->hw_Rx_list[TL_NBUF - 1].fwd = 0;
	sc->Rx_list[TL_NBUF - 1].next = NULL;
	sc->hw_Tx_list[TL_NBUF - 1].fwd = 0;
	sc->Tx_list[TL_NBUF - 1].next = NULL;

	sc->active_Rx = &sc->Rx_list[0];
	sc->last_Rx   = &sc->Rx_list[TL_NBUF - 1];
	sc->active_Tx = sc->last_Tx = NULL;
	sc->Free_Tx   = &sc->Tx_list[0];
	bus_dmamap_sync(sc->tl_dmatag, sc->Rx_dmamap, 0,
	    sizeof(struct tl_Rx_list) * TL_NBUF,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->tl_dmatag, sc->Tx_dmamap, 0,
	    sizeof(struct tl_Tx_list) * TL_NBUF,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->tl_dmatag, sc->null_dmamap, 0, ETHER_MIN_TX,
	    BUS_DMASYNC_PREWRITE);

	/* set media */
	if ((error = mii_mediachg(&sc->tl_mii)) == ENXIO)
		error = 0;
	else if (error != 0) {
		errstring = "could not set media";
		goto bad;
	}

	/* start ticks calls */
	callout_reset(&sc->tl_tick_ch, hz, tl_ticks, sc);
	/* write address of Rx list and enable interrupts */
	TL_HR_WRITE(sc, TL_HOST_CH_PARM, sc->Rx_list[0].hw_listaddr);
	TL_HR_WRITE(sc, TL_HOST_CMD,
	    HOST_CMD_GO | HOST_CMD_RT | HOST_CMD_Nes | HOST_CMD_IntOn);
	sc->tl_if.if_flags |= IFF_RUNNING;
	sc->tl_if.if_flags &= ~IFF_OACTIVE;
	splx(s);
	return 0;
bad:
	printf("%s: %s\n", device_xname(sc->sc_dev), errstring);
	splx(s);
	return error;
}


static uint32_t
tl_intreg_read(tl_softc_t *sc, uint32_t reg)
{

	TL_HR_WRITE(sc, TL_HOST_INTR_DIOADR, reg & TL_HOST_DIOADR_MASK);
	return TL_HR_READ(sc, TL_HOST_DIO_DATA);
}

static uint8_t
tl_intreg_read_byte(tl_softc_t *sc, uint32_t reg)
{

	TL_HR_WRITE(sc, TL_HOST_INTR_DIOADR,
	    (reg & (~0x07)) & TL_HOST_DIOADR_MASK);
	return TL_HR_READ_BYTE(sc, TL_HOST_DIO_DATA + (reg & 0x07));
}

static void
tl_intreg_write(tl_softc_t *sc, uint32_t reg, uint32_t val)
{

	TL_HR_WRITE(sc, TL_HOST_INTR_DIOADR, reg & TL_HOST_DIOADR_MASK);
	TL_HR_WRITE(sc, TL_HOST_DIO_DATA, val);
}

static void
tl_intreg_write_byte(tl_softc_t *sc, uint32_t reg, uint8_t val)
{

	TL_HR_WRITE(sc, TL_HOST_INTR_DIOADR,
	    (reg & (~0x03)) & TL_HOST_DIOADR_MASK);
	TL_HR_WRITE_BYTE(sc, TL_HOST_DIO_DATA + (reg & 0x03), val);
}

void
tl_mii_sync(struct tl_softc *sc)
{
	int i;

	netsio_clr(sc, TL_NETSIO_MTXEN);
	for (i = 0; i < 32; i++) {
		netsio_clr(sc, TL_NETSIO_MCLK);
		netsio_set(sc, TL_NETSIO_MCLK);
	}
}

void
tl_mii_sendbits(struct tl_softc *sc, uint32_t data, int nbits)
{
	int i;

	netsio_set(sc, TL_NETSIO_MTXEN);
	for (i = 1 << (nbits - 1); i; i = i >>  1) {
		netsio_clr(sc, TL_NETSIO_MCLK);
		netsio_read(sc, TL_NETSIO_MCLK);
		if (data & i)
			netsio_set(sc, TL_NETSIO_MDATA);
		else
			netsio_clr(sc, TL_NETSIO_MDATA);
		netsio_set(sc, TL_NETSIO_MCLK);
		netsio_read(sc, TL_NETSIO_MCLK);
	}
}

int
tl_mii_read(device_t self, int phy, int reg)
{
	struct tl_softc *sc = device_private(self);
	int val = 0, i, err;

	/*
	 * Read the PHY register by manually driving the MII control lines.
	 */

	tl_mii_sync(sc);
	tl_mii_sendbits(sc, MII_COMMAND_START, 2);
	tl_mii_sendbits(sc, MII_COMMAND_READ, 2);
	tl_mii_sendbits(sc, phy, 5);
	tl_mii_sendbits(sc, reg, 5);

	netsio_clr(sc, TL_NETSIO_MTXEN);
	netsio_clr(sc, TL_NETSIO_MCLK);
	netsio_set(sc, TL_NETSIO_MCLK);
	netsio_clr(sc, TL_NETSIO_MCLK);

	err = netsio_read(sc, TL_NETSIO_MDATA);
	netsio_set(sc, TL_NETSIO_MCLK);

	/* Even if an error occurs, must still clock out the cycle. */
	for (i = 0; i < 16; i++) {
		val <<= 1;
		netsio_clr(sc, TL_NETSIO_MCLK);
		if (err == 0 && netsio_read(sc, TL_NETSIO_MDATA))
			val |= 1;
		netsio_set(sc, TL_NETSIO_MCLK);
	}
	netsio_clr(sc, TL_NETSIO_MCLK);
	netsio_set(sc, TL_NETSIO_MCLK);

	return err ? 0 : val;
}

void
tl_mii_write(device_t self, int phy, int reg, int val)
{
	struct tl_softc *sc = device_private(self);

	/*
	 * Write the PHY register by manually driving the MII control lines.
	 */

	tl_mii_sync(sc);
	tl_mii_sendbits(sc, MII_COMMAND_START, 2);
	tl_mii_sendbits(sc, MII_COMMAND_WRITE, 2);
	tl_mii_sendbits(sc, phy, 5);
	tl_mii_sendbits(sc, reg, 5);
	tl_mii_sendbits(sc, MII_COMMAND_ACK, 2);
	tl_mii_sendbits(sc, val, 16);

	netsio_clr(sc, TL_NETSIO_MCLK);
	netsio_set(sc, TL_NETSIO_MCLK);
}

void
tl_statchg(struct ifnet *ifp)
{
	tl_softc_t *sc = ifp->if_softc;
	uint32_t reg;

#ifdef TLDEBUG
	printf("%s: media %x\n", __func__, sc->tl_mii.mii_media.ifm_media);
#endif

	/*
	 * We must keep the ThunderLAN and the PHY in sync as
	 * to the status of full-duplex!
	 */
	reg = tl_intreg_read_byte(sc, TL_INT_NET + TL_INT_NetCmd);
	if (sc->tl_mii.mii_media_active & IFM_FDX)
		reg |= TL_NETCOMMAND_DUPLEX;
	else
		reg &= ~TL_NETCOMMAND_DUPLEX;
	tl_intreg_write_byte(sc, TL_INT_NET + TL_INT_NetCmd, reg);
}

/********** I2C glue **********/

static int
tl_i2c_acquire_bus(void *cookie, int flags)
{

	/* private bus */
	return 0;
}

static void
tl_i2c_release_bus(void *cookie, int flags)
{

	/* private bus */
}

static int
tl_i2c_send_start(void *cookie, int flags)
{

	return i2c_bitbang_send_start(cookie, flags, &tl_i2cbb_ops);
}

static int
tl_i2c_send_stop(void *cookie, int flags)
{

	return i2c_bitbang_send_stop(cookie, flags, &tl_i2cbb_ops);
}

static int
tl_i2c_initiate_xfer(void *cookie, i2c_addr_t addr, int flags)
{

	return i2c_bitbang_initiate_xfer(cookie, addr, flags, &tl_i2cbb_ops);
}

static int
tl_i2c_read_byte(void *cookie, uint8_t *valp, int flags)
{

	return i2c_bitbang_read_byte(cookie, valp, flags, &tl_i2cbb_ops);
}

static int
tl_i2c_write_byte(void *cookie, uint8_t val, int flags)
{

	return i2c_bitbang_write_byte(cookie, val, flags, &tl_i2cbb_ops);
}

/********** I2C bit-bang glue **********/

static void
tl_i2cbb_set_bits(void *cookie, uint32_t bits)
{
	struct tl_softc *sc = cookie;
	uint8_t reg;

	reg = tl_intreg_read_byte(sc, TL_INT_NET + TL_INT_NetSio);
	reg = (reg & ~(TL_NETSIO_EDATA|TL_NETSIO_ECLOCK)) | bits;
	tl_intreg_write_byte(sc, TL_INT_NET + TL_INT_NetSio, reg);
}

static void
tl_i2cbb_set_dir(void *cookie, uint32_t bits)
{
	struct tl_softc *sc = cookie;
	uint8_t reg;

	reg = tl_intreg_read_byte(sc, TL_INT_NET + TL_INT_NetSio);
	reg = (reg & ~TL_NETSIO_ETXEN) | bits;
	tl_intreg_write_byte(sc, TL_INT_NET + TL_INT_NetSio, reg);
}

static uint32_t
tl_i2cbb_read(void *cookie)
{

	return tl_intreg_read_byte(cookie, TL_INT_NET + TL_INT_NetSio);
}

/********** End of I2C stuff **********/

static int
tl_intr(void *v)
{
	tl_softc_t *sc = v;
	struct ifnet *ifp = &sc->tl_if;
	struct Rx_list *Rx;
	struct Tx_list *Tx;
	struct mbuf *m;
	uint32_t int_type, int_reg;
	int ack = 0;
	int size;

	int_reg = TL_HR_READ(sc, TL_HOST_INTR_DIOADR);
	int_type = int_reg  & TL_INTR_MASK;
	if (int_type == 0)
		return 0;
#if defined(TLDEBUG_RX) || defined(TLDEBUG_TX)
	printf("%s: interrupt type %x, intr_reg %x\n", device_xname(sc->sc_dev),
	    int_type, int_reg);
#endif
	/* disable interrupts */
	TL_HR_WRITE(sc, TL_HOST_CMD, HOST_CMD_IntOff);
	switch(int_type & TL_INTR_MASK) {
	case TL_INTR_RxEOF:
		bus_dmamap_sync(sc->tl_dmatag, sc->Rx_dmamap, 0,
		    sizeof(struct tl_Rx_list) * TL_NBUF,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		while(le32toh(sc->active_Rx->hw_list->stat) &
		    TL_RX_CSTAT_CPLT) {
			/* dequeue and requeue at end of list */
			ack++;
			Rx = sc->active_Rx;
			sc->active_Rx = Rx->next;
			bus_dmamap_sync(sc->tl_dmatag, Rx->m_dmamap, 0,
			    Rx->m_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->tl_dmatag, Rx->m_dmamap);
			m = Rx->m;
			size = le32toh(Rx->hw_list->stat) >> 16;
#ifdef TLDEBUG_RX
			printf("%s: RX list complete, Rx %p, size=%d\n",
			    __func__, Rx, size);
#endif
			if (tl_add_RxBuff(sc, Rx, m) == 0) {
				/*
				 * No new mbuf, reuse the same. This means
				 * that this packet
				 * is lost
				 */
				m = NULL;
#ifdef TL_PRIV_STATS
				sc->ierr_nomem++;
#endif
#ifdef TLDEBUG
				printf("%s: out of mbuf, lost input packet\n",
				    device_xname(sc->sc_dev));
#endif
			}
			Rx->next = NULL;
			Rx->hw_list->fwd = 0;
			sc->last_Rx->hw_list->fwd = htole32(Rx->hw_listaddr);
			sc->last_Rx->next = Rx;
			sc->last_Rx = Rx;

			/* deliver packet */
			if (m) {
				if (size < sizeof(struct ether_header)) {
					m_freem(m);
					continue;
				}
				m->m_pkthdr.rcvif = ifp;
				m->m_pkthdr.len = m->m_len = size;
#ifdef TLDEBUG_RX
				{
					struct ether_header *eh =
					    mtod(m, struct ether_header *);
					printf("%s: Rx packet:\n", __func__);
					ether_printheader(eh);
				}
#endif
				bpf_mtap(ifp, m);
				(*ifp->if_input)(ifp, m);
			}
		}
		bus_dmamap_sync(sc->tl_dmatag, sc->Rx_dmamap, 0,
		    sizeof(struct tl_Rx_list) * TL_NBUF,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
#ifdef TLDEBUG_RX
		printf("TL_INTR_RxEOF: ack %d\n", ack);
#else
		if (ack == 0) {
			printf("%s: EOF intr without anything to read !\n",
			    device_xname(sc->sc_dev));
			tl_reset(sc);
			/* schedule reinit of the board */
			callout_reset(&sc->tl_restart_ch, 1, tl_restart, ifp);
			return 1;
		}
#endif
		break;
	case TL_INTR_RxEOC:
		ack++;
		bus_dmamap_sync(sc->tl_dmatag, sc->Rx_dmamap, 0,
		    sizeof(struct tl_Rx_list) * TL_NBUF,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
#ifdef TLDEBUG_RX
		printf("TL_INTR_RxEOC: ack %d\n", ack);
#endif
#ifdef DIAGNOSTIC
		if (le32toh(sc->active_Rx->hw_list->stat) & TL_RX_CSTAT_CPLT) {
			printf("%s: Rx EOC interrupt and active Tx list not "
			    "cleared\n", device_xname(sc->sc_dev));
			return 0;
		} else
#endif
		{
		/*
		 * write address of Rx list and send Rx GO command, ack
		 * interrupt and enable interrupts in one command
		 */
		TL_HR_WRITE(sc, TL_HOST_CH_PARM, sc->active_Rx->hw_listaddr);
		TL_HR_WRITE(sc, TL_HOST_CMD,
		    HOST_CMD_GO | HOST_CMD_RT | HOST_CMD_Nes | ack | int_type |
		    HOST_CMD_ACK | HOST_CMD_IntOn);
		return 1;
		}
	case TL_INTR_TxEOF:
	case TL_INTR_TxEOC:
		bus_dmamap_sync(sc->tl_dmatag, sc->Tx_dmamap, 0,
		    sizeof(struct tl_Tx_list) * TL_NBUF,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		while ((Tx = sc->active_Tx) != NULL) {
			if((le32toh(Tx->hw_list->stat) & TL_TX_CSTAT_CPLT) == 0)
				break;
			ack++;
#ifdef TLDEBUG_TX
			printf("TL_INTR_TxEOC: list 0x%x done\n",
			    (int)Tx->hw_listaddr);
#endif
			Tx->hw_list->stat = 0;
			bus_dmamap_sync(sc->tl_dmatag, Tx->m_dmamap, 0,
			    Tx->m_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->tl_dmatag, Tx->m_dmamap);
			m_freem(Tx->m);
			Tx->m = NULL;
			sc->active_Tx = Tx->next;
			if (sc->active_Tx == NULL)
				sc->last_Tx = NULL;
			Tx->next = sc->Free_Tx;
			sc->Free_Tx = Tx;
		}
		bus_dmamap_sync(sc->tl_dmatag, sc->Tx_dmamap, 0,
		    sizeof(struct tl_Tx_list) * TL_NBUF,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/* if this was an EOC, ACK immediatly */
		if (ack)
			sc->tl_if.if_flags &= ~IFF_OACTIVE;
		if (int_type == TL_INTR_TxEOC) {
#ifdef TLDEBUG_TX
			printf("TL_INTR_TxEOC: ack %d (will be set to 1)\n",
			    ack);
#endif
			TL_HR_WRITE(sc, TL_HOST_CMD, 1 | int_type |
			    HOST_CMD_ACK | HOST_CMD_IntOn);
			if (sc->active_Tx != NULL) {
				/* needs a Tx go command */
				TL_HR_WRITE(sc, TL_HOST_CH_PARM,
				    sc->active_Tx->hw_listaddr);
				TL_HR_WRITE(sc, TL_HOST_CMD, HOST_CMD_GO);
			}
			sc->tl_if.if_timer = 0;
			if (IFQ_IS_EMPTY(&sc->tl_if.if_snd) == 0)
				tl_ifstart(&sc->tl_if);
			return 1;
		}
#ifdef TLDEBUG
		else {
			printf("TL_INTR_TxEOF: ack %d\n", ack);
		}
#endif
		sc->tl_if.if_timer = 0;
		if (IFQ_IS_EMPTY(&sc->tl_if.if_snd) == 0)
			tl_ifstart(&sc->tl_if);
		break;
	case TL_INTR_Stat:
		ack++;
#ifdef TLDEBUG
		printf("TL_INTR_Stat: ack %d\n", ack);
#endif
		tl_read_stats(sc);
		break;
	case TL_INTR_Adc:
		if (int_reg & TL_INTVec_MASK) {
			/* adapter check conditions */
			printf("%s: check condition, intvect=0x%x, "
			    "ch_param=0x%x\n", device_xname(sc->sc_dev),
			    int_reg & TL_INTVec_MASK,
			    TL_HR_READ(sc, TL_HOST_CH_PARM));
			tl_reset(sc);
			/* schedule reinit of the board */
			callout_reset(&sc->tl_restart_ch, 1, tl_restart, ifp);
			return 1;
		} else {
			uint8_t netstat;
			/* Network status */
			netstat =
			    tl_intreg_read_byte(sc, TL_INT_NET+TL_INT_NetSts);
			printf("%s: network status, NetSts=%x\n",
			    device_xname(sc->sc_dev), netstat);
			/* Ack interrupts */
			tl_intreg_write_byte(sc, TL_INT_NET+TL_INT_NetSts,
			    netstat);
			ack++;
		}
		break;
	default:
		printf("%s: unhandled interrupt code %x!\n",
		    device_xname(sc->sc_dev), int_type);
		ack++;
	}

	if (ack) {
		/* Ack the interrupt and enable interrupts */
		TL_HR_WRITE(sc, TL_HOST_CMD, ack | int_type | HOST_CMD_ACK |
		    HOST_CMD_IntOn);
		rnd_add_uint32(&sc->rnd_source, int_reg);
		return 1;
	}
	/* ack = 0 ; interrupt was perhaps not our. Just enable interrupts */
	TL_HR_WRITE(sc, TL_HOST_CMD, HOST_CMD_IntOn);
	return 0;
}

static int
tl_ifioctl(struct ifnet *ifp, unsigned long cmd, void *data)
{
	struct tl_softc *sc = ifp->if_softc;
	int s, error;

	s = splnet();
	error = ether_ioctl(ifp, cmd, data);
	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			tl_addr_filter(sc);
		error = 0;
	}
	splx(s);
	return error;
}

static void
tl_ifstart(struct ifnet *ifp)
{
	tl_softc_t *sc = ifp->if_softc;
	struct mbuf *mb_head;
	struct Tx_list *Tx;
	int segment, size;
	int again, error;

	if ((sc->tl_if.if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;
txloop:
	/* If we don't have more space ... */
	if (sc->Free_Tx == NULL) {
#ifdef TLDEBUG
		printf("%s: No free TX list\n", __func__);
#endif
		sc->tl_if.if_flags |= IFF_OACTIVE;
		return;
	}
	/* Grab a paquet for output */
	IFQ_DEQUEUE(&ifp->if_snd, mb_head);
	if (mb_head == NULL) {
#ifdef TLDEBUG_TX
		printf("%s: nothing to send\n", __func__);
#endif
		return;
	}
	Tx = sc->Free_Tx;
	sc->Free_Tx = Tx->next;
	Tx->next = NULL;
	again = 0;
	/*
	 * Go through each of the mbufs in the chain and initialize
	 * the transmit list descriptors with the physical address
	 * and size of the mbuf.
	 */
tbdinit:
	memset(Tx->hw_list, 0, sizeof(struct tl_Tx_list));
	Tx->m = mb_head;
	size = mb_head->m_pkthdr.len;
	if ((error = bus_dmamap_load_mbuf(sc->tl_dmatag, Tx->m_dmamap, mb_head,
	    BUS_DMA_NOWAIT)) || (size < ETHER_MIN_TX &&
	    Tx->m_dmamap->dm_nsegs == TL_NSEG)) {
		struct mbuf *mn;
		/*
		 * We ran out of segments, or we will. We have to recopy this
		 * mbuf chain first.
		 */
		 if (error == 0)
			bus_dmamap_unload(sc->tl_dmatag, Tx->m_dmamap);
		 if (again) {
			/* already copyed, can't do much more */
			m_freem(mb_head);
			goto bad;
		}
		again = 1;
#ifdef TLDEBUG_TX
		printf("%s: need to copy mbuf\n", __func__);
#endif
#ifdef TL_PRIV_STATS
		sc->oerr_mcopy++;
#endif
		MGETHDR(mn, M_DONTWAIT, MT_DATA);
		if (mn == NULL) {
			m_freem(mb_head);
			goto bad;
		}
		if (mb_head->m_pkthdr.len > MHLEN) {
			MCLGET(mn, M_DONTWAIT);
			if ((mn->m_flags & M_EXT) == 0) {
				m_freem(mn);
				m_freem(mb_head);
				goto bad;
			}
		}
		m_copydata(mb_head, 0, mb_head->m_pkthdr.len,
		    mtod(mn, void *));
		mn->m_pkthdr.len = mn->m_len = mb_head->m_pkthdr.len;
		m_freem(mb_head);
		mb_head = mn;
		goto tbdinit;
	}
	for (segment = 0; segment < Tx->m_dmamap->dm_nsegs; segment++) {
		Tx->hw_list->seg[segment].data_addr =
		    htole32(Tx->m_dmamap->dm_segs[segment].ds_addr);
		Tx->hw_list->seg[segment].data_count =
		    htole32(Tx->m_dmamap->dm_segs[segment].ds_len);
	}
	bus_dmamap_sync(sc->tl_dmatag, Tx->m_dmamap, 0,
	    Tx->m_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/* We are at end of mbuf chain. check the size and
	 * see if it needs to be extended
	 */
	if (size < ETHER_MIN_TX) {
#ifdef DIAGNOSTIC
		if (segment >= TL_NSEG) {
			panic("%s: to much segmets (%d)", __func__, segment);
		}
#endif
		/*
	 	 * add the nullbuf in the seg
	 	 */
		Tx->hw_list->seg[segment].data_count =
		    htole32(ETHER_MIN_TX - size);
		Tx->hw_list->seg[segment].data_addr =
		    htole32(sc->null_dmamap->dm_segs[0].ds_addr);
		size = ETHER_MIN_TX;
		segment++;
	}
	/* The list is done, finish the list init */
	Tx->hw_list->seg[segment - 1].data_count |=
	    htole32(TL_LAST_SEG);
	Tx->hw_list->stat = htole32((size << 16) | 0x3000);
#ifdef TLDEBUG_TX
	printf("%s: sending, Tx : stat = 0x%x\n", device_xname(sc->sc_dev),
	    le32toh(Tx->hw_list->stat));
#if 0
	for (segment = 0; segment < TL_NSEG; segment++) {
		printf("    seg %d addr 0x%x len 0x%x\n",
		    segment,
		    le32toh(Tx->hw_list->seg[segment].data_addr),
		    le32toh(Tx->hw_list->seg[segment].data_count));
	}
#endif
#endif
	if (sc->active_Tx == NULL) {
		sc->active_Tx = sc->last_Tx = Tx;
#ifdef TLDEBUG_TX
		printf("%s: Tx GO, addr=0x%ux\n", device_xname(sc->sc_dev),
		    (int)Tx->hw_listaddr);
#endif
		bus_dmamap_sync(sc->tl_dmatag, sc->Tx_dmamap, 0,
		    sizeof(struct tl_Tx_list) * TL_NBUF,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		TL_HR_WRITE(sc, TL_HOST_CH_PARM, Tx->hw_listaddr);
		TL_HR_WRITE(sc, TL_HOST_CMD, HOST_CMD_GO);
	} else {
#ifdef TLDEBUG_TX
		printf("%s: Tx addr=0x%ux queued\n", device_xname(sc->sc_dev),
		    (int)Tx->hw_listaddr);
#endif
		sc->last_Tx->hw_list->fwd = htole32(Tx->hw_listaddr);
		bus_dmamap_sync(sc->tl_dmatag, sc->Tx_dmamap, 0,
		    sizeof(struct tl_Tx_list) * TL_NBUF,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		sc->last_Tx->next = Tx;
		sc->last_Tx = Tx;
#ifdef DIAGNOSTIC
		if (sc->last_Tx->hw_list->fwd & 0x7)
			printf("%s: physical addr 0x%x of list not properly "
			    "aligned\n",
			    device_xname(sc->sc_dev),
			    sc->last_Rx->hw_list->fwd);
#endif
	}
	/* Pass packet to bpf if there is a listener */
	bpf_mtap(ifp, mb_head);
	/*
	 * Set a 5 second timer just in case we don't hear from the card again.
	 */
	ifp->if_timer = 5;
	goto txloop;
bad:
#ifdef TLDEBUG
	printf("%s: Out of mbuf, Tx pkt lost\n", __func__);
#endif
	Tx->next = sc->Free_Tx;
	sc->Free_Tx = Tx;
}

static void
tl_ifwatchdog(struct ifnet *ifp)
{
	tl_softc_t *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
	printf("%s: device timeout\n", device_xname(sc->sc_dev));
	ifp->if_oerrors++;
	tl_init(ifp);
}

static int
tl_mediachange(struct ifnet *ifp)
{

	if (ifp->if_flags & IFF_UP)
		tl_init(ifp);
	return 0;
}

static int
tl_add_RxBuff(tl_softc_t *sc, struct Rx_list *Rx, struct mbuf *oldm)
{
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m != NULL) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			if (oldm == NULL)
				return 0;
			m = oldm;
			m->m_data = m->m_ext.ext_buf;
		}
	} else {
		if (oldm == NULL)
			return 0;
		m = oldm;
		m->m_data = m->m_ext.ext_buf;
	}

	/* (re)init the Rx_list struct */

	Rx->m = m;
	if ((error = bus_dmamap_load(sc->tl_dmatag, Rx->m_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: bus_dmamap_load() failed (error %d) for "
		    "tl_add_RxBuff ", device_xname(sc->sc_dev), error);
		printf("size %d (%d)\n", m->m_pkthdr.len, MCLBYTES);
		m_freem(m);
		Rx->m = NULL;
		return 0;
	}
	bus_dmamap_sync(sc->tl_dmatag, Rx->m_dmamap, 0,
	    Rx->m_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
	/*
	 * Move the data pointer up so that the incoming data packet
	 * will be 32-bit aligned.
	 */
	m->m_data += 2;

	Rx->hw_list->stat =
	    htole32(((Rx->m_dmamap->dm_segs[0].ds_len - 2) << 16) | 0x3000);
	Rx->hw_list->seg.data_count =
	    htole32(Rx->m_dmamap->dm_segs[0].ds_len - 2);
	Rx->hw_list->seg.data_addr =
	    htole32(Rx->m_dmamap->dm_segs[0].ds_addr + 2);
	return (m != oldm);
}

static void
tl_ticks(void *v)
{
	tl_softc_t *sc = v;

	tl_read_stats(sc);

	/* Tick the MII. */
	mii_tick(&sc->tl_mii);

	/* read statistics every seconds */
	callout_reset(&sc->tl_tick_ch, hz, tl_ticks, sc);
}

static void
tl_read_stats(tl_softc_t *sc)
{
	uint32_t reg;
	int ierr_overr;
	int ierr_code;
	int ierr_crc;
	int oerr_underr;
	int oerr_deferred;
	int oerr_coll;
	int oerr_multicoll;
	int oerr_exesscoll;
	int oerr_latecoll;
	int oerr_carrloss;
	struct ifnet *ifp = &sc->tl_if;

	reg =  tl_intreg_read(sc, TL_INT_STATS_TX);
	ifp->if_opackets += reg & 0x00ffffff;
	oerr_underr = reg >> 24;

	reg =  tl_intreg_read(sc, TL_INT_STATS_RX);
	ifp->if_ipackets += reg & 0x00ffffff;
	ierr_overr = reg >> 24;

	reg =  tl_intreg_read(sc, TL_INT_STATS_FERR);
	ierr_crc = (reg & TL_FERR_CRC) >> 16;
	ierr_code = (reg & TL_FERR_CODE) >> 24;
	oerr_deferred = (reg & TL_FERR_DEF);

	reg =  tl_intreg_read(sc, TL_INT_STATS_COLL);
	oerr_multicoll = (reg & TL_COL_MULTI);
	oerr_coll = (reg & TL_COL_SINGLE) >> 16;

	reg =  tl_intreg_read(sc, TL_INT_LERR);
	oerr_exesscoll = (reg & TL_LERR_ECOLL);
	oerr_latecoll = (reg & TL_LERR_LCOLL) >> 8;
	oerr_carrloss = (reg & TL_LERR_CL) >> 16;


	ifp->if_oerrors += oerr_underr + oerr_exesscoll + oerr_latecoll +
	   oerr_carrloss;
	ifp->if_collisions += oerr_coll + oerr_multicoll;
	ifp->if_ierrors += ierr_overr + ierr_code + ierr_crc;

	if (ierr_overr)
		printf("%s: receiver ring buffer overrun\n",
		    device_xname(sc->sc_dev));
	if (oerr_underr)
		printf("%s: transmit buffer underrun\n",
		    device_xname(sc->sc_dev));
#ifdef TL_PRIV_STATS
	sc->ierr_overr		+= ierr_overr;
	sc->ierr_code		+= ierr_code;
	sc->ierr_crc		+= ierr_crc;
	sc->oerr_underr		+= oerr_underr;
	sc->oerr_deferred	+= oerr_deferred;
	sc->oerr_coll		+= oerr_coll;
	sc->oerr_multicoll	+= oerr_multicoll;
	sc->oerr_exesscoll	+= oerr_exesscoll;
	sc->oerr_latecoll	+= oerr_latecoll;
	sc->oerr_carrloss	+= oerr_carrloss;
#endif
}

static void
tl_addr_filter(tl_softc_t *sc)
{
	struct ether_multistep step;
	struct ether_multi *enm;
	uint32_t hash[2] = {0, 0};
	int i;

	sc->tl_if.if_flags &= ~IFF_ALLMULTI;
	ETHER_FIRST_MULTI(step, &sc->tl_ec, enm);
	while (enm != NULL) {
#ifdef TLDEBUG
		printf("%s: addrs %s %s\n", __func__,
		   ether_sprintf(enm->enm_addrlo),
		   ether_sprintf(enm->enm_addrhi));
#endif
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6) == 0) {
			i = tl_multicast_hash(enm->enm_addrlo);
			hash[i / 32] |= 1 << (i%32);
		} else {
			hash[0] = hash[1] = 0xffffffff;
			sc->tl_if.if_flags |= IFF_ALLMULTI;
			break;
		}
		ETHER_NEXT_MULTI(step, enm);
	}
#ifdef TLDEBUG
	printf("%s: hash1 %x has2 %x\n", __func__, hash[0], hash[1]);
#endif
	tl_intreg_write(sc, TL_INT_HASH1, hash[0]);
	tl_intreg_write(sc, TL_INT_HASH2, hash[1]);
}

static int
tl_multicast_hash(uint8_t *a)
{
	int hash;

#define DA(addr,bit) (addr[5 - (bit / 8)] & (1 << (bit % 8)))
#define xor8(a,b,c,d,e,f,g,h)						\
	(((a != 0) + (b != 0) + (c != 0) + (d != 0) + 			\
	  (e != 0) + (f != 0) + (g != 0) + (h != 0)) & 1)

	hash  = xor8(DA(a,0), DA(a, 6), DA(a,12), DA(a,18), DA(a,24), DA(a,30),
	    DA(a,36), DA(a,42));
	hash |= xor8(DA(a,1), DA(a, 7), DA(a,13), DA(a,19), DA(a,25), DA(a,31),
	    DA(a,37), DA(a,43)) << 1;
	hash |= xor8(DA(a,2), DA(a, 8), DA(a,14), DA(a,20), DA(a,26), DA(a,32),
	    DA(a,38), DA(a,44)) << 2;
	hash |= xor8(DA(a,3), DA(a, 9), DA(a,15), DA(a,21), DA(a,27), DA(a,33),
	    DA(a,39), DA(a,45)) << 3;
	hash |= xor8(DA(a,4), DA(a,10), DA(a,16), DA(a,22), DA(a,28), DA(a,34),
	    DA(a,40), DA(a,46)) << 4;
	hash |= xor8(DA(a,5), DA(a,11), DA(a,17), DA(a,23), DA(a,29), DA(a,35),
	    DA(a,41), DA(a,47)) << 5;

	return hash;
}

#if defined(TLDEBUG_RX)
void
ether_printheader(struct ether_header *eh)
{
	uint8_t *c = (uint8_t *)eh;
	int i;

	for (i = 0; i < sizeof(struct ether_header); i++)
		printf("%02x ", (u_int)c[i]);
	printf("\n");
}
#endif
