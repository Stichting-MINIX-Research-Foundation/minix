/*	$NetBSD: miidevs.h,v 1.127 2015/08/14 01:26:38 knakahara Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: miidevs,v 1.124 2015/08/14 01:23:17 knakahara Exp
 */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * List of known MII OUIs.
 * For a complete list see http://standards.ieee.org/regauth/oui/
 *
 * XXX Vendors do obviously not agree how OUIs (24 bit) are mapped
 * to the 22 bits available in the id registers.
 * IEEE 802.3u-1995, subclause 22.2.4.3.1, figure 22-12, depicts the right
 * mapping; the bit positions are defined in IEEE 802-1990, figure 5.2.
 * (There is a formal 802.3 interpretation, number 1-07/98 of July 09 1998,
 * about this.)
 * The MII_OUI() macro in "miivar.h" reflects this.
 * If a vendor uses a different mapping, an "xx" prefixed OUI is defined here
 * which is mangled accordingly to compensate.
 */

/*
 * Use "make -f Makefile.miidevs" to regenerate miidevs.h and miidevs_data.h
 */

#define	MII_OUI_AGERE	0x00053d	/* Agere */
#define	MII_OUI_ALTIMA	0x0010a9	/* Altima Communications */
#define	MII_OUI_AMD	0x00001a	/* Advanced Micro Devices */
#define	MII_OUI_ATHEROS	0x001374	/* Atheros */
#define	MII_OUI_ATTANSIC	0x00c82e	/* Attansic Technology */
#define	MII_OUI_BROADCOM	0x001018	/* Broadcom Corporation */
#define	MII_OUI_BROADCOM2	0x000af7	/* Broadcom Corporation */
#define	MII_OUI_BROADCOM3	0x001be9	/* Broadcom Corporation */
#define	MII_OUI_CICADA	0x0003F1	/* Cicada Semiconductor */
#define	MII_OUI_DAVICOM	0x00606e	/* Davicom Semiconductor */
#define	MII_OUI_ENABLESEMI	0x0010dd	/* Enable Semiconductor */
#define	MII_OUI_ICPLUS	0x0090c3	/* IC Plus Corp. */
#define	MII_OUI_ICS	0x00a0be	/* Integrated Circuit Systems */
#define	MII_OUI_INTEL	0x00aa00	/* Intel */
#define	MII_OUI_JMICRON	0x00d831	/* JMicron */
#define	MII_OUI_LEVEL1	0x00207b	/* Level 1 */
#define	MII_OUI_MARVELL	0x005043	/* Marvell Semiconductor */
#define	MII_OUI_MICREL	0x0010a1	/* Micrel */
#define	MII_OUI_MYSON	0x00c0b4	/* Myson Technology */
#define	MII_OUI_NATSEMI	0x080017	/* National Semiconductor */
#define	MII_OUI_PMCSIERRA	0x00e004	/* PMC-Sierra */
#define	MII_OUI_RDC	0x00d02d	/* RDC Semiconductor */
#define	MII_OUI_REALTEK	0x00e04c	/* RealTek */
#define	MII_OUI_QUALSEMI	0x006051	/* Quality Semiconductor */
#define	MII_OUI_SEEQ	0x00a07d	/* Seeq */
#define	MII_OUI_SIS	0x00e006	/* Silicon Integrated Systems */
#define	MII_OUI_SMSC	0x00800f	/* SMSC */
#define	MII_OUI_TI	0x080028	/* Texas Instruments */
#define	MII_OUI_TSC	0x00c039	/* TDK Semiconductor */
#define	MII_OUI_XAQTI	0x00e0ae	/* XaQti Corp. */

/* Some Intel 82553's use an alternative OUI. */
#define	MII_OUI_xxINTEL	0x001f00	/* Intel */

/* Some VIA 6122's use an alternative OUI. */
#define	MII_OUI_xxCICADA	0x00c08f	/* Cicada Semiconductor */

/* bad bitorder (bits "g" and "h" (= MSBs byte 1) lost) */
#define	MII_OUI_yyAMD	0x000058	/* Advanced Micro Devices */
#define	MII_OUI_xxBROADCOM	0x000818	/* Broadcom Corporation */
#define	MII_OUI_xxBROADCOM_ALT1	0x0050ef	/* Broadcom Corporation */
#define	MII_OUI_xxDAVICOM	0x000676	/* Davicom Semiconductor */
#define	MII_OUI_yyINTEL	0x005500	/* Intel */
#define	MII_OUI_xxMARVELL	0x000ac2	/* Marvell Semiconductor */
#define	MII_OUI_xxMYSON	0x00032d	/* Myson Technology */
#define	MII_OUI_xxNATSEMI	0x1000e8	/* National Semiconductor */
#define	MII_OUI_xxQUALSEMI	0x00068a	/* Quality Semiconductor */
#define	MII_OUI_xxTSC	0x00039c	/* TDK Semiconductor */

/* bad byteorder (bits "q" and "r" (= LSBs byte 3) lost) */
#define	MII_OUI_xxLEVEL1	0x782000	/* Level 1 */
#define	MII_OUI_xxXAQTI	0xace000	/* XaQti Corp. */

/* Don't know what's going on here. */
#define	MII_OUI_xxPMCSIERRA	0x0009c0	/* PMC-Sierra */
#define	MII_OUI_xxPMCSIERRA2	0x009057	/* PMC-Sierra */

#define	MII_OUI_xxREALTEK	0x000732	/* Realtek */
#define	MII_OUI_yyREALTEK	0x000004	/* Realtek */
/*
 * List of known models.  Grouped by oui.
 */

/*
 * Agere PHYs
 */
#define	MII_MODEL_AGERE_ET1011	0x0004
#define	MII_STR_AGERE_ET1011	"Agere ET1011 10/100/1000baseT PHY"

/* Atheros PHYs */
#define	MII_MODEL_ATHEROS_F1	0x0001
#define	MII_STR_ATHEROS_F1	"F1 10/100/1000 PHY"
#define	MII_MODEL_ATHEROS_F2	0x0002
#define	MII_STR_ATHEROS_F2	"F2 10/100 PHY"

/* Attansic PHYs */
#define	MII_MODEL_ATTANSIC_L1	0x0001
#define	MII_STR_ATTANSIC_L1	"L1 10/100/1000 PHY"
#define	MII_MODEL_ATTANSIC_L2	0x0002
#define	MII_STR_ATTANSIC_L2	"L2 10/100 PHY"
#define	MII_MODEL_ATTANSIC_AR8021	0x0004
#define	MII_STR_ATTANSIC_AR8021	"Atheros AR8021 10/100/1000 PHY"
#define	MII_MODEL_ATTANSIC_AR8035	0x0007
#define	MII_STR_ATTANSIC_AR8035	"Atheros AR8035 10/100/1000 PHY"

/* Altima Communications PHYs */
/* Don't know the model for ACXXX */
#define	MII_MODEL_ALTIMA_ACXXX	0x0001
#define	MII_STR_ALTIMA_ACXXX	"ACXXX 10/100 media interface"
#define	MII_MODEL_ALTIMA_AC101	0x0021
#define	MII_STR_ALTIMA_AC101	"AC101 10/100 media interface"
#define	MII_MODEL_ALTIMA_AC101L	0x0012
#define	MII_STR_ALTIMA_AC101L	"AC101L 10/100 media interface"
/* AMD Am79C87[45] have ALTIMA OUI */
#define	MII_MODEL_ALTIMA_Am79C875	0x0014
#define	MII_STR_ALTIMA_Am79C875	"Am79C875 10/100 media interface"
#define	MII_MODEL_ALTIMA_Am79C874	0x0021
#define	MII_STR_ALTIMA_Am79C874	"Am79C874 10/100 media interface"

/* Advanced Micro Devices PHYs */
/* see Davicom DM9101 for Am79C873 */
#define	MII_MODEL_yyAMD_79C972_10T	0x0001
#define	MII_STR_yyAMD_79C972_10T	"Am79C972 internal 10BASE-T interface"
#define	MII_MODEL_yyAMD_79c973phy	0x0036
#define	MII_STR_yyAMD_79c973phy	"Am79C973 internal 10/100 media interface"
#define	MII_MODEL_yyAMD_79c901	0x0037
#define	MII_STR_yyAMD_79c901	"Am79C901 10BASE-T interface"
#define	MII_MODEL_yyAMD_79c901home	0x0039
#define	MII_STR_yyAMD_79c901home	"Am79C901 HomePNA 1.0 interface"

/* Broadcom Corp. PHYs */
#define	MII_MODEL_xxBROADCOM_3C905B	0x0012
#define	MII_STR_xxBROADCOM_3C905B	"Broadcom 3c905B internal PHY"
#define	MII_MODEL_xxBROADCOM_3C905C	0x0017
#define	MII_STR_xxBROADCOM_3C905C	"Broadcom 3c905C internal PHY"
#define	MII_MODEL_xxBROADCOM_BCM5201	0x0021
#define	MII_STR_xxBROADCOM_BCM5201	"BCM5201 10/100 media interface"
#define	MII_MODEL_xxBROADCOM_BCM5214	0x0028
#define	MII_STR_xxBROADCOM_BCM5214	"BCM5214 Quad 10/100 media interface"
#define	MII_MODEL_xxBROADCOM_BCM5221	0x001e
#define	MII_STR_xxBROADCOM_BCM5221	"BCM5221 10/100 media interface"
#define	MII_MODEL_xxBROADCOM_BCM5222	0x0032
#define	MII_STR_xxBROADCOM_BCM5222	"BCM5222 Dual 10/100 media interface"
#define	MII_MODEL_xxBROADCOM_BCM4401	0x0036
#define	MII_STR_xxBROADCOM_BCM4401	"BCM4401 10/100 media interface"
#define	MII_MODEL_xxBROADCOM_BCM5365	0x0037
#define	MII_STR_xxBROADCOM_BCM5365	"BCM5365 10/100 5-port PHY switch"
#define	MII_MODEL_BROADCOM_BCM5400	0x0004
#define	MII_STR_BROADCOM_BCM5400	"BCM5400 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5401	0x0005
#define	MII_STR_BROADCOM_BCM5401	"BCM5401 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5411	0x0007
#define	MII_STR_BROADCOM_BCM5411	"BCM5411 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5464	0x000b
#define	MII_STR_BROADCOM_BCM5464	"BCM5464 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5461	0x000c
#define	MII_STR_BROADCOM_BCM5461	"BCM5461 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5462	0x000d
#define	MII_STR_BROADCOM_BCM5462	"BCM5462 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5421	0x000e
#define	MII_STR_BROADCOM_BCM5421	"BCM5421 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5752	0x0010
#define	MII_STR_BROADCOM_BCM5752	"BCM5752 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5701	0x0011
#define	MII_STR_BROADCOM_BCM5701	"BCM5701 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5706	0x0015
#define	MII_STR_BROADCOM_BCM5706	"BCM5706 1000BASE-T/SX media interface"
#define	MII_MODEL_BROADCOM_BCM5703	0x0016
#define	MII_STR_BROADCOM_BCM5703	"BCM5703 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5750	0x0018
#define	MII_STR_BROADCOM_BCM5750	"BCM5750 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5704	0x0019
#define	MII_STR_BROADCOM_BCM5704	"BCM5704 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5705	0x001a
#define	MII_STR_BROADCOM_BCM5705	"BCM5705 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM54K2	0x002e
#define	MII_STR_BROADCOM_BCM54K2	"BCM54K2 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM_BCM5714	0x0034
#define	MII_STR_BROADCOM_BCM5714	"BCM5714 1000BASE-T/X media interface"
#define	MII_MODEL_BROADCOM_BCM5780	0x0035
#define	MII_STR_BROADCOM_BCM5780	"BCM5780 1000BASE-T/X media interface"
#define	MII_MODEL_BROADCOM_BCM5708C	0x0036
#define	MII_STR_BROADCOM_BCM5708C	"BCM5708C 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM2_BCM5325	0x0003
#define	MII_STR_BROADCOM2_BCM5325	"BCM5325 10/100 5-port PHY switch"
#define	MII_MODEL_BROADCOM2_BCM5906	0x0004
#define	MII_STR_BROADCOM2_BCM5906	"BCM5906 10/100baseTX media interface"
#define	MII_MODEL_BROADCOM2_BCM5481	0x000a
#define	MII_STR_BROADCOM2_BCM5481	"BCM5481 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM2_BCM5482	0x000b
#define	MII_STR_BROADCOM2_BCM5482	"BCM5482 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM2_BCM5755	0x000c
#define	MII_STR_BROADCOM2_BCM5755	"BCM5755 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM2_BCM5756	0x000d
#define	MII_STR_BROADCOM2_BCM5756	"BCM5756 1000BASE-T media interface XXX"
#define	MII_MODEL_BROADCOM2_BCM5754	0x000e
#define	MII_STR_BROADCOM2_BCM5754	"BCM5754/5787 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM2_BCM5708S	0x0015
#define	MII_STR_BROADCOM2_BCM5708S	"BCM5708S 1000/2500baseSX PHY"
#define	MII_MODEL_BROADCOM2_BCM5785	0x0016
#define	MII_STR_BROADCOM2_BCM5785	"BCM5785 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM2_BCM5709CAX	0x002c
#define	MII_STR_BROADCOM2_BCM5709CAX	"BCM5709CAX 10/100/1000baseT PHY"
#define	MII_MODEL_BROADCOM2_BCM5722	0x002d
#define	MII_STR_BROADCOM2_BCM5722	"BCM5722 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM2_BCM5784	0x003a
#define	MII_STR_BROADCOM2_BCM5784	"BCM5784 10/100/1000baseT PHY"
#define	MII_MODEL_BROADCOM2_BCM5709C	0x003c
#define	MII_STR_BROADCOM2_BCM5709C	"BCM5709 10/100/1000baseT PHY"
#define	MII_MODEL_BROADCOM2_BCM5761	0x003d
#define	MII_STR_BROADCOM2_BCM5761	"BCM5761 10/100/1000baseT PHY"
#define	MII_MODEL_BROADCOM2_BCM5709S	0x003f
#define	MII_STR_BROADCOM2_BCM5709S	"BCM5709S 1000/2500baseSX PHY"
#define	MII_MODEL_BROADCOM3_BCM57780	0x0019
#define	MII_STR_BROADCOM3_BCM57780	"BCM57780 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM3_BCM5717C	0x0020
#define	MII_STR_BROADCOM3_BCM5717C	"BCM5717C 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM3_BCM5719C	0x0022
#define	MII_STR_BROADCOM3_BCM5719C	"BCM5719C 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM3_BCM57765	0x0024
#define	MII_STR_BROADCOM3_BCM57765	"BCM57765 1000BASE-T media interface"
#define	MII_MODEL_BROADCOM3_BCM5720C	0x0036
#define	MII_STR_BROADCOM3_BCM5720C	"BCM5720C 1000BASE-T media interface"
#define	MII_MODEL_xxBROADCOM_ALT1_BCM5906	0x0004
#define	MII_STR_xxBROADCOM_ALT1_BCM5906	"BCM5906 10/100baseTX media interface"
 
/* Cicada Semiconductor PHYs (now owned by Vitesse?) */
#define	MII_MODEL_CICADA_CS8201	0x0001
#define	MII_STR_CICADA_CS8201	"Cicada CS8201 10/100/1000TX PHY"
#define	MII_MODEL_CICADA_CS8204	0x0004
#define	MII_STR_CICADA_CS8204	"Cicada CS8204 10/100/1000TX PHY"
#define	MII_MODEL_CICADA_VSC8211	0x000b
#define	MII_STR_CICADA_VSC8211	"Cicada VSC8211 10/100/1000TX PHY"
#define	MII_MODEL_CICADA_CS8201A	0x0020
#define	MII_STR_CICADA_CS8201A	"Cicada CS8201 10/100/1000TX PHY"
#define	MII_MODEL_CICADA_CS8201B	0x0021
#define	MII_STR_CICADA_CS8201B	"Cicada CS8201 10/100/1000TX PHY"
#define	MII_MODEL_xxCICADA_VSC8221	0x0015
#define	MII_STR_xxCICADA_VSC8221	"Vitesse VSC8221 10/100/1000BASE-T PHY"
#define	MII_MODEL_xxCICADA_VSC8244	0x002c
#define	MII_STR_xxCICADA_VSC8244	"Vitesse VSC8244 Quad 10/100/1000BASE-T PHY"
#define	MII_MODEL_xxCICADA_CS8201B	0x0021
#define	MII_STR_xxCICADA_CS8201B	"Cicada CS8201 10/100/1000TX PHY"

/* Davicom Semiconductor PHYs */
/* AMD Am79C873 seems to be a relabeled DM9101 */
#define	MII_MODEL_xxDAVICOM_DM9101	0x0000
#define	MII_STR_xxDAVICOM_DM9101	"DM9101 (AMD Am79C873) 10/100 media interface"
#define	MII_MODEL_xxDAVICOM_DM9102	0x0004
#define	MII_STR_xxDAVICOM_DM9102	"DM9102 10/100 media interface"

/* IC Plus Corp. PHYs */
#define	MII_MODEL_ICPLUS_IP100	0x0004
#define	MII_STR_ICPLUS_IP100	"IP100 10/100 PHY"
#define	MII_MODEL_ICPLUS_IP101	0x0005
#define	MII_STR_ICPLUS_IP101	"IP101 10/100 PHY"
#define	MII_MODEL_ICPLUS_IP1000A	0x0008
#define	MII_STR_ICPLUS_IP1000A	"IP1000A 10/100/1000 PHY"
#define	MII_MODEL_ICPLUS_IP1001	0x0019
#define	MII_STR_ICPLUS_IP1001	"IP1001 10/100/1000 PHY"

/* Integrated Circuit Systems PHYs */
#define	MII_MODEL_ICS_1889	0x0001
#define	MII_STR_ICS_1889	"ICS1889 10/100 media interface"
#define	MII_MODEL_ICS_1890	0x0002
#define	MII_STR_ICS_1890	"ICS1890 10/100 media interface"
#define	MII_MODEL_ICS_1892	0x0003
#define	MII_STR_ICS_1892	"ICS1892 10/100 media interface"
#define	MII_MODEL_ICS_1893	0x0004
#define	MII_STR_ICS_1893	"ICS1893 10/100 media interface"

/* Intel PHYs */
#define	MII_MODEL_xxINTEL_I82553	0x0000
#define	MII_STR_xxINTEL_I82553	"i82553 10/100 media interface"
#define	MII_MODEL_yyINTEL_I82555	0x0015
#define	MII_STR_yyINTEL_I82555	"i82555 10/100 media interface"
#define	MII_MODEL_yyINTEL_I82562EH	0x0017
#define	MII_STR_yyINTEL_I82562EH	"i82562EH HomePNA interface"
#define	MII_MODEL_yyINTEL_I82562G	0x0031
#define	MII_STR_yyINTEL_I82562G	"i82562G 10/100 media interface"
#define	MII_MODEL_yyINTEL_I82562EM	0x0032
#define	MII_STR_yyINTEL_I82562EM	"i82562EM 10/100 media interface"
#define	MII_MODEL_yyINTEL_I82562ET	0x0033
#define	MII_STR_yyINTEL_I82562ET	"i82562ET 10/100 media interface"
#define	MII_MODEL_yyINTEL_I82553	0x0035
#define	MII_STR_yyINTEL_I82553	"i82553 10/100 media interface"
#define	MII_MODEL_yyINTEL_I82566	0x0039
#define	MII_STR_yyINTEL_I82566	"i82566 10/100/1000 media interface"
#define	MII_MODEL_INTEL_I82577	0x0005
#define	MII_STR_INTEL_I82577	"i82577 10/100/1000 media interface"
#define	MII_MODEL_INTEL_I82579	0x0009
#define	MII_STR_INTEL_I82579	"i82579 10/100/1000 media interface"
#define	MII_MODEL_INTEL_I217	0x000a
#define	MII_STR_INTEL_I217	"i217 10/100/1000 media interface"
#define	MII_MODEL_xxMARVELL_I210	0x0000
#define	MII_STR_xxMARVELL_I210	"I210 10/100/1000 media interface"
#define	MII_MODEL_xxMARVELL_I82563	0x000a
#define	MII_STR_xxMARVELL_I82563	"i82563 10/100/1000 media interface"

#define	MII_MODEL_yyINTEL_IGP01E1000	0x0038
#define	MII_STR_yyINTEL_IGP01E1000	"Intel IGP01E1000 Gigabit PHY"

/* JMicron PHYs */
#define	MII_MODEL_JMICRON_JMC250	0x0021
#define	MII_STR_JMICRON_JMC250	"JMC250 10/100/1000 media interface"
#define	MII_MODEL_JMICRON_JMC260	0x0022
#define	MII_STR_JMICRON_JMC260	"JMC260 10/100 media interface"

/* Level 1 PHYs */
#define	MII_MODEL_xxLEVEL1_LXT970	0x0000
#define	MII_STR_xxLEVEL1_LXT970	"LXT970 10/100 media interface"
#define	MII_MODEL_LEVEL1_LXT971	0x000e
#define	MII_STR_LEVEL1_LXT971	"LXT971/2 10/100 media interface"
#define	MII_MODEL_LEVEL1_LXT973	0x0021
#define	MII_STR_LEVEL1_LXT973	"LXT973 10/100 Dual PHY"
#define	MII_MODEL_LEVEL1_LXT974	0x0004
#define	MII_STR_LEVEL1_LXT974	"LXT974 10/100 Quad PHY"
#define	MII_MODEL_LEVEL1_LXT975	0x0005
#define	MII_STR_LEVEL1_LXT975	"LXT975 10/100 Quad PHY"
#define	MII_MODEL_LEVEL1_LXT1000_OLD	0x0003
#define	MII_STR_LEVEL1_LXT1000_OLD	"LXT1000 1000BASE-T media interface"
#define	MII_MODEL_LEVEL1_LXT1000	0x000c
#define	MII_STR_LEVEL1_LXT1000	"LXT1000 1000BASE-T media interface"

/* Marvell Semiconductor PHYs */
#define	MII_MODEL_xxMARVELL_E1000	0x0000
#define	MII_STR_xxMARVELL_E1000	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1011	0x0002
#define	MII_STR_xxMARVELL_E1011	"Marvell 88E1011 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1000_3	0x0003
#define	MII_STR_xxMARVELL_E1000_3	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1000S	0x0004
#define	MII_STR_xxMARVELL_E1000S	"Marvell 88E1000S Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1000_5	0x0005
#define	MII_STR_xxMARVELL_E1000_5	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1101	0x0006
#define	MII_STR_xxMARVELL_E1101	"Marvell 88E1101 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E3082	0x0008
#define	MII_STR_xxMARVELL_E3082	"Marvell 88E3082 10/100 Fast Ethernet PHY"
#define	MII_MODEL_xxMARVELL_E1112	0x0009
#define	MII_STR_xxMARVELL_E1112	"Marvell 88E1112 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1149	0x000b
#define	MII_STR_xxMARVELL_E1149	"Marvell 88E1149 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1111	0x000c
#define	MII_STR_xxMARVELL_E1111	"Marvell 88E1111 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1145	0x000d
#define	MII_STR_xxMARVELL_E1145	"Marvell 88E1145 Quad Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E6060	0x0010
#define	MII_STR_xxMARVELL_E6060	"Marvell 88E6060 6-Port 10/100 Fast Ethernet Switch"
#define	MII_MODEL_xxMARVELL_E1512	0x001d
#define	MII_STR_xxMARVELL_E1512	"Marvell 88E1512 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1116	0x0021
#define	MII_STR_xxMARVELL_E1116	"Marvell 88E1116 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1118	0x0022
#define	MII_STR_xxMARVELL_E1118	"Marvell 88E1118 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1116R	0x0024
#define	MII_STR_xxMARVELL_E1116R	"Marvell 88E1116R Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1149R	0x0025
#define	MII_STR_xxMARVELL_E1149R	"Marvell 88E1149R Quad Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E3016	0x0026
#define	MII_STR_xxMARVELL_E3016	"Marvell 88E3016 10/100 Fast Ethernet PHY"
#define	MII_MODEL_xxMARVELL_PHYG65G	0x0027
#define	MII_STR_xxMARVELL_PHYG65G	"Marvell PHYG65G Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1116R_29	0x0029
#define	MII_STR_xxMARVELL_E1116R_29	"Marvell 88E1116R Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1543	0x002a
#define	MII_STR_xxMARVELL_E1543	"Marvell 88E1543 Alaska Quad Port Gb PHY"
#define	MII_MODEL_MARVELL_E1000	0x0000
#define	MII_STR_MARVELL_E1000	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_MARVELL_E1011	0x0002
#define	MII_STR_MARVELL_E1011	"Marvell 88E1011 Gigabit PHY"
#define	MII_MODEL_MARVELL_E1000_3	0x0003
#define	MII_STR_MARVELL_E1000_3	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_MARVELL_E1000_5	0x0005
#define	MII_STR_MARVELL_E1000_5	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_MARVELL_E1111	0x000c
#define	MII_STR_MARVELL_E1111	"Marvell 88E1111 Gigabit PHY"

/* Micrel PHYs */
#define	MII_MODEL_MICREL_KSZ9021RNI	0x0021
#define	MII_STR_MICREL_KSZ9021RNI	"Micrel KSZ9021RNI 10/100/1000 PHY"

/* Myson Technology PHYs */
#define	MII_MODEL_xxMYSON_MTD972	0x0000
#define	MII_STR_xxMYSON_MTD972	"MTD972 10/100 media interface"
#define	MII_MODEL_MYSON_MTD803	0x0000
#define	MII_STR_MYSON_MTD803	"MTD803 3-in-1 media interface"

/* National Semiconductor PHYs */
#define	MII_MODEL_xxNATSEMI_DP83840	0x0000
#define	MII_STR_xxNATSEMI_DP83840	"DP83840 10/100 media interface"
#define	MII_MODEL_xxNATSEMI_DP83843	0x0001
#define	MII_STR_xxNATSEMI_DP83843	"DP83843 10/100 media interface"
#define	MII_MODEL_xxNATSEMI_DP83815	0x0002
#define	MII_STR_xxNATSEMI_DP83815	"DP83815 10/100 media interface"
#define	MII_MODEL_xxNATSEMI_DP83847	0x0003
#define	MII_STR_xxNATSEMI_DP83847	"DP83847 10/100 media interface"
#define	MII_MODEL_xxNATSEMI_DP83891	0x0005
#define	MII_STR_xxNATSEMI_DP83891	"DP83891 1000BASE-T media interface"
#define	MII_MODEL_xxNATSEMI_DP83861	0x0006
#define	MII_STR_xxNATSEMI_DP83861	"DP83861 1000BASE-T media interface"
#define	MII_MODEL_xxNATSEMI_DP83865	0x0007
#define	MII_STR_xxNATSEMI_DP83865	"DP83865 1000BASE-T media interface"
#define	MII_MODEL_xxNATSEMI_DP83849	0x000a
#define	MII_STR_xxNATSEMI_DP83849	"DP83849 10/100 media interface"

/* PMC Sierra PHYs */
#define	MII_MODEL_xxPMCSIERRA_PM8351	0x0000
#define	MII_STR_xxPMCSIERRA_PM8351	"PM8351 OctalPHY Gigabit interface"
#define	MII_MODEL_xxPMCSIERRA2_PM8352	0x0002
#define	MII_STR_xxPMCSIERRA2_PM8352	"PM8352 OctalPHY Gigabit interface"
#define	MII_MODEL_xxPMCSIERRA2_PM8353	0x0003
#define	MII_STR_xxPMCSIERRA2_PM8353	"PM8353 QuadPHY Gigabit interface"
#define	MII_MODEL_PMCSIERRA_PM8354	0x0004
#define	MII_STR_PMCSIERRA_PM8354	"PM8354 QuadPHY Gigabit interface"

/* Quality Semiconductor PHYs */
#define	MII_MODEL_xxQUALSEMI_QS6612	0x0000
#define	MII_STR_xxQUALSEMI_QS6612	"QS6612 10/100 media interface"

/* RDC Semiconductor PHYs */
#define	MII_MODEL_RDC_R6040	0x0003
#define	MII_STR_RDC_R6040	"R6040 10/100 media interface"
/* RealTek PHYs */
#define	MII_MODEL_yyREALTEK_RTL8201L	0x0020
#define	MII_STR_yyREALTEK_RTL8201L	"RTL8201L 10/100 media interface"
#define	MII_MODEL_xxREALTEK_RTL8169S	0x0011
#define	MII_STR_xxREALTEK_RTL8169S	"RTL8169S/8110S/8211 1000BASE-T media interface"
#define	MII_MODEL_REALTEK_RTL8251	0x0000
#define	MII_STR_REALTEK_RTL8251	"RTL8251 1000BASE-T media interface"
#define	MII_MODEL_REALTEK_RTL8169S	0x0011
#define	MII_STR_REALTEK_RTL8169S	"RTL8169S/8110S/8211 1000BASE-T media interface"

/* Seeq PHYs */
#define	MII_MODEL_SEEQ_80220	0x0003
#define	MII_STR_SEEQ_80220	"Seeq 80220 10/100 media interface"
#define	MII_MODEL_SEEQ_84220	0x0004
#define	MII_STR_SEEQ_84220	"Seeq 84220 10/100 media interface"
#define	MII_MODEL_SEEQ_80225	0x0008
#define	MII_STR_SEEQ_80225	"Seeq 80225 10/100 media interface"

/* Silicon Integrated Systems PHYs */
#define	MII_MODEL_SIS_900	0x0000
#define	MII_STR_SIS_900	"SiS 900 10/100 media interface"

/* SMSC PHYs */
#define	MII_MODEL_SMSC_LAN8700	0x000c
#define	MII_STR_SMSC_LAN8700	"SMSC LAN8700 10/100 Ethernet Transceiver"
#define	MII_MODEL_SMSC_LAN8710_LAN8720	0x000f
#define	MII_STR_SMSC_LAN8710_LAN8720	"SMSC LAN8710/LAN8720 10/100 Ethernet Transceiver"

/* Texas Instruments PHYs */
#define	MII_MODEL_TI_TLAN10T	0x0001
#define	MII_STR_TI_TLAN10T	"ThunderLAN 10BASE-T media interface"
#define	MII_MODEL_TI_100VGPMI	0x0002
#define	MII_STR_TI_100VGPMI	"ThunderLAN 100VG-AnyLan media interface"
#define	MII_MODEL_TI_TNETE2101	0x0003
#define	MII_STR_TI_TNETE2101	"TNETE2101 media interface"

/* TDK Semiconductor PHYs */
#define	MII_MODEL_xxTSC_78Q2120	0x0014
#define	MII_STR_xxTSC_78Q2120	"78Q2120 10/100 media interface"
#define	MII_MODEL_xxTSC_78Q2121	0x0015
#define	MII_STR_xxTSC_78Q2121	"78Q2121 100BASE-TX media interface"

/* XaQti Corp. PHYs */
#define	MII_MODEL_xxXAQTI_XMACII	0x0000
#define	MII_STR_xxXAQTI_XMACII	"XaQti Corp. XMAC II gigabit interface"
