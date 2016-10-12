#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <machine/pci.h>
#include <sys/mman.h>
#include "ip1000.h"
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
static u16_t read_eeprom(u32_t base, int addr) {
	u32_t ret, data, val;
	int i;

	val = EC_READ | (addr & 0xff);
	ndr_out16(base, REG_EEPROM_CTRL, val);
	for (i = 0; i < 100; i++) {
		micro_delay(10000);
		data = ndr_in16(base, REG_EEPROM_CTRL);
		if (!(data & EC_BUSY)) {
			ret = ndr_in16(base, REG_EEPROM_DATA);
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
	field[3] = phy_addr;		fieldlen[3] = 5;
	field[4] = phy_reg;		fieldlen[4] = 5;
	field[5] = 0x0000;		fieldlen[5] = 2;
	field[6] = 0x0000;		fieldlen[6] = 16;
	field[7] = 0x0000;		fieldlen[7] = 1;

	polar = ndr_in8(base, REG_PHY_CTRL) & 0x28;
	for (i = 0; i < 5; i++) {
		for (j = 0; j < fieldlen[i]; j++) {
			data = (field[i] >> (fieldlen[i] - j - 1)) << 1;
			data = (0x02 & data) | (0x04 | polar);
			ndr_out8(base, REG_PHY_CTRL, data);
			micro_delay(10);
			ndr_out8(base, REG_PHY_CTRL, (data | 0x01));
			micro_delay(10);
		}
	}
	ndr_out8(base, REG_PHY_CTRL, (polar | 0x04));
	micro_delay(10);
	ndr_out8(base, REG_PHY_CTRL, (polar | 0x05));
	micro_delay(10);
	ndr_out8(base, REG_PHY_CTRL, polar);
	micro_delay(10);
	data = ndr_in8(base, REG_PHY_CTRL);
	ndr_out8(base, REG_PHY_CTRL, (polar | 0x01));
	micro_delay(10);
	for (i = 0; i < fieldlen[6]; i++) {
		ndr_out8(base, REG_PHY_CTRL, polar);
		micro_delay(10);
		data = ((ndr_in8(base, REG_PHY_CTRL) & 0x02) >> 1) & 0x01;
		ndr_out8(base, REG_PHY_CTRL, (polar | 0x01));
		micro_delay(10);
		field[6] |= (data << (fieldlen[6] - i - 1));
	}

	for (i = 0; i < 3; i++) {
		ndr_out8(base, REG_PHY_CTRL, (polar | 0x04));
		micro_delay(10);
		ndr_out8(base, REG_PHY_CTRL, (polar | 0x05));
		micro_delay(10);
	}
	ndr_out8(base, REG_PHY_CTRL, (polar | 0x04));
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

	polar = ndr_in8(base, REG_PHY_CTRL) & 0x28;
	for (i = 0; i < 7; i++) {
		for (j = 0; j < fieldlen[i]; j++) {
			data = (field[i] >> (field[i] - j - 1)) << 1;
			data = (0x02 & data) | (0x04 | polar);
			ndr_out8(base, REG_PHY_CTRL, data);
			micro_delay(10);
			ndr_out8(base, REG_PHY_CTRL, (data | 0x01));
			micro_delay(10);
		}
	}
	for (i = 0; i < fieldlen[7]; i ++) {
		ndr_out8(base, REG_PHY_CTRL, polar);
		micro_delay(10);
		field[7] |= ((ndr_in8(base, REG_PHY_CTRL) & 0x02) >> 1)
						<< (fieldlen[7] - i -1);
		ndr_out8(base, REG_PHY_CTRL, (polar | 0x01));
		micro_delay(10);
	}
}

/* ====== Developer interface ======*/
/* Real hardware reset (### RESET_HARDWARE_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int dev_real_reset(u32_t *base) {
	u32_t data, base0 = base[0];
	data = ndr_in32(base0, REG_ASIC_CTRL);
	ndr_out32(base0, REG_ASIC_CTRL, data | AC_RESET_ALL);
	micro_delay(5000);
	if (ndr_in32(base0, REG_ASIC_CTRL) & AC_RESET_BUSY)
		return -EIO;
	return OK;
}

/* Intialize other hardware I/O registers (### INIT_HARDWARE_IO_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int dev_init_io(u32_t *base) {
	u32_t mac_ctrl, physet, mode0, mode1, base0 = base[0];
	mode0 = read_eeprom(base0, 6);
	mode1 = ndr_in16(base0, REG_ASIC_CTRL);
	mode1 &= ~(AC_LED_MODE_B1 | AC_LED_MODE | AC_LED_SPEED);
	if ((mode0 & 0x03) > 1)
		mode1 |= AC_LED_MODE_B1;
	if ((mode0 & 0x01) == 1)
		mode1 |= AC_LED_MODE;
	if ((mode0 & 0x08) == 8)
		mode1 |= AC_LED_SPEED;
	ndr_out32(base0, REG_ASIC_CTRL, mode1);
	physet = ndr_in8(base0, REG_PHY_SET);
	physet = (physet & 0xf8) | ((mode0 & 0x70) >> 4);
	ndr_out8(base0, REG_PHY_SET, physet);
	mac_ctrl = ndr_in32(base0, REG_MAC_CTRL);
	mac_ctrl |= (MC_STAT_DISABLE | MC_TX_FC_ENA | MC_RX_FC_ENA);
	ndr_out32(base0, REG_MAC_CTRL, 0);
	ndr_out16(base0, REG_MAX_FRAME, RX_BUF_SIZE);
	ndr_out8(base0, REG_RX_DMA_PERIOD, 0x01);
	ndr_out8(base0, REG_RX_DMA_UTH, 0x30);
	ndr_out8(base0, REG_RX_DMA_BTH, 0x30);
	ndr_out8(base0, REG_TX_DMA_PERIOD, 0x26);
	ndr_out8(base0, REG_TX_DMA_UTH, 0x04);
	ndr_out8(base0, REG_TX_DMA_BTH, 0x30);
	ndr_out16(base0, REG_FLOW_ON_TH, 0x0740);
	ndr_out16(base0, REG_FLOW_OFF_TH, 0x00bf);
	ndr_out32(base0, REG_MAC_CTRL, mac_ctrl);
	return OK;
}

/* Intialize MII interface (### MII_INIT_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int dev_init_mii(u32_t *base) {
	int i, phyaddr;
	u8_t revision;
	u16_t phyctrl, cr1000, length, address, value;
	u16_t *param;
	u32_t status, base0 = base[0];

	for (i = 0; i < 32; i++) {
		phyaddr = (i + 0x01) % 32;
		status = read_phy_reg(base0, phyaddr, 0x01);
		if ((status != 0xffff) && (status != 0))
			break;
	}
	if (i == 32)
		return -EIO;
	if (phyaddr != -1) {
		cr1000 = read_phy_reg(base0, phyaddr, 0x09);
		cr1000 |= 0x0700;
		write_phy_reg(base0, phyaddr, 0x09, cr1000);
		phyctrl = read_phy_reg(base0, phyaddr, 0x00);
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
				write_phy_reg(base0, phyaddr, address, value);
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
	write_phy_reg(base0, phyaddr, 0x00, phyctrl | 0x8200);
	return OK;
}

/* Enable or disable interrupt (### INTR_ENABLE_DISABLE ###) */
static void dev_intr_control(u32_t *base, int flag) {
	u32_t base0 = base[0];
	if (flag == INTR_ENABLE)
		ndr_out16(base0, REG_IMR, CMD_INTR_ENABLE);
	else if (flag == INTR_DISABLE)
		ndr_out16(base0, REG_IMR, 0);
}

/* Enable or disable Rx/Tx (### RX_TX_ENABLE_DISABLE ###) */
static void dev_rx_tx_control(u32_t *base, int flag) {
	u32_t data, base0 = base[0];
	data = ndr_in32(base0, REG_MAC_CTRL);
	if (flag == RX_TX_ENABLE)
		ndr_out32(base0, REG_MAC_CTRL, data | (MC_RX_ENABLE | MC_TX_ENABLE));
	else if (flag == RX_TX_DISABLE) {
		ndr_out32(base0, REG_MAC_CTRL, 0);
		ndr_out32(base0, REG_ASIC_CTRL, AC_RESET_ALL);
	}
}

/* Get MAC address to the array 'pa' (### GET_MAC_ADDR ###) */
static void dev_get_addr(u32_t *base, u8_t *pa) {
	u32_t i, sta_addr[3], base0 = base[0];
	for (i = 0; i < 3; i++)	 {
		sta_addr[i] = read_eeprom(base0, 16 + i);
		ndr_out16(base0, (REG_STA_ADDR0 + i * 2), sta_addr[i]);
	}
	pa[0] = (u8_t)(ndr_in16(base0, REG_STA_ADDR0) & 0x00ff);
	pa[1] = (u8_t)((ndr_in16(base0, REG_STA_ADDR0) & 0xff00) >> 8);
	pa[2] = (u8_t)(ndr_in16(base0, REG_STA_ADDR1) & 0x00ff);
	pa[3] = (u8_t)((ndr_in16(base0, REG_STA_ADDR1) & 0xff00) >> 8);
	pa[4] = (u8_t)(ndr_in16(base0, REG_STA_ADDR2) & 0x00ff);
	pa[5] = (u8_t)((ndr_in16(base0, REG_STA_ADDR2) & 0xff00) >> 8);
}

/* Check link status (### CHECK_LINK ###)
 * -- Return LINK_UP or LINK_DOWN */
static int dev_check_link(u32_t *base) {
	u32_t phy_ctrl, mac_ctrl, base0 = base[0];
	int ret;
	char speed[20], duplex[20];

	phy_ctrl = ndr_in8(base0, REG_PHY_CTRL);
	mac_ctrl = ndr_in8(base0, REG_MAC_CTRL);
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
	ndr_out32(base0, REG_MAC_CTRL, mac_ctrl);
#ifdef MY_DEBUG
	printf("NDR: Link speed is %s, %s duplex\n", speed, duplex);
#endif
	return ret;
}

/* Set driver receive mode (### SET_REC_MODE ###) */
static void dev_set_rec_mode(u32_t *base, int mode) {
	u32_t data, base0 = base[0];
	data = ndr_in8(base0, REG_RCR);
	data &= ~(CMD_RCR_UNICAST | CMD_RCR_MULTICAST | CMD_RCR_BROADCAST);
	if (mode & NDEV_MODE_PROMISC)
		data |= CMD_RCR_UNICAST | CMD_RCR_MULTICAST | CMD_RCR_MULTICAST;
	if (mode & NDEV_MODE_BCAST)
		data |= CMD_RCR_BROADCAST;
	if (mode & (NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL))
		data |= CMD_RCR_MULTICAST;
	data |= CMD_RCR_UNICAST;
	ndr_out8(base0, REG_RCR, data);
}

/* Start Tx channel (### START_TX_CHANNEL ###) */
static void dev_start_tx(u32_t *base) {
	u32_t base0 = base[0];
	ndr_out32(base0, REG_DMA_CTRL, CMD_TX_START);
}

/* Read and clear interrupt (### READ_CLEAR_INTR_STS ###) */
static u32_t dev_read_clear_intr_status(u32_t *base) {
	u32_t data, base0 = base[0];
	data = ndr_in16(base0, REG_ISR);
	ndr_out16(base0, REG_ISR, 0);
	return data;
}

/* ---------- WITH DESCRIPTOR ---------- */
/* Intialize Rx descriptor (### INIT_RX_DESC ###) */
static void dev_init_rx_desc(NDR_desc *desc_start, int index, size_t buf_size,
			phys_bytes buf_dma, int max_desc_num, phys_bytes desc_dma_start) {
	NDR_desc *desc = desc_start + index;
	desc->status = 0;
	desc->frag_info = (u64_t)(buf_dma);
	desc->frag_info |= ((u64_t)buf_size << 48) & RFI_FRAG_LEN;
	if (index == max_desc_num - 1)
		desc->next = desc_dma_start;
	else
		desc->next = desc_dma_start + (index + 1) * sizeof(NDR_desc);
}

/* Intialize Tx descriptor (### INIT_TX_DESC ###) */
static void dev_init_tx_desc(NDR_desc *desc_start, int index, size_t buf_size,
			phys_bytes buf_dma, int max_desc_num, phys_bytes desc_dma_start) {
	NDR_desc *desc = desc_start + index;
	desc->status = TFS_TFD_DONE;
	desc->frag_info = (u64_t)(buf_dma);
	if (index == max_desc_num - 1)
		desc->next = desc_dma_start;
	else
		desc->next = desc_dma_start + (index + 1) * sizeof(NDR_desc);
}

/* Set Rx/Tx descriptor address into device register (### SET_DESC_REG ###) */
static void dev_set_desc_reg(u32_t *base, phys_bytes rx_addr,
								phys_bytes tx_addr) {
	u32_t base0 = base[0];
	ndr_out32(base0, REG_RX_DESC_BASEL, rx_addr);
	ndr_out32(base0, REG_RX_DESC_BASEU, 0);
	ndr_out32(base0, REG_TX_DESC_BASEL, tx_addr);
	ndr_out32(base0, REG_TX_DESC_BASEU, 0);
}

/* Check whether Rx is OK from Rx descriptor (### CHECK_RX_OK_FROM_DESC ###)
 * -- Current buffer number is index
 * -- Return RX_OK or RX_SUSPEND or RX_ERROR */
static int dev_rx_ok_desc(u32_t *base, NDR_desc *desc, int index) {
	if (desc->status & RFS_RFD_DONE) {
		if (desc->status & RFS_ERROR)
			return RX_ERROR;
		if ((desc->status & RFS_NORMAL) == RFS_NORMAL)
			return RX_OK;
	}
	return RX_SUSPEND;
}

/* Get length from Rx descriptor (### GET_RX_LENGTH_FROM_DESC ###)
 * -- Current buffer number is index
 * -- Return the length */
static int dev_rx_len_desc(u32_t *base, NDR_desc *desc, int index) {
	int totlen;
	totlen = (int)(desc->status & RFS_FRAME_LEN);
	return totlen;
}

/* Set Rx descriptor after Rx done (### SET_RX_DESC_DONE ###)
 * -- Current buffer number is index */
static void dev_set_rx_desc_done(u32_t *base, NDR_desc *desc, int index) {
	desc->status = 0;
}

/* Set Tx descriptor to prepare transmitting (### SET_TX_DESC_PREPARE)
 * -- Current buffer number is index */
static void dev_set_tx_desc_prepare(u32_t *base, NDR_desc *desc, int index,
									size_t data_size) {
	desc->status = TFS_TFD_DONE;
	desc->status |= (u64_t)(TFS_WORD_ALIGN | (TFS_FRAMEID & index)
					| (TFS_FRAG_COUNT & (1 << 24))) | TFS_TX_DMA_INDICATE;
	desc->frag_info |= TFI_FRAG_LEN & ((u64_t)((data_size > 60 ? data_size : 60)
							& 0xffff) << 48);
	desc->status &= (u64_t)(~(TFS_TFD_DONE));
}

/* Check whether Tx is OK from Tx descriptor (### CHECK_TX_OK_FROM_DESC ###)
 * -- Current buffer number is index
 * -- Return TX_OK or TX_SUSPEND or TX_ERROR */
static int dev_tx_ok_desc(u32_t *base, NDR_desc *desc, int index) {
	if (desc->status & TFS_TFD_DONE)
		return TX_OK;
	return TX_SUSPEND;
}

/* Set Tx descriptor after Tx done (### SET_TX_DESC_DONE ###)
 * -- Current buffer number is index */
static void dev_set_tx_desc_done(u32_t *base, NDR_desc *desc, int index) {
	desc->status = 0;
}

/* Driver interface table */
static const struct netdriver NDR_table = {
	.ndr_name = "stge",
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
