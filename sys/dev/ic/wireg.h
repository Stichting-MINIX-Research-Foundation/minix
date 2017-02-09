/*	$NetBSD: wireg.h,v 1.53 2007/12/25 18:33:39 perry Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * FreeBSD driver ported to NetBSD by Bill Sommerfeld in the back of the
 * Oslo IETF plenary meeting.
 */

#define WI_DELAY       5
#define WI_TIMEOUT     (500000/WI_DELAY)       /* 500 ms */

#define WI_PORT0	(0 << 8)
#define WI_PORT1	(1 << 8)
#define WI_PORT2	(2 << 8)
#define WI_PORT3	(3 << 8)
#define WI_PORT4	(4 << 8)
#define WI_PORT5	(5 << 8)

/*
 * The WaveLAN/IEEE cards contain an 802.11 MAC controller which Lucent
 * calls 'Hermes.' In typical fashion, getting documentation about this
 * controller is about as easy as squeezing blood from a stone. Here
 * is more or less what I know:
 *
 * - The Hermes controller is firmware driven, and the host interacts
 *   with the Hermes via a firmware interface, which can change.
 *
 * - The Hermes is described in a document called: "Hermes Firmware
 *   WaveLAN/IEEE Station Functions," document #010245, which of course
 *   Lucent will not release without an NDA.
 *
 * - Lucent has created a library called HCF (Hardware Control Functions)
 *   though which it wants developers to interact with the card. The HCF
 *   is needlessly complex, ill conceived and badly documented. Actually,
 *   the comments in the HCP code itself aren't bad, but the publically
 *   available manual that comes with it is awful, probably due largely to
 *   the fact that it has been emasculated in order to hide information
 *   that Lucent wants to keep proprietary. The purpose of the HCF seems
 *   to be to insulate the driver programmer from the Hermes itself so that
 *   Lucent has an excuse not to release programming in for it.
 *
 * - Lucent only makes available documentation and code for 'HCF Light'
 *   which is a stripped down version of HCF with certain features not
 *   implemented, most notably support for 802.11 frames.
 *
 * - The HCF code which I have seen blows goats. Whoever decided to
 *   use a 132 column format should be shot.
 *
 * Rather than actually use the Lucent HCF library, I have stripped all
 * the useful information from it and used it to create a driver in the
 * usual BSD form. Note: I don't want to hear anybody whining about the
 * fact that the Lucent code is GPLed and mine isn't. I did not actually
 * put any of Lucent's code in this driver: I only used it as a reference
 * to obtain information about the underlying hardware. The Hermes
 * programming interface is not GPLed, so bite me.
 */

/*
 * Size of Hermes & Prism2 I/O space.
 */
#define WI_IOSIZE		0x40

/*
 * Hermes & Prism2 register definitions
 */

/* Hermes command/status registers. */
#define WI_COMMAND		0x00
#define WI_PARAM0		0x02
#define WI_PARAM1		0x04
#define WI_PARAM2		0x06
#define WI_STATUS		0x08
#define WI_RESP0		0x0A
#define WI_RESP1		0x0C
#define WI_RESP2		0x0E

/* Command register values. */
#define WI_CMD_BUSY		0x8000 /* busy bit */
#define WI_CMD_INI		0x0000 /* initialize */
#define WI_CMD_ENABLE		0x0001 /* enable */
#define WI_CMD_DISABLE		0x0002 /* disable */
#define WI_CMD_DIAG		0x0003
#define WI_CMD_ALLOC_MEM	0x000A /* allocate NIC memory */
#define WI_CMD_TX		0x000B /* transmit */
#define WI_CMD_NOTIFY		0x0010
#define WI_CMD_INQUIRE		0x0011
#define WI_CMD_ACCESS		0x0021
#define WI_CMD_PROGRAM		0x0022
#define WI_CMD_READEE		0x0030
#define WI_CMD_TEST		0x0038 /* PRISM2 test mode */

#define WI_CMD_CODE_MASK	0x003F

/*
 * Reclaim qualifier bit, applicable to the
 * TX and INQUIRE commands.
 */
#define WI_RECLAIM		0x0100 /* reclaim NIC memory */

/*
 * ACCESS command qualifier bits.
 */
#define WI_ACCESS_READ		0x0000
#define WI_ACCESS_WRITE		0x0100

/*
 * PROGRAM command qualifier bits.
 */
#define WI_PROGRAM_DISABLE	0x0000
#define WI_PROGRAM_ENABLE_RAM	0x0100
#define WI_PROGRAM_ENABLE_NVRAM	0x0200
#define WI_PROGRAM_NVRAM	0x0300

/*
 * DEBUG mode options.
 */
#define WI_TEST_MONITOR		0x0B /* monitor mode for testing */

/* Status register values */
#define WI_STAT_CMD_CODE	0x003F
#define WI_STAT_DIAG_ERR	0x0100
#define WI_STAT_INQ_ERR		0x0500
#define WI_STAT_CMD_RESULT	0x7F00

/* memory handle management registers */
#define WI_INFO_FID		0x10
#define WI_RX_FID		0x20
#define WI_ALLOC_FID		0x22
#define WI_TX_CMP_FID		0x24

/*
 * Buffer Access Path (BAP) registers.
 * These are I/O channels. I believe you can use each one for
 * any desired purpose independently of the other.
 * Currently, we only use BAP0, and perhaps BAP1 can be used
 * within interrupt context.
 */
#define WI_SEL0			0x18
#define WI_SEL1			0x1A
#define WI_OFF0			0x1C
#define WI_OFF1			0x1E
#define WI_DATA0		0x36
#define WI_DATA1		0x38

#define WI_OFF_BUSY		0x8000
#define WI_OFF_ERR		0x4000
#define WI_OFF_DATAOFF		0x0FFF

/* Event registers */
#define WI_EVENT_STAT		0x30	/* Event status */
#define WI_INT_EN		0x32	/* Interrupt enable/disable */
#define WI_EVENT_ACK		0x34	/* Ack event */

/* Events */
#define WI_EV_TICK		0x8000	/* aux timer tick */
#define WI_EV_RES		0x4000	/* controller h/w error (time out) */
#define WI_EV_INFO_DROP		0x2000	/* no RAM to build unsolicited frame */
#define WI_EV_NO_CARD		0x0800	/* card removed (hunh?) */
#define WI_EV_DUIF_RX		0x0400	/* wavelan management packet received */
#define WI_EV_INFO		0x0080	/* async info frame */
#define WI_EV_CMD		0x0010	/* command completed */
#define WI_EV_ALLOC		0x0008	/* async alloc/reclaim completed */
#define WI_EV_TX_EXC		0x0004	/* async xmit completed with failure */
#define WI_EV_TX		0x0002	/* async xmit completed successfully */
#define WI_EV_RX		0x0001	/* async rx completed */

/* Host software registers */
#define WI_SW0			0x28
#define WI_SW1			0x2A
#define WI_SW2			0x2C
#define WI_SW3			0x2E 	/* does not appear in Prism2 */

#define WI_CNTL			0x14

#define WI_CNTL_AUX_ENA		0xC000
#define WI_CNTL_AUX_ENA_STAT	0xC000
#define WI_CNTL_AUX_DIS_STAT	0x0000
#define WI_CNTL_AUX_ENA_CNTL	0x8000
#define WI_CNTL_AUX_DIS_CNTL	0x4000

#define WI_AUX_PAGE		0x3A
#define WI_AUX_OFFSET		0x3C
#define WI_AUX_DATA		0x3E

#define WI_AUX_PGSZ		128
#define WI_AUX_KEY0		0xfe01
#define WI_AUX_KEY1		0xdc23
#define WI_AUX_KEY2		0xba45

#define WI_COR			0x40	/* only for Symbol */
#define WI_COR_RESET		0x0080
#define WI_COR_IOMODE		0x0041

#define WI_HCR			0x42	/* only for Symbol */
#define WI_HCR_4WIRE		0x0010
#define WI_HCR_RUN		0x0007
#define WI_HCR_HOLD		0x000f
#define WI_HCR_EEHOLD		0x00ce

#define WI_TMD_COR		0x14	/* Prism COR (TMD) */
#define WI_TMD_IO		0x18	/* Prism I/O space (TMD) */

/*
 * PCI Host Interface Registers (HFA3842 Specific)
 * The value of all Register's Offset, such as WI_INFO_FID and WI_PARAM0,
 * has doubled.
 * About WI_PCI_COR: In this Register, only soft-reset bit implement; Bit(7).
 */
#define WI_PCI_COR		0x4C
#define WI_PCI_HCR		0x5C
#define WI_PCI_MASTER0_ADDRH	0x80
#define WI_PCI_MASTER0_ADDRL	0x84
#define WI_PCI_MASTER0_LEN	0x88
#define WI_PCI_MASTER0_CON	0x8C

#define WI_PCI_STATUS		0x98

#define WI_PCI_MASTER1_ADDRH	0xA0
#define WI_PCI_MASTER1_ADDRL	0xA4
#define WI_PCI_MASTER1_LEN	0xA8
#define WI_PCI_MASTER1_CON	0xAC

#define WI_COR_SOFT_RESET	(1 << 7)
#define WI_COR_CLEAR		0x00

/*
 * One form of communication with the Hermes is with what Lucent calls
 * LTV records, where LTV stands for Length, Type and Value. The length
 * and type are 16 bits and are in native byte order. The value is in
 * multiples of 16 bits and is in little endian byte order.
 */
struct wi_lt_hdr {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	/* value is vary depends on resource id */
};

/*
 * Download buffer location and length (0xFD01).
 */
struct wi_dnld_buf {
	u_int16_t		wi_buf_pg; /* page addr of intermediate dl buf*/
	u_int16_t		wi_buf_off; /* offset of idb */
	u_int16_t		wi_buf_len; /* len of idb */
};

/*
 * Mem sizes (0xFD02).
 */
struct wi_memsz {
	u_int16_t		wi_mem_ram;
	u_int16_t		wi_mem_nvram;
};

/*
 * NIC Identification (0xFD0B, 0xFD20)
 */
struct wi_ver {
	u_int16_t		wi_ver[4];
};

/* define card ident */
/* Lucent */
#define	WI_NIC_LUCENT_ID	0x0001
#define	WI_NIC_LUCENT_STR	"Lucent Technologies, WaveLAN/IEEE"

#define	WI_NIC_SONY_ID		0x0002
#define	WI_NIC_SONY_STR		"Sony WaveLAN/IEEE"

#define	WI_NIC_LUCENT_EMB_ID	0x0005
#define	WI_NIC_LUCENT_EMB_STR	"Lucent Embedded WaveLAN/IEEE"

/* Intersil */
#define	WI_NIC_EVB2_ID		0x8000
#define	WI_NIC_EVB2_STR		"RF:PRISM2 MAC:HFA3841"

#define	WI_NIC_HWB3763_ID	0x8001
#define	WI_NIC_HWB3763_STR	"RF:PRISM2 MAC:HFA3841 CARD:HWB3763 rev.B"

#define	WI_NIC_HWB3163_ID	0x8002
#define	WI_NIC_HWB3163_STR	"RF:PRISM2 MAC:HFA3841 CARD:HWB3163 rev.A"

#define	WI_NIC_HWB3163B_ID	0x8003
#define	WI_NIC_HWB3163B_STR	"RF:PRISM2 MAC:HFA3841 CARD:HWB3163 rev.B"

#define	WI_NIC_EVB3_ID		0x8004
#define	WI_NIC_EVB3_STR		"RF:PRISM2 MAC:HFA3842 CARD:HFA3842 EVAL"

#define	WI_NIC_HWB1153_ID	0x8007
#define	WI_NIC_HWB1153_STR	"RF:PRISM1 MAC:HFA3841 CARD:HWB1153"

#define	WI_NIC_P2_SST_ID	0x8008	/* Prism2 with SST flush */
#define	WI_NIC_P2_SST_STR	"RF:PRISM2 MAC:HFA3841 CARD:HWB3163-SST-flash"

#define	WI_NIC_EVB2_SST_ID	0x8009
#define	WI_NIC_EVB2_SST_STR	"RF:PRISM2 MAC:HFA3841 CARD:HWB3163-SST-flash"

#define	WI_NIC_3842_EVA_ID	0x800A	/* Prism2 3842 Evaluation Board */
#define	WI_NIC_3842_EVA_STR	"RF:PRISM2 MAC:HFA3842 CARD:HFA3842 EVAL"

#define	WI_NIC_3842_PCMCIA_AMD_ID	0x800B	/* Prism2.5 PCMCIA */
#define	WI_NIC_3842_PCMCIA_SST_ID	0x800C
#define	WI_NIC_3842_PCMCIA_ATM_ID	0x800D
#define	WI_NIC_3842_PCMCIA_STR		"RF:PRISM2.5 MAC:ISL3873B(PCMCIA)"

#define	WI_NIC_3842_MINI_AMD_ID		0x8012	/* Prism2.5 Mini-PCI */
#define	WI_NIC_3842_MINI_SST_ID		0x8013
#define	WI_NIC_3842_MINI_ATM_ID		0x8014
#define	WI_NIC_3842_MINI_STR		"RF:PRISM2.5 MAC:ISL3874A(Mini-PCI)"

#define	WI_NIC_3842_PCI_AMD_ID		0x8016	/* Prism2.5 PCI-bridge */
#define	WI_NIC_3842_PCI_SST_ID		0x8017
#define	WI_NIC_3842_PCI_ATM_ID		0x8018
#define	WI_NIC_3842_PCI_STR		"RF:PRISM2.5 MAC:ISL3874A(PCI-bridge)"

#define	WI_NIC_P3_PCMCIA_AMD_ID		0x801A	/* Prism3 PCMCIA */
#define	WI_NIC_P3_PCMCIA_SST_ID		0x801B
#define	WI_NIC_P3_PCMCIA_STR		"RF:PRISM3 MAC:ISL3871(PCMCIA)"

#define	WI_NIC_P3_MINI_AMD_ID		0x8021	/* Prism3 Mini-PCI */
#define	WI_NIC_P3_MINI_SST_ID		0x8022
#define	WI_NIC_P3_MINI_STR		"RF:PRISM3 MAC:ISL3871(Mini-PCI)"

/*
 * List of intended regulatory domains (0xFD11).
 */
struct wi_domains {
	u_int16_t		wi_domains[6];
};

/*
 * CIS struct (0xFD13).
 */
struct wi_cis {
	u_int16_t		wi_cis[240];
};

/*
 * Communications quality (0xFD43).
 */
struct wi_commqual {
	u_int16_t		wi_coms_qual;
	u_int16_t		wi_sig_lvl;
	u_int16_t		wi_noise_lvl;
};

/*
 * Actual system scale thresholds (0xFC06, 0xFD46).
 */
struct wi_scalethresh {
	u_int16_t		wi_energy_detect;
	u_int16_t		wi_carrier_detect;
	u_int16_t		wi_defer;
	u_int16_t		wi_cell_search;
	u_int16_t		wi_out_of_range;
	u_int16_t		wi_delta_snr;
};

/*
 * PCF info struct (0xFD87).
 */
struct wi_pcf {
	u_int16_t		wi_medium_occupancy_limit;
	u_int16_t		wi_cfp_period;
	u_int16_t		wi_cfp_max_duration;
};

/*
 * Connection control characteristics. (0xFC00)
 * 0 == IBSS (802.11 compliant mode) (Only PRISM2)
 * 1 == Basic Service Set (BSS)
 * 2 == Wireless Distribution System (WDS)
 * 3 == Pseudo IBSS
 *	(Only PRISM2; not 802.11 compliant mode, testing use only)
 * 6 == HOST AP (Only PRISM2)
 */
#define	WI_PORTTYPE_IBSS	0x0
#define WI_PORTTYPE_BSS		0x1
#define WI_PORTTYPE_WDS		0x2
#define WI_PORTTYPE_ADHOC	0x3
#define	WI_PORTTYPE_HOSTAP	0x6

/*
 * Mac addresses. (0xFC01, 0xFC08, 0xFD42)
 */
struct wi_macaddr {
	union {
		u_int16_t	wi_mu_mac_addr_aligned[3];
		u_int8_t	wi_mu_mac_addr[6];
	} mu;
};
#define	wi_mac_addr_aligned	mu.wi_mu_mac_addr_aligned
#define	wi_mac_addr		mu.wi_mu_mac_addr

/*
 * Station set identification (SSID). (0xFC02, 0xFC04)
 */
struct wi_ssid {
	u_int16_t		wi_len;
	u_int8_t		wi_ssid[32];
};

/*
 * Set our station name. (0xFC0E)
 */
struct wi_nodename {
	u_int16_t		wi_nodelen;
	u_int8_t		wi_nodename[32];
};

/*
 * Multicast addresses to be put in filter. We're
 * allowed up to 16 addresses in the filter. (0xFC80)
 */
struct wi_mcast {
	struct ether_addr	wi_mcast[16];
};

/*
 * Join request. (0xFCE2)
 */
struct wi_joinreq {
	struct ether_addr	wi_bssid;
	u_int16_t		wi_chan;
};

/*
 * Information frame types.
 */
#define WI_INFO_NOTIFY		0xF000	/* Handover address */
#define WI_INFO_COUNTERS	0xF100	/* Statistics counters */
#define WI_INFO_SCAN_RESULTS	0xF101	/* Scan results */
#define WI_INFO_HOST_SCAN_RESULTS	0xF104	/* Scan results */
#define WI_INFO_LINK_STAT	0xF200	/* Link status */
#define	CONNECTED	1
#define	DISCONNECTED	2
#define	AP_CHANGE	3
#define	AP_OUT_OF_RANGE	4
#define	AP_IN_RANGE	5
#define	ASSOC_FAILED	6
#define WI_INFO_ASSOC_STAT	0xF201	/* Association status */
struct wi_assoc {
	u_int16_t		wi_assoc_stat;	/* Association Status */
#define	ASSOC		1
#define	REASSOC		2
#define	DISASSOC	3
#define	ASSOCFAIL	4
#define	AUTHFAIL	5
	u_int8_t		wi_assoc_sta[6];	/* Station Address */
	u_int8_t		wi_assoc_osta[6];	/* OLD Station Address */
	u_int16_t		wi_assoc_reason;	/* Reason */
	u_int16_t		wi_assoc_reserve;	/* Reserved */
};

#define	WI_INFO_AUTH_REQUEST	0xF202	/* Authentication Request (AP) */
#define	WI_INFO_POWERSAVE_COUNT	0xF203	/* PowerSave User Count (AP) */

/*
 * Scan Results of Prism2 chip
 */

#define MAXAPINFO		30
struct wi_scan_header {
	u_int16_t		wi_reserve;	/* future use */
	u_int16_t		wi_reason;	/* The reason this scan was initiated
						   1: Host initiated
						   2: Firmware initiated
						   3: Inquiry request from host */
};

struct wi_scan_data_p2 {
	u_int16_t		wi_chid;	/* BSS Channel ID from Probe Res.(PR)*/
	u_int16_t		wi_noise;	/* Average Noise Level of the PR */
	u_int16_t		wi_signal;	/* Signal Level on the PR */
	u_int8_t		wi_bssid[6];	/* MACaddress of BSS responder from PR */
	u_int16_t		wi_interval;	/* BSS beacon interval */
	u_int16_t		wi_capinfo;	/* BSS Capability Information
						   IEEE Std 802.11(1997) ,see 7.3.1.4 */
	u_int16_t		wi_namelen;	/* Length of SSID strings */
	u_int8_t		wi_name[32];	/* SSID strings */
	u_int16_t		wi_suprate[5];	/* Supported Rates element from the PR
						   IEEE Std 802.11(1997) ,see 7.3.2.2 */
	u_int16_t		wi_rate;	/* Data rate of the PR */
#define	WI_APRATE_1		0x0A		/* 1 Mbps */
#define	WI_APRATE_2		0x14		/* 2 Mbps */
#define	WI_APRATE_5		0x37		/* 5.5 Mbps */
#define	WI_APRATE_11		0x6E		/* 11 Mbps */
};

/*
 * Scan Results of Lucent chip
 */
struct wi_scan_data {
	u_int16_t		wi_chid;	/* BSS Channel ID from PR */
	u_int16_t		wi_noise;	/* Average Noise Level of the PR */
	u_int16_t		wi_signal;	/* Signal Level on the PR */
	u_int8_t		wi_bssid[6];	/* MACaddress of BSS responder from PR */
	u_int16_t		wi_interval;	/* BSS beacon interval */
	u_int16_t		wi_capinfo;	/* BSS Capability Information
						   IEEE Std 802.11(1997) ,see 7.3.1.4 */
	u_int16_t		wi_namelen;	/* Length of SSID strings */
	u_int8_t		wi_name[32];	/* SSID strings */
};

/*
 * transmit/receive frame structure
 */
struct wi_frame {
	u_int16_t		wi_status;	/* 0x00 */
	u_int16_t		wi_rx_tstamp1;	/* 0x02 */
	u_int16_t		wi_rx_tstamp0;	/* 0x04 */
	u_int8_t		wi_rx_silence;	/* 0x06 */
	u_int8_t		wi_rx_signal;	/* 0x07 */
	u_int8_t		wi_rx_rate;	/* 0x08 */
	u_int8_t		wi_rx_flow;	/* 0x09 */
	u_int8_t		wi_tx_rtry;	/* 0x0a */ /* Prism2 AP Only */
	u_int8_t		wi_tx_rate;	/* 0x0b */ /* Prism2 AP Only */
	u_int16_t		wi_tx_ctl;	/* 0x0c */
	struct ieee80211_frame_addr4 wi_whdr;	/* 0x0e */
	u_int16_t		wi_dat_len;	/* 0x2c */
	struct ether_header	wi_ehdr;	/* 0x2e */
} __packed;

/* Software support fields are returned untouched by TxOK, TxExc events. */
#define	wi_tx_swsup0		wi_rx_silence
#define	wi_tx_swsup1		wi_rx_signal
#define	wi_tx_swsup2		wi_rx_rate
#define	wi_tx_idx		wi_rx_flow

/* Tx Status Field */
#define	WI_TXSTAT_RET_ERR	0x0001
#define	WI_TXSTAT_AGED_ERR	0x0002
#define	WI_TXSTAT_DISCONNECT	0x0004
#define	WI_TXSTAT_FORM_ERR	0x0008

/* Rx Status Field */
#define WI_STAT_BADCRC		0x0001
#define WI_STAT_UNDECRYPTABLE	0x0002
#define WI_STAT_ERRSTAT		0x0003
#define WI_STAT_MAC_PORT	0x0700
#define	WI_STAT_PCF		0x1000
#define WI_RXSTAT_MSG_TYPE	0xE000
#define  WI_STAT_1042		0x2000	/* RFC1042 encoded */
#define  WI_STAT_TUNNEL		0x4000	/* Bridge-tunnel encoded */
#define  WI_STAT_WMP_MSG	0x6000	/* WaveLAN-II management protocol */
#define	 WI_STAT_MGMT		0x8000	/* 802.11b management frames */

#define WI_ENC_TX_E_II		0x0E

#define WI_ENC_TX_1042		0x00
#define WI_ENC_TX_TUNNEL	0xF8

/* TxControl Field (enhanced) */
#define	WI_TXCNTL_TX_OK		0x0002
#define	WI_TXCNTL_TX_EX		0x0004
#define	WI_TXCNTL_STRUCT_TYPE	0x0018
#define	 WI_ENC_TX_802_3	0x00
#define	 WI_ENC_TX_802_11	0x08
#define	WI_TXCNTL_ALTRTRY	0x0020
#define	WI_TXCNTL_NOCRYPT	0x0080

/* Limit on the number of AIDs handled in the PRISM2-based Host AP mode. */
#define WI_MAX_AID		256
