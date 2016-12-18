/*	$NetBSD: if_bnx.c,v 1.57 2014/07/09 16:30:11 msaitoh Exp $	*/
/*	$OpenBSD: if_bnx.c,v 1.85 2009/11/09 14:32:41 dlg Exp $ */

/*-
 * Copyright (c) 2006-2010 Broadcom Corporation
 *	David Christensen <davidch@broadcom.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if 0
__FBSDID("$FreeBSD: src/sys/dev/bce/if_bce.c,v 1.3 2006/04/13 14:12:26 ru Exp $");
#endif
__KERNEL_RCSID(0, "$NetBSD: if_bnx.c,v 1.57 2014/07/09 16:30:11 msaitoh Exp $");

/*
 * The following controllers are supported by this driver:
 *   BCM5706C A2, A3
 *   BCM5706S A2, A3
 *   BCM5708C B1, B2
 *   BCM5708S B1, B2
 *   BCM5709C A1, C0
 *   BCM5709S A1, C0
 *   BCM5716  C0
 *
 * The following controllers are not supported by this driver:
 *   BCM5706C A0, A1
 *   BCM5706S A0, A1
 *   BCM5708C A0, B0
 *   BCM5708S A0, B0
 *   BCM5709C A0  B0, B1, B2 (pre-production)
 *   BCM5709S A0, B0, B1, B2 (pre-production)
 */

#include <sys/callout.h>
#include <sys/mutex.h>

#include <dev/pci/if_bnxreg.h>
#include <dev/pci/if_bnxvar.h>

#include <dev/microcode/bnx/bnxfw.h>

/****************************************************************************/
/* BNX Driver Version                                                       */
/****************************************************************************/
#define BNX_DRIVER_VERSION	"v0.9.6"

/****************************************************************************/
/* BNX Debug Options                                                        */
/****************************************************************************/
#ifdef BNX_DEBUG
	uint32_t bnx_debug = /*BNX_WARN*/ BNX_VERBOSE_SEND;

	/*          0 = Never              */
	/*          1 = 1 in 2,147,483,648 */
	/*        256 = 1 in     8,388,608 */
	/*       2048 = 1 in     1,048,576 */
	/*      65536 = 1 in        32,768 */
	/*    1048576 = 1 in         2,048 */
	/*  268435456 =	1 in             8 */
	/*  536870912 = 1 in             4 */
	/* 1073741824 = 1 in             2 */

	/* Controls how often the l2_fhdr frame error check will fail. */
	int bnx_debug_l2fhdr_status_check = 0;

	/* Controls how often the unexpected attention check will fail. */
	int bnx_debug_unexpected_attention = 0;

	/* Controls how often to simulate an mbuf allocation failure. */
	int bnx_debug_mbuf_allocation_failure = 0;

	/* Controls how often to simulate a DMA mapping failure. */
	int bnx_debug_dma_map_addr_failure = 0;

	/* Controls how often to simulate a bootcode failure. */
	int bnx_debug_bootcode_running_failure = 0;
#endif

/****************************************************************************/
/* PCI Device ID Table                                                      */
/*                                                                          */
/* Used by bnx_probe() to identify the devices supported by this driver.    */
/****************************************************************************/
static const struct bnx_product {
	pci_vendor_id_t		bp_vendor;
	pci_product_id_t	bp_product;
	pci_vendor_id_t		bp_subvendor;
	pci_product_id_t	bp_subproduct;
	const char		*bp_name;
} bnx_devices[] = {
#ifdef PCI_SUBPRODUCT_HP_NC370T
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5706,
	  PCI_VENDOR_HP, PCI_SUBPRODUCT_HP_NC370T,
	  "HP NC370T Multifunction Gigabit Server Adapter"
	},
#endif
#ifdef PCI_SUBPRODUCT_HP_NC370i
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5706,
	  PCI_VENDOR_HP, PCI_SUBPRODUCT_HP_NC370i,
	  "HP NC370i Multifunction Gigabit Server Adapter"
	},
#endif
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5706,
	  0, 0,
	  "Broadcom NetXtreme II BCM5706 1000Base-T"
	},
#ifdef PCI_SUBPRODUCT_HP_NC370F
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5706S,
	  PCI_VENDOR_HP, PCI_SUBPRODUCT_HP_NC370F,
	  "HP NC370F Multifunction Gigabit Server Adapter"
	},
#endif
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5706S,
	  0, 0,
	  "Broadcom NetXtreme II BCM5706 1000Base-SX"
	},
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5708,
	  0, 0,
	  "Broadcom NetXtreme II BCM5708 1000Base-T"
	},
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5708S,
	  0, 0,
	  "Broadcom NetXtreme II BCM5708 1000Base-SX"
	},
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5709,
	  0, 0,
	  "Broadcom NetXtreme II BCM5709 1000Base-T"
	},
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5709S,
	  0, 0,
	  "Broadcom NetXtreme II BCM5709 1000Base-SX"
	},
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5716,
	  0, 0,
	  "Broadcom NetXtreme II BCM5716 1000Base-T"
	},
	{
	  PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM5716S,
	  0, 0,
	  "Broadcom NetXtreme II BCM5716 1000Base-SX"
	},
};

/****************************************************************************/
/* Supported Flash NVRAM device data.                                       */
/****************************************************************************/
static struct flash_spec flash_table[] =
{
#define BUFFERED_FLAGS		(BNX_NV_BUFFERED | BNX_NV_TRANSLATE)
#define NONBUFFERED_FLAGS	(BNX_NV_WREN)
	/* Slow EEPROM */
	{0x00000000, 0x40830380, 0x009f0081, 0xa184a053, 0xaf000400,
	 BUFFERED_FLAGS, SEEPROM_PAGE_BITS, SEEPROM_PAGE_SIZE,
	 SEEPROM_BYTE_ADDR_MASK, SEEPROM_TOTAL_SIZE,
	 "EEPROM - slow"},
	/* Expansion entry 0001 */
	{0x08000002, 0x4b808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 0001"},
	/* Saifun SA25F010 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x04000001, 0x47808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE*2,
	 "Non-buffered flash (128kB)"},
	/* Saifun SA25F020 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x0c000003, 0x4f808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE*4,
	 "Non-buffered flash (256kB)"},
	/* Expansion entry 0100 */
	{0x11000000, 0x53808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 0100"},
	/* Entry 0101: ST M45PE10 (non-buffered flash, TetonII B0) */
	{0x19000002, 0x5b808201, 0x000500db, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, ST_MICRO_FLASH_PAGE_BITS, ST_MICRO_FLASH_PAGE_SIZE,
	 ST_MICRO_FLASH_BYTE_ADDR_MASK, ST_MICRO_FLASH_BASE_TOTAL_SIZE*2,
	 "Entry 0101: ST M45PE10 (128kB non-bufferred)"},
	/* Entry 0110: ST M45PE20 (non-buffered flash)*/
	{0x15000001, 0x57808201, 0x000500db, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, ST_MICRO_FLASH_PAGE_BITS, ST_MICRO_FLASH_PAGE_SIZE,
	 ST_MICRO_FLASH_BYTE_ADDR_MASK, ST_MICRO_FLASH_BASE_TOTAL_SIZE*4,
	 "Entry 0110: ST M45PE20 (256kB non-bufferred)"},
	/* Saifun SA25F005 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x1d000003, 0x5f808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE,
	 "Non-buffered flash (64kB)"},
	/* Fast EEPROM */
	{0x22000000, 0x62808380, 0x009f0081, 0xa184a053, 0xaf000400,
	 BUFFERED_FLAGS, SEEPROM_PAGE_BITS, SEEPROM_PAGE_SIZE,
	 SEEPROM_BYTE_ADDR_MASK, SEEPROM_TOTAL_SIZE,
	 "EEPROM - fast"},
	/* Expansion entry 1001 */
	{0x2a000002, 0x6b808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1001"},
	/* Expansion entry 1010 */
	{0x26000001, 0x67808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1010"},
	/* ATMEL AT45DB011B (buffered flash) */
	{0x2e000003, 0x6e808273, 0x00570081, 0x68848353, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, BUFFERED_FLASH_TOTAL_SIZE,
	 "Buffered flash (128kB)"},
	/* Expansion entry 1100 */
	{0x33000000, 0x73808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1100"},
	/* Expansion entry 1101 */
	{0x3b000002, 0x7b808201, 0x00050081, 0x03840253, 0xaf020406,
	 NONBUFFERED_FLAGS, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1101"},
	/* Ateml Expansion entry 1110 */
	{0x37000001, 0x76808273, 0x00570081, 0x68848353, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1110 (Atmel)"},
	/* ATMEL AT45DB021B (buffered flash) */
	{0x3f000003, 0x7e808273, 0x00570081, 0x68848353, 0xaf000400,
	 BUFFERED_FLAGS, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, BUFFERED_FLASH_TOTAL_SIZE*2,
	 "Buffered flash (256kB)"},
};

/*
 * The BCM5709 controllers transparently handle the
 * differences between Atmel 264 byte pages and all
 * flash devices which use 256 byte pages, so no
 * logical-to-physical mapping is required in the
 * driver.
 */
static struct flash_spec flash_5709 = {
	.flags		= BNX_NV_BUFFERED,
	.page_bits	= BCM5709_FLASH_PAGE_BITS,
	.page_size	= BCM5709_FLASH_PAGE_SIZE,
	.addr_mask	= BCM5709_FLASH_BYTE_ADDR_MASK,
	.total_size	= BUFFERED_FLASH_TOTAL_SIZE * 2,
	.name		= "5709 buffered flash (256kB)",
};

/****************************************************************************/
/* OpenBSD device entry points.                                             */
/****************************************************************************/
static int	bnx_probe(device_t, cfdata_t, void *);
void	bnx_attach(device_t, device_t, void *);
int	bnx_detach(device_t, int);

/****************************************************************************/
/* BNX Debug Data Structure Dump Routines                                   */
/****************************************************************************/
#ifdef BNX_DEBUG
void	bnx_dump_mbuf(struct bnx_softc *, struct mbuf *);
void	bnx_dump_tx_mbuf_chain(struct bnx_softc *, int, int);
void	bnx_dump_rx_mbuf_chain(struct bnx_softc *, int, int);
void	bnx_dump_txbd(struct bnx_softc *, int, struct tx_bd *);
void	bnx_dump_rxbd(struct bnx_softc *, int, struct rx_bd *);
void	bnx_dump_l2fhdr(struct bnx_softc *, int, struct l2_fhdr *);
void	bnx_dump_tx_chain(struct bnx_softc *, int, int);
void	bnx_dump_rx_chain(struct bnx_softc *, int, int);
void	bnx_dump_status_block(struct bnx_softc *);
void	bnx_dump_stats_block(struct bnx_softc *);
void	bnx_dump_driver_state(struct bnx_softc *);
void	bnx_dump_hw_state(struct bnx_softc *);
void	bnx_breakpoint(struct bnx_softc *);
#endif

/****************************************************************************/
/* BNX Register/Memory Access Routines                                      */
/****************************************************************************/
uint32_t	bnx_reg_rd_ind(struct bnx_softc *, uint32_t);
void	bnx_reg_wr_ind(struct bnx_softc *, uint32_t, uint32_t);
void	bnx_ctx_wr(struct bnx_softc *, uint32_t, uint32_t, uint32_t);
int	bnx_miibus_read_reg(device_t, int, int);
void	bnx_miibus_write_reg(device_t, int, int, int);
void	bnx_miibus_statchg(struct ifnet *);

/****************************************************************************/
/* BNX NVRAM Access Routines                                                */
/****************************************************************************/
int	bnx_acquire_nvram_lock(struct bnx_softc *);
int	bnx_release_nvram_lock(struct bnx_softc *);
void	bnx_enable_nvram_access(struct bnx_softc *);
void	bnx_disable_nvram_access(struct bnx_softc *);
int	bnx_nvram_read_dword(struct bnx_softc *, uint32_t, uint8_t *,
	    uint32_t);
int	bnx_init_nvram(struct bnx_softc *);
int	bnx_nvram_read(struct bnx_softc *, uint32_t, uint8_t *, int);
int	bnx_nvram_test(struct bnx_softc *);
#ifdef BNX_NVRAM_WRITE_SUPPORT
int	bnx_enable_nvram_write(struct bnx_softc *);
void	bnx_disable_nvram_write(struct bnx_softc *);
int	bnx_nvram_erase_page(struct bnx_softc *, uint32_t);
int	bnx_nvram_write_dword(struct bnx_softc *, uint32_t, uint8_t *,
	    uint32_t);
int	bnx_nvram_write(struct bnx_softc *, uint32_t, uint8_t *, int);
#endif

/****************************************************************************/
/*                                                                          */
/****************************************************************************/
void	bnx_get_media(struct bnx_softc *);
void	bnx_init_media(struct bnx_softc *);
int	bnx_dma_alloc(struct bnx_softc *);
void	bnx_dma_free(struct bnx_softc *);
void	bnx_release_resources(struct bnx_softc *);

/****************************************************************************/
/* BNX Firmware Synchronization and Load                                    */
/****************************************************************************/
int	bnx_fw_sync(struct bnx_softc *, uint32_t);
void	bnx_load_rv2p_fw(struct bnx_softc *, uint32_t *, uint32_t,
	    uint32_t);
void	bnx_load_cpu_fw(struct bnx_softc *, struct cpu_reg *,
	    struct fw_info *);
void	bnx_init_cpus(struct bnx_softc *);

static void bnx_print_adapter_info(struct bnx_softc *);
static void bnx_probe_pci_caps(struct bnx_softc *);
void	bnx_stop(struct ifnet *, int);
int	bnx_reset(struct bnx_softc *, uint32_t);
int	bnx_chipinit(struct bnx_softc *);
int	bnx_blockinit(struct bnx_softc *);
static int	bnx_add_buf(struct bnx_softc *, struct mbuf *, uint16_t *,
	    uint16_t *, uint32_t *);
int	bnx_get_buf(struct bnx_softc *, uint16_t *, uint16_t *, uint32_t *);

int	bnx_init_tx_chain(struct bnx_softc *);
void	bnx_init_tx_context(struct bnx_softc *);
int	bnx_init_rx_chain(struct bnx_softc *);
void	bnx_init_rx_context(struct bnx_softc *);
void	bnx_free_rx_chain(struct bnx_softc *);
void	bnx_free_tx_chain(struct bnx_softc *);

int	bnx_tx_encap(struct bnx_softc *, struct mbuf *);
void	bnx_start(struct ifnet *);
int	bnx_ioctl(struct ifnet *, u_long, void *);
void	bnx_watchdog(struct ifnet *);
int	bnx_init(struct ifnet *);

void	bnx_init_context(struct bnx_softc *);
void	bnx_get_mac_addr(struct bnx_softc *);
void	bnx_set_mac_addr(struct bnx_softc *);
void	bnx_phy_intr(struct bnx_softc *);
void	bnx_rx_intr(struct bnx_softc *);
void	bnx_tx_intr(struct bnx_softc *);
void	bnx_disable_intr(struct bnx_softc *);
void	bnx_enable_intr(struct bnx_softc *);

int	bnx_intr(void *);
void	bnx_iff(struct bnx_softc *);
void	bnx_stats_update(struct bnx_softc *);
void	bnx_tick(void *);

struct pool *bnx_tx_pool = NULL;
void	bnx_alloc_pkts(struct work *, void *);

/****************************************************************************/
/* OpenBSD device dispatch table.                                           */
/****************************************************************************/
CFATTACH_DECL3_NEW(bnx, sizeof(struct bnx_softc),
    bnx_probe, bnx_attach, bnx_detach, NULL, NULL, NULL, DVF_DETACH_SHUTDOWN);

/****************************************************************************/
/* Device probe function.                                                   */
/*                                                                          */
/* Compares the device to the driver's list of supported devices and        */
/* reports back to the OS whether this is the right driver for the device.  */
/*                                                                          */
/* Returns:                                                                 */
/*   BUS_PROBE_DEFAULT on success, positive value on failure.               */
/****************************************************************************/
static const struct bnx_product *
bnx_lookup(const struct pci_attach_args *pa)
{
	int i;
	pcireg_t subid;

	for (i = 0; i < __arraycount(bnx_devices); i++) {
		if (PCI_VENDOR(pa->pa_id) != bnx_devices[i].bp_vendor ||
		    PCI_PRODUCT(pa->pa_id) != bnx_devices[i].bp_product)
			continue;
		if (!bnx_devices[i].bp_subvendor)
			return &bnx_devices[i];
		subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
		if (PCI_VENDOR(subid) == bnx_devices[i].bp_subvendor &&
		    PCI_PRODUCT(subid) == bnx_devices[i].bp_subproduct)
			return &bnx_devices[i];
	}

	return NULL;
}
static int
bnx_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (bnx_lookup(pa) != NULL)
		return 1;

	return 0;
}

/****************************************************************************/
/* PCI Capabilities Probe Function.                                         */
/*                                                                          */
/* Walks the PCI capabiites list for the device to find what features are   */
/* supported.                                                               */
/*                                                                          */
/* Returns:                                                                 */
/*   None.                                                                  */
/****************************************************************************/
static void
bnx_print_adapter_info(struct bnx_softc *sc)
{

	aprint_normal_dev(sc->bnx_dev, "ASIC BCM%x %c%d %s(0x%08x)\n",
	    BNXNUM(sc), 'A' + BNXREV(sc), BNXMETAL(sc),
	    (BNX_CHIP_BOND_ID(sc) == BNX_CHIP_BOND_ID_SERDES_BIT)
	    ? "Serdes " : "", sc->bnx_chipid);
	    
	/* Bus info. */
	if (sc->bnx_flags & BNX_PCIE_FLAG) {
		aprint_normal_dev(sc->bnx_dev, "PCIe x%d ",
		    sc->link_width);
		switch (sc->link_speed) {
		case 1: aprint_normal("2.5Gbps\n"); break;
		case 2:	aprint_normal("5Gbps\n"); break;
		default: aprint_normal("Unknown link speed\n");
		}
	} else {
		aprint_normal_dev(sc->bnx_dev, "PCI%s %dbit %dMHz\n",
		    ((sc->bnx_flags & BNX_PCIX_FLAG) ? "-X" : ""),
		    (sc->bnx_flags & BNX_PCI_32BIT_FLAG) ? 32 : 64,
		    sc->bus_speed_mhz);
	}

	aprint_normal_dev(sc->bnx_dev,
	    "Coal (RX:%d,%d,%d,%d; TX:%d,%d,%d,%d)\n",
	    sc->bnx_rx_quick_cons_trip_int,
	    sc->bnx_rx_quick_cons_trip,
	    sc->bnx_rx_ticks_int,
	    sc->bnx_rx_ticks,
	    sc->bnx_tx_quick_cons_trip_int,
	    sc->bnx_tx_quick_cons_trip,
	    sc->bnx_tx_ticks_int,
	    sc->bnx_tx_ticks);
}


/****************************************************************************/
/* PCI Capabilities Probe Function.                                         */
/*                                                                          */
/* Walks the PCI capabiites list for the device to find what features are   */
/* supported.                                                               */
/*                                                                          */
/* Returns:                                                                 */
/*   None.                                                                  */
/****************************************************************************/
static void
bnx_probe_pci_caps(struct bnx_softc *sc)
{
	struct pci_attach_args *pa = &(sc->bnx_pa);
	pcireg_t reg;

	/* Check if PCI-X capability is enabled. */
	if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_PCIX, &reg,
		NULL) != 0) {
		sc->bnx_cap_flags |= BNX_PCIX_CAPABLE_FLAG;
	}

	/* Check if PCIe capability is enabled. */
	if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_PCIEXPRESS, &reg,
		NULL) != 0) {
		pcireg_t link_status = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    reg + PCIE_LCSR);
		DBPRINT(sc, BNX_INFO_LOAD, "PCIe link_status = "
		    "0x%08X\n",	link_status);
		sc->link_speed = (link_status & PCIE_LCSR_LINKSPEED) >> 16;
		sc->link_width = (link_status & PCIE_LCSR_NLW) >> 20;
		sc->bnx_cap_flags |= BNX_PCIE_CAPABLE_FLAG;
		sc->bnx_flags |= BNX_PCIE_FLAG;
	}

	/* Check if MSI capability is enabled. */
	if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_MSI, &reg,
		NULL) != 0)
		sc->bnx_cap_flags |= BNX_MSI_CAPABLE_FLAG;

	/* Check if MSI-X capability is enabled. */
	if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_MSIX, &reg,
		NULL) != 0)
		sc->bnx_cap_flags |= BNX_MSIX_CAPABLE_FLAG;
}


/****************************************************************************/
/* Device attach function.                                                  */
/*                                                                          */
/* Allocates device resources, performs secondary chip identification,      */
/* resets and initializes the hardware, and initializes driver instance     */
/* variables.                                                               */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
void
bnx_attach(device_t parent, device_t self, void *aux)
{
	const struct bnx_product *bp;
	struct bnx_softc	*sc = device_private(self);
	prop_dictionary_t	dict;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;
	const char 		*intrstr = NULL;
	uint32_t		command;
	struct ifnet		*ifp;
	uint32_t		val;
	int			mii_flags = MIIF_FORCEANEG;
	pcireg_t		memtype;
	char intrbuf[PCI_INTRSTR_LEN];

	if (bnx_tx_pool == NULL) {
		bnx_tx_pool = malloc(sizeof(*bnx_tx_pool), M_DEVBUF, M_NOWAIT);
		if (bnx_tx_pool != NULL) {
			pool_init(bnx_tx_pool, sizeof(struct bnx_pkt),
			    0, 0, 0, "bnxpkts", NULL, IPL_NET);
		} else {
			aprint_error(": can't alloc bnx_tx_pool\n");
			return;
		}
	}

	bp = bnx_lookup(pa);
	if (bp == NULL)
		panic("unknown device");

	sc->bnx_dev = self;

	aprint_naive("\n");
	aprint_normal(": %s\n", bp->bp_name);

	sc->bnx_pa = *pa;

	/*
	 * Map control/status registers.
	*/
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		aprint_error_dev(sc->bnx_dev,
		    "failed to enable memory mapping!\n");
		return;
	}

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BNX_PCI_BAR0);
	if (pci_mapreg_map(pa, BNX_PCI_BAR0, memtype, 0, &sc->bnx_btag,
	    &sc->bnx_bhandle, NULL, &sc->bnx_size)) {
		aprint_error_dev(sc->bnx_dev, "can't find mem space\n");
		return;
	}

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->bnx_dev, "couldn't map interrupt\n");
		goto bnx_attach_fail;
	}

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	/*
	 * Configure byte swap and enable indirect register access.
	 * Rely on CPU to do target byte swapping on big endian systems.
	 * Access to registers outside of PCI configurtion space are not
	 * valid until this is done.
	 */
	pci_conf_write(pa->pa_pc, pa->pa_tag, BNX_PCICFG_MISC_CONFIG,
	    BNX_PCICFG_MISC_CONFIG_REG_WINDOW_ENA |
	    BNX_PCICFG_MISC_CONFIG_TARGET_MB_WORD_SWAP);

	/* Save ASIC revsion info. */
	sc->bnx_chipid =  REG_RD(sc, BNX_MISC_ID);

	/*
	 * Find the base address for shared memory access.
	 * Newer versions of bootcode use a signature and offset
	 * while older versions use a fixed address.
	 */
	val = REG_RD_IND(sc, BNX_SHM_HDR_SIGNATURE);
	if ((val & BNX_SHM_HDR_SIGNATURE_SIG_MASK) == BNX_SHM_HDR_SIGNATURE_SIG)
		sc->bnx_shmem_base = REG_RD_IND(sc, BNX_SHM_HDR_ADDR_0 +
		    (sc->bnx_pa.pa_function << 2));
	else
		sc->bnx_shmem_base = HOST_VIEW_SHMEM_BASE;

	DBPRINT(sc, BNX_INFO, "bnx_shmem_base = 0x%08X\n", sc->bnx_shmem_base);

	/* Set initial device and PHY flags */
	sc->bnx_flags = 0;
	sc->bnx_phy_flags = 0;

	bnx_probe_pci_caps(sc);

	/* Get PCI bus information (speed and type). */
	val = REG_RD(sc, BNX_PCICFG_MISC_STATUS);
	if (val & BNX_PCICFG_MISC_STATUS_PCIX_DET) {
		uint32_t clkreg;

		sc->bnx_flags |= BNX_PCIX_FLAG;

		clkreg = REG_RD(sc, BNX_PCICFG_PCI_CLOCK_CONTROL_BITS);

		clkreg &= BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET;
		switch (clkreg) {
		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_133MHZ:
			sc->bus_speed_mhz = 133;
			break;

		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_95MHZ:
			sc->bus_speed_mhz = 100;
			break;

		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_66MHZ:
		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_80MHZ:
			sc->bus_speed_mhz = 66;
			break;

		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_48MHZ:
		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_55MHZ:
			sc->bus_speed_mhz = 50;
			break;

		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_LOW:
		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_32MHZ:
		case BNX_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_38MHZ:
			sc->bus_speed_mhz = 33;
			break;
		}
	} else if (val & BNX_PCICFG_MISC_STATUS_M66EN)
			sc->bus_speed_mhz = 66;
		else
			sc->bus_speed_mhz = 33;

	if (val & BNX_PCICFG_MISC_STATUS_32BIT_DET)
		sc->bnx_flags |= BNX_PCI_32BIT_FLAG;

	/* Reset the controller. */
	if (bnx_reset(sc, BNX_DRV_MSG_CODE_RESET))
		goto bnx_attach_fail;

	/* Initialize the controller. */
	if (bnx_chipinit(sc)) {
		aprint_error_dev(sc->bnx_dev,
		    "Controller initialization failed!\n");
		goto bnx_attach_fail;
	}

	/* Perform NVRAM test. */
	if (bnx_nvram_test(sc)) {
		aprint_error_dev(sc->bnx_dev, "NVRAM test failed!\n");
		goto bnx_attach_fail;
	}

	/* Fetch the permanent Ethernet MAC address. */
	bnx_get_mac_addr(sc);
	aprint_normal_dev(sc->bnx_dev, "Ethernet address %s\n",
	    ether_sprintf(sc->eaddr));

	/*
	 * Trip points control how many BDs
	 * should be ready before generating an
	 * interrupt while ticks control how long
	 * a BD can sit in the chain before
	 * generating an interrupt.  Set the default
	 * values for the RX and TX rings.
	 */

#ifdef BNX_DEBUG
	/* Force more frequent interrupts. */
	sc->bnx_tx_quick_cons_trip_int = 1;
	sc->bnx_tx_quick_cons_trip     = 1;
	sc->bnx_tx_ticks_int           = 0;
	sc->bnx_tx_ticks               = 0;

	sc->bnx_rx_quick_cons_trip_int = 1;
	sc->bnx_rx_quick_cons_trip     = 1;
	sc->bnx_rx_ticks_int           = 0;
	sc->bnx_rx_ticks               = 0;
#else
	sc->bnx_tx_quick_cons_trip_int = 20;
	sc->bnx_tx_quick_cons_trip     = 20;
	sc->bnx_tx_ticks_int           = 80;
	sc->bnx_tx_ticks               = 80;

	sc->bnx_rx_quick_cons_trip_int = 6;
	sc->bnx_rx_quick_cons_trip     = 6;
	sc->bnx_rx_ticks_int           = 18;
	sc->bnx_rx_ticks               = 18;
#endif

	/* Update statistics once every second. */
	sc->bnx_stats_ticks = 1000000 & 0xffff00;

	/* Find the media type for the adapter. */
	bnx_get_media(sc);

	/*
	 * Store config data needed by the PHY driver for
	 * backplane applications
	 */
	sc->bnx_shared_hw_cfg = REG_RD_IND(sc, sc->bnx_shmem_base +
	    BNX_SHARED_HW_CFG_CONFIG);
	sc->bnx_port_hw_cfg = REG_RD_IND(sc, sc->bnx_shmem_base +
	    BNX_PORT_HW_CFG_CONFIG);

	/* Allocate DMA memory resources. */
	sc->bnx_dmatag = pa->pa_dmat;
	if (bnx_dma_alloc(sc)) {
		aprint_error_dev(sc->bnx_dev,
		    "DMA resource allocation failed!\n");
		goto bnx_attach_fail;
	}

	/* Initialize the ifnet interface. */
	ifp = &sc->bnx_ec.ec_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bnx_ioctl;
	ifp->if_stop = bnx_stop;
	ifp->if_start = bnx_start;
	ifp->if_init = bnx_init;
	ifp->if_timer = 0;
	ifp->if_watchdog = bnx_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, USABLE_TX_BD - 1);
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);

	sc->bnx_ec.ec_capabilities |= ETHERCAP_JUMBO_MTU |
	    ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING;

	ifp->if_capabilities |=
	    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;

	/* Hookup IRQ last. */
	sc->bnx_intrhand = pci_intr_establish(pc, ih, IPL_NET, bnx_intr, sc);
	if (sc->bnx_intrhand == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto bnx_attach_fail;
	}
	aprint_normal_dev(sc->bnx_dev, "interrupting at %s\n", intrstr);

	/* create workqueue to handle packet allocations */
	if (workqueue_create(&sc->bnx_wq, device_xname(self),
	    bnx_alloc_pkts, sc, PRI_NONE, IPL_NET, 0) != 0) {
		aprint_error_dev(self, "failed to create workqueue\n");
		goto bnx_attach_fail;
	}

	sc->bnx_mii.mii_ifp = ifp;
	sc->bnx_mii.mii_readreg = bnx_miibus_read_reg;
	sc->bnx_mii.mii_writereg = bnx_miibus_write_reg;
	sc->bnx_mii.mii_statchg = bnx_miibus_statchg;

	/* Handle any special PHY initialization for SerDes PHYs. */
	bnx_init_media(sc);

	sc->bnx_ec.ec_mii = &sc->bnx_mii;
	ifmedia_init(&sc->bnx_mii.mii_media, 0, ether_mediachange,
	    ether_mediastatus);

	/* set phyflags and chipid before mii_attach() */
	dict = device_properties(self);
	prop_dictionary_set_uint32(dict, "phyflags", sc->bnx_phy_flags);
	prop_dictionary_set_uint32(dict, "chipid", sc->bnx_chipid);
	prop_dictionary_set_uint32(dict, "shared_hwcfg",sc->bnx_shared_hw_cfg);
	prop_dictionary_set_uint32(dict, "port_hwcfg", sc->bnx_port_hw_cfg);

	/* Print some useful adapter info */
	bnx_print_adapter_info(sc);

	if (sc->bnx_phy_flags & BNX_PHY_SERDES_FLAG)
		mii_flags |= MIIF_HAVEFIBER;
	mii_attach(self, &sc->bnx_mii, 0xffffffff,
	    MII_PHY_ANY, MII_OFFSET_ANY, mii_flags);

	if (LIST_EMPTY(&sc->bnx_mii.mii_phys)) {
		aprint_error_dev(self, "no PHY found!\n");
		ifmedia_add(&sc->bnx_mii.mii_media,
		    IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->bnx_mii.mii_media, IFM_ETHER | IFM_MANUAL);
	} else
		ifmedia_set(&sc->bnx_mii.mii_media, IFM_ETHER | IFM_AUTO);

	/* Attach to the Ethernet interface list. */
	if_attach(ifp);
	ether_ifattach(ifp,sc->eaddr);

	callout_init(&sc->bnx_timeout, 0);

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* Print some important debugging info. */
	DBRUN(BNX_INFO, bnx_dump_driver_state(sc));

	goto bnx_attach_exit;

bnx_attach_fail:
	bnx_release_resources(sc);

bnx_attach_exit:
	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);
}

/****************************************************************************/
/* Device detach function.                                                  */
/*                                                                          */
/* Stops the controller, resets the controller, and releases resources.     */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_detach(device_t dev, int flags)
{
	int s;
	struct bnx_softc *sc;
	struct ifnet *ifp;

	sc = device_private(dev);
	ifp = &sc->bnx_ec.ec_if;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Stop and reset the controller. */
	s = splnet();
	if (ifp->if_flags & IFF_RUNNING)
		bnx_stop(ifp, 1);
	else {
		/* Disable the transmit/receive blocks. */
		REG_WR(sc, BNX_MISC_ENABLE_CLR_BITS, 0x5ffffff);
		REG_RD(sc, BNX_MISC_ENABLE_CLR_BITS);
		DELAY(20);
		bnx_disable_intr(sc);
		bnx_reset(sc, BNX_DRV_MSG_CODE_RESET);
	}

	splx(s);

	pmf_device_deregister(dev);
	callout_destroy(&sc->bnx_timeout);
	ether_ifdetach(ifp);
	workqueue_destroy(sc->bnx_wq);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->bnx_mii.mii_media, IFM_INST_ANY);

	if_detach(ifp);
	mii_detach(&sc->bnx_mii, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Release all remaining resources. */
	bnx_release_resources(sc);

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	return 0;
}

/****************************************************************************/
/* Indirect register read.                                                  */
/*                                                                          */
/* Reads NetXtreme II registers using an index/data register pair in PCI    */
/* configuration space.  Using this mechanism avoids issues with posted     */
/* reads but is much slower than memory-mapped I/O.                         */
/*                                                                          */
/* Returns:                                                                 */
/*   The value of the register.                                             */
/****************************************************************************/
uint32_t
bnx_reg_rd_ind(struct bnx_softc *sc, uint32_t offset)
{
	struct pci_attach_args	*pa = &(sc->bnx_pa);

	pci_conf_write(pa->pa_pc, pa->pa_tag, BNX_PCICFG_REG_WINDOW_ADDRESS,
	    offset);
#ifdef BNX_DEBUG
	{
		uint32_t val;
		val = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    BNX_PCICFG_REG_WINDOW);
		DBPRINT(sc, BNX_EXCESSIVE, "%s(); offset = 0x%08X, "
		    "val = 0x%08X\n", __func__, offset, val);
		return val;
	}
#else
	return pci_conf_read(pa->pa_pc, pa->pa_tag, BNX_PCICFG_REG_WINDOW);
#endif
}

/****************************************************************************/
/* Indirect register write.                                                 */
/*                                                                          */
/* Writes NetXtreme II registers using an index/data register pair in PCI   */
/* configuration space.  Using this mechanism avoids issues with posted     */
/* writes but is muchh slower than memory-mapped I/O.                       */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_reg_wr_ind(struct bnx_softc *sc, uint32_t offset, uint32_t val)
{
	struct pci_attach_args  *pa = &(sc->bnx_pa);

	DBPRINT(sc, BNX_EXCESSIVE, "%s(); offset = 0x%08X, val = 0x%08X\n",
		__func__, offset, val);

	pci_conf_write(pa->pa_pc, pa->pa_tag, BNX_PCICFG_REG_WINDOW_ADDRESS,
	    offset);
	pci_conf_write(pa->pa_pc, pa->pa_tag, BNX_PCICFG_REG_WINDOW, val);
}

/****************************************************************************/
/* Context memory write.                                                    */
/*                                                                          */
/* The NetXtreme II controller uses context memory to track connection      */
/* information for L2 and higher network protocols.                         */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_ctx_wr(struct bnx_softc *sc, uint32_t cid_addr, uint32_t ctx_offset,
    uint32_t ctx_val)
{
	uint32_t idx, offset = ctx_offset + cid_addr;
	uint32_t val, retry_cnt = 5;

	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		REG_WR(sc, BNX_CTX_CTX_DATA, ctx_val);
		REG_WR(sc, BNX_CTX_CTX_CTRL,
		    (offset | BNX_CTX_CTX_CTRL_WRITE_REQ));

		for (idx = 0; idx < retry_cnt; idx++) {
			val = REG_RD(sc, BNX_CTX_CTX_CTRL);
			if ((val & BNX_CTX_CTX_CTRL_WRITE_REQ) == 0)
				break;
			DELAY(5);
		}

#if 0
		if (val & BNX_CTX_CTX_CTRL_WRITE_REQ)
			BNX_PRINTF("%s(%d); Unable to write CTX memory: "
				"cid_addr = 0x%08X, offset = 0x%08X!\n",
				__FILE__, __LINE__, cid_addr, ctx_offset);
#endif

	} else {
		REG_WR(sc, BNX_CTX_DATA_ADR, offset);
		REG_WR(sc, BNX_CTX_DATA, ctx_val);
	}
}

/****************************************************************************/
/* PHY register read.                                                       */
/*                                                                          */
/* Implements register reads on the MII bus.                                */
/*                                                                          */
/* Returns:                                                                 */
/*   The value of the register.                                             */
/****************************************************************************/
int
bnx_miibus_read_reg(device_t dev, int phy, int reg)
{
	struct bnx_softc	*sc = device_private(dev);
	uint32_t		val;
	int			i;

	/* Make sure we are accessing the correct PHY address. */
	if (phy != sc->bnx_phy_addr) {
		DBPRINT(sc, BNX_VERBOSE,
		    "Invalid PHY address %d for PHY read!\n", phy);
		return 0;
	}

	/*
	 * The BCM5709S PHY is an IEEE Clause 45 PHY
	 * with special mappings to work with IEEE
	 * Clause 22 register accesses.
	 */
	if ((sc->bnx_phy_flags & BNX_PHY_IEEE_CLAUSE_45_FLAG) != 0) {
		if (reg >= MII_BMCR && reg <= MII_ANLPRNP)
			reg += 0x10;
	}

	if (sc->bnx_phy_flags & BNX_PHY_INT_MODE_AUTO_POLLING_FLAG) {
		val = REG_RD(sc, BNX_EMAC_MDIO_MODE);
		val &= ~BNX_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(sc, BNX_EMAC_MDIO_MODE, val);
		REG_RD(sc, BNX_EMAC_MDIO_MODE);

		DELAY(40);
	}

	val = BNX_MIPHY(phy) | BNX_MIREG(reg) |
	    BNX_EMAC_MDIO_COMM_COMMAND_READ | BNX_EMAC_MDIO_COMM_DISEXT |
	    BNX_EMAC_MDIO_COMM_START_BUSY;
	REG_WR(sc, BNX_EMAC_MDIO_COMM, val);

	for (i = 0; i < BNX_PHY_TIMEOUT; i++) {
		DELAY(10);

		val = REG_RD(sc, BNX_EMAC_MDIO_COMM);
		if (!(val & BNX_EMAC_MDIO_COMM_START_BUSY)) {
			DELAY(5);

			val = REG_RD(sc, BNX_EMAC_MDIO_COMM);
			val &= BNX_EMAC_MDIO_COMM_DATA;

			break;
		}
	}

	if (val & BNX_EMAC_MDIO_COMM_START_BUSY) {
		BNX_PRINTF(sc, "%s(%d): Error: PHY read timeout! phy = %d, "
		    "reg = 0x%04X\n", __FILE__, __LINE__, phy, reg);
		val = 0x0;
	} else
		val = REG_RD(sc, BNX_EMAC_MDIO_COMM);

	DBPRINT(sc, BNX_EXCESSIVE,
	    "%s(): phy = %d, reg = 0x%04X, val = 0x%04X\n", __func__, phy,
	    (uint16_t) reg & 0xffff, (uint16_t) val & 0xffff);

	if (sc->bnx_phy_flags & BNX_PHY_INT_MODE_AUTO_POLLING_FLAG) {
		val = REG_RD(sc, BNX_EMAC_MDIO_MODE);
		val |= BNX_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(sc, BNX_EMAC_MDIO_MODE, val);
		REG_RD(sc, BNX_EMAC_MDIO_MODE);

		DELAY(40);
	}

	return (val & 0xffff);
}

/****************************************************************************/
/* PHY register write.                                                      */
/*                                                                          */
/* Implements register writes on the MII bus.                               */
/*                                                                          */
/* Returns:                                                                 */
/*   The value of the register.                                             */
/****************************************************************************/
void
bnx_miibus_write_reg(device_t dev, int phy, int reg, int val)
{
	struct bnx_softc	*sc = device_private(dev);
	uint32_t		val1;
	int			i;

	/* Make sure we are accessing the correct PHY address. */
	if (phy != sc->bnx_phy_addr) {
		DBPRINT(sc, BNX_WARN, "Invalid PHY address %d for PHY write!\n",
		    phy);
		return;
	}

	DBPRINT(sc, BNX_EXCESSIVE, "%s(): phy = %d, reg = 0x%04X, "
	    "val = 0x%04X\n", __func__,
	    phy, (uint16_t) reg & 0xffff, (uint16_t) val & 0xffff);

	/*
	 * The BCM5709S PHY is an IEEE Clause 45 PHY
	 * with special mappings to work with IEEE
	 * Clause 22 register accesses.
	 */
	if ((sc->bnx_phy_flags & BNX_PHY_IEEE_CLAUSE_45_FLAG) != 0) {
		if (reg >= MII_BMCR && reg <= MII_ANLPRNP)
			reg += 0x10;
	}

	if (sc->bnx_phy_flags & BNX_PHY_INT_MODE_AUTO_POLLING_FLAG) {
		val1 = REG_RD(sc, BNX_EMAC_MDIO_MODE);
		val1 &= ~BNX_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(sc, BNX_EMAC_MDIO_MODE, val1);
		REG_RD(sc, BNX_EMAC_MDIO_MODE);

		DELAY(40);
	}

	val1 = BNX_MIPHY(phy) | BNX_MIREG(reg) | val |
	    BNX_EMAC_MDIO_COMM_COMMAND_WRITE |
	    BNX_EMAC_MDIO_COMM_START_BUSY | BNX_EMAC_MDIO_COMM_DISEXT;
	REG_WR(sc, BNX_EMAC_MDIO_COMM, val1);

	for (i = 0; i < BNX_PHY_TIMEOUT; i++) {
		DELAY(10);

		val1 = REG_RD(sc, BNX_EMAC_MDIO_COMM);
		if (!(val1 & BNX_EMAC_MDIO_COMM_START_BUSY)) {
			DELAY(5);
			break;
		}
	}

	if (val1 & BNX_EMAC_MDIO_COMM_START_BUSY) {
		BNX_PRINTF(sc, "%s(%d): PHY write timeout!\n", __FILE__,
		    __LINE__);
	}

	if (sc->bnx_phy_flags & BNX_PHY_INT_MODE_AUTO_POLLING_FLAG) {
		val1 = REG_RD(sc, BNX_EMAC_MDIO_MODE);
		val1 |= BNX_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(sc, BNX_EMAC_MDIO_MODE, val1);
		REG_RD(sc, BNX_EMAC_MDIO_MODE);

		DELAY(40);
	}
}

/****************************************************************************/
/* MII bus status change.                                                   */
/*                                                                          */
/* Called by the MII bus driver when the PHY establishes link to set the    */
/* MAC interface registers.                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_miibus_statchg(struct ifnet *ifp)
{
	struct bnx_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = &sc->bnx_mii;
	int			val;

	val = REG_RD(sc, BNX_EMAC_MODE);
	val &= ~(BNX_EMAC_MODE_PORT | BNX_EMAC_MODE_HALF_DUPLEX |
	    BNX_EMAC_MODE_MAC_LOOP | BNX_EMAC_MODE_FORCE_LINK |
	    BNX_EMAC_MODE_25G);

	/* Set MII or GMII interface based on the speed
	 * negotiated by the PHY.
	 */
	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_10_T:
		if (BNX_CHIP_NUM(sc) != BNX_CHIP_NUM_5706) {
			DBPRINT(sc, BNX_INFO, "Enabling 10Mb interface.\n");
			val |= BNX_EMAC_MODE_PORT_MII_10;
			break;
		}
		/* FALLTHROUGH */
	case IFM_100_TX:
		DBPRINT(sc, BNX_INFO, "Enabling MII interface.\n");
		val |= BNX_EMAC_MODE_PORT_MII;
		break;
	case IFM_2500_SX:
		DBPRINT(sc, BNX_INFO, "Enabling 2.5G MAC mode.\n");
		val |= BNX_EMAC_MODE_25G;
		/* FALLTHROUGH */
	case IFM_1000_T:
	case IFM_1000_SX:
		DBPRINT(sc, BNX_INFO, "Enabling GMII interface.\n");
		val |= BNX_EMAC_MODE_PORT_GMII;
		break;
	default:
		val |= BNX_EMAC_MODE_PORT_GMII;
		break;
	}

	/* Set half or full duplex based on the duplicity
	 * negotiated by the PHY.
	 */
	if ((mii->mii_media_active & IFM_GMASK) == IFM_HDX) {
		DBPRINT(sc, BNX_INFO, "Setting Half-Duplex interface.\n");
		val |= BNX_EMAC_MODE_HALF_DUPLEX;
	} else {
		DBPRINT(sc, BNX_INFO, "Setting Full-Duplex interface.\n");
	}

	REG_WR(sc, BNX_EMAC_MODE, val);
}

/****************************************************************************/
/* Acquire NVRAM lock.                                                      */
/*                                                                          */
/* Before the NVRAM can be accessed the caller must acquire an NVRAM lock.  */
/* Locks 0 and 2 are reserved, lock 1 is used by firmware and lock 2 is     */
/* for use by the driver.                                                   */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_acquire_nvram_lock(struct bnx_softc *sc)
{
	uint32_t		val;
	int			j;

	DBPRINT(sc, BNX_VERBOSE, "Acquiring NVRAM lock.\n");

	/* Request access to the flash interface. */
	REG_WR(sc, BNX_NVM_SW_ARB, BNX_NVM_SW_ARB_ARB_REQ_SET2);
	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		val = REG_RD(sc, BNX_NVM_SW_ARB);
		if (val & BNX_NVM_SW_ARB_ARB_ARB2)
			break;

		DELAY(5);
	}

	if (j >= NVRAM_TIMEOUT_COUNT) {
		DBPRINT(sc, BNX_WARN, "Timeout acquiring NVRAM lock!\n");
		return EBUSY;
	}

	return 0;
}

/****************************************************************************/
/* Release NVRAM lock.                                                      */
/*                                                                          */
/* When the caller is finished accessing NVRAM the lock must be released.   */
/* Locks 0 and 2 are reserved, lock 1 is used by firmware and lock 2 is     */
/* for use by the driver.                                                   */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_release_nvram_lock(struct bnx_softc *sc)
{
	int			j;
	uint32_t		val;

	DBPRINT(sc, BNX_VERBOSE, "Releasing NVRAM lock.\n");

	/* Relinquish nvram interface. */
	REG_WR(sc, BNX_NVM_SW_ARB, BNX_NVM_SW_ARB_ARB_REQ_CLR2);

	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		val = REG_RD(sc, BNX_NVM_SW_ARB);
		if (!(val & BNX_NVM_SW_ARB_ARB_ARB2))
			break;

		DELAY(5);
	}

	if (j >= NVRAM_TIMEOUT_COUNT) {
		DBPRINT(sc, BNX_WARN, "Timeout reeasing NVRAM lock!\n");
		return EBUSY;
	}

	return 0;
}

#ifdef BNX_NVRAM_WRITE_SUPPORT
/****************************************************************************/
/* Enable NVRAM write access.                                               */
/*                                                                          */
/* Before writing to NVRAM the caller must enable NVRAM writes.             */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_enable_nvram_write(struct bnx_softc *sc)
{
	uint32_t		val;

	DBPRINT(sc, BNX_VERBOSE, "Enabling NVRAM write.\n");

	val = REG_RD(sc, BNX_MISC_CFG);
	REG_WR(sc, BNX_MISC_CFG, val | BNX_MISC_CFG_NVM_WR_EN_PCI);

	if (!ISSET(sc->bnx_flash_info->flags, BNX_NV_BUFFERED)) {
		int j;

		REG_WR(sc, BNX_NVM_COMMAND, BNX_NVM_COMMAND_DONE);
		REG_WR(sc, BNX_NVM_COMMAND,
		    BNX_NVM_COMMAND_WREN | BNX_NVM_COMMAND_DOIT);

		for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
			DELAY(5);

			val = REG_RD(sc, BNX_NVM_COMMAND);
			if (val & BNX_NVM_COMMAND_DONE)
				break;
		}

		if (j >= NVRAM_TIMEOUT_COUNT) {
			DBPRINT(sc, BNX_WARN, "Timeout writing NVRAM!\n");
			return EBUSY;
		}
	}

	return 0;
}

/****************************************************************************/
/* Disable NVRAM write access.                                              */
/*                                                                          */
/* When the caller is finished writing to NVRAM write access must be        */
/* disabled.                                                                */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_disable_nvram_write(struct bnx_softc *sc)
{
	uint32_t		val;

	DBPRINT(sc, BNX_VERBOSE,  "Disabling NVRAM write.\n");

	val = REG_RD(sc, BNX_MISC_CFG);
	REG_WR(sc, BNX_MISC_CFG, val & ~BNX_MISC_CFG_NVM_WR_EN);
}
#endif

/****************************************************************************/
/* Enable NVRAM access.                                                     */
/*                                                                          */
/* Before accessing NVRAM for read or write operations the caller must      */
/* enabled NVRAM access.                                                    */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_enable_nvram_access(struct bnx_softc *sc)
{
	uint32_t		val;

	DBPRINT(sc, BNX_VERBOSE, "Enabling NVRAM access.\n");

	val = REG_RD(sc, BNX_NVM_ACCESS_ENABLE);
	/* Enable both bits, even on read. */
	REG_WR(sc, BNX_NVM_ACCESS_ENABLE,
	    val | BNX_NVM_ACCESS_ENABLE_EN | BNX_NVM_ACCESS_ENABLE_WR_EN);
}

/****************************************************************************/
/* Disable NVRAM access.                                                    */
/*                                                                          */
/* When the caller is finished accessing NVRAM access must be disabled.     */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_disable_nvram_access(struct bnx_softc *sc)
{
	uint32_t		val;

	DBPRINT(sc, BNX_VERBOSE, "Disabling NVRAM access.\n");

	val = REG_RD(sc, BNX_NVM_ACCESS_ENABLE);

	/* Disable both bits, even after read. */
	REG_WR(sc, BNX_NVM_ACCESS_ENABLE,
	    val & ~(BNX_NVM_ACCESS_ENABLE_EN | BNX_NVM_ACCESS_ENABLE_WR_EN));
}

#ifdef BNX_NVRAM_WRITE_SUPPORT
/****************************************************************************/
/* Erase NVRAM page before writing.                                         */
/*                                                                          */
/* Non-buffered flash parts require that a page be erased before it is      */
/* written.                                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_nvram_erase_page(struct bnx_softc *sc, uint32_t offset)
{
	uint32_t		cmd;
	int			j;

	/* Buffered flash doesn't require an erase. */
	if (ISSET(sc->bnx_flash_info->flags, BNX_NV_BUFFERED))
		return 0;

	DBPRINT(sc, BNX_VERBOSE, "Erasing NVRAM page.\n");

	/* Build an erase command. */
	cmd = BNX_NVM_COMMAND_ERASE | BNX_NVM_COMMAND_WR |
	    BNX_NVM_COMMAND_DOIT;

	/*
	 * Clear the DONE bit separately, set the NVRAM address to erase,
	 * and issue the erase command.
	 */
	REG_WR(sc, BNX_NVM_COMMAND, BNX_NVM_COMMAND_DONE);
	REG_WR(sc, BNX_NVM_ADDR, offset & BNX_NVM_ADDR_NVM_ADDR_VALUE);
	REG_WR(sc, BNX_NVM_COMMAND, cmd);

	/* Wait for completion. */
	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		uint32_t val;

		DELAY(5);

		val = REG_RD(sc, BNX_NVM_COMMAND);
		if (val & BNX_NVM_COMMAND_DONE)
			break;
	}

	if (j >= NVRAM_TIMEOUT_COUNT) {
		DBPRINT(sc, BNX_WARN, "Timeout erasing NVRAM.\n");
		return EBUSY;
	}

	return 0;
}
#endif /* BNX_NVRAM_WRITE_SUPPORT */

/****************************************************************************/
/* Read a dword (32 bits) from NVRAM.                                       */
/*                                                                          */
/* Read a 32 bit word from NVRAM.  The caller is assumed to have already    */
/* obtained the NVRAM lock and enabled the controller for NVRAM access.     */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success and the 32 bit value read, positive value on failure.     */
/****************************************************************************/
int
bnx_nvram_read_dword(struct bnx_softc *sc, uint32_t offset,
    uint8_t *ret_val, uint32_t cmd_flags)
{
	uint32_t		cmd;
	int			i, rc = 0;

	/* Build the command word. */
	cmd = BNX_NVM_COMMAND_DOIT | cmd_flags;

	/* Calculate the offset for buffered flash if translation is used. */
	if (ISSET(sc->bnx_flash_info->flags, BNX_NV_TRANSLATE)) {
		offset = ((offset / sc->bnx_flash_info->page_size) <<
		    sc->bnx_flash_info->page_bits) +
		    (offset % sc->bnx_flash_info->page_size);
	}

	/*
	 * Clear the DONE bit separately, set the address to read,
	 * and issue the read.
	 */
	REG_WR(sc, BNX_NVM_COMMAND, BNX_NVM_COMMAND_DONE);
	REG_WR(sc, BNX_NVM_ADDR, offset & BNX_NVM_ADDR_NVM_ADDR_VALUE);
	REG_WR(sc, BNX_NVM_COMMAND, cmd);

	/* Wait for completion. */
	for (i = 0; i < NVRAM_TIMEOUT_COUNT; i++) {
		uint32_t val;

		DELAY(5);

		val = REG_RD(sc, BNX_NVM_COMMAND);
		if (val & BNX_NVM_COMMAND_DONE) {
			val = REG_RD(sc, BNX_NVM_READ);

			val = bnx_be32toh(val);
			memcpy(ret_val, &val, 4);
			break;
		}
	}

	/* Check for errors. */
	if (i >= NVRAM_TIMEOUT_COUNT) {
		BNX_PRINTF(sc, "%s(%d): Timeout error reading NVRAM at "
		    "offset 0x%08X!\n", __FILE__, __LINE__, offset);
		rc = EBUSY;
	}

	return rc;
}

#ifdef BNX_NVRAM_WRITE_SUPPORT
/****************************************************************************/
/* Write a dword (32 bits) to NVRAM.                                        */
/*                                                                          */
/* Write a 32 bit word to NVRAM.  The caller is assumed to have already     */
/* obtained the NVRAM lock, enabled the controller for NVRAM access, and    */
/* enabled NVRAM write access.                                              */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_nvram_write_dword(struct bnx_softc *sc, uint32_t offset, uint8_t *val,
    uint32_t cmd_flags)
{
	uint32_t		cmd, val32;
	int			j;

	/* Build the command word. */
	cmd = BNX_NVM_COMMAND_DOIT | BNX_NVM_COMMAND_WR | cmd_flags;

	/* Calculate the offset for buffered flash if translation is used. */
	if (ISSET(sc->bnx_flash_info->flags, BNX_NV_TRANSLATE)) {
		offset = ((offset / sc->bnx_flash_info->page_size) <<
		    sc->bnx_flash_info->page_bits) +
		    (offset % sc->bnx_flash_info->page_size);
	}

	/*
	 * Clear the DONE bit separately, convert NVRAM data to big-endian,
	 * set the NVRAM address to write, and issue the write command
	 */
	REG_WR(sc, BNX_NVM_COMMAND, BNX_NVM_COMMAND_DONE);
	memcpy(&val32, val, 4);
	val32 = htobe32(val32);
	REG_WR(sc, BNX_NVM_WRITE, val32);
	REG_WR(sc, BNX_NVM_ADDR, offset & BNX_NVM_ADDR_NVM_ADDR_VALUE);
	REG_WR(sc, BNX_NVM_COMMAND, cmd);

	/* Wait for completion. */
	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		DELAY(5);

		if (REG_RD(sc, BNX_NVM_COMMAND) & BNX_NVM_COMMAND_DONE)
			break;
	}
	if (j >= NVRAM_TIMEOUT_COUNT) {
		BNX_PRINTF(sc, "%s(%d): Timeout error writing NVRAM at "
		    "offset 0x%08X\n", __FILE__, __LINE__, offset);
		return EBUSY;
	}

	return 0;
}
#endif /* BNX_NVRAM_WRITE_SUPPORT */

/****************************************************************************/
/* Initialize NVRAM access.                                                 */
/*                                                                          */
/* Identify the NVRAM device in use and prepare the NVRAM interface to      */
/* access that device.                                                      */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_init_nvram(struct bnx_softc *sc)
{
	uint32_t		val;
	int			j, entry_count, rc = 0;
	struct flash_spec	*flash;

	DBPRINT(sc,BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		sc->bnx_flash_info = &flash_5709;
		goto bnx_init_nvram_get_flash_size;
	}

	/* Determine the selected interface. */
	val = REG_RD(sc, BNX_NVM_CFG1);

	entry_count = sizeof(flash_table) / sizeof(struct flash_spec);

	/*
	 * Flash reconfiguration is required to support additional
	 * NVRAM devices not directly supported in hardware.
	 * Check if the flash interface was reconfigured
	 * by the bootcode.
	 */

	if (val & 0x40000000) {
		/* Flash interface reconfigured by bootcode. */

		DBPRINT(sc,BNX_INFO_LOAD,
			"bnx_init_nvram(): Flash WAS reconfigured.\n");

		for (j = 0, flash = &flash_table[0]; j < entry_count;
		     j++, flash++) {
			if ((val & FLASH_BACKUP_STRAP_MASK) ==
			    (flash->config1 & FLASH_BACKUP_STRAP_MASK)) {
				sc->bnx_flash_info = flash;
				break;
			}
		}
	} else {
		/* Flash interface not yet reconfigured. */
		uint32_t mask;

		DBPRINT(sc,BNX_INFO_LOAD,
			"bnx_init_nvram(): Flash was NOT reconfigured.\n");

		if (val & (1 << 23))
			mask = FLASH_BACKUP_STRAP_MASK;
		else
			mask = FLASH_STRAP_MASK;

		/* Look for the matching NVRAM device configuration data. */
		for (j = 0, flash = &flash_table[0]; j < entry_count;
		    j++, flash++) {
			/* Check if the dev matches any of the known devices. */
			if ((val & mask) == (flash->strapping & mask)) {
				/* Found a device match. */
				sc->bnx_flash_info = flash;

				/* Request access to the flash interface. */
				if ((rc = bnx_acquire_nvram_lock(sc)) != 0)
					return rc;

				/* Reconfigure the flash interface. */
				bnx_enable_nvram_access(sc);
				REG_WR(sc, BNX_NVM_CFG1, flash->config1);
				REG_WR(sc, BNX_NVM_CFG2, flash->config2);
				REG_WR(sc, BNX_NVM_CFG3, flash->config3);
				REG_WR(sc, BNX_NVM_WRITE1, flash->write1);
				bnx_disable_nvram_access(sc);
				bnx_release_nvram_lock(sc);

				break;
			}
		}
	}

	/* Check if a matching device was found. */
	if (j == entry_count) {
		sc->bnx_flash_info = NULL;
		BNX_PRINTF(sc, "%s(%d): Unknown Flash NVRAM found!\n",
			__FILE__, __LINE__);
		rc = ENODEV;
	}

bnx_init_nvram_get_flash_size:
	/* Write the flash config data to the shared memory interface. */
	val = REG_RD_IND(sc, sc->bnx_shmem_base + BNX_SHARED_HW_CFG_CONFIG2);
	val &= BNX_SHARED_HW_CFG2_NVM_SIZE_MASK;
	if (val)
		sc->bnx_flash_size = val;
	else
		sc->bnx_flash_size = sc->bnx_flash_info->total_size;

	DBPRINT(sc, BNX_INFO_LOAD, "bnx_init_nvram() flash->total_size = "
	    "0x%08X\n", sc->bnx_flash_info->total_size);

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	return rc;
}

/****************************************************************************/
/* Read an arbitrary range of data from NVRAM.                              */
/*                                                                          */
/* Prepares the NVRAM interface for access and reads the requested data     */
/* into the supplied buffer.                                                */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success and the data read, positive value on failure.             */
/****************************************************************************/
int
bnx_nvram_read(struct bnx_softc *sc, uint32_t offset, uint8_t *ret_buf,
    int buf_size)
{
	int			rc = 0;
	uint32_t		cmd_flags, offset32, len32, extra;

	if (buf_size == 0)
		return 0;

	/* Request access to the flash interface. */
	if ((rc = bnx_acquire_nvram_lock(sc)) != 0)
		return rc;

	/* Enable access to flash interface */
	bnx_enable_nvram_access(sc);

	len32 = buf_size;
	offset32 = offset;
	extra = 0;

	cmd_flags = 0;

	if (offset32 & 3) {
		uint8_t buf[4];
		uint32_t pre_len;

		offset32 &= ~3;
		pre_len = 4 - (offset & 3);

		if (pre_len >= len32) {
			pre_len = len32;
			cmd_flags =
			    BNX_NVM_COMMAND_FIRST | BNX_NVM_COMMAND_LAST;
		} else
			cmd_flags = BNX_NVM_COMMAND_FIRST;

		rc = bnx_nvram_read_dword(sc, offset32, buf, cmd_flags);

		if (rc)
			return rc;

		memcpy(ret_buf, buf + (offset & 3), pre_len);

		offset32 += 4;
		ret_buf += pre_len;
		len32 -= pre_len;
	}

	if (len32 & 3) {
		extra = 4 - (len32 & 3);
		len32 = (len32 + 4) & ~3;
	}

	if (len32 == 4) {
		uint8_t buf[4];

		if (cmd_flags)
			cmd_flags = BNX_NVM_COMMAND_LAST;
		else
			cmd_flags =
			    BNX_NVM_COMMAND_FIRST | BNX_NVM_COMMAND_LAST;

		rc = bnx_nvram_read_dword(sc, offset32, buf, cmd_flags);

		memcpy(ret_buf, buf, 4 - extra);
	} else if (len32 > 0) {
		uint8_t buf[4];

		/* Read the first word. */
		if (cmd_flags)
			cmd_flags = 0;
		else
			cmd_flags = BNX_NVM_COMMAND_FIRST;

		rc = bnx_nvram_read_dword(sc, offset32, ret_buf, cmd_flags);

		/* Advance to the next dword. */
		offset32 += 4;
		ret_buf += 4;
		len32 -= 4;

		while (len32 > 4 && rc == 0) {
			rc = bnx_nvram_read_dword(sc, offset32, ret_buf, 0);

			/* Advance to the next dword. */
			offset32 += 4;
			ret_buf += 4;
			len32 -= 4;
		}

		if (rc)
			return rc;

		cmd_flags = BNX_NVM_COMMAND_LAST;
		rc = bnx_nvram_read_dword(sc, offset32, buf, cmd_flags);

		memcpy(ret_buf, buf, 4 - extra);
	}

	/* Disable access to flash interface and release the lock. */
	bnx_disable_nvram_access(sc);
	bnx_release_nvram_lock(sc);

	return rc;
}

#ifdef BNX_NVRAM_WRITE_SUPPORT
/****************************************************************************/
/* Write an arbitrary range of data from NVRAM.                             */
/*                                                                          */
/* Prepares the NVRAM interface for write access and writes the requested   */
/* data from the supplied buffer.  The caller is responsible for            */
/* calculating any appropriate CRCs.                                        */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_nvram_write(struct bnx_softc *sc, uint32_t offset, uint8_t *data_buf,
    int buf_size)
{
	uint32_t		written, offset32, len32;
	uint8_t		*buf, start[4], end[4];
	int			rc = 0;
	int			align_start, align_end;

	buf = data_buf;
	offset32 = offset;
	len32 = buf_size;
	align_start = align_end = 0;

	if ((align_start = (offset32 & 3))) {
		offset32 &= ~3;
		len32 += align_start;
		if ((rc = bnx_nvram_read(sc, offset32, start, 4)))
			return rc;
	}

	if (len32 & 3) {
		if ((len32 > 4) || !align_start) {
			align_end = 4 - (len32 & 3);
			len32 += align_end;
			if ((rc = bnx_nvram_read(sc, offset32 + len32 - 4,
			    end, 4)))
				return rc;
		}
	}

	if (align_start || align_end) {
		buf = malloc(len32, M_DEVBUF, M_NOWAIT);
		if (buf == 0)
			return ENOMEM;

		if (align_start)
			memcpy(buf, start, 4);

		if (align_end)
			memcpy(buf + len32 - 4, end, 4);

		memcpy(buf + align_start, data_buf, buf_size);
	}

	written = 0;
	while ((written < len32) && (rc == 0)) {
		uint32_t page_start, page_end, data_start, data_end;
		uint32_t addr, cmd_flags;
		int i;
		uint8_t flash_buffer[264];

	    /* Find the page_start addr */
		page_start = offset32 + written;
		page_start -= (page_start % sc->bnx_flash_info->page_size);
		/* Find the page_end addr */
		page_end = page_start + sc->bnx_flash_info->page_size;
		/* Find the data_start addr */
		data_start = (written == 0) ? offset32 : page_start;
		/* Find the data_end addr */
		data_end = (page_end > offset32 + len32) ?
		    (offset32 + len32) : page_end;

		/* Request access to the flash interface. */
		if ((rc = bnx_acquire_nvram_lock(sc)) != 0)
			goto nvram_write_end;

		/* Enable access to flash interface */
		bnx_enable_nvram_access(sc);

		cmd_flags = BNX_NVM_COMMAND_FIRST;
		if (!ISSET(sc->bnx_flash_info->flags, BNX_NV_BUFFERED)) {
			int j;

			/* Read the whole page into the buffer
			 * (non-buffer flash only) */
			for (j = 0; j < sc->bnx_flash_info->page_size; j += 4) {
				if (j == (sc->bnx_flash_info->page_size - 4))
					cmd_flags |= BNX_NVM_COMMAND_LAST;

				rc = bnx_nvram_read_dword(sc,
					page_start + j,
					&flash_buffer[j],
					cmd_flags);

				if (rc)
					goto nvram_write_end;

				cmd_flags = 0;
			}
		}

		/* Enable writes to flash interface (unlock write-protect) */
		if ((rc = bnx_enable_nvram_write(sc)) != 0)
			goto nvram_write_end;

		/* Erase the page */
		if ((rc = bnx_nvram_erase_page(sc, page_start)) != 0)
			goto nvram_write_end;

		/* Re-enable the write again for the actual write */
		bnx_enable_nvram_write(sc);

		/* Loop to write back the buffer data from page_start to
		 * data_start */
		i = 0;
		if (!ISSET(sc->bnx_flash_info->flags, BNX_NV_BUFFERED)) {
			for (addr = page_start; addr < data_start;
				addr += 4, i += 4) {

				rc = bnx_nvram_write_dword(sc, addr,
				    &flash_buffer[i], cmd_flags);

				if (rc != 0)
					goto nvram_write_end;

				cmd_flags = 0;
			}
		}

		/* Loop to write the new data from data_start to data_end */
		for (addr = data_start; addr < data_end; addr += 4, i++) {
			if ((addr == page_end - 4) ||
			    (ISSET(sc->bnx_flash_info->flags, BNX_NV_BUFFERED)
			    && (addr == data_end - 4))) {

				cmd_flags |= BNX_NVM_COMMAND_LAST;
			}

			rc = bnx_nvram_write_dword(sc, addr, buf, cmd_flags);

			if (rc != 0)
				goto nvram_write_end;

			cmd_flags = 0;
			buf += 4;
		}

		/* Loop to write back the buffer data from data_end
		 * to page_end */
		if (!ISSET(sc->bnx_flash_info->flags, BNX_NV_BUFFERED)) {
			for (addr = data_end; addr < page_end;
			    addr += 4, i += 4) {

				if (addr == page_end-4)
					cmd_flags = BNX_NVM_COMMAND_LAST;

				rc = bnx_nvram_write_dword(sc, addr,
				    &flash_buffer[i], cmd_flags);

				if (rc != 0)
					goto nvram_write_end;

				cmd_flags = 0;
			}
		}

		/* Disable writes to flash interface (lock write-protect) */
		bnx_disable_nvram_write(sc);

		/* Disable access to flash interface */
		bnx_disable_nvram_access(sc);
		bnx_release_nvram_lock(sc);

		/* Increment written */
		written += data_end - data_start;
	}

nvram_write_end:
	if (align_start || align_end)
		free(buf, M_DEVBUF);

	return rc;
}
#endif /* BNX_NVRAM_WRITE_SUPPORT */

/****************************************************************************/
/* Verifies that NVRAM is accessible and contains valid data.               */
/*                                                                          */
/* Reads the configuration data from NVRAM and verifies that the CRC is     */
/* correct.                                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
int
bnx_nvram_test(struct bnx_softc *sc)
{
	uint32_t		buf[BNX_NVRAM_SIZE / 4];
	uint8_t		*data = (uint8_t *) buf;
	int			rc = 0;
	uint32_t		magic, csum;

	/*
	 * Check that the device NVRAM is valid by reading
	 * the magic value at offset 0.
	 */
	if ((rc = bnx_nvram_read(sc, 0, data, 4)) != 0)
		goto bnx_nvram_test_done;

	magic = bnx_be32toh(buf[0]);
	if (magic != BNX_NVRAM_MAGIC) {
		rc = ENODEV;
		BNX_PRINTF(sc, "%s(%d): Invalid NVRAM magic value! "
		    "Expected: 0x%08X, Found: 0x%08X\n",
		    __FILE__, __LINE__, BNX_NVRAM_MAGIC, magic);
		goto bnx_nvram_test_done;
	}

	/*
	 * Verify that the device NVRAM includes valid
	 * configuration data.
	 */
	if ((rc = bnx_nvram_read(sc, 0x100, data, BNX_NVRAM_SIZE)) != 0)
		goto bnx_nvram_test_done;

	csum = ether_crc32_le(data, 0x100);
	if (csum != BNX_CRC32_RESIDUAL) {
		rc = ENODEV;
		BNX_PRINTF(sc, "%s(%d): Invalid Manufacturing Information "
		    "NVRAM CRC! Expected: 0x%08X, Found: 0x%08X\n",
		    __FILE__, __LINE__, BNX_CRC32_RESIDUAL, csum);
		goto bnx_nvram_test_done;
	}

	csum = ether_crc32_le(data + 0x100, 0x100);
	if (csum != BNX_CRC32_RESIDUAL) {
		BNX_PRINTF(sc, "%s(%d): Invalid Feature Configuration "
		    "Information NVRAM CRC! Expected: 0x%08X, Found: 08%08X\n",
		    __FILE__, __LINE__, BNX_CRC32_RESIDUAL, csum);
		rc = ENODEV;
	}

bnx_nvram_test_done:
	return rc;
}

/****************************************************************************/
/* Identifies the current media type of the controller and sets the PHY     */
/* address.                                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_get_media(struct bnx_softc *sc)
{
	sc->bnx_phy_addr = 1;

	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		uint32_t val = REG_RD(sc, BNX_MISC_DUAL_MEDIA_CTRL);
		uint32_t bond_id = val & BNX_MISC_DUAL_MEDIA_CTRL_BOND_ID;
		uint32_t strap;

		/*
		 * The BCM5709S is software configurable
		 * for Copper or SerDes operation.
		 */
		if (bond_id == BNX_MISC_DUAL_MEDIA_CTRL_BOND_ID_C) {
			DBPRINT(sc, BNX_INFO_LOAD,
			    "5709 bonded for copper.\n");
			goto bnx_get_media_exit;
		} else if (bond_id == BNX_MISC_DUAL_MEDIA_CTRL_BOND_ID_S) {
			DBPRINT(sc, BNX_INFO_LOAD,
			    "5709 bonded for dual media.\n");
			sc->bnx_phy_flags |= BNX_PHY_SERDES_FLAG;
			goto bnx_get_media_exit;
		}

		if (val & BNX_MISC_DUAL_MEDIA_CTRL_STRAP_OVERRIDE)
			strap = (val & BNX_MISC_DUAL_MEDIA_CTRL_PHY_CTRL) >> 21;
		else {
			strap = (val & BNX_MISC_DUAL_MEDIA_CTRL_PHY_CTRL_STRAP)
			    >> 8;
		}

		if (sc->bnx_pa.pa_function == 0) {
			switch (strap) {
			case 0x4:
			case 0x5:
			case 0x6:
				DBPRINT(sc, BNX_INFO_LOAD,
					"BCM5709 s/w configured for SerDes.\n");
				sc->bnx_phy_flags |= BNX_PHY_SERDES_FLAG;
				break;
			default:
				DBPRINT(sc, BNX_INFO_LOAD,
					"BCM5709 s/w configured for Copper.\n");
			}
		} else {
			switch (strap) {
			case 0x1:
			case 0x2:
			case 0x4:
				DBPRINT(sc, BNX_INFO_LOAD,
					"BCM5709 s/w configured for SerDes.\n");
				sc->bnx_phy_flags |= BNX_PHY_SERDES_FLAG;
				break;
			default:
				DBPRINT(sc, BNX_INFO_LOAD,
					"BCM5709 s/w configured for Copper.\n");
			}
		}

	} else if (BNX_CHIP_BOND_ID(sc) & BNX_CHIP_BOND_ID_SERDES_BIT)
		sc->bnx_phy_flags |= BNX_PHY_SERDES_FLAG;

	if (sc->bnx_phy_flags & BNX_PHY_SERDES_FLAG) {
		uint32_t val;

		sc->bnx_flags |= BNX_NO_WOL_FLAG;

		if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709)
			sc->bnx_phy_flags |= BNX_PHY_IEEE_CLAUSE_45_FLAG;

		/*
		 * The BCM5708S, BCM5709S, and BCM5716S controllers use a
		 * separate PHY for SerDes.
		 */
		if (BNX_CHIP_NUM(sc) != BNX_CHIP_NUM_5706) {
			sc->bnx_phy_addr = 2;
			val = REG_RD_IND(sc, sc->bnx_shmem_base +
				 BNX_SHARED_HW_CFG_CONFIG);
			if (val & BNX_SHARED_HW_CFG_PHY_2_5G) {
				sc->bnx_phy_flags |= BNX_PHY_2_5G_CAPABLE_FLAG;
				DBPRINT(sc, BNX_INFO_LOAD,
				    "Found 2.5Gb capable adapter\n");
			}
		}
	} else if ((BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5706) ||
		   (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5708))
		sc->bnx_phy_flags |= BNX_PHY_CRC_FIX_FLAG;

bnx_get_media_exit:
	DBPRINT(sc, (BNX_INFO_LOAD),
		"Using PHY address %d.\n", sc->bnx_phy_addr);
}

/****************************************************************************/
/* Performs PHY initialization required before MII drivers access the       */
/* device.                                                                  */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_init_media(struct bnx_softc *sc)
{
	if (sc->bnx_phy_flags & BNX_PHY_IEEE_CLAUSE_45_FLAG) {
		/*
		 * Configure the BCM5709S / BCM5716S PHYs to use traditional
		 * IEEE Clause 22 method. Otherwise we have no way to attach
		 * the PHY to the mii(4) layer. PHY specific configuration
		 * is done by the mii(4) layer.
		 */

		/* Select auto-negotiation MMD of the PHY. */
		bnx_miibus_write_reg(sc->bnx_dev, sc->bnx_phy_addr,
		    BRGPHY_BLOCK_ADDR, BRGPHY_BLOCK_ADDR_ADDR_EXT);

		bnx_miibus_write_reg(sc->bnx_dev, sc->bnx_phy_addr,
		    BRGPHY_ADDR_EXT, BRGPHY_ADDR_EXT_AN_MMD);

		bnx_miibus_write_reg(sc->bnx_dev, sc->bnx_phy_addr,
		    BRGPHY_BLOCK_ADDR, BRGPHY_BLOCK_ADDR_COMBO_IEEE0);
	}
}

/****************************************************************************/
/* Free any DMA memory owned by the driver.                                 */
/*                                                                          */
/* Scans through each data structre that requires DMA memory and frees      */
/* the memory if allocated.                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_dma_free(struct bnx_softc *sc)
{
	int			i;

	DBPRINT(sc,BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Destroy the status block. */
	if (sc->status_block != NULL && sc->status_map != NULL) {
		bus_dmamap_unload(sc->bnx_dmatag, sc->status_map);
		bus_dmamem_unmap(sc->bnx_dmatag, (void *)sc->status_block,
		    BNX_STATUS_BLK_SZ);
		bus_dmamem_free(sc->bnx_dmatag, &sc->status_seg,
		    sc->status_rseg);
		bus_dmamap_destroy(sc->bnx_dmatag, sc->status_map);
		sc->status_block = NULL;
		sc->status_map = NULL;
	}

	/* Destroy the statistics block. */
	if (sc->stats_block != NULL && sc->stats_map != NULL) {
		bus_dmamap_unload(sc->bnx_dmatag, sc->stats_map);
		bus_dmamem_unmap(sc->bnx_dmatag, (void *)sc->stats_block,
		    BNX_STATS_BLK_SZ);
		bus_dmamem_free(sc->bnx_dmatag, &sc->stats_seg,
		    sc->stats_rseg);
		bus_dmamap_destroy(sc->bnx_dmatag, sc->stats_map);
		sc->stats_block = NULL;
		sc->stats_map = NULL;
	}

	/* Free, unmap and destroy all context memory pages. */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		for (i = 0; i < sc->ctx_pages; i++) {
			if (sc->ctx_block[i] != NULL) {
				bus_dmamap_unload(sc->bnx_dmatag,
				    sc->ctx_map[i]);
				bus_dmamem_unmap(sc->bnx_dmatag,
				    (void *)sc->ctx_block[i],
				    BCM_PAGE_SIZE);
				bus_dmamem_free(sc->bnx_dmatag,
				    &sc->ctx_segs[i], sc->ctx_rsegs[i]);
				bus_dmamap_destroy(sc->bnx_dmatag,
				    sc->ctx_map[i]);
				sc->ctx_block[i] = NULL;
			}
		}
	}

	/* Free, unmap and destroy all TX buffer descriptor chain pages. */
	for (i = 0; i < TX_PAGES; i++ ) {
		if (sc->tx_bd_chain[i] != NULL &&
		    sc->tx_bd_chain_map[i] != NULL) {
			bus_dmamap_unload(sc->bnx_dmatag,
			    sc->tx_bd_chain_map[i]);
			bus_dmamem_unmap(sc->bnx_dmatag,
			    (void *)sc->tx_bd_chain[i], BNX_TX_CHAIN_PAGE_SZ);
			bus_dmamem_free(sc->bnx_dmatag, &sc->tx_bd_chain_seg[i],
			    sc->tx_bd_chain_rseg[i]);
			bus_dmamap_destroy(sc->bnx_dmatag,
			    sc->tx_bd_chain_map[i]);
			sc->tx_bd_chain[i] = NULL;
			sc->tx_bd_chain_map[i] = NULL;
		}
	}

	/* Destroy the TX dmamaps. */
	/* This isn't necessary since we dont allocate them up front */

	/* Free, unmap and destroy all RX buffer descriptor chain pages. */
	for (i = 0; i < RX_PAGES; i++ ) {
		if (sc->rx_bd_chain[i] != NULL &&
		    sc->rx_bd_chain_map[i] != NULL) {
			bus_dmamap_unload(sc->bnx_dmatag,
			    sc->rx_bd_chain_map[i]);
			bus_dmamem_unmap(sc->bnx_dmatag,
			    (void *)sc->rx_bd_chain[i], BNX_RX_CHAIN_PAGE_SZ);
			bus_dmamem_free(sc->bnx_dmatag, &sc->rx_bd_chain_seg[i],
			    sc->rx_bd_chain_rseg[i]);

			bus_dmamap_destroy(sc->bnx_dmatag,
			    sc->rx_bd_chain_map[i]);
			sc->rx_bd_chain[i] = NULL;
			sc->rx_bd_chain_map[i] = NULL;
		}
	}

	/* Unload and destroy the RX mbuf maps. */
	for (i = 0; i < TOTAL_RX_BD; i++) {
		if (sc->rx_mbuf_map[i] != NULL) {
			bus_dmamap_unload(sc->bnx_dmatag, sc->rx_mbuf_map[i]);
			bus_dmamap_destroy(sc->bnx_dmatag, sc->rx_mbuf_map[i]);
		}
	}

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);
}

/****************************************************************************/
/* Allocate any DMA memory needed by the driver.                            */
/*                                                                          */
/* Allocates DMA memory needed for the various global structures needed by  */
/* hardware.                                                                */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_dma_alloc(struct bnx_softc *sc)
{
	int			i, rc = 0;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/*
	 * Allocate DMA memory for the status block, map the memory into DMA
	 * space, and fetch the physical address of the block.
	 */
	if (bus_dmamap_create(sc->bnx_dmatag, BNX_STATUS_BLK_SZ, 1,
	    BNX_STATUS_BLK_SZ, 0, BUS_DMA_NOWAIT, &sc->status_map)) {
		aprint_error_dev(sc->bnx_dev,
		    "Could not create status block DMA map!\n");
		rc = ENOMEM;
		goto bnx_dma_alloc_exit;
	}

	if (bus_dmamem_alloc(sc->bnx_dmatag, BNX_STATUS_BLK_SZ,
	    BNX_DMA_ALIGN, BNX_DMA_BOUNDARY, &sc->status_seg, 1,
	    &sc->status_rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bnx_dev,
		    "Could not allocate status block DMA memory!\n");
		rc = ENOMEM;
		goto bnx_dma_alloc_exit;
	}

	if (bus_dmamem_map(sc->bnx_dmatag, &sc->status_seg, sc->status_rseg,
	    BNX_STATUS_BLK_SZ, (void **)&sc->status_block, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bnx_dev,
		    "Could not map status block DMA memory!\n");
		rc = ENOMEM;
		goto bnx_dma_alloc_exit;
	}

	if (bus_dmamap_load(sc->bnx_dmatag, sc->status_map,
	    sc->status_block, BNX_STATUS_BLK_SZ, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bnx_dev,
		    "Could not load status block DMA memory!\n");
		rc = ENOMEM;
		goto bnx_dma_alloc_exit;
	}

	sc->status_block_paddr = sc->status_map->dm_segs[0].ds_addr;
	memset(sc->status_block, 0, BNX_STATUS_BLK_SZ);

	/* DRC - Fix for 64 bit addresses. */
	DBPRINT(sc, BNX_INFO, "status_block_paddr = 0x%08X\n",
		(uint32_t) sc->status_block_paddr);

	/* BCM5709 uses host memory as cache for context memory. */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		sc->ctx_pages = 0x2000 / BCM_PAGE_SIZE;
		if (sc->ctx_pages == 0)
			sc->ctx_pages = 1;
		if (sc->ctx_pages > 4) /* XXX */
			sc->ctx_pages = 4;

		DBRUNIF((sc->ctx_pages > 512),
			BNX_PRINTF(sc, "%s(%d): Too many CTX pages! %d > 512\n",
				__FILE__, __LINE__, sc->ctx_pages));


		for (i = 0; i < sc->ctx_pages; i++) {
			if (bus_dmamap_create(sc->bnx_dmatag, BCM_PAGE_SIZE,
			    1, BCM_PAGE_SIZE, BNX_DMA_BOUNDARY,
			    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
			    &sc->ctx_map[i]) != 0) {
				rc = ENOMEM;
				goto bnx_dma_alloc_exit;
			}

			if (bus_dmamem_alloc(sc->bnx_dmatag, BCM_PAGE_SIZE,
			    BCM_PAGE_SIZE, BNX_DMA_BOUNDARY, &sc->ctx_segs[i],
			    1, &sc->ctx_rsegs[i], BUS_DMA_NOWAIT) != 0) {
				rc = ENOMEM;
				goto bnx_dma_alloc_exit;
			}

			if (bus_dmamem_map(sc->bnx_dmatag, &sc->ctx_segs[i],
			    sc->ctx_rsegs[i], BCM_PAGE_SIZE,
			    &sc->ctx_block[i], BUS_DMA_NOWAIT) != 0) {
				rc = ENOMEM;
				goto bnx_dma_alloc_exit;
			}

			if (bus_dmamap_load(sc->bnx_dmatag, sc->ctx_map[i],
			    sc->ctx_block[i], BCM_PAGE_SIZE, NULL,
			    BUS_DMA_NOWAIT) != 0) {
				rc = ENOMEM;
				goto bnx_dma_alloc_exit;
			}

			bzero(sc->ctx_block[i], BCM_PAGE_SIZE);
		}
	}

	/*
	 * Allocate DMA memory for the statistics block, map the memory into
	 * DMA space, and fetch the physical address of the block.
	 */
	if (bus_dmamap_create(sc->bnx_dmatag, BNX_STATS_BLK_SZ, 1,
	    BNX_STATS_BLK_SZ, 0, BUS_DMA_NOWAIT, &sc->stats_map)) {
		aprint_error_dev(sc->bnx_dev,
		    "Could not create stats block DMA map!\n");
		rc = ENOMEM;
		goto bnx_dma_alloc_exit;
	}

	if (bus_dmamem_alloc(sc->bnx_dmatag, BNX_STATS_BLK_SZ,
	    BNX_DMA_ALIGN, BNX_DMA_BOUNDARY, &sc->stats_seg, 1,
	    &sc->stats_rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bnx_dev,
		    "Could not allocate stats block DMA memory!\n");
		rc = ENOMEM;
		goto bnx_dma_alloc_exit;
	}

	if (bus_dmamem_map(sc->bnx_dmatag, &sc->stats_seg, sc->stats_rseg,
	    BNX_STATS_BLK_SZ, (void **)&sc->stats_block, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bnx_dev,
		    "Could not map stats block DMA memory!\n");
		rc = ENOMEM;
		goto bnx_dma_alloc_exit;
	}

	if (bus_dmamap_load(sc->bnx_dmatag, sc->stats_map,
	    sc->stats_block, BNX_STATS_BLK_SZ, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bnx_dev,
		    "Could not load status block DMA memory!\n");
		rc = ENOMEM;
		goto bnx_dma_alloc_exit;
	}

	sc->stats_block_paddr = sc->stats_map->dm_segs[0].ds_addr;
	memset(sc->stats_block, 0, BNX_STATS_BLK_SZ);

	/* DRC - Fix for 64 bit address. */
	DBPRINT(sc,BNX_INFO, "stats_block_paddr = 0x%08X\n",
	    (uint32_t) sc->stats_block_paddr);

	/*
	 * Allocate DMA memory for the TX buffer descriptor chain,
	 * and fetch the physical address of the block.
	 */
	for (i = 0; i < TX_PAGES; i++) {
		if (bus_dmamap_create(sc->bnx_dmatag, BNX_TX_CHAIN_PAGE_SZ, 1,
		    BNX_TX_CHAIN_PAGE_SZ, 0, BUS_DMA_NOWAIT,
		    &sc->tx_bd_chain_map[i])) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not create Tx desc %d DMA map!\n", i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}

		if (bus_dmamem_alloc(sc->bnx_dmatag, BNX_TX_CHAIN_PAGE_SZ,
		    BCM_PAGE_SIZE, BNX_DMA_BOUNDARY, &sc->tx_bd_chain_seg[i], 1,
		    &sc->tx_bd_chain_rseg[i], BUS_DMA_NOWAIT)) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not allocate TX desc %d DMA memory!\n",
			    i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}

		if (bus_dmamem_map(sc->bnx_dmatag, &sc->tx_bd_chain_seg[i],
		    sc->tx_bd_chain_rseg[i], BNX_TX_CHAIN_PAGE_SZ,
		    (void **)&sc->tx_bd_chain[i], BUS_DMA_NOWAIT)) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not map TX desc %d DMA memory!\n", i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}

		if (bus_dmamap_load(sc->bnx_dmatag, sc->tx_bd_chain_map[i],
		    (void *)sc->tx_bd_chain[i], BNX_TX_CHAIN_PAGE_SZ, NULL,
		    BUS_DMA_NOWAIT)) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not load TX desc %d DMA memory!\n", i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}

		sc->tx_bd_chain_paddr[i] =
		    sc->tx_bd_chain_map[i]->dm_segs[0].ds_addr;

		/* DRC - Fix for 64 bit systems. */
		DBPRINT(sc, BNX_INFO, "tx_bd_chain_paddr[%d] = 0x%08X\n",
		    i, (uint32_t) sc->tx_bd_chain_paddr[i]);
	}

	/*
	 * Create lists to hold TX mbufs.
	 */
	TAILQ_INIT(&sc->tx_free_pkts);
	TAILQ_INIT(&sc->tx_used_pkts);
	sc->tx_pkt_count = 0;
	mutex_init(&sc->tx_pkt_mtx, MUTEX_DEFAULT, IPL_NET);

	/*
	 * Allocate DMA memory for the Rx buffer descriptor chain,
	 * and fetch the physical address of the block.
	 */
	for (i = 0; i < RX_PAGES; i++) {
		if (bus_dmamap_create(sc->bnx_dmatag, BNX_RX_CHAIN_PAGE_SZ, 1,
		    BNX_RX_CHAIN_PAGE_SZ, 0, BUS_DMA_NOWAIT,
		    &sc->rx_bd_chain_map[i])) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not create Rx desc %d DMA map!\n", i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}

		if (bus_dmamem_alloc(sc->bnx_dmatag, BNX_RX_CHAIN_PAGE_SZ,
		    BCM_PAGE_SIZE, BNX_DMA_BOUNDARY, &sc->rx_bd_chain_seg[i], 1,
		    &sc->rx_bd_chain_rseg[i], BUS_DMA_NOWAIT)) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not allocate Rx desc %d DMA memory!\n", i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}

		if (bus_dmamem_map(sc->bnx_dmatag, &sc->rx_bd_chain_seg[i],
		    sc->rx_bd_chain_rseg[i], BNX_RX_CHAIN_PAGE_SZ,
		    (void **)&sc->rx_bd_chain[i], BUS_DMA_NOWAIT)) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not map Rx desc %d DMA memory!\n", i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}

		if (bus_dmamap_load(sc->bnx_dmatag, sc->rx_bd_chain_map[i],
		    (void *)sc->rx_bd_chain[i], BNX_RX_CHAIN_PAGE_SZ, NULL,
		    BUS_DMA_NOWAIT)) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not load Rx desc %d DMA memory!\n", i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}

		memset(sc->rx_bd_chain[i], 0, BNX_RX_CHAIN_PAGE_SZ);
		sc->rx_bd_chain_paddr[i] =
		    sc->rx_bd_chain_map[i]->dm_segs[0].ds_addr;

		/* DRC - Fix for 64 bit systems. */
		DBPRINT(sc, BNX_INFO, "rx_bd_chain_paddr[%d] = 0x%08X\n",
		    i, (uint32_t) sc->rx_bd_chain_paddr[i]);
		bus_dmamap_sync(sc->bnx_dmatag, sc->rx_bd_chain_map[i],
		    0, BNX_RX_CHAIN_PAGE_SZ,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}

	/*
	 * Create DMA maps for the Rx buffer mbufs.
	 */
	for (i = 0; i < TOTAL_RX_BD; i++) {
		if (bus_dmamap_create(sc->bnx_dmatag, BNX_MAX_JUMBO_MRU,
		    BNX_MAX_SEGMENTS, BNX_MAX_JUMBO_MRU, 0, BUS_DMA_NOWAIT,
		    &sc->rx_mbuf_map[i])) {
			aprint_error_dev(sc->bnx_dev,
			    "Could not create Rx mbuf %d DMA map!\n", i);
			rc = ENOMEM;
			goto bnx_dma_alloc_exit;
		}
	}

 bnx_dma_alloc_exit:
	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	return rc;
}

/****************************************************************************/
/* Release all resources used by the driver.                                */
/*                                                                          */
/* Releases all resources acquired by the driver including interrupts,      */
/* interrupt handler, interfaces, mutexes, and DMA memory.                  */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_release_resources(struct bnx_softc *sc)
{
	struct pci_attach_args	*pa = &(sc->bnx_pa);

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	bnx_dma_free(sc);

	if (sc->bnx_intrhand != NULL)
		pci_intr_disestablish(pa->pa_pc, sc->bnx_intrhand);

	if (sc->bnx_size)
		bus_space_unmap(sc->bnx_btag, sc->bnx_bhandle, sc->bnx_size);

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);
}

/****************************************************************************/
/* Firmware synchronization.                                                */
/*                                                                          */
/* Before performing certain events such as a chip reset, synchronize with  */
/* the firmware first.                                                      */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_fw_sync(struct bnx_softc *sc, uint32_t msg_data)
{
	int			i, rc = 0;
	uint32_t		val;

	/* Don't waste any time if we've timed out before. */
	if (sc->bnx_fw_timed_out) {
		rc = EBUSY;
		goto bnx_fw_sync_exit;
	}

	/* Increment the message sequence number. */
	sc->bnx_fw_wr_seq++;
	msg_data |= sc->bnx_fw_wr_seq;

 	DBPRINT(sc, BNX_VERBOSE, "bnx_fw_sync(): msg_data = 0x%08X\n",
	    msg_data);

	/* Send the message to the bootcode driver mailbox. */
	REG_WR_IND(sc, sc->bnx_shmem_base + BNX_DRV_MB, msg_data);

	/* Wait for the bootcode to acknowledge the message. */
	for (i = 0; i < FW_ACK_TIME_OUT_MS; i++) {
		/* Check for a response in the bootcode firmware mailbox. */
		val = REG_RD_IND(sc, sc->bnx_shmem_base + BNX_FW_MB);
		if ((val & BNX_FW_MSG_ACK) == (msg_data & BNX_DRV_MSG_SEQ))
			break;
		DELAY(1000);
	}

	/* If we've timed out, tell the bootcode that we've stopped waiting. */
	if (((val & BNX_FW_MSG_ACK) != (msg_data & BNX_DRV_MSG_SEQ)) &&
		((msg_data & BNX_DRV_MSG_DATA) != BNX_DRV_MSG_DATA_WAIT0)) {
		BNX_PRINTF(sc, "%s(%d): Firmware synchronization timeout! "
		    "msg_data = 0x%08X\n", __FILE__, __LINE__, msg_data);

		msg_data &= ~BNX_DRV_MSG_CODE;
		msg_data |= BNX_DRV_MSG_CODE_FW_TIMEOUT;

		REG_WR_IND(sc, sc->bnx_shmem_base + BNX_DRV_MB, msg_data);

		sc->bnx_fw_timed_out = 1;
		rc = EBUSY;
	}

bnx_fw_sync_exit:
	return rc;
}

/****************************************************************************/
/* Load Receive Virtual 2 Physical (RV2P) processor firmware.               */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_load_rv2p_fw(struct bnx_softc *sc, uint32_t *rv2p_code,
    uint32_t rv2p_code_len, uint32_t rv2p_proc)
{
	int			i;
	uint32_t		val;

	/* Set the page size used by RV2P. */
	if (rv2p_proc == RV2P_PROC2) {
		BNX_RV2P_PROC2_CHG_MAX_BD_PAGE(rv2p_code,
		    USABLE_RX_BD_PER_PAGE);
	}

	for (i = 0; i < rv2p_code_len; i += 8) {
		REG_WR(sc, BNX_RV2P_INSTR_HIGH, *rv2p_code);
		rv2p_code++;
		REG_WR(sc, BNX_RV2P_INSTR_LOW, *rv2p_code);
		rv2p_code++;

		if (rv2p_proc == RV2P_PROC1) {
			val = (i / 8) | BNX_RV2P_PROC1_ADDR_CMD_RDWR;
			REG_WR(sc, BNX_RV2P_PROC1_ADDR_CMD, val);
		} else {
			val = (i / 8) | BNX_RV2P_PROC2_ADDR_CMD_RDWR;
			REG_WR(sc, BNX_RV2P_PROC2_ADDR_CMD, val);
		}
	}

	/* Reset the processor, un-stall is done later. */
	if (rv2p_proc == RV2P_PROC1)
		REG_WR(sc, BNX_RV2P_COMMAND, BNX_RV2P_COMMAND_PROC1_RESET);
	else
		REG_WR(sc, BNX_RV2P_COMMAND, BNX_RV2P_COMMAND_PROC2_RESET);
}

/****************************************************************************/
/* Load RISC processor firmware.                                            */
/*                                                                          */
/* Loads firmware from the file if_bnxfw.h into the scratchpad memory       */
/* associated with a particular processor.                                  */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_load_cpu_fw(struct bnx_softc *sc, struct cpu_reg *cpu_reg,
    struct fw_info *fw)
{
	uint32_t		offset;
	uint32_t		val;

	/* Halt the CPU. */
	val = REG_RD_IND(sc, cpu_reg->mode);
	val |= cpu_reg->mode_value_halt;
	REG_WR_IND(sc, cpu_reg->mode, val);
	REG_WR_IND(sc, cpu_reg->state, cpu_reg->state_value_clear);

	/* Load the Text area. */
	offset = cpu_reg->spad_base + (fw->text_addr - cpu_reg->mips_view_base);
	if (fw->text) {
		int j;

		for (j = 0; j < (fw->text_len / 4); j++, offset += 4)
			REG_WR_IND(sc, offset, fw->text[j]);
	}

	/* Load the Data area. */
	offset = cpu_reg->spad_base + (fw->data_addr - cpu_reg->mips_view_base);
	if (fw->data) {
		int j;

		for (j = 0; j < (fw->data_len / 4); j++, offset += 4)
			REG_WR_IND(sc, offset, fw->data[j]);
	}

	/* Load the SBSS area. */
	offset = cpu_reg->spad_base + (fw->sbss_addr - cpu_reg->mips_view_base);
	if (fw->sbss) {
		int j;

		for (j = 0; j < (fw->sbss_len / 4); j++, offset += 4)
			REG_WR_IND(sc, offset, fw->sbss[j]);
	}

	/* Load the BSS area. */
	offset = cpu_reg->spad_base + (fw->bss_addr - cpu_reg->mips_view_base);
	if (fw->bss) {
		int j;

		for (j = 0; j < (fw->bss_len/4); j++, offset += 4)
			REG_WR_IND(sc, offset, fw->bss[j]);
	}

	/* Load the Read-Only area. */
	offset = cpu_reg->spad_base +
	    (fw->rodata_addr - cpu_reg->mips_view_base);
	if (fw->rodata) {
		int j;

		for (j = 0; j < (fw->rodata_len / 4); j++, offset += 4)
			REG_WR_IND(sc, offset, fw->rodata[j]);
	}

	/* Clear the pre-fetch instruction. */
	REG_WR_IND(sc, cpu_reg->inst, 0);
	REG_WR_IND(sc, cpu_reg->pc, fw->start_addr);

	/* Start the CPU. */
	val = REG_RD_IND(sc, cpu_reg->mode);
	val &= ~cpu_reg->mode_value_halt;
	REG_WR_IND(sc, cpu_reg->state, cpu_reg->state_value_clear);
	REG_WR_IND(sc, cpu_reg->mode, val);
}

/****************************************************************************/
/* Initialize the RV2P, RX, TX, TPAT, and COM CPUs.                         */
/*                                                                          */
/* Loads the firmware for each CPU and starts the CPU.                      */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_init_cpus(struct bnx_softc *sc)
{
	struct cpu_reg cpu_reg;
	struct fw_info fw;

	switch(BNX_CHIP_NUM(sc)) {
	case BNX_CHIP_NUM_5709:
		/* Initialize the RV2P processor. */
		if (BNX_CHIP_REV(sc) == BNX_CHIP_REV_Ax) {
			bnx_load_rv2p_fw(sc, bnx_xi90_rv2p_proc1,
			    sizeof(bnx_xi90_rv2p_proc1), RV2P_PROC1);
			bnx_load_rv2p_fw(sc, bnx_xi90_rv2p_proc2,
			    sizeof(bnx_xi90_rv2p_proc2), RV2P_PROC2);
		} else {
			bnx_load_rv2p_fw(sc, bnx_xi_rv2p_proc1,
			    sizeof(bnx_xi_rv2p_proc1), RV2P_PROC1);
			bnx_load_rv2p_fw(sc, bnx_xi_rv2p_proc2,
			    sizeof(bnx_xi_rv2p_proc2), RV2P_PROC2);
		}

		/* Initialize the RX Processor. */
		cpu_reg.mode = BNX_RXP_CPU_MODE;
		cpu_reg.mode_value_halt = BNX_RXP_CPU_MODE_SOFT_HALT;
		cpu_reg.mode_value_sstep = BNX_RXP_CPU_MODE_STEP_ENA;
		cpu_reg.state = BNX_RXP_CPU_STATE;
		cpu_reg.state_value_clear = 0xffffff;
		cpu_reg.gpr0 = BNX_RXP_CPU_REG_FILE;
		cpu_reg.evmask = BNX_RXP_CPU_EVENT_MASK;
		cpu_reg.pc = BNX_RXP_CPU_PROGRAM_COUNTER;
		cpu_reg.inst = BNX_RXP_CPU_INSTRUCTION;
		cpu_reg.bp = BNX_RXP_CPU_HW_BREAKPOINT;
		cpu_reg.spad_base = BNX_RXP_SCRATCH;
		cpu_reg.mips_view_base = 0x8000000;

		fw.ver_major = bnx_RXP_b09FwReleaseMajor;
		fw.ver_minor = bnx_RXP_b09FwReleaseMinor;
		fw.ver_fix = bnx_RXP_b09FwReleaseFix;
		fw.start_addr = bnx_RXP_b09FwStartAddr;

		fw.text_addr = bnx_RXP_b09FwTextAddr;
		fw.text_len = bnx_RXP_b09FwTextLen;
		fw.text_index = 0;
		fw.text = bnx_RXP_b09FwText;

		fw.data_addr = bnx_RXP_b09FwDataAddr;
		fw.data_len = bnx_RXP_b09FwDataLen;
		fw.data_index = 0;
		fw.data = bnx_RXP_b09FwData;

		fw.sbss_addr = bnx_RXP_b09FwSbssAddr;
		fw.sbss_len = bnx_RXP_b09FwSbssLen;
		fw.sbss_index = 0;
		fw.sbss = bnx_RXP_b09FwSbss;

		fw.bss_addr = bnx_RXP_b09FwBssAddr;
		fw.bss_len = bnx_RXP_b09FwBssLen;
		fw.bss_index = 0;
		fw.bss = bnx_RXP_b09FwBss;

		fw.rodata_addr = bnx_RXP_b09FwRodataAddr;
		fw.rodata_len = bnx_RXP_b09FwRodataLen;
		fw.rodata_index = 0;
		fw.rodata = bnx_RXP_b09FwRodata;

		DBPRINT(sc, BNX_INFO_RESET, "Loading RX firmware.\n");
		bnx_load_cpu_fw(sc, &cpu_reg, &fw);

		/* Initialize the TX Processor. */
		cpu_reg.mode = BNX_TXP_CPU_MODE;
		cpu_reg.mode_value_halt = BNX_TXP_CPU_MODE_SOFT_HALT;
		cpu_reg.mode_value_sstep = BNX_TXP_CPU_MODE_STEP_ENA;
		cpu_reg.state = BNX_TXP_CPU_STATE;
		cpu_reg.state_value_clear = 0xffffff;
		cpu_reg.gpr0 = BNX_TXP_CPU_REG_FILE;
		cpu_reg.evmask = BNX_TXP_CPU_EVENT_MASK;
		cpu_reg.pc = BNX_TXP_CPU_PROGRAM_COUNTER;
		cpu_reg.inst = BNX_TXP_CPU_INSTRUCTION;
		cpu_reg.bp = BNX_TXP_CPU_HW_BREAKPOINT;
		cpu_reg.spad_base = BNX_TXP_SCRATCH;
		cpu_reg.mips_view_base = 0x8000000;

		fw.ver_major = bnx_TXP_b09FwReleaseMajor;
		fw.ver_minor = bnx_TXP_b09FwReleaseMinor;
		fw.ver_fix = bnx_TXP_b09FwReleaseFix;
		fw.start_addr = bnx_TXP_b09FwStartAddr;

		fw.text_addr = bnx_TXP_b09FwTextAddr;
		fw.text_len = bnx_TXP_b09FwTextLen;
		fw.text_index = 0;
		fw.text = bnx_TXP_b09FwText;

		fw.data_addr = bnx_TXP_b09FwDataAddr;
		fw.data_len = bnx_TXP_b09FwDataLen;
		fw.data_index = 0;
		fw.data = bnx_TXP_b09FwData;

		fw.sbss_addr = bnx_TXP_b09FwSbssAddr;
		fw.sbss_len = bnx_TXP_b09FwSbssLen;
		fw.sbss_index = 0;
		fw.sbss = bnx_TXP_b09FwSbss;

		fw.bss_addr = bnx_TXP_b09FwBssAddr;
		fw.bss_len = bnx_TXP_b09FwBssLen;
		fw.bss_index = 0;
		fw.bss = bnx_TXP_b09FwBss;

		fw.rodata_addr = bnx_TXP_b09FwRodataAddr;
		fw.rodata_len = bnx_TXP_b09FwRodataLen;
		fw.rodata_index = 0;
		fw.rodata = bnx_TXP_b09FwRodata;

		DBPRINT(sc, BNX_INFO_RESET, "Loading TX firmware.\n");
		bnx_load_cpu_fw(sc, &cpu_reg, &fw);

		/* Initialize the TX Patch-up Processor. */
		cpu_reg.mode = BNX_TPAT_CPU_MODE;
		cpu_reg.mode_value_halt = BNX_TPAT_CPU_MODE_SOFT_HALT;
		cpu_reg.mode_value_sstep = BNX_TPAT_CPU_MODE_STEP_ENA;
		cpu_reg.state = BNX_TPAT_CPU_STATE;
		cpu_reg.state_value_clear = 0xffffff;
		cpu_reg.gpr0 = BNX_TPAT_CPU_REG_FILE;
		cpu_reg.evmask = BNX_TPAT_CPU_EVENT_MASK;
		cpu_reg.pc = BNX_TPAT_CPU_PROGRAM_COUNTER;
		cpu_reg.inst = BNX_TPAT_CPU_INSTRUCTION;
		cpu_reg.bp = BNX_TPAT_CPU_HW_BREAKPOINT;
		cpu_reg.spad_base = BNX_TPAT_SCRATCH;
		cpu_reg.mips_view_base = 0x8000000;

		fw.ver_major = bnx_TPAT_b09FwReleaseMajor;
		fw.ver_minor = bnx_TPAT_b09FwReleaseMinor;
		fw.ver_fix = bnx_TPAT_b09FwReleaseFix;
		fw.start_addr = bnx_TPAT_b09FwStartAddr;

		fw.text_addr = bnx_TPAT_b09FwTextAddr;
		fw.text_len = bnx_TPAT_b09FwTextLen;
		fw.text_index = 0;
		fw.text = bnx_TPAT_b09FwText;

		fw.data_addr = bnx_TPAT_b09FwDataAddr;
		fw.data_len = bnx_TPAT_b09FwDataLen;
		fw.data_index = 0;
		fw.data = bnx_TPAT_b09FwData;

		fw.sbss_addr = bnx_TPAT_b09FwSbssAddr;
		fw.sbss_len = bnx_TPAT_b09FwSbssLen;
		fw.sbss_index = 0;
		fw.sbss = bnx_TPAT_b09FwSbss;

		fw.bss_addr = bnx_TPAT_b09FwBssAddr;
		fw.bss_len = bnx_TPAT_b09FwBssLen;
		fw.bss_index = 0;
		fw.bss = bnx_TPAT_b09FwBss;

		fw.rodata_addr = bnx_TPAT_b09FwRodataAddr;
		fw.rodata_len = bnx_TPAT_b09FwRodataLen;
		fw.rodata_index = 0;
		fw.rodata = bnx_TPAT_b09FwRodata;

		DBPRINT(sc, BNX_INFO_RESET, "Loading TPAT firmware.\n");
		bnx_load_cpu_fw(sc, &cpu_reg, &fw);

		/* Initialize the Completion Processor. */
		cpu_reg.mode = BNX_COM_CPU_MODE;
		cpu_reg.mode_value_halt = BNX_COM_CPU_MODE_SOFT_HALT;
		cpu_reg.mode_value_sstep = BNX_COM_CPU_MODE_STEP_ENA;
		cpu_reg.state = BNX_COM_CPU_STATE;
		cpu_reg.state_value_clear = 0xffffff;
		cpu_reg.gpr0 = BNX_COM_CPU_REG_FILE;
		cpu_reg.evmask = BNX_COM_CPU_EVENT_MASK;
		cpu_reg.pc = BNX_COM_CPU_PROGRAM_COUNTER;
		cpu_reg.inst = BNX_COM_CPU_INSTRUCTION;
		cpu_reg.bp = BNX_COM_CPU_HW_BREAKPOINT;
		cpu_reg.spad_base = BNX_COM_SCRATCH;
		cpu_reg.mips_view_base = 0x8000000;

		fw.ver_major = bnx_COM_b09FwReleaseMajor;
		fw.ver_minor = bnx_COM_b09FwReleaseMinor;
		fw.ver_fix = bnx_COM_b09FwReleaseFix;
		fw.start_addr = bnx_COM_b09FwStartAddr;

		fw.text_addr = bnx_COM_b09FwTextAddr;
		fw.text_len = bnx_COM_b09FwTextLen;
		fw.text_index = 0;
		fw.text = bnx_COM_b09FwText;

		fw.data_addr = bnx_COM_b09FwDataAddr;
		fw.data_len = bnx_COM_b09FwDataLen;
		fw.data_index = 0;
		fw.data = bnx_COM_b09FwData;

		fw.sbss_addr = bnx_COM_b09FwSbssAddr;
		fw.sbss_len = bnx_COM_b09FwSbssLen;
		fw.sbss_index = 0;
		fw.sbss = bnx_COM_b09FwSbss;

		fw.bss_addr = bnx_COM_b09FwBssAddr;
		fw.bss_len = bnx_COM_b09FwBssLen;
		fw.bss_index = 0;
		fw.bss = bnx_COM_b09FwBss;

		fw.rodata_addr = bnx_COM_b09FwRodataAddr;
		fw.rodata_len = bnx_COM_b09FwRodataLen;
		fw.rodata_index = 0;
		fw.rodata = bnx_COM_b09FwRodata;
		DBPRINT(sc, BNX_INFO_RESET, "Loading COM firmware.\n");
		bnx_load_cpu_fw(sc, &cpu_reg, &fw);
		break;
	default:
		/* Initialize the RV2P processor. */
		bnx_load_rv2p_fw(sc, bnx_rv2p_proc1, sizeof(bnx_rv2p_proc1),
		    RV2P_PROC1);
		bnx_load_rv2p_fw(sc, bnx_rv2p_proc2, sizeof(bnx_rv2p_proc2),
		    RV2P_PROC2);

		/* Initialize the RX Processor. */
		cpu_reg.mode = BNX_RXP_CPU_MODE;
		cpu_reg.mode_value_halt = BNX_RXP_CPU_MODE_SOFT_HALT;
		cpu_reg.mode_value_sstep = BNX_RXP_CPU_MODE_STEP_ENA;
		cpu_reg.state = BNX_RXP_CPU_STATE;
		cpu_reg.state_value_clear = 0xffffff;
		cpu_reg.gpr0 = BNX_RXP_CPU_REG_FILE;
		cpu_reg.evmask = BNX_RXP_CPU_EVENT_MASK;
		cpu_reg.pc = BNX_RXP_CPU_PROGRAM_COUNTER;
		cpu_reg.inst = BNX_RXP_CPU_INSTRUCTION;
		cpu_reg.bp = BNX_RXP_CPU_HW_BREAKPOINT;
		cpu_reg.spad_base = BNX_RXP_SCRATCH;
		cpu_reg.mips_view_base = 0x8000000;

		fw.ver_major = bnx_RXP_b06FwReleaseMajor;
		fw.ver_minor = bnx_RXP_b06FwReleaseMinor;
		fw.ver_fix = bnx_RXP_b06FwReleaseFix;
		fw.start_addr = bnx_RXP_b06FwStartAddr;

		fw.text_addr = bnx_RXP_b06FwTextAddr;
		fw.text_len = bnx_RXP_b06FwTextLen;
		fw.text_index = 0;
		fw.text = bnx_RXP_b06FwText;

		fw.data_addr = bnx_RXP_b06FwDataAddr;
		fw.data_len = bnx_RXP_b06FwDataLen;
		fw.data_index = 0;
		fw.data = bnx_RXP_b06FwData;

		fw.sbss_addr = bnx_RXP_b06FwSbssAddr;
		fw.sbss_len = bnx_RXP_b06FwSbssLen;
		fw.sbss_index = 0;
		fw.sbss = bnx_RXP_b06FwSbss;

		fw.bss_addr = bnx_RXP_b06FwBssAddr;
		fw.bss_len = bnx_RXP_b06FwBssLen;
		fw.bss_index = 0;
		fw.bss = bnx_RXP_b06FwBss;

		fw.rodata_addr = bnx_RXP_b06FwRodataAddr;
		fw.rodata_len = bnx_RXP_b06FwRodataLen;
		fw.rodata_index = 0;
		fw.rodata = bnx_RXP_b06FwRodata;

		DBPRINT(sc, BNX_INFO_RESET, "Loading RX firmware.\n");
		bnx_load_cpu_fw(sc, &cpu_reg, &fw);

		/* Initialize the TX Processor. */
		cpu_reg.mode = BNX_TXP_CPU_MODE;
		cpu_reg.mode_value_halt = BNX_TXP_CPU_MODE_SOFT_HALT;
		cpu_reg.mode_value_sstep = BNX_TXP_CPU_MODE_STEP_ENA;
		cpu_reg.state = BNX_TXP_CPU_STATE;
		cpu_reg.state_value_clear = 0xffffff;
		cpu_reg.gpr0 = BNX_TXP_CPU_REG_FILE;
		cpu_reg.evmask = BNX_TXP_CPU_EVENT_MASK;
		cpu_reg.pc = BNX_TXP_CPU_PROGRAM_COUNTER;
		cpu_reg.inst = BNX_TXP_CPU_INSTRUCTION;
		cpu_reg.bp = BNX_TXP_CPU_HW_BREAKPOINT;
		cpu_reg.spad_base = BNX_TXP_SCRATCH;
		cpu_reg.mips_view_base = 0x8000000;

		fw.ver_major = bnx_TXP_b06FwReleaseMajor;
		fw.ver_minor = bnx_TXP_b06FwReleaseMinor;
		fw.ver_fix = bnx_TXP_b06FwReleaseFix;
		fw.start_addr = bnx_TXP_b06FwStartAddr;

		fw.text_addr = bnx_TXP_b06FwTextAddr;
		fw.text_len = bnx_TXP_b06FwTextLen;
		fw.text_index = 0;
		fw.text = bnx_TXP_b06FwText;

		fw.data_addr = bnx_TXP_b06FwDataAddr;
		fw.data_len = bnx_TXP_b06FwDataLen;
		fw.data_index = 0;
		fw.data = bnx_TXP_b06FwData;

		fw.sbss_addr = bnx_TXP_b06FwSbssAddr;
		fw.sbss_len = bnx_TXP_b06FwSbssLen;
		fw.sbss_index = 0;
		fw.sbss = bnx_TXP_b06FwSbss;

		fw.bss_addr = bnx_TXP_b06FwBssAddr;
		fw.bss_len = bnx_TXP_b06FwBssLen;
		fw.bss_index = 0;
		fw.bss = bnx_TXP_b06FwBss;

		fw.rodata_addr = bnx_TXP_b06FwRodataAddr;
		fw.rodata_len = bnx_TXP_b06FwRodataLen;
		fw.rodata_index = 0;
		fw.rodata = bnx_TXP_b06FwRodata;

		DBPRINT(sc, BNX_INFO_RESET, "Loading TX firmware.\n");
		bnx_load_cpu_fw(sc, &cpu_reg, &fw);

		/* Initialize the TX Patch-up Processor. */
		cpu_reg.mode = BNX_TPAT_CPU_MODE;
		cpu_reg.mode_value_halt = BNX_TPAT_CPU_MODE_SOFT_HALT;
		cpu_reg.mode_value_sstep = BNX_TPAT_CPU_MODE_STEP_ENA;
		cpu_reg.state = BNX_TPAT_CPU_STATE;
		cpu_reg.state_value_clear = 0xffffff;
		cpu_reg.gpr0 = BNX_TPAT_CPU_REG_FILE;
		cpu_reg.evmask = BNX_TPAT_CPU_EVENT_MASK;
		cpu_reg.pc = BNX_TPAT_CPU_PROGRAM_COUNTER;
		cpu_reg.inst = BNX_TPAT_CPU_INSTRUCTION;
		cpu_reg.bp = BNX_TPAT_CPU_HW_BREAKPOINT;
		cpu_reg.spad_base = BNX_TPAT_SCRATCH;
		cpu_reg.mips_view_base = 0x8000000;

		fw.ver_major = bnx_TPAT_b06FwReleaseMajor;
		fw.ver_minor = bnx_TPAT_b06FwReleaseMinor;
		fw.ver_fix = bnx_TPAT_b06FwReleaseFix;
		fw.start_addr = bnx_TPAT_b06FwStartAddr;

		fw.text_addr = bnx_TPAT_b06FwTextAddr;
		fw.text_len = bnx_TPAT_b06FwTextLen;
		fw.text_index = 0;
		fw.text = bnx_TPAT_b06FwText;

		fw.data_addr = bnx_TPAT_b06FwDataAddr;
		fw.data_len = bnx_TPAT_b06FwDataLen;
		fw.data_index = 0;
		fw.data = bnx_TPAT_b06FwData;

		fw.sbss_addr = bnx_TPAT_b06FwSbssAddr;
		fw.sbss_len = bnx_TPAT_b06FwSbssLen;
		fw.sbss_index = 0;
		fw.sbss = bnx_TPAT_b06FwSbss;

		fw.bss_addr = bnx_TPAT_b06FwBssAddr;
		fw.bss_len = bnx_TPAT_b06FwBssLen;
		fw.bss_index = 0;
		fw.bss = bnx_TPAT_b06FwBss;

		fw.rodata_addr = bnx_TPAT_b06FwRodataAddr;
		fw.rodata_len = bnx_TPAT_b06FwRodataLen;
		fw.rodata_index = 0;
		fw.rodata = bnx_TPAT_b06FwRodata;

		DBPRINT(sc, BNX_INFO_RESET, "Loading TPAT firmware.\n");
		bnx_load_cpu_fw(sc, &cpu_reg, &fw);

		/* Initialize the Completion Processor. */
		cpu_reg.mode = BNX_COM_CPU_MODE;
		cpu_reg.mode_value_halt = BNX_COM_CPU_MODE_SOFT_HALT;
		cpu_reg.mode_value_sstep = BNX_COM_CPU_MODE_STEP_ENA;
		cpu_reg.state = BNX_COM_CPU_STATE;
		cpu_reg.state_value_clear = 0xffffff;
		cpu_reg.gpr0 = BNX_COM_CPU_REG_FILE;
		cpu_reg.evmask = BNX_COM_CPU_EVENT_MASK;
		cpu_reg.pc = BNX_COM_CPU_PROGRAM_COUNTER;
		cpu_reg.inst = BNX_COM_CPU_INSTRUCTION;
		cpu_reg.bp = BNX_COM_CPU_HW_BREAKPOINT;
		cpu_reg.spad_base = BNX_COM_SCRATCH;
		cpu_reg.mips_view_base = 0x8000000;

		fw.ver_major = bnx_COM_b06FwReleaseMajor;
		fw.ver_minor = bnx_COM_b06FwReleaseMinor;
		fw.ver_fix = bnx_COM_b06FwReleaseFix;
		fw.start_addr = bnx_COM_b06FwStartAddr;

		fw.text_addr = bnx_COM_b06FwTextAddr;
		fw.text_len = bnx_COM_b06FwTextLen;
		fw.text_index = 0;
		fw.text = bnx_COM_b06FwText;

		fw.data_addr = bnx_COM_b06FwDataAddr;
		fw.data_len = bnx_COM_b06FwDataLen;
		fw.data_index = 0;
		fw.data = bnx_COM_b06FwData;

		fw.sbss_addr = bnx_COM_b06FwSbssAddr;
		fw.sbss_len = bnx_COM_b06FwSbssLen;
		fw.sbss_index = 0;
		fw.sbss = bnx_COM_b06FwSbss;

		fw.bss_addr = bnx_COM_b06FwBssAddr;
		fw.bss_len = bnx_COM_b06FwBssLen;
		fw.bss_index = 0;
		fw.bss = bnx_COM_b06FwBss;

		fw.rodata_addr = bnx_COM_b06FwRodataAddr;
		fw.rodata_len = bnx_COM_b06FwRodataLen;
		fw.rodata_index = 0;
		fw.rodata = bnx_COM_b06FwRodata;
		DBPRINT(sc, BNX_INFO_RESET, "Loading COM firmware.\n");
		bnx_load_cpu_fw(sc, &cpu_reg, &fw);
		break;
	}
}

/****************************************************************************/
/* Initialize context memory.                                               */
/*                                                                          */
/* Clears the memory associated with each Context ID (CID).                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_init_context(struct bnx_softc *sc)
{
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		/* DRC: Replace this constant value with a #define. */
		int i, retry_cnt = 10;
		uint32_t val;

		/*
		 * BCM5709 context memory may be cached
		 * in host memory so prepare the host memory
		 * for access.
		 */
		val = BNX_CTX_COMMAND_ENABLED | BNX_CTX_COMMAND_MEM_INIT
		    | (1 << 12);
		val |= (BCM_PAGE_BITS - 8) << 16;
		REG_WR(sc, BNX_CTX_COMMAND, val);

		/* Wait for mem init command to complete. */
		for (i = 0; i < retry_cnt; i++) {
			val = REG_RD(sc, BNX_CTX_COMMAND);
			if (!(val & BNX_CTX_COMMAND_MEM_INIT))
				break;
			DELAY(2);
		}

		/* ToDo: Consider returning an error here. */

		for (i = 0; i < sc->ctx_pages; i++) {
			int j;

			/* Set the physaddr of the context memory cache. */
			val = (uint32_t)(sc->ctx_segs[i].ds_addr);
			REG_WR(sc, BNX_CTX_HOST_PAGE_TBL_DATA0, val |
				BNX_CTX_HOST_PAGE_TBL_DATA0_VALID);
			val = (uint32_t)
			    ((uint64_t)sc->ctx_segs[i].ds_addr >> 32);
			REG_WR(sc, BNX_CTX_HOST_PAGE_TBL_DATA1, val);
			REG_WR(sc, BNX_CTX_HOST_PAGE_TBL_CTRL, i |
				BNX_CTX_HOST_PAGE_TBL_CTRL_WRITE_REQ);

			/* Verify that the context memory write was successful. */
			for (j = 0; j < retry_cnt; j++) {
				val = REG_RD(sc, BNX_CTX_HOST_PAGE_TBL_CTRL);
				if ((val & BNX_CTX_HOST_PAGE_TBL_CTRL_WRITE_REQ) == 0)
					break;
				DELAY(5);
			}

			/* ToDo: Consider returning an error here. */
		}
	} else {
		uint32_t vcid_addr, offset;

		/*
		 * For the 5706/5708, context memory is local to
		 * the controller, so initialize the controller
		 * context memory.
		 */

		vcid_addr = GET_CID_ADDR(96);
		while (vcid_addr) {

			vcid_addr -= BNX_PHY_CTX_SIZE;

			REG_WR(sc, BNX_CTX_VIRT_ADDR, 0);
			REG_WR(sc, BNX_CTX_PAGE_TBL, vcid_addr);

			for(offset = 0; offset < BNX_PHY_CTX_SIZE; offset += 4) {
				CTX_WR(sc, 0x00, offset, 0);
			}

			REG_WR(sc, BNX_CTX_VIRT_ADDR, vcid_addr);
			REG_WR(sc, BNX_CTX_PAGE_TBL, vcid_addr);
		}
	}
}

/****************************************************************************/
/* Fetch the permanent MAC address of the controller.                       */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_get_mac_addr(struct bnx_softc *sc)
{
	uint32_t		mac_lo = 0, mac_hi = 0;

	/*
	 * The NetXtreme II bootcode populates various NIC
	 * power-on and runtime configuration items in a
	 * shared memory area.  The factory configured MAC
	 * address is available from both NVRAM and the
	 * shared memory area so we'll read the value from
	 * shared memory for speed.
	 */

	mac_hi = REG_RD_IND(sc, sc->bnx_shmem_base + BNX_PORT_HW_CFG_MAC_UPPER);
	mac_lo = REG_RD_IND(sc, sc->bnx_shmem_base + BNX_PORT_HW_CFG_MAC_LOWER);

	if ((mac_lo == 0) && (mac_hi == 0)) {
		BNX_PRINTF(sc, "%s(%d): Invalid Ethernet address!\n",
		    __FILE__, __LINE__);
	} else {
		sc->eaddr[0] = (u_char)(mac_hi >> 8);
		sc->eaddr[1] = (u_char)(mac_hi >> 0);
		sc->eaddr[2] = (u_char)(mac_lo >> 24);
		sc->eaddr[3] = (u_char)(mac_lo >> 16);
		sc->eaddr[4] = (u_char)(mac_lo >> 8);
		sc->eaddr[5] = (u_char)(mac_lo >> 0);
	}

	DBPRINT(sc, BNX_INFO, "Permanent Ethernet address = "
	    "%s\n", ether_sprintf(sc->eaddr));
}

/****************************************************************************/
/* Program the MAC address.                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_set_mac_addr(struct bnx_softc *sc)
{
	uint32_t		val;
	const uint8_t		*mac_addr = CLLADDR(sc->bnx_ec.ec_if.if_sadl);

	DBPRINT(sc, BNX_INFO, "Setting Ethernet address = "
	    "%s\n", ether_sprintf(sc->eaddr));

	val = (mac_addr[0] << 8) | mac_addr[1];

	REG_WR(sc, BNX_EMAC_MAC_MATCH0, val);

	val = (mac_addr[2] << 24) | (mac_addr[3] << 16) |
		(mac_addr[4] << 8) | mac_addr[5];

	REG_WR(sc, BNX_EMAC_MAC_MATCH1, val);
}

/****************************************************************************/
/* Stop the controller.                                                     */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_stop(struct ifnet *ifp, int disable)
{
	struct bnx_softc *sc = ifp->if_softc;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	callout_stop(&sc->bnx_timeout);

	mii_down(&sc->bnx_mii);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* Disable the transmit/receive blocks. */
	REG_WR(sc, BNX_MISC_ENABLE_CLR_BITS, 0x5ffffff);
	REG_RD(sc, BNX_MISC_ENABLE_CLR_BITS);
	DELAY(20);

	bnx_disable_intr(sc);

	/* Tell firmware that the driver is going away. */
	if (disable)
		bnx_reset(sc, BNX_DRV_MSG_CODE_RESET);
	else
		bnx_reset(sc, BNX_DRV_MSG_CODE_SUSPEND_NO_WOL);

	/* Free RX buffers. */
	bnx_free_rx_chain(sc);

	/* Free TX buffers. */
	bnx_free_tx_chain(sc);

	ifp->if_timer = 0;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

}

int
bnx_reset(struct bnx_softc *sc, uint32_t reset_code)
{
	struct pci_attach_args	*pa = &(sc->bnx_pa);
	uint32_t		val;
	int			i, rc = 0;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Wait for pending PCI transactions to complete. */
	REG_WR(sc, BNX_MISC_ENABLE_CLR_BITS,
	    BNX_MISC_ENABLE_CLR_BITS_TX_DMA_ENABLE |
	    BNX_MISC_ENABLE_CLR_BITS_DMA_ENGINE_ENABLE |
	    BNX_MISC_ENABLE_CLR_BITS_RX_DMA_ENABLE |
	    BNX_MISC_ENABLE_CLR_BITS_HOST_COALESCE_ENABLE);
	val = REG_RD(sc, BNX_MISC_ENABLE_CLR_BITS);
	DELAY(5);

	/* Disable DMA */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		val = REG_RD(sc, BNX_MISC_NEW_CORE_CTL);
		val &= ~BNX_MISC_NEW_CORE_CTL_DMA_ENABLE;
		REG_WR(sc, BNX_MISC_NEW_CORE_CTL, val);
	}

	/* Assume bootcode is running. */
	sc->bnx_fw_timed_out = 0;

	/* Give the firmware a chance to prepare for the reset. */
	rc = bnx_fw_sync(sc, BNX_DRV_MSG_DATA_WAIT0 | reset_code);
	if (rc)
		goto bnx_reset_exit;

	/* Set a firmware reminder that this is a soft reset. */
	REG_WR_IND(sc, sc->bnx_shmem_base + BNX_DRV_RESET_SIGNATURE,
	    BNX_DRV_RESET_SIGNATURE_MAGIC);

	/* Dummy read to force the chip to complete all current transactions. */
	val = REG_RD(sc, BNX_MISC_ID);

	/* Chip reset. */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		REG_WR(sc, BNX_MISC_COMMAND, BNX_MISC_COMMAND_SW_RESET);
		REG_RD(sc, BNX_MISC_COMMAND);
		DELAY(5);

		val = BNX_PCICFG_MISC_CONFIG_REG_WINDOW_ENA |
		      BNX_PCICFG_MISC_CONFIG_TARGET_MB_WORD_SWAP;

		pci_conf_write(pa->pa_pc, pa->pa_tag, BNX_PCICFG_MISC_CONFIG,
		    val);
	} else {
		val = BNX_PCICFG_MISC_CONFIG_CORE_RST_REQ |
			BNX_PCICFG_MISC_CONFIG_REG_WINDOW_ENA |
			BNX_PCICFG_MISC_CONFIG_TARGET_MB_WORD_SWAP;
		REG_WR(sc, BNX_PCICFG_MISC_CONFIG, val);

		/* Allow up to 30us for reset to complete. */
		for (i = 0; i < 10; i++) {
			val = REG_RD(sc, BNX_PCICFG_MISC_CONFIG);
			if ((val & (BNX_PCICFG_MISC_CONFIG_CORE_RST_REQ |
				BNX_PCICFG_MISC_CONFIG_CORE_RST_BSY)) == 0) {
				break;
			}
			DELAY(10);
		}

		/* Check that reset completed successfully. */
		if (val & (BNX_PCICFG_MISC_CONFIG_CORE_RST_REQ |
		    BNX_PCICFG_MISC_CONFIG_CORE_RST_BSY)) {
			BNX_PRINTF(sc, "%s(%d): Reset failed!\n",
			    __FILE__, __LINE__);
			rc = EBUSY;
			goto bnx_reset_exit;
		}
	}

	/* Make sure byte swapping is properly configured. */
	val = REG_RD(sc, BNX_PCI_SWAP_DIAG0);
	if (val != 0x01020304) {
		BNX_PRINTF(sc, "%s(%d): Byte swap is incorrect!\n",
		    __FILE__, __LINE__);
		rc = ENODEV;
		goto bnx_reset_exit;
	}

	/* Just completed a reset, assume that firmware is running again. */
	sc->bnx_fw_timed_out = 0;

	/* Wait for the firmware to finish its initialization. */
	rc = bnx_fw_sync(sc, BNX_DRV_MSG_DATA_WAIT1 | reset_code);
	if (rc)
		BNX_PRINTF(sc, "%s(%d): Firmware did not complete "
		    "initialization!\n", __FILE__, __LINE__);

bnx_reset_exit:
	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	return rc;
}

int
bnx_chipinit(struct bnx_softc *sc)
{
	struct pci_attach_args	*pa = &(sc->bnx_pa);
	uint32_t		val;
	int			rc = 0;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Make sure the interrupt is not active. */
	REG_WR(sc, BNX_PCICFG_INT_ACK_CMD, BNX_PCICFG_INT_ACK_CMD_MASK_INT);

	/* Initialize DMA byte/word swapping, configure the number of DMA  */
	/* channels and PCI clock compensation delay.                      */
	val = BNX_DMA_CONFIG_DATA_BYTE_SWAP |
	    BNX_DMA_CONFIG_DATA_WORD_SWAP |
#if BYTE_ORDER == BIG_ENDIAN
	    BNX_DMA_CONFIG_CNTL_BYTE_SWAP |
#endif
	    BNX_DMA_CONFIG_CNTL_WORD_SWAP |
	    DMA_READ_CHANS << 12 |
	    DMA_WRITE_CHANS << 16;

	val |= (0x2 << 20) | BNX_DMA_CONFIG_CNTL_PCI_COMP_DLY;

	if ((sc->bnx_flags & BNX_PCIX_FLAG) && (sc->bus_speed_mhz == 133))
		val |= BNX_DMA_CONFIG_PCI_FAST_CLK_CMP;

	/*
	 * This setting resolves a problem observed on certain Intel PCI
	 * chipsets that cannot handle multiple outstanding DMA operations.
	 * See errata E9_5706A1_65.
	 */
	if ((BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5706) &&
	    (BNX_CHIP_ID(sc) != BNX_CHIP_ID_5706_A0) &&
	    !(sc->bnx_flags & BNX_PCIX_FLAG))
		val |= BNX_DMA_CONFIG_CNTL_PING_PONG_DMA;

	REG_WR(sc, BNX_DMA_CONFIG, val);

	/* Clear the PCI-X relaxed ordering bit. See errata E3_5708CA0_570. */
	if (sc->bnx_flags & BNX_PCIX_FLAG) {
		val = pci_conf_read(pa->pa_pc, pa->pa_tag, BNX_PCI_PCIX_CMD);
		pci_conf_write(pa->pa_pc, pa->pa_tag, BNX_PCI_PCIX_CMD,
		    val & ~0x20000);
	}

	/* Enable the RX_V2P and Context state machines before access. */
	REG_WR(sc, BNX_MISC_ENABLE_SET_BITS,
	    BNX_MISC_ENABLE_SET_BITS_HOST_COALESCE_ENABLE |
	    BNX_MISC_ENABLE_STATUS_BITS_RX_V2P_ENABLE |
	    BNX_MISC_ENABLE_STATUS_BITS_CONTEXT_ENABLE);

	/* Initialize context mapping and zero out the quick contexts. */
	bnx_init_context(sc);

	/* Initialize the on-boards CPUs */
	bnx_init_cpus(sc);

	/* Prepare NVRAM for access. */
	if (bnx_init_nvram(sc)) {
		rc = ENODEV;
		goto bnx_chipinit_exit;
	}

	/* Set the kernel bypass block size */
	val = REG_RD(sc, BNX_MQ_CONFIG);
	val &= ~BNX_MQ_CONFIG_KNL_BYP_BLK_SIZE;
	val |= BNX_MQ_CONFIG_KNL_BYP_BLK_SIZE_256;

	/* Enable bins used on the 5709. */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		val |= BNX_MQ_CONFIG_BIN_MQ_MODE;
		if (BNX_CHIP_ID(sc) == BNX_CHIP_ID_5709_A1)
			val |= BNX_MQ_CONFIG_HALT_DIS;
	}

	REG_WR(sc, BNX_MQ_CONFIG, val);

	val = 0x10000 + (MAX_CID_CNT * BNX_MB_KERNEL_CTX_SIZE);
	REG_WR(sc, BNX_MQ_KNL_BYP_WIND_START, val);
	REG_WR(sc, BNX_MQ_KNL_WIND_END, val);

	val = (BCM_PAGE_BITS - 8) << 24;
	REG_WR(sc, BNX_RV2P_CONFIG, val);

	/* Configure page size. */
	val = REG_RD(sc, BNX_TBDR_CONFIG);
	val &= ~BNX_TBDR_CONFIG_PAGE_SIZE;
	val |= (BCM_PAGE_BITS - 8) << 24 | 0x40;
	REG_WR(sc, BNX_TBDR_CONFIG, val);

#if 0
	/* Set the perfect match control register to default. */
	REG_WR_IND(sc, BNX_RXP_PM_CTRL, 0);
#endif

bnx_chipinit_exit:
	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	return rc;
}

/****************************************************************************/
/* Initialize the controller in preparation to send/receive traffic.        */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_blockinit(struct bnx_softc *sc)
{
	uint32_t		reg, val;
	int 			rc = 0;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Load the hardware default MAC address. */
	bnx_set_mac_addr(sc);

	/* Set the Ethernet backoff seed value */
	val = sc->eaddr[0] + (sc->eaddr[1] << 8) + (sc->eaddr[2] << 16) +
	    (sc->eaddr[3]) + (sc->eaddr[4] << 8) + (sc->eaddr[5] << 16);
	REG_WR(sc, BNX_EMAC_BACKOFF_SEED, val);

	sc->last_status_idx = 0;
	sc->rx_mode = BNX_EMAC_RX_MODE_SORT_MODE;

	/* Set up link change interrupt generation. */
	REG_WR(sc, BNX_EMAC_ATTENTION_ENA, BNX_EMAC_ATTENTION_ENA_LINK);
	REG_WR(sc, BNX_HC_ATTN_BITS_ENABLE, STATUS_ATTN_BITS_LINK_STATE);

	/* Program the physical address of the status block. */
	REG_WR(sc, BNX_HC_STATUS_ADDR_L, (uint32_t)(sc->status_block_paddr));
	REG_WR(sc, BNX_HC_STATUS_ADDR_H,
	    (uint32_t)((uint64_t)sc->status_block_paddr >> 32));

	/* Program the physical address of the statistics block. */
	REG_WR(sc, BNX_HC_STATISTICS_ADDR_L,
	    (uint32_t)(sc->stats_block_paddr));
	REG_WR(sc, BNX_HC_STATISTICS_ADDR_H,
	    (uint32_t)((uint64_t)sc->stats_block_paddr >> 32));

	/* Program various host coalescing parameters. */
	REG_WR(sc, BNX_HC_TX_QUICK_CONS_TRIP, (sc->bnx_tx_quick_cons_trip_int
	    << 16) | sc->bnx_tx_quick_cons_trip);
	REG_WR(sc, BNX_HC_RX_QUICK_CONS_TRIP, (sc->bnx_rx_quick_cons_trip_int
	    << 16) | sc->bnx_rx_quick_cons_trip);
	REG_WR(sc, BNX_HC_COMP_PROD_TRIP, (sc->bnx_comp_prod_trip_int << 16) |
	    sc->bnx_comp_prod_trip);
	REG_WR(sc, BNX_HC_TX_TICKS, (sc->bnx_tx_ticks_int << 16) |
	    sc->bnx_tx_ticks);
	REG_WR(sc, BNX_HC_RX_TICKS, (sc->bnx_rx_ticks_int << 16) |
	    sc->bnx_rx_ticks);
	REG_WR(sc, BNX_HC_COM_TICKS, (sc->bnx_com_ticks_int << 16) |
	    sc->bnx_com_ticks);
	REG_WR(sc, BNX_HC_CMD_TICKS, (sc->bnx_cmd_ticks_int << 16) |
	    sc->bnx_cmd_ticks);
	REG_WR(sc, BNX_HC_STATS_TICKS, (sc->bnx_stats_ticks & 0xffff00));
	REG_WR(sc, BNX_HC_STAT_COLLECT_TICKS, 0xbb8);  /* 3ms */
	REG_WR(sc, BNX_HC_CONFIG,
	    (BNX_HC_CONFIG_RX_TMR_MODE | BNX_HC_CONFIG_TX_TMR_MODE |
	    BNX_HC_CONFIG_COLLECT_STATS));

	/* Clear the internal statistics counters. */
	REG_WR(sc, BNX_HC_COMMAND, BNX_HC_COMMAND_CLR_STAT_NOW);

	/* Verify that bootcode is running. */
	reg = REG_RD_IND(sc, sc->bnx_shmem_base + BNX_DEV_INFO_SIGNATURE);

	DBRUNIF(DB_RANDOMTRUE(bnx_debug_bootcode_running_failure),
	    BNX_PRINTF(sc, "%s(%d): Simulating bootcode failure.\n",
	    __FILE__, __LINE__); reg = 0);

	if ((reg & BNX_DEV_INFO_SIGNATURE_MAGIC_MASK) !=
	    BNX_DEV_INFO_SIGNATURE_MAGIC) {
		BNX_PRINTF(sc, "%s(%d): Bootcode not running! Found: 0x%08X, "
		    "Expected: 08%08X\n", __FILE__, __LINE__,
		    (reg & BNX_DEV_INFO_SIGNATURE_MAGIC_MASK),
		    BNX_DEV_INFO_SIGNATURE_MAGIC);
		rc = ENODEV;
		goto bnx_blockinit_exit;
	}

	/* Check if any management firmware is running. */
	reg = REG_RD_IND(sc, sc->bnx_shmem_base + BNX_PORT_FEATURE);
	if (reg & (BNX_PORT_FEATURE_ASF_ENABLED |
	    BNX_PORT_FEATURE_IMD_ENABLED)) {
		DBPRINT(sc, BNX_INFO, "Management F/W Enabled.\n");
		sc->bnx_flags |= BNX_MFW_ENABLE_FLAG;
	}

	sc->bnx_fw_ver = REG_RD_IND(sc, sc->bnx_shmem_base +
	    BNX_DEV_INFO_BC_REV);

	DBPRINT(sc, BNX_INFO, "bootcode rev = 0x%08X\n", sc->bnx_fw_ver);

	/* Enable DMA */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		val = REG_RD(sc, BNX_MISC_NEW_CORE_CTL);
		val |= BNX_MISC_NEW_CORE_CTL_DMA_ENABLE;
		REG_WR(sc, BNX_MISC_NEW_CORE_CTL, val);
	}

	/* Allow bootcode to apply any additional fixes before enabling MAC. */
	rc = bnx_fw_sync(sc, BNX_DRV_MSG_DATA_WAIT2 | BNX_DRV_MSG_CODE_RESET);

	/* Enable link state change interrupt generation. */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		REG_WR(sc, BNX_MISC_ENABLE_SET_BITS,
		    BNX_MISC_ENABLE_DEFAULT_XI);
	} else
		REG_WR(sc, BNX_MISC_ENABLE_SET_BITS, BNX_MISC_ENABLE_DEFAULT);

	/* Enable all remaining blocks in the MAC. */
	REG_WR(sc, BNX_MISC_ENABLE_SET_BITS, 0x5ffffff);
	REG_RD(sc, BNX_MISC_ENABLE_SET_BITS);
	DELAY(20);

bnx_blockinit_exit:
	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	return rc;
}

static int
bnx_add_buf(struct bnx_softc *sc, struct mbuf *m_new, uint16_t *prod,
    uint16_t *chain_prod, uint32_t *prod_bseq)
{
	bus_dmamap_t		map;
	struct rx_bd		*rxbd;
	uint32_t		addr;
	int i;
#ifdef BNX_DEBUG
	uint16_t debug_chain_prod =	*chain_prod;
#endif
	uint16_t first_chain_prod;

	m_new->m_len = m_new->m_pkthdr.len = sc->mbuf_alloc_size;

	/* Map the mbuf cluster into device memory. */
	map = sc->rx_mbuf_map[*chain_prod];
	first_chain_prod = *chain_prod;
	if (bus_dmamap_load_mbuf(sc->bnx_dmatag, map, m_new, BUS_DMA_NOWAIT)) {
		BNX_PRINTF(sc, "%s(%d): Error mapping mbuf into RX chain!\n",
		    __FILE__, __LINE__);

		m_freem(m_new);

		DBRUNIF(1, sc->rx_mbuf_alloc--);

		return ENOBUFS;
	}
	/* Make sure there is room in the receive chain. */
	if (map->dm_nsegs > sc->free_rx_bd) {
		bus_dmamap_unload(sc->bnx_dmatag, map);
		m_freem(m_new);
		return EFBIG;
	}
#ifdef BNX_DEBUG
	/* Track the distribution of buffer segments. */
	sc->rx_mbuf_segs[map->dm_nsegs]++;
#endif

	bus_dmamap_sync(sc->bnx_dmatag, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	/* Update some debug statistics counters */
	DBRUNIF((sc->free_rx_bd < sc->rx_low_watermark),
	    sc->rx_low_watermark = sc->free_rx_bd);
	DBRUNIF((sc->free_rx_bd == sc->max_rx_bd), sc->rx_empty_count++);

	/*
	 * Setup the rx_bd for the first segment
	 */
	rxbd = &sc->rx_bd_chain[RX_PAGE(*chain_prod)][RX_IDX(*chain_prod)];

	addr = (uint32_t)map->dm_segs[0].ds_addr;
	rxbd->rx_bd_haddr_lo = addr;
	addr = (uint32_t)((uint64_t)map->dm_segs[0].ds_addr >> 32);
	rxbd->rx_bd_haddr_hi = addr;
	rxbd->rx_bd_len = map->dm_segs[0].ds_len;
	rxbd->rx_bd_flags = RX_BD_FLAGS_START;
	*prod_bseq += map->dm_segs[0].ds_len;
	bus_dmamap_sync(sc->bnx_dmatag,
	    sc->rx_bd_chain_map[RX_PAGE(*chain_prod)],
	    sizeof(struct rx_bd) * RX_IDX(*chain_prod), sizeof(struct rx_bd),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	for (i = 1; i < map->dm_nsegs; i++) {
		*prod = NEXT_RX_BD(*prod);
		*chain_prod = RX_CHAIN_IDX(*prod);

		rxbd =
		    &sc->rx_bd_chain[RX_PAGE(*chain_prod)][RX_IDX(*chain_prod)];

		addr = (uint32_t)map->dm_segs[i].ds_addr;
		rxbd->rx_bd_haddr_lo = addr;
		addr = (uint32_t)((uint64_t)map->dm_segs[i].ds_addr >> 32);
		rxbd->rx_bd_haddr_hi = addr;
		rxbd->rx_bd_len = map->dm_segs[i].ds_len;
		rxbd->rx_bd_flags = 0;
		*prod_bseq += map->dm_segs[i].ds_len;
		bus_dmamap_sync(sc->bnx_dmatag,
		    sc->rx_bd_chain_map[RX_PAGE(*chain_prod)],
		    sizeof(struct rx_bd) * RX_IDX(*chain_prod),
		    sizeof(struct rx_bd), BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}

	rxbd->rx_bd_flags |= RX_BD_FLAGS_END;
	bus_dmamap_sync(sc->bnx_dmatag,
	    sc->rx_bd_chain_map[RX_PAGE(*chain_prod)],
	    sizeof(struct rx_bd) * RX_IDX(*chain_prod),
	    sizeof(struct rx_bd), BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	 * Save the mbuf, adjust the map pointer (swap map for first and
	 * last rx_bd entry so that rx_mbuf_ptr and rx_mbuf_map matches)
	 * and update our counter.
	 */
	sc->rx_mbuf_ptr[*chain_prod] = m_new;
	sc->rx_mbuf_map[first_chain_prod] = sc->rx_mbuf_map[*chain_prod];
	sc->rx_mbuf_map[*chain_prod] = map;
	sc->free_rx_bd -= map->dm_nsegs;

	DBRUN(BNX_VERBOSE_RECV, bnx_dump_rx_mbuf_chain(sc, debug_chain_prod,
	    map->dm_nsegs));
	*prod = NEXT_RX_BD(*prod);
	*chain_prod = RX_CHAIN_IDX(*prod);

	return 0;
}

/****************************************************************************/
/* Encapsulate an mbuf cluster into the rx_bd chain.                        */
/*                                                                          */
/* The NetXtreme II can support Jumbo frames by using multiple rx_bd's.     */
/* This routine will map an mbuf cluster into 1 or more rx_bd's as          */
/* necessary.                                                               */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_get_buf(struct bnx_softc *sc, uint16_t *prod,
    uint16_t *chain_prod, uint32_t *prod_bseq)
{
	struct mbuf 		*m_new = NULL;
	int			rc = 0;
	uint16_t min_free_bd;

	DBPRINT(sc, (BNX_VERBOSE_RESET | BNX_VERBOSE_RECV), "Entering %s()\n",
	    __func__);

	/* Make sure the inputs are valid. */
	DBRUNIF((*chain_prod > MAX_RX_BD),
	    aprint_error_dev(sc->bnx_dev,
	        "RX producer out of range: 0x%04X > 0x%04X\n",
		*chain_prod, (uint16_t)MAX_RX_BD));

	DBPRINT(sc, BNX_VERBOSE_RECV, "%s(enter): prod = 0x%04X, chain_prod = "
	    "0x%04X, prod_bseq = 0x%08X\n", __func__, *prod, *chain_prod,
	    *prod_bseq);

	/* try to get in as many mbufs as possible */
	if (sc->mbuf_alloc_size == MCLBYTES)
		min_free_bd = (MCLBYTES + PAGE_SIZE - 1) / PAGE_SIZE;
	else
		min_free_bd = (BNX_MAX_JUMBO_MRU + PAGE_SIZE - 1) / PAGE_SIZE;
	while (sc->free_rx_bd >= min_free_bd) {
		/* Simulate an mbuf allocation failure. */
		DBRUNIF(DB_RANDOMTRUE(bnx_debug_mbuf_allocation_failure),
		    aprint_error_dev(sc->bnx_dev,
		    "Simulating mbuf allocation failure.\n");
			sc->mbuf_sim_alloc_failed++;
			rc = ENOBUFS;
			goto bnx_get_buf_exit);

		/* This is a new mbuf allocation. */
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			DBPRINT(sc, BNX_WARN,
			    "%s(%d): RX mbuf header allocation failed!\n",
			    __FILE__, __LINE__);

			sc->mbuf_alloc_failed++;

			rc = ENOBUFS;
			goto bnx_get_buf_exit;
		}

		DBRUNIF(1, sc->rx_mbuf_alloc++);

		/* Simulate an mbuf cluster allocation failure. */
		DBRUNIF(DB_RANDOMTRUE(bnx_debug_mbuf_allocation_failure),
			m_freem(m_new);
			sc->rx_mbuf_alloc--;
			sc->mbuf_alloc_failed++;
			sc->mbuf_sim_alloc_failed++;
			rc = ENOBUFS;
			goto bnx_get_buf_exit);

		if (sc->mbuf_alloc_size == MCLBYTES)
			MCLGET(m_new, M_DONTWAIT);
		else
			MEXTMALLOC(m_new, sc->mbuf_alloc_size,
			    M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			DBPRINT(sc, BNX_WARN,
			    "%s(%d): RX mbuf chain allocation failed!\n",
			    __FILE__, __LINE__);

			m_freem(m_new);

			DBRUNIF(1, sc->rx_mbuf_alloc--);
			sc->mbuf_alloc_failed++;

			rc = ENOBUFS;
			goto bnx_get_buf_exit;
		}

		rc = bnx_add_buf(sc, m_new, prod, chain_prod, prod_bseq);
		if (rc != 0)
			goto bnx_get_buf_exit;
	}

bnx_get_buf_exit:
	DBPRINT(sc, BNX_VERBOSE_RECV, "%s(exit): prod = 0x%04X, chain_prod "
	    "= 0x%04X, prod_bseq = 0x%08X\n", __func__, *prod,
	    *chain_prod, *prod_bseq);

	DBPRINT(sc, (BNX_VERBOSE_RESET | BNX_VERBOSE_RECV), "Exiting %s()\n",
	    __func__);

	return rc;
}

void
bnx_alloc_pkts(struct work * unused, void * arg)
{
	struct bnx_softc *sc = arg;
	struct ifnet *ifp = &sc->bnx_ec.ec_if;
	struct bnx_pkt *pkt;
	int i, s;

	for (i = 0; i < 4; i++) { /* magic! */
		pkt = pool_get(bnx_tx_pool, PR_WAITOK);
		if (pkt == NULL)
			break;

		if (bus_dmamap_create(sc->bnx_dmatag,
		    MCLBYTES * BNX_MAX_SEGMENTS, USABLE_TX_BD,
		    MCLBYTES, 0, BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW,
		    &pkt->pkt_dmamap) != 0)
			goto put;

		if (!ISSET(ifp->if_flags, IFF_UP))
			goto stopping;

		mutex_enter(&sc->tx_pkt_mtx);
		TAILQ_INSERT_TAIL(&sc->tx_free_pkts, pkt, pkt_entry);
		sc->tx_pkt_count++;
		mutex_exit(&sc->tx_pkt_mtx);
	}

	mutex_enter(&sc->tx_pkt_mtx);
	CLR(sc->bnx_flags, BNX_ALLOC_PKTS_FLAG);
	mutex_exit(&sc->tx_pkt_mtx);

	/* fire-up TX now that allocations have been done */
	s = splnet();
	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		bnx_start(ifp);
	splx(s);

	return;

stopping:
	bus_dmamap_destroy(sc->bnx_dmatag, pkt->pkt_dmamap);
put:
	pool_put(bnx_tx_pool, pkt);
	return;
}

/****************************************************************************/
/* Initialize the TX context memory.                                        */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing                                                                */
/****************************************************************************/
void
bnx_init_tx_context(struct bnx_softc *sc)
{
	uint32_t val;

	/* Initialize the context ID for an L2 TX chain. */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		/* Set the CID type to support an L2 connection. */
		val = BNX_L2CTX_TYPE_TYPE_L2 | BNX_L2CTX_TYPE_SIZE_L2;
		CTX_WR(sc, GET_CID_ADDR(TX_CID), BNX_L2CTX_TYPE_XI, val);
		val = BNX_L2CTX_CMD_TYPE_TYPE_L2 | (8 << 16);
		CTX_WR(sc, GET_CID_ADDR(TX_CID), BNX_L2CTX_CMD_TYPE_XI, val);

		/* Point the hardware to the first page in the chain. */
		val = (uint32_t)((uint64_t)sc->tx_bd_chain_paddr[0] >> 32);
		CTX_WR(sc, GET_CID_ADDR(TX_CID),
		    BNX_L2CTX_TBDR_BHADDR_HI_XI, val);
		val = (uint32_t)(sc->tx_bd_chain_paddr[0]);
		CTX_WR(sc, GET_CID_ADDR(TX_CID),
		    BNX_L2CTX_TBDR_BHADDR_LO_XI, val);
	} else {
		/* Set the CID type to support an L2 connection. */
		val = BNX_L2CTX_TYPE_TYPE_L2 | BNX_L2CTX_TYPE_SIZE_L2;
		CTX_WR(sc, GET_CID_ADDR(TX_CID), BNX_L2CTX_TYPE, val);
		val = BNX_L2CTX_CMD_TYPE_TYPE_L2 | (8 << 16);
		CTX_WR(sc, GET_CID_ADDR(TX_CID), BNX_L2CTX_CMD_TYPE, val);

		/* Point the hardware to the first page in the chain. */
		val = (uint32_t)((uint64_t)sc->tx_bd_chain_paddr[0] >> 32);
		CTX_WR(sc, GET_CID_ADDR(TX_CID), BNX_L2CTX_TBDR_BHADDR_HI, val);
		val = (uint32_t)(sc->tx_bd_chain_paddr[0]);
		CTX_WR(sc, GET_CID_ADDR(TX_CID), BNX_L2CTX_TBDR_BHADDR_LO, val);
	}
}


/****************************************************************************/
/* Allocate memory and initialize the TX data structures.                   */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_init_tx_chain(struct bnx_softc *sc)
{
	struct tx_bd		*txbd;
	uint32_t		addr;
	int			i, rc = 0;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Force an allocation of some dmamaps for tx up front */
	bnx_alloc_pkts(NULL, sc);

	/* Set the initial TX producer/consumer indices. */
	sc->tx_prod = 0;
	sc->tx_cons = 0;
	sc->tx_prod_bseq = 0;
	sc->used_tx_bd = 0;
	sc->max_tx_bd = USABLE_TX_BD;
	DBRUNIF(1, sc->tx_hi_watermark = USABLE_TX_BD);
	DBRUNIF(1, sc->tx_full_count = 0);

	/*
	 * The NetXtreme II supports a linked-list structure called
	 * a Buffer Descriptor Chain (or BD chain).  A BD chain
	 * consists of a series of 1 or more chain pages, each of which
	 * consists of a fixed number of BD entries.
	 * The last BD entry on each page is a pointer to the next page
	 * in the chain, and the last pointer in the BD chain
	 * points back to the beginning of the chain.
	 */

	/* Set the TX next pointer chain entries. */
	for (i = 0; i < TX_PAGES; i++) {
		int j;

		txbd = &sc->tx_bd_chain[i][USABLE_TX_BD_PER_PAGE];

		/* Check if we've reached the last page. */
		if (i == (TX_PAGES - 1))
			j = 0;
		else
			j = i + 1;

		addr = (uint32_t)sc->tx_bd_chain_paddr[j];
		txbd->tx_bd_haddr_lo = addr;
		addr = (uint32_t)((uint64_t)sc->tx_bd_chain_paddr[j] >> 32);
		txbd->tx_bd_haddr_hi = addr;
		bus_dmamap_sync(sc->bnx_dmatag, sc->tx_bd_chain_map[i], 0,
		    BNX_TX_CHAIN_PAGE_SZ, BUS_DMASYNC_PREWRITE);
	}

	/*
	 * Initialize the context ID for an L2 TX chain.
	 */
	bnx_init_tx_context(sc);

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	return rc;
}

/****************************************************************************/
/* Free memory and clear the TX data structures.                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_free_tx_chain(struct bnx_softc *sc)
{
	struct bnx_pkt		*pkt;
	int			i;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Unmap, unload, and free any mbufs still in the TX mbuf chain. */
	mutex_enter(&sc->tx_pkt_mtx);
	while ((pkt = TAILQ_FIRST(&sc->tx_used_pkts)) != NULL) {
		TAILQ_REMOVE(&sc->tx_used_pkts, pkt, pkt_entry);
		mutex_exit(&sc->tx_pkt_mtx);

		bus_dmamap_sync(sc->bnx_dmatag, pkt->pkt_dmamap, 0,
		    pkt->pkt_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->bnx_dmatag, pkt->pkt_dmamap);

		m_freem(pkt->pkt_mbuf);
		DBRUNIF(1, sc->tx_mbuf_alloc--);

		mutex_enter(&sc->tx_pkt_mtx);
		TAILQ_INSERT_TAIL(&sc->tx_free_pkts, pkt, pkt_entry);
	}

	/* Destroy all the dmamaps we allocated for TX */
	while ((pkt = TAILQ_FIRST(&sc->tx_free_pkts)) != NULL) {
		TAILQ_REMOVE(&sc->tx_free_pkts, pkt, pkt_entry);
		sc->tx_pkt_count--;
		mutex_exit(&sc->tx_pkt_mtx);

		bus_dmamap_destroy(sc->bnx_dmatag, pkt->pkt_dmamap);
		pool_put(bnx_tx_pool, pkt);

		mutex_enter(&sc->tx_pkt_mtx);
	}
	mutex_exit(&sc->tx_pkt_mtx);



	/* Clear each TX chain page. */
	for (i = 0; i < TX_PAGES; i++) {
		memset((char *)sc->tx_bd_chain[i], 0, BNX_TX_CHAIN_PAGE_SZ);
		bus_dmamap_sync(sc->bnx_dmatag, sc->tx_bd_chain_map[i], 0,
		    BNX_TX_CHAIN_PAGE_SZ, BUS_DMASYNC_PREWRITE);
	}

	sc->used_tx_bd = 0;

	/* Check if we lost any mbufs in the process. */
	DBRUNIF((sc->tx_mbuf_alloc),
	    aprint_error_dev(sc->bnx_dev,
	        "Memory leak! Lost %d mbufs from tx chain!\n",
		sc->tx_mbuf_alloc));

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);
}

/****************************************************************************/
/* Initialize the RX context memory.                                        */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing                                                                */
/****************************************************************************/
void
bnx_init_rx_context(struct bnx_softc *sc)
{
	uint32_t val;

	/* Initialize the context ID for an L2 RX chain. */
	val = BNX_L2CTX_CTX_TYPE_CTX_BD_CHN_TYPE_VALUE |
		BNX_L2CTX_CTX_TYPE_SIZE_L2 | (0x02 << 8);

	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		uint32_t lo_water, hi_water;

		lo_water = BNX_L2CTX_RX_LO_WATER_MARK_DEFAULT;
		hi_water = USABLE_RX_BD / 4;

		lo_water /= BNX_L2CTX_RX_LO_WATER_MARK_SCALE;
		hi_water /= BNX_L2CTX_RX_HI_WATER_MARK_SCALE;

		if (hi_water > 0xf)
			hi_water = 0xf;
		else if (hi_water == 0)
			lo_water = 0;
		val |= lo_water |
		    (hi_water << BNX_L2CTX_RX_HI_WATER_MARK_SHIFT);
	}

 	CTX_WR(sc, GET_CID_ADDR(RX_CID), BNX_L2CTX_CTX_TYPE, val);

	/* Setup the MQ BIN mapping for l2_ctx_host_bseq. */
	if (BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5709) {
		val = REG_RD(sc, BNX_MQ_MAP_L2_5);
		REG_WR(sc, BNX_MQ_MAP_L2_5, val | BNX_MQ_MAP_L2_5_ARM);
	}

	/* Point the hardware to the first page in the chain. */
	val = (uint32_t)((uint64_t)sc->rx_bd_chain_paddr[0] >> 32);
	CTX_WR(sc, GET_CID_ADDR(RX_CID), BNX_L2CTX_NX_BDHADDR_HI, val);
	val = (uint32_t)(sc->rx_bd_chain_paddr[0]);
	CTX_WR(sc, GET_CID_ADDR(RX_CID), BNX_L2CTX_NX_BDHADDR_LO, val);
}

/****************************************************************************/
/* Allocate memory and initialize the RX data structures.                   */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_init_rx_chain(struct bnx_softc *sc)
{
	struct rx_bd		*rxbd;
	int			i, rc = 0;
	uint16_t		prod, chain_prod;
	uint32_t		prod_bseq, addr;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Initialize the RX producer and consumer indices. */
	sc->rx_prod = 0;
	sc->rx_cons = 0;
	sc->rx_prod_bseq = 0;
	sc->free_rx_bd = USABLE_RX_BD;
	sc->max_rx_bd = USABLE_RX_BD;
	DBRUNIF(1, sc->rx_low_watermark = USABLE_RX_BD);
	DBRUNIF(1, sc->rx_empty_count = 0);

	/* Initialize the RX next pointer chain entries. */
	for (i = 0; i < RX_PAGES; i++) {
		int j;

		rxbd = &sc->rx_bd_chain[i][USABLE_RX_BD_PER_PAGE];

		/* Check if we've reached the last page. */
		if (i == (RX_PAGES - 1))
			j = 0;
		else
			j = i + 1;

		/* Setup the chain page pointers. */
		addr = (uint32_t)((uint64_t)sc->rx_bd_chain_paddr[j] >> 32);
		rxbd->rx_bd_haddr_hi = addr;
		addr = (uint32_t)sc->rx_bd_chain_paddr[j];
		rxbd->rx_bd_haddr_lo = addr;
		bus_dmamap_sync(sc->bnx_dmatag, sc->rx_bd_chain_map[i],
		    0, BNX_RX_CHAIN_PAGE_SZ,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}

	/* Allocate mbuf clusters for the rx_bd chain. */
	prod = prod_bseq = 0;
	chain_prod = RX_CHAIN_IDX(prod);
	if (bnx_get_buf(sc, &prod, &chain_prod, &prod_bseq)) {
		BNX_PRINTF(sc,
		    "Error filling RX chain: rx_bd[0x%04X]!\n", chain_prod);
	}

	/* Save the RX chain producer index. */
	sc->rx_prod = prod;
	sc->rx_prod_bseq = prod_bseq;

	for (i = 0; i < RX_PAGES; i++)
		bus_dmamap_sync(sc->bnx_dmatag, sc->rx_bd_chain_map[i], 0,
		    sc->rx_bd_chain_map[i]->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Tell the chip about the waiting rx_bd's. */
	REG_WR16(sc, MB_RX_CID_ADDR + BNX_L2CTX_HOST_BDIDX, sc->rx_prod);
	REG_WR(sc, MB_RX_CID_ADDR + BNX_L2CTX_HOST_BSEQ, sc->rx_prod_bseq);

	bnx_init_rx_context(sc);

	DBRUN(BNX_VERBOSE_RECV, bnx_dump_rx_chain(sc, 0, TOTAL_RX_BD));

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	return rc;
}

/****************************************************************************/
/* Free memory and clear the RX data structures.                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_free_rx_chain(struct bnx_softc *sc)
{
	int			i;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	/* Free any mbufs still in the RX mbuf chain. */
	for (i = 0; i < TOTAL_RX_BD; i++) {
		if (sc->rx_mbuf_ptr[i] != NULL) {
			if (sc->rx_mbuf_map[i] != NULL) {
				bus_dmamap_sync(sc->bnx_dmatag,
				    sc->rx_mbuf_map[i],	0,
				    sc->rx_mbuf_map[i]->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->bnx_dmatag,
				    sc->rx_mbuf_map[i]);
			}
			m_freem(sc->rx_mbuf_ptr[i]);
			sc->rx_mbuf_ptr[i] = NULL;
			DBRUNIF(1, sc->rx_mbuf_alloc--);
		}
	}

	/* Clear each RX chain page. */
	for (i = 0; i < RX_PAGES; i++)
		memset((char *)sc->rx_bd_chain[i], 0, BNX_RX_CHAIN_PAGE_SZ);

	sc->free_rx_bd = sc->max_rx_bd;

	/* Check if we lost any mbufs in the process. */
	DBRUNIF((sc->rx_mbuf_alloc),
	    aprint_error_dev(sc->bnx_dev,
	        "Memory leak! Lost %d mbufs from rx chain!\n",
		sc->rx_mbuf_alloc));

	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);
}

/****************************************************************************/
/* Handles PHY generated interrupt events.                                  */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_phy_intr(struct bnx_softc *sc)
{
	uint32_t		new_link_state, old_link_state;

	bus_dmamap_sync(sc->bnx_dmatag, sc->status_map, 0, BNX_STATUS_BLK_SZ,
	    BUS_DMASYNC_POSTREAD);
	new_link_state = sc->status_block->status_attn_bits &
	    STATUS_ATTN_BITS_LINK_STATE;
	old_link_state = sc->status_block->status_attn_bits_ack &
	    STATUS_ATTN_BITS_LINK_STATE;

	/* Handle any changes if the link state has changed. */
	if (new_link_state != old_link_state) {
		DBRUN(BNX_VERBOSE_INTR, bnx_dump_status_block(sc));

		callout_stop(&sc->bnx_timeout);
		bnx_tick(sc);

		/* Update the status_attn_bits_ack field in the status block. */
		if (new_link_state) {
			REG_WR(sc, BNX_PCICFG_STATUS_BIT_SET_CMD,
			    STATUS_ATTN_BITS_LINK_STATE);
			DBPRINT(sc, BNX_INFO, "Link is now UP.\n");
		} else {
			REG_WR(sc, BNX_PCICFG_STATUS_BIT_CLEAR_CMD,
			    STATUS_ATTN_BITS_LINK_STATE);
			DBPRINT(sc, BNX_INFO, "Link is now DOWN.\n");
		}
	}

	/* Acknowledge the link change interrupt. */
	REG_WR(sc, BNX_EMAC_STATUS, BNX_EMAC_STATUS_LINK_CHANGE);
}

/****************************************************************************/
/* Handles received frame interrupt events.                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_rx_intr(struct bnx_softc *sc)
{
	struct status_block	*sblk = sc->status_block;
	struct ifnet		*ifp = &sc->bnx_ec.ec_if;
	uint16_t		hw_cons, sw_cons, sw_chain_cons;
	uint16_t		sw_prod, sw_chain_prod;
	uint32_t		sw_prod_bseq;
	struct l2_fhdr		*l2fhdr;
	int			i;

	DBRUNIF(1, sc->rx_interrupts++);
	bus_dmamap_sync(sc->bnx_dmatag, sc->status_map, 0, BNX_STATUS_BLK_SZ,
	    BUS_DMASYNC_POSTREAD);

	/* Prepare the RX chain pages to be accessed by the host CPU. */
	for (i = 0; i < RX_PAGES; i++)
		bus_dmamap_sync(sc->bnx_dmatag,
		    sc->rx_bd_chain_map[i], 0,
		    sc->rx_bd_chain_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);

	/* Get the hardware's view of the RX consumer index. */
	hw_cons = sc->hw_rx_cons = sblk->status_rx_quick_consumer_index0;
	if ((hw_cons & USABLE_RX_BD_PER_PAGE) == USABLE_RX_BD_PER_PAGE)
		hw_cons++;

	/* Get working copies of the driver's view of the RX indices. */
	sw_cons = sc->rx_cons;
	sw_prod = sc->rx_prod;
	sw_prod_bseq = sc->rx_prod_bseq;

	DBPRINT(sc, BNX_INFO_RECV, "%s(enter): sw_prod = 0x%04X, "
	    "sw_cons = 0x%04X, sw_prod_bseq = 0x%08X\n",
	    __func__, sw_prod, sw_cons, sw_prod_bseq);

	/* Prevent speculative reads from getting ahead of the status block. */
	bus_space_barrier(sc->bnx_btag, sc->bnx_bhandle, 0, 0,
	    BUS_SPACE_BARRIER_READ);

	/* Update some debug statistics counters */
	DBRUNIF((sc->free_rx_bd < sc->rx_low_watermark),
	    sc->rx_low_watermark = sc->free_rx_bd);
	DBRUNIF((sc->free_rx_bd == USABLE_RX_BD), sc->rx_empty_count++);

	/*
	 * Scan through the receive chain as long
	 * as there is work to do.
	 */
	while (sw_cons != hw_cons) {
		struct mbuf *m;
		struct rx_bd *rxbd __diagused;
		unsigned int len;
		uint32_t status;

		/* Convert the producer/consumer indices to an actual
		 * rx_bd index.
		 */
		sw_chain_cons = RX_CHAIN_IDX(sw_cons);
		sw_chain_prod = RX_CHAIN_IDX(sw_prod);

		/* Get the used rx_bd. */
		rxbd = &sc->rx_bd_chain[RX_PAGE(sw_chain_cons)][RX_IDX(sw_chain_cons)];
		sc->free_rx_bd++;

		DBRUN(BNX_VERBOSE_RECV, aprint_error("%s(): ", __func__);
		bnx_dump_rxbd(sc, sw_chain_cons, rxbd));

		/* The mbuf is stored with the last rx_bd entry of a packet. */
		if (sc->rx_mbuf_ptr[sw_chain_cons] != NULL) {
#ifdef DIAGNOSTIC
			/* Validate that this is the last rx_bd. */
			if ((rxbd->rx_bd_flags & RX_BD_FLAGS_END) == 0) {
			    printf("%s: Unexpected mbuf found in "
			        "rx_bd[0x%04X]!\n", device_xname(sc->bnx_dev),
			        sw_chain_cons);
			}
#endif

			/* DRC - ToDo: If the received packet is small, say less
			 *             than 128 bytes, allocate a new mbuf here,
			 *             copy the data to that mbuf, and recycle
			 *             the mapped jumbo frame.
			 */

			/* Unmap the mbuf from DMA space. */
#ifdef DIAGNOSTIC
			if (sc->rx_mbuf_map[sw_chain_cons]->dm_mapsize == 0) {
				printf("invalid map sw_cons 0x%x "
				"sw_prod 0x%x "
				"sw_chain_cons 0x%x "
				"sw_chain_prod 0x%x "
				"hw_cons 0x%x "
				"TOTAL_RX_BD_PER_PAGE 0x%x "
				"TOTAL_RX_BD 0x%x\n",
				sw_cons, sw_prod, sw_chain_cons, sw_chain_prod,
				hw_cons,
				(int)TOTAL_RX_BD_PER_PAGE, (int)TOTAL_RX_BD);
			}
#endif
			bus_dmamap_sync(sc->bnx_dmatag,
			    sc->rx_mbuf_map[sw_chain_cons], 0,
			    sc->rx_mbuf_map[sw_chain_cons]->dm_mapsize,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bnx_dmatag,
			    sc->rx_mbuf_map[sw_chain_cons]);

			/* Remove the mbuf from the driver's chain. */
			m = sc->rx_mbuf_ptr[sw_chain_cons];
			sc->rx_mbuf_ptr[sw_chain_cons] = NULL;

			/*
			 * Frames received on the NetXteme II are prepended
			 * with the l2_fhdr structure which provides status
			 * information about the received frame (including
			 * VLAN tags and checksum info) and are also
			 * automatically adjusted to align the IP header
			 * (i.e. two null bytes are inserted before the
			 * Ethernet header).
			 */
			l2fhdr = mtod(m, struct l2_fhdr *);

			len    = l2fhdr->l2_fhdr_pkt_len;
			status = l2fhdr->l2_fhdr_status;

			DBRUNIF(DB_RANDOMTRUE(bnx_debug_l2fhdr_status_check),
			    aprint_error("Simulating l2_fhdr status error.\n");
			    status = status | L2_FHDR_ERRORS_PHY_DECODE);

			/* Watch for unusual sized frames. */
			DBRUNIF(((len < BNX_MIN_MTU) ||
			    (len > BNX_MAX_JUMBO_ETHER_MTU_VLAN)),
			    aprint_error_dev(sc->bnx_dev,
			        "Unusual frame size found. "
				"Min(%d), Actual(%d), Max(%d)\n",
				(int)BNX_MIN_MTU, len,
				(int)BNX_MAX_JUMBO_ETHER_MTU_VLAN);

			bnx_dump_mbuf(sc, m);
			bnx_breakpoint(sc));

			len -= ETHER_CRC_LEN;

			/* Check the received frame for errors. */
			if ((status &  (L2_FHDR_ERRORS_BAD_CRC |
			    L2_FHDR_ERRORS_PHY_DECODE |
			    L2_FHDR_ERRORS_ALIGNMENT |
			    L2_FHDR_ERRORS_TOO_SHORT |
			    L2_FHDR_ERRORS_GIANT_FRAME)) ||
			    len < (BNX_MIN_MTU - ETHER_CRC_LEN) ||
			    len >
			    (BNX_MAX_JUMBO_ETHER_MTU_VLAN - ETHER_CRC_LEN)) {
				ifp->if_ierrors++;
				DBRUNIF(1, sc->l2fhdr_status_errors++);

				/* Reuse the mbuf for a new frame. */
				if (bnx_add_buf(sc, m, &sw_prod,
				    &sw_chain_prod, &sw_prod_bseq)) {
					DBRUNIF(1, bnx_breakpoint(sc));
					panic("%s: Can't reuse RX mbuf!\n",
					    device_xname(sc->bnx_dev));
				}
				continue;
			}

			/*
			 * Get a new mbuf for the rx_bd.   If no new
			 * mbufs are available then reuse the current mbuf,
			 * log an ierror on the interface, and generate
			 * an error in the system log.
			 */
			if (bnx_get_buf(sc, &sw_prod, &sw_chain_prod,
			    &sw_prod_bseq)) {
				DBRUN(BNX_WARN, aprint_debug_dev(sc->bnx_dev,
				    "Failed to allocate "
				    "new mbuf, incoming frame dropped!\n"));

				ifp->if_ierrors++;

				/* Try and reuse the exisitng mbuf. */
				if (bnx_add_buf(sc, m, &sw_prod,
				    &sw_chain_prod, &sw_prod_bseq)) {
					DBRUNIF(1, bnx_breakpoint(sc));
					panic("%s: Double mbuf allocation "
					    "failure!",
					    device_xname(sc->bnx_dev));
				}
				continue;
			}

			/* Skip over the l2_fhdr when passing the data up
			 * the stack.
			 */
			m_adj(m, sizeof(struct l2_fhdr) + ETHER_ALIGN);

			/* Adjust the pckt length to match the received data. */
			m->m_pkthdr.len = m->m_len = len;

			/* Send the packet to the appropriate interface. */
			m->m_pkthdr.rcvif = ifp;

			DBRUN(BNX_VERBOSE_RECV,
			    struct ether_header *eh;
			    eh = mtod(m, struct ether_header *);
			    aprint_error("%s: to: %s, from: %s, type: 0x%04X\n",
			    __func__, ether_sprintf(eh->ether_dhost),
			    ether_sprintf(eh->ether_shost),
			    htons(eh->ether_type)));

			/* Validate the checksum. */

			/* Check for an IP datagram. */
			if (status & L2_FHDR_STATUS_IP_DATAGRAM) {
				/* Check if the IP checksum is valid. */
				if ((l2fhdr->l2_fhdr_ip_xsum ^ 0xffff)
				    == 0)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_IPv4;
#ifdef BNX_DEBUG
				else
					DBPRINT(sc, BNX_WARN_SEND,
					    "%s(): Invalid IP checksum "
					        "= 0x%04X!\n",
						__func__,
						l2fhdr->l2_fhdr_ip_xsum
						);
#endif
			}

			/* Check for a valid TCP/UDP frame. */
			if (status & (L2_FHDR_STATUS_TCP_SEGMENT |
			    L2_FHDR_STATUS_UDP_DATAGRAM)) {
				/* Check for a good TCP/UDP checksum. */
				if ((status &
				    (L2_FHDR_ERRORS_TCP_XSUM |
				    L2_FHDR_ERRORS_UDP_XSUM)) == 0) {
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCPv4 |
					    M_CSUM_UDPv4;
				} else {
					DBPRINT(sc, BNX_WARN_SEND,
					    "%s(): Invalid TCP/UDP "
					    "checksum = 0x%04X!\n",
					    __func__,
					    l2fhdr->l2_fhdr_tcp_udp_xsum);
				}
			}

			/*
			 * If we received a packet with a vlan tag,
			 * attach that information to the packet.
			 */
			if ((status & L2_FHDR_STATUS_L2_VLAN_TAG) &&
			    !(sc->rx_mode & BNX_EMAC_RX_MODE_KEEP_VLAN_TAG)) {
				VLAN_INPUT_TAG(ifp, m,
				    l2fhdr->l2_fhdr_vlan_tag,
				    continue);
			}

			/*
			 * Handle BPF listeners. Let the BPF
			 * user see the packet.
			 */
			bpf_mtap(ifp, m);

			/* Pass the mbuf off to the upper layers. */
			ifp->if_ipackets++;
			DBPRINT(sc, BNX_VERBOSE_RECV,
			    "%s(): Passing received frame up.\n", __func__);
			(*ifp->if_input)(ifp, m);
			DBRUNIF(1, sc->rx_mbuf_alloc--);

		}

		sw_cons = NEXT_RX_BD(sw_cons);

		/* Refresh hw_cons to see if there's new work */
		if (sw_cons == hw_cons) {
			hw_cons = sc->hw_rx_cons =
			    sblk->status_rx_quick_consumer_index0;
			if ((hw_cons & USABLE_RX_BD_PER_PAGE) ==
			    USABLE_RX_BD_PER_PAGE)
				hw_cons++;
		}

		/* Prevent speculative reads from getting ahead of
		 * the status block.
		 */
		bus_space_barrier(sc->bnx_btag, sc->bnx_bhandle, 0, 0,
		    BUS_SPACE_BARRIER_READ);
	}

	for (i = 0; i < RX_PAGES; i++)
		bus_dmamap_sync(sc->bnx_dmatag,
		    sc->rx_bd_chain_map[i], 0,
		    sc->rx_bd_chain_map[i]->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

	sc->rx_cons = sw_cons;
	sc->rx_prod = sw_prod;
	sc->rx_prod_bseq = sw_prod_bseq;

	REG_WR16(sc, MB_RX_CID_ADDR + BNX_L2CTX_HOST_BDIDX, sc->rx_prod);
	REG_WR(sc, MB_RX_CID_ADDR + BNX_L2CTX_HOST_BSEQ, sc->rx_prod_bseq);

	DBPRINT(sc, BNX_INFO_RECV, "%s(exit): rx_prod = 0x%04X, "
	    "rx_cons = 0x%04X, rx_prod_bseq = 0x%08X\n",
	    __func__, sc->rx_prod, sc->rx_cons, sc->rx_prod_bseq);
}

/****************************************************************************/
/* Handles transmit completion interrupt events.                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_tx_intr(struct bnx_softc *sc)
{
	struct status_block	*sblk = sc->status_block;
	struct ifnet		*ifp = &sc->bnx_ec.ec_if;
	struct bnx_pkt		*pkt;
	bus_dmamap_t		map;
	uint16_t		hw_tx_cons, sw_tx_cons, sw_tx_chain_cons;

	DBRUNIF(1, sc->tx_interrupts++);
	bus_dmamap_sync(sc->bnx_dmatag, sc->status_map, 0, BNX_STATUS_BLK_SZ,
	    BUS_DMASYNC_POSTREAD);

	/* Get the hardware's view of the TX consumer index. */
	hw_tx_cons = sc->hw_tx_cons = sblk->status_tx_quick_consumer_index0;

	/* Skip to the next entry if this is a chain page pointer. */
	if ((hw_tx_cons & USABLE_TX_BD_PER_PAGE) == USABLE_TX_BD_PER_PAGE)
		hw_tx_cons++;

	sw_tx_cons = sc->tx_cons;

	/* Prevent speculative reads from getting ahead of the status block. */
	bus_space_barrier(sc->bnx_btag, sc->bnx_bhandle, 0, 0,
	    BUS_SPACE_BARRIER_READ);

	/* Cycle through any completed TX chain page entries. */
	while (sw_tx_cons != hw_tx_cons) {
#ifdef BNX_DEBUG
		struct tx_bd *txbd = NULL;
#endif
		sw_tx_chain_cons = TX_CHAIN_IDX(sw_tx_cons);

		DBPRINT(sc, BNX_INFO_SEND, "%s(): hw_tx_cons = 0x%04X, "
		    "sw_tx_cons = 0x%04X, sw_tx_chain_cons = 0x%04X\n",
		    __func__, hw_tx_cons, sw_tx_cons, sw_tx_chain_cons);

		DBRUNIF((sw_tx_chain_cons > MAX_TX_BD),
		    aprint_error_dev(sc->bnx_dev,
		        "TX chain consumer out of range! 0x%04X > 0x%04X\n",
			sw_tx_chain_cons, (int)MAX_TX_BD); bnx_breakpoint(sc));

		DBRUNIF(1, txbd = &sc->tx_bd_chain
		    [TX_PAGE(sw_tx_chain_cons)][TX_IDX(sw_tx_chain_cons)]);

		DBRUNIF((txbd == NULL),
		    aprint_error_dev(sc->bnx_dev,
		        "Unexpected NULL tx_bd[0x%04X]!\n", sw_tx_chain_cons);
		    bnx_breakpoint(sc));

		DBRUN(BNX_INFO_SEND, aprint_debug("%s: ", __func__);
		    bnx_dump_txbd(sc, sw_tx_chain_cons, txbd));


		mutex_enter(&sc->tx_pkt_mtx);
		pkt = TAILQ_FIRST(&sc->tx_used_pkts);
		if (pkt != NULL && pkt->pkt_end_desc == sw_tx_chain_cons) {
			TAILQ_REMOVE(&sc->tx_used_pkts, pkt, pkt_entry);
			mutex_exit(&sc->tx_pkt_mtx);
			/*
			 * Free the associated mbuf. Remember
			 * that only the last tx_bd of a packet
			 * has an mbuf pointer and DMA map.
			 */
			map = pkt->pkt_dmamap;
			bus_dmamap_sync(sc->bnx_dmatag, map, 0,
			    map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->bnx_dmatag, map);

			m_freem(pkt->pkt_mbuf);
			DBRUNIF(1, sc->tx_mbuf_alloc--);

			ifp->if_opackets++;

			mutex_enter(&sc->tx_pkt_mtx);
			TAILQ_INSERT_TAIL(&sc->tx_free_pkts, pkt, pkt_entry);
		}
		mutex_exit(&sc->tx_pkt_mtx);

		sc->used_tx_bd--;
		DBPRINT(sc, BNX_INFO_SEND, "%s(%d) used_tx_bd %d\n",
			__FILE__, __LINE__, sc->used_tx_bd);

		sw_tx_cons = NEXT_TX_BD(sw_tx_cons);

		/* Refresh hw_cons to see if there's new work. */
		hw_tx_cons = sc->hw_tx_cons =
		    sblk->status_tx_quick_consumer_index0;
		if ((hw_tx_cons & USABLE_TX_BD_PER_PAGE) ==
		    USABLE_TX_BD_PER_PAGE)
			hw_tx_cons++;

		/* Prevent speculative reads from getting ahead of
		 * the status block.
		 */
		bus_space_barrier(sc->bnx_btag, sc->bnx_bhandle, 0, 0,
		    BUS_SPACE_BARRIER_READ);
	}

	/* Clear the TX timeout timer. */
	ifp->if_timer = 0;

	/* Clear the tx hardware queue full flag. */
	if (sc->used_tx_bd < sc->max_tx_bd) {
		DBRUNIF((ifp->if_flags & IFF_OACTIVE),
		    aprint_debug_dev(sc->bnx_dev,
		        "Open TX chain! %d/%d (used/total)\n",
			sc->used_tx_bd, sc->max_tx_bd));
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	sc->tx_cons = sw_tx_cons;
}

/****************************************************************************/
/* Disables interrupt generation.                                           */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_disable_intr(struct bnx_softc *sc)
{
	REG_WR(sc, BNX_PCICFG_INT_ACK_CMD, BNX_PCICFG_INT_ACK_CMD_MASK_INT);
	REG_RD(sc, BNX_PCICFG_INT_ACK_CMD);
}

/****************************************************************************/
/* Enables interrupt generation.                                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_enable_intr(struct bnx_softc *sc)
{
	uint32_t		val;

	REG_WR(sc, BNX_PCICFG_INT_ACK_CMD, BNX_PCICFG_INT_ACK_CMD_INDEX_VALID |
	    BNX_PCICFG_INT_ACK_CMD_MASK_INT | sc->last_status_idx);

	REG_WR(sc, BNX_PCICFG_INT_ACK_CMD, BNX_PCICFG_INT_ACK_CMD_INDEX_VALID |
	    sc->last_status_idx);

	val = REG_RD(sc, BNX_HC_COMMAND);
	REG_WR(sc, BNX_HC_COMMAND, val | BNX_HC_COMMAND_COAL_NOW);
}

/****************************************************************************/
/* Handles controller initialization.                                       */
/*                                                                          */
/****************************************************************************/
int
bnx_init(struct ifnet *ifp)
{
	struct bnx_softc	*sc = ifp->if_softc;
	uint32_t		ether_mtu;
	int			s, error = 0;

	DBPRINT(sc, BNX_VERBOSE_RESET, "Entering %s()\n", __func__);

	s = splnet();

	bnx_stop(ifp, 0);

	if ((error = bnx_reset(sc, BNX_DRV_MSG_CODE_RESET)) != 0) {
		aprint_error_dev(sc->bnx_dev,
		    "Controller reset failed!\n");
		goto bnx_init_exit;
	}

	if ((error = bnx_chipinit(sc)) != 0) {
		aprint_error_dev(sc->bnx_dev,
		    "Controller initialization failed!\n");
		goto bnx_init_exit;
	}

	if ((error = bnx_blockinit(sc)) != 0) {
		aprint_error_dev(sc->bnx_dev,
		    "Block initialization failed!\n");
		goto bnx_init_exit;
	}

	/* Calculate and program the Ethernet MRU size. */
	if (ifp->if_mtu <= ETHERMTU) {
		ether_mtu = BNX_MAX_STD_ETHER_MTU_VLAN;
		sc->mbuf_alloc_size = MCLBYTES;
	} else {
		ether_mtu = BNX_MAX_JUMBO_ETHER_MTU_VLAN;
		sc->mbuf_alloc_size = BNX_MAX_JUMBO_MRU;
	}


	DBPRINT(sc, BNX_INFO, "%s(): setting MRU = %d\n",
	    __func__, ether_mtu);

	/*
	 * Program the MRU and enable Jumbo frame
	 * support.
	 */
	REG_WR(sc, BNX_EMAC_RX_MTU_SIZE, ether_mtu |
		BNX_EMAC_RX_MTU_SIZE_JUMBO_ENA);

	/* Calculate the RX Ethernet frame size for rx_bd's. */
	sc->max_frame_size = sizeof(struct l2_fhdr) + 2 + ether_mtu + 8;

	DBPRINT(sc, BNX_INFO, "%s(): mclbytes = %d, mbuf_alloc_size = %d, "
	    "max_frame_size = %d\n", __func__, (int)MCLBYTES,
	    sc->mbuf_alloc_size, sc->max_frame_size);

	/* Program appropriate promiscuous/multicast filtering. */
	bnx_iff(sc);

	/* Init RX buffer descriptor chain. */
	bnx_init_rx_chain(sc);

	/* Init TX buffer descriptor chain. */
	bnx_init_tx_chain(sc);

	/* Enable host interrupts. */
	bnx_enable_intr(sc);

	if ((error = ether_mediachange(ifp)) != 0)
		goto bnx_init_exit;

	SET(ifp->if_flags, IFF_RUNNING);
	CLR(ifp->if_flags, IFF_OACTIVE);

	callout_reset(&sc->bnx_timeout, hz, bnx_tick, sc);

bnx_init_exit:
	DBPRINT(sc, BNX_VERBOSE_RESET, "Exiting %s()\n", __func__);

	splx(s);

	return error;
}

/****************************************************************************/
/* Encapsultes an mbuf cluster into the tx_bd chain structure and makes the */
/* memory visible to the controller.                                        */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_tx_encap(struct bnx_softc *sc, struct mbuf *m)
{
	struct bnx_pkt		*pkt;
	bus_dmamap_t		map;
	struct tx_bd		*txbd = NULL;
	uint16_t		vlan_tag = 0, flags = 0;
	uint16_t		chain_prod, prod;
#ifdef BNX_DEBUG
	uint16_t		debug_prod;
#endif
	uint32_t		addr, prod_bseq;
	int			i, error;
	struct m_tag		*mtag;
	static struct work	bnx_wk; /* Dummy work. Statically allocated. */

	mutex_enter(&sc->tx_pkt_mtx);
	pkt = TAILQ_FIRST(&sc->tx_free_pkts);
	if (pkt == NULL) {
		if (!ISSET(sc->bnx_ec.ec_if.if_flags, IFF_UP)) {
			mutex_exit(&sc->tx_pkt_mtx);
			return ENETDOWN;
		}

		if (sc->tx_pkt_count <= TOTAL_TX_BD &&
		    !ISSET(sc->bnx_flags, BNX_ALLOC_PKTS_FLAG)) {
			workqueue_enqueue(sc->bnx_wq, &bnx_wk, NULL);
			SET(sc->bnx_flags, BNX_ALLOC_PKTS_FLAG);
		}

		mutex_exit(&sc->tx_pkt_mtx);
		return ENOMEM;
	}
	TAILQ_REMOVE(&sc->tx_free_pkts, pkt, pkt_entry);
	mutex_exit(&sc->tx_pkt_mtx);

	/* Transfer any checksum offload flags to the bd. */
	if (m->m_pkthdr.csum_flags) {
		if (m->m_pkthdr.csum_flags & M_CSUM_IPv4)
			flags |= TX_BD_FLAGS_IP_CKSUM;
		if (m->m_pkthdr.csum_flags &
		    (M_CSUM_TCPv4 | M_CSUM_UDPv4))
			flags |= TX_BD_FLAGS_TCP_UDP_CKSUM;
	}

	/* Transfer any VLAN tags to the bd. */
	mtag = VLAN_OUTPUT_TAG(&sc->bnx_ec, m);
	if (mtag != NULL) {
		flags |= TX_BD_FLAGS_VLAN_TAG;
		vlan_tag = VLAN_TAG_VALUE(mtag);
	}

	/* Map the mbuf into DMAable memory. */
	prod = sc->tx_prod;
	chain_prod = TX_CHAIN_IDX(prod);
	map = pkt->pkt_dmamap;

	/* Map the mbuf into our DMA address space. */
	error = bus_dmamap_load_mbuf(sc->bnx_dmatag, map, m, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->bnx_dev,
		    "Error mapping mbuf into TX chain!\n");
		sc->tx_dma_map_failures++;
		goto maperr;
	}
	bus_dmamap_sync(sc->bnx_dmatag, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
        /* Make sure there's room in the chain */
	if (map->dm_nsegs > (sc->max_tx_bd - sc->used_tx_bd))
                goto nospace;

	/* prod points to an empty tx_bd at this point. */
	prod_bseq = sc->tx_prod_bseq;
#ifdef BNX_DEBUG
	debug_prod = chain_prod;
#endif
	DBPRINT(sc, BNX_INFO_SEND,
		"%s(): Start: prod = 0x%04X, chain_prod = %04X, "
		"prod_bseq = 0x%08X\n",
		__func__, prod, chain_prod, prod_bseq);

	/*
	 * Cycle through each mbuf segment that makes up
	 * the outgoing frame, gathering the mapping info
	 * for that segment and creating a tx_bd for the
	 * mbuf.
	 */
	for (i = 0; i < map->dm_nsegs ; i++) {
		chain_prod = TX_CHAIN_IDX(prod);
		txbd = &sc->tx_bd_chain[TX_PAGE(chain_prod)][TX_IDX(chain_prod)];

		addr = (uint32_t)map->dm_segs[i].ds_addr;
		txbd->tx_bd_haddr_lo = addr;
		addr = (uint32_t)((uint64_t)map->dm_segs[i].ds_addr >> 32);
		txbd->tx_bd_haddr_hi = addr;
		txbd->tx_bd_mss_nbytes = map->dm_segs[i].ds_len;
		txbd->tx_bd_vlan_tag = vlan_tag;
		txbd->tx_bd_flags = flags;
		prod_bseq += map->dm_segs[i].ds_len;
		if (i == 0)
			txbd->tx_bd_flags |= TX_BD_FLAGS_START;
		prod = NEXT_TX_BD(prod);
	}
	/* Set the END flag on the last TX buffer descriptor. */
	txbd->tx_bd_flags |= TX_BD_FLAGS_END;

	DBRUN(BNX_INFO_SEND, bnx_dump_tx_chain(sc, debug_prod, map->dm_nsegs));

	DBPRINT(sc, BNX_INFO_SEND,
		"%s(): End: prod = 0x%04X, chain_prod = %04X, "
		"prod_bseq = 0x%08X\n",
		__func__, prod, chain_prod, prod_bseq);

	pkt->pkt_mbuf = m;
	pkt->pkt_end_desc = chain_prod;

	mutex_enter(&sc->tx_pkt_mtx);
	TAILQ_INSERT_TAIL(&sc->tx_used_pkts, pkt, pkt_entry);
	mutex_exit(&sc->tx_pkt_mtx);

	sc->used_tx_bd += map->dm_nsegs;
	DBPRINT(sc, BNX_INFO_SEND, "%s(%d) used_tx_bd %d\n",
		__FILE__, __LINE__, sc->used_tx_bd);

	/* Update some debug statistics counters */
	DBRUNIF((sc->used_tx_bd > sc->tx_hi_watermark),
	    sc->tx_hi_watermark = sc->used_tx_bd);
	DBRUNIF(sc->used_tx_bd == sc->max_tx_bd, sc->tx_full_count++);
	DBRUNIF(1, sc->tx_mbuf_alloc++);

	DBRUN(BNX_VERBOSE_SEND, bnx_dump_tx_mbuf_chain(sc, chain_prod,
	    map->dm_nsegs));

	/* prod points to the next free tx_bd at this point. */
	sc->tx_prod = prod;
	sc->tx_prod_bseq = prod_bseq;

	return 0;


nospace:
	bus_dmamap_unload(sc->bnx_dmatag, map);
maperr:
	mutex_enter(&sc->tx_pkt_mtx);
	TAILQ_INSERT_TAIL(&sc->tx_free_pkts, pkt, pkt_entry);
	mutex_exit(&sc->tx_pkt_mtx);

	return ENOMEM;
}

/****************************************************************************/
/* Main transmit routine.                                                   */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_start(struct ifnet *ifp)
{
	struct bnx_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;
	int			count = 0;
#ifdef BNX_DEBUG
	uint16_t		tx_chain_prod;
#endif

	/* If there's no link or the transmit queue is empty then just exit. */
	if ((ifp->if_flags & (IFF_OACTIVE|IFF_RUNNING)) != IFF_RUNNING) {
		DBPRINT(sc, BNX_INFO_SEND,
		    "%s(): output active or device not running.\n", __func__);
		goto bnx_start_exit;
	}

	/* prod points to the next free tx_bd. */
#ifdef BNX_DEBUG
	tx_chain_prod = TX_CHAIN_IDX(sc->tx_prod);
#endif

	DBPRINT(sc, BNX_INFO_SEND, "%s(): Start: tx_prod = 0x%04X, "
	    "tx_chain_prod = %04X, tx_prod_bseq = 0x%08X, "
	    "used_tx %d max_tx %d\n",
	    __func__, sc->tx_prod, tx_chain_prod, sc->tx_prod_bseq,
	    sc->used_tx_bd, sc->max_tx_bd);

	/*
	 * Keep adding entries while there is space in the ring.
	 */
	while (sc->used_tx_bd < sc->max_tx_bd) {
		/* Check for any frames to send. */
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag to wait
		 * for the NIC to drain the chain.
		 */
		if (bnx_tx_encap(sc, m_head)) {
			ifp->if_flags |= IFF_OACTIVE;
			DBPRINT(sc, BNX_INFO_SEND, "TX chain is closed for "
			    "business! Total tx_bd used = %d\n",
			    sc->used_tx_bd);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		count++;

		/* Send a copy of the frame to any BPF listeners. */
		bpf_mtap(ifp, m_head);
	}

	if (count == 0) {
		/* no packets were dequeued */
		DBPRINT(sc, BNX_VERBOSE_SEND,
		    "%s(): No packets were dequeued\n", __func__);
		goto bnx_start_exit;
	}

	/* Update the driver's counters. */
#ifdef BNX_DEBUG
	tx_chain_prod = TX_CHAIN_IDX(sc->tx_prod);
#endif

	DBPRINT(sc, BNX_INFO_SEND, "%s(): End: tx_prod = 0x%04X, tx_chain_prod "
	    "= 0x%04X, tx_prod_bseq = 0x%08X\n", __func__, sc->tx_prod,
	    tx_chain_prod, sc->tx_prod_bseq);

	/* Start the transmit. */
	REG_WR16(sc, MB_TX_CID_ADDR + BNX_L2CTX_TX_HOST_BIDX, sc->tx_prod);
	REG_WR(sc, MB_TX_CID_ADDR + BNX_L2CTX_TX_HOST_BSEQ, sc->tx_prod_bseq);

	/* Set the tx timeout. */
	ifp->if_timer = BNX_TX_TIMEOUT;

bnx_start_exit:
	return;
}

/****************************************************************************/
/* Handles any IOCTL calls from the operating system.                       */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct bnx_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii = &sc->bnx_mii;
	int			s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, command, data)) != 0)
			break;
		/* XXX set an ifflags callback and let ether_ioctl
		 * handle all of this.
		 */
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				bnx_init(ifp);
		} else if (ifp->if_flags & IFF_RUNNING)
			bnx_stop(ifp, 1);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		DBPRINT(sc, BNX_VERBOSE, "bnx_phy_flags = 0x%08X\n",
		    sc->bnx_phy_flags);

		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;

	default:
		error = ether_ioctl(ifp, command, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			bnx_iff(sc);
		error = 0;
	}

	splx(s);
	return error;
}

/****************************************************************************/
/* Transmit timeout handler.                                                */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_watchdog(struct ifnet *ifp)
{
	struct bnx_softc	*sc = ifp->if_softc;

	DBRUN(BNX_WARN_SEND, bnx_dump_driver_state(sc);
	    bnx_dump_status_block(sc));
	/*
	 * If we are in this routine because of pause frames, then
	 * don't reset the hardware.
	 */
	if (REG_RD(sc, BNX_EMAC_TX_STATUS) & BNX_EMAC_TX_STATUS_XOFFED)
		return;

	aprint_error_dev(sc->bnx_dev, "Watchdog timeout -- resetting!\n");

	/* DBRUN(BNX_FATAL, bnx_breakpoint(sc)); */

	bnx_init(ifp);

	ifp->if_oerrors++;
}

/*
 * Interrupt handler.
 */
/****************************************************************************/
/* Main interrupt entry point.  Verifies that the controller generated the  */
/* interrupt and then calls a separate routine for handle the various       */
/* interrupt causes (PHY, TX, RX).                                          */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
int
bnx_intr(void *xsc)
{
	struct bnx_softc	*sc;
	struct ifnet		*ifp;
	uint32_t		status_attn_bits;
	const struct status_block *sblk;

	sc = xsc;

	ifp = &sc->bnx_ec.ec_if;

	if (!device_is_active(sc->bnx_dev) ||
	    (ifp->if_flags & IFF_RUNNING) == 0)
		return 0;

	DBRUNIF(1, sc->interrupts_generated++);

	bus_dmamap_sync(sc->bnx_dmatag, sc->status_map, 0,
	    sc->status_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);

	/*
	 * If the hardware status block index
	 * matches the last value read by the
	 * driver and we haven't asserted our
	 * interrupt then there's nothing to do.
	 */
	if ((sc->status_block->status_idx == sc->last_status_idx) &&
	    (REG_RD(sc, BNX_PCICFG_MISC_STATUS) &
	    BNX_PCICFG_MISC_STATUS_INTA_VALUE))
		return 0;

	/* Ack the interrupt and stop others from occuring. */
	REG_WR(sc, BNX_PCICFG_INT_ACK_CMD,
	    BNX_PCICFG_INT_ACK_CMD_USE_INT_HC_PARAM |
	    BNX_PCICFG_INT_ACK_CMD_MASK_INT);

	/* Keep processing data as long as there is work to do. */
	for (;;) {
		sblk = sc->status_block;
		status_attn_bits = sblk->status_attn_bits;

		DBRUNIF(DB_RANDOMTRUE(bnx_debug_unexpected_attention),
		    aprint_debug("Simulating unexpected status attention bit set.");
		    status_attn_bits = status_attn_bits |
		    STATUS_ATTN_BITS_PARITY_ERROR);

		/* Was it a link change interrupt? */
		if ((status_attn_bits & STATUS_ATTN_BITS_LINK_STATE) !=
		    (sblk->status_attn_bits_ack &
		    STATUS_ATTN_BITS_LINK_STATE))
			bnx_phy_intr(sc);

		/* If any other attention is asserted then the chip is toast. */
		if (((status_attn_bits & ~STATUS_ATTN_BITS_LINK_STATE) !=
		    (sblk->status_attn_bits_ack &
		    ~STATUS_ATTN_BITS_LINK_STATE))) {
			DBRUN(1, sc->unexpected_attentions++);

			BNX_PRINTF(sc,
			    "Fatal attention detected: 0x%08X\n",
			    sblk->status_attn_bits);

			DBRUN(BNX_FATAL,
			    if (bnx_debug_unexpected_attention == 0)
			    bnx_breakpoint(sc));

			bnx_init(ifp);
			return 1;
		}

		/* Check for any completed RX frames. */
		if (sblk->status_rx_quick_consumer_index0 !=
		    sc->hw_rx_cons)
			bnx_rx_intr(sc);

		/* Check for any completed TX frames. */
		if (sblk->status_tx_quick_consumer_index0 !=
		    sc->hw_tx_cons)
			bnx_tx_intr(sc);

		/*
		 * Save the status block index value for use during the
		 * next interrupt.
		 */
		sc->last_status_idx = sblk->status_idx;

		/* Prevent speculative reads from getting ahead of the
		 * status block.
		 */
		bus_space_barrier(sc->bnx_btag, sc->bnx_bhandle, 0, 0,
		    BUS_SPACE_BARRIER_READ);

		/* If there's no work left then exit the isr. */
		if ((sblk->status_rx_quick_consumer_index0 ==
			sc->hw_rx_cons) &&
		    (sblk->status_tx_quick_consumer_index0 == sc->hw_tx_cons))
			break;
	}

	bus_dmamap_sync(sc->bnx_dmatag, sc->status_map, 0,
	    sc->status_map->dm_mapsize, BUS_DMASYNC_PREWRITE);

	/* Re-enable interrupts. */
	REG_WR(sc, BNX_PCICFG_INT_ACK_CMD,
	    BNX_PCICFG_INT_ACK_CMD_INDEX_VALID | sc->last_status_idx |
	    BNX_PCICFG_INT_ACK_CMD_MASK_INT);
	REG_WR(sc, BNX_PCICFG_INT_ACK_CMD,
	    BNX_PCICFG_INT_ACK_CMD_INDEX_VALID | sc->last_status_idx);

	/* Handle any frames that arrived while handling the interrupt. */
	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		bnx_start(ifp);

	return 1;
}

/****************************************************************************/
/* Programs the various packet receive modes (broadcast and multicast).     */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_iff(struct bnx_softc *sc)
{
	struct ethercom		*ec = &sc->bnx_ec;
	struct ifnet		*ifp = &ec->ec_if;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	uint32_t		hashes[NUM_MC_HASH_REGISTERS] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	uint32_t		rx_mode, sort_mode;
	int			h, i;

	/* Initialize receive mode default settings. */
	rx_mode = sc->rx_mode & ~(BNX_EMAC_RX_MODE_PROMISCUOUS |
	    BNX_EMAC_RX_MODE_KEEP_VLAN_TAG);
	sort_mode = 1 | BNX_RPM_SORT_USER0_BC_EN;
	ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * ASF/IPMI/UMP firmware requires that VLAN tag stripping
	 * be enbled.
	 */
	if (!(sc->bnx_flags & BNX_MFW_ENABLE_FLAG))
		rx_mode |= BNX_EMAC_RX_MODE_KEEP_VLAN_TAG;

	/*
	 * Check for promiscuous, all multicast, or selected
	 * multicast address filtering.
	 */
	if (ifp->if_flags & IFF_PROMISC) {
		DBPRINT(sc, BNX_INFO, "Enabling promiscuous mode.\n");

		ifp->if_flags |= IFF_ALLMULTI;
		/* Enable promiscuous mode. */
		rx_mode |= BNX_EMAC_RX_MODE_PROMISCUOUS;
		sort_mode |= BNX_RPM_SORT_USER0_PROM_EN;
	} else if (ifp->if_flags & IFF_ALLMULTI) {
allmulti:
		DBPRINT(sc, BNX_INFO, "Enabling all multicast mode.\n");

		ifp->if_flags |= IFF_ALLMULTI;
		/* Enable all multicast addresses. */
		for (i = 0; i < NUM_MC_HASH_REGISTERS; i++)
			REG_WR(sc, BNX_EMAC_MULTICAST_HASH0 + (i * 4),
			    0xffffffff);
		sort_mode |= BNX_RPM_SORT_USER0_MC_EN;
	} else {
		/* Accept one or more multicast(s). */
		DBPRINT(sc, BNX_INFO, "Enabling selective multicast mode.\n");

		ETHER_FIRST_MULTI(step, ec, enm);
		while (enm != NULL) {
			if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
			    ETHER_ADDR_LEN)) {
				goto allmulti;
			}
			h = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN) &
			    0xFF;
			hashes[(h & 0xE0) >> 5] |= 1 << (h & 0x1F);
			ETHER_NEXT_MULTI(step, enm);
		}

		for (i = 0; i < NUM_MC_HASH_REGISTERS; i++)
			REG_WR(sc, BNX_EMAC_MULTICAST_HASH0 + (i * 4),
			    hashes[i]);

		sort_mode |= BNX_RPM_SORT_USER0_MC_HSH_EN;
	}

	/* Only make changes if the recive mode has actually changed. */
	if (rx_mode != sc->rx_mode) {
		DBPRINT(sc, BNX_VERBOSE, "Enabling new receive mode: 0x%08X\n",
		    rx_mode);

		sc->rx_mode = rx_mode;
		REG_WR(sc, BNX_EMAC_RX_MODE, rx_mode);
	}

	/* Disable and clear the exisitng sort before enabling a new sort. */
	REG_WR(sc, BNX_RPM_SORT_USER0, 0x0);
	REG_WR(sc, BNX_RPM_SORT_USER0, sort_mode);
	REG_WR(sc, BNX_RPM_SORT_USER0, sort_mode | BNX_RPM_SORT_USER0_ENA);
}

/****************************************************************************/
/* Called periodically to updates statistics from the controllers           */
/* statistics block.                                                        */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_stats_update(struct bnx_softc *sc)
{
	struct ifnet		*ifp = &sc->bnx_ec.ec_if;
	struct statistics_block	*stats;

	DBPRINT(sc, BNX_EXCESSIVE, "Entering %s()\n", __func__);
	bus_dmamap_sync(sc->bnx_dmatag, sc->status_map, 0, BNX_STATUS_BLK_SZ,
	    BUS_DMASYNC_POSTREAD);

	stats = (struct statistics_block *)sc->stats_block;

	/*
	 * Update the interface statistics from the
	 * hardware statistics.
	 */
	ifp->if_collisions = (u_long)stats->stat_EtherStatsCollisions;

	ifp->if_ierrors = (u_long)stats->stat_EtherStatsUndersizePkts +
	    (u_long)stats->stat_EtherStatsOverrsizePkts +
	    (u_long)stats->stat_IfInMBUFDiscards +
	    (u_long)stats->stat_Dot3StatsAlignmentErrors +
	    (u_long)stats->stat_Dot3StatsFCSErrors;

	ifp->if_oerrors = (u_long)
	    stats->stat_emac_tx_stat_dot3statsinternalmactransmiterrors +
	    (u_long)stats->stat_Dot3StatsExcessiveCollisions +
	    (u_long)stats->stat_Dot3StatsLateCollisions;

	/*
	 * Certain controllers don't report
	 * carrier sense errors correctly.
	 * See errata E11_5708CA0_1165.
	 */
	if (!(BNX_CHIP_NUM(sc) == BNX_CHIP_NUM_5706) &&
	    !(BNX_CHIP_ID(sc) == BNX_CHIP_ID_5708_A0))
		ifp->if_oerrors += (u_long) stats->stat_Dot3StatsCarrierSenseErrors;

	/*
	 * Update the sysctl statistics from the
	 * hardware statistics.
	 */
	sc->stat_IfHCInOctets = ((uint64_t)stats->stat_IfHCInOctets_hi << 32) +
	    (uint64_t) stats->stat_IfHCInOctets_lo;

	sc->stat_IfHCInBadOctets =
	    ((uint64_t) stats->stat_IfHCInBadOctets_hi << 32) +
	    (uint64_t) stats->stat_IfHCInBadOctets_lo;

	sc->stat_IfHCOutOctets =
	    ((uint64_t) stats->stat_IfHCOutOctets_hi << 32) +
	    (uint64_t) stats->stat_IfHCOutOctets_lo;

	sc->stat_IfHCOutBadOctets =
	    ((uint64_t) stats->stat_IfHCOutBadOctets_hi << 32) +
	    (uint64_t) stats->stat_IfHCOutBadOctets_lo;

	sc->stat_IfHCInUcastPkts =
	    ((uint64_t) stats->stat_IfHCInUcastPkts_hi << 32) +
	    (uint64_t) stats->stat_IfHCInUcastPkts_lo;

	sc->stat_IfHCInMulticastPkts =
	    ((uint64_t) stats->stat_IfHCInMulticastPkts_hi << 32) +
	    (uint64_t) stats->stat_IfHCInMulticastPkts_lo;

	sc->stat_IfHCInBroadcastPkts =
	    ((uint64_t) stats->stat_IfHCInBroadcastPkts_hi << 32) +
	    (uint64_t) stats->stat_IfHCInBroadcastPkts_lo;

	sc->stat_IfHCOutUcastPkts =
	   ((uint64_t) stats->stat_IfHCOutUcastPkts_hi << 32) +
	    (uint64_t) stats->stat_IfHCOutUcastPkts_lo;

	sc->stat_IfHCOutMulticastPkts =
	    ((uint64_t) stats->stat_IfHCOutMulticastPkts_hi << 32) +
	    (uint64_t) stats->stat_IfHCOutMulticastPkts_lo;

	sc->stat_IfHCOutBroadcastPkts =
	    ((uint64_t) stats->stat_IfHCOutBroadcastPkts_hi << 32) +
	    (uint64_t) stats->stat_IfHCOutBroadcastPkts_lo;

	sc->stat_emac_tx_stat_dot3statsinternalmactransmiterrors =
	    stats->stat_emac_tx_stat_dot3statsinternalmactransmiterrors;

	sc->stat_Dot3StatsCarrierSenseErrors =
	    stats->stat_Dot3StatsCarrierSenseErrors;

	sc->stat_Dot3StatsFCSErrors = stats->stat_Dot3StatsFCSErrors;

	sc->stat_Dot3StatsAlignmentErrors =
	    stats->stat_Dot3StatsAlignmentErrors;

	sc->stat_Dot3StatsSingleCollisionFrames =
	    stats->stat_Dot3StatsSingleCollisionFrames;

	sc->stat_Dot3StatsMultipleCollisionFrames =
	    stats->stat_Dot3StatsMultipleCollisionFrames;

	sc->stat_Dot3StatsDeferredTransmissions =
	    stats->stat_Dot3StatsDeferredTransmissions;

	sc->stat_Dot3StatsExcessiveCollisions =
	    stats->stat_Dot3StatsExcessiveCollisions;

	sc->stat_Dot3StatsLateCollisions = stats->stat_Dot3StatsLateCollisions;

	sc->stat_EtherStatsCollisions = stats->stat_EtherStatsCollisions;

	sc->stat_EtherStatsFragments = stats->stat_EtherStatsFragments;

	sc->stat_EtherStatsJabbers = stats->stat_EtherStatsJabbers;

	sc->stat_EtherStatsUndersizePkts = stats->stat_EtherStatsUndersizePkts;

	sc->stat_EtherStatsOverrsizePkts = stats->stat_EtherStatsOverrsizePkts;

	sc->stat_EtherStatsPktsRx64Octets =
	    stats->stat_EtherStatsPktsRx64Octets;

	sc->stat_EtherStatsPktsRx65Octetsto127Octets =
	    stats->stat_EtherStatsPktsRx65Octetsto127Octets;

	sc->stat_EtherStatsPktsRx128Octetsto255Octets =
	    stats->stat_EtherStatsPktsRx128Octetsto255Octets;

	sc->stat_EtherStatsPktsRx256Octetsto511Octets =
	    stats->stat_EtherStatsPktsRx256Octetsto511Octets;

	sc->stat_EtherStatsPktsRx512Octetsto1023Octets =
	    stats->stat_EtherStatsPktsRx512Octetsto1023Octets;

	sc->stat_EtherStatsPktsRx1024Octetsto1522Octets =
	    stats->stat_EtherStatsPktsRx1024Octetsto1522Octets;

	sc->stat_EtherStatsPktsRx1523Octetsto9022Octets =
	    stats->stat_EtherStatsPktsRx1523Octetsto9022Octets;

	sc->stat_EtherStatsPktsTx64Octets =
	    stats->stat_EtherStatsPktsTx64Octets;

	sc->stat_EtherStatsPktsTx65Octetsto127Octets =
	    stats->stat_EtherStatsPktsTx65Octetsto127Octets;

	sc->stat_EtherStatsPktsTx128Octetsto255Octets =
	    stats->stat_EtherStatsPktsTx128Octetsto255Octets;

	sc->stat_EtherStatsPktsTx256Octetsto511Octets =
	    stats->stat_EtherStatsPktsTx256Octetsto511Octets;

	sc->stat_EtherStatsPktsTx512Octetsto1023Octets =
	    stats->stat_EtherStatsPktsTx512Octetsto1023Octets;

	sc->stat_EtherStatsPktsTx1024Octetsto1522Octets =
	    stats->stat_EtherStatsPktsTx1024Octetsto1522Octets;

	sc->stat_EtherStatsPktsTx1523Octetsto9022Octets =
	    stats->stat_EtherStatsPktsTx1523Octetsto9022Octets;

	sc->stat_XonPauseFramesReceived = stats->stat_XonPauseFramesReceived;

	sc->stat_XoffPauseFramesReceived = stats->stat_XoffPauseFramesReceived;

	sc->stat_OutXonSent = stats->stat_OutXonSent;

	sc->stat_OutXoffSent = stats->stat_OutXoffSent;

	sc->stat_FlowControlDone = stats->stat_FlowControlDone;

	sc->stat_MacControlFramesReceived =
	    stats->stat_MacControlFramesReceived;

	sc->stat_XoffStateEntered = stats->stat_XoffStateEntered;

	sc->stat_IfInFramesL2FilterDiscards =
	    stats->stat_IfInFramesL2FilterDiscards;

	sc->stat_IfInRuleCheckerDiscards = stats->stat_IfInRuleCheckerDiscards;

	sc->stat_IfInFTQDiscards = stats->stat_IfInFTQDiscards;

	sc->stat_IfInMBUFDiscards = stats->stat_IfInMBUFDiscards;

	sc->stat_IfInRuleCheckerP4Hit = stats->stat_IfInRuleCheckerP4Hit;

	sc->stat_CatchupInRuleCheckerDiscards =
	    stats->stat_CatchupInRuleCheckerDiscards;

	sc->stat_CatchupInFTQDiscards = stats->stat_CatchupInFTQDiscards;

	sc->stat_CatchupInMBUFDiscards = stats->stat_CatchupInMBUFDiscards;

	sc->stat_CatchupInRuleCheckerP4Hit =
	    stats->stat_CatchupInRuleCheckerP4Hit;

	DBPRINT(sc, BNX_EXCESSIVE, "Exiting %s()\n", __func__);
}

void
bnx_tick(void *xsc)
{
	struct bnx_softc	*sc = xsc;
	struct mii_data		*mii;
	uint32_t		msg;
	uint16_t		prod, chain_prod;
	uint32_t		prod_bseq;
	int s = splnet();

	/* Tell the firmware that the driver is still running. */
#ifdef BNX_DEBUG
	msg = (uint32_t)BNX_DRV_MSG_DATA_PULSE_CODE_ALWAYS_ALIVE;
#else
	msg = (uint32_t)++sc->bnx_fw_drv_pulse_wr_seq;
#endif
	REG_WR_IND(sc, sc->bnx_shmem_base + BNX_DRV_PULSE_MB, msg);

	/* Update the statistics from the hardware statistics block. */
	bnx_stats_update(sc);

	/* Schedule the next tick. */
	callout_reset(&sc->bnx_timeout, hz, bnx_tick, sc);

	mii = &sc->bnx_mii;
	mii_tick(mii);

	/* try to get more RX buffers, just in case */
	prod = sc->rx_prod;
	prod_bseq = sc->rx_prod_bseq;
	chain_prod = RX_CHAIN_IDX(prod);
	bnx_get_buf(sc, &prod, &chain_prod, &prod_bseq);
	sc->rx_prod = prod;
	sc->rx_prod_bseq = prod_bseq;
	splx(s);
	return;
}

/****************************************************************************/
/* BNX Debug Routines                                                       */
/****************************************************************************/
#ifdef BNX_DEBUG

/****************************************************************************/
/* Prints out information about an mbuf.                                    */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_dump_mbuf(struct bnx_softc *sc, struct mbuf *m)
{
	struct mbuf		*mp = m;

	if (m == NULL) {
		/* Index out of range. */
		aprint_error("mbuf ptr is null!\n");
		return;
	}

	while (mp) {
		aprint_debug("mbuf: vaddr = %p, m_len = %d, m_flags = ",
		    mp, mp->m_len);

		if (mp->m_flags & M_EXT)
			aprint_debug("M_EXT ");
		if (mp->m_flags & M_PKTHDR)
			aprint_debug("M_PKTHDR ");
		aprint_debug("\n");

		if (mp->m_flags & M_EXT)
			aprint_debug("- m_ext: vaddr = %p, ext_size = 0x%04zX\n",
			    mp, mp->m_ext.ext_size);

		mp = mp->m_next;
	}
}

/****************************************************************************/
/* Prints out the mbufs in the TX mbuf chain.                               */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
void
bnx_dump_tx_mbuf_chain(struct bnx_softc *sc, int chain_prod, int count)
{
#if 0
	struct mbuf		*m;
	int			i;

	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    "  tx mbuf data  "
	    "----------------------------\n");

	for (i = 0; i < count; i++) {
	 	m = sc->tx_mbuf_ptr[chain_prod];
		BNX_PRINTF(sc, "txmbuf[%d]\n", chain_prod);
		bnx_dump_mbuf(sc, m);
		chain_prod = TX_CHAIN_IDX(NEXT_TX_BD(chain_prod));
	}

	aprint_debug_dev(sc->bnx_dev,
	    "--------------------------------------------"
	    "----------------------------\n");
#endif
}

/*
 * This routine prints the RX mbuf chain.
 */
void
bnx_dump_rx_mbuf_chain(struct bnx_softc *sc, int chain_prod, int count)
{
	struct mbuf		*m;
	int			i;

	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    "  rx mbuf data  "
	    "----------------------------\n");

	for (i = 0; i < count; i++) {
	 	m = sc->rx_mbuf_ptr[chain_prod];
		BNX_PRINTF(sc, "rxmbuf[0x%04X]\n", chain_prod);
		bnx_dump_mbuf(sc, m);
		chain_prod = RX_CHAIN_IDX(NEXT_RX_BD(chain_prod));
	}


	aprint_debug_dev(sc->bnx_dev,
	    "--------------------------------------------"
	    "----------------------------\n");
}

void
bnx_dump_txbd(struct bnx_softc *sc, int idx, struct tx_bd *txbd)
{
	if (idx > MAX_TX_BD)
		/* Index out of range. */
		BNX_PRINTF(sc, "tx_bd[0x%04X]: Invalid tx_bd index!\n", idx);
	else if ((idx & USABLE_TX_BD_PER_PAGE) == USABLE_TX_BD_PER_PAGE)
		/* TX Chain page pointer. */
		BNX_PRINTF(sc, "tx_bd[0x%04X]: haddr = 0x%08X:%08X, chain "
		    "page pointer\n", idx, txbd->tx_bd_haddr_hi,
		    txbd->tx_bd_haddr_lo);
	else
		/* Normal tx_bd entry. */
		BNX_PRINTF(sc, "tx_bd[0x%04X]: haddr = 0x%08X:%08X, nbytes = "
		    "0x%08X, vlan tag = 0x%4X, flags = 0x%08X\n", idx,
		    txbd->tx_bd_haddr_hi, txbd->tx_bd_haddr_lo,
		    txbd->tx_bd_mss_nbytes, txbd->tx_bd_vlan_tag,
		    txbd->tx_bd_flags);
}

void
bnx_dump_rxbd(struct bnx_softc *sc, int idx, struct rx_bd *rxbd)
{
	if (idx > MAX_RX_BD)
		/* Index out of range. */
		BNX_PRINTF(sc, "rx_bd[0x%04X]: Invalid rx_bd index!\n", idx);
	else if ((idx & USABLE_RX_BD_PER_PAGE) == USABLE_RX_BD_PER_PAGE)
		/* TX Chain page pointer. */
		BNX_PRINTF(sc, "rx_bd[0x%04X]: haddr = 0x%08X:%08X, chain page "
		    "pointer\n", idx, rxbd->rx_bd_haddr_hi,
		    rxbd->rx_bd_haddr_lo);
	else
		/* Normal tx_bd entry. */
		BNX_PRINTF(sc, "rx_bd[0x%04X]: haddr = 0x%08X:%08X, nbytes = "
		    "0x%08X, flags = 0x%08X\n", idx,
			rxbd->rx_bd_haddr_hi, rxbd->rx_bd_haddr_lo,
			rxbd->rx_bd_len, rxbd->rx_bd_flags);
}

void
bnx_dump_l2fhdr(struct bnx_softc *sc, int idx, struct l2_fhdr *l2fhdr)
{
	BNX_PRINTF(sc, "l2_fhdr[0x%04X]: status = 0x%08X, "
	    "pkt_len = 0x%04X, vlan = 0x%04x, ip_xsum = 0x%04X, "
	    "tcp_udp_xsum = 0x%04X\n", idx,
	    l2fhdr->l2_fhdr_status, l2fhdr->l2_fhdr_pkt_len,
	    l2fhdr->l2_fhdr_vlan_tag, l2fhdr->l2_fhdr_ip_xsum,
	    l2fhdr->l2_fhdr_tcp_udp_xsum);
}

/*
 * This routine prints the TX chain.
 */
void
bnx_dump_tx_chain(struct bnx_softc *sc, int tx_prod, int count)
{
	struct tx_bd		*txbd;
	int			i;

	/* First some info about the tx_bd chain structure. */
	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    "  tx_bd  chain  "
	    "----------------------------\n");

	BNX_PRINTF(sc,
	    "page size      = 0x%08X, tx chain pages        = 0x%08X\n",
	    (uint32_t)BCM_PAGE_SIZE, (uint32_t) TX_PAGES);

	BNX_PRINTF(sc,
	    "tx_bd per page = 0x%08X, usable tx_bd per page = 0x%08X\n",
	    (uint32_t)TOTAL_TX_BD_PER_PAGE, (uint32_t)USABLE_TX_BD_PER_PAGE);

	BNX_PRINTF(sc, "total tx_bd    = 0x%08X\n", TOTAL_TX_BD);

	aprint_error_dev(sc->bnx_dev, ""
	    "-----------------------------"
	    "   tx_bd data   "
	    "-----------------------------\n");

	/* Now print out the tx_bd's themselves. */
	for (i = 0; i < count; i++) {
	 	txbd = &sc->tx_bd_chain[TX_PAGE(tx_prod)][TX_IDX(tx_prod)];
		bnx_dump_txbd(sc, tx_prod, txbd);
		tx_prod = TX_CHAIN_IDX(NEXT_TX_BD(tx_prod));
	}

	aprint_debug_dev(sc->bnx_dev,
	    "-----------------------------"
	    "--------------"
	    "-----------------------------\n");
}

/*
 * This routine prints the RX chain.
 */
void
bnx_dump_rx_chain(struct bnx_softc *sc, int rx_prod, int count)
{
	struct rx_bd		*rxbd;
	int			i;

	/* First some info about the tx_bd chain structure. */
	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    "  rx_bd  chain  "
	    "----------------------------\n");

	aprint_debug_dev(sc->bnx_dev, "----- RX_BD Chain -----\n");

	BNX_PRINTF(sc,
	    "page size      = 0x%08X, rx chain pages        = 0x%08X\n",
	    (uint32_t)BCM_PAGE_SIZE, (uint32_t)RX_PAGES);

	BNX_PRINTF(sc,
	    "rx_bd per page = 0x%08X, usable rx_bd per page = 0x%08X\n",
	    (uint32_t)TOTAL_RX_BD_PER_PAGE, (uint32_t)USABLE_RX_BD_PER_PAGE);

	BNX_PRINTF(sc, "total rx_bd    = 0x%08X\n", TOTAL_RX_BD);

	aprint_error_dev(sc->bnx_dev,
	    "----------------------------"
	    "   rx_bd data   "
	    "----------------------------\n");

	/* Now print out the rx_bd's themselves. */
	for (i = 0; i < count; i++) {
		rxbd = &sc->rx_bd_chain[RX_PAGE(rx_prod)][RX_IDX(rx_prod)];
		bnx_dump_rxbd(sc, rx_prod, rxbd);
		rx_prod = RX_CHAIN_IDX(NEXT_RX_BD(rx_prod));
	}

	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    "--------------"
	    "----------------------------\n");
}

/*
 * This routine prints the status block.
 */
void
bnx_dump_status_block(struct bnx_softc *sc)
{
	struct status_block	*sblk;
	bus_dmamap_sync(sc->bnx_dmatag, sc->status_map, 0, BNX_STATUS_BLK_SZ,
	    BUS_DMASYNC_POSTREAD);

	sblk = sc->status_block;

   	aprint_debug_dev(sc->bnx_dev, "----------------------------- Status Block "
	    "-----------------------------\n");

	BNX_PRINTF(sc,
	    "attn_bits  = 0x%08X, attn_bits_ack = 0x%08X, index = 0x%04X\n",
	    sblk->status_attn_bits, sblk->status_attn_bits_ack,
	    sblk->status_idx);

	BNX_PRINTF(sc, "rx_cons0   = 0x%08X, tx_cons0      = 0x%08X\n",
	    sblk->status_rx_quick_consumer_index0,
	    sblk->status_tx_quick_consumer_index0);

	BNX_PRINTF(sc, "status_idx = 0x%04X\n", sblk->status_idx);

	/* Theses indices are not used for normal L2 drivers. */
	if (sblk->status_rx_quick_consumer_index1 ||
		sblk->status_tx_quick_consumer_index1)
		BNX_PRINTF(sc, "rx_cons1  = 0x%08X, tx_cons1      = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index1,
		    sblk->status_tx_quick_consumer_index1);

	if (sblk->status_rx_quick_consumer_index2 ||
		sblk->status_tx_quick_consumer_index2)
		BNX_PRINTF(sc, "rx_cons2  = 0x%08X, tx_cons2      = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index2,
		    sblk->status_tx_quick_consumer_index2);

	if (sblk->status_rx_quick_consumer_index3 ||
		sblk->status_tx_quick_consumer_index3)
		BNX_PRINTF(sc, "rx_cons3  = 0x%08X, tx_cons3      = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index3,
		    sblk->status_tx_quick_consumer_index3);

	if (sblk->status_rx_quick_consumer_index4 ||
		sblk->status_rx_quick_consumer_index5)
		BNX_PRINTF(sc, "rx_cons4  = 0x%08X, rx_cons5      = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index4,
		    sblk->status_rx_quick_consumer_index5);

	if (sblk->status_rx_quick_consumer_index6 ||
		sblk->status_rx_quick_consumer_index7)
		BNX_PRINTF(sc, "rx_cons6  = 0x%08X, rx_cons7      = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index6,
		    sblk->status_rx_quick_consumer_index7);

	if (sblk->status_rx_quick_consumer_index8 ||
		sblk->status_rx_quick_consumer_index9)
		BNX_PRINTF(sc, "rx_cons8  = 0x%08X, rx_cons9      = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index8,
		    sblk->status_rx_quick_consumer_index9);

	if (sblk->status_rx_quick_consumer_index10 ||
		sblk->status_rx_quick_consumer_index11)
		BNX_PRINTF(sc, "rx_cons10 = 0x%08X, rx_cons11     = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index10,
		    sblk->status_rx_quick_consumer_index11);

	if (sblk->status_rx_quick_consumer_index12 ||
		sblk->status_rx_quick_consumer_index13)
		BNX_PRINTF(sc, "rx_cons12 = 0x%08X, rx_cons13     = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index12,
		    sblk->status_rx_quick_consumer_index13);

	if (sblk->status_rx_quick_consumer_index14 ||
		sblk->status_rx_quick_consumer_index15)
		BNX_PRINTF(sc, "rx_cons14 = 0x%08X, rx_cons15     = 0x%08X\n",
		    sblk->status_rx_quick_consumer_index14,
		    sblk->status_rx_quick_consumer_index15);

	if (sblk->status_completion_producer_index ||
		sblk->status_cmd_consumer_index)
		BNX_PRINTF(sc, "com_prod  = 0x%08X, cmd_cons      = 0x%08X\n",
		    sblk->status_completion_producer_index,
		    sblk->status_cmd_consumer_index);

	aprint_debug_dev(sc->bnx_dev, "-------------------------------------------"
	    "-----------------------------\n");
}

/*
 * This routine prints the statistics block.
 */
void
bnx_dump_stats_block(struct bnx_softc *sc)
{
	struct statistics_block	*sblk;
	bus_dmamap_sync(sc->bnx_dmatag, sc->status_map, 0, BNX_STATUS_BLK_SZ,
	    BUS_DMASYNC_POSTREAD);

	sblk = sc->stats_block;

	aprint_debug_dev(sc->bnx_dev, ""
	    "-----------------------------"
	    " Stats  Block "
	    "-----------------------------\n");

	BNX_PRINTF(sc, "IfHcInOctets         = 0x%08X:%08X, "
	    "IfHcInBadOctets      = 0x%08X:%08X\n",
	    sblk->stat_IfHCInOctets_hi, sblk->stat_IfHCInOctets_lo,
	    sblk->stat_IfHCInBadOctets_hi, sblk->stat_IfHCInBadOctets_lo);

	BNX_PRINTF(sc, "IfHcOutOctets        = 0x%08X:%08X, "
	    "IfHcOutBadOctets     = 0x%08X:%08X\n",
	    sblk->stat_IfHCOutOctets_hi, sblk->stat_IfHCOutOctets_lo,
	    sblk->stat_IfHCOutBadOctets_hi, sblk->stat_IfHCOutBadOctets_lo);

	BNX_PRINTF(sc, "IfHcInUcastPkts      = 0x%08X:%08X, "
	    "IfHcInMulticastPkts  = 0x%08X:%08X\n",
	    sblk->stat_IfHCInUcastPkts_hi, sblk->stat_IfHCInUcastPkts_lo,
	    sblk->stat_IfHCInMulticastPkts_hi,
	    sblk->stat_IfHCInMulticastPkts_lo);

	BNX_PRINTF(sc, "IfHcInBroadcastPkts  = 0x%08X:%08X, "
	    "IfHcOutUcastPkts     = 0x%08X:%08X\n",
	    sblk->stat_IfHCInBroadcastPkts_hi,
	    sblk->stat_IfHCInBroadcastPkts_lo,
	    sblk->stat_IfHCOutUcastPkts_hi,
	    sblk->stat_IfHCOutUcastPkts_lo);

	BNX_PRINTF(sc, "IfHcOutMulticastPkts = 0x%08X:%08X, "
	    "IfHcOutBroadcastPkts = 0x%08X:%08X\n",
	    sblk->stat_IfHCOutMulticastPkts_hi,
	    sblk->stat_IfHCOutMulticastPkts_lo,
	    sblk->stat_IfHCOutBroadcastPkts_hi,
	    sblk->stat_IfHCOutBroadcastPkts_lo);

	if (sblk->stat_emac_tx_stat_dot3statsinternalmactransmiterrors)
		BNX_PRINTF(sc, "0x%08X : "
		    "emac_tx_stat_dot3statsinternalmactransmiterrors\n",
		    sblk->stat_emac_tx_stat_dot3statsinternalmactransmiterrors);

	if (sblk->stat_Dot3StatsCarrierSenseErrors)
		BNX_PRINTF(sc, "0x%08X : Dot3StatsCarrierSenseErrors\n",
		    sblk->stat_Dot3StatsCarrierSenseErrors);

	if (sblk->stat_Dot3StatsFCSErrors)
		BNX_PRINTF(sc, "0x%08X : Dot3StatsFCSErrors\n",
		    sblk->stat_Dot3StatsFCSErrors);

	if (sblk->stat_Dot3StatsAlignmentErrors)
		BNX_PRINTF(sc, "0x%08X : Dot3StatsAlignmentErrors\n",
		    sblk->stat_Dot3StatsAlignmentErrors);

	if (sblk->stat_Dot3StatsSingleCollisionFrames)
		BNX_PRINTF(sc, "0x%08X : Dot3StatsSingleCollisionFrames\n",
		    sblk->stat_Dot3StatsSingleCollisionFrames);

	if (sblk->stat_Dot3StatsMultipleCollisionFrames)
		BNX_PRINTF(sc, "0x%08X : Dot3StatsMultipleCollisionFrames\n",
		    sblk->stat_Dot3StatsMultipleCollisionFrames);

	if (sblk->stat_Dot3StatsDeferredTransmissions)
		BNX_PRINTF(sc, "0x%08X : Dot3StatsDeferredTransmissions\n",
		    sblk->stat_Dot3StatsDeferredTransmissions);

	if (sblk->stat_Dot3StatsExcessiveCollisions)
		BNX_PRINTF(sc, "0x%08X : Dot3StatsExcessiveCollisions\n",
		    sblk->stat_Dot3StatsExcessiveCollisions);

	if (sblk->stat_Dot3StatsLateCollisions)
		BNX_PRINTF(sc, "0x%08X : Dot3StatsLateCollisions\n",
		    sblk->stat_Dot3StatsLateCollisions);

	if (sblk->stat_EtherStatsCollisions)
		BNX_PRINTF(sc, "0x%08X : EtherStatsCollisions\n",
		    sblk->stat_EtherStatsCollisions);

	if (sblk->stat_EtherStatsFragments)
		BNX_PRINTF(sc, "0x%08X : EtherStatsFragments\n",
		    sblk->stat_EtherStatsFragments);

	if (sblk->stat_EtherStatsJabbers)
		BNX_PRINTF(sc, "0x%08X : EtherStatsJabbers\n",
		    sblk->stat_EtherStatsJabbers);

	if (sblk->stat_EtherStatsUndersizePkts)
		BNX_PRINTF(sc, "0x%08X : EtherStatsUndersizePkts\n",
		    sblk->stat_EtherStatsUndersizePkts);

	if (sblk->stat_EtherStatsOverrsizePkts)
		BNX_PRINTF(sc, "0x%08X : EtherStatsOverrsizePkts\n",
		    sblk->stat_EtherStatsOverrsizePkts);

	if (sblk->stat_EtherStatsPktsRx64Octets)
		BNX_PRINTF(sc, "0x%08X : EtherStatsPktsRx64Octets\n",
		    sblk->stat_EtherStatsPktsRx64Octets);

	if (sblk->stat_EtherStatsPktsRx65Octetsto127Octets)
		BNX_PRINTF(sc, "0x%08X : EtherStatsPktsRx65Octetsto127Octets\n",
		    sblk->stat_EtherStatsPktsRx65Octetsto127Octets);

	if (sblk->stat_EtherStatsPktsRx128Octetsto255Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsRx128Octetsto255Octets\n",
		    sblk->stat_EtherStatsPktsRx128Octetsto255Octets);

	if (sblk->stat_EtherStatsPktsRx256Octetsto511Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsRx256Octetsto511Octets\n",
		    sblk->stat_EtherStatsPktsRx256Octetsto511Octets);

	if (sblk->stat_EtherStatsPktsRx512Octetsto1023Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsRx512Octetsto1023Octets\n",
		    sblk->stat_EtherStatsPktsRx512Octetsto1023Octets);

	if (sblk->stat_EtherStatsPktsRx1024Octetsto1522Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsRx1024Octetsto1522Octets\n",
		sblk->stat_EtherStatsPktsRx1024Octetsto1522Octets);

	if (sblk->stat_EtherStatsPktsRx1523Octetsto9022Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsRx1523Octetsto9022Octets\n",
		    sblk->stat_EtherStatsPktsRx1523Octetsto9022Octets);

	if (sblk->stat_EtherStatsPktsTx64Octets)
		BNX_PRINTF(sc, "0x%08X : EtherStatsPktsTx64Octets\n",
		    sblk->stat_EtherStatsPktsTx64Octets);

	if (sblk->stat_EtherStatsPktsTx65Octetsto127Octets)
		BNX_PRINTF(sc, "0x%08X : EtherStatsPktsTx65Octetsto127Octets\n",
		    sblk->stat_EtherStatsPktsTx65Octetsto127Octets);

	if (sblk->stat_EtherStatsPktsTx128Octetsto255Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsTx128Octetsto255Octets\n",
		    sblk->stat_EtherStatsPktsTx128Octetsto255Octets);

	if (sblk->stat_EtherStatsPktsTx256Octetsto511Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsTx256Octetsto511Octets\n",
		    sblk->stat_EtherStatsPktsTx256Octetsto511Octets);

	if (sblk->stat_EtherStatsPktsTx512Octetsto1023Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsTx512Octetsto1023Octets\n",
		    sblk->stat_EtherStatsPktsTx512Octetsto1023Octets);

	if (sblk->stat_EtherStatsPktsTx1024Octetsto1522Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsTx1024Octetsto1522Octets\n",
		    sblk->stat_EtherStatsPktsTx1024Octetsto1522Octets);

	if (sblk->stat_EtherStatsPktsTx1523Octetsto9022Octets)
		BNX_PRINTF(sc, "0x%08X : "
		    "EtherStatsPktsTx1523Octetsto9022Octets\n",
		    sblk->stat_EtherStatsPktsTx1523Octetsto9022Octets);

	if (sblk->stat_XonPauseFramesReceived)
		BNX_PRINTF(sc, "0x%08X : XonPauseFramesReceived\n",
		    sblk->stat_XonPauseFramesReceived);

	if (sblk->stat_XoffPauseFramesReceived)
		BNX_PRINTF(sc, "0x%08X : XoffPauseFramesReceived\n",
		    sblk->stat_XoffPauseFramesReceived);

	if (sblk->stat_OutXonSent)
		BNX_PRINTF(sc, "0x%08X : OutXonSent\n",
		    sblk->stat_OutXonSent);

	if (sblk->stat_OutXoffSent)
		BNX_PRINTF(sc, "0x%08X : OutXoffSent\n",
		    sblk->stat_OutXoffSent);

	if (sblk->stat_FlowControlDone)
		BNX_PRINTF(sc, "0x%08X : FlowControlDone\n",
		    sblk->stat_FlowControlDone);

	if (sblk->stat_MacControlFramesReceived)
		BNX_PRINTF(sc, "0x%08X : MacControlFramesReceived\n",
		    sblk->stat_MacControlFramesReceived);

	if (sblk->stat_XoffStateEntered)
		BNX_PRINTF(sc, "0x%08X : XoffStateEntered\n",
		    sblk->stat_XoffStateEntered);

	if (sblk->stat_IfInFramesL2FilterDiscards)
		BNX_PRINTF(sc, "0x%08X : IfInFramesL2FilterDiscards\n",
		    sblk->stat_IfInFramesL2FilterDiscards);

	if (sblk->stat_IfInRuleCheckerDiscards)
		BNX_PRINTF(sc, "0x%08X : IfInRuleCheckerDiscards\n",
		    sblk->stat_IfInRuleCheckerDiscards);

	if (sblk->stat_IfInFTQDiscards)
		BNX_PRINTF(sc, "0x%08X : IfInFTQDiscards\n",
		    sblk->stat_IfInFTQDiscards);

	if (sblk->stat_IfInMBUFDiscards)
		BNX_PRINTF(sc, "0x%08X : IfInMBUFDiscards\n",
		    sblk->stat_IfInMBUFDiscards);

	if (sblk->stat_IfInRuleCheckerP4Hit)
		BNX_PRINTF(sc, "0x%08X : IfInRuleCheckerP4Hit\n",
		    sblk->stat_IfInRuleCheckerP4Hit);

	if (sblk->stat_CatchupInRuleCheckerDiscards)
		BNX_PRINTF(sc, "0x%08X : CatchupInRuleCheckerDiscards\n",
		    sblk->stat_CatchupInRuleCheckerDiscards);

	if (sblk->stat_CatchupInFTQDiscards)
		BNX_PRINTF(sc, "0x%08X : CatchupInFTQDiscards\n",
		    sblk->stat_CatchupInFTQDiscards);

	if (sblk->stat_CatchupInMBUFDiscards)
		BNX_PRINTF(sc, "0x%08X : CatchupInMBUFDiscards\n",
		    sblk->stat_CatchupInMBUFDiscards);

	if (sblk->stat_CatchupInRuleCheckerP4Hit)
		BNX_PRINTF(sc, "0x%08X : CatchupInRuleCheckerP4Hit\n",
		    sblk->stat_CatchupInRuleCheckerP4Hit);

	aprint_debug_dev(sc->bnx_dev,
	    "-----------------------------"
	    "--------------"
	    "-----------------------------\n");
}

void
bnx_dump_driver_state(struct bnx_softc *sc)
{
	aprint_debug_dev(sc->bnx_dev,
	    "-----------------------------"
	    " Driver State "
	    "-----------------------------\n");

	BNX_PRINTF(sc, "%p - (sc) driver softc structure virtual "
	    "address\n", sc);

	BNX_PRINTF(sc, "%p - (sc->status_block) status block virtual address\n",
	    sc->status_block);

	BNX_PRINTF(sc, "%p - (sc->stats_block) statistics block virtual "
	    "address\n", sc->stats_block);

	BNX_PRINTF(sc, "%p - (sc->tx_bd_chain) tx_bd chain virtual "
	    "adddress\n", sc->tx_bd_chain);

#if 0
	BNX_PRINTF(sc, "%p - (sc->rx_bd_chain) rx_bd chain virtual address\n",
	    sc->rx_bd_chain);

	BNX_PRINTF(sc, "%p - (sc->tx_mbuf_ptr) tx mbuf chain virtual address\n",
	    sc->tx_mbuf_ptr);
#endif

	BNX_PRINTF(sc, "%p - (sc->rx_mbuf_ptr) rx mbuf chain virtual address\n",
	    sc->rx_mbuf_ptr);

	BNX_PRINTF(sc,
	    "         0x%08X - (sc->interrupts_generated) h/w intrs\n",
	    sc->interrupts_generated);

	BNX_PRINTF(sc,
	    "         0x%08X - (sc->rx_interrupts) rx interrupts handled\n",
	    sc->rx_interrupts);

	BNX_PRINTF(sc,
	    "         0x%08X - (sc->tx_interrupts) tx interrupts handled\n",
	    sc->tx_interrupts);

	BNX_PRINTF(sc,
	    "         0x%08X - (sc->last_status_idx) status block index\n",
	    sc->last_status_idx);

	BNX_PRINTF(sc, "         0x%08X - (sc->tx_prod) tx producer index\n",
	    sc->tx_prod);

	BNX_PRINTF(sc, "         0x%08X - (sc->tx_cons) tx consumer index\n",
	    sc->tx_cons);

	BNX_PRINTF(sc,
	    "         0x%08X - (sc->tx_prod_bseq) tx producer bseq index\n",
	    sc->tx_prod_bseq);
	BNX_PRINTF(sc,
	    "	 0x%08X - (sc->tx_mbuf_alloc) tx mbufs allocated\n",
	    sc->tx_mbuf_alloc);

	BNX_PRINTF(sc,
	    "	 0x%08X - (sc->used_tx_bd) used tx_bd's\n",
	    sc->used_tx_bd);

	BNX_PRINTF(sc,
	    "	 0x%08X/%08X - (sc->tx_hi_watermark) tx hi watermark\n",
	    sc->tx_hi_watermark, sc->max_tx_bd);


	BNX_PRINTF(sc, "         0x%08X - (sc->rx_prod) rx producer index\n",
	    sc->rx_prod);

	BNX_PRINTF(sc, "         0x%08X - (sc->rx_cons) rx consumer index\n",
	    sc->rx_cons);

	BNX_PRINTF(sc,
	    "         0x%08X - (sc->rx_prod_bseq) rx producer bseq index\n",
	    sc->rx_prod_bseq);

	BNX_PRINTF(sc,
	    "         0x%08X - (sc->rx_mbuf_alloc) rx mbufs allocated\n",
	    sc->rx_mbuf_alloc);

	BNX_PRINTF(sc, "         0x%08X - (sc->free_rx_bd) free rx_bd's\n",
	    sc->free_rx_bd);

	BNX_PRINTF(sc,
	    "0x%08X/%08X - (sc->rx_low_watermark) rx low watermark\n",
	    sc->rx_low_watermark, sc->max_rx_bd);

	BNX_PRINTF(sc,
	    "         0x%08X - (sc->mbuf_alloc_failed) "
	    "mbuf alloc failures\n",
	    sc->mbuf_alloc_failed);

	BNX_PRINTF(sc,
	    "         0x%0X - (sc->mbuf_sim_allocated_failed) "
	    "simulated mbuf alloc failures\n",
	    sc->mbuf_sim_alloc_failed);

	aprint_debug_dev(sc->bnx_dev, "-------------------------------------------"
	    "-----------------------------\n");
}

void
bnx_dump_hw_state(struct bnx_softc *sc)
{
	uint32_t		val1;
	int			i;

	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    " Hardware State "
	    "----------------------------\n");

	BNX_PRINTF(sc, "0x%08X : bootcode version\n", sc->bnx_fw_ver);

	val1 = REG_RD(sc, BNX_MISC_ENABLE_STATUS_BITS);
	BNX_PRINTF(sc, "0x%08X : (0x%04X) misc_enable_status_bits\n",
	    val1, BNX_MISC_ENABLE_STATUS_BITS);

	val1 = REG_RD(sc, BNX_DMA_STATUS);
	BNX_PRINTF(sc, "0x%08X : (0x%04X) dma_status\n", val1, BNX_DMA_STATUS);

	val1 = REG_RD(sc, BNX_CTX_STATUS);
	BNX_PRINTF(sc, "0x%08X : (0x%04X) ctx_status\n", val1, BNX_CTX_STATUS);

	val1 = REG_RD(sc, BNX_EMAC_STATUS);
	BNX_PRINTF(sc, "0x%08X : (0x%04X) emac_status\n", val1,
	    BNX_EMAC_STATUS);

	val1 = REG_RD(sc, BNX_RPM_STATUS);
	BNX_PRINTF(sc, "0x%08X : (0x%04X) rpm_status\n", val1, BNX_RPM_STATUS);

	val1 = REG_RD(sc, BNX_TBDR_STATUS);
	BNX_PRINTF(sc, "0x%08X : (0x%04X) tbdr_status\n", val1,
	    BNX_TBDR_STATUS);

	val1 = REG_RD(sc, BNX_TDMA_STATUS);
	BNX_PRINTF(sc, "0x%08X : (0x%04X) tdma_status\n", val1,
	    BNX_TDMA_STATUS);

	val1 = REG_RD(sc, BNX_HC_STATUS);
	BNX_PRINTF(sc, "0x%08X : (0x%04X) hc_status\n", val1, BNX_HC_STATUS);

	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");

	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    " Register  Dump "
	    "----------------------------\n");

	for (i = 0x400; i < 0x8000; i += 0x10)
		BNX_PRINTF(sc, "0x%04X: 0x%08X 0x%08X 0x%08X 0x%08X\n",
		    i, REG_RD(sc, i), REG_RD(sc, i + 0x4),
		    REG_RD(sc, i + 0x8), REG_RD(sc, i + 0xC));

	aprint_debug_dev(sc->bnx_dev,
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");
}

void
bnx_breakpoint(struct bnx_softc *sc)
{
	/* Unreachable code to shut the compiler up about unused functions. */
	if (0) {
   		bnx_dump_txbd(sc, 0, NULL);
		bnx_dump_rxbd(sc, 0, NULL);
		bnx_dump_tx_mbuf_chain(sc, 0, USABLE_TX_BD);
		bnx_dump_rx_mbuf_chain(sc, 0, sc->max_rx_bd);
		bnx_dump_l2fhdr(sc, 0, NULL);
		bnx_dump_tx_chain(sc, 0, USABLE_TX_BD);
		bnx_dump_rx_chain(sc, 0, sc->max_rx_bd);
		bnx_dump_status_block(sc);
		bnx_dump_stats_block(sc);
		bnx_dump_driver_state(sc);
		bnx_dump_hw_state(sc);
	}

	bnx_dump_driver_state(sc);
	/* Print the important status block fields. */
	bnx_dump_status_block(sc);

#if 0
	/* Call the debugger. */
	breakpoint();
#endif

	return;
}
#endif
