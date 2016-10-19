#include "vt6105.h"

/* global value */
static vt_driver g_driver;
static int g_instance;

/* I/O function */
static u8_t my_inb(u16_t port) {
	u32_t value;
	int r;
	if ((r = sys_inb(port, &value)) != OK)
		printf("VT6105: sys_inb failed: %d\n", r);
	return (u8_t)value;
}
#define vt_inb(port, offset)	(my_inb((port) + (offset)))

static u16_t my_inw(u16_t port) {
	u32_t value;
	int r;
	if ((r = sys_inw(port, &value)) != OK)
		printf("VT6105: sys_inw failed: %d\n", r);
	return (u16_t)value;
}
#define vt_inw(port, offset)	(my_inw((port) + (offset)))

static u32_t my_inl(u16_t port) {
	u32_t value;
	int r;
	if ((r = sys_inl(port, &value)) != OK)
		printf("VT6105: sys_inl failed: %d\n", r);
	return value;
}
#define vt_inl(port, offset)	(my_inl((port) + (offset)))

static void my_outb(u16_t port, u8_t value) {
	int r;
	if ((r = sys_outb(port, value)) != OK)
		printf("VT6105: sys_outb failed: %d\n", r);
}
#define vt_outb(port, offset, value)	(my_outb((port) + (offset), (value)))

static void my_outw(u16_t port, u16_t value) {
	int r;
	if ((r = sys_outw(port, value)) != OK)
		printf("VT6105: sys_outw failed: %d\n", r);
}
#define vt_outw(port, offset, value)	(my_outw((port) + (offset), (value)))

static void my_outl(u16_t port, u32_t value) {
	int r;
	if ((r = sys_outl(port, value)) != OK)
		printf("VT6105: sys_outl failed: %d\n", r);
}
#define vt_outl(port, offset, value)	(my_outl((port) + (offset), (value)))

/* driver interface */
static int vt_init(unsigned int instance, ether_addr_t *addr);
static void vt_stop(void);
static void vt_mode(unsigned int mode);
static ssize_t vt_recv(struct netdriver_data *data, size_t max);
static int vt_send(struct netdriver_data *data, size_t size);
static void vt_intr(unsigned int mask);
static void vt_stat(eth_stat_t *stat);
static void vt_alarm(clock_t stamp);

static int vt_probe(vt_driver *pdev, int instance);
static int vt_init_buf(vt_driver *pdev);
static int vt_init_hw(vt_driver *pdev, ether_addr_t *addr);
static void vt_reset_hw(vt_driver *pdev);
static void vt_power_init(vt_driver *pdev);
static void vt_conf_addr(vt_driver *pdev, ether_addr_t *addr);
static void vt_check_link(vt_driver *pdev);
static void vt_handler(vt_driver *pdev);
static void vt_check_ints(vt_driver *pdev);

static const struct netdriver vt_table = {
	.ndr_init = vt_init,
	.ndr_stop = vt_stop,
	.ndr_mode = vt_mode,
	.ndr_recv = vt_recv,
	.ndr_send = vt_send,
	.ndr_stat = vt_stat,
	.ndr_intr = vt_intr,
	.ndr_alarm = vt_alarm
};

int main(int argc, char *argv[]) {
	env_setargs(argc, argv);
	netdriver_task(&vt_table);
}

/* Initialize the driver */
static int vt_init(unsigned int instance, ether_addr_t *addr) {
	int ret = 0;

	/* Intialize driver data structure */
	memset(&g_driver, 0, sizeof(g_driver));
	g_driver.vt_link = VT_LINK_UNKNOWN;
	strcpy(g_driver.vt_name, "vt6105#0");
	g_driver.vt_name[7] += instance;
	g_instance = instance;

	/* Probe the device */
	if (vt_probe(&g_driver, instance)) {
		printf("VT6105: Device is not found!\n");
		ret = -ENODEV;
		goto err_probe;
	}

	/* Allocate and initialize buffer */
	if (vt_init_buf(&g_driver)) {
		printf("VT6105: Device is not found!\n");
		ret = -ENODEV;
		goto err_init_buf;
	}

	/* Intialize hardware */
	if (vt_init_hw(&g_driver, addr)) {
		printf("VT6105: Find to initialize hardware!\n");
		ret = -EIO;
		goto err_init_hw;
	}

	/* Use a synchronous alarm instead of a watchdog timer */
	sys_setalarm(sys_hz(), 0);

	/* Clear send and recv flag */
	g_driver.vt_send_flag = FALSE;
	g_driver.vt_recv_flag = FALSE;

	return 0;

err_init_hw:
	free_contig(g_driver.vt_buf, g_driver.vt_buf_size);
err_init_buf:
err_probe:
	return ret;
}

/* Match the device and get base address */
static int vt_probe(vt_driver *pdev, int instance) {
	int devind;
	u16_t cr, vid, did;
	u32_t bar;
	u8_t irq, rev;

	/* Find pci device */
	pci_init();
	if (!pci_first_dev(&devind, &vid, &did))
		return -EIO;
	while (instance--) {
		if (!pci_next_dev(&devind, &vid, &did))
			return -EIO;
	}
	pci_reserve(devind);

	/* Enable bus mastering */
	cr = pci_attr_r16(devind, PCI_CR);
	if (!(cr & PCI_CR_MAST_EN))
		pci_attr_w16(devind, PCI_CR, cr | PCI_CR_MAST_EN);

	/* Get base address */
	bar = pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
	if (bar < 0x400) {
		printf("VT6105: base address is not properly configured!\n");
		return -EIO;
	}
	pdev->vt_base_addr = bar;

	/* Get irq number */
	irq = pci_attr_r8(devind, PCI_ILR);
	pdev->vt_irq = irq;

	/* Get revision ID */
	rev = pci_attr_r8(devind, PCI_REV);
	pdev->vt_revision = rev;

#ifdef VT6105_DEBUG
	printf("VT6105: Base address is 0x%08x\n", pdev->vt_base_addr);
	printf("VT6105: IRQ number is 0x%08x\n", pdev->vt_irq);
	printf("VT6105: Revision ID is 0x%08x\n", pdev->vt_revision);
#endif

	return 0;
}

/* Allocate and initialize buffer */
static int vt_init_buf(vt_driver *pdev) {
	size_t rx_desc_size, tx_desc_size, rx_buf_size, tx_buf_size, tot_buf_size;
	vt_desc *desc;
	phys_bytes buf_dma, next;
	char *buf;
	int i;

	/* Build Rx and Tx descriptor buffer */
	rx_desc_size = VT_RX_DESC_NUM * sizeof(vt_desc);
	tx_desc_size = VT_TX_DESC_NUM * sizeof(vt_desc);

	/* Allocate Rx and Tx buffer */
	tx_buf_size = VT_TX_BUF_SIZE;
	if (tx_buf_size % 4)
		tx_buf_size += 4 - (tx_buf_size % 4);
	rx_buf_size = VT_RX_BUF_SIZE;
	tot_buf_size = rx_desc_size + tx_desc_size;
	tot_buf_size += VT_TX_DESC_NUM * tx_buf_size + VT_RX_DESC_NUM * rx_buf_size;
	if (tot_buf_size % 4096)
		tot_buf_size += 4096 - (tot_buf_size % 4096);

	if (!(buf = alloc_contig(tot_buf_size, 0, &buf_dma))) {
		printf("VT6105: Fail to allocate memory!\n");
		return -ENOMEM;
	}
	pdev->vt_buf_size = tot_buf_size;
	pdev->vt_buf = buf;

	/* Rx descriptor */
	pdev->vt_rx_desc = (vt_desc *)buf;
	pdev->vt_rx_desc_dma = buf_dma;
	memset(buf, 0, rx_desc_size);
	buf += rx_desc_size;
	buf_dma += rx_desc_size;

	/* Tx descriptor */
	pdev->vt_tx_desc = (vt_desc *)buf;
	pdev->vt_tx_desc_dma = buf_dma;
	memset(buf, 0, tx_desc_size);
	buf += tx_desc_size;
	buf_dma += tx_desc_size;

	/* Rx buffer assignment */
	desc = pdev->vt_rx_desc;
	next = pdev->vt_rx_desc_dma;
	for (i = 0; i < VT_RX_DESC_NUM; i++) {
		/* Set Rx buffer */
		pdev->vt_rx[i].buf_dma = buf_dma;
		pdev->vt_rx[i].buf = buf;
		buf_dma += rx_buf_size;
		buf += rx_buf_size;

		/* Set Rx descriptor */
		desc->status = VT_DESC_OWN |
				((VT_RX_BUF_SIZE << 16) & VT_DESC_RX_LENMASK);
		desc->addr = pdev->vt_rx[i].buf_dma;
		desc->length = VT_RX_BUF_SIZE;
		if (i == (VT_RX_DESC_NUM - 1))
			desc->next_desc = pdev->vt_rx_desc_dma;
		else {
			next += sizeof(vt_desc);
			desc->next_desc = next;
			desc++;
		}
	}

	/* Tx buffer assignment */
	desc = pdev->vt_tx_desc;
	next = pdev->vt_tx_desc_dma;
	for (i = 0; i < VT_TX_DESC_NUM; i++) {
		/* Set Tx buffer */
		pdev->vt_tx[i].busy = 0;
		pdev->vt_tx[i].buf_dma = buf_dma;
		pdev->vt_tx[i].buf = buf;
		buf_dma += tx_buf_size;
		buf += tx_buf_size;

		/* Set Rx descriptor */
		desc->addr = pdev->vt_tx[i].buf_dma;
		desc->length = VT_TX_BUF_SIZE;
		if (i == (VT_TX_DESC_NUM - 1))
			desc->next_desc = pdev->vt_tx_desc_dma;
		else {
			next += sizeof(vt_desc);
			desc->next_desc = next;
			desc++;
		}
	}
	pdev->vt_tx_busy_num = 0;
	pdev->vt_tx_head = 0;
	pdev->vt_tx_tail = 0;
	pdev->vt_rx_head = 0;
	pdev->vt_tx_alive = FALSE;

	return 0;
}

/* Intialize hardware */
static int vt_init_hw(vt_driver *pdev, ether_addr_t *addr) {
	int r, ret;

	/* Set the interrupt handler */
	pdev->vt_hook = pdev->vt_irq;
	if ((r = sys_irqsetpolicy(pdev->vt_irq, 0, &pdev->vt_hook)) != OK) {
		printf("VT6105: Fail to set IRQ policy: %d!\n", r);
		ret = -EFAULT;
		goto err_irq_policy;
	}

	/* Reset hardware */
	vt_reset_hw(pdev);

	/* Enable IRQ */
	if ((r = sys_irqenable(&pdev->vt_hook)) != OK) {
		printf("VT6105: Fail to enable IRQ: %d!\n", r);
		ret = -EFAULT;
		goto err_irq_enable;
	}

	/* Configure MAC address */
	vt_conf_addr(pdev, addr);

	/* Detect link status */
	vt_check_link(pdev);
#ifdef VT6105_DEBUG
	if (pdev->vt_link)
		printf("VT6105: Link up!\n");
	else
		printf("VT6105: Link down!\n");
#endif

	return 0;

err_irq_enable:
err_irq_policy:
	return ret;
}

/* Reset hardware */
static void vt_reset_hw(vt_driver *pdev) {
	u16_t base = pdev->vt_base_addr;
	u8_t db;
	u16_t dw;
	u32_t dl;

	/* Let power registers into sane state */
	vt_power_init(pdev);

	/* Reset the chip */
	vt_outw(base, VT_REG_CR, VT_CMD_RESET);
	vt_inb(base, VT_REG_ADDR);

#ifdef VT6105_DEBUG
	if (vt_inw(base, VT_REG_CR) & VT_CMD_RESET) {
		vt_outb(base, VT_REG_MCR1, 0x40);
		usleep(100000);
		if (vt_inw(base, VT_REG_CR) & VT_CMD_RESET)
			printf("VT6105: Fail to completely reset!\n");
	}
	else
		printf("VT6105: Reset successfully!\n");
#endif

	/* Initialize regsiters */
	vt_outw(base, VT_REG_BCR0, 0x0006);
	vt_outb(base, VT_REG_TCR, 0x20);	/* Tx threshold is 256 bytes */
	vt_outb(base, VT_REG_RCR, 0x78);	/* Rx threshold is 256 bytes */
	vt_outl(base, VT_REG_RD_BASE, pdev->vt_rx_desc_dma);	/* Set Rx descriptor base address */
	vt_outl(base, VT_REG_TD_BASE, pdev->vt_tx_desc_dma);	/* Set Tx descriptor base address */

#ifdef VT6105_DEBUG
	dl = vt_inl(base, VT_REG_RD_BASE);
	printf("VT6105: Rx descriptor DMA address is: 0x%08x\n", dl);
	dl = vt_inl(base, VT_REG_TD_BASE);
	printf("VT6105: Tx descriptor DMA address is: 0x%08x\n", dl);
#endif

	/* Enable interrupts */
	vt_outw(base, VT_REG_IMR, 0xfeff);

	/* Start the device, Rx and Tx */
	dw = VT_CMD_START | VT_CMD_RX_ON | VT_CMD_TX_ON | VT_CMD_NO_POLL | VT_CMD_FDUPLEX;
	vt_outw(base, VT_REG_CR, dw);
	dw = vt_inw(base, VT_REG_CR);

#ifdef VT6105_DEBUG
	dw = vt_inw(base, VT_REG_CR);
	printf("VT6105: Control status is 0x%04x\n", dw);
#endif
}

/* Initialize power registers */
static void vt_power_init(vt_driver *pdev) {
	u8_t db, wolstat;
	u16_t dw, base = pdev->vt_base_addr;
	u32_t dl;

	/* Make sure chip is in power state D0 */
	db = vt_inb(base, VT_REG_STICK);
	vt_outb(base, VT_REG_STICK, db & 0xfc);

	/* Disable "force PME-enable" */
	vt_outb(base, VT_REG_WOLC_SET, 0x80);

	/* Clear power-event config bits (WOL) */
	vt_outb(base, VT_REG_WOL_SET, 0xff);

	/* Save power-event status bits */
	wolstat = vt_inb(base, VT_REG_WOLS_SET);

	/* Clear power-event status bits */
	vt_outb(base, VT_REG_WOLS_CLR, 0xff);

#ifdef VT6105_DEBUG
	if (wolstat) {
		char reason[50];
		switch (wolstat) {
			case VT_WOL_MAGIC:
				strcpy(reason, "Magic packet");
				break;
			case VT_WOL_LINK_UP:
				strcpy(reason, "Link up");
				break;
			case VT_WOL_LINK_DOWN:
				strcpy(reason, "Link down");
				break;
			case VT_WOL_UCAST:
				strcpy(reason, "Unicast packet");
				break;
			case VT_WOL_MCAST:
				strcpy(reason, "Multicast/Broadcast packet");
				break;
			default:
				strcpy(reason, "Unknown");
		}
		printf("VT6105: Wake system up: %s\n", reason);
	}
#endif
}

static void vt_conf_addr(vt_driver *pdev, ether_addr_t *addr) {
	u16_t dw, base = pdev->vt_base_addr;
	int i;

	for (i=0; i<6; i++)
		addr->ea_addr[i] = vt_inb(base, VT_REG_ADDR + i);

#ifdef VT6105_DEBUG
	printf("VT6105: Ethernet address is %02x:%02x:%02x:%02x:%02x:%02x\n",
		addr->ea_addr[0], addr->ea_addr[1], addr->ea_addr[2],
		addr->ea_addr[3], addr->ea_addr[4], addr->ea_addr[5]);
#endif
}

/* Detect link status (MII interface) */
static void vt_check_link(vt_driver *pdev) {
	u16_t base = pdev->vt_base_addr;
	u32_t res;

	vt_outb(base, VT_REG_MII_CFG, 0x01);
	vt_outb(base, VT_REG_MII_ADDR, 0x01);
	vt_outb(base, VT_REG_MII_CR, 0x40);
	usleep(10000);
	res = vt_inw(base, VT_REG_MII_DATA);
	if (res & 0x0004)
		pdev->vt_link = TRUE;
	else
		pdev->vt_link = FALSE;
}

/* Stop the driver */
static void vt_stop(void) {
	u16_t base = g_driver.vt_base_addr;

	/* Free Rx and Tx buffer*/
	free_contig(g_driver.vt_buf, g_driver.vt_buf_size);

	/* Stop IRQ and device */
	vt_outw(base, VT_REG_IMR, 0x0000);
	vt_outw(base, VT_REG_CR, VT_CMD_STOP);
}

/* Set driver mode */
static void vt_mode(unsigned int mode) {
	vt_driver *pdev = &g_driver;
	u16_t base = pdev->vt_base_addr;
	u8_t rcr;

	pdev->vt_mode = mode;
	vt_outl(base, VT_REG_MAR0, 0xffffffff);
	vt_outl(base, VT_REG_MAR1, 0xffffffff);

	rcr = vt_inb(base, VT_REG_RCR);
	rcr &= ~(VT_RCR_AB | VT_RCR_AM | VT_RCR_AP | VT_RCR_AE | VT_RCR_AR);
	if (pdev->vt_mode & NDEV_PROMISC)
		rcr |= VT_RCR_AB | VT_RCR_AM | VT_RCR_AE | VT_RCR_AR;
	if (pdev->vt_mode & NDEV_BROAD)
		rcr |= VT_RCR_AB;
	if (pdev->vt_mode & NDEV_MULTI)
		rcr |= VT_RCR_AM;
	rcr |= VT_RCR_AP;
	vt_outb(base, VT_REG_RCR, 0x60 | rcr);
}

/* Receive data */
static ssize_t vt_recv(struct netdriver_data *data, size_t max) {
	vt_driver *pdev = &g_driver;
	u32_t rxstat, totlen, packlen;
	vt_desc *desc;
	int index, i;

	index = pdev->vt_rx_head;
	desc = pdev->vt_rx_desc;
	desc += index;

	/* Manage Rx buffer */
	for (;;) {
		rxstat = desc->status;

		if (rxstat & VT_DESC_OWN)
			return SUSPEND;

		if (rxstat & VT_DESC_RX_ALL_ERR) {
#ifdef VT6105_DEBUG
			printf("VT6105: Rx error: 0x%08x\n", rxstat);
#endif
			if (rxstat & VT_DESC_RX_FOV) {
#ifdef VT6105_DEBUG
				printf("VT6105: Rx buffer overflow\n");
#endif
				pdev->vt_stat.ets_fifoOver++;
			}
			if (rxstat & VT_DESC_RX_FAE) {
#ifdef VT6105_DEBUG
				printf("VT6105: Rx frames not align\n");
#endif
				pdev->vt_stat.ets_frameAll++;
			}
			if (rxstat & VT_DESC_RX_CRCE) {
#ifdef VT6105_DEBUG
				printf("VT6105: Rx CRC error\n");
#endif
				pdev->vt_stat.ets_CRCerr++;
			}
		}

		/* Normal packet */
		if ((rxstat & VT_DESC_RX_PACK) == VT_DESC_RX_PACK)
			break;

		/* Other packet */
		if (index == VT_RX_DESC_NUM - 1) {
			desc->status = VT_DESC_OWN |
						((VT_RX_BUF_SIZE << 16) & VT_DESC_RX_LENMASK);
			index = 0;
			desc = pdev->vt_rx_desc;
		}
		else {
			desc->status = VT_DESC_OWN |
						((VT_RX_BUF_SIZE << 16) & VT_DESC_RX_LENMASK);
			index++;
			desc++;
		}
	}

	/* Get data length */
	totlen = (rxstat & VT_DESC_RX_LENMASK) >> 16;
	if (totlen < 8 || totlen > 2 * ETH_MAX_PACK_SIZE) {
		printf("VT6105: Bad data length: %d\n", totlen);
		panic(NULL);
	}

	/* Substract CRC to get packet */
	packlen = totlen - ETH_CRC_SIZE;
	if (packlen > max)
		packlen = max;

	/* Copy data to user */
	netdriver_copyout(data, 0, pdev->vt_rx[index].buf, packlen);
	pdev->vt_stat.ets_packetR++;

	/* Set Rx descriptor status */
	if (index == VT_RX_DESC_NUM - 1) {
		desc->status = VT_DESC_OWN |
						((VT_RX_BUF_SIZE << 16) & VT_DESC_RX_LENMASK);
		index = 0;
	}
	else {
		desc->status = VT_DESC_OWN |
						((VT_RX_BUF_SIZE << 16) & VT_DESC_RX_LENMASK);
		index++;
	}
	pdev->vt_rx_head = index;

#ifdef VT6105_DEBUG
	printf("VT6105: Successfully receive a packet, length = %d\n", packlen);
#endif

	return packlen;
}

/* Transmit data */
static int vt_send(struct netdriver_data *data, size_t size) {
	vt_driver *pdev = &g_driver;
	vt_desc *desc;
	int tx_head, i;
	u8_t db;
	u16_t base = pdev->vt_base_addr;

	tx_head = pdev->vt_tx_head;
	desc = pdev->vt_tx_desc;
	desc += tx_head;

	if (pdev->vt_tx[tx_head].busy)
		return SUSPEND;

	/* Copy data from user */
	netdriver_copyin(data, 0, pdev->vt_tx[tx_head].buf, size);

	/* Set busy */
	pdev->vt_tx[tx_head].busy = TRUE;
	pdev->vt_tx_busy_num++;

	/* Set Tx descriptor status */
	desc->status = VT_DESC_OWN | VT_DESC_FIRST | VT_DESC_LAST;
	desc->length = 0x00e08000 | (size >= 60 ? size : 60);
	if (tx_head == VT_TX_DESC_NUM - 1)
		tx_head = 0;
	else
		tx_head++;
	pdev->vt_tx_head = tx_head;

	/* Wake up transmit channel */
	db = vt_inb(base, VT_REG_CR);
	db |= VT_CMD_TX_DEMAND;
	vt_outb(base, VT_REG_CR, db);

	return 0;
}

/* Handle Interrupt */
static void vt_intr(unsigned int mask) {
	int s;

	/* Run interrupt handler at driver level */
	vt_handler(&g_driver);

	/* Reenable interrupts for this hook */
	if ((s = sys_irqenable(&g_driver.vt_hook)) != OK)
		printf("VT6105: Cannot enable interrupts: %d\n", s);

	/* Perform tasks based on the flagged conditions */
	vt_check_ints(&g_driver);
}

/* Real handler interrupt */
static void vt_handler(vt_driver *pdev) {
	u16_t base = pdev->vt_base_addr;
	int intr_status;
	u8_t db;
	u16_t dw;
	u32_t dl, txstat;
	int flag = 0, tx_head, tx_tail;
	vt_desc *desc;

	/* Get interrupt status */
	intr_status = vt_inw(base, VT_REG_ISR);

	/* Clear interrupt */
	dw = intr_status & ~(VT_INTR_PCI_ERR | VT_INTR_PORT_CHANGE);
	vt_outw(base, VT_REG_ISR, dw);

	/* Check link status */
	if (intr_status & VT_INTR_PORT_CHANGE)
		printf("VT6105: Port state change!\n");

	/* Check interrupt status */
	if (intr_status & VT_INTR_RX_DONE) {
		pdev->vt_recv_flag = TRUE;
		flag++;

		if (intr_status & VT_INTR_RX_ERR) {
			pdev->vt_stat.ets_recvErr++;
			printf("VT6105: Rx error in interrupt: 0x%04x\n", intr_status);
		}
		if (intr_status & VT_INTR_RX_NOBUF)
			printf("VT6105: Rx no buffer in interrupt: 0x%04x\n", intr_status);
		if (intr_status & VT_INTR_RX_EMPTY)
			printf("VT6105: Rx buffer empty in interrupt: 0x%04x\n", intr_status);
		if (intr_status & VT_INTR_RX_OVERFLOW)
			printf("VT6105: Rx buffer overflow in interrupt: 0x%04x\n", intr_status);
		if (intr_status & VT_INTR_RX_DROPPED)
			printf("VT6105: Rx buffer dropped in interrupt: 0x%04x\n", intr_status);
	}
	if (intr_status & VT_INTR_TX_DONE) {
		pdev->vt_send_flag = TRUE;
		flag++;

		if (intr_status & VT_INTR_TX_ERR) {
			pdev->vt_stat.ets_sendErr++;
			printf("VT6105: Tx error in interrupt: 0x%04x\n", intr_status);
		}
		if (intr_status & VT_INTR_TX_UNDER_RUN)
			printf("VT6105: Tx buffer underflow in interrupt: 0x%04x\n", intr_status);
		if (intr_status & VT_INTR_TX_ABORT)
			printf("VT6105: Tx abort in interrupt: 0x%04x\n", intr_status);

		/* Manage Tx Buffer */
		tx_head = pdev->vt_tx_head;
		tx_tail = pdev->vt_tx_tail;
		while (tx_tail != tx_head) {
			desc = pdev->vt_tx_desc;
			desc += tx_tail;
			if (!pdev->vt_tx[tx_tail].busy)
				printf("VT6105: Strange, buffer not busy?\n");
			txstat = desc->status;

			/* Check whether the buffer is ready */
			if (txstat & VT_DESC_OWN) {
				break;
			}

			if (txstat & VT_DESC_TX_ERR) {
#ifdef VT6105_DEBUG
				printf("VT6105: Tx error: 0x%08x\n", txstat);
#endif
				if (txstat & VT_DESC_TX_UDF) {
#ifdef VT6105_DEBUG
					printf("VT6105: Tx buffer underflow\n");
#endif
					pdev->vt_stat.ets_fifoUnder++;
				}
				if (txstat & VT_DESC_TX_CRS) {
#ifdef VT6105_DEBUG
					printf("VT6105: Tx carrier sense lost\n");
#endif
					pdev->vt_stat.ets_carrSense++;
				}
				if (txstat & VT_DESC_TX_CDH) {
#ifdef VT6105_DEBUG
					printf("VT6105: Tx collision detect\n");
#endif
					pdev->vt_stat.ets_collision++;
				}
				if (txstat & VT_DESC_TX_OWC) {
#ifdef VT6105_DEBUG
					printf("VT6105: Tx buffer out of winidow\n");
#endif
					pdev->vt_stat.ets_OWC++;
				}
			}

			pdev->vt_stat.ets_packetT++;
			pdev->vt_tx[tx_tail].busy = FALSE;
			pdev->vt_tx_busy_num--;

			if (++tx_tail >= VT_TX_DESC_NUM)
				tx_tail = 0;

			pdev->vt_send_flag = TRUE;
			pdev->vt_recv_flag = TRUE;
			pdev->vt_tx_alive = TRUE;

#ifdef VT6105_DEBUG
			printf("VT6105: Successfully send a packet\n");
#endif
		}
		pdev->vt_tx_tail = tx_tail;
	}
	if (!flag) {
		printf("VT6105: Unknown error in interrupt: 0x%04x\n", intr_status);
		return;
	}

	/* Perform tasks based on the flagged condition */
	vt_check_ints(pdev);
}

/* Check interrupt and perform */
static void vt_check_ints(vt_driver *pdev) {
	if (!pdev->vt_recv_flag)
		return;
	pdev->vt_recv_flag = FALSE;

	/* Handle data receive */
	netdriver_recv();

	/* Handle data transmit */
	if (pdev->vt_send_flag) {
		pdev->vt_send_flag = FALSE;
		netdriver_send();
	}
}

static void vt_stat(eth_stat_t *stat) {
	memcpy(stat, &g_driver.vt_stat, sizeof(*stat));
}

static void vt_alarm(clock_t stamp) {
	vt_driver *pdev = &g_driver;

	/* Use a synchronous alarm instead of a watchdog timer */
	sys_setalarm(sys_hz(), 0);

	/* Assume the an idle system is alive  */
	if (!pdev->vt_tx_busy_num) {
		pdev->vt_tx_alive = TRUE;
		return;
	}
	if (pdev->vt_tx_alive) {
		pdev->vt_tx_alive = FALSE;
		return;
	}

	printf("VT6105: Resetting the driver\n");
	vt_reset_hw(pdev);
	pdev->vt_recv_flag = TRUE;

	vt_check_ints(pdev);
}
