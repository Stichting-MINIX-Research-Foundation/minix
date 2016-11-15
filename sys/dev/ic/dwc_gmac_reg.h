/* $NetBSD: dwc_gmac_reg.h,v 1.14 2014/11/28 09:01:05 martin Exp $ */

/*-
 * Copyright (c) 2013, 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry and Martin Husemann.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	AWIN_GMAC_MAC_CONF		0x0000
#define	AWIN_GMAC_MAC_FFILT		0x0004
#define	AWIN_GMAC_MAC_HTHIGH		0x0008
#define	AWIN_GMAC_MAC_HTLOW		0x000c
#define	AWIN_GMAC_MAC_MIIADDR		0x0010
#define	AWIN_GMAC_MAC_MIIDATA		0x0014
#define	AWIN_GMAC_MAC_FLOWCTRL		0x0018
#define	AWIN_GMAC_MAC_VLANTAG		0x001c
#define	AWIN_GMAC_MAC_VERSION		0x0020	/* not always implemented? */
#define	AWIN_GMAC_MAC_INTR		0x0038
#define	AWIN_GMAC_MAC_INTMASK		0x003c
#define	AWIN_GMAC_MAC_ADDR0HI		0x0040
#define	AWIN_GMAC_MAC_ADDR0LO		0x0044
#define	AWIN_GMAC_MII_STATUS		0x00D8

#define	AWIN_GMAC_MAC_CONF_DISABLEJABBER __BIT(22) /* jabber disable */
#define	AWIN_GMAC_MAC_CONF_FRAMEBURST	__BIT(21) /* allow TX frameburst when
						     in half duplex mode */
#define	AWIN_GMAC_MAC_CONF_MIISEL	__BIT(15) /* select MII phy */
#define	AWIN_GMAC_MAC_CONF_FES100	__BIT(14) /* 100 mbit mode */
#define	AWIN_GMAC_MAC_CONF_DISABLERXOWN	__BIT(13) /* do not receive our own
						     TX frames in half duplex
						     mode */
#define	AWIN_GMAC_MAC_CONF_FULLDPLX	__BIT(11) /* select full duplex */
#define	AWIN_GMAC_MAC_CONF_ACS		__BIT(7)  /* auto pad/CRC stripping */
#define	AWIN_GMAC_MAC_CONF_TXENABLE	__BIT(3)  /* enable TX dma engine */
#define	AWIN_GMAC_MAC_CONF_RXENABLE	__BIT(2)  /* enable RX dma engine */

#define	AWIN_GMAC_MAC_FFILT_RA		__BIT(31) /* receive all mode */
#define	AWIN_GMAC_MAC_FFILT_HPF		__BIT(10) /* hash or perfect filter */
#define	AWIN_GMAC_MAC_FFILT_SAF		__BIT(9)  /* source address filter */
#define	AWIN_GMAC_MAC_FFILT_SAIF	__BIT(8)  /* inverse filtering */
#define	AWIN_GMAC_MAC_FFILT_DBF		__BIT(5)  /* disable broadcast frames */
#define	AWIN_GMAC_MAC_FFILT_PM		__BIT(4)  /* promiscious multicast */
#define	AWIN_GMAC_MAC_FFILT_DAIF	__BIT(3)  /* DA inverse filtering */
#define	AWIN_GMAC_MAC_FFILT_HMC		__BIT(2)  /* multicast hash compare */
#define	AWIN_GMAC_MAC_FFILT_HUC		__BIT(1)  /* unicast hash compare */
#define	AWIN_GMAC_MAC_FFILT_PR		__BIT(0)  /* promiscious mode */

#define	AWIN_GMAC_MAC_INT_LPI		__BIT(10)
#define	AWIN_GMAC_MAC_INT_TSI		__BIT(9)
#define	AWIN_GMAC_MAC_INT_ANEG		__BIT(2)
#define	AWIN_GMAC_MAC_INT_LINKCHG	__BIT(1)
#define	AWIN_GMAC_MAC_INT_RGSMII	__BIT(0)

#define	AWIN_GMAC_MAC_FLOWCTRL_PAUSE	__BITS(31,16)
#define	AWIN_GMAC_MAC_FLOWCTRL_RFE	__BIT(2)
#define	AWIN_GMAC_MAC_FLOWCTRL_TFE	__BIT(1)
#define	AWIN_GMAC_MAC_FLOWCTRL_BUSY	__BIT(0)

#define	AWIN_GMAC_DMA_BUSMODE		0x1000
#define	AWIN_GMAC_DMA_TXPOLL		0x1004
#define	AWIN_GMAC_DMA_RXPOLL		0x1008
#define	AWIN_GMAC_DMA_RX_ADDR		0x100c
#define	AWIN_GMAC_DMA_TX_ADDR		0x1010
#define	AWIN_GMAC_DMA_STATUS		0x1014
#define	AWIN_GMAC_DMA_OPMODE		0x1018
#define	AWIN_GMAC_DMA_INTENABLE		0x101c
#define	AWIN_GMAC_DMA_CUR_TX_DESC	0x1048
#define	AWIN_GMAC_DMA_CUR_RX_DESC	0x104c
#define	AWIN_GMAC_DMA_CUR_TX_BUFADDR	0x1050
#define	AWIN_GMAC_DMA_CUR_RX_BUFADDR	0x1054
#define	AWIN_GMAC_DMA_HWFEATURES	0x1058	/* not always implemented? */

#define	GMAC_MII_PHY_SHIFT		11
#define	GMAC_MII_PHY_MASK		__BITS(15,11)
#define	GMAC_MII_REG_SHIFT		6
#define	GMAC_MII_REG_MASK		__BITS(10,6)

#define	GMAC_MII_BUSY			__BIT(0)
#define	GMAC_MII_WRITE			__BIT(1)
#define	GMAC_MII_CLK_60_100M_DIV42	0x0
#define	GMAC_MII_CLK_100_150M_DIV62	0x1
#define	GMAC_MII_CLK_25_35M_DIV16	0x2
#define	GMAC_MII_CLK_35_60M_DIV26	0x3
#define	GMAC_MII_CLK_150_250M_DIV102	0x4
#define	GMAC_MII_CLK_250_300M_DIV124	0x5
#define	GMAC_MII_CLK_DIV4		0x8
#define	GMAC_MII_CLK_DIV6		0x9
#define	GMAC_MII_CLK_DIV8		0xa
#define	GMAC_MII_CLK_DIV10		0xb
#define	GMAC_MII_CLK_DIV12		0xc
#define	GMAC_MII_CLK_DIV14		0xd
#define	GMAC_MII_CLK_DIV16		0xe
#define	GMAC_MII_CLK_DIV18		0xf
#define	GMAC_MII_CLKMASK		__BITS(5,2)

#define	GMAC_BUSMODE_4PBL		__BIT(24)
#define	GMAC_BUSMODE_RPBL		__BITS(22,17)
#define	GMAC_BUSMODE_FIXEDBURST		__BIT(16)
#define	GMAC_BUSMODE_PRIORXTX		__BITS(15,14)
#define	GMAC_BUSMODE_PRIORXTX_41	3
#define	GMAC_BUSMODE_PRIORXTX_31	2
#define	GMAC_BUSMODE_PRIORXTX_21	1
#define	GMAC_BUSMODE_PRIORXTX_11	0
#define	GMAC_BUSMODE_PBL		__BITS(13,8) /* possible DMA
						        burst len */
#define	GMAC_BUSMODE_RESET		__BIT(0)

#define	AWIN_GMAC_MII_IRQ		__BIT(0)


#define	GMAC_DMA_OP_RXSTOREFORWARD	__BIT(24) /* start RX when a
						    full frame is available */
#define	GMAC_DMA_OP_TXSTOREFORWARD	__BIT(21) /* start TX when a
 						    full frame is available */
#define	GMAC_DMA_OP_FLUSHTX		__BIT(20) /* flush TX fifo */
#define	GMAC_DMA_OP_TXSTART		__BIT(13) /* start TX DMA engine */
#define	GMAC_DMA_OP_RXSTART		__BIT(1)  /* start RX DMA engine */

#define	GMAC_DMA_INT_NIE		__BIT(16) /* Normal/Summary */
#define	GMAC_DMA_INT_AIE		__BIT(15) /* Abnormal/Summary */
#define	GMAC_DMA_INT_ERE		__BIT(14) /* Early receive */
#define	GMAC_DMA_INT_FBE		__BIT(13) /* Fatal bus error */
#define	GMAC_DMA_INT_ETE		__BIT(10) /* Early transmit */
#define	GMAC_DMA_INT_RWE		__BIT(9)  /* Receive watchdog */
#define	GMAC_DMA_INT_RSE		__BIT(8)  /* Receive stopped */
#define	GMAC_DMA_INT_RUE		__BIT(7)  /* Receive buffer unavail. */
#define	GMAC_DMA_INT_RIE		__BIT(6)  /* Receive interrupt */
#define	GMAC_DMA_INT_UNE		__BIT(5)  /* Tx underflow */
#define	GMAC_DMA_INT_OVE		__BIT(4)  /* Receive overflow */
#define	GMAC_DMA_INT_TJE		__BIT(3)  /* Transmit jabber */
#define	GMAC_DMA_INT_TUE		__BIT(2)  /* Transmit buffer unavail. */
#define	GMAC_DMA_INT_TSE		__BIT(1)  /* Transmit stopped */
#define	GMAC_DMA_INT_TIE		__BIT(0)  /* Transmit interrupt */

#define	GMAC_DMA_INT_MASK	__BITS(0,16)	  /* all possible intr bits */

struct dwc_gmac_dev_dmadesc {
	uint32_t ddesc_status;
/* both: */
#define	DDESC_STATUS_OWNEDBYDEV		__BIT(31)

/* for RX descriptors */
#define	DDESC_STATUS_DAFILTERFAIL	__BIT(30)
#define	DDESC_STATUS_FRMLENMSK		__BITS(29,16)
#define	DDESC_STATUS_FRMLENSHIFT	16
#define	DDESC_STATUS_RXERROR		__BIT(15)
#define	DDESC_STATUS_RXTRUNCATED	__BIT(14)
#define	DDESC_STATUS_SAFILTERFAIL	__BIT(13)
#define	DDESC_STATUS_RXIPC_GIANTFRAME	__BIT(12)
#define	DDESC_STATUS_RXDAMAGED		__BIT(11)
#define	DDESC_STATUS_RXVLANTAG		__BIT(10)
#define	DDESC_STATUS_RXFIRST		__BIT(9)
#define	DDESC_STATUS_RXLAST		__BIT(8)
#define	DDESC_STATUS_RXIPC_GIANT	__BIT(7)
#define	DDESC_STATUS_RXCOLLISION	__BIT(6)
#define	DDESC_STATUS_RXFRAMEETHER	__BIT(5)
#define	DDESC_STATUS_RXWATCHDOG		__BIT(4)
#define	DDESC_STATUS_RXMIIERROR		__BIT(3)
#define	DDESC_STATUS_RXDRIBBLING	__BIT(2)
#define	DDESC_STATUS_RXCRC		__BIT(1)

	uint32_t ddesc_cntl;

/* for TX descriptors */
#define	DDESC_CNTL_TXINT		__BIT(31)
#define	DDESC_CNTL_TXLAST		__BIT(30)
#define	DDESC_CNTL_TXFIRST		__BIT(29)
#define	DDESC_CNTL_TXCHECKINSCTRL	__BITS(27,28)

#define	    DDESC_TXCHECK_DISABLED	0
#define	    DDESC_TXCHECK_IP		1
#define	    DDESC_TXCHECK_IP_NO_PSE	2
#define	    DDESC_TXCHECK_FULL		3

#define	DDESC_CNTL_TXCRCDIS		__BIT(26)
#define	DDESC_CNTL_TXRINGEND		__BIT(25)
#define	DDESC_CNTL_TXCHAIN		__BIT(24)
#define	DDESC_CNTL_TXDISPAD		__BIT(23)

/* for RX descriptors */
#define	DDESC_CNTL_RXINTDIS		__BIT(31)
#define	DDESC_CNTL_RXRINGEND		__BIT(25)
#define	DDESC_CNTL_RXCHAIN		__BIT(24)

/* both */
#define	DDESC_CNTL_SIZE1MASK		__BITS(10,0)
#define	DDESC_CNTL_SIZE1SHIFT		0
#define	DDESC_CNTL_SIZE2MASK		__BITS(21,11)
#define	DDESC_CNTL_SIZE2SHIFT		11

	uint32_t ddesc_data;	/* pointer to buffer data */
	uint32_t ddesc_next;	/* link to next descriptor */
};
