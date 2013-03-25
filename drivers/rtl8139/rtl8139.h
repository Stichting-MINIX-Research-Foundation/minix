/*
ibm/rtl8139.h

Created:	Aug 2003 by Philip Homburg <philip@cs.vu.nl>
*/

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <minix/com.h>
#include <minix/ds.h>
#include <minix/keymap.h>
#include <minix/syslib.h>
#include <minix/type.h>
#include <minix/sysutil.h>
#include <minix/endpoint.h>
#include <minix/timers.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <machine/pci.h>

#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/ioc_memory.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/type.h"

#define	RL_IDR		0x00	/* Ethernet address
				 * Note: RL_9346CR_EEM_CONFIG mode is
				 * required the change the ethernet
				 * address.
				 * Note: 4-byte write access only.
				 */
#define		RL_N_TX		4	/* Number of transmit buffers */
#define	RL_TSD0	0x010	/* Transmit Status of Descriptor 0 */
#define		RL_TSD_CRS	0x80000000 /* Carrier Sense Lost */
#define		RL_TSD_TABT	0x40000000 /* Transmit Abort */
#define		RL_TSD_OWC	0x20000000 /* Out of Window Collision */
#define		RL_TSD_CDH	0x10000000 /* CD Heart Beat */
#define		RL_TSD_NCC_M	0x0F000000 /* Number of Collision Count */
#define		RL_TSD_RES	0x00C00000 /* Reserved */
#define		RL_TSD_ERTXTH_M	0x003F0000 /* Early Tx Threshold */
#define			RL_TSD_ERTXTH_S		16		/* shift */
#define			RL_TSD_ERTXTH_8		0x00000000	/* 8 bytes */
#define		RL_TSD_TOK	0x00008000 /* Transmit OK */
#define		RL_TSD_TUN	0x00004000 /* Transmit FIFO Underrun */
#define		RL_TSD_OWN	0x00002000 /* Controller (does not) Own Buf. */
#define		RL_TSD_SIZE	0x00001FFF /* Descriptor Size */
#define RL_TSAD0	0x20	/* Transmit Start Address of Descriptor 0 */
#define RL_RBSTART	0x30	/* Receive Buffer Start Address */
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
#define	RL_CAPR		0x38	/* Current Address of Packet Read */
#define		RL_CAPR_DATA_OFF	0x10	/* Packet Starts at Offset */
#define	RL_CBR		0x3A	/* Current Buffer Address */
#define	RL_IMR		0x3C	/* Interrupt Mask Register */
#define		RL_IMR_SERR	0x8000	/* System Error */
#define		RL_IMR_TIMEOUT	0x4000	/* Time Out */
#define		RL_IMR_LENCHG	0x2000	/* Cable Length Change */
#define		RL_IMR_RES	0x1F80	/* Reserved */
#define		RL_IMR_FOVW	0x0040	/* Rx FIFO Overflow */
#define		RL_IMR_PUN	0x0020	/* Packet Underrun / Link Change */
#define		RL_IMR_RXOVW	0x0010	/* Rx Buffer Overflow */
#define		RL_IMR_TER	0x0008	/* Transmit Error */
#define		RL_IMR_TOK	0x0004	/* Transmit OK */
#define		RL_IMR_RER	0x0002	/* Receive Error */
#define		RL_IMR_ROK	0x0001	/* Receive OK */
#define	RL_ISR		0x3E	/* Interrupt Status Register */
#define		RL_ISR_SERR	0x8000	/* System Error */
#define		RL_ISR_TIMEOUT	0x4000	/* Time Out */
#define		RL_ISR_LENCHG	0x2000	/* Cable Length Change */
#define		RL_ISR_RES	0x1F80	/* Reserved */
#define		RL_ISR_FOVW	0x0040	/* Rx FIFO Overflow */
#define		RL_ISR_PUN	0x0020	/* Packet Underrun / Link Change */
#define		RL_ISR_RXOVW	0x0010	/* Rx Buffer Overflow */
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
#if 0
#undef RL_TCR_IFG_STD
#define			RL_TCR_IFG_STD		0x00000000 
#endif
#define		RL_TCR_HWVER_BM	0x00C00000 /* Hardware Version ID B */
#define			RL_TCR_HWVER_RTL8139	0x60000000 /* RTL8139 */
#define			RL_TCR_HWVER_RTL8139A	0x70000000 /* RTL8139A */
#define			RL_TCR_HWVER_RTL8139AG	0x74000000 /* RTL8139A-G */
#define			RL_TCR_HWVER_RTL8139B	0x78000000 /* RTL8139B */
#define			RL_TCR_HWVER_RTL8130	0x78000000 /* RTL8130 (dup) */
#define			RL_TCR_HWVER_RTL8139C	0x74000000 /* RTL8139C (dup) */
#define			RL_TCR_HWVER_RTL8100	0x78800000 /* RTL8100 */
#define			RL_TCR_HWVER_RTL8100B	0x74400000 /* RTL8100B /
								RTL8139D */
#define			RL_TCR_HWVER_RTL8139CP	0x74800000 /* RTL8139C+ */
#define			RL_TCR_HWVER_RTL8101	0x74C00000 /* RTL8101 */
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
			/* Note: the documentation for the RTL8139C(L) or
			 * for the RTL8139D(L) claims that the buffer should
			 * be 16 bytes larger. Multiples of 8KB are the 
			 * correct values.
			 */
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
#define		RL_RCR_RES2	0x00000040 /* EEPROM type? */
#define		RL_RCR_AER	0x00000020 /* Accept Error Packets */
#define		RL_RCR_AR	0x00000010 /* Accept Runt Packets */
#define		RL_RCR_AB	0x00000008 /* Accept Broadcast Packets */
#define		RL_RCR_AM	0x00000004 /* Accept Multicast Packets */
#define		RL_RCR_APM	0x00000002 /* Accept Physical Match Packets */
#define		RL_RCR_AAP	0x00000001 /* Accept All Packets */
#define	RL_MPC		0x4c	/* Missed Packet Counter */
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
#define RL_CONFIG1	0x52	/* Configuration Register 1 */
#define RL_MSR		0x58	/* Media Status Register */
#define		RL_MSR_TXFCE	0x80	/* Tx Flow Control Enable */
#define		RL_MSR_RXFCE	0x40	/* Rx Flow Control Enable */
#define		RL_MSR_RES	0x20	/* Reserved */
#define		RL_MSR_AUXSTAT	0x10	/* Aux. Power Present */
#define		RL_MSR_SPEED_10	0x08	/* In 10 Mbps mode */
#define		RL_MSR_LINKB	0x04	/* link Failed */
#define		RL_MSR_TXPF	0x02	/* Sent Pause Packet */
#define		RL_MSR_RXPF	0x01	/* Received Pause Packet */
#define RL_CONFIG3	0x59	/* Configuration Register 3 */
#define RL_CONFIG4	0x5A	/* Configuration Register 4 */
/*			0x5B */	/* Reserved */
#define RL_REVID	0x5E	/* PCI Revision ID */
/*			0x5F */	/* Reserved */
#define RL_TSAD		0x60	/* Transmit Status of All Descriptors */
#define		RL_TSAD_TOK3	0x8000	/* TOK bit of Descriptor 3 */
#define		RL_TSAD_TOK2	0x4000	/* TOK bit of Descriptor 2 */
#define		RL_TSAD_TOK1	0x2000	/* TOK bit of Descriptor 1 */
#define		RL_TSAD_TOK0	0x1000	/* TOK bit of Descriptor 0 */
#define		RL_TSAD_TUN3	0x0800	/* TUN bit of Descriptor 3 */
#define		RL_TSAD_TUN2	0x0400	/* TUN bit of Descriptor 2 */
#define		RL_TSAD_TUN1	0x0200	/* TUN bit of Descriptor 1 */
#define		RL_TSAD_TUN0	0x0100	/* TUN bit of Descriptor 0 */
#define		RL_TSAD_TABT3	0x0080	/* TABT bit of Descriptor 3 */
#define		RL_TSAD_TABT2	0x0040	/* TABT bit of Descriptor 2 */
#define		RL_TSAD_TABT1	0x0020	/* TABT bit of Descriptor 1 */
#define		RL_TSAD_TABT0	0x0010	/* TABT bit of Descriptor 0 */
#define		RL_TSAD_OWN3	0x0008	/* OWN bit of Descriptor 3 */
#define		RL_TSAD_OWN2	0x0004	/* OWN bit of Descriptor 2 */
#define		RL_TSAD_OWN1	0x0002	/* OWN bit of Descriptor 1 */
#define		RL_TSAD_OWN0	0x0001	/* OWN bit of Descriptor 0 */
#define RL_BMCR		0x62	/* Basic Mode Control Register (MII_CTRL) */
#define RL_BMSR		0x64	/* Basic Mode Status Register (MII_STATUS) */
#define	RL_ANAR		0x66	/* Auto-Neg Advertisement Register (MII_ANA) */
#define	RL_ANLPAR	0x68	/* Auto-Neg Link Partner Register (MII_ANLPA) */
#define	RL_ANER		0x6a	/* Auto-Neg Expansion Register (MII_ANE) */
#define	RL_NWAYTR	0x70	/* N-way Test Register */
#define	RL_CSCR		0x74	/* CS Configuration Register */
#define RL_CONFIG5	0xD8	/* Configuration Register 5 */

/* Status word in receive buffer */
#define RL_RXS_LEN_M	0xFFFF0000	/* Length Field, Excl. Status word */
#define RL_RXS_LEN_S	16		/* Shift For Length */
#define RL_RXS_MAR	0x00008000	/* Multicast Address Received */
#define RL_RXS_PAR	0x00004000	/* Physical Address Matched */
#define RL_RXS_BAR	0x00002000	/* Broadcast Address Received */
#define RL_RXS_RES_M	0x00001FC0	/* Reserved */
#define RL_RXS_ISE	0x00000020	/* Invalid Symbol Error */
#define RL_RXS_RUNT	0x00000010	/* Runt Packet Received */
#define RL_RXS_LONG	0x00000008	/* Long (>4KB) Packet */
#define RL_RXS_CRC	0x00000004	/* CRC Error */
#define RL_RXS_FAE	0x00000002	/* Frame Alignment Error */
#define RL_RXS_ROK	0x00000001	/* Receive OK */

/* Registers in the Machine Independent Interface (MII) to the PHY.
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
#define		MII_ANA_TAF_M	0x1FE0	 /* Technology Ability Field */
#define		MII_ANA_TAF_S	5	 /* Shift */
#define			MII_ANA_TAF_RES		0x1000	/* Reserved */
#define			MII_ANA_PAUSE_ASYM	0x0800	/* Asym. Pause */
#define			MII_ANA_PAUSE_SYM	0x0400	/* Sym. Pause */
#define			MII_ANA_100T4		0x0200	/* 100Base-T4 */
#define			MII_ANA_100TXFD		0x0100	/* 100Base-TX FD */
#define			MII_ANA_100TXHD		0x0080	/* 100Base-TX HD */
#define			MII_ANA_10TFD		0x0040	/* 10Base-T FD */
#define			MII_ANA_10THD		0x0020	/* 10Base-T HD */
#define		MII_ANA_SEL_M	0x001F	 /* Selector Field */
#define			MII_ANA_SEL_802_3 0x0001 /* 802.3 */
#define MII_ANLPA	0x5	/* Auto-Neg Link Partner Ability Register */
#define		MII_ANLPA_NP	0x8000	/* Next Page */
#define		MII_ANLPA_ACK	0x4000	/* Acknowledge */
#define		MII_ANLPA_RF	0x2000	/* Remote Fault */
#define		MII_ANLPA_TAF_M	0x1FC0	 /* Technology Ability Field */
#define		MII_ANLPA_SEL_M	0x001F	 /* Selector Field */
#define MII_ANE		0x6	/* Auto-Negotiation Expansion */
#define		MII_ANE_RES	0xFFE0	/* Reserved */
#define		MII_ANE_PDF	0x0010	/* Parallel Detection Fault */
#define		MII_ANE_LPNPA	0x0008	/* Link Partner is Next Page Able */
#define		MII_ANE_NPA	0x0002	/* Local Device is Next Page Able */
#define		MII_ANE_PR	0x0002	/* New Page has been received */
#define		MII_ANE_LPANA	0x0001	/* Link Partner is Auto-Neg.able */
#define MII_ANNPT	0x7	/* Auto-Negotiation Next Page Transmit */
#define MII_ANLPRNP	0x8	/* Auto-Neg Link Partner Received Next Page */
#define MII_MS_CTRL	0x9	/* MASTER-SLAVE Control Register */
#define MII_MS_STATUS	0xA	/* MASTER-SLAVE Status Register */
/* 0xB ... 0xE */		/* Reserved */
#define MII_EXT_STATUS	0xF	/* Extended Status */
#define		MII_ESTAT_1000XFD	0x8000	/* 1000Base-X Full Duplex */
#define		MII_ESTAT_1000XHD	0x4000	/* 1000Base-X Half Duplex */
#define		MII_ESTAT_1000TFD	0x2000	/* 1000Base-T Full Duplex */
#define		MII_ESTAT_1000THD	0x1000	/* 1000Base-T Half Duplex */
#define		MII_ESTAT_RES		0x0FFF	/* Reserved */
/* 0x10 ... 0x1F */		/* Vendor Specific */

#if 0
34-35	R	ERBCR		Early Receive (Rx) Byte Count Register
36	R	ERSR		Early Rx Status Register
	7-4			reserved
	3	R	ERGood	Early Rx Good packet
	2	R	ERBad	Early Rx Bad packet
	1	R	EROVW	Early Rx OverWrite
	0	R	EROK	Early Rx OK
51	R/W	CONFIG0		Configuration Register 0
	7	R	SCR	Scrambler Mode
	6	R	PCS	PCS Mode
	5	R	T10	10 Mbps Mode
	4-3	R	PL[1-0]	Select 10 Mbps medium type
	2-0	R	BS[2-0]	Select Boot ROM size
52	R/W	CONFIG1		Configuration Register 1
	7-6	R/W	LEDS[1-0] LED PIN
	5	R/W	DVRLOAD	Driver Load
	4	R/W	LWACT	LWAKE active mode
	3	R	MEMMAP	Memory Mapping
	2	R	IOMAP	I/O Mapping
	1	R/W	VPD	Set to enable Vital Product Data
	0	R/W	PMEn	Power Management Enable
59	R/W	CONFIG3		Configuration Register 3
	7	R	GNTSel	Gnt Select
	6	R/W	PARM_En	Parameter Enable
	5	R/W	Magic	Magic Packet
	4	R/W	LinkUp	Link Up
	3			reserved
	2	R	CLKRUN_En CLKRUN Enable
	1			reserved
	0	R	FBtBEn	Fast Back to Back Enable
5a	R/W	CONFIG4		Configuration Register 4
	7	R/W	RxFIFOAutoClr Auto Clear the Rx FIFO on overflow
	6	R/W	AnaOff	Analog Power Off
	5	R/W	LongWF	Long Wake-up Frame
	4	R/W	LWPME	LANWAKE vs PMEB
	3			reserved
	2	R/W	LWPTN	LWAKE pattern
	1			reserved
	0	R/W	PBWakeup Pre-Boot Wakeup
5c-5d	R/W	MULINT		Multiple Interrupt Select
	15-12			reserved
	11-0	R/W	MISR[11-0] Multiple Interrupt Select
68-69	R	ANLPAR		Auto-Negotiation Link Partnet Register
	15	R	NP	Next Page bit
	14	R	ACK	acknowledge received from link partner
	13	R/W	RF	received remote fault detection capability
	12-11			reserved
	10	R	Pause	Flow control is supported
	9	R	T4	100Base-T4 is supported
	8	R/W	TXFD	100Base-TX full duplex is supported
	7	R/W	TX	100Base-TX is supported
	6	R/W	10FD	10Base-T full duplex is supported
	5	R/W	10	10Base-T is supported
	4-0	R/W	Selector Binary encoded selector
6a-6b	R	ANER		Auto-Negotiation Expansion Register
	15-5			reserved
	4	R	MLF	Multiple link fault occured
	3	R	LP_NP_ABLE Link partner supports Next Page
	2	R	NP_ABLE	Local node is able to send add. Next Pages
	1	R	PAGE_RX	Link Code Word Page received
	0	R	LP_NW_ABLE Link partner supports NWay auto-negotiation
70-71	R/W	NWAYTR		N-way Test Register
	15-8			reserved
	7	R/W	NWLPBK	NWay loopback mode
	6-4			reserved
	3	R	ENNWLE	LED0 pin indicates linkpulse
	2	R	FLAGABD	Auto-neg experienced ability detect state
	1	R	FLAGPDF	Auto-neg exp. par. detection fault state
	0	R	FLAGLSC	Auto-neg experienced link status check state
74-75	R/W	CSCR		CS Configuration Register
	15	W	Testfun	Auto-neg speeds up internal timer
	14-10			reserved
	9	R/W	LD	Active low TPI link disable signal
	8	R/W	HEARTBEAT HEART BEAT enable
	7	R/W	JBEN	Enable jabber function
	6	R/W	F_LINK_100 Force 100 Mbps
	5	R/W	F_Conect Bypass disconnect function
	4			reserved
	3	R	Con_status Connected link detected
	2	R/W	Con_status_En Configures LED1 to indicate conn. stat.
	1			reserved
	0	R/W	PASS_SCR Bypass scramble
76-77				reserved
78-7b	R/W	PHY1_PARM	PHY parameter 1
7c-7f	R/W	TW_PARM		Twister parameter
80	R/W	PHY2_PARM	PHY parameter 2
81-83				reserved
84-8b	R/W	CRC[0-7]	Power Management CRC reg.[0-7] for frame[0-7]
8c-cb	R/W	Wakeup[0-7]	Power Management wakeup frame[0-7] (64 bit)
cc-d3	R/W	LSBCRC[0-7]	LSB of the mask byte of makeup frame[0-7]
d4-d7				reserved
d8	R/W	Config5		Configuration register 5
	7			reserved
	6	R/W	BWF	Broadcast Wakeup Frame
	5	R/W	MWF	Multicast Wakeup Frame
	4	R/W	UWF	Unicast Wakeup Frame
	3	R/W	FifoAddrPtr FIFO Address Pointer
	2	R/W	LDPS	Link Down Power Saving mode
	1	R/W	LANWake	LANWake Signal
	0	R/W	PME_STS	PME_Status bit
d9-ff				reserved
#endif

#define Proc_number(p)		proc_number(p)
#define debug			0
#define printW()		((void)0)
#define vm_1phys2bus(p)		(p)

#define RX_BUFSIZE	RL_RCR_RBLEN_64K_SIZE
#define RX_BUFBITS	RL_RCR_RBLEN_64K
#define N_TX_BUF	RL_N_TX

/* I/O vectors are handled IOVEC_NR entries at a time. */
#define IOVEC_NR	16

/* Configuration */
#define RL_ENVVAR	"RTLETH"

typedef struct re
{
	port_t re_base_port;
	int re_irq;
	int re_mode;
	int re_flags;
	int re_client;
	int re_link_up;
	int re_got_int;
	int re_send_int;
	int re_report_link;
	int re_clear_rx;
	int re_need_reset;
	int re_tx_alive;
	char *re_model;

	/* Rx */
	phys_bytes re_rx_buf;
	char  *v_re_rx_buf;
	vir_bytes re_read_s;

	/* Tx */
	int re_tx_head;
	int re_tx_tail;
	struct
	{
		int ret_busy;
		phys_bytes ret_buf;
		char * v_ret_buf;
	} re_tx[N_TX_BUF];
	u32_t re_ertxth;	/* Early Tx Threshold */

	/* PCI related */
	int re_seen;			/* TRUE iff device available */

	/* 'large' items */
	int re_hook_id;			/* IRQ hook id at kernel */
	eth_stat_t re_stat;
	ether_addr_t re_address;
	message re_rx_mess;
	message re_tx_mess;
	char re_name[sizeof("rtl8139#n")];
	iovec_t re_iovec[IOVEC_NR];
	iovec_s_t re_iovec_s[IOVEC_NR];
}
re_t;

#define REM_DISABLED	0x0
#define REM_ENABLED	0x1

#define REF_PACK_SENT	0x001
#define REF_PACK_RECV	0x002
#define REF_SEND_AVAIL	0x004
#define REF_READING	0x010
#define REF_EMPTY	0x000
#define REF_PROMISC	0x040
#define REF_MULTI	0x080
#define REF_BROAD	0x100
#define REF_ENABLED	0x200

/*
 * $PchId: rtl8139.h,v 1.1 2003/09/05 10:58:50 philip Exp $
 */
