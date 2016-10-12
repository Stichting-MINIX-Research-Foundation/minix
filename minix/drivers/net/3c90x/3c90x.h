/* 3Com 3C90xB/C EtherLink driver, by D.C. van Moolenbroek */
#ifndef _DRIVERS_NET_3C90X_H
#define _DRIVERS_NET_3C90X_H

/* The following time values are in microseconds (us). */
#define XLBC_CMD_TIMEOUT		1000	/* command timeout */
#define XLBC_EEPROM_TIMEOUT		500	/* EEPROM read timeout */
#define XLBC_AUTONEG_TIMEOUT		2000000	/* auto-negotiation timeout */
#define XLBC_RESET_DELAY		1000	/* wait time for reset */
#define XLBC_MII_DELAY			1	/* MII cycle response time */

/*
 * Transmission and receipt memory parameters.  The current values allow for
 * buffering of about 32 full-size packets, requiring 48KB of memory for each
 * direction (and thus 96KB in total).  For transmission, it is possible to
 * queue many more small packets using the same memory area.  For receipt, it
 * is not, since each incoming packet may be of full size.  This explains the
 * seemingly huge difference in descriptor counts.
 */
#define XLBC_DPD_COUNT			256	/* TX descriptor count */
#define XLBC_TXB_SIZE			48128	/* TX buffer size in bytes */
#define XLBC_UPD_COUNT			32	/* RX descriptor count */

#define XLBC_MIN_PKT_LEN		NDEV_ETH_PACKET_MIN
#define XLBC_MAX_PKT_LEN		NDEV_ETH_PACKET_MAX_TAGGED

#define XLBC_MIN_REG_SIZE		128	/* min. register memory size */

#define XLBC_CMD_REG			0x0e	/* command register */
#	define XLBC_CMD_GLOBAL_RESET	0x0000	/* perform overall NIC reset */
#	define XLBC_CMD_RX_RESET	0x2800	/* perform receiver reset */
#	define XLBC_CMD_TX_RESET	0x5800	/* perform transmitter reset */
#	define XLBC_CMD_DN_STALL	0x3002	/* stall download */
#	define XLBC_CMD_DN_UNSTALL	0x3003	/* unstall download */
#	define XLBC_CMD_TX_ENABLE	0x4800	/* enable transmission */
#	define XLBC_CMD_RX_ENABLE	0x2000	/* enable receipt */
#	define XLBC_CMD_SET_FILTER	0x8000	/* set receipt filter */
#	define XLBC_CMD_UP_UNSTALL	0x3001	/* unstall upload */
#	define XLBC_CMD_IND_ENABLE	0x7800	/* enable indications */
#	define XLBC_CMD_INT_ENABLE	0x7000	/* enable interrupts */
#	define XLBC_CMD_SELECT_WINDOW	0x0800	/* select register window */
#	define XLBC_CMD_STATS_ENABLE	0xa800	/* enable statistics */

#define XLBC_FILTER_STATION		0x01	/* packets addressed to NIC */
#define XLBC_FILTER_MULTI		0x02	/* multicast packets */
#define XLBC_FILTER_BROAD		0x04	/* broadcast packets */
#define XLBC_FILTER_PROMISC		0x08	/* all packets (promiscuous) */

#define XLBC_STATUS_REG			0x0e	/* interupt status register */
#	define XLBC_STATUS_HOST_ERROR	0x0002	/* catastrophic host error */
#	define XLBC_STATUS_TX_COMPLETE	0x0004	/* packet transmission done */
#	define XLBC_STATUS_UPDATE_STATS	0x0080	/* statistics need retrieval */
#	define XLBC_STATUS_LINK_EVENT	0x0100	/* link status change event */
#	define XLBC_STATUS_DN_COMPLETE	0x0200	/* packet download completed */
#	define XLBC_STATUS_UP_COMPLETE	0x0400	/* packet upload completed */
#	define XLBC_STATUS_IN_PROGRESS	0x1000	/* command still in progress */

/* The mask of interrupts in which we are interested. */
#define XLBC_STATUS_MASK \
	(XLBC_STATUS_HOST_ERROR | \
	XLBC_STATUS_TX_COMPLETE | \
	XLBC_STATUS_UPDATE_STATS | \
	XLBC_STATUS_LINK_EVENT | \
	XLBC_STATUS_DN_COMPLETE | \
	XLBC_STATUS_UP_COMPLETE)

#define XLBC_TX_STATUS_REG		0x1b	/* TX status register */
#	define XLBC_TX_STATUS_OVERFLOW	0x04	/* TX status stack full */
#	define XLBC_TX_STATUS_MAX_COLL	0x08	/* max collisions reached */
#	define XLBC_TX_STATUS_UNDERRUN	0x10	/* packet transfer underrun */
#	define XLBC_TX_STATUS_JABBER	0x20	/* transmitting for too long */
#	define XLBC_TX_STATUS_COMPLETE	0x80	/* register contents valid */

#define XLBC_STATUS_AUTO_REG		0x1e	/* auto interrupt status reg */

#define XLBC_DMA_CTRL_REG		0x20	/* DMA control register */
#	define XLBC_DMA_CTRL_DN_INPROG	0x00000080	/* dn in progress */
#	define XLBC_DMA_CTRL_UP_NOALT	0x00010000	/* disable up altseq */
#	define XLBC_DMA_CTRL_DN_NOALT	0x00020000	/* disable dn altseq */

#define XLBC_DN_LIST_PTR_REG		0x24	/* download pointer register */

#define XLBC_UP_LIST_PTR_REG		0x38	/* uplist pointer register */

#define XLBC_EEPROM_WINDOW		0	/* EEPROM register window */
#define XLBC_EEPROM_CMD_REG		0x0a	/* EEPROM command register */
#	define XLBC_EEPROM_CMD_ADDR	0x003f	/* address mask */
#	define XLBC_EEPROM_CMD_READ	0x0080	/* read register opcode */
#	define XLBC_EEPROM_CMD_BUSY	0x8000	/* command in progress */
#define XLBC_EEPROM_DATA_REG		0x0c	/* EEPROM data register */

#define XLBC_EEPROM_WORD_OEM_ADDR0	0x0a	/* OEM node address, word 0 */
#define XLBC_EEPROM_WORD_OEM_ADDR1	0x0b	/* OEM node address, word 1 */
#define XLBC_EEPROM_WORD_OEM_ADDR2	0x0c	/* OEM node address, word 2 */

#define XLBC_STATION_WINDOW		2	/* station register window */
#define XLBC_STATION_ADDR0_REG		0x00	/* station address, word 0 */
#define XLBC_STATION_ADDR1_REG		0x02	/* station address, word 1 */
#define XLBC_STATION_ADDR2_REG		0x04	/* station address, word 2 */
#define XLBC_STATION_MASK0_REG		0x06	/* station mask, word 0 */
#define XLBC_STATION_MASK1_REG		0x08	/* station mask, word 1 */
#define XLBC_STATION_MASK2_REG		0x0a	/* station mask, word 2 */

#define XLBC_CONFIG_WINDOW		3	/* configuration window */
#define XLBC_CONFIG_WORD1_REG		0x02	/* high-order 16 config bits */
#	define XLBC_CONFIG_XCVR_MASK	0x00f0	/* transceiver selection */
#	define XLBC_CONFIG_XCVR_AUTO	0x0080	/* auto-negotiation */

#define XLBC_MAC_CTRL_WINDOW		3	/* MAC control window */
#define XLBC_MAC_CTRL_REG		0x06	/* MAC control register */
#	define XLBC_MAC_CTRL_ENA_FD	0x0020	/* enable full duplex */

#define XLBC_MEDIA_OPT_WINDOW		3	/* media options window */
#define XLBC_MEDIA_OPT_REG		0x08	/* media options register */
#	define XLBC_MEDIA_OPT_BASE_TX	0x0002	/* 100BASE-TX available */
#	define XLBC_MEDIA_OPT_10_BT	0x0008	/* 10BASE-T available */

#define XLBC_NET_DIAG_WINDOW		4	/* net diagnostics window */
#define XLBC_NET_DIAG_REG		0x06	/* net diagnostics register */
#	define XLBC_NET_DIAG_UPPER	0x0040	/* enable upper stats bytes */

#define XLBC_PHYS_MGMT_WINDOW		4	/* physical mgmt window */
#define XLBC_PHYS_MGMT_REG		0x08	/* physical mgmt register */
#	define XLBC_PHYS_MGMT_CLK	0x0001	/* MII management clock */
#	define XLBC_PHYS_MGMT_DATA	0x0002	/* MII management data bit */
#	define XLBC_PHYS_MGMT_DIR	0x0004	/* MII data direction bit */

#define XLBC_PHY_ADDR			0x18	/* internal PHY address */

#define XLBC_MII_CONTROL		0x00	/* MII control register */
#	define XLBC_MII_CONTROL_AUTONEG	0x0200	/* restart auto-negotiation */
#	define XLBC_MII_CONTROL_RESET	0x8000	/* reset the PHY */
#define XLBC_MII_STATUS			0x01	/* MII status register */
#	define XLBC_MII_STATUS_EXTCAP	0x0001	/* extended capability */
#	define XLBC_MII_STATUS_AUTONEG	0x0008	/* auto-neg capability */
#	define XLBC_MII_STATUS_COMPLETE	0x0020	/* auto-neg complete */
#define XLBC_MII_AUTONEG_ADV		0x04	/* MII auto-neg advertise */
#	define XLBC_MII_LINK_T_HD	0x0020	/* 10BASE-T half-duplex */
#	define XLBC_MII_LINK_T_FD	0x0040	/* 10BASE-T full-duplex */
#	define XLBC_MII_LINK_TX_HD	0x0080	/* 100BASE-TX half-duplex */
#	define XLBC_MII_LINK_TX_FD	0x0100	/* 100BASE-TX full-duplex */
#define XLBC_MII_LP_ABILITY		0x05	/* MII link partner ability */
#define XLBC_MII_AUTONEG_EXP		0x06	/* MII auto-neg expansion */

#define XLBC_MEDIA_STS_WINDOW		4	/* media status window */
#define XLBC_MEDIA_STS_REG		0x0a	/* media status register */
#	define XLBC_MEDIA_STS_LINK_DET	0x0800	/* link detected */
#	define XLBC_MEDIA_STS_TX_INPROG	0x1000	/* TX in progress */

#define XLBC_SSD_STATS_WINDOW		4	/* SSD statistics window */
#define XLBC_BAD_SSD_REG		0x0c	/* bad start-of-stream delim */

#define XLBC_STATS_WINDOW		6	/* statistics window */
#define XLBC_CARRIER_LOST_REG		0x00	/* # packets w/ carrier lost */
#define XLBC_SQE_ERR_REG		0x01	/* # SQE pulse errors */
#define XLBC_MULTI_COLL_REG		0x02	/* # multiple collisions */
#define XLBC_SINGLE_COLL_REG		0x03	/* # single collisions */
#define XLBC_LATE_COLL_REG		0x04	/* # late collisions */
#define XLBC_RX_OVERRUNS_REG		0x05	/* # receiver overruns */
#define XLBC_FRAMES_XMIT_OK_REG		0x06	/* # frames transmitted */
#define XLBC_FRAMES_RCVD_OK_REG		0x07	/* # frames received */
#define XLBC_FRAMES_DEFERRED_REG	0x08	/* # frames deferred */
#define XLBC_UPPER_FRAMES_REG		0x09	/* upper bits of frame stats */
#	define XLBC_UPPER_RX_MASK	0x03	/* mask for frames received */
#	define XLBC_UPPER_RX_SHIFT	0	/* shift for frames received */
#	define XLBC_UPPER_TX_MASK	0x30	/* mask for frames sent */
#	define XLBC_UPPER_TX_SHIFT	4	/* shift for frames sent */
#define XLBC_BYTES_RCVD_OK_REG		0x0a	/* # bytes received */
#define XLBC_BYTES_XMIT_OK_REG		0x0c	/* # bytes transmitted */

typedef struct {
	uint32_t next;		/* physical address of next descriptor */
	uint32_t flags;		/* frame start header or packet status */
	uint32_t addr;		/* address of first (and only) fragment */
	uint32_t len;		/* length of first (and only) fragment */
} xlbc_pd_t;

/* Bits for the 'flags' field of download descriptors. */
#define XLBC_DN_RNDUP_WORD		0x00000002	/* round up to word */
#define XLBC_DN_DN_COMPLETE		0x00010000	/* download complete */
#define XLBC_DN_DN_INDICATE		0x80000000	/* fire DN_COMPLETE */

/* Bits for the 'flags' field of upload descriptors. */
#define XLBC_UP_LEN			0x00001fff	/* packet length */
#define XLBC_UP_ERROR			0x00004000	/* receive error */
#define XLBC_UP_COMPLETE		0x00008000	/* packet complete */
#define XLBC_UP_OVERRUN			0x00010000	/* FIFO overrun */
#define XLBC_UP_ALIGN_ERR		0x00040000	/* alignment error */
#define XLBC_UP_CRC_ERR			0x00080000	/* CRC error */
#define XLBC_UP_OVERFLOW		0x01000000	/* buffer too small */

/* Bits for the 'len' field of upload and download descriptors. */
#define XLBC_LEN_LAST			0x80000000	/* last fragment */

#endif /* !_DRIVERS_NET_3C90X_H */
