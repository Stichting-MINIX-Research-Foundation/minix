/*	$NetBSD: pcmciadevs_data.h,v 1.232 2012/08/23 10:39:25 kiyohara Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: pcmciadevs,v 1.230 2012/08/23 10:37:56 kiyohara Exp
 */
/* $FreeBSD: src/sys/dev/pccard/pccarddevs,v 1.20 2001/11/19 05:02:55 imp Exp $*/

/*-
 * Copyright (c) 1998, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

struct pcmcia_knowndev {
	int vendorid;
	int productid;
	struct pcmcia_knowndev_cis {
		char *vendor;
		char *product;
		char *version;
		char *revision;
	}cis;
	int flags;
	char *vendorname;
	char *devicename;
	int reserve;
};

#define	PCMCIA_CIS_INVALID		{ NULL, NULL, NULL, NULL }
#define	PCMCIA_KNOWNDEV_NOPROD		0

struct pcmcia_knowndev pcmcia_knowndevs[] = {
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CRWE737A,
	    PCMCIA_CIS_3COM_3CRWE737A,
	    0,
	    "3Com Corporation",
	    "3Com AirConnect Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXM056BNW,
	    PCMCIA_CIS_3COM_3CXM056BNW,
	    0,
	    "3Com Corporation",
	    "3Com/NoteWorthy 3CXM056-BNW 56K Modem",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556,
	    PCMCIA_CIS_3COM_3CXEM556,
	    0,
	    "3Com Corporation",
	    "3Com/Megahertz 3CXEM556 Ethernet/Modem",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556INT,
	    PCMCIA_CIS_3COM_3CXEM556INT,
	    0,
	    "3Com Corporation",
	    "3Com/Megahertz 3CXEM556-INT Ethernet/Modem",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CRWB6096,
	    PCMCIA_CIS_3COM_3CRWB6096,
	    0,
	    "3Com Corporation",
	    "3Com 3CRWB60-A Bluetooth PC Card",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CCFEM556BI,
	    PCMCIA_CIS_3COM_3CCFEM556BI,
	    0,
	    "3Com Corporation",
	    "3Com/Megahertz 3CCFEM556BI Ethernet/Modem",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3C562,
	    PCMCIA_CIS_3COM_3C562,
	    0,
	    "3Com Corporation",
	    "3Com 3c562 33.6 Modem/10Mbps Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3C589,
	    PCMCIA_CIS_3COM_3C589,
	    0,
	    "3Com Corporation",
	    "3Com 3c589 10Mbps Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3C574,
	    PCMCIA_CIS_3COM_3C574,
	    0,
	    "3Com Corporation",
	    "3Com 3c574-TX 10/100Mbps Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3C1,
	    PCMCIA_CIS_3COM_3C1,
	    0,
	    "3Com Corporation",
	    "3Com Megahertz 3C1 10Mbps LAN CF+ Card",	}
	,
	{
	    PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CRWE62092A,
	    PCMCIA_CIS_3COM_3CRWE62092A,
	    0,
	    "3Com Corporation",
	    "3Com 3CRWE62092A Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_ACTIONTEC, PCMCIA_PRODUCT_ACTIONTEC_PRISM,
	    PCMCIA_CIS_ACTIONTEC_PRISM,
	    0,
	    "ACTIONTEC",
	    "PRISM Wireless LAN PC Card",	}
	,
	{
	    PCMCIA_VENDOR_ADAPTEC, PCMCIA_PRODUCT_ADAPTEC_APA1460,
	    PCMCIA_CIS_ADAPTEC_APA1460,
	    0,
	    "Adaptec Corporation",
	    "Adaptec APA-1460 SlimSCSI",	}
	,
	{
	    PCMCIA_VENDOR_ADAPTEC, PCMCIA_PRODUCT_ADAPTEC_APA1460A,
	    PCMCIA_CIS_ADAPTEC_APA1460A,
	    0,
	    "Adaptec Corporation",
	    "Adaptec APA-1460A SlimSCSI",	}
	,
	{
	    PCMCIA_VENDOR_AIRONET, PCMCIA_PRODUCT_AIRONET_PC4500,
	    PCMCIA_CIS_AIRONET_PC4500,
	    0,
	    "Aironet Wireless Communications",
	    "Aironet PC4500 Wireless LAN Adapter",	}
	,
	{
	    PCMCIA_VENDOR_AIRONET, PCMCIA_PRODUCT_AIRONET_PC4800,
	    PCMCIA_CIS_AIRONET_PC4800,
	    0,
	    "Aironet Wireless Communications",
	    "Aironet PC4800 Wireless LAN Adapter",	}
	,
	{
	    PCMCIA_VENDOR_AIRONET, PCMCIA_PRODUCT_AIRONET_350,
	    PCMCIA_CIS_AIRONET_350,
	    0,
	    "Aironet Wireless Communications",
	    "Aironet 350 Wireless LAN Adapter",	}
	,
	{
	    PCMCIA_VENDOR_ALLIEDTELESIS, PCMCIA_PRODUCT_ALLIEDTELESIS_LA_PCM,
	    PCMCIA_CIS_ALLIEDTELESIS_LA_PCM,
	    0,
	    "Allied Telesis K.K.",
	    "Allied Telesis LA-PCM",	}
	,
	{
	    PCMCIA_VENDOR_ALVARION, PCMCIA_PRODUCT_ALVARION_BREEZENET,
	    PCMCIA_CIS_ALVARION_BREEZENET,
	    0,
	    "Alvarion Ltd.",
	    "Alvarion Ltd. BreezeNET PC-DS.11b",	}
	,
	{
	    PCMCIA_VENDOR_AMBICOM, PCMCIA_PRODUCT_AMBICOM_WL54CF,
	    PCMCIA_CIS_AMBICOM_WL54CF,
	    0,
	    "AmbiCom Inc",
	    "AmbiCom WL54-CF",	}
	,
	{
	    PCMCIA_VENDOR_ANYCOM, PCMCIA_PRODUCT_ANYCOM_LSE041,
	    PCMCIA_CIS_ANYCOM_LSE041,
	    0,
	    "Anycom",
	    "AnyCom BlueCard LSE041 R1B",	}
	,
	{
	    PCMCIA_VENDOR_ANYCOM, PCMCIA_PRODUCT_ANYCOM_LSE039,
	    PCMCIA_CIS_ANYCOM_LSE039,
	    0,
	    "Anycom",
	    "Anycom Bluetooth CF Card LSE039",	}
	,
	{
	    PCMCIA_VENDOR_ANYCOM, PCMCIA_PRODUCT_ANYCOM_LSE139,
	    PCMCIA_CIS_ANYCOM_LSE139,
	    0,
	    "Anycom",
	    "Anycom Bluetooth CF Card LSE139",	}
	,
	{
	    PCMCIA_VENDOR_ARCHOS, PCMCIA_PRODUCT_ARCHOS_ARC_ATAPI,
	    PCMCIA_CIS_ARCHOS_ARC_ATAPI,
	    0,
	    "Archos",
	    "MiniCD",	}
	,
	{
	    PCMCIA_VENDOR_ARTEM, PCMCIA_PRODUCT_ARTEM_ONAIR,
	    PCMCIA_CIS_ARTEM_ONAIR,
	    0,
	    "ARtem",
	    "ARtem Onair Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_ASUSTEK, PCMCIA_PRODUCT_ASUSTEK_WL_100,
	    PCMCIA_CIS_ASUSTEK_WL_100,
	    0,
	    "Asustek Computer",
	    "SpaceLink WL-100 Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_BAY, PCMCIA_PRODUCT_BAY_STACK_650,
	    PCMCIA_CIS_BAY_STACK_650,
	    0,
	    "Bay Networks",
	    "BayStack 650 Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_BAY, PCMCIA_PRODUCT_BAY_SURFER_PRO,
	    PCMCIA_CIS_BAY_SURFER_PRO,
	    0,
	    "Bay Networks",
	    "AirSurfer Pro Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_BAY, PCMCIA_PRODUCT_BAY_STACK_660,
	    PCMCIA_CIS_BAY_STACK_660,
	    0,
	    "Bay Networks",
	    "BayStack 660 Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_BAY, PCMCIA_PRODUCT_BAY_EMOBILITY_11B,
	    PCMCIA_CIS_BAY_EMOBILITY_11B,
	    0,
	    "Bay Networks",
	    "e-Mobility 11Mb Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_BREEZECOM, PCMCIA_PRODUCT_BREEZECOM_BREEZENET,
	    PCMCIA_CIS_BREEZECOM_BREEZENET,
	    0,
	    "BreezeCOM",
	    "BreezeCOM BreezeNET",	}
	,
	{
	    PCMCIA_VENDOR_BELKIN, PCMCIA_PRODUCT_BELKIN_F5D5020,
	    PCMCIA_CIS_BELKIN_F5D5020,
	    0,
	    "Belkin",
	    "Belkin F5D5020 PCMCIA Network Card",	}
	,
	{
	    PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_WLI_PCM_S11,
	    PCMCIA_CIS_BUFFALO_WLI_PCM_S11,
	    0,
	    "BUFFALO (Melco Corporation)",
	    "BUFFALO AirStation 11Mbps WLAN",	}
	,
	{
	    PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_LPC_CF_CLT,
	    PCMCIA_CIS_BUFFALO_LPC_CF_CLT,
	    0,
	    "BUFFALO (Melco Corporation)",
	    "BUFFALO LPC-CF-CLT Ethernet Adapter",	}
	,
	{
	    PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_LPC4_CLX,
	    PCMCIA_CIS_BUFFALO_LPC4_CLX,
	    0,
	    "BUFFALO (Melco Corporation)",
	    "BUFFALO LPC4-CLX Ethernet Adapter",	}
	,
	{
	    PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_LPC3_CLT,
	    PCMCIA_CIS_BUFFALO_LPC3_CLT,
	    0,
	    "BUFFALO (Melco Corporation)",
	    "BUFFALO LPC3-CLT Ethernet Adapter",	}
	,
	{
	    PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_WLI_CF_S11G,
	    PCMCIA_CIS_BUFFALO_WLI_CF_S11G,
	    0,
	    "BUFFALO (Melco Corporation)",
	    "BUFFALO AirStation 11Mbps CF WLAN",	}
	,
	{
	    PCMCIA_VENDOR_COMPAQ, PCMCIA_PRODUCT_COMPAQ_NC5004,
	    PCMCIA_CIS_COMPAQ_NC5004,
	    0,
	    "Compaq",
	    "Compaq Agency NC5004 Wireless Card",	}
	,
	{
	    PCMCIA_VENDOR_COMPAQ2, PCMCIA_PRODUCT_COMPAQ2_CPQ_10_100,
	    PCMCIA_CIS_COMPAQ2_CPQ_10_100,
	    0,
	    "Compaq",
	    "Compaq Netelligent 10/100 Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_COMPEX, PCMCIA_PRODUCT_COMPEX_AMP_WIRELESS,
	    PCMCIA_CIS_COMPEX_AMP_WIRELESS,
	    0,
	    "Compex Corporation",
	    "AMP",	}
	,
	{
	    PCMCIA_VENDOR_COMPEX, PCMCIA_PRODUCT_COMPEX_LINKPORT_ENET_B,
	    PCMCIA_CIS_COMPEX_LINKPORT_ENET_B,
	    0,
	    "Compex Corporation",
	    "Compex Linkport ENET-B Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_CONTEC, PCMCIA_PRODUCT_CONTEC_CNETPC,
	    PCMCIA_CIS_CONTEC_CNETPC,
	    0,
	    "Contec",
	    "Contec C-NET(PC)C",	}
	,
	{
	    PCMCIA_VENDOR_CONTEC, PCMCIA_PRODUCT_CONTEC_FX_DS110_PCC,
	    PCMCIA_CIS_CONTEC_FX_DS110_PCC,
	    0,
	    "Contec",
	    "Contec FLEXLAN/FX-DS110-PCC",	}
	,
	{
	    PCMCIA_VENDOR_DAYNA, PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_1,
	    PCMCIA_CIS_DAYNA_COMMUNICARD_E_1,
	    0,
	    "Dayna Corporation",
	    "Dayna CommuniCard E",	}
	,
	{
	    PCMCIA_VENDOR_DAYNA, PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_2,
	    PCMCIA_CIS_DAYNA_COMMUNICARD_E_2,
	    0,
	    "Dayna Corporation",
	    "Dayna CommuniCard E",	}
	,
	{
	    PCMCIA_VENDOR_DIGITAL, PCMCIA_PRODUCT_DIGITAL_MOBILE_MEDIA_CDROM,
	    PCMCIA_CIS_DIGITAL_MOBILE_MEDIA_CDROM,
	    0,
	    "Digital Equipment Corporation",
	    "Digital Mobile Media CD-ROM",	}
	,
	{
	    PCMCIA_VENDOR_DLINK_2, PCMCIA_PRODUCT_DLINK_2_DMF560TX,
	    PCMCIA_CIS_DLINK_2_DMF560TX,
	    0,
	    "D-Link",
	    "D-Link DMF-650TX",	}
	,
	{
	    PCMCIA_VENDOR_DUAL, PCMCIA_PRODUCT_DUAL_NE2000,
	    PCMCIA_CIS_DUAL_NE2000,
	    0,
	    "Dual",
	    "Dual NE2000",	}
	,
	{
	    PCMCIA_VENDOR_EDIMAX, PCMCIA_PRODUCT_EDIMAX_EP4000A,
	    PCMCIA_CIS_EDIMAX_EP4000A,
	    0,
	    "Edimax Technology Inc.",
	    "Edimax EP4000A",	}
	,
	{
	    PCMCIA_VENDOR_EDIMAX, PCMCIA_PRODUCT_EDIMAX_EP4101,
	    PCMCIA_CIS_EDIMAX_EP4101,
	    0,
	    "Edimax Technology Inc.",
	    "Edimax EP4101",	}
	,
	{
	    PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_MC2_IEEE,
	    PCMCIA_CIS_ELSA_MC2_IEEE,
	    0,
	    "Elsa",
	    "AirLancer MC-2 IEEE",	}
	,
	{
	    PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_XI300_IEEE,
	    PCMCIA_CIS_ELSA_XI300_IEEE,
	    0,
	    "Elsa",
	    "XI300 Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_XI800_IEEE,
	    PCMCIA_CIS_ELSA_XI800_IEEE,
	    0,
	    "Elsa",
	    "XI800 CF Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_XI325_IEEE,
	    PCMCIA_CIS_ELSA_XI325_IEEE,
	    0,
	    "Elsa",
	    "XI325 Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_ELSA, PCMCIA_PRODUCT_ELSA_SMC2531WB,
	    PCMCIA_CIS_ELSA_SMC2531WB,
	    0,
	    "Elsa",
	    "SMC2531W-B EliteConnect Wireless Adapter",	}
	,
	{
	    PCMCIA_VENDOR_EMTAC, PCMCIA_PRODUCT_EMTAC_WLAN,
	    PCMCIA_CIS_EMTAC_WLAN,
	    0,
	    "EMTAC Technology Corporation",
	    "EMTAC A2424i 11Mbps WLAN Card",	}
	,
	{
	    PCMCIA_VENDOR_ERICSSON, PCMCIA_PRODUCT_ERICSSON_WIRELESSLAN,
	    PCMCIA_CIS_ERICSSON_WIRELESSLAN,
	    0,
	    "Ericsson",
	    "DSSS Wireless LAN PC Card",	}
	,
	{
	    PCMCIA_VENDOR_FARALLON, PCMCIA_PRODUCT_FARALLON_SKYLINE,
	    PCMCIA_CIS_FARALLON_SKYLINE,
	    0,
	    "Farallon Communications",
	    "SkyLINE Wireless",	}
	,
	{
	    PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_SCSI600,
	    PCMCIA_CIS_FUJITSU_SCSI600,
	    0,
	    "Fujitsu Corporation",
	    "Fujitsu SCSI 600 Interface",	}
	,
	{
	    PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_LA10S,
	    PCMCIA_CIS_FUJITSU_LA10S,
	    0,
	    "Fujitsu Corporation",
	    "Fujitsu Compact Flash Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_LA501,
	    PCMCIA_CIS_FUJITSU_LA501,
	    0,
	    "Fujitsu Corporation",
	    "Fujitsu Towa LA501 Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_WL110,
	    PCMCIA_CIS_FUJITSU_WL110,
	    0,
	    "Fujitsu Corporation",
	    "PEGA-WL110 Wireless LAN",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_MICRODRIVE,
	    PCMCIA_CIS_IBM_MICRODRIVE,
	    0,
	    "IBM Corporation",
	    "IBM Microdrive",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_3270,
	    PCMCIA_CIS_IBM_3270,
	    0,
	    "IBM Corporation",
	    "IBM 3270 Emulation",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_INFOMOVER,
	    PCMCIA_CIS_IBM_INFOMOVER,
	    0,
	    "IBM Corporation",
	    "IBM InfoMover",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_5250,
	    PCMCIA_CIS_IBM_5250,
	    0,
	    "IBM Corporation",
	    "IBM 5250 Emulation",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_TROPIC,
	    PCMCIA_CIS_IBM_TROPIC,
	    0,
	    "IBM Corporation",
	    "IBM Token Ring 4/16",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_SCSI,
	    PCMCIA_CIS_IBM_SCSI,
	    0,
	    "IBM Corporation",
	    "IBM SCSI PCMCIA",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_PORTABLE_CDROM,
	    PCMCIA_CIS_IBM_PORTABLE_CDROM,
	    0,
	    "IBM Corporation",
	    "IBM PCMCIA Portable CD-ROM Drive",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_HOME_AND_AWAY,
	    PCMCIA_CIS_IBM_HOME_AND_AWAY,
	    0,
	    "IBM Corporation",
	    "IBM Home and Away Modem",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_WIRELESS_LAN_ENTRY,
	    PCMCIA_CIS_IBM_WIRELESS_LAN_ENTRY,
	    0,
	    "IBM Corporation",
	    "IBM Wireless LAN Entry",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_SMART_CAPTURE_II,
	    PCMCIA_CIS_IBM_SMART_CAPTURE_II,
	    0,
	    "IBM Corporation",
	    "IBM Smart Capture Card II",	}
	,
	{
	    PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_ETHERJET,
	    PCMCIA_CIS_IBM_ETHERJET,
	    0,
	    "IBM Corporation",
	    "IBM EtherJet Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_INTEL, PCMCIA_PRODUCT_INTEL_PRO_WLAN_2011,
	    PCMCIA_CIS_INTEL_PRO_WLAN_2011,
	    0,
	    "Intel",
	    "Intel PRO/Wireless 2011 LAN PC Card",	}
	,
	{
	    PCMCIA_VENDOR_INTEL, PCMCIA_PRODUCT_INTEL_EEPRO100,
	    PCMCIA_CIS_INTEL_EEPRO100,
	    0,
	    "Intel",
	    "Intel EtherExpress PRO/100",	}
	,
	{
	    PCMCIA_VENDOR_INTERSIL, PCMCIA_PRODUCT_INTERSIL_ISL37100P,
	    PCMCIA_CIS_INTERSIL_ISL37100P,
	    0,
	    "Netgear",
	    "Intersil ISL37100P",	}
	,
	{
	    PCMCIA_VENDOR_INTERSIL, PCMCIA_PRODUCT_INTERSIL_ISL37110P,
	    PCMCIA_CIS_INTERSIL_ISL37110P,
	    0,
	    "Netgear",
	    "Intersil ISL37110P",	}
	,
	{
	    PCMCIA_VENDOR_INTERSIL, PCMCIA_PRODUCT_INTERSIL_ISL37300P,
	    PCMCIA_CIS_INTERSIL_ISL37300P,
	    0,
	    "Netgear",
	    "Intersil ISL37300P",	}
	,
	{
	    PCMCIA_VENDOR_IODATA, PCMCIA_PRODUCT_IODATA_PCLATE,
	    PCMCIA_CIS_IODATA_PCLATE,
	    0,
	    "I-O DATA",
	    "I-O DATA PCLA/TE",	}
	,
	{
	    PCMCIA_VENDOR_IODATA2, PCMCIA_PRODUCT_IODATA2_WNB11PCM,
	    PCMCIA_CIS_IODATA2_WNB11PCM,
	    0,
	    "I-O DATA",
	    "I-O DATA WN-B11/PCM",	}
	,
	{
	    PCMCIA_VENDOR_IODATA2, PCMCIA_PRODUCT_IODATA2_WCF12,
	    PCMCIA_CIS_IODATA2_WCF12,
	    0,
	    "I-O DATA",
	    "Wireless CF Card",	}
	,
	{
	    PCMCIA_VENDOR_IODATA3, PCMCIA_PRODUCT_IODATA3_PCETTXR,
	    PCMCIA_CIS_IODATA3_PCETTXR,
	    0,
	    "I-O DATA",
	    "I-O DATA PCET/TX-R",	}
	,
	{
	    PCMCIA_VENDOR_KINGSTON, PCMCIA_PRODUCT_KINGSTON_KNE2,
	    PCMCIA_CIS_KINGSTON_KNE2,
	    0,
	    "Kingston",
	    "Kingston KNE-PC2 Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_LASAT, PCMCIA_PRODUCT_LASAT_CREDIT_288,
	    PCMCIA_CIS_LASAT_CREDIT_288,
	    0,
	    "Lasat Communications A/S",
	    "Lasat Credit 288 Modem",	}
	,
	{
	    PCMCIA_VENDOR_LEXARMEDIA, PCMCIA_PRODUCT_LEXARMEDIA_COMPACTFLASH,
	    PCMCIA_CIS_LEXARMEDIA_COMPACTFLASH,
	    0,
	    "Lexar Media",
	    "Lexar Media CompactFlash",	}
	,
	{
	    PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_TRUST_COMBO_ECARD,
	    PCMCIA_CIS_LINKSYS_TRUST_COMBO_ECARD,
	    0,
	    "Linksys Corporation",
	    "Trust (Linksys) Combo EthernetCard",	}
	,
	{
	    PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ETHERFAST,
	    PCMCIA_CIS_LINKSYS_ETHERFAST,
	    0,
	    "Linksys Corporation",
	    "Linksys Etherfast 10/100 Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ECARD_1,
	    PCMCIA_CIS_LINKSYS_ECARD_1,
	    0,
	    "Linksys Corporation",
	    "Linksys EthernetCard or D-Link DE-650",	}
	,
	{
	    PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
	    PCMCIA_CIS_LINKSYS_COMBO_ECARD,
	    0,
	    "Linksys Corporation",
	    "Linksys Combo EthernetCard",	}
	,
	{
	    PCMCIA_VENDOR_LINKSYS2, PCMCIA_PRODUCT_LINKSYS2_IWN,
	    PCMCIA_CIS_LINKSYS2_IWN,
	    0,
	    "The Linksys Group",
	    "Instant Wireless Network PC Card",	}
	,
	{
	    PCMCIA_VENDOR_LINKSYS2, PCMCIA_PRODUCT_LINKSYS2_IWN3,
	    PCMCIA_CIS_LINKSYS2_IWN3,
	    0,
	    "The Linksys Group",
	    "Instant Wireless Network PC Card, Versin 3",	}
	,
	{
	    PCMCIA_VENDOR_LINKSYS2, PCMCIA_PRODUCT_LINKSYS2_WCF11,
	    PCMCIA_CIS_LINKSYS2_WCF11,
	    0,
	    "The Linksys Group",
	    "Instant Wireless Network CF Card",	}
	,
	{
	    PCMCIA_VENDOR_LUCENT, PCMCIA_PRODUCT_LUCENT_HERMES,
	    PCMCIA_CIS_LUCENT_HERMES,
	    0,
	    "Lucent Technologies",
	    "Hermes",	}
	,
	{
	    PCMCIA_VENDOR_LUCENT, PCMCIA_PRODUCT_LUCENT_HERMES2,
	    PCMCIA_CIS_LUCENT_HERMES2,
	    0,
	    "Lucent Technologies",
	    "Hermes II",	}
	,
	{
	    PCMCIA_VENDOR_LUCENT, PCMCIA_PRODUCT_LUCENT_HERMES25,
	    PCMCIA_CIS_LUCENT_HERMES25,
	    0,
	    "Lucent Technologies",
	    "Hermes II.5",	}
	,
	{
	    PCMCIA_VENDOR_MACNICA, PCMCIA_PRODUCT_MACNICA_ME1_JEIDA,
	    PCMCIA_CIS_MACNICA_ME1_JEIDA,
	    0,
	    "MACNICA",
	    "MACNICA ME1 for JEIDA",	}
	,
	{
	    PCMCIA_VENDOR_MEGAHERTZ, PCMCIA_PRODUCT_MEGAHERTZ_EM3336,
	    PCMCIA_CIS_MEGAHERTZ_EM3336,
	    0,
	    "Megahertz Corporation",
	    "Megahertz Ethernet+Modem 33.6k",	}
	,
	{
	    PCMCIA_VENDOR_MEGAHERTZ, PCMCIA_PRODUCT_MEGAHERTZ_XJ4288,
	    PCMCIA_CIS_MEGAHERTZ_XJ4288,
	    0,
	    "Megahertz Corporation",
	    "Megahertz XJ4288 Modem",	}
	,
	{
	    PCMCIA_VENDOR_MEGAHERTZ, PCMCIA_PRODUCT_MEGAHERTZ_XJ4336,
	    PCMCIA_CIS_MEGAHERTZ_XJ4336,
	    0,
	    "Megahertz Corporation",
	    "Megahertz XJ4336 Modem",	}
	,
	{
	    PCMCIA_VENDOR_MEGAHERTZ, PCMCIA_PRODUCT_MEGAHERTZ_XJ5560,
	    PCMCIA_CIS_MEGAHERTZ_XJ5560,
	    0,
	    "Megahertz Corporation",
	    "Megahertz X-JACK 56kbps Modem",	}
	,
	{
	    PCMCIA_VENDOR_MEGAHERTZ2, PCMCIA_PRODUCT_MEGAHERTZ2_EM1144,
	    PCMCIA_CIS_MEGAHERTZ2_EM1144,
	    0,
	    "Megahertz Corporation",
	    "Megahertz Ethernet+Modem 14.4k",	}
	,
	{
	    PCMCIA_VENDOR_MEGAHERTZ2, PCMCIA_PRODUCT_MEGAHERTZ2_XJACK,
	    PCMCIA_CIS_MEGAHERTZ2_XJACK,
	    0,
	    "Megahertz Corporation",
	    "Megahertz X-JACK Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_MELCO, PCMCIA_PRODUCT_MELCO_LPC3_TX,
	    PCMCIA_CIS_MELCO_LPC3_TX,
	    0,
	    "Melco Corporation",
	    "Melco LPC3-TX",	}
	,
	{
	    PCMCIA_VENDOR_MICROSOFT, PCMCIA_PRODUCT_MICROSOFT_MN_520,
	    PCMCIA_CIS_MICROSOFT_MN_520,
	    0,
	    "Microsoft Corporation",
	    "Microsoft MN-520 WLAN Card",	}
	,
	{
	    PCMCIA_VENDOR_MOTOROLA, PCMCIA_PRODUCT_MOTOROLA_POWER144,
	    PCMCIA_CIS_MOTOROLA_POWER144,
	    0,
	    "Motorola Corporation",
	    "Motorola Power 14.4 Modem",	}
	,
	{
	    PCMCIA_VENDOR_MOTOROLA, PCMCIA_PRODUCT_MOTOROLA_PM100C,
	    PCMCIA_CIS_MOTOROLA_PM100C,
	    0,
	    "Motorola Corporation",
	    "Motorola Personal Messenger 100C CDPD Modem",	}
	,
	{
	    PCMCIA_VENDOR_MOTOROLA, PCMCIA_PRODUCT_MOTOROLA_MONTANA_336,
	    PCMCIA_CIS_MOTOROLA_MONTANA_336,
	    0,
	    "Motorola Corporation",
	    "Motorola Montana 33.6",	}
	,
	{
	    PCMCIA_VENDOR_NECINFRONTIA, PCMCIA_PRODUCT_NECINFRONTIA_AX420N,
	    PCMCIA_CIS_NECINFRONTIA_AX420N,
	    0,
	    "NEC Infrontia",
	    "NEC Infrontia AX420N AirH Modem",	}
	,
	{
	    PCMCIA_VENDOR_NEWMEDIA, PCMCIA_PRODUCT_NEWMEDIA_BASICS,
	    PCMCIA_CIS_NEWMEDIA_BASICS,
	    0,
	    "New Media Corporation",
	    "New Media BASICS Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_NEWMEDIA, PCMCIA_PRODUCT_NEWMEDIA_LANSURFER,
	    PCMCIA_CIS_NEWMEDIA_LANSURFER,
	    0,
	    "New Media Corporation",
	    "NewMedia LANSurfer",	}
	,
	{
	    PCMCIA_VENDOR_NEWMEDIA, PCMCIA_PRODUCT_NEWMEDIA_LIVEWIRE,
	    PCMCIA_CIS_NEWMEDIA_LIVEWIRE,
	    0,
	    "New Media Corporation",
	    "NewMedia LiveWire Ethernet LAN Adapter",	}
	,
	{
	    PCMCIA_VENDOR_NEWMEDIA, PCMCIA_PRODUCT_NEWMEDIA_MULTIMEDIA,
	    PCMCIA_CIS_NEWMEDIA_MULTIMEDIA,
	    0,
	    "New Media Corporation",
	    "NewMedia Multimedia",	}
	,
	{
	    PCMCIA_VENDOR_NEWMEDIA, PCMCIA_PRODUCT_NEWMEDIA_BUSTOASTER,
	    PCMCIA_CIS_NEWMEDIA_BUSTOASTER,
	    0,
	    "New Media Corporation",
	    "New Media BusToaster SCSI Host Adapter",	}
	,
	{
	    PCMCIA_VENDOR_NEWMEDIA, PCMCIA_PRODUCT_NEWMEDIA_WAVJAMMER,
	    PCMCIA_CIS_NEWMEDIA_WAVJAMMER,
	    0,
	    "New Media Corporation",
	    "NewMedia .WAVjammer",	}
	,
	{
	    PCMCIA_VENDOR_NEWMEDIA2, PCMCIA_PRODUCT_NEWMEDIA2_BUSTOASTER,
	    PCMCIA_CIS_NEWMEDIA2_BUSTOASTER,
	    0,
	    "NewMedia",
	    "NewMedia BusToaster",	}
	,
	{
	    PCMCIA_VENDOR_NETGEAR, PCMCIA_PRODUCT_NETGEAR_FA410TX,
	    PCMCIA_CIS_NETGEAR_FA410TX,
	    0,
	    "Netgear",
	    "Netgear FA410TX",	}
	,
	{
	    PCMCIA_VENDOR_NETGEAR, PCMCIA_PRODUCT_NETGEAR_FA410TXC,
	    PCMCIA_CIS_NETGEAR_FA410TXC,
	    0,
	    "Netgear",
	    "Netgear FA410TXC",	}
	,
	{
	    PCMCIA_VENDOR_NETGEAR, PCMCIA_PRODUCT_NETGEAR_FA411,
	    PCMCIA_CIS_NETGEAR_FA411,
	    0,
	    "Netgear",
	    "Netgear FA411",	}
	,
	{
	    PCMCIA_VENDOR_NI, PCMCIA_PRODUCT_NI_PCMCIA_GPIB,
	    PCMCIA_CIS_NI_PCMCIA_GPIB,
	    0,
	    "National Instruments",
	    "National Instruments PCMCIA-GPIB",	}
	,
	{
	    PCMCIA_VENDOR_NEXTCOM, PCMCIA_PRODUCT_NEXTCOM_NEXTHAWK,
	    PCMCIA_CIS_NEXTCOM_NEXTHAWK,
	    0,
	    "NextCom K.K.",
	    "Next Hawk Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_NOKIA, PCMCIA_PRODUCT_NOKIA_C020_WLAN,
	    PCMCIA_CIS_NOKIA_C020_WLAN,
	    0,
	    "Nokia Communications",
	    "Nokia C020 WLAN Card",	}
	,
	{
	    PCMCIA_VENDOR_OLICOM, PCMCIA_PRODUCT_OLICOM_OC2220,
	    PCMCIA_CIS_OLICOM_OC2220,
	    0,
	    "Olicom",
	    "GoCard Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_OLICOM, PCMCIA_PRODUCT_OLICOM_TR,
	    PCMCIA_CIS_OLICOM_TR,
	    0,
	    "Olicom",
	    "GoCard Token Ring 16/4",	}
	,
	{
	    PCMCIA_VENDOR_OLICOM, PCMCIA_PRODUCT_OLICOM_OC2231,
	    PCMCIA_CIS_OLICOM_OC2231,
	    0,
	    "Olicom",
	    "GoCard Combo Eth/Modem 288",	}
	,
	{
	    PCMCIA_VENDOR_OLICOM, PCMCIA_PRODUCT_OLICOM_OC3231,
	    PCMCIA_CIS_OLICOM_OC3231,
	    0,
	    "Olicom",
	    "GoCard Token Ring 16/4/Modem 288",	}
	,
	{
	    PCMCIA_VENDOR_OLICOM, PCMCIA_PRODUCT_OLICOM_OC2232,
	    PCMCIA_CIS_OLICOM_OC2232,
	    0,
	    "Olicom",
	    "GoCard Combo Eth/Modem 336",	}
	,
	{
	    PCMCIA_VENDOR_OLICOM, PCMCIA_PRODUCT_OLICOM_OC3232,
	    PCMCIA_CIS_OLICOM_OC3232,
	    0,
	    "Olicom",
	    "GoCard Token Ring 16/4/Modem 336",	}
	,
	{
	    PCMCIA_VENDOR_OSITECH, PCMCIA_PRODUCT_OSITECH_TRUMPCARD_SOD,
	    PCMCIA_CIS_OSITECH_TRUMPCARD_SOD,
	    0,
	    "Ositech",
	    "Ositech Seven of Diamonds Ethernet Card",	}
	,
	{
	    PCMCIA_VENDOR_PANASONIC, PCMCIA_PRODUCT_PANASONIC_KXLC002,
	    PCMCIA_CIS_PANASONIC_KXLC002,
	    0,
	    "Matsushita Electric Industrial Co.",
	    "Panasonic 4X CD-ROM Interface Card",	}
	,
	{
	    PCMCIA_VENDOR_PANASONIC, PCMCIA_PRODUCT_PANASONIC_KXLC003,
	    PCMCIA_CIS_PANASONIC_KXLC003,
	    0,
	    "Matsushita Electric Industrial Co.",
	    "Panasonic 8X CD-ROM Interface Card",	}
	,
	{
	    PCMCIA_VENDOR_PANASONIC, PCMCIA_PRODUCT_PANASONIC_KXLC005,
	    PCMCIA_CIS_PANASONIC_KXLC005,
	    0,
	    "Matsushita Electric Industrial Co.",
	    "Panasonic 16X CD-ROM Interface Card",	}
	,
	{
	    PCMCIA_VENDOR_PANASONIC, PCMCIA_PRODUCT_PANASONIC_KME,
	    PCMCIA_CIS_PANASONIC_KME,
	    0,
	    "Matsushita Electric Industrial Co.",
	    "Panasonic CD-R/RW Interface",	}
	,
	{
	    PCMCIA_VENDOR_PROXIM, PCMCIA_PRODUCT_PROXIM_ROAMABOUT_2400FH,
	    PCMCIA_CIS_PROXIM_ROAMABOUT_2400FH,
	    0,
	    "Proxim",
	    "Digital RoamAbout 2400FH",	}
	,
	{
	    PCMCIA_VENDOR_PROXIM, PCMCIA_PRODUCT_PROXIM_RANGELAN2_7401,
	    PCMCIA_CIS_PROXIM_RANGELAN2_7401,
	    0,
	    "Proxim",
	    "Proxim RangeLAN2 7401",	}
	,
	{
	    PCMCIA_VENDOR_PROXIM, PCMCIA_PRODUCT_PROXIM_RANGELANDS_8430,
	    PCMCIA_CIS_PROXIM_RANGELANDS_8430,
	    0,
	    "Proxim",
	    "Proxim RangeLAN-DS 8430",	}
	,
	{
	    PCMCIA_VENDOR_PSION, PCMCIA_PRODUCT_PSION_GOLDCARD,
	    PCMCIA_CIS_PSION_GOLDCARD,
	    0,
	    "Psion",
	    "Psion Gold Card",	}
	,
	{
	    PCMCIA_VENDOR_QUATECH, PCMCIA_PRODUCT_QUATECH_SPP_100,
	    PCMCIA_CIS_QUATECH_SPP_100,
	    0,
	    "Quatech",
	    "Quatech Enhanced Parallel Port",	}
	,
	{
	    PCMCIA_VENDOR_QUATECH, PCMCIA_PRODUCT_QUATECH_DSP_225,
	    PCMCIA_CIS_QUATECH_DSP_225,
	    0,
	    "Quatech",
	    "Quatech Dual Serial Port",	}
	,
	{
	    PCMCIA_VENDOR_RAYTHEON, PCMCIA_PRODUCT_RAYTHEON_WLAN,
	    PCMCIA_CIS_RAYTHEON_WLAN,
	    0,
	    "Raytheon",
	    "WLAN Adapter",	}
	,
	{
	    PCMCIA_VENDOR_ROLAND, PCMCIA_PRODUCT_ROLAND_SCP55,
	    PCMCIA_CIS_ROLAND_SCP55,
	    0,
	    "Roland",
	    "Roland SCP-55",	}
	,
	{
	    PCMCIA_VENDOR_SAMSUNG, PCMCIA_PRODUCT_SAMSUNG_SWL_2000N,
	    PCMCIA_CIS_SAMSUNG_SWL_2000N,
	    0,
	    "Samsung",
	    "Samsung MagicLAN SWL-2000N",	}
	,
	{
	    PCMCIA_VENDOR_SANDISK, PCMCIA_PRODUCT_SANDISK_SDCFB,
	    PCMCIA_CIS_SANDISK_SDCFB,
	    0,
	    "Sandisk Corporation",
	    "Sandisk CompactFlash Card",	}
	,
	{
	    PCMCIA_VENDOR_SHARP, PCMCIA_PRODUCT_SHARP_PALDIO611S,
	    PCMCIA_CIS_SHARP_PALDIO611S,
	    0,
	    "Sharp Corporation",
	    "PALDIO 611S PC CARD",	}
	,
	{
	    PCMCIA_VENDOR_SIERRA, PCMCIA_PRODUCT_SIERRA_AC850,
	    PCMCIA_CIS_SIERRA_AC850,
	    0,
	    "Sierra Wireless",
	    "AC850",	}
	,
	{
	    PCMCIA_VENDOR_SIMPLETECH, PCMCIA_PRODUCT_SIMPLETECH_COMMUNICATOR288,
	    PCMCIA_CIS_SIMPLETECH_COMMUNICATOR288,
	    0,
	    "Simple Technology",
	    "Simple Technology 28.8 Communicator",	}
	,
	{
	    PCMCIA_VENDOR_SIMPLETECH, PCMCIA_PRODUCT_SIMPLETECH_SPECTRUM24,
	    PCMCIA_CIS_SIMPLETECH_SPECTRUM24,
	    0,
	    "Simple Technology",
	    "Symbol Spectrum24 WLAN Adapter",	}
	,
	{
	    PCMCIA_VENDOR_SMC, PCMCIA_PRODUCT_SMC_8016,
	    PCMCIA_CIS_SMC_8016,
	    0,
	    "Standard Microsystems Corporation",
	    "SMC 8016 EtherCard",	}
	,
	{
	    PCMCIA_VENDOR_SMC, PCMCIA_PRODUCT_SMC_EZCARD,
	    PCMCIA_CIS_SMC_EZCARD,
	    0,
	    "Standard Microsystems Corporation",
	    "SMC EZCard 10 PCMCIA",	}
	,
	{
	    PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_EA_ETHER,
	    PCMCIA_CIS_SOCKET_EA_ETHER,
	    0,
	    "Socket Communications",
	    "Socket Communications EA",	}
	,
	{
	    PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_WLAN_CF,
	    PCMCIA_CIS_SOCKET_LP_WLAN_CF,
	    0,
	    "Socket Communications",
	    "Socket Communications Low Power WLAN Card",	}
	,
	{
	    PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_PAGECARD,
	    PCMCIA_CIS_SOCKET_PAGECARD,
	    0,
	    "Socket Communications",
	    "Socket Communications PageCard",	}
	,
	{
	    PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_DUAL_RS232,
	    PCMCIA_CIS_SOCKET_DUAL_RS232,
	    0,
	    "Socket Communications",
	    "Socket Communications Dual RS232",	}
	,
	{
	    PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_ETHER,
	    PCMCIA_CIS_SOCKET_LP_ETHER,
	    0,
	    "Socket Communications",
	    "Socket Communications LP-E",	}
	,
	{
	    PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_ETHER_CF,
	    PCMCIA_CIS_SOCKET_LP_ETHER_CF,
	    0,
	    "Socket Communications",
	    "Socket Communications LP-E CF",	}
	,
	{
	    PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_DUAL_RS232_A,
	    PCMCIA_CIS_SOCKET_DUAL_RS232_A,
	    0,
	    "Socket Communications",
	    "Socket Communications Dual RS232_A",	}
	,
	{
	    PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_ETH_10_100_CF,
	    PCMCIA_CIS_SOCKET_LP_ETH_10_100_CF,
	    0,
	    "Socket Communications",
	    "Socket Communications 10/100 Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_SIEMENS, PCMCIA_PRODUCT_SIEMENS_SS1021,
	    PCMCIA_CIS_SIEMENS_SS1021,
	    0,
	    "Siemens",
	    "SpeedStream SS1021 WLAN",	}
	,
	{
	    PCMCIA_VENDOR_SYMBOL, PCMCIA_PRODUCT_SYMBOL_LA4100,
	    PCMCIA_CIS_SYMBOL_LA4100,
	    0,
	    "Symbol",
	    "Symbol Spectrum24 LA4100 Series WLAN",	}
	,
	{
	    PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_CD011WL,
	    PCMCIA_CIS_TDK_LAK_CD011WL,
	    0,
	    "TDK Corporation",
	    "TDK LAK-CD011WL",	}
	,
	{
	    PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_CD021BX,
	    PCMCIA_CIS_TDK_LAK_CD021BX,
	    0,
	    "TDK Corporation",
	    "TDK LAK-CD021BX Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_CF010,
	    PCMCIA_CIS_TDK_LAK_CF010,
	    0,
	    "TDK Corporation",
	    "TDK LAC-CF010",	}
	,
	{
	    PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_DFL9610,
	    PCMCIA_CIS_TDK_DFL9610,
	    0,
	    "TDK Corporation",
	    "TDK DFL9610 Ethernet & Digital Cellular",	}
	,
	{
	    PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_C6500012,
	    PCMCIA_CIS_TDK_C6500012,
	    0,
	    "TDK Corporation",
	    "TDK ELSA MicroLink MC all",	}
	,
	{
	    PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_BLUETOOTH_PCCARD,
	    PCMCIA_CIS_TDK_BLUETOOTH_PCCARD,
	    0,
	    "TDK Corporation",
	    "TDK Bluetooth PCcard",	}
	,
	{
	    PCMCIA_VENDOR_TELECOMDEVICE, PCMCIA_PRODUCT_TELECOMDEVICE_TCD_HPC100,
	    PCMCIA_CIS_TELECOMDEVICE_TCD_HPC100,
	    0,
	    "Telecom Device",
	    "Telecom Device TCD-HPC100",	}
	,
	{
	    PCMCIA_VENDOR_USROBOTICS, PCMCIA_PRODUCT_USROBOTICS_WORLDPORT144,
	    PCMCIA_CIS_USROBOTICS_WORLDPORT144,
	    0,
	    "US Robotics Corporation",
	    "US Robotics WorldPort 14.4 Modem",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CE,
	    PCMCIA_CIS_XIRCOM_CE,
	    0,
	    "Xircom",
	    "Xircom CreditCard Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CE3,
	    PCMCIA_CIS_XIRCOM_CE3,
	    0,
	    "Xircom",
	    "Xircom CreditCard 10/100 Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CE2,
	    PCMCIA_CIS_XIRCOM_CE2,
	    0,
	    "Xircom",
	    "Xircom CreditCard Ethernet II",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_XE2000,
	    PCMCIA_CIS_XIRCOM_XE2000,
	    0,
	    "Xircom",
	    "Xircom XE2000 10/100 Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CNW_801,
	    PCMCIA_CIS_XIRCOM_CNW_801,
	    0,
	    "Xircom",
	    "Xircom CreditCard Netwave (Canada)",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CNW_802,
	    PCMCIA_CIS_XIRCOM_CNW_802,
	    0,
	    "Xircom",
	    "Xircom CreditCard Netwave (US)",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CT2,
	    PCMCIA_CIS_XIRCOM_CT2,
	    0,
	    "Xircom",
	    "Xircom CreditCard Token Ring II",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CEM,
	    PCMCIA_CIS_XIRCOM_CEM,
	    0,
	    "Xircom",
	    "Xircom CreditCard Ethernet + Modem",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_REM56,
	    PCMCIA_CIS_XIRCOM_REM56,
	    0,
	    "Xircom",
	    "Xircom RealPort Ethernet 10/100 + Modem 56",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CEM28,
	    PCMCIA_CIS_XIRCOM_CEM28,
	    0,
	    "Xircom",
	    "Xircom CreditCard Ethernet + Modem 28",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CEM33,
	    PCMCIA_CIS_XIRCOM_CEM33,
	    0,
	    "Xircom",
	    "Xircom CreditCard Ethernet + Modem 33",	}
	,
	{
	    PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CEM56,
	    PCMCIA_CIS_XIRCOM_CEM56,
	    0,
	    "Xircom",
	    "Xircom CreditCard Ethernet + Modem 56",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_ACCTON_EN2212,
	    PCMCIA_CIS_ACCTON_EN2212,
	    0,
	    "ACCTON",
	    "Accton EN2212",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_ACCTON_EN2216,
	    PCMCIA_CIS_ACCTON_EN2216,
	    0,
	    "ACCTON",
	    "Accton EN2216",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_AMBICOM_AMB8002T,
	    PCMCIA_CIS_AMBICOM_AMB8002T,
	    0,
	    "AmbiCom Inc",
	    "AmbiCom AMB8002T",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_AMBICOM_AMB8110,
	    PCMCIA_CIS_AMBICOM_AMB8110,
	    0,
	    "AmbiCom Inc",
	    "AmbiCom AMB8110",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_AMD_AM79C930,
	    PCMCIA_CIS_AMD_AM79C930,
	    0,
	    "AMD",
	    "AMD Am79C930",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_BILLIONTON_CFLT10N,
	    PCMCIA_CIS_BILLIONTON_CFLT10N,
	    0,
	    "Billionton Systems Inc.",
	    "Billionton Systems Inc. CFLT10N Ethernet",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_BILLIONTON_LNT10TN,
	    PCMCIA_CIS_BILLIONTON_LNT10TN,
	    0,
	    "Billionton Systems Inc.",
	    "Billionton Systems Inc. LNT-10TN NE2000 Compatible Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_CNET_NE2000,
	    PCMCIA_CIS_CNET_NE2000,
	    0,
	    "CNet",
	    "CNet CN40BC NE2000 Compatible",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_ETHER_CF_TD,
	    PCMCIA_CIS_COREGA_ETHER_CF_TD,
	    0,
	    "Corega K.K.",
	    "Corega Ether CF-TD LAN Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_ETHER_PCC_T,
	    PCMCIA_CIS_COREGA_ETHER_PCC_T,
	    0,
	    "Corega K.K.",
	    "Corega Ether PCC-T",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_ETHER_PCC_TD,
	    PCMCIA_CIS_COREGA_ETHER_PCC_TD,
	    0,
	    "Corega K.K.",
	    "Corega Ether PCC-TD",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_ETHER_PCC_TL,
	    PCMCIA_CIS_COREGA_ETHER_PCC_TL,
	    0,
	    "Corega K.K.",
	    "Corega Ether PCC-TL",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_ETHER_II_PCC_TD,
	    PCMCIA_CIS_COREGA_ETHER_II_PCC_TD,
	    0,
	    "Corega K.K.",
	    "Corega EtherII PCC-TD",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_ETHER_II_PCC_T,
	    PCMCIA_CIS_COREGA_ETHER_II_PCC_T,
	    0,
	    "Corega K.K.",
	    "Corega EtherII PCC-T",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_FAST_ETHER_PCC_TX,
	    PCMCIA_CIS_COREGA_FAST_ETHER_PCC_TX,
	    0,
	    "Corega K.K.",
	    "Corega FastEther PCC-TX",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_FETHER_PCC_TXD,
	    PCMCIA_CIS_COREGA_FETHER_PCC_TXD,
	    0,
	    "Corega K.K.",
	    "Corega FEther PCC-TXD",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_FETHER_PCC_TXF,
	    PCMCIA_CIS_COREGA_FETHER_PCC_TXF,
	    0,
	    "Corega K.K.",
	    "Corega FEther PCC-TXF",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_FETHER_II_PCC_TXD,
	    PCMCIA_CIS_COREGA_FETHER_II_PCC_TXD,
	    0,
	    "Corega K.K.",
	    "Corega FEtherII PCC-TXD",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_LAPCCTXD,
	    PCMCIA_CIS_COREGA_LAPCCTXD,
	    0,
	    "Corega K.K.",
	    "Corega LAPCCTXD",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_PCC_11,
	    PCMCIA_CIS_COREGA_WIRELESS_LAN_PCC_11,
	    0,
	    "Corega K.K.",
	    "Corega Wireless LAN PCC-11",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_PCCA_11,
	    PCMCIA_CIS_COREGA_WIRELESS_LAN_PCCA_11,
	    0,
	    "Corega K.K.",
	    "Corega Wireless LAN PCCA-11",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_PCCB_11,
	    PCMCIA_CIS_COREGA_WIRELESS_LAN_PCCB_11,
	    0,
	    "Corega K.K.",
	    "Corega Wireless LAN PCCB-11",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_PCCL_11,
	    PCMCIA_CIS_COREGA_WIRELESS_LAN_PCCL_11,
	    0,
	    "Corega K.K.",
	    "Corega Wireless LAN PCCL-11",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_COREGA_WIRELESS_LAN_WLCFL_11,
	    PCMCIA_CIS_COREGA_WIRELESS_LAN_WLCFL_11,
	    0,
	    "Corega K.K.",
	    "Corega Wireless LAN WLCFL-11",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_DLINK_DE650,
	    PCMCIA_CIS_DLINK_DE650,
	    0,
	    "D-Link",
	    "D-Link DE-650",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_DLINK_DE660,
	    PCMCIA_CIS_DLINK_DE660,
	    0,
	    "D-Link",
	    "D-Link DE-660",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_DLINK_DE660PLUS,
	    PCMCIA_CIS_DLINK_DE660PLUS,
	    0,
	    "D-Link",
	    "D-Link DE-660+",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_DLINK_DFE670TXD,
	    PCMCIA_CIS_DLINK_DFE670TXD,
	    0,
	    "D-Link",
	    "D-Link DFE-670TXD",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_DLINK_DWL650H,
	    PCMCIA_CIS_DLINK_DWL650H,
	    0,
	    "D-Link",
	    "D-Link DWL-650H",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_DYNALINK_L10C,
	    PCMCIA_CIS_DYNALINK_L10C,
	    0,
	    "DynaLink",
	    "Dynalink L10C",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_EIGERLABS_EPX_AA2000,
	    PCMCIA_CIS_EIGERLABS_EPX_AA2000,
	    0,
	    "Eiger labs,Inc.",
	    "EPX-AA2000 PC Sound Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_EPSON_EEN10B,
	    PCMCIA_CIS_EPSON_EEN10B,
	    0,
	    "Seiko Epson Corporation",
	    "Epson EEN10B",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_EXP_EXPMULTIMEDIA,
	    PCMCIA_CIS_EXP_EXPMULTIMEDIA,
	    0,
	    "EXP Computer Inc",
	    "EXP IDE/ATAPI DVD Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_FUJITSU_FMV_J181,
	    PCMCIA_CIS_FUJITSU_FMV_J181,
	    0,
	    "Fujitsu Corporation",
	    "FUJITSU FMV-J181 PCMCIA Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_FUJITSU_FMV_J182,
	    PCMCIA_CIS_FUJITSU_FMV_J182,
	    0,
	    "Fujitsu Corporation",
	    "FUJITSU FMV-J182 PCMCIA Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_FUJITSU_FMV_J182A,
	    PCMCIA_CIS_FUJITSU_FMV_J182A,
	    0,
	    "Fujitsu Corporation",
	    "FUJITSU FMV-J182A PCMCIA Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_FUJITSU_ITCFJ182A,
	    PCMCIA_CIS_FUJITSU_ITCFJ182A,
	    0,
	    "Fujitsu Corporation",
	    "FUJITSU ITCFJ182A CompactCard",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_GEMTEK_WLAN,
	    PCMCIA_CIS_GEMTEK_WLAN,
	    0,
	    "Gem Tek",
	    "GEMTEK Prism2_5 WaveLAN Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_GENIUS_ME3000II,
	    PCMCIA_CIS_GENIUS_ME3000II,
	    0,
	    "Genius",
	    "",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_ICOM_SL200,
	    PCMCIA_CIS_ICOM_SL200,
	    0,
	    "ICOM Inc",
	    "Icom SL-200",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_INTERSIL2_PRISM2,
	    PCMCIA_CIS_INTERSIL2_PRISM2,
	    0,
	    "Intersil",
	    "Intersil Prism II",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_IODATA_CBIDE2,
	    PCMCIA_CIS_IODATA_CBIDE2,
	    0,
	    "I-O DATA",
	    "IO-DATA CBIDE2/16-bit mode",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_IODATA_PCLAT,
	    PCMCIA_CIS_IODATA_PCLAT,
	    0,
	    "I-O DATA",
	    "IO-DATA PCLA/T",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_LANTECH_FASTNETTX,
	    PCMCIA_CIS_LANTECH_FASTNETTX,
	    0,
	    "Lantech Computer Company",
	    "Lantech Fastnet/TX",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_LINKSYS_ECARD_2,
	    PCMCIA_CIS_LINKSYS_ECARD_2,
	    0,
	    "Linksys Corporation",
	    "Linksys E-Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_MEGAHERTZ_XJ2288,
	    PCMCIA_CIS_MEGAHERTZ_XJ2288,
	    0,
	    "Megahertz Corporation",
	    "Megahertz XJ2288 Modem",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_MELCO_LPC2_TX,
	    PCMCIA_CIS_MELCO_LPC2_TX,
	    0,
	    "Melco Corporation",
	    "Melco LPC2-TX",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_NAKAGAWAMETAL_LNT10TN,
	    PCMCIA_CIS_NAKAGAWAMETAL_LNT10TN,
	    0,
	    "NAKAGAWA METAL",
	    "NAKAGAWA METAL LNT-10TN NE2000 Compatible Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_NANOSPEED_PRISM2,
	    PCMCIA_CIS_NANOSPEED_PRISM2,
	    0,
	    "",
	    "NANOSPEED ROOT-RZ2000 WLAN Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_NDC_ND5100_E,
	    PCMCIA_CIS_NDC_ND5100_E,
	    0,
	    "NDC",
	    "Sohoware ND5100E NE2000 Compatible Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_NEC_CMZ_RT_WP,
	    PCMCIA_CIS_NEC_CMZ_RT_WP,
	    0,
	    "",
	    "NEC Wireless Card CMZ-RT-WP",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_NTT_ME_WLAN,
	    PCMCIA_CIS_NTT_ME_WLAN,
	    0,
	    "",
	    "NTT-ME 11Mbps Wireless LAN PC Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_PLANET_SMARTCOM2000,
	    PCMCIA_CIS_PLANET_SMARTCOM2000,
	    0,
	    "Planet",
	    "Planet SmartCOM 2000",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_PLANEX_FNW3600T,
	    PCMCIA_CIS_PLANEX_FNW3600T,
	    0,
	    "Planex Communications Inc",
	    "Planex FNW-3600-T",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_PLANEX_FNW3700T,
	    PCMCIA_CIS_PLANEX_FNW3700T,
	    0,
	    "Planex Communications Inc",
	    "Planex FNW-3700-T",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_PLANEX_GWNS11H,
	    PCMCIA_CIS_PLANEX_GWNS11H,
	    0,
	    "Planex Communications Inc",
	    "PLANEX GW-NS11H Wireless LAN PC Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_RATOC_REX_9530,
	    PCMCIA_CIS_RATOC_REX_9530,
	    0,
	    "RATOC System Inc.",
	    "RATOC REX-9530",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_RATOC_REX_CFU1,
	    PCMCIA_CIS_RATOC_REX_CFU1,
	    0,
	    "RATOC System Inc.",
	    "RATOC REX-CFU1",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_RATOC_REX_R280,
	    PCMCIA_CIS_RATOC_REX_R280,
	    0,
	    "RATOC System Inc.",
	    "RATOC REX-R280",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_RPTI_EP400,
	    PCMCIA_CIS_RPTI_EP400,
	    0,
	    "RPTI",
	    "RPTI EP400",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_RPTI_EP401,
	    PCMCIA_CIS_RPTI_EP401,
	    0,
	    "RPTI",
	    "RPTI EP401",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_PREMAX_PE200,
	    PCMCIA_CIS_PREMAX_PE200,
	    0,
	    "Premax",
	    "PreMax PE-200",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_SMC_2632W,
	    PCMCIA_CIS_SMC_2632W,
	    0,
	    "Standard Microsystems Corporation",
	    "SMC 2632 EZ Connect Wireless PC Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_SMC_8041,
	    PCMCIA_CIS_SMC_8041,
	    0,
	    "Standard Microsystems Corporation",
	    "SMC 8041TX 10/100 PC Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_SVEC_COMBOCARD,
	    PCMCIA_CIS_SVEC_COMBOCARD,
	    0,
	    "SVEC/Hawking Technology",
	    "SVEC/Hawking Tech. Combo Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_SVEC_LANCARD,
	    PCMCIA_CIS_SVEC_LANCARD,
	    0,
	    "SVEC/Hawking Technology",
	    "SVEC PCMCIA Lan Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_SVEC_PN650TX,
	    PCMCIA_CIS_SVEC_PN650TX,
	    0,
	    "SVEC/Hawking Technology",
	    "SVEC PN650TX 10/100 Dual Speed Fast Ethernet PC Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_SYNERGY21_S21810,
	    PCMCIA_CIS_SYNERGY21_S21810,
	    0,
	    "Synergy 21",
	    "Synergy 21 S21810+ NE2000 Compatible Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_TAMARACK_ETHERNET,
	    PCMCIA_CIS_TAMARACK_ETHERNET,
	    0,
	    "TAMARACK",
	    "TAMARACK Ethernet Card",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_TEAC_IDECARDII,
	    PCMCIA_CIS_TEAC_IDECARDII,
	    0,
	    "TEAC",
	    "TEAC IDE Card/II",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_TOSHIBA_CBIDE2,
	    PCMCIA_CIS_TOSHIBA_CBIDE2,
	    0,
	    "",
	    "TOSHIBA PA2673U CBIDE2/16-bit mode (IO-DATA OEM)",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_XIRCOM_CFE_10,
	    PCMCIA_CIS_XIRCOM_CFE_10,
	    0,
	    "Xircom",
	    "Xircom CompactCard CFE-10",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_YEDATA_EXTERNAL_FDD,
	    PCMCIA_CIS_YEDATA_EXTERNAL_FDD,
	    0,
	    "Y-E DATA",
	    "Y-E DATA External FDD",	}
	,
	{
	    PCMCIA_VENDOR_UNKNOWN, PCMCIA_PRODUCT_ZOOM_AIR_4000,
	    PCMCIA_CIS_ZOOM_AIR_4000,
	    0,
	    "",
	    "Zoom Air-4000",	}
	,
	{
	    PCMCIA_VENDOR_FUJITSU,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Fujitsu Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_INTERSIL,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Netgear",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_PANASONIC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Matsushita Electric Industrial Co.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SANDISK,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Sandisk Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NEWMEDIA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "New Media Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_INTEL,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Intel",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_IBM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "IBM Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SHARP,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Sharp Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_DIGITAL,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Digital Equipment Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_3COM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "3Com Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_MEGAHERTZ,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Megahertz Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SOCKET,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Socket Communications",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_TDK,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "TDK Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_XIRCOM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Xircom",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SMC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Standard Microsystems Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_MOTOROLA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Motorola Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NI,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "National Instruments",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_USROBOTICS,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "US Robotics Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_OLICOM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Olicom",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_PROXIM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Proxim",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_MEGAHERTZ2,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Megahertz Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ADAPTEC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Adaptec Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_QUATECH,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Quatech",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_COMPAQ,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Compaq",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_OSITECH,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Ositech",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_DLINK_2,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "D-Link",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_DLINK_3,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "D-Link",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_LINKSYS,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Linksys Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NETGEAR,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Netgear",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_IODATA3,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "I-O DATA",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SIMPLETECH,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Simple Technology",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SYMBOL2,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Symbol",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_LUCENT,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Lucent Technologies",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_AIRONET,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Aironet Wireless Communications",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ERICSSON,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Ericsson",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_PSION,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Psion",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_COMPAQ2,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Compaq",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_KINGSTON,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Kingston",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SIERRA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Sierra Wireless",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_DAYNA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Dayna Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_RAYTHEON,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Raytheon",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_IODATA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "I-O DATA",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_BELKIN,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Belkin",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_BAY,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Bay Networks",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_FARALLON,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Farallon Communications",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_TELECOMDEVICE,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Telecom Device",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NOKIA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Nokia Communications",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SAMSUNG,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Samsung",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ANYCOM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Anycom",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ALVARION,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Alvarion Ltd.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ARTEM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "ARtem",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SYMBOL,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Symbol",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_BUFFALO,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "BUFFALO (Melco Corporation)",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_LINKSYS2,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "The Linksys Group",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NECINFRONTIA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "NEC Infrontia",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_IODATA2,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "I-O DATA",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ASUSTEK,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Asustek Computer",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SIEMENS,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Siemens",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_MICROSOFT,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Microsoft Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_AMBICOM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "AmbiCom Inc",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_BREEZECOM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "BreezeCOM",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NEWMEDIA2,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "NewMedia",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ACTIONTEC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "ACTIONTEC",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_LASAT,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Lasat Communications A/S",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_LEXARMEDIA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Lexar Media",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ARCHOS,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Archos",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_DUAL,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Dual",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_EDIMAX,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Edimax Technology Inc.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_COMPEX,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Compex Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_MELCO,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Melco Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_CONTEC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Contec",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_MACNICA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "MACNICA",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ROLAND,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Roland",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_COREGA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Corega K.K.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ALLIEDTELESIS,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Allied Telesis K.K.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_HAGIWARASYSCOM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Hagiwara SYS-COM",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_RATOC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "RATOC System Inc.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NEXTCOM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "NextCom K.K.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_EMTAC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "EMTAC Technology Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ELSA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Elsa",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ACCTON,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "ACCTON",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_AMD,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "AMD",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_BILLIONTON,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Billionton Systems Inc.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_CNET,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "CNet",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_DLINK,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "D-Link",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_DYNALINK,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "DynaLink",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_EIGERLABS,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Eiger labs,Inc.",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_EPSON,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Seiko Epson Corporation",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_EXP,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "EXP Computer Inc",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_GEMTEK,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Gem Tek",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_GENIUS,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Genius",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_ICOM,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "ICOM Inc",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_INTERSIL2,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Intersil",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_LANTECH,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Lantech Computer Company",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NAKAGAWAMETAL,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "NAKAGAWA METAL",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_NDC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "NDC",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_PLANET,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Planet",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_PLANEX,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Planex Communications Inc",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_PREMAX,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Premax",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_RPTI,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "RPTI",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SVEC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "SVEC/Hawking Technology",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_SYNERGY21,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Synergy 21",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_TAMARACK,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "TAMARACK",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_TEAC,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "TEAC",
	    NULL,
	},
	{
	    PCMCIA_VENDOR_YEDATA,
	    PCMCIA_KNOWNDEV_NOPROD,
	    PCMCIA_CIS_INVALID,
	    0,
	    "Y-E DATA",
	    NULL,
	},
	{ 0, 0, { NULL, NULL, NULL, NULL }, 0, NULL, NULL, }
};
