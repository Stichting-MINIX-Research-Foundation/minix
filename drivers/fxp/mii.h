/*
ibm/mii.h

Created:	Nov 2004 by Philip Homburg <philip@f-mnx.phicoh.com>

Definitions for the Media Independent (Ethernet) Interface
*/

/* Registers in the Machine Independent Interface (MII) to the PHY.
 * IEEE 802.3 (2000 Edition) Clause 22.
 */
#define MII_CTRL	0x0	/* Control Register (basic) */
#define		MII_CTRL_RST	0x8000	/* Reset PHY */
#define		MII_CTRL_LB	0x4000	/* Enable Loopback Mode */
#define		MII_CTRL_SP_LSB	0x2000	/* Speed Selection (LSB) */
#define		MII_CTRL_ANE	0x1000	/* Auto Negotiation Enable */
#define		MII_CTRL_PD	0x0800	/* Power Down */
#define		MII_CTRL_ISO	0x0400	/* Isolate */
#define		MII_CTRL_RAN	0x0200	/* Restart Auto-Negotiation Process */
#define		MII_CTRL_DM	0x0100	/* Full Duplex */
#define		MII_CTRL_CT	0x0080	/* Enable COL Signal Test */
#define		MII_CTRL_SP_MSB	0x0040	/* Speed Selection (MSB) */
#define			MII_CTRL_SP_10		0x0000	/* 10 Mb/s */
#define			MII_CTRL_SP_100		0x2000	/* 100 Mb/s */
#define			MII_CTRL_SP_1000	0x0040	/* 1000 Mb/s */
#define			MII_CTRL_SP_RES		0x2040	/* Reserved */
#define		MII_CTRL_RES	0x003F	/* Reserved */
#define MII_STATUS	0x1	/* Status Register (basic) */
#define		MII_STATUS_100T4	0x8000	/* 100Base-T4 support */
#define		MII_STATUS_100XFD	0x4000	/* 100Base-X FD support */
#define		MII_STATUS_100XHD	0x2000	/* 100Base-X HD support */
#define		MII_STATUS_10FD		0x1000	/* 10 Mb/s FD support */
#define		MII_STATUS_10HD		0x0800	/* 10 Mb/s HD support */
#define		MII_STATUS_100T2FD	0x0400	/* 100Base-T2 FD support */
#define		MII_STATUS_100T2HD	0x0200	/* 100Base-T2 HD support */
#define		MII_STATUS_EXT_STAT	0x0100	/* Supports MII_EXT_STATUS */
#define		MII_STATUS_RES		0x0080	/* Reserved */
#define		MII_STATUS_MFPS		0x0040	/* MF Preamble Suppression */
#define		MII_STATUS_ANC		0x0020	/* Auto-Negotiation Completed */
#define		MII_STATUS_RF		0x0010	/* Remote Fault Detected */
#define		MII_STATUS_ANA		0x0008	/* Auto-Negotiation Ability */
#define		MII_STATUS_LS		0x0004	/* Link Up */
#define		MII_STATUS_JD		0x0002	/* Jabber Condition Detected */
#define		MII_STATUS_EC		0x0001	/* Ext Register Capabilities */
#define MII_PHYID_H	0x2	/* PHY ID (high) */
#define		MII_PH_OUI_H_MASK	0xFFFF	/* High part of OUI */
#define			MII_PH_OUI_H_C_SHIFT	6	/* Shift up in OUI */
#define MII_PHYID_L	0x3	/* PHY ID (low) */
#define		MII_PL_OUI_L_MASK	0xFC00	/* Low part of OUI */
#define			MII_PL_OUI_L_SHIFT	10
#define		MII_PL_MODEL_MASK	0x03F0	/* Model */
#define			MII_PL_MODEL_SHIFT	4
#define		MII_PL_REV_MASK		0x000F	/* Revision */
#define MII_ANA		0x4	/* Auto-Negotiation Advertisement */
#define		MII_ANA_NP	0x8000	/* Next PAge */
#define		MII_ANA_RES	0x4000	/* Reserved */
#define		MII_ANA_RF	0x2000	/* Remote Fault */
#define		MII_ANA_TAF_M	0x1FE0	 /* Technology Ability Field */
#define		MII_ANA_TAF_S	5	 /* Shift */
#define			MII_ANA_TAF_RES		0x1000	/* Reserved */
#define			MII_ANA_PAUSE_ASYM	0x0800	/* Asym. Pause */
#define			MII_ANA_PAUSE_SYM	0x0400	/* Sym. Pause */
#define			MII_ANA_100T4		0x0200	/* 100Base-T4 */
#define			MII_ANA_100TXFD		0x0100	/* 100Base-TX FD */
#define			MII_ANA_100TXHD		0x0080	/* 100Base-TX HD */
#define			MII_ANA_10TFD		0x0040	/* 10Base-T FD */
#define			MII_ANA_10THD		0x0020	/* 10Base-T HD */
#define		MII_ANA_SEL_M	0x001F	 /* Selector Field */
#define			MII_ANA_SEL_802_3 0x0001 /* 802.3 */
#define MII_ANLPA	0x5	/* Auto-Neg Link Partner Ability Register */
#define		MII_ANLPA_NP	0x8000	/* Next Page */
#define		MII_ANLPA_ACK	0x4000	/* Acknowledge */
#define		MII_ANLPA_RF	0x2000	/* Remote Fault */
#define		MII_ANLPA_TAF_M	0x1FC0	 /* Technology Ability Field */
#define		MII_ANLPA_SEL_M	0x001F	 /* Selector Field */
#define MII_ANE		0x6	/* Auto-Negotiation Expansion */
#define		MII_ANE_RES	0xFFE0	/* Reserved */
#define		MII_ANE_PDF	0x0010	/* Parallel Detection Fault */
#define		MII_ANE_LPNPA	0x0008	/* Link Partner is Next Page Able */
#define		MII_ANE_NPA	0x0002	/* Local Device is Next Page Able */
#define		MII_ANE_PR	0x0002	/* New Page has been received */
#define		MII_ANE_LPANA	0x0001	/* Link Partner is Auto-Neg.able */
#define MII_ANNPT	0x7	/* Auto-Negotiation Next Page Transmit */
#define MII_ANLPRNP	0x8	/* Auto-Neg Link Partner Received Next Page */
#define MII_MS_CTRL	0x9	/* MASTER-SLAVE Control Register */
#define		MII_MSC_TEST_MODE	0xE000	/* Test mode */
#define		MII_MSC_MS_MANUAL	0x1000	/* Master/slave manual config */
#define		MII_MSC_MS_VAL		0x0800	/* Master/slave value */
#define		MII_MSC_MULTIPORT	0x0400	/* Multi-port device */
#define		MII_MSC_1000T_FD	0x0200	/* 1000Base-T Full Duplex */
#define		MII_MSC_1000T_HD	0x0100	/* 1000Base-T Half Duplex */
#define		MII_MSC_RES		0x00FF	/* Reserved */
#define MII_MS_STATUS	0xA	/* MASTER-SLAVE Status Register */
#define		MII_MSS_FAULT		0x8000	/* Master/slave config fault */
#define		MII_MSS_MASTER		0x4000	/* Master */
#define		MII_MSS_LOCREC		0x2000	/* Local Receiver OK */
#define		MII_MSS_REMREC		0x1000	/* Remote Receiver OK */
#define		MII_MSS_LP1000T_FD	0x0800	/* Link Partner 1000-T FD */
#define		MII_MSS_LP1000T_HD	0x0400	/* Link Partner 1000-T HD */
#define		MII_MSS_RES		0x0300	/* Reserved */
#define		MII_MSS_IDLE_ERR	0x00FF	/* Idle Error Counter */
/* 0xB ... 0xE */		/* Reserved */
#define MII_EXT_STATUS	0xF	/* Extended Status */
#define		MII_ESTAT_1000XFD	0x8000	/* 1000Base-X Full Duplex */
#define		MII_ESTAT_1000XHD	0x4000	/* 1000Base-X Half Duplex */
#define		MII_ESTAT_1000TFD	0x2000	/* 1000Base-T Full Duplex */
#define		MII_ESTAT_1000THD	0x1000	/* 1000Base-T Half Duplex */
#define		MII_ESTAT_RES		0x0FFF	/* Reserved */
/* 0x10 ... 0x1F */		/* Vendor Specific */

_PROTOTYPE( void mii_print_stat_speed, (U16_t stat, U16_t extstat)	);
_PROTOTYPE( void mii_print_techab, (U16_t techab)			);

/*
 * $PchId: mii.h,v 1.1 2004/12/27 13:33:30 philip Exp $
 */
