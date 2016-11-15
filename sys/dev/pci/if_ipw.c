/*	$NetBSD: if_ipw.c,v 1.58 2015/01/07 07:05:48 ozaki-r Exp $	*/
/*	FreeBSD: src/sys/dev/ipw/if_ipw.c,v 1.15 2005/11/13 17:17:40 damien Exp 	*/

/*-
 * Copyright (c) 2004, 2005
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ipw.c,v 1.58 2015/01/07 07:05:48 ozaki-r Exp $");

/*-
 * Intel(R) PRO/Wireless 2100 MiniPCI driver
 * http://www.intel.com/network/connectivity/products/wireless/prowireless_mobile.htm
 */


#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/proc.h>

#include <sys/bus.h>
#include <machine/endian.h>
#include <sys/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <dev/firmload.h>

#include <dev/pci/if_ipwreg.h>
#include <dev/pci/if_ipwvar.h>

#ifdef IPW_DEBUG
#define DPRINTF(x)	if (ipw_debug > 0) printf x
#define DPRINTFN(n, x)	if (ipw_debug >= (n)) printf x
int ipw_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/* Permit loading the Intel firmware */
static int ipw_accept_eula;

static int	ipw_dma_alloc(struct ipw_softc *);
static void	ipw_release(struct ipw_softc *);
static int	ipw_match(device_t, cfdata_t, void *);
static void	ipw_attach(device_t, device_t, void *);
static int	ipw_detach(device_t, int);

static int	ipw_media_change(struct ifnet *);
static void	ipw_media_status(struct ifnet *, struct ifmediareq *);
static int	ipw_newstate(struct ieee80211com *, enum ieee80211_state, int);
static uint16_t	ipw_read_prom_word(struct ipw_softc *, uint8_t);
static void	ipw_command_intr(struct ipw_softc *, struct ipw_soft_buf *);
static void	ipw_newstate_intr(struct ipw_softc *, struct ipw_soft_buf *);
static void	ipw_data_intr(struct ipw_softc *, struct ipw_status *,
    struct ipw_soft_bd *, struct ipw_soft_buf *);
static void	ipw_rx_intr(struct ipw_softc *);
static void	ipw_release_sbd(struct ipw_softc *, struct ipw_soft_bd *);
static void	ipw_tx_intr(struct ipw_softc *);
static int	ipw_intr(void *);
static int	ipw_cmd(struct ipw_softc *, uint32_t, void *, uint32_t);
static int	ipw_tx_start(struct ifnet *, struct mbuf *,
    struct ieee80211_node *);
static void	ipw_start(struct ifnet *);
static void	ipw_watchdog(struct ifnet *);
static int	ipw_ioctl(struct ifnet *, u_long, void *);
static int	ipw_get_table1(struct ipw_softc *, uint32_t *);
static int	ipw_get_radio(struct ipw_softc *, int *);
static void	ipw_stop_master(struct ipw_softc *);
static int	ipw_reset(struct ipw_softc *);
static int	ipw_load_ucode(struct ipw_softc *, u_char *, int);
static int	ipw_load_firmware(struct ipw_softc *, u_char *, int);
static int	ipw_cache_firmware(struct ipw_softc *);
static void	ipw_free_firmware(struct ipw_softc *);
static int	ipw_config(struct ipw_softc *);
static int	ipw_init(struct ifnet *);
static void	ipw_stop(struct ifnet *, int);
static uint32_t	ipw_read_table1(struct ipw_softc *, uint32_t);
static void	ipw_write_table1(struct ipw_softc *, uint32_t, uint32_t);
static int	ipw_read_table2(struct ipw_softc *, uint32_t, void *, uint32_t *);
static void	ipw_read_mem_1(struct ipw_softc *, bus_size_t, uint8_t *,
    bus_size_t);
static void	ipw_write_mem_1(struct ipw_softc *, bus_size_t, uint8_t *,
    bus_size_t);

/*
 * Supported rates for 802.11b mode (in 500Kbps unit).
 */
static const struct ieee80211_rateset ipw_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

static inline uint8_t
MEM_READ_1(struct ipw_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_1(sc, IPW_CSR_INDIRECT_DATA);
}

static inline uint32_t
MEM_READ_4(struct ipw_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_4(sc, IPW_CSR_INDIRECT_DATA);
}

CFATTACH_DECL_NEW(ipw, sizeof (struct ipw_softc), ipw_match, ipw_attach,
    ipw_detach, NULL);

static int
ipw_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR (pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_WL_2100)
		return 1;

	return 0;
}

/* Base Address Register */
#define IPW_PCI_BAR0	0x10

static void
ipw_attach(device_t parent, device_t self, void *aux)
{
	struct ipw_softc *sc = device_private(self);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_addr_t base;
	pci_intr_handle_t ih;
	uint32_t data;
	uint16_t val;
	int i, error;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	pci_aprint_devinfo(pa, NULL);

	/* enable bus-mastering */
	data = pci_conf_read(sc->sc_pct, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	data |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(sc->sc_pct, pa->pa_tag, PCI_COMMAND_STATUS_REG, data);

	/* map the register window */
	error = pci_mapreg_map(pa, IPW_PCI_BAR0, PCI_MAPREG_TYPE_MEM |
	    PCI_MAPREG_MEM_TYPE_32BIT, 0, &memt, &memh, &base, &sc->sc_sz);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map memory space\n");
		return;
	}

	sc->sc_st = memt;
	sc->sc_sh = memh;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_fwname = "ipw2100-1.2.fw";

	/* disable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, 0);

	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(sc->sc_dev, "could not map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(sc->sc_pct, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, ipw_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "could not establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	if (ipw_reset(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "could not reset adapter\n");
		goto fail;
	}

	if (ipw_dma_alloc(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate DMA resources\n");
		goto fail;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = ipw_init;
	ifp->if_stop = ipw_stop;
	ifp->if_ioctl = ipw_ioctl;
	ifp->if_start = ipw_start;
	ifp->if_watchdog = ipw_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	      IEEE80211_C_SHPREAMBLE	/* short preamble supported */
	    | IEEE80211_C_TXPMGT	/* tx power management */
	    | IEEE80211_C_IBSS		/* ibss mode */
	    | IEEE80211_C_MONITOR	/* monitor mode */
	    ;

	/* read MAC address from EEPROM */
	val = ipw_read_prom_word(sc, IPW_EEPROM_MAC + 0);
	ic->ic_myaddr[0] = val >> 8;
	ic->ic_myaddr[1] = val & 0xff;
	val = ipw_read_prom_word(sc, IPW_EEPROM_MAC + 1);
	ic->ic_myaddr[2] = val >> 8;
	ic->ic_myaddr[3] = val & 0xff;
	val = ipw_read_prom_word(sc, IPW_EEPROM_MAC + 2);
	ic->ic_myaddr[4] = val >> 8;
	ic->ic_myaddr[5] = val & 0xff;

	/* set supported .11b rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ipw_rateset_11b;

	/* set supported .11b channels (read from EEPROM) */
	if ((val = ipw_read_prom_word(sc, IPW_EEPROM_CHANNEL_LIST)) == 0)
		val = 0x7ff; /* default to channels 1-11 */
	val <<= 1;
	for (i = 1; i < 16; i++) {
		if (val & (1 << i)) {
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, IEEE80211_CHAN_B);
			ic->ic_channels[i].ic_flags = IEEE80211_CHAN_B;
		}
	}

	/* check support for radio transmitter switch in EEPROM */
	if (!(ipw_read_prom_word(sc, IPW_EEPROM_RADIO) & 8))
		sc->flags |= IPW_FLAG_HAS_RADIO_SWITCH;

	aprint_normal_dev(sc->sc_dev, "802.11 address %s\n",
	    ether_sprintf(ic->ic_myaddr));

	if_attach(ifp);
	ieee80211_ifattach(ic);

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ipw_newstate;

	ieee80211_media_init(ic, ipw_media_change, ipw_media_status);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + 64, &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IPW_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IPW_TX_RADIOTAP_PRESENT);

	/*
	 * Add a few sysctl knobs.
	 * XXX: Not yet
	 */
	sc->dwelltime = 100;

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	ieee80211_announce(ic);

	return;

fail:	ipw_detach(self, 0);
}

static int
ipw_detach(device_t self, int flags)
{
	struct ipw_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_if;

	if (ifp->if_softc) {
		ipw_stop(ifp, 1);
		ipw_free_firmware(sc);

		bpf_detach(ifp);
		ieee80211_ifdetach(&sc->sc_ic);
		if_detach(ifp);

		ipw_release(sc);
	}

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);

	return 0;
}

static int
ipw_dma_alloc(struct ipw_softc *sc)
{
	struct ipw_soft_bd *sbd;
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;
	int error, i, nsegs;

	/*
	 * Allocate and map tx ring.
	 */
	error = bus_dmamap_create(sc->sc_dmat, IPW_TBD_SZ, 1, IPW_TBD_SZ, 0,
	    BUS_DMA_NOWAIT, &sc->tbd_map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not create tbd dma map\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_TBD_SZ, PAGE_SIZE, 0,
	    &sc->tbd_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate tbd dma memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->tbd_seg, nsegs, IPW_TBD_SZ,
	    (void **)&sc->tbd_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map tbd dma memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->tbd_map, sc->tbd_list,
	    IPW_TBD_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load tbd dma memory\n");
		goto fail;
	}

	(void)memset(sc->tbd_list, 0, IPW_TBD_SZ);

	/*
	 * Allocate and map rx ring.
	 */
	error = bus_dmamap_create(sc->sc_dmat, IPW_RBD_SZ, 1, IPW_RBD_SZ, 0,
	    BUS_DMA_NOWAIT, &sc->rbd_map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not create rbd dma map\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_RBD_SZ, PAGE_SIZE, 0,
	    &sc->rbd_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate rbd dma memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->rbd_seg, nsegs, IPW_RBD_SZ,
	    (void **)&sc->rbd_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map rbd dma memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->rbd_map, sc->rbd_list,
	    IPW_RBD_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load rbd dma memory\n");
		goto fail;
	}

	(void)memset(sc->rbd_list, 0, IPW_RBD_SZ);

	/*
	 * Allocate and map status ring.
	 */
	error = bus_dmamap_create(sc->sc_dmat, IPW_STATUS_SZ, 1, IPW_STATUS_SZ,
	    0, BUS_DMA_NOWAIT, &sc->status_map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not create status dma map\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_STATUS_SZ, PAGE_SIZE, 0,
	    &sc->status_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate status dma memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->status_seg, nsegs,
	    IPW_STATUS_SZ, (void **)&sc->status_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map status dma memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->status_map, sc->status_list,
	    IPW_STATUS_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load status dma memory\n");
		goto fail;
	}

	(void)memset(sc->status_list, 0, IPW_STATUS_SZ);

	/*
	 * Allocate command DMA map.
	 */
	error = bus_dmamap_create(sc->sc_dmat, sizeof (struct ipw_cmd),
	    1, sizeof (struct ipw_cmd), 0, BUS_DMA_NOWAIT, &sc->cmd_map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not create cmd dma map\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, sizeof (struct ipw_cmd),
	    PAGE_SIZE, 0, &sc->cmd_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate cmd dma memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->cmd_seg, nsegs,
	    sizeof (struct ipw_cmd), (void **)&sc->cmd, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map cmd dma memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->cmd_map, &sc->cmd,
	    sizeof (struct ipw_cmd), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map cmd dma memory\n");
		return error;
	}

	/*
	 * Allocate and map hdr list.
	 */

	error = bus_dmamap_create(sc->sc_dmat,
	    IPW_NDATA * sizeof(struct ipw_hdr), 1,
	    sizeof(struct ipw_hdr), 0, BUS_DMA_NOWAIT,
	    &sc->hdr_map);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not create hdr dma map\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    IPW_NDATA * sizeof(struct ipw_hdr), PAGE_SIZE, 0, &sc->hdr_seg,
	    1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate hdr memory\n");
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->hdr_seg, nsegs,
	    IPW_NDATA * sizeof(struct ipw_hdr), (void **)&sc->hdr_list,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not map hdr memory\n");
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->hdr_map, sc->hdr_list,
	    IPW_NDATA * sizeof(struct ipw_hdr), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load hdr memory\n");
		goto fail;
	}

	(void)memset(sc->hdr_list, 0, IPW_HDR_SZ);

	/*
	 * Create DMA hdrs tailq.
	 */
	TAILQ_INIT(&sc->sc_free_shdr);
	for (i = 0; i < IPW_NDATA; i++) {
		shdr = &sc->shdr_list[i];
		shdr->hdr = sc->hdr_list + i;
		shdr->offset = sizeof(struct ipw_hdr) * i;
		shdr->addr = sc->hdr_map->dm_segs[0].ds_addr + shdr->offset;
		TAILQ_INSERT_TAIL(&sc->sc_free_shdr, shdr, next);
	}

	/*
	 * Allocate tx buffers DMA maps.
	 */
	TAILQ_INIT(&sc->sc_free_sbuf);
	for (i = 0; i < IPW_NDATA; i++) {
		sbuf = &sc->tx_sbuf_list[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    IPW_MAX_NSEG, MCLBYTES, 0, BUS_DMA_NOWAIT, &sbuf->map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "could not create txbuf dma map\n");
			goto fail;
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_sbuf, sbuf, next);
	}

	/*
	 * Initialize tx ring.
	 */
	for (i = 0; i < IPW_NTBD; i++) {
		sbd = &sc->stbd_list[i];
		sbd->bd = &sc->tbd_list[i];
		sbd->type = IPW_SBD_TYPE_NOASSOC;
	}

	/*
	 * Pre-allocate rx buffers and DMA maps
	 */
	for (i = 0; i < IPW_NRBD; i++) {
		sbd = &sc->srbd_list[i];
		sbuf = &sc->rx_sbuf_list[i];
		sbd->bd = &sc->rbd_list[i];

		MGETHDR(sbuf->m, M_DONTWAIT, MT_DATA);
		if (sbuf->m == NULL) {
			aprint_error_dev(sc->sc_dev, "could not allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}

		MCLGET(sbuf->m, M_DONTWAIT);
		if (!(sbuf->m->m_flags & M_EXT)) {
			m_freem(sbuf->m);
			aprint_error_dev(sc->sc_dev, "could not allocate rx mbuf cluster\n");
			error = ENOMEM;
			goto fail;
		}

		sbuf->m->m_pkthdr.len = sbuf->m->m_len = sbuf->m->m_ext.ext_size;

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sbuf->map);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "could not create rxbuf dma map\n");
			m_freem(sbuf->m);
			goto fail;
		}

		error = bus_dmamap_load_mbuf(sc->sc_dmat, sbuf->map,
		    sbuf->m, BUS_DMA_READ | BUS_DMA_NOWAIT);
		if (error != 0) {
			bus_dmamap_destroy(sc->sc_dmat, sbuf->map);
			m_freem(sbuf->m);
			aprint_error_dev(sc->sc_dev, "could not map rxbuf dma memory\n");
			goto fail;
		}

		sbd->type = IPW_SBD_TYPE_DATA;
		sbd->priv = sbuf;
		sbd->bd->physaddr = htole32(sbuf->map->dm_segs[0].ds_addr);
		sbd->bd->len = htole32(MCLBYTES);

		bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0,
		    sbuf->map->dm_mapsize, BUS_DMASYNC_PREREAD);

	}

	bus_dmamap_sync(sc->sc_dmat, sc->rbd_map, 0, IPW_RBD_SZ,
	    BUS_DMASYNC_PREREAD);

	return 0;

fail:	ipw_release(sc);
	return error;
}

static void
ipw_release(struct ipw_softc *sc)
{
	struct ipw_soft_buf *sbuf;
	int i;

	if (sc->tbd_map != NULL) {
		if (sc->tbd_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->tbd_map);
			bus_dmamem_unmap(sc->sc_dmat, (void *)sc->tbd_list,
			    IPW_TBD_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->tbd_seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->tbd_map);
	}

	if (sc->rbd_map != NULL) {
		if (sc->rbd_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->rbd_map);
			bus_dmamem_unmap(sc->sc_dmat, (void *)sc->rbd_list,
			    IPW_RBD_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->rbd_seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->rbd_map);
	}

	if (sc->status_map != NULL) {
		if (sc->status_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->status_map);
			bus_dmamem_unmap(sc->sc_dmat, (void *)sc->status_list,
			    IPW_RBD_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->status_seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->status_map);
	}

	for (i = 0; i < IPW_NTBD; i++)
		ipw_release_sbd(sc, &sc->stbd_list[i]);

	if (sc->cmd_map != NULL)
		bus_dmamap_destroy(sc->sc_dmat, sc->cmd_map);

 	if (sc->hdr_list != NULL) {
 		bus_dmamap_unload(sc->sc_dmat, sc->hdr_map);
 		bus_dmamem_unmap(sc->sc_dmat, (void *)sc->hdr_list,
 		    IPW_NDATA * sizeof(struct ipw_hdr));
 	}
 	if (sc->hdr_map != NULL) {
 		bus_dmamem_free(sc->sc_dmat, &sc->hdr_seg, 1);
 		bus_dmamap_destroy(sc->sc_dmat, sc->hdr_map);
 	}

	for (i = 0; i < IPW_NDATA; i++)
		bus_dmamap_destroy(sc->sc_dmat, sc->tx_sbuf_list[i].map);

	for (i = 0; i < IPW_NRBD; i++) {
		sbuf = &sc->rx_sbuf_list[i];
		if (sbuf->map != NULL) {
			if (sbuf->m != NULL) {
				bus_dmamap_unload(sc->sc_dmat, sbuf->map);
				m_freem(sbuf->m);
			}
			bus_dmamap_destroy(sc->sc_dmat, sbuf->map);
		}
	}

}

static int
ipw_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		ipw_init(ifp);

	return 0;
}

/*
 * The firmware automatically adapts the transmit speed. We report the current
 * transmit speed here.
 */
static void
ipw_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
#define N(a)	(sizeof (a) / sizeof (a[0]))
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	static const struct {
		uint32_t	val;
		int		rate;
	} rates[] = {
		{ IPW_RATE_DS1,   2 },
		{ IPW_RATE_DS2,   4 },
		{ IPW_RATE_DS5,  11 },
		{ IPW_RATE_DS11, 22 },
	};
	uint32_t val;
	int rate, i;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;

	/* read current transmission rate from adapter */
	val = ipw_read_table1(sc, IPW_INFO_CURRENT_TX_RATE) & 0xf;

	/* convert ipw rate to 802.11 rate */
	for (i = 0; i < N(rates) && rates[i].val != val; i++);
	rate = (i < N(rates)) ? rates[i].rate : 0;

	imr->ifm_active |= IFM_IEEE80211_11B;
	imr->ifm_active |= ieee80211_rate2media(ic, rate, IEEE80211_MODE_11B);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;

	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;

	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;

	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_HOSTAP:
		/* should not get there */
		break;
	}
#undef N
}

static int
ipw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate,
    int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	uint8_t macaddr[IEEE80211_ADDR_LEN];
	uint32_t len;
	struct ipw_rx_radiotap_header *wr = &sc->sc_rxtap;
	struct ipw_tx_radiotap_header *wt = &sc->sc_txtap;

	switch (nstate) {
	case IEEE80211_S_INIT:
		break;
	default:
		KASSERT(ic->ic_curchan != IEEE80211_CHAN_ANYC);
		KASSERT(ic->ic_curchan != NULL);
		wt->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		wt->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);
		wr->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		wr->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		break;
	}

	switch (nstate) {
	case IEEE80211_S_RUN:
		DELAY(200); /* firmware needs a short delay here */

		len = IEEE80211_ADDR_LEN;
		ipw_read_table2(sc, IPW_INFO_CURRENT_BSSID, macaddr, &len);

		ni = ieee80211_find_node(&ic->ic_scan, macaddr);
		if (ni == NULL)
			break;

		ieee80211_ref_node(ni);
		ieee80211_sta_join(ic, ni);
		ieee80211_node_authorize(ni);

		if (ic->ic_opmode == IEEE80211_M_STA)
			ieee80211_notify_node_join(ic, ni, 1);
		break;

	case IEEE80211_S_INIT:
	case IEEE80211_S_SCAN:
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		break;
	}

	ic->ic_state = nstate;
	return 0;
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM.
 */
static uint16_t
ipw_read_prom_word(struct ipw_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* clock C once before the first command */
	IPW_EEPROM_CTL(sc, 0);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_C);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);

	/* write start bit (1) */
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_D);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_D | IPW_EEPROM_C);

	/* write READ opcode (10) */
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_D);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_D | IPW_EEPROM_C);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_C);

	/* write address A7-A0 */
	for (n = 7; n >= 0; n--) {
		IPW_EEPROM_CTL(sc, IPW_EEPROM_S |
		    (((addr >> n) & 1) << IPW_EEPROM_SHIFT_D));
		IPW_EEPROM_CTL(sc, IPW_EEPROM_S |
		    (((addr >> n) & 1) << IPW_EEPROM_SHIFT_D) | IPW_EEPROM_C);
	}

	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);

	/* read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_C);
		IPW_EEPROM_CTL(sc, IPW_EEPROM_S);
		tmp = MEM_READ_4(sc, IPW_MEM_EEPROM_CTL);
		val |= ((tmp & IPW_EEPROM_Q) >> IPW_EEPROM_SHIFT_Q) << n;
	}

	IPW_EEPROM_CTL(sc, 0);

	/* clear Chip Select and clock C */
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);
	IPW_EEPROM_CTL(sc, 0);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_C);

	return le16toh(val);
}

static void
ipw_command_intr(struct ipw_softc *sc, struct ipw_soft_buf *sbuf)
{

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, sizeof (struct ipw_cmd),
	    BUS_DMASYNC_POSTREAD);

#ifdef IPW_DEBUG
	struct ipw_cmd *cmd = mtod(sbuf->m, struct ipw_cmd *);

	DPRINTFN(2, ("cmd ack'ed (%u, %u, %u, %u, %u)\n", le32toh(cmd->type),
	    le32toh(cmd->subtype), le32toh(cmd->seq), le32toh(cmd->len),
	    le32toh(cmd->status)));
#endif

	wakeup(&sc->cmd);
}

static void
ipw_newstate_intr(struct ipw_softc *sc, struct ipw_soft_buf *sbuf)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	uint32_t state;

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, sizeof state,
	    BUS_DMASYNC_POSTREAD);

	state = le32toh(*mtod(sbuf->m, uint32_t *));

	DPRINTFN(2, ("entering state %u\n", state));

	switch (state) {
	case IPW_STATE_ASSOCIATED:
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
		break;

	case IPW_STATE_SCANNING:
		/* don't leave run state on background scan */
		if (ic->ic_state != IEEE80211_S_RUN)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

		ic->ic_flags |= IEEE80211_F_SCAN;
		break;

	case IPW_STATE_SCAN_COMPLETE:
		ieee80211_notify_scan_done(ic);
		ic->ic_flags &= ~IEEE80211_F_SCAN;
		break;

	case IPW_STATE_ASSOCIATION_LOST:
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		break;

	case IPW_STATE_RADIO_DISABLED:
		ic->ic_ifp->if_flags &= ~IFF_UP;
		ipw_stop(ifp, 1);
		break;
	}
}

/*
 * XXX: Hack to set the current channel to the value advertised in beacons or
 * probe responses. Only used during AP detection.
 */
static void
ipw_fix_channel(struct ieee80211com *ic, struct mbuf *m)
{
	struct ieee80211_frame *wh;
	uint8_t subtype;
	uint8_t *frm, *efrm;

	wh = mtod(m, struct ieee80211_frame *);

	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_MGT)
		return;

	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
	    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP)
		return;

	frm = (uint8_t *)(wh + 1);
	efrm = mtod(m, uint8_t *) + m->m_len;

	frm += 12;	/* skip tstamp, bintval and capinfo fields */
	while (frm < efrm) {
		if (*frm == IEEE80211_ELEMID_DSPARMS)
#if IEEE80211_CHAN_MAX < 255
		if (frm[2] <= IEEE80211_CHAN_MAX)
#endif
			ic->ic_curchan = &ic->ic_channels[frm[2]];

		frm += frm[1] + 2;
	}
}

static void
ipw_data_intr(struct ipw_softc *sc, struct ipw_status *status,
    struct ipw_soft_bd *sbd, struct ipw_soft_buf *sbuf)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct mbuf *mnew, *m;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	int error;

	DPRINTFN(5, ("received frame len=%u, rssi=%u\n", le32toh(status->len),
	    status->rssi));

	if (le32toh(status->len) < sizeof (struct ieee80211_frame_min) ||
	    le32toh(status->len) > MCLBYTES)
		return;

	/*
	 * Try to allocate a new mbuf for this ring element and load it before
	 * processing the current mbuf. If the ring element cannot be loaded,
	 * drop the received packet and reuse the old mbuf. In the unlikely
	 * case that the old mbuf can't be reloaded either, explicitly panic.
	 */
	MGETHDR(mnew, M_DONTWAIT, MT_DATA);
	if (mnew == NULL) {
		aprint_error_dev(sc->sc_dev, "could not allocate rx mbuf\n");
		ifp->if_ierrors++;
		return;
	}

	MCLGET(mnew, M_DONTWAIT);
	if (!(mnew->m_flags & M_EXT)) {
		aprint_error_dev(sc->sc_dev, "could not allocate rx mbuf cluster\n");
		m_freem(mnew);
		ifp->if_ierrors++;
		return;
	}

	mnew->m_pkthdr.len = mnew->m_len = mnew->m_ext.ext_size;

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, le32toh(status->len),
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->sc_dmat, sbuf->map);

	error = bus_dmamap_load_mbuf(sc->sc_dmat, sbuf->map, mnew,
	    BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load rx buf DMA map\n");
		m_freem(mnew);

		/* try to reload the old mbuf */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, sbuf->map,
		    sbuf->m, BUS_DMA_READ | BUS_DMA_NOWAIT);
		if (error != 0) {
			/* very unlikely that it will fail... */
			panic("%s: unable to remap rx buf",
			    device_xname(sc->sc_dev));
		}
		ifp->if_ierrors++;
		return;
	}

	/*
	 * New mbuf successfully loaded, update Rx ring and continue
	 * processing.
	 */
	m = sbuf->m;
	sbuf->m = mnew;
	sbd->bd->physaddr = htole32(sbuf->map->dm_segs[0].ds_addr);

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = le32toh(status->len);

	if (sc->sc_drvbpf != NULL) {
		struct ipw_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_antsignal = status->rssi;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	if (ic->ic_state == IEEE80211_S_SCAN)
		ipw_fix_channel(ic, m);

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* send the frame to the 802.11 layer */
	ieee80211_input(ic, m, ni, status->rssi, 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0,
	    sbuf->map->dm_mapsize, BUS_DMASYNC_PREREAD);
}

static void
ipw_rx_intr(struct ipw_softc *sc)
{
	struct ipw_status *status;
	struct ipw_soft_bd *sbd;
	struct ipw_soft_buf *sbuf;
	uint32_t r, i;

	if (!(sc->flags & IPW_FLAG_FW_INITED))
		return;

	r = CSR_READ_4(sc, IPW_CSR_RX_READ);

	for (i = (sc->rxcur + 1) % IPW_NRBD; i != r; i = (i + 1) % IPW_NRBD) {

		/* firmware was killed, stop processing received frames */
		if (!(sc->flags & IPW_FLAG_FW_INITED))
			return;

		bus_dmamap_sync(sc->sc_dmat, sc->rbd_map,
		    i * sizeof (struct ipw_bd), sizeof (struct ipw_bd),
		    BUS_DMASYNC_POSTREAD);

		bus_dmamap_sync(sc->sc_dmat, sc->status_map,
		    i * sizeof (struct ipw_status), sizeof (struct ipw_status),
		    BUS_DMASYNC_POSTREAD);

		status = &sc->status_list[i];
		sbd = &sc->srbd_list[i];
		sbuf = sbd->priv;

		switch (le16toh(status->code) & 0xf) {
		case IPW_STATUS_CODE_COMMAND:
			ipw_command_intr(sc, sbuf);
			break;

		case IPW_STATUS_CODE_NEWSTATE:
			ipw_newstate_intr(sc, sbuf);
			break;

		case IPW_STATUS_CODE_DATA_802_3:
		case IPW_STATUS_CODE_DATA_802_11:
			ipw_data_intr(sc, status, sbd, sbuf);
			break;

		case IPW_STATUS_CODE_NOTIFICATION:
			DPRINTFN(2, ("received notification\n"));
			break;

		default:
			aprint_error_dev(sc->sc_dev, "unknown status code %u\n",
			    le16toh(status->code));
		}

		sbd->bd->flags = 0;

		bus_dmamap_sync(sc->sc_dmat, sc->rbd_map,
		    i * sizeof (struct ipw_bd), sizeof (struct ipw_bd),
		    BUS_DMASYNC_PREREAD);

		bus_dmamap_sync(sc->sc_dmat, sc->status_map,
		    i * sizeof (struct ipw_status), sizeof (struct ipw_status),
		    BUS_DMASYNC_PREREAD);
	}

	/* Tell the firmware what we have processed */
	sc->rxcur = (r == 0) ? IPW_NRBD - 1 : r - 1;
	CSR_WRITE_4(sc, IPW_CSR_RX_WRITE, sc->rxcur);
}

static void
ipw_release_sbd(struct ipw_softc *sc, struct ipw_soft_bd *sbd)
{
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;

	switch (sbd->type) {
	case IPW_SBD_TYPE_COMMAND:
		bus_dmamap_sync(sc->sc_dmat, sc->cmd_map,
		    0, sizeof(struct ipw_cmd), BUS_DMASYNC_POSTWRITE);
/*		bus_dmamap_unload(sc->sc_dmat, sc->cmd_map); */
		break;

	case IPW_SBD_TYPE_HEADER:
		shdr = sbd->priv;
 		bus_dmamap_sync(sc->sc_dmat, sc->hdr_map,
 		    shdr->offset, sizeof(struct ipw_hdr), BUS_DMASYNC_POSTWRITE);
		TAILQ_INSERT_TAIL(&sc->sc_free_shdr, shdr, next);
		break;

	case IPW_SBD_TYPE_DATA:
		sbuf = sbd->priv;

		bus_dmamap_sync(sc->sc_dmat, sbuf->map,
		    0, MCLBYTES, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sbuf->map);
		m_freem(sbuf->m);
		if (sbuf->ni != NULL)
			ieee80211_free_node(sbuf->ni);
		/* kill watchdog timer */
		sc->sc_tx_timer = 0;
		TAILQ_INSERT_TAIL(&sc->sc_free_sbuf, sbuf, next);
		break;
	}
	sbd->type = IPW_SBD_TYPE_NOASSOC;
}

static void
ipw_tx_intr(struct ipw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ipw_soft_bd *sbd;
	uint32_t r, i;

	if (!(sc->flags & IPW_FLAG_FW_INITED))
		return;

	r = CSR_READ_4(sc, IPW_CSR_TX_READ);

	for (i = (sc->txold + 1) % IPW_NTBD; i != r; i = (i + 1) % IPW_NTBD) {
		sbd = &sc->stbd_list[i];

		if (sbd->type == IPW_SBD_TYPE_DATA)
			ifp->if_opackets++;

		ipw_release_sbd(sc, sbd);
		sc->txfree++;
	}

	/* remember what the firmware has processed */
	sc->txold = (r == 0) ? IPW_NTBD - 1 : r - 1;

	/* Call start() since some buffer descriptors have been released */
	ifp->if_flags &= ~IFF_OACTIVE;
	(*ifp->if_start)(ifp);
}

static int
ipw_intr(void *arg)
{
	struct ipw_softc *sc = arg;
	uint32_t r;

	r = CSR_READ_4(sc, IPW_CSR_INTR);
	if (r == 0 || r == 0xffffffff)
		return 0;

	/* Disable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, 0);

	if (r & (IPW_INTR_FATAL_ERROR | IPW_INTR_PARITY_ERROR)) {
		aprint_error_dev(sc->sc_dev, "fatal error\n");
		sc->sc_ic.ic_ifp->if_flags &= ~IFF_UP;
		ipw_stop(&sc->sc_if, 1);
	}

	if (r & IPW_INTR_FW_INIT_DONE) {
		if (!(r & (IPW_INTR_FATAL_ERROR | IPW_INTR_PARITY_ERROR)))
			wakeup(sc);
	}

	if (r & IPW_INTR_RX_TRANSFER)
		ipw_rx_intr(sc);

	if (r & IPW_INTR_TX_TRANSFER)
		ipw_tx_intr(sc);

	/* Acknowledge all interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR, r);

	/* Re-enable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, IPW_INTR_MASK);

	return 0;
}

/*
 * Send a command to the firmware and wait for the acknowledgement.
 */
static int
ipw_cmd(struct ipw_softc *sc, uint32_t type, void *data, uint32_t len)
{
	struct ipw_soft_bd *sbd;

	sbd = &sc->stbd_list[sc->txcur];

	sc->cmd.type = htole32(type);
	sc->cmd.subtype = 0;
	sc->cmd.len = htole32(len);
	sc->cmd.seq = 0;

	(void)memcpy(sc->cmd.data, data, len);

	sbd->type = IPW_SBD_TYPE_COMMAND;
	sbd->bd->physaddr = htole32(sc->cmd_map->dm_segs[0].ds_addr);
	sbd->bd->len = htole32(sizeof (struct ipw_cmd));
	sbd->bd->nfrag = 1;
	sbd->bd->flags = IPW_BD_FLAG_TX_FRAME_COMMAND |
			 IPW_BD_FLAG_TX_LAST_FRAGMENT;

	bus_dmamap_sync(sc->sc_dmat, sc->cmd_map, 0, sizeof (struct ipw_cmd),
	    BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->sc_dmat, sc->tbd_map,
	    sc->txcur * sizeof (struct ipw_bd), sizeof (struct ipw_bd),
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(2, ("sending command (%u, %u, %u, %u)\n", type, 0, 0, len));

	/* kick firmware */
	sc->txfree--;
	sc->txcur = (sc->txcur + 1) % IPW_NTBD;
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE, sc->txcur);

	/* Wait at most one second for command to complete */
	return tsleep(&sc->cmd, 0, "ipwcmd", hz);
}

static int
ipw_tx_start(struct ifnet *ifp, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ipw_soft_bd *sbd;
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;
	struct ieee80211_key *k;
	struct mbuf *mnew;
	int error, i;

	wh = mtod(m0, struct ieee80211_frame *);

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	if (sc->sc_drvbpf != NULL) {
		struct ipw_tx_radiotap_header *tap = &sc->sc_txtap;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	shdr = TAILQ_FIRST(&sc->sc_free_shdr);
	sbuf = TAILQ_FIRST(&sc->sc_free_sbuf);
	KASSERT(shdr != NULL && sbuf != NULL);

	shdr->hdr->type = htole32(IPW_HDR_TYPE_SEND);
	shdr->hdr->subtype = 0;
	shdr->hdr->encrypted = (wh->i_fc[1] & IEEE80211_FC1_WEP) ? 1 : 0;
	shdr->hdr->encrypt = 0;
	shdr->hdr->keyidx = 0;
	shdr->hdr->keysz = 0;
	shdr->hdr->fragmentsz = 0;
	IEEE80211_ADDR_COPY(shdr->hdr->src_addr, wh->i_addr2);
	if (ic->ic_opmode == IEEE80211_M_STA)
		IEEE80211_ADDR_COPY(shdr->hdr->dst_addr, wh->i_addr3);
	else
		IEEE80211_ADDR_COPY(shdr->hdr->dst_addr, wh->i_addr1);

	/* trim IEEE802.11 header */
	m_adj(m0, sizeof (struct ieee80211_frame));

	error = bus_dmamap_load_mbuf(sc->sc_dmat, sbuf->map, m0, BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		aprint_error_dev(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}

	if (error != 0) {
		/* too many fragments, linearize */

		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			m_freem(m0);
			return ENOMEM;
		}

		M_COPY_PKTHDR(mnew, m0);

		/* If the data won't fit in the header, get a cluster */
		if (m0->m_pkthdr.len > MHLEN) {
			MCLGET(mnew, M_DONTWAIT);
			if (!(mnew->m_flags & M_EXT)) {
				m_freem(m0);
				m_freem(mnew);
				return ENOMEM;
			}
		}
		m_copydata(m0, 0, m0->m_pkthdr.len, mtod(mnew, void *));
		m_freem(m0);
		mnew->m_len = mnew->m_pkthdr.len;
		m0 = mnew;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, sbuf->map, m0,
		    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev, "could not map mbuf (error %d)\n", error);
			m_freem(m0);
			return error;
		}
	}

	TAILQ_REMOVE(&sc->sc_free_sbuf, sbuf, next);
	TAILQ_REMOVE(&sc->sc_free_shdr, shdr, next);

	sbd = &sc->stbd_list[sc->txcur];
	sbd->type = IPW_SBD_TYPE_HEADER;
	sbd->priv = shdr;
 	sbd->bd->physaddr = htole32(shdr->addr);
	sbd->bd->len = htole32(sizeof (struct ipw_hdr));
	sbd->bd->nfrag = 1 + sbuf->map->dm_nsegs;
	sbd->bd->flags = IPW_BD_FLAG_TX_FRAME_802_3 |
			 IPW_BD_FLAG_TX_NOT_LAST_FRAGMENT;

	DPRINTFN(5, ("sending tx hdr (%u, %u, %u, %u, )\n",
	    shdr->hdr->type, shdr->hdr->subtype, shdr->hdr->encrypted,
	    shdr->hdr->encrypt));
	DPRINTFN(5, ("%s->", ether_sprintf(shdr->hdr->src_addr)));
	DPRINTFN(5, ("%s\n", ether_sprintf(shdr->hdr->dst_addr)));

	bus_dmamap_sync(sc->sc_dmat, sc->tbd_map,
	    sc->txcur * sizeof (struct ipw_bd),
	    sizeof (struct ipw_bd), BUS_DMASYNC_PREWRITE);

	sc->txfree--;
	sc->txcur = (sc->txcur + 1) % IPW_NTBD;

	sbuf->m = m0;
	sbuf->ni = ni;

	for (i = 0; i < sbuf->map->dm_nsegs; i++) {
		sbd = &sc->stbd_list[sc->txcur];

		sbd->bd->physaddr = htole32(sbuf->map->dm_segs[i].ds_addr);
		sbd->bd->len = htole32(sbuf->map->dm_segs[i].ds_len);
		sbd->bd->nfrag = 0;
		sbd->bd->flags = IPW_BD_FLAG_TX_FRAME_802_3;
		if (i == sbuf->map->dm_nsegs - 1) {
			sbd->type = IPW_SBD_TYPE_DATA;
			sbd->priv = sbuf;
			sbd->bd->flags |= IPW_BD_FLAG_TX_LAST_FRAGMENT;
		} else {
			sbd->type = IPW_SBD_TYPE_NOASSOC;
			sbd->bd->flags |= IPW_BD_FLAG_TX_NOT_LAST_FRAGMENT;
		}

		DPRINTFN(5, ("sending fragment (%d, %d)\n", i,
		    (int)sbuf->map->dm_segs[i].ds_len));

		bus_dmamap_sync(sc->sc_dmat, sc->tbd_map,
		    sc->txcur * sizeof (struct ipw_bd),
		    sizeof (struct ipw_bd), BUS_DMASYNC_PREWRITE);

		sc->txfree--;
		sc->txcur = (sc->txcur + 1) % IPW_NTBD;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->hdr_map, shdr->offset,
	    sizeof (struct ipw_hdr), BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, MCLBYTES,
	    BUS_DMASYNC_PREWRITE);

	/* Inform firmware about this new packet */
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE, sc->txcur);

	return 0;
}

static void
ipw_start(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ether_header *eh;
	struct ieee80211_node *ni;


	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (sc->txfree < 1 + IPW_MAX_NSEG) {
			IF_PREPEND(&ifp->if_snd, m0);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		if (m0->m_len < sizeof (struct ether_header) &&
		    (m0 = m_pullup(m0, sizeof (struct ether_header))) == NULL)
			continue;

		eh = mtod(m0, struct ether_header *);
		ni = ieee80211_find_txnode(ic, eh->ether_dhost);
		if (ni == NULL) {
			m_freem(m0);
			continue;
		}

		bpf_mtap(ifp, m0);

		m0 = ieee80211_encap(ic, m0, ni);
		if (m0 == NULL) {
			ieee80211_free_node(ni);
			continue;
		}

		bpf_mtap3(ic->ic_rawbpf, m0);

		if (ipw_tx_start(ifp, m0, ni) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			break;
		}

		/* start watchdog timer */
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

static void
ipw_watchdog(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev, "device timeout\n");
			ifp->if_oerrors++;
			ifp->if_flags &= ~IFF_UP;
			ipw_stop(ifp, 1);
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(&sc->sc_ic);
}

static int
ipw_get_table1(struct ipw_softc *sc, uint32_t *tbl)
{
	uint32_t addr, size, i;

	if (!(sc->flags & IPW_FLAG_FW_INITED))
		return ENOTTY;

	CSR_WRITE_4(sc, IPW_CSR_AUTOINC_ADDR, sc->table1_base);

	size = CSR_READ_4(sc, IPW_CSR_AUTOINC_DATA);
	if (suword(tbl, size) != 0)
		return EFAULT;

	for (i = 1, ++tbl; i < size; i++, tbl++) {
		addr = CSR_READ_4(sc, IPW_CSR_AUTOINC_DATA);
		if (suword(tbl, MEM_READ_4(sc, addr)) != 0)
			return EFAULT;
	}
	return 0;
}

static int
ipw_get_radio(struct ipw_softc *sc, int *ret)
{
	uint32_t addr;

	if (!(sc->flags & IPW_FLAG_FW_INITED))
		return ENOTTY;

	addr = ipw_read_table1(sc, IPW_INFO_EEPROM_ADDRESS);
	if ((MEM_READ_4(sc, addr + 32) >> 24) & 1) {
		suword(ret, -1);
		return 0;
	}

	if (CSR_READ_4(sc, IPW_CSR_IO) & IPW_IO_RADIO_DISABLED)
		suword(ret, 0);
	else
		suword(ret, 1);

	return 0;
}

static int
ipw_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
#define	IS_RUNNING(ifp) \
	((ifp->if_flags & IFF_UP) && (ifp->if_flags & IFF_RUNNING))

	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				ipw_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ipw_stop(ifp, 1);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* XXX no h/w multicast filter? --dyoung */
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/* setup multicast filter, etc */
			error = 0;
		}
		break;

	case SIOCGTABLE1:
		error = ipw_get_table1(sc, (uint32_t *)ifr->ifr_data);
		break;

	case SIOCGRADIO:
		error = ipw_get_radio(sc, (int *)ifr->ifr_data);
		break;

	case SIOCSIFMEDIA:
		if (ifr->ifr_media & IFM_IEEE80211_ADHOC)
			sc->sc_fwname = "ipw2100-1.2-i.fw";
		else if (ifr->ifr_media & IFM_IEEE80211_MONITOR)
			sc->sc_fwname = "ipw2100-1.2-p.fw";
		else
			sc->sc_fwname = "ipw2100-1.2.fw";

		ipw_free_firmware(sc);
		/* FALLTRHOUGH */
	default:
		error = ieee80211_ioctl(&sc->sc_ic, cmd, data);
		if (error != ENETRESET)
			break;

		if (error == ENETRESET) {
			if (IS_RUNNING(ifp) &&
			    (ic->ic_roaming != IEEE80211_ROAMING_MANUAL))
				ipw_init(ifp);
			error = 0;
		}

	}

	splx(s);
	return error;
#undef IS_RUNNING
}

static uint32_t
ipw_read_table1(struct ipw_softc *sc, uint32_t off)
{
	return MEM_READ_4(sc, MEM_READ_4(sc, sc->table1_base + off));
}

static void
ipw_write_table1(struct ipw_softc *sc, uint32_t off, uint32_t info)
{
	MEM_WRITE_4(sc, MEM_READ_4(sc, sc->table1_base + off), info);
}

static int
ipw_read_table2(struct ipw_softc *sc, uint32_t off, void *buf, uint32_t *len)
{
	uint32_t addr, info;
	uint16_t count, size;
	uint32_t total;

	/* addr[4] + count[2] + size[2] */
	addr = MEM_READ_4(sc, sc->table2_base + off);
	info = MEM_READ_4(sc, sc->table2_base + off + 4);

	count = info >> 16;
	size = info & 0xffff;
	total = count * size;

	if (total > *len) {
		*len = total;
		return EINVAL;
	}

	*len = total;
	ipw_read_mem_1(sc, addr, buf, total);

	return 0;
}

static void
ipw_stop_master(struct ipw_softc *sc)
{
	int ntries;

	/* disable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, 0);

	CSR_WRITE_4(sc, IPW_CSR_RST, IPW_RST_STOP_MASTER);
	for (ntries = 0; ntries < 50; ntries++) {
		if (CSR_READ_4(sc, IPW_CSR_RST) & IPW_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 50)
		aprint_error_dev(sc->sc_dev, "timeout waiting for master\n");

	CSR_WRITE_4(sc, IPW_CSR_RST, CSR_READ_4(sc, IPW_CSR_RST) |
	    IPW_RST_PRINCETON_RESET);

	sc->flags &= ~IPW_FLAG_FW_INITED;
}

static int
ipw_reset(struct ipw_softc *sc)
{
	int ntries;

	ipw_stop_master(sc);

	/* move adapter to D0 state */
	CSR_WRITE_4(sc, IPW_CSR_CTL, CSR_READ_4(sc, IPW_CSR_CTL) |
	    IPW_CTL_INIT);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (CSR_READ_4(sc, IPW_CSR_CTL) & IPW_CTL_CLOCK_READY)
			break;
		DELAY(200);
	}
	if (ntries == 1000)
		return EIO;

	CSR_WRITE_4(sc, IPW_CSR_RST, CSR_READ_4(sc, IPW_CSR_RST) |
	    IPW_RST_SW_RESET);

	DELAY(10);

	CSR_WRITE_4(sc, IPW_CSR_CTL, CSR_READ_4(sc, IPW_CSR_CTL) |
	    IPW_CTL_INIT);

	return 0;
}

/*
 * Upload the microcode to the device.
 */
static int
ipw_load_ucode(struct ipw_softc *sc, u_char *uc, int size)
{
	int ntries;

	MEM_WRITE_4(sc, 0x3000e0, 0x80000000);
	CSR_WRITE_4(sc, IPW_CSR_RST, 0);

	MEM_WRITE_2(sc, 0x220000, 0x0703);
	MEM_WRITE_2(sc, 0x220000, 0x0707);

	MEM_WRITE_1(sc, 0x210014, 0x72);
	MEM_WRITE_1(sc, 0x210014, 0x72);

	MEM_WRITE_1(sc, 0x210000, 0x40);
	MEM_WRITE_1(sc, 0x210000, 0x00);
	MEM_WRITE_1(sc, 0x210000, 0x40);

	MEM_WRITE_MULTI_1(sc, 0x210010, uc, size);

	MEM_WRITE_1(sc, 0x210000, 0x00);
	MEM_WRITE_1(sc, 0x210000, 0x00);
	MEM_WRITE_1(sc, 0x210000, 0x80);

	MEM_WRITE_2(sc, 0x220000, 0x0703);
	MEM_WRITE_2(sc, 0x220000, 0x0707);

	MEM_WRITE_1(sc, 0x210014, 0x72);
	MEM_WRITE_1(sc, 0x210014, 0x72);

	MEM_WRITE_1(sc, 0x210000, 0x00);
	MEM_WRITE_1(sc, 0x210000, 0x80);

	for (ntries = 0; ntries < 10; ntries++) {
		if (MEM_READ_1(sc, 0x210000) & 1)
			break;
		DELAY(10);
	}
	if (ntries == 10) {
		aprint_error_dev(sc->sc_dev, "timeout waiting for ucode to initialize\n");
		return EIO;
	}

	MEM_WRITE_4(sc, 0x3000e0, 0);

	return 0;
}

/* set of macros to handle unaligned little endian data in firmware image */
#define GETLE32(p) ((p)[0] | (p)[1] << 8 | (p)[2] << 16 | (p)[3] << 24)
#define GETLE16(p) ((p)[0] | (p)[1] << 8)
static int
ipw_load_firmware(struct ipw_softc *sc, u_char *fw, int size)
{
	u_char *p, *end;
	uint32_t dst;
	uint16_t len;
	int error;

	p = fw;
	end = fw + size;
	while (p < end) {
		dst = GETLE32(p); p += 4;
		len = GETLE16(p); p += 2;

		ipw_write_mem_1(sc, dst, p, len);
		p += len;
	}

	CSR_WRITE_4(sc, IPW_CSR_IO, IPW_IO_GPIO1_ENABLE | IPW_IO_GPIO3_MASK |
	    IPW_IO_LED_OFF);

	/* enable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, IPW_INTR_MASK);

	/* kick the firmware */
	CSR_WRITE_4(sc, IPW_CSR_RST, 0);

	CSR_WRITE_4(sc, IPW_CSR_CTL, CSR_READ_4(sc, IPW_CSR_CTL) |
	    IPW_CTL_ALLOW_STANDBY);

	/* wait at most one second for firmware initialization to complete */
	if ((error = tsleep(sc, 0, "ipwinit", hz)) != 0) {
		aprint_error_dev(sc->sc_dev, "timeout waiting for firmware initialization "
		    "to complete\n");
		return error;
	}

	CSR_WRITE_4(sc, IPW_CSR_IO, CSR_READ_4(sc, IPW_CSR_IO) |
	    IPW_IO_GPIO1_MASK | IPW_IO_GPIO3_MASK);

	return 0;
}

/*
 * Store firmware into kernel memory so we can download it when we need to,
 * e.g when the adapter wakes up from suspend mode.
 */
static int
ipw_cache_firmware(struct ipw_softc *sc)
{
	struct ipw_firmware *fw = &sc->fw;
	struct ipw_firmware_hdr hdr;
	firmware_handle_t fwh;
	off_t fwsz, p;
	int error;

	ipw_free_firmware(sc);

	if (ipw_accept_eula == 0) {
		aprint_error_dev(sc->sc_dev,
		    "EULA not accepted; please see the ipw(4) man page.\n");
		return EPERM;
	}

	if ((error = firmware_open("if_ipw", sc->sc_fwname, &fwh)) != 0)
		goto fail0;

	fwsz = firmware_get_size(fwh);

	if (fwsz < sizeof(hdr))
		goto fail2;

	if ((error = firmware_read(fwh, 0, &hdr, sizeof(hdr))) != 0)
		goto fail2;

	fw->main_size  = le32toh(hdr.main_size);
	fw->ucode_size = le32toh(hdr.ucode_size);

	fw->main = firmware_malloc(fw->main_size);
	if (fw->main == NULL) {
		error = ENOMEM;
		goto fail1;
	}

	fw->ucode = firmware_malloc(fw->ucode_size);
	if (fw->ucode == NULL) {
		error = ENOMEM;
		goto fail2;
	}

	p = sizeof(hdr);
	if ((error = firmware_read(fwh, p, fw->main, fw->main_size)) != 0)
		goto fail3;

	p += fw->main_size;
	if ((error = firmware_read(fwh, p, fw->ucode, fw->ucode_size)) != 0)
		goto fail3;

	DPRINTF(("Firmware cached: main %u, ucode %u\n", fw->main_size,
	    fw->ucode_size));

	sc->flags |= IPW_FLAG_FW_CACHED;

	firmware_close(fwh);

	return 0;

fail3:	firmware_free(fw->ucode, fw->ucode_size);
fail2:	firmware_free(fw->main, fw->main_size);
fail1:  firmware_close(fwh);
fail0:
	return error;
}

static void
ipw_free_firmware(struct ipw_softc *sc)
{
	if (!(sc->flags & IPW_FLAG_FW_CACHED))
		return;

	firmware_free(sc->fw.main, sc->fw.main_size);
	firmware_free(sc->fw.ucode, sc->fw.ucode_size);

	sc->flags &= ~IPW_FLAG_FW_CACHED;
}

static int
ipw_config(struct ipw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct ipw_security security;
	struct ieee80211_key *k;
	struct ipw_wep_key wepkey;
	struct ipw_scan_options options;
	struct ipw_configuration config;
	uint32_t data;
	int error, i;

	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
	case IEEE80211_M_HOSTAP:
		data = htole32(IPW_MODE_BSS);
		break;

	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		data = htole32(IPW_MODE_IBSS);
		break;

	case IEEE80211_M_MONITOR:
		data = htole32(IPW_MODE_MONITOR);
		break;
	}
	DPRINTF(("Setting mode to %u\n", le32toh(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_MODE, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS ||
	    ic->ic_opmode == IEEE80211_M_MONITOR) {
		data = htole32(ieee80211_chan2ieee(ic, ic->ic_ibss_chan));
		DPRINTF(("Setting channel to %u\n", le32toh(data)));
		error = ipw_cmd(sc, IPW_CMD_SET_CHANNEL, &data, sizeof data);
		if (error != 0)
			return error;
	}

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		DPRINTF(("Enabling adapter\n"));
		return ipw_cmd(sc, IPW_CMD_ENABLE, NULL, 0);
	}

	DPRINTF(("Setting MAC to %s\n", ether_sprintf(ic->ic_myaddr)));
	error = ipw_cmd(sc, IPW_CMD_SET_MAC_ADDRESS, ic->ic_myaddr,
	    IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;

	config.flags = htole32(IPW_CFG_BSS_MASK | IPW_CFG_IBSS_MASK |
	    IPW_CFG_PREAMBLE_AUTO | IPW_CFG_802_1x_ENABLE);

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		config.flags |= htole32(IPW_CFG_IBSS_AUTO_START);
	if (ifp->if_flags & IFF_PROMISC)
		config.flags |= htole32(IPW_CFG_PROMISCUOUS);
	config.bss_chan = htole32(0x3fff); /* channels 1-14 */
	config.ibss_chan = htole32(0x7ff); /* channels 1-11 */
	DPRINTF(("Setting adapter configuration 0x%08x\n", config.flags));
	error = ipw_cmd(sc, IPW_CMD_SET_CONFIGURATION, &config, sizeof config);
	if (error != 0)
		return error;

	data = htole32(0x3); /* 1, 2 */
	DPRINTF(("Setting basic tx rates to 0x%x\n", le32toh(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_BASIC_TX_RATES, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(0xf); /* 1, 2, 5.5, 11 */
	DPRINTF(("Setting tx rates to 0x%x\n", le32toh(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_TX_RATES, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(IPW_POWER_MODE_CAM);
	DPRINTF(("Setting power mode to %u\n", le32toh(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_POWER_MODE, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		data = htole32(32); /* default value */
		DPRINTF(("Setting tx power index to %u\n", le32toh(data)));
		error = ipw_cmd(sc, IPW_CMD_SET_TX_POWER_INDEX, &data,
		    sizeof data);
		if (error != 0)
			return error;
	}

	data = htole32(ic->ic_rtsthreshold);
	DPRINTF(("Setting RTS threshold to %u\n", le32toh(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_RTS_THRESHOLD, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(ic->ic_fragthreshold);
	DPRINTF(("Setting frag threshold to %u\n", le32toh(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_FRAG_THRESHOLD, &data, sizeof data);
	if (error != 0)
		return error;

#ifdef IPW_DEBUG
	if (ipw_debug > 0) {
		printf("Setting ESSID to ");
		ieee80211_print_essid(ic->ic_des_essid, ic->ic_des_esslen);
		printf("\n");
	}
#endif
	error = ipw_cmd(sc, IPW_CMD_SET_ESSID, ic->ic_des_essid,
	    ic->ic_des_esslen);
	if (error != 0)
		return error;

	/* no mandatory BSSID */
	DPRINTF(("Setting mandatory BSSID to null\n"));
	error = ipw_cmd(sc, IPW_CMD_SET_MANDATORY_BSSID, NULL, 0);
	if (error != 0)
		return error;

	if (ic->ic_flags & IEEE80211_F_DESBSSID) {
		DPRINTF(("Setting desired BSSID to %s\n",
		    ether_sprintf(ic->ic_des_bssid)));
		error = ipw_cmd(sc, IPW_CMD_SET_DESIRED_BSSID,
		    ic->ic_des_bssid, IEEE80211_ADDR_LEN);
		if (error != 0)
			return error;
	}

	(void)memset(&security, 0, sizeof(security));
	security.authmode = (ic->ic_bss->ni_authmode == IEEE80211_AUTH_SHARED) ?
	    IPW_AUTH_SHARED : IPW_AUTH_OPEN;
	security.ciphers = htole32(IPW_CIPHER_NONE);
	DPRINTF(("Setting authmode to %u\n", security.authmode));
	error = ipw_cmd(sc, IPW_CMD_SET_SECURITY_INFORMATION, &security,
	    sizeof security);
	if (error != 0)
		return error;

	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		k = ic->ic_crypto.cs_nw_keys;
		for (i = 0; i < IEEE80211_WEP_NKID; i++, k++) {
			if (k->wk_keylen == 0)
				continue;

			wepkey.idx = i;
			wepkey.len = k->wk_keylen;
			memset(wepkey.key, 0, sizeof(wepkey.key));
			memcpy(wepkey.key, k->wk_key, k->wk_keylen);
			DPRINTF(("Setting wep key index %u len %u\n",
			    wepkey.idx, wepkey.len));
			error = ipw_cmd(sc, IPW_CMD_SET_WEP_KEY, &wepkey,
			    sizeof wepkey);
			if (error != 0)
				return error;
		}

		data = htole32(ic->ic_crypto.cs_def_txkey);
		DPRINTF(("Setting tx key index to %u\n", le32toh(data)));
		error = ipw_cmd(sc, IPW_CMD_SET_WEP_KEY_INDEX, &data,
		    sizeof data);
		if (error != 0)
			return error;
	}

	data = htole32((sc->sc_ic.ic_flags & IEEE80211_F_PRIVACY) ? IPW_WEPON : 0);
	DPRINTF(("Setting wep flags to 0x%x\n", le32toh(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_WEP_FLAGS, &data, sizeof data);
	if (error != 0)
		return error;

#if 0
	struct ipw_wpa_ie ie;

	memset(&ie, 0 sizeof(ie));
	ie.len = htole32(sizeof (struct ieee80211_ie_wpa));
	DPRINTF(("Setting wpa ie\n"));
	error = ipw_cmd(sc, IPW_CMD_SET_WPA_IE, &ie, sizeof ie);
	if (error != 0)
		return error;
#endif

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		data = htole32(ic->ic_bintval);
		DPRINTF(("Setting beacon interval to %u\n", le32toh(data)));
		error = ipw_cmd(sc, IPW_CMD_SET_BEACON_INTERVAL, &data,
		    sizeof data);
		if (error != 0)
			return error;
	}

	options.flags = 0;
	options.channels = htole32(0x3fff); /* scan channels 1-14 */
	DPRINTF(("Setting scan options to 0x%x\n", le32toh(options.flags)));
	error = ipw_cmd(sc, IPW_CMD_SET_SCAN_OPTIONS, &options, sizeof options);
	if (error != 0)
		return error;

	/* finally, enable adapter (start scanning for an access point) */
	DPRINTF(("Enabling adapter\n"));
	return ipw_cmd(sc, IPW_CMD_ENABLE, NULL, 0);
}

static int
ipw_init(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ipw_firmware *fw = &sc->fw;

	if (!(sc->flags & IPW_FLAG_FW_CACHED)) {
		if (ipw_cache_firmware(sc) != 0) {
			aprint_error_dev(sc->sc_dev, "could not cache the firmware (%s)\n",
			    sc->sc_fwname);
			goto fail;
		}
	}

	ipw_stop(ifp, 0);

	if (ipw_reset(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "could not reset adapter\n");
		goto fail;
	}

	if (ipw_load_ucode(sc, fw->ucode, fw->ucode_size) != 0) {
		aprint_error_dev(sc->sc_dev, "could not load microcode\n");
		goto fail;
	}

	ipw_stop_master(sc);

	/*
	 * Setup tx, rx and status rings.
	 */
	sc->txold = IPW_NTBD - 1;
	sc->txcur = 0;
	sc->txfree = IPW_NTBD - 2;
	sc->rxcur = IPW_NRBD - 1;

	CSR_WRITE_4(sc, IPW_CSR_TX_BASE,  sc->tbd_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IPW_CSR_TX_SIZE,  IPW_NTBD);
	CSR_WRITE_4(sc, IPW_CSR_TX_READ,  0);
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE, sc->txcur);

	CSR_WRITE_4(sc, IPW_CSR_RX_BASE,  sc->rbd_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IPW_CSR_RX_SIZE,  IPW_NRBD);
	CSR_WRITE_4(sc, IPW_CSR_RX_READ,  0);
	CSR_WRITE_4(sc, IPW_CSR_RX_WRITE, sc->rxcur);

	CSR_WRITE_4(sc, IPW_CSR_STATUS_BASE, sc->status_map->dm_segs[0].ds_addr);

	if (ipw_load_firmware(sc, fw->main, fw->main_size) != 0) {
		aprint_error_dev(sc->sc_dev, "could not load firmware\n");
		goto fail;
	}

	sc->flags |= IPW_FLAG_FW_INITED;

	/* retrieve information tables base addresses */
	sc->table1_base = CSR_READ_4(sc, IPW_CSR_TABLE1_BASE);
	sc->table2_base = CSR_READ_4(sc, IPW_CSR_TABLE2_BASE);

	ipw_write_table1(sc, IPW_INFO_LOCK, 0);

	if (ipw_config(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "device configuration failed\n");
		goto fail;
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	return 0;

fail:	ifp->if_flags &= ~IFF_UP;
	ipw_stop(ifp, 0);

	return EIO;
}

static void
ipw_stop(struct ifnet *ifp, int disable)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int i;

	ipw_stop_master(sc);

	CSR_WRITE_4(sc, IPW_CSR_RST, IPW_RST_SW_RESET);

	/*
	 * Release tx buffers.
	 */
	for (i = 0; i < IPW_NTBD; i++)
		ipw_release_sbd(sc, &sc->stbd_list[i]);

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
}

static void
ipw_read_mem_1(struct ipw_softc *sc, bus_size_t offset, uint8_t *datap,
    bus_size_t count)
{
	for (; count > 0; offset++, datap++, count--) {
		CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, offset & ~3);
		*datap = CSR_READ_1(sc, IPW_CSR_INDIRECT_DATA + (offset & 3));
	}
}

static void
ipw_write_mem_1(struct ipw_softc *sc, bus_size_t offset, uint8_t *datap,
    bus_size_t count)
{
	for (; count > 0; offset++, datap++, count--) {
		CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, offset & ~3);
		CSR_WRITE_1(sc, IPW_CSR_INDIRECT_DATA + (offset & 3), *datap);
	}
}

SYSCTL_SETUP(sysctl_hw_ipw_accept_eula_setup, "sysctl hw.ipw.accept_eula")
{
	const struct sysctlnode *rnode;
	const struct sysctlnode *cnode;

	sysctl_createv(NULL, 0, NULL, &rnode,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "ipw",
		NULL,
		NULL, 0,
		NULL, 0,
		CTL_HW, CTL_CREATE, CTL_EOL);

	sysctl_createv(NULL, 0, &rnode, &cnode,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "accept_eula",
		SYSCTL_DESCR("Accept Intel EULA and permit use of ipw(4) firmware"),
		NULL, 0,
		&ipw_accept_eula, sizeof(ipw_accept_eula),
		CTL_CREATE, CTL_EOL);
}
