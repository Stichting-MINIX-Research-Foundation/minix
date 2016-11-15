/*      $NetBSD: cemacreg.h,v 1.3 2015/08/24 18:51:37 rjs Exp $	*/

/*-
 * Copyright (c) 2015  Genetec Corporation.  All rights reserved.
 * Written by Hashimoto Kenichi for Genetec Corporation.
 *
 * Copyright (c) 2007 Embedtronics Oy
 * All rights reserved
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_IF_CEMACREG_H_
#define	_IF_CEMACREG_H_

/* Ethernet MAC (EMAC),
 * at91rm9200.pdf, page 573 */

#define	ETH_CTL		0x00U	/* 0x00: Control Register		*/
#define	ETH_CFG		0x04U	/* 0x04: Configuration Register		*/
#define	ETH_SR		0x08U	/* 0x08: Status Register		*/
#define	ETH_TAR		0x0CU	/* 0x0C: Transmit Address Register (at91rm9200 only)	*/
#define	ETH_TCR		0x10U	/* 0x10: Transmit Control Register (at91rm9200 only)	*/
#define	ETH_TSR		0x14U	/* 0x14: Transmit Status Register	*/
#define	ETH_RBQP	0x18U	/* 0x18: Receive Buffer Queue Pointer	*/
#define ETH_TBQP	0x1CU	/* 0x1C: Transmit Buffer Queue Pointer	*/
#define	ETH_RSR		0x20U	/* 0x20: Receive Status Register	*/
#define	ETH_ISR		0x24U	/* 0x24: Interrupt Status Register	*/
#define	ETH_IER		0x28U	/* 0x28: Interrupt Enable Register	*/
#define	ETH_IDR		0x2CU	/* 0x2C: Interrupt Disable Register	*/
#define	ETH_IMR		0x30U	/* 0x30: Interrupt Mask Register	*/
#define	ETH_MAN		0x34U	/* 0x34: PHY Maintenance Register	*/

#define	ETH_FRA		0x40U	/* 0x40: Frames Transmitted OK		*/
#define	ETH_SCOL	0x44U	/* 0x44: Single Collision Frames	*/
#define	ETH_MCOL	0x48U	/* 0x48: Multiple Collision Frames	*/
#define	ETH_OK		0x4CU	/* 0x4C: Frames Received OK		*/
#define	ETH_SEQE	0x50U	/* 0x50: Frame Check Sequence Errors	*/
#define	ETH_ALE		0x54U	/* 0x54: Alignment Errors		*/
#define	ETH_DTE		0x58U	/* 0x58: Deferred Transmission Frame	*/
#define	ETH_LCOL	0x5CU	/* 0x5C: Late Collisions		*/
#define	ETH_ECOL	0x60U	/* 0x60: Excessive Collisions		*/
#define	ETH_CSE		0x64U	/* 0x64: Carrier Sense Errors		*/
#define	ETH_TUE		0x68U	/* 0x68: Transmit Underrun Errors	*/
#define	ETH_CDE		0x6CU	/* 0x6C: Code Errors			*/
#define	ETH_ELR		0x70U	/* 0x70: Excessive Length Errors	*/
#define	ETH_RJB		0x74U	/* 0x74: Receive Jabbers		*/
#define	ETH_USF		0x78U	/* 0x78: Undersize Frames		*/
#define	ETH_SQEE	0x7CU	/* 0x7C: SQE Test Errors		*/
#define	ETH_DRFC	0x80U	/* 0x80: Discarded RX Frames		*/

#define	ETH_HSH		0x90U	/* 0x90: Hash Address High		*/
#define	ETH_HSL		0x94U	/* 0x94: Hash Address Low		*/

#define	ETH_SA1L	0x98U	/* 0x98: Specific Address 1 Low		*/
#define	ETH_SA1H	0x9CU	/* 0x9C: Specific Address 1 High	*/

#define	ETH_SA2L	0xA0U	/* 0xA0: Specific Address 2 Low		*/
#define	ETH_SA2H	0xA4U	/* 0xA4: Specific Address 2 High	*/

#define	ETH_SA3L	0xA8U	/* 0xA8: Specific Address 3 Low		*/
#define	ETH_SA3H	0xACU	/* 0xAC: Specific Address 3 High	*/

#define	ETH_SA4L	0xB0U	/* 0xB0: Specific Address 4 Low		*/
#define	ETH_SA4H	0xB4U	/* 0xB4: Specific Address 4 High	*/

/*
 * Gigabit Ethernet Controller (GEM)
 * ug585-Zynq-7000-TRM.pdf
 */

#define GEM_USER_IO	0x000C
#define GEM_DMA_CFG	0x0010	/* DMA Configuration */
#define  GEM_DMA_CFG_DISC_WHEN_NO_AHB		__BIT(24)
#define  GEM_DMA_CFG_RX_BUF_SIZE		__BITS(23, 16)
#define  GEM_DMA_CFG_CHKSUM_GEN_OFFLOAD_EN	__BIT(11)
#define  GEM_DMA_CFG_TX_PKTBUF_MEMSZ_SEL	__BIT(10)
#define  GEM_DMA_CFG_RX_PKTBUF_MEMSZ_SEL	__BITS(9, 8)
#define  GEM_DMA_CFG_AHB_ENDIAN_SWAP_PKT_EN	__BIT(7)
#define  GEM_DMA_CFG_AHB_ENDIAN_SWAP_MGMT_EN	__BIT(6)
#define  GEM_DMA_CFG_AHB_FIXED_BURST_LEN	__BITS(4, 0)
#define GEM_HSH		0x0080
#define GEM_HSL		0x0084
#define	GEM_SA1L	0x0088
#define	GEM_SA1H	0x008C
#define	GEM_SA2L	0x0090
#define	GEM_SA2H	0x0094
#define	GEM_SA3L	0x0098
#define	GEM_SA3H	0x009C
#define	GEM_SA4L	0x0090
#define	GEM_SA4H	0x0094
#define	GEM_SCOL	0x0138
#define	GEM_MCOL	0x013C
#define	GEM_DCFG2	0x0284
#define	GEM_DCFG3	0x0288
#define	GEM_DCFG4	0x028C
#define	GEM_DCFG5	0x0290

#define ETH_SIZE	0x1000

/* Control Register bits: */
#define GEM_CTL_ZEROPAUSETX	__BIT(12)
#define GEM_CTL_PAUSETX		__BIT(11)
#define GEM_CTL_HALTTX		__BIT(10)
#define GEM_CTL_STARTTX		__BIT(9)

#define	ETH_CTL_BP	0x100U	/* 1 = back pressure enabled		*/
#define	ETH_CTL_WES	0x080U	/* 1 = statistics registers writeable	*/
#define	ETH_CTL_ISR	0x040U	/* 1 = increment statistics registers	*/
#define	ETH_CTL_CSR	0x020U	/* 1 = clear statistics registers	*/
#define	ETH_CTL_MPE	0x010U	/* 1 = management port enabled		*/
#define	ETH_CTL_TE	0x008U	/* 1 = transmit enable			*/
#define	ETH_CTL_RE	0x004U	/* 1 = receive enable			*/
#define	ETH_CTL_LBL	0x002U	/* 1 = local loopback enabled		*/
#define	ETH_CTL_LB	0x001U	/* 1 = loopback signal is at high level	*/


/* Configuration Register bits: */
#define	ETH_CFG_RMII	0x2000U	/* 1 = enable RMII (Reduce MII)	(AT91RM9200 only) */
#define	ETH_CFG_RTY	0x1000U	/* 1 = retry test enabled		*/

#define	ETH_CFG_CLK	0x0C00U	/* clock				*/
#define	ETH_CFG_CLK_8	0x0000U
#define	ETH_CFG_CLK_16	0x0400U
#define	ETH_CFG_CLK_32	0x0800U
#define	ETH_CFG_CLK_64	0x0C00U

#define	ETH_CFG_EAE	0x0200U	/* 1 = external address match enable	*/
#define	ETH_CFG_BIG	0x0100U	/* 1 = receive up to 1522 bytes	(VLAN)	*/
#define	ETH_CFG_UNI	0x0080U	/* 1 = enable unicast hash		*/
#define	ETH_CFG_MTI	0x0040U	/* 1 = enable multicast hash		*/
#define	ETH_CFG_NBC	0x0020U	/* 1 = ignore received broadcasts	*/
#define	ETH_CFG_CAF	0x0010U	/* 1 = receive all valid frames		*/
#define	ETH_CFG_BR	0x0004U
#define	ETH_CFG_FD	0x0002U	/* 1 = force full duplex		*/
#define	ETH_CFG_SPD	0x0001U	/* 1 = 100 Mbps				*/

#define GEM_CFG_GEN	__BIT(10)
#define GEM_CFG_CLK	__BITS(20, 18)
#define GEM_CFG_CLK_8	__SHIFTIN(0, GEM_CFG_CLK)
#define GEM_CFG_CLK_16	__SHIFTIN(1, GEM_CFG_CLK)
#define GEM_CFG_CLK_32	__SHIFTIN(2, GEM_CFG_CLK)
#define GEM_CFG_CLK_48	__SHIFTIN(3, GEM_CFG_CLK)
#define GEM_CFG_CLK_64	__SHIFTIN(4, GEM_CFG_CLK)
#define GEM_CFG_CLK_96	__SHIFTIN(5, GEM_CFG_CLK)
#define GEM_CFG_DBW	__BITS(22, 21)
#define	GEM_CFG_RX_CHKSUM_OFFLD_EN	__BIT(24)

/* Status Register bits: */
#define	ETH_SR_IDLE	0x0004U	/* 1 = PHY logic is running		*/
#define	ETH_SR_MDIO	0x0002U	/* 1 = MDIO pin set			*/
#define	ETH_SR_LINK	0x0001U


/* Transmit Control Register bits: */
#define	ETH_TCR_NCRC	0x8000U	/* 1 = don't append CRC			*/
#define	ETH_TCR_LEN	0x07FFU	/* transmit frame length		*/


/* Transmit Status Register bits: */
#define	ETH_TSR_UND	0x40U	/* 1 = transmit underrun detected	*/
#define	ETH_TSR_COMP	0x20U	/* 1 = transmit complete		*/
#define	ETH_TSR_BNQ	0x10U	/* 1 = transmit buffer not queued (at91rm9200 only)	*/
#define	ETH_TSR_IDLE	0x08U	/* 1 = transmitter idle			*/
#define	ETH_TSR_RLE	0x04U	/* 1 = retry limit exceeded		*/
#define	ETH_TSR_COL	0x02U	/* 1 = collision occurred		*/
#define	ETH_TSR_OVR	0x01U	/* 1 = transmit buffer overrun		*/

#define	GEM_TSR_TXGO	__BIT(3)

/* Receive Status Register bits: */
#define	ETH_RSR_OVR	0x04U	/* 1 = RX overrun			*/
#define	ETH_RSR_REC	0x02U	/* 1 = frame received			*/
#define	ETH_RSR_BNA	0x01U	/* 1 = buffer not available		*/


/* Interrupt bits: */
#define	ETH_ISR_ABT	0x0800U	/* 1 = abort during DMA transfer	*/
#define	ETH_ISR_ROVR	0x0400U	/* 1 = RX overrun			*/
#define	ETH_ISR_LINK	0x0200U	/* 1 = link pin changed			*/
#define	ETH_ISR_TIDLE	0x0100U	/* 1 = transmitter idle			*/
#define	ETH_ISR_TCOM	0x0080U	/* 1 = transmit complete		*/
#define	ETH_ISR_TBRE	0x0040U	/* 1 = transmit buffer register empty	*/
#define	ETH_ISR_RTRY	0x0020U	/* 1 = retry limit exceeded		*/
#define	ETH_ISR_TUND	0x0010U	/* 1 = transmit buffer underrun		*/
#define	ETH_ISR_TOVR	0x0008U	/* 1 = transmit buffer overrun		*/
#define	ETH_ISR_RBNA	0x0004U	/* 1 = receive buffer not available	*/
#define	ETH_ISR_RCOM	0x0002U	/* 1 = receive complete			*/
#define	ETH_ISR_DONE	0x0001U	/* 1 = management done			*/


/* PHY Maintenance Register bits: */
#define	ETH_MAN_LOW	0x80000000U /* must not be set			*/
#define	ETH_MAN_HIGH	0x40000000U /* must be set			*/

#define	ETH_MAN_RW	0x30000000U
#define	ETH_MAN_RW_RD	0x20000000U
#define	ETH_MAN_RW_WR	0x10000000U

#define	ETH_MAN_PHYA	0x0F800000U /* PHY address (normally 0)		*/
#define	ETH_MAN_PHYA_SHIFT 23U
#define	ETH_MAN_REGA	0x007C0000U
#define	ETH_MAN_REGA_SHIFT 18U
#define	ETH_MAN_CODE	0x00030000U /* must be 10			*/
#define	ETH_MAN_CODE_IEEE802_3 \
			0x00020000U
#define	ETH_MAN_DATA	0x0000FFFFU /* data to be written to the PHY	*/

#define	ETH_MAN_VAL	(ETH_MAN_HIGH|ETH_MAN_CODE_IEEE802_3)


/* received buffer descriptor: */
#define	ETH_DSC_ADDR		0x00U
#define	ETH_DSC_FLAGS		0x00U
#define	ETH_DSC_INFO		0x04U
#define	ETH_DSC_SIZE		0x08U

typedef struct eth_dsc {
	volatile uint32_t	Addr;
	volatile uint32_t	Info;
} __attribute__ ((aligned(4))) eth_dsc_t;

/* flags: */
#define	ETH_RDSC_F_WRAP		0x00000002U
#define	ETH_RDSC_F_USED		0x00000001U

/* frame info bits: */
#define	ETH_RDSC_I_BCAST	__BIT(31)
#define	ETH_RDSC_I_MULTICAST	__BIT(30)
#define	ETH_RDSC_I_UNICAST	__BIT(29)
#define	ETH_RDSC_I_VLAN		0x10000000U
#define	ETH_RDSC_I_UNKNOWN_SRC	0x08000000U
#define	ETH_RDSC_I_MATCH1	0x04000000U
#define	ETH_RDSC_I_MATCH2	0x02000000U
#define	ETH_RDSC_I_MATCH3	0x01000000U
#define	ETH_RDSC_I_MATCH4	0x00800000U
#define	ETH_RDSC_I_CHKSUM	__BITS(23, 22)
#define	ETH_RDSC_I_CHKSUM_NONE	__SHIFTIN(0, ETH_RDSC_I_CHKSUM)
#define	ETH_RDSC_I_CHKSUM_IP	__SHIFTIN(1, ETH_RDSC_I_CHKSUM)
#define	ETH_RDSC_I_CHKSUM_TCP	__SHIFTIN(2, ETH_RDSC_I_CHKSUM)
#define	ETH_RDSC_I_CHKSUM_UDP	__SHIFTIN(3, ETH_RDSC_I_CHKSUM)
#define	ETH_RDSC_I_LEN		__BITS(13, 0)

#define ETH_TDSC_I_USED				__BIT(31)	/* done transmitting */
#define ETH_TDSC_I_WRAP				__BIT(30)	/* end of descr ring */
#define ETH_TDSC_I_RETRY_ERR			__BIT(29)
#define ETH_TDSC_I_AHB_ERR			__BIT(27)
#define ETH_TDSC_I_LATE_COLL			__BIT(26)
#define	ETH_TDSC_I_CHKSUM			__BITS(22, 20)
#define ETH_TDSC_I_CHKSUM_GEN_STAT_NO_ERR	__SHIFTIN(0, ETH_TDSC_I_CHKSUM)
#define ETH_TDSC_I_CHKSUM_GEN_STAT_VLAN_HDR_ERR	__SHIFTIN(1, ETH_TDSC_I_CHKSUM)
#define ETH_TDSC_I_CHKSUM_GEN_STAT_SNAP_HDR_ERR	__SHIFTIN(2, ETH_TDSC_I_CHKSUM)
#define ETH_TDSC_I_CHKSUM_GEN_STAT_IP_HDR_ERR	__SHIFTIN(3, ETH_TDSC_I_CHKSUM)
#define ETH_TDSC_I_CHKSUM_GEN_STAT_UNKNOWN_TYPE	__SHIFTIN(4, ETH_TDSC_I_CHKSUM)
#define ETH_TDSC_I_CHKSUM_GEN_STAT_UNSUPP_FRAG	__SHIFTIN(5, ETH_TDSC_I_CHKSUM)
#define ETH_TDSC_I_CHKSUM_GEN_STAT_NOT_TCPUDP	__SHIFTIN(6, ETH_TDSC_I_CHKSUM)
#define ETH_TDSC_I_CHKSUM_GEN_STAT_SHORT_PKT	__SHIFTIN(7, ETH_TDSC_I_CHKSUM)
#define ETH_TDSC_I_NO_CRC_APPENDED		__BIT(16)
#define ETH_TDSC_I_LAST_BUF			__BIT(15)	/* last buf in frame */
#define ETH_TDSC_I_LEN				__BITS(13, 0)

#endif /* !_IF_CEMACREG_H_ */
