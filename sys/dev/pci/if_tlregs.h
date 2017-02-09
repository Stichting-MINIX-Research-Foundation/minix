/*	$NetBSD: if_tlregs.h,v 1.11 2014/10/18 08:33:28 snj Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(_DEV_PCI_IF_TLREGS_H_)
#define _DEV_PCI_IF_TLREGS_H_

#define PCI_CFID    0x00    /* Configuration ID */
#define PCI_CFCS    0x04    /* Configurtion Command/Status */
#define PCI_CFRV    0x08    /* Configuration Revision */
#define PCI_CFLT    0x0c    /* Configuration Latency Timer */
#define PCI_CBIO    0x10    /* Configuration Base IO Address */
#define PCI_CBMA    0x14    /* Configuration Base Memory Address */
#define PCI_CFIT    0x3c    /* Configuration Interrupt */
#define PCI_CFDA    0x40    /* Configuration Driver Area */

/* Host registers */
#define TL_HOST_CMD			0x00
#define TL_HOST_CH_PARM		0x04
#define TL_HOST_INTR_DIOADR	0x08
#	define TL_HOST_INTR_MASK	0xffff0000
#	define TL_HOST_DIOADR_MASK	0x0000ffff
#define TL_HOST_DIO_DATA	0x0c

#define TL_INTR_MASK	0x001c0000
#define TL_INTR_TxEOF	0x00040000
#define TL_INTR_Stat	0x00080000
#define TL_INTR_RxEOF	0x000c0000
#define TL_INTR_Dumm	0x00100000
#define TL_INTR_TxEOC	0x00140000
#define TL_INTR_Adc	0x00180000
#define TL_INTR_RxEOC	0x001c0000
#define TL_INTVec_MASK	0x1fe00000

/* HOST_CMD bits */
#define HOST_CMD_GO		0x80000000
#define HOST_CMD_STOP		0x40000000
#define HOST_CMD_ACK		0x20000000
#define HOST_CMD_CHSEL_mask	0x1fe00000
#define HOST_CMD_EOC		0x00100000
#define HOST_CMD_RT		0x00080000
#define HOST_CMD_Nes		0x00040000
#define HOST_CMD_Ad_Rst		0x00008000
#define HOST_CMD_LdTmr		0x00004000
#define HOST_CMD_LdThr		0x00002000
#define HOST_CMD_ReqInt		0x00001000
#define HOST_CMD_IntOff		0x00000800
#define HOST_CMD_IntOn		0x00000400
#define HOST_CMD_AckCnt_mask	0x000000ff


/* Internal registers */
#define TL_INT_NET			0x00
#	define TL_INT_NetCmd		0x00 /* offsets from TL_INT_NET */
#	define TL_INT_NetSio		0x01
#	define TL_INT_NetSts		0x02
#	define TL_INT_NetMask		0x03
#define TL_INT_NetConfig	0x04
#define TL_INT_Defaults		0x08
#define TL_INT_Areg0		0x10
#define TL_INT_HASH1		0x28
#define TL_INT_HASH2		0x2c
#define TL_INT_STATS_TX		0x30
#define TL_INT_STATS_RX		0x34
#define TL_INT_STATS_FERR	0x38
#	define	TL_FERR_DEF		0x0000ffff
#	define	TL_FERR_CRC		0x00ff0000
#	define	TL_FERR_CODE		0xff000000
#define TL_INT_STATS_COLL	0x3c
#	define TL_COL_MULTI		0x0000ffff
#	define TL_COL_SINGLE		0xffff0000
#define TL_INT_LERR		0x40
#	define	TL_LERR_ECOLL		0x000000ff
#	define	TL_LERR_LCOLL		0x0000ff00
#	define	TL_LERR_CL		0x00ff0000
#	define	TL_LERR_AC		0xff000000
#define TL_INT_MISC		0x44
#	define TL_MISC_LED		0x1
#	define TL_MISC_BSIZE		0x2
#	define TL_MISC_MaxRxL		0x3
#	define TL_MISC_MaxRxH		0x4

/* LEDs for the Integrated Netelligent 10/100 TX */
#define TL_LED_LINK	0x01
#define TL_LED_ACT	0x10

/* NETCOMMAND bits */
#define TL_NETCOMMAND_NRESET	0x80
#define TL_NETCOMMAND_NWRAP	0x40
#define TL_NETCOMMAND_CSF	0x20
#define TL_NETCOMMAND_CAF	0x10
#define TL_NETCOMMAND_NOBRX	0x08
#define TL_NETCOMMAND_DUPLEX	0x04
#define TL_NETCOMMAND_TRFRAM	0x02
#define TL_NETCOMMAND_TXPACE	0x01

/* NETCONFIG bits */
#define TL_NETCONFIG_Trclk		0x8000
#define TL_NETCONFIG_Ttclk		0x4000
#define TL_NETCONFIG_Brate		0x2000
#define TL_NETCONFIG_RxCRC		0x1000
#define TL_NETCONFIG_PEF		0x0800
#define TL_NETCONFIG_1F			0x0400
#define TL_NETCONFIG_1chn		0x0200
#define TL_NETCONFIG_Mtest		0x0100
#define TL_NETCONFIG_PHY_EN		0x0080
#define TL_NETCONFIG_MAC_MASK		0x007f

/* NetSio bits definition */
#define TL_NETSIO_MDATA		0x01
#define TL_NETSIO_MTXEN		0x02
#define TL_NETSIO_MCLK		0x04
#define TL_NETSIO_NMRST		0x08
#define TL_NETSIO_EDATA		0x10
#define TL_NETSIO_ETXEN		0x20
#define TL_NETSIO_ECLOCK	0x40
#define TL_NETSIO_MINTEN	0x80

/* NetSts buts definition */
#define TL_NETSTS_MIRQ		0x80
#define TL_NETSTS_HBEAT		0x40
#define TL_NETSTS_TXSTOP	0x20
#define TL_NETSTS_RXSTOP	0x10

/* Linked lists for receive/transmit of datagrams */

struct tl_data_seg {
	u_int32_t data_count;
	u_int32_t data_addr;
} __packed;

/* Receive list (one_frag = 1) */
struct tl_Rx_list {
	u_int32_t fwd;
	u_int32_t stat;
	struct tl_data_seg seg;
}__packed;

#define TL_RX_CSTAT_CPLT	0x4000 /* Frame complete */
#define TL_RX_CSTAT_EOC		0x0800 /* Rx EOC */
#define TL_RX_CSTAT_Err		0x0400 /* Error frame */

/* transmit list */
#define TL_NSEG 10
#define TL_LAST_SEG 0x80000000
struct tl_Tx_list {
	u_int32_t fwd;
	u_int32_t stat;
	struct tl_data_seg seg[TL_NSEG];
}__packed;

#define TL_TX_CSTAT_CPLT	0x4000 /* Frame complete */
#define TL_TX_CSTAT_EOC		0x0800 /* Tx EOC */

/*
 * Structs used by the host used for lists management. Note that the adapter's
 * lists must start on an 8 bytes boundary.
 */

struct Rx_list {
	struct mbuf *m; /* mbuf associated with this list */
	bus_dmamap_t m_dmamap; /* and its DMA map */
	struct Rx_list *next;
	bus_addr_t hw_listaddr;
	struct tl_Rx_list *hw_list;
};

struct Tx_list {
	struct mbuf *m; /* mbuf associated with this list */
	bus_dmamap_t m_dmamap; /* and its DMA map */
	struct Tx_list *next;
	bus_addr_t hw_listaddr;
	struct tl_Tx_list *hw_list;
};

#endif /* ! _DEV_PCI_IF_TLREGS_H_ */
