/*	$NetBSD: if_otusreg.h,v 1.4 2013/01/20 21:50:41 christos Exp $	*/
/*	$OpenBSD: if_otusreg.h,v 1.6 2009/04/06 18:17:01 damien Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
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
#ifndef _IF_OTUSREG_H_
#define _IF_OTUSREG_H_

/* USB Endpoints addresses. */
#define AR_EPT_BULK_TX_NO	(UE_DIR_OUT | 1)
#define AR_EPT_BULK_RX_NO	(UE_DIR_IN  | 2)
#define AR_EPT_INTR_RX_NO	(UE_DIR_IN  | 3)
#define AR_EPT_INTR_TX_NO	(UE_DIR_OUT | 4)

/* USB Requests. */
#define AR_FW_DOWNLOAD			0x30
#define AR_FW_DOWNLOAD_COMPLETE		0x31

/* Maximum number of writes that can fit in a single FW command is 7. */
#define AR_FW_MAX_WRITES		7	/* 56 bytes */

#define AR_FW_INIT_ADDR			0x102800
#define AR_FW_MAIN_ADDR			0x200000

/*
 * AR9170 MAC registers.
 */
#define AR_MAC_REG_BASE			0x1c3000
#define AR_MAC_REG_MAC_ADDR_L		(AR_MAC_REG_BASE + 0x610)
#define AR_MAC_REG_MAC_ADDR_H		(AR_MAC_REG_BASE + 0x614)
#define AR_MAC_REG_BSSID_L		(AR_MAC_REG_BASE + 0x618)
#define AR_MAC_REG_BSSID_H		(AR_MAC_REG_BASE + 0x61c)
#define AR_MAC_REG_GROUP_HASH_TBL_L	(AR_MAC_REG_BASE + 0x624)
#define AR_MAC_REG_GROUP_HASH_TBL_H	(AR_MAC_REG_BASE + 0x628)
#define AR_MAC_REG_RX_TIMEOUT		(AR_MAC_REG_BASE + 0x62c)
#define AR_MAC_REG_BASIC_RATE		(AR_MAC_REG_BASE + 0x630)
#define AR_MAC_REG_MANDATORY_RATE	(AR_MAC_REG_BASE + 0x634)
#define AR_MAC_REG_RTS_CTS_RATE		(AR_MAC_REG_BASE + 0x638)
#define AR_MAC_REG_BACKOFF_PROTECT	(AR_MAC_REG_BASE + 0x63c)
#define AR_MAC_REG_RX_THRESHOLD		(AR_MAC_REG_BASE + 0x640)
#define AR_MAC_REG_RX_PE_DELAY		(AR_MAC_REG_BASE + 0x64c)
#define AR_MAC_REG_DYNAMIC_SIFS_ACK	(AR_MAC_REG_BASE + 0x658)
#define AR_MAC_REG_SNIFFER		(AR_MAC_REG_BASE + 0x674)
#define	  AR_MAC_REG_SNIFFER_ENABLE_PROMISC	(1 << 0)
#define   AR_MAC_REG_SNIFFER_HW_MIC_CHECK	0x02000000
#define	  AR_MAC_REG_SNIFFER_DEFAULTS		0x02000000
#define AR_MAC_REG_ENCRYPTION		(AR_MAC_REG_BASE + 0x678)
#define   AR_MAC_REG_ENCRYPTION_RX_SOFTWARE	(1 << 3)
#define   AR_MAC_REG_ENCRYPTION_DEFAULTS	0x00000070

#define AR_MAC_REG_MISC_680		(AR_MAC_REG_BASE + 0x680)
#define AR_MAC_REG_TX_UNDERRUN		(AR_MAC_REG_BASE + 0x688)

#define AR_MAC_REG_FRAMETYPE_FILTER	(AR_MAC_REG_BASE + 0x68c)
#define	  AR_MAC_REG_FTF_ASSOC_REQ		(1 << 0)
#define	  AR_MAC_REG_FTF_ASSOC_RESP		(1 << 1)
#define	  AR_MAC_REG_FTF_REASSOC_REQ		(1 << 2)
#define	  AR_MAC_REG_FTF_REASSOC_RESP		(1 << 3)
#define	  AR_MAC_REG_FTF_PRB_REQ		(1 << 4)
#define	  AR_MAC_REG_FTF_PRB_RESP		(1 << 5)
#define	  AR_MAC_REG_FTF_BIT6			(1 << 6)
#define	  AR_MAC_REG_FTF_BIT7			(1 << 7)
#define	  AR_MAC_REG_FTF_BEACON			(1 << 8)
#define	  AR_MAC_REG_FTF_ATIM			(1 << 9)
#define	  AR_MAC_REG_FTF_DEASSOC		(1 << 10)
#define	  AR_MAC_REG_FTF_AUTH			(1 << 11)
#define	  AR_MAC_REG_FTF_DEAUTH			(1 << 12)
#define	  AR_MAC_REG_FTF_BIT13			(1 << 13)
#define	  AR_MAC_REG_FTF_BIT14			(1 << 14)
#define	  AR_MAC_REG_FTF_BIT15			(1 << 15)
#define	  AR_MAC_REG_FTF_BAR			(1 << 24)
#define	  AR_MAC_REG_FTF_BA			(1 << 25)
#define	  AR_MAC_REG_FTF_PSPOLL			(1 << 26)
#define	  AR_MAC_REG_FTF_RTS			(1 << 27)
#define	  AR_MAC_REG_FTF_CTS			(1 << 28)
#define	  AR_MAC_REG_FTF_ACK			(1 << 29)
#define	  AR_MAC_REG_FTF_CFE			(1 << 30)
#define	  AR_MAC_REG_FTF_CFE_ACK		(1 << 31)
#define	  AR_MAC_REG_FTF_DEFAULTS		0x0700ffff
#define	  AR_MAC_REG_FTF_MONITOR		0xfd00ffff
//#define	  AR_MAC_REG_FTF_MONITOR		0xff00ffff

#define AR_MAC_REG_ACK_EXTENSION	(AR_MAC_REG_BASE + 0x690)
#define AR_MAC_REG_EIFS_AND_SIFS	(AR_MAC_REG_BASE + 0x698)
#define AR_MAC_REG_BUSY			(AR_MAC_REG_BASE + 0x6e8)
#define AR_MAC_REG_BUSY_EXT		(AR_MAC_REG_BASE + 0x6ec)
#define AR_MAC_REG_SLOT_TIME		(AR_MAC_REG_BASE + 0x6f0)
#define AR_MAC_REG_POWERMANAGEMENT	(AR_MAC_REG_BASE + 0x700)
#define   AR_MAC_REG_POWERMGT_IBSS		0xe0
#define	  AR_MAC_REG_POWERMGT_AP		0xa1
#define	  AR_MAC_REG_POWERMGT_STA		0x02
#define	  AR_MAC_REG_POWERMGT_AP_WDS		0x03
#define	  AR_MAC_REG_POWERMGT_DEFAULTS		0x0f000000

#define AR_MAC_REG_AC0_CW		(AR_MAC_REG_BASE + 0xb00)
#define AR_MAC_REG_AC1_CW		(AR_MAC_REG_BASE + 0xb04)
#define AR_MAC_REG_AC2_CW		(AR_MAC_REG_BASE + 0xb08)
#define AR_MAC_REG_AC3_CW		(AR_MAC_REG_BASE + 0xb0c)
#define AR_MAC_REG_AC4_CW		(AR_MAC_REG_BASE + 0xb10)
#define AR_MAC_REG_AC1_AC0_AIFS		(AR_MAC_REG_BASE + 0xb14)
#define AR_MAC_REG_AC3_AC2_AIFS		(AR_MAC_REG_BASE + 0xb18)
#define AR_MAC_REG_RETRY_MAX		(AR_MAC_REG_BASE + 0xb28)
#define AR_MAC_REG_FCS_SELECT		(AR_MAC_REG_BASE + 0xbb0)
#define	  AR_MAC_FCS_SWFCS			(1 << 0)
#define	  AR_MAC_FCS_FIFO_PROT			(1 << 2)

#define AR_MAC_REG_TXOP_NOT_ENOUGH_INDICATION	\
					(AR_MAC_REG_BASE + 0xb30)
#define AR_MAC_REG_AC1_AC0_TXOP		(AR_MAC_REG_BASE + 0xb44)
#define AR_MAC_REG_AC3_AC2_TXOP		(AR_MAC_REG_BASE + 0xb48)

#define AR_MAC_REG_AMPDU_FACTOR		(AR_MAC_REG_BASE + 0xb9c)
#define AR_MAC_REG_AMPDU_DENSITY	(AR_MAC_REG_BASE + 0xba0)

#define AR_MAC_REG_ACK_TABLE		(AR_MAC_REG_BASE + 0xc00)
#define AR_MAC_REG_AMPDU_RX_THRESH	(AR_MAC_REG_BASE + 0xc50)

#define AR_MAC_REG_OFDM_PHY_ERRORS	(AR_MAC_REG_BASE + 0xcb4)
#define AR_MAC_REG_CCK_PHY_ERRORS	(AR_MAC_REG_BASE + 0xcb8)

#define AR_MAC_REG_DMA			(AR_MAC_REG_BASE + 0xd30)
#define   AR_MAC_REG_DMA_OFF			0
#define   AR_MAC_REG_DMA_ENABLE			(1 << 8)

#define AR_MAC_REG_TXRX_MPI		(AR_MAC_REG_BASE + 0xd7c)
#define	  AR_MAC_TXRX_MPI_TX_MPI_MASK		0x0000000f
#define	  AR_MAC_TXRX_MPI_TX_TO_MASK		0x0000fff0
#define	  AR_MAC_TXRX_MPI_RX_MPI_MASK		0x000f0000
#define	  AR_MAC_TXRX_MPI_RX_TO_MASK		0xfff00000

#define AR_MAC_REG_BCN_ADDR		(AR_MAC_REG_BASE + 0xd84)
#define AR_MAC_REG_BCN_LENGTH		(AR_MAC_REG_BASE + 0xd88)
#define AR_MAC_REG_BCN_PLCP		(AR_MAC_REG_BASE + 0xd90)
#define AR_MAC_REG_BCN_CTRL		(AR_MAC_REG_BASE + 0xd94)
#define AR_MAC_REG_BCN_HT1		(AR_MAC_REG_BASE + 0xda0)
#define AR_MAC_REG_BCN_HT2		(AR_MAC_REG_BASE + 0xda4)

/*
 * GPIO register
 */
#define AR_GPIO_REG_BASE		0x1d0100
#define AR_GPIO_REG_PORT_TYPE		(AR_GPIO_REG_BASE + 0)
#define AR_GPIO_REG_DATA		(AR_GPIO_REG_BASE + 4)
#define   AR_GPIO_REG_DATA_LED0_ON		(1 << 0)
#define   AR_GPIO_REG_DATA_LED0_OFF		(0 << 0)
#define   AR_GPIO_REG_DATA_LED1_ON		(1 << 1)
#define   AR_GPIO_REG_DATA_LED1_OFF		(0 << 1)
#define AR_NUM_LEDS				2

/*
 * Power register
 */
#define AR_PWR_REG_BASE			0x1d4000
#define AR_PWR_REG_CLOCK_SEL		(AR_PWR_REG_BASE + 0x008)
#define	  AR_PWR_CLK_AHB_40MHZ			0
#define	  AR_PWR_CLK_AHB_20_22MHZ		1
#define	  AR_PWR_CLK_AHB_40_44MHZ		2
#define	  AR_PWR_CLK_AHB_80_88MHZ		3
#define	  AR_PWR_CLK_DAC_160_INV_DLY		0x70

/*
 * USB register
 */
#define AR_USB_REG_BASE			0x1e1000
#define AR_USB_REG_DMA_CTL		(AR_USB_REG_BASE + 0x108)
#define   AR_USB_REG_DMA_CTL_ENABLE_TO_DEVICE		(1 << 0)
#define	  AR_USB_REG_DMA_CTL_ENABLE_FROM_DEVICE		(1 << 1)
#define	  AR_USB_REG_DMA_CTL_HIGH_SPEED			(1 << 2)
#define	  AR_USB_REG_DMA_CTL_PACKET_MODE		(1 << 3)
#define   AR_USB_REG_DMA_CTL_RX_STREAM_4K		(0 << 4)
#define   AR_USB_REG_DMA_CTL_RX_STREAM_8K		(1 << 4)
#define   AR_USB_REG_DMA_CTL_RX_STREAM_16K		(2 << 4)
#define   AR_USB_REG_DMA_CTL_RX_STREAM_32K		(3 << 4)
#define   AR_USB_REG_DMA_CTL_TX_STREAM_MODE		(1 << 6)

#define AR_USB_REG_MAX_AGG_UPLOAD	(AR_USB_REG_BASE + 0x110)
#define AR_USB_REG_UPLOAD_TIME_CTL	(AR_USB_REG_BASE + 0x114)

/* Possible values for register AR_USB_MODE_CTRL. */
#define AR_USB_DS_ENA		(1 << 0)
#define AR_USB_US_ENA		(1 << 1)
#define AR_USB_US_PACKET_MODE	(1 << 3)

/*
 * PHY registers.
 */
#define AR_PHY_BASE			0x1c5800
#define AR_PHY(reg)			(AR_PHY_BASE + (reg) * 4)
#define AR_PHY_TURBO			(AR_PHY_BASE + 0x0004)
#define AR_PHY_RF_CTL3			(AR_PHY_BASE + 0x0028)
#define AR_PHY_RF_CTL4			(AR_PHY_BASE + 0x0034)
#define AR_PHY_SETTLING			(AR_PHY_BASE + 0x0044)
#define AR_PHY_RXGAIN			(AR_PHY_BASE + 0x0048)
#define AR_PHY_DESIRED_SZ		(AR_PHY_BASE + 0x0050)
#define AR_PHY_FIND_SIG			(AR_PHY_BASE + 0x0058)
#define AR_PHY_AGC_CTL1			(AR_PHY_BASE + 0x005c)
#define AR_PHY_SFCORR			(AR_PHY_BASE + 0x0068)
#define AR_PHY_SFCORR_LOW		(AR_PHY_BASE + 0x006c)
#define AR_PHY_TIMING_CTRL4		(AR_PHY_BASE + 0x0120)
#define AR_PHY_TIMING5			(AR_PHY_BASE + 0x0124)
#define AR_PHY_POWER_TX_RATE1		(AR_PHY_BASE + 0x0134)
#define AR_PHY_POWER_TX_RATE2		(AR_PHY_BASE + 0x0138)
#define AR_PHY_POWER_TX_RATE_MAX	(AR_PHY_BASE + 0x013c)
#define AR_PHY_SWITCH_CHAIN_0		(AR_PHY_BASE + 0x0160)
#define AR_PHY_SWITCH_COM		(AR_PHY_BASE + 0x0164)
#define AR_PHY_HEAVY_CLIP_ENABLE	(AR_PHY_BASE + 0x01e0)
#define AR_PHY_CCK_DETECT		(AR_PHY_BASE + 0x0a08)
#define AR_PHY_GAIN_2GHZ		(AR_PHY_BASE + 0x0a0c)
#define AR_PHY_POWER_TX_RATE3		(AR_PHY_BASE + 0x0a34)
#define AR_PHY_POWER_TX_RATE4		(AR_PHY_BASE + 0x0a38)
#define AR_PHY_TPCRG1			(AR_PHY_BASE + 0x0a58)
#define AR_PHY_POWER_TX_RATE5		(AR_PHY_BASE + 0x0b8c)
#define AR_PHY_POWER_TX_RATE6		(AR_PHY_BASE + 0x0b90)
#define AR_PHY_POWER_TX_RATE7		(AR_PHY_BASE + 0x0bcc)
#define AR_PHY_POWER_TX_RATE8		(AR_PHY_BASE + 0x0bd0)
#define AR_PHY_POWER_TX_RATE9		(AR_PHY_BASE + 0x0bd4)
#define AR_PHY_CCA			(AR_PHY_BASE + 0x3064)

#define AR_SEEPROM_HW_TYPE_OFFSET	0x1374
#define AR_EEPROM_OFFSET		0x1600

#define AR_BANK4_CHUP			(1 << 0)
#define AR_BANK4_BMODE_LF_SYNTH_FREQ	(1 << 1)
#define AR_BANK4_AMODE_REFSEL(x)	((x) << 2)
#define AR_BANK4_ADDR(x)		((x) << 5)

/* Tx descriptor. */
struct ar_tx_head {
	uint16_t	len;
	uint16_t	macctl;
#define AR_TX_MAC_RTS		(1 <<  0)
#define AR_TX_MAC_CTS		(1 <<  1)
#define AR_TX_MAC_BACKOFF	(1 <<  3)
#define AR_TX_MAC_NOACK		(1 <<  2)
#define AR_TX_MAC_HW_DUR	(1 <<  9)
#define AR_TX_MAC_QID(qid)	((qid) << 10)
#define AR_TX_MAC_RATE_PROBING	(1 << 15)

	uint32_t	phyctl;
/* Modulation type. */
#define AR_TX_PHY_MT_CCK	0
#define AR_TX_PHY_MT_OFDM	1
#define AR_TX_PHY_MT_HT		2
#define AR_TX_PHY_GF		(1 << 2)
#define AR_TX_PHY_BW_SHIFT	3
#define AR_TX_PHY_TPC_SHIFT	9
#define AR_TX_PHY_ANTMSK(msk)	((msk) << 15)
#define AR_TX_PHY_MCS(mcs)	((mcs) << 18)
#define AR_TX_PHY_SHGI		(1 << 31)
} __packed;

/* USB Rx stream mode header. */
struct ar_rx_head {
	uint16_t	len;
	uint16_t	tag;
#define AR_RX_HEAD_TAG	0x4e00
} __packed;

/* Rx descriptor. */
struct ar_rx_tail {
	uint8_t	rssi_ant[3];
	uint8_t	rssi_ant_ext[3];
	uint8_t	rssi;		/* Combined RSSI. */
	uint8_t	evm[2][6];	/* Error Vector Magnitude. */
	uint8_t	phy_err;
	uint8_t	sa_idx;
	uint8_t	da_idx;
	uint8_t	error;
#define AR_RX_ERROR_TIMEOUT	(1 << 0)
#define AR_RX_ERROR_OVERRUN	(1 << 1)
#define AR_RX_ERROR_DECRYPT	(1 << 2)
#define AR_RX_ERROR_FCS		(1 << 3)
#define AR_RX_ERROR_BAD_RA	(1 << 4)
#define AR_RX_ERROR_PLCP	(1 << 5)
#define AR_RX_ERROR_MMIC	(1 << 6)

	uint8_t	status;
/* Modulation type (same as AR_TX_PHY_MT). */
#define AR_RX_STATUS_MT_MASK	0x3
#define AR_RX_STATUS_MT_CCK	0
#define AR_RX_STATUS_MT_OFDM	1
#define AR_RX_STATUS_MT_HT	2
#define AR_RX_STATUS_SHPREAMBLE	(1 << 3)
} __packed;

#define AR_PLCP_HDR_LEN	12
/* Magic PLCP header for firmware notifications through Rx bulk pipe. */
static uint8_t AR_PLCP_HDR_INTR[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/* Firmware command/reply header. */
struct ar_cmd_hdr {
	uint8_t		len;
	uint8_t		code;
#define AR_CMD_RREG		0x00
#define AR_CMD_WREG		0x01
#define AR_CMD_RMEM		0x02
#define AR_CMD_WMEM		0x03
#define AR_CMD_BITAND		0x04
#define AR_CMD_BITOR		0x05
#define AR_CMD_EKEY		0x28
#define AR_CMD_DKEY		0x29
#define AR_CMD_FREQUENCY	0x30
#define AR_CMD_RF_INIT		0x31
#define AR_CMD_SYNTH		0x32
#define AR_CMD_FREQ_STRAT	0x33
#define AR_CMD_ECHO		0x80
#define AR_CMD_TALLY		0x81
#define AR_CMD_TALLY_APD	0x82
#define AR_CMD_CONFIG		0x83
#define AR_CMD_RESET		0x90
#define AR_CMD_DKRESET		0x91
#define AR_CMD_DKTX_STATUS	0x92
#define AR_CMD_FDC		0xa0
#define AR_CMD_WREEPROM		0xb0
#define AR_CMD_WFLASH		AR_CMD_WREEPROM
#define AR_CMD_FLASH_ERASE	0xb1
#define AR_CMD_FLASH_PROG	0xb2
#define AR_CMD_FLASH_CHKSUM	0xb3
#define AR_CMD_FLASH_READ	0xb4
#define AR_CMD_FW_DL_INIT	0xb5
#define AR_CMD_MEM_WREEPROM	0xbb
/* Those have the 2 MSB set to 1. */
#define AR_EVT_BEACON		0x00
#define AR_EVT_TX_COMP		0x01
#define AR_EVT_TBTT		0x02
#define AR_EVT_ATIM		0x03

	uint16_t	token;	/* Driver private data. */
} __packed;

/* Structure for command AR_CMD_RF_INIT/AR_CMD_FREQUENCY. */
struct ar_cmd_frequency {
	uint32_t	freq;
	uint32_t	dynht2040;
	uint32_t	htena;
	uint32_t	dsc_exp;
	uint32_t	dsc_man;
	uint32_t	dsc_shgi_exp;
	uint32_t	dsc_shgi_man;
	uint32_t	check_loop_count;
} __packed;

/* Firmware reply for command AR_CMD_FREQUENCY. */
struct ar_rsp_frequency {
	uint32_t	status;
#define AR_CAL_ERR_AGC		(1 << 0)	/* AGC cal unfinished. */
#define AR_CAL_ERR_NF		(1 << 1)	/* Noise cal unfinished. */
#define AR_CAL_ERR_NF_VAL	(1 << 2)	/* NF value unexpected. */

	uint32_t	nf[3];		/* Noisefloor. */
	uint32_t	nf_ext[3];	/* Noisefloor ext. */
} __packed;

/* Structure for command AR_CMD_EKEY. */
struct ar_cmd_ekey {
	uint16_t	uid;	/* user ID */
	uint16_t	kix;
	uint16_t	cipher;
#define AR_CIPHER_NONE		0
#define AR_CIPHER_WEP64		1
#define AR_CIPHER_TKIP		2
#define AR_CIPHER_AES		4
#define AR_CIPHER_WEP128	5
#define AR_CIPHER_WEP256	6
#define AR_CIPHER_CENC		7

	uint8_t		macaddr[IEEE80211_ADDR_LEN];
	uint8_t		key[16];
} __packed;

/* Structure for event AR_EVT_TX_COMP. */
struct ar_evt_tx_comp {
	uint8_t		macaddr[IEEE80211_ADDR_LEN];
	uint32_t	phy;
	uint16_t	status;
#define AR_TX_STATUS_COMP	0
#define AR_TX_STATUS_RETRY_COMP	1
#define AR_TX_STATUS_FAILED	2
} __packed;

/*
 * EEPROM.
 */
/* Possible flags for opCapFlags. */
#define AR5416_OPFLAGS_11A	0x01
#define AR5416_OPFLAGS_11G	0x02
#define AR5416_OPFLAGS_5G_HT40	0x04
#define AR5416_OPFLAGS_2G_HT40	0x08
#define AR5416_OPFLAGS_5G_HT20	0x10
#define AR5416_OPFLAGS_2G_HT20	0x20

#define AR5416_NUM_5G_CAL_PIERS		8
#define AR5416_NUM_2G_CAL_PIERS		4
#define AR5416_NUM_5G_20_TARGET_POWERS	8
#define AR5416_NUM_5G_40_TARGET_POWERS	8
#define AR5416_NUM_2G_CCK_TARGET_POWERS	3
#define AR5416_NUM_2G_20_TARGET_POWERS	4
#define AR5416_NUM_2G_40_TARGET_POWERS	4
#define AR5416_NUM_CTLS			24
#define AR5416_NUM_BAND_EDGES		8
#define AR5416_NUM_PD_GAINS		4
#define AR5416_PD_GAIN_ICEPTS		5
#define AR5416_EEPROM_MODAL_SPURS	5
#define AR5416_MAX_CHAINS		2

typedef struct BaseEepHeader {
	uint16_t	length;
	uint16_t	checksum;
	uint16_t	version;
	uint8_t		opCapFlags;
	uint8_t		eepMisc;
	uint16_t	regDmn[2];
	uint8_t		macAddr[6];
	uint8_t		rxMask;
	uint8_t		txMask;
	uint16_t	rfSilent;
	uint16_t	blueToothOptions;
	uint16_t	deviceCap;
	uint32_t	binBuildNumber;
	uint8_t		deviceType;
	uint8_t		futureBase[33];
} __packed BASE_EEP_HEADER;

typedef struct spurChanStruct {
	uint16_t	spurChan;
	uint8_t		spurRangeLow;
	uint8_t		spurRangeHigh;
} __packed SPUR_CHAN;

typedef struct ModalEepHeader {
	uint32_t	antCtrlChain[AR5416_MAX_CHAINS];
	uint32_t	antCtrlCommon;
	int8_t		antennaGainCh[AR5416_MAX_CHAINS];
	uint8_t		switchSettling;
	uint8_t		txRxAttenCh[AR5416_MAX_CHAINS];
	uint8_t		rxTxMarginCh[AR5416_MAX_CHAINS];
	uint8_t		adcDesiredSize;
	int8_t		pgaDesiredSize;
	uint8_t		xlnaGainCh[AR5416_MAX_CHAINS];
	uint8_t		txEndToXpaOff;
	uint8_t		txEndToRxOn;
	uint8_t		txFrameToXpaOn;
	uint8_t		thresh62;
	uint8_t		noiseFloorThreshCh[AR5416_MAX_CHAINS];
	uint8_t		xpdGain;
	uint8_t		xpd;
	int8_t		iqCalICh[AR5416_MAX_CHAINS];
	int8_t		iqCalQCh[AR5416_MAX_CHAINS];
	uint8_t		pdGainOverlap;
	uint8_t		ob;
	uint8_t		db;
	uint8_t		xpaBiasLvl;
	uint8_t		pwrDecreaseFor2Chain;
	uint8_t		pwrDecreaseFor3Chain;
	uint8_t		txFrameToDataStart;
	uint8_t		txFrameToPaOn;
	uint8_t		ht40PowerIncForPdadc;
	uint8_t		bswAtten[AR5416_MAX_CHAINS];
	uint8_t		bswMargin[AR5416_MAX_CHAINS];
	uint8_t		swSettleHt40;
	uint8_t		futureModal[22];
	SPUR_CHAN	spurChans[AR5416_EEPROM_MODAL_SPURS];
} __packed MODAL_EEP_HEADER;

typedef struct calDataPerFreq {
	uint8_t		pwrPdg[AR5416_NUM_PD_GAINS][AR5416_PD_GAIN_ICEPTS];
	uint8_t		vpdPdg[AR5416_NUM_PD_GAINS][AR5416_PD_GAIN_ICEPTS];
} __packed CAL_DATA_PER_FREQ;

typedef struct CalTargetPowerLegacy {
	uint8_t		bChannel;
	uint8_t		tPow2x[4];
} __packed CAL_TARGET_POWER_LEG;

typedef struct CalTargetPowerHt {
	uint8_t		bChannel;
	uint8_t		tPow2x[8];
} __packed CAL_TARGET_POWER_HT;

typedef struct CalCtlEdges {
	uint8_t		bChannel;
	uint8_t		tPowerFlag;
} __packed CAL_CTL_EDGES;

typedef struct CalCtlData {
	CAL_CTL_EDGES	ctlEdges[AR5416_MAX_CHAINS][AR5416_NUM_BAND_EDGES];
} __packed CAL_CTL_DATA;

typedef struct ar5416eeprom {
	BASE_EEP_HEADER		baseEepHeader;
	uint8_t			custData[64];
	MODAL_EEP_HEADER	modalHeader[2];
	uint8_t			calFreqPier5G[AR5416_NUM_5G_CAL_PIERS];
	uint8_t			calFreqPier2G[AR5416_NUM_2G_CAL_PIERS];
	CAL_DATA_PER_FREQ	calPierData5G[AR5416_MAX_CHAINS]
					     [AR5416_NUM_5G_CAL_PIERS];
	CAL_DATA_PER_FREQ	calPierData2G[AR5416_MAX_CHAINS]
					     [AR5416_NUM_2G_CAL_PIERS];
	CAL_TARGET_POWER_LEG	calTPow5G[AR5416_NUM_5G_20_TARGET_POWERS];
	CAL_TARGET_POWER_HT	calTPow5GHT20[AR5416_NUM_5G_20_TARGET_POWERS];
	CAL_TARGET_POWER_HT	calTPow5GHT40[AR5416_NUM_5G_40_TARGET_POWERS];
	CAL_TARGET_POWER_LEG	calTPowCck[AR5416_NUM_2G_CCK_TARGET_POWERS];
	CAL_TARGET_POWER_LEG	calTPow2G[AR5416_NUM_2G_20_TARGET_POWERS];
	CAL_TARGET_POWER_HT	calTPow2GHT20[AR5416_NUM_2G_20_TARGET_POWERS];
	CAL_TARGET_POWER_HT	calTPow2GHT40[AR5416_NUM_2G_40_TARGET_POWERS];
	uint8_t			ctlIndex[AR5416_NUM_CTLS];
	CAL_CTL_DATA		ctlData[AR5416_NUM_CTLS];
	uint8_t			padding[3];
} __packed AR5416_EEPROM;

#endif /* _IF_OTUSREG_H_ */

