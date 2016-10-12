/* 3Com 3C90xB/C EtherLink driver, by D.C. van Moolenbroek */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <machine/pci.h>
#include <sys/mman.h>
#include <assert.h>

#include "3c90x.h"

#define VERBOSE		0	/* verbose debugging output */

#if VERBOSE
#define XLBC_DEBUG(x)	printf x
#else
#define XLBC_DEBUG(x)
#endif

static struct {
	int hook_id;		/* IRQ hook ID */
	uint8_t *base;		/* base address of memory-mapped registers */
	uint32_t size;		/* size of memory-mapped register area */
	uint16_t window;	/* currently active register window */
	uint16_t filter;	/* packet receipt filter flags */

	xlbc_pd_t *dpd_base;	/* TX descriptor array, virtual address */
	phys_bytes dpd_phys;	/* TX descriptor array, physical address */
	uint8_t *txb_base;	/* transmission buffer, virtual address */
	phys_bytes txb_phys;	/* transmission buffer, physical address */
	xlbc_pd_t *upd_base;	/* RX descriptor array, virtual address */
	phys_bytes upd_phys;	/* RX descriptor array, physical address */
	uint8_t *rxb_base;	/* receipt buffers, virtual address */
	phys_bytes rxb_phys;	/* receipt buffers, physical address */

	unsigned int dpd_tail;	/* index of tail TX descriptor */
	unsigned int dpd_used;	/* number of in-use TX descriptors */
	size_t txb_tail;	/* index of tail TX byte in buffer */
	size_t txb_used;	/* number of in-use TX buffer bytes */
	unsigned int upd_head;	/* index of head RX descriptor */
} state;

enum xlbc_link_type {
	XLBC_LINK_DOWN,
	XLBC_LINK_UP,
	XLBC_LINK_UP_T_HD,
	XLBC_LINK_UP_T_FD,
	XLBC_LINK_UP_TX_HD,
	XLBC_LINK_UP_TX_FD
};

#define XLBC_READ_8(off)	(*(volatile uint8_t *)(state.base + (off)))
#define XLBC_READ_16(off)	(*(volatile uint16_t *)(state.base + (off)))
#define XLBC_READ_32(off)	(*(volatile uint32_t *)(state.base + (off)))
#define XLBC_WRITE_8(off, val)	\
	(*(volatile uint8_t *)(state.base + (off)) = (val))
#define XLBC_WRITE_16(off, val)	\
	(*(volatile uint16_t *)(state.base + (off)) = (val))
#define XLBC_WRITE_32(off, val)	\
	(*(volatile uint32_t *)(state.base + (off)) = (val))

static int xlbc_init(unsigned int, netdriver_addr_t *, uint32_t *,
	unsigned int *);
static void xlbc_stop(void);
static void xlbc_set_mode(unsigned int, const netdriver_addr_t *,
	unsigned int);
static ssize_t xlbc_recv(struct netdriver_data *, size_t);
static int xlbc_send(struct netdriver_data *, size_t);
static void xlbc_intr(unsigned int);
static void xlbc_tick(void);

static const struct netdriver xlbc_table = {
	.ndr_name	= "xl",
	.ndr_init	= xlbc_init,
	.ndr_stop	= xlbc_stop,
	.ndr_set_mode	= xlbc_set_mode,
	.ndr_recv	= xlbc_recv,
	.ndr_send	= xlbc_send,
	.ndr_intr	= xlbc_intr,
	.ndr_tick	= xlbc_tick
};

/*
 * Find a matching PCI device.
 */
static int
xlbc_probe(unsigned int skip)
{
	uint16_t vid, did;
	int devind;
#if VERBOSE
	const char *dname;
#endif

	pci_init();

	if (pci_first_dev(&devind, &vid, &did) <= 0)
		return -1;

	while (skip--) {
		if (pci_next_dev(&devind, &vid, &did) <= 0)
			return -1;
	}

#if VERBOSE
	dname = pci_dev_name(vid, did);
	XLBC_DEBUG(("%s: found %s (%04x:%04x) at %s\n", netdriver_name(),
		dname ? dname : "<unknown>", vid, did, pci_slot_name(devind)));
#endif

	pci_reserve(devind);

	return devind;
}

/*
 * Issue a command to the command register.
 */
static void
xlbc_issue_cmd(uint16_t cmd)
{

	assert(!(XLBC_READ_16(XLBC_STATUS_REG) & XLBC_STATUS_IN_PROGRESS));

	XLBC_WRITE_16(XLBC_CMD_REG, cmd);
}

/*
 * Wait for a command to be acknowledged.  Return TRUE iff the command
 * completed within the timeout period.
 */
static int
xlbc_wait_cmd(void)
{
	spin_t spin;

	/*
	 * The documentation implies that a timeout of 1ms is an upper bound
	 * for all commands.
	 */
	SPIN_FOR(&spin, XLBC_CMD_TIMEOUT) {
		if (!(XLBC_READ_16(XLBC_STATUS_REG) & XLBC_STATUS_IN_PROGRESS))
			return TRUE;
	}

	return FALSE;
}

/*
 * Reset the device to its initial state.  Return TRUE iff successful.
 */
static int
xlbc_reset(void)
{

	(void)xlbc_wait_cmd();

	xlbc_issue_cmd(XLBC_CMD_GLOBAL_RESET);

	/*
	 * It appears that the "command in progress" bit may be cleared before
	 * the reset has completed, resulting in strange behavior afterwards.
	 * Thus, we wait for the maximum reset time (1ms) regardless first, and
	 * only then start checking the command-in-progress bit.
	 */
	micro_delay(XLBC_RESET_DELAY);

	if (!xlbc_wait_cmd())
		return FALSE;

	state.window = 0;

	return TRUE;
}

/*
 * Select a register window.
 */
static void
xlbc_select_window(unsigned int window)
{

	if (state.window == window)
		return;

	xlbc_issue_cmd(XLBC_CMD_SELECT_WINDOW | window);

	state.window = window;
}

/*
 * Read a word from the EEPROM.  On failure, return a value with all bits set.
 */
static uint16_t
xlbc_read_eeprom(unsigned int word)
{
	spin_t spin;

	/* The B revision supports 64 EEPROM words only. */
	assert(!(word & ~XLBC_EEPROM_CMD_ADDR));

	xlbc_select_window(XLBC_EEPROM_WINDOW);

	assert(!(XLBC_READ_16(XLBC_EEPROM_CMD_REG) & XLBC_EEPROM_CMD_BUSY));

	XLBC_WRITE_16(XLBC_EEPROM_CMD_REG, XLBC_EEPROM_CMD_READ | word);

	/* The documented maximum delay for reads is 162us. */
	SPIN_FOR(&spin, XLBC_EEPROM_TIMEOUT) {
		if (!(XLBC_READ_16(XLBC_EEPROM_CMD_REG) &
		    XLBC_EEPROM_CMD_BUSY))
			return XLBC_READ_16(XLBC_EEPROM_DATA_REG);
	}

	return (uint16_t)-1;
}

/*
 * Obtain the preconfigured hardware address of the device.
 */
static void
xlbc_get_hwaddr(netdriver_addr_t * addr)
{
	uint16_t word[3];

	/* TODO: allow overriding through environment variables */

	word[0] = xlbc_read_eeprom(XLBC_EEPROM_WORD_OEM_ADDR0);
	word[1] = xlbc_read_eeprom(XLBC_EEPROM_WORD_OEM_ADDR1);
	word[2] = xlbc_read_eeprom(XLBC_EEPROM_WORD_OEM_ADDR2);

	addr->na_addr[0] = word[0] >> 8;
	addr->na_addr[1] = word[0] & 0xff;
	addr->na_addr[2] = word[1] >> 8;
	addr->na_addr[3] = word[1] & 0xff;
	addr->na_addr[4] = word[2] >> 8;
	addr->na_addr[5] = word[2] & 0xff;

	XLBC_DEBUG(("%s: MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
	    netdriver_name(),
	    addr->na_addr[0], addr->na_addr[1], addr->na_addr[2],
	    addr->na_addr[3], addr->na_addr[4], addr->na_addr[5]));
}

/*
 * Configure the device to use the given hardware address.
 */
static void
xlbc_set_hwaddr(netdriver_addr_t * addr)
{

	xlbc_select_window(XLBC_STATION_WINDOW);

	/* Set station address. */
	XLBC_WRITE_16(XLBC_STATION_ADDR0_REG,
	    addr->na_addr[0] | (addr->na_addr[1] << 8));
	XLBC_WRITE_16(XLBC_STATION_ADDR1_REG,
	    addr->na_addr[2] | (addr->na_addr[3] << 8));
	XLBC_WRITE_16(XLBC_STATION_ADDR2_REG,
	    addr->na_addr[4] | (addr->na_addr[5] << 8));

	/* Set station mask. */
	XLBC_WRITE_16(XLBC_STATION_MASK0_REG, 0);
	XLBC_WRITE_16(XLBC_STATION_MASK1_REG, 0);
	XLBC_WRITE_16(XLBC_STATION_MASK2_REG, 0);
}

/*
 * Perform one-time initialization of various settings.
 */
static void
xlbc_init_once(void)
{
	uint16_t word;
	uint32_t dword;

	/*
	 * Verify the presence of a 10BASE-T or 100BASE-TX port.  Those are the
	 * only port types that are supported and have been tested so far.
	 */
	xlbc_select_window(XLBC_MEDIA_OPT_WINDOW);

	word = XLBC_READ_16(XLBC_MEDIA_OPT_REG);
	if (!(word & (XLBC_MEDIA_OPT_BASE_TX | XLBC_MEDIA_OPT_10_BT)))
		panic("no 100BASE-TX or 10BASE-T port on device");

	/* Initialize the device's internal configuration. */
	xlbc_select_window(XLBC_CONFIG_WINDOW);

	word = XLBC_READ_16(XLBC_CONFIG_WORD1_REG);
	word = (word & ~XLBC_CONFIG_XCVR_MASK) | XLBC_CONFIG_XCVR_AUTO;
	XLBC_WRITE_16(XLBC_CONFIG_WORD1_REG, word);

	/* Disable alternate upload and download sequences. */
	dword = XLBC_READ_32(XLBC_DMA_CTRL_REG);
	dword |= XLBC_DMA_CTRL_UP_NOALT | XLBC_DMA_CTRL_DN_NOALT;
	XLBC_WRITE_32(XLBC_DMA_CTRL_REG, dword);

	/* Specify in which status events we are interested. */
	xlbc_issue_cmd(XLBC_CMD_IND_ENABLE | XLBC_STATUS_MASK);

	/* Enable statistics, including support for counters' upper bits. */
	xlbc_select_window(XLBC_NET_DIAG_WINDOW);

	word = XLBC_READ_16(XLBC_NET_DIAG_REG);
	XLBC_WRITE_16(XLBC_NET_DIAG_REG, word | XLBC_NET_DIAG_UPPER);

	xlbc_issue_cmd(XLBC_CMD_STATS_ENABLE);
}

/*
 * Allocate memory for DMA.
 */
static void
xlbc_alloc_dma(void)
{

	/* Packet descriptors require 8-byte alignment. */
	assert(!(sizeof(xlbc_pd_t) % 8));

	/*
	 * For packet transmission, we use one single circular buffer in which
	 * we store packet data.  We do not split packets in two when the
	 * buffer wraps; instead we waste the trailing bytes and move on to the
	 * start of the buffer.  This allows us to use a single fragment for
	 * each transmitted packet, thus keeping the descriptors small (16
	 * bytes).  The descriptors themselves are allocated as a separate
	 * array.  There is obviously room for improvement here, but the
	 * approach should be good enough.
	 */
	state.dpd_base = alloc_contig(XLBC_DPD_COUNT * sizeof(xlbc_pd_t),
	    AC_ALIGN4K, &state.dpd_phys);
	state.txb_base = alloc_contig(XLBC_TXB_SIZE, 0, &state.txb_phys);

	if (state.dpd_base == NULL || state.txb_base == NULL)
		panic("unable to allocate memory for packet transmission");

	/*
	 * For packet receipt, we have a number of pairs of buffers and
	 * corresponding descriptors.  Each buffer is large enough to contain
	 * an entire packet.  We avoid wasting memory by allocating the buffers
	 * in one go, at the cost of requiring a large contiguous area.  The
	 * descriptors are allocated as a separate array, thus matching the
	 * scheme for transmission in terms of allocation strategy.  Here, too,
	 * there is clear room for improvement at the cost of extra complexity.
	 */
	state.upd_base = alloc_contig(XLBC_UPD_COUNT * sizeof(xlbc_pd_t),
	    AC_ALIGN4K, &state.upd_phys);
	state.rxb_base = alloc_contig(XLBC_UPD_COUNT * XLBC_MAX_PKT_LEN, 0,
	    &state.rxb_phys);

	if (state.upd_base == NULL || state.rxb_base == NULL)
		panic("unable to allocate memory for packet receipt");
}

/*
 * Reset the transmitter.
 */
static void
xlbc_reset_tx(void)
{

	xlbc_issue_cmd(XLBC_CMD_TX_RESET);
	if (!xlbc_wait_cmd())
		panic("timeout trying to reset transmitter");

	state.dpd_tail = 0;
	state.dpd_used = 0;
	state.txb_tail = 0;
	state.txb_used = 0;

	xlbc_issue_cmd(XLBC_CMD_TX_ENABLE);
}

/*
 * Reset the receiver.
 */
static void
xlbc_reset_rx(void)
{
	unsigned int i;

	xlbc_issue_cmd(XLBC_CMD_RX_RESET);
	if (!xlbc_wait_cmd())
		panic("timeout trying to reset receiver");

	xlbc_issue_cmd(XLBC_CMD_SET_FILTER | state.filter);

	for (i = 0; i < XLBC_UPD_COUNT; i++) {
		state.upd_base[i].next = state.upd_phys +
		    ((i + 1) % XLBC_UPD_COUNT) * sizeof(xlbc_pd_t);
		state.upd_base[i].flags = 0;
		state.upd_base[i].addr = state.rxb_phys + i * XLBC_MAX_PKT_LEN;
		state.upd_base[i].len = XLBC_LEN_LAST | XLBC_MAX_PKT_LEN;
	}

	XLBC_WRITE_32(XLBC_UP_LIST_PTR_REG, state.upd_phys);

	state.upd_head = 0;

	__insn_barrier();

	xlbc_issue_cmd(XLBC_CMD_RX_ENABLE);
}

/*
 * Execute a MII read, write, or Z cycle.  Stop the clock, wait, start the
 * clock, optionally change direction and/or data bits, and wait again.
 */
static uint16_t
xlbc_mii_cycle(uint16_t val, uint16_t mask, uint16_t bits)
{

	val &= ~XLBC_PHYS_MGMT_CLK;
	XLBC_WRITE_16(XLBC_PHYS_MGMT_REG, val);

	/* All the delays should be 200ns minimum. */
	micro_delay(XLBC_MII_DELAY);

	/* The clock must be enabled separately from other bit updates. */
	val |= XLBC_PHYS_MGMT_CLK;
	XLBC_WRITE_16(XLBC_PHYS_MGMT_REG, val);

	if (mask != 0) {
		val = (val & ~mask) | bits;
		XLBC_WRITE_16(XLBC_PHYS_MGMT_REG, val);
	}

	micro_delay(XLBC_MII_DELAY);

	return val;
}

/*
 * Read a MII register.
 */
static uint16_t
xlbc_mii_read(uint16_t phy, uint16_t reg)
{
	uint32_t dword;
	uint16_t val;
	int i;

	xlbc_select_window(XLBC_PHYS_MGMT_WINDOW);

	/* Set the direction to write. */
	val = XLBC_READ_16(XLBC_PHYS_MGMT_REG) | XLBC_PHYS_MGMT_DIR;

	XLBC_WRITE_16(XLBC_PHYS_MGMT_REG, val);

	/* Execute write cycles to submit the preamble: PR=1..1 (32 bits) */
	for (i = 0; i < 32; i++)
		val = xlbc_mii_cycle(val, XLBC_PHYS_MGMT_DATA,
		    XLBC_PHYS_MGMT_DATA);

	/* Execute write cycles to submit the rest of the read frame. */
	/* ST=01 OP=10 PHYAD=aaaaa REGAD=rrrrr */
	dword = 0x1800 | (phy << 5) | reg;

	for (i = 13; i >= 0; i--)
		val = xlbc_mii_cycle(val, XLBC_PHYS_MGMT_DATA,
		    ((dword >> i) & 1) ? XLBC_PHYS_MGMT_DATA : 0);

	/* Execute a Z cycle to set the direction to read. */
	val = xlbc_mii_cycle(val, XLBC_PHYS_MGMT_DIR, 0);

	dword = 0;

	/* Receive one status bit and 16 actual data bits. */
	for (i = 16; i >= 0; i--) {
		(void)xlbc_mii_cycle(val, 0, 0);

		val = XLBC_READ_16(XLBC_PHYS_MGMT_REG);

		dword = (dword << 1) | !!(val & XLBC_PHYS_MGMT_DATA);

		micro_delay(XLBC_MII_DELAY);
	}

	/* Execute a Z cycle to terminate the read frame. */
	(void)xlbc_mii_cycle(val, 0, 0);

	/* If the status bit was set, the results are invalid. */
	if (dword & 0x10000)
		dword = 0xffff;

	return (uint16_t)dword;
}

/*
 * Write a MII register.
 */
static void
xlbc_mii_write(uint16_t phy, uint16_t reg, uint16_t data)
{
	uint32_t dword;
	uint16_t val;
	int i;

	xlbc_select_window(XLBC_PHYS_MGMT_WINDOW);

	/* Set the direction to write. */
	val = XLBC_READ_16(XLBC_PHYS_MGMT_REG) | XLBC_PHYS_MGMT_DIR;

	XLBC_WRITE_16(XLBC_PHYS_MGMT_REG, val);

	/* Execute write cycles to submit the preamble: PR=1..1 (32 bits) */
	for (i = 0; i < 32; i++)
		val = xlbc_mii_cycle(val, XLBC_PHYS_MGMT_DATA,
		    XLBC_PHYS_MGMT_DATA);

	/* Execute write cycles to submit the rest of the read frame. */
	/* ST=01 OP=01 PHYAD=aaaaa REGAD=rrrrr TA=10 DATA=d..d (16 bits) */
	dword = 0x50020000 | (phy << 23) | (reg << 18) | data;

	for (i = 31; i >= 0; i--)
		val = xlbc_mii_cycle(val, XLBC_PHYS_MGMT_DATA,
		    ((dword >> i) & 1) ? XLBC_PHYS_MGMT_DATA : 0);

	/* Execute a Z cycle to terminate the write frame. */
	(void)xlbc_mii_cycle(val, 0, 0);
}

/*
 * Return a human-readable description for the given link type.
 */
#if VERBOSE
static const char *
xlbc_get_link_name(enum xlbc_link_type link_type)
{

	switch (link_type) {
	case XLBC_LINK_DOWN:		return "down";
	case XLBC_LINK_UP:		return "up";
	case XLBC_LINK_UP_T_HD:		return "up (10Mbps, half duplex)";
	case XLBC_LINK_UP_T_FD:		return "up (10Mbps, full duplex)";
	case XLBC_LINK_UP_TX_HD:	return "up (100Mbps, half duplex)";
	case XLBC_LINK_UP_TX_FD:	return "up (100Mbps, full duplex)";
	default:			return "(unknown)";
	}
}
#endif /* VERBOSE */

/*
 * Determine the current link status, and return the resulting link type.
 */
static enum xlbc_link_type
xlbc_get_link_type(void)
{
	uint16_t status, control, mask;

	xlbc_select_window(XLBC_MEDIA_STS_WINDOW);

	if (!(XLBC_READ_16(XLBC_MEDIA_STS_REG) & XLBC_MEDIA_STS_LINK_DET))
		return XLBC_LINK_DOWN;

	status = xlbc_mii_read(XLBC_PHY_ADDR, XLBC_MII_STATUS);
	if (!(status & XLBC_MII_STATUS_EXTCAP))
		return XLBC_LINK_UP;
	if (!(status & XLBC_MII_STATUS_AUTONEG))
		return XLBC_LINK_UP;

	/* Wait for auto-negotiation to complete first. */
	if (!(status & XLBC_MII_STATUS_COMPLETE)) {
		control = xlbc_mii_read(XLBC_PHY_ADDR, XLBC_MII_CONTROL);
		control |= XLBC_MII_CONTROL_AUTONEG;
		xlbc_mii_write(XLBC_PHY_ADDR, XLBC_MII_CONTROL, control);

		SPIN_UNTIL(xlbc_mii_read(XLBC_PHY_ADDR, XLBC_MII_STATUS) &
		    XLBC_MII_STATUS_COMPLETE, XLBC_AUTONEG_TIMEOUT);

		status = xlbc_mii_read(XLBC_PHY_ADDR, XLBC_MII_STATUS);
		if (!(status & XLBC_MII_STATUS_COMPLETE))
			return XLBC_LINK_UP;
	}

	/* The highest bit set in both registers is the selected link type. */
	mask = xlbc_mii_read(XLBC_PHY_ADDR, XLBC_MII_AUTONEG_ADV) &
	    xlbc_mii_read(XLBC_PHY_ADDR, XLBC_MII_LP_ABILITY);

	if (mask & XLBC_MII_LINK_TX_FD)
		return XLBC_LINK_UP_TX_FD;
	if (mask & XLBC_MII_LINK_TX_HD)
		return XLBC_LINK_UP_TX_HD;
	if (mask & XLBC_MII_LINK_T_FD)
		return XLBC_LINK_UP_T_FD;
	if (mask & XLBC_MII_LINK_T_HD)
		return XLBC_LINK_UP_T_HD;

	return XLBC_LINK_UP;
}

/*
 * Set the duplex mode to full or half, based on the current link type.
 */
static void
xlbc_set_duplex(enum xlbc_link_type link)
{
	uint16_t word;
	int duplex;

	/*
	 * If the link is down, do not change modes.  In fact, the link may go
	 * down as a result of the reset that is part of changing the mode.
	 */
	if (link == XLBC_LINK_DOWN)
		return;

	/* See if the desired duplex mode differs from the current mode. */
	duplex = (link == XLBC_LINK_UP_T_FD || link == XLBC_LINK_UP_TX_FD);

	xlbc_select_window(XLBC_MAC_CTRL_WINDOW);

	word = XLBC_READ_16(XLBC_MAC_CTRL_REG);

	if (!!(word & XLBC_MAC_CTRL_ENA_FD) == duplex)
		return; /* already in the desired mode */

	/*
	 * Change duplex mode.  Unfortunately, that also means we need to
	 * reset the RX and TX engines.  Fortunately, this should happen only
	 * on a link change, so we're probably not doing much extra damage.
	 * TODO: recovery for packets currently on the transmission queue.
	 */
	XLBC_DEBUG(("%s: %s full-duplex mode\n", netdriver_name(),
	    duplex ? "setting" : "clearing"));

	XLBC_WRITE_16(XLBC_MAC_CTRL_REG, word ^ XLBC_MAC_CTRL_ENA_FD);

	xlbc_reset_rx();

	xlbc_reset_tx();
}

/*
 * The link status has changed.
 */
static void
xlbc_link_event(void)
{
	enum xlbc_link_type link_type;

	/*
	 * The 3c90xB is documented to require a read from the internal
	 * auto-negotiation expansion MII register in order to clear the link
	 * event interrupt.  The 3c90xC resets the link event interrupt as part
	 * of automatic interrupt acknowledgment.
	 */
	(void)xlbc_mii_read(XLBC_PHY_ADDR, XLBC_MII_AUTONEG_EXP);

	link_type = xlbc_get_link_type();

#if VERBOSE
	XLBC_DEBUG(("%s: link %s\n", netdriver_name(),
	    xlbc_get_link_name(link_type)));
#endif

	xlbc_set_duplex(link_type);
}

/*
 * Initialize the device.
 */
static void
xlbc_init_hw(int devind, netdriver_addr_t * addr)
{
	uint32_t bar;
	uint16_t cr;
	int r, io, irq;

	/* Map in the device's memory-mapped registers. */
	if ((r = pci_get_bar(devind, PCI_BAR_2, &bar, &state.size, &io)) != OK)
		panic("unable to retrieve bar: %d", r);

	if (state.size < XLBC_MIN_REG_SIZE || io)
		panic("invalid register bar");

	state.base = vm_map_phys(SELF, (void *)bar, state.size);
	if (state.base == MAP_FAILED)
		panic("unable to map in registers");

	/* Reset the device to a known initial state. */
	if (!xlbc_reset())
		panic("unable to reset hardware");

	/* Now that the device is reset, enable bus mastering if needed. */
	cr = pci_attr_r8(devind, PCI_CR);
	if (!(cr & PCI_CR_MAST_EN))
		pci_attr_w8(devind, PCI_CR, cr | PCI_CR_MAST_EN);

	/* Obtain and apply the hardware address. */
	xlbc_get_hwaddr(addr);

	xlbc_set_hwaddr(addr);

	/* Perform various one-time initialization actions. */
	xlbc_init_once();

	/* Allocate memory for DMA. */
	xlbc_alloc_dma();

	/* Initialize the transmitter. */
	xlbc_reset_tx();

	/* Initialize the receiver. */
	state.filter = XLBC_FILTER_STATION;

	xlbc_reset_rx();

	/* Enable interrupts. */
	irq = pci_attr_r8(devind, PCI_ILR);
	state.hook_id = 0;

	if ((r = sys_irqsetpolicy(irq, 0, &state.hook_id)) != OK)
		panic("unable to register IRQ: %d", r);

	if ((r = sys_irqenable(&state.hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);

	xlbc_issue_cmd(XLBC_CMD_INT_ENABLE | XLBC_STATUS_MASK);

	/*
	 * We will probably get a link event anyway, but trigger one now in
	 * case that does not happen.  The main purpose of this call is to
	 * set the right duplex mode.
	 */
	xlbc_link_event();
}

/*
 * Initialize the 3c90x driver and device.
 */
static int
xlbc_init(unsigned int instance, netdriver_addr_t * addr, uint32_t * caps,
	unsigned int * ticks)
{
	int devind;

	memset(&state, 0, sizeof(state));

	/* Try to find a recognized device. */
	if ((devind = xlbc_probe(instance)) < 0)
		return ENXIO;

	/* Initialize the device. */
	xlbc_init_hw(devind, addr);

	*caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST;
	*ticks = sys_hz() / 10; /* update statistics 10x/sec */
	return OK;
}

/*
 * Stop the device.  The main purpose is to stop any ongoing and future DMA.
 */
static void
xlbc_stop(void)
{

	/* A full reset ought to do it. */
	(void)xlbc_reset();
}

/*
 * Set packet receipt mode.
 */
static void
xlbc_set_mode(unsigned int mode, const netdriver_addr_t * mcast_list __unused,
	unsigned int mcast_count __unused)
{

	state.filter = XLBC_FILTER_STATION;

	if (mode & (NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL))
		state.filter |= XLBC_FILTER_MULTI;
	if (mode & NDEV_MODE_BCAST)
		state.filter |= XLBC_FILTER_BROAD;
	if (mode & NDEV_MODE_PROMISC)
		state.filter |= XLBC_FILTER_PROMISC;

	xlbc_issue_cmd(XLBC_CMD_SET_FILTER | state.filter);
}

/*
 * Try to receive a packet.
 */
static ssize_t
xlbc_recv(struct netdriver_data * data, size_t max)
{
	uint32_t flags;
	uint8_t *ptr;
	unsigned int head;
	size_t len;

	head = state.upd_head;
	flags = *(volatile uint32_t *)&state.upd_base[head].flags;

	/*
	 * The documentation implies, but does not state, that UP_COMPLETE is
	 * set whenever UP_ERROR is.  We rely exclusively on UP_COMPLETE.
	 */
	if (!(flags & XLBC_UP_COMPLETE))
		return SUSPEND;

	if (flags & XLBC_UP_ERROR) {
		XLBC_DEBUG(("%s: received error\n", netdriver_name()));

		netdriver_stat_ierror(1);

		len = 0; /* immediately move on to the next descriptor */
	} else {
		len = flags & XLBC_UP_LEN;

		XLBC_DEBUG(("%s: received packet (size %zu)\n",
		    netdriver_name(), len));

		/* The device is supposed to not give us runt frames. */
		assert(len >= XLBC_MIN_PKT_LEN);

		/* Truncate large packets. */
		if (flags & XLBC_UP_OVERFLOW)
			len = XLBC_MAX_PKT_LEN;
		if (len > max)
			len = max;

		ptr = state.rxb_base + head * XLBC_MAX_PKT_LEN;

		netdriver_copyout(data, 0, ptr, len);
	}

	/* Mark the descriptor as ready for reuse. */
	*(volatile uint32_t *)&state.upd_base[head].flags = 0;

	/*
	 * At this point, the receive engine may have stalled as a result of
	 * filling up all descriptors.  Now that we have a free descriptor, we
	 * can restart it.  As per the documentation, we unstall blindly.
	 */
	xlbc_issue_cmd(XLBC_CMD_UP_UNSTALL);

	/* Advance to the next descriptor in our ring. */
	state.upd_head = (head + 1) % XLBC_UPD_COUNT;

	return len;
}

/*
 * Return how much padding (if any) must be prepended to a packet of the given
 * size so that it does not have to be split due to wrapping.  The given offset
 * is the starting point of the packet; this may be beyond the transmission
 * buffer size in the case that the current buffer contents already wrap.
 */
static size_t
xlbc_pad_tx(size_t off, size_t size)
{

	if (off < XLBC_TXB_SIZE && off + size >= XLBC_TXB_SIZE)
		return XLBC_TXB_SIZE - off;
	else
		return 0;
}

/*
 * Try to send a packet.
 */
static int
xlbc_send(struct netdriver_data * data, size_t size)
{
	size_t used, off, left;
	unsigned int head, last;
	uint32_t phys;

	/* We need a free transmission descriptor. */
	if (state.dpd_used == XLBC_DPD_COUNT)
		return SUSPEND;

	/*
	 * See if we can fit the packet in the circular transmission buffer.
	 * The packet may not be broken up in two parts as the buffer wraps.
	 */
	used = state.txb_used;
	used += xlbc_pad_tx(state.txb_tail + used, size);
	left = XLBC_TXB_SIZE - used;

	if (left < size)
		return SUSPEND;

	XLBC_DEBUG(("%s: transmitting packet (size %zu)\n",
	    netdriver_name(), size));

	/* Copy in the packet. */
	off = (state.txb_tail + used) % XLBC_TXB_SIZE;

	netdriver_copyin(data, 0, &state.txb_base[off], size);

	/* Set up a descriptor for the packet. */
	head = (state.dpd_tail + state.dpd_used) % XLBC_DPD_COUNT;

	state.dpd_base[head].next = 0;
	state.dpd_base[head].flags = XLBC_DN_RNDUP_WORD | XLBC_DN_DN_INDICATE;
	state.dpd_base[head].addr = state.txb_phys + off;
	state.dpd_base[head].len = XLBC_LEN_LAST | size;

	phys = state.dpd_phys + head * sizeof(xlbc_pd_t);

	__insn_barrier();

	/* We need to stall only if other packets were already pending. */
	if (XLBC_READ_32(XLBC_DN_LIST_PTR_REG) != 0) {
		assert(state.dpd_used > 0);

		xlbc_issue_cmd(XLBC_CMD_DN_STALL);
		if (!xlbc_wait_cmd())
			panic("timeout trying to stall downloads");

		last = (state.dpd_tail + state.dpd_used - 1) % XLBC_DPD_COUNT;
		state.dpd_base[last].next = phys;
		/* Group interrupts a bit.  This is a tradeoff. */
		state.dpd_base[last].flags &= ~XLBC_DN_DN_INDICATE;

		if (XLBC_READ_32(XLBC_DN_LIST_PTR_REG) == 0)
			XLBC_WRITE_32(XLBC_DN_LIST_PTR_REG, phys);

		xlbc_issue_cmd(XLBC_CMD_DN_UNSTALL);
	} else
		XLBC_WRITE_32(XLBC_DN_LIST_PTR_REG, phys);

	/* Advance internal queue heads. */
	state.dpd_used++;

	state.txb_used = used + size;
	assert(state.txb_used <= XLBC_TXB_SIZE);

	return OK;
}

/*
 * One or more packets have been downloaded.  Free up the corresponding
 * descriptors for later reuse.
 */
static void
xlbc_advance_tx(void)
{
	uint32_t flags, len;

	while (state.dpd_used > 0) {
		flags = *(volatile uint32_t *)
		    &state.dpd_base[state.dpd_tail].flags;

		if (!(flags & XLBC_DN_DN_COMPLETE))
			break;

		XLBC_DEBUG(("%s: packet copied to transmitter\n",
		    netdriver_name()));

		len = state.dpd_base[state.dpd_tail].len & ~XLBC_LEN_LAST;

		state.dpd_tail = (state.dpd_tail + 1) % XLBC_DPD_COUNT;
		state.dpd_used--;

		len += xlbc_pad_tx(state.txb_tail, len);
		assert(state.txb_used >= len);

		state.txb_tail = (state.txb_tail + len) % XLBC_TXB_SIZE;
		state.txb_used -= len;
	}
}

/*
 * A transmission error has occurred.  Restart, and if necessary even reset,
 * the transmitter.
 */
static void
xlbc_recover_tx(void)
{
	uint8_t status;
	int enable, reset;

	enable = reset = FALSE;

	while ((status = XLBC_READ_8(XLBC_TX_STATUS_REG)) &
	    XLBC_TX_STATUS_COMPLETE) {
		XLBC_DEBUG(("%s: transmission error (0x%04x)\n",
		    netdriver_name(), status));

		/* This is an internal (non-packet) error status. */
		if (status & XLBC_TX_STATUS_OVERFLOW)
			enable = TRUE;

		if (status & XLBC_TX_STATUS_MAX_COLL) {
			netdriver_stat_coll(1);
			enable = TRUE;
		}
		if (status &
		    (XLBC_TX_STATUS_UNDERRUN | XLBC_TX_STATUS_JABBER)) {
			netdriver_stat_oerror(1);
			reset = TRUE;
		}

		XLBC_WRITE_8(XLBC_TX_STATUS_REG, status);
	}

	if (reset) {
		/*
		 * Below is the documented Underrun Recovery procedure.  We use
		 * it for jabber errors as well, because there is no indication
		 * that another procedure should be followed for that case.
		 */
		xlbc_issue_cmd(XLBC_CMD_DN_STALL);
		if (!xlbc_wait_cmd())
			panic("download stall timeout during recovery");

		SPIN_UNTIL(!(XLBC_READ_32(XLBC_DMA_CTRL_REG) &
		    XLBC_DMA_CTRL_DN_INPROG), XLBC_CMD_TIMEOUT);

		xlbc_select_window(XLBC_MEDIA_STS_WINDOW);

		SPIN_UNTIL(!(XLBC_READ_16(XLBC_MEDIA_STS_REG) &
		    XLBC_MEDIA_STS_TX_INPROG), XLBC_CMD_TIMEOUT);

		xlbc_issue_cmd(XLBC_CMD_TX_RESET);
		if (!xlbc_wait_cmd())
			panic("transmitter reset timeout during recovery");

		xlbc_issue_cmd(XLBC_CMD_TX_ENABLE);

		XLBC_WRITE_32(XLBC_DN_LIST_PTR_REG,
		    state.dpd_phys + state.dpd_tail * sizeof(xlbc_pd_t));

		XLBC_DEBUG(("%s: performed recovery\n", netdriver_name()));
	} else if (enable)
		xlbc_issue_cmd(XLBC_CMD_TX_ENABLE);
}

/*
 * Update statistics.  We read all registers, not just the ones we are
 * interested in, so as to limit the number of useless statistics interrupts.
 */
static void
xlbc_update_stats(void)
{

	xlbc_select_window(XLBC_STATS_WINDOW);

	(void)XLBC_READ_8(XLBC_CARRIER_LOST_REG);
	(void)XLBC_READ_8(XLBC_SQE_ERR_REG);
	netdriver_stat_coll(XLBC_READ_8(XLBC_MULTI_COLL_REG));
	netdriver_stat_coll(XLBC_READ_8(XLBC_SINGLE_COLL_REG));
	netdriver_stat_coll(XLBC_READ_8(XLBC_LATE_COLL_REG));
	netdriver_stat_ierror(XLBC_READ_8(XLBC_RX_OVERRUNS_REG));
	(void)XLBC_READ_8(XLBC_FRAMES_DEFERRED_REG);

	(void)XLBC_READ_8(XLBC_UPPER_FRAMES_REG);
	(void)XLBC_READ_8(XLBC_FRAMES_XMIT_OK_REG);
	(void)XLBC_READ_8(XLBC_FRAMES_RCVD_OK_REG);

	(void)XLBC_READ_16(XLBC_BYTES_RCVD_OK_REG);
	(void)XLBC_READ_16(XLBC_BYTES_XMIT_OK_REG);

	xlbc_select_window(XLBC_SSD_STATS_WINDOW);

	(void)XLBC_READ_8(XLBC_BAD_SSD_REG);
}

/*
 * Process an interrupt.
 */
static void
xlbc_intr(unsigned int __unused mask)
{
	uint32_t val;
	int r;

	/*
	 * Get interrupt mask.  Acknowledge some interrupts, and disable all
	 * interrupts as automatic side effect.  The assumption is that any new
	 * events are stored as indications which are then translated into
	 * interrupts as soon as interrupts are reenabled, but this is not
	 * documented explicitly.
	 */
	val = XLBC_READ_16(XLBC_STATUS_AUTO_REG);

	XLBC_DEBUG(("%s: interrupt (0x%04x)\n", netdriver_name(), val));

	if (val & XLBC_STATUS_UP_COMPLETE)
		netdriver_recv();

	if (val & (XLBC_STATUS_DN_COMPLETE | XLBC_STATUS_TX_COMPLETE))
		xlbc_advance_tx();

	if (val & XLBC_STATUS_TX_COMPLETE)
		xlbc_recover_tx();

	if (val & XLBC_STATUS_HOST_ERROR) {
		/*
		 * A catastrophic host error has occurred.  Reset both the
		 * transmitter and the receiver.  This should be enough to
		 * clear the host error, but may be overkill in the cases where
		 * the error direction (TX or RX) can be clearly identified.
		 * Since this entire condition is effectively untestable, we
		 * do not even try to be smart about it.
		 */
		XLBC_DEBUG(("%s: host error, performing reset\n",
		    netdriver_name()));

		xlbc_reset_tx();

		xlbc_reset_rx();

		/* If this has not resolved the problem, restart the driver. */
		if (XLBC_READ_16(XLBC_STATUS_REG) & XLBC_STATUS_HOST_ERROR)
			panic("host error not cleared");
	}

	if (val & XLBC_STATUS_UPDATE_STATS)
		xlbc_update_stats();

	if (val & XLBC_STATUS_LINK_EVENT)
		xlbc_link_event();

	/* See if we should try to send more packets. */
	if (val & (XLBC_STATUS_DN_COMPLETE | XLBC_STATUS_TX_COMPLETE |
	    XLBC_STATUS_HOST_ERROR))
		netdriver_send();

	/* Reenable interrupts. */
	if ((r = sys_irqenable(&state.hook_id)) != OK)
		panic("unable to reenable IRQ: %d", r);

	xlbc_issue_cmd(XLBC_CMD_INT_ENABLE | XLBC_STATUS_MASK);
}

/*
 * Do regular processing.
 */
static void
xlbc_tick(void)
{

	xlbc_update_stats();
}

/*
 * The 3c90x ethernet driver.
 */
int
main(int argc, char ** argv)
{

	env_setargs(argc, argv);

	netdriver_task(&xlbc_table);

	return EXIT_SUCCESS;
}
