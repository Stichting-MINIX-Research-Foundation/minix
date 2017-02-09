/*	$NetBSD: if_stereg.h,v 1.5 2008/04/28 20:23:55 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _DEV_PCI_IF_STEREG_H_
#define	_DEV_PCI_IF_STEREG_H_

/*
 * Register description for the Sundance Tech. ST-201 10/100
 * Ethernet controller.
 */

/*
 * ST-201 buffer fragment descriptor.
 */
struct ste_frag {
	uint32_t	frag_addr;	/* buffer address */
	uint32_t	frag_len;	/* buffer length */
} __packed;

#define	FRAG_LEN	0x00001fff	/* length mask */
#define	FRAG_LAST	(1U << 31)	/* last frag in list */

/*
 * ST-201 Transmit Frame Descriptor.  Note the number of fragments
 * here is arbitrary, but we can't exceed 512 bytes of TFD.
 */
#define	STE_NTXFRAGS	16
struct ste_tfd {
	uint32_t	tfd_next;	/* next TFD in list */
	uint32_t	tfd_control;	/* control bits */
					/* the buffer fragments */
	struct ste_frag tfd_frags[STE_NTXFRAGS];
} __packed;

#define	TFD_WordAlign_dword	0		/* align to dword in TxFIFO */
#define	TFD_WordAlign_word	2		/* align to word in TxFIFO */
#define	TFD_WordAlign_disable	1		/* disable alignment */
#define	TFD_FrameId(x)		((x) << 2)
#define	TFD_FrameId_MAX		0xff
#define	TFD_FcsAppendDisable	(1U << 13)
#define	TFD_TxIndicate		(1U << 15)
#define	TFD_TxDMAComplete	(1U << 16)
#define	TFD_TxDMAIndicate	(1U << 31)

/*
 * ST-201 Receive Frame Descriptor.  Note the number of fragments
 * here is arbitrary (we only use one), but we can't exceed 512
 * bytes of RFD.
 */
struct ste_rfd {
	uint32_t	rfd_next;	/* next RFD in list */
	uint32_t	rfd_status;	/* status bits */
	struct ste_frag rfd_frag;	/* the buffer */
} __packed;

#define	RFD_RxDMAFrameLen(x)	((x) & FRAG_LEN)
#define	RFD_RxFrameError	(1U << 14)
#define	RFD_RxDMAComplete	(1U << 15)
#define	RFD_RxFIFOOverrun	(1U << 16)
#define	RFD_RxRuntFrame		(1U << 17)
#define	RFD_RxAlignmentError	(1U << 18)
#define	RFD_RxFCSError		(1U << 19)
#define	RFD_RxOversizedFrame	(1U << 20)
#define	RFD_DribbleBits		(1U << 23)
#define	RFD_RxDMAOverflow	(1U << 24)
#define	RFD_ImpliedBufferEnable	(1U << 28)

/*
 * PCI configuration registers used by the ST-201.
 */

#define	STE_PCI_IOBA		(PCI_MAPREG_START + 0x00)
#define	STE_PCI_MMBA		(PCI_MAPREG_START + 0x04)

/*
 * EEPROM offsets.
 */
#define	STE_EEPROM_ConfigParam		0x00
#define	STE_EEPROM_AsicCtrl		0x02
#define	STE_EEPROM_SubSystemVendorId	0x04
#define	STE_EEPROM_SubSystemId		0x06
#define	STE_EEPROM_StationAddress0	0x10
#define	STE_EEPROM_StationAddress1	0x12
#define	STE_EEPROM_StationAddress2	0x14

/*
 * The ST-201 register space.
 */

#define	STE_DMACtrl		0x00	/* 32-bit */
#define	DC_RxDMAHalted		(1U << 0)
#define	DC_TxDMACmplReq		(1U << 1)
#define	DC_TxDMAHalted		(1U << 2)
#define	DC_RxDMAComplete	(1U << 3)
#define	DC_TxDMAComplete	(1U << 4)
#define	DC_RxDMAHalt		(1U << 8)
#define	DC_RxDMAResume		(1U << 9)
#define	DC_TxDMAHalt		(1U << 10)
#define	DC_TxDMAResume		(1U << 11)
#define	DC_TxDMAInProg		(1U << 14)
#define	DC_DMAHaltBusy		(1U << 15)
#define	DC_RxEarlyEnable	(1U << 17)
#define	DC_CountdownSpeed	(1U << 18)
#define	DC_CountdownMode	(1U << 19)
#define	DC_MWIDisable		(1U << 20)
#define	DC_RxDMAOverrunFrame	(1U << 22)
#define	DC_CountdownIntEnable	(1U << 23)
#define	DC_TargetAbort		(1U << 30)
#define	DC_MasterAbort		(1U << 31)

#define	STE_TxDMAListPtr	0x04	/* 32-bit */

#define	STE_TxDMABurstThresh	0x08	/* 8-bit */

#define	STE_TxDMAUrgentThresh	0x09	/* 8-bit */

#define	STE_TxDMAPollPeriod	0x0a	/* 8-bit */

#define	STE_RxDMAStatus		0x0c	/* 32-bit */
#define	RDS_RxDMAFrameLen(x)	((x) & 0x1fff)
#define	RDS_RxFrameError	(1U << 14)
#define	RDS_RxDMAComplete	(1U << 15)
#define	RDS_RxFIFOOverrun	(1U << 16)
#define	RDS_RxRuntFrame		(1U << 17)
#define	RDS_RxAlignmentError	(1U << 18)
#define	RDS_RxFCSError		(1U << 19)
#define	RDS_RxOversizedFrame	(1U << 20)
#define	RDS_DribbleBits		(1U << 23)
#define	RDS_RxDMAOverflow	(1U << 24)

#define	STE_RxDMAListPtr	0x10	/* 32-bit */

#define	STE_RxDMABurstThresh	0x14	/* 8-bit */

#define	STE_RxDMAUrgentThresh	0x15	/* 8-bit */

#define	STE_RxDMAPollPeriod	0x16	/* 8-bit */

#define	STE_DebugCtrl		0x1a	/* 16-bit */
#define	DC_GPIO0Ctrl		(1U << 0)	/* 1 = input */
#define	DC_GPIO1Ctrl		(1U << 1)	/* 1 = input */
#define	DC_GPIO0		(1U << 2)
#define	DC_GPIO1		(1U << 3)

#define	STE_AsicCtrl		0x30	/* 32-bit */
#define	AC_ExpRomSize		(1U << 1)	/* 0 = 32K, 1 = 64K */
#define	AC_TxLargeEnable	(1U << 2)	/* > 2K */
#define	AC_RxLargeEnable	(1U << 3)	/* > 2K */
#define	AC_ExpRomDisable	(1U << 4)
#define	AC_PhySpeed10		(1U << 5)
#define	AC_PhySpeed100		(1U << 6)
#define	AC_PhyMedia(x)		(((x) >> 7) & 0x7)
#define	AC_PhyMedia_10T		1
#define	AC_PhyMedia_100T	2
#define	AC_PhyMedia_10_100T	3
#define	AC_PhyMedia_10F		5
#define	AC_PhyMedia_100F	6
#define	AC_PhyMedia_10_100F	7
#define	AC_ForcedConfig(x)	(((x) >> 8) & 0x7)
#define	AC_D3ResetDisable	(1U << 11)
#define	AC_SpeedupMode		(1U << 13)
#define	AC_LEDMode		(1U << 14)
#define	AC_RstOutPolarity	(1U << 15)
#define	AC_GlobalReset		(1U << 16)
#define	AC_RxReset		(1U << 17)
#define	AC_TxReset		(1U << 18)
#define	AC_DMA			(1U << 19)
#define	AC_FIFO			(1U << 20)
#define	AC_Network		(1U << 21)
#define	AC_Host			(1U << 22)
#define	AC_AutoInit		(1U << 23)
#define	AC_RstOut		(1U << 24)
#define	AC_InterruptRequest	(1U << 25)
#define	AC_ResetBusy		(1U << 26)

#define	STE_EepromData		0x34	/* 16-bit */

#define	STE_EepromCtrl		0x36	/* 16-bit */
#define	EC_EepromAddress(x)	((x) & 0xff)
#define	EC_EepromOpcode(x)	((x) << 8)
#define	EC_OP_WE		0
#define	EC_OP_W			1
#define	EC_OP_R			2
#define	EC_OP_E			3
#define	EC_EepromBusy		(1U << 15)

#define	STE_FIFOCtrl		0x3a	/* 16-bit */
#define	FC_RAMTestMode		(1U << 0)
#define	FC_RxOverrunFrame	(1U << 9)
#define	FC_RxFIFOFull		(1U << 11)
#define	FC_Transmitting		(1U << 14)
#define	FC_Receiving		(1U << 15)

#define	STE_TxStartThresh	0x3c	/* 16-bit */

#define	STE_RxEarlyThresh	0x3e	/* 16-bit */

#define	STE_ExpRomAddr		0x40	/* 32-bit */

#define	STE_ExpRomData		0x44	/* 8-bit */

#define	STE_WakeEvent		0x45	/* 8-bit */
#define	WE_WakePktEnable	(1U << 0)
#define	WE_MagicPktEnable	(1U << 1)
#define	WE_LinkEventEnable	(1U << 2)
#define	WE_WakePolarity		(1U << 3)
#define	WE_WakePktEvent		(1U << 4)
#define	WE_MagicPktEvent	(1U << 5)
#define	WE_LinkEvent		(1U << 6)
#define	WE_WakeOnLanEnable	(1U << 7)

#define	STE_TxStatus		0x46	/* 8-bit */
#define	TS_TxReleaseError	(1U << 1)
#define	TS_TxStatusOverflow	(1U << 2)
#define	TS_MaxCollisions	(1U << 3)
#define	TS_TxUnderrun		(1U << 4)
#define	TS_TxIndicateReqd	(1U << 6)
#define	TS_TxComplete		(1U << 7)

#define	STE_TxFrameId		0x47	/* 8-bit */

#define	STE_Countdown		0x48	/* 16-bit */

#define	STE_IntStatusAck	0x4a	/* 16-bit */

#define	STE_IntEnable		0x4c	/* 16-bit */
#define	IE_HostError		(1U << 1)
#define	IE_TxComplete		(1U << 2)
#define	IE_MACControlFrame	(1U << 3)
#define	IE_RxComplete		(1U << 4)
#define	IE_RxEarly		(1U << 5)
#define	IE_IntRequested		(1U << 6)
#define	IE_UpdateStats		(1U << 7)
#define	IE_LinkEvent		(1U << 8)
#define	IE_TxDMAComplete	(1U << 9)
#define	IE_RxDMAComplete	(1U << 10)

#define	STE_IntStatus		0x4e	/* 16-bit */
#define	IS_InterruptStatus	(1U << 0)

#define	STE_MacCtrl0		0x50	/* 16-bit */
#define	MC0_IFSSelect(x)	((x) << 0)
#define	MC0_FullDuplexEnable	(1U << 5)
#define	MC0_RcvLargeFrames	(1U << 6)
#define	MC0_FlowControlEnable	(1U << 8)
#define	MC0_RcvFCS		(1U << 9)
#define	MC0_FIFOLoopback	(1U << 10)
#define	MC0_MACLoopback		(1U << 11)

#define	STE_MacCtrl1		0x52	/* 16-bit */
#define	MC1_CollsionDetect	(1U << 0)
#define	MC1_CarrierSense	(1U << 1)
#define	MC1_TxInProg		(1U << 2)
#define	MC1_TxError		(1U << 3)
#define	MC1_StatisticsEnable	(1U << 5)
#define	MC1_StatisticsDisable	(1U << 6)
#define	MC1_StatisticsEnabled	(1U << 7)
#define	MC1_TxEnable		(1U << 8)
#define	MC1_TxDisable		(1U << 9)
#define	MC1_TxEnabled		(1U << 10)
#define	MC1_RxEnable		(1U << 11)
#define	MC1_RxDisable		(1U << 12)
#define	MC1_RxEnabled		(1U << 13)
#define	MC1_Paused		(1U << 14)

#define	STE_StationAddress0	0x54	/* 16-bit */

#define	STE_StationAddress1	0x56	/* 16-bit */

#define	STE_StationAddress2	0x58	/* 16-bit */

#define	STE_MaxFrameSize	0x5a	/* 16-bit */

#define	STE_ReceiveMode		0x5c	/* 8-bit */
#define	RM_ReceiveUnicast	(1U << 0)
#define	RM_ReceiveMulticast	(1U << 1)
#define	RM_ReceiveBroadcast	(1U << 2)
#define	RM_ReceiveAllFrames	(1U << 3)
#define	RM_ReceiveMulticastHash	(1U << 4)
#define	RM_ReceiveIPMulticast	(1U << 5)

#define	STE_TxReleaseThresh	0x5d	/* 8-bit */

#define	STE_PhyCtrl		0x5e	/* 8-bit */
#define	PC_MgmtClk		(1U << 0)
#define	PC_MgmtData		(1U << 1)
#define	PC_MgmtDir		(1U << 2)	/* 1 = MAC->Phy */
#define	PC_DisableClk25		(1U << 3)
#define	PC_PhyDuplexPolarity	(1U << 4)
#define	PC_PhyDuplexStatus	(1U << 5)
#define	PC_PhySpeedStatus	(1U << 6)
#define	PC_PhyLinkStatus	(1U << 7)

#define	STE_HashTable0		0x60	/* 16-bit */

#define	STE_HashTable1		0x62	/* 16-bit */

#define	STE_HashTable2		0x64	/* 16-bit */

#define	STE_HashTable3		0x66	/* 16-bit */

#define	STE_OctetsReceivedOk0	0x68	/* 16-bit */

#define	STE_OctetsReceivedOk1	0x6a	/* 16-bit */

#define	STE_OctetsTransmittedOk0 0x6c	/* 16-bit */

#define	STE_OctetsTransmittedOk1 0x6e	/* 16-bit */

#define	STE_FramesTransmittedOK	0x70	/* 16-bit */

#define	STE_FramesReceivedOK	0x72	/* 16-bit */

#define	STE_CarrierSenseErrors	0x74	/* 8-bit */

#define	STE_LateCollisions	0x75	/* 8-bit */

#define	STE_MultipleColFrames	0x76	/* 8-bit */

#define	STE_SingleColFrames	0x77	/* 8-bit */

#define	STE_FramesWDeferredXmt	0x78	/* 8-bit */

#define	STE_FramesLostRxErrors	0x79	/* 8-bit */

#define	STE_FramesWExDeferral	0x7a	/* 8-bit */

#define	STE_FramesXbortXSColls	0x7b	/* 8-bit */

#define	STE_BcstFramesXmtdOk	0x7c	/* 8-bit */

#define	STE_BcstFramesRcvdOk	0x7d	/* 8-bit */

#define	STE_McstFramesXmtdOk	0x7e	/* 8-bit */

#define	STE_McstFramesRcvdOk	0x7f	/* 8-bit */

#endif /* _DEV_PCI_IF_STEREG_H_ */
