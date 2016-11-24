#ifndef _vt_H
#define _vt_H

#include <minix/drivers.h>

/* ======= General Parameter ======= */
/* Global configure */
#define DESC_BASE64

/* Key internal register */
#define REG_RCR				0x06
#define REG_ISR				0x0c
#define REG_IMR				0x0e
#define REG_RX_DESC_BASEL	0x18
#define REG_TX_DESC_BASEL	0x1c

#ifdef DESC_BASE_DMA64
#define REG_RX_DESC_BASEU	0
#define REG_TX_DESC_BASEU	0
#endif

/* Key internal register width */
#define WIDTH_REG_RCR		8
#define WIDTH_REG_ISR		16
#define WIDTH_REG_IMR		16

/* Interrupt statu and command */
#define INTR_ISR_ERR			0x3ffc
#define INTR_ISR_LINK_EVENT		0x4000
#define INTR_ISR_RX_DONE		0x0001
#define INTR_ISR_TX_DONE		0x0002
#define INTR_ISR_CLEAR			0xbfbf
#define INTR_IMR_ENABLE			0xfeff
#define INTR_IMR_DISABLE		0x0000

/* Descriptor status */
#define DESC_STATUS_RX_RECV_ERR		0x000000bf
#define DESC_STATUS_RX_RECV_CLEAR	0x80000000
#define DESC_STATUS_TX_SEND_ERR		0x00008f10
#define DESC_STATUS_TX_SEND_CLEAR	0x00000000

/* Rx mode */
#define RCR_UNICAST		0x10
#define RCR_MULTICAST	0x04
#define RCR_BROADCAST	0x08

/* Link status */
#define LINK_UP			1
#define LINK_DOWN		0
#define LINK_UNKNOWN	-1

/* Basic Rx/Tx parameters */
#define RX_BUF_SIZE		1536
#define TX_BUF_SIZE		1536
#define RX_DESC_NUM		64
#define TX_DESC_NUM		64

/* ======= Self-defined Parameter ======= */
#define DESC_OWN			0x80000000
#define DESC_FIRST			0x00000200
#define DESC_LAST			0x00000100

#define DESC_RX_LENMASK		0x7fff0000
#define DESC_RX_OK			0x00008000
#define DESC_RX_NORMAL		(DESC_FIRST | DESC_LAST)

#define REG_ADDR		0x00
#define REG_TCR			0x07
#define REG_CR			0x08
#define REG_BCR0		0x6e
#define REG_MCR1		0x81
#define REG_STICK		0x83
#define REG_MII_CFG		0x6c
#define REG_MII_CR		0x70
#define REG_MII_ADDR	0x71
#define REG_MII_DATA	0x72
#define REG_WOL_SET		0xa4
#define REG_WOLC_SET	0xa7

#define CMD_START		0x0002
#define CMD_STOP		0x0004
#define CMD_RX_ON		0x0008
#define CMD_TX_ON		0x0010
#define CMD_TX_DEMAND	0x0020
#define CMD_RX_DEMAND	0x0040
#define CMD_FDUPLEX		0x0400
#define CMD_NO_POLL		0x0800
#define CMD_RESET		0x8000

/* ======= Data Descriptor ======= */
typedef struct vt_desc {
	u32_t status;
	u32_t length;
	u32_t addr;
	u32_t next;
} vt_desc;

/* Driver Data Structure */
typedef struct vt_driver {
	u32_t base_addr;		/* Base address */
	int revision;			/* Revision ID */
	int irq;				/* IRQ number */
	int mode;
	int link;				/* Whether link-up */
	int recv_flag;			/* Receive flag */
	int send_flag;			/* Send flag */
	int tx_busy;

	/* Buffer */
	size_t buf_size;
	char *buf;

	/* Rx data */
	int rx_head;
	struct {
		phys_bytes buf_dma;
		char *buf;
	} rx[RX_DESC_NUM];
	vt_desc *rx_desc;			/* Rx descriptor buffer */
	phys_bytes rx_desc_dma;		/* Rx descriptor DMA buffer */

	/* Tx data */
	int tx_head;
	int tx_tail;
	struct {
		int busy;
		phys_bytes buf_dma;
		char *buf;
	} tx[TX_DESC_NUM];
	vt_desc *tx_desc;			/* Tx descriptor buffer */
	phys_bytes tx_desc_dma;		/* Tx descriptor DMA buffer */
	int tx_busy_num;			/* Number of busy Tx descriptors */

	int hook;		/* IRQ hook id at kernel */
	eth_stat_t stat;

	char name[20];
} vt_driver;

#endif
