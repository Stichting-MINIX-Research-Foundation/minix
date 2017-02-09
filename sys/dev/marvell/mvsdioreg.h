/*	$NetBSD: mvsdioreg.h,v 1.1 2010/09/23 12:36:01 kiyohara Exp $	*/
/*
 * Copyright (c) 2010 KIYOHARA Takashi
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _MVSDIOREG_H_
#define _MVSDIOREG_H_

#define MVSDIO_SIZE	0x10000

#define MVSDIO_MAX_CLOCK	(50 * 1000)	/* 50,000 kHz */

#define MVSDIO_DMABA16LSB	0x0000	/* DMA Buffer Address 16 LSB */
#define MVSDIO_DMABA16MSB	0x0004	/* DMA Buffer Address 16 MSB */
#define MVSDIO_DBS		0x0008	/* Data Block Size */
#define   DBS_BLOCKSIZE_MASK		0xfff
#define   DBS_BLOCKSIZE(s)		((s) & DBS_BLOCKSIZE_MASK)
#define   DBS_BLOCKSIZE_MAX		0x800
#define MVSDIO_DBC		0x000c	/* Data Block Count */
#define   DBC_BLOCKCOUNT_MASK		0xfff
#define   DBC_BLOCKCOUNT(c)		((c) & DBC_BLOCKCOUNT_MASK)
#define MVSDIO_AC16LSB		0x0010	/* Argument in Command 16 LSB */
#define MVSDIO_AC16MSB		0x0014	/* Argument in Command 16 MSB */
#define MVSDIO_TM		0x0018	/* Transfer Mode */
#define   TM_SWWRDATASTART		(1 << 0)	/* InitWrDataXfer */
#define   TM_HWWRDATAEN			(1 << 1)	/* SWInitWrDataXfer */
#define   TM_AUTOCMD12EN		(1 << 2)	/* SWIssuesCMD12 */
#define   TM_INTCHKEN			(1 << 3)	/* CheckInterrupts */
#define   TM_DATAXFERTOWARDHOST		(1 << 4)	/* XferDataToSDHost */
#define   TM_STOPCLKEN			(1 << 5)	/* StopSDClocks */
#define   TM_HOSTXFERMODE		(1 << 6)	/* SW Write */
#define MVSDIO_C		0x001c	/* Command */
#define   C_RESPTYPE_NR			(0 << 0)	/* NoResponse */
#define   C_RESPTYPE_136BR		(1 << 0)	/* 136BitResponse */
#define   C_RESPTYPE_48BR		(2 << 0)	/* 48BitResponse */
#define   C_RESPTYPE_48BRCB		(3 << 0)	/*48BitResponseChkBusy*/
#define   C_DATACRC16CHKEN		(1 << 2)	/* EnableCrc16Chk */
#define   C_CMDCRCCHKEN			(1 << 3)	/* HCCrcChk */
#define   C_CMDINDEXCHKEN		(1 << 4)	/* HCChkIndex */
#define   C_DATAPRESENT			(1 << 5)	/* DataAwaitsTransfer */
#define   C_UNEXPECTEDRESPEN		(1 << 7)	/* UnexpectedRespEn */
#define   C_CMDINDEX(c)			((c) << 8)
#define MVSDIO_NRH		8
#define MVSDIO_RH(n)		(0x0020	+ ((n) << 2)) /* Response Halfword n */
#define   RH_MASK			0xffff
#define MVSDIO_16DWACPU		0x0040	/* 16-bit Data Word Accessed by CPU */
#define MVSDIO_CRC7lR		0x0044	/* CRC7 of l Response */
#define   CRC7lR_CRC7RESPTOKEN_MASK	0x7f
#define MVSDIO_HPS16LSB		0x0048	/* Host Present State 16 LSB */
#define   HPS16LSB_CMDINHIBITCMD	(1 <<  0)	/* CmdRegWrite */
#define   HPS16LSB_CARDBUSY		(1 <<  1)	/* Card Busy */
#define   HPS16LSB_DATLEVEL(x)		(((x) >> 3) & 0xf)	/* DAT[3:0] Line Signal Level */
#define   HPS16LSB_CMDLEVEL		(1 <<  7)    /* CMD line Signal Level */
#define   HPS16LSB_TXACTIVE		(1 <<  8)	/* TxEnabled */
#define   HPS16LSB_RXACTIVE		(1 <<  9)	/* RxDisabled */
#define   HPS16LSB_FIFOFULL		(1 << 12)	/* FIFO Full */
#define   HPS16LSB_FIFOEMPTY		(1 << 13)	/* FIFO Empty */
#define   HPS16LSB_AUTOCMD12ACTIVE	(1 << 14)	/*auto_cmd12 is active*/
#define MVSDIO_HC		0x0050	/* Host Control */
#define   HC_PUSHPULLEN			(1 <<  0)	/* PushPullEn */
#define   HC_CARDTYPE_MASK		(3 <<  1)	/* Card type */
#define   HC_CARDTYPE_MEMORYONLY	(0 <<  1)	/*   Mem only SD card */
#define   HC_CARDTYPE_IOONLY		(1 <<  1)	/*   IO only SD card */
#define   HC_CARDTYPE_IOMEMCOMBO	(2 <<  1)	/*   IO and mem combo */
#define   HC_CARDTYPE_MMC		(3 <<  1)	/*   MMC card */
#define   HC_BIGENDIAN			(1 <<  3)	/* BigEndian */
#define   HC_LSBFIRST			(1 <<  4)	/* LSB */
#define   HC_DATAWIDTH			(1 <<  9)	/* Data Width */
#define   HC_HISPEEDEN			(1 << 10)	/* HighSpeedEnable */
#define   HC_TIMEOUTVALUE_MAX		(0xf << 11)
#define   HC_TIMEOUTEN			(1 << 15)	/* Timeout */
#define MVSDIO_DBGC		0x0054	/* Data Block Gap Control */
#define   DBGC_STOPATBLOCKGAPREQ	(1 << 0) /* Stop at block gap request */
#define   DBGC_CONTREQ			(1 << 1)	/* Continue request */
#define   DBGC_RDWAITCTL		(1 << 2)	/* EnableRdWait */
#define   DBGC_STOPDATXFER		(1 << 3)	/* StopDataXferEn */
#define   DBGC_RESUME			(1 << 4)
#define   DBGC_SUSPEND			(1 << 5)
#define MVSDIO_CC		0x0058	/* Clock Control */
#define   CC_SCLKMASTEREN		(1 << 0)	/* SdclkEn */
#define MVSDIO_SR		0x005c	/* Software Reset */
#define   SR_SWRESET			(1 << 8)

#define MVSDIO_NIS		0x0060	/* Normal Interrupt Status */
#define MVSDIO_NISE		0x0068	/* Normal Interrupt Status Enable */
#define MVSDIO_NISIE		0x0070	/* Normal Intr Status Intr Enable */
#define   NIS_CMDCOMPLETE		(1 <<  0)	/* Command Complete */
#define   NIS_XFERCOMPLETE		(1 <<  1)	/* Transfer Complete */
#define   NIS_BLOCKGAPEV		(1 <<  2)	/* Block gap event */
#define   NIS_DMAINT			(1 <<  3)	/* DMA interrupt */
#define   NIS_TXRDY			(1 <<  4)
#define   NIS_RXRDY			(1 <<  5)
#define   NIS_CARDINT			(1 <<  8)	/* Card interrupt */
#define   NIS_READWAITON		(1 <<  9)    /* Read Wait state is on */
#define   NIS_IMBFIFO8WFULL		(1 << 10)
#define   NIS_IMBFIFO8WAVAIL		(1 << 11)
#define   NIS_SUSPENSEON		(1 << 12)
#define   NIS_AUTOCMD12COMPLETE		(1 << 13)	/* Auto_cmd12 is comp */
#define   NIS_UNEXPECTEDRESPDET		(1 << 14)
#define   NIS_ERRINT			(1 << 15)	/* Error interrupt */
#define MVSDIO_EIS		0x0064	/* Error Interrupt Status */
#define MVSDIO_EISE		0x006c	/* Error Interrupt Status Enable */
#define MVSDIO_EISIE		0x0074	/* Error Intr Status Interrupt Enable */
#define   EIS_CMDTIMEOUTERR		(1 <<  0)	/*Command timeout err*/
#define   EIS_CMDCRCERR			(1 <<  1)	/* Command CRC Error */
#define   EIS_CMDENDBITERR		(1 <<  2)	/*Command end bit err*/
#define   EIS_CMDINDEXERR		(1 <<  3)	/*Command Index Error*/
#define   EIS_DATATIMEOUTERR		(1 <<  4)	/* Data timeout error */
#define   EIS_RDDATACRCERR		(1 <<  5)	/* Read data CRC err */
#define   EIS_RDDATAENDBITERR		(1 <<  6)	/*Rd data end bit err*/
#define   EIS_AUTOCMD12ERR		(1 <<  8)	/* Auto CMD12 error */
#define   EIS_CMDSTARTBITERR		(1 <<  9)	/*Cmd start bit error*/
#define   EIS_XFERSIZEERR		(1 << 10)     /*Tx size mismatched err*/
#define   EIS_RESPTBITERR		(1 << 11)	/* Response T bit err */
#define   EIS_CRCENDBITERR		(1 << 12)	/* CRC end bit error */
#define   EIS_CRCSTARTBITERR		(1 << 13)	/* CRC start bit err */
#define   EIS_CRCSTATERR		(1 << 14)	/* CRC status error */

#define MVSDIO_ACMD12IS		0x0078	/* Auto CMD12 Interrupt Status */
#define   ACMD12IS_AUTOCMD12NOTEXE	(1 << 0)
#define   ACMD12IS_AUTOCMD12TIMEOUTER	(1 << 1)
#define   ACMD12IS_AUTOCMD12CRCER	(1 << 2)
#define   ACMD12IS_AUTOCMD12ENDBITER	(1 << 3)
#define   ACMD12IS_AUTOCMD12INDEXER	(1 << 4)
#define   ACMD12IS_AUTOCMD12RESPTBITER	(1 << 5)
#define   ACMD12IS_AUTOCMD12RESPSTARTBITER (1 << 6)
#define MVSDIO_CNBRDB		0x007c/*Current Num of Bytes Remaining in Data*/
#define MVSDIO_CNDBLBT		0x0080/*Current Num of Data Blk Left ToBe Txed*/
#define MVSDIO_AACC16LSBT	0x0084 /*Arg in Auto Cmd12 Command 16 LSB Txed*/
#define MVSDIO_AACC16MSBT	0x0088 /*Arg in Auto Cmd12 Command 16 MSB Txed*/
#define MVSDIO_IACCT		0x008c	/* Index of Auto Cmd12 Commands Tx-ed */
#define   IACCT_AUTOCMD12BUSYCHKEN	(1 << 0)
#define   IACCT_AUTOCMD12INDEXCHKEN	(1 << 1)
#define   IACCT_AUTOCMD12INDEX		(MMC_STOP_TRANSMISSION << 8)
#define MVSDIO_ACRH(n)		(0x0090	+ ((n) << 2)) /* Auto Cmd12 Response Halfword n */


#define MVSDIO_MCL		0x0100	/* Mbus Control Low */
#define MVSDIO_MCH		0x0104	/* Mbus Control High */
#define   MCL_SDARBENTRY(n, x)		(((x) & 0xf) << ((n) << 2))

#define MVSDIO_NWINDOW	4
#define MVSDIO_WC(n)		(0x0108 + ((n) << 3)) 	/* Window n Control */
#define   WC_WINEN			(1 << 0)	/* Window n Enable */
#define   WC_TARGET(t)			(((t) & 0xf) << 4)
#define   WC_ATTR(a)			(((a) & 0xff) << 8)
#define   WC_SIZE(s)			(((s) - 1) & 0xffff0000)
#define MVSDIO_WB(n)		(0x010c	+ ((n) << 3))	/* Window n Base */
#define   WB_BASE(b)			((b) & 0xffff0000)
#define MVSDIO_CDV		0x0128	/* Clock Divider Value */
#define   CDV_CLKDVDRMVALUE_MASK	0x7ff
#define MVSDIO_ADE		0x012c	/* Address Decoder Error */
#define   ADE_ADD_DEC_MISS_ERR		(1 << 0)
#define   ADE_ADD_DEC_MULTI_ERR		(1 << 1)
#define MVSDIO_ADEM		0x0130	/* Address Decoder Error Mask */
#define   ADEM_VARIOUS(x)		((x) << 0)	/* Do not mask */

#endif	/* _MVSDIOREG_H_ */
