#ifndef _NDR_H
#define _NDR_H

/* ======= General Parameter ======= */
/* Global configure */

#include <minix/drivers.h>

#define DRIVER_NAME		"IP1000"

/* Rx/Tx buffer parameter */
#define RX_BUF_SIZE		1536
#define TX_BUF_SIZE		1536
#define RX_BUFFER_NUM	64
#define TX_BUFFER_NUM	64

/* Interrupt status */
#define INTR_STS_LINK	0x0100
#define INTR_STS_RX		0x0400
#define INTR_STS_TX		0x0200

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
#define RFI_FRAG_LEN		0xffff000000000000ULL
#define RFS_FRAME_LEN		0x000000000000ffffULL
#define RFS_FRAME_START		0x0000000020000000ULL
#define RFS_FRAME_END		0x0000000040000000ULL
#define RFS_RFD_DONE		0x0000000080000000ULL
#define RFS_ERROR			0x00000000003f0000ULL
#define RFS_NORMAL			(RFS_RFD_DONE | RFS_FRAME_START | RFS_FRAME_END)

#define TFI_FRAG_LEN		0xffff000000000000ULL
#define TFS_FRAMEID			0x000000000000ffffULL
#define TFS_WORD_ALIGN		0x0000000000030000ULL
#define TFS_TX_DMA_INDICATE	0x0000000000800000ULL
#define TFS_FRAG_COUNT		0x000000000f000000ULL
#define TFS_TFD_DONE		0x0000000080000000ULL

#define REG_DMA_CTRL		0x00
#define REG_TX_DESC_BASEL	0x10
#define REG_TX_DESC_BASEU	0x14
#define REG_TX_DMA_BTH		0x18
#define REG_TX_DMA_UTH		0x19
#define REG_TX_DMA_PERIOD	0x1a
#define REG_RX_DESC_BASEL	0x1c
#define REG_RX_DESC_BASEU	0x20
#define REG_RX_DMA_BTH		0x24
#define REG_RX_DMA_UTH		0x25
#define REG_RX_DMA_PERIOD	0x26
#define REG_ASIC_CTRL		0x30
#define REG_FLOW_OFF_TH		0x3c
#define REG_FLOW_ON_TH		0x3e
#define REG_EEPROM_DATA		0x48
#define REG_EEPROM_CTRL		0x4a
#define REG_ISR				0x5a
#define REG_IMR				0x5c
#define REG_MAC_CTRL		0x6c
#define REG_PHY_SET			0x75
#define REG_PHY_CTRL		0x76
#define REG_STA_ADDR0		0x78
#define REG_STA_ADDR1		0x7a
#define REG_STA_ADDR2		0x7c
#define REG_MAX_FRAME		0x86
#define REG_RCR				0x88

#define AC_LED_MODE		0x00004000
#define AC_GB_RESET		0x00010000
#define AC_RX_RESET		0x00020000
#define AC_TX_RESET		0x00040000
#define AC_DMA			0x00080000
#define AC_FIFO			0x00100000
#define AC_NETWORK		0x00200000
#define AC_HOST			0x00400000
#define AC_AUTO_INIT	0x00800000
#define AC_RESET_BUSY	0x04000000
#define AC_LED_SPEED	0x08000000
#define AC_LED_MODE_B1	0x20000000
#define AC_RESET_ALL	(AC_GB_RESET | AC_RX_RESET | AC_TX_RESET | AC_DMA | \
						AC_FIFO | AC_NETWORK | AC_HOST | AC_AUTO_INIT)

#define MC_DUPLEX_SEL	0x00000020
#define MC_TX_FC_ENA	0x00000080
#define MC_RX_FC_ENA	0x00000100
#define MC_STAT_DISABLE	0x00400000
#define MC_TX_ENABLE	0x01000000
#define MC_TX_DISABLE	0x02000000
#define MC_RX_ENABLE	0x08000000
#define MC_RX_DISABLE	0x10000000
#define MC_PAUSED		0x40000000

#define PC_DUPLEX_STS		0x10
#define PC_LINK_SPEED		0xc0
#define PC_LINK_SPEED10		0x40
#define PC_LINK_SPEED100	0x80
#define PC_LINK_SPEED1000	0xc0

#define CMD_INTR_ENABLE		0x17e6
#define CMD_RCR_UNICAST		0x01
#define CMD_RCR_MULTICAST	0x02
#define CMD_RCR_BROADCAST	0x04
#define CMD_TX_START		0x1000

#define EC_READ		0x0200
#define EC_BUSY		0x8000

static u16_t PhyParam[] = {
	(0x4000|(07*4)), 31, 0x0001, 27, 0x01e0, 31, 0x0002, 27, 0xeb8e, 31, 0x0000,
					30, 0x005e, 9, 0x0700,
	(0x4100|(07*4)), 31, 0x0001, 27, 0x01e0, 31, 0x0002, 27, 0xeb8e, 31, 0x0000,
					30, 0x005e, 9, 0x0700,
	(0x4200|(07*4)), 31, 0x0001, 27, 0x01e0, 31, 0x0002, 27, 0xeb8e, 31, 0x0000,
					30, 0x005e, 9, 0x0700,
	(0x4300|(07*4)), 31, 0x0001, 27, 0x01e0, 31, 0x0002, 27, 0xeb8e, 31, 0x0000,
					30, 0x005e, 9, 0x0700, 0x0000
};

/* ======= Data Descriptor ======= */
typedef struct NDR_desc {
	u64_t next;
	u64_t status;
	u64_t frag_info;
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
