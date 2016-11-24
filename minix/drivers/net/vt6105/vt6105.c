#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <machine/pci.h>
#include "vt6105.h"
#include "io.h"

/* global value */
static vt_driver g_driver;
static int g_instance;

/* driver interface */
static int vt_init(unsigned int instance, ether_addr_t *addr);
static void vt_stop(void);
static void vt_mode(unsigned int mode);
static ssize_t vt_recv(struct netdriver_data *data, size_t max);
static int vt_send(struct netdriver_data *data, size_t size);
static void vt_intr(unsigned int mask);
static void vt_stat(eth_stat_t *stat);

/* internal function */
static int vt_probe(vt_driver *pdev, int instance);
static int vt_init_buf(vt_driver *pdev);
static int vt_init_hw(vt_driver *pdev, ether_addr_t *addr);
static int vt_reset_hw(vt_driver *pdev);
static void vt_conf_addr(vt_driver *pdev, ether_addr_t *addr);
static void vt_handler(vt_driver *pdev);
static void vt_check_ints(vt_driver *pdev);

/* developer interface */
static void vt_init_rx_desc(vt_desc *desc, size_t size, phys_bytes dma);
static void vt_init_tx_desc(vt_desc *desc, size_t size, phys_bytes dma);
static int vt_real_reset(u32_t base);
static int vt_init_power(u32_t base);
static int vt_init_mii(u32_t base);
static int vt_init_io(u32_t base);
static void vt_start_rx_tx(u32_t base);
static void vt_get_addr(u32_t base, u8_t *pa);
static int vt_check_link(u32_t base);
static void vt_stop_rx_tx(u32_t base);
static int vt_rx_status_ok(vt_desc *desc);
static int vt_get_rx_len(vt_desc *desc);
static void vt_tx_desc_start(vt_desc *desc, size_t size);
static void vt_wakeup_tx(u32_t base);
static int vt_tx_status_ok(vt_desc *desc);

/* ======= Developer-defined function ======= */
/* Intialize Rx descriptor (### RX_DESC_INIT ###) */
static void vt_init_rx_desc(vt_desc *desc, size_t size, phys_bytes dma) {
	desc->status = DESC_OWN | ((size << 16) & DESC_RX_LENMASK);
	desc->addr = dma;
	desc->length = size;
}

/* Intialize Tx descriptor (### TX_DESC_INIT ###) */
static void vt_init_tx_desc(vt_desc *desc, size_t size, phys_bytes dma) {
	desc->addr = dma;
	desc->length = size;
}

/* Real hardware reset (### RESET_HARDWARE_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int vt_real_reset(u32_t base) {
	vt_out16(base, REG_CR, CMD_RESET);
	micro_delay(10000);
	if (vt_in16(base, REG_CR) & CMD_RESET) {
		vt_out8(base, REG_MCR1, 0x40);
		micro_delay(10000);
		if (vt_in16(base, REG_CR) & CMD_RESET)
			return -EIO;
	}
	return OK;
}

/* Intialize power (### POWER_INIT_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int vt_init_power(u32_t base) {
	u8_t stick;
	stick = vt_in8(base, REG_STICK);
	vt_out8(base, REG_STICK, stick & 0xfc);
	return OK;
}

/* Intialize MII interface (### MII_INIT_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int vt_init_mii(u32_t base) {
	return OK;
}

/* Intialize other hardware I/O registers (### INIT_HARDWARE_IO_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int vt_init_io(u32_t base) {
	vt_out16(base, REG_BCR0, 0x0006);
	vt_out8(base, REG_TCR, 0x20);
	vt_out8(base, REG_RCR, 0x78);
	return OK;
}

/* Start Rx/Tx (### START_RX_TX ###) */
static void vt_start_rx_tx(u32_t base) {
	u16_t cmd = CMD_START | CMD_RX_ON | CMD_TX_ON | CMD_NO_POLL | CMD_FDUPLEX;
	vt_out16(base, REG_CR, cmd);
	micro_delay(1000);
}

/* Get MAC address to the array 'pa' (### GET_MAC_ADDR ###) */
static void vt_get_addr(u32_t base, u8_t *pa) {
	int i;
	for (i = 0; i < 6; i++)
		pa[i] = vt_in8(base, REG_ADDR + i);
}

/* Check link status (### CHECK_LINK ###)
 * -- Return LINK_UP or LINK_DOWN */
static int vt_check_link(u32_t base) {
	u32_t r;
	vt_out8(base, REG_MII_CFG, 0x01);
	vt_out8(base, REG_MII_ADDR, 0x01);
	vt_out8(base, REG_MII_CR, 0x40);
	micro_delay(10000);
	r = vt_in16(base, REG_MII_DATA);
	if (r & 0x0004)
		return LINK_UP;
	return LINK_DOWN;
}

/* Stop Rx/Tx (### STOP_RX_TX ###) */
static void vt_stop_rx_tx(u32_t base) {
	vt_out16(base, REG_CR, CMD_STOP);
}

/* Check whether Rx status OK (### CHECK_RX_STATUS_OK ###)
 * -- Return TRUE or FALSE */
static int vt_rx_status_ok(vt_desc *desc) {
	if (!(desc->status & DESC_OWN)) {
		if ((desc->status & DESC_RX_NORMAL) == DESC_RX_NORMAL)
			return TRUE;
	}
	return FALSE;
}

/* Get Rx data length from descriptor (### GET_RX_LEN ###)
 * --- Return the length */
static int vt_get_rx_len(vt_desc *desc) {
	int len;
	len = ((desc->status & DESC_RX_LENMASK) >> 16) - ETH_CRC_SIZE;
	return len;
}

/* Set Tx descriptor in send (### TX_DESC_START ###) */
static void vt_tx_desc_start(vt_desc *desc, size_t size) {
	desc->status = DESC_OWN | DESC_FIRST | DESC_LAST;
	desc->length = 0x00e08000 | (size > 60 ? size : 60);
}

/* Wake up Tx channel (### WAKE_UP_TX ###) */
static void vt_wakeup_tx(u32_t base) {
	u8_t cmd;
	cmd = vt_in8(base, REG_CR);
	cmd |= CMD_TX_DEMAND;
	vt_out8(base, REG_CR, cmd);
}

/* Check whether Tx status OK (### CHECK_TX_STATUS_OK ###)
 * -- Return TRUE or FALSE */
static int vt_tx_status_ok(vt_desc *desc) {
	if (!(desc->status & DESC_OWN))
		return TRUE;
	return FALSE;
}

/* Driver interface table */
static const struct netdriver vt_table = {
	.ndr_init = vt_init,
	.ndr_stop = vt_stop,
	.ndr_mode = vt_mode,
	.ndr_recv = vt_recv,
	.ndr_send = vt_send,
	.ndr_stat = vt_stat,
	.ndr_intr = vt_intr,
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
	g_driver.link = LINK_UNKNOWN;
	strcpy(g_driver.name, "netdriver#0");
	g_driver.name[10] += instance;
	g_instance = instance;

	/* Probe the device */
	if (vt_probe(&g_driver, instance)) {
		printf("vt6105: Device is not found\n");
		ret = -ENODEV;
		goto err_probe;
	}

	/* Allocate and initialize buffer */
	if (vt_init_buf(&g_driver)) {
		printf("vt6105: Fail to initialize buffer\n");
		ret = -ENODEV;
		goto err_init_buf;
	}

	/* Intialize hardware */
	if (vt_init_hw(&g_driver, addr)) {
		printf("vt6105: Fail to initialize hardware\n");
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
static int vt_probe(vt_driver *pdev, int instance) {
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
		printf("vt6105: Fail to get PCI BAR\n");
		return -EIO;
	}
	if (ioflag) {
		printf("vt6105: PCI BAR is not for memory\n");
		return -EIO;
	}
	if ((reg = vm_map_phys(SELF, (void *)base, size)) == MAP_FAILED) {
		printf("vt6105: Fail to map hardware registers from PCI\n");
		return -EIO;
	}
	pdev->base_addr = (u32_t)reg;
#else
	bar = pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
	if (bar < 0x400) {
		printf("vt6105: Base address is not properly configured\n");
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
	printf("vt6105: Base address is 0x%08x\n", pdev->base_addr);
	printf("vt6105: IRQ number is 0x%02x\n", pdev->irq);
	printf("vt6105: Revision ID is 0x%02x\n", pdev->revision);
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
	rx_desc_size = RX_DESC_NUM * sizeof(vt_desc);
	tx_desc_size = TX_DESC_NUM * sizeof(vt_desc);

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
		printf("vt6105: Fail to allocate memory\n");
		return -ENOMEM;
	}
	pdev->buf_size = tot_buf_size;
	pdev->buf = buf;

	/* Rx descriptor */
	pdev->rx_desc = (vt_desc *)buf;
	pdev->rx_desc_dma = buf_dma;
	memset(buf, 0, rx_desc_size);
	buf += rx_desc_size;
	buf_dma += rx_desc_size;

	/* Tx descriptor */
	pdev->tx_desc = (vt_desc *)buf;
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
		vt_init_rx_desc(desc, rx_buf_size, pdev->rx[i].buf_dma);
		if (i == (RX_DESC_NUM - 1))
			desc->next = pdev->rx_desc_dma;
		else {
			next += sizeof(vt_desc);
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
		vt_init_tx_desc(desc, tx_buf_size, pdev->tx[i].buf_dma);
		if (i == (TX_DESC_NUM - 1))
			desc->next = pdev->tx_desc_dma;
		else {
			next += sizeof(vt_desc);
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
static int vt_init_hw(vt_driver *pdev, ether_addr_t *addr) {
	int r, ret;

	/* Set the OS interrupt handler */
	pdev->hook = pdev->irq;
	if ((r = sys_irqsetpolicy(pdev->irq, 0, &pdev->hook)) != OK) {
		printf("vt6105: Fail to set OS IRQ policy: %d\n", r);
		ret = -EFAULT;
		goto err_irq_policy;
	}

	/* Reset hardware */
	if (vt_reset_hw(pdev)) {
		printf("vt6105: Fail to reset the device\n");
		ret = -EIO;
		goto err_reset_hw;
	}

	/* Enable OS IRQ */
	if ((r = sys_irqenable(&pdev->hook)) != OK) {
		printf("vt6105: Fail to enable OS IRQ: %d\n", r);
		ret = -EFAULT;
		goto err_irq_enable;
	}

	/* Configure MAC address */
	vt_conf_addr(pdev, addr);

	/* Detect link status */
	pdev->link = vt_check_link(pdev->base_addr);
#ifdef MY_DEBUG
	if (pdev->link)
		printf("vt6105: Link up\n");
	else
		printf("vt6105: Link down\n");
#endif

	return 0;

err_reset_hw:
err_irq_enable:
err_irq_policy:
	return ret;
}

/* Reset hardware */
static int vt_reset_hw(vt_driver *pdev) {
	u32_t base = pdev->base_addr;
	int ret;

	/* Reset the chip */
	/* ### RESET_HARDWARE_CAN_FAIL ### */
	if (vt_real_reset(base)) {
		printf("vt6105: Fail to reset the hardware\n");
		ret = -EIO;
		goto err_real_reset;
	}

	/* Initialize power */
	/* ### POWER_INIT_CAN_FAIL ### */
	if (vt_init_power(base)) {
		printf("vt6105: Fail to initialize power\n");
		ret = -EIO;
		goto err_init_power;
	}

	/* Initialize MII interface */
	/* ### MII_INIT_CAN_FAIL ### */
	if (vt_init_mii(base)) {
		printf("vt6105: Fail to initialize MII interface\n");
		ret = -EIO;
		goto err_init_mii;
	}

	/* Initialize hardware I/O registers */
	/* ### SET_RX_DESC_REG ### */
	if (vt_init_io(base)) {
		printf("vt6105: Fail to initialize I/O registers\n");
		ret = -EIO;
		goto err_init_io;
	}

	/* Set Rx/Tx descriptor into register */
	/* ### SET_RX_DESC_REG ### */
	vt_out32(base, REG_RX_DESC_BASEL, pdev->rx_desc_dma);
#ifdef DESC_BASE_DMA64
	vt_out32(base, REG_RX_DESC_BASEU, 0x00000000);
#endif
	/* ### SET_TX_DESC_REG ### */
	vt_out32(base, REG_TX_DESC_BASEL, pdev->tx_desc_dma);
#ifdef DESC_BASE_DMA64
	vt_out32(base, REG_TX_DESC_BASEU, 0x00000000);
#endif

	/* Enable interrupts */
	/* ### ENABLE_INTR ### */
	vt_out16(base, REG_IMR, INTR_IMR_ENABLE);

	/* Start the device, Rx and Tx */
	/* ### START_RX_TX ### */
	vt_start_rx_tx(base);

	return 0;

err_init_io:
err_init_mii:
err_init_power:
err_real_reset:
	return ret;
}

/* Configure MAC address */
static void vt_conf_addr(vt_driver *pdev, ether_addr_t *addr) {
	u8_t pa[6];
	u32_t base = pdev->base_addr;

	/* Get MAC address */
	/* ### GET_MAC_ADDR ### */
	vt_get_addr(base, pa);
	addr->ea_addr[0] = pa[0];
	addr->ea_addr[1] = pa[1];
	addr->ea_addr[2] = pa[2];
	addr->ea_addr[3] = pa[3];
	addr->ea_addr[4] = pa[4];
	addr->ea_addr[5] = pa[5];
#ifdef MY_DEBUG
	printf("vt6105: Ethernet address is %02x:%02x:%02x:%02x:%02x:%02x\n",
			addr->ea_addr[0], addr->ea_addr[1], addr->ea_addr[2],
			addr->ea_addr[3], addr->ea_addr[4], addr->ea_addr[5]);
#endif
}

/* Stop the driver */
static void vt_stop(void) {
	u32_t base = g_driver.base_addr;

	/* Free Rx and Tx buffer*/
	free_contig(g_driver.buf, g_driver.buf_size);

	/* Stop interrupt */
	/* ### DISABLE_INTR ### */
	vt_out16(base, REG_IMR, INTR_IMR_DISABLE);

	/* Stop Rx/Tx */
	/* ### STOP_RX_TX ### */
	vt_stop_rx_tx(base);
}

/* Set driver mode */
static void vt_mode(unsigned int mode) {
	vt_driver *pdev = &g_driver;
	u32_t base = pdev->base_addr;
	u8_t rcr;

	pdev->mode = mode;

	/* ### READ_RCR ### */
	rcr = vt_in8(base, REG_RCR);
	rcr &= ~(RCR_UNICAST | RCR_MULTICAST | RCR_BROADCAST);
	if (pdev->mode & NDEV_PROMISC)
		rcr |= RCR_UNICAST | RCR_MULTICAST;
	if (pdev->mode & NDEV_BROAD)
		rcr |= RCR_BROADCAST;
	if (pdev->mode & NDEV_MULTI)
		rcr |= RCR_MULTICAST;
	rcr |= RCR_UNICAST;
	/* ### WRITE_RCR ### */
	vt_out8(base, REG_RCR, rcr);
}

/* Receive data */
static ssize_t vt_recv(struct netdriver_data *data, size_t max) {
	vt_driver *pdev = &g_driver;
	u32_t totlen, packlen;
	vt_desc *desc;
	int index, i;

	index = pdev->rx_head;
	desc = pdev->rx_desc;
	desc += index;

	/* Check whether the receiving is OK */
	/* ### CHECK_RX_STATUS_OK ### */
	if (vt_rx_status_ok(desc) != TRUE)
		return SUSPEND;

	/* Check Rx status error */
	/* ### CHECK_RX_STATUS_ERROR ### */
	if (desc->status & DESC_STATUS_RX_RECV_ERR)
		printf("vt6105: Rx error\n");

	/* Get data length */
	/* ### Get Rx data length ### */
	totlen = vt_get_rx_len(desc);
	if (totlen < 8 || totlen > 2 * ETH_MAX_PACK_SIZE) {
		printf("vt6105: Bad data length: %d\n", totlen);
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
	printf("vt6105: Successfully receive a packet, length = %d\n", packlen);
#endif

	return packlen;
}

/* Transmit data */
static int vt_send(struct netdriver_data *data, size_t size) {
	vt_driver *pdev = &g_driver;
	vt_desc *desc;
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
	vt_tx_desc_start(desc, size);
	if (tx_head == TX_DESC_NUM - 1)
		tx_head = 0;
	else
		tx_head++;
	pdev->tx_head = tx_head;

	/* Wake up transmit channel */
	/* ### WAKE_UP_TX ### */
	vt_wakeup_tx(base);

	return 0;
}

/* Handle Interrupt */
static void vt_intr(unsigned int mask) {
	int s;

	/* Run interrupt handler at driver level */
	vt_handler(&g_driver);

	/* Reenable interrupts for this hook */
	if ((s = sys_irqenable(&g_driver.hook)) != OK)
		printf("vt6105: Cannot enable OS interrupts: %d\n", s);

	/* Perform tasks based on the flagged conditions */
	vt_check_ints(&g_driver);
}

/* Real handler interrupt */
static void vt_handler(vt_driver *pdev) {
	u32_t base = pdev->base_addr;
	u16_t intr_status;
	int flag = 0, tx_head, tx_tail;
	vt_desc *desc;

	/* Get interrupt status */
	/* ### GET_INTR_STATUS ### */
	intr_status = vt_in16(base, REG_ISR);

	/* Clear interrupt */
	/* ### CLEAR_INTR ### */
	vt_out16(base, REG_ISR, intr_status & INTR_ISR_CLEAR);

	/* Enable interrupt */
	/* ### ENABLE_INTR ### */
	vt_out16(base, REG_IMR, INTR_IMR_ENABLE);

	/* Check interrupt error */
	/* ### CHECK_INTR_ERROR ### */
	if (intr_status & INTR_ISR_ERR) {
		printf("vt6105: interrupt error\n");
		return;
	}

	/* Check link status */
	/* ### CHECK_LINK_INTR ### */
	if (intr_status & INTR_ISR_LINK_EVENT) {
		pdev->link = vt_check_link(base);
#ifdef MY_DEBUG
		printf("vt6105: Link state change\n");
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
				printf("vt6105: Strange, buffer not busy?\n");

			/* Check whether the transmiting is OK */
			/* ### CHECK_TX_STATUS_OK ### */
			if (vt_tx_status_ok(desc) != TRUE)
				break;

			/* Check Tx status error */
			/* ### CHECK_TX_STATUS_ERROR ### */
			if (desc->status & DESC_STATUS_TX_SEND_ERR)
				printf("vt6105: Tx error\n");

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
			printf("vt6105: Successfully send a packet\n");
#endif
		}
		pdev->tx_tail = tx_tail;
	}
#ifdef MY_DEBUG
	if (!flag) {
		printf("vt6105: Unknown error in interrupt\n");
		return;
	}
#endif
}

/* Check interrupt and perform */
static void vt_check_ints(vt_driver *pdev) {
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

static void vt_stat(eth_stat_t *stat) {
	memcpy(stat, &g_driver.stat, sizeof(*stat));
}
