/*	$NetBSD: if_lii.c,v 1.13 2014/03/29 19:28:24 christos Exp $	*/

/*
 *  Copyright (c) 2008 The NetBSD Foundation.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for Attansic/Atheros's L2 Fast Ethernet controller
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_lii.c,v 1.13 2014/03/29 19:28:24 christos Exp $");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_liireg.h>

/* #define LII_DEBUG */
#ifdef LII_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

struct lii_softc {
	device_t		sc_dev;
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	bus_space_tag_t		sc_mmiot;
	bus_space_handle_t	sc_mmioh;

	/*
	 * We allocate a big chunk of DMA-safe memory for all data exchanges.
	 * It is unfortunate that this chip doesn't seem to do scatter-gather.
	 */
	bus_dma_tag_t		sc_dmat;
	bus_dmamap_t		sc_ringmap;
	bus_dma_segment_t	sc_ringseg;

	uint8_t			*sc_ring; /* the whole area */
	size_t			sc_ringsize;

	struct rx_pkt		*sc_rxp; /* the part used for RX */
	struct tx_pkt_status	*sc_txs; /* the parts used for TX */
	bus_addr_t		sc_txsp;
	char			*sc_txdbase;
	bus_addr_t		sc_txdp;

	unsigned int		sc_rxcur;
	/* the active area is [ack; cur[ */
	int			sc_txs_cur;
	int			sc_txs_ack;
	int			sc_txd_cur;
	int			sc_txd_ack;
	bool			sc_free_tx_slots;

	void			*sc_ih;

	struct ethercom		sc_ec;
	struct mii_data		sc_mii;
	callout_t		sc_tick_ch;
	uint8_t			sc_eaddr[ETHER_ADDR_LEN];

	int			(*sc_memread)(struct lii_softc *, uint32_t,
				     uint32_t *);
};

static int	lii_match(device_t, cfdata_t, void *);
static void	lii_attach(device_t, device_t, void *);

static int	lii_reset(struct lii_softc *);
static bool	lii_eeprom_present(struct lii_softc *);
static int	lii_read_macaddr(struct lii_softc *, uint8_t *);
static int	lii_eeprom_read(struct lii_softc *, uint32_t, uint32_t *);
static void	lii_spi_configure(struct lii_softc *);
static int	lii_spi_read(struct lii_softc *, uint32_t, uint32_t *);
static void	lii_setmulti(struct lii_softc *);
static void	lii_tick(void *);

static int	lii_alloc_rings(struct lii_softc *);
static int	lii_free_tx_space(struct lii_softc *);

static int	lii_mii_readreg(device_t, int, int);
static void	lii_mii_writereg(device_t, int, int, int);
static void	lii_mii_statchg(struct ifnet *);

static int	lii_media_change(struct ifnet *);
static void	lii_media_status(struct ifnet *, struct ifmediareq *);

static int	lii_init(struct ifnet *);
static void	lii_start(struct ifnet *);
static void	lii_stop(struct ifnet *, int);
static void	lii_watchdog(struct ifnet *);
static int	lii_ioctl(struct ifnet *, u_long, void *);

static int	lii_intr(void *);
static void	lii_rxintr(struct lii_softc *);
static void	lii_txintr(struct lii_softc *);

CFATTACH_DECL_NEW(lii, sizeof(struct lii_softc),
    lii_match, lii_attach, NULL, NULL);

/* #define LII_DEBUG_REGS */
#ifndef LII_DEBUG_REGS
#define AT_READ_4(sc,reg) \
    bus_space_read_4((sc)->sc_mmiot, (sc)->sc_mmioh, (reg))
#define AT_READ_2(sc,reg) \
    bus_space_read_2((sc)->sc_mmiot, (sc)->sc_mmioh, (reg))
#define AT_READ_1(sc,reg) \
    bus_space_read_1((sc)->sc_mmiot, (sc)->sc_mmioh, (reg))
#define AT_WRITE_4(sc,reg,val) \
    bus_space_write_4((sc)->sc_mmiot, (sc)->sc_mmioh, (reg), (val))
#define AT_WRITE_2(sc,reg,val) \
    bus_space_write_2((sc)->sc_mmiot, (sc)->sc_mmioh, (reg), (val))
#define AT_WRITE_1(sc,reg,val) \
    bus_space_write_1((sc)->sc_mmiot, (sc)->sc_mmioh, (reg), (val))
#else
static inline uint32_t
AT_READ_4(struct lii_softc *sc, bus_size_t reg)
{
	uint32_t r = bus_space_read_4(sc->sc_mmiot, sc->sc_mmioh, reg);
	printf("AT_READ_4(%x) = %x\n", (unsigned int)reg, r);
	return r;
}

static inline uint16_t
AT_READ_2(struct lii_softc *sc, bus_size_t reg)
{
	uint16_t r = bus_space_read_2(sc->sc_mmiot, sc->sc_mmioh, reg);
	printf("AT_READ_2(%x) = %x\n", (unsigned int)reg, r);
	return r;
}

static inline uint8_t
AT_READ_1(struct lii_softc *sc, bus_size_t reg)
{
	uint8_t r = bus_space_read_1(sc->sc_mmiot, sc->sc_mmioh, reg);
	printf("AT_READ_1(%x) = %x\n", (unsigned int)reg, r);
	return r;
}

static inline void
AT_WRITE_4(struct lii_softc *sc, bus_size_t reg, uint32_t val)
{
	printf("AT_WRITE_4(%x, %x)\n", (unsigned int)reg, val);
	bus_space_write_4(sc->sc_mmiot, sc->sc_mmioh, reg, val);
}

static inline void
AT_WRITE_2(struct lii_softc *sc, bus_size_t reg, uint16_t val)
{
	printf("AT_WRITE_2(%x, %x)\n", (unsigned int)reg, val);
	bus_space_write_2(sc->sc_mmiot, sc->sc_mmioh, reg, val);
}

static inline void
AT_WRITE_1(struct lii_softc *sc, bus_size_t reg, uint8_t val)
{
	printf("AT_WRITE_1(%x, %x)\n", (unsigned int)reg, val);
	bus_space_write_1(sc->sc_mmiot, sc->sc_mmioh, reg, val);
}
#endif

/*
 * Those are the default Linux parameters.
 */

#define AT_TXD_NUM		64
#define AT_TXD_BUFFER_SIZE	8192
#define AT_RXD_NUM		64

/*
 * Assuming (you know what that word makes of you) the chunk of memory
 * bus_dmamem_alloc returns us is 128-byte aligned, we won't use the
 * first 120 bytes of it, so that the space for the packets, and not the
 * whole descriptors themselves, are on a 128-byte boundary.
 */

#define AT_RXD_PADDING		120

static int
lii_match(device_t parent, cfdata_t cfmatch, void *aux)
{
	struct pci_attach_args *pa = aux;

	return (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ATTANSIC &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATTANSIC_ETHERNET_100);
}

static void
lii_attach(device_t parent, device_t self, void *aux)
{
	struct lii_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	uint8_t eaddr[ETHER_ADDR_LEN];
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t cmd;
	bus_size_t memsize = 0;
	char intrbuf[PCI_INTRSTR_LEN];

	aprint_naive("\n");
	aprint_normal(": Attansic/Atheros L2 Fast Ethernet\n");

	sc->sc_dev = self;
	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	cmd = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	cmd &= ~PCI_COMMAND_IO_ENABLE;
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG, cmd);

	switch (cmd = pci_mapreg_type(sc->sc_pc, sc->sc_tag, PCI_MAPREG_START)) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT_1M:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		break;
	default:
		aprint_error_dev(self, "invalid base address register\n");
		break;
	}
	if (pci_mapreg_map(pa, PCI_MAPREG_START, cmd, 0,
	    &sc->sc_mmiot, &sc->sc_mmioh, NULL, &memsize) != 0) {
		aprint_error_dev(self, "failed to map registers\n");
		return;
	}

	if (lii_reset(sc))
		return;

	lii_spi_configure(sc);

	if (lii_eeprom_present(sc))
		sc->sc_memread = lii_eeprom_read;
	else
		sc->sc_memread = lii_spi_read;

	if (lii_read_macaddr(sc, eaddr))
		return;
	memcpy(sc->sc_eaddr, eaddr, ETHER_ADDR_LEN);

	aprint_normal_dev(self, "Ethernet address %s\n",
	    ether_sprintf(eaddr));

	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "failed to map interrupt\n");
		goto fail;
	}
	intrstr = pci_intr_string(sc->sc_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_NET, lii_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "failed to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto fail;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	if (lii_alloc_rings(sc))
		goto fail;

	callout_init(&sc->sc_tick_ch, 0);
	callout_setfunc(&sc->sc_tick_ch, lii_tick, sc);

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = lii_mii_readreg;
	sc->sc_mii.mii_writereg = lii_mii_writereg;
	sc->sc_mii.mii_statchg = lii_mii_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, lii_media_change,
	    lii_media_status);
	mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, 1,
	    MII_OFFSET_ANY, 0);
	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	strlcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = lii_ioctl;
	ifp->if_start = lii_start;
	ifp->if_watchdog = lii_watchdog;
	ifp->if_init = lii_init;
	ifp->if_stop = lii_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * While the device does support HW VLAN tagging, there is no
	 * real point using that feature.
	 */
	sc->sc_ec.ec_capabilities = ETHERCAP_VLAN_MTU;

	if_attach(ifp);
	ether_ifattach(ifp, eaddr);

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

fail:
	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (memsize)
		bus_space_unmap(sc->sc_mmiot, sc->sc_mmioh, memsize);
}

static int
lii_reset(struct lii_softc *sc)
{
	int i;

	DPRINTF(("lii_reset\n"));

	AT_WRITE_4(sc, ATL2_SMC, SMC_SOFT_RST);
	DELAY(1000);

	for (i = 0; i < 10; ++i) {
		if (AT_READ_4(sc, ATL2_BIS) == 0)
			break;
		DELAY(1000);
	}

	if (i == 10) {
		aprint_error_dev(sc->sc_dev, "reset failed\n");
		return 1;
	}

	AT_WRITE_4(sc, ATL2_PHYC, PHYC_ENABLE);
	DELAY(10);

	/* Init PCI-Express module */
	/* Magic Numbers Warning */
	AT_WRITE_4(sc, ATL2_PCELTM, PCELTM_DEF);
	AT_WRITE_4(sc, ATL2_PCEDTXC, PCEDTX_DEF);

	return 0;
}

static bool
lii_eeprom_present(struct lii_softc *sc)
{
	/*
	 * The Linux driver does this, but then it has a very weird way of
	 * checking whether the PCI configuration space exposes the Vital
	 * Product Data capability, so maybe it's not really needed.
	 */

#ifdef weirdloonix
	uint32_t val;

	val = AT_READ_4(sc, ATL2_SFC);
	if (val & SFC_EN_VPD)
		AT_WRITE_4(sc, ATL2_SFC, val & ~(SFC_EN_VPD));
#endif

	return pci_get_capability(sc->sc_pc, sc->sc_tag, PCI_CAP_VPD,
	    NULL, NULL) == 1;
}

static int
lii_eeprom_read(struct lii_softc *sc, uint32_t reg, uint32_t *val)
{
	int r = pci_vpd_read(sc->sc_pc, sc->sc_tag, reg, 1, (pcireg_t *)val);

	DPRINTF(("lii_eeprom_read(%x) = %x\n", reg, *val));

	return r;
}

static void
lii_spi_configure(struct lii_softc *sc)
{
	/*
	 * We don't offer a way to configure the SPI Flash vendor parameter, so
	 * the table is given for reference
	 */
	static const struct lii_spi_flash_vendor {
	    const char *sfv_name;
	    const uint8_t sfv_opcodes[9];
	} lii_sfv[] = {
	    { "Atmel", { 0x00, 0x03, 0x02, 0x06, 0x04, 0x05, 0x15, 0x52, 0x62 } },
	    { "SST",   { 0x01, 0x03, 0x02, 0x06, 0x04, 0x05, 0x90, 0x20, 0x60 } },
	    { "ST",    { 0x01, 0x03, 0x02, 0x06, 0x04, 0x05, 0xab, 0xd8, 0xc7 } },
	};
#define SF_OPCODE_WRSR	0
#define SF_OPCODE_READ	1
#define SF_OPCODE_PRGM	2
#define SF_OPCODE_WREN	3
#define SF_OPCODE_WRDI	4
#define SF_OPCODE_RDSR	5
#define SF_OPCODE_RDID	6
#define SF_OPCODE_SECT_ER	7
#define SF_OPCODE_CHIP_ER	8

#define SF_DEFAULT_VENDOR	0
	static const uint8_t vendor = SF_DEFAULT_VENDOR;

	/*
	 * Why isn't WRDI used?  Heck if I know.
	 */

	AT_WRITE_1(sc, ATL2_SFOP_WRSR,
	    lii_sfv[vendor].sfv_opcodes[SF_OPCODE_WRSR]);
	AT_WRITE_1(sc, ATL2_SFOP_READ,
	    lii_sfv[vendor].sfv_opcodes[SF_OPCODE_READ]);
	AT_WRITE_1(sc, ATL2_SFOP_PROGRAM,
	    lii_sfv[vendor].sfv_opcodes[SF_OPCODE_PRGM]);
	AT_WRITE_1(sc, ATL2_SFOP_WREN,
	    lii_sfv[vendor].sfv_opcodes[SF_OPCODE_WREN]);
	AT_WRITE_1(sc, ATL2_SFOP_RDSR,
	    lii_sfv[vendor].sfv_opcodes[SF_OPCODE_RDSR]);
	AT_WRITE_1(sc, ATL2_SFOP_RDID,
	    lii_sfv[vendor].sfv_opcodes[SF_OPCODE_RDID]);
	AT_WRITE_1(sc, ATL2_SFOP_SC_ERASE,
	    lii_sfv[vendor].sfv_opcodes[SF_OPCODE_SECT_ER]);
	AT_WRITE_1(sc, ATL2_SFOP_CHIP_ERASE,
	    lii_sfv[vendor].sfv_opcodes[SF_OPCODE_CHIP_ER]);
}

#define MAKE_SFC(cssetup, clkhi, clklo, cshold, cshi, ins) \
    ( (((cssetup) & SFC_CS_SETUP_MASK)	\
        << SFC_CS_SETUP_SHIFT)		\
    | (((clkhi) & SFC_CLK_HI_MASK)	\
        << SFC_CLK_HI_SHIFT)		\
    | (((clklo) & SFC_CLK_LO_MASK)	\
        << SFC_CLK_LO_SHIFT)		\
    | (((cshold) & SFC_CS_HOLD_MASK)	\
        << SFC_CS_HOLD_SHIFT)		\
    | (((cshi) & SFC_CS_HI_MASK)	\
        << SFC_CS_HI_SHIFT)		\
    | (((ins) & SFC_INS_MASK)		\
        << SFC_INS_SHIFT))

/* Magic settings from the Linux driver */

#define CUSTOM_SPI_CS_SETUP	2
#define CUSTOM_SPI_CLK_HI	2
#define CUSTOM_SPI_CLK_LO	2
#define CUSTOM_SPI_CS_HOLD	2
#define CUSTOM_SPI_CS_HI	3

static int
lii_spi_read(struct lii_softc *sc, uint32_t reg, uint32_t *val)
{
	uint32_t v;
	int i;

	AT_WRITE_4(sc, ATL2_SF_DATA, 0);
	AT_WRITE_4(sc, ATL2_SF_ADDR, reg);

	v = SFC_WAIT_READY |
	    MAKE_SFC(CUSTOM_SPI_CS_SETUP, CUSTOM_SPI_CLK_HI,
	         CUSTOM_SPI_CLK_LO, CUSTOM_SPI_CS_HOLD, CUSTOM_SPI_CS_HI, 1);

	AT_WRITE_4(sc, ATL2_SFC, v);
	v |= SFC_START;
	AT_WRITE_4(sc, ATL2_SFC, v);

	for (i = 0; i < 10; ++i) {
		DELAY(1000);
		if (!(AT_READ_4(sc, ATL2_SFC) & SFC_START))
			break;
	}
	if (i == 10)
		return EBUSY;

	*val = AT_READ_4(sc, ATL2_SF_DATA);
	return 0;
}

static int
lii_read_macaddr(struct lii_softc *sc, uint8_t *ea)
{
	uint32_t offset = 0x100;
	uint32_t val, val1, addr0 = 0, addr1 = 0;
	uint8_t found = 0;

	while ((*sc->sc_memread)(sc, offset, &val) == 0) {
		offset += 4;

		/* Each chunk of data starts with a signature */
		if ((val & 0xff) != 0x5a)
			break;
		if ((*sc->sc_memread)(sc, offset, &val1))
			break;

		offset += 4;

		val >>= 16;
		switch (val) {
		case ATL2_MAC_ADDR_0:
			addr0 = val1;
			++found;
			break;
		case ATL2_MAC_ADDR_1:
			addr1 = val1;
			++found;
			break;
		default:
			continue;
		}
	}

	if (found < 2) {
		/* Make sure we try the BIOS method before giving up */
		addr0 = htole32(AT_READ_4(sc, ATL2_MAC_ADDR_0));
		addr1 = htole32(AT_READ_4(sc, ATL2_MAC_ADDR_1));
		if ((addr0 == 0xffffff && (addr1 & 0xffff) == 0xffff) ||
		    (addr0 == 0 && (addr1 & 0xffff) == 0)) {
			aprint_error_dev(sc->sc_dev,
			    "error reading MAC address\n");
			return 1;
		}
	} else {
		addr0 = htole32(addr0);
		addr1 = htole32(addr1);
	}

	ea[0] = (addr1 & 0x0000ff00) >> 8;
	ea[1] = (addr1 & 0x000000ff);
	ea[2] = (addr0 & 0xff000000) >> 24;
	ea[3] = (addr0 & 0x00ff0000) >> 16;
	ea[4] = (addr0 & 0x0000ff00) >> 8;
	ea[5] = (addr0 & 0x000000ff);

	return 0;
}

static int
lii_mii_readreg(device_t dev, int phy, int reg)
{
	struct lii_softc *sc = device_private(dev);
	uint32_t val;
	int i;

	val = (reg & MDIOC_REG_MASK) << MDIOC_REG_SHIFT;

	val |= MDIOC_START | MDIOC_SUP_PREAMBLE;
	val |= MDIOC_CLK_25_4 << MDIOC_CLK_SEL_SHIFT;

	val |= MDIOC_READ;

	AT_WRITE_4(sc, ATL2_MDIOC, val);

	for (i = 0; i < MDIO_WAIT_TIMES; ++i) {
		DELAY(2);
		val = AT_READ_4(sc, ATL2_MDIOC);
		if ((val & (MDIOC_START | MDIOC_BUSY)) == 0)
			break;
	}

	if (i == MDIO_WAIT_TIMES)
		aprint_error_dev(dev, "timeout reading PHY %d reg %d\n", phy,
		    reg);

	return (val & 0x0000ffff);
}

static void
lii_mii_writereg(device_t dev, int phy, int reg, int data)
{
	struct lii_softc *sc = device_private(dev);
	uint32_t val;
	int i;

	val = (reg & MDIOC_REG_MASK) << MDIOC_REG_SHIFT;
	val |= (data & MDIOC_DATA_MASK) << MDIOC_DATA_SHIFT;

	val |= MDIOC_START | MDIOC_SUP_PREAMBLE;
	val |= MDIOC_CLK_25_4 << MDIOC_CLK_SEL_SHIFT;

	/* val |= MDIOC_WRITE; */

	AT_WRITE_4(sc, ATL2_MDIOC, val);

	for (i = 0; i < MDIO_WAIT_TIMES; ++i) {
		DELAY(2);
		val = AT_READ_4(sc, ATL2_MDIOC);
		if ((val & (MDIOC_START | MDIOC_BUSY)) == 0)
			break;
	}

	if (i == MDIO_WAIT_TIMES)
		aprint_error_dev(dev, "timeout writing PHY %d reg %d\n", phy,
		    reg);
}

static void
lii_mii_statchg(struct ifnet *ifp)
{
	struct lii_softc *sc = ifp->if_softc;
	uint32_t val;

	DPRINTF(("lii_mii_statchg\n"));

	val = AT_READ_4(sc, ATL2_MACC);

	if ((sc->sc_mii.mii_media_active & IFM_GMASK) == IFM_FDX)
		val |= MACC_FDX;
	else
		val &= ~MACC_FDX;

	AT_WRITE_4(sc, ATL2_MACC, val);
}

static int
lii_media_change(struct ifnet *ifp)
{
	struct lii_softc *sc = ifp->if_softc;

	DPRINTF(("lii_media_change\n"));

	if (ifp->if_flags & IFF_UP)
		mii_mediachg(&sc->sc_mii);
	return 0;
}

static void
lii_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct lii_softc *sc = ifp->if_softc;

	DPRINTF(("lii_media_status\n"));

	mii_pollstat(&sc->sc_mii);
	imr->ifm_status = sc->sc_mii.mii_media_status;
	imr->ifm_active = sc->sc_mii.mii_media_active;
}

static int
lii_init(struct ifnet *ifp)
{
	struct lii_softc *sc = ifp->if_softc;
	uint32_t val;
	int error;

	DPRINTF(("lii_init\n"));

	lii_stop(ifp, 0);

	memset(sc->sc_ring, 0, sc->sc_ringsize);

	/* Disable all interrupts */
	AT_WRITE_4(sc, ATL2_ISR, 0xffffffff);

	/* XXX endianness */
	AT_WRITE_4(sc, ATL2_MAC_ADDR_0,
	    sc->sc_eaddr[2] << 24 |
	    sc->sc_eaddr[3] << 16 |
	    sc->sc_eaddr[4] << 8 |
	    sc->sc_eaddr[5]);
	AT_WRITE_4(sc, ATL2_MAC_ADDR_1,
	    sc->sc_eaddr[0] << 8 |
	    sc->sc_eaddr[1]);

	AT_WRITE_4(sc, ATL2_DESC_BASE_ADDR_HI, 0);
/* XXX
	    sc->sc_ringmap->dm_segs[0].ds_addr >> 32);
*/
	AT_WRITE_4(sc, ATL2_RXD_BASE_ADDR_LO,
	    (sc->sc_ringmap->dm_segs[0].ds_addr & 0xffffffff)
	    + AT_RXD_PADDING);
	AT_WRITE_4(sc, ATL2_TXS_BASE_ADDR_LO,
	    sc->sc_txsp & 0xffffffff);
	AT_WRITE_4(sc, ATL2_TXD_BASE_ADDR_LO,
	    sc->sc_txdp & 0xffffffff);

	AT_WRITE_2(sc, ATL2_TXD_BUFFER_SIZE, AT_TXD_BUFFER_SIZE / 4);
	AT_WRITE_2(sc, ATL2_TXS_NUM_ENTRIES, AT_TXD_NUM);
	AT_WRITE_2(sc, ATL2_RXD_NUM_ENTRIES, AT_RXD_NUM);

	/*
	 * Inter Paket Gap Time = 0x60 (IPGT)
	 * Minimum inter-frame gap for RX = 0x50 (MIFG)
	 * 64-bit Carrier-Sense window = 0x40 (IPGR1)
	 * 96-bit IPG window = 0x60 (IPGR2)
	 */
	AT_WRITE_4(sc, ATL2_MIPFG, 0x60405060);

	/*
	 * Collision window = 0x37 (LCOL)
	 * Maximum # of retrans = 0xf (RETRY)
	 * Maximum binary expansion # = 0xa (ABEBT)
	 * IPG to start jam = 0x7 (JAMIPG)
	*/
	AT_WRITE_4(sc, ATL2_MHDC, 0x07a0f037 |
	     MHDC_EXC_DEF_EN);

	/* 100 means 200us */
	AT_WRITE_2(sc, ATL2_IMTIV, 100);
	AT_WRITE_2(sc, ATL2_SMC, SMC_ITIMER_EN);

	/* 500000 means 100ms */
	AT_WRITE_2(sc, ATL2_IALTIV, 50000);

	AT_WRITE_4(sc, ATL2_MTU, ifp->if_mtu + ETHER_HDR_LEN
	    + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN);

	/* unit unknown for TX cur-through threshold */
	AT_WRITE_4(sc, ATL2_TX_CUT_THRESH, 0x177);

	AT_WRITE_2(sc, ATL2_PAUSE_ON_TH, AT_RXD_NUM * 7 / 8);
	AT_WRITE_2(sc, ATL2_PAUSE_OFF_TH, AT_RXD_NUM / 12);

	sc->sc_rxcur = 0;
	sc->sc_txs_cur = sc->sc_txs_ack = 0;
	sc->sc_txd_cur = sc->sc_txd_ack = 0;
	sc->sc_free_tx_slots = true;
	AT_WRITE_2(sc, ATL2_MB_TXD_WR_IDX, sc->sc_txd_cur);
	AT_WRITE_2(sc, ATL2_MB_RXD_RD_IDX, sc->sc_rxcur);

	AT_WRITE_1(sc, ATL2_DMAR, DMAR_EN);
	AT_WRITE_1(sc, ATL2_DMAW, DMAW_EN);

	AT_WRITE_4(sc, ATL2_SMC, AT_READ_4(sc, ATL2_SMC) | SMC_MANUAL_INT);

	error = ((AT_READ_4(sc, ATL2_ISR) & ISR_PHY_LINKDOWN) != 0);
	AT_WRITE_4(sc, ATL2_ISR, 0x3fffffff);
	AT_WRITE_4(sc, ATL2_ISR, 0);
	if (error) {
		aprint_error_dev(sc->sc_dev, "init failed\n");
		goto out;
	}

	lii_setmulti(sc);

	val = AT_READ_4(sc, ATL2_MACC) & MACC_FDX;

	val |= MACC_RX_EN | MACC_TX_EN | MACC_MACLP_CLK_PHY |
	    MACC_TX_FLOW_EN | MACC_RX_FLOW_EN |
	    MACC_ADD_CRC | MACC_PAD | MACC_BCAST_EN;

	if (ifp->if_flags & IFF_PROMISC)
		val |= MACC_PROMISC_EN;
	else if (ifp->if_flags & IFF_ALLMULTI)
		val |= MACC_ALLMULTI_EN;

	val |= 7 << MACC_PREAMBLE_LEN_SHIFT;
	val |= 2 << MACC_HDX_LEFT_BUF_SHIFT;

	AT_WRITE_4(sc, ATL2_MACC, val);

	mii_mediachg(&sc->sc_mii);

	AT_WRITE_4(sc, ATL2_IMR, IMR_NORMAL_MASK);

	callout_schedule(&sc->sc_tick_ch, hz);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

out:
	return error;
}

static void
lii_tx_put(struct lii_softc *sc, struct mbuf *m)
{
	int left;
	struct tx_pkt_header *tph =
	    (struct tx_pkt_header *)(sc->sc_txdbase + sc->sc_txd_cur);

	memset(tph, 0, sizeof *tph);
	tph->txph_size = m->m_pkthdr.len;

	sc->sc_txd_cur = (sc->sc_txd_cur + 4) % AT_TXD_BUFFER_SIZE;

	/*
	 * We already know we have enough space, so if there is a part of the
	 * space ahead of txd_cur that is active, it doesn't matter because
	 * left will be large enough even without it.
	 */
	left  = AT_TXD_BUFFER_SIZE - sc->sc_txd_cur;

	if (left > m->m_pkthdr.len) {
		m_copydata(m, 0, m->m_pkthdr.len,
		    sc->sc_txdbase + sc->sc_txd_cur);
		sc->sc_txd_cur += m->m_pkthdr.len;
	} else {
		m_copydata(m, 0, left, sc->sc_txdbase + sc->sc_txd_cur);
		m_copydata(m, left, m->m_pkthdr.len - left, sc->sc_txdbase);
		sc->sc_txd_cur = m->m_pkthdr.len - left;
	}

	/* Round to a 32-bit boundary */
	sc->sc_txd_cur = ((sc->sc_txd_cur + 3) & ~3) % AT_TXD_BUFFER_SIZE;
	if (sc->sc_txd_cur == sc->sc_txd_ack)
		sc->sc_free_tx_slots = false;
}

static int
lii_free_tx_space(struct lii_softc *sc)
{
	int space;

	if (sc->sc_txd_cur >= sc->sc_txd_ack)
		space = (AT_TXD_BUFFER_SIZE - sc->sc_txd_cur) +
		    sc->sc_txd_ack;
	else
		space = sc->sc_txd_ack - sc->sc_txd_cur;

	/* Account for the tx_pkt_header */
	return (space - 4);
}

static void
lii_start(struct ifnet *ifp)
{
	struct lii_softc *sc = ifp->if_softc;
	struct mbuf *m0;

	DPRINTF(("lii_start\n"));

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (!sc->sc_free_tx_slots ||
		    lii_free_tx_space(sc) < m0->m_pkthdr.len) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		lii_tx_put(sc, m0);

		DPRINTF(("lii_start: put %d\n", sc->sc_txs_cur));

		sc->sc_txs[sc->sc_txs_cur].txps_update = 0;
		sc->sc_txs_cur = (sc->sc_txs_cur + 1) % AT_TXD_NUM;
		if (sc->sc_txs_cur == sc->sc_txs_ack)
			sc->sc_free_tx_slots = false;

		AT_WRITE_2(sc, ATL2_MB_TXD_WR_IDX, sc->sc_txd_cur/4);

		IFQ_DEQUEUE(&ifp->if_snd, m0);

		bpf_mtap(ifp, m0);
		m_freem(m0);
	}
}

static void
lii_stop(struct ifnet *ifp, int disable)
{
	struct lii_softc *sc = ifp->if_softc;

	callout_stop(&sc->sc_tick_ch);

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	mii_down(&sc->sc_mii);

	lii_reset(sc);

	AT_WRITE_4(sc, ATL2_IMR, 0);
}

static int
lii_intr(void *v)
{
	struct lii_softc *sc = v;
	uint32_t status;

	status = AT_READ_4(sc, ATL2_ISR);
	if (status == 0)
		return 0;

	DPRINTF(("lii_intr (%x)\n", status));

	/* Clear the interrupt and disable them */
	AT_WRITE_4(sc, ATL2_ISR, status | ISR_DIS_INT);

	if (status & (ISR_PHY | ISR_MANUAL)) {
		/* Ack PHY interrupt.  Magic register */
		if (status & ISR_PHY)
			(void)lii_mii_readreg(sc->sc_dev, 1, 19);
		mii_mediachg(&sc->sc_mii);
	}

	if (status & (ISR_DMAR_TO_RST | ISR_DMAW_TO_RST | ISR_PHY_LINKDOWN)) {
		lii_init(&sc->sc_ec.ec_if);
		return 1;
	}

	if (status & ISR_RX_EVENT) {
#ifdef LII_DEBUG
		if (!(status & ISR_RS_UPDATE))
			printf("rxintr %08x\n", status);
#endif
		lii_rxintr(sc);
	}

	if (status & ISR_TX_EVENT)
		lii_txintr(sc);

	/* Re-enable interrupts */
	AT_WRITE_4(sc, ATL2_ISR, 0);

	return 1;
}

static void
lii_rxintr(struct lii_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct rx_pkt *rxp;
	struct mbuf *m;
	uint16_t size;

	DPRINTF(("lii_rxintr\n"));

	for (;;) {
		rxp = &sc->sc_rxp[sc->sc_rxcur];
		if (rxp->rxp_update == 0)
			break;

		DPRINTF(("lii_rxintr: getting %u (%u) [%x]\n", sc->sc_rxcur,
		    rxp->rxp_size, rxp->rxp_flags));
		sc->sc_rxcur = (sc->sc_rxcur + 1) % AT_RXD_NUM;
		rxp->rxp_update = 0;
		if (!(rxp->rxp_flags & ATL2_RXF_SUCCESS)) {
			++ifp->if_ierrors;
			continue;
		}

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			++ifp->if_ierrors;
			continue;
		}
		size = rxp->rxp_size - ETHER_CRC_LEN;
		if (size > MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				++ifp->if_ierrors;
				continue;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		/* Copy the packet withhout the FCS */
		m->m_pkthdr.len = m->m_len = size;
		memcpy(mtod(m, void *), &rxp->rxp_data[0], size);
		++ifp->if_ipackets;

		bpf_mtap(ifp, m);

		(*ifp->if_input)(ifp, m);
	}

	AT_WRITE_4(sc, ATL2_MB_RXD_RD_IDX, sc->sc_rxcur);
}

static void
lii_txintr(struct lii_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct tx_pkt_status *txs;
	struct tx_pkt_header *txph;

	DPRINTF(("lii_txintr\n"));

	for (;;) {
		txs = &sc->sc_txs[sc->sc_txs_ack];
		if (txs->txps_update == 0)
			break;
		DPRINTF(("lii_txintr: ack'd %d\n", sc->sc_txs_ack));
		sc->sc_txs_ack = (sc->sc_txs_ack + 1) % AT_TXD_NUM;
		sc->sc_free_tx_slots = true;

		txs->txps_update = 0;

		txph =  (struct tx_pkt_header *)
		    (sc->sc_txdbase + sc->sc_txd_ack);

		if (txph->txph_size != txs->txps_size)
			aprint_error_dev(sc->sc_dev,
			    "mismatched status and packet\n");
		/*
		 * Move ack by the packet size, taking the packet header in
		 * account and round to the next 32-bit boundary
		 * (7 = sizeof(header) + 3)
		 */
		sc->sc_txd_ack = (sc->sc_txd_ack + txph->txph_size + 7 ) & ~3;
		sc->sc_txd_ack %= AT_TXD_BUFFER_SIZE;

		if (txs->txps_flags & ATL2_TXF_SUCCESS)
			++ifp->if_opackets;
		else
			++ifp->if_oerrors;
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	if (sc->sc_free_tx_slots)
		lii_start(ifp);
}

static int
lii_alloc_rings(struct lii_softc *sc)
{
	int nsegs;
	bus_size_t bs;

	/*
	 * We need a big chunk of DMA-friendly memory because descriptors
	 * are not separate from data on that crappy hardware, which means
	 * we'll have to copy data from and to that memory zone to and from
	 * the mbufs.
	 *
	 * How lame is that?  Using the default values from the Linux driver,
	 * we allocate space for receiving up to 64 full-size Ethernet frames,
	 * and only 8kb for transmitting up to 64 Ethernet frames.
	 */

	sc->sc_ringsize = bs = AT_RXD_PADDING
	    + AT_RXD_NUM * sizeof(struct rx_pkt)
	    + AT_TXD_NUM * sizeof(struct tx_pkt_status)
	    + AT_TXD_BUFFER_SIZE;

	if (bus_dmamap_create(sc->sc_dmat, bs, 1, bs, (1<<30),
	    BUS_DMA_NOWAIT, &sc->sc_ringmap) != 0) {
		aprint_error_dev(sc->sc_dev, "bus_dmamap_create failed\n");
		return 1;
	}

	if (bus_dmamem_alloc(sc->sc_dmat, bs, PAGE_SIZE, (1<<30),
	    &sc->sc_ringseg, 1, &nsegs, BUS_DMA_NOWAIT) != 0) {
		aprint_error_dev(sc->sc_dev, "bus_dmamem_alloc failed\n");
		goto fail;
	}

	if (bus_dmamem_map(sc->sc_dmat, &sc->sc_ringseg, nsegs, bs,
	    (void **)&sc->sc_ring, BUS_DMA_NOWAIT) != 0) {
		aprint_error_dev(sc->sc_dev, "bus_dmamem_map failed\n");
		goto fail1;
	}

	if (bus_dmamap_load(sc->sc_dmat, sc->sc_ringmap, sc->sc_ring,
	    bs, NULL, BUS_DMA_NOWAIT) != 0) {
		aprint_error_dev(sc->sc_dev, "bus_dmamap_load failed\n");
		goto fail2;
	}

	sc->sc_rxp = (void *)(sc->sc_ring + AT_RXD_PADDING);
	sc->sc_txs = (void *)(sc->sc_ring + AT_RXD_PADDING
	    + AT_RXD_NUM * sizeof(struct rx_pkt));
	sc->sc_txdbase = ((char *)sc->sc_txs)
	    + AT_TXD_NUM * sizeof(struct tx_pkt_status);
	sc->sc_txsp = sc->sc_ringmap->dm_segs[0].ds_addr
	    + ((char *)sc->sc_txs - (char *)sc->sc_ring);
	sc->sc_txdp = sc->sc_ringmap->dm_segs[0].ds_addr
	    + ((char *)sc->sc_txdbase - (char *)sc->sc_ring);

	return 0;

fail2:
	bus_dmamem_unmap(sc->sc_dmat, sc->sc_ring, bs);
fail1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_ringseg, nsegs);
fail:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ringmap);
	return 1;
}

static void
lii_watchdog(struct ifnet *ifp)
{
	struct lii_softc *sc = ifp->if_softc;

	aprint_error_dev(sc->sc_dev, "watchdog timeout\n");
	++ifp->if_oerrors;
	lii_init(ifp);
}

static int
lii_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct lii_softc *sc = ifp->if_softc;
	int s, error;

	s = splnet();

	switch(cmd) {
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				lii_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, (struct ifreq *)data,
		    &sc->sc_mii.mii_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				lii_setmulti(sc);
			error = 0;
		}
		break;
	}

	splx(s);

	return error;
}

static void
lii_setmulti(struct lii_softc *sc)
{
	struct ethercom *ec = &sc->sc_ec;
	struct ifnet *ifp = &ec->ec_if;
	uint32_t mht0 = 0, mht1 = 0, crc;
	struct ether_multi *enm;
	struct ether_multistep step;

	/* Clear multicast hash table */
	AT_WRITE_4(sc, ATL2_MHT, 0);
	AT_WRITE_4(sc, ATL2_MHT + 4, 0);

	ifp->if_flags &= ~IFF_ALLMULTI;

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			mht0 = mht1 = 0;
			goto alldone;
		}

		crc = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN);

		if (crc & (1 << 31))
			mht1 |= (1 << ((crc >> 26) & 0x0000001f));
		else
			mht0 |= (1 << ((crc >> 26) & 0x0000001f));

	     ETHER_NEXT_MULTI(step, enm);
	}

alldone:
	AT_WRITE_4(sc, ATL2_MHT, mht0);
	AT_WRITE_4(sc, ATL2_MHT+4, mht1);
}

static void
lii_tick(void *v)
{
	struct lii_softc *sc = v;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_schedule(&sc->sc_tick_ch, hz);
}
