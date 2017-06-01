/* A device driver for Intel Pro/1000 Gigabit Ethernet Controllers. */

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <machine/pci.h>
#include <sys/mman.h>
#include "assert.h"
#include "e1000.h"
#include "e1000_hw.h"
#include "e1000_reg.h"
#include "e1000_pci.h"

static int e1000_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks);
static void e1000_stop(void);
static void e1000_set_mode(unsigned int, const netdriver_addr_t *,
	unsigned int);
static void e1000_set_hwaddr(const netdriver_addr_t *);
static int e1000_send(struct netdriver_data *data, size_t size);
static ssize_t e1000_recv(struct netdriver_data *data, size_t max);
static unsigned int e1000_get_link(uint32_t *);
static void e1000_intr(unsigned int mask);
static void e1000_tick(void);
static int e1000_probe(e1000_t *e, int skip);
static void e1000_init_hw(e1000_t *e, netdriver_addr_t *addr);
static uint32_t e1000_reg_read(e1000_t *e, uint32_t reg);
static void e1000_reg_write(e1000_t *e, uint32_t reg, uint32_t value);
static void e1000_reg_set(e1000_t *e, uint32_t reg, uint32_t value);
static void e1000_reg_unset(e1000_t *e, uint32_t reg, uint32_t value);
static u16_t eeprom_eerd(e1000_t *e, int reg);
static u16_t eeprom_ich(e1000_t *e, int reg);
static int eeprom_ich_init(e1000_t *e);
static int eeprom_ich_cycle(e1000_t *e, u32_t timeout);

static int e1000_instance;
static e1000_t e1000_state;

static const struct netdriver e1000_table = {
	.ndr_name	= "em",
	.ndr_init	= e1000_init,
	.ndr_stop	= e1000_stop,
	.ndr_set_mode	= e1000_set_mode,
	.ndr_set_hwaddr	= e1000_set_hwaddr,
	.ndr_recv	= e1000_recv,
	.ndr_send	= e1000_send,
	.ndr_get_link	= e1000_get_link,
	.ndr_intr	= e1000_intr,
	.ndr_tick	= e1000_tick
};

/*
 * The e1000 driver.
 */
int
main(int argc, char * argv[])
{

	env_setargs(argc, argv);

	/* Let the netdriver library take control. */
	netdriver_task(&e1000_table);

	return 0;
}

/*
 * Initialize the e1000 driver and device.
 */
static int
e1000_init(unsigned int instance, netdriver_addr_t * addr, uint32_t * caps,
	unsigned int * ticks)
{
	e1000_t *e;
	int r;

	e1000_instance = instance;

	/* Clear state. */
	memset(&e1000_state, 0, sizeof(e1000_state));

	e = &e1000_state;

	/* Perform calibration. */
	if ((r = tsc_calibrate()) != OK)
		panic("tsc_calibrate failed: %d", r);

	/* See if we can find a matching device. */
	if (!e1000_probe(e, instance))
		return ENXIO;

	/* Initialize the hardware, and return its ethernet address. */
	e1000_init_hw(e, addr);

	*caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST | NDEV_CAP_HWADDR;
	*ticks = sys_hz() / 10; /* update statistics 10x/sec */
	return OK;
}

/*
 * Map flash memory.  This step is optional.
 */
static void
e1000_map_flash(e1000_t * e, int devind, int did)
{
	u32_t flash_addr, gfpreg, sector_base_addr;
	size_t flash_size;

	/* The flash memory is pointed to by BAR2.  It may not be present. */
	if ((flash_addr = pci_attr_r32(devind, PCI_BAR_2)) == 0)
		return;

	/* The default flash size. */
	flash_size = 0x10000;

	switch (did) {
	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82545EM:
	case E1000_DEV_ID_82540EP:
	case E1000_DEV_ID_82540EP_LP:
		return; /* don't even try */

	/* 82566/82567/82562V series support mapping 4kB of flash memory. */
	case E1000_DEV_ID_ICH10_D_BM_LM:
	case E1000_DEV_ID_ICH10_R_BM_LF:
		flash_size = 0x1000;
		break;
	}

	e->flash = vm_map_phys(SELF, (void *)flash_addr, flash_size);
	if (e->flash == MAP_FAILED)
		panic("e1000: couldn't map in flash");

	/* sector_base_addr is a "sector"-aligned address (4096 bytes). */
	gfpreg = E1000_READ_FLASH_REG(e, ICH_FLASH_GFPREG);
	sector_base_addr = gfpreg & FLASH_GFPREG_BASE_MASK;

	/* flash_base_addr is byte-aligned. */
	e->flash_base_addr = sector_base_addr << FLASH_SECTOR_ADDR_SHIFT;
}

/*
 * Find a matching device.  Return TRUE on success.
 */
static int
e1000_probe(e1000_t * e, int skip)
{
	int r, devind, ioflag;
	u16_t vid, did, cr;
	u32_t status;
	u32_t base, size;
	const char *dname;

	E1000_DEBUG(3, ("%s: probe()\n", netdriver_name()));

	/* Initialize communication to the PCI driver. */
	pci_init();

	/* Attempt to iterate the PCI bus. Start at the beginning. */
	if ((r = pci_first_dev(&devind, &vid, &did)) == 0)
		return FALSE;

	/* Loop devices on the PCI bus. */
	while (skip--) {
		E1000_DEBUG(3, ("%s: probe() devind %d vid 0x%x did 0x%x\n",
		    netdriver_name(), devind, vid, did));

		if (!(r = pci_next_dev(&devind, &vid, &did)))
			return FALSE;
	}

	/* We found a matching card.  Set card-specific properties. */
	e->eeprom_read = eeprom_eerd;

	switch (did) {
	case E1000_DEV_ID_ICH10_D_BM_LM:
	case E1000_DEV_ID_ICH10_R_BM_LF:
		e->eeprom_read = eeprom_ich;
		break;

	case E1000_DEV_ID_82540EM:
	case E1000_DEV_ID_82545EM:
	case E1000_DEV_ID_82540EP_LP:
		e->eeprom_done_bit = (1 << 4);
		e->eeprom_addr_off = 8;
		break;

	default:
		e->eeprom_done_bit = (1 << 1);
		e->eeprom_addr_off = 2;
		break;
	}

	/* Inform the user about the new card. */
	if (!(dname = pci_dev_name(vid, did)))
		dname = "Intel Pro/1000 Gigabit Ethernet Card";
	E1000_DEBUG(1, ("%s: %s (%04x/%04x) at %s\n",
	    netdriver_name(), dname, vid, did, pci_slot_name(devind)));

	/* Reserve PCI resources found. */
	pci_reserve(devind);

	/* Read PCI configuration. */
	e->irq = pci_attr_r8(devind, PCI_ILR);

	if ((r = pci_get_bar(devind, PCI_BAR, &base, &size, &ioflag)) != OK)
		panic("failed to get PCI BAR: %d", r);
	if (ioflag)
		panic("PCI BAR is not for memory");

	if ((e->regs = vm_map_phys(SELF, (void *)base, size)) == MAP_FAILED)
		panic("failed to map hardware registers from PCI");

	/* Enable DMA bus mastering if necessary. */
	cr = pci_attr_r16(devind, PCI_CR);
	if (!(cr & PCI_CR_MAST_EN))
		pci_attr_w16(devind, PCI_CR, cr | PCI_CR_MAST_EN);

	/* Optionally map flash memory. */
	e1000_map_flash(e, devind, did);

	/* Output debug information. */
	status = e1000_reg_read(e, E1000_REG_STATUS);
	E1000_DEBUG(3, ("%s: MEM at %p, IRQ %d\n", netdriver_name(),
	    e->regs, e->irq));
	E1000_DEBUG(3, ("%s: link %s, %s duplex\n", netdriver_name(),
	    status & 3 ? "up"   : "down", status & 1 ? "full" : "half"));

	return TRUE;
}

/*
 * Reset the card.
 */
static void
e1000_reset_hw(e1000_t * e)
{

	/* Assert a Device Reset signal. */
	e1000_reg_set(e, E1000_REG_CTRL, E1000_REG_CTRL_RST);

	/* Wait one microsecond. */
	micro_delay(16000);
}

/*
 * Initialize and return the card's ethernet address.
 */
static void
e1000_init_addr(e1000_t * e, netdriver_addr_t * addr)
{
	static char eakey[] = E1000_ENVVAR "#_EA";
	static char eafmt[] = "x:x:x:x:x:x";
	u16_t word;
	int i;
	long v;

	/* Do we have a user defined ethernet address? */
	eakey[sizeof(E1000_ENVVAR)-1] = '0' + e1000_instance;

	for (i = 0; i < 6; i++) {
		if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
			break;
		else
			addr->na_addr[i] = v;
	}

	/* If that fails, read Ethernet Address from EEPROM. */
	if (i != 6) {
		for (i = 0; i < 3; i++) {
			word = e->eeprom_read(e, i);
			addr->na_addr[i * 2]     = (word & 0x00ff);
			addr->na_addr[i * 2 + 1] = (word & 0xff00) >> 8;
		}
	}

	/* Set Receive Address. */
	e1000_set_hwaddr(addr);

	E1000_DEBUG(3, ("%s: Ethernet Address %x:%x:%x:%x:%x:%x\n",
	    netdriver_name(),
	    addr->na_addr[0], addr->na_addr[1], addr->na_addr[2],
	    addr->na_addr[3], addr->na_addr[4], addr->na_addr[5]));
}

/*
 * Initialize receive and transmit buffers.
 */
static void
e1000_init_buf(e1000_t * e)
{
	phys_bytes rx_desc_p, rx_buff_p;
	phys_bytes tx_desc_p, tx_buff_p;
	int i;

	/* Number of descriptors. */
	e->rx_desc_count = E1000_RXDESC_NR;
	e->tx_desc_count = E1000_TXDESC_NR;

	/* Allocate receive descriptors. */
	if ((e->rx_desc = alloc_contig(sizeof(e1000_rx_desc_t) *
	    e->rx_desc_count, AC_ALIGN4K, &rx_desc_p)) == NULL)
		panic("failed to allocate RX descriptors");

	memset(e->rx_desc, 0, sizeof(e1000_rx_desc_t) * e->rx_desc_count);

	/* Allocate receive buffers. */
	e->rx_buffer_size = E1000_RXDESC_NR * E1000_IOBUF_SIZE;

	if ((e->rx_buffer = alloc_contig(e->rx_buffer_size, AC_ALIGN4K,
	    &rx_buff_p)) == NULL)
		panic("failed to allocate RX buffers");

	/* Set up receive descriptors. */
	for (i = 0; i < E1000_RXDESC_NR; i++)
		e->rx_desc[i].buffer = rx_buff_p + i * E1000_IOBUF_SIZE;

	/* Allocate transmit descriptors. */
	if ((e->tx_desc = alloc_contig(sizeof(e1000_tx_desc_t) *
	    e->tx_desc_count, AC_ALIGN4K, &tx_desc_p)) == NULL)
		panic("failed to allocate TX descriptors");

	memset(e->tx_desc, 0, sizeof(e1000_tx_desc_t) * e->tx_desc_count);

	/* Allocate transmit buffers. */
	e->tx_buffer_size = E1000_TXDESC_NR * E1000_IOBUF_SIZE;

	if ((e->tx_buffer = alloc_contig(e->tx_buffer_size, AC_ALIGN4K,
	    &tx_buff_p)) == NULL)
		panic("failed to allocate TX buffers");

	/* Set up transmit descriptors. */
	for (i = 0; i < E1000_TXDESC_NR; i++)
		e->tx_desc[i].buffer = tx_buff_p + i * E1000_IOBUF_SIZE;

	/* Set up the receive ring registers. */
	e1000_reg_write(e, E1000_REG_RDBAL, rx_desc_p);
	e1000_reg_write(e, E1000_REG_RDBAH, 0);
	e1000_reg_write(e, E1000_REG_RDLEN,
	    e->rx_desc_count * sizeof(e1000_rx_desc_t));
	e1000_reg_write(e, E1000_REG_RDH, 0);
	e1000_reg_write(e, E1000_REG_RDT, e->rx_desc_count - 1);
	e1000_reg_unset(e, E1000_REG_RCTL, E1000_REG_RCTL_BSIZE);
	e1000_reg_set(e, E1000_REG_RCTL, E1000_REG_RCTL_EN);

	/* Set up the transmit ring registers. */
	e1000_reg_write(e, E1000_REG_TDBAL, tx_desc_p);
	e1000_reg_write(e, E1000_REG_TDBAH, 0);
	e1000_reg_write(e, E1000_REG_TDLEN,
	    e->tx_desc_count * sizeof(e1000_tx_desc_t));
	e1000_reg_write(e, E1000_REG_TDH, 0);
	e1000_reg_write(e, E1000_REG_TDT, 0);
	e1000_reg_set(e, E1000_REG_TCTL,
	    E1000_REG_TCTL_EN | E1000_REG_TCTL_PSP);
}

/*
 * Initialize the hardware.  Return the ethernet address.
 */
static void
e1000_init_hw(e1000_t * e, netdriver_addr_t * addr)
{
	int r, i;

	e->irq_hook = e->irq;

	/*
	 * Set the interrupt handler and policy.  Do not automatically
	 * reenable interrupts.  Return the IRQ line number on interrupts.
	 */
	if ((r = sys_irqsetpolicy(e->irq, 0, &e->irq_hook)) != OK)
		panic("sys_irqsetpolicy failed: %d", r);
	if ((r = sys_irqenable(&e->irq_hook)) != OK)
		panic("sys_irqenable failed: %d", r);

	/* Reset hardware. */
	e1000_reset_hw(e);

	/*
	 * Initialize appropriately, according to section 14.3 General
	 * Configuration of Intel's Gigabit Ethernet Controllers Software
	 * Developer's Manual.
	 */
	e1000_reg_set(e, E1000_REG_CTRL,
	    E1000_REG_CTRL_ASDE | E1000_REG_CTRL_SLU);
	e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_LRST);
	e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_PHY_RST);
	e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_ILOS);
	e1000_reg_write(e, E1000_REG_FCAL, 0);
	e1000_reg_write(e, E1000_REG_FCAH, 0);
	e1000_reg_write(e, E1000_REG_FCT, 0);
	e1000_reg_write(e, E1000_REG_FCTTV, 0);
	e1000_reg_unset(e, E1000_REG_CTRL, E1000_REG_CTRL_VME);

	/* Clear Multicast Table Array (MTA). */
	for (i = 0; i < 128; i++)
		e1000_reg_write(e, E1000_REG_MTA + i * 4, 0);

	/* Initialize statistics registers. */
	for (i = 0; i < 64; i++)
		e1000_reg_write(e, E1000_REG_CRCERRS + i * 4, 0);

	/* Acquire MAC address and set up RX/TX buffers. */
	e1000_init_addr(e, addr);
	e1000_init_buf(e);

	/* Enable interrupts. */
	e1000_reg_set(e, E1000_REG_IMS, E1000_REG_IMS_LSC | E1000_REG_IMS_RXO |
	    E1000_REG_IMS_RXT | E1000_REG_IMS_TXQE | E1000_REG_IMS_TXDW);
}

/*
 * Set receive mode.
 */
static void
e1000_set_mode(unsigned int mode, const netdriver_addr_t * mcast_list __unused,
	unsigned int mcast_count __unused)
{
	e1000_t *e;
	uint32_t rctl;

	e = &e1000_state;

	rctl = e1000_reg_read(e, E1000_REG_RCTL);

	rctl &= ~(E1000_REG_RCTL_BAM | E1000_REG_RCTL_MPE |
	    E1000_REG_RCTL_UPE);

	/* TODO: support for NDEV_MODE_DOWN and multicast lists */
	if (mode & NDEV_MODE_BCAST)
		rctl |= E1000_REG_RCTL_BAM;
	if (mode & (NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL))
		rctl |= E1000_REG_RCTL_MPE;
	if (mode & NDEV_MODE_PROMISC)
		rctl |= E1000_REG_RCTL_BAM | E1000_REG_RCTL_MPE |
		    E1000_REG_RCTL_UPE;

	e1000_reg_write(e, E1000_REG_RCTL, rctl);
}

/*
 * Set hardware address.
 */
static void
e1000_set_hwaddr(const netdriver_addr_t * hwaddr)
{
	e1000_t *e;

	e = &e1000_state;

	e1000_reg_write(e, E1000_REG_RAL,
	    *(const u32_t *)(&hwaddr->na_addr[0]));
	e1000_reg_write(e, E1000_REG_RAH,
	    *(const u16_t *)(&hwaddr->na_addr[4]));
	e1000_reg_set(e, E1000_REG_RAH, E1000_REG_RAH_AV);
}

/*
 * Try to send a packet.
 */
static int
e1000_send(struct netdriver_data * data, size_t size)
{
	e1000_t *e;
	e1000_tx_desc_t *desc;
	unsigned int head, tail, next;
	char *ptr;

	e = &e1000_state;

	if (size > E1000_IOBUF_SIZE)
		panic("packet too large to send");

	/*
	 * The queue tail must not advance to the point that it is equal to the
	 * queue head, since this condition indicates that the queue is empty.
	 */
	head = e1000_reg_read(e, E1000_REG_TDH);
	tail = e1000_reg_read(e, E1000_REG_TDT);
	next = (tail + 1) % e->tx_desc_count;

	if (next == head)
		return SUSPEND;

	/* The descriptor to use is the one pointed to by the current tail. */
	desc = &e->tx_desc[tail];

	/* Copy the packet from the caller. */
	ptr = e->tx_buffer + tail * E1000_IOBUF_SIZE;

	netdriver_copyin(data, 0, ptr, size);

	/* Mark this descriptor ready. */
	desc->status = 0;
	desc->length = size;
	desc->command = E1000_TX_CMD_EOP | E1000_TX_CMD_FCS | E1000_TX_CMD_RS;

	/* Increment tail.  Start transmission. */
	e1000_reg_write(e, E1000_REG_TDT, next);

	return OK;
}

/*
 * Try to receive a packet.
 */
static ssize_t
e1000_recv(struct netdriver_data * data, size_t max)
{
	e1000_t *e;
	e1000_rx_desc_t *desc;
	unsigned int head, tail, cur;
	char *ptr;
	size_t size;

	e = &e1000_state;

	/* If the queue head and tail are equal, the queue is empty. */
	head = e1000_reg_read(e, E1000_REG_RDH);
	tail = e1000_reg_read(e, E1000_REG_RDT);

	E1000_DEBUG(4, ("%s: head=%u, tail=%u\n",
	    netdriver_name(), head, tail));

	if (head == tail)
		return SUSPEND;

	/* Has a packet been received? */
	cur = (tail + 1) % e->rx_desc_count;
	desc = &e->rx_desc[cur];

	if (!(desc->status & E1000_RX_STATUS_DONE))
		return SUSPEND;

	/*
	 * HACK: we expect all packets to fit in a single receive buffer.
	 * Eventually, some sort of support to deal with packets spanning
	 * multiple receive descriptors should be added.  For now, we panic,
	 * so that we can continue after the restart; this is already an
	 * improvement over freezing (the old behavior of this driver).
	 */
	size = desc->length;

	if (!(desc->status & E1000_RX_STATUS_EOP))
		panic("received packet too large");

	/* Copy the packet to the caller. */
	ptr = e->rx_buffer + cur * E1000_IOBUF_SIZE;

	if (size > max)
		size = max;

	netdriver_copyout(data, 0, ptr, size);

	/* Reset the descriptor. */
	desc->status = 0;

	/* Increment tail. */
	e1000_reg_write(e, E1000_REG_RDT, cur);

	/* Return the size of the received packet. */
	return size;
}

/*
 * Return the link and media status.
 */
static unsigned int
e1000_get_link(uint32_t * media)
{
	uint32_t status, type;

	status = e1000_reg_read(&e1000_state, E1000_REG_STATUS);

	if (!(status & E1000_REG_STATUS_LU))
		return NDEV_LINK_DOWN;

	if (status & E1000_REG_STATUS_FD)
		type = IFM_ETHER | IFM_FDX;
	else
		type = IFM_ETHER | IFM_HDX;

	switch (status & E1000_REG_STATUS_SPEED) {
	case E1000_REG_STATUS_SPEED_10:
		type |= IFM_10_T;
		break;
	case E1000_REG_STATUS_SPEED_100:
		type |= IFM_100_TX;
		break;
	case E1000_REG_STATUS_SPEED_1000_A:
	case E1000_REG_STATUS_SPEED_1000_B:
		type |= IFM_1000_T;
		break;
	}

	*media = type;
	return NDEV_LINK_UP;
}

/*
 * Handle an interrupt.
 */
static void
e1000_intr(unsigned int __unused mask)
{
	e1000_t *e;
	u32_t cause;

	E1000_DEBUG(3, ("e1000: interrupt\n"));

	e = &e1000_state;

	/* Reenable interrupts. */
	if (sys_irqenable(&e->irq_hook) != OK)
		panic("failed to re-enable IRQ");

	/* Read the Interrupt Cause Read register. */
	if ((cause = e1000_reg_read(e, E1000_REG_ICR)) != 0) {
		if (cause & E1000_REG_ICR_LSC)
			netdriver_link();

		if (cause & (E1000_REG_ICR_RXO | E1000_REG_ICR_RXT))
			netdriver_recv();

		if (cause & (E1000_REG_ICR_TXQE | E1000_REG_ICR_TXDW))
			netdriver_send();
	}
}

/*
 * Do regular processing.
 */
static void
e1000_tick(void)
{
	e1000_t *e;

	e = &e1000_state;

	/* Update statistics. */
	netdriver_stat_ierror(e1000_reg_read(e, E1000_REG_RXERRC));
	netdriver_stat_ierror(e1000_reg_read(e, E1000_REG_CRCERRS));
	netdriver_stat_ierror(e1000_reg_read(e, E1000_REG_MPC));
	netdriver_stat_coll(e1000_reg_read(e, E1000_REG_COLC));
}

/*
 * Stop the card.
 */
static void
e1000_stop(void)
{
	e1000_t *e;

	e = &e1000_state;

	E1000_DEBUG(3, ("%s: stop()\n", netdriver_name()));

	e1000_reset_hw(e);
}

/*
 * Read from a register.
 */
static uint32_t
e1000_reg_read(e1000_t * e, uint32_t reg)
{
	uint32_t value;

	/* Assume a sane register. */
	assert(reg < 0x1ffff);

	/* Read from memory mapped register. */
	value = *(volatile uint32_t *)(e->regs + reg);

	/* Return the result. */
	return value;
}

/*
 * Write to a register.
 */
static void
e1000_reg_write(e1000_t * e, uint32_t reg, uint32_t value)
{

	/* Assume a sane register. */
	assert(reg < 0x1ffff);

	/* Write to memory mapped register. */
	*(volatile u32_t *)(e->regs + reg) = value;
}

/*
 * Set bits in a register.
 */
static void
e1000_reg_set(e1000_t * e, uint32_t reg, uint32_t value)
{
	uint32_t data;

	/* First read the current value. */
	data = e1000_reg_read(e, reg);

	/* Set bits, and write back. */
	e1000_reg_write(e, reg, data | value);
}

/*
 * Clear bits in a register.
 */
static void
e1000_reg_unset(e1000_t * e, uint32_t reg, uint32_t value)
{
	uint32_t data;

	/* First read the current value. */
	data = e1000_reg_read(e, reg);

	/* Unset bits, and write back. */
	e1000_reg_write(e, reg, data & ~value);
}

/*
 * Read from EEPROM.
 */
static u16_t
eeprom_eerd(e1000_t * e, int reg)
{
	u32_t data;

	/* Request EEPROM read. */
	e1000_reg_write(e, E1000_REG_EERD,
	    (reg << e->eeprom_addr_off) | (E1000_REG_EERD_START));

	/* Wait until ready. */
	while (!((data = (e1000_reg_read(e, E1000_REG_EERD))) &
	    e->eeprom_done_bit));

	return data >> 16;
}

/*
 * Initialize ICH8 flash.
 */
static int
eeprom_ich_init(e1000_t * e)
{
	union ich8_hws_flash_status hsfsts;
	int ret_val = -1;
	int i = 0;

	hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);

	/* Check if the flash descriptor is valid */
	if (hsfsts.hsf_status.fldesvalid == 0) {
		E1000_DEBUG(3, ("Flash descriptor invalid. "
		    "SW Sequencing must be used."));
		return ret_val;
	}

	/* Clear FCERR and DAEL in hw status by writing 1 */
	hsfsts.hsf_status.flcerr = 1;
	hsfsts.hsf_status.dael = 1;

	E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS, hsfsts.regval);

	/*
	 * Either we should have a hardware SPI cycle in progress bit to check
	 * against, in order to start a new cycle or FDONE bit should be
	 * changed in the hardware so that it is 1 after hardware reset, which
	 * can then be used as an indication whether a cycle is in progress or
	 * has been completed.
	 */
	if (hsfsts.hsf_status.flcinprog == 0) {
		/*
		 * There is no cycle running at present, so we can start a
		 * cycle.  Begin by setting Flash Cycle Done.
		 */
		hsfsts.hsf_status.flcdone = 1;
		E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS, hsfsts.regval);
		ret_val = 0;
	} else {
		/*
		 * Otherwise poll for sometime so the current cycle has a
		 * chance to end before giving up.
		 */
		for (i = 0; i < ICH_FLASH_READ_COMMAND_TIMEOUT; i++) {
			hsfsts.regval = E1000_READ_FLASH_REG16(e,
			    ICH_FLASH_HSFSTS);

			if (hsfsts.hsf_status.flcinprog == 0) {
				ret_val = 0;
				break;
			}
			micro_delay(16000);
		}
		if (ret_val == 0) {
			/*
			 * Successful in waiting for previous cycle to timeout,
			 * now set the Flash Cycle Done.
			 */
			hsfsts.hsf_status.flcdone = 1;
			E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFSTS,
			    hsfsts.regval);
		} else {
			E1000_DEBUG(3,
			    ("Flash controller busy, cannot get access"));
		}
	}

	return ret_val;
}

/*
 * Start ICH8 flash cycle.
 */
static int
eeprom_ich_cycle(e1000_t * e, u32_t timeout)
{
	union ich8_hws_flash_ctrl hsflctl;
	union ich8_hws_flash_status hsfsts;
	int ret_val = -1;
	u32_t i = 0;

	E1000_DEBUG(3, ("e1000_flash_cycle_ich8lan"));

	/* Start a cycle by writing 1 in Flash Cycle Go in Hw Flash Control */
	hsflctl.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFCTL);
	hsflctl.hsf_ctrl.flcgo = 1;
	E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFCTL, hsflctl.regval);

	/* Wait till the FDONE bit is set to 1 */
	do {
		hsfsts.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFSTS);
		if (hsfsts.hsf_status.flcdone == 1)
			break;
		micro_delay(16000);
	} while (i++ < timeout);

	if (hsfsts.hsf_status.flcdone == 1 && hsfsts.hsf_status.flcerr == 0)
		ret_val = 0;

	return ret_val;
}

/*
 * Read from ICH8 flash.
 */
static u16_t
eeprom_ich(e1000_t * e, int reg)
{
	union ich8_hws_flash_status hsfsts;
	union ich8_hws_flash_ctrl hsflctl;
	u32_t flash_linear_addr;
	u32_t flash_data = 0;
	int ret_val = -1;
	u8_t count = 0;
	u16_t data = 0;

	E1000_DEBUG(3, ("e1000_read_flash_data_ich8lan"));

	if (reg > ICH_FLASH_LINEAR_ADDR_MASK)
		return data;

	reg *= sizeof(u16_t);
	flash_linear_addr = (ICH_FLASH_LINEAR_ADDR_MASK & reg) +
	    e->flash_base_addr;

	do {
		micro_delay(16000);

		/* Steps */
		ret_val = eeprom_ich_init(e);
		if (ret_val != 0)
			break;

		hsflctl.regval = E1000_READ_FLASH_REG16(e, ICH_FLASH_HSFCTL);
		/* 0b/1b corresponds to 1 or 2 byte size, respectively. */
		hsflctl.hsf_ctrl.fldbcount = 1;
		hsflctl.hsf_ctrl.flcycle = ICH_CYCLE_READ;
		E1000_WRITE_FLASH_REG16(e, ICH_FLASH_HSFCTL, hsflctl.regval);
		E1000_WRITE_FLASH_REG(e, ICH_FLASH_FADDR, flash_linear_addr);

		ret_val = eeprom_ich_cycle(e, ICH_FLASH_READ_COMMAND_TIMEOUT);

		/*
		 * Check if FCERR is set to 1, if set to 1, clear it and try
		 * the whole sequence a few more times, else read in (shift in)
		 * the Flash Data0, the order is least significant byte first
		 * msb to lsb.
		 */
		if (ret_val == 0) {
			flash_data = E1000_READ_FLASH_REG(e, ICH_FLASH_FDATA0);
			data = (u16_t)(flash_data & 0x0000FFFF);
			break;
		} else {
			/*
			 * If we've gotten here, then things are probably
			 * completely hosed, but if the error condition is
			 * detected, it won't hurt to give it another try...
			 * ICH_FLASH_CYCLE_REPEAT_COUNT times.
			 */
			hsfsts.regval = E1000_READ_FLASH_REG16(e,
			    ICH_FLASH_HSFSTS);

			if (hsfsts.hsf_status.flcerr == 1) {
				/* Repeat for some time before giving up. */
				continue;
			} else if (hsfsts.hsf_status.flcdone == 0) {
				E1000_DEBUG(3, ("Timeout error - flash cycle "
				    "did not complete."));
				break;
			}
		}
	} while (count++ < ICH_FLASH_CYCLE_REPEAT_COUNT);

	return data;
}
