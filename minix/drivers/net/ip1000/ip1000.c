#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <machine/pci.h>
#include "ip1000.h"
#include "io.h"

/* global value */
static ic_driver g_driver;
static int g_instance;

/* driver interface */
static int ic_init(unsigned int instance, ether_addr_t *addr);
static void ic_stop(void);
static void ic_mode(unsigned int mode);
static ssize_t ic_recv(struct netdriver_data *data, size_t max);
static int ic_send(struct netdriver_data *data, size_t size);
static void ic_intr(unsigned int mask);
static void ic_stat(eth_stat_t *stat);

/* internal function */
static int ic_probe(ic_driver *pdev, int instance);
static int ic_init_buf(ic_driver *pdev);
static int ic_init_hw(ic_driver *pdev, ether_addr_t *addr);
static int ic_reset_hw(ic_driver *pdev);
static void ic_conf_addr(ic_driver *pdev, ether_addr_t *addr);
static void ic_handler(ic_driver *pdev);
static void ic_check_ints(ic_driver *pdev);

/* developer interface */
static void ic_init_rx_desc(ic_desc *desc, size_t size, phys_bytes dma);
static void ic_init_tx_desc(ic_desc *desc, size_t size, phys_bytes dma);
static int ic_real_reset(u32_t base);
static int ic_init_power(u32_t base);
static int ic_init_mii(u32_t base);
static int ic_init_io(u32_t base);
static void ic_start_rx_tx(u32_t base);
static void ic_get_addr(u32_t base, u8_t *pa);
static int ic_check_link(u32_t base);
static void ic_stop_rx_tx(u32_t base);
static int ic_rx_status_ok(ic_desc *desc);
static int ic_get_rx_len(ic_desc *desc);
static void ic_tx_desc_start(ic_desc *desc, size_t size);
static void ic_wakeup_tx(u32_t base);
static int ic_tx_status_ok(ic_desc *desc);

/* ======= Developer-defined function ======= */
/* ====== Self-defined function ======*/

static u16_t read_eeprom(u32_t base, int addr) {
	u16_t ret, data, val;
	int i;

	val = EC_READ | (addr & 0xff);
	ic_out16(base, REG_EEPROM_CTRL, val);
	for (i = 0; i < 100; i++) {
		micro_delay(10000);
		data = ic_in16(base, REG_EEPROM_CTRL);
		if (!(data & EC_BUSY)) {
			ret = ic_in16(base, REG_EEPROM_DATA);
			break;
		}
	}
	if (i == 100)
		printf("IP1000: Fail to read EEPROM\n");
	return ret;
}

static u16_t read_phy_reg(u32_t base, int phy_addr, int phy_reg) {
	int i, j, fieldlen[8];
	u32_t field[8];
	u8_t data, polar;

	field[0] = 0xffffffff;		fieldlen[0] = 32;
	field[1] = 0x0001;		fieldlen[1] = 2;
	field[2] = 0x0002;		fieldlen[2] = 2;
	field[3] = phy_addr;	fieldlen[3] = 5;
	field[4] = phy_reg;		fieldlen[4] = 5;
	field[5] = 0x0000;		fieldlen[5] = 2;
	field[6] = 0x0000;		fieldlen[6] = 16;
	field[7] = 0x0000;		fieldlen[7] = 1;

	polar = ic_in8(base, REG_PHY_CTRL) & 0x28;
	for (i = 0; i < 5; i++) {
		for (j = 0; j < fieldlen[i]; j++) {
			data = (field[i] >> (fieldlen[i] - j - 1)) << 1;
			data = (0x02 & data) | (0x04 | polar);
			ic_out8(base, REG_PHY_CTRL, data);
			micro_delay(10);
			ic_out8(base, REG_PHY_CTRL, (data | 0x01));
			micro_delay(10);
		}
	}
	ic_out8(base, REG_PHY_CTRL, (polar | 0x04));
	micro_delay(10);
	ic_out8(base, REG_PHY_CTRL, (polar | 0x05));
	micro_delay(10);
	ic_out8(base, REG_PHY_CTRL, polar);
	micro_delay(10);
	data = ic_in8(base, REG_PHY_CTRL);
	ic_out8(base, REG_PHY_CTRL, (polar | 0x01));
	micro_delay(10);
	for (i = 0; i < fieldlen[6]; i++) {
		ic_out8(base, REG_PHY_CTRL, polar);
		micro_delay(10);
		data = ((ic_in8(base, REG_PHY_CTRL) & 0x02) >> 1) & 0x01;
		ic_out8(base, REG_PHY_CTRL, (polar | 0x01));
		micro_delay(10);
		field[6] |= (data << (fieldlen[6] - i - 1));
	}

	for (i = 0; i < 3; i++) {
		ic_out8(base, REG_PHY_CTRL, (polar | 0x04));
		micro_delay(10);
		ic_out8(base, REG_PHY_CTRL, (polar | 0x05));
		micro_delay(10);
	}
	ic_out8(base, REG_PHY_CTRL, (polar | 0x04));
	return field[6];
}

static void write_phy_reg(u32_t base, int phy_addr, int phy_reg, u16_t val) {
	int i, j, fieldlen[8];
	u32_t field[8];
	u8_t data, polar;

	field[0] = 0xffffffff;		fieldlen[0] = 32;
	field[1] = 0x0001;		fieldlen[1] = 2;
	field[2] = 0x0001;		fieldlen[2] = 2;
	field[3] = phy_addr;		fieldlen[3] = 5;
	field[4] = phy_reg;		fieldlen[4] = 5;
	field[5] = 0x0002;		fieldlen[5] = 2;
	field[6] = val;			fieldlen[6] = 16;
	field[7] = 0x0000;		fieldlen[7] = 1;

	polar = ic_in8(base, REG_PHY_CTRL) & 0x28;
	for (i = 0; i < 7; i++) {
		for (j = 0; j < fieldlen[i]; j++) {
			data = (field[i] >> (field[j] - j - 1)) << 1;
			data = (0x02 & data) | (0x04 | polar);
			ic_out8(base, REG_PHY_CTRL, data);
			micro_delay(10);
			ic_out8(base, REG_PHY_CTRL, (data | 0x01));
			micro_delay(10);
		}
	}
	for (i = 0; i < fieldlen[7]; i ++) {
		ic_out8(base, REG_PHY_CTRL, polar);
		micro_delay(10);
		field[7] |= ((ic_in8(base, REG_PHY_CTRL) & 0x02) >> 1)
						<< (fieldlen[7] - i -1);
		ic_out8(base, REG_PHY_CTRL, (data | 0x01));
		micro_delay(10);
	}
}

/* ====== Developer interface ======*/

/* Intialize Rx descriptor (### RX_DESC_INIT ###) */
static void ic_init_rx_desc(ic_desc *desc, size_t size, phys_bytes dma) {
	desc->status = 0x0000000000000000ULL;
	desc->frag_info = (u64_t)dma;
	desc->frag_info |= ((u64_t)size << 48) & RFI_FRAG_LEN;
}

/* Intialize Tx descriptor (### TX_DESC_INIT ###) */
static void ic_init_tx_desc(ic_desc *desc, size_t size, phys_bytes dma) {
	desc->status = TFS_TFD_DONE;
	desc->frag_info = (u64_t)dma;
}

/* Real hardware reset (### RESET_HARDWARE_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int ic_real_reset(u32_t base) {
	u32_t data;
	data = ic_in32(base, REG_ASIC_CTRL);
	data |= AC_RESET_ALL;
	ic_out32(base, REG_ASIC_CTRL, data);
	micro_delay(10000);
	if (ic_in32(base, REG_ASIC_CTRL) & AC_RESET_BUSY)
		return -EIO;
	return OK;
}

/* Intialize power (### POWER_INIT_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int ic_init_power(u32_t base) {
	u8_t physet;
	u32_t mode0, mode1;

	mode0 = read_eeprom(base, 6);
	mode1 = ic_in16(base, REG_ASIC_CTRL);
	mode1 &= ~(AC_LED_MODE_B1 | AC_LED_MODE | AC_LED_SPEED);
	if ((mode0 & 0x03) > 1)
		mode1 |= AC_LED_MODE_B1;
	if ((mode0 & 0x01) == 1)
		mode1 |= AC_LED_MODE;
	if ((mode0 & 0x08) == 8)
		mode1 |= AC_LED_SPEED;
	ic_out32(base, REG_ASIC_CTRL, mode1);

	physet = ic_in8(base, REG_PHY_SET);
	physet &= ~(0x07);
	physet |= (mode0 & 0x70) >> 4;
	ic_out8(base, REG_PHY_SET, physet);

	return OK;
}

/* Intialize MII interface (### MII_INIT_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int ic_init_mii(u32_t base) {
	int i, phyaddr;
	u8_t revision;
	u16_t phyctrl, cr1000, length, address, value;
	u16_t *param;
	u32_t status;

	for (i = 0; i < 32; i++) {
		phyaddr = (i + 0x01) % 32;
		status = read_phy_reg(base, phyaddr, 0x01);
		if ((status != 0xffff) && (status != 0))
			break;
	}
	if (i == 32)
		return -EIO;
	if (phyaddr != -1) {
		cr1000 = read_phy_reg(base, phyaddr, 0x09);
		cr1000 |= 0x0700;
		write_phy_reg(base, phyaddr, 0x09, cr1000);
		phyctrl = read_phy_reg(base, phyaddr, 0x00);
	}

	param = &PhyParam[0];
	length = (*param) & 0x00ff;
	revision = (u8_t)((*param) >> 8);
	param++;
	while (length != 0) {
		if (g_driver.revision == revision) {
			while (length > 1) {
				address = *param;
				value = *(param + 1);
				param += 2;
				write_phy_reg(base, phyaddr, address, value);
				length -= 4;
			}
			break;
		}
		else {
			param += length / 2;
			length = *param & 0x00ff;
			revision = (u8_t)((*param) >> 8);
			param++;
		}
	}

	phyctrl |= 0x8200;
	write_phy_reg(base, phyaddr, 0x00, phyctrl);

	return OK;
}

/* Intialize other hardware I/O registers (### INIT_HARDWARE_IO_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int ic_init_io(u32_t base) {
	u32_t mac_ctrl;

	mac_ctrl = ic_in32(base, REG_MAC_CTRL);
	mac_ctrl |= (MC_STAT_DISABLE | MC_TX_FC_ENA | MC_RX_FC_ENA);
	ic_out32(base, REG_MAC_CTRL, 0x00000000);
	ic_out16(base, REG_MAX_FRAME, RX_BUF_SIZE);
	ic_out8(base, REG_RX_DMA_PERIOD, 0x01);
	ic_out8(base, REG_RX_DMA_UTH, 0x30);
	ic_out8(base, REG_RX_DMA_BTH, 0x30);
	ic_out8(base, REG_TX_DMA_PERIOD, 0x26);
	ic_out8(base, REG_TX_DMA_UTH, 0x04);
	ic_out8(base, REG_TX_DMA_BTH, 0x30);
	ic_out16(base, REG_FLOW_ON_TH, 0x0740);
	ic_out16(base, REG_FLOW_OFF_TH, 0x00bf);
	ic_out32(base, REG_MAC_CTRL, mac_ctrl);
	return OK;
}

/* Start Rx/Tx (### START_RX_TX ###) */
static void ic_start_rx_tx(u32_t base) {
	u32_t mac_ctrl;

	mac_ctrl = ic_in32(base, REG_MAC_CTRL);
	mac_ctrl |= (MC_RX_ENABLE | MC_TX_ENABLE);
	ic_out32(base, REG_MAC_CTRL, mac_ctrl);
}

/* Get MAC address to the array 'pa' (### GET_MAC_ADDR ###) */
static void ic_get_addr(u32_t base, u8_t *pa) {
	int i, sta_addr[3];
	for (i = 0; i < 3; i++) {
		sta_addr[i] = read_eeprom(base, 16 + i);
		ic_out16(base, (REG_STA_ADDR0 + i * 2), sta_addr[i]);
	}
	pa[0] = (u8_t)(ic_in16(base, REG_STA_ADDR0) & 0x00ff);
	pa[1] = (u8_t)((ic_in16(base, REG_STA_ADDR0) & 0xff00) >> 8);
	pa[2] = (u8_t)(ic_in16(base, REG_STA_ADDR1) & 0x00ff);
	pa[3] = (u8_t)((ic_in16(base, REG_STA_ADDR1) & 0xff00) >> 8);
	pa[4] = (u8_t)(ic_in16(base, REG_STA_ADDR2) & 0x00ff);
	pa[5] = (u8_t)((ic_in16(base, REG_STA_ADDR2) & 0xff00) >> 8);
}

/* Check link status (### CHECK_LINK ###)
 * -- Return LINK_UP or LINK_DOWN */
static int ic_check_link(u32_t base) {
	u8_t phy_ctrl;
	u32_t mac_ctrl;
	int ret;
	char speed[20], duplex[20];

	phy_ctrl = ic_in8(base, REG_PHY_CTRL);
	mac_ctrl = ic_in8(base, REG_MAC_CTRL);
	switch (phy_ctrl & PC_LINK_SPEED) {
		case PC_LINK_SPEED10:
			strcpy(speed, "10Mbps");
			ret = LINK_UP;
			break;
		case PC_LINK_SPEED100:
			strcpy(speed, "100Mbps");
			ret = LINK_UP;
			break;
		case PC_LINK_SPEED1000:
			strcpy(speed, "1000Mbps");
			ret = LINK_UP;
			break;
		default:
			strcpy(speed, "unknown");
			ret = LINK_DOWN;
			break;
	}
	if (phy_ctrl & PC_DUPLEX_STS) {
		strcpy(duplex, "full");
		mac_ctrl |= (MC_DUPLEX_SEL | MC_TX_FC_ENA | MC_RX_FC_ENA);
	}
	else
		strcpy(duplex, "half");
	ic_out32(base, REG_MAC_CTRL, mac_ctrl);
#ifdef MY_DEBUG
	printf("ip1000: Link speed is %s, %s duplex\n", speed, duplex);
#endif
	return ret;
}

/* Stop Rx/Tx (### STOP_RX_TX ###) */
static void ic_stop_rx_tx(u32_t base) {
	ic_out32(base, REG_ASIC_CTRL, AC_RESET_ALL);
}

/* Check whether Rx status OK (### CHECK_RX_STATUS_OK ###)
 * -- Return TRUE or FALSE */
static int ic_rx_status_ok(ic_desc *desc) {
	if ((desc->status & RFS_NORMAL) == RFS_NORMAL)
		return TRUE;
	return FALSE;
}

/* Get Rx data length from descriptor (### GET_RX_LEN ###)
 * --- Return the length */
static int ic_get_rx_len(ic_desc *desc) {
	int totlen;
	totlen = (u32_t)(desc->status & RFS_FRAME_LEN);
	return totlen;
}

/* Set Tx descriptor in send (### TX_DESC_START ###) */
static void ic_tx_desc_start(ic_desc *desc, size_t size) {
	desc->status = TFS_TFD_DONE;
	desc->status |= (u64_t)(TFS_WORD_ALIGN | (TFS_FRAMEID & (g_driver.tx_head))
					| (TFS_FRAG_COUNT & (1 << 24)));
	desc->status |= TFS_TX_DMA_INDICATE;
	desc->frag_info |= TFI_FRAG_LEN & ((u64_t)((size >= 60 ? size : 60) &
						0xffff) << 48);
	desc->status &= (u64_t)(~(TFS_TFD_DONE));
}

/* Wake up Tx channel (### WAKE_UP_TX ###) */
static void ic_wakeup_tx(u32_t base) {
	ic_out32(base, REG_DMA_CTRL, 0x00001000);
}

/* Check whether Tx status OK (### CHECK_TX_STATUS_OK ###)
 * -- Return TRUE or FALSE */
static int ic_tx_status_ok(ic_desc *desc) {
	if (desc->status & TFS_TFD_DONE)
		return TRUE;
	return FALSE;
}

/* Driver interface table */
static const struct netdriver ic_table = {
	.ndr_init = ic_init,
	.ndr_stop = ic_stop,
	.ndr_mode = ic_mode,
	.ndr_recv = ic_recv,
	.ndr_send = ic_send,
	.ndr_stat = ic_stat,
	.ndr_intr = ic_intr,
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
	g_driver.link = LINK_UNKNOWN;
	strcpy(g_driver.name, "netdriver#0");
	g_driver.name[10] += instance;
	g_instance = instance;

	/* Probe the device */
	if (ic_probe(&g_driver, instance)) {
		printf("ip1000: Device is not found\n");
		ret = -ENODEV;
		goto err_probe;
	}

	/* Allocate and initialize buffer */
	if (ic_init_buf(&g_driver)) {
		printf("ip1000: Fail to initialize buffer\n");
		ret = -ENODEV;
		goto err_init_buf;
	}

	/* Intialize hardware */
	if (ic_init_hw(&g_driver, addr)) {
		printf("ip1000: Fail to initialize hardware\n");
		ret = -EIO;
		goto err_init_hw;
	}

	/* Use a synchronous alarm instead of a watchdog timer */
	sys_setalarm(sys_hz(), 0);

	/* Clear send and recv flag */
	g_driver.send_flag = FALSE;
	g_driver.recv_flag = FALSE;

	return 0;

err_init_hw:
	free_contig(g_driver.buf, g_driver.buf_size);
err_init_buf:
err_probe:
	return ret;
}

/* Match the device and get base address */
static int ic_probe(ic_driver *pdev, int instance) {
	int devind, ioflag;
	u16_t cr, vid, did;
	u32_t bar, size;
	u8_t irq, rev;
	u8_t *reg;

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
#ifdef DMA_REG_MODE
	if (pci_get_bar(devind, PCI_BAR, &base, &size, &ioflag)) {
		printf("ip1000: Fail to get PCI BAR\n");
		return -EIO;
	}
	if (ioflag) {
		printf("ip1000: PCI BAR is not for memory\n");
		return -EIO;
	}
	if ((reg = vm_map_phys(SELF, (void *)base, size)) == MAP_FAILED) {
		printf("ip1000: Fail to map hardware registers from PCI\n");
		return -EIO;
	}
	pdev->base_addr = (u32_t)reg;
#else
	bar = pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
	if (bar < 0x400) {
		printf("ip1000: Base address is not properly configured\n");
		return -EIO;
	}
	pdev->base_addr = bar;
#endif

	/* Get irq number */
	irq = pci_attr_r8(devind, PCI_ILR);
	pdev->irq = irq;

	/* Get revision ID */
	rev = pci_attr_r8(devind, PCI_REV);
	pdev->revision = rev;

#ifdef MY_DEBUG
	printf("ip1000: Base address is 0x%08x\n", pdev->base_addr);
	printf("ip1000: IRQ number is 0x%02x\n", pdev->irq);
	printf("ip1000: Revision ID is 0x%02x\n", pdev->revision);
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

	/* Build Rx and Tx descriptor buffer */
	rx_desc_size = RX_DESC_NUM * sizeof(ic_desc);
	tx_desc_size = TX_DESC_NUM * sizeof(ic_desc);

	/* Allocate Rx and Tx buffer */
	tx_buf_size = TX_BUF_SIZE;
	if (tx_buf_size % 4)
		tx_buf_size += 4 - (tx_buf_size % 4);
	rx_buf_size = RX_BUF_SIZE;
	tot_buf_size = rx_desc_size + tx_desc_size;
	tot_buf_size += TX_DESC_NUM * tx_buf_size + RX_DESC_NUM * rx_buf_size;
	if (tot_buf_size % 4096)
		tot_buf_size += 4096 - (tot_buf_size % 4096);

	if (!(buf = alloc_contig(tot_buf_size, 0, &buf_dma))) {
		printf("ip1000: Fail to allocate memory\n");
		return -ENOMEM;
	}
	pdev->buf_size = tot_buf_size;
	pdev->buf = buf;

	/* Rx descriptor */
	pdev->rx_desc = (ic_desc *)buf;
	pdev->rx_desc_dma = buf_dma;
	memset(buf, 0, rx_desc_size);
	buf += rx_desc_size;
	buf_dma += rx_desc_size;

	/* Tx descriptor */
	pdev->tx_desc = (ic_desc *)buf;
	pdev->tx_desc_dma = buf_dma;
	memset(buf, 0, tx_desc_size);
	buf += tx_desc_size;
	buf_dma += tx_desc_size;

	/* Rx buffer assignment */
	desc = pdev->rx_desc;
	next = pdev->rx_desc_dma;
	for (i = 0; i < RX_DESC_NUM; i++) {
		/* Set Rx buffer */
		pdev->rx[i].buf_dma = buf_dma;
		pdev->rx[i].buf = buf;
		buf_dma += rx_buf_size;
		buf += rx_buf_size;

		/* Set Rx descriptor */
		/* ### RX_DESC_INIT ### */
		ic_init_rx_desc(desc, rx_buf_size, pdev->rx[i].buf_dma);
		if (i == (RX_DESC_NUM - 1))
			desc->next = pdev->rx_desc_dma;
		else {
			next += sizeof(ic_desc);
			desc->next = next;
			desc++;
		}
	}

	/* Tx buffer assignment */
	desc = pdev->tx_desc;
	next = pdev->tx_desc_dma;
	for (i = 0; i < TX_DESC_NUM; i++) {
		/* Set Tx buffer */
		pdev->tx[i].busy = 0;
		pdev->tx[i].buf_dma = buf_dma;
		pdev->tx[i].buf = buf;
		buf_dma += tx_buf_size;
		buf += tx_buf_size;

		/* Set Rx descriptor */
		/* ### TX_DESC_INIT ### */
		ic_init_tx_desc(desc, tx_buf_size, pdev->tx[i].buf_dma);
		if (i == (TX_DESC_NUM - 1))
			desc->next = pdev->tx_desc_dma;
		else {
			next += sizeof(ic_desc);
			desc->next = next;
			desc++;
		}
	}
	pdev->tx_busy_num = 0;
	pdev->tx_head = 0;
	pdev->tx_tail = 0;
	pdev->rx_head = 0;

	return 0;
}

/* Intialize hardware */
static int ic_init_hw(ic_driver *pdev, ether_addr_t *addr) {
	int r, ret;

	/* Set the OS interrupt handler */
	pdev->hook = pdev->irq;
	if ((r = sys_irqsetpolicy(pdev->irq, 0, &pdev->hook)) != OK) {
		printf("ip1000: Fail to set OS IRQ policy: %d\n", r);
		ret = -EFAULT;
		goto err_irq_policy;
	}

	/* Reset hardware */
	if (ic_reset_hw(pdev)) {
		printf("ip1000: Fail to reset the device\n");
		ret = -EIO;
		goto err_reset_hw;
	}

	/* Enable OS IRQ */
	if ((r = sys_irqenable(&pdev->hook)) != OK) {
		printf("ip1000: Fail to enable OS IRQ: %d\n", r);
		ret = -EFAULT;
		goto err_irq_enable;
	}

	/* Configure MAC address */
	ic_conf_addr(pdev, addr);

	/* Detect link status */
	pdev->link = ic_check_link(pdev->base_addr);
#ifdef MY_DEBUG
	if (pdev->link)
		printf("ip1000: Link up\n");
	else
		printf("ip1000: Link down\n");
#endif

	return 0;

err_reset_hw:
err_irq_enable:
err_irq_policy:
	return ret;
}

/* Reset hardware */
static int ic_reset_hw(ic_driver *pdev) {
	u32_t base = pdev->base_addr;
	int ret;

	/* Reset the chip */
	/* ### RESET_HARDWARE_CAN_FAIL ### */
	if (ic_real_reset(base)) {
		printf("ip1000: Fail to reset the hardware\n");
		ret = -EIO;
		goto err_real_reset;
	}

	/* Initialize power */
	/* ### POWER_INIT_CAN_FAIL ### */
	if (ic_init_power(base)) {
		printf("ip1000: Fail to initialize power\n");
		ret = -EIO;
		goto err_init_power;
	}

	/* Initialize MII interface */
	/* ### MII_INIT_CAN_FAIL ### */
	if (ic_init_mii(base)) {
		printf("ip1000: Fail to initialize MII interface\n");
		ret = -EIO;
		goto err_init_mii;
	}

	/* Initialize hardware I/O registers */
	/* ### SET_RX_DESC_REG ### */
	if (ic_init_io(base)) {
		printf("ip1000: Fail to initialize I/O registers\n");
		ret = -EIO;
		goto err_init_io;
	}

	/* Set Rx/Tx descriptor into register */
	/* ### SET_RX_DESC_REG ### */
	ic_out32(base, REG_RX_DESC_BASEL, pdev->rx_desc_dma);
#ifdef DESC_BASE_DMA64
	ic_out32(base, REG_RX_DESC_BASEU, 0x00000000);
#endif
	/* ### SET_TX_DESC_REG ### */
	ic_out32(base, REG_TX_DESC_BASEL, pdev->tx_desc_dma);
#ifdef DESC_BASE_DMA64
	ic_out32(base, REG_TX_DESC_BASEU, 0x00000000);
#endif

	/* Enable interrupts */
	/* ### ENABLE_INTR ### */
	ic_out16(base, REG_IMR, INTR_IMR_ENABLE);

	/* Start the device, Rx and Tx */
	/* ### START_RX_TX ### */
	ic_start_rx_tx(base);

	return 0;

err_init_io:
err_init_mii:
err_init_power:
err_real_reset:
	return ret;
}

/* Configure MAC address */
static void ic_conf_addr(ic_driver *pdev, ether_addr_t *addr) {
	u8_t pa[6];
	u32_t base = pdev->base_addr;

	/* Get MAC address */
	/* ### GET_MAC_ADDR ### */
	ic_get_addr(base, pa);
	addr->ea_addr[0] = pa[0];
	addr->ea_addr[1] = pa[1];
	addr->ea_addr[2] = pa[2];
	addr->ea_addr[3] = pa[3];
	addr->ea_addr[4] = pa[4];
	addr->ea_addr[5] = pa[5];
#ifdef MY_DEBUG
	printf("ip1000: Ethernet address is %02x:%02x:%02x:%02x:%02x:%02x\n",
			addr->ea_addr[0], addr->ea_addr[1], addr->ea_addr[2],
			addr->ea_addr[3], addr->ea_addr[4], addr->ea_addr[5]);
#endif
}

/* Stop the driver */
static void ic_stop(void) {
	u32_t base = g_driver.base_addr;

	/* Free Rx and Tx buffer*/
	free_contig(g_driver.buf, g_driver.buf_size);

	/* Stop interrupt */
	/* ### DISABLE_INTR ### */
	ic_out16(base, REG_IMR, INTR_IMR_DISABLE);

	/* Stop Rx/Tx */
	/* ### STOP_RX_TX ### */
	ic_stop_rx_tx(base);
}

/* Set driver mode */
static void ic_mode(unsigned int mode) {
	ic_driver *pdev = &g_driver;
	u32_t base = pdev->base_addr;
	u8_t rcr;

	pdev->mode = mode;

	/* ### READ_RCR ### */
	rcr = ic_in8(base, REG_RCR);
	rcr &= ~(RCR_UNICAST | RCR_MULTICAST | RCR_BROADCAST);
	if (pdev->mode & NDEV_PROMISC)
		rcr |= RCR_UNICAST | RCR_MULTICAST;
	if (pdev->mode & NDEV_BROAD)
		rcr |= RCR_BROADCAST;
	if (pdev->mode & NDEV_MULTI)
		rcr |= RCR_MULTICAST;
	rcr |= RCR_UNICAST;
	/* ### WRITE_RCR ### */
	ic_out8(base, REG_RCR, rcr);
}

/* Receive data */
static ssize_t ic_recv(struct netdriver_data *data, size_t max) {
	ic_driver *pdev = &g_driver;
	u32_t totlen, packlen;
	ic_desc *desc;
	int index, i;

	index = pdev->rx_head;
	desc = pdev->rx_desc;
	desc += index;

	/* Check whether the receiving is OK */
	/* ### CHECK_RX_STATUS_OK ### */
	if (ic_rx_status_ok(desc) != TRUE)
		return SUSPEND;

	/* Check Rx status error */
	/* ### CHECK_RX_STATUS_ERROR ### */
	if (desc->status & DESC_STATUS_RX_RECV_ERR)
		printf("ip1000: Rx error\n");

	/* Get data length */
	/* ### Get Rx data length ### */
	totlen = ic_get_rx_len(desc);
	if (totlen < 8 || totlen > 2 * ETH_MAX_PACK_SIZE) {
		printf("ip1000: Bad data length: %d\n", totlen);
		panic(NULL);
	}

	packlen = totlen;
	if (packlen > max)
		packlen = max;

	/* Copy data to user */
	netdriver_copyout(data, 0, pdev->rx[index].buf, packlen);
	pdev->stat.ets_packetR++;

	/* Set Rx descriptor status */
	/* ### SET_RX_STATUS_INTR ### */
	desc->status = DESC_STATUS_RX_RECV_CLEAR;
	if (index == RX_DESC_NUM - 1)
		index = 0;
	else
		index++;
	pdev->rx_head = index;

#ifdef MY_DEBUG
	printf("ip1000: Successfully receive a packet, length = %d\n", packlen);
#endif

	return packlen;
}

/* Transmit data */
static int ic_send(struct netdriver_data *data, size_t size) {
	ic_driver *pdev = &g_driver;
	ic_desc *desc;
	int tx_head, i;
	u32_t base = pdev->base_addr;

	tx_head = pdev->tx_head;
	desc = pdev->tx_desc;
	desc += tx_head;

	if (pdev->tx[tx_head].busy)
		return SUSPEND;

	/* Copy data from user */
	netdriver_copyin(data, 0, pdev->tx[tx_head].buf, size);

	/* Set busy */
	pdev->tx[tx_head].busy = TRUE;
	pdev->tx_busy_num++;

	/* Set Tx descriptor status */
	/* ### TX_DESC_START ### */
	ic_tx_desc_start(desc, size);
	if (tx_head == TX_DESC_NUM - 1)
		tx_head = 0;
	else
		tx_head++;
	pdev->tx_head = tx_head;

	/* Wake up transmit channel */
	/* ### WAKE_UP_TX ### */
	ic_wakeup_tx(base);

	return 0;
}

/* Handle Interrupt */
static void ic_intr(unsigned int mask) {
	int s;

	/* Run interrupt handler at driver level */
	ic_handler(&g_driver);

	/* Reenable interrupts for this hook */
	if ((s = sys_irqenable(&g_driver.hook)) != OK)
		printf("ip1000: Cannot enable OS interrupts: %d\n", s);

	/* Perform tasks based on the flagged conditions */
	ic_check_ints(&g_driver);
}

/* Real handler interrupt */
static void ic_handler(ic_driver *pdev) {
	u32_t base = pdev->base_addr;
	u16_t intr_status;
	int flag = 0, tx_head, tx_tail;
	ic_desc *desc;

	/* Get interrupt status */
	/* ### GET_INTR_STATUS ### */
	intr_status = ic_in16(base, REG_ISR);

	/* Clear interrupt */
	/* ### CLEAR_INTR ### */
	ic_out16(base, REG_ISR, intr_status & INTR_ISR_CLEAR);

	/* Enable interrupt */
	/* ### ENABLE_INTR ### */
	ic_out16(base, REG_IMR, INTR_IMR_ENABLE);

	/* Check interrupt error */
	/* ### CHECK_INTR_ERROR ### */
	if (intr_status & INTR_ISR_ERR) {
		printf("ip1000: interrupt error\n");
		return;
	}

	/* Check link status */
	/* ### CHECK_LINK_INTR ### */
	if (intr_status & INTR_ISR_LINK_EVENT) {
		pdev->link = ic_check_link(base);
#ifdef MY_DEBUG
		printf("ip1000: Link state change\n");
#endif
		flag++;
	}

	/* Check Rx request status */
	/* ### CHECK_RX_INTR ### */
	if (intr_status & INTR_ISR_RX_DONE) {
		pdev->recv_flag = TRUE;
		flag++;
	}

	/* Check Tx request status */
	/* ### CHECK_TX_INTR ### */
	if (intr_status & INTR_ISR_TX_DONE) {
		pdev->send_flag = TRUE;
		flag++;

		/* Manage Tx Buffer */
		tx_head = pdev->tx_head;
		tx_tail = pdev->tx_tail;
		while (tx_tail != tx_head) {
			desc = pdev->tx_desc;
			desc += tx_tail;
			if (!pdev->tx[tx_tail].busy)
				printf("ip1000: Strange, buffer not busy?\n");

			/* Check whether the transmiting is OK */
			/* ### CHECK_TX_STATUS_OK ### */
			if (ic_tx_status_ok(desc) != TRUE)
				break;

			/* Check Tx status error */
			/* ### CHECK_TX_STATUS_ERROR ### */
			if (desc->status & DESC_STATUS_TX_SEND_ERR)
				printf("ip1000: Tx error\n");

			pdev->stat.ets_packetT++;
			pdev->tx[tx_tail].busy = FALSE;
			pdev->tx_busy_num--;

			if (++tx_tail >= TX_DESC_NUM)
				tx_tail = 0;

			pdev->send_flag = TRUE;
			pdev->recv_flag = TRUE;

			/* Set Tx descriptor status in interrupt */
			/* ### SET_TX_STATUS_INTR ### */
			desc->status = DESC_STATUS_TX_SEND_CLEAR;

#ifdef MY_DEBUG
			printf("ip1000: Successfully send a packet\n");
#endif
		}
		pdev->tx_tail = tx_tail;
	}
#ifdef MY_DEBUG
	if (!flag) {
		printf("ip1000: Unknown error in interrupt\n");
		return;
	}
#endif
}

/* Check interrupt and perform */
static void ic_check_ints(ic_driver *pdev) {
	if (!pdev->recv_flag)
		return;
	pdev->recv_flag = FALSE;

	/* Handle data receive */
	netdriver_recv();

	/* Handle data transmit */
	if (pdev->send_flag) {
		pdev->send_flag = FALSE;
		netdriver_send();
	}
}

static void ic_stat(eth_stat_t *stat) {
	memcpy(stat, &g_driver.stat, sizeof(*stat));
}
