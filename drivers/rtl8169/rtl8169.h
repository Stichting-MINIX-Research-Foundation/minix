/*
 * rtl8169.h
 */

#define	RL_N_DESC	1024		/* Number of descriptors */
#define N_RX_DESC	RL_N_DESC	/* Number of receive descriptors */
#define N_TX_DESC	RL_N_DESC	/* Number of transmit descriptors */

#define RX_BUFSIZE	1536		/* Maximum gigabit ethernet frame size */

/* Transmit Descriptor control */
#define DESC_RX_LGSEN	0x08000000	/* Large Send */
#define DESC_RX_IPCS	0x00040000	/* IP Checksum Offload */
#define DESC_RX_UDPCS	0x00020000	/* UDP Checksum Offload */
#define DESC_RX_TCPCS	0x00010000	/* TCP Checksum Offload */
#define DESC_TX_LENMASK	0x0000FFFF	/* Transmit Frame Length Mask */

/* Receive Descriptor control */
#define DESC_RX_MAR	0x08000000	/* Multicast Address Received */
#define DESC_RX_PAM	0x04000000	/* Physical Address Matched */
#define DESC_RX_BAR	0x02000000	/* Broadcast Address Received */
#define DESC_RX_BOVF	0x01000000	/* Buffer Overflow */
#define DESC_RX_FOVF	0x00800000	/* FIFO Overflow */
#define DESC_RX_RWT	0x00400000	/* Receive Watchdog Timer Expired */
#define DESC_RX_RES	0x00200000	/* Receive Error Summary */
#define DESC_RX_RUNT	0x00100000	/* Runt Packet */
#define DESC_RX_CRC	0x00080000	/* CRC Error */
#define DESC_RX_PID1	0x00040000	/* Protocol ID1 */
#define DESC_RX_PID0	0x00020000	/* Protocol ID0 */
#define DESC_RX_IPF	0x00010000	/* IP Checksum Failure */
#define DESC_RX_UDPF	0x00008000	/* UDP Checksum Failure */
#define DESC_RX_TCPF	0x00004000	/* TCP Checksum Failure */
#define DESC_RX_LENMASK	0x00001FFF	/* Receive Frame Length Mask */

/* General Descriptor control */
#define DESC_OWN	0x80000000	/* Ownership */
#define DESC_EOR	0x40000000	/* End of Descriptor Ring */
#define DESC_FS		0x20000000	/* First Segment Descriptor */
#define DESC_LS		0x10000000	/* Last Segment Descriptor */


#define	RL_IDR		0x00	/* Ethernet address
				 * Note: RL_9346CR_EEM_CONFIG mode is
				 * required the change the ethernet address.
				 * Note: 4-byte write access only.
				 */
#define	RL_MAR		0x08	/* Multicast */
#define	RL_DTCCR_LO	0x10	/* Dump Tally Counter Command Register LOW */
#define	RL_DTCCR_HI	0x14	/* Dump Tally Counter Command Register HIGH */
#define		RL_DTCCR_CMD	0x08	/* Command */
#define RL_TNPDS_LO	0x20	/* Transmit Normal Priority Descriptors Start Address LOW */
#define RL_TNPDS_HI	0x24	/* Transmit Normal Priority Descriptors Start Address HIGH */
#define RL_THPDS_LO	0x28	/* Transmit High Priority Descriptors Start Address LOW */
#define RL_THPDS_HI	0x2C	/* Transmit High Priority Descriptors Start Address HIGH */
#define	RL_FLASH	0x30	/* Flash Memory Read/Write Register */
#define RL_ERBCR	0x34	/* Early Receive (Rx) Byte Count Register */
#define RL_ERSR		0x36	/* Early Rx Status Register */
#define		RL_ERSR_RES	0xF0	/* Reserved */
#define		RL_ERSR_ERGOOD	0x08	/* Early Rx Good packet */
#define		RL_ERSR_ERBAD	0x04	/* Early Rx Bad packet */
#define		RL_ERSR_EROVW	0x02	/* Early Rx OverWrite */
#define		RL_ERSR_EROK	0x01	/* Early Rx OK */
#define	RL_CR		0x37	/* Command Register */
#define		RL_CR_RES0	0xE0	/* Reserved */
#define		RL_CR_RST	0x10	/* Reset */
#define		RL_CR_RE	0x08	/* Receiver Enable */
#define		RL_CR_TE	0x04	/* Transmitter Enable *
					 * Note: start with transmit buffer
					 * 0 after RL_CR_TE has been reset.
					 */
#define		RL_CR_RES1	0x02	/* Reserved */
#define		RL_CR_BUFE	0x01	/* Receive Buffer Empty */
#define	RL_TPPOLL	0x38	/* Transmit Priority Polling Register */
#define		RL_TPPOLL_HPQ	0x80	/* High Priority Queue Polling */
#define		RL_TPPOLL_NPQ	0x40	/* Normal Priority Queue Polling */
#define		RL_TPPOLL_FSW	0x01	/* Forced Software Interrupt */
#define	RL_IMR		0x3C	/* Interrupt Mask Register */
#define		RL_IMR_SERR	0x8000	/* System Error */
#define		RL_IMR_TIMEOUT	0x4000	/* Time Out */
#define		RL_IMR_RES	0x3E00	/* Reserved */
#define		RL_IMR_SWINT	0x0100	/* Software Interrupt */
#define		RL_IMR_TDU	0x0080	/* Tx Descriptor Unavailable */
#define		RL_IMR_FOVW	0x0040	/* Rx FIFO Overflow */
#define		RL_IMR_PUN	0x0020	/* Packet Underrun / Link Change */
#define		RL_IMR_RDU	0x0010	/* Rx Descriptor Unavailable */
#define		RL_IMR_TER	0x0008	/* Transmit Error */
#define		RL_IMR_TOK	0x0004	/* Transmit OK */
#define		RL_IMR_RER	0x0002	/* Receive Error */
#define		RL_IMR_ROK	0x0001	/* Receive OK */
#define	RL_ISR		0x3E	/* Interrupt Status Register */
#define		RL_ISR_SERR	0x8000	/* System Error */
#define		RL_ISR_TIMEOUT	0x4000	/* Time Out */
#define		RL_ISR_RES	0x3E00	/* Reserved */
#define		RL_ISR_SWINT	0x0100	/* Software Interrupt */
#define		RL_ISR_TDU	0x0080	/* Tx Descriptor Unavailable */
#define		RL_ISR_FOVW	0x0040	/* Rx FIFO Overflow */
#define		RL_ISR_PUN	0x0020	/* Packet Underrun / Link Change */
#define		RL_ISR_RDU	0x0010	/* Rx Descriptor Unavailable */
#define		RL_ISR_TER	0x0008	/* Transmit Error */
#define		RL_ISR_TOK	0x0004	/* Transmit OK */
#define		RL_ISR_RER	0x0002	/* Receive Error */
#define		RL_ISR_ROK	0x0001	/* Receive OK */
#define	RL_TCR		0x40	/* Transmit Configuration Register
				 * Note: RL_CR_TE has to be set to
				 * set/change RL_TCR.
				 */
#define		RL_TCR_RES0	0x80000000 /* Reserved */
#define		RL_TCR_HWVER_AM 0x7C000000 /* Hardware Version ID A */
#define		RL_TCR_IFG_M	0x03000000 /* Interframe Gap Time */
#define			RL_TCR_IFG_STD		0x03000000 /* IEEE 802.3 std */
#define		RL_TCR_HWVER_BM	0x00800000 /* Hardware Version ID B */
#define			RL_TCR_HWVER_RTL8169	0x00000000 /* RTL8169 */
#define			RL_TCR_HWVER_RTL8169S	0x00800000 /* RTL8169S */
#define			RL_TCR_HWVER_RTL8110S	0x04000000 /* RTL8110S */
#define			RL_TCR_HWVER_RTL8169SB	0x10000000 /* RTL8169sb/8110sb */
#define			RL_TCR_HWVER_RTL8110SCd	0x18000000 /* RTL8169sc/8110sc */
#define			RL_TCR_HWVER_RTL8105E	0x40800000 /* RTL8105E */
#define		RL_TCR_RES1	0x00380000 /* Reserved */
#define		RL_TCR_LBK_M	0x00060000 /* Loopback Test */
#define			RL_TCR_LBK_NORMAL	0x00000000 /* Normal */
#define			RL_TCR_LBK_LOOKBOCK	0x00060000 /* Loopback Mode */
#define		RL_TCR_CRC	0x00010000 /* (Do not) Append CRC */
#define		RL_TCR_RES2	0x0000F800 /* Reserved */
#define		RL_TCR_MXDMA_M	0x00000700 /* Max DMA Burst Size Tx */
#define			RL_TCR_MXDMA_16		0x00000000 /* 16 bytes */
#define			RL_TCR_MXDMA_32		0x00000100 /* 32 bytes */
#define			RL_TCR_MXDMA_64		0x00000200 /* 64 bytes */
#define			RL_TCR_MXDMA_128	0x00000300 /* 128 bytes */
#define			RL_TCR_MXDMA_128	0x00000300 /* 128 bytes */
#define			RL_TCR_MXDMA_256	0x00000400 /* 256 bytes */
#define			RL_TCR_MXDMA_512	0x00000500 /* 512 bytes */
#define			RL_TCR_MXDMA_1024	0x00000600 /* 1024 bytes */
#define			RL_TCR_MXDMA_2048	0x00000700 /* 2048 bytes */
#define		RL_TCR_TXRR_M	0x000000F0 /* Tx Retry Count */
#define		RL_TCR_RES3	0x0000000E /* Reserved */
#define		RL_TCR_CLRABT	0x00000001 /* Clear Abort */
#define RL_RCR		0x44	/* Receive Configuration Register
				 * Note: RL_CR_RE has to be set to
				 * set/change RL_RCR.
				 */
#define		RL_RCR_RES0	0xF0000000 /* Reserved */
#define		RL_RCR_ERTH_M	0x0F000000 /* Early Rx Threshold */
#define			RL_RCR_ERTH_0		0x00000000 /* No threshold */
#define			RL_RCR_ERTH_1		0x01000000 /* 1/16 */
#define			RL_RCR_ERTH_2		0x02000000 /* 2/16 */
#define			RL_RCR_ERTH_3		0x03000000 /* 3/16 */
#define			RL_RCR_ERTH_4		0x04000000 /* 4/16 */
#define			RL_RCR_ERTH_5		0x05000000 /* 5/16 */
#define			RL_RCR_ERTH_6		0x06000000 /* 6/16 */
#define			RL_RCR_ERTH_7		0x07000000 /* 7/16 */
#define			RL_RCR_ERTH_8		0x08000000 /* 8/16 */
#define			RL_RCR_ERTH_9		0x09000000 /* 9/16 */
#define			RL_RCR_ERTH_10		0x0A000000 /* 10/16 */
#define			RL_RCR_ERTH_11		0x0B000000 /* 11/16 */
#define			RL_RCR_ERTH_12		0x0C000000 /* 12/16 */
#define			RL_RCR_ERTH_13		0x0D000000 /* 13/16 */
#define			RL_RCR_ERTH_14		0x0E000000 /* 14/16 */
#define			RL_RCR_ERTH_15		0x0F000000 /* 15/16 */
#define		RL_RCR_RES1	0x00FC0000 /* Reserved */
#define		RL_RCR_MULERINT	0x00020000 /* Multiple Early Int Select */
#define		RL_RCR_RER8	0x00010000 /* Receive small error packet */
#define		RL_RCR_RXFTH_M	0x0000E000 /* Rx FIFO Threshold */
#define			RL_RCR_RXFTH_16		0x00000000 /* 16 bytes */
#define			RL_RCR_RXFTH_32		0x00002000 /* 32 bytes */
#define			RL_RCR_RXFTH_64		0x00004000 /* 64 bytes */
#define			RL_RCR_RXFTH_128	0x00006000 /* 128 bytes */
#define			RL_RCR_RXFTH_256	0x00008000 /* 256 bytes */
#define			RL_RCR_RXFTH_512	0x0000A000 /* 512 bytes */
#define			RL_RCR_RXFTH_1024	0x0000C000 /* 1024 bytes */
#define			RL_RCR_RXFTH_UNLIM	0x0000E000 /* unlimited */
#define		RL_RCR_RBLEM_M	0x00001800 /* Rx Buffer Length */
#define			RL_RCR_RBLEN_8K		0x00000000 /* 8KB + 16 bytes */
#define			RL_RCR_RBLEN_8K_SIZE	(8*1024)
#define			RL_RCR_RBLEN_16K	0x00000800 /* 16KB + 16 bytes */
#define			RL_RCR_RBLEN_16K_SIZE	(16*1024)
#define			RL_RCR_RBLEN_32K	0x00001000 /* 32KB + 16 bytes */
#define			RL_RCR_RBLEN_32K_SIZE	(32*1024)
#define			RL_RCR_RBLEN_64K	0x00001800 /* 64KB + 16 bytes */
#define			RL_RCR_RBLEN_64K_SIZE	(64*1024)
#define		RL_RCR_MXDMA_M	0x00000700 /* Rx DMA burst size */
#define			RL_RCR_MXDMA_16		0x00000000 /* 16 bytes */
#define			RL_RCR_MXDMA_32		0x00000100 /* 32 bytes */
#define			RL_RCR_MXDMA_64		0x00000200 /* 64 bytes */
#define			RL_RCR_MXDMA_128	0x00000300 /* 128 bytes */
#define			RL_RCR_MXDMA_256	0x00000400 /* 256 bytes */
#define			RL_RCR_MXDMA_512	0x00000500 /* 512 bytes */
#define			RL_RCR_MXDMA_1024	0x00000600 /* 1024 bytes */
#define			RL_RCR_MXDMA_UNLIM	0x00000700 /* unlimited */
#define		RL_RCR_WRAP	0x00000080 /* (Do not) Wrap on receive */
#define		RL_RCR_9356	0x00000040 /* EEPROM 1:9356 0:9346 */
#define		RL_RCR_AER	0x00000020 /* Accept Error Packets */
#define		RL_RCR_AR	0x00000010 /* Accept Runt Packets */
#define		RL_RCR_AB	0x00000008 /* Accept Broadcast Packets */
#define		RL_RCR_AM	0x00000004 /* Accept Multicast Packets */
#define		RL_RCR_APM	0x00000002 /* Accept Physical Match Packets */
#define		RL_RCR_AAP	0x00000001 /* Accept All Packets */
#define	RL_TCTR		0x48	/* Timer Count Register */
#define	RL_MPC		0x4C	/* Missed Packet Counter */
#define	RL_9346CR	0x50	/* 93C46 Command Register */
#define		RL_9346CR_EEM_M	0xC0	/* Operating Mode */
#define			RL_9346CR_EEM_NORMAL	0x00 /* Normal Mode */
#define			RL_9346CR_EEM_AUTOLOAD	0x40 /* Load from 93C46 */
#define			RL_9346CR_EEM_PROG	0x80 /* 93C46 Programming */
#define			RL_9346CR_EEM_CONFIG	0xC0 /* Config Write Enable */
#define		RL_9346CR_RES	0x30	/* Reserved */
#define		RL_9346CR_EECS	0x08	/* EECS Pin */
#define		RL_9346CR_EESK	0x04	/* EESK Pin */
#define		RL_9346CR_EEDI	0x02	/* EEDI Pin */
#define		RL_9346CR_EEDO	0x01	/* EEDO Pin */
#define RL_CONFIG0	0x51	/* Configuration Register 0 */
#define		RL_CFG0_RES	0x000000F8 /* Reserved */
#define		RL_CFG0_ROM	0x00000007 /* Select Boot ROM Size */
#define			RL_CFG0_ROM128K	0x00000005 /* 128K Boot ROM */
#define			RL_CFG0_ROM64K	0x00000004 /* 64K Boot ROM */
#define			RL_CFG0_ROM32K	0x00000003 /* 32K Boot ROM */
#define			RL_CFG0_ROM16K	0x00000002 /* 16K Boot ROM */
#define			RL_CFG0_ROM8K	0x00000001 /* 8K Boot ROM */
#define			RL_CFG0_ROMNO	0x00000000 /* No Boot ROM */
#define RL_CONFIG1	0x52	/* Configuration Register 1 */
#define		RL_CFG1_LEDS1	0x00000080 /* LED1 */
#define		RL_CFG1_LEDS0	0x00000040 /* LED0 */
#define		RL_CFG1_DVRLOAD	0x00000020 /* Driver Load */
#define		RL_CFG1_LWACT	0x00000010 /* LWAKE Active Mode */
#define		RL_CFG1_MEMMAP	0x00000008 /* Memory Mapping */
#define		RL_CFG1_IOMAP	0x00000004 /* I/O Mapping */
#define		RL_CFG1_VPD	0x00000002 /* Enable Vital Product Data */
#define		RL_CFG1_PME	0x00000001 /* Power Management Enable */
#define RL_CONFIG2	0x53	/* Configuration Register 2 */
#define		RL_CFG2_RES	0x000000E0 /* Reserved */
#define		RL_CFG2_AUX	0x00000010 /* Auxiliary Power Present Status */
#define		RL_CFG2_PCIBW	0x00000008 /* PCI Bus Width 1:64 0:32 */
#define		RL_CFG2_PCICLK	0x00000007 /* PCI Clock Frequency */
#define			RL_CFG2_66MHZ	0x00000001 /* 66 MHz */
#define			RL_CFG2_33MHZ	0x00000000 /* 33 MHz */
#define RL_CONFIG3	0x54	/* Configuration Register 3 */
#define		RL_CFG3_MAGIC	0x00000020 /* Wake up when receives a Magic Packet */
#define		RL_CFG3_LINKUP	0x00000010 /* Wake up when the cable connection is re-established */
#define		RL_CFG3_BEACON	0x00000001 /* 8168 only, Reserved in the 8168b */
#define RL_CONFIG4	0x55	/* Configuration Register 4 */
#define RL_CONFIG5	0x56	/* Configuration Register 5 */
#define		RL_CFG5_BWF	0x00000040 /* Accept Broadcast Wakeup Frame */
#define		RL_CFG5_MWF	0x00000020 /* Accept Multicast Eakeup Frame */
#define		RL_CFG5_UWF	0x00000010 /* Accept Unicast Wakeup Frame */
#define		RL_CFG5_LAN	0x00000002 /* LANWake Singnal enable/disable */
#define		RL_CFG5_PME	0x00000001 /* PME status can be reset by PCI RST# */
#define RL_TIMERINT	0x58	/* Timer Interrupt Select */
#define RL_MULINT	0x5C	/* Multiple Interrupt Select */
/*			0x5E */	/* Reserved */
/*			0x5F */	/* Reserved */
#define RL_PHYAR	0x60	/* PHY Access */
#define RL_TBICSR0	0x64	/* TBI Control and Status Register */
#define RL_TBIANAR	0x68	/* TBI Auto-Negotiation Advertisement Register */
#define RL_TBILPAR	0x6A	/* TBI Auto-Negotiation Link Partner Ability Register */
#define RL_PHYSTAT	0x6C	/* MII PHY Status */
#define		RL_STAT_TBI	0x00000080 /* TBI Enable */
#define		RL_STAT_TXFLOW	0x00000040 /* Tx Flow Control */
#define		RL_STAT_RXFLOW	0x00000020 /* Rx Flow Control */
#define		RL_STAT_1000	0x00000010 /* 1000 Mbps */
#define		RL_STAT_100	0x00000008 /* 100 Mbps */
#define		RL_STAT_10	0x00000004 /* 10 Mbps */
#define		RL_STAT_LINK	0x00000002 /* Link Status */
#define		RL_STAT_FULLDUP	0x00000001 /* Full Duplex */

#define RL_CCR_UNDOC	0x82	/* Undocumented C+ Command Register */

#define RL_RMS		0xDA	/* Rx Maximum Size */
#define RL_CPLUSCMD	0xE0	/* C+ Command Register */
#define		RL_CPLUS_VLAN	0x00000040 /* Receive VLAN D-tagging Enable */
#define		RL_CPLUS_CHKSUM	0x00000020 /* Receive Checksum Offload Enable */
#define		RL_CPLUS_DAC	0x00000010 /* PCI Dual Address Cycles Enable */
#define		RL_CPLUS_MULRW	0x00000008 /* PCI Multiple Read/Write Enable */
#define RL_INTRMITIGATE	0xE2	/* Interrupt Mitigate */
#define RL_RDSAR_LO	0xE4	/* Receive Descriptor Start Address Register
				 * 256-byte alignment Low*/
#define RL_RDSAR_HI	0xE8	/* Receive Descriptor Start Address High */
#define RL_ETTHR	0xEC	/* Early Transmit Threshold Register */
#define RL_FER		0xF0	/* Function Event Register */
#define RL_FEMR		0xF4	/* Function Event Mask Register */
#define RL_FPSR		0xF8	/* Function Present State Register */
#define RL_FFER		0xFC	/* Function Force Event Register */

/*
 * Registers in the Machine Independent Interface (MII) to the PHY.
 * IEEE 802.3 (2000 Edition) Clause 22.
 */

#define MII_CTRL	0x0	/* Control Register (basic) */
#define		MII_CTRL_RST	0x8000	/* Reset PHY */
#define		MII_CTRL_LB	0x4000	/* Enable Loopback Mode */
#define		MII_CTRL_SP_LSB	0x2000	/* Speed Selection (LSB) */
#define		MII_CTRL_ANE	0x1000	/* Auto Negotiation Enable */
#define		MII_CTRL_PD	0x0800	/* Power Down */
#define		MII_CTRL_ISO	0x0400	/* Isolate */
#define		MII_CTRL_RAN	0x0200	/* Restart Auto-Negotiation Process */
#define		MII_CTRL_DM	0x0100	/* Full Duplex */
#define		MII_CTRL_CT	0x0080	/* Enable COL Signal Test */
#define		MII_CTRL_SP_MSB	0x0040	/* Speed Selection (MSB) */
#define			MII_CTRL_SP_10		0x0000	/* 10 Mb/s */
#define			MII_CTRL_SP_100		0x2000	/* 100 Mb/s */
#define			MII_CTRL_SP_1000	0x0040	/* 1000 Mb/s */
#define			MII_CTRL_SP_RES		0x2040	/* Reserved */
#define		MII_CTRL_RES	0x003F	/* Reserved */
#define MII_STATUS	0x1	/* Status Register (basic) */
#define		MII_STATUS_100T4	0x8000	/* 100Base-T4 support */
#define		MII_STATUS_100XFD	0x4000	/* 100Base-X FD support */
#define		MII_STATUS_100XHD	0x2000	/* 100Base-X HD support */
#define		MII_STATUS_10FD		0x1000	/* 10 Mb/s FD support */
#define		MII_STATUS_10HD		0x0800	/* 10 Mb/s HD support */
#define		MII_STATUS_100T2FD	0x0400	/* 100Base-T2 FD support */
#define		MII_STATUS_100T2HD	0x0200	/* 100Base-T2 HD support */
#define		MII_STATUS_EXT_STAT	0x0100	/* Supports MII_EXT_STATUS */
#define		MII_STATUS_RES		0x0080	/* Reserved */
#define		MII_STATUS_MFPS		0x0040	/* MF Preamble Suppression */
#define		MII_STATUS_ANC		0x0020	/* Auto-Negotiation Completed */
#define		MII_STATUS_RF		0x0010	/* Remote Fault Detected */
#define		MII_STATUS_ANA		0x0008	/* Auto-Negotiation Ability */
#define		MII_STATUS_LS		0x0004	/* Link Up */
#define		MII_STATUS_JD		0x0002	/* Jabber Condition Detected */
#define		MII_STATUS_EC		0x0001	/* Ext Register Capabilities */
#define MII_PHYID_H	0x2	/* PHY ID (high) */
#define MII_PHYID_L	0x3	/* PHY ID (low) */
#define MII_ANA		0x4	/* Auto-Negotiation Advertisement */
#define		MII_ANA_NP	0x8000	/* Next PAge */
#define		MII_ANA_RES	0x4000	/* Reserved */
#define		MII_ANA_RF	0x2000	/* Remote Fault */
#define		MII_ANA_TAF_M	0x1FE0	/* Technology Ability Field */
#define		MII_ANA_TAF_S	5	/* Shift */
#define			MII_ANA_TAF_RES		0x1000	/* Reserved */
#define			MII_ANA_PAUSE_ASYM	0x0800	/* Asym. Pause */
#define			MII_ANA_PAUSE_SYM	0x0400	/* Sym. Pause */
#define			MII_ANA_100T4		0x0200	/* 100Base-T4 */
#define			MII_ANA_100TXFD		0x0100	/* 100Base-TX FD */
#define			MII_ANA_100TXHD		0x0080	/* 100Base-TX HD */
#define			MII_ANA_10TFD		0x0040	/* 10Base-T FD */
#define			MII_ANA_10THD		0x0020	/* 10Base-T HD */
#define		MII_ANA_SEL_M	0x001F	/* Selector Field */
#define			MII_ANA_SEL_802_3 0x0001 /* 802.3 */
#define MII_ANLPA	0x5	/* Auto-Neg Link Partner Ability Register */
#define		MII_ANLPA_NP	0x8000	/* Next Page */
#define		MII_ANLPA_ACK	0x4000	/* Acknowledge */
#define		MII_ANLPA_RF	0x2000	/* Remote Fault */
#define		MII_ANLPA_TAF_M	0x1FC0	/* Technology Ability Field */
#define		MII_ANLPA_SEL_M	0x001F	/* Selector Field */
#define MII_ANE		0x6	/* Auto-Negotiation Expansion */
#define		MII_ANE_RES	0xFFE0	/* Reserved */
#define		MII_ANE_PDF	0x0010	/* Parallel Detection Fault */
#define		MII_ANE_LPNPA	0x0008	/* Link Partner is Next Page Able */
#define		MII_ANE_NPA	0x0002	/* Local Device is Next Page Able */
#define		MII_ANE_PR	0x0002	/* New Page has been received */
#define		MII_ANE_LPANA	0x0001	/* Link Partner is Auto-Neg.able */
#define MII_ANNPT	0x7	/* Auto-Negotiation Next Page Transmit */
#define MII_ANLPRNP	0x8	/* Auto-Neg Link Partner Received Next Page */
#define MII_1000_CTRL	0x9	/* 1000BASE-T Control Register */
#define		MII_1000C_FULL	0x0200	/* Advertise 1000BASE-T full duplex */
#define		MII_1000C_HALF	0x0100	/* Advertise 1000BASE-T half duplex */
#define MII_1000_STATUS	0xA	/* 1000BASE-T Status Register */
#define		MII_1000S_LRXOK	0x2000	/* Link partner local receiver status */
#define		MII_1000S_RRXOK	0x1000	/* Link partner remote receiver status */
#define		MII_1000S_FULL	0x0800	/* Link partner 1000BASE-T full duplex */
#define		MII_1000S_HALF	0x0400	/* Link partner 1000BASE-T half duplex */
/* 0xB ... 0xE */		/* Reserved */
#define MII_EXT_STATUS	0xF	/* Extended Status */
#define		MII_ESTAT_1000XFD	0x8000	/* 1000Base-X Full Duplex */
#define		MII_ESTAT_1000XHD	0x4000	/* 1000Base-X Half Duplex */
#define		MII_ESTAT_1000TFD	0x2000	/* 1000Base-T Full Duplex */
#define		MII_ESTAT_1000THD	0x1000	/* 1000Base-T Half Duplex */
#define		MII_ESTAT_RES		0x0FFF	/* Reserved */
/* 0x10 ... 0x1F */		/* Vendor Specific */
