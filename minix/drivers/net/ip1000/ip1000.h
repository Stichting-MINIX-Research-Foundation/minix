#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <machine/pci.h>
#include <minix/ds.h>
#include <assert.h>

/* PCI Register */
#define IC_PCI_VID		0x00	/* Vendor ID */
#define IC_PCI_DID		0x02	/* Device ID */
#define IC_PCI_CMD		0x04	/* Command */
#define IC_PCI_STS		0x06	/* Status */
#define IC_PCI_RID		0x08	/* Revision ID */
#define IC_PCI_CC		0x09	/* Class Code */
#define IC_PCI_CLS		0x0c	/* Cache Line Size */
#define IC_PCI_LT		0x0d	/* Latency Timer */
#define IC_PCI_HT		0x0e	/* Header Type */

/* Internal Register */
#define IC_REG_DMA_CTRL			0x00	/* DMA Control */
#define IC_REG_TX_DESC0			0x10	/* Tx Descriptor Address 0 */
#define IC_REG_TX_DESC1			0x14	/* Tx Descriptor Address 1 */
#define IC_REG_TX_DMA_BTH		0x18	/* Tx DMA Burst Threshold */
#define IC_REG_TX_DMA_UTH		0x19	/* Tx DMA Urgent Threshold */
#define IC_REG_TX_DMA_PERIOD	0x1a	/* Tx DMA Poll Period */
#define IC_REG_RX_DESC0			0x1c	/* Rx Descriptor Address 0 */
#define IC_REG_RX_DESC1			0x20	/* Rx Descriptor Address 1 */
#define IC_REG_RX_DMA_BTH		0x24	/* Rx DMA Burst Threshold */
#define IC_REG_RX_DMA_UTH		0x25	/* Rx DMA Urgent Threshold */
#define IC_REG_RX_DMA_PERIOD	0x26	/* Rx DMA Poll Period */
#define IC_REG_DEBUG_CTRL		0x2c	/* Debug Control */
#define IC_REG_ASIC_CTRL		0x30	/* Main Control */
#define IC_REG_FLOW_OFF_TH		0x3c	/* Flow Off Threshold */
#define IC_REG_FLOW_ON_TH		0x3e	/* Flow On Threshold */
#define IC_REG_EEPROM_DATA		0x48	/* EEPROM Data */
#define IC_REG_EEPROM_CTRL		0x4a	/* EEPROM Control */
#define IC_REG_INTR_STS_ACK		0x5a	/* Interrupt Status Acknowlege */
#define IC_REG_INTR_ENA			0x5c	/* Interrupt Enable */
#define IC_REG_INTR_STS			0x5e	/* Interrupt Status */
#define IC_REG_TX_STS			0x60	/* Tx Status */
#define IC_REG_MAC_CTRL			0x6c	/* MAC Control */
#define IC_REG_PHY_SET			0x75	/* Physical Register Set */
#define IC_REG_PHY_CTRL			0x76	/* Physical Register Control */
#define IC_REG_STA_ADDR0		0x78	/* Station Address 0 */
#define IC_REG_STA_ADDR1		0x7a	/* Station Address 1 */
#define IC_REG_STA_ADDR2		0x7c	/* Station Address 2 */
#define IC_REG_MAX_FRAME		0x86	/* Max Frame Size */
#define IC_REG_RX_MODE			0x88	/* Receive Mode */
#define IC_REG_HASH_TLB0		0x8c	/* Hash Table 0 */
#define IC_REG_HASH_TLB1		0x90	/* Hash Table 1 */

/* Receive Configure Mode */
#define IC_RM_UNICAST		0x01
#define IC_RM_MULTICAST		0x02
#define IC_RM_BROADCAST		0x04
#define IC_RM_ALLFRAMES		0x08

/* Interrupt Enable/Status Command */
#define IC_IR_IRQ_STS		0x0001
#define IC_IR_HOST_ERR		0x0002
#define IC_IR_TX_DONE		0x0004
#define IC_IR_MAC_CTRL		0x0008
#define IC_IR_RX_DONE		0x0010
#define IC_IR_RX_EARLY		0x0020
#define IC_IR_REQUESTED		0x0040
#define IC_IR_UPDATE_STS	0x0080
#define IC_IR_LINK_EVENT	0x0100
#define IC_IR_TX_DMA_DONE	0x0200
#define IC_IR_RX_DMA_DONE	0x0400
#define IC_IR_RFD_END		0x0800
#define IC_IR_RX_DMA_PRIOR	0x1000
#define IC_IR_COMMON		(IC_IR_HOST_ERR | IC_IR_TX_DMA_DONE | \
							IC_IR_TX_DONE | IC_IR_REQUESTED | \
							IC_IR_UPDATE_STS | IC_IR_LINK_EVENT | \
							IC_IR_RX_DMA_DONE | IC_IR_RX_DONE | \
							IC_IR_RX_DMA_PRIOR)

/* ASIC Control Register Command */
#define IC_AC_SPEED10			0x00000010
#define IC_AC_SPEED100			0x00000020
#define IC_AC_SPEED1000			0x00000040
#define IC_AC_LED_MODE			0x00004000
#define IC_AC_GB_RESET			0x00010000
#define IC_AC_RX_RESET			0x00020000
#define IC_AC_TX_RESET			0x00040000
#define IC_AC_DMA				0x00080000
#define IC_AC_FIFO				0x00100000
#define IC_AC_NETWORK			0x00200000
#define IC_AC_HOST				0x00400000
#define IC_AC_AUTO_INIT			0x00800000
#define IC_AC_INTR_IRQ			0x02000000
#define IC_AC_RESET_BUSY		0x04000000
#define IC_AC_LED_SPEED			0x08000000
#define IC_AC_LED_MODE_BIT1		0x20000000
#define IC_AC_RESET_ALL			(IC_AC_GB_RESET | IC_AC_RX_RESET | \
								IC_AC_TX_RESET | IC_AC_DMA | IC_AC_FIFO | \
								IC_AC_NETWORK | IC_AC_HOST | IC_AC_AUTO_INIT)

/* EEPROM Control Command */
#define IC_EC_ADDDR		0x83ff
#define IC_EC_OPCODE	0x00ff
#define IC_EC_WRITE		0x0100
#define IC_EC_READ		0x0200
#define IC_EC_ERASE		0x0300
#define IC_EC_BUSY		0x8000

/* MAC Control Command */
#define IC_MC_DUPLEX_SEL	0x00000020
#define IC_MC_TX_FC_ENA		0x00000080
#define IC_MC_RX_FC_ENA		0x00000100
#define IC_MC_STAT_DISABLE	0x00400000
#define IC_MC_TX_ENABLE		0x01000000
#define IC_MC_TX_DISABLE	0x02000000
#define IC_MC_TX_ENABLED	0x04000000
#define IC_MC_RX_ENABLE		0x08000000
#define IC_MC_RX_DISABLE	0x10000000
#define IC_MC_RX_ENABLED	0x20000000
#define IC_MC_PAUSED		0x40000000

/* DMA Control Command */
#define IC_DC_TX_POLL		0x00001000

/* Physical Control Command */
#define IC_PC_MGMTCLK_LO		0x00
#define IC_PC_MGMTCLK_HI		0x01
#define IC_PC_MGMTDATA			0x02
#define IC_PC_MGMTDIR			0x04
#define IC_PC_DUPLEX_POLARITY	0x08
#define IC_PC_DUPLEX_STS		0x10
#define IC_PC_LINK_POLARITY		0x20
#define IC_PC_LINK_SPEED		0xc0
#define IC_PC_LINK_SPEED10		0x40
#define IC_PC_LINK_SPEED100		0x80
#define IC_PC_LINK_SPEED1000	0xc0

/* Tx Status */
#define IC_TS_TX_ERR			0x00000001
#define IC_TS_TX_COLLISION		0x00000004
#define IC_TS_TX_MAX_COLL		0x00000008
#define IC_TS_TX_UNDERRUN		0x00000010
#define IC_TS_TX_IND_REQD		0x00000040
#define IC_TS_TX_DONE			0x00000080
#define IC_TS_TX_FRAMEID		0xffff0000

/* Tx Frame Status */
#define IC_TFS_FRAMEID			0x000000000000ffffULL
#define IC_TFS_WORD_ALIGN		0x0000000000030000ULL
#define IC_TFS_TCP_CHECKSUM		0x0000000000040000ULL
#define IC_TFS_UDP_CHECKSUM		0x0000000000080000ULL
#define IC_TFS_IP_CHECKSUM		0x0000000000100000ULL
#define IC_TFS_FRAG_COUNT		0x000000000f000000ULL
#define IC_TFS_WORD_ALIGN		0x0000000000030000ULL
#define IC_TFS_TX_DMA_INDICATE	0x0000000000800000ULL
#define IC_TFS_TFD_DONE			0x0000000080000000ULL

/* Tx Frame Information */
#define IC_TFI_FRAG_ADDR		0x000000ffffffffffULL
#define IC_TFI_FRAG_LEN			0xffff000000000000ULL

/* Rx Frame Status */
#define IC_RFS_FRAME_LEN		0x000000000000ffffULL
#define IC_RFS_OVERRUN			0x0000000000010000ULL
#define IC_RFS_RUNT				0x0000000000020000ULL
#define IC_RFS_ALIGN_ERR		0x0000000000040000ULL
#define IC_RFS_FCS_ERR			0x0000000000080000ULL
#define IC_RFS_OVERSIZE			0x0000000000100000ULL
#define IC_RFS_LEN_ERR			0x0000000000200000ULL
#define IC_RFS_VLAN_DETECT		0x0000000000400000ULL
#define IC_RFS_TCP_DETECT		0x0000000000800000ULL
#define IC_RFS_TCP_ERR			0x0000000001000000ULL
#define IC_RFS_UCP_DETECT		0x0000000002000000ULL
#define IC_RFS_UCP_ERR			0x0000000004000000ULL
#define IC_RFS_IP_DETECT		0x0000000008000000ULL
#define IC_RFS_IP_ERR			0x0000000010000000ULL
#define IC_RFS_FRAME_START		0x0000000020000000ULL
#define IC_RFS_FRAME_END		0x0000000040000000ULL
#define IC_RFS_RFD_DONE			0x0000000080000000ULL
#define IC_RFS_TCI				0x0000ffff00000000ULL
#define IC_RFS_NORMAL			(IC_RFS_RFD_DONE | IC_RFS_FRAME_START | \
								IC_RFS_FRAME_END)
#define IC_RFS_ALL_ERR			(IC_RFS_OVERRUN | IC_RFS_RUNT | \
								IC_RFS_ALIGN_ERR | IC_RFS_FCS_ERR | \
								IC_RFS_OVERSIZE | IC_RFS_LEN_ERR)

/* Rx Frame Information */
#define IC_RFI_FRAG_ADDR		0x000000ffffffffffULL
#define IC_RFI_FRAG_LEN			0xffff000000000000ULL

/* Link Status */
#define IC_LINK_UP			1
#define IC_LINK_DOWN		0
#define IC_LINK_UNKNOWN		-1

#define IC_RX_BUF_SIZE		1536
#define IC_TX_BUF_SIZE		1536
#define IC_RX_DESC_NUM		256
#define IC_TX_DESC_NUM		256

/* #define IP1000_DEBUG */

/* Physical parameters */
static const u16_t DefaultPhyParam[] = {
	(0x4000 | (07 * 4)), 31, 0x0001, 27, 0x01e0, 31, 0x0002, 27, 0xeb8e, 31,
	0x0000, 30, 0x005e, 9, 0x0700,
	(0x4100 | (07 * 4)), 31, 0x0001, 27, 0x01e0, 31, 0x0002, 27, 0xeb8e, 31,
	0x0000, 30, 0x005e, 9, 0x0700, 0x0000
};

/* Data Descriptor */
typedef struct ic_desc {
	u64_t next_desc;	/* Next descriptor */
	u64_t status;		/* Status information */
	u64_t frag_info;	/* Length and DMA buffer address */
} ic_desc;

/* Driver Data Structure */
typedef struct ic_driver {
	u16_t ic_base_addr;		/* Base address */
	int ic_revision;		/* Revision ID */
	int ic_irq;				/* IRQ number */
	int ic_mode;
	int ic_link;			/* Whether link-up */
	int ic_recv_flag;		/* Receive flag */
	int ic_send_flag;		/* Send flag */
	int ic_tx_alive;
	int ic_tx_busy;

	/* LED mode read from EEPROM */
	u16_t ic_led_mode;

	/* Station address read from EEPROM */
	u16_t sta_addr[3];

	/* Buffer */
	size_t ic_buf_size;
	char *ic_buf;

	/* Rx data */
	int ic_rx_head;
	struct {
		phys_bytes buf_dma;
		char *buf;
	} ic_rx[IC_RX_DESC_NUM];
	ic_desc *ic_rx_desc;			/* Rx descriptor buffer */
	phys_bytes ic_rx_desc_dma;		/* Rx descriptor DMA buffer */

	/* Tx data */
	int ic_tx_head;
	int ic_tx_tail;
	struct {
		int busy;
		phys_bytes buf_dma;
		char *buf;
	} ic_tx[IC_TX_DESC_NUM];
	ic_desc *ic_tx_desc;			/* Tx descriptor buffer */
	phys_bytes ic_tx_desc_dma;		/* Tx descriptor DMA buffer */
	int ic_tx_busy_num;				/* Number of busy Tx descriptors */

	int ic_hook;		/* IRQ hook id at kernel */
	eth_stat_t ic_stat;

	char ic_name[20];
} ic_driver;
