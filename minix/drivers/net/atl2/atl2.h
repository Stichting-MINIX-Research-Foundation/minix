/* Attansic/Atheros L2 FastEthernet driver, by D.C. van Moolenbroek */

#define ATL2_MIN_MMAP_SIZE		0x1608	/* min. register memory size */

/* The first three are configurable to a certain extent; the last is not. */
#define ATL2_TXD_BUFSIZE		8192	/* TxD ring buffer size */
#define ATL2_TXS_COUNT			64	/* Tx status ring array size */
#define ATL2_RXD_COUNT			64	/* Rx descriptors */
#define ATL2_RXD_SIZE			1536	/* Rx element size */

#define ATL2_MASTER_REG			0x1400	/* master register */
#	define ATL2_MASTER_SOFT_RESET	0x00000001	/* soft reset */
#	define ATL2_MASTER_IMT_EN	0x00000004	/* IMT enabled */

#define ATL2_RESET_NTRIES		100	/* #tries to wait for reset */
#define ATL2_RESET_DELAY		10	/* delay (us) between tries */

#define ATL2_PHY_ENABLE_REG		0x140c	/* PHY enable register */
#	define ATL2_PHY_ENABLE		1		/* enable PHY */

#define ATL2_IDLE_REG			0x1410	/* idle status register */

#define ATL2_IDLE_NTRIES		100	/* #tries to wait for idle */
#define ATL2_IDLE_DELAY			100	/* delay (us) between tries */

#define ATL2_HWADDR0_REG		0x1488	/* Hardware address (part 0) */
#define ATL2_HWADDR1_REG		0x148c	/* Hardware address (part 1) */

#define ATL2_ISR_REG			0x1600	/* interrupt status register */
#	define ATL2_ISR_RXF_OVERFLOW	0x00000004	/* RxF overflow */
#	define ATL2_ISR_TXF_UNDERRUN	0x00000008	/* TxF underrun */
#	define ATL2_ISR_TXS_OVERFLOW	0x00000010	/* TxS overflow */
#	define ATL2_ISR_RXS_OVERFLOW	0x00000020	/* RxS overflow */
#	define ATL2_ISR_TXD_UNDERRUN	0x00000080	/* TxD underrun */
#	define ATL2_ISR_RXD_OVERFLOW	0x00000100	/* RxD overflow */
#	define ATL2_ISR_DMAR_TIMEOUT	0x00000200	/* DMA read timeout */
#	define ATL2_ISR_DMAW_TIMEOUT	0x00000400	/* DMA write timeout */
#	define ATL2_ISR_TXS_UPDATED	0x00010000	/* Tx status updated */
#	define ATL2_ISR_RXD_UPDATED	0x00020000	/* Rx status updated */
#	define ATL2_ISR_TX_EARLY	0x00040000	/* Tx started xmit */
#	define ATL2_ISR_PHY_LINKDOWN	0x10000000	/* PHY link down */
#	define ATL2_ISR_DISABLE		0x80000000	/* disable intrs */
#	define ATL2_ISR_TX_EVENT	(ATL2_ISR_TXF_UNDERRUN | \
					 ATL2_ISR_TXS_OVERFLOW | \
					 ATL2_ISR_TXD_UNDERRUN | \
					 ATL2_ISR_TXS_UPDATED | \
					 ATL2_ISR_TX_EARLY)
#	define ATL2_ISR_RX_EVENT	(ATL2_ISR_RXF_OVERFLOW | \
					 ATL2_ISR_RXS_OVERFLOW | \
					 ATL2_ISR_RXD_OVERFLOW | \
					 ATL2_ISR_RXD_UPDATED)

#define ATL2_IMR_REG			0x1604	/* interrupt mask register */
#	define ATL2_IMR_DEFAULT		(ATL2_ISR_DMAR_TIMEOUT | \
					 ATL2_ISR_DMAW_TIMEOUT | \
					 ATL2_ISR_TXS_UPDATED | \
					 ATL2_ISR_RXD_UPDATED | \
					 ATL2_ISR_PHY_LINKDOWN)

#define ATL2_MAC_REG			0x1480	/* MAC config register */
#	define ATL2_MAC_TX_EN		0x00000001	/* enable transmit */
#	define ATL2_MAC_RX_EN		0x00000002	/* enable receive */
#	define ATL2_MAC_PROMISC_EN	0x00008000	/* promiscuous */
#	define ATL2_MAC_MCAST_EN	0x02000000	/* multicast */
#	define ATL2_MAC_BCAST_EN	0x04000000	/* broadcast */
#	define ATL2_MAC_DEFAULT		0x28001cec	/* (magic) */

#define ATL2_MHT0_REG			0x1490	/* multicast hash table bits */
#define ATL2_MHT1_REG			0x1494	/* 64 slots in total */

#define ATL2_DMAREAD_REG		0x1580	/* read DMA config register */
#	define ATL2_DMAREAD_EN		1		/* read DMA enabled */
#define ATL2_DMAWRITE_REG		0x15a0	/* write DMA config register */
#	define ATL2_DMAWRITE_EN		1		/* write DMA enabled */

#define ATL2_DESC_ADDR_HI_REG		0x1540	/* high 32 bits of addresses */
#define ATL2_TXD_ADDR_LO_REG		0x1544	/* low 32 bits of TxD base */
#define ATL2_TXD_BUFSIZE_REG		0x1548	/* size of TxD ring buffer */
#define ATL2_TXS_ADDR_LO_REG		0x154c	/* low 32 bits of TxS base */
#define ATL2_TXS_COUNT_REG		0x1550	/* number of TxS descriptors */
#define ATL2_RXD_ADDR_LO_REG		0x1554	/* low 32 bits of RxD base */
#define ATL2_RXD_COUNT_REG		0x1558	/* number of RxD descriptors */

#define ATL2_IFG_REG			0x1484	/* inter-frame gap config */
#	define ATL2_IFG_DEFAULT		0x60405060	/* (magic) */

#define ATL2_HDPX_REG			0x1498	/* half-duplex mode config */
#	define ATL2_HDPX_DEFAULT	0x07a1f037	/* (magic) */

#define ATL2_IMT_REG			0x1408	/* intr moderation timer */
#	define ATL2_IMT_DEFAULT		100		/* 200 us */

#define ATL2_ICT_REG			0x140e	/* intr clearing timer */
#	define ATL2_ICT_DEFAULT		50000		/* 100 ms */

#define ATL2_MTU_REG			0x149c	/* MTU config */
#	define ATL2_MTU_DEFAULT		NDEV_ETH_PACKET_MAX

#define ATL2_CUT_THRESH_REG		0x1590	/* cut-through config */
#	define ATL2_CUT_THRESH_DEFAULT	0x177		/* (magic) */

#define ATL2_FLOW_THRESH_HI_REG		0x15a8	/* RxD overflow hi watermark */
#define ATL2_FLOW_THRESH_LO_REG		0x15aa	/* RxD overflow lo watermark */

#define ATL2_TXD_IDX_REG		0x15f0	/* TxD read index */
#define ATL2_RXD_IDX_REG		0x15f4	/* RxD write index */

#define ATL2_LTSSM_TESTMODE_REG		0x12fc	/* PCIE configuration */
#define ATL2_LTSSM_TESTMODE_DEFAULT	0x6500		/* (magic) */
#define ATL2_DLL_TX_CTRL_REG		0x1104	/* PCIE configuration */
#define ATL2_DLL_TX_CTRL_DEFAULT	0x0568		/* (magic) */

#define ATL2_VPD_CAP_REG		0x6c	/* VPD command register */
#	define ATL2_VPD_CAP_ADDR_SHIFT	16
#	define ATL2_VPD_CAP_ADDR_MASK	0x7fff0000
#	define ATL2_VPD_CAP_DONE	0x80000000
#define ATL2_VPD_DATA_REG		0x70	/* VPD data register */

#define ATL2_SPICTL_REG			0x200	/* SPI control register */
#	define ATL2_SPICTL_VPD_EN	0x2000		/* enable VPD */

#define ATL2_VPD_REGBASE		0x100	/* VPD register base */
#define ATL2_VPD_NREGS			64	/* number of VPD registers */
#define ATL2_VPD_SIG_MASK		0xff	/* signature mask */
#define ATL2_VPD_SIG			0x5a	/* VPD entry signature */
#define ATL2_VPD_REG_SHIFT		16	/* key shift */

#define ATL2_VPD_NTRIES			10	/* #tries to read from VPD */
#define ATL2_VPD_DELAY			2000	/* delay (us) between tries */

#define ATL2_MDIO_REG			0x1414	/* Management Data I/O reg */
#	define ATL2_MDIO_ADDR_SHIFT	16		/* register address */
#	define ATL2_MDIO_ADDR_MASK	0x001f0000	/* (shift and mask) */
#	define ATL2_MDIO_READ		0x00200000	/* read operation */
#	define ATL2_MDIO_SUP_PREAMBLE	0x00400000	/* suppress preamble */
#	define ATL2_MDIO_START		0x00800000	/* initiate xfer */
#	define ATL2_MDIO_CLK_25_4	0x00000000	/* 25MHz, 4bit */
#	define ATL2_MDIO_BUSY		0x08000000	/* in progress */
#	define ATL2_MDIO_DATA_MASK	0x0000ffff	/* result data mask */

#define ATL2_MDIO_NTRIES		10	/* #tries to access MDIO */
#define ATL2_MDIO_DELAY			2	/* delay (us) between tries */

#define ATL2_MII_BMSR			1	/* basic mode status reg */
#	define ATL2_MII_BMSR_LSTATUS	0x0004		/* link status */
#define ATL2_MII_PSSR			17	/* PHY specific status reg */
#	define ATL2_MII_PSSR_RESOLVED	0x0800		/* speed/duplex OK */
#	define ATL2_MII_PSSR_DUPLEX	0x2000		/* full duplex */
#	define ATL2_MII_PSSR_SPEED	0xc000		/* link speed */
#	define ATL2_MII_PSSR_10		0x0000			/* 10Mbps */
#	define ATL2_MII_PSSR_100	0x4000			/* 100Mbps */
#	define ATL2_MII_PSSR_1000	0x8000			/* 1000Mbps */

#define ATL2_RXD_SIZE_MASK		0x000007ff	/* packet size mask */
#define ATL2_RXD_SUCCESS		0x00010000	/* successful receipt */
#define ATL2_RXD_BCAST			0x00020000	/* broadcast frame */
#define ATL2_RXD_MCAST			0x00040000	/* multicast frame */
#define ATL2_RXD_PAUSE			0x00080000	/* pause frame */
#define ATL2_RXD_CTRL			0x00100000	/* control frame */
#define ATL2_RXD_CRCERR			0x00200000	/* invalid frame CRC */
#define ATL2_RXD_CODEERR		0x00400000	/* invalid opcode */
#define ATL2_RXD_RUNT			0x00800000	/* short frame */
#define ATL2_RXD_FRAG			0x01000000	/* collision fragment */
#define ATL2_RXD_TRUNC			0x02000000	/* frame truncated */
#define ATL2_RXD_ALIGN			0x04000000	/* frame align error */
#define ATL2_RXD_UPDATE			0x80000000	/* updated by device */

#define ATL2_TXS_SIZE_MASK		0x000007ff	/* packet size mask */
#define ATL2_TXS_SUCCESS		0x00010000	/* successful xmit */
#define ATL2_TXS_BCAST			0x00020000	/* broadcast frame */
#define ATL2_TXS_MCAST			0x00040000	/* multicast frame */
#define ATL2_TXS_PAUSE			0x00080000	/* pause frame */
#define ATL2_TXS_CTRL			0x00100000	/* control frame */
#define ATL2_TXS_DEFER			0x00200000	/* deferred transmit */
#define ATL2_TXS_EXCDEFER		0x00400000	/* excess defer */
#define ATL2_TXS_SINGLECOL		0x00800000	/* single collision */
#define ATL2_TXS_MULTICOL		0x01000000	/* multi collisions */
#define ATL2_TXS_LATECOL		0x02000000	/* late collision */
#define ATL2_TXS_ABORTCOL		0x04000000	/* collision abort */
#define ATL2_TXS_UNDERRUN		0x08000000	/* buffer underrun */
#define ATL2_TXS_UPDATE			0x80000000	/* updated by device */
