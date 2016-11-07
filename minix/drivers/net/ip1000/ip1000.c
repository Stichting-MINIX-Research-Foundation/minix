#include "ip1000.h"

/* global value */
static ic_driver g_driver;
static int g_instance;

/* I/O function */
static u8_t my_inb(u16_t port) {
	u32_t value;
	int r;
	if ((r = sys_inb(port, &value)) != OK)
		printf("IP1000: sys_inb failed: %d\n", r);
	return (u8_t)value;
}
#define ic_inb(port, offset)	(my_inb((port) + (offset)))

static u16_t my_inw(u16_t port) {
	u32_t value;
	int r;
	if ((r = sys_inw(port, &value)) != OK)
		printf("IP1000: sys_inw failed: %d\n", r);
	return (u16_t)value;
}
#define ic_inw(port, offset)	(my_inw((port) + (offset)))

static u32_t my_inl(u16_t port) {
	u32_t value;
	int r;
	if ((r = sys_inl(port, &value)) != OK)
		printf("IP1000: sys_inl failed: %d\n", r);
	return value;
}
#define ic_inl(port, offset)	(my_inl((port) + (offset)))

static void my_outb(u16_t port, u8_t value) {
	int r;
	if ((r = sys_outb(port, value)) != OK)
		printf("IP1000: sys_outb failed: %d\n", r);
}
#define ic_outb(port, offset, value)	(my_outb((port) + (offset), (value)))

static void my_outw(u16_t port, u16_t value) {
	int r;
	if ((r = sys_outw(port, value)) != OK)
		printf("IP1000: sys_outw failed: %d\n", r);
}
#define ic_outw(port, offset, value)	(my_outw((port) + (offset), (value)))

static void my_outl(u16_t port, u32_t value) {
	int r;
	if ((r = sys_outl(port, value)) != OK)
		printf("IP1000: sys_outl failed: %d\n", r);
}
#define ic_outl(port, offset, value)	(my_outl((port) + (offset), (value)))

static int udelay(int n) {
	int i, j;
	int ret = 0;

	for (i=0; i<n; i++) {
		ret = 0;
		for (j=0; j<1000000; j++)
			ret++;
	}
	return ret;
}

static u16_t read_eeprom(ic_driver *pdev, int addr) {
	u16_t ret, data, val, base = pdev->ic_base_addr;
	int i;

	val = IC_EC_READ | (addr & 0xff);
	ic_outw(base, IC_REG_EEPROM_CTRL, val);
	for (i = 0; i < 100; i++) {
		usleep(100000);
		data = ic_inw(base, IC_REG_EEPROM_CTRL);
		if (!(data & IC_EC_BUSY)) {
			ret = ic_inw(base, IC_REG_EEPROM_DATA);
			break;
		}
	}
	if (i == 100)
		printf("IP1000: Fail to read EEPROM\n");
	return ret;
}

static int mdio_read(ic_driver *pdev, int phy_id, int phy_reg) {
	u16_t bit_data, base = pdev->ic_base_addr;
	struct {
		u32_t field;
		u32_t len;
	} p[] = {
		{0xffffffff, 32}, {0x01, 2}, {0x02, 2},
		{phy_id, 5}, {phy_reg, 5}, {0x00, 2}, {0x00, 16}, {0x00, 1}};
	int i, j;
	u8_t polarity, data;

	polarity = ic_inb(base, IC_REG_PHY_CTRL);
	polarity &= (IC_PC_DUPLEX_POLARITY | IC_PC_LINK_POLARITY);
	for (j = 0; j < 5; j++) {
		for (i = 0; i < p[j].len; i++) {
			data = (p[j].field >> (p[j].len - 1 - i)) << 1;
			data &= IC_PC_MGMTDATA;
			data |= polarity | IC_PC_MGMTDIR;
			ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_LO | data);
			udelay(10);
			ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_HI | data);
			udelay(10);
		}
	}
	ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_LO | 
						(polarity | IC_PC_MGMTDIR));
	udelay(10);
	ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_HI | 
						(polarity | IC_PC_MGMTDIR));
	udelay(10);
	
	ic_outb(base, IC_REG_PHY_CTRL, polarity | IC_PC_MGMTCLK_LO);
	udelay(10);
	bit_data = ((ic_inb(base, IC_REG_PHY_CTRL) & IC_PC_MGMTDATA) >> 1) & 1;
	ic_outb(base, IC_REG_PHY_CTRL, polarity | IC_PC_MGMTCLK_HI);
	udelay(10);

	for (i = 0; i < p[6].len; i++) {
		ic_outb(base, IC_REG_PHY_CTRL, polarity | IC_PC_MGMTCLK_LO);
		udelay(10);
		bit_data = ((ic_inb(base, IC_REG_PHY_CTRL) & IC_PC_MGMTDATA) >> 1) & 1;
		ic_outb(base, IC_REG_PHY_CTRL, polarity | IC_PC_MGMTCLK_HI);
		udelay(10);
		p[6].field |= (bit_data << (p[6].len - 1 - i));
	}

	for (i = 0; i < 3; i++) {
		ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_LO | 
							(polarity | IC_PC_MGMTDIR));
		udelay(10);
		ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_HI | 
							(polarity | IC_PC_MGMTDIR));
		udelay(10);
	}
	ic_outb(base, IC_REG_PHY_CTRL, polarity | IC_PC_MGMTCLK_LO | IC_PC_MGMTDIR);

	return p[6].field;
}

static void mdio_write(ic_driver *pdev, int phy_id, int phy_reg, int val) {
	u16_t bit_data, base = pdev->ic_base_addr;
	struct {
		u32_t field;
		u32_t len;
	} p[] = {
		{0xffffffff, 32}, {0x01, 2}, {0x01, 2},
		{phy_id, 5}, {phy_reg, 5}, {0x02, 2}, {val & 0xffff, 16}, {0x00, 1}};
	int i, j;
	u8_t polarity, data;

	polarity = ic_inb(base, IC_REG_PHY_CTRL);
	polarity &= (IC_PC_DUPLEX_POLARITY | IC_PC_LINK_POLARITY);
	for (j = 0; j < 7; j++) {
		for (i = 0; i < p[j].len; i++) {
			data = (p[j].field >> (p[j].len - 1 - i)) << 1;
			data &= IC_PC_MGMTDATA;
			data |= polarity | IC_PC_MGMTDIR;
			ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_LO | data);
			udelay(10);
			ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_HI | data);
			udelay(10);
		}
	}
	ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_LO | polarity);
	udelay(10);
	ic_inb(base, IC_REG_PHY_CTRL);
	ic_outb(base, IC_REG_PHY_CTRL, IC_PC_MGMTCLK_HI | polarity);
	udelay(10);
}

/* driver interface */
static int ic_init(unsigned int instance, ether_addr_t *addr);
static void ic_stop(void);
static void ic_mode(unsigned int mode);
static ssize_t ic_recv(struct netdriver_data *data, size_t max);
static int ic_send(struct netdriver_data *data, size_t size);
static void ic_intr(unsigned int mask);
static void ic_stat(eth_stat_t *stat);
static void ic_alarm(clock_t stamp);

static int ic_probe(ic_driver *pdev, int instance);
static int ic_init_buf(ic_driver *pdev);
static int ic_init_hw(ic_driver *pdev, ether_addr_t *addr);
static int ic_reset_hw(ic_driver *pdev);
static void ic_power_init(ic_driver *pdev);
static void ic_init_mii(ic_driver *pdev);
static void ic_init_mode(ic_driver *pdev);
static void ic_check_link(ic_driver *pdev);
static void ic_conf_addr(ic_driver *pdev, ether_addr_t *addr);
static void ic_handler(ic_driver *pdev);
static void ic_check_ints(ic_driver *pdev);

static const struct netdriver ic_table = {
	.ndr_init = ic_init,
	.ndr_stop = ic_stop,
	.ndr_mode = ic_mode,
	.ndr_recv = ic_recv,
	.ndr_send = ic_send,
	.ndr_stat = ic_stat,
	.ndr_intr = ic_intr
};

int main(int argc, char *argv[]) {
	env_setargs(argc, argv);
	netdriver_task(&ic_table);
}

/* Initialize the driver */
static int ic_init(unsigned int instance, ether_addr_t *addr) {
	int ret = 0;

	/* Intialize driver data structure */
	memset(&g_driver, 0, sizeof(g_driver));
	g_driver.ic_link = IC_LINK_UNKNOWN;
	strcpy(g_driver.ic_name, "ip1000#0");
	g_driver.ic_name[7] += instance;
	g_instance = instance;

	/* Probe the device */
	if (ic_probe(&g_driver, instance)) {
		printf("IP1000: Device is not found\n");
		ret = -ENODEV;
		goto err_probe;
	}

	/* Allocate and initialize buffer */
	if (ic_init_buf(&g_driver)) {
		printf("IP1000: Fail to initialize buffer\n");
		ret = -ENODEV;
		goto err_init_buf;
	}

	/* Intialize hardware */
	if (ic_init_hw(&g_driver, addr)) {
		printf("IP1000: Find to initialize hardware\n");
		ret = -EIO;
		goto err_init_hw;
	}

	/* Use a synchronous alarm instead of a watchdog timer */
	sys_setalarm(sys_hz(), 0);

	/* Clear send and recv flag */
	g_driver.ic_send_flag = FALSE;
	g_driver.ic_recv_flag = FALSE;

	return 0;

err_init_hw:
	free_contig(g_driver.ic_buf, g_driver.ic_buf_size);
err_init_buf:
err_probe:
	return ret;
}

/* Match the device and get base address */
static int ic_probe(ic_driver *pdev, int instance) {
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
		printf("IP1000: Base address is not properly configured\n");
		return -EIO;
	}
	pdev->ic_base_addr = bar;
	
	/* Get irq number */
	irq = pci_attr_r8(devind, PCI_ILR);
	pdev->ic_irq = irq;

	/* Get revision ID */
	rev = pci_attr_r8(devind, PCI_REV);
	pdev->ic_revision = rev;

#ifdef IP1000_DEBUG
	printf("IP1000: Base address is 0x%08x\n", pdev->ic_base_addr);
	printf("IP1000: IRQ number is 0x%08x\n", pdev->ic_irq);
	printf("IP1000: Revision ID is 0x%08x\n", pdev->ic_revision);
#endif

	return 0;
}

/* Allocate and initialize buffer */
static int ic_init_buf(ic_driver *pdev) {
	size_t rx_desc_size, tx_desc_size, rx_buf_size, tx_buf_size, tot_buf_size;
	ic_desc *desc;
	phys_bytes buf_dma, next;
	char *buf;
	int i;
	u64_t frag_size;

	/* Build Rx and Tx descriptor buffer */
	rx_desc_size = IC_RX_DESC_NUM * sizeof(ic_desc);
	tx_desc_size = IC_TX_DESC_NUM * sizeof(ic_desc);

	/* Allocate Rx and Tx buffer */
	tx_buf_size = IC_TX_BUF_SIZE;
	if (tx_buf_size % 4)
		tx_buf_size += 4 - (tx_buf_size % 4);
	rx_buf_size = IC_RX_BUF_SIZE;
	if (rx_buf_size % 4)
		rx_buf_size += 4 - (rx_buf_size % 4);
	tot_buf_size = rx_desc_size + tx_desc_size;
	tot_buf_size += IC_TX_DESC_NUM * tx_buf_size + IC_RX_DESC_NUM * rx_buf_size;
	if (tot_buf_size % 4096)
		tot_buf_size += 4096 - (tot_buf_size % 4096);

	if (!(buf = alloc_contig(tot_buf_size, 0, &buf_dma))) {
		printf("IP1000: Fail to allocate memory!\n");
		return -ENOMEM;
	}
	pdev->ic_buf_size = tot_buf_size;
	pdev->ic_buf = buf;

	/* Rx descriptor */
	pdev->ic_rx_desc = (ic_desc *)buf;
	pdev->ic_rx_desc_dma = buf_dma;
	memset(buf, 0, rx_desc_size);
	buf += rx_desc_size;
	buf_dma += rx_desc_size;

	/* Tx descriptor */
	pdev->ic_tx_desc = (ic_desc *)buf;
	pdev->ic_tx_desc_dma = buf_dma;
	memset(buf, 0, tx_desc_size);
	buf += tx_desc_size;
	buf_dma += tx_desc_size;

	/* Rx buffer assignment */
	desc = pdev->ic_rx_desc;
	next = pdev->ic_rx_desc_dma;
	for (i = 0; i < IC_RX_DESC_NUM; i++) {
		/* Set Rx buffer */
		pdev->ic_rx[i].buf_dma = buf_dma;
		pdev->ic_rx[i].buf = buf;
		buf_dma += rx_buf_size;
		buf += rx_buf_size;

		/* Set Rx descriptor */
		frag_size = (u64_t)rx_buf_size;
		desc->status = 0x0000000000000000ULL;
		desc->frag_info = (u64_t)(pdev->ic_rx[i].buf_dma);
		desc->frag_info |= (u64_t)((frag_size << 48) & IC_RFI_FRAG_LEN);
		if (i == (IC_RX_DESC_NUM - 1))
			desc->next_desc = (u64_t)(pdev->ic_rx_desc_dma);
		else {
			next += sizeof(ic_desc);
			desc->next_desc = (u64_t)next;
			desc++;
		}
	}

	/* Tx buffer assignment */
	desc = pdev->ic_tx_desc;
	next = pdev->ic_tx_desc_dma;
	for (i = 0; i < IC_TX_DESC_NUM; i++) {
		/* Set Tx buffer */
		pdev->ic_tx[i].busy = 0;
		pdev->ic_tx[i].buf_dma = buf_dma;
		pdev->ic_tx[i].buf = buf;
		buf_dma += tx_buf_size;
		buf += tx_buf_size;

		/* Set Rx descriptor */
		desc->status = IC_TFS_TFD_DONE;
		desc->frag_info = (u64_t)(pdev->ic_tx[i].buf_dma);
		if (i == (IC_TX_DESC_NUM - 1))
			desc->next_desc = (u64_t)(pdev->ic_tx_desc_dma);
		else {
			next += sizeof(ic_desc);
			desc->next_desc = (u64_t)next;
			desc++;
		}
	}
	pdev->ic_tx_busy_num = 0;
	pdev->ic_tx_head = 0;
	pdev->ic_tx_tail = 0;
	pdev->ic_rx_head = 0;
	pdev->ic_tx_alive = FALSE;

	return 0;
}

/* Intialize hardware */
static int ic_init_hw(ic_driver *pdev, ether_addr_t *addr) {
	int r, ret;

	/* Set the interrupt handler */
	pdev->ic_hook = pdev->ic_irq;
	if ((r = sys_irqsetpolicy(pdev->ic_irq, 0, &pdev->ic_hook)) != OK) {
		printf("IP1000: Fail to set IRQ policy: %d\n", r);
		ret = -EFAULT;
		goto err_irq_policy;
	}

	/* Reset hardware */
	if (ic_reset_hw(pdev)) {
		printf("IP1000: Fail to reset the hardware\n");
		ret = -EIO;
		goto err_reset;
	}

	/* Enable IRQ */
	if ((r = sys_irqenable(&pdev->ic_hook)) != OK) {
		printf("IP1000: Fail to enable IRQ: %d\n", r);
		ret = -EFAULT;
		goto err_irq_enable;
	}

	/* Configure MAC address */
	ic_conf_addr(pdev, addr);

	return 0;

err_irq_enable:
err_reset:
err_irq_policy:
	return ret;
}

/* Reset hardware */
static int ic_reset_hw(ic_driver *pdev) {
	u16_t base = pdev->ic_base_addr;
	u32_t dl, mac_ctrl;

	/* Reset the chip */
	dl = ic_inl(base, IC_REG_ASIC_CTRL);
	dl |= IC_AC_RESET_ALL;
	ic_outl(base, IC_REG_ASIC_CTRL, dl);
	usleep(10000);
	dl = ic_inl(base, IC_REG_ASIC_CTRL);
	if (dl & IC_AC_RESET_BUSY) {
#ifdef IP1000_DEBUG
		printf("IP1000: Fail to completely reset\n");
#endif
		return -EIO;
	}
#ifdef IP1000_DEBUG
	printf("IP1000: Reset successfully\n");
#endif

	/* Let power registers into sane state */
	ic_power_init(pdev);

	/* Initialize MII registers */
	ic_init_mii(pdev);

	/* Initialize hardware mode */
	ic_init_mode(pdev);

	/* Set Rx/Tx descriptor base address */
	ic_outl(base, IC_REG_RX_DESC0, pdev->ic_rx_desc_dma);
	ic_outl(base, IC_REG_RX_DESC1, 0x00000000);
	ic_outl(base, IC_REG_TX_DESC0, pdev->ic_tx_desc_dma);
	ic_outl(base, IC_REG_TX_DESC1, 0x00000000);

#ifdef IP1000_DEBUG
	dl = ic_inl(base, IC_REG_RX_DESC0);
	printf("IP1000: Rx descriptor DMA address is: 0x%08x\n", dl);
	dl = ic_inl(base, IC_REG_TX_DESC0);
	printf("IP1000: Tx descriptor DMA address is: 0x%08x\n", dl);
#endif

	/* Enable interrupts */
	ic_outw(base, IC_REG_INTR_ENA, IC_IR_COMMON);

	/* Start Rx and Tx */
	mac_ctrl = ic_inl(base, IC_REG_MAC_CTRL);
	mac_ctrl |= (IC_MC_RX_ENABLE | IC_MC_TX_ENABLE);
	ic_outl(base, IC_REG_MAC_CTRL, mac_ctrl);

	return 0;
}

/* Initialize power registers */
static void ic_power_init(ic_driver *pdev) {
	u8_t physet;
	u16_t base = pdev->ic_base_addr;
	u32_t mode;

	/* Read LED mode from EEPROM and set LED mode*/
	pdev->ic_led_mode = read_eeprom(pdev, 6);
	mode = ic_inl(base, IC_REG_ASIC_CTRL);
	mode &= ~(IC_AC_LED_MODE_BIT1 | IC_AC_LED_MODE | IC_AC_LED_SPEED);
	if ((pdev->ic_led_mode & 0x03) > 1)
		mode |= IC_AC_LED_MODE_BIT1;
	if ((pdev->ic_led_mode & 0x01) == 1)
		mode |= IC_AC_LED_MODE;
	if ((pdev->ic_led_mode & 0x08) == 8)
		mode |= IC_AC_LED_SPEED;
	ic_outl(base, IC_REG_ASIC_CTRL, mode);

	/* Set physet register*/
	physet = ic_inb(base, IC_REG_PHY_SET);
	physet &= ~(0x07);
	physet |= ((pdev->ic_led_mode & 0x70) >> 4);
	ic_outb(base, IC_REG_PHY_SET, physet);
}

/* Initialize MII registers */
static void ic_init_mii(ic_driver *pdev) {
	u8_t revision;
	u16_t base = pdev->ic_base_addr;
	u32_t status;
	u16_t length, address, value, mii_phyctrl, mii_1000cr; 
	const u16_t *phy_param;
	int i, phyaddr;

	/* Read MII physical address */
	for (i = 0; i < 32; i++) {
		phyaddr = (0x01 + i) % 32;
		status = mdio_read(pdev, phyaddr, 0x01);
		if ((status != 0xffff) && (status != 0))
			break;
	}
	if (i == 32) {
		printf("IP1000: Fail to read MII phyical address\n");
		return;
	}
#ifdef IP1000_DEBUG
	printf("IP1000: MII Physical address is %d\n", phyaddr);
#endif

	mii_1000cr = mdio_read(pdev, phyaddr, 0x09);
	mii_1000cr |= 0x0700;
	mdio_write(pdev, phyaddr, 0x09, mii_1000cr);
	mii_phyctrl = mdio_read(pdev, phyaddr, 0x00);

	phy_param = &DefaultPhyParam[0];
	length = *phy_param & 0x00ff;
	revision = (u8_t)((*phy_param) >> 8);
	phy_param++;
	while (length != 0) {
		if (pdev->ic_revision == revision) {
			while (length > 1) {
				address = *phy_param;
				value = *(phy_param + 1);
				phy_param += 2;
				mdio_write(pdev, phyaddr, address, value);
				length -= 4;
			}
			break;
		}
		else {
			phy_param += length / 2;
			length = *phy_param & 0x00ff;
			revision = (u8_t)((*phy_param) >> 8);
			phy_param++;
		}
	}

	mii_phyctrl |= 0x8200;
	mdio_write(pdev, phyaddr, 0x00, mii_phyctrl);
}

/* Intialize hardware mode */
static void ic_init_mode(ic_driver *pdev) {
	u16_t base = pdev->ic_base_addr;
	u32_t mac_ctrl, mac_ctrl1;

	/* Disable interrupt first */
	ic_outw(base, IC_REG_INTR_ENA, 0x0000);

	/* Configure MAC register */
	mac_ctrl = ic_inl(base, IC_REG_MAC_CTRL);
	mac_ctrl1 = mac_ctrl | IC_MC_STAT_DISABLE;
	if (mac_ctrl & IC_MC_TX_ENABLED)
		mac_ctrl1 |= IC_MC_TX_ENABLE;
	if (mac_ctrl & IC_MC_RX_ENABLED)
		mac_ctrl1 |= IC_MC_RX_ENABLE;
	ic_outl(base, IC_REG_MAC_CTRL, (mac_ctrl & (IC_MC_RX_DISABLE | 
									IC_MC_TX_DISABLE)));
	ic_outl(base, IC_REG_MAC_CTRL, 0x00000000);

	/* Set ic Rx mode */
	ic_mode(0x00);

	/* Set RX max frame size */
	ic_outw(base, IC_REG_MAX_FRAME, IC_RX_BUF_SIZE);

	/* Set some values */
	ic_outb(base, IC_REG_RX_DMA_PERIOD, 0x01);
	ic_outb(base, IC_REG_RX_DMA_UTH, 0x30);
	ic_outb(base, IC_REG_RX_DMA_BTH, 0x30);
	ic_outb(base, IC_REG_TX_DMA_PERIOD, 0x26);
	ic_outb(base, IC_REG_TX_DMA_UTH, 0x04);
	ic_outb(base, IC_REG_TX_DMA_BTH, 0x30);
	ic_outw(base, IC_REG_FLOW_ON_TH, 0x0740);
	ic_outw(base, IC_REG_FLOW_OFF_TH, 0x00bf);

	ic_outw(base, IC_REG_DEBUG_CTRL, ic_inw(base, IC_REG_DEBUG_CTRL) | 0x0200);
	ic_outw(base, IC_REG_DEBUG_CTRL, ic_inw(base, IC_REG_DEBUG_CTRL) | 0x0010);
	ic_outw(base, IC_REG_DEBUG_CTRL, ic_inw(base, IC_REG_DEBUG_CTRL) | 0x0020);

	ic_outl(base, 0x98, 0x0fffffff);

	/* Restore MAC register */
	ic_outl(base, IC_REG_MAC_CTRL, mac_ctrl1);

	/* Check link state */
	ic_check_link(pdev);
}

/* Check link state */
static void ic_check_link(ic_driver *pdev) {
	u8_t phy_ctrl;
	u16_t base = pdev->ic_base_addr;
	u32_t mac_ctrl;
	char speed[20], duplex[20];

	phy_ctrl = ic_inb(base, IC_REG_PHY_CTRL);
	mac_ctrl = ic_inl(base, IC_REG_MAC_CTRL);

	/* Check link speed */
	switch (phy_ctrl & IC_PC_LINK_SPEED) {
		case IC_PC_LINK_SPEED10:
			strcpy(speed, "10Mbps");
			pdev->ic_link = IC_LINK_UP;
			break;
		case IC_PC_LINK_SPEED100:
			strcpy(speed, "100Mbps");
			pdev->ic_link = IC_LINK_UP;
			break;
		case IC_PC_LINK_SPEED1000:
			strcpy(speed, "1000Mbps");
			pdev->ic_link = IC_LINK_UP;
			break;
		default:
			strcpy(speed, "unknown");
			pdev->ic_link = IC_LINK_DOWN;
			break;
	}

	/* Check link duplex */
	if (phy_ctrl & IC_PC_DUPLEX_STS) {
		strcpy(duplex, "full");
		mac_ctrl |= (IC_MC_DUPLEX_SEL | IC_MC_TX_FC_ENA | IC_MC_RX_FC_ENA);
	}
	else
		strcpy(duplex, "half");

#ifdef IP1000_DEBUG
	printf("IP1000: Link speed is %s, %s duplex\n", speed, duplex);
#endif

	ic_outl(base, IC_REG_MAC_CTRL, mac_ctrl);
}

/* Configure MAC address */
static void ic_conf_addr(ic_driver *pdev, ether_addr_t *addr) {
	u16_t dw, base = pdev->ic_base_addr;
	int i;

	/* Read station address from EEPROM */
	for (i = 0; i< 3; i++)
		pdev->sta_addr[i] = read_eeprom(pdev, 16 + i);
	for (i = 0; i < 3; i++)
		ic_outw(base, IC_REG_STA_ADDR0 + i * 2, pdev->sta_addr[i]);

	/* Read MAC address */
	addr->ea_addr[0] = (u8_t)(ic_inw(base, IC_REG_STA_ADDR0) & 0x00ff);
	addr->ea_addr[1] = (u8_t)((ic_inw(base, IC_REG_STA_ADDR0) & 0xff00) >> 8);
	addr->ea_addr[2] = (u8_t)(ic_inw(base, IC_REG_STA_ADDR1) & 0x00ff);
	addr->ea_addr[3] = (u8_t)((ic_inw(base, IC_REG_STA_ADDR1) & 0xff00) >> 8);
	addr->ea_addr[4] = (u8_t)(ic_inw(base, IC_REG_STA_ADDR2) & 0x00ff);
	addr->ea_addr[5] = (u8_t)((ic_inw(base, IC_REG_STA_ADDR2) & 0xff00) >> 8);

#ifdef IP1000_DEBUG
	printf("IP1000: Ethernet address is %02x:%02x:%02x:%02x:%02x:%02x\n", 
		addr->ea_addr[0], addr->ea_addr[1], addr->ea_addr[2], 
		addr->ea_addr[3], addr->ea_addr[4], addr->ea_addr[5]);
#endif
}

/* Stop the driver */
static void ic_stop(void) {	
	u16_t base = g_driver.ic_base_addr;

	/* Free Rx and Tx buffer*/
	free_contig(g_driver.ic_buf, g_driver.ic_buf_size);

	/* Stop IRQ, Rx and Tx */
	ic_outw(base, IC_REG_INTR_ENA, 0x0000);
	ic_outl(base, IC_REG_ASIC_CTRL, IC_AC_RESET_ALL);
}

/* Set driver mode */
static void ic_mode(unsigned int mode) {
	ic_driver *pdev = &g_driver;
	u16_t base = pdev->ic_base_addr;
	u8_t rm;

	pdev->ic_mode = mode;
	ic_outl(base, IC_REG_HASH_TLB0, 0x00000000);
	ic_outl(base, IC_REG_HASH_TLB1, 0x00000000);

	rm = ic_inb(base, IC_REG_RX_MODE);
	rm &= ~(IC_RM_UNICAST | IC_RM_MULTICAST | IC_RM_BROADCAST | 
					IC_RM_ALLFRAMES);
	if (pdev->ic_mode & NDEV_PROMISC)
		rm |= IC_RM_ALLFRAMES;
	if (pdev->ic_mode & NDEV_BROAD)
		rm |= IC_RM_BROADCAST;
	if (pdev->ic_mode & NDEV_MULTI)
		rm |= IC_RM_MULTICAST;
	rm |= (IC_RM_UNICAST | IC_RM_BROADCAST);
	ic_outb(base, IC_REG_RX_MODE, rm);
}

/* Receive data */
static ssize_t ic_recv(struct netdriver_data *data, size_t max) {
	ic_driver *pdev = &g_driver;
	u32_t totlen, packlen;
	u64_t rxstat;
	ic_desc *desc;
	int index, i;

	index = pdev->ic_rx_head;
	desc = pdev->ic_rx_desc;
	desc += index;

	/* Manage Rx buffer */
	rxstat = desc->status;

	if ((rxstat & IC_RFS_NORMAL) != IC_RFS_NORMAL)
		return SUSPEND;

	if (rxstat & IC_RFS_ALL_ERR) {
#ifdef IP1000_DEBUG
		printf("IP1000: Rx error: 0x%016llx\n", rxstat);
#endif
		if (rxstat & IC_RFS_OVERRUN) {
#ifdef IP1000_DEBUG
			printf("IP1000: Rx buffer overflow\n");
#endif
			pdev->ic_stat.ets_fifoOver++;
		}
		if (rxstat & IC_RFS_ALIGN_ERR) {
#ifdef IP1000_DEBUG
			printf("IP1000: Rx frames not align\n");
#endif
			pdev->ic_stat.ets_frameAll++;
		}
	}

	/* Get data length */
	totlen = (u32_t)(rxstat & IC_RFS_FRAME_LEN);
	if (totlen < 8 || totlen > 2 * ETH_MAX_PACK_SIZE) {
		printf("IP1000: Bad data length: %d\n", totlen);
		panic(NULL);
	}

	/* Do not need to substract CRC to get packet */
	packlen = totlen;

	/* Copy data to user */
	netdriver_copyout(data, 0, pdev->ic_rx[index].buf, packlen);
	pdev->ic_stat.ets_packetR++;

	/* Set Rx descriptor status */
	desc->status = 0x0000000000000000ULL;
	if (index == IC_RX_DESC_NUM - 1)
		index = 0;
	else
		index++;

	pdev->ic_rx_head = index;

#ifdef IP1000_DEBUG
	printf("IP1000: Successfully receive a packet, length = %d\n", packlen);
#endif

	return packlen;
}

/* Transmit data */
static int ic_send(struct netdriver_data *data, size_t size) {
	ic_driver *pdev = &g_driver;
	ic_desc *desc;
	int tx_head, i;
	u16_t base = pdev->ic_base_addr;

	tx_head = pdev->ic_tx_head;
	desc = pdev->ic_tx_desc;
	desc += tx_head;

	if (pdev->ic_tx[tx_head].busy)
		return SUSPEND;

	/* Copy data from user */
	netdriver_copyin(data, 0, pdev->ic_tx[tx_head].buf, size);

	/* Set busy */
	pdev->ic_tx[tx_head].busy = TRUE;
	pdev->ic_tx_busy_num++;

	/* Set Tx descriptor status */
	desc->status = IC_TFS_TFD_DONE;
	desc->status |= (u64_t)(IC_TFS_WORD_ALIGN | (IC_TFS_FRAMEID & tx_head) | 
							(IC_TFS_FRAG_COUNT & (1 << 24)));
	desc->status |= IC_TFS_TX_DMA_INDICATE;
	desc->frag_info |= IC_TFI_FRAG_LEN & 
					((u64_t)((size >= 60 ? size : 60) & 0xffff) << 48);
	desc->status &= (u64_t)(~(IC_TFS_TFD_DONE));

	if (tx_head == IC_TX_DESC_NUM - 1)
		tx_head = 0;
	else
		tx_head++;
	pdev->ic_tx_head = tx_head;

	/* Wake up transmit channel */
	ic_outl(base, IC_REG_DMA_CTRL, IC_DC_TX_POLL);

	return 0;
}

/* Handle Interrupt */
static void ic_intr(unsigned int mask) {
	int s;

	/* Run interrupt handler at driver level */
	ic_handler(&g_driver);

	/* Reenable interrupts for this hook */
	if ((s = sys_irqenable(&g_driver.ic_hook)) != OK)
		printf("IP1000: Cannot enable interrupts: %d\n", s);

	/* Perform tasks based on the flagged conditions */
	ic_check_ints(&g_driver);
}

/* Real handler interrupt */
static void ic_handler(ic_driver *pdev) {
	u16_t intr_status, base = pdev->ic_base_addr;	
	int flag = 0, tx_head, tx_tail;
	ic_desc *desc;

	/* Get interrupt status */
	intr_status = ic_inw(base, IC_REG_INTR_STS_ACK);

	/* Clear interrupt */
	ic_outw(base, IC_REG_INTR_ENA, IC_IR_COMMON);

	/* Check link status */
	if (intr_status & IC_IR_LINK_EVENT) {
#ifdef IP1000_DEBUG
		printf("IP1000: Link state change!\n");
#endif
		flag++;
		ic_check_link(pdev);
	}

	/* Check interrupt error */
	if (intr_status & IC_IR_HOST_ERR)
		printf("IP1000: Host error in interrupt: 0x%04x\n", intr_status);
	if (intr_status & IC_IR_RFD_END)
		printf("IP1000: Rx list end in interrupt: 0x%04x\n", intr_status);
	if (intr_status & IC_IR_MAC_CTRL)
		printf("IP1000: MAC control frame in interrupt: 0x%04x\n", intr_status);
	if (intr_status & IC_IR_RX_EARLY)
		printf("IP1000: Rx early in interrupt: 0x%04x\n", intr_status);
	if (intr_status & IC_IR_UPDATE_STS)
		printf("IP1000: Status update in interrupt: 0x%04x\n", intr_status);

	/* Check interrupt status */
	if (intr_status & IC_IR_RX_DMA_DONE) {
		pdev->ic_recv_flag = TRUE;	
		flag++;
	}
	if (intr_status & IC_IR_TX_DMA_DONE) {
		pdev->ic_send_flag = TRUE;
		flag++;

		/* Manage Tx Buffer */
		tx_head = pdev->ic_tx_head;
		tx_tail = pdev->ic_tx_tail;
		while (tx_tail != tx_head) {
			desc = pdev->ic_tx_desc;
			desc += tx_tail;
			if (!pdev->ic_tx[tx_tail].busy)
				printf("IP1000: Strange, buffer not busy?\n");

			if (!(desc->status & IC_TFS_TFD_DONE))
				break;

			pdev->ic_stat.ets_packetT++;
			pdev->ic_tx[tx_tail].busy = FALSE;
			pdev->ic_tx_busy_num--;
			
			if (++tx_tail >= IC_TX_DESC_NUM)
				tx_tail = 0;

			pdev->ic_send_flag = TRUE;
			pdev->ic_recv_flag = TRUE;
			pdev->ic_tx_alive = TRUE;

#ifdef IP1000_DEBUG
			printf("IP1000: Successfully send a packet\n");
#endif
		}
		pdev->ic_tx_tail = tx_tail;
	}
	if (!flag) {
#ifdef IP1000_DEBUG
		printf("IP1000: Unknown error in interrupt: 0x%04x\n", intr_status);
#endif
		return;
	}
}

/* Check interrupt and perform */
static void ic_check_ints(ic_driver *pdev) {
	if (!pdev->ic_recv_flag)
		return;
	pdev->ic_recv_flag = FALSE;

	/* Handle data receive */
	netdriver_recv();

	/* Handle data transmit */
	if (pdev->ic_send_flag) {
		pdev->ic_send_flag = FALSE;
		netdriver_send();
	}
}

static void ic_stat(eth_stat_t *stat) {
	memcpy(stat, &g_driver.ic_stat, sizeof(*stat));	
}
