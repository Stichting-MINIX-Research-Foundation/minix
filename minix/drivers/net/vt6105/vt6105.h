#ifndef _NDR_H
#define _NDR_H

/* ======= General Parameter ======= */
/* Global configure */

#include <minix/drivers.h>

#define DRIVER_NAME		"VT6105"

/* Rx/Tx buffer parameter */
#define RX_BUF_SIZE		1536
#define TX_BUF_SIZE		1536
#define RX_BUFFER_NUM	64
#define TX_BUFFER_NUM	64

/* Interrupt status */
#define INTR_STS_LINK	0x4000
#define INTR_STS_RX		0x0001
#define INTR_STS_TX		0x0002

/* Link status */
#define LINK_UP			1
#define LINK_DOWN		0
#define LINK_UNKNOWN	-1

/* Interrupt control */
#define INTR_ENABLE		1
#define INTR_DISABLE	0

/* Rx status */
#define RX_ERROR		1
#define RX_OK			0
#define RX_SUSPEND		-1

/* Tx status */
#define TX_ERROR		1
#define TX_OK			0
#define TX_SUSPEND		-1

/* Rx/Tx control */
#define RX_TX_ENABLE	1
#define RX_TX_DISABLE	0

/* ======= Self-defined Parameter ======= */
#define DESC_OWN			0x80000000
#define DESC_FIRST			0x00000200
#define DESC_LAST			0x00000100
#define DESC_RX_LENMASK		0x7fff0000
#define DESC_RX_ERROR		0x000000bf
#define DESC_RX_NORMAL		(DESC_FIRST | DESC_LAST)
#define DESC_TX_ERROR		0x00008f10

#define REG_ADDR			0x00
#define REG_RCR				0x06
#define REG_TCR				0x07
#define REG_CR				0x08
#define REG_IMR				0x0e
#define REG_ISR				0x0c
#define REG_RX_DESC_BASE	0x18
#define REG_TX_DESC_BASE	0x1c
#define REG_MII_PHY			0x6c
#define REG_BCR				0x6e
#define REG_MII_CR			0x70
#define REG_MII_REG			0x71
#define REG_MII_DATA		0x72
#define REG_MCR				0x81
#define REG_STICK			0x83

#define CMD_START			0x0002
#define CMD_STOP			0x0004
#define CMD_RX_ON			0x0008
#define CMD_TX_ON			0x0010
#define CMD_TX_DEMAND		0x0020
#define CMD_RX_DEMAND		0x0040
#define CMD_FDUPLEX			0x0400
#define CMD_NO_POLL			0x0800
#define CMD_RESET			0x8000
#define CMD_INTR_ENABLE		0xfeff
#define CMD_RCR_UNICAST		0x10
#define CMD_RCR_MULTICAST	0x04
#define CMD_RCR_BROADCAST	0x08
#define INTR_STS_CLEAR		0xbfbf
#define LINK_STATUS			0x0004

/* ======= Data Descriptor ======= */
typedef struct NDR_desc {
	u32_t status;
	u32_t length;
	u32_t addr;
	u32_t next;
} NDR_desc;

/* Driver Data Structure */
typedef struct NDR_driver {
	char *dev_name;			/* Device name */
	u16_t vid, did;			/* Vendor and device ID */
	u32_t devind;			/* Device index */
	u32_t base[6];			/* Base address */
	char irq;				/* IRQ number */
	char revision;			/* Revision ID */

	int mode;
	int link;				/* Whether link-up */
	int recv_flag;			/* Receive flag */
	int send_flag;			/* Send flag */
	int tx_busy;			/* Whether Tx is busy */

	/* Buffer */
	size_t buf_size;
	char *buf;

	/* Rx data */
	int rx_head;
	struct {
		phys_bytes buf_dma;
		char *buf;
	} rx[RX_BUFFER_NUM];

	/* Tx data */
	int tx_head;
	int tx_tail;
	struct {
		int busy;
		phys_bytes buf_dma;
		char *buf;
	} tx[TX_BUFFER_NUM];
	int tx_busy_num;			/* Number of busy Tx buffer */

	NDR_desc *rx_desc;			/* Rx descriptor buffer */
	phys_bytes rx_desc_dma;		/* Rx descriptor DMA buffer */
	NDR_desc *tx_desc;			/* Tx descriptor buffer */
	phys_bytes tx_desc_dma;		/* Tx descriptor DMA buffer */

	int hook;			/* IRQ hook id at kernel */
} NDR_driver;

#endif
