/* $NetBSD: xhcireg.h,v 1.3 2015/03/28 08:19:20 skrll Exp $ */
/* $FreeBSD$ */

/*-
 * Copyright (c) 2010 Hans Petter Selasky. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _XHCIREG_H_
#define	_XHCIREG_H_

/* XHCI PCI config registers */
#define	PCI_CBMEM		0x10	/* configuration base MEM */
#define	PCI_INTERFACE_XHCI	0x30

#define	PCI_USBREV		0x60	/* RO USB protocol revision */
#define	 PCI_USBREV_MASK	0xFF
#define	 PCI_USBREV_3_0		0x30	/* USB 3.0 */
#define	PCI_XHCI_FLADJ		0x61	/* RW frame length adjust */

#define	PCI_XHCI_INTEL_XUSB2PR	0xD0    /* Intel USB2 Port Routing */
#define	PCI_XHCI_INTEL_USB2PRM	0xD4    /* Intel USB2 Port Routing Mask */
#define	PCI_XHCI_INTEL_USB3_PSSEN 0xD8  /* Intel USB3 Port SuperSpeed Enable */
#define	PCI_XHCI_INTEL_USB3PRM	0xDC    /* Intel USB3 Port Routing Mask */

/* XHCI capability registers */
#define XHCI_CAPLENGTH		0x00	/* RO capability */
#define	XHCI_CAP_CAPLENGTH(x)	((x) & 0xFF)
#define	XHCI_CAP_HCIVERSION(x)	(((x) >> 16) & 0xFFFF)	/* RO Interface version number */
#define	XHCI_HCIVERSION_0_9	0x0090	/* xHCI version 0.9 */
#define	XHCI_HCIVERSION_1_0	0x0100	/* xHCI version 1.0 */
#define	XHCI_HCSPARAMS1		0x04	/* RO structual parameters 1 */
#define	XHCI_HCS1_MAXSLOTS(x)	((x) & 0xFF)
#define	XHCI_HCS1_MAXINTRS(x)	(((x) >> 8) & 0x3FF)
#define	XHCI_HCS1_MAXPORTS(x)	(((x) >> 24) & 0xFF)
#define	XHCI_HCSPARAMS2		0x08	/* RO structual parameters 2 */
#define	XHCI_HCS2_IST(x)	((x) & 0xF)
#define	XHCI_HCS2_ERST_MAX(x)	(((x) >> 4) & 0xF)
#define	XHCI_HCS2_SPR(x)	(((x) >> 24) & 0x1)
#define	XHCI_HCS2_MAXSPBUF(x)	(((x) >> 27) & 0x7F)
#define	XHCI_HCSPARAMS3		0x0C	/* RO structual parameters 3 */
#define	XHCI_HCS3_U1_DEL(x)	((x) & 0xFF)
#define	XHCI_HCS3_U2_DEL(x)	(((x) >> 16) & 0xFFFF)
#define	XHCI_HCCPARAMS		0x10	/* RO capability parameters */
#define	XHCI_HCC_AC64(x)	((x) & 0x1)		/* 64-bit capable */
#define	XHCI_HCC_BNC(x)	(((x) >> 1) & 0x1)	/* BW negotiation */
#define	XHCI_HCC_CSZ(x)	(((x) >> 2) & 0x1)	/* context size */
#define	XHCI_HCC_PPC(x)	(((x) >> 3) & 0x1)	/* port power control */
#define	XHCI_HCC_PIND(x)	(((x) >> 4) & 0x1)	/* port indicators */
#define	XHCI_HCC_LHRC(x)	(((x) >> 5) & 0x1)	/* light HC reset */
#define	XHCI_HCC_LTC(x)	(((x) >> 6) & 0x1)	/* latency tolerance msg */
#define	XHCI_HCC_NSS(x)	(((x) >> 7) & 0x1)	/* no secondary sid */
#define	XHCI_HCC_MAXPSASIZE(x)	(((x) >> 12) & 0xF)	/* max pri. stream array size */
#define	XHCI_HCC_XECP(x)	(((x) >> 16) & 0xFFFF)	/* extended capabilities pointer */
#define	XHCI_DBOFF		0x14	/* RO doorbell offset */
#define	XHCI_RTSOFF		0x18	/* RO runtime register space offset */

/* XHCI operational registers.  Offset given by XHCI_CAPLENGTH register */
#define	XHCI_USBCMD		0x00	/* XHCI command */
#define	XHCI_CMD_RS		0x00000001	/* RW Run/Stop */
#define	XHCI_CMD_HCRST		0x00000002	/* RW Host Controller Reset */
#define	XHCI_CMD_INTE		0x00000004	/* RW Interrupter Enable */
#define	XHCI_CMD_HSEE		0x00000008	/* RW Host System Error Enable */
#define	XHCI_CMD_LHCRST		0x00000080	/* RO/RW Light Host Controller Reset */
#define	XHCI_CMD_CSS		0x00000100	/* RW Controller Save State */
#define	XHCI_CMD_CRS		0x00000200	/* RW Controller Restore State */
#define	XHCI_CMD_EWE		0x00000400	/* RW Enable Wrap Event */
#define	XHCI_CMD_EU3S		0x00000800	/* RW Enable U3 MFINDEX Stop */
#define	XHCI_USBSTS		0x04	/* XHCI status */
#define	XHCI_STS_HCH		0x00000001	/* RO - Host Controller Halted */
#define	XHCI_STS_HSE		0x00000004	/* RW - Host System Error */
#define	XHCI_STS_EINT		0x00000008	/* RW - Event Interrupt */
#define	XHCI_STS_PCD		0x00000010	/* RW - Port Change Detect */
#define	XHCI_STS_SSS		0x00000100	/* RO - Save State Status */
#define	XHCI_STS_RSS		0x00000200	/* RO - Restore State Status */
#define	XHCI_STS_SRE		0x00000400	/* RW - Save/Restore Error */
#define	XHCI_STS_CNR		0x00000800	/* RO - Controller Not Ready */
#define	XHCI_STS_HCE		0x00001000	/* RO - Host Controller Error */
#define	XHCI_PAGESIZE		0x08	/* XHCI page size mask */
#define	XHCI_PAGESIZE_4K	0x00000001	/* 4K Page Size */
#define	XHCI_PAGESIZE_8K	0x00000002	/* 8K Page Size */
#define	XHCI_PAGESIZE_16K	0x00000004	/* 16K Page Size */
#define	XHCI_PAGESIZE_32K	0x00000008	/* 32K Page Size */
#define	XHCI_PAGESIZE_64K	0x00000010	/* 64K Page Size */
#define	XHCI_DNCTRL		0x14	/* XHCI device notification control */
#define	XHCI_DNCTRL_MASK(n)	(1U << (n))
#define	XHCI_CRCR		0x18	/* XHCI command ring control */
#define	XHCI_CRCR_LO_RCS	0x00000001	/* RW - consumer cycle state */
#define	XHCI_CRCR_LO_CS		0x00000002	/* RW - command stop */
#define	XHCI_CRCR_LO_CA		0x00000004	/* RW - command abort */
#define	XHCI_CRCR_LO_CRR	0x00000008	/* RW - command ring running */
#define	XHCI_CRCR_LO_MASK	0x0000000F
#define	XHCI_CRCR_HI		0x1C	/* XHCI command ring control */
#define	XHCI_DCBAAP		0x30	/* XHCI dev context BA pointer */
#define	XHCI_DCBAAP_HI		0x34	/* XHCI dev context BA pointer */
#define	XHCI_CONFIG		0x38
#define	XHCI_CONFIG_SLOTS_MASK	0x000000FF	/* RW - number of device slots enabled */

/* XHCI port status registers */
#define	XHCI_PORTSC(n)		(0x3F0 + (0x10 * (n)))	/* XHCI port status */
#define	XHCI_PS_CCS		0x00000001	/* RO - current connect status */
#define	XHCI_PS_PED		0x00000002	/* RW - port enabled / disabled */
#define	XHCI_PS_OCA		0x00000008	/* RO - over current active */
#define	XHCI_PS_PR		0x00000010	/* RW - port reset */
#define	XHCI_PS_PLS_GET(x)	(((x) >> 5) & 0xF)	/* RW - port link state */
#define	XHCI_PS_PLS_SET(x)	(((x) & 0xF) << 5)	/* RW - port link state */
#define	XHCI_PS_PP		0x00000200	/* RW - port power */
#define	XHCI_PS_SPEED_GET(x)	(((x) >> 10) & 0xF)	/* RO - port speed */
#define	XHCI_PS_PIC_GET(x)	(((x) >> 14) & 0x3)	/* RW - port indicator */
#define	XHCI_PS_PIC_SET(x)	(((x) & 0x3) << 14)	/* RW - port indicator */
#define	XHCI_PS_LWS		0x00010000	/* RW - port link state write strobe */
#define	XHCI_PS_CSC		0x00020000	/* RW - connect status change */
#define	XHCI_PS_PEC		0x00040000	/* RW - port enable/disable change */
#define	XHCI_PS_WRC		0x00080000	/* RW - warm port reset change */
#define	XHCI_PS_OCC		0x00100000	/* RW - over-current change */
#define	XHCI_PS_PRC		0x00200000	/* RW - port reset change */
#define	XHCI_PS_PLC		0x00400000	/* RW - port link state change */
#define	XHCI_PS_CEC		0x00800000	/* RW - config error change */
#define	XHCI_PS_CAS		0x01000000	/* RO - cold attach status */
#define	XHCI_PS_WCE		0x02000000	/* RW - wake on connect enable */
#define	XHCI_PS_WDE		0x04000000	/* RW - wake on disconnect enable */
#define	XHCI_PS_WOE		0x08000000	/* RW - wake on over-current enable */
#define	XHCI_PS_DR		0x40000000	/* RO - device removable */
#define	XHCI_PS_WPR		0x80000000U	/* RW - warm port reset */
#define	XHCI_PS_CLEAR		0x80FF01FFU	/* command bits */

#define	XHCI_PORTPMSC(n)	(0x3F4 + (0x10 * (n)))	/* XHCI status and control */
#define	XHCI_PM3_U1TO_GET(x)	(((x) >> 0) & 0xFF)	/* RW - U1 timeout */
#define	XHCI_PM3_U1TO_SET(x)	(((x) & 0xFF) << 0)	/* RW - U1 timeout */
#define	XHCI_PM3_U2TO_GET(x)	(((x) >> 8) & 0xFF)	/* RW - U2 timeout */
#define	XHCI_PM3_U2TO_SET(x)	(((x) & 0xFF) << 8)	/* RW - U2 timeout */
#define	XHCI_PM3_FLA		0x00010000	/* RW - Force Link PM Accept */
#define	XHCI_PM2_L1S_GET(x)	(((x) >> 0) & 0x7)	/* RO - L1 status */
#define	XHCI_PM2_RWE		0x00000008		/* RW - remote wakup enable */
#define	XHCI_PM2_HIRD_GET(x)	(((x) >> 4) & 0xF)	/* RW - host initiated resume duration */
#define	XHCI_PM2_HIRD_SET(x)	(((x) & 0xF) << 4)	/* RW - host initiated resume duration */
#define	XHCI_PM2_L1SLOT_GET(x)	(((x) >> 8) & 0xFF)	/* RW - L1 device slot */
#define	XHCI_PM2_L1SLOT_SET(x)	(((x) & 0xFF) << 8)	/* RW - L1 device slot */
#define	XHCI_PM2_HLE		0x00010000		/* RW - hardware LPM enable */
#define	XHCI_PORTLI(n)		(0x3F8 + (0x10 * (n)))	/* XHCI port link info */
#define	XHCI_PLI3_ERR_GET(x)	(((x) >> 0) & 0xFFFF)	/* RO - port link errors */
#define	XHCI_PORTRSV(n)		(0x3FC + (0x10 * (n)))	/* XHCI port reserved */

/* XHCI runtime registers.  Offset given by XHCI_CAPLENGTH + XHCI_RTSOFF registers */
#define	XHCI_MFINDEX		0x0000		/* RO - microframe index */
#define	XHCI_MFINDEX_GET(x)	((x) & 0x3FFF)
#define	XHCI_IMAN(n)		(0x0020 + (0x20 * (n)))	/* XHCI interrupt management */
#define	XHCI_IMAN_INTR_PEND	0x00000001	/* RW - interrupt pending */
#define	XHCI_IMAN_INTR_ENA	0x00000002	/* RW - interrupt enable */
#define	XHCI_IMOD(n)		(0x0024 + (0x20 * (n)))	/* XHCI interrupt moderation */
#define	XHCI_IMOD_IVAL_GET(x)	(((x) >> 0) & 0xFFFF)	/* 250ns unit */
#define	XHCI_IMOD_IVAL_SET(x)	(((x) & 0xFFFF) << 0)	/* 250ns unit */
#define	XHCI_IMOD_ICNT_GET(x)	(((x) >> 16) & 0xFFFF)	/* 250ns unit */
#define	XHCI_IMOD_ICNT_SET(x)	(((x) & 0xFFFF) << 16)	/* 250ns unit */
#define	XHCI_IMOD_DEFAULT	0x000001F4U	/* 8000 IRQ/second */
#define	XHCI_IMOD_DEFAULT_LP	0x000003E8U	/* 4000 IRQ/sec for LynxPoint */
#define	XHCI_ERSTSZ(n)		(0x0028 + (0x20 * (n)))	/* XHCI event ring segment table size */
#define	XHCI_ERSTS_GET(x)	((x) & 0xFFFF)
#define	XHCI_ERSTS_SET(x)	((x) & 0xFFFF)
#define	XHCI_ERSTBA(n)	(0x0030 + (0x20 * (n)))	/* XHCI event ring segment table BA */
#define	XHCI_ERSTBA_HI(n)	(0x0034 + (0x20 * (n)))	/* XHCI event ring segment table BA */
#define	XHCI_ERDP(n)	(0x0038 + (0x20 * (n)))	/* XHCI event ring dequeue pointer */
#define	XHCI_ERDP_LO_SINDEX(x)	((x) & 0x7)	/* RO - dequeue segment index */
#define	XHCI_ERDP_LO_BUSY	0x00000008	/* RW - event handler busy */
#define	XHCI_ERDP_HI(n)	(0x003C + (0x20 * (n)))	/* XHCI event ring dequeue pointer */

/* XHCI doorbell registers. Offset given by XHCI_CAPLENGTH + XHCI_DBOFF registers */
#define	XHCI_DOORBELL(n)	(0x0000 + (4 * (n)))
#define	XHCI_DB_TARGET_GET(x)	((x) & 0xFF)		/* RW - doorbell target */
#define	XHCI_DB_TARGET_SET(x)	((x) & 0xFF)		/* RW - doorbell target */
#define	XHCI_DB_SID_GET(x)	(((x) >> 16) & 0xFFFF)	/* RW - doorbell stream ID */
#define	XHCI_DB_SID_SET(x)	(((x) & 0xFFFF) << 16)	/* RW - doorbell stream ID */

/* XHCI legacy support */
#define	XHCI_XECP_ID(x)		((x) & 0xFF)
#define	XHCI_XECP_NEXT(x)	(((x) >> 8) & 0xFF)
#define	XHCI_XECP_BIOS_SEM	0x0002
#define	XHCI_XECP_OS_SEM	0x0003

/* XHCI extended capability ID's */
#define	XHCI_ID_USB_LEGACY	0x0001	/* USB Legacy Support */
#define	 XHCI_XECP_USBLESUP	0x0000	/* Legacy Support Capability Reg */
#define	 XHCI_XECP_USBLEGCTLSTS	0x0004	/* Legacy Support Ctrl & Status Reg */
#define	XHCI_ID_PROTOCOLS	0x0002	/* Supported Protocol */
#define	XHCI_ID_POWER_MGMT	0x0003	/* Extended Power Management */
#define	XHCI_ID_VIRTUALIZATION	0x0004	/* I/O Virtualization */
#define	XHCI_ID_MSG_IRQ		0x0005	/* Message Interrupt */
#define	XHCI_ID_USB_LOCAL_MEM	0x0006	/* Local Memory */
#define	XHCI_ID_USB_DEBUG	0x000A	/* USB Debug Capability */
#define	XHCI_ID_XMSG_IRQ	0x0011	/* Extended Message Interrupt */

#define XHCI_PAGE_SIZE(sc) ((sc)->sc_pgsz)

/* Chapter 6, Table 49 */
#define XHCI_DEVICE_CONTEXT_BASE_ADDRESS_ARRAY_ALIGN 64
#define XHCI_DEVICE_CONTEXT_ALIGN 64
#define XHCI_INPUT_CONTROL_CONTEXT_ALIGN 64
#define XHCI_SLOT_CONTEXT_ALIGN 32
#define XHCI_ENDPOINT_CONTEXT_ALIGN 32
#define XHCI_STREAM_CONTEXT_ALIGN 16
#define XHCI_STREAM_ARRAY_ALIGN 16
#define XHCI_TRANSFER_RING_SEGMENTS_ALIGN 16
#define XHCI_COMMAND_RING_SEGMENTS_ALIGN 64
#define XHCI_EVENT_RING_SEGMENTS_ALIGN 64
#define XHCI_EVENT_RING_SEGMENT_TABLE_ALIGN 64
#define XHCI_SCRATCHPAD_BUFFER_ARRAY_ALIGN 64
#define XHCI_SCRATCHPAD_BUFFERS_ALIGN XHCI_PAGE_SIZE

#define XHCI_ERSTE_ALIGN 16
#define XHCI_TRB_ALIGN 16

struct xhci_trb {
	uint64_t trb_0;
	uint32_t trb_2;
#define XHCI_TRB_2_ERROR_GET(x)         (((x) >> 24) & 0xFF)
#define XHCI_TRB_2_ERROR_SET(x)         (((x) & 0xFF) << 24)
#define XHCI_TRB_2_TDSZ_GET(x)          (((x) >> 17) & 0x1F)
#define XHCI_TRB_2_TDSZ_SET(x)          (((x) & 0x1F) << 17)
#define XHCI_TRB_2_REM_GET(x)           ((x) & 0xFFFFFF)
#define XHCI_TRB_2_REM_SET(x)           ((x) & 0xFFFFFF)
#define XHCI_TRB_2_BYTES_GET(x)         ((x) & 0x1FFFF)
#define XHCI_TRB_2_BYTES_SET(x)         ((x) & 0x1FFFF)
#define XHCI_TRB_2_IRQ_GET(x)           (((x) >> 22) & 0x3FF)
#define XHCI_TRB_2_IRQ_SET(x)           (((x) & 0x3FF) << 22)
#define XHCI_TRB_2_STREAM_GET(x)        (((x) >> 16) & 0xFFFF)
#define XHCI_TRB_2_STREAM_SET(x)        (((x) & 0xFFFF) << 16)
	uint32_t trb_3;
#define XHCI_TRB_3_TYPE_GET(x)          (((x) >> 10) & 0x3F)
#define XHCI_TRB_3_TYPE_SET(x)          (((x) & 0x3F) << 10)
#define XHCI_TRB_3_CYCLE_BIT            (1U << 0)
#define XHCI_TRB_3_TC_BIT               (1U << 1)       /* command ring only */
#define XHCI_TRB_3_ENT_BIT              (1U << 1)       /* transfer ring only */
#define XHCI_TRB_3_ISP_BIT              (1U << 2)
#define XHCI_TRB_3_NSNOOP_BIT           (1U << 3)
#define XHCI_TRB_3_CHAIN_BIT            (1U << 4)
#define XHCI_TRB_3_IOC_BIT              (1U << 5)
#define XHCI_TRB_3_IDT_BIT              (1U << 6)
#define XHCI_TRB_3_TBC_GET(x)           (((x) >> 7) & 3)
#define XHCI_TRB_3_TBC_SET(x)           (((x) & 3) << 7)
#define XHCI_TRB_3_BEI_BIT              (1U << 9)
#define XHCI_TRB_3_DCEP_BIT             (1U << 9)
#define XHCI_TRB_3_PRSV_BIT             (1U << 9)
#define XHCI_TRB_3_BSR_BIT              (1U << 9)
#define XHCI_TRB_3_TRT_MASK             (3U << 16)
#define XHCI_TRB_3_TRT_NONE             (0U << 16)
#define XHCI_TRB_3_TRT_OUT              (2U << 16)
#define XHCI_TRB_3_TRT_IN               (3U << 16)
#define XHCI_TRB_3_DIR_IN               (1U << 16)
#define XHCI_TRB_3_TLBPC_GET(x)         (((x) >> 16) & 0xF)
#define XHCI_TRB_3_TLBPC_SET(x)         (((x) & 0xF) << 16)
#define XHCI_TRB_3_EP_GET(x)            (((x) >> 16) & 0x1F)
#define XHCI_TRB_3_EP_SET(x)            (((x) & 0x1F) << 16)
#define XHCI_TRB_3_FRID_GET(x)          (((x) >> 20) & 0x7FF)
#define XHCI_TRB_3_FRID_SET(x)          (((x) & 0x7FF) << 20)
#define XHCI_TRB_3_ISO_SIA_BIT          (1U << 31)
#define XHCI_TRB_3_SUSP_EP_BIT          (1U << 23)
#define XHCI_TRB_3_VFID_GET(x)          (((x) >> 16) & 0xFF)
#define XHCI_TRB_3_VFID_SET(x)          (((x) & 0xFF) << 16)
#define XHCI_TRB_3_SLOT_GET(x)          (((x) >> 24) & 0xFF)
#define XHCI_TRB_3_SLOT_SET(x)          (((x) & 0xFF) << 24)

	/* Commands */
#define XHCI_TRB_TYPE_RESERVED          0x00
#define XHCI_TRB_TYPE_NORMAL            0x01
#define XHCI_TRB_TYPE_SETUP_STAGE       0x02
#define XHCI_TRB_TYPE_DATA_STAGE        0x03
#define XHCI_TRB_TYPE_STATUS_STAGE      0x04
#define XHCI_TRB_TYPE_ISOCH             0x05
#define XHCI_TRB_TYPE_LINK              0x06
#define XHCI_TRB_TYPE_EVENT_DATA        0x07
#define XHCI_TRB_TYPE_NOOP              0x08
#define XHCI_TRB_TYPE_ENABLE_SLOT       0x09
#define XHCI_TRB_TYPE_DISABLE_SLOT      0x0A
#define XHCI_TRB_TYPE_ADDRESS_DEVICE    0x0B
#define XHCI_TRB_TYPE_CONFIGURE_EP      0x0C
#define XHCI_TRB_TYPE_EVALUATE_CTX      0x0D
#define XHCI_TRB_TYPE_RESET_EP          0x0E
#define XHCI_TRB_TYPE_STOP_EP           0x0F
#define XHCI_TRB_TYPE_SET_TR_DEQUEUE    0x10
#define XHCI_TRB_TYPE_RESET_DEVICE      0x11
#define XHCI_TRB_TYPE_FORCE_EVENT       0x12
#define XHCI_TRB_TYPE_NEGOTIATE_BW      0x13
#define XHCI_TRB_TYPE_SET_LATENCY_TOL   0x14
#define XHCI_TRB_TYPE_GET_PORT_BW       0x15
#define XHCI_TRB_TYPE_FORCE_HEADER      0x16
#define XHCI_TRB_TYPE_NOOP_CMD          0x17

	/* Events */
#define XHCI_TRB_EVENT_TRANSFER         0x20
#define XHCI_TRB_EVENT_CMD_COMPLETE     0x21
#define XHCI_TRB_EVENT_PORT_STS_CHANGE  0x22
#define XHCI_TRB_EVENT_BW_REQUEST       0x23
#define XHCI_TRB_EVENT_DOORBELL         0x24
#define XHCI_TRB_EVENT_HOST_CTRL        0x25
#define XHCI_TRB_EVENT_DEVICE_NOTIFY    0x26
#define XHCI_TRB_EVENT_MFINDEX_WRAP     0x27

	/* Error codes */
#define XHCI_TRB_ERROR_INVALID          0x00
#define XHCI_TRB_ERROR_SUCCESS          0x01
#define XHCI_TRB_ERROR_DATA_BUF         0x02
#define XHCI_TRB_ERROR_BABBLE           0x03
#define XHCI_TRB_ERROR_XACT             0x04
#define XHCI_TRB_ERROR_TRB              0x05
#define XHCI_TRB_ERROR_STALL            0x06
#define XHCI_TRB_ERROR_RESOURCE         0x07
#define XHCI_TRB_ERROR_BANDWIDTH        0x08
#define XHCI_TRB_ERROR_NO_SLOTS         0x09
#define XHCI_TRB_ERROR_STREAM_TYPE      0x0A
#define XHCI_TRB_ERROR_SLOT_NOT_ON      0x0B
#define XHCI_TRB_ERROR_ENDP_NOT_ON      0x0C
#define XHCI_TRB_ERROR_SHORT_PKT        0x0D
#define XHCI_TRB_ERROR_RING_UNDERRUN    0x0E
#define XHCI_TRB_ERROR_RING_OVERRUN     0x0F
#define XHCI_TRB_ERROR_VF_RING_FULL     0x10
#define XHCI_TRB_ERROR_PARAMETER        0x11
#define XHCI_TRB_ERROR_BW_OVERRUN       0x12
#define XHCI_TRB_ERROR_CONTEXT_STATE    0x13
#define XHCI_TRB_ERROR_NO_PING_RESP     0x14
#define XHCI_TRB_ERROR_EV_RING_FULL     0x15
#define XHCI_TRB_ERROR_INCOMPAT_DEV     0x16
#define XHCI_TRB_ERROR_MISSED_SERVICE   0x17
#define XHCI_TRB_ERROR_CMD_RING_STOP    0x18
#define XHCI_TRB_ERROR_CMD_ABORTED      0x19
#define XHCI_TRB_ERROR_STOPPED          0x1A
#define XHCI_TRB_ERROR_LENGTH           0x1B
#define XHCI_TRB_ERROR_BAD_MELAT        0x1D
#define XHCI_TRB_ERROR_ISOC_OVERRUN     0x1F
#define XHCI_TRB_ERROR_EVENT_LOST       0x20
#define XHCI_TRB_ERROR_UNDEFINED        0x21
#define XHCI_TRB_ERROR_INVALID_SID      0x22
#define XHCI_TRB_ERROR_SEC_BW           0x23
#define XHCI_TRB_ERROR_SPLIT_XACT       0x24
} __packed __aligned(XHCI_TRB_ALIGN);
#define XHCI_TRB_SIZE sizeof(struct xhci_trb)

#define XHCI_SCTX_0_ROUTE_SET(x)                ((x) & 0xFFFFF)
#define XHCI_SCTX_0_ROUTE_GET(x)                ((x) & 0xFFFFF)
#define XHCI_SCTX_0_SPEED_SET(x)                (((x) & 0xF) << 20)
#define XHCI_SCTX_0_SPEED_GET(x)                (((x) >> 20) & 0xF)
#define XHCI_SCTX_0_MTT_SET(x)                  (((x) & 0x1) << 25)
#define XHCI_SCTX_0_MTT_GET(x)                  (((x) >> 25) & 0x1)
#define XHCI_SCTX_0_HUB_SET(x)                  (((x) & 0x1) << 26)
#define XHCI_SCTX_0_HUB_GET(x)                  (((x) >> 26) & 0x1)
#define XHCI_SCTX_0_CTX_NUM_SET(x)              (((x) & 0x1F) << 27)
#define XHCI_SCTX_0_CTX_NUM_GET(x)              (((x) >> 27) & 0x1F)

#define XHCI_SCTX_1_MAX_EL_SET(x)               ((x) & 0xFFFF)
#define XHCI_SCTX_1_MAX_EL_GET(x)               ((x) & 0xFFFF)
#define XHCI_SCTX_1_RH_PORT_SET(x)              (((x) & 0xFF) << 16)
#define XHCI_SCTX_1_RH_PORT_GET(x)              (((x) >> 16) & 0xFF)
#define XHCI_SCTX_1_NUM_PORTS_SET(x)            (((x) & 0xFF) << 24)
#define XHCI_SCTX_1_NUM_PORTS_GET(x)            (((x) >> 24) & 0xFF)

#define XHCI_SCTX_2_TT_HUB_SID_SET(x)           ((x) & 0xFF)
#define XHCI_SCTX_2_TT_HUB_SID_GET(x)           ((x) & 0xFF)
#define XHCI_SCTX_2_TT_PORT_NUM_SET(x)          (((x) & 0xFF) << 8)
#define XHCI_SCTX_2_TT_PORT_NUM_GET(x)          (((x) >> 8) & 0xFF)
#define XHCI_SCTX_2_TT_THINK_TIME_SET(x)        (((x) & 0x3) << 16)
#define XHCI_SCTX_2_TT_THINK_TIME_GET(x)        (((x) >> 16) & 0x3)
#define XHCI_SCTX_2_IRQ_TARGET_SET(x)           (((x) & 0x3FF) << 22)
#define XHCI_SCTX_2_IRQ_TARGET_GET(x)           (((x) >> 22) & 0x3FF)

#define XHCI_SCTX_3_DEV_ADDR_SET(x)             ((x) & 0xFF)
#define XHCI_SCTX_3_DEV_ADDR_GET(x)             ((x) & 0xFF)
#define XHCI_SCTX_3_SLOT_STATE_SET(x)           (((x) & 0x1F) << 27)
#define XHCI_SCTX_3_SLOT_STATE_GET(x)           (((x) >> 27) & 0x1F)
#define XHCI_SLOTSTATE_DISABLED			0 /* disabled or enabled */
#define XHCI_SLOTSTATE_ENABLED			0
#define XHCI_SLOTSTATE_DEFAULT			1
#define XHCI_SLOTSTATE_ADDRESSED		2
#define XHCI_SLOTSTATE_CONFIGURED		3


#define XHCI_EPCTX_0_EPSTATE_SET(x)             ((x) & 0x7)
#define XHCI_EPCTX_0_EPSTATE_GET(x)             ((x) & 0x7)
#define XHCI_EPSTATE_DISABLED			0
#define XHCI_EPSTATE_RUNNING			1
#define XHCI_EPSTATE_HALTED			2
#define XHCI_EPSTATE_STOPPED			3
#define XHCI_EPSTATE_ERROR			4
#define XHCI_EPCTX_0_MULT_SET(x)                (((x) & 0x3) << 8)
#define XHCI_EPCTX_0_MULT_GET(x)                (((x) >> 8) & 0x3)
#define XHCI_EPCTX_0_MAXP_STREAMS_SET(x)        (((x) & 0x1F) << 10)
#define XHCI_EPCTX_0_MAXP_STREAMS_GET(x)        (((x) >> 10) & 0x1F)
#define XHCI_EPCTX_0_LSA_SET(x)                 (((x) & 0x1) << 15)
#define XHCI_EPCTX_0_LSA_GET(x)                 (((x) >> 15) & 0x1)
#define XHCI_EPCTX_0_IVAL_SET(x)                (((x) & 0xFF) << 16)
#define XHCI_EPCTX_0_IVAL_GET(x)                (((x) >> 16) & 0xFF)

#define XHCI_EPCTX_1_CERR_SET(x)                (((x) & 0x3) << 1)
#define XHCI_EPCTX_1_CERR_GET(x)                (((x) >> 1) & 0x3)
#define XHCI_EPCTX_1_EPTYPE_SET(x)              (((x) & 0x7) << 3)
#define XHCI_EPCTX_1_EPTYPE_GET(x)              (((x) >> 3) & 0x7)
#define XHCI_EPCTX_1_HID_SET(x)                 (((x) & 0x1) << 7)
#define XHCI_EPCTX_1_HID_GET(x)                 (((x) >> 7) & 0x1)
#define XHCI_EPCTX_1_MAXB_SET(x)                (((x) & 0xFF) << 8)
#define XHCI_EPCTX_1_MAXB_GET(x)                (((x) >> 8) & 0xFF)
#define XHCI_EPCTX_1_MAXP_SIZE_SET(x)           (((x) & 0xFFFF) << 16)
#define XHCI_EPCTX_1_MAXP_SIZE_GET(x)           (((x) >> 16) & 0xFFFF)

#define XHCI_EPCTX_2_DCS_SET(x)                 ((x) & 0x1)
#define XHCI_EPCTX_2_DCS_GET(x)                 ((x) & 0x1)
#define XHCI_EPCTX_2_TR_DQ_PTR_MASK             0xFFFFFFFFFFFFFFF0U

#define XHCI_EPCTX_4_AVG_TRB_LEN_SET(x)         ((x) & 0xFFFF)
#define XHCI_EPCTX_4_AVG_TRB_LEN_GET(x)         ((x) & 0xFFFF)
#define XHCI_EPCTX_4_MAX_ESIT_PAYLOAD_SET(x)    (((x) & 0xFFFF) << 16)
#define XHCI_EPCTX_4_MAX_ESIT_PAYLOAD_GET(x)    (((x) >> 16) & 0xFFFF)


#define XHCI_INCTX_NON_CTRL_MASK        0xFFFFFFFCU

#define XHCI_INCTX_0_DROP_MASK(n)       (1U << (n))

#define XHCI_INCTX_1_ADD_MASK(n)        (1U << (n))


struct xhci_erste {
	uint64_t       erste_0;		/* 63:6 base */
	uint32_t       erste_2;		/* 15:0 trb count (16 to 4096) */
	uint32_t       erste_3;		/* RsvdZ */
} __packed __aligned(XHCI_ERSTE_ALIGN);
#define XHCI_ERSTE_SIZE sizeof(struct xhci_erste)

#endif	/* _XHCIREG_H_ */
