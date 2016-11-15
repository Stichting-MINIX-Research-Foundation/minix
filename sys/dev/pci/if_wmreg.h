/*	$NetBSD: if_wmreg.h,v 1.82 2015/10/08 04:30:25 msaitoh Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************

  Copyright (c) 2001-2012, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

/*
 * Register description for the Intel i82542 (``Wiseman''),
 * i82543 (``Livengood''), and i82544 (``Cordova'') Gigabit
 * Ethernet chips.
 */

/*
 * The wiseman supports 64-bit PCI addressing.  This structure
 * describes the address in descriptors.
 */
typedef struct wiseman_addr {
	uint32_t	wa_low;		/* low-order 32 bits */
	uint32_t	wa_high;	/* high-order 32 bits */
} __packed wiseman_addr_t;

/*
 * The Wiseman receive descriptor.
 *
 * The receive descriptor ring must be aligned to a 4K boundary,
 * and there must be an even multiple of 8 descriptors in the ring.
 */
typedef struct wiseman_rxdesc {
	volatile wiseman_addr_t	wrx_addr;	/* buffer address */

	volatile uint16_t	wrx_len;	/* buffer length */
	volatile uint16_t	wrx_cksum;	/* checksum (starting at PCSS)*/

	volatile uint8_t	wrx_status;	/* Rx status */
	volatile uint8_t	wrx_errors;	/* Rx errors */
	volatile uint16_t	wrx_special;	/* special field (VLAN, etc.) */
} __packed wiseman_rxdesc_t;

/* wrx_status bits */
#define	WRX_ST_DD	(1U << 0)	/* descriptor done */
#define	WRX_ST_EOP	(1U << 1)	/* end of packet */
#define	WRX_ST_IXSM	(1U << 2)	/* ignore checksum indication */
#define	WRX_ST_VP	(1U << 3)	/* VLAN packet */
#define	WRX_ST_BPDU	(1U << 4)	/* ??? */
#define	WRX_ST_TCPCS	(1U << 5)	/* TCP checksum performed */
#define	WRX_ST_IPCS	(1U << 6)	/* IP checksum performed */
#define	WRX_ST_PIF	(1U << 7)	/* passed in-exact filter */

/* wrx_error bits */
#define	WRX_ER_CE	(1U << 0)	/* CRC error */
#define	WRX_ER_SE	(1U << 1)	/* symbol error */
#define	WRX_ER_SEQ	(1U << 2)	/* sequence error */
#define	WRX_ER_ICE	(1U << 3)	/* ??? */
#define	WRX_ER_CXE	(1U << 4)	/* carrier extension error */
#define	WRX_ER_TCPE	(1U << 5)	/* TCP checksum error */
#define	WRX_ER_IPE	(1U << 6)	/* IP checksum error */
#define	WRX_ER_RXE	(1U << 7)	/* Rx data error */

/* wrx_special field for VLAN packets */
#define	WRX_VLAN_ID(x)	((x) & 0x0fff)	/* VLAN identifier */
#define	WRX_VLAN_CFI	(1U << 12)	/* Canonical Form Indicator */
#define	WRX_VLAN_PRI(x)	(((x) >> 13) & 7)/* VLAN priority field */

/*
 * The Wiseman transmit descriptor.
 *
 * The transmit descriptor ring must be aligned to a 4K boundary,
 * and there must be an even multiple of 8 descriptors in the ring.
 */
typedef struct wiseman_tx_fields {
	uint8_t wtxu_status;		/* Tx status */
	uint8_t wtxu_options;		/* options */
	uint16_t wtxu_vlan;		/* VLAN info */
} __packed wiseman_txfields_t;
typedef struct wiseman_txdesc {
	wiseman_addr_t	wtx_addr;	/* buffer address */
	uint32_t	wtx_cmdlen;	/* command and length */
	wiseman_txfields_t wtx_fields;	/* fields; see below */
} __packed wiseman_txdesc_t;

/* Commands for wtx_cmdlen */
#define	WTX_CMD_EOP	(1U << 24)	/* end of packet */
#define	WTX_CMD_IFCS	(1U << 25)	/* insert FCS */
#define	WTX_CMD_RS	(1U << 27)	/* report status */
#define	WTX_CMD_RPS	(1U << 28)	/* report packet sent */
#define	WTX_CMD_DEXT	(1U << 29)	/* descriptor extension */
#define	WTX_CMD_VLE	(1U << 30)	/* VLAN enable */
#define	WTX_CMD_IDE	(1U << 31)	/* interrupt delay enable */

/* Descriptor types (if DEXT is set) */
#define	WTX_DTYP_C	(0U << 20)	/* context */
#define	WTX_DTYP_D	(1U << 20)	/* data */

/* wtx_fields status bits */
#define	WTX_ST_DD	(1U << 0)	/* descriptor done */
#define	WTX_ST_EC	(1U << 1)	/* excessive collisions */
#define	WTX_ST_LC	(1U << 2)	/* late collision */
#define	WTX_ST_TU	(1U << 3)	/* transmit underrun */

/* wtx_fields option bits for IP/TCP/UDP checksum offload */
#define	WTX_IXSM	(1U << 0)	/* IP checksum offload */
#define	WTX_TXSM	(1U << 1)	/* TCP/UDP checksum offload */

/* Maximum payload per Tx descriptor */
#define	WTX_MAX_LEN	4096

/*
 * The Livengood TCP/IP context descriptor.
 */
struct livengood_tcpip_ctxdesc {
	uint32_t	tcpip_ipcs;	/* IP checksum context */
	uint32_t	tcpip_tucs;	/* TCP/UDP checksum context */
	uint32_t	tcpip_cmdlen;
	uint32_t	tcpip_seg;	/* TCP segmentation context */
};

/* commands for context descriptors */
#define	WTX_TCPIP_CMD_TCP	(1U << 24)	/* 1 = TCP, 0 = UDP */
#define	WTX_TCPIP_CMD_IP	(1U << 25)	/* 1 = IPv4, 0 = IPv6 */
#define	WTX_TCPIP_CMD_TSE	(1U << 26)	/* segmentation context valid */

#define	WTX_TCPIP_IPCSS(x)	((x) << 0)	/* checksum start */
#define	WTX_TCPIP_IPCSO(x)	((x) << 8)	/* checksum value offset */
#define	WTX_TCPIP_IPCSE(x)	((x) << 16)	/* checksum end */

#define	WTX_TCPIP_TUCSS(x)	((x) << 0)	/* checksum start */
#define	WTX_TCPIP_TUCSO(x)	((x) << 8)	/* checksum value offset */
#define	WTX_TCPIP_TUCSE(x)	((x) << 16)	/* checksum end */

#define	WTX_TCPIP_SEG_STATUS(x)	((x) << 0)
#define	WTX_TCPIP_SEG_HDRLEN(x)	((x) << 8)
#define	WTX_TCPIP_SEG_MSS(x)	((x) << 16)

/*
 * PCI config registers used by the Wiseman.
 */
#define	WM_PCI_MMBA	PCI_MAPREG_START
/* registers for FLASH access on ICH8 */
#define WM_ICH8_FLASH	0x0014

/*
 * Wiseman Control/Status Registers.
 */
#define	WMREG_CTRL	0x0000	/* Device Control Register */
#define	CTRL_FD		(1U << 0)	/* full duplex */
#define	CTRL_BEM	(1U << 1)	/* big-endian mode */
#define	CTRL_PRIOR	(1U << 2)	/* 0 = receive, 1 = fair */
#define	CTRL_GIO_M_DIS	(1U << 2)	/* disabl PCI master access */
#define	CTRL_LRST	(1U << 3)	/* link reset */
#define	CTRL_ASDE	(1U << 5)	/* auto speed detect enable */
#define	CTRL_SLU	(1U << 6)	/* set link up */
#define	CTRL_ILOS	(1U << 7)	/* invert loss of signal */
#define	CTRL_SPEED(x)	((x) << 8)	/* speed (Livengood) */
#define	CTRL_SPEED_10	CTRL_SPEED(0)
#define	CTRL_SPEED_100	CTRL_SPEED(1)
#define	CTRL_SPEED_1000	CTRL_SPEED(2)
#define	CTRL_SPEED_MASK	CTRL_SPEED(3)
#define	CTRL_FRCSPD	(1U << 11)	/* force speed (Livengood) */
#define	CTRL_FRCFDX	(1U << 12)	/* force full-duplex (Livengood) */
#define CTRL_D_UD_EN	(1U << 13)	/* Dock/Undock enable */
#define CTRL_D_UD_POL	(1U << 14)	/* Defined polarity of Dock/Undock indication in SDP[0] */
#define CTRL_F_PHY_R 	(1U << 15)	/* Reset both PHY ports, through PHYRST_N pin */
#define CTRL_EXT_LINK_EN (1U << 16)	/* enable link status from external LINK_0 and LINK_1 pins */
#define CTRL_LANPHYPC_OVERRIDE (1U << 16) /* SW control of LANPHYPC */
#define CTRL_LANPHYPC_VALUE (1U << 17)	/* SW value of LANPHYPC */
#define	CTRL_SWDPINS_SHIFT	18
#define	CTRL_SWDPINS_MASK	0x0f
#define	CTRL_SWDPIN(x)		(1U << (CTRL_SWDPINS_SHIFT + (x)))
#define	CTRL_SWDPIO_SHIFT	22
#define	CTRL_SWDPIO_MASK	0x0f
#define	CTRL_SWDPIO(x)		(1U << (CTRL_SWDPIO_SHIFT + (x)))
#define CTRL_MEHE	(1U << 17)	/* Memory Error Handling Enable(I217)*/
#define	CTRL_RST	(1U << 26)	/* device reset */
#define	CTRL_RFCE	(1U << 27)	/* Rx flow control enable */
#define	CTRL_TFCE	(1U << 28)	/* Tx flow control enable */
#define	CTRL_VME	(1U << 30)	/* VLAN Mode Enable */
#define	CTRL_PHY_RESET	(1U << 31)	/* PHY reset (Cordova) */

#define	WMREG_CTRL_SHADOW 0x0004	/* Device Control Register (shadow) */

#define	WMREG_STATUS	0x0008	/* Device Status Register */
#define	STATUS_FD	(1U << 0)	/* full duplex */
#define	STATUS_LU	(1U << 1)	/* link up */
#define	STATUS_TCKOK	(1U << 2)	/* Tx clock running */
#define	STATUS_RBCOK	(1U << 3)	/* Rx clock running */
#define	STATUS_FUNCID_SHIFT 2		/* 82546 function ID */
#define	STATUS_FUNCID_MASK  3		/* ... */
#define	STATUS_TXOFF	(1U << 4)	/* Tx paused */
#define	STATUS_TBIMODE	(1U << 5)	/* fiber mode (Livengood) */
#define	STATUS_SPEED(x)	((x) << 6)	/* speed indication */
#define	STATUS_SPEED_10	  STATUS_SPEED(0)
#define	STATUS_SPEED_100  STATUS_SPEED(1)
#define	STATUS_SPEED_1000 STATUS_SPEED(2)
#define	STATUS_ASDV(x)	((x) << 8)	/* auto speed det. val. (Livengood) */
#define	STATUS_LAN_INIT_DONE (1U << 9)	/* Lan Init Completion by NVM */
#define	STATUS_MTXCKOK	(1U << 10)	/* MTXD clock running */
#define	STATUS_PHYRA	(1U << 10)	/* PHY Reset Asserted (PCH) */
#define	STATUS_PCI66	(1U << 11)	/* 66MHz bus (Livengood) */
#define	STATUS_BUS64	(1U << 12)	/* 64-bit bus (Livengood) */
#define	STATUS_PCIX_MODE (1U << 13)	/* PCIX mode (Cordova) */
#define	STATUS_PCIXSPD(x) ((x) << 14)	/* PCIX speed indication (Cordova) */
#define	STATUS_PCIXSPD_50_66   STATUS_PCIXSPD(0)
#define	STATUS_PCIXSPD_66_100  STATUS_PCIXSPD(1)
#define	STATUS_PCIXSPD_100_133 STATUS_PCIXSPD(2)
#define	STATUS_PCIXSPD_MASK    STATUS_PCIXSPD(3)
#define	STATUS_GIO_M_ENA (1U << 19)	/* GIO master enable */
#define	STATUS_DEV_RST_SET (1U << 20)	/* Device Reset Set */

#define	WMREG_EECD	0x0010	/* EEPROM Control Register */
#define	EECD_SK		(1U << 0)	/* clock */
#define	EECD_CS		(1U << 1)	/* chip select */
#define	EECD_DI		(1U << 2)	/* data in */
#define	EECD_DO		(1U << 3)	/* data out */
#define	EECD_FWE(x)	((x) << 4)	/* flash write enable control */
#define	EECD_FWE_DISABLED EECD_FWE(1)
#define	EECD_FWE_ENABLED  EECD_FWE(2)
#define	EECD_EE_REQ	(1U << 6)	/* (shared) EEPROM request */
#define	EECD_EE_GNT	(1U << 7)	/* (shared) EEPROM grant */
#define	EECD_EE_PRES	(1U << 8)	/* EEPROM present */
#define	EECD_EE_SIZE	(1U << 9)	/* EEPROM size
					   (0 = 64 word, 1 = 256 word) */
#define	EECD_EE_AUTORD	(1U << 9)	/* auto read done */
#define	EECD_EE_ABITS	(1U << 10)	/* EEPROM address bits
					   (based on type) */
#define	EECD_EE_SIZE_EX_MASK __BITS(14,11) /* EEPROM size for new devices */
#define	EECD_EE_TYPE	(1U << 13)	/* EEPROM type
					   (0 = Microwire, 1 = SPI) */
#define EECD_SEC1VAL	(1U << 22)	/* Sector One Valid */
#define EECD_SEC1VAL_VALMASK (EECD_EE_AUTORD | EECD_EE_PRES) /* Valid Mask */

#define	WMREG_EERD	0x0014	/* EEPROM read */
#define	EERD_DONE	0x02    /* done bit */
#define	EERD_START	0x01	/* First bit for telling part to start operation */
#define	EERD_ADDR_SHIFT	2	/* Shift to the address bits */
#define	EERD_DATA_SHIFT	16	/* Offset to data in EEPROM read/write registers */

#define	WMREG_CTRL_EXT	0x0018	/* Extended Device Control Register */
#define	CTRL_EXT_NSICR		__BIT(0) /* Non Interrupt clear on read */
#define	CTRL_EXT_GPI_EN(x)	(1U << (x)) /* gpin interrupt enable */
#define	CTRL_EXT_SWDPINS_SHIFT	4
#define	CTRL_EXT_SWDPINS_MASK	0x0d
/* The bit order of the SW Definable pin is not 6543 but 3654! */
#define	CTRL_EXT_SWDPIN(x)	(1U << (CTRL_EXT_SWDPINS_SHIFT \
		+ ((x) == 3 ? 3 : ((x) - 4))))
#define	CTRL_EXT_SWDPIO_SHIFT	8
#define	CTRL_EXT_SWDPIO_MASK	0x0d
#define	CTRL_EXT_SWDPIO(x)	(1U << (CTRL_EXT_SWDPIO_SHIFT \
		+ ((x) == 3 ? 3 : ((x) - 4))))
#define	CTRL_EXT_ASDCHK		(1U << 12) /* ASD check */
#define	CTRL_EXT_EE_RST		(1U << 13) /* EEPROM reset */
#define	CTRL_EXT_IPS		(1U << 14) /* invert power state bit 0 */
#define	CTRL_EXT_SPD_BYPS	(1U << 15) /* speed select bypass */
#define	CTRL_EXT_IPS1		(1U << 16) /* invert power state bit 1 */
#define	CTRL_EXT_RO_DIS		(1U << 17) /* relaxed ordering disabled */
#define	CTRL_EXT_SDLPE		(1U << 18) /* SerDes Low Power Enable */
#define	CTRL_EXT_DMA_DYN_CLK	(1U << 19) /* DMA Dynamic Gating Enable */
#define	CTRL_EXT_LINK_MODE_MASK		0x00C00000
#define	CTRL_EXT_LINK_MODE_GMII		0x00000000
#define	CTRL_EXT_LINK_MODE_KMRN		0x00000000
#define	CTRL_EXT_LINK_MODE_1000KX	0x00400000
#define	CTRL_EXT_LINK_MODE_SGMII	0x00800000
#define	CTRL_EXT_LINK_MODE_PCIX_SERDES	0x00800000
#define	CTRL_EXT_LINK_MODE_TBI		0x00C00000
#define	CTRL_EXT_LINK_MODE_PCIE_SERDES	0x00C00000
#define	CTRL_EXT_PHYPDEN	0x00100000
#define	CTRL_EXT_EIAME		__BIT(24) /* Extended Interrupt Auto Mask En */
#define CTRL_EXT_I2C_ENA	0x02000000  /* I2C enable */
#define	CTRL_EXT_DRV_LOAD	0x10000000
#define	CTRL_EXT_PBA		__BIT(31) /* PBA Support */

#define	WMREG_MDIC	0x0020	/* MDI Control Register */
#define	MDIC_DATA(x)	((x) & 0xffff)
#define	MDIC_REGADD(x)	((x) << 16)
#define	MDIC_PHY_SHIFT	21
#define	MDIC_PHY_MASK	__BITS(25, 21)
#define	MDIC_PHYADD(x)	((x) << 21)
#define	MDIC_OP_WRITE	(1U << 26)
#define	MDIC_OP_READ	(2U << 26)
#define	MDIC_READY	(1U << 28)
#define	MDIC_I		(1U << 29)	/* interrupt on MDI complete */
#define	MDIC_E		(1U << 30)	/* MDI error */
#define	MDIC_DEST	(1U << 31)	/* Destination */

#define WMREG_SCTL	0x0024	/* SerDes Control - RW */
/*
 * These 4 macros are also used for other 8bit control registers on the
 * 82575
 */
#define SCTL_CTL_READY  (1U << 31)
#define SCTL_CTL_DATA_MASK 0x000000ff
#define SCTL_CTL_ADDR_SHIFT 8
#define SCTL_CTL_POLL_TIMEOUT 640
#define SCTL_DISABLE_SERDES_LOOPBACK 0x0400

#define	WMREG_FCAL	0x0028	/* Flow Control Address Low */
#define	FCAL_CONST	0x00c28001	/* Flow Control MAC addr low */

#define	WMREG_FCAH	0x002c	/* Flow Control Address High */
#define	FCAH_CONST	0x00000100	/* Flow Control MAC addr high */

#define	WMREG_FCT	0x0030	/* Flow Control Type */

#define	WMREG_KUMCTRLSTA 0x0034	/* MAC-PHY interface - RW */
#define	KUMCTRLSTA_MASK			0x0000FFFF
#define	KUMCTRLSTA_OFFSET		0x001F0000
#define	KUMCTRLSTA_OFFSET_SHIFT		16
#define	KUMCTRLSTA_REN			0x00200000

#define	KUMCTRLSTA_OFFSET_FIFO_CTRL	0x00000000
#define	KUMCTRLSTA_OFFSET_CTRL		0x00000001
#define	KUMCTRLSTA_OFFSET_INB_CTRL	0x00000002
#define	KUMCTRLSTA_OFFSET_DIAG		0x00000003
#define	KUMCTRLSTA_OFFSET_TIMEOUTS	0x00000004
#define	KUMCTRLSTA_OFFSET_K1_CONFIG	0x00000007
#define	KUMCTRLSTA_OFFSET_INB_PARAM	0x00000009
#define	KUMCTRLSTA_OFFSET_HD_CTRL	0x00000010
#define	KUMCTRLSTA_OFFSET_M2P_SERDES	0x0000001E
#define	KUMCTRLSTA_OFFSET_M2P_MODES	0x0000001F

/* FIFO Control */
#define	KUMCTRLSTA_FIFO_CTRL_RX_BYPASS	0x00000008
#define	KUMCTRLSTA_FIFO_CTRL_TX_BYPASS	0x00000800

/* In-Band Control */
#define	KUMCTRLSTA_INB_CTRL_LINK_TMOUT_DFLT 0x00000500
#define	KUMCTRLSTA_INB_CTRL_DIS_PADDING	0x00000010

/* Diag */
#define	KUMCTRLSTA_DIAG_NELPBK	0x1000

/* K1 Config */
#define	KUMCTRLSTA_K1_ENABLE	0x0002

/* Half-Duplex Control */
#define	KUMCTRLSTA_HD_CTRL_10_100_DEFAULT 0x00000004
#define	KUMCTRLSTA_HD_CTRL_1000_DEFAULT	0x00000000

#define	WMREG_VET	0x0038	/* VLAN Ethertype */
#define	WMREG_MDPHYA	0x003C	/* PHY address - RW */
#define	WMREG_RAL_BASE	0x0040	/* Receive Address List */
#define	WMREG_CORDOVA_RAL_BASE 0x5400
#define	WMREG_RAL_LO(b, x) ((b) + ((x) << 3))
#define	WMREG_RAL_HI(b, x) (WMREG_RAL_LO(b, x) + 4)
	/*
	 * Receive Address List: The LO part is the low-order 32-bits
	 * of the MAC address.  The HI part is the high-order 16-bits
	 * along with a few control bits.
	 */
#define	RAL_AS(x)	((x) << 16)	/* address select */
#define	RAL_AS_DEST	RAL_AS(0)	/* (cordova?) */
#define	RAL_AS_SOURCE	RAL_AS(1)	/* (cordova?) */
#define	RAL_RDR1	(1U << 30)	/* put packet in alt. rx ring */
#define	RAL_AV		(1U << 31)	/* entry is valid */

#define	WM_RAL_TABSIZE		15	/* RAL size for old devices */
#define	WM_RAL_TABSIZE_ICH8	7	/* RAL size for ICH* and PCH* */
#define	WM_RAL_TABSIZE_82575	16	/* RAL size for 82575 */
#define	WM_RAL_TABSIZE_82576	24	/* RAL size for 82576 and 82580 */
#define	WM_RAL_TABSIZE_I350	32	/* RAL size for I350 */

#define	WMREG_ICR	0x00c0	/* Interrupt Cause Register */
#define	ICR_TXDW	(1U << 0)	/* Tx desc written back */
#define	ICR_TXQE	(1U << 1)	/* Tx queue empty */
#define	ICR_LSC		(1U << 2)	/* link status change */
#define	ICR_RXSEQ	(1U << 3)	/* receive sequence error */
#define	ICR_RXDMT0	(1U << 4)	/* Rx ring 0 nearly empty */
#define	ICR_RXO		(1U << 6)	/* Rx overrun */
#define	ICR_RXT0	(1U << 7)	/* Rx ring 0 timer */
#define	ICR_MDAC	(1U << 9)	/* MDIO access complete */
#define	ICR_RXCFG	(1U << 10)	/* Receiving /C/ */
#define	ICR_GPI(x)	(1U << (x))	/* general purpose interrupts */
#define	ICR_RXQ0	__BIT(20)	/* 82574: Rx queue 0 interrupt */
#define	ICR_RXQ1	__BIT(21)	/* 82574: Rx queue 1 interrupt */
#define	ICR_TXQ0	__BIT(22)	/* 82574: Tx queue 0 interrupt */
#define	ICR_TXQ1	__BIT(23)	/* 82574: Tx queue 1 interrupt */
#define	ICR_OTHER	__BIT(24)	/* 82574: Other interrupt */
#define	ICR_INT		(1U << 31)	/* device generated an interrupt */

#define WMREG_ITR	0x00c4	/* Interrupt Throttling Register */
#define ITR_IVAL_MASK	0xffff		/* Interval mask */
#define ITR_IVAL_SHIFT	0		/* Interval shift */

#define	WMREG_ICS	0x00c8	/* Interrupt Cause Set Register */
	/* See ICR bits. */

#define WMREG_IVAR	0x00e4  /* Interrupt Vector Allocation Register */
#define WMREG_IVAR0	0x01700 /* Interrupt Vector Allocation */
#define IVAR_ALLOC_MASK  __BITS(0, 6)	/* Bit 5 and 6 are reserved */
#define IVAR_VALID       __BIT(7)
/* IVAR definitions for 82580 and newer */
#define WMREG_IVAR_Q(x)	(WMREG_IVAR0 + ((x) / 2) * 4)
#define IVAR_TX_MASK_Q(x) (0x000000ff << (((x) % 2) == 0 ? 8 : 24))
#define IVAR_RX_MASK_Q(x) (0x000000ff << (((x) % 2) == 0 ? 0 : 16))
/* IVAR definitions for 82576 */
#define WMREG_IVAR_Q_82576(x)	(WMREG_IVAR0 + ((x) & 0x7) * 4)
#define IVAR_TX_MASK_Q_82576(x) (0x000000ff << (((x) / 8) == 0 ? 8 : 24))
#define IVAR_RX_MASK_Q_82576(x) (0x000000ff << (((x) / 8) == 0 ? 0 : 16))
/* IVAR definitions for 82574 */
#define IVAR_ALLOC_MASK_82574	__BITS(0, 2)
#define IVAR_VALID_82574	__BIT(3)
#define IVAR_TX_MASK_Q_82574(x) (0x0000000f << ((x) == 0 ? 8 : 12))
#define IVAR_RX_MASK_Q_82574(x) (0x0000000f << ((x) == 0 ? 0 : 4))
#define IVAR_OTHER_MASK		__BITS(16, 19)
#define IVAR_INT_ON_ALL_WB	__BIT(31)

#define WMREG_IVAR_MISC	0x01740 /* IVAR for other causes */
#define IVAR_MISC_TCPTIMER __BITS(0, 7)
#define IVAR_MISC_OTHER	__BITS(8, 15)

#define	WMREG_IMS	0x00d0	/* Interrupt Mask Set Register */
	/* See ICR bits. */

#define	WMREG_IMC	0x00d8	/* Interrupt Mask Clear Register */
	/* See ICR bits. */

#define	WMREG_EIAC_82574 0x00dc	/* Interrupt Auto Clear Register */
#define	WMREG_EIAC_82574_MSIX_MASK	(ICR_RXQ0 | ICR_RXQ1		\
	| ICR_TXQ0 | ICR_TXQ1 | ICR_OTHER)

#define	WMREG_RCTL	0x0100	/* Receive Control */
#define	RCTL_EN		(1U << 1)	/* receiver enable */
#define	RCTL_SBP	(1U << 2)	/* store bad packets */
#define	RCTL_UPE	(1U << 3)	/* unicast promisc. enable */
#define	RCTL_MPE	(1U << 4)	/* multicast promisc. enable */
#define	RCTL_LPE	(1U << 5)	/* large packet enable */
#define	RCTL_LBM(x)	((x) << 6)	/* loopback mode */
#define	RCTL_LBM_NONE	RCTL_LBM(0)
#define	RCTL_LBM_PHY	RCTL_LBM(3)
#define	RCTL_RDMTS(x)	((x) << 8)	/* receive desc. min thresh size */
#define	RCTL_RDMTS_1_2	RCTL_RDMTS(0)
#define	RCTL_RDMTS_1_4	RCTL_RDMTS(1)
#define	RCTL_RDMTS_1_8	RCTL_RDMTS(2)
#define	RCTL_RDMTS_MASK	RCTL_RDMTS(3)
#define	RCTL_MO(x)	((x) << 12)	/* multicast offset */
#define	RCTL_BAM	(1U << 15)	/* broadcast accept mode */
#define	RCTL_2k		(0 << 16)	/* 2k Rx buffers */
#define	RCTL_1k		(1 << 16)	/* 1k Rx buffers */
#define	RCTL_512	(2 << 16)	/* 512 byte Rx buffers */
#define	RCTL_256	(3 << 16)	/* 256 byte Rx buffers */
#define	RCTL_BSEX_16k	(1 << 16)	/* 16k Rx buffers (BSEX) */
#define	RCTL_BSEX_8k	(2 << 16)	/* 8k Rx buffers (BSEX) */
#define	RCTL_BSEX_4k	(3 << 16)	/* 4k Rx buffers (BSEX) */
#define	RCTL_DPF	(1U << 22)	/* discard pause frames */
#define	RCTL_PMCF	(1U << 23)	/* pass MAC control frames */
#define	RCTL_BSEX	(1U << 25)	/* buffer size extension (Livengood) */
#define	RCTL_SECRC	(1U << 26)	/* strip Ethernet CRC */

#define	WMREG_OLD_RDTR0	0x0108	/* Receive Delay Timer (ring 0) */
#define	WMREG_RDTR	0x2820
#define	RDTR_FPD	(1U << 31)	/* flush partial descriptor */

#define WMREG_LTRC	0x01a0	/* Latency Tolerance Reportiong Control */

#define	WMREG_OLD_RDBAL0 0x0110	/* Receive Descriptor Base Low (ring 0) */
#define	WMREG_RDBAL	0x2800
#define	WMREG_RDBAL_2	0x0c00	/* for 82576 ... */

#define	WMREG_OLD_RDBAH0 0x0114	/* Receive Descriptor Base High (ring 0) */
#define	WMREG_RDBAH	0x2804
#define	WMREG_RDBAH_2	0x0c04	/* for 82576 ... */

#define	WMREG_OLD_RDLEN0 0x0118	/* Receive Descriptor Length (ring 0) */
#define	WMREG_RDLEN	0x2808
#define	WMREG_RDLEN_2	0x0c08	/* for 82576 ... */

#define WMREG_SRRCTL	0x280c	/* additional recv control used in 82575 ... */
#define WMREG_SRRCTL_2	0x0c0c	/* for 82576 ... */
#define SRRCTL_BSIZEPKT_MASK		0x0000007f
#define SRRCTL_BSIZEPKT_SHIFT		10	/* Shift _right_ */
#define SRRCTL_BSIZEHDRSIZE_MASK	0x00000f00
#define SRRCTL_BSIZEHDRSIZE_SHIFT	2	/* Shift _left_ */
#define SRRCTL_DESCTYPE_LEGACY		0x00000000
#define SRRCTL_DESCTYPE_ADV_ONEBUF	(1U << 25)
#define SRRCTL_DESCTYPE_HDR_SPLIT	(2U << 25)
#define SRRCTL_DESCTYPE_HDR_REPLICATION	(3U << 25)
#define SRRCTL_DESCTYPE_HDR_REPLICATION_LARGE_PKT (4U << 25)
#define SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS (5U << 25) /* 82575 only */
#define SRRCTL_DESCTYPE_MASK		(7U << 25)
#define SRRCTL_DROP_EN			0x80000000

#define	WMREG_OLD_RDH0	0x0120	/* Receive Descriptor Head (ring 0) */
#define	WMREG_RDH	0x2810
#define	WMREG_RDH_2	0x0c10	/* for 82576 ... */

#define	WMREG_OLD_RDT0	0x0128	/* Receive Descriptor Tail (ring 0) */
#define	WMREG_RDT	0x2818
#define	WMREG_RDT_2	0x0c18	/* for 82576 ... */

#define	WMREG_RXDCTL	0x2828	/* Receive Descriptor Control */
#define	WMREG_RXDCTL_2	0x0c28	/* for 82576 ... */
#define	RXDCTL_PTHRESH(x) ((x) << 0)	/* prefetch threshold */
#define	RXDCTL_HTHRESH(x) ((x) << 8)	/* host threshold */
#define	RXDCTL_WTHRESH(x) ((x) << 16)	/* write back threshold */
#define	RXDCTL_GRAN	(1U << 24)	/* 0 = cacheline, 1 = descriptor */
/* flags used starting with 82575 ... */
#define RXDCTL_QUEUE_ENABLE  0x02000000 /* Enable specific Tx Queue */
#define RXDCTL_SWFLSH        0x04000000 /* Rx Desc. write-back flushing */

#define	WMREG_OLD_RDTR1	0x0130	/* Receive Delay Timer (ring 1) */
#define	WMREG_OLD_RDBA1_LO 0x0138 /* Receive Descriptor Base Low (ring 1) */
#define	WMREG_OLD_RDBA1_HI 0x013c /* Receive Descriptor Base High (ring 1) */
#define	WMREG_OLD_RDLEN1 0x0140	/* Receive Drscriptor Length (ring 1) */
#define	WMREG_OLD_RDH1	0x0148
#define	WMREG_OLD_RDT1	0x0150
#define	WMREG_OLD_FCRTH 0x0160	/* Flow Control Rx Threshold Hi (OLD) */
#define	WMREG_FCRTL	0x2160	/* Flow Control Rx Threshold Lo */
#define	FCRTH_DFLT	0x00008000

#define	WMREG_OLD_FCRTL 0x0168	/* Flow Control Rx Threshold Lo (OLD) */
#define	WMREG_FCRTH	0x2168	/* Flow Control Rx Threhsold Hi */
#define	FCRTL_DFLT	0x00004000
#define	FCRTL_XONE	0x80000000	/* Enable XON frame transmission */

#define	WMREG_FCTTV	0x0170	/* Flow Control Transmit Timer Value */
#define	FCTTV_DFLT	0x00000600

#define	WMREG_TXCW	0x0178	/* Transmit Configuration Word (TBI mode) */
	/* See MII ANAR_X bits. */
#define	TXCW_FD		(1U << 5)	/* Full Duplex */
#define	TXCW_HD		(1U << 6)	/* Half Duplex */
#define	TXCW_SYM_PAUSE	(1U << 7)	/* sym pause request */
#define	TXCW_ASYM_PAUSE	(1U << 8)	/* asym pause request */
#define	TXCW_TxConfig	(1U << 30)	/* Tx Config */
#define	TXCW_ANE	(1U << 31)	/* Autonegotiate */

#define	WMREG_RXCW	0x0180	/* Receive Configuration Word (TBI mode) */
	/* See MII ANLPAR_X bits. */
#define	RXCW_NC		(1U << 26)	/* no carrier */
#define	RXCW_IV		(1U << 27)	/* config invalid */
#define	RXCW_CC		(1U << 28)	/* config change */
#define	RXCW_C		(1U << 29)	/* /C/ reception */
#define	RXCW_SYNCH	(1U << 30)	/* synchronized */
#define	RXCW_ANC	(1U << 31)	/* autonegotiation complete */

#define	WMREG_MTA	0x0200	/* Multicast Table Array */
#define	WMREG_CORDOVA_MTA 0x5200

#define	WMREG_TCTL	0x0400	/* Transmit Control Register */
#define	TCTL_EN		(1U << 1)	/* transmitter enable */
#define	TCTL_PSP	(1U << 3)	/* pad short packets */
#define	TCTL_CT(x)	(((x) & 0xff) << 4)   /* 4:11 - collision threshold */
#define	TCTL_COLD(x)	(((x) & 0x3ff) << 12) /* 12:21 - collision distance */
#define	TCTL_SWXOFF	(1U << 22)	/* software XOFF */
#define	TCTL_RTLC	(1U << 24)	/* retransmit on late collision */
#define	TCTL_NRTU	(1U << 25)	/* no retransmit on underrun */
#define	TCTL_MULR	(1U << 28)	/* multiple request */

#define	TX_COLLISION_THRESHOLD		15
#define	TX_COLLISION_DISTANCE_HDX	512
#define	TX_COLLISION_DISTANCE_FDX	64

#define	WMREG_TCTL_EXT	0x0404	/* Transmit Control Register */
#define	TCTL_EXT_BST_MASK	0x000003FF /* Backoff Slot Time */
#define	TCTL_EXT_GCEX_MASK	0x000FFC00 /* Gigabit Carry Extend Padding */

#define	DEFAULT_80003ES2LAN_TCTL_EXT_GCEX 0x00010000

#define	WMREG_TIPG	0x0410	/* Transmit IPG Register */
#define	TIPG_IPGT(x)	(x)		/* IPG transmit time */
#define	TIPG_IPGR1(x)	((x) << 10)	/* IPG receive time 1 */
#define	TIPG_IPGR2(x)	((x) << 20)	/* IPG receive time 2 */
#define	TIPG_WM_DFLT	(TIPG_IPGT(0x0a) | TIPG_IPGR1(0x02) | TIPG_IPGR2(0x0a))
#define	TIPG_LG_DFLT	(TIPG_IPGT(0x06) | TIPG_IPGR1(0x08) | TIPG_IPGR2(0x06))
#define	TIPG_1000T_DFLT	(TIPG_IPGT(0x08) | TIPG_IPGR1(0x08) | TIPG_IPGR2(0x06))
#define	TIPG_1000T_80003_DFLT \
    (TIPG_IPGT(0x08) | TIPG_IPGR1(0x02) | TIPG_IPGR2(0x07))
#define	TIPG_10_100_80003_DFLT \
    (TIPG_IPGT(0x09) | TIPG_IPGR1(0x02) | TIPG_IPGR2(0x07))

#define	WMREG_TQC	0x0418

#define	WMREG_OLD_TDBAL	0x0420	/* Transmit Descriptor Base Lo */
#define	WMREG_TDBAL	0x3800

#define	WMREG_OLD_TDBAH	0x0424	/* Transmit Descriptor Base Hi */
#define	WMREG_TDBAH	0x3804

#define	WMREG_OLD_TDLEN	0x0428	/* Transmit Descriptor Length */
#define	WMREG_TDLEN	0x3808

#define	WMREG_OLD_TDH	0x0430	/* Transmit Descriptor Head */
#define	WMREG_TDH	0x3810

#define	WMREG_OLD_TDT	0x0438	/* Transmit Descriptor Tail */
#define	WMREG_TDT	0x3818

#define	WMREG_OLD_TIDV	0x0440	/* Transmit Delay Interrupt Value */
#define	WMREG_TIDV	0x3820

#define	WMREG_AIT	0x0458	/* Adaptive IFS Throttle */
#define	WMREG_VFTA	0x0600

#define	WMREG_MDICNFG	0x0e04	/* MDC/MDIO Configuration Register */
#define MDICNFG_PHY_SHIFT	21
#define MDICNFG_PHY_MASK	__BITS(25, 21)
#define MDICNFG_COM_MDIO	__BIT(30)
#define MDICNFG_DEST		__BIT(31)

#define	WM_MC_TABSIZE	128
#define	WM_ICH8_MC_TABSIZE 32
#define	WM_VLAN_TABSIZE	128

#define	WMREG_PHPM	0x0e14	/* PHY Power Management */
#define	PHPM_GO_LINK_D		__BIT(5)	/* Go Link Disconnect */

#define WMREG_EEER	0x0e30	/* Energy Efficiency Ethernet "EEE" */
#define EEER_TX_LPI_EN		0x00010000 /* EEER Tx LPI Enable */
#define EEER_RX_LPI_EN		0x00020000 /* EEER Rx LPI Enable */
#define EEER_LPI_FC		0x00040000 /* EEER Ena on Flow Cntrl */
#define EEER_EEER_NEG		0x20000000 /* EEER capability nego */
#define EEER_EEER_RX_LPI_STATUS	0x40000000 /* EEER Rx in LPI state */
#define EEER_EEER_TX_LPI_STATUS	0x80000000 /* EEER Tx in LPI state */
#define WMREG_EEE_SU	0x0e34	/* EEE Setup */
#define WMREG_IPCNFG	0x0e38	/* Internal PHY Configuration */
#define IPCNFG_10BASE_TE	0x00000002 /* IPCNFG 10BASE-Te low power op. */
#define IPCNFG_EEE_100M_AN	0x00000004 /* IPCNFG EEE Ena 100M AN */
#define IPCNFG_EEE_1G_AN	0x00000008 /* IPCNFG EEE Ena 1G AN */

#define WMREG_EXTCNFCTR	0x0f00  /* Extended Configuration Control */
#define EXTCNFCTR_PCIE_WRITE_ENABLE	0x00000001
#define EXTCNFCTR_PHY_WRITE_ENABLE	0x00000002
#define EXTCNFCTR_D_UD_ENABLE		0x00000004
#define EXTCNFCTR_D_UD_LATENCY		0x00000008
#define EXTCNFCTR_D_UD_OWNER		0x00000010
#define EXTCNFCTR_MDIO_SW_OWNERSHIP	0x00000020
#define EXTCNFCTR_MDIO_HW_OWNERSHIP	0x00000040
#define EXTCNFCTR_GATE_PHY_CFG		0x00000080
#define EXTCNFCTR_EXT_CNF_POINTER	0x0FFF0000

#define	WMREG_PHY_CTRL	0x0f10	/* PHY control */
#define	PHY_CTRL_SPD_EN		(1 << 0)
#define	PHY_CTRL_D0A_LPLU	(1 << 1)
#define	PHY_CTRL_NOND0A_LPLU	(1 << 2)
#define	PHY_CTRL_NOND0A_GBE_DIS	(1 << 3)
#define	PHY_CTRL_GBE_DIS	(1 << 4)

#define	WMREG_PBA	0x1000	/* Packet Buffer Allocation */
#define	PBA_BYTE_SHIFT	10		/* KB -> bytes */
#define	PBA_ADDR_SHIFT	7		/* KB -> quadwords */
#define	PBA_8K		0x0008
#define	PBA_10K		0x000a
#define	PBA_12K		0x000c
#define	PBA_14K		0x000e
#define	PBA_16K		0x0010		/* 16K, default Tx allocation */
#define	PBA_20K		0x0014
#define	PBA_22K		0x0016
#define	PBA_24K		0x0018
#define	PBA_26K		0x001a
#define	PBA_30K		0x001e
#define	PBA_32K		0x0020
#define	PBA_34K		0x0022
#define	PBA_35K		0x0023
#define	PBA_40K		0x0028
#define	PBA_48K		0x0030		/* 48K, default Rx allocation */
#define	PBA_64K		0x0040

#define	WMREG_PBS	0x1008	/* Packet Buffer Size (ICH) */

#define	WMREG_PBECCSTS	0x100c	/* Packet Buffer ECC Status (PCH_LPT) */
#define	PBECCSTS_CORR_ERR_CNT_MASK	0x000000ff
#define	PBECCSTS_UNCORR_ERR_CNT_MASK	0x0000ff00
#define	PBECCSTS_UNCORR_ECC_ENABLE	0x00010000

#define WMREG_EEMNGCTL	0x1010	/* MNG EEprom Control */
#define EEMNGCTL_CFGDONE_0 0x040000	/* MNG config cycle done */
#define EEMNGCTL_CFGDONE_1 0x080000	/*  2nd port */

#define WMREG_I2CCMD	0x1028	/* SFPI2C Command Register - RW */
#define I2CCMD_REG_ADDR_SHIFT	16
#define I2CCMD_REG_ADDR		0x00ff0000
#define I2CCMD_PHY_ADDR_SHIFT	24
#define I2CCMD_PHY_ADDR		0x07000000
#define I2CCMD_OPCODE_READ	0x08000000
#define I2CCMD_OPCODE_WRITE	0x00000000
#define I2CCMD_RESET		0x10000000
#define I2CCMD_READY		0x20000000
#define I2CCMD_INTERRUPT_ENA	0x40000000
#define I2CCMD_ERROR		0x80000000
#define MAX_SGMII_PHY_REG_ADDR	255
#define I2CCMD_PHY_TIMEOUT	200

#define	WMREG_EEWR	0x102c	/* EEPROM write */

#define WMREG_PBA_ECC	0x01100	/* PBA ECC */
#define PBA_ECC_COUNTER_MASK	0xfff00000 /* ECC counter mask */
#define PBA_ECC_COUNTER_SHIFT	20	   /* ECC counter shift value */
#define	PBA_ECC_CORR_EN		0x00000001 /* Enable ECC error correction */
#define	PBA_ECC_STAT_CLR	0x00000002 /* Clear ECC error counter */
#define	PBA_ECC_INT_EN		0x00000004 /* Enable ICR bit 5 on ECC error */

#define WMREG_GPIE	0x01514 /* General Purpose Interrupt Enable */
#define GPIE_NSICR	__BIT(0)	/* Non Selective Interrupt Clear */
#define GPIE_MULTI_MSIX	__BIT(4)	/* Multiple MSIX */
#define GPIE_EIAME	__BIT(30)	/* Extended Interrupt Auto Mask Ena. */
#define GPIE_PBA	__BIT(31)	/* PBA support */

#define WMREG_EICS	0x01520  /* Ext. Interrupt Cause Set - WO */
#define WMREG_EIMS	0x01524  /* Ext. Interrupt Mask Set/Read - RW */
#define WMREG_EIMC	0x01528  /* Ext. Interrupt Mask Clear - WO */
#define WMREG_EIAC	0x0152C  /* Ext. Interrupt Auto Clear - RW */
#define WMREG_EIAM	0x01530  /* Ext. Interrupt Ack Auto Clear Mask - RW */

#define WMREG_EICR	0x01580  /* Ext. Interrupt Cause Read - R/clr */

#define WMREG_MSIXBM(x)	(0x1600 + (x) * 4) /* MSI-X Allocation */

#define EITR_RX_QUEUE0	0x00000001 /* Rx Queue 0 Interrupt */
#define EITR_RX_QUEUE1	0x00000002 /* Rx Queue 1 Interrupt */
#define EITR_RX_QUEUE2	0x00000004 /* Rx Queue 2 Interrupt */
#define EITR_RX_QUEUE3	0x00000008 /* Rx Queue 3 Interrupt */
#define EITR_TX_QUEUE0	0x00000100 /* Tx Queue 0 Interrupt */
#define EITR_TX_QUEUE1	0x00000200 /* Tx Queue 1 Interrupt */
#define EITR_TX_QUEUE2	0x00000400 /* Tx Queue 2 Interrupt */
#define EITR_TX_QUEUE3	0x00000800 /* Tx Queue 3 Interrupt */
#define EITR_TCP_TIMER	0x40000000 /* TCP Timer */
#define EITR_OTHER	0x80000000 /* Interrupt Cause Active */

#define WMREG_EITR(x)	(0x01680 + (0x4 * (x)))
#define EITR_ITR_INT_MASK	0x0000ffff

#define	WMREG_RXPBS	0x2404	/* Rx Packet Buffer Size  */
#define RXPBS_SIZE_MASK_82576	0x0000007F

#define	WMREG_RDFH	0x2410	/* Receive Data FIFO Head */
#define	WMREG_RDFT	0x2418	/* Receive Data FIFO Tail */
#define	WMREG_RDFHS	0x2420	/* Receive Data FIFO Head Saved */
#define	WMREG_RDFTS	0x2428	/* Receive Data FIFO Tail Saved */
#define	WMREG_RADV	0x282c	/* Receive Interrupt Absolute Delay Timer */

#define	WMREG_TXDMAC	0x3000	/* Transfer DMA Control */
#define	TXDMAC_DPP	(1U << 0)	/* disable packet prefetch */

#define WMREG_KABGTXD	0x3004	/* AFE and Gap Transmit Ref Data */
#define	KABGTXD_BGSQLBIAS 0x00050000

#define	WMREG_TDFH	0x3410	/* Transmit Data FIFO Head */
#define	WMREG_TDFT	0x3418	/* Transmit Data FIFO Tail */
#define	WMREG_TDFHS	0x3420	/* Transmit Data FIFO Head Saved */
#define	WMREG_TDFTS	0x3428	/* Transmit Data FIFO Tail Saved */
#define	WMREG_TDFPC	0x3430	/* Transmit Data FIFO Packet Count */

#define	WMREG_TXDCTL(n)		/* Trandmit Descriptor Control */ \
	(((n) < 4) ? (0x3828 + ((n) * 0x100)) : (0xe028 + ((n) * 0x40)))
#define	TXDCTL_PTHRESH(x) ((x) << 0)	/* prefetch threshold */
#define	TXDCTL_HTHRESH(x) ((x) << 8)	/* host threshold */
#define	TXDCTL_WTHRESH(x) ((x) << 16)	/* write back threshold */
/* flags used starting with 82575 ... */
#define TXDCTL_COUNT_DESC	__BIT(22) /* Enable the counting of desc.
					   still to be processed. */
#define TXDCTL_QUEUE_ENABLE  0x02000000 /* Enable specific Tx Queue */
#define TXDCTL_SWFLSH        0x04000000 /* Tx Desc. write-back flushing */
#define TXDCTL_PRIORITY      0x08000000

#define	WMREG_TADV	0x382c	/* Transmit Absolute Interrupt Delay Timer */
#define	WMREG_TSPMT	0x3830	/* TCP Segmentation Pad and Minimum
				   Threshold (Cordova) */
#define	TSPMT_TSMT(x)	(x)		/* TCP seg min transfer */
#define	TSPMT_TSPBP(x)	((x) << 16)	/* TCP seg pkt buf padding */

#define	WMREG_TARC0	0x3840	/* Tx arbitration count (0) */
#define	WMREG_TARC1	0x3940	/* Tx arbitration count (1) */

#define	WMREG_CRCERRS	0x4000	/* CRC Error Count */
#define	WMREG_ALGNERRC	0x4004	/* Alignment Error Count */
#define	WMREG_SYMERRC	0x4008	/* Symbol Error Count */
#define	WMREG_RXERRC	0x400c	/* receive error Count - R/clr */
#define	WMREG_MPC	0x4010	/* Missed Packets Count - R/clr */
#define	WMREG_COLC	0x4028	/* collision Count - R/clr */
#define	WMREG_SEC	0x4038	/* Sequence Error Count */
#define	WMREG_CEXTERR	0x403c	/* Carrier Extension Error Count */
#define	WMREG_RLEC	0x4040	/* Receive Length Error Count */
#define	WMREG_XONRXC	0x4048	/* XON Rx Count - R/clr */
#define	WMREG_XONTXC	0x404c	/* XON Tx Count - R/clr */
#define	WMREG_XOFFRXC	0x4050	/* XOFF Rx Count - R/clr */
#define	WMREG_XOFFTXC	0x4054	/* XOFF Tx Count - R/clr */
#define	WMREG_FCRUC	0x4058	/* Flow Control Rx Unsupported Count - R/clr */
#define WMREG_RNBC	0x40a0	/* Receive No Buffers Count */
#define WMREG_TLPIC	0x4148	/* EEE Tx LPI Count */
#define WMREG_RLPIC	0x414c	/* EEE Rx LPI Count */

#define	WMREG_PCS_CFG	0x4200	/* PCS Configuration */
#define	PCS_CFG_PCS_EN	__BIT(3)

#define	WMREG_PCS_LCTL	0x4208	/* PCS Link Control */
#define	PCS_LCTL_FSV_1000 __BIT(2)	/* AN Timeout Enable */
#define	PCS_LCTL_FDV_FULL __BIT(3)	/* AN Timeout Enable */
#define	PCS_LCTL_FSD __BIT(4)	/* AN Timeout Enable */
#define	PCS_LCTL_FORCE_FC __BIT(7)	/* AN Timeout Enable */
#define	PCS_LCTL_AN_ENABLE __BIT(16)	/* AN Timeout Enable */
#define	PCS_LCTL_AN_RESTART __BIT(17)	/* AN Timeout Enable */
#define	PCS_LCTL_AN_TIMEOUT __BIT(18)	/* AN Timeout Enable */

#define	WMREG_PCS_LSTS	0x420c	/* PCS Link Status */
#define PCS_LSTS_LINKOK	__BIT(0)
#define PCS_LSTS_SPEED_100  __BIT(1)
#define PCS_LSTS_SPEED_1000 __BIT(2)
#define PCS_LSTS_FDX	__BIT(3)
#define PCS_LSTS_AN_COMP __BIT(16)

#define	WMREG_PCS_ANADV	0x4218	/* AN Advertsement */
#define	WMREG_PCS_LPAB	0x421c	/* Link Partnet Ability */

#define	WMREG_RXCSUM	0x5000	/* Receive Checksum register */
#define	RXCSUM_PCSS	0x000000ff	/* Packet Checksum Start */
#define	RXCSUM_IPOFL	(1U << 8)	/* IP checksum offload */
#define	RXCSUM_TUOFL	(1U << 9)	/* TCP/UDP checksum offload */
#define	RXCSUM_IPV6OFL	(1U << 10)	/* IPv6 checksum offload */

#define WMREG_RLPML	0x5004	/* Rx Long Packet Max Length */

#define WMREG_RFCTL	0x5008	/* Receive Filter Control */
#define WMREG_RFCTL_NFSWDIS	__BIT(6)  /* NFS Write Disable */
#define WMREG_RFCTL_NFSRDIS	__BIT(7)  /* NFS Read Disable */
#define WMREG_RFCTL_ACKDIS	__BIT(12) /* ACK Accelerate Disable */
#define WMREG_RFCTL_ACKD_DIS	__BIT(13) /* ACK data Disable */
#define WMREG_RFCTL_IPV6EXDIS	__BIT(16) /* IPv6 Extension Header Disable */
#define WMREG_RFCTL_NEWIPV6EXDIS __BIT(17) /* New IPv6 Extension Header */

#define	WMREG_WUC	0x5800	/* Wakeup Control */
#define	WUC_APME		0x00000001 /* APM Enable */
#define	WUC_PME_EN		0x00000002 /* PME Enable */

#define	WMREG_WUFC	0x5808	/* Wakeup Filter COntrol */
#define WUFC_MAG		0x00000002 /* Magic Packet Wakeup Enable */
#define WUFC_EX			0x00000004 /* Directed Exact Wakeup Enable */
#define WUFC_MC			0x00000008 /* Directed Multicast Wakeup En */
#define WUFC_BC			0x00000010 /* Broadcast Wakeup Enable */
#define WUFC_ARP		0x00000020 /* ARP Request Packet Wakeup En */
#define WUFC_IPV4		0x00000040 /* Directed IPv4 Packet Wakeup En */
#define WUFC_IPV6		0x00000080 /* Directed IPv6 Packet Wakeup En */

#define	WMREG_MANC	0x5820	/* Management Control */
#define	MANC_SMBUS_EN		0x00000001
#define	MANC_ASF_EN		0x00000002
#define	MANC_ARP_EN		0x00002000
#define	MANC_RECV_TCO_RESET	0x00010000
#define	MANC_RECV_TCO_EN	0x00020000
#define	MANC_BLK_PHY_RST_ON_IDE	0x00040000
#define	MANC_RECV_ALL		0x00080000
#define	MANC_EN_MAC_ADDR_FILTER	0x00100000
#define	MANC_EN_MNG2HOST	0x00200000

#define	WMREG_MANC2H	0x5860	/* Manaegment Control To Host - RW */
#define MANC2H_PORT_623		(1 << 5)
#define MANC2H_PORT_624		(1 << 6)

#define WMREG_GCR	0x5b00	/* PCIe Control */
#define GCR_RXD_NO_SNOOP	0x00000001
#define GCR_RXDSCW_NO_SNOOP	0x00000002
#define GCR_RXDSCR_NO_SNOOP	0x00000004
#define GCR_TXD_NO_SNOOP	0x00000008
#define GCR_TXDSCW_NO_SNOOP	0x00000010
#define GCR_TXDSCR_NO_SNOOP	0x00000020
#define GCR_CMPL_TMOUT_MASK	0x0000f000
#define GCR_CMPL_TMOUT_10MS	0x00001000
#define GCR_CMPL_TMOUT_RESEND	0x00010000
#define GCR_CAP_VER2		0x00040000
#define GCR_L1_ACT_WITHOUT_L0S_RX 0x08000000

#define WMREG_FACTPS	0x5b30	/* Function Active and Power State to MNG */
#define FACTPS_MNGCG		0x20000000
#define FACTPS_LFS		0x40000000	/* LAN Function Select */

#define WMREG_GIOCTL	0x5b44	/* GIO Analog Control Register */
#define WMREG_CCMCTL	0x5b48	/* CCM Control Register */
#define WMREG_SCCTL	0x5b4c	/* PCIc PLL Configuration Register */

#define	WMREG_SWSM	0x5b50	/* SW Semaphore */
#define	SWSM_SMBI	0x00000001	/* Driver Semaphore bit */
#define	SWSM_SWESMBI	0x00000002	/* FW Semaphore bit */
#define	SWSM_WMNG	0x00000004	/* Wake MNG Clock */
#define	SWSM_DRV_LOAD	0x00000008	/* Driver Loaded Bit */

#define	WMREG_FWSM	0x5b54	/* FW Semaphore */
#define	FWSM_MODE_MASK		0xe
#define	FWSM_MODE_SHIFT		0x1
#define	MNG_ICH_IAMT_MODE	0x2	/* PT mode? */
#define	MNG_IAMT_MODE		0x3
#define FWSM_RSPCIPHY		0x00000040	/* Reset PHY on PCI reset */
#define FWSM_FW_VALID		0x00008000 /* FW established a valid mode */

#define	WMREG_SWSM2	0x5b58	/* SW Semaphore 2 */
#define SWSM2_LOCK		0x00000002 /* Secondary driver semaphore bit */

#define	WMREG_SW_FW_SYNC 0x5b5c	/* software-firmware semaphore */
#define	SWFW_EEP_SM		0x0001 /* eeprom access */
#define	SWFW_PHY0_SM		0x0002 /* first ctrl phy access */
#define	SWFW_PHY1_SM		0x0004 /* second ctrl phy access */
#define	SWFW_MAC_CSR_SM		0x0008
#define	SWFW_PHY2_SM		0x0020 /* first ctrl phy access */
#define	SWFW_PHY3_SM		0x0040 /* first ctrl phy access */
#define	SWFW_SOFT_SHIFT		0	/* software semaphores */
#define	SWFW_FIRM_SHIFT		16	/* firmware semaphores */

#define WMREG_GCR2	0x5b64	/* 3GPIO Control Register 2 */

#define WMREG_CRC_OFFSET 0x5f50

#define WMREG_EEC	0x12010
#define EEC_FLASH_DETECTED (1U << 19)	/* FLASH */
#define EEC_FLUPD	(1U << 23)	/* Update FLASH */

#define WMREG_EEARBC_I210 0x12024

/*
 * NVM related values.
 *  Microwire, SPI, and flash
 */
#define	UWIRE_OPC_ERASE	0x04		/* MicroWire "erase" opcode */
#define	UWIRE_OPC_WRITE	0x05		/* MicroWire "write" opcode */
#define	UWIRE_OPC_READ	0x06		/* MicroWire "read" opcode */

#define	SPI_OPC_WRITE	0x02		/* SPI "write" opcode */
#define	SPI_OPC_READ	0x03		/* SPI "read" opcode */
#define	SPI_OPC_A8	0x08		/* opcode bit 3 == address bit 8 */
#define	SPI_OPC_WREN	0x06		/* SPI "set write enable" opcode */
#define	SPI_OPC_WRDI	0x04		/* SPI "clear write enable" opcode */
#define	SPI_OPC_RDSR	0x05		/* SPI "read status" opcode */
#define	SPI_OPC_WRSR	0x01		/* SPI "write status" opcode */
#define	SPI_MAX_RETRIES	5000		/* max wait of 5ms for RDY signal */

#define	SPI_SR_RDY	0x01
#define	SPI_SR_WEN	0x02
#define	SPI_SR_BP0	0x04
#define	SPI_SR_BP1	0x08
#define	SPI_SR_WPEN	0x80

#define NVM_CHECKSUM		0xBABA
#define NVM_SIZE		0x0040
#define NVM_WORD_SIZE_BASE_SHIFT 6

#define	NVM_OFF_MACADDR		0x0000	/* MAC address offset 0 */
#define	NVM_OFF_MACADDR1	0x0001	/* MAC address offset 1 */
#define	NVM_OFF_MACADDR2	0x0002	/* MAC address offset 2 */
#define NVM_OFF_COMPAT		0x0003
#define NVM_OFF_ID_LED_SETTINGS	0x0004
#define NVM_OFF_VERSION		0x0005
#define	NVM_OFF_CFG1		0x000a	/* config word 1 */
#define	NVM_OFF_CFG2		0x000f	/* config word 2 */
#define	NVM_OFF_EEPROM_SIZE	0x0012	/* NVM SIZE */
#define	NVM_OFF_CFG4		0x0013	/* config word 4 */
#define	NVM_OFF_CFG3_PORTB	0x0014	/* config word 3 */
#define NVM_OFF_FUTURE_INIT_WORD1 0x0019
#define	NVM_OFF_INIT_3GIO_3	0x001a	/* PCIe Initial Configuration Word 3 */
#define	NVM_OFF_K1_CONFIG	0x001b	/* NVM K1 Config */
#define	NVM_OFF_LED_1_CFG	0x001c
#define	NVM_OFF_LED_0_2_CFG	0x001f
#define	NVM_OFF_SWDPIN		0x0020	/* SWD Pins (Cordova) */
#define	NVM_OFF_CFG3_PORTA	0x0024	/* config word 3 */
#define NVM_OFF_ALT_MAC_ADDR_PTR 0x0037	/* to the alternative MAC addresses */
#define NVM_OFF_COMB_VER_PTR	0x003d
#define NVM_OFF_IMAGE_UID0	0x0042
#define NVM_OFF_IMAGE_UID1	0x0043

#define NVM_COMPAT_VALID_CHECKSUM	0x0001

#define	NVM_CFG1_LVDID		(1U << 0)
#define	NVM_CFG1_LSSID		(1U << 1)
#define	NVM_CFG1_PME_CLOCK	(1U << 2)
#define	NVM_CFG1_PM		(1U << 3)
#define	NVM_CFG1_ILOS		(1U << 4)
#define	NVM_CFG1_SWDPIO_SHIFT	5
#define	NVM_CFG1_SWDPIO_MASK	(0xf << NVM_CFG1_SWDPIO_SHIFT)
#define	NVM_CFG1_IPS1		(1U << 8)
#define	NVM_CFG1_LRST		(1U << 9)
#define	NVM_CFG1_FD		(1U << 10)
#define	NVM_CFG1_FRCSPD		(1U << 11)
#define	NVM_CFG1_IPS0		(1U << 12)
#define	NVM_CFG1_64_32_BAR	(1U << 13)

#define	NVM_CFG2_CSR_RD_SPLIT	(1U << 1)
#define	NVM_CFG2_82544_APM_EN	(1U << 2)
#define	NVM_CFG2_64_BIT		(1U << 3)
#define	NVM_CFG2_MAX_READ	(1U << 4)
#define	NVM_CFG2_DMCR_MAP	(1U << 5)
#define	NVM_CFG2_133_CAP	(1U << 6)
#define	NVM_CFG2_MSI_DIS	(1U << 7)
#define	NVM_CFG2_FLASH_DIS	(1U << 8)
#define	NVM_CFG2_FLASH_SIZE(x)	(((x) & 3) >> 9)
#define	NVM_CFG2_APM_EN		(1U << 10)
#define	NVM_CFG2_ANE		(1U << 11)
#define	NVM_CFG2_PAUSE(x)	(((x) & 3) >> 12)
#define	NVM_CFG2_ASDE		(1U << 14)
#define	NVM_CFG2_APM_PME	(1U << 15)
#define	NVM_CFG2_SWDPIO_SHIFT	4
#define	NVM_CFG2_SWDPIO_MASK	(0xf << NVM_CFG2_SWDPIO_SHIFT)
#define	NVM_CFG2_MNGM_SHIFT	13	/* Manageability Operation mode */
#define	NVM_CFG2_MNGM_MASK	(3U << NVM_CFG2_MNGM_SHIFT)
#define	NVM_CFG2_MNGM_DIS	0
#define	NVM_CFG2_MNGM_NCSI	1
#define	NVM_CFG2_MNGM_PT	2

#define	NVM_COMPAT_SERDES_FORCE_MODE	__BIT(14) /* Don't use autonego */

#define NVM_FUTURE_INIT_WORD1_VALID_CHECKSUM	0x0040

#define	NVM_K1_CONFIG_ENABLE	0x01

#define	NVM_SWDPIN_MASK		0xdf
#define	NVM_SWDPIN_SWDPIN_SHIFT 0
#define	NVM_SWDPIN_SWDPIO_SHIFT 8

#define NVM_3GIO_3_ASPM_MASK	(0x3 << 2)	/* Active State PM Support */

#define NVM_CFG3_APME		(1U << 10)	
#define NVM_CFG3_PORTA_EXT_MDIO	(1U << 2)	/* External MDIO Interface */
#define NVM_CFG3_PORTA_COM_MDIO	(1U << 3)	/* MDIO Interface is shared */

#define	NVM_OFF_MACADDR_82571(x)	(3 * (x))

/*
 * EEPROM Partitioning. See Table 6-1, "EEPROM Top Level Partitioning"
 * in 82580's datasheet.
 */
#define NVM_OFF_LAN_FUNC_82580(x)	((x) ? (0x40 + (0x40 * (x))) : 0)

#define NVM_COMBO_VER_OFF	0x0083

#define NVM_MAJOR_MASK		0xf000
#define NVM_MAJOR_SHIFT		12
#define NVM_MINOR_MASK		0x0ff0
#define NVM_MINOR_SHIFT		4
#define NVM_BUILD_MASK		0x000f
#define NVM_UID_VALID		0x8000

/* iNVM Registers for i21[01] */
#define WM_INVM_DATA_REG(reg)	(0x12120 + 4*(reg))
#define INVM_SIZE			64 /* Number of INVM Data Registers */

/* iNVM default vaule */
#define NVM_INIT_CTRL_2_DEFAULT_I211	0x7243
#define NVM_INIT_CTRL_4_DEFAULT_I211	0x00c1
#define NVM_LED_1_CFG_DEFAULT_I211	0x0184
#define NVM_LED_0_2_CFG_DEFAULT_I211	0x200c
#define NVM_RESERVED_WORD		0xffff

#define INVM_DWORD_TO_RECORD_TYPE(dword)	((dword) & 0x7)
#define INVM_DWORD_TO_WORD_ADDRESS(dword)	(((dword) & 0x0000FE00) >> 9)
#define INVM_DWORD_TO_WORD_DATA(dword)		(((dword) & 0xFFFF0000) >> 16)

#define INVM_UNINITIALIZED_STRUCTURE		0x0
#define INVM_WORD_AUTOLOAD_STRUCTURE		0x1
#define INVM_CSR_AUTOLOAD_STRUCTURE		0x2
#define INVM_PHY_REGISTER_AUTOLOAD_STRUCTURE	0x3
#define INVM_RSA_KEY_SHA256_STRUCTURE		0x4
#define INVM_INVALIDATED_STRUCTURE		0xf

#define INVM_RSA_KEY_SHA256_DATA_SIZE_IN_DWORDS	8
#define INVM_CSR_AUTOLOAD_DATA_SIZE_IN_DWORDS	1

#define INVM_DEFAULT_AL		0x202f
#define INVM_AUTOLOAD		0x0a
#define INVM_PLL_WO_VAL		0x0010

/* Version and Image Type field */
#define INVM_VER_1	__BITS(12,3)
#define INVM_VER_2	__BITS(22,13)
#define INVM_IMGTYPE	__BITS(28,23)
#define INVM_MINOR	__BITS(3,0)
#define INVM_MAJOR	__BITS(9,4)

/* Word definitions for ID LED Settings */
#define ID_LED_RESERVED_FFFF 0xFFFF

/* ich8 flash control */
#define ICH_FLASH_COMMAND_TIMEOUT            5000    /* 5000 uSecs - adjusted */
#define ICH_FLASH_ERASE_TIMEOUT              3000000 /* Up to 3 seconds - worst case */
#define ICH_FLASH_CYCLE_REPEAT_COUNT         10      /* 10 cycles */
#define ICH_FLASH_SEG_SIZE_256               256
#define ICH_FLASH_SEG_SIZE_4K                4096
#define ICH_FLASH_SEG_SIZE_64K               65536

#define ICH_CYCLE_READ                       0x0
#define ICH_CYCLE_RESERVED                   0x1
#define ICH_CYCLE_WRITE                      0x2
#define ICH_CYCLE_ERASE                      0x3

#define ICH_FLASH_GFPREG   0x0000
#define ICH_FLASH_HSFSTS   0x0004 /* Flash Status Register */
#define HSFSTS_DONE		0x0001 /* Flash Cycle Done */
#define HSFSTS_ERR		0x0002 /* Flash Cycle Error */
#define HSFSTS_DAEL		0x0004 /* Direct Access error Log */
#define HSFSTS_ERSZ_MASK	0x0018 /* Block/Sector Erase Size */
#define HSFSTS_ERSZ_SHIFT	3
#define HSFSTS_FLINPRO		0x0020 /* flash SPI cycle in Progress */
#define HSFSTS_FLDVAL		0x4000 /* Flash Descriptor Valid */
#define HSFSTS_FLLK		0x8000 /* Flash Configuration Lock-Down */
#define ICH_FLASH_HSFCTL   0x0006 /* Flash control Register */
#define HSFCTL_GO		0x0001 /* Flash Cycle Go */
#define HSFCTL_CYCLE_MASK	0x0006 /* Flash Cycle */
#define HSFCTL_CYCLE_SHIFT	1
#define HSFCTL_BCOUNT_MASK	0x0300 /* Data Byte Count */
#define HSFCTL_BCOUNT_SHIFT	8
#define ICH_FLASH_FADDR    0x0008
#define ICH_FLASH_FDATA0   0x0010
#define ICH_FLASH_FRACC    0x0050
#define ICH_FLASH_FREG0    0x0054
#define ICH_FLASH_FREG1    0x0058
#define ICH_FLASH_FREG2    0x005C
#define ICH_FLASH_FREG3    0x0060
#define ICH_FLASH_FPR0     0x0074
#define ICH_FLASH_FPR1     0x0078
#define ICH_FLASH_SSFSTS   0x0090
#define ICH_FLASH_SSFCTL   0x0092
#define ICH_FLASH_PREOP    0x0094
#define ICH_FLASH_OPTYPE   0x0096
#define ICH_FLASH_OPMENU   0x0098

#define ICH_FLASH_REG_MAPSIZE      0x00A0
#define ICH_FLASH_SECTOR_SIZE      4096
#define ICH_GFPREG_BASE_MASK       0x1FFF
#define ICH_FLASH_LINEAR_ADDR_MASK 0x00FFFFFF

#define ICH_NVM_SIG_WORD	0x13
#define ICH_NVM_SIG_MASK	0xc000
#define ICH_NVM_VALID_SIG_MASK	0xc0
#define ICH_NVM_SIG_VALUE	0x80

/* for PCI express Capability registers */
#define	WM_PCIE_DCSR2_16MS	0x00000005

/* SFF SFP ROM data */
#define SFF_SFP_ID_OFF		0x00
#define SFF_SFP_ID_UNKNOWN	0x00	/* Unknown */
#define SFF_SFP_ID_SFF		0x02	/* Module soldered to motherboard */
#define SFF_SFP_ID_SFP		0x03	/* SFP transceiver */

#define SFF_SFP_ETH_FLAGS_OFF	0x06
#define SFF_SFP_ETH_FLAGS_1000SX	0x01
#define SFF_SFP_ETH_FLAGS_1000LX	0x02
#define SFF_SFP_ETH_FLAGS_1000CX	0x04
#define SFF_SFP_ETH_FLAGS_1000T		0x08
#define SFF_SFP_ETH_FLAGS_100FX		0x10

/* I21[01] PHY related definitions */
#define GS40G_PAGE_SELECT	0x16
#define GS40G_PAGE_SHIFT	16
#define GS40G_OFFSET_MASK	0xffff
#define GS40G_PHY_PLL_FREQ_PAGE	0xfc0000
#define GS40G_PHY_PLL_FREQ_REG	0x000e
#define GS40G_PHY_PLL_UNCONF	0xff

/* advanced TX descriptor for 82575 and newer */
typedef union nq_txdesc {
	struct {
		uint64_t nqtxd_addr;
		uint32_t nqtxd_cmdlen;
		uint32_t nqtxd_fields;
	} nqtx_data;
	struct {
		uint32_t nqtxc_vl_len;
		uint32_t nqtxc_sn;
		uint32_t nqtxc_cmd;
		uint32_t nqtxc_mssidx;
	} nqrx_ctx;
} __packed nq_txdesc_t;


/* Commands for nqtxd_cmdlen and nqtxc_cmd */
#define	NQTX_CMD_EOP	(1U << 24)	/* end of packet */
#define	NQTX_CMD_IFCS	(1U << 25)	/* insert FCS */
#define	NQTX_CMD_RS	(1U << 27)	/* report status */
#define	NQTX_CMD_DEXT	(1U << 29)	/* descriptor extension */
#define	NQTX_CMD_VLE	(1U << 30)	/* VLAN enable */
#define	NQTX_CMD_TSE	(1U << 31)	/* TCP segmentation enable */

/* Descriptor types (if DEXT is set) */
#define	NQTX_DTYP_C	(2U << 20)	/* context */
#define	NQTX_DTYP_D	(3U << 20)	/* data */

#define NQTXD_FIELDS_IDX_SHIFT		4	/* context index shift */
#define NQTXD_FIELDS_IDX_MASK		0xf
#define NQTXD_FIELDS_PAYLEN_SHIFT	14	/* payload len shift */
#define NQTXD_FIELDS_PAYLEN_MASK	0x3ffff

#define NQTXD_FIELDS_IXSM		(1U << 8) /* do IP checksum */
#define NQTXD_FIELDS_TUXSM		(1U << 9) /* do TCP/UDP checksum */

#define NQTXC_VLLEN_IPLEN_SHIFT		0	/* IP header len */
#define NQTXC_VLLEN_IPLEN_MASK		0x1ff
#define NQTXC_VLLEN_MACLEN_SHIFT	9	/* MAC header len */
#define NQTXC_VLLEN_MACLEN_MASK		0x7f
#define NQTXC_VLLEN_VLAN_SHIFT		16	/* vlan number */
#define NQTXC_VLLEN_VLAN_MASK		0xffff

#define NQTXC_CMD_MKRLOC_SHIFT		0	/* IP checksum offset */
#define NQTXC_CMD_MKRLOC_MASK		0x1ff
#define NQTXC_CMD_SNAP			(1U << 9)
#define NQTXC_CMD_IP4			(1U << 10)
#define NQTXC_CMD_IP6			(0U << 10)
#define NQTXC_CMD_TCP			(1U << 11)
#define NQTXC_CMD_UDP			(0U << 11)
#define NQTXC_MSSIDX_IDX_SHIFT		4	/* context index shift */
#define NQTXC_MSSIDX_IDX_MASK		0xf
#define NQTXC_MSSIDX_L4LEN_SHIFT	8	/* L4 header len shift */
#define NQTXC_MSSIDX_L4LEN_MASK		0xff
#define NQTXC_MSSIDX_MSS_SHIFT		16	/* MSS */
#define NQTXC_MSSIDX_MSS_MASK		0xffff
