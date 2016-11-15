/*	$NetBSD: if_urtwnreg.h,v 1.7 2014/07/20 13:25:23 nonaka Exp $	*/
/*	$OpenBSD: if_urtwnreg.h,v 1.3 2010/11/16 18:02:59 damien Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define	URTWN_NOISE_FLOOR	-95

#define R92C_MAX_CHAINS	2

/* Maximum number of output pipes is 3. */
#define R92C_MAX_EPOUT	3

#define R92C_MAX_TX_PWR	0x3f

#define R92C_PUBQ_NPAGES	231
#define R92C_TXPKTBUF_COUNT	256
#define R92C_TX_PAGE_COUNT	248
#define R92C_TX_PAGE_BOUNDARY	(R92C_TX_PAGE_COUNT + 1)
#define R88E_TXPKTBUF_COUNT	177
#define R88E_TX_PAGE_COUNT	169
#define R88E_TX_PAGE_BOUNDARY	(R88E_TX_PAGE_COUNT + 1)

#define R92C_H2C_NBOX	4

/* USB Requests. */
#define R92C_REQ_REGS	0x05

/*
 * MAC registers.
 */
/* System Configuration. */
#define R92C_SYS_ISO_CTRL		0x000
#define R92C_SYS_FUNC_EN		0x002
#define R92C_APS_FSMCO			0x004
#define R92C_SYS_CLKR			0x008
#define R92C_AFE_MISC			0x010
#define R92C_SPS0_CTRL			0x011
#define R92C_SPS_OCP_CFG		0x018
#define R92C_RSV_CTRL			0x01c
#define R92C_RF_CTRL			0x01f
#define R92C_LDOA15_CTRL		0x020
#define R92C_LDOV12D_CTRL		0x021
#define R92C_LDOHCI12_CTRL		0x022
#define R92C_LPLDO_CTRL			0x023
#define R92C_AFE_XTAL_CTRL		0x024
#define R92C_AFE_PLL_CTRL		0x028
#define R92C_EFUSE_CTRL			0x030
#define R92C_EFUSE_TEST			0x034
#define R92C_PWR_DATA			0x038
#define R92C_CAL_TIMER			0x03c
#define R92C_ACLK_MON			0x03e
#define R92C_GPIO_MUXCFG		0x040
#define R92C_GPIO_IO_SEL		0x042
#define R92C_MAC_PINMUX_CFG		0x043
#define R92C_GPIO_PIN_CTRL		0x044
#define R92C_GPIO_INTM			0x048
#define R92C_LEDCFG0			0x04c
#define R92C_LEDCFG1			0x04d
#define R92C_LEDCFG2			0x04e
#define R92C_LEDCFG3			0x04f
#define R92C_FSIMR			0x050
#define R92C_FSISR			0x054
#define R92C_HSIMR			0x058
#define R92C_HSISR			0x05c
#define R92C_MCUFWDL			0x080
#define R92C_HMEBOX_EXT(idx)		(0x088 + (idx) * 2)
#define R88E_HIMR			0x0b0
#define R88E_HISR			0x0b4
#define R88E_HIMRE			0x0b8
#define R88E_HISRE			0x0bc
#define R92C_EFUSE_ACCESS		0x0cf
#define R92C_BIST_SCAN			0x0d0
#define R92C_BIST_RPT			0x0d4
#define R92C_BIST_ROM_RPT		0x0d8
#define R92C_USB_SIE_INTF		0x0e0
#define R92C_PCIE_MIO_INTF		0x0e4
#define R92C_PCIE_MIO_INTD		0x0e8
#define R92C_HPON_FSM			0x0ec
#define R92C_SYS_CFG			0x0f0
/* MAC General Configuration. */
#define R92C_CR				0x100
#define R92C_MSR			0x102
#define R92C_PBP			0x104
#define R92C_TRXDMA_CTRL		0x10c
#define R92C_TRXFF_BNDY			0x114
#define R92C_TRXFF_STATUS		0x118
#define R92C_RXFF_PTR			0x11c
#define R92C_HIMR			0x120
#define R92C_HISR			0x124
#define R92C_HIMRE			0x128
#define R92C_HISRE			0x12c
#define R92C_CPWM			0x12f
#define R92C_FWIMR			0x130
#define R92C_FWISR			0x134
#define R92C_PKTBUF_DBG_CTRL		0x140
#define R92C_PKTBUF_DBG_DATA_L		0x144
#define R92C_PKTBUF_DBG_DATA_H		0x148
#define R92C_TC0_CTRL(i)		(0x150 + (i) * 4)
#define R92C_TCUNIT_BASE		0x164
#define R92C_MBIST_START		0x174
#define R92C_MBIST_DONE			0x178
#define R92C_MBIST_FAIL			0x17c
#define R92C_C2HEVT_MSG_NORMAL		0x1a0
#define R92C_C2HEVT_MSG_TEST		0x1b8
#define R92C_C2HEVT_CLEAR		0x1bf
#define R92C_MCUTST_1			0x1c0
#define R92C_FMETHR			0x1c8
#define R92C_HMETFR			0x1cc
#define R92C_HMEBOX(idx)		(0x1d0 + (idx) * 4)
#define R92C_LLT_INIT			0x1e0
#define R92C_BB_ACCESS_CTRL		0x1e8
#define R92C_BB_ACCESS_DATA		0x1ec
#define R88E_HMEBOX_EXT(idx)		(0x1f0 + (idx) * 4)
/* Tx DMA Configuration. */
#define R92C_RQPN			0x200
#define R92C_FIFOPAGE			0x204
#define R92C_TDECTRL			0x208
#define R92C_TXDMA_OFFSET_CHK		0x20c
#define R92C_TXDMA_STATUS		0x210
#define R92C_RQPN_NPQ			0x214
/* Rx DMA Configuration. */
#define R92C_RXDMA_AGG_PG_TH		0x280
#define R92C_RXPKT_NUM			0x284
#define R92C_RXDMA_STATUS		0x288
/* Protocol Configuration. */
#define R92C_FWHW_TXQ_CTRL		0x420
#define R92C_HWSEQ_CTRL			0x423
#define R92C_TXPKTBUF_BCNQ_BDNY		0x424
#define R92C_TXPKTBUF_MGQ_BDNY		0x425
#define R92C_SPEC_SIFS			0x428
#define R92C_RL				0x42a
#define R92C_DARFRC			0x430
#define R92C_RARFRC			0x438
#define R92C_RRSR			0x440
#define R92C_ARFR(i)			(0x444 + (i) * 4)
#define R92C_AGGLEN_LMT			0x458
#define R92C_AMPDU_MIN_SPACE		0x45c
#define R92C_TXPKTBUF_WMAC_LBK_BF_HD	0x45d
#define R92C_FAST_EDCA_CTRL		0x460
#define R92C_RD_RESP_PKT_TH		0x463
#define R92C_INIRTS_RATE_SEL		0x480
#define R92C_INIDATA_RATE_SEL(macid)	(0x484 + (macid))
#define R92C_MAX_AGGR_NUM		0x4ca
#define R92C_PROT_MODE_CTRL		0x4c8
#define R92C_BAR_MODE_CTRL		0x4cc
/* EDCA Configuration. */
#define R92C_EDCA_VO_PARAM		0x500
#define R92C_EDCA_VI_PARAM		0x504
#define R92C_EDCA_BE_PARAM		0x508
#define R92C_EDCA_BK_PARAM		0x50c
#define R92C_BCNTCFG			0x510
#define R92C_PIFS			0x512
#define R92C_RDG_PIFS			0x513
#define R92C_SIFS_CCK			0x514
#define R92C_SIFS_OFDM			0x516
#define R92C_AGGR_BREAK_TIME		0x51a
#define R92C_SLOT			0x51b
#define R92C_TX_PTCL_CTRL		0x520
#define R92C_TXPAUSE			0x522
#define R92C_DIS_TXREQ_CLR		0x523
#define R92C_RD_CTRL			0x524
#define R92C_TBTT_PROHIBIT		0x540
#define R92C_RD_NAV_NXT			0x544
#define R92C_NAV_PROT_LEN		0x546
#define R92C_BCN_CTRL			0x550
#define R92C_USTIME_TSF			0x551
#define R92C_MBID_NUM			0x552
#define R92C_DUAL_TSF_RST		0x553
#define R92C_BCN_INTERVAL		0x554
#define R92C_DRVERLYINT			0x558
#define R92C_BCNDMATIM			0x559
#define R92C_ATIMWND			0x55a
#define R92C_BCN_MAX_ERR		0x55d
#define R92C_RXTSF_OFFSET_CCK		0x55e
#define R92C_RXTSF_OFFSET_OFDM		0x55f
#define R92C_TSFTR			0x560
#define R92C_INIT_TSFTR			0x564
#define R92C_PSTIMER			0x580
#define R92C_TIMER0			0x584
#define R92C_TIMER1			0x588
#define R92C_ACMHWCTRL			0x5c0
#define R92C_ACMRSTCTRL			0x5c1
#define R92C_ACMAVG			0x5c2
#define R92C_VO_ADMTIME			0x5c4
#define R92C_VI_ADMTIME			0x5c6
#define R92C_BE_ADMTIME			0x5c8
#define R92C_EDCA_RANDOM_GEN		0x5cc
#define R92C_SCH_TXCMD			0x5d0
/* WMAC Configuration. */
#define R92C_APSD_CTRL			0x600
#define R92C_BWOPMODE			0x603
#define R92C_TCR			0x604
#define R92C_RCR			0x608
#define R92C_RX_PKT_LIMIT		0x60c
#define R92C_RX_DLK_TIME		0x60d
#define R92C_RX_DRVINFO_SZ		0x60f
#define R92C_MACID			0x610
#define R92C_BSSID			0x618
#define R92C_MAR			0x620
#define R92C_MBIDCAMCFG			0x628
#define R92C_USTIME_EDCA		0x638
#define R92C_MAC_SPEC_SIFS		0x63a
#define R92C_R2T_SIFS			0x63c
#define R92C_T2T_SIFS			0x63e
#define R92C_ACKTO			0x640
#define R92C_CTS2TO			0x641
#define R92C_EIFS			0x642
#define R92C_NAV_CTRL			0x650
#define R92C_BACAMCMD			0x654
#define R92C_BACAMCONTENT		0x658
#define R92C_LBDLY			0x660
#define R92C_FWDLY			0x661
#define R92C_RXERR_RPT			0x664
#define R92C_WMAC_TRXPTCL_CTL		0x668
#define R92C_CAMCMD			0x670
#define R92C_CAMWRITE			0x674
#define R92C_CAMREAD			0x678
#define R92C_CAMDBG			0x67c
#define R92C_SECCFG			0x680
#define R92C_WOW_CTRL			0x690
#define R92C_PSSTATUS			0x691
#define R92C_PS_RX_INFO			0x692
#define R92C_LPNAV_CTRL			0x694
#define R92C_WKFMCAM_CMD		0x698
#define R92C_WKFMCAM_RWD		0x69c
#define R92C_RXFLTMAP0			0x6a0
#define R92C_RXFLTMAP1			0x6a2
#define R92C_RXFLTMAP2			0x6a4
#define R92C_BCN_PSR_RPT		0x6a8
#define R92C_CALB32K_CTRL		0x6ac
#define R92C_PKT_MON_CTRL		0x6b4
#define R92C_BT_COEX_TABLE		0x6c0
#define R92C_WMAC_RESP_TXINFO		0x6d8

/* Bits for R92C_SYS_ISO_CTRL. */
#define R92C_SYS_ISO_CTRL_MD2PP		0x0001
#define R92C_SYS_ISO_CTRL_UA2USB	0x0002
#define R92C_SYS_ISO_CTRL_UD2CORE	0x0004
#define R92C_SYS_ISO_CTRL_PA2PCIE	0x0008
#define R92C_SYS_ISO_CTRL_PD2CORE	0x0010
#define R92C_SYS_ISO_CTRL_IP2MAC	0x0020
#define R92C_SYS_ISO_CTRL_DIOP		0x0040
#define R92C_SYS_ISO_CTRL_DIOE		0x0080
#define R92C_SYS_ISO_CTRL_EB2CORE	0x0100
#define R92C_SYS_ISO_CTRL_DIOR		0x0200
#define R92C_SYS_ISO_CTRL_PWC_EV25V	0x4000
#define R92C_SYS_ISO_CTRL_PWC_EV12V	0x8000

/* Bits for R92C_SYS_FUNC_EN. */
#define R92C_SYS_FUNC_EN_BBRSTB		0x0001
#define R92C_SYS_FUNC_EN_BB_GLB_RST	0x0002
#define R92C_SYS_FUNC_EN_USBA		0x0004
#define R92C_SYS_FUNC_EN_UPLL		0x0008
#define R92C_SYS_FUNC_EN_USBD		0x0010
#define R92C_SYS_FUNC_EN_DIO_PCIE	0x0020
#define R92C_SYS_FUNC_EN_PCIEA		0x0040
#define R92C_SYS_FUNC_EN_PPLL		0x0080
#define R92C_SYS_FUNC_EN_PCIED		0x0100
#define R92C_SYS_FUNC_EN_DIOE		0x0200
#define R92C_SYS_FUNC_EN_CPUEN		0x0400
#define R92C_SYS_FUNC_EN_DCORE		0x0800
#define R92C_SYS_FUNC_EN_ELDR		0x1000
#define R92C_SYS_FUNC_EN_DIO_RF		0x2000
#define R92C_SYS_FUNC_EN_HWPDN		0x4000
#define R92C_SYS_FUNC_EN_MREGEN		0x8000

/* Bits for R92C_APS_FSMCO. */
#define R92C_APS_FSMCO_PFM_LDALL	0x00000001
#define R92C_APS_FSMCO_PFM_ALDN		0x00000002
#define R92C_APS_FSMCO_PFM_LDKP		0x00000004
#define R92C_APS_FSMCO_PFM_WOWL		0x00000008
#define R92C_APS_FSMCO_PDN_EN		0x00000010
#define R92C_APS_FSMCO_PDN_PL		0x00000020
#define R92C_APS_FSMCO_APFM_ONMAC	0x00000100
#define R92C_APS_FSMCO_APFM_OFF		0x00000200
#define R92C_APS_FSMCO_APFM_RSM		0x00000400
#define R92C_APS_FSMCO_AFSM_HSUS	0x00000800
#define R92C_APS_FSMCO_AFSM_PCIE	0x00001000
#define R92C_APS_FSMCO_APDM_MAC		0x00002000
#define R92C_APS_FSMCO_APDM_HOST	0x00004000
#define R92C_APS_FSMCO_APDM_HPDN	0x00008000
#define R92C_APS_FSMCO_RDY_MACON	0x00010000
#define R92C_APS_FSMCO_SUS_HOST		0x00020000
#define R92C_APS_FSMCO_ROP_ALD		0x00100000
#define R92C_APS_FSMCO_ROP_PWR		0x00200000
#define R92C_APS_FSMCO_ROP_SPS		0x00400000
#define R92C_APS_FSMCO_SOP_MRST		0x02000000
#define R92C_APS_FSMCO_SOP_FUSE		0x04000000
#define R92C_APS_FSMCO_SOP_ABG		0x08000000
#define R92C_APS_FSMCO_SOP_AMB		0x10000000
#define R92C_APS_FSMCO_SOP_RCK		0x20000000
#define R92C_APS_FSMCO_SOP_A8M		0x40000000
#define R92C_APS_FSMCO_XOP_BTCK		0x80000000

/* Bits for R92C_SYS_CLKR. */
#define R92C_SYS_CLKR_ANAD16V_EN	0x00000001
#define R92C_SYS_CLKR_ANA8M		0x00000002
#define R92C_SYS_CLKR_MACSLP		0x00000010
#define R92C_SYS_CLKR_LOADER_EN		0x00000020
#define R92C_SYS_CLKR_80M_SSC_DIS	0x00000080
#define R92C_SYS_CLKR_80M_SSC_EN_HO	0x00000100
#define R92C_SYS_CLKR_PHY_SSC_RSTB	0x00000200
#define R92C_SYS_CLKR_SEC_EN		0x00000400
#define R92C_SYS_CLKR_MAC_EN		0x00000800
#define R92C_SYS_CLKR_SYS_EN		0x00001000
#define R92C_SYS_CLKR_RING_EN		0x00002000

/* Bits for R92C_RF_CTRL. */
#define R92C_RF_CTRL_EN		0x01
#define R92C_RF_CTRL_RSTB	0x02
#define R92C_RF_CTRL_SDMRSTB	0x04

/* Bits for R92C_LDOV12D_CTRL. */
#define R92C_LDOV12D_CTRL_LDV12_EN	0x01

/* Bits for R92C_AFE_XTAL_CTRL. */
#define R92C_AFE_XTAL_CTRL_ADDR_M	0x007ff800
#define R92C_AFE_XTAL_CTRL_ADDR_S	11

/* Bits for R92C_EFUSE_CTRL. */
#define R92C_EFUSE_CTRL_DATA_M	0x000000ff
#define R92C_EFUSE_CTRL_DATA_S	0
#define R92C_EFUSE_CTRL_ADDR_M	0x0003ff00
#define R92C_EFUSE_CTRL_ADDR_S	8
#define R92C_EFUSE_CTRL_VALID	0x80000000

/* Bits for R92C_GPIO_MUXCFG. */
#define R92C_GPIO_MUXCFG_ENBT	0x0020

/* Bits for R92C_LEDCFG0. */
#define R92C_LEDCFG0_DIS	0x08

/* Bits for R92C_MCUFWDL. */
#define R92C_MCUFWDL_EN			0x00000001
#define R92C_MCUFWDL_RDY		0x00000002
#define R92C_MCUFWDL_CHKSUM_RPT		0x00000004
#define R92C_MCUFWDL_MACINI_RDY		0x00000008
#define R92C_MCUFWDL_BBINI_RDY		0x00000010
#define R92C_MCUFWDL_RFINI_RDY		0x00000020
#define R92C_MCUFWDL_WINTINI_RDY	0x00000040
#define R92C_MCUFWDL_RAM_DL_SEL		0x00000080
#define R92C_MCUFWDL_PAGE_M		0x00070000
#define R92C_MCUFWDL_PAGE_S		16
#define R92C_MCUFWDL_CPRST		0x00800000

/* Bits for R88E_HIMR. */
#define R88E_HIMR_CPWM			0x00000100
#define R88E_HIMR_CPWM2			0x00000200
#define R88E_HIMR_TBDER			0x04000000
#define R88E_HIMR_PSTIMEOUT		0x20000000

/* Bits for R88E_HIMRE.*/
#define R88E_HIMRE_RXFOVW		0x00000100
#define R88E_HIMRE_TXFOVW		0x00000200
#define R88E_HIMRE_RXERR		0x00000400
#define R88E_HIMRE_TXERR		0x00000800

/* Bits for R92C_EFUSE_ACCESS. */
#define R92C_EFUSE_ACCESS_OFF		0x00
#define R92C_EFUSE_ACCESS_ON		0x69

/* Bits for R92C_HPON_FSM. */
#define R92C_HPON_FSM_CHIP_BONDING_ID_S		22
#define R92C_HPON_FSM_CHIP_BONDING_ID_M		0x00c00000
#define R92C_HPON_FSM_CHIP_BONDING_ID_92C_1T2R	1

/* Bits for R92C_SYS_CFG. */
#define R92C_SYS_CFG_XCLK_VLD		0x00000001
#define R92C_SYS_CFG_ACLK_VLD		0x00000002
#define R92C_SYS_CFG_UCLK_VLD		0x00000004
#define R92C_SYS_CFG_PCLK_VLD		0x00000008
#define R92C_SYS_CFG_PCIRSTB		0x00000010
#define R92C_SYS_CFG_V15_VLD		0x00000020
#define R92C_SYS_CFG_TRP_B15V_EN	0x00000080
#define R92C_SYS_CFG_SIC_IDLE		0x00000100
#define R92C_SYS_CFG_BD_MAC2		0x00000200
#define R92C_SYS_CFG_BD_MAC1		0x00000400
#define R92C_SYS_CFG_IC_MACPHY_MODE	0x00000800
#define R92C_SYS_CFG_CHIP_VER_RTL_M	0x0000f000
#define R92C_SYS_CFG_CHIP_VER_RTL_S	12
#define R92C_SYS_CFG_BT_FUNC		0x00010000
#define R92C_SYS_CFG_VENDOR_UMC		0x00080000
#define R92C_SYS_CFG_PAD_HWPD_IDN	0x00400000
#define R92C_SYS_CFG_TRP_VAUX_EN	0x00800000
#define R92C_SYS_CFG_TRP_BT_EN		0x01000000
#define R92C_SYS_CFG_BD_PKG_SEL		0x02000000
#define R92C_SYS_CFG_BD_HCI_SEL		0x04000000
#define R92C_SYS_CFG_TYPE_92C		0x08000000

/* Bits for R92C_CR. */
#define R92C_CR_HCI_TXDMA_EN	0x00000001
#define R92C_CR_HCI_RXDMA_EN	0x00000002
#define R92C_CR_TXDMA_EN	0x00000004
#define R92C_CR_RXDMA_EN	0x00000008
#define R92C_CR_PROTOCOL_EN	0x00000010
#define R92C_CR_SCHEDULE_EN	0x00000020
#define R92C_CR_MACTXEN		0x00000040
#define R92C_CR_MACRXEN		0x00000080
#define R92C_CR_ENSEC		0x00000200
#define R92C_CR_CALTMR_EN	0x00000400
#define R92C_CR_NETTYPE_S	16
#define R92C_CR_NETTYPE_M	0x00030000
#define R92C_CR_NETTYPE_NOLINK	0
#define R92C_CR_NETTYPE_ADHOC	1
#define R92C_CR_NETTYPE_INFRA	2
#define R92C_CR_NETTYPE_AP	3

/* Bits for R92C_MSR. */
#define R92C_MSR_NOLINK		0x00
#define R92C_MSR_ADHOC		0x01
#define R92C_MSR_INFRA		0x02
#define R92C_MSR_AP		0x03
#define R92C_MSR_MASK		(~R92C_MSR_AP)

/* Bits for R92C_PBP. */
#define R92C_PBP_PSRX_M		0x0f
#define R92C_PBP_PSRX_S		0
#define R92C_PBP_PSTX_M		0xf0
#define R92C_PBP_PSTX_S		4
#define R92C_PBP_64		0
#define R92C_PBP_128		1
#define R92C_PBP_256		2
#define R92C_PBP_512		3
#define R92C_PBP_1024		4

/* Bits for R92C_TRXDMA_CTRL. */
#define R92C_TRXDMA_CTRL_RXDMA_AGG_EN		0x0004
#define R92C_TRXDMA_CTRL_TXDMA_VOQ_MAP_M	0x0030
#define R92C_TRXDMA_CTRL_TXDMA_VOQ_MAP_S	4
#define R92C_TRXDMA_CTRL_TXDMA_VIQ_MAP_M	0x00c0
#define R92C_TRXDMA_CTRL_TXDMA_VIQ_MAP_S	6
#define R92C_TRXDMA_CTRL_TXDMA_BEQ_MAP_M	0x0300
#define R92C_TRXDMA_CTRL_TXDMA_BEQ_MAP_S	8
#define R92C_TRXDMA_CTRL_TXDMA_BKQ_MAP_M	0x0c00
#define R92C_TRXDMA_CTRL_TXDMA_BKQ_MAP_S	10
#define R92C_TRXDMA_CTRL_TXDMA_MGQ_MAP_M	0x3000
#define R92C_TRXDMA_CTRL_TXDMA_MGQ_MAP_S	12
#define R92C_TRXDMA_CTRL_TXDMA_HIQ_MAP_M	0xc000
#define R92C_TRXDMA_CTRL_TXDMA_HIQ_MAP_S	14
#define R92C_TRXDMA_CTRL_QUEUE_LOW		1
#define R92C_TRXDMA_CTRL_QUEUE_NORMAL		2
#define R92C_TRXDMA_CTRL_QUEUE_HIGH		3
#define R92C_TRXDMA_CTRL_QMAP_M			0xfff0
/* Shortcuts. */
#define R92C_TRXDMA_CTRL_QMAP_3EP		0xf5b0
#define R92C_TRXDMA_CTRL_QMAP_HQ_LQ		0xf5f0
#define R92C_TRXDMA_CTRL_QMAP_HQ_NQ		0xfaf0
#define R92C_TRXDMA_CTRL_QMAP_LQ		0x5550
#define R92C_TRXDMA_CTRL_QMAP_NQ		0xaaa0
#define R92C_TRXDMA_CTRL_QMAP_HQ		0xfff0

/* Bits for R92C_LLT_INIT. */
#define R92C_LLT_INIT_DATA_M		0x000000ff
#define R92C_LLT_INIT_DATA_S		0
#define R92C_LLT_INIT_ADDR_M		0x0000ff00
#define R92C_LLT_INIT_ADDR_S		8
#define R92C_LLT_INIT_OP_M		0xc0000000
#define R92C_LLT_INIT_OP_S		30
#define R92C_LLT_INIT_OP_NO_ACTIVE	0
#define R92C_LLT_INIT_OP_WRITE		1

/* Bits for R92C_RQPN. */
#define R92C_RQPN_HPQ_M		0x000000ff
#define R92C_RQPN_HPQ_S		0
#define R92C_RQPN_LPQ_M		0x0000ff00
#define R92C_RQPN_LPQ_S		8
#define R92C_RQPN_PUBQ_M	0x00ff0000
#define R92C_RQPN_PUBQ_S	16
#define R92C_RQPN_LD		0x80000000

/* Bits for R92C_TDECTRL. */
#define R92C_TDECTRL_BLK_DESC_NUM_M	0x0000000f
#define R92C_TDECTRL_BLK_DESC_NUM_S	4

/* Bits for R92C_FWHW_TXQ_CTRL. */
#define R92C_FWHW_TXQ_CTRL_AMPDU_RTY_NEW	0x80

/* Bits for R92C_SPEC_SIFS. */
#define R92C_SPEC_SIFS_CCK_M	0x00ff
#define R92C_SPEC_SIFS_CCK_S	0
#define R92C_SPEC_SIFS_OFDM_M	0xff00
#define R92C_SPEC_SIFS_OFDM_S	8

/* Bits for R92C_RL. */
#define R92C_RL_LRL_M		0x003f
#define R92C_RL_LRL_S		0
#define R92C_RL_SRL_M		0x3f00
#define R92C_RL_SRL_S		8

/* Bits for R92C_RRSR. */
#define R92C_RRSR_RATE_BITMAP_M		0x000fffff
#define R92C_RRSR_RATE_BITMAP_S		0
#define R92C_RRSR_RATE_CCK_ONLY_1M	0xffff1
#define R92C_RRSR_RSC_LOWSUBCHNL	0x00200000
#define R92C_RRSR_RSC_UPSUBCHNL		0x00400000
#define R92C_RRSR_SHORT			0x00800000

/* Bits for R92C_EDCA_XX_PARAM. */
#define R92C_EDCA_PARAM_AIFS_M		0x000000ff
#define R92C_EDCA_PARAM_AIFS_S		0
#define R92C_EDCA_PARAM_ECWMIN_M	0x00000f00
#define R92C_EDCA_PARAM_ECWMIN_S	8
#define R92C_EDCA_PARAM_ECWMAX_M	0x0000f000
#define R92C_EDCA_PARAM_ECWMAX_S	12
#define R92C_EDCA_PARAM_TXOP_M		0xffff0000
#define R92C_EDCA_PARAM_TXOP_S		16

/* Bits for R92C_BCN_CTRL. */
#define R92C_BCN_CTRL_EN_MBSSID		0x02
#define R92C_BCN_CTRL_TXBCN_RPT		0x04
#define R92C_BCN_CTRL_EN_BCN		0x08
#define R92C_BCN_CTRL_DIS_TSF_UDT0	0x10

/* Bits for R92C_DRVERLYINT */
#define R92C_DRIVER_EARLY_INT_TIME	0x05

/* Bits for R92C_BCNDMATIM */
#define R92C_DMA_ATIME_INT_TIME		0x02
 
/* Bits for R92C_APSD_CTRL. */
#define R92C_APSD_CTRL_OFF		0x40
#define R92C_APSD_CTRL_OFF_STATUS	0x80

/* Bits for R92C_BWOPMODE. */
#define R92C_BWOPMODE_11J	0x01
#define R92C_BWOPMODE_5G	0x02
#define R92C_BWOPMODE_20MHZ	0x04

/* Bits for R92C_RCR. */
#define R92C_RCR_AAP		0x00000001	// Accept all unicast packet
#define R92C_RCR_APM		0x00000002	// Accept physical match packet
#define R92C_RCR_AM		0x00000004	// Accept multicast packet
#define R92C_RCR_AB		0x00000008	// Accept broadcast packet
#define R92C_RCR_ADD3		0x00000010	// Accept address 3 match packet
#define R92C_RCR_APWRMGT	0x00000020	// Accept power management packet
#define R92C_RCR_CBSSID_DATA	0x00000040	// Accept BSSID match packet (Data)
#define R92C_RCR_CBSSID_BCN	0x00000080	// Accept BSSID match packet (Rx beacon, probe rsp)
#define R92C_RCR_ACRC32		0x00000100	// Accept CRC32 error packet
#define R92C_RCR_AICV		0x00000200	// Accept ICV error packet
#define R92C_RCR_ADF		0x00000800	// Accept data type frame
#define R92C_RCR_ACF		0x00001000	// Accept control type frame
#define R92C_RCR_AMF		0x00002000	// Accept management type frame
#define R92C_RCR_HTC_LOC_CTRL	0x00004000	// MFC<--HTC=1 MFC-->HTC=0
#define R92C_RCR_MFBEN		0x00400000
#define R92C_RCR_LSIGEN		0x00800000
#define R92C_RCR_ENMBID		0x01000000	// Enable Multiple BssId.
#define R92C_RCR_APP_BA_SSN	0x08000000	// Accept BA SSN
#define R92C_RCR_APP_PHYSTS	0x10000000
#define R92C_RCR_APP_ICV	0x20000000
#define R92C_RCR_APP_MIC	0x40000000
#define R92C_RCR_APPFCS		0x80000000	// WMAC append FCS after payload

/* Bits for R92C_CAMCMD. */
#define R92C_CAMCMD_ADDR_M	0x0000ffff
#define R92C_CAMCMD_ADDR_S	0
#define R92C_CAMCMD_WRITE	0x00010000
#define R92C_CAMCMD_CLR		0x40000000
#define R92C_CAMCMD_POLLING	0x80000000


/*
 * Baseband registers.
 */
#define R92C_FPGA0_RFMOD		0x800
#define R92C_FPGA0_TXINFO		0x804
#define R92C_HSSI_PARAM1(chain)		(0x820 + (chain) * 8)
#define R92C_HSSI_PARAM2(chain)		(0x824 + (chain) * 8)
#define R92C_TXAGC_RATE18_06(i)		(((i) == 0) ? 0xe00 : 0x830)
#define R92C_TXAGC_RATE54_24(i)		(((i) == 0) ? 0xe04 : 0x834)
#define R92C_TXAGC_A_CCK1_MCS32		0xe08
#define	R92C_FPGA0_XA_HSSIPARAM1	0x820
#define R92C_TXAGC_B_CCK1_55_MCS32	0x838
#define R92C_TXAGC_B_CCK11_A_CCK2_11	0x86c
#define R92C_TXAGC_MCS03_MCS00(i)	(((i) == 0) ? 0xe10 : 0x83c)
#define R92C_TXAGC_MCS07_MCS04(i)	(((i) == 0) ? 0xe14 : 0x848)
#define R92C_TXAGC_MCS11_MCS08(i)	(((i) == 0) ? 0xe18 : 0x84c)
#define R92C_TXAGC_MCS15_MCS12(i)	(((i) == 0) ? 0xe1c : 0x868)
#define R92C_LSSI_PARAM(chain)		(0x840 + (chain) * 4)
#define R92C_FPGA0_RFIFACEOE(chain)	(0x860 + (chain) * 4)
#define R92C_FPGA0_RFIFACESW(idx)	(0x870 + (idx) * 4)
#define R92C_FPGA0_RFPARAM(idx)		(0x878 + (idx) * 4)
#define R92C_FPGA0_ANAPARAM2		0x884
#define R92C_LSSI_READBACK(chain)	(0x8a0 + (chain) * 4)
#define R92C_HSPI_READBACK(chain)	(0x8b8 + (chain) * 4)
#define R92C_FPGA1_RFMOD		0x900
#define R92C_FPGA1_TXINFO		0x90c
#define R92C_CCK0_SYSTEM		0xa00
#define R92C_CCK0_AFESETTING		0xa04
#define R92C_OFDM0_TRXPATHENA		0xc04
#define R92C_OFDM0_TRMUXPAR		0xc08
#define R92C_OFDM0_XARXIQIMBALANCE	0xc14
#define R92C_OFDM0_ECCATHRESHOLD	0xc4c
#define R92C_OFDM0_AGCCORE1(chain)	(0xc50 + (chain) * 8)
#define R92C_OFDM0_AGCPARAM1		0xc70
#define R92C_OFDM0_AGCRSSITABLE		0xc78
#define R92C_OFDM0_HTSTFAGC		0xc7c
#define R92C_OFDM0_XATXIQIMBALANCE	0xc80
#define R92C_OFDM0_XBTXIQIMBALANCE	0xc88
#define R92C_OFDM0_XCTXIQIMBALANCE	0xc90
#define R92C_OFDM0_XCTXAFE		0xc94
#define R92C_OFDM0_XDTXAFE		0xc9c
#define R92C_OFDM0_RXIQEXTANTA		0xca0
#define R92C_OFDM1_LSTF			0xd00

/* Bits for R92C_FPGA[01]_RFMOD. */
#define R92C_RFMOD_40MHZ	0x00000001
#define R92C_RFMOD_JAPAN	0x00000002
#define R92C_RFMOD_CCK_TXSC	0x00000030
#define R92C_RFMOD_CCK_EN	0x01000000
#define R92C_RFMOD_OFDM_EN	0x02000000

/* Bits for R92C_HSSI_PARAM1(i). */
#define R92C_HSSI_PARAM1_PI	0x00000100

/* Bits for R92C_HSSI_PARAM2(i). */
#define R92C_HSSI_PARAM2_CCK_HIPWR	0x00000200
#define R92C_HSSI_PARAM2_ADDR_LENGTH	0x00000400
#define R92C_HSSI_PARAM2_DATA_LENGTH	0x00000800
#define R92C_HSSI_PARAM2_READ_ADDR_M	0x7f800000
#define R92C_HSSI_PARAM2_READ_ADDR_S	23
#define R92C_HSSI_PARAM2_READ_EDGE	0x80000000

/* Bits for R92C_TXAGC_A_CCK1_MCS32. */
#define R92C_TXAGC_A_CCK1_M	0x0000ff00
#define R92C_TXAGC_A_CCK1_S	8

/* Bits for R92C_TXAGC_B_CCK11_A_CCK2_11. */
#define R92C_TXAGC_B_CCK11_M	0x000000ff
#define R92C_TXAGC_B_CCK11_S	0
#define R92C_TXAGC_A_CCK2_M	0x0000ff00
#define R92C_TXAGC_A_CCK2_S	8
#define R92C_TXAGC_A_CCK55_M	0x00ff0000
#define R92C_TXAGC_A_CCK55_S	16
#define R92C_TXAGC_A_CCK11_M	0xff000000
#define R92C_TXAGC_A_CCK11_S	24

/* Bits for R92C_TXAGC_B_CCK1_55_MCS32. */
#define R92C_TXAGC_B_CCK1_M	0x0000ff00
#define R92C_TXAGC_B_CCK1_S	8
#define R92C_TXAGC_B_CCK2_M	0x00ff0000
#define R92C_TXAGC_B_CCK2_S	16
#define R92C_TXAGC_B_CCK55_M	0xff000000
#define R92C_TXAGC_B_CCK55_S	24

/* Bits for R92C_TXAGC_RATE18_06(x). */
#define R92C_TXAGC_RATE06_M	0x000000ff
#define R92C_TXAGC_RATE06_S	0
#define R92C_TXAGC_RATE09_M	0x0000ff00
#define R92C_TXAGC_RATE09_S	8
#define R92C_TXAGC_RATE12_M	0x00ff0000
#define R92C_TXAGC_RATE12_S	16
#define R92C_TXAGC_RATE18_M	0xff000000
#define R92C_TXAGC_RATE18_S	24

/* Bits for R92C_TXAGC_RATE54_24(x). */
#define R92C_TXAGC_RATE24_M	0x000000ff
#define R92C_TXAGC_RATE24_S	0
#define R92C_TXAGC_RATE36_M	0x0000ff00
#define R92C_TXAGC_RATE36_S	8
#define R92C_TXAGC_RATE48_M	0x00ff0000
#define R92C_TXAGC_RATE48_S	16
#define R92C_TXAGC_RATE54_M	0xff000000
#define R92C_TXAGC_RATE54_S	24

/* Bits for R92C_TXAGC_MCS03_MCS00(x). */
#define R92C_TXAGC_MCS00_M	0x000000ff
#define R92C_TXAGC_MCS00_S	0
#define R92C_TXAGC_MCS01_M	0x0000ff00
#define R92C_TXAGC_MCS01_S	8
#define R92C_TXAGC_MCS02_M	0x00ff0000
#define R92C_TXAGC_MCS02_S	16
#define R92C_TXAGC_MCS03_M	0xff000000
#define R92C_TXAGC_MCS03_S	24

/* Bits for R92C_TXAGC_MCS07_MCS04(x). */
#define R92C_TXAGC_MCS04_M	0x000000ff
#define R92C_TXAGC_MCS04_S	0
#define R92C_TXAGC_MCS05_M	0x0000ff00
#define R92C_TXAGC_MCS05_S	8
#define R92C_TXAGC_MCS06_M	0x00ff0000
#define R92C_TXAGC_MCS06_S	16
#define R92C_TXAGC_MCS07_M	0xff000000
#define R92C_TXAGC_MCS07_S	24

/* Bits for R92C_TXAGC_MCS11_MCS08(x). */
#define R92C_TXAGC_MCS08_M	0x000000ff
#define R92C_TXAGC_MCS08_S	0
#define R92C_TXAGC_MCS09_M	0x0000ff00
#define R92C_TXAGC_MCS09_S	8
#define R92C_TXAGC_MCS10_M	0x00ff0000
#define R92C_TXAGC_MCS10_S	16
#define R92C_TXAGC_MCS11_M	0xff000000
#define R92C_TXAGC_MCS11_S	24

/* Bits for R92C_TXAGC_MCS15_MCS12(x). */
#define R92C_TXAGC_MCS12_M	0x000000ff
#define R92C_TXAGC_MCS12_S	0
#define R92C_TXAGC_MCS13_M	0x0000ff00
#define R92C_TXAGC_MCS13_S	8
#define R92C_TXAGC_MCS14_M	0x00ff0000
#define R92C_TXAGC_MCS14_S	16
#define R92C_TXAGC_MCS15_M	0xff000000
#define R92C_TXAGC_MCS15_S	24

/* Bits for R92C_LSSI_PARAM(i). */
#define R92C_LSSI_PARAM_DATA_M	0x000fffff
#define R92C_LSSI_PARAM_DATA_S	0
#define R92C_LSSI_PARAM_ADDR_M	0x03f00000
#define R92C_LSSI_PARAM_ADDR_S	20
#define R88E_LSSI_PARAM_ADDR_M	0x0ff00000
#define R88E_LSSI_PARAM_ADDR_S	20

/* Bits for R92C_FPGA0_ANAPARAM2. */
#define R92C_FPGA0_ANAPARAM2_CBW20	0x00000400

/* Bits for R92C_LSSI_READBACK(i). */
#define R92C_LSSI_READBACK_DATA_M	0x000fffff
#define R92C_LSSI_READBACK_DATA_S	0

/* Bits for R92C_OFDM0_AGCCORE1(i). */
#define R92C_OFDM0_AGCCORE1_GAIN_M	0x0000007f
#define R92C_OFDM0_AGCCORE1_GAIN_S	0

/*
 * USB registers.
 */
#define R92C_USB_INFO			0xfe17
#define R92C_TEST_USB_TXQS		0xfe48
#define R92C_USB_SPECIAL_OPTION		0xfe55
#define R92C_USB_HCPWM			0xfe57
#define R92C_USB_HRPWM			0xfe58
#define R92C_USB_DMA_AGG_TO		0xfe5b
#define R92C_USB_AGG_TO			0xfe5c
#define R92C_USB_AGG_TH			0xfe5d
#define R92C_USB_VID			0xfe60
#define R92C_USB_PID			0xfe62
#define R92C_USB_OPTIONAL		0xfe64
#define R92C_USB_EP			0xfe65
#define R92C_USB_PHY			0xfe68	/* XXX: linux-3.7.4(rtlwifi/rtl8192ce/reg.h) has 0xfe66 */
#define R92C_USB_MAC_ADDR		0xfe70
#define R92C_USB_STRING			0xfe80

/* Bits for R92C_USB_SPECIAL_OPTION. */
#define R92C_USB_SPECIAL_OPTION_AGG_EN		0x08
#define R92C_USB_SPECIAL_OPTION_INT_BULK_SEL	0x10

/* Bits for R92C_USB_EP. */
#define R92C_USB_EP_HQ_M	0x000f
#define R92C_USB_EP_HQ_S	0
#define R92C_USB_EP_NQ_M	0x00f0
#define R92C_USB_EP_NQ_S	4
#define R92C_USB_EP_LQ_M	0x0f00
#define R92C_USB_EP_LQ_S	8

/* Bits for R92C_RD_CTRL. */
#define R92C_RD_CTRL_DIS_EDCA_CNT_DWN	__BIT(11)

/*
 * Firmware base address.
 */
#define R92C_FW_START_ADDR	0x1000
#define R92C_FW_PAGE_SIZE	4096


/*
 * RF (6052) registers.
 */
#define R92C_RF_AC		0x00
#define R92C_RF_IQADJ_G(i)	(0x01 + (i))
#define R92C_RF_POW_TRSW	0x05
#define R92C_RF_GAIN_RX		0x06
#define R92C_RF_GAIN_TX		0x07
#define R92C_RF_TXM_IDAC	0x08
#define R92C_RF_BS_IQGEN	0x0f
#define R92C_RF_MODE1		0x10
#define R92C_RF_MODE2		0x11
#define R92C_RF_RX_AGC_HP	0x12
#define R92C_RF_TX_AGC		0x13
#define R92C_RF_BIAS		0x14
#define R92C_RF_IPA		0x15
#define R92C_RF_POW_ABILITY	0x17
#define R92C_RF_CHNLBW		0x18
#define R92C_RF_RX_G1		0x1a
#define R92C_RF_RX_G2		0x1b
#define R92C_RF_RX_BB2		0x1c
#define R92C_RF_RX_BB1		0x1d
#define R92C_RF_RCK1		0x1e
#define R92C_RF_RCK2		0x1f
#define R92C_RF_TX_G(i)		(0x20 + (i))
#define R92C_RF_TX_BB1		0x23
#define R92C_RF_T_METER		0x24
#define R92C_RF_SYN_G(i)	(0x25 + (i))
#define R92C_RF_RCK_OS		0x30
#define R92C_RF_TXPA_G(i)	(0x31 + (i))

/* Bits for R92C_RF_AC. */
#define R92C_RF_AC_MODE_M	0x70000
#define R92C_RF_AC_MODE_S	16
#define R92C_RF_AC_MODE_STANDBY	1

/* Bits for R92C_RF_CHNLBW. */
#define R92C_RF_CHNLBW_CHNL_M	0x003ff
#define R92C_RF_CHNLBW_CHNL_S	0
#define R92C_RF_CHNLBW_BW20	0x00400
#define R88E_RF_CHNLBW_BW20	0x00c00
#define R92C_RF_CHNLBW_LCSTART	0x08000


/*
 * CAM entries.
 */
#define R92C_CAM_ENTRY_COUNT	32

#define R92C_CAM_CTL0(entry)	((entry) * 8 + 0)
#define R92C_CAM_CTL1(entry)	((entry) * 8 + 1)
#define R92C_CAM_KEY(entry, i)	((entry) * 8 + 2 + (i))

/* Bits for R92C_CAM_CTL0(i). */
#define R92C_CAM_KEYID_M	0x00000003
#define R92C_CAM_KEYID_S	0
#define R92C_CAM_ALGO_M		0x0000001c
#define R92C_CAM_ALGO_S		2
#define R92C_CAM_ALGO_NONE	0
#define R92C_CAM_ALGO_WEP40	1
#define R92C_CAM_ALGO_TKIP	2
#define R92C_CAM_ALGO_AES	4
#define R92C_CAM_ALGO_WEP104	5
#define R92C_CAM_VALID		0x00008000
#define R92C_CAM_MACLO_M	0xffff0000
#define R92C_CAM_MACLO_S	16

/* Rate adaptation modes. */
#define R92C_RAID_11GN	1
#define R92C_RAID_11N	3
#define R92C_RAID_11BG	4
#define R92C_RAID_11G	5	/* "pure" 11g */
#define R92C_RAID_11B	6


/* Macros to access unaligned little-endian memory. */
#define LE_READ_2(x)	((x)[0] | ((x)[1]<<8))
#define LE_READ_4(x)	((x)[0] | ((x)[1]<<8) | ((x)[2]<<16) | ((x)[3]<<24))

/*
 * Macros to access subfields in registers.
 */
/* Mask and Shift (getter). */
#define MS(val, field)							\
	(((val) & field##_M) >> field##_S)

/* Shift and Mask (setter). */
#define SM(field, val)							\
	(((val) << field##_S) & field##_M)

/* Rewrite. */
#define RW(var, field, val)						\
	(((var) & ~field##_M) | SM(field, val))

/*
 * Firmware image header.
 */
struct r92c_fw_hdr {
	/* QWORD0 */
	uint16_t	signature;
	uint8_t		category;
	uint8_t		function;
	uint16_t	version;
	uint16_t	subversion;
	/* QWORD1 */
	uint8_t		month;
	uint8_t		date;
	uint8_t		hour;
	uint8_t		minute;
	uint16_t	ramcodesize;
	uint16_t	reserved2;
	/* QWORD2 */
	uint32_t	svnidx;
	uint32_t	reserved3;
	/* QWORD3 */
	uint32_t	reserved4;
	uint32_t	reserved5;
} __packed;

/*
 * Host to firmware commands.
 */
struct r92c_fw_cmd {
	uint8_t	id;
#define R92C_CMD_AP_OFFLOAD		0
#define R92C_CMD_SET_PWRMODE		1
#define R92C_CMD_JOINBSS_RPT		2
#define R92C_CMD_RSVD_PAGE		3
#define R92C_CMD_RSSI			4
#define R92C_CMD_RSSI_SETTING		5
#define R92C_CMD_MACID_CONFIG		6
#define R92C_CMD_MACID_PS_MODE		7
#define R92C_CMD_P2P_PS_OFFLOAD		8
#define R92C_CMD_SELECTIVE_SUSPEND	9
#define R92C_CMD_FLAG_EXT		0x80

	uint8_t	msg[5];
} __packed;

/* Structure for R92C_CMD_RSSI_SETTING. */
struct r92c_fw_cmd_rssi {
	uint8_t	macid;
	uint8_t	reserved;
	uint8_t	pwdb;
} __packed;

/* Structure for R92C_CMD_MACID_CONFIG. */
struct r92c_fw_cmd_macid_cfg {
	uint8_t	mask[4];
	uint8_t	macid;
#define URTWN_MACID_BSS		0
#define URTWN_MACID_BC		4	/* Broadcast. */
#define URTWN_MACID_VALID	0x80
} __packed;

/*
 * RTL8192CU ROM image.
 */
struct r92c_rom {
	uint16_t	id;		/* 0x8192 */
	uint8_t		reserved1[5];
	uint8_t		dbg_sel;
	uint16_t	reserved2;
	uint16_t	vid;
	uint16_t	pid;
	uint8_t		usb_opt;
	uint8_t		ep_setting;
	uint16_t	reserved3;
	uint8_t		usb_phy;
	uint8_t		reserved4[3];
	uint8_t		macaddr[6];
	uint8_t		string[61];	/* "Realtek" */
	uint8_t		subcustomer_id;
	uint8_t		cck_tx_pwr[R92C_MAX_CHAINS][3];
	uint8_t		ht40_1s_tx_pwr[R92C_MAX_CHAINS][3];
	uint8_t		ht40_2s_tx_pwr_diff[3];
	uint8_t		ht20_tx_pwr_diff[3];
	uint8_t		ofdm_tx_pwr_diff[3];
	uint8_t		ht40_max_pwr[3];
	uint8_t		ht20_max_pwr[3];
	uint8_t		xtal_calib;
	uint8_t		tssi[R92C_MAX_CHAINS];
	uint8_t		thermal_meter;
	uint8_t		rf_opt1;
#define R92C_ROM_RF1_REGULATORY_M	0x07
#define R92C_ROM_RF1_REGULATORY_S	0
#define R92C_ROM_RF1_BOARD_TYPE_M	0xe0
#define R92C_ROM_RF1_BOARD_TYPE_S	5
#define R92C_BOARD_TYPE_DONGLE		0
#define R92C_BOARD_TYPE_HIGHPA		1
#define R92C_BOARD_TYPE_MINICARD	2
#define R92C_BOARD_TYPE_SOLO		3
#define R92C_BOARD_TYPE_COMBO		4

	uint8_t		rf_opt2;
	uint8_t		rf_opt3;
	uint8_t		rf_opt4;
	uint8_t		channel_plan;
	uint8_t		version;
	uint8_t		curstomer_id;
} __packed;

/* Rx MAC descriptor. */
struct r92c_rx_stat {
	uint32_t	rxdw0;
#define R92C_RXDW0_PKTLEN_M	0x00003fff
#define R92C_RXDW0_PKTLEN_S	0
#define R92C_RXDW0_CRCERR	0x00004000
#define R92C_RXDW0_ICVERR	0x00008000
#define R92C_RXDW0_INFOSZ_M	0x000f0000
#define R92C_RXDW0_INFOSZ_S	16
#define R92C_RXDW0_QOS		0x00800000
#define R92C_RXDW0_SHIFT_M	0x03000000
#define R92C_RXDW0_SHIFT_S	24
#define R92C_RXDW0_PHYST	0x04000000
#define R92C_RXDW0_DECRYPTED	0x08000000

	uint32_t	rxdw1;
	uint32_t	rxdw2;
#define R92C_RXDW2_PKTCNT_M	0x00ff0000
#define R92C_RXDW2_PKTCNT_S	16

	uint32_t	rxdw3;
#define R92C_RXDW3_RATE_M	0x0000003f
#define R92C_RXDW3_RATE_S	0
#define R92C_RXDW3_HT		0x00000040
#define R92C_RXDW3_HTC		0x00000400

	uint32_t	rxdw4;
	uint32_t	rxdw5;
} __packed __aligned(4);

/* Rx PHY descriptor. */
struct r92c_rx_phystat {
	uint32_t	phydw0;
	uint32_t	phydw1;
	uint32_t	phydw2;
	uint32_t	phydw3;
	uint32_t	phydw4;
	uint32_t	phydw5;
	uint32_t	phydw6;
	uint32_t	phydw7;
} __packed __aligned(4);

/* Rx PHY CCK descriptor. */
struct r92c_rx_cck {
	uint8_t		adc_pwdb[4];
	uint8_t		sq_rpt;
	uint8_t		agc_rpt;
} __packed;

struct r88e_rx_cck {
	uint8_t		path_agc[2];
	uint8_t		sig_qual;
	uint8_t		agc_rpt;
	uint8_t		rpt_b;
	uint8_t		reserved1;
	uint8_t		noise_power;
	uint8_t		path_cfotail[2];
	uint8_t		pcts_mask[2];
	uint8_t		stream_rxevm[2];
	uint8_t		path_rxsnr[2];
	uint8_t		noise_power_db_lsb;
	uint8_t		reserved2[3];
	uint8_t		stream_csi[2];
	uint8_t		stream_target_csi[2];
	uint8_t		sig_evm;
	uint8_t		reserved3;
	uint8_t		reserved4;
} __packed;

/* Tx MAC descriptor. */
struct r92c_tx_desc {
	uint32_t	txdw0;
#define R92C_TXDW0_PKTLEN_M	0x0000ffff
#define R92C_TXDW0_PKTLEN_S	0
#define R92C_TXDW0_OFFSET_M	0x00ff0000
#define R92C_TXDW0_OFFSET_S	16
#define R92C_TXDW0_BMCAST	0x01000000
#define R92C_TXDW0_LSG		0x04000000
#define R92C_TXDW0_FSG		0x08000000
#define R92C_TXDW0_OWN		0x80000000

	uint32_t	txdw1;
#define R92C_TXDW1_MACID_M	0x0000001f
#define R92C_TXDW1_MACID_S	0
#define R88E_TXDW1_MACID_M	0x0000003f
#define R88E_TXDW1_MACID_S	0
#define R92C_TXDW1_AGGEN	0x00000020
#define R92C_TXDW1_AGGBK	0x00000040
#define R92C_TXDW1_QSEL_M	0x00001f00
#define R92C_TXDW1_QSEL_S	8
#define R92C_TXDW1_QSEL_BE	0x00
#define R92C_TXDW1_QSEL_MGNT	0x12
#define R92C_TXDW1_RAID_M	0x000f0000
#define R92C_TXDW1_RAID_S	16
#define R92C_TXDW1_CIPHER_M	0x00c00000
#define R92C_TXDW1_CIPHER_S	22
#define R92C_TXDW1_CIPHER_NONE	0
#define R92C_TXDW1_CIPHER_RC4	1
#define R92C_TXDW1_CIPHER_AES	3
#define R92C_TXDW1_PKTOFF_M	0x7c000000
#define R92C_TXDW1_PKTOFF_S	26

	uint32_t	txdw2;
#define R88E_TXDW2_AGGBK	0x00010000

	uint16_t	txdw3;
	uint16_t	txdseq;

	uint32_t	txdw4;
#define R92C_TXDW4_RTSRATE_M	0x0000003f
#define R92C_TXDW4_RTSRATE_S	0
#define R92C_TXDW4_QOS		0x00000040
#define R92C_TXDW4_HWSEQ	0x00000080
#define R92C_TXDW4_DRVRATE	0x00000100
#define R92C_TXDW4_CTS2SELF	0x00000800
#define R92C_TXDW4_RTSEN	0x00001000
#define R92C_TXDW4_HWRTSEN	0x00002000
#define R92C_TXDW4_SCO_M	0x003f0000
#define R92C_TXDW4_SCO_S	20
#define R92C_TXDW4_SCO_SCA	1
#define R92C_TXDW4_SCO_SCB	2
#define R92C_TXDW4_40MHZ	0x02000000

	uint32_t	txdw5;
#define R92C_TXDW5_DATARATE_M	0x0000003f
#define R92C_TXDW5_DATARATE_S	0
#define R92C_TXDW5_SGI		0x00000040
#define R92C_TXDW5_AGGNUM_M	0xff000000
#define R92C_TXDW5_AGGNUM_S	24

	uint32_t	txdw6;
	uint16_t	txdsum;
	uint16_t	pad;
} __packed __aligned(4);
