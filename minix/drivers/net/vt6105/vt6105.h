#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <machine/pci.h>
#include <minix/ds.h>
#include <assert.h>

/* PCI Register */
#define VT_PCI_VID		0x00	/* Vendor ID */
#define VT_PCI_DID		0x02	/* Device ID */
#define VT_PCI_CMD		0x04	/* Command */
#define VT_PCI_STS		0x06	/* Status */
#define VT_PCI_RID		0x08	/* Revision ID */
#define VT_PCI_CC		0x09	/* Class Code */
#define VT_PCI_CLS		0x0c	/* Cache Line Size */
#define VT_PCI_LT		0x0d	/* Latency Timer */
#define VT_PCI_HT		0x0e	/* Header Type */

/* Internal Register */
#define VT_REG_ADDR		0x00	/* Ethernet Address */
#define VT_REG_RCR		0x06	/* Receive Configure Request */
#define VT_REG_TCR		0x07	/* Transmit Configure Request */
#define VT_REG_CR		0x08	/* Control 0 */
#define VT_REG_ISR		0x0c	/* Interrupt Status */
#define VT_REG_IMR		0x0e	/* Interrupt Mask */
#define VT_REG_MAR0		0x10	/* Multicast Address 0 */
#define VT_REG_MAR1		0x14	/* Multicast Address 1 */
#define VT_REG_RD_BASE		0x18	/* Receive Descriptor Base Address */
#define VT_REG_TD_BASE		0x1c	/* Transmit Descriptor Base Address */
#define VT_REG_MII_CFG		0x6c	/* MII Configuration */
#define VT_REG_MII_STS		0x6d	/* MII Status */
#define VT_REG_BCR0		0x6e	/* Bus Control 0 */
#define VT_REG_BCR1		0x6f	/* Bus Control 1 */
#define VT_REG_MII_CR		0x70	/* MII Control */
#define VT_REG_MII_ADDR		0x71	/* MII Interface Address*/
#define VT_REG_MII_DATA		0x72	/* MII Data Port */
#define VT_REG_EECSR	0x74	/* EEPROM Control/Status */
#define VT_REG_CFG_A	0x78	/* Chip Configuration A */
#define VT_REG_CFG_B	0x79	/* Chip Configuration B */
#define VT_REG_CFG_C	0x7a	/* Chip Configuration C */
#define VT_REG_CFG_D	0x7b	/* Chip Configuration D */
#define VT_REG_MCR0		0x80	/* Miscellaneous Control 0 */
#define VT_REG_MCR1		0x81	/* Miscellaneous Control 1 */
#define VT_REG_STICK	0x83	/* Sticky Hardware Control */
#define VT_REG_MISR		0x84	/* MII Interrupt Status */
#define VT_REG_MIMR		0x86	/* MII Interrupt Mask */
#define VT_REG_FCR0		0x98	/* Flow Control 0 */
#define VT_REG_FCR1		0x99	/* Flow Control 1 */
#define VT_REG_WOL_SET		0xA4	/* Wake On LAN Set */
#define VT_REG_WOL_CLR		0xA0	/* Wake On LAN Clear */
#define VT_REG_PCS_CLR		0xA5	/* Power Configuration Set */
#define VT_REG_PCS_SET		0xA1	/* Power Configuration Clear */
#define VT_REG_WOLC_SET		0xA7	/* WOL Configuration Set */
#define VT_REG_WOLC_CLR		0xA3	/* WOL Configuration Clear */
#define VT_REG_WOLS_SET		0xA8	/* WOL Status Clear */
#define VT_REG_WOLS_CLR		0xAC	/* WOL Status Set */

/* Interrupt Status/Mask Register */
#define VT_INTR_RX_DONE			0x0001
#define VT_INTR_TX_DONE			0x0002
#define VT_INTR_RX_ERR			0x0004
#define VT_INTR_TX_ERR			0x0008
#define VT_INTR_RX_EMPTY		0x0020
#define VT_INTR_PCI_ERR			0x0040
#define VT_INTR_STS_MAX			0x0080
#define VT_INTR_RX_EARLY		0x0100
#define VT_INTR_TX_UNDER_RUN	0x0210
#define VT_INTR_RX_OVERFLOW		0x0400
#define VT_INTR_RX_DROPPED		0x0800
#define VT_INTR_RX_NOBUF		0x1000
#define VT_INTR_TX_ABORT		0x2000
#define VT_INTR_PORT_CHANGE		0x4000
#define VT_INTR_RX_WAKE_UP		0x8000

/* Control Register Command */
#define VT_CMD_START		0x0002
#define VT_CMD_STOP			0x0004
#define VT_CMD_RX_ON		0x0008
#define VT_CMD_TX_ON		0x0010
#define VT_CMD_TX_DEMAND	0x0020
#define VT_CMD_RX_DEMAND	0x0040
#define VT_CMD_EARLY_RX		0x0100
#define VT_CMD_EARLY_TX		0x0200
#define VT_CMD_FDUPLEX		0x0400
#define VT_CMD_NO_POLL		0x0800
#define VT_CMD_RESET		0x8000

/* Receive Configure Mode */
#define VT_RCR_AP		0x10
#define VT_RCR_AB		0x08
#define VT_RCR_AM		0x04
#define VT_RCR_AR		0x02
#define VT_RCR_AE		0x01

/* General Descriptor Flag */
#define VT_DESC_OWN		0x80000000	/* Ownership */
#define VT_DESC_FIRST	0x00000200	/* First Descriptor */
#define VT_DESC_LAST	0x00000100	/* Last Descriptor */

/* Rx Descriptor Flag */
#define VT_DESC_RX_LENMASK	0x7fff0000	/* Receive Frame Length Mask */
#define VT_DESC_RX_OK		0x00008000	/* Receive OK */
#define VT_DESC_RX_MAR		0x00002000	/* Multicast Address Received */
#define VT_DESC_RX_BAR		0x00001000	/* Broadcast Address Received */
#define VT_DESC_RX_PHY		0x00000800	/* Physical Address Received */
#define VT_DESC_RX_LINK_ERR	0x00000080	/* Descriptor Link Structure Error */
#define VT_DESC_RX_RUNT		0x00000020	/* Runt Packet */
#define VT_DESC_RX_LONG		0x00000010	/* Long Packet */
#define VT_DESC_RX_FOV		0x00000008	/* FIFO Overflow */
#define VT_DESC_RX_FAE		0x00000004	/* Frame Align Error */
#define VT_DESC_RX_CRCE		0x00000002	/* CRC Error */
#define VT_DESC_RX_ERR		0x00000001	/* Receive Error */
#define VT_DESC_RX_ALL_ERR	(VT_DESC_RX_LINK_ERR | VT_DESC_RX_RUNT | \
							VT_DESC_RX_LONG | VT_DESC_RX_FOV | \
							VT_DESC_RX_FAE | VT_DESC_RX_CRCE | VT_DESC_RX_ERR)
#define VT_DESC_RX_PACK		(VT_DESC_FIRST | VT_DESC_LAST)

/* Tx Descriptor Flag */
#define VT_DESC_TX_ERR		0x00008000	/* Transmit Error */
#define VT_DESC_TX_UDF		0x00000800	/* FIFO Underflow */
#define VT_DESC_TX_CRS		0x00000400	/* Carrier Sense Lost Detect */
#define VT_DESC_TX_OWC		0x00000200	/* Out of Window Collision */
#define VT_DESC_TX_ABT		0x00000100	/* Excessive Collision Abort */
#define VT_DESC_TX_CDH		0x00000010	/* Collision Detect */

/* WOL status */
#define VT_WOL_UCAST		0x10
#define VT_WOL_MAGIC		0x20
#define VT_WOL_MCAST		0x30
#define VT_WOL_LINK_UP		0x40
#define VT_WOL_LINK_DOWN	0x80

/* Link status */
#define VT_LINK_UP			1
#define VT_LINK_DOWN		0
#define VT_LINK_UNKNOWN		-1

#define VT_RX_BUF_SIZE		1536
#define VT_TX_BUF_SIZE		1536
#define VT_RX_DESC_NUM		64
#define VT_TX_DESC_NUM		64

/* #define VT6105_DEBUG */

/* Data Descriptor */
typedef struct vt_desc {
	u32_t status;		/* command/status */
	u32_t length;		/* Buffer/Frame length */
	u32_t addr;			/* DMA buffer address */
	u32_t next_desc;	/* Next descriptor */
} vt_desc;

/* Driver Data Structure */
typedef struct vt_driver {
	u16_t vt_base_addr;		/* Base address */
	int vt_revision;		/* Revision ID */
	int vt_irq;				/* IRQ number */
	int vt_mode;
	int vt_link;			/* Whether link-up */
	int vt_recv_flag;		/* Receive flag */
	int vt_send_flag;		/* Send flag */
	int vt_report_link;
	int vt_tx_alive;
	int vt_tx_busy;

	/* Buffer */
	size_t vt_buf_size;
	char *vt_buf;

	/* Rx data */
	int vt_rx_head;
	struct {
		phys_bytes buf_dma;
		char *buf;
	} vt_rx[VT_RX_DESC_NUM];
	vt_desc *vt_rx_desc;			/* Rx descriptor buffer */
	phys_bytes vt_rx_desc_dma;		/* Rx descriptor DMA buffer */

	/* Tx data */
	int vt_tx_head;
	int vt_tx_tail;
	struct {
		int busy;
		phys_bytes buf_dma;
		char *buf;
	} vt_tx[VT_TX_DESC_NUM];
	vt_desc *vt_tx_desc;			/* Tx descriptor buffer */
	phys_bytes vt_tx_desc_dma;		/* Tx descriptor DMA buffer */
	int vt_tx_busy_num;				/* Number of busy Tx descriptors */

	int vt_hook;		/* IRQ hook id at kernel */
	eth_stat_t vt_stat;

	char vt_name[20];
} vt_driver;
