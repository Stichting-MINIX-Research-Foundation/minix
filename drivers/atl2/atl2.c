/* Attansic/Atheros L2 FastEthernet driver, by D.C. van Moolenbroek */

/* No documentation is available for this card. The FreeBSD driver is based
 * heavily on the official Linux driver; this driver is based heavily on both.
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <sys/mman.h>
#include <minix/ds.h>
#include <minix/vm.h>
#include <machine/pci.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <assert.h>

#include "atl2.h"

#define VERBOSE		0	/* Verbose debugging output */
#define ATL2_FKEY	1	/* Register Shift+F11 for dumping statistics */

#if VERBOSE
#define ATL2_DEBUG(x) printf x
#else
#define ATL2_DEBUG(x)
#endif

typedef struct {
	u32_t hdr;
	u32_t vtag;
	u8_t data[ATL2_RXD_SIZE - sizeof(u32_t) * 2];
} rxd_t;

static struct {
	int devind;		/* PCI device index */
	int irq;		/* IRQ number */
	int hook_id;		/* IRQ hook ID */
	int mode;		/* datalink mode */
	volatile u8_t *base;	/* base address of memory-mapped registers */
	u32_t size;		/* size of memory-mapped area */
	u32_t hwaddr[2];	/* MAC address, in register representation */

	u8_t *txd_base;		/* local address of TxD ring buffer base */
	u32_t *txs_base;	/* local address of TxS ring buffer base */
	u8_t *rxd_base_u;	/* unaligned base address of RxD ring buffer */
	rxd_t *rxd_base;	/* local address of RxD ring buffer base */

	int rxd_align;		/* alignment offset of RxD ring buffer */

	vir_bytes txd_phys;	/* physical address of TxD ring buffer */
	vir_bytes txs_phys;	/* physical address of TxS ring buffer */
	vir_bytes rxd_phys;	/* physical address of RxD ring buffer */

	int txd_tail;		/* tail index into TxD, in bytes */
	int txd_num;		/* head-tail offset into TxD, in bytes */
	int txs_tail;		/* tail index into TxS, in elements */
	int txs_num;		/* head-tail offset into TxS, in elements */
	int rxd_tail;		/* tail index into RxD, in elements */

	int flags;		/* state flags (ATL2_FLAG_) */
	message read_msg;	/* suspended read request (READ_PEND) */
	message write_msg;	/* suspended write request (WRITE_PEND) */
	endpoint_t task_endpt;	/* requester endpoint (PACK_RCVD|PACK_SENT) */
	size_t recv_count;	/* packet size (PACK_RCVD) */

	eth_stat_t stat;	/* statistics */
} state;

#define ATL2_FLAG_RX_AVAIL	0x01	/* packet available for receipt */
#define ATL2_FLAG_READ_PEND	0x02	/* read request pending */
#define ATL2_FLAG_WRITE_PEND	0x04	/* write request pending */
#define ATL2_FLAG_PACK_RCVD	0x08	/* packet received */
#define ATL2_FLAG_PACK_SENT	0x10	/* packet transmitted */

#define ATL2_READ_U8(off) (* (u8_t *) (state.base + (off)))
#define ATL2_READ_U16(off) (* (u16_t *) (state.base + (off)))
#define ATL2_READ_U32(off) (* (u32_t *) (state.base + (off)))
#define ATL2_WRITE_U8(off, val) * (u8_t *) (state.base + (off)) = (val);
#define ATL2_WRITE_U16(off, val) * (u16_t *) (state.base + (off)) = (val);
#define ATL2_WRITE_U32(off, val) * (u32_t *) (state.base + (off)) = (val);

#define ATL2_ALIGN_32(n) (((n) + 3) & ~3)

static iovec_s_t iovec[NR_IOREQS];

static int instance;

/*===========================================================================*
 *				atl2_read_vpd				     *
 *===========================================================================*/
static int atl2_read_vpd(int index, u32_t *res)
{
	/* Read a value from the VPD register area.
	 */
	u32_t off, val;
	int i;

	ATL2_WRITE_U32(ATL2_VPD_DATA_REG, 0);

	off = ATL2_VPD_REGBASE + index * sizeof(u32_t);

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

/*===========================================================================*
 *				atl2_get_vpd_hwaddr			     *
 *===========================================================================*/
static int atl2_get_vpd_hwaddr(void)
{
	/* Read the MAC address from the EEPROM, using the Vital Product Data
	 * register interface.
	 */
	u32_t key, val;
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

	/* Read out the set of key/value pairs. Look for the two parts that
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

/*===========================================================================*
 *				atl2_get_hwaddr				     *
 *===========================================================================*/
static void atl2_get_hwaddr(void)
{
	/* Get the MAC address of the card. First try the EEPROM; if that
	 * fails, just use whatever the card was already set to.
	 */

	if (!atl2_get_vpd_hwaddr()) {
		printf("ATL2: unable to read from VPD\n");

		state.hwaddr[0] = ATL2_READ_U32(ATL2_HWADDR0_REG);
		state.hwaddr[1] = ATL2_READ_U32(ATL2_HWADDR1_REG) & 0xffff;
	}

	ATL2_DEBUG(("ATL2: MAC address %04lx%08lx\n",
		state.hwaddr[1], state.hwaddr[0]));
}

/*===========================================================================*
 *				atl2_read_mdio				     *
 *===========================================================================*/
static int atl2_read_mdio(int addr, u16_t *res)
{
	/* Read a MII PHY register using MDIO.
	 */
	u32_t rval;
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

	*res = (u16_t) (rval & ATL2_MDIO_DATA_MASK);
	return TRUE;
}

/*===========================================================================*
 *				atl2_alloc_dma				     *
 *===========================================================================*/
static int atl2_alloc_dma(void)
{
	/* Allocate DMA ring buffers.
	 */

	state.txd_base = alloc_contig(ATL2_TXD_BUFSIZE,
		AC_ALIGN4K, &state.txd_phys);
	state.txs_base = alloc_contig(ATL2_TXS_COUNT * sizeof(u32_t),
		AC_ALIGN4K, &state.txs_phys);

	/* The data buffer in each RxD descriptor must be 128-byte aligned.
	 * The two Tx buffers merely require a 4-byte start alignment.
	 */
	state.rxd_align = 128 - offsetof(rxd_t, data);
	state.rxd_base_u =
		alloc_contig(state.rxd_align + ATL2_RXD_COUNT * ATL2_RXD_SIZE,
		AC_ALIGN4K, &state.rxd_phys);

	/* Unlike mmap, alloc_contig returns NULL on failure. */
	if (!state.txd_base || !state.txs_base || !state.rxd_base_u)
		return ENOMEM;

	state.rxd_base = (rxd_t *) (state.rxd_base_u + state.rxd_align);
	state.rxd_phys += state.rxd_align;

	/* Zero out just in case. */
	memset(state.txd_base, 0, ATL2_TXD_BUFSIZE);
	memset(state.txs_base, 0, ATL2_TXS_COUNT * sizeof(u32_t));
	memset(state.rxd_base, 0, ATL2_RXD_COUNT * ATL2_RXD_SIZE);

	return OK;
}

/*===========================================================================*
 *				atl2_stop				     *
 *===========================================================================*/
static int atl2_stop(void)
{
	/* Stop the device.
	 */
	u32_t val;
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

	/* The caller will generally ignore this return value. */
	return (i < ATL2_IDLE_NTRIES);
}

/*===========================================================================*
 *				atl2_reset				     *
 *===========================================================================*/
static int atl2_reset(void)
{
	/* Reset the device to a known good state.
	 */
	u32_t val;
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

/*===========================================================================*
 *				atl2_set_mode				     *
 *===========================================================================*/
static void atl2_set_mode(void)
{
	/* Reconfigure the device's promiscuity, multicast, and broadcast mode
	 * settings.
	 */
	u32_t val;

	val = ATL2_READ_U32(ATL2_MAC_REG);
	val &= ~(ATL2_MAC_PROMISC_EN | ATL2_MAC_MCAST_EN | ATL2_MAC_BCAST_EN);

	if (state.mode & DL_PROMISC_REQ)
		val |= ATL2_MAC_PROMISC_EN;
	if (state.mode & DL_MULTI_REQ)
		val |= ATL2_MAC_MCAST_EN;
	if (state.mode & DL_BROAD_REQ)
		val |= ATL2_MAC_BCAST_EN;

	ATL2_WRITE_U32(ATL2_MAC_REG, val);
}

/*===========================================================================*
 *				atl2_setup				     *
 *===========================================================================*/
static int atl2_setup(void)
{
	/* Set up the device for normal operation.
	 */
	u32_t val;

	atl2_stop();

	if (!atl2_reset())
		return FALSE;

	/* Initialize PCIE module. Magic. */
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
	ATL2_WRITE_U16(ATL2_TXD_BUFSIZE_REG, ATL2_TXD_BUFSIZE / sizeof(u32_t));
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
	state.flags &= ~ATL2_FLAG_RX_AVAIL;
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

	/* Inet does not tell us about the multicast addresses that it is
	 * interested in, so we have to simply accept all multicast packets.
	 */
	ATL2_WRITE_U32(ATL2_MHT0_REG, 0xffffffff);
	ATL2_WRITE_U32(ATL2_MHT1_REG, 0xffffffff);

	atl2_set_mode();

	/* Enable Tx/Rx. */
	val = ATL2_READ_U32(ATL2_MAC_REG);
	ATL2_WRITE_U32(ATL2_MAC_REG, val | ATL2_MAC_TX_EN | ATL2_MAC_RX_EN);

	return TRUE;
}

/*===========================================================================*
 *				atl2_probe				     *
 *===========================================================================*/
static int atl2_probe(int skip)
{
	/* Find a matching PCI device.
	 */
	u16_t vid, did;
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
		dname ? dname : "<unknown>", vid, did,
		pci_slot_name(devind)));
#endif

	pci_reserve(devind);

	return devind;
}

/*===========================================================================*
 *				atl2_init				     *
 *===========================================================================*/
static void atl2_init(int devind)
{
	/* Initialize the device.
	 */
	u32_t bar;
	int r, flag;

	/* Initialize global state. */
	state.devind = devind;
	state.mode = DL_NOMODE;
	state.flags = 0;
	state.recv_count = 0;

	memset(&state.stat, 0, sizeof(state.stat));

	if ((r = pci_get_bar(devind, PCI_BAR, &bar, &state.size, &flag)) != OK)
		panic("unable to retrieve bar: %d", r);

	if (state.size < ATL2_MIN_MMAP_SIZE || flag)
		panic("invalid register bar");

	state.base = vm_map_phys(SELF, (void *) bar, state.size);
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

	atl2_get_hwaddr();

	atl2_setup();
}

/*===========================================================================*
 *				atl2_tx_stat				     *
 *===========================================================================*/
static void atl2_tx_stat(u32_t stat)
{
	/* Update statistics for packet transmission.
	 */

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

/*===========================================================================*
 *				atl2_rx_stat				     *
 *===========================================================================*/
static void atl2_rx_stat(u32_t stat)
{
	/* Update statistics for packet receipt.
	 */

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

/*===========================================================================*
 *				atl2_tx_advance				     *
 *===========================================================================*/
static int atl2_tx_advance(void)
{
	/* Advance the TxD/TxS tails by as many sent packets as found.
	 */
	u32_t stat, size, dsize;
	int advanced;

	advanced = FALSE;

	while (state.txs_num > 0) {
		/* Has the tail packet been processed by the driver? */
		stat = state.txs_base[state.txs_tail];

		if (!(stat & ATL2_TXS_UPDATE))
			break;

		/* The packet size from the status must match the packet size
		 * we put in. If they don't, there's not much we can do..
		 */
		size = stat & ATL2_TXS_SIZE_MASK;

		assert((u32_t) state.txd_tail <=
			ATL2_TXD_BUFSIZE - sizeof(u32_t));
		dsize = * (u32_t *) (state.txd_base + state.txd_tail);
		if (size != dsize)
			printf("ATL2: TxD/TxS size mismatch (%x vs %x)\n",
				size, dsize);

		/* Advance tails accordingly. */
		size = sizeof(u32_t) + ATL2_ALIGN_32(dsize);
		assert((u32_t) state.txd_num >= size);
		state.txd_tail = (state.txd_tail + size) % ATL2_TXD_BUFSIZE;
		state.txd_num -= size;

		state.txs_tail = (state.txs_tail + 1) % ATL2_TXS_COUNT;
		state.txs_num--;

		if (stat & ATL2_TXS_SUCCESS) {
			ATL2_DEBUG(("ATL2: successfully sent packet\n"));
		} else {
			ATL2_DEBUG(("ATL2: failed to send packet\n"));
		}

		/* Update statistics. */
		atl2_tx_stat(stat);

		advanced = TRUE;
	}

	return advanced;
}

/*===========================================================================*
 *				atl2_rx_advance				     *
 *===========================================================================*/
static void atl2_rx_advance(int next)
{
	/* Advance the RxD tail by as many failed receipts as possible, and
	 * see if there is an actual packet left to receive. If 'next' is set,
	 * the packet at the current tail has been processed.
	 */
	int update_tail;
	rxd_t *rxd;
	u32_t hdr, size;

	update_tail = FALSE;

	if (next) {
		state.rxd_tail = (state.rxd_tail + 1) % ATL2_RXD_COUNT;
		update_tail = TRUE;

		ATL2_DEBUG(("ATL2: successfully received packet\n"));

		state.flags &= ~ATL2_FLAG_RX_AVAIL;
	}

	assert(!(state.flags & ATL2_FLAG_RX_AVAIL));

	for (;;) {
		/* Check the RxD tail for updates. */
		rxd = &state.rxd_base[state.rxd_tail];

		hdr = rxd->hdr;

		if (!(hdr & ATL2_RXD_UPDATE))
			break;

		rxd->hdr = hdr & ~(ATL2_RXD_UPDATE);

		/* Update statistics. */
		atl2_rx_stat(hdr);

		/* Stop at the first successful receipt. The packet will be
		 * picked up by Inet later.
		 */
		size = hdr & ATL2_RXD_SIZE_MASK;

		if ((hdr & ATL2_RXD_SUCCESS) && size >= ETH_MIN_PACK_SIZE) {
			ATL2_DEBUG(("ATL2: packet available, size %ld\n",
				size));

			state.flags |= ATL2_FLAG_RX_AVAIL;
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

/*===========================================================================*
 *				atl2_reply				     *
 *===========================================================================*/
static void atl2_reply(void)
{
	/* Send a task reply to Inet.
	 */
	message m;
	int r, flags;

	flags = DL_NOFLAGS;
	if (state.flags & ATL2_FLAG_PACK_SENT)
		flags |= DL_PACK_SEND;
	if (state.flags & ATL2_FLAG_PACK_RCVD)
		flags |= DL_PACK_RECV;

	m.m_type = DL_TASK_REPLY;
	m.DL_FLAGS = flags;
	m.DL_COUNT = state.recv_count;

	ATL2_DEBUG(("ATL2: sending reply, flags %x count %d\n", flags,
		m.DL_COUNT));

	if ((r = send(state.task_endpt, &m)) != OK)
		panic("unable to reply: %d", r);

	state.flags &= ~(ATL2_FLAG_PACK_SENT | ATL2_FLAG_PACK_RCVD);
	state.recv_count = 0;
}

/*===========================================================================*
 *				atl2_readv				     *
 *===========================================================================*/
static void atl2_readv(const message *m, int from_int)
{
	/* Read packet data.
	 */
	rxd_t *rxd;
	iovec_s_t *iovp;
	size_t count, off, left, size;
	u8_t *pos;
	int i, j, r, batch;

	/* We can deal with only one read request from Inet at a time. */
	assert(from_int || !(state.flags & ATL2_FLAG_READ_PEND));

	state.task_endpt = m->m_source;

	/* Are there any packets available at all? */
	if (!(state.flags & ATL2_FLAG_RX_AVAIL))
		goto suspend;

	/* Get the first available packet's size. Cut off the CRC. */
	rxd = &state.rxd_base[state.rxd_tail];

	count = rxd->hdr & ATL2_RXD_SIZE_MASK;
	count -= ETH_CRC_SIZE;

	ATL2_DEBUG(("ATL2: readv: found packet with length %d\n", count));

	/* Copy out the packet. */
	off = 0;
	left = count;
	pos = rxd->data;

	for (i = 0; i < m->DL_COUNT && left > 0; i += batch) {
		/* Copy in the next batch. */
		batch = MIN(m->DL_COUNT - i, NR_IOREQS);

		r = sys_safecopyfrom(m->m_source, m->DL_GRANT, off, 
			(vir_bytes) iovec, batch * sizeof(iovec[0]));
		if (r != OK)
			panic("vector copy failed: %d", r);

		/* Copy out each element in the batch, until we run out. */
		for (j = 0, iovp = iovec; j < batch && left > 0; j++, iovp++) {
			size = MIN(iovp->iov_size, left);

			r = sys_safecopyto(m->m_source, iovp->iov_grant, 0,
				(vir_bytes) pos, size);
			if (r != OK)
				panic("safe copy failed: %d", r);

			pos += size;
			left -= size;
		}

		off += batch * sizeof(iovec[0]);
	}

	/* Not sure what to do here. Inet shouldn't mess this up anyway. */
	if (left > 0) {
		printf("ATL2: truncated packet of %d bytes by %d bytes\n",
			count, left);
		count -= left;
	}

	/* We are done with this packet. Move on to the next. */
	atl2_rx_advance(TRUE /*next*/);

	/* We have now successfully received a packet. */
	state.flags &= ~ATL2_FLAG_READ_PEND;
	state.flags |= ATL2_FLAG_PACK_RCVD;
	state.recv_count = count;

	/* If called from the interrupt handler, the caller will reply. */
	if (!from_int)
		atl2_reply();

	return;

suspend:
	/* No packets are available at this time. If we were not already
	 * trying to resume receipt, save the read request for later, and tell
	 * Inet that the request has been suspended.
	 */
	if (from_int)
		return;

	state.flags |= ATL2_FLAG_READ_PEND;
	state.read_msg = *m;

	atl2_reply();
}

/*===========================================================================*
 *				atl2_writev				     *
 *===========================================================================*/
static void atl2_writev(const message *m, int from_int)
{
	/* Write packet data.
	 */
	iovec_s_t *iovp;
	size_t off, count, left, pos, skip;
	vir_bytes size;
	u8_t *sizep;
	int i, j, r, batch, maxnum;

	/* We can deal with only one write request from Inet at a time. */
	assert(from_int || !(state.flags & ATL2_FLAG_WRITE_PEND));

	state.task_endpt = m->m_source;

	/* If we are already certain that the packet won't fit, bail out.
	 * Keep at least some space between TxD head and tail, as it is not
	 * clear whether the device deals well with the case that they collide.
	 */
	if (state.txs_num >= ATL2_TXS_COUNT)
		goto suspend;
	maxnum = ATL2_TXD_BUFSIZE - ETH_MIN_PACK_SIZE - sizeof(u32_t);
	if (state.txd_num >= maxnum)
		goto suspend;

	/* Optimistically try to copy in the data; suspend if it turns out
	 * that it does not fit.
	 */
	off = 0;
	count = 0;
	left = state.txd_num - sizeof(u32_t);
	pos = (state.txd_tail + state.txd_num +
		sizeof(u32_t)) % ATL2_TXD_BUFSIZE;

	for (i = 0; i < m->DL_COUNT; i += batch) {
		/* Copy in the next batch. */
		batch = MIN(m->DL_COUNT - i, NR_IOREQS);

		r = sys_safecopyfrom(m->m_source, m->DL_GRANT, off, 
			(vir_bytes) iovec, batch * sizeof(iovec[0]));
		if (r != OK)
			panic("vector copy failed: %d", r);

		/* Copy in each element in the batch. */
		for (j = 0, iovp = iovec; j < batch; j++, iovp++) {
			size = iovp->iov_size;
			if (size > left)
				goto suspend;

			skip = 0;
			if (size > ATL2_TXD_BUFSIZE - pos) {
				skip = ATL2_TXD_BUFSIZE - pos;
				r = sys_safecopyfrom(m->m_source,
					iovp->iov_grant, 0,
					(vir_bytes) (state.txd_base + pos),
					skip);
				if (r != OK)
					panic("safe copy failed: %d", r);
				pos = 0;
			}

			r = sys_safecopyfrom(m->m_source, iovp->iov_grant,
				skip, (vir_bytes) (state.txd_base + pos),
				size - skip);
			if (r != OK)
				panic("safe copy failed: %d", r);

			pos = (pos + size - skip) % ATL2_TXD_BUFSIZE;
			left -= size;
			count += size;
		}

		off += batch * sizeof(iovec[0]);
	}

	assert(count <= ETH_MAX_PACK_SIZE_TAGGED);

	/* Write the length to the DWORD right before the packet. */
	sizep = state.txd_base +
		(state.txd_tail + state.txd_num) % ATL2_TXD_BUFSIZE;
	* (u32_t *) sizep = count;

	/* Update the TxD head. */
	state.txd_num += sizeof(u32_t) + ATL2_ALIGN_32(count);
	pos = ATL2_ALIGN_32(pos) % ATL2_TXD_BUFSIZE;
	assert((int) pos ==
		(state.txd_tail + state.txd_num) % ATL2_TXD_BUFSIZE);

	/* Initialize and update the TxS head. */
	state.txs_base[(state.txs_tail + state.txs_num) % ATL2_TXS_COUNT] = 0;
	state.txs_num++;

	/* Tell the device about our new position. */
	__insn_barrier();

	ATL2_WRITE_U32(ATL2_TXD_IDX_REG, pos / sizeof(u32_t));

	/* We have now successfully set up the transmission of a packet. */
	state.flags &= ~ATL2_FLAG_WRITE_PEND;
	state.flags |= ATL2_FLAG_PACK_SENT;

	/* If called from the interrupt handler, the caller will reply. */
	if (!from_int)
		atl2_reply();

	return;

suspend:
	/* We cannot transmit the packet at this time. If we were not already
	 * trying to resume transmission, save the write request for later,
	 * and tell Inet that the request has been suspended.
	 */
	if (from_int)
		return;

	state.flags |= ATL2_FLAG_WRITE_PEND;
	state.write_msg = *m;

	atl2_reply();
}

/*===========================================================================*
 *				atl2_intr				     *
 *===========================================================================*/
static void atl2_intr(const message *UNUSED(m))
{
	/* Interrupt received.
	 */
	u32_t val;
	int r, try_write, try_read;

	/* Clear and disable interrupts. */
	val = ATL2_READ_U32(ATL2_ISR_REG);

	ATL2_WRITE_U32(ATL2_ISR_REG, val | ATL2_ISR_DISABLE);

	ATL2_DEBUG(("ATL2: interrupt (0x%08lx)\n", val));

	/* If an error occurred, reset the card. */
	if (val & (ATL2_ISR_DMAR_TIMEOUT | ATL2_ISR_DMAW_TIMEOUT |
			ATL2_ISR_PHY_LINKDOWN)) {
		atl2_setup();
	}

	try_write = try_read = FALSE;

	/* Process sent data, and possibly send pending data. */
	if (val & ATL2_ISR_TX_EVENT) {
		if (atl2_tx_advance())
			try_write = (state.flags & ATL2_FLAG_WRITE_PEND);
	}

	/* Receive new data, and possible satisfy a pending receive request. */
	if (val & ATL2_ISR_RX_EVENT) {
		if (!(state.flags & ATL2_FLAG_RX_AVAIL)) {
			atl2_rx_advance(FALSE /*next*/);

			try_read = (state.flags & ATL2_FLAG_READ_PEND);
		}
	}

	/* Reenable interrupts. */
	ATL2_WRITE_U32(ATL2_ISR_REG, 0);

	if ((r = sys_irqenable(&state.hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);

	/* Attempt to satisfy pending write and read requests. */
	if (try_write)
		atl2_writev(&state.write_msg, TRUE /*from_int*/);
	if (try_read)
		atl2_readv(&state.read_msg, TRUE /*from_int*/);
	if (state.flags & (ATL2_FLAG_PACK_SENT | ATL2_FLAG_PACK_RCVD))
		atl2_reply();
}

/*===========================================================================*
 *				atl2_conf				     *
 *===========================================================================*/
static void atl2_conf(message *m)
{
	/* Configure the mode of the card.
	 */
	ether_addr_t addr;
	int r;

	state.mode = m->DL_MODE;

	atl2_set_mode();

	addr.ea_addr[0] = state.hwaddr[1] >> 8;
	addr.ea_addr[1] = state.hwaddr[1] & 0xff;
	addr.ea_addr[2] = state.hwaddr[0] >> 24;
	addr.ea_addr[3] = (state.hwaddr[0] >> 16) & 0xff;
	addr.ea_addr[4] = (state.hwaddr[0] >> 8) & 0xff;
	addr.ea_addr[5] = state.hwaddr[0] & 0xff;

	memcpy(m->DL_HWADDR, &addr, sizeof(addr));

	m->m_type = DL_CONF_REPLY;
	m->DL_STAT = OK;

	if ((r = send(m->m_source, m)) != OK)
		printf("ATL2: unable to send reply (%d)\n", r);
}

/*===========================================================================*
 *				atl2_getstat				     *
 *===========================================================================*/
static void atl2_getstat(message *m)
{
	/* Copy out statistics.
	 */
	int r;

	sys_safecopyto(m->m_source, m->DL_GRANT, 0,
		(vir_bytes) &state.stat, sizeof(state.stat));

	m->m_type = DL_STAT_REPLY;

	if ((r = send(m->m_source, m)) != OK)
		printf("ATL2: unable to send reply (%d)\n", r);
}

/*===========================================================================*
 *				atl2_dump_link				     *
 *===========================================================================*/
static void atl2_dump_link(void)
{
	/* Dump link status.
	 */
	u16_t val;
	int link_up;

	/* The link status bit is latched. Read the status register twice. */
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

/*===========================================================================*
 *				atl2_dump				     *
 *===========================================================================*/
static void atl2_dump(void)
{
	/* Dump statistics.
	 */

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

	printf("flags:         0x%04x\t", state.flags);
	atl2_dump_link();
	printf("\n");
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	/* Initialize the atl2 driver.
	 */
	int r, devind;
	long v;
#if ATL2_FKEY
	int fkeys, sfkeys;
#endif

	/* How many matching devices should we skip? */
	v = 0;
	(void) env_parse("instance", "d", 0, &v, 0, 255);
	instance = (int) v;

	/* Try to find a recognized device. */
	devind = atl2_probe(instance);

	if (devind < 0)
		panic("no matching device found");

	/* Initialize the device. */
	atl2_init(devind);

	/* Announce we are up! */
	netdriver_announce();

#if ATL2_FKEY
	/* Register debug dump function key. */
	fkeys = sfkeys = 0;
	bit_set(sfkeys, 11);
	if ((r = fkey_map(&fkeys, &sfkeys)) != OK)
		printf("ATL2: warning, could not map Shift+F11 key (%d)\n", r);
#endif

	return(OK);
}

/*===========================================================================*
 *			    sef_cb_signal_handler			     *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
	/* In case of a termination signal, shut down this driver.
	 * Stop the device, and deallocate resources as proof of concept.
	 */
	int r;

	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	atl2_stop();

	if ((r = sys_irqrmpolicy(&state.hook_id)) != OK)
		panic("unable to deregister IRQ: %d", r);

	free_contig(state.txd_base, ATL2_TXD_BUFSIZE);
	free_contig(state.txs_base, ATL2_TXS_COUNT * sizeof(u32_t));
	free_contig(state.rxd_base_u,
		state.rxd_align + ATL2_RXD_COUNT * ATL2_RXD_SIZE);

	vm_unmap_phys(SELF, (void *) state.base, state.size);

	/* We cannot free the PCI device at this time. */

	exit(0);
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_lu(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);

	/* Register live update callbacks. */
	sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
	sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_workfree);

	/* Register signal callbacks. */
	sef_setcb_signal_handler(sef_cb_signal_handler);

	/* Let SEF perform startup. */
	sef_startup();
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	/* Driver task.
	 */
	message m;
	int ipc_status;
	int r;

	/* Initialize SEF. */
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {
		if ((r = netdriver_receive(ANY, &m, &ipc_status)) != OK)
			panic("netdriver_receive failed: %d", r);

		if (is_ipc_notify(ipc_status)) {
			switch (m.m_source) {
			case HARDWARE:		/* interrupt */
				atl2_intr(&m);

				break;

			case TTY_PROC_NR:	/* function key */
				atl2_dump();

				break;

			default:
				printf("ATL2: illegal notify from %d\n",
					m.m_source);
			}

			continue;
		}

		/* Process requests from Inet. */
		switch (m.m_type) {
		case DL_CONF:		atl2_conf(&m);			break;
		case DL_GETSTAT_S:	atl2_getstat(&m);		break;
		case DL_WRITEV_S:	atl2_writev(&m, FALSE);		break;
		case DL_READV_S:	atl2_readv(&m, FALSE);		break;
		default:
			printf("ATL2: illegal message %d from %d\n",
				m.m_type, m.m_source);
		}
	}
}
