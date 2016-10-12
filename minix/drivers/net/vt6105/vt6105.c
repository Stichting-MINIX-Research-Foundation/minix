#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <machine/pci.h>
#include <sys/mman.h>
#include "vt6105.h"
#include "io.h"

/* global value */
static NDR_driver g_driver;
static int g_instance;

/* driver interface */
static int NDR_init(unsigned int instance, netdriver_addr_t * addr,
	uint32_t * caps, unsigned int * ticks);
static void NDR_stop(void);
static void NDR_set_mode(unsigned int mode,
	const netdriver_addr_t * mcast_list, unsigned int mcast_count);
static ssize_t NDR_recv(struct netdriver_data *data, size_t max);
static int NDR_send(struct netdriver_data *data, size_t size);
static void NDR_intr(unsigned int mask);

/* internal function */
static int dev_probe(NDR_driver *pdev, int instance);
static int dev_init_buf(NDR_driver *pdev);
static int dev_init_hw(NDR_driver *pdev, netdriver_addr_t *addr);
static int dev_reset_hw(NDR_driver *pdev);
static void dev_conf_addr(NDR_driver *pdev, netdriver_addr_t *addr);
static void dev_handler(NDR_driver *pdev);
static void dev_check_ints(NDR_driver *pdev);

/* developer interface */
static int dev_real_reset(u32_t *base);
static int dev_init_io(u32_t *base);
static int dev_init_mii(u32_t *base);
static void dev_intr_control(u32_t *base, int flag);
static void dev_rx_tx_control(u32_t *base, int flag);
static void dev_get_addr(u32_t *base, u8_t *pa);
static int dev_check_link(u32_t *base);
static void dev_set_rec_mode(u32_t *base, int mode);
static void dev_start_tx(u32_t *base);
static u32_t dev_read_clear_intr_status(u32_t *base);
static void dev_init_rx_desc(NDR_desc *desc_start, int index, size_t buf_size,
			phys_bytes buf_dma, int max_desc_num, phys_bytes desc_dma_start);
static void dev_init_tx_desc(NDR_desc *desc_start, int index, size_t buf_size,
			phys_bytes buf_dma, int max_desc_num, phys_bytes desc_dma_start);
static void dev_set_desc_reg(u32_t *base, phys_bytes rx_addr,
								phys_bytes tx_addr);
static int dev_rx_ok_desc(u32_t *base, NDR_desc *desc, int index);
static int dev_rx_len_desc(u32_t *base, NDR_desc *desc, int index);
static void dev_set_rx_desc_done(u32_t *base, NDR_desc *desc, int index);
static void dev_set_tx_desc_prepare(u32_t *base, NDR_desc *desc, int index,
										size_t data_size);
static int dev_tx_ok_desc(u32_t *base, NDR_desc *desc, int index);
static void dev_set_tx_desc_done(u32_t *base, NDR_desc *desc, int index);

/* ======= Developer implemented function ======= */
/* ====== Self-defined function ======*/

/* ====== Developer interface ======*/
/* Real hardware reset (### RESET_HARDWARE_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int dev_real_reset(u32_t *base) {
	u32_t base0 = base[0];
	ndr_out16(base0, REG_CR, CMD_RESET);
	micro_delay(5000);
	if (ndr_in16(base0, REG_CR) & CMD_RESET) {
		ndr_out8(base0, REG_MCR, 0x40);
		micro_delay(5000);
		if (ndr_in16(base0, REG_CR) & CMD_RESET)
			return -EIO;
	}
	return OK;
}

/* Intialize other hardware I/O registers (### INIT_HARDWARE_IO_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int dev_init_io(u32_t *base) {
	u32_t base0 = base[0];
	u8_t stick;
	stick = ndr_in8(base0, REG_STICK);
	ndr_out8(base0, REG_STICK, stick & 0xfc);
	ndr_out16(base0, REG_BCR, 0x0006);
	ndr_out8(base0, REG_TCR, 0x20);
	ndr_out8(base0, REG_RCR, 0x78);
	return OK;
}

/* Intialize MII interface (### MII_INIT_CAN_FAIL ###)
  -- Return OK means success, Others means failure */
static int dev_init_mii(u32_t *base) {
	u32_t base0 = base[0];
	ndr_out8(base0, REG_MII_CR, 0);
	ndr_out8(base0, REG_MII_REG, 0x01);
	ndr_out8(base0, REG_MII_CR, 0x80);
	micro_delay(5000);
	if (!(ndr_in8(base0, REG_MII_REG) & 0x20)) {
		printf("NDR: Fail to monitor linkage\n");
		return -EIO;
	}
	ndr_out8(base0, REG_MII_REG, 0x41);
	return OK;
}

/* Enable or disable interrupt (### INTR_ENABLE_DISABLE ###) */
static void dev_intr_control(u32_t *base, int flag) {
	u32_t data, base0 = base[0];
	data = ndr_in16(base0, REG_IMR);
	if (flag == INTR_ENABLE)
		ndr_out16(base0, REG_IMR, data | CMD_INTR_ENABLE);
	else if (flag == INTR_DISABLE)
		ndr_out16(base0, REG_IMR, data & ~CMD_INTR_ENABLE);
}

/* Enable or disable Rx/Tx (### RX_TX_ENABLE_DISABLE ###) */
static void dev_rx_tx_control(u32_t *base, int flag) {
	u32_t data, base0 = base[0];
	if (flag == RX_TX_ENABLE) {
		data = CMD_START | CMD_RX_ON | CMD_TX_ON | CMD_NO_POLL | CMD_FDUPLEX;
		ndr_out16(base0, REG_CR, data);
	}
	else if (flag == RX_TX_DISABLE) {
		ndr_out16(base0, REG_CR, CMD_STOP);
	}
}

/* Get MAC address to the array 'pa' (### GET_MAC_ADDR ###) */
static void dev_get_addr(u32_t *base, u8_t *pa) {
	u32_t i, base0 = base[0];
	for (i = 0; i < 6; i++)
		pa[i] = ndr_in8(base0, REG_ADDR + i);
}

/* Check link status (### CHECK_LINK ###)
 * -- Return LINK_UP or LINK_DOWN */
static int dev_check_link(u32_t *base) {
	u32_t data, base0 = base[0];
	ndr_out8(base0, REG_MII_PHY, 0x01);
	ndr_out8(base0, REG_MII_REG, 0x01);
	ndr_out8(base0, REG_MII_CR, 0x40);
	micro_delay(5000);
	if (ndr_in8(base0, REG_MII_CR) & 0x40)
		printf("NDR: Fail to get linkage\n");
	data = ndr_in16(base0, REG_MII_DATA);
	if (data & LINK_STATUS)
		return LINK_UP;
	return LINK_DOWN;
}

/* Set driver receive mode (### SET_REC_MODE ###) */
static void dev_set_rec_mode(u32_t *base, int mode) {
	u32_t data, base0 = base[0];
	data = ndr_in8(base0, REG_RCR);
	data &= ~(CMD_RCR_UNICAST | CMD_RCR_MULTICAST | CMD_RCR_BROADCAST);
	if (mode & NDEV_MODE_PROMISC)
		data |= CMD_RCR_UNICAST | CMD_RCR_BROADCAST | CMD_RCR_MULTICAST;
	if (mode & NDEV_MODE_BCAST)
		data |= CMD_RCR_BROADCAST;
	if (mode & (NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL))
		data |= CMD_RCR_MULTICAST;
	data |= CMD_RCR_UNICAST;
	ndr_out8(base0, REG_RCR, data);
}

/* Start Tx channel (### START_TX_CHANNEL ###) */
static void dev_start_tx(u32_t *base) {
	u32_t data, base0 = base[0];
	data = ndr_in8(base0, REG_CR);
	ndr_out8(base0, REG_CR, data | CMD_TX_DEMAND);
}

/* Read and clear interrupt (### READ_CLEAR_INTR_STS ###) */
static u32_t dev_read_clear_intr_status(u32_t *base) {
	u32_t data, base0 = base[0];
	data = ndr_in16(base0, REG_ISR);
	ndr_out16(base0, REG_ISR, data & INTR_STS_CLEAR);
	return data;
}

/* ---------- WITH DESCRIPTOR ---------- */
/* Intialize Rx descriptor (### INIT_RX_DESC ###) */
static void dev_init_rx_desc(NDR_desc *desc_start, int index, size_t buf_size,
			phys_bytes buf_dma, int max_desc_num, phys_bytes desc_dma_start) {
	NDR_desc *desc = desc_start + index;
	desc->status = DESC_OWN | ((buf_size << 16) & DESC_RX_LENMASK);
	desc->addr = buf_dma;
	desc->length = buf_size;
	if (index == max_desc_num - 1)
		desc->next = desc_dma_start;
	else
		desc->next = desc_dma_start + (index + 1) * sizeof(NDR_desc);
}

/* Intialize Tx descriptor (### INIT_TX_DESC ###) */
static void dev_init_tx_desc(NDR_desc *desc_start, int index, size_t buf_size,
			phys_bytes buf_dma, int max_desc_num, phys_bytes desc_dma_start) {
	NDR_desc *desc = desc_start + index;
	desc->addr = buf_dma;
	desc->length = buf_size;
	if (index == max_desc_num - 1)
		desc->next = desc_dma_start;
	else
		desc->next = desc_dma_start + (index + 1) * sizeof(NDR_desc);
}

/* Set Rx/Tx descriptor address into device register (### SET_DESC_REG ###) */
static void dev_set_desc_reg(u32_t *base, phys_bytes rx_addr,
								phys_bytes tx_addr) {
	u32_t base0 = base[0];
	ndr_out32(base0, REG_RX_DESC_BASE, rx_addr);
	ndr_out32(base0, REG_TX_DESC_BASE, tx_addr);
}

/* Check whether Rx is OK from Rx descriptor (### CHECK_RX_OK_FROM_DESC ###)
 * -- Current buffer number is index
 * -- Return RX_OK or RX_SUSPEND or RX_ERROR */
static int dev_rx_ok_desc(u32_t *base, NDR_desc *desc, int index) {
	if (!(desc->status & DESC_OWN)) {
		if (desc->status & DESC_RX_ERROR)
			return RX_ERROR;
		if ((desc->status & DESC_RX_NORMAL) == DESC_RX_NORMAL)
			return RX_OK;
	}
	return RX_SUSPEND;
}

/* Get length from Rx descriptor (### GET_RX_LENGTH_FROM_DESC ###)
 * -- Current buffer number is index
 * -- Return the length */
static int dev_rx_len_desc(u32_t *base, NDR_desc *desc, int index) {
	return ((desc->status & DESC_RX_LENMASK) >> 16) - NDEV_ETH_PACKET_CRC;
}

/* Set Rx descriptor after Rx done (### SET_RX_DESC_DONE ###)
 * -- Current buffer number is index */
static void dev_set_rx_desc_done(u32_t *base, NDR_desc *desc, int index) {
	desc->status = DESC_OWN;
}

/* Set Tx descriptor to prepare transmitting (### SET_TX_DESC_PREPARE)
 * -- Current buffer number is index */
static void dev_set_tx_desc_prepare(u32_t *base, NDR_desc *desc, int index,
								size_t data_size) {
	desc->status = DESC_OWN | DESC_FIRST | DESC_LAST;
	desc->length = 0x00e08000 | (data_size > 60 ? data_size : 60);
}

/* Check whether Tx is OK from Tx descriptor (### CHECK_TX_OK_FROM_DESC ###)
 * -- Current buffer number is index
 * -- Return TX_OK or TX_SUSPEND or TX_ERROR */
static int dev_tx_ok_desc(u32_t *base, NDR_desc *desc, int index) {
	if (!(desc->status & DESC_OWN)) {
		if (desc->status & DESC_TX_ERROR)
			return TX_ERROR;
		return TX_OK;
	}
	return TX_SUSPEND;
}

/* Set Tx descriptor after Tx done (### SET_TX_DESC_DONE ###)
 * -- Current buffer number is index */
static void dev_set_tx_desc_done(u32_t *base, NDR_desc *desc, int index) {
	desc->status = 0;
}

/* Driver interface table */
static const struct netdriver NDR_table = {
	.ndr_name = "vr",
	.ndr_init = NDR_init,
	.ndr_stop = NDR_stop,
	.ndr_set_mode = NDR_set_mode,
	.ndr_recv = NDR_recv,
	.ndr_send = NDR_send,
	.ndr_intr = NDR_intr,
};

int main(int argc, char *argv[]) {
	env_setargs(argc, argv);
	netdriver_task(&NDR_table);
}

/* Initialize the driver */
static int
NDR_init(unsigned int instance, netdriver_addr_t * addr, uint32_t * caps,
	unsigned int * ticks __unused)
{
	int i, ret = 0;

	/* Intialize driver data structure */
	memset(&g_driver, 0, sizeof(g_driver));
	g_driver.link = LINK_UNKNOWN;
	g_instance = instance;

	/* Probe the device */
	if (dev_probe(&g_driver, instance)) {
		printf("NDR: Device is not found\n");
		ret = -ENODEV;
		goto err_probe;
	}

	/* Intialize hardware */
	if (dev_init_hw(&g_driver, addr)) {
		printf("NDR: Fail to initialize hardware\n");
		ret = -EIO;
		goto err_init_hw;
	}

	/* Allocate and initialize buffer */
	if (dev_init_buf(&g_driver)) {
		printf("NDR: Fail to initialize buffer\n");
		ret = -ENODEV;
		goto err_init_buf;
	}

	/* Enable interrupts */
	/* ### INTR_ENABLE_DISABLE ### */
	dev_intr_control(g_driver.base, INTR_ENABLE);

	/* Start Rx and Tx */
	/* ### RX_TX_ENABLE_DISABLE ### */
	dev_rx_tx_control(g_driver.base, RX_TX_ENABLE);

	/* Clear send and recv flag */
	g_driver.send_flag = FALSE;
	g_driver.recv_flag = FALSE;

	*caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST;
	return OK;

err_init_buf:
err_init_hw:
err_probe:
	return ret;
}

/* Stop the driver */
static void NDR_stop(void) {
	/* Free Rx and Tx buffer*/
	free_contig(g_driver.buf, g_driver.buf_size);

	/* Stop interrupt */
	/* ### INTR_ENABLE_DISABLE ### */
	dev_intr_control(g_driver.base, INTR_DISABLE);

	/* Stop Rx and Tx */
	/* ### RX_TX_ENABLE_DISABLE ### */
	dev_rx_tx_control(g_driver.base, RX_TX_DISABLE);
}

/* Set driver mode */
static void
NDR_set_mode(unsigned int mode, const netdriver_addr_t * mcast_list __unused,
	unsigned int mcast_count __unused)
{
	g_driver.mode = mode;
	/* Set driver receive mode */
	/* ### SET_REC_MODE ### */
	dev_set_rec_mode(g_driver.base, mode);
}

/* Receive data */
static ssize_t NDR_recv(struct netdriver_data *data, size_t max) {
	NDR_driver *pdev = &g_driver;
	u32_t totlen, packlen;
	int index, ret, offset = 0;
	NDR_desc *desc;

	index = pdev->rx_head;
	desc = pdev->rx_desc;
	desc += index;
	/* Check whether Rx is OK from Rx descriptor */
	/* ### CHECK_RX_OK_FROM_DESC ### */
	ret = dev_rx_ok_desc(pdev->base, desc, index);
	if (ret == RX_SUSPEND)
		return SUSPEND;
	else if (ret == RX_ERROR)
		printf("NDR: Rx error now\n");
	/* Get length from Rx descriptor */
	/* ### GET_RX_LENGTH_FROM_DESC ### */
	totlen = dev_rx_len_desc(pdev->base, desc, index);

	/* Get data length */
	/* ### Get , int inde, int indexxRx data length ### */
	if (totlen < 8 || totlen > 2 * NDEV_ETH_PACKET_MAX) {
		printf("NDR: Bad data length: %d\n", totlen);
		panic(NULL);
	}

	packlen = totlen;
	if (packlen > max)
		packlen = max;

	/* Copy data to user */
	netdriver_copyout(data, 0, pdev->rx[index].buf + offset, packlen);

	/* Set Rx descriptor after Rx done */
	/* ### SET_RX_DESC_DONE ### */
	dev_set_rx_desc_done(pdev->base, desc, index);
	if (index == RX_BUFFER_NUM - 1)
		index = 0;
	else
		index++;
	pdev->rx_head = index;

#ifdef MY_DEBUG
	printf("NDR: Successfully receive a packet, length = %d\n", packlen);
#endif

	return packlen;
}

/* Transmit data */
static int NDR_send(struct netdriver_data *data, size_t size) {
	NDR_driver *pdev = &g_driver;
	int tx_head, i;
	NDR_desc *desc;

	tx_head = pdev->tx_head;
	if (pdev->tx[tx_head].busy)
		return SUSPEND;

	/* Copy data from user */
	netdriver_copyin(data, 0, pdev->tx[tx_head].buf, size);

	/* Set busy */
	pdev->tx[tx_head].busy = TRUE;
	pdev->tx_busy_num++;

	desc = pdev->tx_desc;
	desc += tx_head;
	/* Set Tx descriptor to prepare transmitting */
	/* ### SET_TX_DESC_PREPARE ### */
	dev_set_tx_desc_prepare(pdev->base, desc, tx_head, size);
	if (tx_head == TX_BUFFER_NUM - 1)
		tx_head = 0;
	else
		tx_head++;
	pdev->tx_head = tx_head;

	/* Start Tx channel */
	/* ### START_TX ### */
	dev_start_tx(pdev->base);

	return 0;
}

/* Handle interrupt */
static void NDR_intr(unsigned int mask) {
	int s;

	/* Run interrupt handler at driver level */
	dev_handler(&g_driver);

	/* Reenable interrupts for this hook */
	if ((s = sys_irqenable(&g_driver.hook)) != OK)
		printf("NDR: Cannot enable OS interrupts: %d\n", s);

	/* Perform tasks based on the flagged conditions */
	dev_check_ints(&g_driver);
}

/* Match the device and get base address */
static int dev_probe(NDR_driver *pdev, int instance) {
	int devind, ioflag, i;
	u16_t cr, vid, did;
	u32_t bar, size, base;
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

	/* Enable bus mastering and I/O space */
	cr = pci_attr_r16(devind, PCI_CR);
	pci_attr_w16(devind, PCI_CR, cr | 0x105);

	/* Get base address */
	for (i = 0; i < 6; i++)
		pdev->base[i] = 0;
#ifdef DMA_BASE_IOMAP
	for (i = 0; i < 6; i++) {
		if (pci_get_bar(devind, PCI_BAR + i * 4, &base, &size, &ioflag)) {
			/* printf("NDR: Fail to get PCI BAR\n"); */
			continue;
		}
		if (ioflag) {
			/* printf("NDR: PCI BAR is not for memory\n"); */
			continue;
		}
		if ((reg = vm_map_phys(SELF, (void *)base, size)) == MAP_FAILED) {
			printf("NDR: Fail to map hardware registers from PCI\n");
			return -EIO;
		}
		pdev->base[i] = (u32_t)reg;
	}
#else
	for (i = 0; i < 6; i++)
		pdev->base[i] = pci_attr_r32(devind, PCI_BAR + i * 4) & 0xffffffe0;
#endif
	pdev->dev_name = pci_dev_name(vid, did);
	pdev->irq = pci_attr_r8(devind, PCI_ILR);
	pdev->revision = pci_attr_r8(devind, PCI_REV);
	pdev->did = did;
	pdev->vid = vid;
	pdev->devind = devind;

#ifdef MY_DEBUG
	printf("NDR: Hardware name is %s\n", pdev->dev_name);
	for (i = 0; i < 6; i++)
		printf("NDR: PCI BAR%d is 0x%08x\n", i, pdev->base[i]);
	printf("NDR: IRQ number is 0x%02x\n", pdev->irq);
#endif

	return 0;
}

/* Intialize hardware */
static int dev_init_hw(NDR_driver *pdev, netdriver_addr_t *addr) {
	int r, ret;

	/* Set the OS interrupt handler */
	pdev->hook = pdev->irq;
	if ((r = sys_irqsetpolicy(pdev->irq, 0, &pdev->hook)) != OK) {
		printf("NDR: Fail to set OS IRQ policy: %d\n", r);
		ret = -EFAULT;
		goto err_irq_policy;
	}

	/* Reset hardware */
	if (dev_reset_hw(pdev)) {
		printf("NDR: Fail to reset the device\n");
		ret = -EIO;
		goto err_reset_hw;
	}

	/* Enable OS IRQ */
	if ((r = sys_irqenable(&pdev->hook)) != OK) {
		printf("NDR: Fail to enable OS IRQ: %d\n", r);
		ret = -EFAULT;
		goto err_irq_enable;
	}

	/* Configure MAC address */
	dev_conf_addr(pdev, addr);

	/* Detect link status */
	/* ### CHECK_LINK ### */
	pdev->link = dev_check_link(pdev->base);
#ifdef MY_DEBUG
	if (pdev->link)
		printf("NDR: Link up\n");
	else
		printf("NDR: Link down\n");
#endif

	return 0;

err_reset_hw:
err_irq_enable:
err_irq_policy:
	return ret;
}

/* Reset hardware */
static int dev_reset_hw(NDR_driver *pdev) {
	int ret;

	/* Reset the chip */
	/* ### RESET_HARDWARE_CAN_FAIL ### */
	if (dev_real_reset(pdev->base)) {
		printf("NDR: Fail to reset the hardware\n");
		ret = -EIO;
		goto err_real_reset;
	}

	/* Initialize other hardware I/O registers */
	/* ### SET_RX_DESC_REG ### */
	if (dev_init_io(pdev->base)) {
		printf("NDR: Fail to initialize I/O registers\n");
		ret = -EIO;
		goto err_init_io;
	}

	/* Initialize MII interface */
	/* ### MII_INIT_CAN_FAIL ### */
	if (dev_init_mii(pdev->base)) {
		printf("NDR: Fail to initialize MII interface\n");
		ret = -EIO;
		goto err_init_mii;
	}

	return 0;

err_init_mii:
err_init_io:
err_real_reset:
	return ret;
}

/* Configure MAC address */
static void dev_conf_addr(NDR_driver *pdev, netdriver_addr_t *addr) {
	u8_t pa[6];

	/* Get MAC address */
	/* ### GET_MAC_ADDR ### */
	dev_get_addr(pdev->base, pa);
	addr->na_addr[0] = pa[0];
	addr->na_addr[1] = pa[1];
	addr->na_addr[2] = pa[2];
	addr->na_addr[3] = pa[3];
	addr->na_addr[4] = pa[4];
	addr->na_addr[5] = pa[5];
#ifdef MY_DEBUG
	printf("NDR: Ethernet address is %02x:%02x:%02x:%02x:%02x:%02x\n",
			addr->na_addr[0], addr->na_addr[1], addr->na_addr[2],
			addr->na_addr[3], addr->na_addr[4], addr->na_addr[5]);
#endif
}

/* Allocate and initialize buffer */
static int dev_init_buf(NDR_driver *pdev) {
	size_t rx_desc_size, tx_desc_size, rx_buf_size, tx_buf_size, tot_buf_size;
	phys_bytes buf_dma;
	char *buf;
	int i;

	/* Build Rx and Tx buffer */
	tx_buf_size = TX_BUF_SIZE;
	if (tx_buf_size % 4)
		tx_buf_size += 4 - (tx_buf_size % 4);
	rx_buf_size = RX_BUF_SIZE;
	if (rx_buf_size % 4)
		rx_buf_size += 4 - (rx_buf_size % 4);
	tot_buf_size = TX_BUFFER_NUM * tx_buf_size + RX_BUFFER_NUM * rx_buf_size;
	rx_desc_size = RX_BUFFER_NUM * sizeof(NDR_desc);
	tx_desc_size = TX_BUFFER_NUM * sizeof(NDR_desc);
	tot_buf_size += rx_desc_size + tx_desc_size;
	if (tot_buf_size % 4096)
		tot_buf_size += 4096 - (tot_buf_size % 4096);

	if (!(buf = alloc_contig(tot_buf_size, 0, &buf_dma))) {
		printf("NDR: Fail to allocate memory\n");
		return -ENOMEM;
	}
	pdev->buf_size = tot_buf_size;
	pdev->buf = buf;

	/* Rx descriptor buffer location */
	pdev->rx_desc = (NDR_desc *)buf;
	pdev->rx_desc_dma = buf_dma;
	memset(buf, 0, rx_desc_size);
	buf += rx_desc_size;
	buf_dma += rx_desc_size;

	/* Tx descriptor buffer location */
	pdev->tx_desc = (NDR_desc *)buf;
	pdev->tx_desc_dma = buf_dma;
	memset(buf, 0, tx_desc_size);
	buf += tx_desc_size;
	buf_dma += tx_desc_size;

	/* Rx buffer assignment */
	for (i = 0; i < RX_BUFFER_NUM; i++) {
		/* Initialize Rx buffer */
		pdev->rx[i].buf_dma = buf_dma;
		pdev->rx[i].buf = buf;
		buf_dma += rx_buf_size;
		buf += rx_buf_size;
		/* Set Rx descriptor */
		/* ### INIT_RX_DESC ### */
		dev_init_rx_desc(pdev->rx_desc, i, rx_buf_size, pdev->rx[i].buf_dma,
							RX_BUFFER_NUM, pdev->rx_desc_dma);
	}

	/* Tx buffer assignment */
	for (i = 0; i < TX_BUFFER_NUM; i++) {
		/* Set Tx buffer */
		pdev->tx[i].busy = 0;
		pdev->tx[i].buf_dma = buf_dma;
		pdev->tx[i].buf = buf;
		buf_dma += tx_buf_size;
		buf += tx_buf_size;
		/* Initialize Tx descriptor */
		/* ### INIT_TX_DESC ### */
		dev_init_tx_desc(pdev->tx_desc, i, tx_buf_size, pdev->tx[i].buf_dma,
							TX_BUFFER_NUM, pdev->tx_desc_dma);
	}

	/* Set Rx/Tx descriptor address into device register */
	/* ### SET_DESC_REG ### */
	dev_set_desc_reg(pdev->base, g_driver.rx_desc_dma,
						g_driver.tx_desc_dma);

	pdev->tx_busy_num = 0;
	pdev->tx_head = 0;
	pdev->tx_tail = 0;
	pdev->rx_head = 0;

	return 0;
}

/* Real handler interrupt */
static void dev_handler(NDR_driver *pdev) {
	u32_t intr_status;
	int tx_head, tx_tail, index, flag = 0, ret;
	NDR_desc *desc;

	/* Read and clear interrupt status */
	/* ### READ_CLEAR_INTR_STS ### */
	intr_status = dev_read_clear_intr_status(pdev->base);

	/* Enable interrupt */
	/* ### INTR_ENABLE_DISABLE ### */
	dev_intr_control(pdev->base, INTR_ENABLE);

	/* Check link status */
	if (intr_status & INTR_STS_LINK) {
		pdev->link = dev_check_link(pdev->base);
#ifdef MY_DEBUG
		printf("NDR: Link state change\n");
#endif
		flag++;
	}
	/* Check Rx request status */
	if (intr_status & INTR_STS_RX) {
		pdev->recv_flag = TRUE;
		flag++;
	}
	/* Check Tx request status */
	if (intr_status & INTR_STS_TX) {
		pdev->send_flag = TRUE;
		flag++;

		/* Manage Tx Buffer */
		tx_head = pdev->tx_head;
		tx_tail = pdev->tx_tail;
		while (tx_tail != tx_head) {
			if (!pdev->tx[tx_tail].busy)
				printf("NDR: Strange, buffer not busy?\n");
			index = tx_tail;
			desc = pdev->tx_desc;
			desc += tx_tail;
			/* Check whether Tx is OK from Tx descriptor */
			/* ### CHECK_TX_OK_FROM_DESC ### */
			ret = dev_tx_ok_desc(pdev->base, desc, index);
			if (ret == TX_SUSPEND)
				break;
			else if (ret == TX_ERROR)
				printf("NDR: Tx error now\n");

			pdev->tx[tx_tail].busy = FALSE;
			pdev->tx_busy_num--;

			if (++tx_tail >= TX_BUFFER_NUM)
				tx_tail = 0;

			pdev->send_flag = TRUE;
			pdev->recv_flag = TRUE;

			/* Set Tx descriptor after Tx done */
			/* ### SET_TX_DESC_DONE ### */
			dev_set_tx_desc_done(pdev->base, desc, index);
#ifdef MY_DEBUG
			printf("NDR: Successfully send a packet\n");
#endif
		}
		pdev->tx_tail = tx_tail;
	}
#ifdef MY_DEBUG
	if (!flag) {
		printf("NDR: Unknown error in interrupt 0x%08x\n", intr_status);
		return;
	}
#endif
}

/* Check interrupt and perform */
static void dev_check_ints(NDR_driver *pdev) {
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
