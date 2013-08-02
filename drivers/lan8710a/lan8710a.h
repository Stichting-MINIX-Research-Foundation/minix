#ifndef LAN8710A_H_
#define LAN8710A_H_

#include <net/gen/ether.h>

#define LAN8710A_DEBUG		(1)

#if LAN8710A_DEBUG == 1
	#define LAN8710A_DEBUG_PRINT(args) 		\
		do {					\
			printf("LAN8710A DEBUG: ");	\
			printf args; 			\
			printf("\n");			\
		} while (0)
#else
	#define LAN8710A_DEBUG_PRINT(args)
#endif

#ifndef ERR
	#define ERR (-1)	/* general error flag */
#endif
#ifndef OK
	#define OK 0		/* general OK flag */
#endif

#define MAP_FAILED 		((void *) -1)	/* mmap() failed */

/* Ethernet driver defines */
#define LAN8710A_NAME_LEN	(11)

/* Ethernet driver states */
#define LAN8710A_DETECTED	(1 << 0)
#define LAN8710A_ENABLED	(1 << 1)
#define LAN8710A_READING	(1 << 2)
#define LAN8710A_WRITING	(1 << 3)
#define LAN8710A_RECEIVED	(1 << 4)
#define LAN8710A_TRANSMIT	(1 << 5)

/* Descriptors flags */
#define LAN8710A_DESC_FLAG_OWN		(1 << 29) /* ownership flag */
#define LAN8710A_DESC_FLAG_SOP		(1 << 31) /* start of packet flag */
#define LAN8710A_DESC_FLAG_EOP		(1 << 30) /* end of packet flag */

/* Number of Tx and Rx interrupts */
#define LAN8710A_RX_INTR		(41)
#define LAN8710A_TX_INTR		(42)

/* Values to be written after interrupt handle and interrupt masks*/
#define RX_INT				(1)
#define TX_INT				(2)

/** Numbers of Tx DMA channels */
#define TX_DMA_CHANNELS			(8)

/** Number of transmit descriptors */
#define LAN8710A_NUM_TX_DESC		(255)

/** Number of receive descriptors */
#define LAN8710A_NUM_RX_DESC		(255)

/** Number of I/O vectors to use. */
#define LAN8710A_IOVEC_NR		(16)

/** Size of each I/O buffer per descriptor. */
#define LAN8710A_IOBUF_SIZE		(1520)

/** MAC address override variable. */
#define LAN8710A_ENVVAR 		"LAN8710AETH"

/** MAX DMA Channels */
#define DMA_MAX_CHANNELS		(8)

/* Setting of Tx descriptors */
#define TX_DESC_TO_PORT1 		(1 << 16)
#define TX_DESC_TO_PORT_EN 		(1 << 20)

typedef struct lan8710a_desc_t
{
	u32_t  next_pointer;
	u32_t  buffer_pointer;
	u32_t  buffer_length_off;
	u32_t  pkt_len_flags;
} lan8710a_desc_t;

typedef struct lan8710a_t
{
	lan8710a_desc_t  *rx_desc;
	lan8710a_desc_t  *tx_desc;
	phys_bytes  rx_desc_phy;
	phys_bytes  tx_desc_phy;
	char  name[LAN8710A_NAME_LEN];
	int  status;
	int  irq_rx_hook;	/* Rx interrupt Request Vector Hook. */
	int  irq_tx_hook;	/* Tx interrupt Request Vector Hook. */
	int  instance;
	ether_addr_t  address;	/* Ethernet MAC address. */
	u8_t  *regs;
	u32_t  phy_address;
	u8_t  *p_rx_buf;	/* pointer to the buffer with receive frames */
	u8_t  *p_tx_buf;	/* pointer to the buffer with transmit frames */

	u16_t  tx_desc_idx;	/* index of the next transmit desciptor */
	u16_t  rx_desc_idx;	/* index of the next receive desciptor */
	int  client;
	message  tx_message;
	message  rx_message;
	unsigned int  rx_size;

	/* register mapping */
	vir_bytes  regs_cp_per;
	vir_bytes  regs_mdio;
	vir_bytes  regs_cpsw_cpdma;
	vir_bytes  regs_ctrl_mod;
	vir_bytes  regs_cpsw_sl;
	vir_bytes  regs_cpsw_ss;
	vir_bytes  regs_cpsw_stats;
	vir_bytes  regs_cpsw_ale;
	vir_bytes  regs_cpsw_wr;
	vir_bytes  regs_intc;
	vir_bytes  regs_cpdma_stram;
} lan8710a_t;

#endif /* LAN8710A_H_ */
