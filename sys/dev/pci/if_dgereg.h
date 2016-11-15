/*	$NetBSD: if_dgereg.h,v 1.3 2007/12/25 18:33:40 perry Exp $	*/

/*
 * Copyright (c) 2004, SUNET, Swedish University Computer Network.
 * All rights reserved.
 *
 * Written by Anders Magnusson for SUNET, Swedish University Computer Network.
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
 *	SUNET, Swedish University Computer Network.
 * 4. The name of SUNET may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SUNET ``AS IS'' AND
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

/* PCI registers */
#define	DGE_PCI_BAR	0x10
#define	DGE_PCIX_CMD	0xe4

/* PCIX CMD bits */
#define	PCIX_MMRBC_MSK	0x000c0000
#define	PCIX_MMRBC_512	0x00000000
#define	PCIX_MMRBC_1024	0x00040000
#define	PCIX_MMRBC_2048	0x00080000
#define	PCIX_MMRBC_4096	0x000c0000


/* General registers */
#define	DGE_CTRL0	0x000
#define	DGE_CTRL1	0x008
#define	DGE_STATUS	0x010
#define DGE_EECD	0x018
#define DGE_MFS		0x020

/* Interrupt control registers */
#define DGE_ICR		0x080
#define DGE_ICS		0x088
#define DGE_IMS		0x090
#define DGE_IMC		0x098

/* Receiver control registers */
#define DGE_RCTL	0x0100
#define DGE_FCRTL	0x0108
#define DGE_FCRTH	0x0110
#define DGE_RDBAL	0x0118
#define DGE_RDBAH	0x011c
#define DGE_RDLEN	0x0120
#define DGE_RDH		0x0128
#define DGE_RDT		0x0130
#define DGE_RDTR	0x0138
#define DGE_RXDCTL	0x0140
#define DGE_RAIDC	0x0148
#define DGE_RXCSUM	0x0158
#define	DGE_RAL		0x0180
#define	DGE_RAH		0x0184
#define	DGE_MTA		0x0200

/* Transmit control registers */
#define	DGE_TCTL	0x0600
#define	DGE_TDBAL	0x0608
#define	DGE_TDBAH	0x060c
#define	DGE_TDLEN	0x0610
#define	DGE_TDH		0x0618
#define	DGE_TDT		0x0620
#define	DGE_TIDV	0x0628
#define	DGE_TXDCTL	0x0630
#define	DGE_TSPMT	0x0638
#define	DGE_PAP		0x0640

/* PHY communications */
#define	DGE_MDIO	0x0758
#define	DGE_MDIRW	0x0760

/* Statistics */
#define	DGE_TPRL	0x2000
#define	DGE_TPRH	0x2004

/*
 * CTRL0 bit definitions.
 */
#define	CTRL0_LRST	0x00000008
#define	CTRL0_JFE	0x00000010
#define	CTRL0_XLE	0x00000020
#define	CTRL0_MDCS	0x00000040
#define	CTRL0_CMDC	0x00000080
#define	CTRL0_SDP0	0x00040000
#define	CTRL0_SDP1	0x00080000
#define	CTRL0_SDP2	0x00100000
#define	CTRL0_SDP3	0x00200000
#define	CTRL0_SDP0_DIR	0x00400000
#define	CTRL0_SDP1_DIR	0x00800000
#define	CTRL0_SDP2_DIR	0x01000000
#define	CTRL0_SDP3_DIR	0x02000000
#define	CTRL0_RST	0x04000000
#define	CTRL0_RPE	0x08000000
#define	CTRL0_TPE	0x10000000
#define	CTRL0_VME	0x40000000

/*
 * CTRL1 bit definitions.
 */
#define	CTRL1_EE_RST	0x00002000
/*
 * STATUS bit definitions.
 */
#define	STATUS_LINKUP	0x00000002
#define	STATUS_BUS64	0x00001000
#define	STATUS_PCIX	0x00002000
#define	STATUS_PCIX_MSK	0x0000C000
#define	STATUS_PCIX_66	0x00000000
#define	STATUS_PCIX_100	0x00004000
#define	STATUS_PCIX_133	0x00008000

/*
 * Interrupt control registers bit definitions.
 */
#define	ICR_TXDW	0x00000001
#define	ICR_TXQE	0x00000002
#define	ICR_LSC		0x00000004
#define	ICR_RXSEQ	0x00000008
#define	ICR_RXDMT0	0x00000010
#define	ICR_RXO		0x00000040
#define	ICR_RXT0	0x00000080
#define	ICR_GPI0	0x00000800
#define	ICR_GPI1	0x00001000
#define	ICR_GPI2	0x00002000
#define	ICR_GPI3	0x00004000

/*
 * RCTL bit definitions.
 */
#define	RCTL_RXEN	0x00000002
#define	RCTL_SBP	0x00000004
#define	RCTL_UPE	0x00000008
#define	RCTL_MPE	0x00000010
#define	RCTL_RDMTS_12	0x00000000
#define	RCTL_RDMTS_14	0x00000100
#define	RCTL_RDMTS_18	0x00000200
#define	RCTL_BAM	0x00008000
#define	RCTL_BSIZE_2k	0x00000000
#define	RCTL_BSIZE_4k	0x00010000
#define	RCTL_BSIZE_8k	0x00020000
#define	RCTL_BSIZE_16k	0x00030000
#define	RCTL_VFE	0x00040000
#define	RCTL_CFIEN	0x00080000
#define	RCTL_CFI	0x00100000
#define	RCTL_RPDA_MC	0x00400000
#define	RCTL_CFF	0x00800000
#define	RCTL_SECRC	0x04000000

#define RCTL_MO(x)	((x) << 12)

#define	FCRTL_XONE	0x80000000

/*
 * RXDCTL macros.
 */
#define	RXDCTL_PTHRESH(x) (x)
#define	RXDCTL_HTHRESH(x) ((x) << 9)
#define	RXDCTL_WTHRESH(x) ((x) << 18)

/*
 * RXCSUM bit definitions.
 */
#define	RXCSUM_IPOFL	0x00000100
#define	RXCSUM_TUOFL	0x00000200

/*
 * RAH/RAL macros.
 */
#define	RAH_AV		0x80000000
#define	RA_TABSIZE	16		/* # of direct-filtered addresses */
#define	RA_ADDR(reg, idx) ((reg) + (idx) * 8)

/*
 * MTA macros.
 */
#define	MC_TABSIZE	128		/* Size of multicast array table */

/*
 * TCTL bit definitions.
 */
#define	TCTL_TCE	0x00000001
#define	TCTL_TXEN	0x00000002
#define	TCTL_TPDE	0x00000004

/*
 * TXDCTL macros.
 */
#define	TXDCTL_PTHRESH(x) (x)
#define	TXDCTL_HTHRESH(x) ((x) << 8)
#define	TXDCTL_WTHRESH(x) ((x) << 16)

/*
 * MDIO communication bits.
 * This is for "New Protocol".
 */
#define	MDIO_REG(x)	((x) & 0xffff)
#define	MDIO_DEV(x)	((x) << 16)
#define	MDIO_PHY(x)	((x) << 21)
#define	MDIO_ADDR	0
#define	MDIO_WRITE	(1 << 26)
#define	MDIO_READ	(1 << 27)
#define	MDIO_OLD_P	(1 << 28)
#define	MDIO_CMD	(1 << 30)

/*
 * EEPROM stuff.
 * The 10GbE card uses an ATMEL AT93C46 in 64x16 mode,
 * see http://www.atmel.com/dyn/resources/prod_documents/doc0172.pdf
 */
/* EEPROM bit masks in the EECD register */
#define EECD_SK		0x01
#define EECD_CS		0x02
#define EECD_DI		0x04
#define EECD_DO		0x08

#define	EEPROM_SIZE	64	/* 64 word in length */
#define	EEPROM_CKSUM	0xbaba

#define	EE_ADDR01	0	/* Offset in EEPROM for MAC address 0-1 */
#define	EE_ADDR23	1	/* Offset in EEPROM for MAC address 2-3 */
#define	EE_ADDR45	2	/* Offset in EEPROM for MAC address 4-5 */

/*
 * Transmit descriptor definitions.
 */
struct dge_tdes {
	uint32_t dt_baddrl;	/* Lower 32 bits of buffer address */
	uint32_t dt_baddrh;	/* Upper 32 bits of buffer address */
	uint32_t dt_ctl;	/* Command/Type/Length */
	uint8_t  dt_status;	/* Transmitted data status info */
	uint8_t  dt_popts;	/* Packet options */
	uint16_t dt_vlan;	/* VLAN information */
} __packed;

/*
 * Context transmit descriptor, "overlayed" on the above struct.
 */
struct dge_ctdes {
#if 0
	uint8_t	 dc_ipcss;	/* IP checksum start */
	uint8_t	 dc_ipcso;	/* IP checksum offset */
	uint16_t dc_ipcse;	/* IP checksum ending */
	uint8_t	 dc_tucss;	/* TCP/UDP checksum start */
	uint8_t	 dc_tucso;	/* TCP/UDP checksum offset */
	uint16_t dc_tucse;	/* TCP/UDP checksum ending */
	uint32_t dc_ctl;	/* Command/Type/Length (as above) */
	uint8_t	 dc_status;	/* Status info (as above) */
	uint8_t	 dc_hdrlen;	/* Header length */
	uint16_t dc_mss;	/* Maximum segment size */
#else
        uint32_t dc_tcpip_ipcs;	/* IP checksum context */
        uint32_t dc_tcpip_tucs;	/* TCP/UDP checksum context */
        uint32_t dc_tcpip_cmdlen;
        uint32_t dc_tcpip_seg;	/* TCP segmentation context */
#endif
} __packed;

#define	TDESC_DTYP_CTD	0x00000000
#define	TDESC_DTYP_DATA	0x00100000
#define	TDESC_DCMD_IDE	0x80000000
#define	TDESC_DCMD_VLE	0x40000000
#define	TDESC_DCMD_RS	0x08000000
#define	TDESC_DCMD_TSE	0x04000000
#define	TDESC_DCMD_EOP	0x01000000
#define	TDESC_TUCMD_IDE	0x80000000
#define	TDESC_TUCMD_RS	0x08000000
#define	TDESC_TUCMD_TSE	0x04000000
#define	TDESC_TUCMD_IP	0x02000000
#define	TDESC_TUCMD_TCP	0x01000000

#define	DGE_TCPIP_IPCSS(x) (x)
#define	DGE_TCPIP_IPCSO(x) ((x) << 8)
#define	DGE_TCPIP_IPCSE(x) ((x) << 16)
#define	DGE_TCPIP_TUCSS(x) (x)
#define	DGE_TCPIP_TUCSO(x) ((x) << 8)
#define	DGE_TCPIP_TUCSE(x) ((x) << 16)

#define	TDESC_STA_DD	0x01

#define	TDESC_POPTS_TXSM 0x02
#define	TDESC_POPTS_IXSM 0x01
/*
 * Receive descriptor definitions.
 */
struct dge_rdes {
	uint32_t dr_baddrl;	/* Lower 32 bits of buffer address */
	uint32_t dr_baddrh;	/* Upper 32 bits of buffer address */
	uint16_t dr_len;	/* Length of receive packet */
	uint16_t dr_cksum;	/* Packet checksum */
	uint8_t  dr_status;	/* Received data status info */
	uint8_t  dr_errors;	/* Receive errors */
	uint16_t dr_special;	/* VLAN (802.1q) information */
} __packed;

#define	RDESC_STS_PIF	0x80	/* Exact filter match */
#define	RDESC_STS_IPCS	0x40	/* IP Checksum calculated */
#define	RDESC_STS_TCPCS	0x20	/* TCP checksum calculated */
#define	RDESC_STS_VP	0x08	/* Packet is 802.1q */
#define	RDESC_STS_IXSM	0x04	/* Ignore checksum */
#define	RDESC_STS_EOP	0x02	/* End of packet */
#define	RDESC_STS_DD	0x01	/* Descriptor done */

#define	RDESC_ERR_RXE	0x80	/* RX data error */
#define	RDESC_ERR_IPE	0x40	/* IP checksum error */
#define	RDESC_ERR_TCPE	0x20	/* TCP/UDP checksum error */
#define	RDESC_ERR_P	0x08	/* Parity error */
#define	RDESC_ERR_SE	0x02	/* Symbol error */
#define	RDESC_ERR_CE	0x01	/* CRC/Alignment error */
