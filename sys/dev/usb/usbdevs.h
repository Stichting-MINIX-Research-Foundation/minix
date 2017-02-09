/*	$NetBSD: usbdevs.h,v 1.693 2015/09/14 15:51:29 nonaka Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: usbdevs,v 1.701 2015/09/14 15:51:07 nonaka Exp
 */

/*
 * Copyright (c) 1998-2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 * Use "make -f Makefile.usbdevs" to regenerate usbdevs.h and usbdevs_data.h
 */

/*
 * List of known USB vendors
 *
 * USB.org publishes a VID list of USB-IF member companies at
 * http://www.usb.org/developers/tools
 * Note that it does not show companies that have obtained a Vendor ID
 * without becoming full members.
 *
 * Please note that these IDs do not do anything. Adding an ID here and
 * regenerating the usbdevs.h and usbdevs_data.h only makes a symbolic name
 * available to the source code and does not change any functionality, nor
 * does it make your device available to a specific driver.
 * It will however make the descriptive string available if a device does not
 * provide the string itself.
 *
 * After adding a vendor ID VNDR and a product ID PRDCT you will have the
 * following extra defines:
 * #define USB_VENDOR_VNDR		0x????
 * #define USB_PRODUCT_VNDR_PRDCT	0x????
 *
 * You may have to add these defines to the respective probe routines to
 * make the device recognised by the appropriate device driver.
 */

#define	USB_VENDOR_UNKNOWN1	0x0053		/* Unknown vendor */
#define	USB_VENDOR_UNKNOWN2	0x0105		/* Unknown vendor */
#define	USB_VENDOR_EGALAX2	0x0123		/* eGalax, Inc. */
#define	USB_VENDOR_QUAN	0x01e1		/* Quan */
#define	USB_VENDOR_CHIPSBANK	0x0204		/* Chipsbank */
#define	USB_VENDOR_AOX	0x03e8		/* AOX */
#define	USB_VENDOR_ATMEL	0x03eb		/* Atmel */
#define	USB_VENDOR_MITSUMI	0x03ee		/* Mitsumi */
#define	USB_VENDOR_HP	0x03f0		/* Hewlett Packard */
#define	USB_VENDOR_ADAPTEC	0x03f3		/* Adaptec */
#define	USB_VENDOR_NATIONAL	0x0400		/* National Semiconductor */
#define	USB_VENDOR_ACERLABS	0x0402		/* Acer Labs */
#define	USB_VENDOR_FTDI	0x0403		/* Future Technology Devices */
#define	USB_VENDOR_NEC	0x0409		/* NEC */
#define	USB_VENDOR_KODAK	0x040a		/* Eastman Kodak */
#define	USB_VENDOR_WELTREND	0x040b		/* Weltrend Semiconductor */
#define	USB_VENDOR_VIA	0x040d		/* VIA */
#define	USB_VENDOR_MELCO	0x0411		/* Melco */
#define	USB_VENDOR_CREATIVE	0x041e		/* Creative Labs */
#define	USB_VENDOR_NOKIA	0x0421		/* Nokia */
#define	USB_VENDOR_ADI	0x0422		/* ADI Systems */
#define	USB_VENDOR_CATC	0x0423		/* Computer Access Technology */
#define	USB_VENDOR_SMSC	0x0424		/* SMSC */
#define	USB_VENDOR_GRAVIS	0x0428		/* Advanced Gravis Computer */
#define	USB_VENDOR_SUN	0x0430		/* Sun Microsystems */
#define	USB_VENDOR_TAUGA	0x0436		/* Taugagreining HF */
#define	USB_VENDOR_AMD	0x0438		/* Advanced Micro Devices */
#define	USB_VENDOR_LEXMARK	0x043d		/* Lexmark International */
#define	USB_VENDOR_NANAO	0x0440		/* NANAO */
#define	USB_VENDOR_ALPS	0x044e		/* Alps Electric */
#define	USB_VENDOR_THRUST	0x044f		/* Thrustmaster */
#define	USB_VENDOR_TI	0x0451		/* Texas Instruments */
#define	USB_VENDOR_ANALOGDEVICES	0x0456		/* Analog Devices */
#define	USB_VENDOR_SIS	0x0457		/* Silicon Integrated Systems */
#define	USB_VENDOR_KYE	0x0458		/* KYE Systems */
#define	USB_VENDOR_DIAMOND2	0x045a		/* Diamond (Supra) */
#define	USB_VENDOR_MICROSOFT	0x045e		/* Microsoft */
#define	USB_VENDOR_PRIMAX	0x0461		/* Primax Electronics */
#define	USB_VENDOR_MGE	0x0463		/* MGE UPS Systems */
#define	USB_VENDOR_AMP	0x0464		/* AMP */
#define	USB_VENDOR_CHERRY	0x046a		/* Cherry Mikroschalter */
#define	USB_VENDOR_MEGATRENDS	0x046b		/* American Megatrends */
#define	USB_VENDOR_LOGITECH	0x046d		/* Logitech */
#define	USB_VENDOR_BTC	0x046e		/* Behavior Tech. Computer */
#define	USB_VENDOR_PHILIPS	0x0471		/* Philips */
#define	USB_VENDOR_SANYO	0x0474		/* Sanyo Electric */
#define	USB_VENDOR_CONNECTIX	0x0478		/* Connectix */
#define	USB_VENDOR_KENSINGTON	0x047d		/* Kensington */
#define	USB_VENDOR_LUCENT	0x047e		/* Lucent */
#define	USB_VENDOR_PLANTRONICS	0x047f		/* Plantronics */
#define	USB_VENDOR_KYOCERA	0x0482		/* Kyocera */
#define	USB_VENDOR_STMICRO	0x0483		/* STMicroelectronics */
#define	USB_VENDOR_FOXCONN	0x0489		/* Foxconn / Hon Hai */
#define	USB_VENDOR_MEIZU	0x0492		/* Meizu Electronics */
#define	USB_VENDOR_YAMAHA	0x0499		/* YAMAHA */
#define	USB_VENDOR_COMPAQ	0x049f		/* Compaq */
#define	USB_VENDOR_HITACHI	0x04a4		/* Hitachi */
#define	USB_VENDOR_ACERP	0x04a5		/* Acer Peripherals */
#define	USB_VENDOR_VISIONEER	0x04a7		/* Visioneer */
#define	USB_VENDOR_CANON	0x04a9		/* Canon */
#define	USB_VENDOR_NIKON	0x04b0		/* Nikon */
#define	USB_VENDOR_IBM	0x04b3		/* IBM */
#define	USB_VENDOR_CYPRESS	0x04b4		/* Cypress Semiconductor */
#define	USB_VENDOR_EPSON	0x04b8		/* Seiko Epson */
#define	USB_VENDOR_RAINBOW	0x04b9		/* Rainbow Technologies */
#define	USB_VENDOR_IODATA	0x04bb		/* I-O Data */
#define	USB_VENDOR_TDK	0x04bf		/* TDK */
#define	USB_VENDOR_3COMUSR	0x04c1		/* U.S. Robotics */
#define	USB_VENDOR_METHODE	0x04c2		/* Methode Electronics Far East */
#define	USB_VENDOR_MAXISWITCH	0x04c3		/* Maxi Switch */
#define	USB_VENDOR_LOCKHEEDMER	0x04c4		/* Lockheed Martin Energy Research */
#define	USB_VENDOR_FUJITSU	0x04c5		/* Fujitsu */
#define	USB_VENDOR_TOSHIBAAM	0x04c6		/* Toshiba America */
#define	USB_VENDOR_MICROMACRO	0x04c7		/* Micro Macro Technologies */
#define	USB_VENDOR_KONICA	0x04c8		/* Konica */
#define	USB_VENDOR_LITEON	0x04ca		/* Lite-On Technology */
#define	USB_VENDOR_FUJIPHOTO	0x04cb		/* Fuji Photo Film */
#define	USB_VENDOR_PHILIPSSEMI	0x04cc		/* Philips Semiconductors */
#define	USB_VENDOR_TATUNG	0x04cd		/* Tatung Co. Of America */
#define	USB_VENDOR_SCANLOGIC	0x04ce		/* ScanLogic */
#define	USB_VENDOR_MYSON	0x04cf		/* Myson Technology */
#define	USB_VENDOR_DIGI2	0x04d0		/* Digi */
#define	USB_VENDOR_ITTCANON	0x04d1		/* ITT Canon */
#define	USB_VENDOR_ALTEC	0x04d2		/* Altec Lansing */
#define	USB_VENDOR_MICROCHIP	0x04d8		/* Microchip Technology */
#define	USB_VENDOR_HOLTEK	0x04d9		/* Holtek Semiconductor */
#define	USB_VENDOR_PANASONIC	0x04da		/* Panasonic (Matsushita) */
#define	USB_VENDOR_SHARP	0x04dd		/* Sharp */
#define	USB_VENDOR_IIYAMA	0x04e1		/* Iiyama */
#define	USB_VENDOR_SHUTTLE	0x04e6		/* Shuttle Technology */
#define	USB_VENDOR_SAMSUNG	0x04e8		/* Samsung Electronics */
#define	USB_VENDOR_ANNABOOKS	0x04ed		/* Annabooks */
#define	USB_VENDOR_JVC	0x04f1		/* JVC */
#define	USB_VENDOR_CHICONY	0x04f2		/* Chicony Electronics */
#define	USB_VENDOR_BROTHER	0x04f9		/* Brother Industries */
#define	USB_VENDOR_DALLAS	0x04fa		/* Dallas Semiconductor */
#define	USB_VENDOR_AIPTEK2	0x04fc		/* AIPTEK International */
#define	USB_VENDOR_ACER	0x0502		/* Acer */
#define	USB_VENDOR_3COM	0x0506		/* 3Com */
#define	USB_VENDOR_HOSIDEN	0x0507		/* Hosiden Corporation */
#define	USB_VENDOR_AZTECH	0x0509		/* Aztech Systems */
#define	USB_VENDOR_BELKIN	0x050d		/* Belkin Components */
#define	USB_VENDOR_KAWATSU	0x050f		/* Kawatsu Semiconductor */
#define	USB_VENDOR_COMPOSITE	0x0518		/* Composite */
#define	USB_VENDOR_APC	0x051d		/* American Power Conversion */
#define	USB_VENDOR_CONNECTEK	0x0522		/* Advanced Connectek USA */
#define	USB_VENDOR_NETCHIP	0x0525		/* NetChip Technology */
#define	USB_VENDOR_ALTRA	0x0527		/* ALTRA */
#define	USB_VENDOR_ATI	0x0528		/* ATI Technologies */
#define	USB_VENDOR_AKS	0x0529		/* Aladdin Knowledge Systems */
#define	USB_VENDOR_UNIACCESS	0x0540		/* Universal Access */
#define	USB_VENDOR_VIEWSONIC	0x0543		/* ViewSonic */
#define	USB_VENDOR_XIRLINK	0x0545		/* Xirlink */
#define	USB_VENDOR_ANCHOR	0x0547		/* Anchor Chips */
#define	USB_VENDOR_SONY	0x054c		/* Sony */
#define	USB_VENDOR_VISION	0x0553		/* VLSI Vision */
#define	USB_VENDOR_ASAHIKASEI	0x0556		/* Asahi Kasei Microsystems */
#define	USB_VENDOR_ATEN	0x0557		/* ATEN International */
#define	USB_VENDOR_MUSTEK	0x055f		/* Mustek Systems */
#define	USB_VENDOR_TELEX	0x0562		/* Telex Communications */
#define	USB_VENDOR_PERACOM	0x0565		/* Peracom Networks */
#define	USB_VENDOR_ALCOR2	0x0566		/* Alcor Micro */
#define	USB_VENDOR_XYRATEX	0x0567		/* Xyratex */
#define	USB_VENDOR_WACOM	0x056a		/* WACOM */
#define	USB_VENDOR_ETEK	0x056c		/* e-TEK Labs */
#define	USB_VENDOR_EIZO	0x056d		/* EIZO */
#define	USB_VENDOR_ELECOM	0x056e		/* Elecom */
#define	USB_VENDOR_CONEXANT	0x0572		/* Conexant */
#define	USB_VENDOR_HAUPPAUGE	0x0573		/* Hauppauge Computer Works */
#define	USB_VENDOR_BAFO	0x0576		/* BAFO/Quality Computer Accessories */
#define	USB_VENDOR_YEDATA	0x057b		/* Y-E Data */
#define	USB_VENDOR_AVM	0x057c		/* AVM */
#define	USB_VENDOR_NINTENDO	0x057e		/* Nintendo */
#define	USB_VENDOR_QUICKSHOT	0x057f		/* Quickshot */
#define	USB_VENDOR_ROLAND	0x0582		/* Roland */
#define	USB_VENDOR_ROCKFIRE	0x0583		/* Rockfire */
#define	USB_VENDOR_RATOC	0x0584		/* RATOC Systems */
#define	USB_VENDOR_ZYXEL	0x0586		/* ZyXEL Communication */
#define	USB_VENDOR_ALCOR	0x058f		/* Alcor Micro */
#define	USB_VENDOR_IOMEGA	0x059b		/* Iomega */
#define	USB_VENDOR_ATREND	0x059c		/* A-Trend Technology */
#define	USB_VENDOR_AID	0x059d		/* Advanced Input Devices */
#define	USB_VENDOR_LACIE	0x059f		/* LaCie */
#define	USB_VENDOR_CISCOLINKSYS3	0x05a6		/* Cisco-Linksys */
#define	USB_VENDOR_OMNIVISION	0x05a9		/* OmniVision */
#define	USB_VENDOR_INSYSTEM	0x05ab		/* In-System Design */
#define	USB_VENDOR_APPLE	0x05ac		/* Apple Computer */
#define	USB_VENDOR_DIGI	0x05c5		/* Digi International */
#define	USB_VENDOR_QUALCOMM2	0x05c6		/* Qualcomm */
#define	USB_VENDOR_QTRONIX	0x05c7		/* Qtronix */
#define	USB_VENDOR_ELSA	0x05cc		/* ELSA */
#define	USB_VENDOR_BRAINBOXES	0x05d1		/* Brainboxes Limited */
#define	USB_VENDOR_ULTIMA	0x05d8		/* Ultima */
#define	USB_VENDOR_AXIOHM	0x05d9		/* Axiohm Transaction Solutions */
#define	USB_VENDOR_MICROTEK	0x05da		/* Microtek */
#define	USB_VENDOR_SUNTAC	0x05db		/* SUN Corporation */
#define	USB_VENDOR_LEXAR	0x05dc		/* Lexar Media */
#define	USB_VENDOR_ADDTRON	0x05dd		/* Addtron */
#define	USB_VENDOR_SYMBOL	0x05e0		/* Symbol Technologies */
#define	USB_VENDOR_GENESYS	0x05e3		/* Genesys Logic */
#define	USB_VENDOR_FUJI	0x05e5		/* Fuji Electric */
#define	USB_VENDOR_KEITHLEY	0x05e6		/* Keithley Instruments */
#define	USB_VENDOR_EIZONANAO	0x05e7		/* EIZO Nanao */
#define	USB_VENDOR_KLSI	0x05e9		/* Kawasaki LSI */
#define	USB_VENDOR_FFC	0x05eb		/* FFC */
#define	USB_VENDOR_ANKO	0x05ef		/* Anko Electronic */
#define	USB_VENDOR_PIENGINEERING	0x05f3		/* P.I. Engineering */
#define	USB_VENDOR_AOC	0x05f6		/* AOC International */
#define	USB_VENDOR_CHIC	0x05fe		/* Chic Technology */
#define	USB_VENDOR_BARCO	0x0600		/* Barco Display Systems */
#define	USB_VENDOR_BRIDGE	0x0607		/* Bridge Information */
#define	USB_VENDOR_SMK	0x0609		/* SMK */
#define	USB_VENDOR_SOLIDYEAR	0x060b		/* Solid Year */
#define	USB_VENDOR_BIORAD	0x0614		/* Bio-Rad Laboratories */
#define	USB_VENDOR_MACALLY	0x0618		/* Macally */
#define	USB_VENDOR_ACTLABS	0x061c		/* Act Labs */
#define	USB_VENDOR_ALARIS	0x0620		/* Alaris */
#define	USB_VENDOR_APEX	0x0624		/* Apex */
#define	USB_VENDOR_CREATIVE3	0x062a		/* Creative Labs */
#define	USB_VENDOR_VIVITAR	0x0636		/* Vivitar */
#define	USB_VENDOR_AVISION	0x0638		/* Avision */
#define	USB_VENDOR_TEAC	0x0644		/* TEAC */
#define	USB_VENDOR_SGI	0x065e		/* Silicon Graphics */
#define	USB_VENDOR_SANWASUPPLY	0x0663		/* Sanwa Supply */
#define	USB_VENDOR_LINKSYS	0x066b		/* Linksys */
#define	USB_VENDOR_ACERSA	0x066e		/* Acer Semiconductor America */
#define	USB_VENDOR_SIGMATEL	0x066f		/* Sigmatel */
#define	USB_VENDOR_DRAYTEK	0x0675		/* DrayTek */
#define	USB_VENDOR_AIWA	0x0677		/* Aiwa */
#define	USB_VENDOR_ACARD	0x0678		/* ACARD Technology */
#define	USB_VENDOR_PROLIFIC	0x067b		/* Prolific Technology */
#define	USB_VENDOR_SIEMENS	0x067c		/* Siemens */
#define	USB_VENDOR_AVANCELOGIC	0x0680		/* Avance Logic */
#define	USB_VENDOR_SIEMENS2	0x0681		/* Siemens */
#define	USB_VENDOR_MINOLTA	0x0686		/* Minolta */
#define	USB_VENDOR_CHPRODUCTS	0x068e		/* CH Products */
#define	USB_VENDOR_HAGIWARA	0x0693		/* Hagiwara Sys-Com */
#define	USB_VENDOR_CTX	0x0698		/* Chuntex */
#define	USB_VENDOR_ASKEY	0x069a		/* Askey Computer */
#define	USB_VENDOR_SAITEK	0x06a3		/* Saitek */
#define	USB_VENDOR_ALCATELT	0x06b9		/* Alcatel Telecom */
#define	USB_VENDOR_AGFA	0x06bd		/* AGFA-Gevaert */
#define	USB_VENDOR_ASIAMD	0x06be		/* Asia Microelectronic Development */
#define	USB_VENDOR_BIZLINK	0x06c4		/* Bizlink International */
#define	USB_VENDOR_KEYSPAN	0x06cd		/* Keyspan */
#define	USB_VENDOR_AASHIMA	0x06d6		/* Aashima Technology */
#define	USB_VENDOR_MULTITECH	0x06e0		/* MultiTech */
#define	USB_VENDOR_ADS	0x06e1		/* ADS Technologies */
#define	USB_VENDOR_ALCATELM	0x06e4		/* Alcatel Microelectronics */
#define	USB_VENDOR_SIRIUS	0x06ea		/* Sirius Technologies */
#define	USB_VENDOR_GUILLEMOT	0x06f8		/* Guillemot */
#define	USB_VENDOR_BOSTON	0x06fd		/* Boston Acoustics */
#define	USB_VENDOR_SMC	0x0707		/* Standard Microsystems */
#define	USB_VENDOR_PUTERCOM	0x0708		/* Putercom */
#define	USB_VENDOR_MCT	0x0711		/* MCT */
#define	USB_VENDOR_IMATION	0x0718		/* Imation */
#define	USB_VENDOR_SUSTEEN	0x0731		/* Susteen */
#define	USB_VENDOR_EICON	0x0734		/* Eicon Networks */
#define	USB_VENDOR_MADCATZ	0x0738		/* Mad Catz, Inc. */
#define	USB_VENDOR_DIGITALSTREAM	0x074e		/* Digital Stream */
#define	USB_VENDOR_AUREAL	0x0755		/* Aureal Semiconductor */
#define	USB_VENDOR_MIDIMAN	0x0763		/* Midiman */
#define	USB_VENDOR_CYBERPOWER	0x0764		/* CyberPower Systems, Inc. */
#define	USB_VENDOR_SURECOM	0x0769		/* Surecom Technology */
#define	USB_VENDOR_LINKSYS2	0x077b		/* Linksys */
#define	USB_VENDOR_GRIFFIN	0x077d		/* Griffin Technology */
#define	USB_VENDOR_SANDISK	0x0781		/* SanDisk */
#define	USB_VENDOR_JENOPTIK	0x0784		/* Jenoptik */
#define	USB_VENDOR_LOGITEC	0x0789		/* Logitec */
#define	USB_VENDOR_BRIMAX	0x078e		/* Brimax */
#define	USB_VENDOR_AXIS	0x0792		/* Axis Communications */
#define	USB_VENDOR_ABL	0x0794		/* ABL Electronics */
#define	USB_VENDOR_SAGEM	0x079b		/* Sagem */
#define	USB_VENDOR_SUNCOMM	0x079c		/* Sun Communications, Inc. */
#define	USB_VENDOR_ALFADATA	0x079d		/* Alfadata Computer */
#define	USB_VENDOR_NATIONALTECH	0x07a2		/* National Technical Systems */
#define	USB_VENDOR_ONNTO	0x07a3		/* Onnto */
#define	USB_VENDOR_BE	0x07a4		/* Be */
#define	USB_VENDOR_ADMTEK	0x07a6		/* ADMtek */
#define	USB_VENDOR_COREGA	0x07aa		/* Corega */
#define	USB_VENDOR_FREECOM	0x07ab		/* Freecom */
#define	USB_VENDOR_MICROTECH	0x07af		/* Microtech */
#define	USB_VENDOR_GENERALINSTMNTS	0x07b2		/* General Instruments (Motorola) */
#define	USB_VENDOR_OLYMPUS	0x07b4		/* Olympus */
#define	USB_VENDOR_ABOCOM	0x07b8		/* AboCom Systems */
#define	USB_VENDOR_KINGSUN	0x07c0		/* KingSun */
#define	USB_VENDOR_KEISOKUGIKEN	0x07c1		/* Keisokugiken */
#define	USB_VENDOR_ONSPEC	0x07c4		/* OnSpec */
#define	USB_VENDOR_APG	0x07c5		/* APG Cash Drawer */
#define	USB_VENDOR_BUG	0x07c8		/* B.U.G. */
#define	USB_VENDOR_ALLIEDTELESYN	0x07c9		/* Allied Telesyn International */
#define	USB_VENDOR_AVERMEDIA	0x07ca		/* AVerMedia Technologies */
#define	USB_VENDOR_SIIG	0x07cc		/* SIIG */
#define	USB_VENDOR_CASIO	0x07cf		/* CASIO */
#define	USB_VENDOR_DLINK2	0x07d1		/* D-Link */
#define	USB_VENDOR_APTIO	0x07d2		/* Aptio Products */
#define	USB_VENDOR_ARASAN	0x07da		/* Arasan Chip Systems */
#define	USB_VENDOR_ALLIEDCABLE	0x07e6		/* Allied Cable */
#define	USB_VENDOR_STSN	0x07ef		/* STSN */
#define	USB_VENDOR_BEWAN	0x07fa		/* Bewan */
#define	USB_VENDOR_ZOOM	0x0803		/* Zoom Telephonics */
#define	USB_VENDOR_BROADLOGIC	0x0827		/* BroadLogic */
#define	USB_VENDOR_HANDSPRING	0x082d		/* Handspring */
#define	USB_VENDOR_PALM	0x0830		/* Palm Computing */
#define	USB_VENDOR_SOURCENEXT	0x0833		/* SOURCENEXT */
#define	USB_VENDOR_ACTIONSTAR	0x0835		/* Action Star Enterprise */
#define	USB_VENDOR_ACCTON	0x083a		/* Accton Technology */
#define	USB_VENDOR_DIAMOND	0x0841		/* Diamond */
#define	USB_VENDOR_NETGEAR	0x0846		/* BayNETGEAR */
#define	USB_VENDOR_ACTIVEWIRE	0x0854		/* ActiveWire */
#define	USB_VENDOR_BBELECTRONICS	0x0856		/* B&B Electronics */
#define	USB_VENDOR_PORTGEAR	0x085a		/* PortGear */
#define	USB_VENDOR_NETGEAR2	0x0864		/* Netgear */
#define	USB_VENDOR_SYSTEMTALKS	0x086e		/* System Talks */
#define	USB_VENDOR_METRICOM	0x0870		/* Metricom */
#define	USB_VENDOR_ADESSOKBTEK	0x087c		/* ADESSO/Kbtek America */
#define	USB_VENDOR_JATON	0x087d		/* Jaton */
#define	USB_VENDOR_APT	0x0880		/* APT Technologies */
#define	USB_VENDOR_BOCARESEARCH	0x0885		/* Boca Research */
#define	USB_VENDOR_ANDREA	0x08a8		/* Andrea Electronics */
#define	USB_VENDOR_BURRBROWN	0x08bb		/* Burr-Brown Japan */
#define	USB_VENDOR_2WIRE	0x08c8		/* 2Wire */
#define	USB_VENDOR_AIPTEK	0x08ca		/* AIPTEK International */
#define	USB_VENDOR_SMARTBRIDGES	0x08d1		/* SmartBridges */
#define	USB_VENDOR_BILLIONTON	0x08dd		/* Billionton Systems */
#define	USB_VENDOR_EXTENDED	0x08e9		/* Extended Systems */
#define	USB_VENDOR_MSYSTEMS	0x08ec		/* M-Systems */
#define	USB_VENDOR_AUTHENTEC	0x08ff		/* AuthenTec */
#define	USB_VENDOR_AUDIOTECHNICA	0x0909		/* Audio-Technica */
#define	USB_VENDOR_TRUMPION	0x090a		/* Trumpion Microelectronics */
#define	USB_VENDOR_ALATION	0x0910		/* Alation Systems */
#define	USB_VENDOR_GLOBESPAN	0x0915		/* Globespan */
#define	USB_VENDOR_CONCORDCAMERA	0x0919		/* Concord Camera */
#define	USB_VENDOR_GARMIN	0x091e		/* Garmin */
#define	USB_VENDOR_GOHUBS	0x0921		/* GoHubs */
#define	USB_VENDOR_BIOMETRIC	0x0929		/* American Biometric Company */
#define	USB_VENDOR_TOSHIBA	0x0930		/* Toshiba */
#define	USB_VENDOR_PLEXTOR	0x093b		/* Plextor */
#define	USB_VENDOR_INTREPIDCS	0x093c		/* Intrepid */
#define	USB_VENDOR_YANO	0x094f		/* Yano */
#define	USB_VENDOR_KINGSTON	0x0951		/* Kingston Technology */
#define	USB_VENDOR_BLUEWATER	0x0956		/* BlueWater Systems */
#define	USB_VENDOR_AGILENT	0x0957		/* Agilent Technologies */
#define	USB_VENDOR_GUDE	0x0959		/* Gude ADS */
#define	USB_VENDOR_PORTSMITH	0x095a		/* Portsmith */
#define	USB_VENDOR_ACERW	0x0967		/* Acer */
#define	USB_VENDOR_ADIRONDACK	0x0976		/* Adirondack Wire & Cable */
#define	USB_VENDOR_BECKHOFF	0x0978		/* Beckhoff */
#define	USB_VENDOR_MINDSATWORK	0x097a		/* Minds At Work */
#define	USB_VENDOR_ZIPPY	0x099a		/* Zippy Technology Corporation */
#define	USB_VENDOR_POINTCHIPS	0x09a6		/* PointChips */
#define	USB_VENDOR_INTERSIL	0x09aa		/* Intersil */
#define	USB_VENDOR_TRIPPLITE2	0x09ae		/* Tripp Lite */
#define	USB_VENDOR_ALTIUS	0x09b3		/* Altius Solutions */
#define	USB_VENDOR_ARRIS	0x09c1		/* Arris Interactive */
#define	USB_VENDOR_ACTIVCARD	0x09c3		/* ACTIVCARD */
#define	USB_VENDOR_ACTISYS	0x09c4		/* ACTiSYS */
#define	USB_VENDOR_NOVATEL	0x09d7		/* NovAtel */
#define	USB_VENDOR_AFOURTECH	0x09da		/* A-FOUR TECH */
#define	USB_VENDOR_AIMEX	0x09dc		/* AIMEX */
#define	USB_VENDOR_ADDONICS	0x09df		/* Addonics Technologies */
#define	USB_VENDOR_AKAI	0x09e8		/* AKAI professional M.I. */
#define	USB_VENDOR_ARESCOM	0x09f5		/* ARESCOM */
#define	USB_VENDOR_BAY	0x09f9		/* Bay Associates */
#define	USB_VENDOR_ALTERA	0x09fb		/* Altera */
#define	USB_VENDOR_CSR	0x0a12		/* Cambridge Silicon Radio */
#define	USB_VENDOR_TREK	0x0a16		/* Trek Technology */
#define	USB_VENDOR_ASAHIOPTICAL	0x0a17		/* Asahi Optical */
#define	USB_VENDOR_BOCASYSTEMS	0x0a43		/* Boca Systems */
#define	USB_VENDOR_SHANTOU	0x0a46		/* ShanTou */
#define	USB_VENDOR_MEDIAGEAR	0x0a48		/* MediaGear */
#define	USB_VENDOR_BROADCOM	0x0a5c		/* Broadcom */
#define	USB_VENDOR_GREENHOUSE	0x0a6b		/* GREENHOUSE */
#define	USB_VENDOR_GEOCAST	0x0a79		/* Geocast Network Systems */
#define	USB_VENDOR_ZYDAS	0x0ace		/* Zydas Technology Corporation */
#define	USB_VENDOR_NEODIO	0x0aec		/* Neodio */
#define	USB_VENDOR_OPTIONNV	0x0af0		/* Option N.V: */
#define	USB_VENDOR_ASUSTEK	0x0b05		/* ASUSTeK Computer */
#define	USB_VENDOR_TODOS	0x0b0c		/* Todos Data System */
#define	USB_VENDOR_SIIG2	0x0b39		/* SIIG */
#define	USB_VENDOR_TEKRAM	0x0b3b		/* Tekram Technology */
#define	USB_VENDOR_HAL	0x0b41		/* HAL Corporation */
#define	USB_VENDOR_EMS	0x0b43		/* EMS Production */
#define	USB_VENDOR_NEC2	0x0b62		/* NEC */
#define	USB_VENDOR_ATI2	0x0b6f		/* ATI */
#define	USB_VENDOR_ZEEVO	0x0b7a		/* Zeevo, Inc. */
#define	USB_VENDOR_KURUSUGAWA	0x0b7e		/* Kurusugawa Electronics, Inc. */
#define	USB_VENDOR_ASIX	0x0b95		/* ASIX Electronics */
#define	USB_VENDOR_PROLIFIC2	0x0b8c		/* Prolific Technology Inc */
#define	USB_VENDOR_O2MICRO	0x0b97		/* O2 Micro */
#define	USB_VENDOR_USR	0x0baf		/* U.S. Robotics */
#define	USB_VENDOR_AMBIT	0x0bb2		/* Ambit Microsystems */
#define	USB_VENDOR_HTC	0x0bb4		/* HTC */
#define	USB_VENDOR_REALTEK	0x0bda		/* Realtek */
#define	USB_VENDOR_ADDONICS2	0x0bf6		/* Addonics Technology */
#define	USB_VENDOR_FSC	0x0bf8		/* Fujitsu Siemens Computers */
#define	USB_VENDOR_AGATE	0x0c08		/* Agate Technologies */
#define	USB_VENDOR_DMI	0x0c0b		/* DMI */
#define	USB_VENDOR_CHICONY2	0x0c45		/* Chicony Electronics */
#define	USB_VENDOR_MICRODIA	0x0c45		/* Microdia / Sonix Technology Co., Ltd. */
#define	USB_VENDOR_SEALEVEL	0x0c52		/* Sealevel System */
#define	USB_VENDOR_LUWEN	0x0c76		/* EasyDisk */
#define	USB_VENDOR_QUALCOMM_K	0x0c88		/* Qualcomm Kyocera */
#define	USB_VENDOR_ZCOM	0x0cde		/* Z-Com */
#define	USB_VENDOR_ATHEROS2	0x0cf3		/* Atheros Communications */
#define	USB_VENDOR_TANGTOP	0x0d3d		/* Tangtop */
#define	USB_VENDOR_SMC3	0x0d5c		/* Standard Microsystems */
#define	USB_VENDOR_PEN	0x0d7d		/* Pen Drive */
#define	USB_VENDOR_ACDC	0x0d7e		/* American Computer & Digital Components */
#define	USB_VENDOR_CMEDIA	0x0d8c		/* C-Media Electronics Inc. */
#define	USB_VENDOR_CONCEPTRONIC2	0x0d8e		/* Conceptronic */
#define	USB_VENDOR_MSI	0x0db0		/* Micro Star */
#define	USB_VENDOR_ELCON	0x0db7		/* ELCON Systemtechnik */
#define	USB_VENDOR_UNKNOWN5	0x0dcd		/* Unknown Vendor */
#define	USB_VENDOR_SITECOMEU	0x0df6		/* Sitecom Europe */
#define	USB_VENDOR_AMIGO	0x0e0b		/* Amigo Technology */
#define	USB_VENDOR_HAWKING	0x0e66		/* Hawking */
#define	USB_VENDOR_GMATE	0x0e7e		/* G.Mate, Inc */
#define	USB_VENDOR_MTK	0x0e8d		/* MTK */
#define	USB_VENDOR_OTI	0x0ea0		/* Ours Technology */
#define	USB_VENDOR_PILOTECH	0x0eaf		/* Pilotech */
#define	USB_VENDOR_NOVATECH	0x0eb0		/* Nova Tech */
#define	USB_VENDOR_EGALAX	0x0eef		/* eGalax */
#define	USB_VENDOR_TOD	0x0ede		/* TOD */
#define	USB_VENDOR_AIRPRIME	0x0f3d		/* AirPrime, Incorporated */
#define	USB_VENDOR_VTECH	0x0f88		/* VTech */
#define	USB_VENDOR_FALCOM	0x0f94		/* Falcom Wireless Communications GmbH */
#define	USB_VENDOR_RIM	0x0fca		/* Research In Motion */
#define	USB_VENDOR_DYNASTREAM	0x0fcf		/* Dynastream Innovations */
#define	USB_VENDOR_SUNRISING	0x0fe6		/* SUNRISING */
#define	USB_VENDOR_DVICO	0x0fe9		/* DViCO */
#define	USB_VENDOR_QUALCOMM	0x1004		/* Qualcomm */
#define	USB_VENDOR_MOTOROLA4	0x100d		/* Motorola */
#define	USB_VENDOR_HP3	0x103c		/* Hewlett Packard */
#define	USB_VENDOR_GIGABYTE	0x1044		/* GIGABYTE */
#define	USB_VENDOR_WESTERN	0x1058		/* Western Digital */
#define	USB_VENDOR_MOTOROLA	0x1063		/* Motorola */
#define	USB_VENDOR_CCYU	0x1065		/* CCYU Technology */
#define	USB_VENDOR_HYUNDAI	0x106c		/* Hyundai CuriTel */
#define	USB_VENDOR_SILABS2	0x10a6		/* SILABS2 */
#define	USB_VENDOR_USI	0x10ab		/* USI */
#define	USB_VENDOR_PLX	0x10b5		/* PLX */
#define	USB_VENDOR_ASANTE	0x10bd		/* Asante */
#define	USB_VENDOR_SILABS	0x10c4		/* Silicon Labs */
#define	USB_VENDOR_TENX	0x1130		/* Ten X Technology, Inc. */
#define	USB_VENDOR_JRC	0x1145		/* Japan Radio Company */
#define	USB_VENDOR_SPHAIRON	0x114b		/* Sphairon Access Systems GmbH */
#define	USB_VENDOR_DELORME	0x1163		/* DeLorme */
#define	USB_VENDOR_SERVERWORKS	0x1166		/* ServerWorks */
#define	USB_VENDOR_ACERCM	0x1189		/* Acer Communications & Multimedia */
#define	USB_VENDOR_SIERRA	0x1199		/* Sierra Wireless */
#define	USB_VENDOR_TOPFIELD	0x11db		/* Topfield Co., Ltd */
#define	USB_VENDOR_NETINDEX	0x11f6		/* NetIndex */
#define	USB_VENDOR_INTERBIO	0x1209		/* InterBiometrics */
#define	USB_VENDOR_FUJITSU2	0x1221		/* Fujitsu Ltd. */
#define	USB_VENDOR_UNKNOWN3	0x1233		/* Unknown vendor */
#define	USB_VENDOR_TSUNAMI	0x1241		/* Tsunami */
#define	USB_VENDOR_PHEENET	0x124a		/* Pheenet */
#define	USB_VENDOR_TARGUS	0x1267		/* Targus */
#define	USB_VENDOR_TWINMOS	0x126f		/* TwinMOS */
#define	USB_VENDOR_CREATIVE2	0x1292		/* Creative Labs */
#define	USB_VENDOR_BELKIN2	0x1293		/* Belkin Components */
#define	USB_VENDOR_CYBERTAN	0x129b		/* CyberTAN Technology */
#define	USB_VENDOR_HUAWEI	0x12d1		/* Huawei Technologies */
#define	USB_VENDOR_AINCOMM	0x12fd		/* Aincomm */
#define	USB_VENDOR_MOBILITY	0x1342		/* Mobility */
#define	USB_VENDOR_DICKSMITH	0x1371		/* Dick Smith Electronics */
#define	USB_VENDOR_NETGEAR3	0x1385		/* Netgear */
#define	USB_VENDOR_VALIDITY	0x138a		/* Validity Sensors, Inc. */
#define	USB_VENDOR_BALTECH	0x13ad		/* Baltech */
#define	USB_VENDOR_CISCOLINKSYS	0x13b1		/* Cisco-Linksys */
#define	USB_VENDOR_SHARK	0x13d2		/* Shark */
#define	USB_VENDOR_AZUREWAVE	0x13d3		/* AzureWave */
#define	USB_VENDOR_PHISON	0x13fe		/* Phison Electronics Corp. */
#define	USB_VENDOR_NOVATEL2	0x1410		/* Novatel */
#define	USB_VENDOR_OMNIVISION2	0x1415		/* OmniVision Technologies, Inc. */
#define	USB_VENDOR_MERLIN	0x1416		/* Merlin */
#define	USB_VENDOR_WISTRONNEWEB	0x1435		/* Wistron NeWeb */
#define	USB_VENDOR_HUAWEI3COM	0x1472		/* Huawei-3Com */
#define	USB_VENDOR_ABOCOM2	0x1482		/* AboCom Systems */
#define	USB_VENDOR_SILICOM	0x1485		/* Silicom */
#define	USB_VENDOR_RALINK	0x148f		/* Ralink Technology */
#define	USB_VENDOR_CONCEPTRONIC	0x14b2		/* Conceptronic */
#define	USB_VENDOR_SUPERTOP	0x14cd		/* SuperTop */
#define	USB_VENDOR_PLANEX3	0x14ea		/* Planex Communications */
#define	USB_VENDOR_SILICONPORTALS	0x1527		/* Silicon Portals */
#define	USB_VENDOR_JMICRON	0x152d		/* JMicron */
#define	USB_VENDOR_OQO	0x1557		/* OQO */
#define	USB_VENDOR_UMEDIA	0x157e		/* U-MEDIA Communications */
#define	USB_VENDOR_FIBERLINE	0x1582		/* Fiberline */
#define	USB_VENDOR_SPARKLAN	0x15a9		/* SparkLAN */
#define	USB_VENDOR_AMIT2	0x15c5		/* AMIT */
#define	USB_VENDOR_SOHOWARE	0x15e8		/* SOHOware */
#define	USB_VENDOR_UMAX	0x1606		/* UMAX Data Systems */
#define	USB_VENDOR_INSIDEOUT	0x1608		/* Inside Out Networks */
#define	USB_VENDOR_GOODWAY	0x1631		/* Good Way Technology */
#define	USB_VENDOR_ENTREGA	0x1645		/* Entrega */
#define	USB_VENDOR_ACTIONTEC	0x1668		/* Actiontec Electronics */
#define	USB_VENDOR_CISCOLINKSYS2	0x167b		/* Cisco-Linksys */
#define	USB_VENDOR_ATHEROS	0x168c		/* Atheros Communications */
#define	USB_VENDOR_GIGASET	0x1690		/* Gigaset */
#define	USB_VENDOR_ANYDATA	0x16d5		/* AnyDATA Inc. */
#define	USB_VENDOR_JABLOTRON	0x16d6		/* Jablotron */
#define	USB_VENDOR_LINKSYS4	0x1737		/* Linksys */
#define	USB_VENDOR_SENAO	0x1740		/* Senao */
#define	USB_VENDOR_ASUSTEK2	0x1761		/* ASUSTeK Computer */
#define	USB_VENDOR_SWEEX2	0x177f		/* Sweex */
#define	USB_VENDOR_MISC	0x1781		/* Misc Vendors */
#define	USB_VENDOR_DISPLAYLINK	0x17e9		/* DisplayLink */
#define	USB_VENDOR_LENOVO	0x17ef		/* Lenovo */
#define	USB_VENDOR_E3C	0x18b4		/* E3C Technologies */
#define	USB_VENDOR_AMIT	0x18c5		/* AMIT */
#define	USB_VENDOR_QCOM	0x18e8		/* Qcom */
#define	USB_VENDOR_LINKSYS3	0x1915		/* Linksys */
#define	USB_VENDOR_MEINBERG	0x1938		/* Meinberg Funkuhren */
#define	USB_VENDOR_BECEEM	0x198f		/* Beceem Communications */
#define	USB_VENDOR_ZTE	0x19d2		/* ZTE */
#define	USB_VENDOR_QUANTA	0x1a32		/* Quanta */
#define	USB_VENDOR_TERMINUS	0x1a40		/* Terminus Technology */
#define	USB_VENDOR_WINCHIPHEAD2	0x1a86		/* QinHeng Electronics */
#define	USB_VENDOR_OVISLINK	0x1b75		/* OvisLink */
#define	USB_VENDOR_MPMAN	0x1cae		/* MPMan */
#define	USB_VENDOR_4GSYSTEMS	0x1c9e		/* 4G Systems */
#define	USB_VENDOR_PEGATRON	0x1d4d		/* Pegatron */
#define	USB_VENDOR_FUTUREBITS	0x1d50		/* Future Bits */
#define	USB_VENDOR_AIRTIES	0x1eda		/* AirTies */
#define	USB_VENDOR_DLINK	0x2001		/* D-Link */
#define	USB_VENDOR_PLANEX2	0x2019		/* Planex Communications */
#define	USB_VENDOR_ENCORE	0x203d		/* Encore */
#define	USB_VENDOR_HAUPPAUGE2	0x2040		/* Hauppauge Computer Works */
#define	USB_VENDOR_PARA	0x20b8		/* PARA Industrial */
#define	USB_VENDOR_TRENDNET	0x20f4		/* TRENDnet */
#define	USB_VENDOR_DLINK3	0x2101		/* D-Link */
#define	USB_VENDOR_VIALABS	0x2109		/* VIA Labs */
#define	USB_VENDOR_ERICSSON	0x2282		/* Ericsson */
#define	USB_VENDOR_MOTOROLA2	0x22b8		/* Motorola */
#define	USB_VENDOR_PINNACLE	0x2304		/* Pinnacle Systems */
#define	USB_VENDOR_ARDUINO	0x2341		/* Arduino SA */
#define	USB_VENDOR_TPLINK	0x2357		/* TP-Link */
#define	USB_VENDOR_TRIPPLITE	0x2478		/* Tripp-Lite */
#define	USB_VENDOR_HIROSE	0x2631		/* Hirose Electric */
#define	USB_VENDOR_NHJ	0x2770		/* NHJ */
#define	USB_VENDOR_PLANEX	0x2c02		/* Planex Communications */
#define	USB_VENDOR_VIDZMEDIA	0x3275		/* VidzMedia Pte Ltd */
#define	USB_VENDOR_AEI	0x3334		/* AEI */
#define	USB_VENDOR_HANK	0x3353		/* Hank Connection */
#define	USB_VENDOR_PQI	0x3538		/* PQI */
#define	USB_VENDOR_DAISY	0x3579		/* Daisy Technology */
#define	USB_VENDOR_NI	0x3923		/* National Instruments */
#define	USB_VENDOR_MICRONET	0x3980		/* Micronet Communications */
#define	USB_VENDOR_IODATA2	0x40bb		/* I-O Data */
#define	USB_VENDOR_IRIVER	0x4102		/* iRiver */
#define	USB_VENDOR_DELL	0x413c		/* Dell */
#define	USB_VENDOR_WINCHIPHEAD	0x4348		/* WinChipHead */
#define	USB_VENDOR_FEIXUN	0x4855		/* FeiXun Communication */
#define	USB_VENDOR_AVERATEC	0x50c2		/* Averatec */
#define	USB_VENDOR_SWEEX	0x5173		/* Sweex */
#define	USB_VENDOR_ONSPEC2	0x55aa		/* OnSpec Electronic Inc. */
#define	USB_VENDOR_ZINWELL	0x5a57		/* Zinwell */
#define	USB_VENDOR_INGENIC	0x601a		/* Ingenic Semiconductor Ltd. */
#define	USB_VENDOR_SITECOM	0x6189		/* Sitecom */
#define	USB_VENDOR_SPRINGERDESIGN	0x6400		/* Springer Design, Inc. */
#define	USB_VENDOR_ARKMICROCHIPS	0x6547		/* ArkMicroChips */
#define	USB_VENDOR_3COM2	0x6891		/* 3Com */
#define	USB_VENDOR_EDIMAX	0x7392		/* EDIMAX */
#define	USB_VENDOR_INTEL	0x8086		/* Intel */
#define	USB_VENDOR_INTEL2	0x8087		/* Intel */
#define	USB_VENDOR_ALLWIN	0x8516		/* ALLWIN Tech */
#define	USB_VENDOR_MOSCHIP	0x9710		/* MosChip Semiconductor */
#define	USB_VENDOR_NETGEAR4	0x9846		/* Netgear */
#define	USB_VENDOR_xxFTDI	0x9e88		/* FTDI */
#define	USB_VENDOR_CACE	0xcace		/* CACE Technologies */
#define	USB_VENDOR_COMPARE	0xcdab		/* Compare */
#define	USB_VENDOR_DATAAPEX	0xdaae		/* DataApex */
#define	USB_VENDOR_EVOLUTION	0xdeee		/* Evolution Robotics */
#define	USB_VENDOR_EMPIA	0xeb1a		/* eMPIA Technology */
#define	USB_VENDOR_HP2	0xf003		/* Hewlett Packard */
#define	USB_VENDOR_USRP	0xfffe		/* GNU Radio USRP */

/*
 * List of known products.  Grouped by vendor.
 */

/* 3Com products */
#define	USB_PRODUCT_3COM_HOMECONN	0x009d		/* HomeConnect USB Camera */
#define	USB_PRODUCT_3COM_3CREB96	0x00a0		/* Bluetooth USB Adapter */
#define	USB_PRODUCT_3COM_3C19250	0x03e8		/* 3C19250 Ethernet adapter */
#define	USB_PRODUCT_3COM_3CRSHEW696	0x0a01		/* 3CRSHEW696 */
#define	USB_PRODUCT_3COM_3C460	0x11f8		/* HomeConnect 3C460 */
#define	USB_PRODUCT_3COM_USR56K	0x3021		/* U.S.Robotics 56000 Voice Faxmodem Pro */
#define	USB_PRODUCT_3COM_3C460B	0x4601		/* HomeConnect 3C460B */
#define	USB_PRODUCT_3COM2_3CRUSB10075	0xa727		/* 3CRUSB10075 */

#define	USB_PRODUCT_3COMUSR_OFFICECONN	0x0082		/* 3Com OfficeConnect Analog Modem */
#define	USB_PRODUCT_3COMUSR_USRISDN	0x008f		/* 3Com U.S. Robotics Pro ISDN TA */
#define	USB_PRODUCT_3COMUSR_HOMECONN	0x009d		/* 3Com HomeConnect camera */
#define	USB_PRODUCT_3COMUSR_USR56K	0x3021		/* U.S.Robotics 56000 Voice Faxmodem Pro */

/* 4G Systems products */
#define	USB_PRODUCT_4GSYSTEMS_XSSTICK_W14	0x9603		/* 4G Systems XSStick W14 */
#define	USB_PRODUCT_4GSYSTEMS_XSSTICK_P14	0x9605		/* 4G Systems XSStick P14 */
#define	USB_PRODUCT_4GSYSTEMS_XSSTICK_P14_INSTALLER	0xf000		/* 4G Systems XSStick P14 - Windows driver */

/* ACDC products */
#define	USB_PRODUCT_ACDC_HUB	0x2315		/* USB Pen Drive HUB */
#define	USB_PRODUCT_ACDC_SECWRITE	0x2316		/* USB Pen Drive Secure Write */
#define	USB_PRODUCT_ACDC_PEN	0x2317		/* USB Pen Drive with Secure Write */

/* AboCom products */
#define	USB_PRODUCT_ABOCOM_XX1	0x110c		/* XX1 */
#define	USB_PRODUCT_ABOCOM_XX2	0x200c		/* XX2 */
#define	USB_PRODUCT_ABOCOM_RT2770	0x2770		/* RT2770 */
#define	USB_PRODUCT_ABOCOM_RT2870	0x2870		/* RT2870 */
#define	USB_PRODUCT_ABOCOM_RT3070	0x3070		/* RT3070 */
#define	USB_PRODUCT_ABOCOM_RT3071	0x3071		/* RT3071 */
#define	USB_PRODUCT_ABOCOM_RT3072	0x3072		/* RT3072 */
#define	USB_PRODUCT_ABOCOM2_RT2870_1	0x3c09		/* RT2870 */
#define	USB_PRODUCT_ABOCOM_URE450	0x4000		/* URE450 Ethernet Adapter */
#define	USB_PRODUCT_ABOCOM_UFE1000	0x4002		/* UFE1000 Fast Ethernet Adapter */
#define	USB_PRODUCT_ABOCOM_DSB650TX_PNA	0x4003		/* 1/10/100 ethernet adapter */
#define	USB_PRODUCT_ABOCOM_XX4	0x4004		/* XX4 */
#define	USB_PRODUCT_ABOCOM_XX5	0x4007		/* XX5 */
#define	USB_PRODUCT_ABOCOM_XX6	0x400b		/* XX6 */
#define	USB_PRODUCT_ABOCOM_XX7	0x400c		/* XX7 */
#define	USB_PRODUCT_ABOCOM_LCS8138TX	0x401a		/* LCS-8138TX */
#define	USB_PRODUCT_ABOCOM_XX8	0x4102		/* XX8 */
#define	USB_PRODUCT_ABOCOM_XX9	0x4104		/* XX9 */
#define	USB_PRODUCT_ABOCOM_UFE2000	0x420a		/* UFE2000 USB2.0 Fast Ethernet Adapter */
#define	USB_PRODUCT_ABOCOM_WL54	0x6001		/* WL54 */
#define	USB_PRODUCT_ABOCOM_RTL8192CU	0x8178		/* RTL8192CU */
#define	USB_PRODUCT_ABOCOM_RTL8188CU_1	0x8188		/* RTL8188CU */
#define	USB_PRODUCT_ABOCOM_RTL8188CU_2	0x8189		/* RTL8188CU */
#define	USB_PRODUCT_ABOCOM_XX10	0xabc1		/* XX10 */
#define	USB_PRODUCT_ABOCOM_BWU613	0xb000		/* BWU613 */
#define	USB_PRODUCT_ABOCOM_HWU54DM	0xb21b		/* HWU54DM */
#define	USB_PRODUCT_ABOCOM_RT2573_2	0xb21c		/* RT2573 */
#define	USB_PRODUCT_ABOCOM_RT2573_3	0xb21d		/* RT2573 */
#define	USB_PRODUCT_ABOCOM_RT2573_4	0xb21e		/* RT2573 */
#define	USB_PRODUCT_ABOCOM_WUG2700	0xb21f		/* WUG2700 */

/* Accton products */
#define	USB_PRODUCT_ACCTON_USB320_EC	0x1046		/* USB320-EC Ethernet Adapter */
#define	USB_PRODUCT_ACCTON_2664W	0x3501		/* 2664W */
#define	USB_PRODUCT_ACCTON_111	0x3503		/* T-Sinus 111 WLAN */
#define	USB_PRODUCT_ACCTON_SMCWUSBG	0x4505		/* SMCWUSB-G */
#define	USB_PRODUCT_ACCTON_SMCWUSBTG2	0x4506		/* SMCWUSBT-G2 */
#define	USB_PRODUCT_ACCTON_SMCWUSBTG2_NF	0x4507		/* SMCWUSBT-G2 */
#define	USB_PRODUCT_ACCTON_PRISM_GT	0x4521		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_ACCTON_SS1001	0x5046		/* SpeedStream Ethernet Adapter */
#define	USB_PRODUCT_ACCTON_RT2870_2	0x6618		/* RT2870 */
#define	USB_PRODUCT_ACCTON_RT3070	0x7511		/* RT3070 */
#define	USB_PRODUCT_ACCTON_RT2770	0x7512		/* RT2770 */
#define	USB_PRODUCT_ACCTON_RT2870_3	0x7522		/* RT2870 */
#define	USB_PRODUCT_ACCTON_RT2870_5	0x8522		/* RT2870 */
#define	USB_PRODUCT_ACCTON_RT3070_4	0xa512		/* RT3070 */
#define	USB_PRODUCT_ACCTON_RT2870_4	0xa618		/* RT2870 */
#define	USB_PRODUCT_ACCTON_RT3070_1	0xa701		/* RT3070 */
#define	USB_PRODUCT_ACCTON_RT3070_2	0xa702		/* RT3070 */
#define	USB_PRODUCT_ACCTON_RT3070_6	0xa703		/* RT3070 */
#define	USB_PRODUCT_ACCTON_AR9280	0xa704		/* AR9280+AR7010 */
#define	USB_PRODUCT_ACCTON_RT2870_1	0xb522		/* RT2870 */
#define	USB_PRODUCT_ACCTON_RTL8192SU	0xc512		/* RTL8192SU */
#define	USB_PRODUCT_ACCTON_RT3070_3	0xc522		/* RT3070 */
#define	USB_PRODUCT_ACCTON_RT3070_5	0xd522		/* RT3070 */
#define	USB_PRODUCT_ACCTON_ZD1211B	0xe501		/* ZD1211B */
#define	USB_PRODUCT_ACCTON_WN4501H_LF_IR	0xe503		/* WN4501H-LF-IR */
#define	USB_PRODUCT_ACCTON_WUS201	0xe506		/* WUS-201 */
#define	USB_PRODUCT_ACCTON_WN7512	0xf522		/* WN7512 */

/* Acer Communications & Multimedia products */
#define	USB_PRODUCT_ACERCM_EP1427X2	0x0893		/* EP-1427X-2 Ethernet */

/* Acer Labs products */
#define	USB_PRODUCT_ACERLABS_M5632	0x5632		/* USB 2.0 Data Link */

/* Acer Peripherals, Inc. products */
#define	USB_PRODUCT_ACERP_ACERSCAN_C310U	0x12a6		/* Acerscan C310U */
#define	USB_PRODUCT_ACERP_ACERSCAN_320U	0x2022		/* Acerscan 320U */
#define	USB_PRODUCT_ACERP_ACERSCAN_640U	0x2040		/* Acerscan 640U */
#define	USB_PRODUCT_ACERP_ACERSCAN_620U	0x2060		/* Acerscan 620U */
#define	USB_PRODUCT_ACERP_ATAPI	0x6003		/* ATA/ATAPI adapter */
#define	USB_PRODUCT_ACERP_AWL300	0x9000		/* AWL300 */
#define	USB_PRODUCT_ACERP_AWL400	0x9001		/* AWL400 */

/* Acer Products */
#define	USB_PRODUCT_ACERW_WARPLINK	0x0204		/* Warplink */

/* Actiontec products */
#define	USB_PRODUCT_ACTIONTEC_PRISM_25	0x0408		/* Prism2.5 WLAN */
#define	USB_PRODUCT_ACTIONTEC_PRISM_25A	0x0421		/* Prism2.5 WLAN A */
#define	USB_PRODUCT_ACTIONTEC_AR9287	0x1200		/* AR9287+AR7010 */
#define	USB_PRODUCT_ACTIONTEC_FREELAN	0x6106		/* ROPEX FreeLan 802.11b */
#define	USB_PRODUCT_ACTIONTEC_UAT1	0x7605		/* UAT1 Wireless Ethernet adapter */

/* ACTiSYS products */
#define	USB_PRODUCT_ACTISYS_IR2000U	0x0011		/* ACT-IR2000U FIR */

/* ActiveWire, Inc. products */
#define	USB_PRODUCT_ACTIVEWIRE_IOBOARD	0x0100		/* I/O Board */
#define	USB_PRODUCT_ACTIVEWIRE_IOBOARD_FW1	0x0101		/* I/O Board, rev. 1 firmware */

/* Adaptec products */
#define	USB_PRODUCT_ADAPTEC_AWN8020	0x0020		/* AWN-8020 WLAN */

/* Addonics products */
#define	USB_PRODUCT_ADDONICS2_205	0xa001		/* Cable 205 */

/* Addtron products */
#define	USB_PRODUCT_ADDTRON_AWU120	0xff31		/* AWU-120 */

/* ADMtek products */
#define	USB_PRODUCT_ADMTEK_PEGASUSII_4	0x07c2		/* AN986A Ethernet */
#define	USB_PRODUCT_ADMTEK_PEGASUS	0x0986		/* AN986 USB Ethernet */
#define	USB_PRODUCT_ADMTEK_PEGASUSII	0x8511		/* AN8511 USB Ethernet */
#define	USB_PRODUCT_ADMTEK_PEGASUSII_2	0x8513		/* AN8513 Ethernet */
#define	USB_PRODUCT_ADMTEK_PEGASUSII_3	0x8515		/* ADM8515 USB 2.0 Ethernet */

/* ADS products */
#define	USB_PRODUCT_ADS_UBS10BT	0x0008		/* UBS-10BT Ethernet */
#define	USB_PRODUCT_ADS_UBS10BTX	0x0009		/* UBS-10BT Ethernet */
#define	USB_PRODUCT_ADS_RDX155	0xa155		/* InstantFM Music */

/* AEI products */
#define	USB_PRODUCT_AEI_USBTOLAN	0x1701		/* AEI USB to Lan adapter */

/* Agate Technologies products */
#define	USB_PRODUCT_AGATE_QDRIVE	0x0378		/* Q-Drive */

/* AGFA products */
#define	USB_PRODUCT_AGFA_SNAPSCAN1212U	0x0001		/* SnapScan 1212U */
#define	USB_PRODUCT_AGFA_SNAPSCAN1236U	0x0002		/* SnapScan 1236U */
#define	USB_PRODUCT_AGFA_SNAPSCANTOUCH	0x0100		/* SnapScan Touch */
#define	USB_PRODUCT_AGFA_SNAPSCAN1212U2	0x2061		/* SnapScan 1212U */
#define	USB_PRODUCT_AGFA_SNAPSCANE40	0x208d		/* SnapScan e40 */
#define	USB_PRODUCT_AGFA_SNAPSCANE50	0x208f		/* SnapScan e50 */
#define	USB_PRODUCT_AGFA_SNAPSCANE20	0x2091		/* SnapScan e20 */
#define	USB_PRODUCT_AGFA_SNAPSCANE25	0x2095		/* SnapScan e25 */
#define	USB_PRODUCT_AGFA_SNAPSCANE26	0x2097		/* SnapScan e26 */
#define	USB_PRODUCT_AGFA_SNAPSCANE52	0x20fd		/* SnapScan e52 */

/* Aincomm products */
#define	USB_PRODUCT_AINCOMM_AWU2000B	0x1001		/* AWU2000B */

/* AIPTEK International products */
#define	USB_PRODUCT_AIPTEK2_PENCAM_MEGA_1_3	0x504a		/* PenCam Mega 1.3 */

/* AirPrime products */
#define	USB_PRODUCT_AIRPRIME_PC5220	0x0112		/* CDMA Wireless PC Card */

/* Airties products */
#define	USB_PRODUCT_AIRTIES_RT3070_2	0x2012		/* RT3070 */
#define	USB_PRODUCT_AIRTIES_RT3070	0x2310		/* RT3070 */

/* AKS products */
#define	USB_PRODUCT_AKS_USBHASP	0x0001		/* USB-HASP 0.06 */

/* Alcatel Telecom products */
#define	USB_PRODUCT_ALCATELT_ST120G	0x0120		/* SpeedTouch 120g */
#define	USB_PRODUCT_ALCATELT_ST121G	0x0121		/* SpeedTouch 121g */

/* Alcor Micro, Inc. products */
#define	USB_PRODUCT_ALCOR2_KBD_HUB	0x2802		/* Kbd Hub */

#define	USB_PRODUCT_ALCOR_MA_KBD_HUB	0x9213		/* MacAlly Kbd Hub */
#define	USB_PRODUCT_ALCOR_AU9814	0x9215		/* AU9814 Hub */
#define	USB_PRODUCT_ALCOR_SM_KBD	0x9410		/* MicroConnectors/StrongMan Keyboard */
#define	USB_PRODUCT_ALCOR_NEC_KBD_HUB	0x9472		/* NEC Kbd Hub */

/* ALLWIN Tech products */
#define	USB_PRODUCT_ALLWIN_RT2070	0x2070		/* RT2070 */
#define	USB_PRODUCT_ALLWIN_RT2770	0x2770		/* RT2770 */
#define	USB_PRODUCT_ALLWIN_RT2870	0x2870		/* RT2870 */
#define	USB_PRODUCT_ALLWIN_RT3070	0x3070		/* RT3070 */
#define	USB_PRODUCT_ALLWIN_RT3071	0x3071		/* RT3071 */
#define	USB_PRODUCT_ALLWIN_RT3072	0x3072		/* RT3072 */
#define	USB_PRODUCT_ALLWIN_RT3572	0x3572		/* RT3572 */

/* Altec Lansing products */
#define	USB_PRODUCT_ALTEC_ADA70	0x0070		/* ADA70 Speakers */
#define	USB_PRODUCT_ALTEC_ASC495	0xff05		/* ASC495 Speakers */

/* American Power Conversion products */
#define	USB_PRODUCT_APC_UPS	0x0002		/* Uninterruptible Power Supply */

/* Ambit Microsystems products */
#define	USB_PRODUCT_AMBIT_NTL_250	0x6098		/* NTL 250 cable modem */

/* AMD product */
#define	USB_PRODUCT_AMD_TV_WONDER_600_USB	0xb002		/* TV Wonder 600 USB */

/* Amigo products */
#define	USB_PRODUCT_AMIGO_RT2870_1	0x9031		/* RT2870 */
#define	USB_PRODUCT_AMIGO_RT2870_2	0x9041		/* RT2870 */

/* AMIT products */
#define	USB_PRODUCT_AMIT_CGWLUSB2GO	0x0002		/* CG-WLUSB2GO */
#define	USB_PRODUCT_AMIT_CGWLUSB2GNR	0x0008		/* CG-WLUSB2GNR */
#define	USB_PRODUCT_AMIT_RT2870_1	0x0012		/* RT2870 */
#define	USB_PRODUCT_AMIT2_RT2870	0x0008		/* RT2870 */

/* Anchor products */
#define	USB_PRODUCT_ANCHOR_EZUSB	0x2131		/* EZUSB */
#define	USB_PRODUCT_ANCHOR_EZLINK	0x2720		/* EZLINK */

/* AnyDATA Inc. products */
#define	USB_PRODUCT_ANYDATA_A2502	0x6202		/* NTT DoCoMo A2502 */
#define	USB_PRODUCT_ANYDATA_ADU_E100H	0x6501		/* ADU-E100H */
#define	USB_PRODUCT_ANYDATA_ADU_500A	0x6502		/* ADU-E500A */

/* AOX, Inc. products */
#define	USB_PRODUCT_AOX_USB101	0x0008		/* USB ethernet controller engine */

/* Apple Computer products */
#define	USB_PRODUCT_APPLE_EXT_KBD	0x020c		/* Apple Extended USB Keyboard */
#define	USB_PRODUCT_APPLE_FOUNTAIN_ANSI	0x020e		/* Apple Internal Keyboard/Trackpad (Fountain/ANSI) */
#define	USB_PRODUCT_APPLE_FOUNTAIN_ISO	0x020f		/* Apple Internal Keyboard/Trackpad (Fountain/ISO) */
#define	USB_PRODUCT_APPLE_GEYSER_ANSI	0x0214		/* Apple Internal Keyboard/Trackpad (Geyser/ANSI) */
#define	USB_PRODUCT_APPLE_GEYSER_ISO	0x0215		/* Apple Internal Keyboard/Trackpad (Geyser/ISO) */
#define	USB_PRODUCT_APPLE_GEYSER_JIS	0x0216		/* Apple Internal Keyboard/Trackpad (Geyser/JIS) */
#define	USB_PRODUCT_APPLE_GEYSER3_ANSI	0x0217		/* Apple Internal Keyboard/Trackpad (Geyser3/ANSI) */
#define	USB_PRODUCT_APPLE_GEYSER3_ISO	0x0218		/* Apple Internal Keyboard/Trackpad (Geyser3/ISO) */
#define	USB_PRODUCT_APPLE_GEYSER3_JIS	0x0219		/* Apple Internal Keyboard/Trackpad (Geyser3/JIS) */
#define	USB_PRODUCT_APPLE_GEYSER4_ANSI	0x021a		/* Apple Internal Keyboard/Trackpad (Geyser4/ANSI) */
#define	USB_PRODUCT_APPLE_GEYSER4_ISO	0x021b		/* Apple Internal Keyboard/Trackpad (Geyser4/ISO) */
#define	USB_PRODUCT_APPLE_GEYSER4_JIS	0x021c		/* Apple Internal Keyboard/Trackpad (Geyser4/JIS) */
#define	USB_PRODUCT_APPLE_WELLSPRING_ANSI	0x0223		/* Apple Internal Keyboard/Trackpad (Wellspring/ANSI) */
#define	USB_PRODUCT_APPLE_WELLSPRING_ISO	0x0224		/* Apple Internal Keyboard/Trackpad (Wellspring/ISO) */
#define	USB_PRODUCT_APPLE_WELLSPRING_JIS	0x0225		/* Apple Internal Keyboard/Trackpad (Wellspring/JIS) */
#define	USB_PRODUCT_APPLE_WELLSPRING2_ANSI	0x0230		/* Apple Internal Keyboard/Trackpad (Wellspring2/ANSI) */
#define	USB_PRODUCT_APPLE_WELLSPRING2_ISO	0x0231		/* Apple Internal Keyboard/Trackpad (Wellspring2/ISO) */
#define	USB_PRODUCT_APPLE_WELLSPRING2_JIS	0x0232		/* Apple Internal Keyboard/Trackpad (Wellspring2/JIS) */
#define	USB_PRODUCT_APPLE_OPTMOUSE	0x0302		/* Optical mouse */
#define	USB_PRODUCT_APPLE_MIGHTYMOUSE	0x0304		/* Mighty Mouse */
#define	USB_PRODUCT_APPLE_FOUNTAIN_TP	0x030a		/* Apple Internal Trackpad (Fountain) */
#define	USB_PRODUCT_APPLE_GEYSER1_TP	0x030b		/* Apple Internal Trackpad (Geyser) */
#define	USB_PRODUCT_APPLE_MAGICMOUSE	0x030d		/* Magic Mouse */
#define	USB_PRODUCT_APPLE_MAGICTRACKPAD	0x030e		/* Magic Trackpad */
#define	USB_PRODUCT_APPLE_BLUETOOTH_HIDMODE	0x1000		/* Bluetooth HCI (HID-proxy mode) */
#define	USB_PRODUCT_APPLE_EXT_KBD_HUB	0x1003		/* Hub in Apple Extended USB Keyboard */
#define	USB_PRODUCT_APPLE_SPEAKERS	0x1101		/* Speakers */
#define	USB_PRODUCT_APPLE_SHUFFLE2	0x1301		/* iPod Shuffle (2nd generation) */
#define	USB_PRODUCT_APPLE_IPHONE	0x1290		/* iPhone */
#define	USB_PRODUCT_APPLE_IPOD_TOUCH	0x1291		/* iPod Touch */
#define	USB_PRODUCT_APPLE_IPOD_TOUCH_4G	0x129e		/* iPod Touch 4G */
#define	USB_PRODUCT_APPLE_IPHONE_3G	0x1292		/* iPhone 3G */
#define	USB_PRODUCT_APPLE_IPHONE_3GS	0x1294		/* iPhone 3GS */
#define	USB_PRODUCT_APPLE_IPAD	0x129a		/* Apple iPad */
#define	USB_PRODUCT_APPLE_ETHERNET	0x1402		/* Apple USB to Ethernet */
#define	USB_PRODUCT_APPLE_BLUETOOTH2	0x8205		/* Bluetooth */
#define	USB_PRODUCT_APPLE_BLUETOOTH_HOST_C	0x821f		/* Bluetooth USB Host Controller */
#define	USB_PRODUCT_APPLE_BLUETOOTH	0x8300		/* Bluetooth */

/* ArkMicroChips products */
#define	USB_PRODUCT_ARKMICROCHIPS_USBSERIAL	0x0232		/* USB-UART Controller */

/* Asahi Optical products */
#define	USB_PRODUCT_ASAHIOPTICAL_OPTIO230	0x0004		/* PENTAX Optio230 */
#define	USB_PRODUCT_ASAHIOPTICAL_OPTIO330	0x0006		/* Digital camera */

/* Asante products */
#define	USB_PRODUCT_ASANTE_EA	0x1427		/* Ethernet Adapter */

/* Askey Computer products */
#define	USB_PRODUCT_ASKEY_WLL013I	0x0320		/* WLL013 (Intersil) */
#define	USB_PRODUCT_ASKEY_WLL013	0x0321		/* WLL013 */
#define	USB_PRODUCT_ASKEY_VOYAGER1010	0x0821		/* Voyager 1010 */

/* ASIX Electronics products */
#define	USB_PRODUCT_ASIX_AX88172	0x1720		/* AX88172 USB 2.0 10/100 ethernet controller */
#define	USB_PRODUCT_ASIX_AX88178	0x1780		/* AX88178 USB 2.0 gigabit ethernet controller */
#define	USB_PRODUCT_ASIX_AX88178A	0x178a		/* AX88178A USB 2.0 gigabit ethernet controller */
#define	USB_PRODUCT_ASIX_AX88179	0x1790		/* AX88179 USB 3.0 gigabit ethernet controller */
#define	USB_PRODUCT_ASIX_AX88772	0x7720		/* AX88772 USB 2.0 10/100 ethernet controller */
#define	USB_PRODUCT_ASIX_AX88772A	0x772a		/* AX88772A USB 2.0 10/100 Ethernet adapter */
#define	USB_PRODUCT_ASIX_AX88772B	0x772b		/* AX88772B USB 2.0 10/100 Ethernet adapter */
#define	USB_PRODUCT_ASIX_AX88772B_1	0x7e2b		/* AX88772B1 USB 2.0 10/100 Ethernet adapter */

/* ASUSTeK computer products */
#define	USB_PRODUCT_ASUSTEK_RT2570	0x1706		/* RT2570 */
#define	USB_PRODUCT_ASUSTEK_WL167G	0x1707		/* WL-167g USB2.0 WLAN Adapter */
#define	USB_PRODUCT_ASUSTEK_WL159G	0x170c		/* WL-159g */
#define	USB_PRODUCT_ASUSTEK_A9T_WIFI	0x171b		/* A9T wireless */
#define	USB_PRODUCT_ASUSTEK_P5B_WIFI	0x171d		/* P5B wireless */
#define	USB_PRODUCT_ASUSTEK_WL167G_2	0x1723		/* WL-167g USB2.0 WLAN Adapter (version 2) */
#define	USB_PRODUCT_ASUSTEK_WL167G_3	0x1724		/* WL-167g USB2.0 WLAN Adapter (version 2) */
#define	USB_PRODUCT_ASUSTEK_RT2870_1	0x1731		/* RT2870 */
#define	USB_PRODUCT_ASUSTEK_RT2870_2	0x1732		/* RT2870 */
#define	USB_PRODUCT_ASUSTEK_U3100	0x173f		/* My Cinema U3100 Mini DVB-T */
#define	USB_PRODUCT_ASUSTEK_RT2870_3	0x1742		/* RT2870 */
#define	USB_PRODUCT_ASUSTEK_RT2870_4	0x1760		/* RT2870 */
#define	USB_PRODUCT_ASUSTEK_RT2870_5	0x1761		/* RT2870 */
#define	USB_PRODUCT_ASUSTEK_RT3070	0x1784		/* RT3070 */
#define	USB_PRODUCT_ASUSTEK_USBN10	0x1786		/* USB-N10 */
#define	USB_PRODUCT_ASUSTEK_RT3070_1	0x1790		/* RT3070 */
#define	USB_PRODUCT_ASUSTEK_RTL8192SU_1	0x1791		/* RTL8192SU */
#define	USB_PRODUCT_ASUSTEK_USBN53	0x179d		/* USB-N53 */
#define	USB_PRODUCT_ASUSTEK_RTL8192CU	0x17ab		/* RTL8192CU */
#define	USB_PRODUCT_ASUSTEK_USBN66	0x17ad		/* USB-N66 */
#define	USB_PRODUCT_ASUSTEK_USBN10NANO	0x17ba		/* USB-N10 Nano */
#define	USB_PRODUCT_ASUSTEK_RTL8192CU_3	0x17c0		/* RTL8192CU_3 */
#define	USB_PRODUCT_ASUSTEK_MYPAL_A730	0x4202		/* MyPal A730 */
#define	USB_PRODUCT_ASUSTEK2_USBN11	0x0b05		/* USB-N11 */

/* ATen products */
#define	USB_PRODUCT_ATEN_UC1284	0x2001		/* Parallel printer adapter */
#define	USB_PRODUCT_ATEN_UC10T	0x2002		/* 10Mbps ethernet adapter */
#define	USB_PRODUCT_ATEN_UC232A	0x2008		/* Serial adapter */
#define	USB_PRODUCT_ATEN_UC210T	0x2009		/* UC210T Ethernet adapter */
#define	USB_PRODUCT_ATEN_UC2324	0x2011		/* UC2324 USB to Serial Hub */
#define	USB_PRODUCT_ATEN_DSB650C	0x4000		/* DSB-650C */

/* Atheros Communications products */
#define	USB_PRODUCT_ATHEROS_AR5523	0x0001		/* AR5523 */
#define	USB_PRODUCT_ATHEROS_AR5523_NF	0x0002		/* AR5523 */

/* Atheros Communications(2) products */
#define	USB_PRODUCT_ATHEROS2_AR5523_1	0x0001		/* AR5523 */
#define	USB_PRODUCT_ATHEROS2_AR5523_1_NF	0x0002		/* AR5523 */
#define	USB_PRODUCT_ATHEROS2_AR5523_2	0x0003		/* AR5523 */
#define	USB_PRODUCT_ATHEROS2_AR5523_2_NF	0x0004		/* AR5523 */
#define	USB_PRODUCT_ATHEROS2_AR5523_3	0x0005		/* AR5523 */
#define	USB_PRODUCT_ATHEROS2_AR5523_3_NF	0x0006		/* AR5523 */
#define	USB_PRODUCT_ATHEROS2_TG121N	0x1001		/* TG121N */
#define	USB_PRODUCT_ATHEROS2_WN821NV2	0x1002		/* WN821NV2 */
#define	USB_PRODUCT_ATHEROS2_AR9271_1	0x1006		/* AR9271 */
#define	USB_PRODUCT_ATHEROS2_3CRUSBN275	0x1010		/* 3CRUSBN275 */
#define	USB_PRODUCT_ATHEROS2_WN612	0x1011		/* WN612 */
#define	USB_PRODUCT_ATHEROS2_AR3011	0x3000		/* AR3011 */
#define	USB_PRODUCT_ATHEROS2_AR3012	0x3004		/* AR3012 */
#define	USB_PRODUCT_ATHEROS2_AR9280	0x7010		/* AR9280+AR7010 */
#define	USB_PRODUCT_ATHEROS2_AR9287	0x7015		/* AR9287+AR7010 */
#define	USB_PRODUCT_ATHEROS2_AR9170	0x9170		/* AR9170 */
#define	USB_PRODUCT_ATHEROS2_AR9271_2	0x9271		/* AR9271 */
#define	USB_PRODUCT_ATHEROS2_AR9271_3	0xb003		/* AR9271 */

/* ATI products */
#define	USB_PRODUCT_ATI2_205	0xa001		/* USB Cable 205 */

/* Atmel Comp. products */
#define	USB_PRODUCT_ATMEL_UHB124	0x3301		/* UHB124 hub */
#define	USB_PRODUCT_ATMEL_WN210	0x4102		/* W-Buddie WN210 */
#define	USB_PRODUCT_ATMEL_DWL900AP	0x5601		/* DWL-900AP Wireless access point */
#define	USB_PRODUCT_ATMEL_SAM_BA	0x6124		/* ARM SAM-BA programming port */
#define	USB_PRODUCT_ATMEL_DWL120	0x7602		/* DWL-120 Wireless adapter */
#define	USB_PRODUCT_ATMEL_AT76C503I1	0x7603		/* AT76C503 (Intersil 3861 Radio) */
#define	USB_PRODUCT_ATMEL_AT76C503I2	0x7604		/* AT76C503 (Intersil 3863 Radio) */
#define	USB_PRODUCT_ATMEL_AT76C503RFMD	0x7605		/* AT76C503 (RFMD Radio) */
#define	USB_PRODUCT_ATMEL_AT76C505RFMD	0x7606		/* AT76C505 (RFMD Radio) */
#define	USB_PRODUCT_ATMEL_AT76C505RFMD2958	0x7613		/* AT76C505 (RFMD 2958 Radio) */
#define	USB_PRODUCT_ATMEL_AT76C505A	0x7614		/* AT76C505A (RFMD 2958 Radio) */
#define	USB_PRODUCT_ATMEL_AT76C505AS	0x7617		/* AT76C505AS (RFMD 2958 Radio) */

/* Audio-Technica products */
#define	USB_PRODUCT_AUDIOTECHNICA_ATCHA4USB	0x0009		/* ATC-HA4USB USB headphone */

/* Avance Logic products */
#define	USB_PRODUCT_AVANCELOGIC_USBAUDIO	0x0100		/* USB Audio Speaker */

/* Averatec products */
#define	USB_PRODUCT_AVERATEC_USBWLAN	0x4013		/* WLAN */

/* Avision products */
#define	USB_PRODUCT_AVISION_1200U	0x0268		/* 1200U scanner */

/* AVM products */
#define	USB_PRODUCT_AVM_FRITZWLAN	0x8401		/* FRITZ!WLAN N */

/* Azurewave products */
#define	USB_PRODUCT_AZUREWAVE_RT2870_1	0x3247		/* RT2870 */
#define	USB_PRODUCT_AZUREWAVE_RT2870_2	0x3262		/* RT2870 */
#define	USB_PRODUCT_AZUREWAVE_RT3070	0x3273		/* RT3070 */
#define	USB_PRODUCT_AZUREWAVE_RT3070_2	0x3284		/* RT3070 */
#define	USB_PRODUCT_AZUREWAVE_RT3070_3	0x3305		/* RT3070 */
#define	USB_PRODUCT_AZUREWAVE_RTL8192SU_1	0x3306		/* RTL8192SU */
#define	USB_PRODUCT_AZUREWAVE_RT3070_4	0x3307		/* RT3070 */
#define	USB_PRODUCT_AZUREWAVE_RTL8192SU_2	0x3309		/* RTL8192SU */
#define	USB_PRODUCT_AZUREWAVE_RTL8192SU_3	0x3310		/* RTL8192SU */
#define	USB_PRODUCT_AZUREWAVE_RTL8192SU_4	0x3311		/* RTL8192SU */
#define	USB_PRODUCT_AZUREWAVE_RT3070_5	0x3321		/* RT3070 */
#define	USB_PRODUCT_AZUREWAVE_RTL8192SU_5	0x3325		/* RTL8192SU */
#define	USB_PRODUCT_AZUREWAVE_AR9271_1	0x3327		/* AR9271 */
#define	USB_PRODUCT_AZUREWAVE_AR9271_2	0x3328		/* AR9271 */
#define	USB_PRODUCT_AZUREWAVE_AR9271_3	0x3346		/* AR9271 */
#define	USB_PRODUCT_AZUREWAVE_AR9271_4	0x3348		/* AR9271 */
#define	USB_PRODUCT_AZUREWAVE_AR9271_5	0x3349		/* AR9271 */
#define	USB_PRODUCT_AZUREWAVE_AR9271_6	0x3350		/* AR9271 */
#define	USB_PRODUCT_AZUREWAVE_RTL8188CU	0x3357		/* RTL8188CU */
#define	USB_PRODUCT_AZUREWAVE_RTL8188CE_1	0x3358		/* RTL8188CE */
#define	USB_PRODUCT_AZUREWAVE_RTL8188CE_2	0x3359		/* RTL8188CE */

/* Baltech products */
#define	USB_PRODUCT_BALTECH_CARDREADER	0x9999		/* Card reader */

/* B&B Electronics products */
#define	USB_PRODUCT_BBELECTRONICS_USOTL4	0xAC01		/* uLinks RS-422/485 */

/* Beceem Communications products */
#define	USB_PRODUCT_BECEEM_250U	0x0220		/* Mobile WiMax SS */

/* Belkin products */
/*product BELKIN F5U111		0x????	F5U111 Ethernet adapter*/
#define	USB_PRODUCT_BELKIN2_F5U002	0x0002		/* F5U002 Parallel printer adapter */
#define	USB_PRODUCT_BELKIN_F5D6050	0x0050		/* F5D6050 802.11b Wireless adapter */
#define	USB_PRODUCT_BELKIN_F5U103	0x0103		/* F5U103 Serial adapter */
#define	USB_PRODUCT_BELKIN_F5U109	0x0109		/* F5U109 Serial adapter */
#define	USB_PRODUCT_BELKIN_SCSI	0x0115		/* SCSI Adaptor */
#define	USB_PRODUCT_BELKIN_USB2LAN	0x0121		/* USB to LAN Converter */
#define	USB_PRODUCT_BELKIN_F5U208	0x0208		/* F5U208 VideoBus II */
#define	USB_PRODUCT_BELKIN_F5U237	0x0237		/* F5U237 USB 2.0 7-Port Hub */
#define	USB_PRODUCT_BELKIN_F5U409	0x0409		/* F5U409 Serial */
#define	USB_PRODUCT_BELKIN_UPS	0x0980		/* UPS */
#define	USB_PRODUCT_BELKIN_RTL8192CU_2	0x1004		/* RTL8192CU */
#define	USB_PRODUCT_BELKIN_RTL8188CU	0x1102		/* RTL8188CU */
#define	USB_PRODUCT_BELKIN_RTL8188CUS	0x11f2		/* RTL8188CUS */
#define	USB_PRODUCT_BELKIN_F5U120	0x1203		/* F5U120-PC Hub */
#define	USB_PRODUCT_BELKIN_RTL8192CU	0x2102		/* RTL8192CU */
#define	USB_PRODUCT_BELKIN_F7D2102	0x2103		/* F7D2102 */
#define	USB_PRODUCT_BELKIN_RTL8192CU_1	0x21f2		/* RTL8192CU */
#define	USB_PRODUCT_BELKIN_ZD1211B	0x4050		/* ZD1211B */
#define	USB_PRODUCT_BELKIN_F5D5055	0x5055		/* F5D5055 Ethernet adapter */
#define	USB_PRODUCT_BELKIN_F5D7050	0x7050		/* F5D7050 54g USB Network Adapter */
#define	USB_PRODUCT_BELKIN_F5D7051	0x7051		/* F5D7051 54g USB Network Adapter */
#define	USB_PRODUCT_BELKIN_F5D7050A	0x705a		/* F5D705A 54g USB Network Adapter */
#define	USB_PRODUCT_BELKIN_F5D7050C	0x705c		/* F5D705C 54g USB Network Adapter */
#define	USB_PRODUCT_BELKIN_F5D7050E	0x705c		/* F5D705E 54g USB Network Adapter */
#define	USB_PRODUCT_BELKIN_RT2870_1	0x8053		/* RT2870 */
#define	USB_PRODUCT_BELKIN_RT2870_2	0x805c		/* RT2870 */
#define	USB_PRODUCT_BELKIN_F5D8053V3	0x815c		/* F5D8053 v3 */
#define	USB_PRODUCT_BELKIN_RTL8192SU_1	0x815f		/* RTL8192SU */
#define	USB_PRODUCT_BELKIN_F5D8055	0x825a		/* F5D8055 */
#define	USB_PRODUCT_BELKIN_F5D8055V2	0x825b		/* F5D8055 v2 */
#define	USB_PRODUCT_BELKIN_RTL8192SU_2	0x845a		/* RTL8192SU */
#define	USB_PRODUCT_BELKIN_F5D9050V3	0x905b		/* F5D9050 ver 3 */
#define	USB_PRODUCT_BELKIN_F5D9050C	0x905c		/* F5D9050C */
#define	USB_PRODUCT_BELKIN_F6D4050V1	0x935a		/* F6D4050 ver 1 */
#define	USB_PRODUCT_BELKIN_F6D4050V2	0x935b		/* F6D4050 ver 2 */
#define	USB_PRODUCT_BELKIN_RTL8192SU_3	0x945a		/* RTL8192SU */
#define	USB_PRODUCT_BELKIN_F7D1101V2	0x945b		/* F7D1101 v2 */

/* Bewan products */
#define	USB_PRODUCT_BEWAN_BWIFI_USB54AR	0x1196		/* BWIFI-USB54AR */
#define	USB_PRODUCT_BEWAN_RT3070	0x7712		/* RT3070 */

/* Billionton products */
#define	USB_PRODUCT_BILLIONTON_USB100	0x0986		/* USB100N 10/100 FastEthernet Adapter */
#define	USB_PRODUCT_BILLIONTON_USBLP100	0x0987		/* USB100LP */
#define	USB_PRODUCT_BILLIONTON_USBEL100	0x0988		/* USB100EL */
#define	USB_PRODUCT_BILLIONTON_USBE100	0x8511		/* USBE100 */
#define	USB_PRODUCT_BILLIONTON_USB2AR	0x90ff		/* USB2AR Ethernet */

/* Broadcom products */
#define	USB_PRODUCT_BROADCOM_BCM2033	0x2000		/* BCM2033 */
#define	USB_PRODUCT_BROADCOM_BCM2033NF	0x2033		/* BCM2033 (no firmware) */

/* Brother Industries products */
#define	USB_PRODUCT_BROTHER_HL1050	0x0002		/* HL-1050 laser printer */

/* Behavior Technology Computer products */
#define	USB_PRODUCT_BTC_BTC7932	0x6782		/* Keyboard with mouse port */

/* CACE Technologies products */
#define	USB_PRODUCT_CACE_AIRPCAPNX	0x0300		/* AirPcap Nx */

/* Canon, Inc. products */
#define	USB_PRODUCT_CANON_N656U	0x2206		/* CanoScan N656U */
#define	USB_PRODUCT_CANON_N1220U	0x2207		/* CanoScan N1220U */
#define	USB_PRODUCT_CANON_N670U	0x220d		/* CanoScan N670U */
#define	USB_PRODUCT_CANON_N1240U	0x220e		/* CanoScan N1240U */
#define	USB_PRODUCT_CANON_S10	0x3041		/* PowerShot S10 */
#define	USB_PRODUCT_CANON_S20	0x3043		/* PowerShot S20 */
#define	USB_PRODUCT_CANON_S100_US	0x3045		/* PowerShot S100 */
#define	USB_PRODUCT_CANON_S100_EU	0x3047		/* PowerShot S100 */
#define	USB_PRODUCT_CANON_G1	0x3048		/* PowerShot G1 */
#define	USB_PRODUCT_CANON_A20	0x304e		/* PowerShot A20 */
#define	USB_PRODUCT_CANON_S200	0x3065		/* PowerShot S200 */
#define	USB_PRODUCT_CANON_EOS300D	0x3084		/* EOS 300D / Digital Rebel */
#define	USB_PRODUCT_CANON_SD630	0x30fe		/* PowerShot SD630 */

/* CASIO products */
#define	USB_PRODUCT_CASIO_QV	0x1001		/* QV DigitalCamera */
#define	USB_PRODUCT_CASIO_BE300	0x2002		/* BE-300 PDA */
#define	USB_PRODUCT_CASIO_NAMELAND	0x4001		/* CASIO Nameland EZ-USB */

/* CATC products */
#define	USB_PRODUCT_CATC_NETMATE	0x000a		/* Netmate ethernet adapter */
#define	USB_PRODUCT_CATC_NETMATE2	0x000c		/* Netmate2 ethernet adapter */
#define	USB_PRODUCT_CATC_CHIEF	0x000d		/* USB Chief Bus & Protocol Analyzer */
#define	USB_PRODUCT_CATC_ANDROMEDA	0x1237		/* Andromeda hub */

/* CCYU Technology products */
#define	USB_PRODUCT_CCYU_EASYDISK	0x2136		/* EasyDisk Portable Device */

/* Cherry products */
#define	USB_PRODUCT_CHERRY_MY3000KBD	0x0001		/* My3000 keyboard */
#define	USB_PRODUCT_CHERRY_MY3000HUB	0x0003		/* My3000 hub */
#define	USB_PRODUCT_CHERRY_CYBOARD	0x0004		/* CyBoard Keyboard */
#define	USB_PRODUCT_CHERRY_MY6000KBD	0x0011		/* My6000 keyboard */

/* Chic Technology products */
#define	USB_PRODUCT_CHIC_MOUSE1	0x0001		/* mouse */
#define	USB_PRODUCT_CHIC_CYPRESS	0x0003		/* Cypress USB Mouse */

/* Chicony products */
#define	USB_PRODUCT_CHICONY_KB8933	0x0001		/* KB-8933 keyboard */
#define	USB_PRODUCT_CHICONY_RTL8188CUS_1	0xaff7		/* RTL8188CUS */
#define	USB_PRODUCT_CHICONY_RTL8188CUS_2	0xaff8		/* RTL8188CUS */
#define	USB_PRODUCT_CHICONY_RTL8188CUS_3	0xaff9		/* RTL8188CUS */
#define	USB_PRODUCT_CHICONY_RTL8188CUS_4	0xaffa		/* RTL8188CUS */
#define	USB_PRODUCT_CHICONY_RTL8188CUS_5	0xaffb		/* RTL8188CUS */
#define	USB_PRODUCT_CHICONY_RTL8188CUS_6	0xaffc		/* RTL8188CUS */
#define	USB_PRODUCT_CHICONY2_TWINKLECAM	0x600d		/* TwinkleCam USB camera */

/* CH Products */
#define	USB_PRODUCT_CHPRODUCTS_PROTHROTTLE	0x00f1		/* Pro Throttle */
#define	USB_PRODUCT_CHPRODUCTS_PROPEDALS	0x00f2		/* Pro Pedals */
#define	USB_PRODUCT_CHPRODUCTS_FIGHTERSTICK	0x00f3		/* Fighterstick */
#define	USB_PRODUCT_CHPRODUCTS_FLIGHTYOKE	0x00ff		/* Flight Sim Yoke */

/* Cisco-Linksys products */
#define	USB_PRODUCT_CISCOLINKSYS_WUSB54GV2	0x000a		/* WUSB54G v2 */
#define	USB_PRODUCT_CISCOLINKSYS_WUSB54AG	0x000c		/* WUSB54AG */
#define	USB_PRODUCT_CISCOLINKSYS_WUSB54G	0x000d		/* WUSB54G Wireless-G USB Network Adapter */
#define	USB_PRODUCT_CISCOLINKSYS_WUSB54GP	0x0011		/* WUSB54GP Wireless-G USB Network Adapter */
#define	USB_PRODUCT_CISCOLINKSYS_USB200MV2	0x0018		/* USB200M v2 */
#define	USB_PRODUCT_CISCOLINKSYS_HU200TS	0x001a		/* HU200-TS */
#define	USB_PRODUCT_CISCOLINKSYS_WUSB54GC	0x0020		/* WUSB54GC */
#define	USB_PRODUCT_CISCOLINKSYS_WUSB54GR	0x0023		/* WUSB54GR */
#define	USB_PRODUCT_CISCOLINKSYS_WUSBF54G	0x0024		/* WUSBF54G */
#define	USB_PRODUCT_CISCOLINKSYS_WUSB200	0x0028		/* WUSB200 */
#define	USB_PRODUCT_CISCOLINKSYS_AE1000	0x002f		/* AE1000 */
#define	USB_PRODUCT_CISCOLINKSYS_AM10	0x0031		/* AM10 */
#define	USB_PRODUCT_CISCOLINKSYS2_RT3070	0x4001		/* RT3070 */
#define	USB_PRODUCT_CISCOLINKSYS3_RT3070	0x0101		/* RT3070 */

/* Compaq products */
#define	USB_PRODUCT_COMPAQ_IPAQPOCKETPC	0x0003		/* iPAQ PocketPC */
#define	USB_PRODUCT_COMPAQ_A1500	0x0012		/* A1500 */
#define	USB_PRODUCT_COMPAQ_IPAQWLAN	0x0032		/* iPAQ WLAN */
#define	USB_PRODUCT_COMPAQ_W100	0x0033		/* W100 */
#define	USB_PRODUCT_COMPAQ_W200	0x0076		/* WLAN MultiPort W200 */
#define	USB_PRODUCT_COMPAQ_PJB100	0x504a		/* Personal Jukebox PJB100 */
#define	USB_PRODUCT_COMPAQ_IPAQLINUX	0x505a		/* iPAQ Linux */
#define	USB_PRODUCT_COMPAQ_HNE200	0x8511		/* HNE-200 USB Ethernet adapter */

/* Compare products */
#define	USB_PRODUCT_COMPARE_RTL8192CU	0x8010		/* RTL8192CU */

/* Composite Corp products looks the same as "TANGTOP" */
#define	USB_PRODUCT_COMPOSITE_USBPS2	0x0001		/* USB to PS2 Adaptor */

/* Conceptronic products */
#define	USB_PRODUCT_CONCEPTRONIC_RTL8192SU_1	0x3300		/* RTL8192SU */
#define	USB_PRODUCT_CONCEPTRONIC_RTL8192SU_2	0x3301		/* RTL8192SU */
#define	USB_PRODUCT_CONCEPTRONIC_RTL8192SU_3	0x3302		/* RTL8192SU */
#define	USB_PRODUCT_CONCEPTRONIC_C54RU	0x3c02		/* C54RU WLAN */
#define	USB_PRODUCT_CONCEPTRONIC_RT2870_1	0x3c06		/* RT2870 */
#define	USB_PRODUCT_CONCEPTRONIC_RT2870_2	0x3c07		/* RT2870 */
#define	USB_PRODUCT_CONCEPTRONIC_RT3070_1	0x3c08		/* RT3070 */
#define	USB_PRODUCT_CONCEPTRONIC_RT2870_7	0x3c09		/* RT2870 */
#define	USB_PRODUCT_CONCEPTRONIC_RT3070_2	0x3c11		/* RT3070 */
#define	USB_PRODUCT_CONCEPTRONIC_RT2870_8	0x3c12		/* RT2870 */
#define	USB_PRODUCT_CONCEPTRONIC_C54RU2	0x3c22		/* C54RU */
#define	USB_PRODUCT_CONCEPTRONIC_RT2870_3	0x3c23		/* RT2870 */
#define	USB_PRODUCT_CONCEPTRONIC_RT2573	0x3c24		/* RT2573M */
#define	USB_PRODUCT_CONCEPTRONIC_RT2870_4	0x3c25		/* RT2870 */
#define	USB_PRODUCT_CONCEPTRONIC_RT2870_5	0x3c27		/* RT2870 */
#define	USB_PRODUCT_CONCEPTRONIC_RT2870_6	0x3c28		/* RT2870 */
#define	USB_PRODUCT_CONCEPTRONIC_RT3070_3	0x3c2c		/* RT3070 */
#define	USB_PRODUCT_CONCEPTRONIC2_PRISM_GT	0x3762		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_CONCEPTRONIC_C11U	0x7100		/* C11U */
#define	USB_PRODUCT_CONCEPTRONIC_WL210	0x7110		/* WL-210 */
#define	USB_PRODUCT_CONCEPTRONIC_AR5523_1	0x7801		/* AR5523 */
#define	USB_PRODUCT_CONCEPTRONIC_AR5523_1_NF	0x7802		/* AR5523 */
#define	USB_PRODUCT_CONCEPTRONIC_AR5523_2	0x7811		/* AR5523 */
#define	USB_PRODUCT_CONCEPTRONIC_AR5523_2_NF	0x7812		/* AR5523 */

/* Concord Camera products */
#define	USB_PRODUCT_CONCORDCAMERA_EYE_Q_3X	0x0100		/* Eye Q 3x */

/* Connectix products */
#define	USB_PRODUCT_CONNECTIX_QUICKCAM	0x0001		/* QuickCam */

/* Corega products */
#define	USB_PRODUCT_COREGA_ETHER_USB_T	0x0001		/* Ether USB-T */
#define	USB_PRODUCT_COREGA_FETHER_USB_TX	0x0004		/* FEther USB-TX */
#define	USB_PRODUCT_COREGA_WLAN_USB_USB_11	0x000c		/* WirelessLAN USB-11 */
#define	USB_PRODUCT_COREGA_FETHER_USB_TXS	0x000d		/* FEther USB-TXS */
#define	USB_PRODUCT_COREGA_WLANUSB	0x0012		/* Wireless LAN USB Stick-11 */
#define	USB_PRODUCT_COREGA_FETHER_USB2_TX	0x0017		/* FEther USB2-TX */
#define	USB_PRODUCT_COREGA_WLUSB_11_KEY	0x001a		/* ULUSB-11 Key */
#define	USB_PRODUCT_COREGA_CGWLUSB2GTST	0x0020		/* CG-WLUSB2GTST */
#define	USB_PRODUCT_COREGA_CGUSBRS232R	0x002a		/* CG-USBRS232R */
#define	USB_PRODUCT_COREGA_CGWLUSB2GL	0x002d		/* CG-WLUSB2GL */
#define	USB_PRODUCT_COREGA_CGWLUSB2GPX	0x002e		/* CG-WLUSB2GPX */
#define	USB_PRODUCT_COREGA_RT2870_1	0x002f		/* RT2870 */
#define	USB_PRODUCT_COREGA_RT2870_2	0x003c		/* RT2870 */
#define	USB_PRODUCT_COREGA_RT2870_3	0x003f		/* RT2870 */
#define	USB_PRODUCT_COREGA_RT3070	0x0041		/* RT3070 */
#define	USB_PRODUCT_COREGA_CGWLUSBNM	0x0047		/* CG-WLUSBNM */
#define	USB_PRODUCT_COREGA_RTL8192CU	0x0056		/* RTL8192CU */
#define	USB_PRODUCT_COREGA_CGWLUSB300GNM	0x0042		/* CG-WLUSB300GNM */
#define	USB_PRODUCT_COREGA_WLUSB_11_STICK	0x7613		/* WLAN USB Stick 11 */
#define	USB_PRODUCT_COREGA_FETHER_USB_TXC	0x9601		/* FEther USB-TXC */

/* Creative products */
#define	USB_PRODUCT_CREATIVE_NOMAD_II	0x1002		/* Nomad II MP3 player */
#define	USB_PRODUCT_CREATIVE_NOMAD_IIMG	0x4004		/* Nomad II MG */
#define	USB_PRODUCT_CREATIVE_NOMAD	0x4106		/* Nomad */

#define	USB_PRODUCT_CREATIVE2_VOIP_BLASTER	0x0258		/* Voip Blaster */

#define	USB_PRODUCT_CREATIVE3_OPTICAL_MOUSE	0x0001		/* Notebook Optical Mouse */

/* Cambridge Silicon Radio products */
#define	USB_PRODUCT_CSR_BLUETOOTH	0x0001		/* Bluetooth USB Adapter */
#define	USB_PRODUCT_CSR_BLUETOOTH_NF	0xffff		/* Bluetooth USB Adapter */

/* CTX products */
#define	USB_PRODUCT_CTX_EX1300	0x9999		/* Ex1300 hub */

/* CyberPower Systems, Inc. products */
#define	USB_PRODUCT_CYBERPOWER_UPS	0x0501		/* Uninterruptible Power Supply */

/* CyberTAN Technology products */
#define	USB_PRODUCT_CYBERTAN_TG54USB	0x1666		/* TG54USB */
#define	USB_PRODUCT_CYBERTAN_ZD1211B	0x1667		/* ZD1211B */
#define	USB_PRODUCT_CYBERTAN_RT2870	0x1828		/* RT2870 */

/* Cypress Semiconductor products */
#define	USB_PRODUCT_CYPRESS_MOUSE	0x0001		/* mouse */
#define	USB_PRODUCT_CYPRESS_THERMO	0x0002		/* thermometer */
#define	USB_PRODUCT_CYPRESS_KBDHUB	0x0101		/* Keyboard/Hub */
#define	USB_PRODUCT_CYPRESS_FMRADIO	0x1002		/* FM Radio */
#define	USB_PRODUCT_CYPRESS_USBRS232	0x5500		/* USB-RS232 Interface */
#define	USB_PRODUCT_CYPRESS_HUB2	0x6560		/* USB2 Hub */
#define	USB_PRODUCT_CYPRESS_LPRDK	0xe001		/* CY4636 LP RDK Bridge */

/* Daisy Technology products */
#define	USB_PRODUCT_DAISY_DMC	0x6901		/* PhotoClip USBMediaReader */

/* Dallas Semiconductor products */
#define	USB_PRODUCT_DALLAS_J6502	0x4201		/* J-6502 speakers */
#define	USB_PRODUCT_DALLAS_USB_FOB_IBUTTON	0x2490		/* USB-FOB/iBUTTON */

/* Dell products */
#define	USB_PRODUCT_DELL_PORT	0x0058		/* Port Replicator */
#define	USB_PRODUCT_DELL_SK8125	0x2002		/* SK-8125 keyboard */
#define	USB_PRODUCT_DELL_X3	0x4002		/* Axim X3 PDA */
#define	USB_PRODUCT_DELL_X30	0x4003		/* Axim X30 PDA */
#define	USB_PRODUCT_DELL_BC02	0x8000		/* BC02 Bluetooth USB Adapter */
#define	USB_PRODUCT_DELL_TM1180	0x8100		/* TrueMobile 1180 WLAN */
#define	USB_PRODUCT_DELL_PRISM_GT_1	0x8102		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_DELL_TM350	0x8103		/* TrueMobile 350 Bluetooth USB Adapter */
#define	USB_PRODUCT_DELL_PRISM_GT_2	0x8104		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_DELL_HSDPA	0x8137		/* Dell/Novatel Wireless HSDPA Modem */
#define	USB_PRODUCT_DELL_W5500	0x8155		/* Dell Wireless W5500 HSDPA Modem */

/* DeLorme products */
#define	USB_PRODUCT_DELORME_EARTHMATE	0x0100		/* Earthmate GPS */
#define	USB_PRODUCT_DELORME_EARTHMATE_LT20	0x0200		/* Earthmate LT-20 GPS */

/* Diamond products */
#define	USB_PRODUCT_DIAMOND_RIO500USB	0x0001		/* Rio 500 USB */

/* Dick Smith Electronics (really C-Net) products */
#define	USB_PRODUCT_DICKSMITH_WL200U	0x0002		/* WL-200U */
#define	USB_PRODUCT_DICKSMITH_CHUSB611G	0x0013		/* CHUSB 611G */
#define	USB_PRODUCT_DICKSMITH_WL240U	0x0014		/* WL-240U */
#define	USB_PRODUCT_DICKSMITH_XH1153	0x5743		/* XH1153 802.11b */
#define	USB_PRODUCT_DICKSMITH_RT2573	0x9022		/* RT2573 */
#define	USB_PRODUCT_DICKSMITH_CWD854F	0x9032		/* C-Net CWD-854 rev F */
#define	USB_PRODUCT_DICKSMITH_RTL8187	0x9401		/* RTL8187 */

/* Digi International products */
#define	USB_PRODUCT_DIGI_ACCELEPORT2	0x0002		/* AccelePort USB 2 */
#define	USB_PRODUCT_DIGI_ACCELEPORT4	0x0004		/* AccelePort USB 4 */
#define	USB_PRODUCT_DIGI_ACCELEPORT8	0x0008		/* AccelePort USB 8 */

/* Digital Stream Corp. products */
#define	USB_PRODUCT_DIGITALSTREAM_PS2	0x0001		/* PS/2 Active Adapter */

/* DisplayLink products */
#define	USB_PRODUCT_DISPLAYLINK_GUC2020	0x0059		/* IOGEAR DVI GUC2020 */
#define	USB_PRODUCT_DISPLAYLINK_LD220	0x0100		/* Samsung LD220 */
#define	USB_PRODUCT_DISPLAYLINK_LD190	0x0102		/* Samsung LD190 */
#define	USB_PRODUCT_DISPLAYLINK_U70	0x0103		/* Samsung U70 */
#define	USB_PRODUCT_DISPLAYLINK_VCUD60	0x0136		/* Rextron DVI */
#define	USB_PRODUCT_DISPLAYLINK_CONV	0x0138		/* StarTech CONV-USB2DVI */
#define	USB_PRODUCT_DISPLAYLINK_DLDVI	0x0141		/* DisplayLink DVI */
#define	USB_PRODUCT_DISPLAYLINK_USBRGB	0x0150		/* IO-DATA USB-RGB */
#define	USB_PRODUCT_DISPLAYLINK_LCDUSB7X	0x0153		/* IO-DATA LCD-USB7X */
#define	USB_PRODUCT_DISPLAYLINK_LCDUSB10X	0x0156		/* IO-DATA LCD-USB10XB-T */
#define	USB_PRODUCT_DISPLAYLINK_VGA10	0x015a		/* CMP-USBVGA10 */
#define	USB_PRODUCT_DISPLAYLINK_WSDVI	0x0198		/* WS Tech DVI */
#define	USB_PRODUCT_DISPLAYLINK_EC008	0x019b		/* EasyCAP008 DVI */
#define	USB_PRODUCT_DISPLAYLINK_GXDVIU2	0x01ac		/* BUFFALO GX-DVI/U2 */
#define	USB_PRODUCT_DISPLAYLINK_LCD4300U	0x01ba		/* LCD-4300U */
#define	USB_PRODUCT_DISPLAYLINK_LCD8000U	0x01bb		/* LCD-8000U */
#define	USB_PRODUCT_DISPLAYLINK_HPDOCK	0x01d4		/* HP USB Docking */
#define	USB_PRODUCT_DISPLAYLINK_NL571	0x01d7		/* HP USB DVI */
#define	USB_PRODUCT_DISPLAYLINK_M01061	0x01e2		/* Lenovo DVI */
#define	USB_PRODUCT_DISPLAYLINK_NBDOCK	0x0215		/* VideoHome NBdock1920 */
#define	USB_PRODUCT_DISPLAYLINK_GXDVIU2B	0x0223		/* BUFFALO GX-DVI/U2B */
#define	USB_PRODUCT_DISPLAYLINK_SWDVI	0x024c		/* SUNWEIT DVI */
#define	USB_PRODUCT_DISPLAYLINK_LUM70	0x02a9		/* Lilliput UM-70 */
#define	USB_PRODUCT_DISPLAYLINK_LCD8000UD_DVI	0x02b8		/* LCD-8000UD-DVI */
#define	USB_PRODUCT_DISPLAYLINK_LDEWX015U	0x02e3		/* Logitec LDE-WX015U */
#define	USB_PRODUCT_DISPLAYLINK_LT1421WIDE	0x03e0		/* Lenovo ThinkVision LT1421 Wide */
#define	USB_PRODUCT_DISPLAYLINK_SD_U2VDH	0x046d		/* AREA SD-U2VDH */
#define	USB_PRODUCT_DISPLAYLINK_UM7X0	0x401a		/* nanovision MiMo */

/* D-Link products */
/*product DLINK DSBS25		0x0100	DSB-S25 serial adapter*/
#define	USB_PRODUCT_DLINK_DUBE100	0x1a00		/* 10/100 ethernet adapter */
#define	USB_PRODUCT_DLINK_DUBE100C1	0x1a02		/* DUB-E100 rev C1 */
#define	USB_PRODUCT_DLINK_DSB650TX4	0x200c		/* 10/100 ethernet adapter */
#define	USB_PRODUCT_DLINK_DWL120E	0x3200		/* DWL-120 rev E */
#define	USB_PRODUCT_DLINK_DWA130C	0x3301		/* DWA-130 rev C */
#define	USB_PRODUCT_DLINK_RTL8192CU_1	0x3307		/* RTL8192CU */
#define	USB_PRODUCT_DLINK_RTL8188CU	0x3308		/* RTL8188CU */
#define	USB_PRODUCT_DLINK_RTL8192CU_2	0x3309		/* RTL8192CU */
#define	USB_PRODUCT_DLINK_RTL8192CU_3	0x330a		/* RTL8192CU */
#define	USB_PRODUCT_DLINK_RTL8192CU_4	0x330b		/* RTL8192CU */
#define	USB_PRODUCT_DLINK_DWA131B	0x330d		/* DWA-131 rev B */
#define	USB_PRODUCT_DLINK_DWL122	0x3700		/* Wireless DWL122 */
#define	USB_PRODUCT_DLINK_DWLG120	0x3701		/* DWL-G120 */
#define	USB_PRODUCT_DLINK_DWL120F	0x3702		/* DWL-120 rev F */
#define	USB_PRODUCT_DLINK_DWLG122A2	0x3704		/* DWL-G122 rev A2 */
#define	USB_PRODUCT_DLINK_DWLAG132	0x3a00		/* DWL-AG132 */
#define	USB_PRODUCT_DLINK_DWLAG132_NF	0x3a01		/* DWL-AG132 */
#define	USB_PRODUCT_DLINK_DWLG132	0x3a02		/* DWL-G132 */
#define	USB_PRODUCT_DLINK_DWLG132_NF	0x3a03		/* DWL-G132 */
#define	USB_PRODUCT_DLINK_DWLAG122	0x3a04		/* DWL-AG122 */
#define	USB_PRODUCT_DLINK_DWLAG122_NF	0x3a05		/* DWL-AG122 */
#define	USB_PRODUCT_DLINK_DWLG122	0x3c00		/* AirPlus G Wireless USB Adapter */
/* product DLINK RT2570		0x3c00	RT2570 */
#define	USB_PRODUCT_DLINK_DUBE100B1	0x3c05		/* DUB-E100 rev B1 */
#define	USB_PRODUCT_DLINK_RT2870	0x3c09		/* RT2870 */
#define	USB_PRODUCT_DLINK_RT3072	0x3c0a		/* RT3072 */
#define	USB_PRODUCT_DLINK_DWA140B3	0x3c15		/* DWA-140 rev B3 */
#define	USB_PRODUCT_DLINK_DWA160B2	0x3c1a		/* DWA-160 rev B2 */
#define	USB_PRODUCT_DLINK_DWA127	0x3c1b		/* DWA-127 */
#define	USB_PRODUCT_DLINK_DWA162	0x3c1f		/* DWA-162 Wireless Adapter */
#define	USB_PRODUCT_DLINK_DSB650C	0x4000		/* 10Mbps ethernet adapter */
#define	USB_PRODUCT_DLINK_DSB650TX1	0x4001		/* 10/100 ethernet adapter */
#define	USB_PRODUCT_DLINK_DSB650TX	0x4002		/* 10/100 ethernet adapter */
#define	USB_PRODUCT_DLINK_DSB650TX_PNA	0x4003		/* 1/10/100 ethernet adapter */
#define	USB_PRODUCT_DLINK_DSB650TX3	0x400b		/* 10/100 ethernet adapter */
#define	USB_PRODUCT_DLINK_DSB650TX2	0x4102		/* 10/100 ethernet adapter */
#define	USB_PRODUCT_DLINK_DSB650	0xabc1		/* 10/100 ethernet adapter */

/* D-Link(2) products */
#define	USB_PRODUCT_DLINK2_RTL8192SU_1	0x3300		/* RTL8192SU */
#define	USB_PRODUCT_DLINK2_RTL8192SU_2	0x3302		/* RTL8192SU */
#define	USB_PRODUCT_DLINK2_DWA131A1	0x3303		/* DWA-131 A1 */
#define	USB_PRODUCT_DLINK2_WUA2340	0x3a07		/* WUA-2340 */
#define	USB_PRODUCT_DLINK2_WUA2340_NF	0x3a08		/* WUA-2340 */
#define	USB_PRODUCT_DLINK2_DWA160A2	0x3a09		/* DWA-160 A2 */
#define	USB_PRODUCT_DLINK2_DWA130D1	0x3a0f		/* DWA-130 rev D1 */
#define	USB_PRODUCT_DLINK2_AR9271	0x3a10		/* AR9271 */
#define	USB_PRODUCT_DLINK2_DWLG122C1	0x3c03		/* DWL-G122 rev C1 */
#define	USB_PRODUCT_DLINK2_WUA1340	0x3c04		/* WUA-1340 */
#define	USB_PRODUCT_DLINK2_DWA111	0x3c06		/* DWA-111 */
#define	USB_PRODUCT_DLINK2_DWA110	0x3c07		/* DWA-110 */
#define	USB_PRODUCT_DLINK2_RT2870_1	0x3c09		/* RT2870 */
#define	USB_PRODUCT_DLINK2_RT3072	0x3c0a		/* RT3072 */
#define	USB_PRODUCT_DLINK2_RT3072_1	0x3c0b		/* RT3072 */
#define	USB_PRODUCT_DLINK2_RT3070_1	0x3c0d		/* RT3070 */
#define	USB_PRODUCT_DLINK2_RT3070_2	0x3c0e		/* RT3070 */
#define	USB_PRODUCT_DLINK2_RT3070_3	0x3c0f		/* RT3070 */
#define	USB_PRODUCT_DLINK2_DWA160A1	0x3c10		/* DWA-160 A1 */
#define	USB_PRODUCT_DLINK2_RT2870_2	0x3c11		/* RT2870 */
#define	USB_PRODUCT_DLINK2_DWA130	0x3c13		/* DWA-130 */
#define	USB_PRODUCT_DLINK2_RT3070_4	0x3c15		/* RT3070 */
#define	USB_PRODUCT_DLINK2_RT3070_5	0x3c16		/* RT3070 */

/* D-Link(3) products */
#define	USB_PRODUCT_DLINK3_KVM221	0x020f		/* KVM-221 */

/* DMI products */
#define	USB_PRODUCT_DMI_SA2_0	0xb001		/* Storage Adapter */

/* DrayTek products */
#define	USB_PRODUCT_DRAYTEK_VIGOR550	0x0550		/* Vigor550 */

/* DViCO products */
#define	USB_PRODUCT_DVICO_RT3070	0xb307		/* RT3070 */

/* Dynastream Innovations */
#define	USB_PRODUCT_DYNASTREAM_ANTDEVBOARD	0x1003		/* ANT dev board */

/* E3C products */
#define	USB_PRODUCT_E3C_EC168	0x1001		/* EC168 DVB-T Adapter */

/* Edimax products */
#define	USB_PRODUCT_EDIMAX_EW7318	0x7318		/* EW-7318 */
#define	USB_PRODUCT_EDIMAX_MT7610U	0x7610		/* MT7610U */
#define	USB_PRODUCT_EDIMAX_RTL8192SU_1	0x7611		/* RTL8192SU */
#define	USB_PRODUCT_EDIMAX_RTL8192SU_2	0x7612		/* RTL8192SU */
#define	USB_PRODUCT_EDIMAX_EW7618	0x7618		/* EW-7618 */
#define	USB_PRODUCT_EDIMAX_RTL8192SU_3	0x7622		/* RTL8192SU */
#define	USB_PRODUCT_EDIMAX_RT2870_1	0x7711		/* RT2870 */
#define	USB_PRODUCT_EDIMAX_EW7717	0x7717		/* EW-7717 */
#define	USB_PRODUCT_EDIMAX_EW7718	0x7718		/* EW-7718 */
#define	USB_PRODUCT_EDIMAX_EW7722UTN	0x7722		/* EW-7722UTn */
#define	USB_PRODUCT_EDIMAX_RTL8188CU	0x7811		/* RTL8188CU */
#define	USB_PRODUCT_EDIMAX_RTL8192CU	0x7822		/* RTL8192CU */
#define	USB_PRODUCT_EDIMAX_ELECOM_WDC433SU2M	0xb711		/* ELECOM WDC-433SU2M */

/* eGalax Products */
#define	USB_PRODUCT_EGALAX_TPANEL	0x0001		/* Touch Panel */
#define	USB_PRODUCT_EGALAX_TPANEL2	0x0002		/* Touch Panel */
#define	USB_PRODUCT_EGALAX2_TPANEL	0x0001		/* Touch Panel */

/* Eicon Networks */
#define	USB_PRODUCT_EICON_DIVA852	0x4905		/* Diva 852 ISDN TA */

/* EIZO products */
#define	USB_PRODUCT_EIZO_HUB	0x0000		/* hub */
#define	USB_PRODUCT_EIZO_MONITOR	0x0001		/* monitor */

/* ELCON Systemtechnik products */
#define	USB_PRODUCT_ELCON_PLAN	0x0002		/* Goldpfeil P-LAN */

/* Elecom products */
#define	USB_PRODUCT_ELECOM_MOUSE29UO	0x0002		/* mouse 29UO */
#define	USB_PRODUCT_ELECOM_LDUSBTX0	0x200c		/* LD-USB/TX */
#define	USB_PRODUCT_ELECOM_LDUSBTX1	0x4002		/* LD-USB/TX */
#define	USB_PRODUCT_ELECOM_LDUSBLTX	0x4005		/* LD-USBL/TX */
#define	USB_PRODUCT_ELECOM_WDC150SU2M	0x4008		/* WDC-150SU2M */
#define	USB_PRODUCT_ELECOM_LDUSBTX2	0x400b		/* LD-USB/TX */
#define	USB_PRODUCT_ELECOM_LDUSB20	0x4010		/* LD-USB20 */
#define	USB_PRODUCT_ELECOM_UCSGT	0x5003		/* UC-SGT serial adapter */
#define	USB_PRODUCT_ELECOM_UCSGT0	0x5004		/* UC-SGT0 Serial */
#define	USB_PRODUCT_ELECOM_LDUSBTX3	0xabc1		/* LD-USB/TX */

/* Elsa products */
#define	USB_PRODUCT_ELSA_MODEM1	0x2265		/* ELSA Modem Board */
#define	USB_PRODUCT_ELSA_USB2ETHERNET	0x3000		/* Microlink USB2Ethernet */

/* eMPIA products */
#define	USB_PRODUCT_EMPIA_CAMERA	0x2761		/* Camera */
#define	USB_PRODUCT_EMPIA_EM2883	0x2883		/* EM2883 */

/* EMS products */
#define	USB_PRODUCT_EMS_DUAL_SHOOTER	0x0003		/* PSX gun controller converter */

/* Encore products */
#define	USB_PRODUCT_ENCORE_RT3070	0x1480		/* RT3070 */
#define	USB_PRODUCT_ENCORE_RT3070_2	0x14a1		/* RT3070 */
#define	USB_PRODUCT_ENCORE_RT3070_3	0x14a9		/* RT3070 */

/* Entrega products */
#define	USB_PRODUCT_ENTREGA_1S	0x0001		/* 1S serial connector */
#define	USB_PRODUCT_ENTREGA_2S	0x0002		/* 2S serial connector */
#define	USB_PRODUCT_ENTREGA_1S25	0x0003		/* 1S25 serial connector */
#define	USB_PRODUCT_ENTREGA_4S	0x0004		/* 4S serial connector */
#define	USB_PRODUCT_ENTREGA_E45	0x0005		/* E45 Ethernet adapter */
#define	USB_PRODUCT_ENTREGA_CENTRONICS	0x0006		/* Centronics connector */
#define	USB_PRODUCT_ENTREGA_XX1	0x0008		/* Ethernet Adapter */
#define	USB_PRODUCT_ENTREGA_1S9	0x0093		/* 1S9 serial connector */
#define	USB_PRODUCT_ENTREGA_EZUSB	0x8000		/* EZ-USB */
/*product ENTREGA SERIAL	0x8001	DB25 Serial connector*/
#define	USB_PRODUCT_ENTREGA_2U4S	0x8004		/* 2U4S serial connector/usb hub */
#define	USB_PRODUCT_ENTREGA_XX2	0x8005		/* Ethernet Adapter */
/*product ENTREGA SERIAL_DB9	0x8093	DB9 Serial connector*/

/* Epson products */
#define	USB_PRODUCT_EPSON_PRINTER1	0x0001		/* USB Printer */
#define	USB_PRODUCT_EPSON_PRINTER2	0x0002		/* ISD USB Smart Cable for Mac */
#define	USB_PRODUCT_EPSON_PRINTER3	0x0003		/* ISD USB Smart Cable */
#define	USB_PRODUCT_EPSON_PRINTER5	0x0005		/* USB Printer */
#define	USB_PRODUCT_EPSON_636	0x0101		/* Perfection 636U / 636Photo scanner */
#define	USB_PRODUCT_EPSON_610	0x0103		/* Perfection 610 scanner */
#define	USB_PRODUCT_EPSON_1200	0x0104		/* Perfection 1200U / 1200Photo scanner */
#define	USB_PRODUCT_EPSON_1600	0x0107		/* Expression 1600 scanner */
#define	USB_PRODUCT_EPSON_1640	0x010a		/* Perfection 1640SU scanner */
#define	USB_PRODUCT_EPSON_1240	0x010b		/* Perfection 1240U / 1240Photo scanner */
#define	USB_PRODUCT_EPSON_640U	0x010c		/* Perfection 640U scanner */
#define	USB_PRODUCT_EPSON_1250	0x010f		/* Perfection 1250U / 1250Photo scanner */
#define	USB_PRODUCT_EPSON_1650	0x0110		/* Perfection 1650 scanner */
#define	USB_PRODUCT_EPSON_GT9700F	0x0112		/* GT-9700F scanner */
#define	USB_PRODUCT_EPSON_2400	0x011b		/* Perfection 2400 scanner */
#define	USB_PRODUCT_EPSON_1260	0x011d		/* Perfection 1260 scanner */
#define	USB_PRODUCT_EPSON_1660	0x011e		/* Perfection 1660 scanner */
#define	USB_PRODUCT_EPSON_1670	0x011f		/* Perfection 1670 scanner */

/* e-TEK Labs products */
#define	USB_PRODUCT_ETEK_1COM	0x8007		/* Serial port */

/* Extended Systems products */
#define	USB_PRODUCT_EXTENDED_XTNDACCESS	0x0100		/* XTNDAccess IrDA */

/* Falcom products */
#define	USB_PRODUCT_FALCOM_TWIST	0x0001		/* Twist GSM/GPRS modem */
#define	USB_PRODUCT_FALCOM_SAMBA	0x0005		/* Samba 55/56 GSM/GPRS modem */

/* FeiXun Communication products */
#define	USB_PRODUCT_FEIXUN_RTL8188CU	0x0090		/* RTL8188CU */
#define	USB_PRODUCT_FEIXUN_RTL8192CU	0x0091		/* RTL8192CU */

/* Fiberline */
#define	USB_PRODUCT_FIBERLINE_WL430U	0x6003		/* WL-430U */

/* Foxconn / Hon Hai products */
#define	USB_PRODUCT_FOXCONN_AR3012	0xe04e		/* Bluetooth AR3012 */

/* Freecom products */
#define	USB_PRODUCT_FREECOM_DVD	0xfc01		/* Connector for DVD drive */

/* Future Technology Devices products */
#define	USB_PRODUCT_FTDI_SERIAL_8U232AM	0x6001		/* 8U232AM Serial converter */
#define	USB_PRODUCT_FTDI_SERIAL_232RL	0x6006		/* FT232RL Serial converter */
#define	USB_PRODUCT_FTDI_SERIAL_2232C	0x6010		/* 2232C USB dual FAST SERIAL ADAPTER */
#define	USB_PRODUCT_FTDI_SERIAL_4232H	0x6011		/* 2232H USB quad FAST SERIAL ADAPTER */
#define	USB_PRODUCT_FTDI_SERIAL_232H	0x6014		/* C232HM USB Multipurpose UART */
#define	USB_PRODUCT_FTDI_SERIAL_230X	0x6015		/* FT230X Serial converter */
#define	USB_PRODUCT_FTDI_PS2KBDMS	0x8371		/* PS/2 Keyboard/Mouse */
#define	USB_PRODUCT_FTDI_SERIAL_8U100AX	0x8372		/* 8U100AX Serial converter */
#define	USB_PRODUCT_FTDI_OPENRD_JTAGKEY	0x9e90		/* OpenRD JTAGKey FT2232D B */
#define	USB_PRODUCT_FTDI_BEAGLEBONE	0xa6d0		/* BeagleBone */
#define	USB_PRODUCT_FTDI_MAXSTREAM_PKG_U	0xee18		/* MaxStream PKG-U */
#define	USB_PRODUCT_FTDI_MHAM_KW	0xeee8		/* KW */
#define	USB_PRODUCT_FTDI_MHAM_YS	0xeee9		/* YS */
#define	USB_PRODUCT_FTDI_MHAM_Y6	0xeeea		/* Y6 */
#define	USB_PRODUCT_FTDI_MHAM_Y8	0xeeeb		/* Y8 */
#define	USB_PRODUCT_FTDI_MHAM_IC	0xeeec		/* IC */
#define	USB_PRODUCT_FTDI_MHAM_DB9	0xeeed		/* DB9 */
#define	USB_PRODUCT_FTDI_MHAM_RS232	0xeeee		/* RS232 */
#define	USB_PRODUCT_FTDI_MHAM_Y9	0xeeef		/* Y9 */
#define	USB_PRODUCT_FTDI_COASTAL_TNCX	0xf448		/* Coastal ChipWorks TNC-X */
#define	USB_PRODUCT_FTDI_CTI_485_MINI	0xf608		/* CTI 485 Mini */
#define	USB_PRODUCT_FTDI_CTI_NANO_485	0xf60b		/* CTI Nano 485 */
#define	USB_PRODUCT_FTDI_LCD_MX200_USB	0xfa01		/* Matrix Orbital MX2/MX3/MX6 Series */
#define	USB_PRODUCT_FTDI_LCD_MX4_MX5_USB	0xfa02		/* Matrix Orbital MX4/MX5 Series LCD */
#define	USB_PRODUCT_FTDI_LCD_LK202_24_USB	0xfa03		/* Matrix Orbital LK/VK/PK202-24 LCD */
#define	USB_PRODUCT_FTDI_LCD_LK204_24_USB	0xfa04		/* Matrix Orbital LK/VK204-24 LCD */
#define	USB_PRODUCT_FTDI_LCD_CFA_632	0xfc08		/* Crystalfontz CFA-632 LCD */
#define	USB_PRODUCT_FTDI_LCD_CFA_634	0xfc09		/* Crystalfontz CFA-634 LCD */
#define	USB_PRODUCT_FTDI_LCD_CFA_633	0xfc0b		/* Crystalfontz CFA-633 LCD */
#define	USB_PRODUCT_FTDI_LCD_CFA_631	0xfc0c		/* Crystalfontz CFA-631 LCD */
#define	USB_PRODUCT_FTDI_LCD_CFA_635	0xfc0d		/* Crystalfontz CFA-635 LCD */
#define	USB_PRODUCT_FTDI_SEMC_DSS20	0xfc82		/* SEMC DSS-20 SyncStation */
#define	USB_PRODUCT_xxFTDI_SHEEVAPLUG_JTAG	0x9e8f		/* SheevaPlug JTAGKey */

/* Fuji photo products */
#define	USB_PRODUCT_FUJIPHOTO_MASS0100	0x0100		/* Mass Storage */

/* Fujitsu protducts */
#define	USB_PRODUCT_FUJITSU_AH_F401U	0x105b		/* AH-F401U Air H device */

/* Fujitsu Siemens Computers products */
#define	USB_PRODUCT_FSC_E5400	0x1009		/* PrismGT USB 2.0 WLAN */

/* General Instruments (Motorola) products */
#define	USB_PRODUCT_GENERALINSTMNTS_SB5100	0x5100		/* SURFboard SB5100 Cable modem */

/* Genesys Logic products */
#define	USB_PRODUCT_GENESYS_GENELINK	0x05e3		/* GeneLink Host-Host Bridge */
#define	USB_PRODUCT_GENESYS_GL650	0x0604		/* GL650 Hub */
#define	USB_PRODUCT_GENESYS_GL641USB	0x0700		/* GL641USB CompactFlash Card Reader */
#define	USB_PRODUCT_GENESYS_GL641USB2IDE_2	0x0701		/* GL641USB USB-IDE Bridge */
#define	USB_PRODUCT_GENESYS_GL641USB2IDE	0x0702		/* GL641USB USB-IDE Bridge */

/* GIGABYTE products */
#define	USB_PRODUCT_GIGABYTE_GN54G	0x8001		/* GN-54G */
#define	USB_PRODUCT_GIGABYTE_GNBR402W	0x8002		/* GN-BR402W */
#define	USB_PRODUCT_GIGABYTE_GNWLBM101	0x8003		/* GN-WLBM101 */
#define	USB_PRODUCT_GIGABYTE_GNWBKG	0x8007		/* GN-WBKG */
#define	USB_PRODUCT_GIGABYTE_GNWB01GS	0x8008		/* GN-WB01GS */
#define	USB_PRODUCT_GIGABYTE_GNWI05GS	0x800a		/* GN-WI05GS */
#define	USB_PRODUCT_GIGABYTE_RT2870_1	0x800b		/* RT2870 */
#define	USB_PRODUCT_GIGABYTE_GNWB31N	0x800c		/* GN-WB31N */
#define	USB_PRODUCT_GIGABYTE_GNWB32L	0x800d		/* GN-WB32L */

/* Gigaset products */
#define	USB_PRODUCT_GIGASET_WLAN	0x0701		/* WLAN */
#define	USB_PRODUCT_GIGASET_SMCWUSBTG	0x0710		/* SMCWUSBT-G */
#define	USB_PRODUCT_GIGASET_SMCWUSBTG_NF	0x0711		/* SMCWUSBT-G */
#define	USB_PRODUCT_GIGASET_AR5523	0x0712		/* AR5523 */
#define	USB_PRODUCT_GIGASET_AR5523_NF	0x0713		/* AR5523 */
#define	USB_PRODUCT_GIGASET_RT2573	0x0722		/* RT2573 */
#define	USB_PRODUCT_GIGASET_RT3070_1	0x0740		/* RT3070 */
#define	USB_PRODUCT_GIGASET_RT3070_2	0x0744		/* RT3070 */

/* G.Mate, Inc products */
#define	USB_PRODUCT_GMATE_YP3X00	0x1001		/* YP3X00 PDA */

/* MTK products */
#define	USB_PRODUCT_MTK_GPS_RECEIVER	0x3329		/* GPS receiver */
#define	USB_PRODUCT_MTK_MT7610U	0x7610		/* MT7610U */
#define	USB_PRODUCT_MTK_MT7630U	0x7630		/* MT7630U */
#define	USB_PRODUCT_MTK_MT7650U	0x7650		/* MT7650U */

/* Garmin products */
#define	USB_PRODUCT_GARMIN_FORERUNNER305	0x0003		/* Forerunner 305 */

/* Globespan products */
#define	USB_PRODUCT_GLOBESPAN_PRISM_GT_1	0x2000		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_GLOBESPAN_PRISM_GT_2	0x2002		/* PrismGT USB 2.0 WLAN */

/* GoHubs products */
#define	USB_PRODUCT_GOHUBS_GOCOM232	0x1001		/* GoCOM232 Serial converter */

/* Good Way Technology products */
#define	USB_PRODUCT_GOODWAY_GWUSB2E	0x6200		/* GWUSB2E */
#define	USB_PRODUCT_GOODWAY_RT2573	0xc019		/* RT2573 */

/* Gravis products */
#define	USB_PRODUCT_GRAVIS_GAMEPADPRO	0x4001		/* GamePad Pro */

/* GREENHOUSE products */
#define	USB_PRODUCT_GREENHOUSE_KANA21	0x0001		/* CF-writer with Portable MP3 Player */

/* Griffin Technology */
#define	USB_PRODUCT_GRIFFIN_IMATE	0x0405		/* iMate, ADB adapter */
#define	USB_PRODUCT_GRIFFIN_POWERMATE	0x0410		/* PowerMate Assignable Controller */

/* Gude ADS */
#define	USB_PRODUCT_GUDE_DCF	0xdcf7		/* Exper mouseCLOCK USB */

/* Guillemot Corporation */
#define	USB_PRODUCT_GUILLEMOT_DALEADER	0xa300		/* DA Leader */
#define	USB_PRODUCT_GUILLEMOT_HWGUSB254	0xe000		/* HWGUSB2-54 WLAN */
#define	USB_PRODUCT_GUILLEMOT_HWGUSB254LB	0xe010		/* HWGUSB2-54-LB */
#define	USB_PRODUCT_GUILLEMOT_HWGUSB254V2AP	0xe020		/* HWGUSB2-54V2-AP */
#define	USB_PRODUCT_GUILLEMOT_HWNU300	0xe030		/* HWNU-300 */
#define	USB_PRODUCT_GUILLEMOT_HWNUM300	0xe031		/* HWNUm-300 */
#define	USB_PRODUCT_GUILLEMOT_HWGUN54	0xe032		/* HWGUn-54 */
#define	USB_PRODUCT_GUILLEMOT_HWNUP150	0xe033		/* HWNUP-150 */
#define	USB_PRODUCT_GUILLEMOT_RTL8192CU	0xe035		/* RTL8192CU */

/* Hagiwara products */
#define	USB_PRODUCT_HAGIWARA_FGSM	0x0002		/* FlashGate SmartMedia Card Reader */
#define	USB_PRODUCT_HAGIWARA_FGCF	0x0003		/* FlashGate CompactFlash Card Reader */
#define	USB_PRODUCT_HAGIWARA_FG	0x0005		/* FlashGate */

/* HAL Corporation products */
#define	USB_PRODUCT_HAL_IMR001	0x0011		/* Crossam2+USB IR commander */

/* Handspring, Inc. */
#define	USB_PRODUCT_HANDSPRING_VISOR	0x0100		/* Handspring Visor */
#define	USB_PRODUCT_HANDSPRING_TREO	0x0200		/* Handspring Treo */
#define	USB_PRODUCT_HANDSPRING_TREO600	0x0300		/* Handspring Treo 600 */

/* Hank Connection */
#define	USB_PRODUCT_HANK_HP5187	0x3713		/* HP Wireless Keyboard&Mouse */

/* Hauppauge Computer Works */
#define	USB_PRODUCT_HAUPPAUGE_WINTV_USB_FM	0x4d12		/* WinTV USB FM */
#define	USB_PRODUCT_HAUPPAUGE2_WINTV_USB2_FM	0xb110		/* WinTV USB2 FM */
#define	USB_PRODUCT_HAUPPAUGE2_WINTV_NOVAT_7700M	0x7050		/* WinTV Nova-T DVB-T */
#define	USB_PRODUCT_HAUPPAUGE2_WINTV_NOVAT_7700PC	0x7060		/* WinTV Nova-T DVB-T */
#define	USB_PRODUCT_HAUPPAUGE2_WINTV_NOVAT_7070P	0x7070		/* WinTV Nova-T DVB-T */

/* Hawking Technologies products */
#define	USB_PRODUCT_HAWKING_RT2870_1	0x0001		/* RT2870 */
#define	USB_PRODUCT_HAWKING_RT2870_2	0x0003		/* RT2870 */
#define	USB_PRODUCT_HAWKING_HWUN2	0x0009		/* HWUN2 */
#define	USB_PRODUCT_HAWKING_RT3070	0x000b		/* RT3070 */
#define	USB_PRODUCT_HAWKING_RT2870_3	0x0013		/* RT2870 */
#define	USB_PRODUCT_HAWKING_RTL8192SU_1	0x0015		/* RTL8192SU */
#define	USB_PRODUCT_HAWKING_RTL8192SU_2	0x0016		/* RTL8192SU */
#define	USB_PRODUCT_HAWKING_RT2870_4	0x0017		/* RT2870 */
#define	USB_PRODUCT_HAWKING_RT2870_5	0x0018		/* RT2870 */
#define	USB_PRODUCT_HAWKING_RTL8192CU	0x0019		/* RTL8192CU */
#define	USB_PRODUCT_HAWKING_RTL8192CU_2	0x0020		/* RTL8192CU */
#define	USB_PRODUCT_HAWKING_UF100	0x400c		/* 10/100 USB Ethernet */

/* Hitachi, Ltd. products */
#define	USB_PRODUCT_HITACHI_DZMV100A	0x0004		/* DVD-CAM DZ-MV100A Camcorder */
#define	USB_PRODUCT_HITACHI_DVDCAM_USB	0x001e		/* DVDCAM USB HS Interface */

/* Holtek Semiconductor products */
#define	USB_PRODUCT_HOLTEK_MOP35	0x0499		/* MOP-35 */

/* Hosiden Corporation products */
#define	USB_PRODUCT_HOSIDEN_PPP	0x0011		/* ParaParaParadise Controller */

/* HP products */
#define	USB_PRODUCT_HP_895C	0x0004		/* DeskJet 895C */
#define	USB_PRODUCT_HP_4100C	0x0101		/* Scanjet 4100C */
#define	USB_PRODUCT_HP_S20	0x0102		/* Photosmart S20 */
#define	USB_PRODUCT_HP_880C	0x0104		/* DeskJet 880C */
#define	USB_PRODUCT_HP_4200C	0x0105		/* ScanJet 4200C */
#define	USB_PRODUCT_HP_CDWRITERPLUS	0x0107		/* CD-Writer Plus */
#define	USB_PRODUCT_HP_KBDHUB	0x010c		/* Multimedia Keyboard Hub */
#define	USB_PRODUCT_HP_HN210W	0x011c		/* HN210W */
#define	USB_PRODUCT_HP_6200C	0x0201		/* ScanJet 6200C */
#define	USB_PRODUCT_HP_S20b	0x0202		/* PhotoSmart S20 */
#define	USB_PRODUCT_HP_815C	0x0204		/* DeskJet 815C */
#define	USB_PRODUCT_HP_3300C	0x0205		/* ScanJet 3300C */
#define	USB_PRODUCT_HP_CDW8200	0x0207		/* CD-Writer Plus 8200e */
#define	USB_PRODUCT_HP_1220C	0x0212		/* DeskJet 1220C */
#define	USB_PRODUCT_HP_810C	0x0304		/* DeskJet 810C/812C */
#define	USB_PRODUCT_HP_4300C	0x0305		/* Scanjet 4300C */
#define	USB_PRODUCT_HP_CD4E	0x0307		/* CD-Writer+ CD-4e */
#define	USB_PRODUCT_HP_G85XI	0x0311		/* OfficeJet G85xi */
#define	USB_PRODUCT_HP_1200	0x0317		/* LaserJet 1200 */
#define	USB_PRODUCT_HP_5200C	0x0401		/* Scanjet 5200C */
#define	USB_PRODUCT_HP_830C	0x0404		/* DeskJet 830C */
#define	USB_PRODUCT_HP_3400CSE	0x0405		/* ScanJet 3400cse */
#define	USB_PRODUCT_HP_885C	0x0504		/* DeskJet 885C */
#define	USB_PRODUCT_HP_6300C	0x0601		/* Scanjet 6300C */
#define	USB_PRODUCT_HP_840C	0x0604		/* DeskJet 840c */
#define	USB_PRODUCT_HP_2200C	0x0605		/* ScanJet 2200C */
#define	USB_PRODUCT_HP_5300C	0x0701		/* Scanjet 5300C */
#define	USB_PRODUCT_HP_4400C	0x0705		/* Scanjet 4400C */
#define	USB_PRODUCT_HP_816C	0x0804		/* DeskJet 816C */
#define	USB_PRODUCT_HP_2300D	0x0b17		/* Laserjet 2300d */
#define	USB_PRODUCT_HP_970CSE	0x1004		/* Deskjet 970Cse */
#define	USB_PRODUCT_HP_5400C	0x1005		/* Scanjet 5400C */
#define	USB_PRODUCT_HP_2215	0x1016		/* iPAQ 22xx/Jornada 548 */
#define	USB_PRODUCT_HP_959C	0x1104		/* Deskjet 959C */
#define	USB_PRODUCT_HP_568J	0x1116		/* Jornada 568 */
#define	USB_PRODUCT_HP_930C	0x1204		/* DeskJet 930c */
#define	USB_PRODUCT_HP_P2000U	0x1801		/* Inkjet P-2000U */
#define	USB_PRODUCT_HP_RNDIS	0x1c1d		/* Generic RNDIS */
#define	USB_PRODUCT_HP_640C	0x2004		/* DeskJet 640c */
#define	USB_PRODUCT_HP_4670V	0x3005		/* ScanJet 4670v */
#define	USB_PRODUCT_HP_P1100	0x3102		/* Photosmart P1100 */
#define	USB_PRODUCT_HP_V125W	0x3307		/* v125w */
#define	USB_PRODUCT_HP_6127	0x3504		/* Deskjet 6127 */
#define	USB_PRODUCT_HP_HN210E	0x811c		/* Ethernet HN210E */

/* HP products */
#define	USB_PRODUCT_HP3_RTL8188CU	0x1629		/* RTL8188CU */
#define	USB_PRODUCT_HP2_C500	0x6002		/* PhotoSmart C500 */

/* HTC products */
#define	USB_PRODUCT_HTC_ANDROID	0x0ffe		/* Android */

/* Huawei Technologies products */
#define	USB_PRODUCT_HUAWEI_MOBILE	0x1001		/* Huawei Mobile */
#define	USB_PRODUCT_HUAWEI_E220	0x1003		/* Huawei E220 */
#define	USB_PRODUCT_HUAWEI_U8150	0x1037		/* Huawei U8150 */
#define	USB_PRODUCT_HUAWEI_EM770W	0x1404		/* Huawei EM770W */
#define	USB_PRODUCT_HUAWEI_E1750	0x140c		/* Huawei E1750 */
#define	USB_PRODUCT_HUAWEI_E1750INIT	0x1446		/* Huawei E1750 USB CD */
#define	USB_PRODUCT_HUAWEI_K3765	0x1465		/* Huawei K3765 */
#define	USB_PRODUCT_HUAWEI_E1820	0x14ac		/* Huawei E1820 */
#define	USB_PRODUCT_HUAWEI_E171INIT	0x14fe		/* Huawei E171 USB CD */
#define	USB_PRODUCT_HUAWEI_E171	0x1506		/* Huawei E171 */
#define	USB_PRODUCT_HUAWEI_E353	0x1507		/* Huawei E353 */
#define	USB_PRODUCT_HUAWEI_K3765INIT	0x1520		/* Huawei K3765 USB CD */
#define	USB_PRODUCT_HUAWEI_E353INIT	0x1f01		/* Huawei E353 USB CD */

/* Huawei-3Com products */
#define	USB_PRODUCT_HUAWEI3COM_RT2573	0x0009		/* RT2573 */

/* Hyundai CuriTel (Audiovox, Pantech) products */
#define	USB_PRODUCT_HYUNDAI_PC5740	0x3701		/* PC5740 EVDO */
#define	USB_PRODUCT_HYUNDAI_UM175	0x3714		/* UM175 EVDO */

/* IBM Corporation */
#define	USB_PRODUCT_IBM_OPTTRAVELMOUSE	0x3107		/* Optical */
#define	USB_PRODUCT_IBM_USBCDROMDRIVE	0x4427		/* USB CD-ROM Drive */

/* Iiyama products */
#define	USB_PRODUCT_IIYAMA_HUB	0x0201		/* Hub */

/* Imation */
#define	USB_PRODUCT_IMATION_FLASHGO	0xb000		/* Flash Go! */

/* Inside Out Networks products */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT4	0x0001		/* EdgePort/4 RS232 */
#define	USB_PRODUCT_INSIDEOUT_HUBPORT7	0x0002		/* Hubport/7 */
#define	USB_PRODUCT_INSIDEOUT_RAPIDPORT4	0x0003		/* Rapidport/4 */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT4T	0x0004		/* Edgeport/4 RS232 for Telxon */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT2	0x0005		/* Edgeport/2 RS232 */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT4I	0x0006		/* Edgeport/4 RS422 */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT2I	0x0007		/* Edgeport/2 RS422/RS485 */
#define	USB_PRODUCT_INSIDEOUT_HUBPORT4	0x0008		/* Hubport/4 */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT8HAND	0x0009		/* Hand-built Edgeport/8 */
#define	USB_PRODUCT_INSIDEOUT_MULTIMODEM	0x000A		/* MultiTech version of RP/4 */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORTPPORT	0x000B		/* Edgeport/(4)21 Parallel port (USS720) */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT421	0x000C		/* Edgeport/421 Hub+RS232+Parallel */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT21	0x000D		/* Edgeport/21 RS232+Parallel */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT8DC	0x000E		/* 1/2 Edgeport/8 (2 EP/4s on 1 PCB) */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT8	0x000F		/* Edgeport/8 */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT2DIN	0x0010		/* Edgeport/2 RS232 / Apple DIN connector */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT4DIN	0x0011		/* Edgeport/4 RS232 / Apple DIN connector */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT16DC	0x0012		/* 1/2 Edgeport/16 (2 EP/8s on 1 PCB)) */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORTCOMP	0x0013		/* Edgeport Compatible */
#define	USB_PRODUCT_INSIDEOUT_EDGEPORT8I	0x0014		/* Edgeport/8 RS422 */
#define	USB_PRODUCT_INSIDEOUT_MT4X56USB	0x1403		/* OEM device */

/* In-System products */
#define	USB_PRODUCT_INSYSTEM_F5U002	0x0002		/* Parallel printer adapter */
#define	USB_PRODUCT_INSYSTEM_ATAPI	0x0031		/* ATAPI adapter */
#define	USB_PRODUCT_INSYSTEM_IDEUSB2	0x0060		/* USB2 Storage Adapter */
#define	USB_PRODUCT_INSYSTEM_ISD110	0x0200		/* IDE adapter ISD110 */
#define	USB_PRODUCT_INSYSTEM_ISD105	0x0202		/* IDE adapter ISD105 */
#define	USB_PRODUCT_INSYSTEM_DRIVEV2	0x0301		/* Portable USB Harddrive V2 */
#define	USB_PRODUCT_INSYSTEM_DRIVEV2_5	0x0351		/* Portable USB Harddrive V2 */
#define	USB_PRODUCT_INSYSTEM_USBCABLE	0x081a		/* USB cable */
#define	USB_PRODUCT_INSYSTEM_ADAPTERV2	0x5701		/* USB Storage Adapter V2 */

/* Intel products */
#define	USB_PRODUCT_INTEL_EASYPC_CAMERA	0x0110		/* Easy PC Camera */
#define	USB_PRODUCT_INTEL_AP310	0x0200		/* AP310 AnyPoint II */
#define	USB_PRODUCT_INTEL_I2011B	0x1111		/* Wireless 2011B */
#define	USB_PRODUCT_INTEL_TESTBOARD	0x9890		/* 82930 test board */

#define	USB_PRODUCT_INTEL2_RMH	0x0020		/* Rate Matching Hub */
#define	USB_PRODUCT_INTEL2_RMH2	0x0024		/* Rate Matching Hub */

/* Intersil products */
#define	USB_PRODUCT_INTERSIL_PRISM_GT	0x1000		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_INTERSIL_PRISM_2X	0x3642		/* Prism2.x WLAN */

/* Intrepid Control Systems products */
#define	USB_PRODUCT_INTREPIDCS_VALUECAN	0x0601		/* ValueCAN */
#define	USB_PRODUCT_INTREPIDCS_NEOVI	0x0701		/* NeoVI Blue */

/* I-O DATA products */
#define	USB_PRODUCT_IODATA_IU_CD2	0x0204		/* DVD Multi-plus unit iU-CD2 */
#define	USB_PRODUCT_IODATA_DVR_UEH8	0x0206		/* DVD Multi-plus unit DVR-UEH8 */
#define	USB_PRODUCT_IODATA_USBSSMRW	0x0314		/* USB-SSMRW SD-card adapter */
#define	USB_PRODUCT_IODATA_USBSDRW	0x031e		/* USB-SDRW SD-card adapter */
#define	USB_PRODUCT_IODATA_USBETT	0x0901		/* USB ET/T */
#define	USB_PRODUCT_IODATA_USBETTX	0x0904		/* USB ET/TX */
#define	USB_PRODUCT_IODATA_USBETTXS	0x0913		/* USB ET/TX-S */
#define	USB_PRODUCT_IODATA_USBWNB11A	0x0919		/* USB WN-B11 */
#define	USB_PRODUCT_IODATA_USBWNB11	0x0922		/* USB Airport WN-B11 */
#define	USB_PRODUCT_IODATA_USBWNG54US	0x0928		/* USB WN-G54/US */
#define	USB_PRODUCT_IODATA_USBWNG54US_NF	0x0929		/* USB WN-G54/US */
#define	USB_PRODUCT_IODATA_ETXUS2	0x092a		/* ETX-US2 */
#define	USB_PRODUCT_IODATA_ETGUS2	0x0930		/* ETG-US2 */
#define	USB_PRODUCT_IODATA_FT232R	0x093c		/* FT232R */
#define	USB_PRODUCT_IODATA_WNGDNUS2	0x093f		/* WN-GDN/US2 */
#define	USB_PRODUCT_IODATA_RT3072_1	0x0944		/* RT3072 */
#define	USB_PRODUCT_IODATA_RT3072_2	0x0945		/* RT3072 */
#define	USB_PRODUCT_IODATA_RT3072_3	0x0947		/* RT3072 */
#define	USB_PRODUCT_IODATA_RT3072_4	0x0948		/* RT3072 */
#define	USB_PRODUCT_IODATA_WNG150UM	0x094c		/* WN-G150UM */
#define	USB_PRODUCT_IODATA_RTL8192CU	0x0950		/* RTL8192CU */
#define	USB_PRODUCT_IODATA_USBRSAQ	0x0a03		/* USB serial adapter USB-RSAQ1 */
#define	USB_PRODUCT_IODATA_USBRSAQ5	0x0a0e		/* USB serial adapter USB-RSAQ5 */

/* I-O DATA(2) products */
#define	USB_PRODUCT_IODATA2_USB2SC	0x0a09		/* USB2.0-SCSI Bridge USB2-SC */

/* Iomega products */
#define	USB_PRODUCT_IOMEGA_ZIP100	0x0001		/* Zip 100 */
#define	USB_PRODUCT_IOMEGA_ZIP250	0x0030		/* Zip 250 */
#define	USB_PRODUCT_IOMEGA_ZIP250_2	0x0032		/* Zip 250 */
#define	USB_PRODUCT_IOMEGA_CDRW	0x0055		/* CDRW 9602 */

/* iRiver products */
#define	USB_PRODUCT_IRIVER_IFP_1XX	0x1101		/* iFP-1xx */
#define	USB_PRODUCT_IRIVER_IFP_3XX	0x1103		/* iFP-3xx */
#define	USB_PRODUCT_IRIVER_IFP_5XX	0x1105		/* iFP-5xx */

/* Jablotron products */
#define	USB_PRODUCT_JABLOTRON_PC60B	0x0001		/* PC-60B */

/* Jaton products */
#define	USB_PRODUCT_JATON_EDA	0x5704		/* Ethernet Device Adapter */

/* Jenoptik products */
#define	USB_PRODUCT_JENOPTIK_JD350	0x5300		/* JD 350 Camera/mp3 player */

/* JMicron products */
#define	USB_PRODUCT_JMICRON_JM20329	0x2329		/* USB to ATA/ATAPI Bridge */
#define	USB_PRODUCT_JMICRON_JM20336	0x2336		/* USB to SATA Bridge */
#define	USB_PRODUCT_JMICRON_JM20337	0x2338		/* USB to ATA/ATAPI Bridge */

/* JRC products */
#define	USB_PRODUCT_JRC_AH_J3001V_J3002V	0x0001		/* AirH\"PHONE AH-J3001V/J3002V */

/* JVC products */
#define	USB_PRODUCT_JVC_GR_DX95	0x000a		/* GR-DX95 */
#define	USB_PRODUCT_JVC_MP_PRX1	0x3008		/* MP-PRX1 Ethernet */
#define	USB_PRODUCT_JVC_MP_XP7250_WL	0x3009		/* MP-XP7250 Builtin WLAN */

/* Kawasaki products */
#define	USB_PRODUCT_KLSI_DUH3E10BT	0x0008		/* 10BT Ethernet adapter, in the DU-H3E */
#define	USB_PRODUCT_KLSI_DUH3E10BTN	0x0009		/* 10BT Ethernet adapter, in the DU-H3E */

/* Kawatsu products */
#define	USB_PRODUCT_KAWATSU_MH4000P	0x0003		/* MiniHub 4000P */
#define	USB_PRODUCT_KAWATSU_KC180	0x0180		/* KC-180 IrDA */

/* Keisokugiken products */
#define	USB_PRODUCT_KEISOKUGIKEN_USBDAQ	0x0068		/* HKS-0200 USBDAQ */

/* Kensington products */
#define	USB_PRODUCT_KENSINGTON_ORBIT	0x1003		/* Orbit USB/PS2 trackball */
#define	USB_PRODUCT_KENSINGTON_TURBOBALL	0x1005		/* TurboBall */
#define	USB_PRODUCT_KENSINGTON_ORBIT_MAC	0x1009		/* Orbit USB trackball for Mac */
#define	USB_PRODUCT_KENSINGTON_VIDEOCAM_VGA	0x5002		/* VideoCAM VGA */

/* Keyspan products */
#define	USB_PRODUCT_KEYSPAN_USA28_NF	0x0101		/* USA-28 serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA28X_NF	0x0102		/* USA-28X serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA19_NF	0x0103		/* USA-19 serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA18_NF	0x0104		/* USA-18 serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA18X_NF	0x0105		/* USA-18X serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA19W_NF	0x0106		/* USA-19W serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA19	0x0107		/* USA-19 serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA19W	0x0108		/* USA-19W serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA49W_NF	0x0109		/* USA-49W serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA49W	0x010a		/* USA-49W serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA19QI_NF	0x010b		/* USA-19QI serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA19QI	0x010c		/* USA-19QI serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA19Q_NF	0x010d		/* USA-19Q serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA19Q	0x010e		/* USA-19Q serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA28	0x010f		/* USA-28 serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA28XXB	0x0110		/* USA-28X/XB serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA18	0x0111		/* USA-18 serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA18X	0x0112		/* USA-18X serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA28XB_NF	0x0113		/* USA-28XB serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA28XA_NF	0x0114		/* USA-28XB serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA28XA	0x0115		/* USA-28XA serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA18XA_NF	0x0116		/* USA-18XA serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA18XA	0x0117		/* USA-18XA serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA19QW_NF	0x0118		/* USA-19QW serial adapter (no firmware) */
#define	USB_PRODUCT_KEYSPAN_USA19QW	0x0119		/* USA-19QW serial adapter */
#define	USB_PRODUCT_KEYSPAN_USA19H	0x0121		/* USA-19H serial adapter */
#define	USB_PRODUCT_KEYSPAN_UIA10	0x0201		/* UIA-10 remote control */
#define	USB_PRODUCT_KEYSPAN_UIA11	0x0202		/* UIA-11 remote control */

/* Kingston products */
#define	USB_PRODUCT_KINGSTON_XX1	0x0008		/* Ethernet Adapter */
#define	USB_PRODUCT_KINGSTON_KNU101TX	0x000a		/* KNU101TX USB Ethernet */
#define	USB_PRODUCT_KINGSTON_DT102_G2	0x1624		/* DT 102 G2 */
#define	USB_PRODUCT_KINGSTON_DT101_II	0x1625		/* DT 101 II */
#define	USB_PRODUCT_KINGSTON_DTMINI10	0x162c		/* DT Mini 10 */
#define	USB_PRODUCT_KINGSTON_DT101_G2	0x1642		/* DT 101 G2 */
#define	USB_PRODUCT_KINGSTON_DT100_G2	0x6544		/* DT 100 G2 */

/* KingSun products */
#define	USB_PRODUCT_KINGSUN_IRDA	0x4200		/* KingSun/DonShine IrDA */

/* Kodak products */
#define	USB_PRODUCT_KODAK_DC220	0x0100		/* Digital Science DC220 */
#define	USB_PRODUCT_KODAK_DC260	0x0110		/* Digital Science DC260 */
#define	USB_PRODUCT_KODAK_DC265	0x0111		/* Digital Science DC265 */
#define	USB_PRODUCT_KODAK_DC290	0x0112		/* Digital Science DC290 */
#define	USB_PRODUCT_KODAK_DC240	0x0120		/* Digital Science DC240 */
#define	USB_PRODUCT_KODAK_DC280	0x0130		/* Digital Science DC280 */
#define	USB_PRODUCT_KODAK_DX4900	0x0550		/* EasyShare DX4900 */

/* Konica Corp. Products */
#define	USB_PRODUCT_KONICA_CAMERA	0x0720		/* Digital Color Camera */

/* KYE products */
#define	USB_PRODUCT_KYE_NICHE	0x0001		/* Niche mouse */
#define	USB_PRODUCT_KYE_NETSCROLL	0x0003		/* Genius NetScroll mouse */
#define	USB_PRODUCT_KYE_G07	0x1002		/* MaxFire G-07 gamepad */
#define	USB_PRODUCT_KYE_FLIGHT2000	0x1004		/* Flight 2000 joystick */
#define	USB_PRODUCT_KYE_VIVIDPRO	0x2001		/* ColorPage Vivid-Pro scanner */

/* Kyocera products */
#define	USB_PRODUCT_KYOCERA_AHK3001V	0x0203		/* AH-K3001V */

/* LaCie products */
#define	USB_PRODUCT_LACIE_PKTDRV	0x0211		/* PocketDrive */
#define	USB_PRODUCT_LACIE_HD	0xa601		/* Hard Disk */
#define	USB_PRODUCT_LACIE_CDRW	0xa602		/* CD R/W */

/* Lenovo products */
#define	USB_PRODUCT_LENOVO_AX88179	0x304b		/* AX88179 USB 3.0 gigabit ethernet controller */
#define	USB_PRODUCT_LENOVO_COMPACTKBDWTP	0x6047		/* ThinkPad Compact USB keyboard with TrackPoint */
#define	USB_PRODUCT_LENOVO_ETHERNET	0x7203		/* USB 2.0 Ethernet */

/* Lexar products */
#define	USB_PRODUCT_LEXAR_JUMPSHOT	0x0001		/* jumpSHOT CompactFlash Reader */
#define	USB_PRODUCT_LEXAR_2662WAR	0xa002		/* 2662W-AR */
#define	USB_PRODUCT_LEXAR_MCR	0xb018		/* Multi-Card Reader */

/* Lexmark products */
#define	USB_PRODUCT_LEXMARK_S2450	0x0009		/* Optra S 2450 */

/* Linksys products */
#define	USB_PRODUCT_LINKSYS_MAUSB2	0x0105		/* Camedia MAUSB-2 */
#define	USB_PRODUCT_LINKSYS_USB10TX1	0x200c		/* USB10TX */
#define	USB_PRODUCT_LINKSYS_USB10T	0x2202		/* USB10T Ethernet */
#define	USB_PRODUCT_LINKSYS_USB100TX	0x2203		/* USB100TX Ethernet */
#define	USB_PRODUCT_LINKSYS_USB100H1	0x2204		/* USB100H1 Ethernet/HPNA */
#define	USB_PRODUCT_LINKSYS_USB10TA	0x2206		/* USB10TA Ethernet */
#define	USB_PRODUCT_LINKSYS_WUSB11	0x2211		/* WUSB11 Wireless USB Network Adapter */
#define	USB_PRODUCT_LINKSYS_WUSB11_25	0x2212		/* WUSB11 Wireless USB Network Adapter (version 2.5) */
#define	USB_PRODUCT_LINKSYS_WUSB12_11	0x2213		/* WUSB12 802.11b v1.1 */
#define	USB_PRODUCT_LINKSYS_USB10TX2	0x400b		/* USB10TX */
#define	USB_PRODUCT_LINKSYS2_WUSB11	0x2219		/* WUSB11 */
#define	USB_PRODUCT_LINKSYS2_NWU11B	0x2219		/* Network Everywhere NWU11B */
#define	USB_PRODUCT_LINKSYS2_USB200M	0x2226		/* USB 2.0 10/100 ethernet controller */
#define	USB_PRODUCT_LINKSYS3_WUSB11V28	0x2233		/* WUSB11-V28 */
#define	USB_PRODUCT_LINKSYS4_USB1000	0x0039		/* USB1000 */
#define	USB_PRODUCT_LINKSYS4_WUSB100	0x0070		/* WUSB100 */
#define	USB_PRODUCT_LINKSYS4_WUSB600N	0x0071		/* WUSB600N */
#define	USB_PRODUCT_LINKSYS4_WUSB54GC_2	0x0073		/* WUSB54GC v2 */
#define	USB_PRODUCT_LINKSYS4_WUSB54GC_3	0x0077		/* WUSB54GC v3 */
#define	USB_PRODUCT_LINKSYS4_RT3070	0x0078		/* RT3070 */
#define	USB_PRODUCT_LINKSYS4_WUSB600NV2	0x0079		/* WUSB600N v2 */

/* Lite-On Technology */
#define	USB_PRODUCT_LITEON_AR9271	0x4605		/* AR9271 */

/* Logitec products */
#define	USB_PRODUCT_LOGITEC_LDR_H443SU2	0x0033		/* DVD Multi-plus unit LDR-H443SU2 */
#define	USB_PRODUCT_LOGITEC_LDR_H443U2	0x00b3		/* DVD Multi-plus unit LDR-H443U2 */
#define	USB_PRODUCT_LOGITEC_LAN_GTJU2	0x0102		/* LAN-GTJ/U2 */
#define	USB_PRODUCT_LOGITEC_LANTX	0x0105		/* LAN-TX */
#define	USB_PRODUCT_LOGITEC_RTL8187	0x010c		/* RTL8187 */
#define	USB_PRODUCT_LOGITEC_RT2870_1	0x0162		/* RT2870 */
#define	USB_PRODUCT_LOGITEC_RT2870_2	0x0163		/* RT2870 */
#define	USB_PRODUCT_LOGITEC_RT2870_3	0x0164		/* RT2870 */
#define	USB_PRODUCT_LOGITEC_LANW300NU2	0x0166		/* LAN-W300N/U2 */
#define	USB_PRODUCT_LOGITEC_RT3020	0x0168		/* RT3020 */
#define	USB_PRODUCT_LOGITEC_LANW300NU2S	0x0169		/* LAN-W300N/U2S */
#define	USB_PRODUCT_LOGITEC_LAN_W450ANU2E	0x016b		/* LAN-W450ANU2E */
#define	USB_PRODUCT_LOGITEC_LAN_W300ANU2	0x0170		/* LAN-W300AN/U2 */

/* Logitech products */
#define	USB_PRODUCT_LOGITECH_M2452	0x0203		/* M2452 keyboard */
#define	USB_PRODUCT_LOGITECH_M4848	0x0301		/* M4848 mouse */
#define	USB_PRODUCT_LOGITECH_PAGESCAN	0x040f		/* PageScan */
#define	USB_PRODUCT_LOGITECH_QUICKCAMWEB	0x0801		/* QuickCam Web */
#define	USB_PRODUCT_LOGITECH_QUICKCAMPRO	0x0810		/* QuickCam Pro */
#define	USB_PRODUCT_LOGITECH_QUICKCAMEXP	0x0840		/* QuickCam Express */
#define	USB_PRODUCT_LOGITECH_QUICKCAM	0x0850		/* QuickCam */
#define	USB_PRODUCT_LOGITECH_QUICKCAMEXP2	0x0870		/* QuickCam Express */
#define	USB_PRODUCT_LOGITECH_QUICKCAMPRO3k	0x08b0		/* QuickCam Pro 3000 */
#define	USB_PRODUCT_LOGITECH_QUICKCAMPRONB	0x08b1		/* QuickCam for Notebook Pro */
#define	USB_PRODUCT_LOGITECH_QUICKCAMPRO4K	0x08b2		/* QuickCam Pro 4000 */
#define	USB_PRODUCT_LOGITECH_QUICKCAMMESS	0x08f0		/* QuickCam Messenger */
#define	USB_PRODUCT_LOGITECH_N43	0xc000		/* N43 */
#define	USB_PRODUCT_LOGITECH_N48	0xc001		/* N48 mouse */
#define	USB_PRODUCT_LOGITECH_MBA47	0xc002		/* M-BA47 mouse */
#define	USB_PRODUCT_LOGITECH_WMMOUSE	0xc004		/* WingMan Gaming Mouse */
#define	USB_PRODUCT_LOGITECH_BD58	0xc00c		/* BD58 mouse */
#define	USB_PRODUCT_LOGITECH_USBPS2MOUSE	0xc00e		/* USB-PS/2 Optical Mouse */
#define	USB_PRODUCT_LOGITECH_MUV55A	0xc016		/* M-UV55a */
#define	USB_PRODUCT_LOGITECH_UN58A	0xc030		/* iFeel Mouse */
#define	USB_PRODUCT_LOGITECH_WMPAD	0xc208		/* WingMan GamePad Extreme */
#define	USB_PRODUCT_LOGITECH_WMRPAD	0xc20a		/* WingMan RumblePad */
#define	USB_PRODUCT_LOGITECH_WMJOY	0xc281		/* WingMan Force joystick */
#define	USB_PRODUCT_LOGITECH_WMFFGP	0xc293		/* WingMan Formula Force GP (GT-Force) */
#define	USB_PRODUCT_LOGITECH_BB13	0xc401		/* USB-PS/2 Trackball */
#define	USB_PRODUCT_LOGITECH_BB18	0xc404		/* TrackMan Wheel */
#define	USB_PRODUCT_LOGITECH_MARBLEMOUSE	0xc408		/* Marble Mouse */
#define	USB_PRODUCT_LOGITECH_RK53	0xc501		/* Cordless mouse */
#define	USB_PRODUCT_LOGITECH_RB6	0xc503		/* Cordless keyboard */
#define	USB_PRODUCT_LOGITECH_CDO	0xc504		/* Cordless Desktop Optical */
#define	USB_PRODUCT_LOGITECH_MX700	0xc506		/* Cordless optical mouse */
#define	USB_PRODUCT_LOGITECH_CBT44	0xc517		/* C-BT44 Receiver */
#define	USB_PRODUCT_LOGITECH_QUICKCAMPRO2	0xd001		/* QuickCam Pro */

/* Lucent products */
#define	USB_PRODUCT_LUCENT_EVALKIT	0x1001		/* USS-720 evaluation kit */

/* Luwen products */
#define	USB_PRODUCT_LUWEN_EASYDISK	0x0005		/* EasyDisc */

/* Macally products */
#define	USB_PRODUCT_MACALLY_MOUSE1	0x0101		/* mouse */

/* Mad Catz, Inc. */
#define	USB_PRODUCT_MADCATZ_CYBORG_RAT7	0x1708		/* Cyborg R.A.T. 7 */

/* MCT Corp. products */
#define	USB_PRODUCT_MCT_HUB0100	0x0100		/* Hub */
#define	USB_PRODUCT_MCT_DU_H3SP_USB232	0x0200		/* D-Link DU-H3SP USB BAY Hub */
#define	USB_PRODUCT_MCT_USB232	0x0210		/* USB-232 Interface */
#define	USB_PRODUCT_MCT_SITECOM_USB232	0x0230		/* Sitecom USB-232 Products */
#define	USB_PRODUCT_MCT_ML_4500	0x0302		/* ML-4500 */

/* MediaGear products */
#define	USB_PRODUCT_MEDIAGEAR_READER9IN1	0x5003		/* USB2.0 9 in 1 Reader */

/* Meinberg Funkuhren products */
#define	USB_PRODUCT_MEINBERG_USB5131	0x0301		/* USB 5131 DCF77 - Radio Clock */

/* Meizo Electronics */
#define	USB_PRODUCT_MEIZU_M6_SL	0x0140		/* MiniPlayer M6 (SL) */

/* Melco, Inc products */
#define	USB_PRODUCT_MELCO_LUATX1	0x0001		/* LUA-TX Ethernet */
#define	USB_PRODUCT_MELCO_LUATX5	0x0005		/* LUA-TX Ethernet */
#define	USB_PRODUCT_MELCO_LUA2TX5	0x0009		/* LUA2-TX Ethernet */
#define	USB_PRODUCT_MELCO_LUAKTX	0x0012		/* LUA-KTX Ethernet */
#define	USB_PRODUCT_MELCO_S11	0x0016		/* WLI-USB-S11 */
#define	USB_PRODUCT_MELCO_MCRSM2	0x001b		/* MCR-SM2 SmartMedia Card Reader/Writer */
#define	USB_PRODUCT_MELCO_DUBPXXG	0x001c		/* USB-IDE Bridge: DUB-PxxG */
#define	USB_PRODUCT_MELCO_KS11G	0x0027		/* WLI-USB-KS11G USB-wlan */
#define	USB_PRODUCT_MELCO_LUAU2KTX	0x003d		/* LUA-U2-KTX Ethernet */
#define	USB_PRODUCT_MELCO_KB11	0x0044		/* WLI-USB-KB11 WLAN */
#define	USB_PRODUCT_MELCO_KG54YB	0x005e		/* WLI-U2-KG54-YB WLAN */
#define	USB_PRODUCT_MELCO_KG54	0x0066		/* WLI-U2-KG54 WLAN */
#define	USB_PRODUCT_MELCO_KG54AI	0x0067		/* WLI-U2-KG54-AI WLAN */
#define	USB_PRODUCT_MELCO_LUAU2GT	0x006e		/* LUA-U2-GT Ethernet */
#define	USB_PRODUCT_MELCO_NINWIFI	0x008b		/* Nintendo Wi-Fi */
#define	USB_PRODUCT_MELCO_PCOPRS1	0x00b3		/* RemoteStation PC-OP-RS1 */
#define	USB_PRODUCT_MELCO_SG54HP	0x00d8		/* WLI-U2-SG54HP */
#define	USB_PRODUCT_MELCO_G54HP	0x00d9		/* WLI-U2-G54HP */
#define	USB_PRODUCT_MELCO_KG54L	0x00da		/* WLI-U2-KG54L */
#define	USB_PRODUCT_MELCO_WLIUCG300N	0x00e8		/* WLI-UC-G300N */
#define	USB_PRODUCT_MELCO_SG54HG	0x00f4		/* WLI-U2-SG54HG */
#define	USB_PRODUCT_MELCO_WLIUCAG300N	0x012e		/* WLI-UC-AG300N */
#define	USB_PRODUCT_MELCO_WLIUCG	0x0137		/* WLI-UC-G */
#define	USB_PRODUCT_MELCO_RT2870_1	0x0148		/* RT2870 */
#define	USB_PRODUCT_MELCO_RT2870_2	0x0150		/* RT2870 */
#define	USB_PRODUCT_MELCO_WLIUCGNHP	0x0158		/* WLI-UC-GNHP */
#define	USB_PRODUCT_MELCO_WLIUCGN	0x015d		/* WLI-UC-GN */
#define	USB_PRODUCT_MELCO_WLIUCG301N	0x016f		/* WLI-UC-G301N */
#define	USB_PRODUCT_MELCO_WLIUCGNM	0x01a2		/* WLI-UC-GNM */
#define	USB_PRODUCT_MELCO_WLIUCGNM2T	0x01ee		/* WLI-UC-GNM2T */

/* Merlin products */
#define	USB_PRODUCT_MERLIN_V620	0x1110		/* Merlin V620 */

/* Metricom products */
#define	USB_PRODUCT_METRICOM_RICOCHET_GS	0x0001		/* Ricochet GS */

/* MGE UPS Systems */
#define	USB_PRODUCT_MGE_UPS1	0x0001		/* MGE UPS SYSTEMS PROTECTIONCENTER 1 */
#define	USB_PRODUCT_MGE_UPS2	0xffff		/* MGE UPS SYSTEMS PROTECTIONCENTER 2 */

/* Micro Star International products */
#define	USB_PRODUCT_MSI_WLAN	0x1020		/* WLAN */
#define	USB_PRODUCT_MSI_BLUETOOTH	0x1967		/* Bluetooth USB Adapter */
#define	USB_PRODUCT_MSI_RT3070	0x3820		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_2	0x3821		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_8	0x3822		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_3	0x3870		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_9	0x3871		/* RT3070 */
#define	USB_PRODUCT_MSI_MEGASKY580	0x5580		/* MSI MegaSky DVB-T Adapter */
#define	USB_PRODUCT_MSI_MEGASKY580_55801	0x5581		/* MSI MegaSky DVB-T Adapter */
#define	USB_PRODUCT_MSI_MS6861	0x6861		/* MS-6861 */
#define	USB_PRODUCT_MSI_MS6865	0x6865		/* MS-6865 */
#define	USB_PRODUCT_MSI_MS6869	0x6869		/* MS-6869 */
#define	USB_PRODUCT_MSI_RT2573	0x6874		/* RT2573 */
#define	USB_PRODUCT_MSI_RT2573_2	0x6877		/* RT2573 */
#define	USB_PRODUCT_MSI_RT3070_4	0x6899		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_5	0x821a		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_10	0x822a		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_12	0x822b		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_13	0x822c		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_6	0x870a		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_11	0x871a		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_14	0x871b		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_15	0x871c		/* RT3070 */
#define	USB_PRODUCT_MSI_RT3070_7	0x899a		/* RT3070 */
#define	USB_PRODUCT_MSI_RT2573_3	0xa861		/* RT2573 */
#define	USB_PRODUCT_MSI_RT2573_4	0xa874		/* RT2573 */
#define	USB_PRODUCT_MSI_AX88772A	0xa877		/* AX88772A USB 2.0 10/100 Ethernet adapter */
#define	USB_PRODUCT_MSI_BLUETOOTH_2	0xa970		/* Bluetooth */
#define	USB_PRODUCT_MSI_BLUETOOTH_3	0xa97a		/* Bluetooth */

/* Microchip Technology products */
#define	USB_PRODUCT_MICROCHIP_PICKIT1	0x0032		/* PICkit(TM) 1 FLASH Starter Kit */
#define	USB_PRODUCT_MICROCHIP_PICKIT2	0x0033		/* PICkit 2 Microcontroller Programmer */

/* Microdia / Sonix Techonology Co., Ltd. products */
#define	USB_PRODUCT_MICRODIA_YUREX	0x1010		/* YUREX */

/* Micronet Communications products */
#define	USB_PRODUCT_MICRONET_SP128AR	0x0003		/* SP128AR EtherFast */

/* Microsoft products */
#define	USB_PRODUCT_MICROSOFT_SIDEPREC	0x0008		/* SideWinder Precision Pro */
#define	USB_PRODUCT_MICROSOFT_INTELLIMOUSE	0x0009		/* IntelliMouse */
#define	USB_PRODUCT_MICROSOFT_NATURALKBD	0x000b		/* Natural Keyboard Elite */
#define	USB_PRODUCT_MICROSOFT_DDS80	0x0014		/* Digital Sound System 80 */
#define	USB_PRODUCT_MICROSOFT_SIDEWINDER	0x001a		/* Sidewinder Precision Racing Wheel */
#define	USB_PRODUCT_MICROSOFT_INETPRO	0x001c		/* Internet Keyboard Pro */
#define	USB_PRODUCT_MICROSOFT_TBEXPLORER	0x0024		/* Trackball Explorer */
#define	USB_PRODUCT_MICROSOFT_INTELLIEYE	0x0025		/* IntelliEye mouse */
#define	USB_PRODUCT_MICROSOFT_INETPRO2	0x002b		/* Internet Keyboard Pro */
#define	USB_PRODUCT_MICROSOFT_MN510	0x006e		/* MN510 Wireless */
#define	USB_PRODUCT_MICROSOFT_MN110	0x007a		/* 10/100 USB NIC */
#define	USB_PRODUCT_MICROSOFT_XBOX_MEMORY_UNIT	0x0280		/* Xbox Memory Unit */
#define	USB_PRODUCT_MICROSOFT_XBOX_DVD_PLAYBACK	0x0284		/* Xbox DVD Movie Playback Kit */
#define	USB_PRODUCT_MICROSOFT_XBOX_CONTROLLER_S10	0x0285		/* Xbox Controller S (1.0) */
#define	USB_PRODUCT_MICROSOFT_XBOX_CONTROLLER_HUB	0x0288		/* Xbox Controller Hub */
#define	USB_PRODUCT_MICROSOFT_XBOX_CONTROLLER_S12	0x0289		/* Xbox Controller S (1.2) */
#define	USB_PRODUCT_MICROSOFT_XBOX_360_WIRELESS_RECEIVER	0x0291		/* Xbox 360 Wireless Receiver */
#define	USB_PRODUCT_MICROSOFT_24GHZ_XCVR10	0x071d		/* 2.4GHz Transceiver V1.0 */
#define	USB_PRODUCT_MICROSOFT_24GHZ_XCVR20	0x071f		/* 2.4GHz Transceiver V2.0 */

/* Microtech products */
#define	USB_PRODUCT_MICROTECH_SCSIDB25	0x0004		/* USB-SCSI-DB25 */
#define	USB_PRODUCT_MICROTECH_SCSIHD50	0x0005		/* USB-SCSI-HD50 */
#define	USB_PRODUCT_MICROTECH_DPCM	0x0006		/* USB CameraMate */
#define	USB_PRODUCT_MICROTECH_FREECOM	0xfc01		/* Freecom USB-IDE */

/* Microtek products */
#define	USB_PRODUCT_MICROTEK_336CX	0x0094		/* Phantom 336CX - C3 scanner */
#define	USB_PRODUCT_MICROTEK_X6U	0x0099		/* ScanMaker X6 - X6U */
#define	USB_PRODUCT_MICROTEK_C6	0x009a		/* Phantom C6 scanner */
#define	USB_PRODUCT_MICROTEK_336CX2	0x00a0		/* Phantom 336CX - C3 scanner */
#define	USB_PRODUCT_MICROTEK_V6USL	0x00a3		/* ScanMaker V6USL */
#define	USB_PRODUCT_MICROTEK_6000	0x30e5		/* ScanMaker 6000 */
#define	USB_PRODUCT_MICROTEK_V6USL2	0x80a3		/* ScanMaker V6USL */
#define	USB_PRODUCT_MICROTEK_V6UL	0x80ac		/* ScanMaker V6UL */

/* Midiman products */
#define	USB_PRODUCT_MIDIMAN_MIDISPORT2X2	0x1001		/* Midisport 2x2 */
#define	USB_PRODUCT_MIDIMAN_MIDISPORT2X4	0x1041		/* Midisport 2x4 */

/* Minds At Work LLC products */
#define	USB_PRODUCT_MINDSATWORK_DW	0x0001		/* Digital Wallet */

/* Minolta Co., Ltd. */
#define	USB_PRODUCT_MINOLTA_2300	0x4001		/* Dimage 2300 */
#define	USB_PRODUCT_MINOLTA_S304	0x4007		/* Dimage S304 */
#define	USB_PRODUCT_MINOLTA_X	0x4009		/* Dimage X */
#define	USB_PRODUCT_MINOLTA_DIMAGE7I	0x400b		/* Dimage 7i */
#define	USB_PRODUCT_MINOLTA_5400	0x400e		/* Dimage 5400 */
#define	USB_PRODUCT_MINOLTA_DIMAGEA1	0x401a		/* Dimage A1 */
#define	USB_PRODUCT_MINOLTA_XT	0x4015		/* Dimage Xt */

/* Misc Vendors (sharing a Vendor ID) */
#define	USB_PRODUCT_MISC_WISPY_24X	0x083f		/* MetaGeek Wi-Spy 2.4x */
#define	USB_PRODUCT_MISC_TELLSTICK	0x0c30		/* Telldus Tellstick */
#define	USB_PRODUCT_MISC_TELLSTICK_DUO	0x0c31		/* Telldus Tellstick Duo */

/* Mitsumi products */
#define	USB_PRODUCT_MITSUMI_CDRRW	0x0000		/* CD-R/RW Drive */
#define	USB_PRODUCT_MITSUMI_MOUSE	0x6407		/* Mouse */
#define	USB_PRODUCT_MITSUMI_SONY_MOUSE	0x6408		/* Quick Scroll Mouse */
#define	USB_PRODUCT_MITSUMI_BT_DONGLE	0x641f		/* Bluetooth USB dongle */
#define	USB_PRODUCT_MITSUMI_FDD	0x6901		/* FDD */

/* Mobility products */
#define	USB_PRODUCT_MOBILITY_EA	0x0204		/* Ethernet Adapter */
#define	USB_PRODUCT_MOBILITY_EASIDOCK	0x0304		/* EasiDock Ethernet */

/* MosChip Semiconductor */
#define	USB_PRODUCT_MOSCHIP_MCS7703	0x7703		/* MCS7703 USB Serial Adapter */
#define	USB_PRODUCT_MOSCHIP_MCS7780	0x7780		/* Fast IrDA Adapter */
#define	USB_PRODUCT_MOSCHIP_MCS7781	0x7781		/* Fast IrDA Adapter */
#define	USB_PRODUCT_MOSCHIP_MCS7784	0x7784		/* Slow IrDA Adapter */
#define	USB_PRODUCT_MOSCHIP_MCS7810	0x7810		/* MCS7810 USB Serial Adapter */
#define	USB_PRODUCT_MOSCHIP_MCS7820	0x7820		/* MCS7820 USB Serial Adapter */
#define	USB_PRODUCT_MOSCHIP_MCS7830	0x7830		/* Ethernet Adapter */
#define	USB_PRODUCT_MOSCHIP_MCS7840	0x7840		/* MCS7840 USB Serial Adapter */

/* Motorola products */
#define	USB_PRODUCT_MOTOROLA_MC141555	0x1555		/* MC141555 hub controller */
#define	USB_PRODUCT_MOTOROLA_SB4100	0x4100		/* SB4100 USB Cable Modem */
#define	USB_PRODUCT_MOTOROLA2_T720C	0x2822		/* T720c */
#define	USB_PRODUCT_MOTOROLA2_A920	0x4002		/* A920 */
#define	USB_PRODUCT_MOTOROLA2_USBLAN	0x600c		/* USBLAN (A780, E680, ...) */
#define	USB_PRODUCT_MOTOROLA2_USBLAN2	0x6027		/* USBLAN (A910, A1200, Rokr E2, Rokr E6, ...) */
#define	USB_PRODUCT_MOTOROLA4_RT2770	0x9031		/* RT2770 */
#define	USB_PRODUCT_MOTOROLA4_RT3070	0x9032		/* RT3070 */

/* M-Systems products */
#define	USB_PRODUCT_MSYSTEMS_DISKONKEY	0x0010		/* DiskOnKey */
#define	USB_PRODUCT_MSYSTEMS_DISKONKEY2	0x0011		/* DiskOnKey */
#define	USB_PRODUCT_MSYSTEMS_DISKONKEY3	0x0012		/* DiskOnKey */

/* MultiTech products */
#define	USB_PRODUCT_MULTITECH_ATLAS	0xf101		/* MT5634ZBA-USB modem */

/* Mustek products */
#define	USB_PRODUCT_MUSTEK_1200CU	0x0001		/* 1200 CU scanner */
#define	USB_PRODUCT_MUSTEK_600CU	0x0002		/* 600 CU scanner */
#define	USB_PRODUCT_MUSTEK_1200USB	0x0003		/* 1200 USB scanner */
#define	USB_PRODUCT_MUSTEK_1200UB	0x0006		/* 1200 UB scanner */
#define	USB_PRODUCT_MUSTEK_1200USBPLUS	0x0007		/* 1200 USB Plus scanner */
#define	USB_PRODUCT_MUSTEK_1200CUPLUS	0x0008		/* 1200 CU Plus scanner */
#define	USB_PRODUCT_MUSTEK_BEARPAW1200F	0x0010		/* BearPaw 1200F scanner */
#define	USB_PRODUCT_MUSTEK_BEARPAW1200TA	0x021e		/* BearPaw 1200TA scanner */
#define	USB_PRODUCT_MUSTEK_600USB	0x0873		/* 600 USB scanner */
#define	USB_PRODUCT_MUSTEK_MDC800	0xa800		/* MDC-800 digital camera */
#define	USB_PRODUCT_MUSTEK_DV2000	0xc441		/* DV2000 digital camera */

/* National Instruments */
#define	USB_PRODUCT_NI_GPIB_USB_A	0xc920		/* GPIB-USB-A */

/* National Semiconductor */
#define	USB_PRODUCT_NATIONAL_BEARPAW1200	0x1000		/* BearPaw 1200 */
#define	USB_PRODUCT_NATIONAL_BEARPAW2400	0x1001		/* BearPaw 2400 */

/* NEC products */
#define	USB_PRODUCT_NEC_HUB_20	0x0059		/* 2.0 hub */
#define	USB_PRODUCT_NEC_WL300NUG	0x0249		/* WL300NU-G */
#define	USB_PRODUCT_NEC_HUB	0x55aa		/* hub */
#define	USB_PRODUCT_NEC_HUB_B	0x55ab		/* hub */
#define	USB_PRODUCT_NEC_PICTY760	0xbef4		/* Picty760 */
#define	USB_PRODUCT_NEC_PICTY900	0xefbe		/* Picty900 */
#define	USB_PRODUCT_NEC_PICTY920	0xf0be		/* Picty920 */
#define	USB_PRODUCT_NEC_PICTY800	0xf1be		/* Picty800 */

/* NEC2 products */
#define	USB_PRODUCT_NEC2_HUB2_0	0x0058		/* USB2.0 Hub Controller */

/* NEODIO products */
#define	USB_PRODUCT_NEODIO_ND3050	0x3050		/* 6-in-1 Flash Device Controller */
#define	USB_PRODUCT_NEODIO_ND5010	0x5010		/* Multi-format Flash Controller */

/* NetChip Technology Products */
#define	USB_PRODUCT_NETCHIP_TURBOCONNECT	0x1080		/* Turbo-Connect */
#define	USB_PRODUCT_NETCHIP_CLIK40	0xa140		/* Clik! 40 */
#define	USB_PRODUCT_NETCHIP_ETHERNETGADGET	0xa4a2		/* Linux Ethernet/RNDIS gadget on pxa210/25x/26x */

/* Netgear products */
#define	USB_PRODUCT_NETGEAR_EA101	0x1001		/* Ethernet adapter */
#define	USB_PRODUCT_NETGEAR_EA101X	0x1002		/* Ethernet adapter */
#define	USB_PRODUCT_NETGEAR_FA101	0x1020		/* 10/100 Ethernet */
#define	USB_PRODUCT_NETGEAR_FA120	0x1040		/* USB 2.0 Fast Ethernet Adapter */
#define	USB_PRODUCT_NETGEAR_MA111NA	0x4110		/* 802.11b Adapter */
#define	USB_PRODUCT_NETGEAR_MA111V2	0x4230		/* 802.11b V2 */
#define	USB_PRODUCT_NETGEAR_WG111V2_2	0x4240		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_NETGEAR_WG111V3	0x4260		/* WG111v3 */
#define	USB_PRODUCT_NETGEAR_WG111U	0x4300		/* WG111U */
#define	USB_PRODUCT_NETGEAR_WG111U_NF	0x4301		/* WG111U */
#define	USB_PRODUCT_NETGEAR_WG111V2	0x6a00		/* WG111v2 */
#define	USB_PRODUCT_NETGEAR_XA601	0x8100		/* USB to PL Adapter */
#define	USB_PRODUCT_NETGEAR_WN111V2	0x9001		/* WN111V2 */
#define	USB_PRODUCT_NETGEAR_WNDA3100	0x9010		/* WNDA3100 */
#define	USB_PRODUCT_NETGEAR_WNDA3200	0x9018		/* WNDA3200 */
#define	USB_PRODUCT_NETGEAR_RTL8192CU	0x9021		/* RTL8192CU */
#define	USB_PRODUCT_NETGEAR_WNA1100	0x9030		/* WNA1100 */
#define	USB_PRODUCT_NETGEAR_WNA1000	0x9040		/* WNA1000 */
#define	USB_PRODUCT_NETGEAR_WNA1000M	0x9041		/* WNA1000M */

/* Netgear(2) products */
#define	USB_PRODUCT_NETGEAR2_MA101	0x4100		/* MA101 */
#define	USB_PRODUCT_NETGEAR2_MA101B	0x4102		/* MA101 Rev B */

/* Netgear(3) products */
#define	USB_PRODUCT_NETGEAR3_WG111T	0x4250		/* WG111T */
#define	USB_PRODUCT_NETGEAR3_WG111T_NF	0x4251		/* WG111T */
#define	USB_PRODUCT_NETGEAR3_WPN111	0x5f00		/* WPN111 */
#define	USB_PRODUCT_NETGEAR3_WPN111_NF	0x5f01		/* WPN111 */

/* Netgear(4) products */
#define	USB_PRODUCT_NETGEAR4_RTL8188CU	0x9041		/* RTL8188CU */

/* NetIndex products */
#define	USB_PRODUCT_NETINDEX_WS002IN	0x2001		/* Willcom WS002IN (DD) */

/* NHJ product */
#define	USB_PRODUCT_NHJ_CAM2	0x9120		/* Camera */

/* Nikon products */
#define	USB_PRODUCT_NIKON_E990	0x0102		/* Digital Camera E990 */
#define	USB_PRODUCT_NIKON_E880	0x0103		/* Digital Camera E880 */
#define	USB_PRODUCT_NIKON_E885	0x0105		/* Digital Camera E885 */

/* Nintendo products */
#define	USB_PRODUCT_NINTENDO_BCM2045A	0x0305		/* Broadcom BCM2045A Bluetooth Radio */
#define	USB_PRODUCT_NINTENDO_RVLCNT01	0x0306		/* Wii Remote RVL-CNT-01 (BT-HID) */
/* product NINTENDO WIIMIC1	0x0308	Wii Party-Mic */
/* product NINTENDO WIIMIC2	0x0309	USB Microphone for Wii */
#define	USB_PRODUCT_NINTENDO_RVLCNT01TR	0x0330		/* Wii Remote Plus RVL-CNT-01-TR (BT-HID) */

/* Nokia products */
#define	USB_PRODUCT_NOKIA_CA42	0x0802		/* Mobile Phone adapter */

/* Nova Tech products */
#define	USB_PRODUCT_NOVATECH_NV902W	0x9020		/* NV-902W */
#define	USB_PRODUCT_NOVATECH_RT2573	0x9021		/* RT2573 */
#define	USB_PRODUCT_NOVATECH_RTL8188CU	0x9071		/* RTL8188CU */

/* NovAtel products */
#define	USB_PRODUCT_NOVATEL_FLEXPACKGPS	0x0100		/* NovAtel FlexPack GPS receiver */
#define	USB_PRODUCT_NOVATEL2_EXPRESSCARD	0x1100		/* ExpressCard 3G */
#define	USB_PRODUCT_NOVATEL2_MERLINV620	0x1110		/* Novatel Wireless Merlin CDMA */
#define	USB_PRODUCT_NOVATEL2_V740	0x1120		/* Merlin V740 */
#define	USB_PRODUCT_NOVATEL2_S720	0x1130		/* S720 */
#define	USB_PRODUCT_NOVATEL2_MERLINU740	0x1400		/* Novatel Merlin U740 */
#define	USB_PRODUCT_NOVATEL2_U740_2	0x1410		/* Merlin U740 */
#define	USB_PRODUCT_NOVATEL2_U870	0x1420		/* Merlin U870 */
#define	USB_PRODUCT_NOVATEL2_XU870	0x1430		/* Merlin XU870 */
#define	USB_PRODUCT_NOVATEL2_X950D	0x1450		/* Merlin X950D */
#define	USB_PRODUCT_NOVATEL2_ES620	0x2100		/* ES620 CDMA */
#define	USB_PRODUCT_NOVATEL2_U720	0x2110		/* U720 */
#define	USB_PRODUCT_NOVATEL2_EU8X0D	0x2420		/* Expedite EU850D/EU860D/EU870D */
#define	USB_PRODUCT_NOVATEL2_U727	0x4100		/* U727 */
#define	USB_PRODUCT_NOVATEL2_MC950D	0x4400		/* Novatel Wireless HSUPA Modem */
#define	USB_PRODUCT_NOVATEL2_MC950D_DRIVER	0x5010		/* Novatel Wireless HSUPA Modem Windows Driver */
#define	USB_PRODUCT_NOVATEL2_U760_DRIVER	0x5030		/* Novatel Wireless U760 Windows/Mac Driver */
#define	USB_PRODUCT_NOVATEL2_U760	0x6000		/* Novatel 760USB */

/* Olympus products */
#define	USB_PRODUCT_OLYMPUS_C1	0x0102		/* C-1 Digital Camera */
#define	USB_PRODUCT_OLYMPUS_C700	0x0105		/* C-700 Ultra Zoom */

/* OmniVision Technologies, Inc. products */
#define	USB_PRODUCT_OMNIVISION_OV511	0x0511		/* OV511 Camera */
#define	USB_PRODUCT_OMNIVISION_OV511PLUS	0xa511		/* OV511+ Camera */
#define	USB_PRODUCT_OMNIVISION2_PSEYE	0x2000		/* Sony PLAYSTATION(R) Eye */

/* OnSpec Electronic, Inc. */
#define	USB_PRODUCT_ONSPEC_MD2	0x0103		/* disk */
#define	USB_PRODUCT_ONSPEC_MDCFEB	0xa000		/* MDCFE-B USB CF Reader */
#define	USB_PRODUCT_ONSPEC_SIIGMS	0xa001		/* Memory Stick+CF Reader/Writer */
#define	USB_PRODUCT_ONSPEC_DATAFAB3	0xa003		/* Datafab-based Reader */
#define	USB_PRODUCT_ONSPEC_DATAFAB4	0xa004		/* Datafab-based Reader */
#define	USB_PRODUCT_ONSPEC_PNYCFSM	0xa005		/* PNY/Datafab CF+SM Reader */
#define	USB_PRODUCT_ONSPEC_STECHCFSM	0xa006		/* Simple Tech/Datafab CF+SM Reader */
#define	USB_PRODUCT_ONSPEC_LC1	0xa109		/* CF + SM Combo (LC1) */
#define	USB_PRODUCT_ONSPEC_UCF100	0xa400		/* FlashLink UCF-100 CompactFlash Reader */
#define	USB_PRODUCT_ONSPEC_MD1II	0xb006		/* Datafab MD1-II PC-Card Reader */

#define	USB_PRODUCT_ONSPEC2_8IN2	0xb012		/* 8In2 */

/* Option N.V. products */
#define	USB_PRODUCT_OPTIONNV_MC3G	0x5000		/* Vodafone Mobile Connect 3G datacard */
#define	USB_PRODUCT_OPTIONNV_QUADUMTS2	0x6000		/* GlobeTrotter Fusion Quad Lite UMTS/GPRS */
#define	USB_PRODUCT_OPTIONNV_QUADUMTS	0x6300		/* GlobeTrotter Fusion Quad Lite 3D */
#define	USB_PRODUCT_OPTIONNV_QUADPLUSUMTS	0x6600		/* GlobeTrotter 3G Quad Plus */
#define	USB_PRODUCT_OPTIONNV_HSDPA	0x6701		/* GlobeTrotter HSDPA Modem */
#define	USB_PRODUCT_OPTIONNV_MAXHSDPA	0x6701		/* GlobeTrotter Max HSDPA Modem */
#define	USB_PRODUCT_OPTIONNV_GSICON72	0x6911		/* GlobeSurfer iCON 7.2 */
#define	USB_PRODUCT_OPTIONNV_ICON225	0x6971		/* iCON 225 */
#define	USB_PRODUCT_OPTIONNV_GTMAXHSUPA	0x7001		/* GlobeTrotter HSUPA */
#define	USB_PRODUCT_OPTIONNV_GEHSUPA	0x7011		/* GlobeTrotter Express HSUPA */
#define	USB_PRODUCT_OPTIONNV_GTHSUPA	0x7031		/* GlobeTrotter HSUPA */
#define	USB_PRODUCT_OPTIONNV_GSHSUPA	0x7251		/* GlobeSurfer HSUPA */
#define	USB_PRODUCT_OPTIONNV_GE40X1	0x7301		/* GE40x */
#define	USB_PRODUCT_OPTIONNV_GE40X2	0x7361		/* GE40x */
#define	USB_PRODUCT_OPTIONNV_GE40X3	0x7381		/* GE40x */
#define	USB_PRODUCT_OPTIONNV_ICON401	0x7401		/* iCON 401 */
#define	USB_PRODUCT_OPTIONNV_GTM382	0x7501		/* GTM 382 */
#define	USB_PRODUCT_OPTIONNV_GE40X4	0x7601		/* GE40x */
#define	USB_PRODUCT_OPTIONNV_GTHSUPAM	0x8300		/* Globetrotter HSUPA Modem */
#define	USB_PRODUCT_OPTIONNV_ICONEDGE	0xc031		/* iCON EDGE */
#define	USB_PRODUCT_OPTIONNV_MODHSXPA	0xd013		/* Module HSxPA */
#define	USB_PRODUCT_OPTIONNV_ICON321	0xd031		/* iCON 321 */
#define	USB_PRODUCT_OPTIONNV_ICON322	0xd033		/* iCON 322 */
#define	USB_PRODUCT_OPTIONNV_ICON505	0xd055		/* iCON 505 */

/* OQO */
#define	USB_PRODUCT_OQO_WIFI01	0x0002		/* model 01 WiFi interface */
#define	USB_PRODUCT_OQO_ETHER01PLUS	0x7720		/* model 01+ Ethernet */
#define	USB_PRODUCT_OQO_ETHER01	0x8150		/* model 01 Ethernet interface */

/* Ours Technology Inc. */
#define	USB_PRODUCT_OTI_SOLID	0x6803		/* Solid state disk */
#define	USB_PRODUCT_OTI_FLASHDISK	0x6828		/* Flash Disk 128M */

/* OvisLink product */
#define	USB_PRODUCT_OVISLINK_RT3071	0x3071		/* RT3071 */
#define	USB_PRODUCT_OVISLINK_RT3072	0x3072		/* RT3072 */

/* Palm Computing, Inc. product */
#define	USB_PRODUCT_PALM_SERIAL	0x0080		/* USB Serial Adaptor */
#define	USB_PRODUCT_PALM_M500	0x0001		/* Palm m500 */
#define	USB_PRODUCT_PALM_M505	0x0002		/* Palm m505 */
#define	USB_PRODUCT_PALM_M515	0x0003		/* Palm m515 */
#define	USB_PRODUCT_PALM_I705	0x0020		/* Palm i705 */
#define	USB_PRODUCT_PALM_TUNGSTEN_Z	0x0031		/* Palm Tungsten Z */
#define	USB_PRODUCT_PALM_M125	0x0040		/* Palm m125 */
#define	USB_PRODUCT_PALM_M130	0x0050		/* Palm m130 */
#define	USB_PRODUCT_PALM_TUNGSTEN_T	0x0060		/* Palm Tungsten T */
#define	USB_PRODUCT_PALM_ZIRE31	0x0061		/* Palm Zire 31 */
#define	USB_PRODUCT_PALM_ZIRE	0x0070		/* Palm Zire */

/* Panasonic products */
#define	USB_PRODUCT_PANASONIC_LS120	0x0901		/* LS-120 Camera */
#define	USB_PRODUCT_PANASONIC_KXLRW32AN	0x0d09		/* CD-R Drive KXL-RW32AN */
#define	USB_PRODUCT_PANASONIC_KXLCB20AN	0x0d0a		/* CD-R Drive KXL-CB20AN */
#define	USB_PRODUCT_PANASONIC_KXLCB35AN	0x0d0e		/* DVD-ROM & CD-R/RW */
#define	USB_PRODUCT_PANASONIC_SDCAAE	0x1b00		/* MultiMediaCard Adapter */
#define	USB_PRODUCT_PANASONIC_DMCFS45	0x2372		/* Lumix Camera DMC-FS45 */
#define	USB_PRODUCT_PANASONIC_TYTP50P6S	0x3900		/* TY-TP50P6-S 50in Touch Panel */

/* PARA Industrial products */
#define	USB_PRODUCT_PARA_RT3070	0x8888		/* RT3070 */

/* Pegatron products */
#define	USB_PRODUCT_PEGATRON_RT2870	0x0002		/* RT2870 */
#define	USB_PRODUCT_PEGATRON_RT3070	0x000c		/* RT3070 */
#define	USB_PRODUCT_PEGATRON_RT3070_2	0x000e		/* RT3070 */
#define	USB_PRODUCT_PEGATRON_RT3070_3	0x0010		/* RT3070 */
#define	USB_PRODUCT_PEGATRON_RT3072	0x0011		/* RT3072 */

/* Future Bits products */
#define	USB_PRODUCT_FUTUREBITS_4PI	0x6019		/* 4Pi reprap */

/* Pen Driver */
#define	USB_PRODUCT_PEN_USBDISKPRO	0x0120		/* USB Disk Pro */
#define	USB_PRODUCT_PEN_USBREADER	0x0240		/* USB 6 in 1 Card Reader/Writer */
#define	USB_PRODUCT_PEN_MOBILEDRIVE	0x0280		/* USB 3 in 1 Card Reader/Writer */
#define	USB_PRODUCT_PEN_USBDISK	0x0d7d		/* USB Disk */
#define	USB_PRODUCT_PEN_ATTACHE	0x1300		/* USB 2.0 Flash Drive */

/* Peracom products */
#define	USB_PRODUCT_PERACOM_SERIAL1	0x0001		/* Serial Converter */
#define	USB_PRODUCT_PERACOM_ENET	0x0002		/* Ethernet adapter */
#define	USB_PRODUCT_PERACOM_ENET3	0x0003		/* At Home Ethernet Adapter */
#define	USB_PRODUCT_PERACOM_ENET2	0x0005		/* Ethernet adapter */

/* Pheenet products */
#define	USB_PRODUCT_PHEENET_GWU513	0x4025		/* GWU513 */

/* Philips products */
#define	USB_PRODUCT_PHILIPS_DSS350	0x0101		/* DSS 350 Digital Speaker System */
#define	USB_PRODUCT_PHILIPS_DSS	0x0104		/* DSS XXX Digital Speaker System */
#define	USB_PRODUCT_PHILIPS_SA235	0x016a		/* SA235 */
#define	USB_PRODUCT_PHILIPS_HUB	0x0201		/* hub */
#define	USB_PRODUCT_PHILIPS_PCA645VC	0x0302		/* PCA645VC PC Camera */
#define	USB_PRODUCT_PHILIPS_PCA646VC	0x0303		/* PCA646VC PC Camera */
#define	USB_PRODUCT_PHILIPS_PCVC675K	0x0307		/* PCVC675K Vesta PC Camera */
#define	USB_PRODUCT_PHILIPS_PCVC680K	0x0308		/* PCVC680K Vesta Pro PC Camera */
#define	USB_PRODUCT_PHILIPS_PCVC690K	0x030c		/* PCVC690K Vesta Pro Scan PC Camera */
#define	USB_PRODUCT_PHILIPS_PCVC730K	0x0310		/* PCVC730K ToUCam Fun PC Camera */
#define	USB_PRODUCT_PHILIPS_PCVC740K	0x0311		/* PCVC740K ToUCam Pro PC Camera */
#define	USB_PRODUCT_PHILIPS_PCVC750K	0x0312		/* PCVC750K ToUCam Pro Scan PC Camera */
#define	USB_PRODUCT_PHILIPS_DSS150	0x0471		/* DSS 150 Digital Speaker System */
#define	USB_PRODUCT_PHILIPS_CPWUA054	0x1230		/* CPWUA054 */
#define	USB_PRODUCT_PHILIPS_SNU5600	0x1236		/* SNU5600 */
#define	USB_PRODUCT_PHILIPS_SNU5630NS05	0x1237		/* SNU5630NS/05 */
#define	USB_PRODUCT_PHILIPS_DIVAUSB	0x1801		/* DIVA USB mp3 player */
#define	USB_PRODUCT_PHILIPS_RT2870	0x200f		/* RT2870 */

/* Philips Semiconductor products */
#define	USB_PRODUCT_PHILIPSSEMI_HUB1122	0x1122		/* hub */

/* P.I. Engineering products */
#define	USB_PRODUCT_PIENGINEERING_PS2USB	0x020b		/* PS2 to Mac USB Adapter */
#define	USB_PRODUCT_PIENGINEERING_XKEYS58	0x0232		/* Xkeys Programmable Keyboard (58 Keys) */
#define	USB_PRODUCT_PIENGINEERING_XKEYS	0x0233		/* Xkeys Programmable Keyboard */

/* Pilotech Systems Co., Ltd products */
#define	USB_PRODUCT_PILOTECH_CRW600	0x0001		/* CRW-600 6-in-1 Reader */

/* Pinnacle Systems, Inc. products */
#define	USB_PRODUCT_PINNACLE_PCTV800E	0x0227		/* PCTV 800e */
#define	USB_PRODUCT_PINNACLE_PCTVDVBTFLASH	0x0228		/* Pinnacle PCTV DVB-T Flash */
#define	USB_PRODUCT_PINNACLE_PCTV72E	0x0236		/* Pinnacle PCTV 72e */
#define	USB_PRODUCT_PINNACLE_PCTV73E	0x0237		/* Pinnacle PCTV 73e */

/* Planex Communications products */
#define	USB_PRODUCT_PLANEX_GW_US11H	0x14ea		/* GW-US11H WLAN */
#define	USB_PRODUCT_PLANEX2_RTL8188CUS	0x1201		/* RTL8188CUS */
#define	USB_PRODUCT_PLANEX2_GW_US11S	0x3220		/* GW-US11S WLAN */
#define	USB_PRODUCT_PLANEX2_RTL8188CU_3	0x4902		/* RTL8188CU */
#define	USB_PRODUCT_PLANEX2_GWUSFANG300	0x4903		/* GW-USFang300 */
#define	USB_PRODUCT_PLANEX2_GWUS54GXS	0x5303		/* GW-US54GXS */
#define	USB_PRODUCT_PLANEX2_GW_US300	0x5304		/* GW-US300 */
#define	USB_PRODUCT_PLANEX2_GWUS54HP	0xab01		/* GW-US54HP */
#define	USB_PRODUCT_PLANEX2_GWUS300MINIS	0xab24		/* GW-US300MiniS */
#define	USB_PRODUCT_PLANEX2_RT3070	0xab25		/* RT3070 */
#define	USB_PRODUCT_PLANEX2_GWUSNANO	0xab28		/* GW-USNano */
#define	USB_PRODUCT_PLANEX2_GWUSMICRO300	0xab29		/* GW-USMicro300 */
#define	USB_PRODUCT_PLANEX2_RTL8188CU_1	0xab2a		/* GW-USNano2 */
#define	USB_PRODUCT_PLANEX2_RTL8192CU	0xab2b		/* GW-USEco300 */
#define	USB_PRODUCT_PLANEX2_RTL8188CU_4	0xab2e		/* RTL8188CU */
#define	USB_PRODUCT_PLANEX2_GW900D	0xab30		/* GW-900D */
#define	USB_PRODUCT_PLANEX2_GW450D	0xab31		/* GW-450D */
#define	USB_PRODUCT_PLANEX2_GW450S	0xab32		/* GW-450S */
#define	USB_PRODUCT_PLANEX2_GWUS54MINI2	0xab50		/* GW-US54Mini2 */
#define	USB_PRODUCT_PLANEX2_GWUS54SG	0xc002		/* GW-US54SG */
#define	USB_PRODUCT_PLANEX2_GWUS54GZL	0xc007		/* GW-US54GZL */
#define	USB_PRODUCT_PLANEX2_GWUS54GD	0xed01		/* GW-US54GD */
#define	USB_PRODUCT_PLANEX2_GWUSMM	0xed02		/* GW-USMM */
#define	USB_PRODUCT_PLANEX2_GWUS300MINIX	0xed06		/* GW-US300Mini-X/MiniW */
#define	USB_PRODUCT_PLANEX2_GWUSMICRON	0xed14		/* GW-USMicroN */
#define	USB_PRODUCT_PLANEX2_GWUSMICRON2	0xed16		/* GW-USMicroN2 */
#define	USB_PRODUCT_PLANEX2_RTL8188CU_2	0xed17		/* RTL8188CU */
#define	USB_PRODUCT_PLANEX2_GWUSH300N	0xed18		/* GW-USH300N */
#define	USB_PRODUCT_PLANEX3_GWUS54GZ	0xab10		/* GW-US54GZ */
#define	USB_PRODUCT_PLANEX3_GU1000T	0xab11		/* GU-1000T */
#define	USB_PRODUCT_PLANEX3_GWUS54MINI	0xab13		/* GW-US54Mini */

/* Plantronics products */
#define	USB_PRODUCT_PLANTRONICS_HEADSET	0x0ca1		/* Platronics DSP-400 Headset */

/* Plextor Corp. */
#define	USB_PRODUCT_PLEXTOR_40_12_40U	0x0011		/* PlexWriter 40/12/40U */

/* PLX products */
#define	USB_PRODUCT_PLX_TESTBOARD	0x9060		/* test board */

/* PointChips */
#define	USB_PRODUCT_POINTCHIPS_FLASH	0x8001		/* Flash */

/* PortGear products */
#define	USB_PRODUCT_PORTGEAR_EA8	0x0008		/* Ethernet Adapter */
#define	USB_PRODUCT_PORTGEAR_EA9	0x0009		/* Ethernet Adapter */

/* Portsmith products */
#define	USB_PRODUCT_PORTSMITH_EEA	0x3003		/* Express Ethernet Adapter */

/* PQI products */
#define	USB_PRODUCT_PQI_TRAVELFLASH	0x0001		/* Travel Flash Drive */

/* Primax products */
#define	USB_PRODUCT_PRIMAX_G2X300	0x0300		/* G2-200 scanner */
#define	USB_PRODUCT_PRIMAX_G2E300	0x0301		/* G2E-300 scanner */
#define	USB_PRODUCT_PRIMAX_G2300	0x0302		/* G2-300 scanner */
#define	USB_PRODUCT_PRIMAX_G2E3002	0x0303		/* G2E-300 scanner */
#define	USB_PRODUCT_PRIMAX_9600	0x0340		/* Colorado USB 9600 scanner */
#define	USB_PRODUCT_PRIMAX_600U	0x0341		/* Colorado 600u scanner */
#define	USB_PRODUCT_PRIMAX_6200	0x0345		/* Visioneer 6200 scanner */
#define	USB_PRODUCT_PRIMAX_19200	0x0360		/* Colorado USB 19200 scanner */
#define	USB_PRODUCT_PRIMAX_1200U	0x0361		/* Colorado 1200u scanner */
#define	USB_PRODUCT_PRIMAX_G600	0x0380		/* G2-600 scanner */
#define	USB_PRODUCT_PRIMAX_636I	0x0381		/* ReadyScan 636i */
#define	USB_PRODUCT_PRIMAX_G2600	0x0382		/* G2-600 scanner */
#define	USB_PRODUCT_PRIMAX_G2E600	0x0383		/* G2E-600 scanner */
#define	USB_PRODUCT_PRIMAX_COMFORT	0x4d01		/* Comfort */
#define	USB_PRODUCT_PRIMAX_MOUSEINABOX	0x4d02		/* Mouse-in-a-Box */
#define	USB_PRODUCT_PRIMAX_PCGAUMS1	0x4d04		/* Sony PCGA-UMS1 */

/* Prolific products */
#define	USB_PRODUCT_PROLIFIC_PL2301	0x0000		/* PL2301 Host-Host interface */
#define	USB_PRODUCT_PROLIFIC_PL2302	0x0001		/* PL2302 Host-Host interface */
#define	USB_PRODUCT_PROLIFIC_RSAQ2	0x04bb		/* PL2303 Serial adapter (IODATA USB-RSAQ2) */
#define	USB_PRODUCT_PROLIFIC_PL2303	0x2303		/* PL2303 Serial adapter (ATEN/IOGEAR UC232A) */
#define	USB_PRODUCT_PROLIFIC_PL2305	0x2305		/* Parallel printer adapter */
#define	USB_PRODUCT_PROLIFIC_ATAPI4	0x2307		/* ATAPI-4 Bridge Controller */
#define	USB_PRODUCT_PROLIFIC_PL2501	0x2501		/* PL2501 Host-Host interface */
#define	USB_PRODUCT_PROLIFIC_PL2303X	0xaaa0		/* PL2303 Serial adapter (Pharos GPS) */
#define	USB_PRODUCT_PROLIFIC_RSAQ3	0xaaa2		/* PL2303 Serial adapter (IODATA USB-RSAQ3) */
#define	USB_PRODUCT_PROLIFIC2_PL2303	0x2303		/* PL2303 Serial adapter (SMART Technologies) */

/* Putercom products */
#define	USB_PRODUCT_PUTERCOM_UPA100	0x047e		/* USB-1284 BRIDGE */

/* Qcom products */
#define	USB_PRODUCT_QCOM_RT2573	0x6196		/* RT2573 */
#define	USB_PRODUCT_QCOM_RT2573_2	0x6229		/* RT2573 */
#define	USB_PRODUCT_QCOM_RT2573_3	0x6238		/* RT2573 */
#define	USB_PRODUCT_QCOM_RT2870	0x6259		/* RT2870 */

/* Qtronix products */
#define	USB_PRODUCT_QTRONIX_980N	0x2011		/* Scorpion-980N keyboard */

/* Qualcomm products */
#define	USB_PRODUCT_QUALCOMM_CDMA_MSM	0x6000		/* CDMA Technologies MSM phone */
#define	USB_PRODUCT_QUALCOMM_NTT_DOCOMO_L02C_MODEM	0x618f		/* NTT DOCOMO L-02C */
#define	USB_PRODUCT_QUALCOMM_NTT_DOCOMO_L02C_STORAGE	0x61dd		/* NTT DOCOMO L-02C */
#define	USB_PRODUCT_QUALCOMM_MSM_HSDPA	0x6613		/* HSDPA MSM */
#define	USB_PRODUCT_QUALCOMM2_RWT_FCT	0x3100		/* RWT FCT-CDMA 2000 1xRTT modem */
#define	USB_PRODUCT_QUALCOMM2_CDMA_MSM	0x3196		/* CDMA Technologies MSM modem */

/* Qualcomm Kyocera products */
#define	USB_PRODUCT_QUALCOMM_K_CDMA_MSM_K	0x17da		/* Qualcomm Kyocera CDMA Technologies MSM */

/* Quan products */
#define	USB_PRODUCT_QUAN_DM9601	0x9601		/* USB ethernet */

/* Quanta products */
#define	USB_PRODUCT_QUANTA_RT3070	0x0304		/* RT3070 */

/* Quickshot products */
#define	USB_PRODUCT_QUICKSHOT_STRIKEPAD	0x6238		/* USB StrikePad */

/* Rainbow Technologies products */
#define	USB_PRODUCT_RAINBOW_IKEY2000	0x1200		/* i-Key 2000 */

/* Ralink Technology products */
#define	USB_PRODUCT_RALINK_RT2570	0x1706		/* RT2570 */
#define	USB_PRODUCT_RALINK_RT2070	0x2070		/* RT2070 */
#define	USB_PRODUCT_RALINK_RT2570_2	0x2570		/* RT2570 */
#define	USB_PRODUCT_RALINK_RT2573	0x2573		/* RT2573 */
#define	USB_PRODUCT_RALINK_RT2671	0x2671		/* RT2671 */
#define	USB_PRODUCT_RALINK_RT2770	0x2770		/* RT2770 */
#define	USB_PRODUCT_RALINK_RT2870	0x2870		/* RT2870 */
#define	USB_PRODUCT_RALINK_RT3070	0x3070		/* RT3070 */
#define	USB_PRODUCT_RALINK_RT3071	0x3071		/* RT3071 */
#define	USB_PRODUCT_RALINK_RT3072	0x3072		/* RT3072 */
#define	USB_PRODUCT_RALINK_RT3370	0x3370		/* RT3370 */
#define	USB_PRODUCT_RALINK_RT3572	0x3572		/* RT3572 */
#define	USB_PRODUCT_RALINK_RT3573	0x3573		/* RT3573 */
#define	USB_PRODUCT_RALINK_RT5370	0x5370		/* RT5370 */
#define	USB_PRODUCT_RALINK_RT5572	0x5572		/* RT5572 */
#define	USB_PRODUCT_RALINK_MT7610U	0x7610		/* MT7610U */
#define	USB_PRODUCT_RALINK_RT8070	0x8070		/* RT8070 */
#define	USB_PRODUCT_RALINK_RT2570_3	0x9020		/* RT2570 */

/* RATOC Systems products */
#define	USB_PRODUCT_RATOC_REXUSB60	0xb000		/* USB serial adapter REX-USB60 */
#define	USB_PRODUCT_RATOC_REXUSB60F	0xb020		/* USB serial adapter REX-USB60F */

/* Realtek products */
#define	USB_PRODUCT_REALTEK_RTL8188ETV	0x0179		/* RTL8188ETV */
#define	USB_PRODUCT_REALTEK_RTL8188CTV	0x018a		/* RTL8188CTV */
#define	USB_PRODUCT_REALTEK_RTL8188RU_2	0x317f		/* RTL8188RU */
#define	USB_PRODUCT_REALTEK_RTL8150L	0x8150		/* RTL8150L USB-Ethernet Bridge */
#define	USB_PRODUCT_REALTEK_RTL8151	0x8151		/* RTL8151 PNA */
#define	USB_PRODUCT_REALTEK_RTL8152	0x8152		/* RTL8152 */
#define	USB_PRODUCT_REALTEK_RTL8153	0x8153		/* RTL8153 */
#define	USB_PRODUCT_REALTEK_RTL8188CE_0	0x8170		/* RTL8188CE */
#define	USB_PRODUCT_REALTEK_RTL8171	0x8171		/* RTL8171 */
#define	USB_PRODUCT_REALTEK_RTL8172	0x8172		/* RTL8172 */
#define	USB_PRODUCT_REALTEK_RTL8173	0x8173		/* RTL8173 */
#define	USB_PRODUCT_REALTEK_RTL8174	0x8174		/* RTL8174 */
#define	USB_PRODUCT_REALTEK_RTL8188CU_0	0x8176		/* RTL8188CU */
#define	USB_PRODUCT_REALTEK_RTL8191CU	0x8177		/* RTL8191CU */
#define	USB_PRODUCT_REALTEK_RTL8192CU	0x8178		/* RTL8192CU */
#define	USB_PRODUCT_REALTEK_RTL8188EU	0x8179		/* RTL8188EU */
#define	USB_PRODUCT_REALTEK_RTL8188CU_1	0x817a		/* RTL8188CU */
#define	USB_PRODUCT_REALTEK_RTL8188CU_2	0x817b		/* RTL8188CU */
#define	USB_PRODUCT_REALTEK_RTL8192CE	0x817c		/* RTL8192CE */
#define	USB_PRODUCT_REALTEK_RTL8188RU	0x817d		/* RTL8188RU */
#define	USB_PRODUCT_REALTEK_RTL8188CE_1	0x817e		/* RTL8188CE */
#define	USB_PRODUCT_REALTEK_RTL8188RU_3	0x817f		/* RTL8188RU */
#define	USB_PRODUCT_REALTEK_RTL8187	0x8187		/* RTL8187 */
#define	USB_PRODUCT_REALTEK_RTL8187B_0	0x8189		/* RTL8187B */
#define	USB_PRODUCT_REALTEK_RTL8188CUS	0x818a		/* RTL8188CUS */
#define	USB_PRODUCT_REALTEK_RTL8187B_1	0x8197		/* RTL8187B */
#define	USB_PRODUCT_REALTEK_RTL8187B_2	0x8198		/* RTL8187B */
#define	USB_PRODUCT_REALTEK_RTL8712	0x8712		/* RTL8712 */
#define	USB_PRODUCT_REALTEK_RTL8713	0x8713		/* RTL8713 */
#define	USB_PRODUCT_REALTEK_RTL8188CU_COMBO	0x8754		/* RTL8188CU */
#define	USB_PRODUCT_REALTEK_RTL8192SU	0xc512		/* RTL8192SU */

/* Research In Motion */
#define	USB_PRODUCT_RIM_BLACKBERRY	0x0001		/* BlackBerry */
#define	USB_PRODUCT_RIM_BLACKBERRY_PEARL_DUAL	0x0004		/* BlackBerry Pearl Dual */
#define	USB_PRODUCT_RIM_BLACKBERRY_PEARL	0x0006		/* BlackBerry Pearl */

/* Rockfire products */
#define	USB_PRODUCT_ROCKFIRE_GAMEPAD	0x2033		/* gamepad 203USB */

/* Roland products */
#define	USB_PRODUCT_ROLAND_UA100	0x0000		/* UA-100 USB Audio I/F */
#define	USB_PRODUCT_ROLAND_UM4	0x0002		/* UM-4 MIDI I/F */
#define	USB_PRODUCT_ROLAND_SC8850	0x0003		/* RolandED SC-8850 SOUND Canvas MIDI Synth. */
#define	USB_PRODUCT_ROLAND_U8	0x0004		/* U-8 USB Audio I/F */
#define	USB_PRODUCT_ROLAND_UM2	0x0005		/* UM-2 MIDI I/F */
#define	USB_PRODUCT_ROLAND_SC8820	0x0007		/* SoundCanvas SC-8820 MIDI Synth. */
#define	USB_PRODUCT_ROLAND_PC300	0x0008		/* PC-300 MIDI Keyboard */
#define	USB_PRODUCT_ROLAND_UM1	0x0009		/* UM-1 MIDI I/F */
#define	USB_PRODUCT_ROLAND_SK500	0x000b		/* SoundCanvas SK-500 MIDI Keyboard */
#define	USB_PRODUCT_ROLAND_SCD70	0x000c		/* SC-D70 MIDI Synth. */
#define	USB_PRODUCT_ROLAND_UA3	0x000f		/* EDIROL UA-3 USB audio I/F */
#define	USB_PRODUCT_ROLAND_XV5050	0x0012		/* XV-5050 MIDI Synth. */
#define	USB_PRODUCT_ROLAND_UM880N	0x0014		/* EDIROL UM-880 MIDI I/F (native) */
#define	USB_PRODUCT_ROLAND_UM880G	0x0015		/* EDIROL UM-880 MIDI I/F (generic) */
#define	USB_PRODUCT_ROLAND_SD90	0x0016		/* EDIROL SD-90 STDIO Canvas MIDI Synth. */
#define	USB_PRODUCT_ROLAND_UA1A	0x0018		/* UA-1A USB Audio I/F */
#define	USB_PRODUCT_ROLAND_UM550	0x0023		/* UM-550 MIDI I/F */
#define	USB_PRODUCT_ROLAND_SD20	0x0027		/* SD-20 MIDI Synth. */
#define	USB_PRODUCT_ROLAND_SD80	0x0029		/* SD-80 MIDI Synth. */
#define	USB_PRODUCT_ROLAND_UA700	0x002b		/* UA-700 USB Audio I/F */
#define	USB_PRODUCT_ROLAND_PCRA	0x0033		/* EDIROL PCR MIDI keyboard (advanced) */
#define	USB_PRODUCT_ROLAND_PCR	0x0034		/* EDIROL PCR MIDI keyboard */
#define	USB_PRODUCT_ROLAND_M1000	0x0035		/* M-1000 audio I/F */
#define	USB_PRODUCT_ROLAND_UA1000	0x0044		/* EDIROL UA-1000 USB audio I/F */
#define	USB_PRODUCT_ROLAND_UA3FXA	0x0050		/* EDIROL UA-3FX USB audio I/F (advanced) */
#define	USB_PRODUCT_ROLAND_UA3FX	0x0051		/* EDIROL UA-3FX USB audio I/F */
#define	USB_PRODUCT_ROLAND_FANTOMX	0x006d		/* Fantom-X MIDI Synth. */
#define	USB_PRODUCT_ROLAND_UA25	0x0074		/* EDIROL UA-25 */
#define	USB_PRODUCT_ROLAND_UA101	0x007d		/* EDIROL UA-101 */
#define	USB_PRODUCT_ROLAND_PC50A	0x008b		/* EDIROL PC-50 (advanced) */
#define	USB_PRODUCT_ROLAND_PC50	0x008c		/* EDIROL PC-50 */
#define	USB_PRODUCT_ROLAND_UA101F	0x008d		/* EDIROL UA-101 USB1 */
#define	USB_PRODUCT_ROLAND_UA1EX	0x0096		/* EDIROL UA-1EX */
#define	USB_PRODUCT_ROLAND_UM3	0x009A		/* EDIROL UM-3 */
#define	USB_PRODUCT_ROLAND_UA4FX	0x00A3		/* EDIROL UA-4FX */
#define	USB_PRODUCT_ROLAND_SONICCELL	0x00C2		/* SonicCell */
#define	USB_PRODUCT_ROLAND_UMONE	0x012a		/* UM-ONE MIDI I/F */

/* Sagem products */
#define	USB_PRODUCT_SAGEM_XG760A	0x004a		/* XG-760A */
#define	USB_PRODUCT_SAGEM_XG76NA	0x0062		/* XG-76NA */

/* Saitek products */
#define	USB_PRODUCT_SAITEK_CYBORG_3D_GOLD	0x0006		/* Cyborg 3D Gold Joystick */

/* Samsung products */
#define	USB_PRODUCT_SAMSUNG_MIGHTYDRIVE	0x1623		/* Mighty Drive */
#define	USB_PRODUCT_SAMSUNG_RT2870_1	0x2018		/* RT2870 */
#define	USB_PRODUCT_SAMSUNG_ML6060	0x3008		/* ML-6060 laser printer */
#define	USB_PRODUCT_SAMSUNG_ANDROID	0x6863		/* Android */
#define	USB_PRODUCT_SAMSUNG_GTB3710	0x6876		/* GT-B3710 LTE/4G datacard */
#define	USB_PRODUCT_SAMSUNG_ANDROID2	0x6881		/* Android */
#define	USB_PRODUCT_SAMSUNG_GTB3730	0x689a		/* GT-B3730 LTE/4G datacard */
#define	USB_PRODUCT_SAMSUNG_SWL2100W	0xa000		/* SWL-2100U */

/* SanDisk products */
#define	USB_PRODUCT_SANDISK_SDDR05A	0x0001		/* ImageMate SDDR-05a */
#define	USB_PRODUCT_SANDISK_SDDR31	0x0002		/* ImageMate SDDR-31 */
#define	USB_PRODUCT_SANDISK_SDDR05	0x0005		/* ImageMate SDDR-05 */
#define	USB_PRODUCT_SANDISK_SDDR12	0x0100		/* ImageMate SDDR-12 */
#define	USB_PRODUCT_SANDISK_SDDR09	0x0200		/* ImageMate SDDR-09 */
#define	USB_PRODUCT_SANDISK_SDDR86	0x0621		/* ImageMate SDDR-86 */
#define	USB_PRODUCT_SANDISK_SDDR75	0x0810		/* ImageMate SDDR-75 */
#define	USB_PRODUCT_SANDISK_SANSA_CLIP	0x7433		/* Sansa Clip */

/* Sanwa Supply products */
#define	USB_PRODUCT_SANWASUPPLY_JYDV9USB	0x9806		/* JY-DV9USB gamepad */

/* Sanyo Electric products */
#define	USB_PRODUCT_SANYO_SCP4900	0x0701		/* Sanyo SCP-4900 USB Phone */

/* ScanLogic products */
#define	USB_PRODUCT_SCANLOGIC_SL11R	0x0002		/* SL11R-IDE */
#define	USB_PRODUCT_SCANLOGIC_336CX	0x0300		/* Phantom 336CX - C3 scanner */

/* Sealevel products */
#define	USB_PRODUCT_SEALEVEL_USBSERIAL	0x2101		/* USB-Serial converter */
#define	USB_PRODUCT_SEALEVEL_SEAPORT4P1	0x2413		/* SeaPort+4 Port 1 */
#define	USB_PRODUCT_SEALEVEL_SEAPORT4P2	0x2423		/* SeaPort+4 Port 2 */
#define	USB_PRODUCT_SEALEVEL_SEAPORT4P3	0x2433		/* SeaPort+4 Port 3 */
#define	USB_PRODUCT_SEALEVEL_SEAPORT4P4	0x2443		/* SeaPort+4 Port 4 */

/* Senao products */
#define	USB_PRODUCT_SENAO_RT2870_3	0x0605		/* RT2870 */
#define	USB_PRODUCT_SENAO_RT2870_4	0x0615		/* RT2870 */
#define	USB_PRODUCT_SENAO_NUB8301	0x2000		/* NUB-8301 */
#define	USB_PRODUCT_SENAO_RTL8192SU_1	0x9603		/* RTL8192SU */
#define	USB_PRODUCT_SENAO_RTL8192SU_2	0x9605		/* RTL8192SU */
#define	USB_PRODUCT_SENAO_RT2870_1	0x9701		/* RT2870 */
#define	USB_PRODUCT_SENAO_RT2870_2	0x9702		/* RT2870 */
#define	USB_PRODUCT_SENAO_RT3070	0x9703		/* RT3070 */
#define	USB_PRODUCT_SENAO_RT3071	0x9705		/* RT3071 */
#define	USB_PRODUCT_SENAO_RT3072	0x9706		/* RT3072 */
#define	USB_PRODUCT_SENAO_RT3072_2	0x9707		/* RT3072 */
#define	USB_PRODUCT_SENAO_RT3072_3	0x9708		/* RT3072 */
#define	USB_PRODUCT_SENAO_RT3072_4	0x9709		/* RT3072 */
#define	USB_PRODUCT_SENAO_RT3072_5	0x9801		/* RT3072 */

/* SGI products */
#define	USB_PRODUCT_SGI_SN1_L1_SC	0x1234		/* SN1 L1 System Controller */

/* ShanTou products */
#define	USB_PRODUCT_SHANTOU_ST268_USB_NIC	0x0268		/* ST268 USB NIC */
#define	USB_PRODUCT_SHANTOU_ADM8515	0x8515		/* ADM8515 Ethernet */

/* Shark products */
#define	USB_PRODUCT_SHARK_PA	0x0400		/* Pocket Adapter */

/* Sharp products */
#define	USB_PRODUCT_SHARP_CE175TU	0x8000		/* CE175TU */
#define	USB_PRODUCT_SHARP_SL5500	0x8004		/* SL5500 */
#define	USB_PRODUCT_SHARP_A300	0x8005		/* A300 */
#define	USB_PRODUCT_SHARP_SL5600	0x8006		/* SL5600 */
#define	USB_PRODUCT_SHARP_C700	0x8007		/* C700 */
#define	USB_PRODUCT_SHARP_C750	0x9031		/* C750 */
#define	USB_PRODUCT_SHARP_RUITZ1016YCZZ	0x90fd		/* WS003SH WLAN */
#define	USB_PRODUCT_SHARP_WS007SH	0x9123		/* WS007SH */
#define	USB_PRODUCT_SHARP_WS011SH	0x91ac		/* WS011SH */
#define	USB_PRODUCT_SHARP_NWKBD	0x92E7		/* NetWalker Keyboard */

/* Shuttle Technology products */
#define	USB_PRODUCT_SHUTTLE_EUSB	0x0001		/* E-USB Bridge */
#define	USB_PRODUCT_SHUTTLE_EUSCSI	0x0002		/* eUSCSI Bridge */
#define	USB_PRODUCT_SHUTTLE_SDDR09	0x0003		/* ImageMate SDDR09 */
#define	USB_PRODUCT_SHUTTLE_EUSBSMCF	0x0005		/* eUSB SmartMedia / CompactFlash Adapter */
#define	USB_PRODUCT_SHUTTLE_ZIOMMC	0x0006		/* eUSB MultiMediaCard Adapter */
#define	USB_PRODUCT_SHUTTLE_HIFD	0x0007		/* Sony Hifd */
#define	USB_PRODUCT_SHUTTLE_EUSBATAPI	0x0009		/* eUSB ATA/ATAPI Adapter */
#define	USB_PRODUCT_SHUTTLE_CF	0x000a		/* eUSB CompactFlash Adapter */
#define	USB_PRODUCT_SHUTTLE_EUSCSI_B	0x000b		/* eUSCSI Bridge */
#define	USB_PRODUCT_SHUTTLE_EUSCSI_C	0x000c		/* eUSCSI Bridge */
#define	USB_PRODUCT_SHUTTLE_CDRW	0x0101		/* CD-RW Device */
#define	USB_PRODUCT_SHUTTLE_ORCA	0x0325		/* eUSB ORCA Quad Reader */
#define	USB_PRODUCT_SHUTTLE_SCM	0x1010		/* SCM Micro */

/* Siemens products */
#define	USB_PRODUCT_SIEMENS_SPEEDSTREAM	0x1001		/* SpeedStream USB */
#define	USB_PRODUCT_SIEMENS_SPEEDSTREAM22	0x1022		/* SpeedStream USB 1022 */

/* Siemens Info products */
#define	USB_PRODUCT_SIEMENS2_WLL013	0x001b		/* WLL013 */
#define	USB_PRODUCT_SIEMENS2_MC75	0x0034		/* Wireless Modules MC75 */
#define	USB_PRODUCT_SIEMENS2_WL54G	0x3c06		/* 54g USB Network Adapter */

/* Sierra Wireless products */
#define	USB_PRODUCT_SIERRA_EM5625	0x0017		/* EM5625 */
#define	USB_PRODUCT_SIERRA_MC5720_2	0x0018		/* MC5720 */
#define	USB_PRODUCT_SIERRA_AIRCARD595	0x0019		/* Sierra Wireless AirCard 595 */
#define	USB_PRODUCT_SIERRA_MC5725	0x0020		/* MC5725 */
#define	USB_PRODUCT_SIERRA_AC597E	0x0021		/* Sierra Wireless AirCard 597E */
#define	USB_PRODUCT_SIERRA_C597	0x0023		/* Sierra Wireless Compass 597 */
#define	USB_PRODUCT_SIERRA_AIRCARD580	0x0112		/* Sierra Wireless AirCard 580 */
#define	USB_PRODUCT_SIERRA_AC595U	0x0120		/* Sierra Wireless AirCard 595U */
#define	USB_PRODUCT_SIERRA_MC5720	0x0218		/* MC5720 Wireless Modem */
#define	USB_PRODUCT_SIERRA_MINI5725	0x0220		/* Sierra Wireless miniPCI 5275 */
#define	USB_PRODUCT_SIERRA_250U	0x0301		/* Sieral Wireless 250U 3G */
#define	USB_PRODUCT_SIERRA_INSTALLER	0x0fff		/* Aircard Driver Installer */
#define	USB_PRODUCT_SIERRA_MC8755_2	0x6802		/* MC8755 */
#define	USB_PRODUCT_SIERRA_MC8765	0x6803		/* MC8765 */
#define	USB_PRODUCT_SIERRA_MC8755	0x6804		/* MC8755 */
#define	USB_PRODUCT_SIERRA_AC875U	0x6812		/* AC875U HSDPA USB Modem */
#define	USB_PRODUCT_SIERRA_MC8755_3	0x6813		/* MC8755 HSDPA */
#define	USB_PRODUCT_SIERRA_MC8775_2	0x6815		/* MC8775 */
#define	USB_PRODUCT_SIERRA_AIRCARD875	0x6820		/* Aircard 875 HSDPA */
#define	USB_PRODUCT_SIERRA_MC8780	0x6832		/* MC8780 */
#define	USB_PRODUCT_SIERRA_MC8781	0x6833		/* MC8781 */
#define	USB_PRODUCT_SIERRA_AC880	0x6850		/* Sierra Wireless AirCard 880 */
#define	USB_PRODUCT_SIERRA_AC881	0x6851		/* Sierra Wireless AirCard 881 */
#define	USB_PRODUCT_SIERRA_AC880E	0x6852		/* Sierra Wireless AirCard 880E */
#define	USB_PRODUCT_SIERRA_AC881E	0x6853		/* Sierra Wireless AirCard 881E */
#define	USB_PRODUCT_SIERRA_AC880U	0x6855		/* Sierra Wireless AirCard 880U */
#define	USB_PRODUCT_SIERRA_AC881U	0x6856		/* Sierra Wireless AirCard 881U */
#define	USB_PRODUCT_SIERRA_AC885U	0x6880		/* Sierra Wireless AirCard 885U */
#define	USB_PRODUCT_SIERRA_USB305	0x68a3		/* Sierra Wireless AirCard USB 305 */

/* Sigmatel products */
#define	USB_PRODUCT_SIGMATEL_SIR4116	0x4116		/* StIR4116 SIR */
#define	USB_PRODUCT_SIGMATEL_IRDA	0x4200		/* IrDA */
#define	USB_PRODUCT_SIGMATEL_FIR4210	0x4210		/* StIR4210 FIR */
#define	USB_PRODUCT_SIGMATEL_VFIR4220	0x4220		/* StIR4220 VFIR */
#define	USB_PRODUCT_SIGMATEL_I_BEAD100	0x8008		/* i-Bead 100 MP3 Player */
#define	USB_PRODUCT_SIGMATEL_I_BEAD150	0x8009		/* i-Bead 150 MP3 Player */
#define	USB_PRODUCT_SIGMATEL_DNSSF7X	0x8020		/* Datum Networks SSF-7X Multi Players */
#define	USB_PRODUCT_SIGMATEL_MUSICSTICK	0x8134		/* TrekStor Musicstick */

/* SIIG products */
#define	USB_PRODUCT_SIIG_DIGIFILMREADER	0x0004		/* DigiFilm-Combo Reader */
#define	USB_PRODUCT_SIIG_UISDMC2S	0x0200		/* MULTICARDREADER */
#define	USB_PRODUCT_SIIG_MULTICARDREADER	0x0201		/* MULTICARDREADER */

#define	USB_PRODUCT_SIIG2_USBTOETHER	0x0109		/* USB TO Ethernet */
#define	USB_PRODUCT_SIIG2_US2308	0x0421		/* Serial */

/* Silicom products */
#define	USB_PRODUCT_SILICOM_U2E	0x0001		/* U2E */
#define	USB_PRODUCT_SILICOM_GPE	0x0002		/* Psion Gold Port Ethernet */

/* Silicon Labs products */
#define	USB_PRODUCT_SILABS_POLOLU	0x803b		/* Pololu Serial */
#define	USB_PRODUCT_SILABS_ARGUSISP	0x8066		/* Argussoft ISP */
#define	USB_PRODUCT_SILABS_CRUMB128	0x807a		/* Crumb128 */
#define	USB_PRODUCT_SILABS_DEGREECONT	0x80ca		/* Degree Controls */
#define	USB_PRODUCT_SILABS_SUNNTO	0x80f6		/* Suunto sports */
#define	USB_PRODUCT_SILABS_DESKTOPMOBILE	0x813d		/* Burnside Desktop mobile */
#define	USB_PRODUCT_SILABS_IPLINK1220	0x815e		/* IP-Link 1220 */
#define	USB_PRODUCT_SILABS_LIPOWSKY_JTAG	0x81c8		/* Lipowsky Baby-JTAG */
#define	USB_PRODUCT_SILABS_LIPOWSKY_LIN	0x81e2		/* Lipowsky Baby-LIN */
#define	USB_PRODUCT_SILABS_LIPOWSKY_HARP	0x8218		/* Lipowsky HARP-1 */
#define	USB_PRODUCT_SILABS2_DCU11CLONE	0xaa26		/* DCU-11 clone */
#define	USB_PRODUCT_SILABS_CP210X_1	0xea60		/* CP210x Serial */
#define	USB_PRODUCT_SILABS_CP210X_2	0xea61		/* CP210x Serial */
#define	USB_PRODUCT_SILABS_EC3	0x8044		/* EC3 USB Debug Adapter */

/* Silicon Portals Inc. */
#define	USB_PRODUCT_SILICONPORTALS_YAPPH_NF	0x0200		/* YAP Phone (no firmware) */
#define	USB_PRODUCT_SILICONPORTALS_YAPPHONE	0x0201		/* YAP Phone */

/* Silicon Integrated Systems products */
#define	USB_PRODUCT_SIS_SIS_163U	0x0163		/* 802.11g Wireless LAN Adapter */

/* Sirius Technologies products */
#define	USB_PRODUCT_SIRIUS_ROADSTER	0x0001		/* NetComm Roadster II 56 USB */

/* Sitecom products */
#define	USB_PRODUCT_SITECOM_LN029	0x182d		/* LN029 */
#define	USB_PRODUCT_SITECOM_CN104	0x2068		/* CN104 serial */

/* Sitecom Europe products */
#define	USB_PRODUCT_SITECOMEU_WL168V1	0x000d		/* WL-168 v1 */
#define	USB_PRODUCT_SITECOMEU_RT2870_1	0x0017		/* RT2870 */
#define	USB_PRODUCT_SITECOMEU_WL168V4	0x0028		/* WL-168 v4 */
#define	USB_PRODUCT_SITECOMEU_RT2870_2	0x002b		/* RT2870 */
#define	USB_PRODUCT_SITECOMEU_RT2870_3	0x002c		/* RT2870 */
#define	USB_PRODUCT_SITECOMEU_WL302	0x002d		/* WL-302 */
#define	USB_PRODUCT_SITECOMEU_WL603	0x0036		/* WL-603 */
#define	USB_PRODUCT_SITECOMEU_WL315	0x0039		/* WL-315 */
#define	USB_PRODUCT_SITECOMEU_WL321	0x003b		/* WL-321 */
#define	USB_PRODUCT_SITECOMEU_RT3070_3	0x003c		/* RT3070 */
#define	USB_PRODUCT_SITECOMEU_WL324	0x003d		/* WL-324 */
#define	USB_PRODUCT_SITECOMEU_WL343	0x003e		/* WL-343 */
#define	USB_PRODUCT_SITECOMEU_WL608	0x003f		/* WL-608 */
#define	USB_PRODUCT_SITECOMEU_WL344	0x0040		/* WL-344 */
#define	USB_PRODUCT_SITECOMEU_WL329	0x0041		/* WL-329 */
#define	USB_PRODUCT_SITECOMEU_WL345	0x0042		/* WL-345 */
#define	USB_PRODUCT_SITECOMEU_WL353	0x0045		/* WL-353 */
#define	USB_PRODUCT_SITECOMEU_RT3072_3	0x0047		/* RT3072 */
#define	USB_PRODUCT_SITECOMEU_RT3072_4	0x0048		/* RT3072 */
#define	USB_PRODUCT_SITECOMEU_WL349V1	0x004b		/* WL-349 v1 */
#define	USB_PRODUCT_SITECOMEU_RT3072_6	0x004d		/* RT3072 */
#define	USB_PRODUCT_SITECOMEU_WL349V4	0x0050		/* WL-349 v4 */
#define	USB_PRODUCT_SITECOMEU_RT3070_1	0x0051		/* RT3070 */
#define	USB_PRODUCT_SITECOMEU_RTL8188CU	0x0052		/* RTL8188CU */
#define	USB_PRODUCT_SITECOMEU_RTL8188CU_2	0x005c		/* RTL8188CU */
#define	USB_PRODUCT_SITECOMEU_RT3072_5	0x005f		/* RT3072 */
#define	USB_PRODUCT_SITECOMEU_WLA4000	0x0060		/* WLA-4000 */
#define	USB_PRODUCT_SITECOMEU_RTL8192CU	0x0061		/* RTL8192CU */
#define	USB_PRODUCT_SITECOMEU_WLA5000	0x0062		/* WLA-5000 */
#define	USB_PRODUCT_SITECOMEU_AX88179	0x0072		/* AX88179 USB 3.0 gigabit ethernet controller */
#define	USB_PRODUCT_SITECOMEU_LN028	0x061c		/* LN-028 */
#define	USB_PRODUCT_SITECOMEU_RTL8192CUR2	0x0070		/* RTL8192CU rev 2/2 */
#define	USB_PRODUCT_SITECOMEU_WL113	0x9071		/* WL-113 */
#define	USB_PRODUCT_SITECOMEU_ZD1211B	0x9075		/* ZD1211B */
#define	USB_PRODUCT_SITECOMEU_WL172	0x90ac		/* WL-172 */
#define	USB_PRODUCT_SITECOMEU_WL113R2	0x9712		/* WL-113 rev 2 */

/* SmartBridges products */
#define	USB_PRODUCT_SMARTBRIDGES_SMARTLINK	0x0001		/* SmartLink USB ethernet adapter */
#define	USB_PRODUCT_SMARTBRIDGES_SMARTNIC	0x0003		/* smartNIC 2 PnP Adapter */

/* SMC Networks products */
#define	USB_PRODUCT_SMC_2102USB	0x0100		/* 10Mbps ethernet adapter */
#define	USB_PRODUCT_SMC_2202USB	0x0200		/* 10/100 ethernet adapter */
#define	USB_PRODUCT_SMC_2206USB	0x0201		/* EZ Connect USB Ethernet Adapter */
#define	USB_PRODUCT_SMC3_2662WV1	0xa001		/* EZ Connect 11Mbps */
#define	USB_PRODUCT_SMC3_2662WV2	0xa002		/* EZ Connect 11Mbps v2 */
#define	USB_PRODUCT_SMC_2862WG	0xee13		/* EZ Connect 54Mbps v2 USB 2.0 */
#define	USB_PRODUCT_SMC_2862WG_V1	0xee06		/* EZ Connect 54Mbps v1 USB 1.0 */

/* SMK products */
#define	USB_PRODUCT_SMK_MCE_IR	0x031d		/* eHome Infrared Transceiver */

/* SMSC products */
#define	USB_PRODUCT_SMSC_2020HUB	0x2020		/* USB Hub */
#define	USB_PRODUCT_SMSC_2512HUB	0x2512		/* USB 2.0 2-Port Hub */
#define	USB_PRODUCT_SMSC_2513HUB	0x2513		/* USB 2.0 3-Port Hub */
#define	USB_PRODUCT_SMSC_2514HUB	0x2514		/* USB 2.0 4-Port Hub */
#define	USB_PRODUCT_SMSC_LAN7500	0x7500		/* LAN7500 USB 2.0 gigabit ethernet device */
#define	USB_PRODUCT_SMSC_SMSC9500	0x9500		/* SMSC9500 Ethernet device */
#define	USB_PRODUCT_SMSC_SMSC9505	0x9505		/* SMSC9505 Ethernet device */
#define	USB_PRODUCT_SMSC_SMSC9512	0x9512		/* SMSC9512 USB Hub & Ethernet device */
#define	USB_PRODUCT_SMSC_SMSC9514	0x9514		/* SMSC9514 USB Hub & Ethernet device */
#define	USB_PRODUCT_SMSC_LAN9530	0x9530		/* LAN9530 Ethernet Device */
#define	USB_PRODUCT_SMSC_LAN9730	0x9730		/* LAN9730 Ethernet Device */
#define	USB_PRODUCT_SMSC_SMSC9500_SAL10	0x9900		/* SMSC9500 Ethernet device (SAL10) */
#define	USB_PRODUCT_SMSC_SMSC9505_SAL10	0x9901		/* SMSC9505 Ethernet device (SAL10) */
#define	USB_PRODUCT_SMSC_SMSC9500A_SAL10	0x9902		/* SMSC9500A Ethernet device (SAL10) */
#define	USB_PRODUCT_SMSC_SMSC9505A_SAL10	0x9903		/* SMSC9505A Ethernet device (SAL10) */
#define	USB_PRODUCT_SMSC_SMSC9512_14_SAL10	0x9904		/* SMSC9512/14 Hub & Ethernet Device (SAL10) */
#define	USB_PRODUCT_SMSC_SMSC9500A_HAL	0x9905		/* SMSC9500A Ethernet Device (HAL) */
#define	USB_PRODUCT_SMSC_SMSC9505A_HAL	0x9906		/* SMSC9505A Ethernet Device (HAL) */
#define	USB_PRODUCT_SMSC_SMSC9500_ALT	0x9907		/* SMSC9500 Ethernet Device */
#define	USB_PRODUCT_SMSC_SMSC9500A_ALT	0x9908		/* SMSC9500A Ethernet Device */
#define	USB_PRODUCT_SMSC_SMSC9512_14_ALT	0x9909		/* SMSC9512 Hub & Ethernet Device */
#define	USB_PRODUCT_SMSC_SMSC9500A	0x9e00		/* SMSC9500A Ethernet device */
#define	USB_PRODUCT_SMSC_SMSC9505A	0x9e01		/* SMSC9505A Ethernet device */
#define	USB_PRODUCT_SMSC_LAN89530	0x9e08		/* LAN89530 */
#define	USB_PRODUCT_SMSC_SMSC9512_14	0xec00		/* SMSC9512/9514 USB Hub & Ethernet device */

/* SOHOware products */
#define	USB_PRODUCT_SOHOWARE_NUB100	0x9100		/* 10/100 USB Ethernet */
#define	USB_PRODUCT_SOHOWARE_NUB110	0x9110		/* NUB110 Ethernet */

/* SOLID YEAR products */
#define	USB_PRODUCT_SOLIDYEAR_KEYBOARD	0x2101		/* Solid Year USB keyboard */

/* SONY products */
#define	USB_PRODUCT_SONY_DSC	0x0010		/* DSC cameras */
#define	USB_PRODUCT_SONY_NWMS7	0x0025		/* Memorystick NW-MS7 */
#define	USB_PRODUCT_SONY_DRIVEV2	0x002b		/* Harddrive V2 */
#define	USB_PRODUCT_SONY_MSACUS1	0x002d		/* Memorystick MSAC-US1 */
#define	USB_PRODUCT_SONY_HANDYCAM	0x002e		/* Handycam */
#define	USB_PRODUCT_SONY_MSC	0x0032		/* MSC memory stick slot */
#define	USB_PRODUCT_SONY_CLIE_35	0x0038		/* Sony Clie v3.5 */
#define	USB_PRODUCT_SONY_PS2KEYBOARD	0x005c		/* PlayStation2 keyboard */
#define	USB_PRODUCT_SONY_PS2KEYBOARDHUB	0x005d		/* PlayStation2 keyboard hub */
#define	USB_PRODUCT_SONY_PS2MOUSE	0x0061		/* PlayStation2 mouse */
#define	USB_PRODUCT_SONY_CLIE_40	0x0066		/* Sony Clie v4.0 */
#define	USB_PRODUCT_SONY_MSC_U03	0x0069		/* MSC memory stick slot MSC-U03 */
#define	USB_PRODUCT_SONY_CLIE_40_MS	0x006d		/* Sony Clie v4.0 Memory Stick slot */
#define	USB_PRODUCT_SONY_CLIE_S360	0x0095		/* Sony Clie s360 */
#define	USB_PRODUCT_SONY_CLIE_41_MS	0x0099		/* Sony Clie v4.1 Memory Stick slot */
#define	USB_PRODUCT_SONY_CLIE_41	0x009a		/* Sony Clie v4.1 */
#define	USB_PRODUCT_SONY_CLIE_NX60	0x00da		/* Sony Clie nx60 */
#define	USB_PRODUCT_SONY_PS2EYETOY4	0x0154		/* PlayStation2 EyeToy v154 */
#define	USB_PRODUCT_SONY_PS2EYETOY5	0x0155		/* PlayStation2 EyeToy v155 */
#define	USB_PRODUCT_SONY_CLIE_TJ25	0x0169		/* Sony Clie tj25 */
#define	USB_PRODUCT_SONY_IFU_WLM2	0x0257		/* IFU-WLM2 */
#define	USB_PRODUCT_SONY_PS3CONTROLLER	0x0268		/* Sony PLAYSTATION(R)3 Controller */
#define	USB_PRODUCT_SONY_GPS_CS1	0x0298		/* Sony GPS GPS-CS1 */

/* SOURCENEXT products */
#define	USB_PRODUCT_SOURCENEXT_KEIKAI8_CHG	0x012e		/* KeikaiDenwa 8 with charger */
#define	USB_PRODUCT_SOURCENEXT_KEIKAI8	0x039f		/* KeikaiDenwa 8 */

/* SparkLAN products */
#define	USB_PRODUCT_SPARKLAN_RT2573	0x0004		/* RT2573 */
#define	USB_PRODUCT_SPARKLAN_RT2870_1	0x0006		/* RT2870 */
#define	USB_PRODUCT_SPARKLAN_RT3070	0x0010		/* RT3070 */
#define	USB_PRODUCT_SPARKLAN_RT2870_2	0x0012		/* RT2870 */

/*Springer Design Systems Inc.*/
#define	USB_PRODUCT_SPRINGERDESIGN_TTSMP3PLAYER	0x1111		/* Springer Design TTSMP3Player */

/* Sphairon Access Systems GmbH products */
#define	USB_PRODUCT_SPHAIRON_UB801R	0x0110		/* UB801R */
#define	USB_PRODUCT_SPHAIRON_RTL8187	0x0150		/* RTL8187 */

/* STMicroelectronics products */
#define	USB_PRODUCT_STMICRO_COMMUNICATOR	0x7554		/* USB Communicator */

/* STSN products */
#define	USB_PRODUCT_STSN_STSN0001	0x0001		/* Internet Access Device */

/* Sun Communications products */
#define	USB_PRODUCT_SUNCOMM_MB_ADAPTOR	0x0003		/* Mobile Adaptor */

/* SUN Corporation products */
#define	USB_PRODUCT_SUNTAC_DS96L	0x0003		/* SUNTAC U-Cable type D2 */
#define	USB_PRODUCT_SUNTAC_PS64P1	0x0005		/* SUNTAC U-Cable type P1 */
#define	USB_PRODUCT_SUNTAC_VS10U	0x0009		/* SUNTAC Slipper U */
#define	USB_PRODUCT_SUNTAC_IS96U	0x000a		/* SUNTAC Ir-Trinity */
#define	USB_PRODUCT_SUNTAC_AS64LX	0x000b		/* SUNTAC U-Cable type A3 */
#define	USB_PRODUCT_SUNTAC_AS144L4	0x0011		/* U-Cable type A4 */

/* Sun Microsystems products */
#define	USB_PRODUCT_SUN_KEYBOARD_TYPE_6	0x0005		/* Type 6 USB keyboard */
#define	USB_PRODUCT_SUN_KEYBOARD_TYPE_7	0x00a2		/* Type 7 USB keyboard */
/* XXX The above is a North American PC style keyboard possibly */
#define	USB_PRODUCT_SUN_MOUSE	0x0100		/* Type 6 USB mouse */

/* SUNRISING products */
#define	USB_PRODUCT_SUNRISING_SR9600	0x8101		/* SR9600 Ethernet */
#define	USB_PRODUCT_SUNRISING_QF9700	0x9700		/* QF9700 Ethernet */

/* SuperTop products */
#define	USB_PRODUCT_SUPERTOP_IDEBRIDGE	0x6600		/* SuperTop IDE Bridge */

/* Supra products */
#define	USB_PRODUCT_DIAMOND2_SUPRAEXPRESS56K	0x07da		/* Supra Express 56K modem */
#define	USB_PRODUCT_DIAMOND2_SUPRA2890	0x0b4a		/* SupraMax 2890 56K Modem */
#define	USB_PRODUCT_DIAMOND2_RIO600USB	0x5001		/* Rio 600 USB */
#define	USB_PRODUCT_DIAMOND2_RIO800USB	0x5002		/* Rio 800 USB */
#define	USB_PRODUCT_DIAMOND2_PSAPLAY120	0x5003		/* Nike psa[play 120 */

/* Surecom Technology products */
#define	USB_PRODUCT_SURECOM_EP9001G2A	0x11f2		/* EP-9001-g rev 2a */
#define	USB_PRODUCT_SURECOM_EP9001G	0x11f3		/* EP-9001-g */
#define	USB_PRODUCT_SURECOM_RT2573	0x31f3		/* RT2573 */

/* Susteen products */
#define	USB_PRODUCT_SUSTEEN_DCU10	0x0528		/* USB Cable */

/* Sweex products */
#define	USB_PRODUCT_SWEEX_ZD1211	0x1809		/* ZD1211 */
#define	USB_PRODUCT_SWEEX2_LW153	0x0153		/* LW153 */
#define	USB_PRODUCT_SWEEX2_LW154	0x0154		/* LW154 */
#define	USB_PRODUCT_SWEEX2_LW303	0x0302		/* LW303 */
#define	USB_PRODUCT_SWEEX2_LW313	0x0313		/* LW313 */

/* System TALKS, Inc. */
#define	USB_PRODUCT_SYSTEMTALKS_SGCX2UL	0x1920		/* SGC-X2UL */

/* Tangtop products */
#define	USB_PRODUCT_TANGTOP_USBPS2	0x0001		/* USBPS2 */

/* Targus products */
#define	USB_PRODUCT_TARGUS_PAUM004	0x0201		/* PAUM004 Mouse */

/* Taugagreining products */
#define	USB_PRODUCT_TAUGA_CAMERAMATE	0x0005		/* CameraMate (DPCM_USB) */

/* TDK products */
#define	USB_PRODUCT_TDK_UPA9664	0x0115		/* USB-PDC Adapter UPA9664 */
#define	USB_PRODUCT_TDK_UCA1464	0x0116		/* USB-cdmaOne Adapter UCA1464 */
#define	USB_PRODUCT_TDK_UHA6400	0x0117		/* USB-PHS Adapter UHA6400 */
#define	USB_PRODUCT_TDK_UPA6400	0x0118		/* USB-PHS Adapter UPA6400 */
#define	USB_PRODUCT_TDK_BT_DONGLE	0x0309		/* Bluetooth USB dongle */

/* TEAC products */
#define	USB_PRODUCT_TEAC_FD05PUB	0x0000		/* FD-05PUB floppy */

/* Tekram Technology products */
#define	USB_PRODUCT_TEKRAM_0193	0x1601		/* ALLNET 0193 WLAN */
#define	USB_PRODUCT_TEKRAM_ZYAIR_B200	0x1602		/* ZyXEL ZyAIR B200 WLAN */
#define	USB_PRODUCT_TEKRAM_U300C	0x1612		/* U-300C */
#define	USB_PRODUCT_TEKRAM_QUICKWLAN	0x1630		/* QuickWLAN */
#define	USB_PRODUCT_TEKRAM_ZD1211_1	0x5630		/* ZD1211 */
#define	USB_PRODUCT_TEKRAM_ZD1211_2	0x6630		/* ZD1211 */

/* Telex Communications products */
#define	USB_PRODUCT_TELEX_MIC1	0x0001		/* Enhanced USB Microphone */

/* Ten X Technology, Inc. */
#define	USB_PRODUCT_TENX_MISSILE	0x0202		/* Missile Launcher */
#define	USB_PRODUCT_TENX_TEMPER	0x660c		/* TEMPer sensor */

/* Texas Instruments products */
#define	USB_PRODUCT_TI_UTUSB41	0x1446		/* UT-USB41 hub */
#define	USB_PRODUCT_TI_TUSB2046	0x2046		/* TUSB2046 hub */
#define	USB_PRODUCT_TI_TUSB3410	0x3410		/* TUSB3410 */
#define	USB_PRODUCT_TI_NEXII	0x5409		/* Nex II Digital */
#define	USB_PRODUCT_TI_MSP430_JTAG	0xf430		/* MSP-FET430UIF JTAG */
#define	USB_PRODUCT_TI_MSP430	0xf432		/* MSP-FET430UIF */

/* Thrustmaster products */
#define	USB_PRODUCT_THRUST_FUSION_PAD	0xa0a3		/* Fusion Digital Gamepad */

/* TOD Co. Ltd products */
#define	USB_PRODUCT_TOD_DOOGI_SLIM	0x0411		/* DOOGI SLIM USB Keyboard */

/* Todos Data System products */
#define	USB_PRODUCT_TODOS_ARGOS_MINI	0x0002		/* Argos Mini Smartcard Reader */

/* Topfield Co. Ltd products */
#define	USB_PRODUCT_TOPFIELD_TF5000PVR	0x1000		/* TF5000PVR Digital Video Recorder */

/* Toshiba Corporation products */
#define	USB_PRODUCT_TOSHIBA_POCKETPC_E740	0x0706		/* PocketPC e740 */
#define	USB_PRODUCT_TOSHIBA_RT3070	0x0a07		/* RT3070 */
#define	USB_PRODUCT_TOSHIBA_AX88179	0x0a13		/* AX88179 USB 3.0 gigabit ethernet controller */
#define	USB_PRODUCT_TOSHIBA_HSDPA_MODEM_EU870DT1	0x1302		/* HSDPA 3G Modem Card */

/* TP-Link products */
#define	USB_PRODUCT_TPLINK_RTL8192CU	0x0100		/* RTL8192CU */

/* Trek Technology products */
#define	USB_PRODUCT_TREK_THUMBDRIVE	0x1111		/* ThumbDrive */
#define	USB_PRODUCT_TREK_THUMBDRIVE_8MB	0x9988		/* ThumbDrive 8MB */

/* TRENDnet products */
#define	USB_PRODUCT_TRENDNET_RTL8192CU	0x624d		/* RTL8192CU */
#define	USB_PRODUCT_TRENDNET_RTL8188CU	0x648b		/* RTL8188CU */

/* Tripp-Lite products */
#define	USB_PRODUCT_TRIPPLITE_U209	0x2008		/* U209 Serial adapter */
#define	USB_PRODUCT_TRIPPLITE2_UPS	0x1007		/* Tripp Lite UPS */
#define	USB_PRODUCT_TRIPPLITE2_SMARTLCD	0x2009		/* SmartLCD UPS */
#define	USB_PRODUCT_TRIPPLITE2_AVR550U	0x2010		/* Tripp Lite AVR550U */

/* Trumpion products */
#define	USB_PRODUCT_TRUMPION_T33521	0x1003		/* USB/MP3 decoder */
#define	USB_PRODUCT_TRUMPION_XXX1100	0x1100		/* XXX 1100 */

/* Tsunami products */
#define	USB_PRODUCT_TSUNAMI_SM2000	0x1111		/* SM-2000 */

/* TwinMOS products */
#define	USB_PRODUCT_TWINMOS_G240	0xa006		/* G240 */

/* Ultima products */
#define	USB_PRODUCT_ULTIMA_1200UBPLUS	0x4002		/* 1200 UB Plus scanner */
#define	USB_PRODUCT_ULTIMA_T14BR	0x810f		/* Artec T14BR DVB-T */

/* UMAX products */
#define	USB_PRODUCT_UMAX_ASTRA1236U	0x0002		/* Astra 1236U Scanner */
#define	USB_PRODUCT_UMAX_ASTRA1220U	0x0010		/* Astra 1220U Scanner */
#define	USB_PRODUCT_UMAX_ASTRA2000U	0x0030		/* Astra 2000U Scanner */
#define	USB_PRODUCT_UMAX_ASTRA3400	0x0060		/* Astra 3400 Scanner */
#define	USB_PRODUCT_UMAX_ASTRA2100U	0x0130		/* Astra 2100U Scanner */
#define	USB_PRODUCT_UMAX_ASTRA2200U	0x0230		/* Astra 2200U Scanner */

/* U-MEDIA Communications products */
#define	USB_PRODUCT_UMEDIA_TEW429UB_A	0x300a		/* TEW-429UB_A */
#define	USB_PRODUCT_UMEDIA_TEW429UB	0x300b		/* TEW-429UB */
#define	USB_PRODUCT_UMEDIA_TEW429UBC1	0x300d		/* TEW-429UB C1 */
#define	USB_PRODUCT_UMEDIA_RT2870_1	0x300e		/* RT2870 */
#define	USB_PRODUCT_UMEDIA_TEW645UB	0x3013		/* TEW-645UB */
#define	USB_PRODUCT_UMEDIA_ALL0298V2	0x3204		/* ALL0298 v2 */

/* Universal Access products */
#define	USB_PRODUCT_UNIACCESS_PANACHE	0x0101		/* Panache Surf USB ISDN Adapter */

/* Unknown vendor 1 */
#define	USB_PRODUCT_UNKNOWN1_ZD1211B_1	0x5301		/* ZD1211B */
#define	USB_PRODUCT_UNKNOWN1_ZD1211B_2	0x5301		/* ZD1211B */

/* Unknown vendor 2 */
#define	USB_PRODUCT_UNKNOWN2_ZD1211B	0x0105		/* ZD1211B */
#define	USB_PRODUCT_UNKNOWN2_NW3100	0x145f		/* NW-3100 */

/* Unknown vendor 3 */
#define	USB_PRODUCT_UNKNOWN3_ZD1211B	0x1233		/* ZD1211B */

/* Unknown vendor 5 */
#define	USB_PRODUCT_UNKNOWN5_NF_RIC	0x0001		/* NF RIC */

/* U.S. Robotics products */
#define	USB_PRODUCT_USR_USR1120	0x00eb		/* USR1120 WLAN */
#define	USB_PRODUCT_USR_USR5422	0x0118		/* USR5422 WLAN */
#define	USB_PRODUCT_USR_USR5423	0x0121		/* USR5423 WLAN */

/* USI products */
#define	USB_PRODUCT_USI_MC60	0x10c5		/* MC60 Serial */

/* GNU Radio USRP */
#define	USB_PRODUCT_USRP_USRPv2	0x0002		/* USRP Revision 2 */

/* Validity */
#define	USB_PRODUCT_VALIDITY_VFS101	0x0001		/* VFS101 Fingerprint Reader */
#define	USB_PRODUCT_VALIDITY_VFS301	0x0005		/* VFS301 Fingerprint Reader */
#define	USB_PRODUCT_VALIDITY_VFS451	0x0007		/* VFS451 Fingerprint Reader */
#define	USB_PRODUCT_VALIDITY_VFS300	0x0008		/* VFS300 Fingerprint Reader */
#define	USB_PRODUCT_VALIDITY_VFS5011	0x0011		/* VFS5011 Fingerprint Reader */
#define	USB_PRODUCT_VALIDITY_VFS471	0x003c		/* VFS471 Fingerprint Reader */

/* VidzMedia products */
#define	USB_PRODUCT_VIDZMEDIA_MONSTERTV	0x4fb1		/* MonsterTV P2H */

/* VIA products */
#define	USB_PRODUCT_VIA_AR9271	0x3801		/* AR9271 */

/* ViewSonic products */
#define	USB_PRODUCT_VIEWSONIC_G773HUB	0x00fe		/* G773 Monitor Hub */
#define	USB_PRODUCT_VIEWSONIC_P815HUB	0x00ff		/* P815 Monitor Hub */
#define	USB_PRODUCT_VIEWSONIC_G773CTRL	0x4153		/* G773 Monitor Control */

/* Vision products */
#define	USB_PRODUCT_VISION_VC6452V002	0x0002		/* CPiA Camera */

/* Visioneer products */
#define	USB_PRODUCT_VISIONEER_7600	0x0211		/* OneTouch 7600 */
#define	USB_PRODUCT_VISIONEER_5300	0x0221		/* OneTouch 5300 */
#define	USB_PRODUCT_VISIONEER_3000	0x0224		/* Scanport 3000 */
#define	USB_PRODUCT_VISIONEER_6100	0x0231		/* OneTouch 6100 */
#define	USB_PRODUCT_VISIONEER_6200	0x0311		/* OneTouch 6200 */
#define	USB_PRODUCT_VISIONEER_8100	0x0321		/* OneTouch 8100 */
#define	USB_PRODUCT_VISIONEER_8600	0x0331		/* OneTouch 8600 */

/* Vivitar products */
#define	USB_PRODUCT_VIVITAR_DSC350	0x0003		/* DSC350 Camera */

/* VTech products */
#define	USB_PRODUCT_VTECH_RT2570	0x3012		/* RT2570 */
#define	USB_PRODUCT_VTECH_ZD1211B	0x3014		/* ZD1211B */

/* Wacom products */
#define	USB_PRODUCT_WACOM_CT0405U	0x0000		/* CT-0405-U Tablet */
#define	USB_PRODUCT_WACOM_GRAPHIRE	0x0010		/* Graphire */
#define	USB_PRODUCT_WACOM_GRAPHIRE2	0x0011		/* Graphire2 ET-0405A-U */
#define	USB_PRODUCT_WACOM_GRAPHIRE3_4X5	0x0013		/* Graphire3 4x5 */
#define	USB_PRODUCT_WACOM_GRAPHIRE3_6X8	0x0014		/* Graphire3 6x8 */
#define	USB_PRODUCT_WACOM_GRAPHIRE4_4X5	0x0015		/* Graphire4 4x5 */
#define	USB_PRODUCT_WACOM_INTUOSA5	0x0021		/* Intuos A5 */
#define	USB_PRODUCT_WACOM_GD0912U	0x0022		/* Intuos 9x12 Graphics Tablet */

/* Weltrend Semiconductor */
#define	USB_PRODUCT_WELTREND_HID	0x2201		/* HID Device */

/* Western Digital products */
#define	USB_PRODUCT_WESTERN_EXTHDD	0x0400		/* External HDD */

/* WinChipHead products */
#define	USB_PRODUCT_WINCHIPHEAD_CH341SER	0x5523		/* CH341/CH340 USB-Serial Bridge */
#define	USB_PRODUCT_WINCHIPHEAD2_CH341	0x7523		/* CH341 serial/parallel */

/* Wistron NeWeb products */
#define	USB_PRODUCT_WISTRONNEWEB_WNC0600	0x0326		/* WNC-0600USB */
#define	USB_PRODUCT_WISTRONNEWEB_UR045G	0x0427		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_WISTRONNEWEB_UR055G	0x0711		/* UR055G */
#define	USB_PRODUCT_WISTRONNEWEB_O8494	0x0804		/* ORiNOCO 802.11n */
#define	USB_PRODUCT_WISTRONNEWEB_AR5523_1	0x0826		/* AR5523 */
#define	USB_PRODUCT_WISTRONNEWEB_AR5523_1_NF	0x0827		/* AR5523 */
#define	USB_PRODUCT_WISTRONNEWEB_AR5523_2	0x082a		/* AR5523 */
#define	USB_PRODUCT_WISTRONNEWEB_AR5523_2_NF	0x0829		/* AR5523 */

/* Xirlink products */
#define	USB_PRODUCT_XIRLINK_IMAGING	0x800d		/* IMAGING DEVICE */
#define	USB_PRODUCT_XIRLINK_PCCAM	0x8080		/* IBM PC Camera */

/* Conexant */
#define	USB_PRODUCT_CONEXANT_MODEM_1	0x1329		/* USB Modem */
#define	USB_PRODUCT_CONEXANT_PRISM_GT_1	0x2000		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_CONEXANT_PRISM_GT_2	0x2002		/* PrismGT USB 2.0 WLAN */

/* Yamaha products */
#define	USB_PRODUCT_YAMAHA_UX256	0x1000		/* UX256 MIDI I/F */
#define	USB_PRODUCT_YAMAHA_MU1000	0x1001		/* MU1000 MIDI Synth. */
#define	USB_PRODUCT_YAMAHA_MU2000	0x1002		/* MU2000 MIDI Synth. */
#define	USB_PRODUCT_YAMAHA_MU500	0x1003		/* MU500 MIDI Synth. */
#define	USB_PRODUCT_YAMAHA_UW500	0x1004		/* UW500 USB Audio I/F */
#define	USB_PRODUCT_YAMAHA_MOTIF6	0x1005		/* MOTIF6 MIDI Synth. Workstation */
#define	USB_PRODUCT_YAMAHA_MOTIF7	0x1006		/* MOTIF7 MIDI Synth. Workstation */
#define	USB_PRODUCT_YAMAHA_MOTIF8	0x1007		/* MOTIF8 MIDI Synth. Workstation */
#define	USB_PRODUCT_YAMAHA_UX96	0x1008		/* UX96 MIDI I/F */
#define	USB_PRODUCT_YAMAHA_UX16	0x1009		/* UX16 MIDI I/F */
#define	USB_PRODUCT_YAMAHA_S08	0x100e		/* S08 MIDI Keyboard */
#define	USB_PRODUCT_YAMAHA_CLP150	0x100f		/* CLP-150 digital piano */
#define	USB_PRODUCT_YAMAHA_CLP170	0x1010		/* CLP-170 digital piano */
#define	USB_PRODUCT_YAMAHA_RPU200	0x3104		/* RP-U200 */
#define	USB_PRODUCT_YAMAHA_RTA54I	0x4000		/* NetVolante RTA54i Broadband&ISDN Router */
#define	USB_PRODUCT_YAMAHA_RTW65B	0x4001		/* NetVolante RTW65b Broadband Wireless Router */
#define	USB_PRODUCT_YAMAHA_RTW65I	0x4002		/* NetVolante RTW65i Broadband&ISDN Wireless Router */
#define	USB_PRODUCT_YAMAHA_RTA55I	0x4004		/* NetVolante RTA55i Broadband VoIP Router */

/* Yano products */
#define	USB_PRODUCT_YANO_U640MO	0x0101		/* U640MO-03 */

/* Y-E Data products */
#define	USB_PRODUCT_YEDATA_FLASHBUSTERU	0x0000		/* Flashbuster-U */

/* Z-Com products */
#define	USB_PRODUCT_ZCOM_M4Y750	0x0001		/* M4Y-750 */
#define	USB_PRODUCT_ZCOM_725	0x0002		/* 725/726 Prism2.5 WLAN */
#define	USB_PRODUCT_ZCOM_XI735	0x0005		/* XI-735 */
#define	USB_PRODUCT_ZCOM_MD40900	0x0006		/* MD40900 */
#define	USB_PRODUCT_ZCOM_XG703A	0x0008		/* PrismGT USB 2.0 WLAN */
#define	USB_PRODUCT_ZCOM_ZD1211	0x0011		/* ZD1211 */
#define	USB_PRODUCT_ZCOM_AR5523	0x0012		/* AR5523 */
#define	USB_PRODUCT_ZCOM_AR5523_NF	0x0013		/* AR5523 */
#define	USB_PRODUCT_ZCOM_ZD1211B	0x001a		/* ZD1211B */
#define	USB_PRODUCT_ZCOM_RT2870_1	0x0022		/* RT2870 */
#define	USB_PRODUCT_ZCOM_UB81	0x0023		/* UB81 */
#define	USB_PRODUCT_ZCOM_RT2870_2	0x0025		/* RT2870 */
#define	USB_PRODUCT_ZCOM_UB82	0x0026		/* UB82 */

/* Zeevo, Inc. products */
#define	USB_PRODUCT_ZEEVO_BLUETOOTH	0x07d0		/* BT-500 Bluetooth USB Adapter */

/* Zinwell products */
#define	USB_PRODUCT_ZINWELL_ZWXG261	0x0260		/* ZWX-G261 */
#define	USB_PRODUCT_ZINWELL_RT2870_1	0x0280		/* RT2870 */
#define	USB_PRODUCT_ZINWELL_RT2870_2	0x0282		/* RT2870 */
#define	USB_PRODUCT_ZINWELL_RT3072	0x0283		/* RT3072 */
#define	USB_PRODUCT_ZINWELL_RT3072_2	0x0284		/* RT3072 */
#define	USB_PRODUCT_ZINWELL_RT3070	0x5257		/* RT3070 */

/* Zoom Telephonics, Inc. products */
#define	USB_PRODUCT_ZOOM_2986L	0x9700		/* 2986L Fax modem */
#define	USB_PRODUCT_ZOOM_3095	0x3095		/* 3095 USB Fax modem */

/* ZTE products */
#define	USB_PRODUCT_ZTE_MF622	0x0001		/* MF622 modem */
#define	USB_PRODUCT_ZTE_MF628	0x0015		/* MF628 modem */
#define	USB_PRODUCT_ZTE_MF626	0x0031		/* MF626 modem */
#define	USB_PRODUCT_ZTE_MF820D_INSTALLER	0x0166		/* MF820D CD */
#define	USB_PRODUCT_ZTE_MF820D	0x0167		/* MF820D modem */
#define	USB_PRODUCT_ZTE_INSTALLER	0x2000		/* UMTS CD */
#define	USB_PRODUCT_ZTE_MC2718	0xffe8		/* MC2718 modem */
#define	USB_PRODUCT_ZTE_AC8700	0xfffe		/* CDMA 1xEVDO USB modem */

/* Zydas Technology Corporation products */
#define	USB_PRODUCT_ZYDAS_ZD1201	0x1201		/* ZD1201 */
#define	USB_PRODUCT_ZYDAS_ZD1211	0x1211		/* ZD1211 WLAN abg */
#define	USB_PRODUCT_ZYDAS_ZD1211B	0x1215		/* ZD1211B */
#define	USB_PRODUCT_ZYDAS_ZD1221	0x1221		/* ZD1221 */
#define	USB_PRODUCT_ZYDAS_ALL0298	0xa211		/* ALL0298 */
#define	USB_PRODUCT_ZYDAS_ZD1211B_2	0xb215		/* ZD1211B */

/* ZyXEL Communication Co. products */
#define	USB_PRODUCT_ZYXEL_OMNI56K	0x1500		/* Omni 56K Plus */
#define	USB_PRODUCT_ZYXEL_980N	0x2011		/* Scorpion-980N keyboard */
#define	USB_PRODUCT_ZYXEL_ZYAIRG220	0x3401		/* ZyAIR G-220 */
#define	USB_PRODUCT_ZYXEL_G200V2	0x3407		/* G-200 v2 */
#define	USB_PRODUCT_ZYXEL_AG225H	0x3409		/* AG-225H */
#define	USB_PRODUCT_ZYXEL_M202	0x340a		/* M-202 */
#define	USB_PRODUCT_ZYXEL_G270S	0x340c		/* G-270S */
#define	USB_PRODUCT_ZYXEL_G220V2	0x340f		/* G-220 v2 */
#define	USB_PRODUCT_ZYXEL_G202	0x3410		/* G-202 */
#define	USB_PRODUCT_ZYXEL_RT2573	0x3415		/* RT2573 */
#define	USB_PRODUCT_ZYXEL_RT2870_1	0x3416		/* RT2870 */
#define	USB_PRODUCT_ZYXEL_NWD271N	0x3417		/* NWD-271N */
#define	USB_PRODUCT_ZYXEL_NWD211AN	0x3418		/* NWD-211AN */
#define	USB_PRODUCT_ZYXEL_RT2870_2	0x341a		/* RT2870 */
#define	USB_PRODUCT_ZYXEL_NWD2105	0x341e		/* NWD2105 */
#define	USB_PRODUCT_ZYXEL_RTL8192CU	0x341f		/* RTL8192CU */
#define	USB_PRODUCT_ZYXEL_RT3070	0x343e		/* RT3070 */
#define	USB_PRODUCT_ZYXEL_PRESTIGE	0x401a		/* Prestige */
