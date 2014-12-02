/* Attansic/Atheros L2 FastEthernet driver, by D.C. van Moolenbroek */
/*
 * No documentation is available for this card.  The FreeBSD driver is based
 * heavily on the official Linux driver; this driver is based heavily on both.
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <machine/pci.h>
#include <sys/mman.h>
#include <assert.h>

#include "atl2.h"

#define VERBOSE		0	/* Verbose debugging output */
#define ATL2_FKEY	11	/* Use Shift+Fn to dump statistics (0=off) */

#if VERBOSE
#define ATL2_DEBUG(x) printf x
#else
#define ATL2_DEBUG(x)
#endif

typedef struct {
	uint32_t hdr;
	uint32_t vtag;
	uint8_t data[ATL2_RXD_SIZE - sizeof(uint32_t) * 2];
} rxd_t;

static struct {
	int devind;		/* PCI device index */
	int irq;		/* IRQ number */
	int hook_id;		/* IRQ hook ID */
	uint8_t *base;		/* base address of memory-mapped registers */
	uint32_t size;		/* size of memory-mapped area */
	uint32_t hwaddr[2];	/* MAC address, in register representation */

	uint8_t *txd_base;	/* local address of TxD ring buffer base */
	uint32_t *txs_base;	/* local address of TxS ring buffer base */
	uint8_t *rxd_base_u;	/* unaligned base address of RxD ring buffer */
	rxd_t *rxd_base; 	/* local address of RxD ring buffer base */

	int rxd_align;		/* alignment offset of RxD ring buffer */

	vir_bytes txd_phys;	/* physical address of TxD ring buffer */
	vir_bytes txs_phys;	/* physical address of TxS ring buffer */
	vir_bytes rxd_phys;	/* physical address of RxD ring buffer */

	int txd_tail;		/* tail index into TxD, in bytes */
	int txd_num;		/* head-tail offset into TxD, in bytes */
	int txs_tail;		/* tail index into TxS, in elements */
	int txs_num;		/* head-tail offset into TxS, in elements */
	int rxd_tail;		/* tail index into RxD, in elements */

	int rx_avail;		/* is there a packet available for receipt? */

	eth_stat_t stat;	/* statistics */
} state;

#define ATL2_READ_U8(off) (*(volatile uint8_t *)(state.base + (off)))
#define ATL2_READ_U16(off) (*(volatile uint16_t *)(state.base + (off)))
#define ATL2_READ_U32(off) (*(volatile uint32_t *)(state.base + (off)))
#define ATL2_WRITE_U8(off, val) \
	*(volatile uint8_t *)(state.base + (off)) = (val)
#define ATL2_WRITE_U16(off, val) \
	*(volatile uint16_t *)(state.base + (off)) = (val)
#define ATL2_WRITE_U32(off, val) \
	*(volatile uint32_t *)(state.base + (off)) = (val)

#define ATL2_ALIGN_32(n) (((n) + 3) & ~3)

static int atl2_init(unsigned int instance, ether_addr_t *addr);
static void atl2_stop(void);
static void atl2_mode(unsigned int mode);
static int atl2_send(struct netdriver_data *data, size_t size);
static ssize_t atl2_recv(struct netdriver_data *data, size_t max);
static void atl2_stat(eth_stat_t *stat);
static void atl2_intr(unsigned int mask);
static void atl2_other(const message *m_ptr, int ipc_status);

static const struct netdriver atl2_table = {
	.ndr_init	= atl2_init,
	.ndr_stop	= atl2_stop,
	.ndr_mode	= atl2_mode,
	.ndr_recv	= atl2_recv,
	.ndr_send	= atl2_send,
	.ndr_stat	= atl2_stat,
	.ndr_intr	= atl2_intr,
	.ndr_other	= atl2_other
};

/*
 * Read a value from the VPD register area.
 */
static int
atl2_read_vpd(int index, uint32_t * res)
{
	uint32_t off, val;
	int i;

	ATL2_WRITE_U32(ATL2_VPD_DATA_REG, 0);

	off = ATL2_VPD_REGBASE + index * sizeof(uint32_t);

	ATL2_WRITE_U32(ATL2_VPD_CAP_REG,
	    (off << ATL2_VPD_CAP_ADDR_SHIFT) & ATL2_VPD_CAP_ADDR_MASK);

	for (i = 0; i < ATL2_VPD_NTRIES; i++) {
		micro_delay(ATL2_VPD_DELAY);

		val = ATL2_READ_U32(ATL2_VPD_CAP_REG);
		if (val & ATL2_VPD_CAP_DONE)
			break;
	}

	if (i == ATL2_VPD_NTRIES) {
		printf("ATL2: timeout reading EEPROM register %d\n", index);
		return FALSE;
	}

	*res = ATL2_READ_U32(ATL2_VPD_DATA_REG);
	return TRUE;
}

/*
 * Read the MAC address from the EEPROM, using the Vital Product Data register
 * interface.
 */
static int
atl2_get_vpd_hwaddr(void)
{
	uint32_t key, val;
	int i, n, found[2];

	/* No idea, copied from FreeBSD which copied it from Linux. */
	val = ATL2_READ_U32(ATL2_SPICTL_REG);
	if (val & ATL2_SPICTL_VPD_EN) {
		val &= ~ATL2_SPICTL_VPD_EN;
		ATL2_WRITE_U32(ATL2_SPICTL_REG, val);
	}

	/* Is VPD supported? */
#ifdef PCI_CAP_VPD	/* FIXME: just a guess at the future name */
	if (!pci_find_cap(state.devind, PCI_CAP_VPD, &n))
		return FALSE;
#endif

	/*
	 * Read out the set of key/value pairs.  Look for the two parts that
	 * make up the MAC address.
	 */
	found[0] = found[1] = FALSE;
	for (i = 0; i < ATL2_VPD_NREGS; i += 2) {
		if (!atl2_read_vpd(i, &key))
			break;

		if ((key & ATL2_VPD_SIG_MASK) != ATL2_VPD_SIG)
			break;

		key >>= ATL2_VPD_REG_SHIFT;

		if (key != ATL2_HWADDR0_REG && key != ATL2_HWADDR1_REG)
			continue;

		if (!atl2_read_vpd(i + 1, &val))
			break;

		n = (key == ATL2_HWADDR1_REG);
		state.hwaddr[n] = val;
		found[n] = TRUE;

		if (found[1 - n]) break;
	}

	return found[0] && found[1];
}

/*
 * Get the MAC address of the card.  First try the EEPROM; if that fails, just
 * use whatever the card was already set to.
 */
static void
atl2_get_hwaddr(ether_addr_t * addr)
{

	if (!atl2_get_vpd_hwaddr()) {
		printf("ATL2: unable to read from VPD\n");

		state.hwaddr[0] = ATL2_READ_U32(ATL2_HWADDR0_REG);
		state.hwaddr[1] = ATL2_READ_U32(ATL2_HWADDR1_REG) & 0xffff;
	}

	ATL2_DEBUG(("ATL2: MAC address %04x%08x\n",
	    state.hwaddr[1], state.hwaddr[0]));

	addr->ea_addr[0] = state.hwaddr[1] >> 8;
	addr->ea_addr[1] = state.hwaddr[1] & 0xff;
	addr->ea_addr[2] = state.hwaddr[0] >> 24;
	addr->ea_addr[3] = (state.hwaddr[0] >> 16) & 0xff;
	addr->ea_addr[4] = (state.hwaddr[0] >> 8) & 0xff;
	addr->ea_addr[5] = state.hwaddr[0] & 0xff;
}

/*
 * Read a MII PHY register using MDIO.
 */
static int
atl2_read_mdio(int addr, uint16_t * res)
{
	uint32_t rval;
	int i;

	rval = ((addr << ATL2_MDIO_ADDR_SHIFT) & ATL2_MDIO_ADDR_MASK) |
	    ATL2_MDIO_START | ATL2_MDIO_READ | ATL2_MDIO_SUP_PREAMBLE |
	    ATL2_MDIO_CLK_25_4;

	ATL2_WRITE_U32(ATL2_MDIO_REG, rval);

	for (i = 0; i < ATL2_MDIO_NTRIES; i++) {
		micro_delay(ATL2_MDIO_DELAY);

		rval = ATL2_READ_U32(ATL2_MDIO_REG);

		if (!(rval & (ATL2_MDIO_START | ATL2_MDIO_BUSY)))
			break;
	}

	if (i == ATL2_MDIO_NTRIES) return FALSE;

	*res = (uint16_t)(rval & ATL2_MDIO_DATA_MASK);
	return TRUE;
}

/*
 * Allocate DMA ring buffers.
 */
static int
atl2_alloc_dma(void)
{

	state.txd_base = alloc_contig(ATL2_TXD_BUFSIZE, AC_ALIGN4K,
	    &state.txd_phys);
	state.txs_base = alloc_contig(ATL2_TXS_COUNT * sizeof(uint32_t),
	    AC_ALIGN4K, &state.txs_phys);

	/*
	 * The data buffer in each RxD descriptor must be 128-byte aligned.
	 * The two Tx buffers merely require a 4-byte start alignment.
	 */
	state.rxd_align = 128 - offsetof(rxd_t, data);
	state.rxd_base_u = alloc_contig(state.rxd_align +
	    ATL2_RXD_COUNT * ATL2_RXD_SIZE, AC_ALIGN4K, &state.rxd_phys);

	/* Unlike mmap, alloc_contig returns NULL on failure. */
	if (!state.txd_base || !state.txs_base || !state.rxd_base_u)
		return ENOMEM;

	state.rxd_base = (rxd_t *)(state.rxd_base_u + state.rxd_align);
	state.rxd_phys += state.rxd_align;

	/* Zero out just in case. */
	memset(state.txd_base, 0, ATL2_TXD_BUFSIZE);
	memset(state.txs_base, 0, ATL2_TXS_COUNT * sizeof(uint32_t));
	memset(state.rxd_base, 0, ATL2_RXD_COUNT * ATL2_RXD_SIZE);

	return OK;
}

/*
 * Stop the device.
 */
static void
atl2_stop(void)
{
	uint32_t val;
	int i;

	/* Clear and disable interrupts. */
	ATL2_WRITE_U32(ATL2_IMR_REG, 0);
	ATL2_WRITE_U32(ATL2_ISR_REG, 0xffffffff);

	/* Stop Rx/Tx MACs. */
	val = ATL2_READ_U32(ATL2_MAC_REG);
	if (val & (ATL2_MAC_RX_EN | ATL2_MAC_TX_EN)) {
		val &= ~(ATL2_MAC_RX_EN | ATL2_MAC_TX_EN);
		ATL2_WRITE_U32(ATL2_MAC_REG, val);
	}

	ATL2_WRITE_U8(ATL2_DMAWRITE_REG, 0);
	ATL2_WRITE_U8(ATL2_DMAREAD_REG, 0);

	/* Wait until everything is idle. */
	for (i = 0; i < ATL2_IDLE_NTRIES; i++) {
		if (ATL2_READ_U32(ATL2_IDLE_REG) == 0)
			break;

		micro_delay(ATL2_IDLE_DELAY);
	}

	assert(i < ATL2_IDLE_NTRIES);
}

/*
 * Reset the device to a known good state.
 */
static int
atl2_reset(void)
{
	uint32_t val;
	int i;

	/* Issue a soft reset, and wait for the device to respond. */
	ATL2_WRITE_U32(ATL2_MASTER_REG, ATL2_MASTER_SOFT_RESET);

	for (i = 0; i < ATL2_RESET_NTRIES; i++) {
		val = ATL2_READ_U32(ATL2_MASTER_REG);
		if (!(val & ATL2_MASTER_SOFT_RESET))
			break;

		micro_delay(ATL2_RESET_DELAY);
	}

	if (i == ATL2_RESET_NTRIES)
		return FALSE;

	/* Wait until everything is idle. */
	for (i = 0; i < ATL2_IDLE_NTRIES; i++) {
		if (ATL2_READ_U32(ATL2_IDLE_REG) == 0)
			break;

		micro_delay(ATL2_IDLE_DELAY);
	}

	return (i < ATL2_IDLE_NTRIES);
}

/*
 * Reconfigure the device's promiscuity, multicast, and broadcast mode
 * settings.
 */
static void
atl2_mode(unsigned int mode)
{
	uint32_t val;

	val = ATL2_READ_U32(ATL2_MAC_REG);
	val &= ~(ATL2_MAC_PROMISC_EN | ATL2_MAC_MCAST_EN | ATL2_MAC_BCAST_EN);

	if (mode & NDEV_PROMISC)
		val |= ATL2_MAC_PROMISC_EN;
	if (mode & NDEV_MULTI)
		val |= ATL2_MAC_MCAST_EN;
	if (mode & NDEV_BROAD)
		val |= ATL2_MAC_BCAST_EN;

	ATL2_WRITE_U32(ATL2_MAC_REG, val);
}

/*
 * Set up the device for normal operation.
 */
static int
atl2_setup(void)
{
	uint32_t val;

	atl2_stop();

	if (!atl2_reset())
		return FALSE;

	/* Initialize PCIe module.  Magic. */
	ATL2_WRITE_U32(ATL2_LTSSM_TESTMODE_REG, ATL2_LTSSM_TESTMODE_DEFAULT);
	ATL2_WRITE_U32(ATL2_DLL_TX_CTRL_REG, ATL2_DLL_TX_CTRL_DEFAULT);

	/* Enable PHY. */
	ATL2_WRITE_U32(ATL2_PHY_ENABLE_REG, ATL2_PHY_ENABLE);
	micro_delay(1000);

	/* Clear and disable interrupts. */
	ATL2_WRITE_U32(ATL2_ISR_REG, 0xffffffff);

	/* Set the MAC address. */
	ATL2_WRITE_U32(ATL2_HWADDR0_REG, state.hwaddr[0]);
	ATL2_WRITE_U32(ATL2_HWADDR1_REG, state.hwaddr[1]);

	/* Initialize ring buffer addresses and sizes. */
	ATL2_WRITE_U32(ATL2_DESC_ADDR_HI_REG, 0);	/* no 64 bit */
	ATL2_WRITE_U32(ATL2_TXD_ADDR_LO_REG, state.txd_phys);
	ATL2_WRITE_U32(ATL2_TXS_ADDR_LO_REG, state.txs_phys);
	ATL2_WRITE_U32(ATL2_RXD_ADDR_LO_REG, state.rxd_phys);

	ATL2_WRITE_U16(ATL2_RXD_COUNT_REG, ATL2_RXD_COUNT);
	ATL2_WRITE_U16(ATL2_TXD_BUFSIZE_REG,
	    ATL2_TXD_BUFSIZE / sizeof(uint32_t));
	ATL2_WRITE_U16(ATL2_TXS_COUNT_REG, ATL2_TXS_COUNT);

	/* A whole lot of other initialization copied from Linux/FreeBSD. */
	ATL2_WRITE_U32(ATL2_IFG_REG, ATL2_IFG_DEFAULT);

	ATL2_WRITE_U32(ATL2_HDPX_REG, ATL2_HDPX_DEFAULT);

	ATL2_WRITE_U16(ATL2_IMT_REG, ATL2_IMT_DEFAULT);
	val = ATL2_READ_U32(ATL2_MASTER_REG);
	ATL2_WRITE_U32(ATL2_MASTER_REG, val | ATL2_MASTER_IMT_EN);

	ATL2_WRITE_U16(ATL2_ICT_REG, ATL2_ICT_DEFAULT);

	ATL2_WRITE_U32(ATL2_CUT_THRESH_REG, ATL2_CUT_THRESH_DEFAULT);

	ATL2_WRITE_U16(ATL2_FLOW_THRESH_HI_REG, (ATL2_RXD_COUNT / 8) * 7);
	ATL2_WRITE_U16(ATL2_FLOW_THRESH_LO_REG, ATL2_RXD_COUNT / 12);

	/* Set MTU. */
	ATL2_WRITE_U16(ATL2_MTU_REG, ATL2_MTU_DEFAULT);

	/* Reset descriptors, and enable DMA. */
	state.txd_tail = state.txs_tail = state.rxd_tail = 0;
	state.txd_num = state.txs_num = 0;
	state.rx_avail = FALSE;
	ATL2_WRITE_U16(ATL2_TXD_IDX_REG, 0);
	ATL2_WRITE_U16(ATL2_RXD_IDX_REG, 0);

	ATL2_WRITE_U8(ATL2_DMAREAD_REG, ATL2_DMAREAD_EN);
	ATL2_WRITE_U8(ATL2_DMAWRITE_REG, ATL2_DMAWRITE_EN);

	/* Did everything go alright? */
	val = ATL2_READ_U32(ATL2_ISR_REG);
	if (val & ATL2_ISR_PHY_LINKDOWN) {
		printf("ATL2: initialization failed\n");
		return FALSE;
	}

	/* Clear interrupt status. */
	ATL2_WRITE_U32(ATL2_ISR_REG, 0x3fffffff);
	ATL2_WRITE_U32(ATL2_ISR_REG, 0);

	/* Enable interrupts. */
	ATL2_WRITE_U32(ATL2_IMR_REG, ATL2_IMR_DEFAULT);

	/* Configure MAC. */
	ATL2_WRITE_U32(ATL2_MAC_REG, ATL2_MAC_DEFAULT);

	/*
	 * Inet does not tell us about the multicast addresses that it is
	 * interested in, so we have to simply accept all multicast packets.
	 */
	ATL2_WRITE_U32(ATL2_MHT0_REG, 0xffffffff);
	ATL2_WRITE_U32(ATL2_MHT1_REG, 0xffffffff);

	atl2_mode(NDEV_NOMODE);

	/* Enable Tx/Rx. */
	val = ATL2_READ_U32(ATL2_MAC_REG);
	ATL2_WRITE_U32(ATL2_MAC_REG, val | ATL2_MAC_TX_EN | ATL2_MAC_RX_EN);

	return TRUE;
}

/*
 * Find a matching PCI device.
 */
static int
atl2_probe(int skip)
{
	uint16_t vid, did;
#if VERBOSE
	char *dname;
#endif
	int r, devind;

	pci_init();

	r = pci_first_dev(&devind, &vid, &did);
	if (r <= 0)
		return -1;

	while (skip--) {
		r = pci_next_dev(&devind, &vid, &did);
		if (r <= 0)
			return -1;
	}

#if VERBOSE
	dname = pci_dev_name(vid, did);
	ATL2_DEBUG(("ATL2: found %s (%x/%x) at %s\n",
	    dname ? dname : "<unknown>", vid, did, pci_slot_name(devind)));
#endif

	pci_reserve(devind);

	return devind;
}

/*
 * Initialize the device.
 */
static void
atl2_init_hw(int devind, ether_addr_t * addr)
{
	uint32_t bar;
	int r, flag;

	/* Initialize global state. */
	state.devind = devind;

	if ((r = pci_get_bar(devind, PCI_BAR, &bar, &state.size, &flag)) != OK)
		panic("unable to retrieve bar: %d", r);

	if (state.size < ATL2_MIN_MMAP_SIZE || flag)
		panic("invalid register bar");

	state.base = vm_map_phys(SELF, (void *)bar, state.size);
	if (state.base == MAP_FAILED)
		panic("unable to map in registers");

	if ((r = atl2_alloc_dma()) != OK)
		panic("unable to allocate DMA buffers: %d", r);

	state.irq = pci_attr_r8(devind, PCI_ILR);
	state.hook_id = 0;

	if ((r = sys_irqsetpolicy(state.irq, 0, &state.hook_id)) != OK)
		panic("unable to register IRQ: %d", r);

	if (!atl2_reset())
		panic("unable to reset hardware");

	if ((r = sys_irqenable(&state.hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);

	atl2_get_hwaddr(addr);

	atl2_setup();
}

/*
 * Update statistics for packet transmission.
 */
static void
atl2_tx_stat(uint32_t stat)
{

	if (stat & ATL2_TXS_SUCCESS)
		state.stat.ets_packetT++;
	else
		state.stat.ets_recvErr++;

	if (stat & ATL2_TXS_DEFER)
		state.stat.ets_transDef++;
	if (stat & (ATL2_TXS_EXCDEFER | ATL2_TXS_ABORTCOL))
		state.stat.ets_transAb++;
	if (stat & ATL2_TXS_SINGLECOL)
		state.stat.ets_collision++;
	if (stat & ATL2_TXS_MULTICOL)
		state.stat.ets_collision++;
	if (stat & ATL2_TXS_LATECOL)
		state.stat.ets_OWC++;
	if (stat & ATL2_TXS_UNDERRUN)
		state.stat.ets_fifoUnder++;
}

/*
 * Update statistics for packet receipt.
 */
static void
atl2_rx_stat(uint32_t stat)
{

	if (stat & ATL2_RXD_SUCCESS)
		state.stat.ets_packetR++;
	else
		state.stat.ets_recvErr++;

	if (stat & ATL2_RXD_CRCERR)
		state.stat.ets_CRCerr++;
	if (stat & ATL2_RXD_FRAG)
		state.stat.ets_collision++;
	if (stat & ATL2_RXD_TRUNC)
		state.stat.ets_fifoOver++;
	if (stat & ATL2_RXD_ALIGN)
		state.stat.ets_frameAll++;
}

/*
 * Advance the TxD/TxS tails by as many sent packets as found.
 */
static int
atl2_tx_advance(void)
{
	uint32_t stat, size, dsize;
	int advanced;

	advanced = FALSE;

	while (state.txs_num > 0) {
		/* Has the tail packet been processed by the driver? */
		stat = state.txs_base[state.txs_tail];

		if (!(stat & ATL2_TXS_UPDATE))
			break;

		/*
		 * The packet size from the status must match the packet size
		 * we put in.  If they don't, there's not much we can do..
		 */
		size = stat & ATL2_TXS_SIZE_MASK;

		assert((uint32_t)state.txd_tail <=
		    ATL2_TXD_BUFSIZE - sizeof(uint32_t));
		dsize =
		    *(volatile uint32_t *)(state.txd_base + state.txd_tail);
		if (size != dsize)
			printf("ATL2: TxD/TxS size mismatch (%x vs %x)\n",
			    size, dsize);

		/* Advance tails accordingly. */
		size = sizeof(uint32_t) + ATL2_ALIGN_32(dsize);
		assert((uint32_t)state.txd_num >= size);
		state.txd_tail = (state.txd_tail + size) % ATL2_TXD_BUFSIZE;
		state.txd_num -= size;

		state.txs_tail = (state.txs_tail + 1) % ATL2_TXS_COUNT;
		state.txs_num--;

		if (stat & ATL2_TXS_SUCCESS)
			ATL2_DEBUG(("ATL2: successfully sent packet\n"));
		else
			ATL2_DEBUG(("ATL2: failed to send packet\n"));

		/* Update statistics. */
		atl2_tx_stat(stat);

		advanced = TRUE;
	}

	return advanced;
}

/*
 * Advance the RxD tail by as many failed receipts as possible, and see if
 * there is an actual packet left to receive.  If 'next' is set, the packet at
 * the current tail has been processed.
 */
static void
atl2_rx_advance(int next)
{
	int update_tail;
	rxd_t *rxd;
	uint32_t hdr;
	size_t size;

	update_tail = FALSE;

	if (next) {
		state.rxd_tail = (state.rxd_tail + 1) % ATL2_RXD_COUNT;
		update_tail = TRUE;

		ATL2_DEBUG(("ATL2: successfully received packet\n"));

		state.rx_avail = FALSE;
	}

	assert(!state.rx_avail);

	for (;;) {
		/* Check the RxD tail for updates. */
		rxd = &state.rxd_base[state.rxd_tail];

		hdr = rxd->hdr;

		if (!(hdr & ATL2_RXD_UPDATE))
			break;

		rxd->hdr = hdr & ~ATL2_RXD_UPDATE;

		/* Update statistics. */
		atl2_rx_stat(hdr);

		/*
		 * Stop at the first successful receipt.  The packet will be
		 * picked up by Inet later.
		 */
		size = hdr & ATL2_RXD_SIZE_MASK;

		if ((hdr & ATL2_RXD_SUCCESS) &&
		    size >= ETH_MIN_PACK_SIZE + ETH_CRC_SIZE) {
			ATL2_DEBUG(("ATL2: packet available, size %zu\n",
			    size));

			state.rx_avail = TRUE;
			break;
		}

		ATL2_DEBUG(("ATL2: packet receipt failed\n"));

		/* Advance tail. */
		state.rxd_tail = (state.rxd_tail + 1) % ATL2_RXD_COUNT;
		update_tail = TRUE;
	}

	/* If new RxD descriptors are now up for reuse, tell the device. */
	if (update_tail) {
		__insn_barrier();

		ATL2_WRITE_U32(ATL2_RXD_IDX_REG, state.rxd_tail);
	}
}

/*
 * Receive a packet.
 */
static ssize_t
atl2_recv(struct netdriver_data * data, size_t max)
{
	rxd_t *rxd;
	size_t size;

	/* Are there any packets available at all? */
	if (!state.rx_avail)
		return SUSPEND;

	/* Get the first available packet's size.  Cut off the CRC. */
	rxd = &state.rxd_base[state.rxd_tail];

	size = rxd->hdr & ATL2_RXD_SIZE_MASK;
	size -= ETH_CRC_SIZE;

	ATL2_DEBUG(("ATL2: receiving packet with length %zu\n", size));

	/* Truncate large packets. */
	if (size > max)
		size = max;

	/* Copy out the packet. */
		netdriver_copyout(data, 0, rxd->data, size);

	/* We are done with this packet.  Move on to the next. */
	atl2_rx_advance(TRUE /*next*/);

	return size;
}

/*
 * Send a packet.
 */
static int
atl2_send(struct netdriver_data * data, size_t size)
{
	size_t pos, chunk;
	uint8_t *sizep;

	/*
	 * If the packet won't fit, bail out.  Keep at least some space between
	 * TxD head and tail, as it is not clear whether the device deals well
	 * with the case that they collide.
	 */
	if (state.txs_num >= ATL2_TXS_COUNT)
		return SUSPEND;

	if (state.txd_num + sizeof(uint32_t) + ATL2_ALIGN_32(size) >=
	    ATL2_TXD_BUFSIZE)
		return SUSPEND;

	/* Copy in the packet. */
	pos = (state.txd_tail + state.txd_num +
	    sizeof(uint32_t)) % ATL2_TXD_BUFSIZE;
	chunk = ATL2_TXD_BUFSIZE - pos;
	if (size > chunk) {
		netdriver_copyin(data, 0, state.txd_base + pos, chunk);
		netdriver_copyin(data, chunk, state.txd_base, size - chunk);
	} else
		netdriver_copyin(data, 0, state.txd_base + pos, size);

	/* Write the length to the DWORD right before the packet. */
	sizep = state.txd_base +
	    (state.txd_tail + state.txd_num) % ATL2_TXD_BUFSIZE;
	*(volatile uint32_t *)sizep = size;

	/* Update the TxD head. */
	state.txd_num += sizeof(uint32_t) + ATL2_ALIGN_32(size);
	pos = ATL2_ALIGN_32(pos + size) % ATL2_TXD_BUFSIZE;
	assert((int)pos ==
	    (state.txd_tail + state.txd_num) % ATL2_TXD_BUFSIZE);

	/* Initialize and update the TxS head. */
	state.txs_base[(state.txs_tail + state.txs_num) % ATL2_TXS_COUNT] = 0;
	state.txs_num++;

	/* Tell the device about our new position. */
	__insn_barrier();

	ATL2_WRITE_U32(ATL2_TXD_IDX_REG, pos / sizeof(uint32_t));

	return OK;
}

/*
 * Process an interrupt.
 */
static void
atl2_intr(unsigned int __unused mask)
{
	uint32_t val;
	int r, try_send, try_recv;

	/* Clear and disable interrupts. */
	val = ATL2_READ_U32(ATL2_ISR_REG);

	ATL2_WRITE_U32(ATL2_ISR_REG, val | ATL2_ISR_DISABLE);

	ATL2_DEBUG(("ATL2: interrupt (0x%08x)\n", val));

	/* If an error occurred, reset the card. */
	if (val & (ATL2_ISR_DMAR_TIMEOUT | ATL2_ISR_DMAW_TIMEOUT |
	    ATL2_ISR_PHY_LINKDOWN))
		atl2_setup();

	try_send = try_recv = FALSE;

	/* Process sent data, and possibly send pending data. */
	if (val & ATL2_ISR_TX_EVENT) {
		if (atl2_tx_advance())
			try_send = TRUE;
	}

	/* Receive new data, and possible satisfy a pending receive request. */
	if (val & ATL2_ISR_RX_EVENT) {
		if (!state.rx_avail) {
			atl2_rx_advance(FALSE /*next*/);

			try_recv = TRUE;
		}
	}

	/* Reenable interrupts. */
	ATL2_WRITE_U32(ATL2_ISR_REG, 0);

	if ((r = sys_irqenable(&state.hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);

	/* Attempt to satisfy pending send and receive requests. */
	if (try_send)
		netdriver_send();
	if (try_recv)
		netdriver_recv();
}

/*
 * Copy out statistics.
 */
static void
atl2_stat(eth_stat_t * stat)
{

	memcpy(stat, &state.stat, sizeof(*stat));
}

/*
 * Dump link status.
 */
static void
atl2_dump_link(void)
{
	uint16_t val;
	int link_up;

	/* The link status bit is latched.  Read the status register twice. */
	atl2_read_mdio(ATL2_MII_BMSR, &val);
	if (!atl2_read_mdio(ATL2_MII_BMSR, &val)) return;

	link_up = val & ATL2_MII_BMSR_LSTATUS;
	printf("link status:     %4s\t", link_up ? "up" : "down");

	if (!link_up) return;

	if (!atl2_read_mdio(ATL2_MII_PSSR, &val)) return;

	if (!(val & ATL2_MII_PSSR_RESOLVED)) {
		printf("(not resolved)\n");

		return;
	}

	switch (val & ATL2_MII_PSSR_SPEED) {
	case ATL2_MII_PSSR_10: printf("(10Mbps "); break;
	case ATL2_MII_PSSR_100: printf("(100Mbps "); break;
	case ATL2_MII_PSSR_1000: printf("(1000Mbps "); break;
	default: printf("(unknown, ");
	}

	printf("%s duplex)", (val & ATL2_MII_PSSR_DUPLEX) ? "full" : "half");
}

/*
 * Dump statistics.
 */
static void
atl2_dump(void)
{

	printf("\n");
	printf("Attansic L2 statistics:\n");

	printf("recvErr:     %8ld\t", state.stat.ets_recvErr);
	printf("sendErr:     %8ld\t", state.stat.ets_sendErr);
	printf("OVW:         %8ld\n", state.stat.ets_OVW);

	printf("CRCerr:      %8ld\t", state.stat.ets_CRCerr);
	printf("frameAll:    %8ld\t", state.stat.ets_frameAll);
	printf("missedP:     %8ld\n", state.stat.ets_missedP);

	printf("packetR:     %8ld\t", state.stat.ets_packetR);
	printf("packetT:     %8ld\t", state.stat.ets_packetT);
	printf("transDef:    %8ld\n", state.stat.ets_transDef);

	printf("collision:   %8ld\t", state.stat.ets_collision);
	printf("transAb:     %8ld\t", state.stat.ets_transAb);
	printf("carrSense:   %8ld\n", state.stat.ets_carrSense);

	printf("fifoUnder:   %8ld\t", state.stat.ets_fifoUnder);
	printf("fifoOver:    %8ld\t", state.stat.ets_fifoOver);
	printf("CDheartbeat: %8ld\n", state.stat.ets_CDheartbeat);

	printf("OWC:         %8ld\t", state.stat.ets_OWC);
	printf("TxD tail:    %8d\t", state.txd_tail);
	printf("TxD count:   %8d\n", state.txd_num);

	printf("RxD tail:    %8d\t", state.rxd_tail);
	printf("TxS tail:    %8d\t", state.txs_tail);
	printf("TxS count:   %8d\n", state.txs_num);

	atl2_dump_link();
	printf("\n");
}

/*
 * Process miscellaneous messages.
 */
static void
atl2_other(const message * m_ptr, int ipc_status)
{
#if ATL2_FKEY
	int sfkeys;

	if (!is_ipc_notify(ipc_status) || m_ptr->m_source != TTY_PROC_NR)
		return;

	if (fkey_events(NULL, &sfkeys) == OK && bit_isset(sfkeys, ATL2_FKEY))
		atl2_dump();
#endif
}

/*
 * Initialize the atl2 driver.
 */
static int
atl2_init(unsigned int instance, ether_addr_t * addr)
{
	int devind;
#if ATL2_FKEY
	int r, fkeys, sfkeys;
#endif

	memset(&state, 0, sizeof(state));

	/* Try to find a recognized device. */
	devind = atl2_probe(instance);

	if (devind < 0)
		return ENXIO;

	/* Initialize the device. */
	atl2_init_hw(devind, addr);

#if ATL2_FKEY
	/* Register debug dump function key. */
	fkeys = sfkeys = 0;
	bit_set(sfkeys, ATL2_FKEY);
	if ((r = fkey_map(&fkeys, &sfkeys)) != OK)
		printf("ATL2: warning, could not map Shift+F%u key (%d)\n",
		    r, ATL2_FKEY);
#endif

	return OK;
}

#if 0
/*
 * Deallocate resources as proof of concept.  Currently unused.
 */
static void
atl2_cleanup(void)
{
	int r;

	if ((r = sys_irqrmpolicy(&state.hook_id)) != OK)
		panic("unable to deregister IRQ: %d", r);

	free_contig(state.txd_base, ATL2_TXD_BUFSIZE);
	free_contig(state.txs_base, ATL2_TXS_COUNT * sizeof(uint32_t));
	free_contig(state.rxd_base_u,
	    state.rxd_align + ATL2_RXD_COUNT * ATL2_RXD_SIZE);

	vm_unmap_phys(SELF, (void *)state.base, state.size);

	/* We cannot free the PCI device at this time. */
}
#endif

/*
 * The ATL2 ethernet driver.
 */
int
main(int argc, char ** argv)
{

	env_setargs(argc, argv);

	netdriver_task(&atl2_table);

	return EXIT_SUCCESS;
}
