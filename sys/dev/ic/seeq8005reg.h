/* $NetBSD: seeq8005reg.h,v 1.4 2001/04/01 21:15:15 bjh21 Exp $ */

/*
 * Copyright (c) 1995-1998 Mark Brinicombe
 * All rights reserved.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * SEEQ 8005/80C04/80C04A registers
 *
 * Note that A0 is only used to distinguish halves of 16-bit registers in
 * 8-bit mode.
 */

#define SEEQ_COMMAND		0x0
#define SEEQ_STATUS		0x0
#define SEEQ_CONFIG1		0x2
#define SEEQ_CONFIG2		0x4
#define SEEQ_RX_END		0x6
#define SEEQ_BUFWIN		0x8
#define SEEQ_RX_PTR		0xa
#define SEEQ_TX_PTR		0xc
#define SEEQ_DMA_ADDR		0xe

#define	SEEQ_CMD_DMA_INTEN	(1 << 0)	/* 8005 */
#define	SEEQ_CMD_TEST_INTEN	(1 << 0)	/* 8004 */
#define	SEEQ_CMD_RX_INTEN	(1 << 1)
#define	SEEQ_CMD_TX_INTEN	(1 << 2)
#define	SEEQ_CMD_BW_INTEN	(1 << 3)
#define	SEEQ_CMD_DMA_INTACK	(1 << 4)	/* 8005 */
#define	SEEQ_CMD_TEST_INTACK	(1 << 4)	/* 8004 */
#define	SEEQ_CMD_RX_INTACK	(1 << 5)
#define	SEEQ_CMD_TX_INTACK	(1 << 6)
#define	SEEQ_CMD_BW_INTACK	(1 << 7)
#define	SEEQ_CMD_DMA_ON		(1 << 8)	/* 8005 */
#define	SEEQ_CMD_TEST_INT1	(1 << 8)	/* 8004 */
#define	SEEQ_CMD_RX_ON		(1 << 9)
#define	SEEQ_CMD_TX_ON		(1 << 10)
#define	SEEQ_CMD_DMA_OFF	(1 << 11)	/* 8005 */
#define	SEEQ_CMD_TEST_INT2	(1 << 11)	/* 8004 */
#define	SEEQ_CMD_RX_OFF		(1 << 12)
#define	SEEQ_CMD_TX_OFF		(1 << 13)
#define	SEEQ_CMD_FIFO_READ	(1 << 14)
#define	SEEQ_CMD_FIFO_WRITE	(1 << 15)

#define	SEEQ_STATUS_DMA_INT	(1 << 4)	/* 8005 */
#define	SEEQ_STATUS_TEST_INT	(1 << 4)	/* 8004 */
#define	SEEQ_STATUS_RX_INT	(1 << 5)
#define	SEEQ_STATUS_TX_INT	(1 << 6)
#define	SEEQ_STATUS_BW_INT	(1 << 7)
#define	SEEQ_STATUS_DMA_ON	(1 << 8)	/* 8005 */
#define	SEEQ_STATUS_TEST_ON	(1 << 8)	/* 8004 */
#define	SEEQ_STATUS_RX_ON	(1 << 9)
#define	SEEQ_STATUS_TX_ON	(1 << 10)
#define	SEEQ_STATUS_TX_NOFAIL	(1 << 12)	/* 8004 */
#define	SEEQ_STATUS_FIFO_FULL	(1 << 13)
#define	SEEQ_STATUS_FIFO_EMPTY	(1 << 14)
#define	SEEQ_STATUS_FIFO_DIR	(1 << 15)
#define	SEEQ_STATUS_FIFO_READ	(1 << 15)

#define	SEEQ_BUFCODE_STATION_ADDR0	0x00
#define	SEEQ_BUFCODE_STATION_ADDR1	0x01	/* 8005 and 80C04A */
#define	SEEQ_BUFCODE_STATION_ADDR2	0x02	/* 8005 */
#define SEEQ_BUFCODE_CRCERR_COUNT      	0x02	/* 80C04A */
#define	SEEQ_BUFCODE_STATION_ADDR3	0x03	/* 8005 */
#define SEEQ_BUFCODE_DRIBBLE_COUNT	0x03	/* 80C04A */
#define	SEEQ_BUFCODE_STATION_ADDR4	0x04	/* 8005 */
#define SEEQ_BUFCODE_OVERSIZE_COUNT	0x04	/* 80C04A */
#define	SEEQ_BUFCODE_STATION_ADDR5	0x05	/* 8005 */
#define	SEEQ_BUFCODE_ADDRESS_PROM	0x06
#define	SEEQ_BUFCODE_TX_EAP		0x07
#define	SEEQ_BUFCODE_LOCAL_MEM		0x08
#define	SEEQ_BUFCODE_INT_VECTOR		0x09	/* 8005 */
#define SEEQ_BUFCODE_LC_DFR_COUNT	0x09	/* 80C04A */
#define	SEEQ_BUFCODE_TX_COLLS		0x0b	/* 8004 */
#define	SEEQ_BUFCODE_CONFIG3		0x0c	/* 8004 */
#define	SEEQ_BUFCODE_PRODUCTID		0x0d	/* 8004 */
#define	SEEQ_BUFCODE_TESTENABLE		0x0e	/* 8004 */
#define	SEEQ_BUFCODE_MULTICAST		0x0f	/* 8004 */

#define	SEEQ_CFG1_DMA_BURST_CONT	(0 << 4)	/* 8005 */
#define	SEEQ_CFG1_DMA_BURST_800		(1 << 4)	/* 8005 */
#define	SEEQ_CFG1_DMA_BURST_1600	(2 << 4)	/* 8005 */
#define	SEEQ_CFG1_DMA_BURST_3200	(3 << 4)	/* 8005 */
#define	SEEQ_CFG1_DMA_BSIZE_1		(0 << 6)	/* 8005 */
#define	SEEQ_CFG1_DMA_BSIZE_4		(1 << 6)	/* 8005 */
#define	SEEQ_CFG1_DMA_BSIZE_8		(2 << 6)	/* 8005 */
#define	SEEQ_CFG1_DMA_BSIZE_16		(3 << 6)	/* 8005 */

#define	SEEQ_CFG1_STATION_ADDR0		(1 << 8)	/* 8005 */
#define	SEEQ_CFG1_STATION_ADDR1		(1 << 9)	/* 8005 */
#define	SEEQ_CFG1_STATION_ADDR2		(1 << 10)	/* 8005 */
#define	SEEQ_CFG1_STATION_ADDR3		(1 << 11)	/* 8005 */
#define	SEEQ_CFG1_STATION_ADDR4		(1 << 12)	/* 8005 */
#define	SEEQ_CFG1_STATION_ADDR5		(1 << 13)	/* 8005 */
#define	SEEQ_CFG1_SPECIFIC		((0 << 15) | (0 << 14))
#define	SEEQ_CFG1_BROADCAST		((0 << 15) | (1 << 14))
#define	SEEQ_CFG1_MULTICAST		((1 << 15) | (0 << 14))
#define	SEEQ_CFG1_PROMISCUOUS		((1 << 15) | (1 << 14))

#define	SEEQ_CFG2_BYTESWAP		(1 << 0)
#define	SEEQ_CFG2_REA_AUTOUPDATE	(1 << 1)	/* 8004 only */
#define	SEEQ_CFG2_RX_TX_DISABLE		(1 << 2)	/* 8004 only */
#define	SEEQ_CFG2_CRC_ERR_ENABLE	(1 << 3)
#define	SEEQ_CFG2_DRIB_ERR_ENABLE	(1 << 4)
#define	SEEQ_CFG2_PASS_SHORT		(1 << 5)	/* 8005 */
#define	SEEQ_CFG2_PASS_LONGSHORT	(1 << 5)	/* 8004 */
#define	SEEQ_CFG2_SLOT_SELECT		(1 << 6)	/* 8005 only */
#define	SEEQ_CFG2_PREAM_SELECT		(1 << 7)
#define	SEEQ_CFG2_ADDR_LENGTH		(1 << 8)	/* 8005 only */
#define	SEEQ_CFG2_RX_CRC		(1 << 9)
#define	SEEQ_CFG2_NO_TX_CRC		(1 << 10)
#define	SEEQ_CFG2_LOOPBACK		(1 << 11)
#define	SEEQ_CFG2_OUTPUT		(1 << 12)
#define	SEEQ_CFG2_RESET			(1 << 15)

#define	SEEQ_CFG3_AUTOPAD		(1 << 0)	/* 80C04 */
#define SEEQ_CFG3_SAHASHENABLE		(1 << 1)	/* 80C04A */
#define	SEEQ_CFG3_SQEENABLE		(1 << 2)	/* 80C04 */
#define	SEEQ_CFG3_SLEEP			(1 << 3)	/* 80C04 */
#define	SEEQ_CFG3_READYADVD		(1 << 4)	/* 80C04 only */
#define	SEEQ_CFG3_SECONDADDRENABLE	(1 << 5)	/* 80C04A */
#define	SEEQ_CFG3_GROUPADDR		(1 << 6)	/* 80C04 */
#define	SEEQ_CFG3_NPPBYTE		(1 << 7)	/* 80C04 */

#define	SEEQ_PRODUCTID_MASK		0xf0
#define	SEEQ_PRODUCTID_8004		0xa0
#define	SEEQ_PRODUCTID_REV_MASK		0x0f
#define SEEQ_PRODUCTID_REV_80C04	0x0f
#define SEEQ_PRODUCTID_REV_80C04A	0x0e

#define	SEEQ_PKTCMD_TX			(1 << 7)
#define	SEEQ_PKTCMD_RX			(0 << 7)
#define	SEEQ_PKTCMD_CHAIN_CONT		(1 << 6)
#define	SEEQ_PKTCMD_DATA_FOLLOWS	(1 << 5)

#define	SEEQ_PKTSTAT_DONE		(1 << 7)

#define	SEEQ_TXSTAT_BABBLE		(1 << 0)
#define	SEEQ_TXSTAT_COLLISION		(1 << 1)
#define	SEEQ_TXSTAT_COLLISION16		(1 << 2)
#define	SEEQ_TXSTAT_COLLISIONS_SHIFT	3		/* SEEQ 8004 */
#define	SEEQ_TXSTAT_COLLISION_MASK	0x0f		/* SEEQ 8004 */
#define SEEQ_TXSTAT_CARRIER_DROPOUT	(1 << 3)	/* SEEQ 80C04A */
#define SEEQ_TXSTAT_OK_BUT_DEFERRED	(1 << 4)	/* SEEQ 80C04A */
#define SEEQ_TXSTAT_OK_BUT_COLLISIONS	(1 << 5)	/* SEEQ 80C04A */
#define SEEQ_TXSTAT_OK_BUT_COLLISION	(1 << 6)	/* SEEQ 80C04A */

#define	SEEQ_TXCMD_BABBLE_INT		(1 << 0)
#define	SEEQ_TXCMD_COLLISION_INT	(1 << 1)
#define	SEEQ_TXCMD_COLLISION16_INT	(1 << 2)
#define	SEEQ_TXCMD_XMIT_SUCCESS_INT	(1 << 3)
#define	SEEQ_TXCMD_SQE_TEST_INT		(1 << 4)	/* SEEQ 8004 */

#define	SEEQ_RXSTAT_OVERSIZE		(1 << 0)
#define	SEEQ_RXSTAT_CRC_ERROR		(1 << 1)
#define	SEEQ_RXSTAT_DRIBBLE_ERROR	(1 << 2)
#define	SEEQ_RXSTAT_SHORT_FRAME		(1 << 3)
#define	SEEQ_RXSTAT_ERROR_MASK		0x0f

#define	SEEQ_MAX_BUFFER_SIZE		0x10000
