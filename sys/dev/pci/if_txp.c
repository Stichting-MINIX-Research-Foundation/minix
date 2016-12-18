/* $NetBSD: if_txp.c,v 1.41 2014/06/16 16:48:16 msaitoh Exp $ */

/*
 * Copyright (c) 2001
 *	Jason L. Wright <jason@thought.net>, Theo de Raadt, and
 *	Aaron Campbell <aaron@monkey.org>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR THE VOICES IN THEIR HEADS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for 3c990 (Typhoon) Ethernet ASIC
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_txp.c,v 1.41 2014/06/16 16:48:16 msaitoh Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/callout.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_ether.h>
#include <net/if_arp.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#include <net/if_media.h>

#include <net/bpf.h>

#include <sys/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_txpreg.h>

#include <dev/microcode/typhoon/3c990img.h>

/*
 * These currently break the 3c990 firmware, hopefully will be resolved
 * at some point.
 */
#undef	TRY_TX_UDP_CSUM
#undef	TRY_TX_TCP_CSUM

int txp_probe(device_t, cfdata_t, void *);
void txp_attach(device_t, device_t, void *);
int txp_intr(void *);
void txp_tick(void *);
bool txp_shutdown(device_t, int);
int txp_ioctl(struct ifnet *, u_long, void *);
void txp_start(struct ifnet *);
void txp_stop(struct txp_softc *);
void txp_init(struct txp_softc *);
void txp_watchdog(struct ifnet *);

int txp_chip_init(struct txp_softc *);
int txp_reset_adapter(struct txp_softc *);
int txp_download_fw(struct txp_softc *);
int txp_download_fw_wait(struct txp_softc *);
int txp_download_fw_section(struct txp_softc *,
    const struct txp_fw_section_header *, int);
int txp_alloc_rings(struct txp_softc *);
void txp_dma_free(struct txp_softc *, struct txp_dma_alloc *);
int txp_dma_malloc(struct txp_softc *, bus_size_t, struct txp_dma_alloc *, int);
void txp_set_filter(struct txp_softc *);

int txp_cmd_desc_numfree(struct txp_softc *);
int txp_command(struct txp_softc *, u_int16_t, u_int16_t, u_int32_t,
    u_int32_t, u_int16_t *, u_int32_t *, u_int32_t *, int);
int txp_command2(struct txp_softc *, u_int16_t, u_int16_t,
    u_int32_t, u_int32_t, struct txp_ext_desc *, u_int8_t,
    struct txp_rsp_desc **, int);
int txp_response(struct txp_softc *, u_int32_t, u_int16_t, u_int16_t,
    struct txp_rsp_desc **);
void txp_rsp_fixup(struct txp_softc *, struct txp_rsp_desc *,
    struct txp_rsp_desc *);
void txp_capabilities(struct txp_softc *);

void txp_ifmedia_sts(struct ifnet *, struct ifmediareq *);
int txp_ifmedia_upd(struct ifnet *);
void txp_show_descriptor(void *);
void txp_tx_reclaim(struct txp_softc *, struct txp_tx_ring *,
    struct txp_dma_alloc *);
void txp_rxbuf_reclaim(struct txp_softc *);
void txp_rx_reclaim(struct txp_softc *, struct txp_rx_ring *,
    struct txp_dma_alloc *);

CFATTACH_DECL_NEW(txp, sizeof(struct txp_softc), txp_probe, txp_attach,
	      NULL, NULL);

const struct txp_pci_match {
	int vid, did, flags;
} txp_devices[] = {
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CR990, 0 },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CR990TX95, 0 },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CR990TX97, 0 },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CR990SVR95, TXP_SERVERVERSION },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CR990SVR97, TXP_SERVERVERSION },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C990B, TXP_USESUBSYSTEM },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C990BSVR, TXP_SERVERVERSION },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3CR990FX, TXP_USESUBSYSTEM },
};

static const struct txp_pci_match *txp_pcilookup(pcireg_t);

static const struct {
	u_int16_t mask, value;
	int flags;
} txp_subsysinfo[] = {
	{0xf000, 0x2000, TXP_SERVERVERSION},
	{0x0100, 0x0100, TXP_FIBER},
#if 0 /* information from 3com header, unused */
	{0x0010, 0x0010, /* secured firmware */},
	{0x0003, 0x0000, /* variable DES */},
	{0x0003, 0x0001, /* single DES - "95" */},
	{0x0003, 0x0002, /* triple DES - "97" */},
#endif
};

static const struct txp_pci_match *
txp_pcilookup(pcireg_t id)
{
	int i;

	for (i = 0; i < __arraycount(txp_devices); i++)
		if (PCI_VENDOR(id) == txp_devices[i].vid &&
		    PCI_PRODUCT(id) == txp_devices[i].did)
			return &txp_devices[i];
	return (0);
}

int
txp_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (txp_pcilookup(pa->pa_id))
			return (1);
	return (0);
}

void
txp_attach(device_t parent, device_t self, void *aux)
{
	struct txp_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	u_int32_t command;
	u_int16_t p1;
	u_int32_t p2;
	u_char enaddr[6];
	const struct txp_pci_match *match;
	u_int16_t subsys;
	int i, flags;
	char devinfo[256];
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_cold = 1;

	match = txp_pcilookup(pa->pa_id);
	flags = match->flags;
	if (match->flags & TXP_USESUBSYSTEM) {
		subsys = PCI_PRODUCT(pci_conf_read(pc, pa->pa_tag,
						   PCI_SUBSYS_ID_REG));
		for (i = 0;
		     i < sizeof(txp_subsysinfo)/sizeof(txp_subsysinfo[0]);
		     i++)
			if ((subsys & txp_subsysinfo[i].mask) ==
			    txp_subsysinfo[i].value)
				flags |= txp_subsysinfo[i].flags;
	}
	sc->sc_flags = flags;

	pci_devinfo(pa->pa_id, 0, 0, devinfo, sizeof(devinfo));
#define TXP_EXTRAINFO ((flags & (TXP_USESUBSYSTEM|TXP_SERVERVERSION)) == \
  (TXP_USESUBSYSTEM|TXP_SERVERVERSION) ? " (SVR)" : "")
	printf(": %s%s\n%s", devinfo, TXP_EXTRAINFO, device_xname(sc->sc_dev));

	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(command & PCI_COMMAND_MASTER_ENABLE)) {
		printf(": failed to enable bus mastering\n");
		return;
	}

	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}
	if (pci_mapreg_map(pa, TXP_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_bt, &sc->sc_bh, NULL, NULL)) {
		printf(": can't map mem space %d\n", 0);
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, txp_intr, sc);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": interrupting at %s\n", intrstr);

	if (txp_chip_init(sc))
		goto cleanupintr;

	if (txp_download_fw(sc))
		goto cleanupintr;

	if (txp_alloc_rings(sc))
		goto cleanupintr;

	if (txp_command(sc, TXP_CMD_MAX_PKT_SIZE_WRITE, TXP_MAX_PKTLEN, 0, 0,
	    NULL, NULL, NULL, 1))
		goto cleanupintr;

	if (txp_command(sc, TXP_CMD_STATION_ADDRESS_READ, 0, 0, 0,
	    &p1, &p2, NULL, 1))
		goto cleanupintr;

	txp_set_filter(sc);

	p1 = htole16(p1);
	enaddr[0] = ((u_int8_t *)&p1)[1];
	enaddr[1] = ((u_int8_t *)&p1)[0];
	p2 = htole32(p2);
	enaddr[2] = ((u_int8_t *)&p2)[3];
	enaddr[3] = ((u_int8_t *)&p2)[2];
	enaddr[4] = ((u_int8_t *)&p2)[1];
	enaddr[5] = ((u_int8_t *)&p2)[0];

	printf("%s: Ethernet address %s\n", device_xname(sc->sc_dev),
	       ether_sprintf(enaddr));
	sc->sc_cold = 0;

	ifmedia_init(&sc->sc_ifmedia, 0, txp_ifmedia_upd, txp_ifmedia_sts);
	if (flags & TXP_FIBER) {
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_FX,
			    0, NULL);
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_FX|IFM_HDX,
			    0, NULL);
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_FX|IFM_FDX,
			    0, NULL);
	} else {
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T,
			    0, NULL);
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T|IFM_HDX,
			    0, NULL);
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_10_T|IFM_FDX,
			    0, NULL);
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX,
			    0, NULL);
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX|IFM_HDX,
			    0, NULL);
		ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_100_TX|IFM_FDX,
			    0, NULL);
	}
	ifmedia_add(&sc->sc_ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);

	sc->sc_xcvr = TXP_XCVR_AUTO;
	txp_command(sc, TXP_CMD_XCVR_SELECT, TXP_XCVR_AUTO, 0, 0,
	    NULL, NULL, NULL, 0);
	ifmedia_set(&sc->sc_ifmedia, IFM_ETHER|IFM_AUTO);

	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = txp_ioctl;
	ifp->if_start = txp_start;
	ifp->if_watchdog = txp_watchdog;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, TX_ENTRIES);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_capabilities = 0;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	txp_capabilities(sc);

	callout_init(&sc->sc_tick, 0);
	callout_setfunc(&sc->sc_tick, txp_tick, sc);

	/*
	 * Attach us everywhere
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);

	if (pmf_device_register1(self, NULL, NULL, txp_shutdown))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

cleanupintr:
	pci_intr_disestablish(pc,sc->sc_ih);

	return;

}

int
txp_chip_init(struct txp_softc *sc)
{
	/* disable interrupts */
	WRITE_REG(sc, TXP_IER, 0);
	WRITE_REG(sc, TXP_IMR,
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_LATCH);

	/* ack all interrupts */
	WRITE_REG(sc, TXP_ISR, TXP_INT_RESERVED | TXP_INT_LATCH |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_A2H_3 | TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0);

	if (txp_reset_adapter(sc))
		return (-1);

	/* disable interrupts */
	WRITE_REG(sc, TXP_IER, 0);
	WRITE_REG(sc, TXP_IMR,
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_LATCH);

	/* ack all interrupts */
	WRITE_REG(sc, TXP_ISR, TXP_INT_RESERVED | TXP_INT_LATCH |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_A2H_3 | TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0);

	return (0);
}

int
txp_reset_adapter(struct txp_softc *sc)
{
	u_int32_t r;
	int i;

	WRITE_REG(sc, TXP_SRR, TXP_SRR_ALL);
	DELAY(1000);
	WRITE_REG(sc, TXP_SRR, 0);

	/* Should wait max 6 seconds */
	for (i = 0; i < 6000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_HOST_REQUEST)
			break;
		DELAY(1000);
	}

	if (r != STAT_WAITING_FOR_HOST_REQUEST) {
		printf("%s: reset hung\n", TXP_DEVNAME(sc));
		return (-1);
	}

	return (0);
}

int
txp_download_fw(struct txp_softc *sc)
{
	const struct txp_fw_file_header *fileheader;
	const struct txp_fw_section_header *secthead;
	int sect;
	u_int32_t r, i, ier, imr;

	ier = READ_REG(sc, TXP_IER);
	WRITE_REG(sc, TXP_IER, ier | TXP_INT_A2H_0);

	imr = READ_REG(sc, TXP_IMR);
	WRITE_REG(sc, TXP_IMR, imr | TXP_INT_A2H_0);

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_HOST_REQUEST)
			break;
		DELAY(50);
	}
	if (r != STAT_WAITING_FOR_HOST_REQUEST) {
		printf(": not waiting for host request\n");
		return (-1);
	}

	/* Ack the status */
	WRITE_REG(sc, TXP_ISR, TXP_INT_A2H_0);

	fileheader = (const struct txp_fw_file_header *)tc990image;
	if (memcmp("TYPHOON", fileheader->magicid, sizeof(fileheader->magicid))) {
		printf(": fw invalid magic\n");
		return (-1);
	}

	/* Tell boot firmware to get ready for image */
	WRITE_REG(sc, TXP_H2A_1, le32toh(fileheader->addr));
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_RUNTIME_IMAGE);

	if (txp_download_fw_wait(sc)) {
		printf("%s: fw wait failed, initial\n", device_xname(sc->sc_dev));
		return (-1);
	}

	secthead = (const struct txp_fw_section_header *)
		(((const u_int8_t *)tc990image) +
		 sizeof(struct txp_fw_file_header));

	for (sect = 0; sect < le32toh(fileheader->nsections); sect++) {
		if (txp_download_fw_section(sc, secthead, sect))
			return (-1);
		secthead = (const struct txp_fw_section_header *)
		    (((const u_int8_t *)secthead) + le32toh(secthead->nbytes) +
			sizeof(*secthead));
	}

	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_DOWNLOAD_COMPLETE);

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_BOOT)
			break;
		DELAY(50);
	}
	if (r != STAT_WAITING_FOR_BOOT) {
		printf(": not waiting for boot\n");
		return (-1);
	}

	WRITE_REG(sc, TXP_IER, ier);
	WRITE_REG(sc, TXP_IMR, imr);

	return (0);
}

int
txp_download_fw_wait(struct txp_softc *sc)
{
	u_int32_t i, r;

	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_ISR);
		if (r & TXP_INT_A2H_0)
			break;
		DELAY(50);
	}

	if (!(r & TXP_INT_A2H_0)) {
		printf(": fw wait failed comm0\n");
		return (-1);
	}

	WRITE_REG(sc, TXP_ISR, TXP_INT_A2H_0);

	r = READ_REG(sc, TXP_A2H_0);
	if (r != STAT_WAITING_FOR_SEGMENT) {
		printf(": fw not waiting for segment\n");
		return (-1);
	}
	return (0);
}

int
txp_download_fw_section(struct txp_softc *sc, const struct txp_fw_section_header *sect, int sectnum)
{
	struct txp_dma_alloc dma;
	int rseg, err = 0;
	struct mbuf m;
#ifdef INET
	u_int16_t csum;
#endif

	/* Skip zero length sections */
	if (sect->nbytes == 0)
		return (0);

	/* Make sure we aren't past the end of the image */
	rseg = ((const u_int8_t *)sect) - ((const u_int8_t *)tc990image);
	if (rseg >= sizeof(tc990image)) {
		printf(": fw invalid section address, section %d\n", sectnum);
		return (-1);
	}

	/* Make sure this section doesn't go past the end */
	rseg += le32toh(sect->nbytes);
	if (rseg >= sizeof(tc990image)) {
		printf(": fw truncated section %d\n", sectnum);
		return (-1);
	}

	/* map a buffer, copy segment to it, get physaddr */
	if (txp_dma_malloc(sc, le32toh(sect->nbytes), &dma, 0)) {
		printf(": fw dma malloc failed, section %d\n", sectnum);
		return (-1);
	}

	memcpy(dma.dma_vaddr, ((const u_int8_t *)sect) + sizeof(*sect),
	    le32toh(sect->nbytes));

	/*
	 * dummy up mbuf and verify section checksum
	 */
	m.m_type = MT_DATA;
	m.m_next = m.m_nextpkt = NULL;
	m.m_len = le32toh(sect->nbytes);
	m.m_data = dma.dma_vaddr;
	m.m_flags = 0;
#ifdef INET
	csum = in_cksum(&m, le32toh(sect->nbytes));
	if (csum != sect->cksum) {
		printf(": fw section %d, bad cksum (expected 0x%x got 0x%x)\n",
		    sectnum, sect->cksum, csum);
		txp_dma_free(sc, &dma);
		return -1;
	}
#endif

	bus_dmamap_sync(sc->sc_dmat, dma.dma_map, 0,
	    dma.dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);

	WRITE_REG(sc, TXP_H2A_1, le32toh(sect->nbytes));
	WRITE_REG(sc, TXP_H2A_2, le32toh(sect->cksum));
	WRITE_REG(sc, TXP_H2A_3, le32toh(sect->addr));
	WRITE_REG(sc, TXP_H2A_4, dma.dma_paddr >> 32);
	WRITE_REG(sc, TXP_H2A_5, dma.dma_paddr & 0xffffffff);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_SEGMENT_AVAILABLE);

	if (txp_download_fw_wait(sc)) {
		printf("%s: fw wait failed, section %d\n",
		    device_xname(sc->sc_dev), sectnum);
		err = -1;
	}

	bus_dmamap_sync(sc->sc_dmat, dma.dma_map, 0,
	    dma.dma_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);

	txp_dma_free(sc, &dma);
	return (err);
}

int
txp_intr(void *vsc)
{
	struct txp_softc *sc = vsc;
	struct txp_hostvar *hv = sc->sc_hostvar;
	u_int32_t isr;
	int claimed = 0;

	/* mask all interrupts */
	WRITE_REG(sc, TXP_IMR, TXP_INT_RESERVED | TXP_INT_SELF |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0 |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |  TXP_INT_LATCH);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_host_dma.dma_map, 0,
	    sizeof(struct txp_hostvar), BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);

	isr = READ_REG(sc, TXP_ISR);
	while (isr) {
		claimed = 1;
		WRITE_REG(sc, TXP_ISR, isr);

		if ((*sc->sc_rxhir.r_roff) != (*sc->sc_rxhir.r_woff))
			txp_rx_reclaim(sc, &sc->sc_rxhir, &sc->sc_rxhiring_dma);
		if ((*sc->sc_rxlor.r_roff) != (*sc->sc_rxlor.r_woff))
			txp_rx_reclaim(sc, &sc->sc_rxlor, &sc->sc_rxloring_dma);

		if (hv->hv_rx_buf_write_idx == hv->hv_rx_buf_read_idx)
			txp_rxbuf_reclaim(sc);

		if (sc->sc_txhir.r_cnt && (sc->sc_txhir.r_cons !=
		    TXP_OFFSET2IDX(le32toh(*(sc->sc_txhir.r_off)))))
			txp_tx_reclaim(sc, &sc->sc_txhir, &sc->sc_txhiring_dma);

		if (sc->sc_txlor.r_cnt && (sc->sc_txlor.r_cons !=
		    TXP_OFFSET2IDX(le32toh(*(sc->sc_txlor.r_off)))))
			txp_tx_reclaim(sc, &sc->sc_txlor, &sc->sc_txloring_dma);

		isr = READ_REG(sc, TXP_ISR);
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_host_dma.dma_map, 0,
	    sizeof(struct txp_hostvar), BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);

	/* unmask all interrupts */
	WRITE_REG(sc, TXP_IMR, TXP_INT_A2H_3);

	txp_start(&sc->sc_arpcom.ec_if);

	return (claimed);
}

void
txp_rx_reclaim(struct txp_softc *sc, struct txp_rx_ring *r, struct txp_dma_alloc *dma)
{
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	struct txp_rx_desc *rxd;
	struct mbuf *m;
	struct txp_swdesc *sd;
	u_int32_t roff, woff;
	int sumflags = 0;
	int idx;

	roff = le32toh(*r->r_roff);
	woff = le32toh(*r->r_woff);
	idx = roff / sizeof(struct txp_rx_desc);
	rxd = r->r_desc + idx;

	while (roff != woff) {

		bus_dmamap_sync(sc->sc_dmat, dma->dma_map,
		    idx * sizeof(struct txp_rx_desc), sizeof(struct txp_rx_desc),
		    BUS_DMASYNC_POSTREAD);

		if (rxd->rx_flags & RX_FLAGS_ERROR) {
			printf("%s: error 0x%x\n", device_xname(sc->sc_dev),
			    le32toh(rxd->rx_stat));
			ifp->if_ierrors++;
			goto next;
		}

		/* retrieve stashed pointer */
		memcpy(&sd, __UNVOLATILE(&rxd->rx_vaddrlo), sizeof(sd));

		bus_dmamap_sync(sc->sc_dmat, sd->sd_map, 0,
		    sd->sd_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, sd->sd_map);
		bus_dmamap_destroy(sc->sc_dmat, sd->sd_map);
		m = sd->sd_mbuf;
		free(sd, M_DEVBUF);
		m->m_pkthdr.len = m->m_len = le16toh(rxd->rx_len);

#ifdef __STRICT_ALIGNMENT
		{
			/*
			 * XXX Nice chip, except it won't accept "off by 2"
			 * buffers, so we're force to copy.  Supposedly
			 * this will be fixed in a newer firmware rev
			 * and this will be temporary.
			 */
			struct mbuf *mnew;

			MGETHDR(mnew, M_DONTWAIT, MT_DATA);
			if (mnew == NULL) {
				m_freem(m);
				goto next;
			}
			if (m->m_len > (MHLEN - 2)) {
				MCLGET(mnew, M_DONTWAIT);
				if (!(mnew->m_flags & M_EXT)) {
					m_freem(mnew);
					m_freem(m);
					goto next;
				}
			}
			mnew->m_pkthdr.rcvif = ifp;
			mnew->m_pkthdr.len = mnew->m_len = m->m_len;
			mnew->m_data += 2;
			memcpy(mnew->m_data, m->m_data, m->m_len);
			m_freem(m);
			m = mnew;
		}
#endif

		/*
		 * Handle BPF listeners. Let the BPF user see the packet.
		 */
		bpf_mtap(ifp, m);

		if (rxd->rx_stat & htole32(RX_STAT_IPCKSUMBAD))
			sumflags |= (M_CSUM_IPv4|M_CSUM_IPv4_BAD);
		else if (rxd->rx_stat & htole32(RX_STAT_IPCKSUMGOOD))
			sumflags |= M_CSUM_IPv4;

		if (rxd->rx_stat & htole32(RX_STAT_TCPCKSUMBAD))
			sumflags |= (M_CSUM_TCPv4|M_CSUM_TCP_UDP_BAD);
		else if (rxd->rx_stat & htole32(RX_STAT_TCPCKSUMGOOD))
			sumflags |= M_CSUM_TCPv4;

		if (rxd->rx_stat & htole32(RX_STAT_UDPCKSUMBAD))
			sumflags |= (M_CSUM_UDPv4|M_CSUM_TCP_UDP_BAD);
		else if (rxd->rx_stat & htole32(RX_STAT_UDPCKSUMGOOD))
			sumflags |= M_CSUM_UDPv4;

		m->m_pkthdr.csum_flags = sumflags;

		if (rxd->rx_stat & htole32(RX_STAT_VLAN)) {
			VLAN_INPUT_TAG(ifp, m, htons(rxd->rx_vlan >> 16),
			    continue);
		}

		(*ifp->if_input)(ifp, m);

next:
		bus_dmamap_sync(sc->sc_dmat, dma->dma_map,
		    idx * sizeof(struct txp_rx_desc), sizeof(struct txp_rx_desc),
		    BUS_DMASYNC_PREREAD);

		roff += sizeof(struct txp_rx_desc);
		if (roff == (RX_ENTRIES * sizeof(struct txp_rx_desc))) {
			idx = 0;
			roff = 0;
			rxd = r->r_desc;
		} else {
			idx++;
			rxd++;
		}
		woff = le32toh(*r->r_woff);
	}

	*r->r_roff = htole32(woff);
}

void
txp_rxbuf_reclaim(struct txp_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_rxbuf_desc *rbd;
	struct txp_swdesc *sd;
	u_int32_t i, end;

	end = TXP_OFFSET2IDX(le32toh(hv->hv_rx_buf_read_idx));
	i = TXP_OFFSET2IDX(le32toh(hv->hv_rx_buf_write_idx));

	if (++i == RXBUF_ENTRIES)
		i = 0;

	rbd = sc->sc_rxbufs + i;

	while (i != end) {
		sd = (struct txp_swdesc *)malloc(sizeof(struct txp_swdesc),
		    M_DEVBUF, M_NOWAIT);
		if (sd == NULL)
			break;

		MGETHDR(sd->sd_mbuf, M_DONTWAIT, MT_DATA);
		if (sd->sd_mbuf == NULL)
			goto err_sd;

		MCLGET(sd->sd_mbuf, M_DONTWAIT);
		if ((sd->sd_mbuf->m_flags & M_EXT) == 0)
			goto err_mbuf;
		sd->sd_mbuf->m_pkthdr.rcvif = ifp;
		sd->sd_mbuf->m_pkthdr.len = sd->sd_mbuf->m_len = MCLBYTES;
		if (bus_dmamap_create(sc->sc_dmat, TXP_MAX_PKTLEN, 1,
		    TXP_MAX_PKTLEN, 0, BUS_DMA_NOWAIT, &sd->sd_map))
			goto err_mbuf;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, sd->sd_map, sd->sd_mbuf,
		    BUS_DMA_NOWAIT)) {
			bus_dmamap_destroy(sc->sc_dmat, sd->sd_map);
			goto err_mbuf;
		}

		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxbufring_dma.dma_map,
		    i * sizeof(struct txp_rxbuf_desc),
		    sizeof(struct txp_rxbuf_desc), BUS_DMASYNC_POSTWRITE);

		/* stash away pointer */
		memcpy(__UNVOLATILE(&rbd->rb_vaddrlo), &sd, sizeof(sd));

		rbd->rb_paddrlo = ((u_int64_t)sd->sd_map->dm_segs[0].ds_addr)
		    & 0xffffffff;
		rbd->rb_paddrhi = ((u_int64_t)sd->sd_map->dm_segs[0].ds_addr)
		    >> 32;

		bus_dmamap_sync(sc->sc_dmat, sd->sd_map, 0,
		    sd->sd_map->dm_mapsize, BUS_DMASYNC_PREREAD);

		bus_dmamap_sync(sc->sc_dmat, sc->sc_rxbufring_dma.dma_map,
		    i * sizeof(struct txp_rxbuf_desc),
		    sizeof(struct txp_rxbuf_desc), BUS_DMASYNC_PREWRITE);

		hv->hv_rx_buf_write_idx = htole32(TXP_IDX2OFFSET(i));

		if (++i == RXBUF_ENTRIES) {
			i = 0;
			rbd = sc->sc_rxbufs;
		} else
			rbd++;
	}
	return;

err_mbuf:
	m_freem(sd->sd_mbuf);
err_sd:
	free(sd, M_DEVBUF);
}

/*
 * Reclaim mbufs and entries from a transmit ring.
 */
void
txp_tx_reclaim(struct txp_softc *sc, struct txp_tx_ring *r, struct txp_dma_alloc *dma)
{
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	u_int32_t idx = TXP_OFFSET2IDX(le32toh(*(r->r_off)));
	u_int32_t cons = r->r_cons, cnt = r->r_cnt;
	struct txp_tx_desc *txd = r->r_desc + cons;
	struct txp_swdesc *sd = sc->sc_txd + cons;
	struct mbuf *m;

	while (cons != idx) {
		if (cnt == 0)
			break;

		bus_dmamap_sync(sc->sc_dmat, dma->dma_map,
		    cons * sizeof(struct txp_tx_desc),
		    sizeof(struct txp_tx_desc),
		    BUS_DMASYNC_POSTWRITE);

		if ((txd->tx_flags & TX_FLAGS_TYPE_M) ==
		    TX_FLAGS_TYPE_DATA) {
			bus_dmamap_sync(sc->sc_dmat, sd->sd_map, 0,
			    sd->sd_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, sd->sd_map);
			m = sd->sd_mbuf;
			if (m != NULL) {
				m_freem(m);
				txd->tx_addrlo = 0;
				txd->tx_addrhi = 0;
				ifp->if_opackets++;
			}
		}
		ifp->if_flags &= ~IFF_OACTIVE;

		if (++cons == TX_ENTRIES) {
			txd = r->r_desc;
			cons = 0;
			sd = sc->sc_txd;
		} else {
			txd++;
			sd++;
		}

		cnt--;
	}

	r->r_cons = cons;
	r->r_cnt = cnt;
	if (cnt == 0)
		ifp->if_timer = 0;
}

bool
txp_shutdown(device_t self, int howto)
{
	struct txp_softc *sc;

	sc = device_private(self);

	/* mask all interrupts */
	WRITE_REG(sc, TXP_IMR,
	    TXP_INT_SELF | TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_LATCH);

	txp_command(sc, TXP_CMD_TX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 0);
	txp_command(sc, TXP_CMD_RX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 0);
	txp_command(sc, TXP_CMD_HALT, 0, 0, 0, NULL, NULL, NULL, 0);

	return true;
}

int
txp_alloc_rings(struct txp_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	struct txp_boot_record *boot;
	struct txp_swdesc *sd;
	u_int32_t r;
	int i, j, nb;

	/* boot record */
	if (txp_dma_malloc(sc, sizeof(struct txp_boot_record), &sc->sc_boot_dma,
	    BUS_DMA_COHERENT)) {
		printf(": can't allocate boot record\n");
		return (-1);
	}
	boot = (struct txp_boot_record *)sc->sc_boot_dma.dma_vaddr;
	memset(boot, 0, sizeof(*boot));
	sc->sc_boot = boot;

	/* host variables */
	if (txp_dma_malloc(sc, sizeof(struct txp_hostvar), &sc->sc_host_dma,
	    BUS_DMA_COHERENT)) {
		printf(": can't allocate host ring\n");
		goto bail_boot;
	}
	memset(sc->sc_host_dma.dma_vaddr, 0, sizeof(struct txp_hostvar));
	boot->br_hostvar_lo = htole32(sc->sc_host_dma.dma_paddr & 0xffffffff);
	boot->br_hostvar_hi = htole32(sc->sc_host_dma.dma_paddr >> 32);
	sc->sc_hostvar = (struct txp_hostvar *)sc->sc_host_dma.dma_vaddr;

	/* high priority tx ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_tx_desc) * TX_ENTRIES,
	    &sc->sc_txhiring_dma, BUS_DMA_COHERENT)) {
		printf(": can't allocate high tx ring\n");
		goto bail_host;
	}
	memset(sc->sc_txhiring_dma.dma_vaddr, 0, sizeof(struct txp_tx_desc) * TX_ENTRIES);
	boot->br_txhipri_lo = htole32(sc->sc_txhiring_dma.dma_paddr & 0xffffffff);
	boot->br_txhipri_hi = htole32(sc->sc_txhiring_dma.dma_paddr >> 32);
	boot->br_txhipri_siz = htole32(TX_ENTRIES * sizeof(struct txp_tx_desc));
	sc->sc_txhir.r_reg = TXP_H2A_1;
	sc->sc_txhir.r_desc = (struct txp_tx_desc *)sc->sc_txhiring_dma.dma_vaddr;
	sc->sc_txhir.r_cons = sc->sc_txhir.r_prod = sc->sc_txhir.r_cnt = 0;
	sc->sc_txhir.r_off = &sc->sc_hostvar->hv_tx_hi_desc_read_idx;
	for (i = 0; i < TX_ENTRIES; i++) {
		if (bus_dmamap_create(sc->sc_dmat, TXP_MAX_PKTLEN,
		    TX_ENTRIES - 4, TXP_MAX_SEGLEN, 0,
		    BUS_DMA_NOWAIT, &sc->sc_txd[i].sd_map) != 0) {
			for (j = 0; j < i; j++) {
				bus_dmamap_destroy(sc->sc_dmat,
				    sc->sc_txd[j].sd_map);
				sc->sc_txd[j].sd_map = NULL;
			}
			goto bail_txhiring;
		}
	}

	/* low priority tx ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_tx_desc) * TX_ENTRIES,
	    &sc->sc_txloring_dma, BUS_DMA_COHERENT)) {
		printf(": can't allocate low tx ring\n");
		goto bail_txhiring;
	}
	memset(sc->sc_txloring_dma.dma_vaddr, 0, sizeof(struct txp_tx_desc) * TX_ENTRIES);
	boot->br_txlopri_lo = htole32(sc->sc_txloring_dma.dma_paddr & 0xffffffff);
	boot->br_txlopri_hi = htole32(sc->sc_txloring_dma.dma_paddr >> 32);
	boot->br_txlopri_siz = htole32(TX_ENTRIES * sizeof(struct txp_tx_desc));
	sc->sc_txlor.r_reg = TXP_H2A_3;
	sc->sc_txlor.r_desc = (struct txp_tx_desc *)sc->sc_txloring_dma.dma_vaddr;
	sc->sc_txlor.r_cons = sc->sc_txlor.r_prod = sc->sc_txlor.r_cnt = 0;
	sc->sc_txlor.r_off = &sc->sc_hostvar->hv_tx_lo_desc_read_idx;

	/* high priority rx ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_rx_desc) * RX_ENTRIES,
	    &sc->sc_rxhiring_dma, BUS_DMA_COHERENT)) {
		printf(": can't allocate high rx ring\n");
		goto bail_txloring;
	}
	memset(sc->sc_rxhiring_dma.dma_vaddr, 0, sizeof(struct txp_rx_desc) * RX_ENTRIES);
	boot->br_rxhipri_lo = htole32(sc->sc_rxhiring_dma.dma_paddr & 0xffffffff);
	boot->br_rxhipri_hi = htole32(sc->sc_rxhiring_dma.dma_paddr >> 32);
	boot->br_rxhipri_siz = htole32(RX_ENTRIES * sizeof(struct txp_rx_desc));
	sc->sc_rxhir.r_desc =
	    (struct txp_rx_desc *)sc->sc_rxhiring_dma.dma_vaddr;
	sc->sc_rxhir.r_roff = &sc->sc_hostvar->hv_rx_hi_read_idx;
	sc->sc_rxhir.r_woff = &sc->sc_hostvar->hv_rx_hi_write_idx;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_rxhiring_dma.dma_map,
	    0, sc->sc_rxhiring_dma.dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	/* low priority ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_rx_desc) * RX_ENTRIES,
	    &sc->sc_rxloring_dma, BUS_DMA_COHERENT)) {
		printf(": can't allocate low rx ring\n");
		goto bail_rxhiring;
	}
	memset(sc->sc_rxloring_dma.dma_vaddr, 0, sizeof(struct txp_rx_desc) * RX_ENTRIES);
	boot->br_rxlopri_lo = htole32(sc->sc_rxloring_dma.dma_paddr & 0xffffffff);
	boot->br_rxlopri_hi = htole32(sc->sc_rxloring_dma.dma_paddr >> 32);
	boot->br_rxlopri_siz = htole32(RX_ENTRIES * sizeof(struct txp_rx_desc));
	sc->sc_rxlor.r_desc =
	    (struct txp_rx_desc *)sc->sc_rxloring_dma.dma_vaddr;
	sc->sc_rxlor.r_roff = &sc->sc_hostvar->hv_rx_lo_read_idx;
	sc->sc_rxlor.r_woff = &sc->sc_hostvar->hv_rx_lo_write_idx;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_rxloring_dma.dma_map,
	    0, sc->sc_rxloring_dma.dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	/* command ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_cmd_desc) * CMD_ENTRIES,
	    &sc->sc_cmdring_dma, BUS_DMA_COHERENT)) {
		printf(": can't allocate command ring\n");
		goto bail_rxloring;
	}
	memset(sc->sc_cmdring_dma.dma_vaddr, 0, sizeof(struct txp_cmd_desc) * CMD_ENTRIES);
	boot->br_cmd_lo = htole32(sc->sc_cmdring_dma.dma_paddr & 0xffffffff);
	boot->br_cmd_hi = htole32(sc->sc_cmdring_dma.dma_paddr >> 32);
	boot->br_cmd_siz = htole32(CMD_ENTRIES * sizeof(struct txp_cmd_desc));
	sc->sc_cmdring.base = (struct txp_cmd_desc *)sc->sc_cmdring_dma.dma_vaddr;
	sc->sc_cmdring.size = CMD_ENTRIES * sizeof(struct txp_cmd_desc);
	sc->sc_cmdring.lastwrite = 0;

	/* response ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_rsp_desc) * RSP_ENTRIES,
	    &sc->sc_rspring_dma, BUS_DMA_COHERENT)) {
		printf(": can't allocate response ring\n");
		goto bail_cmdring;
	}
	memset(sc->sc_rspring_dma.dma_vaddr, 0, sizeof(struct txp_rsp_desc) * RSP_ENTRIES);
	boot->br_resp_lo = htole32(sc->sc_rspring_dma.dma_paddr & 0xffffffff);
	boot->br_resp_hi = htole32(sc->sc_rspring_dma.dma_paddr >> 32);
	boot->br_resp_siz = htole32(CMD_ENTRIES * sizeof(struct txp_rsp_desc));
	sc->sc_rspring.base = (struct txp_rsp_desc *)sc->sc_rspring_dma.dma_vaddr;
	sc->sc_rspring.size = RSP_ENTRIES * sizeof(struct txp_rsp_desc);
	sc->sc_rspring.lastwrite = 0;

	/* receive buffer ring */
	if (txp_dma_malloc(sc, sizeof(struct txp_rxbuf_desc) * RXBUF_ENTRIES,
	    &sc->sc_rxbufring_dma, BUS_DMA_COHERENT)) {
		printf(": can't allocate rx buffer ring\n");
		goto bail_rspring;
	}
	memset(sc->sc_rxbufring_dma.dma_vaddr, 0, sizeof(struct txp_rxbuf_desc) * RXBUF_ENTRIES);
	boot->br_rxbuf_lo = htole32(sc->sc_rxbufring_dma.dma_paddr & 0xffffffff);
	boot->br_rxbuf_hi = htole32(sc->sc_rxbufring_dma.dma_paddr >> 32);
	boot->br_rxbuf_siz = htole32(RXBUF_ENTRIES * sizeof(struct txp_rxbuf_desc));
	sc->sc_rxbufs = (struct txp_rxbuf_desc *)sc->sc_rxbufring_dma.dma_vaddr;
	for (nb = 0; nb < RXBUF_ENTRIES; nb++) {
		sd = (struct txp_swdesc *)malloc(sizeof(struct txp_swdesc),
		    M_DEVBUF, M_NOWAIT);
		/* stash away pointer */
		memcpy(__UNVOLATILE(&sc->sc_rxbufs[nb].rb_vaddrlo), &sd, sizeof(sd));
		if (sd == NULL)
			break;

		MGETHDR(sd->sd_mbuf, M_DONTWAIT, MT_DATA);
		if (sd->sd_mbuf == NULL) {
			goto bail_rxbufring;
		}

		MCLGET(sd->sd_mbuf, M_DONTWAIT);
		if ((sd->sd_mbuf->m_flags & M_EXT) == 0) {
			goto bail_rxbufring;
		}
		sd->sd_mbuf->m_pkthdr.len = sd->sd_mbuf->m_len = MCLBYTES;
		sd->sd_mbuf->m_pkthdr.rcvif = ifp;
		if (bus_dmamap_create(sc->sc_dmat, TXP_MAX_PKTLEN, 1,
		    TXP_MAX_PKTLEN, 0, BUS_DMA_NOWAIT, &sd->sd_map)) {
			goto bail_rxbufring;
		}
		if (bus_dmamap_load_mbuf(sc->sc_dmat, sd->sd_map, sd->sd_mbuf,
		    BUS_DMA_NOWAIT)) {
			bus_dmamap_destroy(sc->sc_dmat, sd->sd_map);
			goto bail_rxbufring;
		}
		bus_dmamap_sync(sc->sc_dmat, sd->sd_map, 0,
		    sd->sd_map->dm_mapsize, BUS_DMASYNC_PREREAD);


		sc->sc_rxbufs[nb].rb_paddrlo =
		    ((u_int64_t)sd->sd_map->dm_segs[0].ds_addr) & 0xffffffff;
		sc->sc_rxbufs[nb].rb_paddrhi =
		    ((u_int64_t)sd->sd_map->dm_segs[0].ds_addr) >> 32;
	}
	bus_dmamap_sync(sc->sc_dmat, sc->sc_rxbufring_dma.dma_map,
	    0, sc->sc_rxbufring_dma.dma_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	sc->sc_hostvar->hv_rx_buf_write_idx = htole32((RXBUF_ENTRIES - 1) *
	    sizeof(struct txp_rxbuf_desc));

	/* zero dma */
	if (txp_dma_malloc(sc, sizeof(u_int32_t), &sc->sc_zero_dma,
	    BUS_DMA_COHERENT)) {
		printf(": can't allocate response ring\n");
		goto bail_rxbufring;
	}
	memset(sc->sc_zero_dma.dma_vaddr, 0, sizeof(u_int32_t));
	boot->br_zero_lo = htole32(sc->sc_zero_dma.dma_paddr & 0xffffffff);
	boot->br_zero_hi = htole32(sc->sc_zero_dma.dma_paddr >> 32);

	/* See if it's waiting for boot, and try to boot it */
	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_WAITING_FOR_BOOT)
			break;
		DELAY(50);
	}
	if (r != STAT_WAITING_FOR_BOOT) {
		printf(": not waiting for boot\n");
		goto bail;
	}
	WRITE_REG(sc, TXP_H2A_2, sc->sc_boot_dma.dma_paddr >> 32);
	WRITE_REG(sc, TXP_H2A_1, sc->sc_boot_dma.dma_paddr & 0xffffffff);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_REGISTER_BOOT_RECORD);

	/* See if it booted */
	for (i = 0; i < 10000; i++) {
		r = READ_REG(sc, TXP_A2H_0);
		if (r == STAT_RUNNING)
			break;
		DELAY(50);
	}
	if (r != STAT_RUNNING) {
		printf(": fw not running\n");
		goto bail;
	}

	/* Clear TX and CMD ring write registers */
	WRITE_REG(sc, TXP_H2A_1, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_2, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_3, TXP_BOOTCMD_NULL);
	WRITE_REG(sc, TXP_H2A_0, TXP_BOOTCMD_NULL);

	return (0);

bail:
	txp_dma_free(sc, &sc->sc_zero_dma);
bail_rxbufring:
	if (nb == RXBUF_ENTRIES)
		nb--;
	for (i = 0; i <= nb; i++) {
		memcpy(&sd, __UNVOLATILE(&sc->sc_rxbufs[i].rb_vaddrlo),
		    sizeof(sd));
		if (sd)
			free(sd, M_DEVBUF);
	}
	txp_dma_free(sc, &sc->sc_rxbufring_dma);
bail_rspring:
	txp_dma_free(sc, &sc->sc_rspring_dma);
bail_cmdring:
	txp_dma_free(sc, &sc->sc_cmdring_dma);
bail_rxloring:
	txp_dma_free(sc, &sc->sc_rxloring_dma);
bail_rxhiring:
	txp_dma_free(sc, &sc->sc_rxhiring_dma);
bail_txloring:
	txp_dma_free(sc, &sc->sc_txloring_dma);
bail_txhiring:
	txp_dma_free(sc, &sc->sc_txhiring_dma);
bail_host:
	txp_dma_free(sc, &sc->sc_host_dma);
bail_boot:
	txp_dma_free(sc, &sc->sc_boot_dma);
	return (-1);
}

int
txp_dma_malloc(struct txp_softc *sc, bus_size_t size, struct txp_dma_alloc *dma, int mapflags)
{
	int r;

	if ((r = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0,
	    &dma->dma_seg, 1, &dma->dma_nseg, 0)) != 0)
		goto fail_0;

	if ((r = bus_dmamem_map(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg,
	    size, &dma->dma_vaddr, mapflags | BUS_DMA_NOWAIT)) != 0)
		goto fail_1;

	if ((r = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &dma->dma_map)) != 0)
		goto fail_2;

	if ((r = bus_dmamap_load(sc->sc_dmat, dma->dma_map, dma->dma_vaddr,
	    size, NULL, BUS_DMA_NOWAIT)) != 0)
		goto fail_3;

	dma->dma_paddr = dma->dma_map->dm_segs[0].ds_addr;
	return (0);

fail_3:
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, size);
fail_1:
	bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
fail_0:
	return (r);
}

void
txp_dma_free(struct txp_softc *sc, struct txp_dma_alloc *dma)
{
	bus_dmamap_unload(sc->sc_dmat, dma->dma_map);
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, dma->dma_map->dm_mapsize);
	bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
}

int
txp_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

#if 0
	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}
#endif

	switch(command) {
	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP;
		txp_init(sc);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(ifp, ifa);
			break;
#endif /* INET */
		default:
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, command, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			txp_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				txp_stop(sc);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, command, data)) != ENETRESET)
			break;

		error = 0;

		if (command != SIOCADDMULTI && command != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			txp_set_filter(sc);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_ifmedia, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	splx(s);

	return(error);
}

void
txp_init(struct txp_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	int s;

	txp_stop(sc);

	s = splnet();

	txp_set_filter(sc);

	txp_command(sc, TXP_CMD_TX_ENABLE, 0, 0, 0, NULL, NULL, NULL, 1);
	txp_command(sc, TXP_CMD_RX_ENABLE, 0, 0, 0, NULL, NULL, NULL, 1);

	WRITE_REG(sc, TXP_IER, TXP_INT_RESERVED | TXP_INT_SELF |
	    TXP_INT_A2H_7 | TXP_INT_A2H_6 | TXP_INT_A2H_5 | TXP_INT_A2H_4 |
	    TXP_INT_A2H_2 | TXP_INT_A2H_1 | TXP_INT_A2H_0 |
	    TXP_INT_DMA3 | TXP_INT_DMA2 | TXP_INT_DMA1 | TXP_INT_DMA0 |
	    TXP_INT_PCI_TABORT | TXP_INT_PCI_MABORT |  TXP_INT_LATCH);
	WRITE_REG(sc, TXP_IMR, TXP_INT_A2H_3);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;

	if (!callout_pending(&sc->sc_tick))
		callout_schedule(&sc->sc_tick, hz);

	splx(s);
}

void
txp_tick(void *vsc)
{
	struct txp_softc *sc = vsc;
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	struct txp_rsp_desc *rsp = NULL;
	struct txp_ext_desc *ext;
	int s;

	s = splnet();
	txp_rxbuf_reclaim(sc);

	if (txp_command2(sc, TXP_CMD_READ_STATISTICS, 0, 0, 0, NULL, 0,
	    &rsp, 1))
		goto out;
	if (rsp->rsp_numdesc != 6)
		goto out;
	if (txp_command(sc, TXP_CMD_CLEAR_STATISTICS, 0, 0, 0,
	    NULL, NULL, NULL, 1))
		goto out;
	ext = (struct txp_ext_desc *)(rsp + 1);

	ifp->if_ierrors += ext[3].ext_2 + ext[3].ext_3 + ext[3].ext_4 +
	    ext[4].ext_1 + ext[4].ext_4;
	ifp->if_oerrors += ext[0].ext_1 + ext[1].ext_1 + ext[1].ext_4 +
	    ext[2].ext_1;
	ifp->if_collisions += ext[0].ext_2 + ext[0].ext_3 + ext[1].ext_2 +
	    ext[1].ext_3;
	ifp->if_opackets += rsp->rsp_par2;
	ifp->if_ipackets += ext[2].ext_3;

out:
	if (rsp != NULL)
		free(rsp, M_DEVBUF);

	splx(s);
	callout_schedule(&sc->sc_tick, hz);
}

void
txp_start(struct ifnet *ifp)
{
	struct txp_softc *sc = ifp->if_softc;
	struct txp_tx_ring *r = &sc->sc_txhir;
	struct txp_tx_desc *txd;
	int txdidx;
	struct txp_frag_desc *fxd;
	struct mbuf *m, *mnew;
	struct txp_swdesc *sd;
	u_int32_t firstprod, firstcnt, prod, cnt, i;
	struct m_tag *mtag;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	prod = r->r_prod;
	cnt = r->r_cnt;

	while (1) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;
		mnew = NULL;

		firstprod = prod;
		firstcnt = cnt;

		sd = sc->sc_txd + prod;
		sd->sd_mbuf = m;

		if (bus_dmamap_load_mbuf(sc->sc_dmat, sd->sd_map, m,
		    BUS_DMA_NOWAIT)) {
			MGETHDR(mnew, M_DONTWAIT, MT_DATA);
			if (mnew == NULL)
				goto oactive1;
			if (m->m_pkthdr.len > MHLEN) {
				MCLGET(mnew, M_DONTWAIT);
				if ((mnew->m_flags & M_EXT) == 0) {
					m_freem(mnew);
					goto oactive1;
				}
			}
			m_copydata(m, 0, m->m_pkthdr.len, mtod(mnew, void *));
			mnew->m_pkthdr.len = mnew->m_len = m->m_pkthdr.len;
			IFQ_DEQUEUE(&ifp->if_snd, m);
			m_freem(m);
			m = mnew;
			if (bus_dmamap_load_mbuf(sc->sc_dmat, sd->sd_map, m,
			    BUS_DMA_NOWAIT))
				goto oactive1;
		}

		if ((TX_ENTRIES - cnt) < 4)
			goto oactive;

		txd = r->r_desc + prod;
		txdidx = prod;
		txd->tx_flags = TX_FLAGS_TYPE_DATA;
		txd->tx_numdesc = 0;
		txd->tx_addrlo = 0;
		txd->tx_addrhi = 0;
		txd->tx_totlen = m->m_pkthdr.len;
		txd->tx_pflags = 0;
		txd->tx_numdesc = sd->sd_map->dm_nsegs;

		if (++prod == TX_ENTRIES)
			prod = 0;

		if (++cnt >= (TX_ENTRIES - 4))
			goto oactive;

		if ((mtag = VLAN_OUTPUT_TAG(&sc->sc_arpcom, m)))
			txd->tx_pflags = TX_PFLAGS_VLAN |
			  (htons(VLAN_TAG_VALUE(mtag)) << TX_PFLAGS_VLANTAG_S);

		if (m->m_pkthdr.csum_flags & M_CSUM_IPv4)
			txd->tx_pflags |= TX_PFLAGS_IPCKSUM;
#ifdef TRY_TX_TCP_CSUM
		if (m->m_pkthdr.csum_flags & M_CSUM_TCPv4)
			txd->tx_pflags |= TX_PFLAGS_TCPCKSUM;
#endif
#ifdef TRY_TX_UDP_CSUM
		if (m->m_pkthdr.csum_flags & M_CSUM_UDPv4)
			txd->tx_pflags |= TX_PFLAGS_UDPCKSUM;
#endif

		bus_dmamap_sync(sc->sc_dmat, sd->sd_map, 0,
		    sd->sd_map->dm_mapsize, BUS_DMASYNC_PREWRITE);

		fxd = (struct txp_frag_desc *)(r->r_desc + prod);
		for (i = 0; i < sd->sd_map->dm_nsegs; i++) {
			if (++cnt >= (TX_ENTRIES - 4)) {
				bus_dmamap_sync(sc->sc_dmat, sd->sd_map,
				    0, sd->sd_map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				goto oactive;
			}

			fxd->frag_flags = FRAG_FLAGS_TYPE_FRAG |
			    FRAG_FLAGS_VALID;
			fxd->frag_rsvd1 = 0;
			fxd->frag_len = sd->sd_map->dm_segs[i].ds_len;
			fxd->frag_addrlo =
			    ((u_int64_t)sd->sd_map->dm_segs[i].ds_addr) &
			    0xffffffff;
			fxd->frag_addrhi =
			    ((u_int64_t)sd->sd_map->dm_segs[i].ds_addr) >>
			    32;
			fxd->frag_rsvd2 = 0;

			bus_dmamap_sync(sc->sc_dmat,
			    sc->sc_txhiring_dma.dma_map,
			    prod * sizeof(struct txp_frag_desc),
			    sizeof(struct txp_frag_desc), BUS_DMASYNC_PREWRITE);

			if (++prod == TX_ENTRIES) {
				fxd = (struct txp_frag_desc *)r->r_desc;
				prod = 0;
			} else
				fxd++;

		}

		/*
		 * if mnew isn't NULL, we already dequeued and copied
		 * the packet.
		 */
		if (mnew == NULL)
			IFQ_DEQUEUE(&ifp->if_snd, m);

		ifp->if_timer = 5;

		bpf_mtap(ifp, m);

		txd->tx_flags |= TX_FLAGS_VALID;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_txhiring_dma.dma_map,
		    txdidx * sizeof(struct txp_tx_desc),
		    sizeof(struct txp_tx_desc), BUS_DMASYNC_PREWRITE);

#if 0
		{
			struct mbuf *mx;
			int i;

			printf("txd: flags 0x%x ndesc %d totlen %d pflags 0x%x\n",
			    txd->tx_flags, txd->tx_numdesc, txd->tx_totlen,
			    txd->tx_pflags);
			for (mx = m; mx != NULL; mx = mx->m_next) {
				for (i = 0; i < mx->m_len; i++) {
					printf(":%02x",
					    (u_int8_t)m->m_data[i]);
				}
			}
			printf("\n");
		}
#endif

		WRITE_REG(sc, r->r_reg, TXP_IDX2OFFSET(prod));
	}

	r->r_prod = prod;
	r->r_cnt = cnt;
	return;

oactive:
	bus_dmamap_unload(sc->sc_dmat, sd->sd_map);
oactive1:
	ifp->if_flags |= IFF_OACTIVE;
	r->r_prod = firstprod;
	r->r_cnt = firstcnt;
}

/*
 * Handle simple commands sent to the typhoon
 */
int
txp_command(struct txp_softc *sc, u_int16_t id, u_int16_t in1, u_int32_t in2, u_int32_t in3, u_int16_t *out1, u_int32_t *out2, u_int32_t *out3, int wait)
{
	struct txp_rsp_desc *rsp = NULL;

	if (txp_command2(sc, id, in1, in2, in3, NULL, 0, &rsp, wait))
		return (-1);

	if (!wait)
		return (0);

	if (out1 != NULL)
		*out1 = le16toh(rsp->rsp_par1);
	if (out2 != NULL)
		*out2 = le32toh(rsp->rsp_par2);
	if (out3 != NULL)
		*out3 = le32toh(rsp->rsp_par3);
	free(rsp, M_DEVBUF);
	return (0);
}

int
txp_command2(struct txp_softc *sc, u_int16_t id, u_int16_t in1, u_int32_t in2, u_int32_t in3, struct txp_ext_desc *in_extp, u_int8_t in_extn, struct txp_rsp_desc **rspp, int wait)
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_cmd_desc *cmd;
	struct txp_ext_desc *ext;
	u_int32_t idx, i;
	u_int16_t seq;

	if (txp_cmd_desc_numfree(sc) < (in_extn + 1)) {
		printf("%s: no free cmd descriptors\n", TXP_DEVNAME(sc));
		return (-1);
	}

	idx = sc->sc_cmdring.lastwrite;
	cmd = (struct txp_cmd_desc *)(((u_int8_t *)sc->sc_cmdring.base) + idx);
	memset(cmd, 0, sizeof(*cmd));

	cmd->cmd_numdesc = in_extn;
	seq = sc->sc_seq++;
	cmd->cmd_seq = htole16(seq);
	cmd->cmd_id = htole16(id);
	cmd->cmd_par1 = htole16(in1);
	cmd->cmd_par2 = htole32(in2);
	cmd->cmd_par3 = htole32(in3);
	cmd->cmd_flags = CMD_FLAGS_TYPE_CMD |
	    (wait ? CMD_FLAGS_RESP : 0) | CMD_FLAGS_VALID;

	idx += sizeof(struct txp_cmd_desc);
	if (idx == sc->sc_cmdring.size)
		idx = 0;

	for (i = 0; i < in_extn; i++) {
		ext = (struct txp_ext_desc *)(((u_int8_t *)sc->sc_cmdring.base) + idx);
		memcpy(ext, in_extp, sizeof(struct txp_ext_desc));
		in_extp++;
		idx += sizeof(struct txp_cmd_desc);
		if (idx == sc->sc_cmdring.size)
			idx = 0;
	}

	sc->sc_cmdring.lastwrite = idx;

	WRITE_REG(sc, TXP_H2A_2, sc->sc_cmdring.lastwrite);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_host_dma.dma_map, 0,
	    sizeof(struct txp_hostvar), BUS_DMASYNC_PREREAD);

	if (!wait)
		return (0);

	for (i = 0; i < 10000; i++) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_host_dma.dma_map, 0,
		    sizeof(struct txp_hostvar), BUS_DMASYNC_POSTREAD);
		idx = le32toh(hv->hv_resp_read_idx);
		if (idx != le32toh(hv->hv_resp_write_idx)) {
			*rspp = NULL;
			if (txp_response(sc, idx, id, seq, rspp))
				return (-1);
			if (*rspp != NULL)
				break;
		}
		bus_dmamap_sync(sc->sc_dmat, sc->sc_host_dma.dma_map, 0,
		    sizeof(struct txp_hostvar), BUS_DMASYNC_PREREAD);
		DELAY(50);
	}
	if (i == 1000 || (*rspp) == NULL) {
		printf("%s: 0x%x command failed\n", TXP_DEVNAME(sc), id);
		return (-1);
	}

	return (0);
}

int
txp_response(struct txp_softc *sc, u_int32_t ridx, u_int16_t id, u_int16_t seq, struct txp_rsp_desc **rspp)
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_rsp_desc *rsp;

	while (ridx != le32toh(hv->hv_resp_write_idx)) {
		rsp = (struct txp_rsp_desc *)(((u_int8_t *)sc->sc_rspring.base) + ridx);

		if (id == le16toh(rsp->rsp_id) && le16toh(rsp->rsp_seq) == seq) {
			*rspp = (struct txp_rsp_desc *)malloc(
			    sizeof(struct txp_rsp_desc) * (rsp->rsp_numdesc + 1),
			    M_DEVBUF, M_NOWAIT);
			if ((*rspp) == NULL)
				return (-1);
			txp_rsp_fixup(sc, rsp, *rspp);
			return (0);
		}

		if (rsp->rsp_flags & RSP_FLAGS_ERROR) {
			printf("%s: response error: id 0x%x\n",
			    TXP_DEVNAME(sc), le16toh(rsp->rsp_id));
			txp_rsp_fixup(sc, rsp, NULL);
			ridx = le32toh(hv->hv_resp_read_idx);
			continue;
		}

		switch (le16toh(rsp->rsp_id)) {
		case TXP_CMD_CYCLE_STATISTICS:
		case TXP_CMD_MEDIA_STATUS_READ:
			break;
		case TXP_CMD_HELLO_RESPONSE:
			printf("%s: hello\n", TXP_DEVNAME(sc));
			break;
		default:
			printf("%s: unknown id(0x%x)\n", TXP_DEVNAME(sc),
			    le16toh(rsp->rsp_id));
		}

		txp_rsp_fixup(sc, rsp, NULL);
		ridx = le32toh(hv->hv_resp_read_idx);
		hv->hv_resp_read_idx = le32toh(ridx);
	}

	return (0);
}

void
txp_rsp_fixup(struct txp_softc *sc, struct txp_rsp_desc *rsp, struct txp_rsp_desc *dst)
{
	struct txp_rsp_desc *src = rsp;
	struct txp_hostvar *hv = sc->sc_hostvar;
	u_int32_t i, ridx;

	ridx = le32toh(hv->hv_resp_read_idx);

	for (i = 0; i < rsp->rsp_numdesc + 1; i++) {
		if (dst != NULL)
			memcpy(dst++, src, sizeof(struct txp_rsp_desc));
		ridx += sizeof(struct txp_rsp_desc);
		if (ridx == sc->sc_rspring.size) {
			src = sc->sc_rspring.base;
			ridx = 0;
		} else
			src++;
		sc->sc_rspring.lastwrite = ridx;
		hv->hv_resp_read_idx = htole32(ridx);
	}

	hv->hv_resp_read_idx = htole32(ridx);
}

int
txp_cmd_desc_numfree(struct txp_softc *sc)
{
	struct txp_hostvar *hv = sc->sc_hostvar;
	struct txp_boot_record *br = sc->sc_boot;
	u_int32_t widx, ridx, nfree;

	widx = sc->sc_cmdring.lastwrite;
	ridx = le32toh(hv->hv_cmd_read_idx);

	if (widx == ridx) {
		/* Ring is completely free */
		nfree = le32toh(br->br_cmd_siz) - sizeof(struct txp_cmd_desc);
	} else {
		if (widx > ridx)
			nfree = le32toh(br->br_cmd_siz) -
			    (widx - ridx + sizeof(struct txp_cmd_desc));
		else
			nfree = ridx - widx - sizeof(struct txp_cmd_desc);
	}

	return (nfree / sizeof(struct txp_cmd_desc));
}

void
txp_stop(struct txp_softc *sc)
{
	txp_command(sc, TXP_CMD_TX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 1);
	txp_command(sc, TXP_CMD_RX_DISABLE, 0, 0, 0, NULL, NULL, NULL, 1);

	if (callout_pending(&sc->sc_tick))
		callout_stop(&sc->sc_tick);
}

void
txp_watchdog(struct ifnet *ifp)
{
}

int
txp_ifmedia_upd(struct ifnet *ifp)
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;
	u_int16_t new_xcvr;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_10_T) {
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			new_xcvr = TXP_XCVR_10_FDX;
		else
			new_xcvr = TXP_XCVR_10_HDX;
	} else if ((IFM_SUBTYPE(ifm->ifm_media) == IFM_100_TX) ||
		   (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_FX)) {
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			new_xcvr = TXP_XCVR_100_FDX;
		else
			new_xcvr = TXP_XCVR_100_HDX;
	} else if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO) {
		new_xcvr = TXP_XCVR_AUTO;
	} else
		return (EINVAL);

	/* nothing to do */
	if (sc->sc_xcvr == new_xcvr)
		return (0);

	txp_command(sc, TXP_CMD_XCVR_SELECT, new_xcvr, 0, 0,
	    NULL, NULL, NULL, 0);
	sc->sc_xcvr = new_xcvr;

	return (0);
}

void
txp_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct txp_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_ifmedia;
	u_int16_t bmsr, bmcr, anlpar;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMSR, 0,
	    &bmsr, NULL, NULL, 1))
		goto bail;
	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMSR, 0,
	    &bmsr, NULL, NULL, 1))
		goto bail;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_BMCR, 0,
	    &bmcr, NULL, NULL, 1))
		goto bail;

	if (txp_command(sc, TXP_CMD_PHY_MGMT_READ, 0, MII_ANLPAR, 0,
	    &anlpar, NULL, NULL, 1))
		goto bail;

	if (bmsr & BMSR_LINK)
		ifmr->ifm_status |= IFM_ACTIVE;

	if (bmcr & BMCR_ISO) {
		ifmr->ifm_active |= IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		ifmr->ifm_active |= IFM_LOOP;

	if (!(sc->sc_flags & TXP_FIBER) && (bmcr & BMCR_AUTOEN)) {
		if ((bmsr & BMSR_ACOMP) == 0) {
			ifmr->ifm_active |= IFM_NONE;
			return;
		}

		if (anlpar & ANLPAR_TX_FD)
			ifmr->ifm_active |= IFM_100_TX|IFM_FDX;
		else if (anlpar & ANLPAR_T4)
			ifmr->ifm_active |= IFM_100_T4|IFM_HDX;
		else if (anlpar & ANLPAR_TX)
			ifmr->ifm_active |= IFM_100_TX|IFM_HDX;
		else if (anlpar & ANLPAR_10_FD)
			ifmr->ifm_active |= IFM_10_T|IFM_FDX;
		else if (anlpar & ANLPAR_10)
			ifmr->ifm_active |= IFM_10_T|IFM_HDX;
		else
			ifmr->ifm_active |= IFM_NONE;
	} else
		ifmr->ifm_active = ifm->ifm_cur->ifm_media;
	return;

bail:
	ifmr->ifm_active |= IFM_NONE;
	ifmr->ifm_status &= ~IFM_AVALID;
}

void
txp_show_descriptor(void *d)
{
	struct txp_cmd_desc *cmd = d;
	struct txp_rsp_desc *rsp = d;
	struct txp_tx_desc *txd = d;
	struct txp_frag_desc *frgd = d;

	switch (cmd->cmd_flags & CMD_FLAGS_TYPE_M) {
	case CMD_FLAGS_TYPE_CMD:
		/* command descriptor */
		printf("[cmd flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    cmd->cmd_flags, cmd->cmd_numdesc, le16toh(cmd->cmd_id),
		    le16toh(cmd->cmd_seq), le16toh(cmd->cmd_par1),
		    le32toh(cmd->cmd_par2), le32toh(cmd->cmd_par3));
		break;
	case CMD_FLAGS_TYPE_RESP:
		/* response descriptor */
		printf("[rsp flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    rsp->rsp_flags, rsp->rsp_numdesc, le16toh(rsp->rsp_id),
		    le16toh(rsp->rsp_seq), le16toh(rsp->rsp_par1),
		    le32toh(rsp->rsp_par2), le32toh(rsp->rsp_par3));
		break;
	case CMD_FLAGS_TYPE_DATA:
		/* data header (assuming tx for now) */
		printf("[data flags 0x%x num %d totlen %d addr 0x%x/0x%x pflags 0x%x]",
		    txd->tx_flags, txd->tx_numdesc, txd->tx_totlen,
		    txd->tx_addrlo, txd->tx_addrhi, txd->tx_pflags);
		break;
	case CMD_FLAGS_TYPE_FRAG:
		/* fragment descriptor */
		printf("[frag flags 0x%x rsvd1 0x%x len %d addr 0x%x/0x%x rsvd2 0x%x]",
		    frgd->frag_flags, frgd->frag_rsvd1, frgd->frag_len,
		    frgd->frag_addrlo, frgd->frag_addrhi, frgd->frag_rsvd2);
		break;
	default:
		printf("[unknown(%x) flags 0x%x num %d id %d seq %d par1 0x%x par2 0x%x par3 0x%x]\n",
		    cmd->cmd_flags & CMD_FLAGS_TYPE_M,
		    cmd->cmd_flags, cmd->cmd_numdesc, le16toh(cmd->cmd_id),
		    le16toh(cmd->cmd_seq), le16toh(cmd->cmd_par1),
		    le32toh(cmd->cmd_par2), le32toh(cmd->cmd_par3));
		break;
	}
}

void
txp_set_filter(struct txp_softc *sc)
{
	struct ethercom *ac = &sc->sc_arpcom;
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	u_int32_t crc, carry, hashbit, hash[2];
	u_int16_t filter;
	u_int8_t octet;
	int i, j, mcnt = 0;
	struct ether_multi *enm;
	struct ether_multistep step;

	if (ifp->if_flags & IFF_PROMISC) {
		filter = TXP_RXFILT_PROMISC;
		goto setit;
	}

again:
	filter = TXP_RXFILT_DIRECT;

	if (ifp->if_flags & IFF_BROADCAST)
		filter |= TXP_RXFILT_BROADCAST;

	if (ifp->if_flags & IFF_ALLMULTI)
		filter |= TXP_RXFILT_ALLMULTI;
	else {
		hash[0] = hash[1] = 0;

		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
				/*
				 * We must listen to a range of multicast
				 * addresses.  For now, just accept all
				 * multicasts, rather than trying to set only
				 * those filter bits needed to match the range.
				 * (At this time, the only use of address
				 * ranges is for IP multicast routing, for
				 * which the range is big enough to require
				 * all bits set.)
				 */
				ifp->if_flags |= IFF_ALLMULTI;
				goto again;
			}

			mcnt++;
			crc = 0xffffffff;

			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				octet = enm->enm_addrlo[i];
				for (j = 0; j < 8; j++) {
					carry = ((crc & 0x80000000) ? 1 : 0) ^
					    (octet & 1);
					crc <<= 1;
					octet >>= 1;
					if (carry)
						crc = (crc ^ TXP_POLYNOMIAL) |
						    carry;
				}
			}
			hashbit = (u_int16_t)(crc & (64 - 1));
			hash[hashbit / 32] |= (1 << hashbit % 32);
			ETHER_NEXT_MULTI(step, enm);
		}

		if (mcnt > 0) {
			filter |= TXP_RXFILT_HASHMULTI;
			txp_command(sc, TXP_CMD_MCAST_HASH_MASK_WRITE,
			    2, hash[0], hash[1], NULL, NULL, NULL, 0);
		}
	}

setit:
	txp_command(sc, TXP_CMD_RX_FILTER_WRITE, filter, 0, 0,
	    NULL, NULL, NULL, 1);
}

void
txp_capabilities(struct txp_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ec_if;
	struct txp_rsp_desc *rsp = NULL;
	struct txp_ext_desc *ext;

	if (txp_command2(sc, TXP_CMD_OFFLOAD_READ, 0, 0, 0, NULL, 0, &rsp, 1))
		goto out;

	if (rsp->rsp_numdesc != 1)
		goto out;
	ext = (struct txp_ext_desc *)(rsp + 1);

	sc->sc_tx_capability = ext->ext_1 & OFFLOAD_MASK;
	sc->sc_rx_capability = ext->ext_2 & OFFLOAD_MASK;

	sc->sc_arpcom.ec_capabilities |= ETHERCAP_VLAN_MTU;
	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_VLAN) {
		sc->sc_tx_capability |= OFFLOAD_VLAN;
		sc->sc_rx_capability |= OFFLOAD_VLAN;
		sc->sc_arpcom.ec_capabilities |= ETHERCAP_VLAN_HWTAGGING;
	}

#if 0
	/* not ready yet */
	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_IPSEC) {
		sc->sc_tx_capability |= OFFLOAD_IPSEC;
		sc->sc_rx_capability |= OFFLOAD_IPSEC;
		ifp->if_capabilities |= IFCAP_IPSEC;
	}
#endif

	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_IPCKSUM) {
		sc->sc_tx_capability |= OFFLOAD_IPCKSUM;
		sc->sc_rx_capability |= OFFLOAD_IPCKSUM;
		ifp->if_capabilities |= IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx;
	}

	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_TCPCKSUM) {
		sc->sc_rx_capability |= OFFLOAD_TCPCKSUM;
#ifdef TRY_TX_TCP_CSUM
		sc->sc_tx_capability |= OFFLOAD_TCPCKSUM;
		ifp->if_capabilities |=
		    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx;
#endif
	}

	if (rsp->rsp_par2 & rsp->rsp_par3 & OFFLOAD_UDPCKSUM) {
		sc->sc_rx_capability |= OFFLOAD_UDPCKSUM;
#ifdef TRY_TX_UDP_CSUM
		sc->sc_tx_capability |= OFFLOAD_UDPCKSUM;
		ifp->if_capabilities |=
		    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;
#endif
	}

	if (txp_command(sc, TXP_CMD_OFFLOAD_WRITE, 0,
	    sc->sc_tx_capability, sc->sc_rx_capability, NULL, NULL, NULL, 1))
		goto out;

out:
	if (rsp != NULL)
		free(rsp, M_DEVBUF);
}
