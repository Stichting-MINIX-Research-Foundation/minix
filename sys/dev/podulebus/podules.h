/*	$NetBSD: podules.h,v 1.16 2005/12/11 12:23:28 christos Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: podules,v 1.16 2004/01/07 22:00:51 bjh21 Exp
 */

/*
 * Copyright (c) 1996 Mark Brinicombe
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * List of known podule manufacturers
 */

#define	MANUFACTURER_ACORN	0x0000		/* Acorn Computers */
#define	MANUFACTURER_ACORNUSA	0x0001		/* Acorn Computers (USA) */
#define	MANUFACTURER_OLIVETTI	0x0002		/* Olivetti */
#define	MANUFACTURER_WATFORD	0x0003		/* Watford Electronics */
#define	MANUFACTURER_CCONCEPTS	0x0004		/* Computer Concepts */
#define	MANUFACTURER_IINTERFACES	0x0005		/* Intelligent Interfaces */
#define	MANUFACTURER_CAMAN	0x0006		/* Caman */
#define	MANUFACTURER_ARMADILLO	0x0007		/* Armadillo Systems */
#define	MANUFACTURER_SOFTOPTION	0x0008		/* Soft Option */
#define	MANUFACTURER_WILDVISION	0x0009		/* Wild Vision */
#define	MANUFACTURER_ANGLOCOMPUTERS	0x000a		/* Anglo Computers */
#define	MANUFACTURER_RESOURCE	0x000b		/* Resource */
/* RISC iX: #define XCB_COMPANY_ALLIEDINTERACTIVE 12 */
#define	MANUFACTURER_HCCS	0x000c		/* HCCS */
#define	MANUFACTURER_MUSBURYCONSULT	0x000d		/* Musbury Consultants */
#define	MANUFACTURER_GNOME	0x000e		/* Gnome */
#define	MANUFACTURER_AANDGELEC	0x000f		/* A and G Electronics */
#define	MANUFACTURER_SPACETECH	0x0010		/* Spacetech */
#define	MANUFACTURER_ATOMWIDE	0x0011		/* Atomwide */
#define	MANUFACTURER_SYNTEC	0x0012		/* Syntec */
#define	MANUFACTURER_EMR	0x0013		/* ElectroMusic Research */
#define	MANUFACTURER_MILLIPEDE	0x0014		/* Millipede */
#define	MANUFACTURER_VIDEOELEC	0x0015		/* Video Electronics */
#define	MANUFACTURER_BRAINSOFT	0x0016		/* Brainsoft */
/* RISC iX: #define XCB_COMPANY_ASP 23 */
#define	MANUFACTURER_ATOMWIDE2	0x0017		/* Atomwide */
#define	MANUFACTURER_LENDAC	0x0018		/* Lendac Data Systems */
#define	MANUFACTURER_CAMMICROSYS	0x0019		/* Cambridge Micro Systems */
/* RISC iX: #define XCB_COMPANY_JOHNBALANCECOMPUTING 26 */
#define	MANUFACTURER_LINGENUITY	0x001a		/* Lingenuity */
#define	MANUFACTURER_SIPLAN	0x001b		/* Siplan Electronics Research */
#define	MANUFACTURER_SCIFRONTIERS	0x001c		/* Science Frontiers */
#define	MANUFACTURER_PINEAPPLE	0x001d		/* Pineapple Software */
#define	MANUFACTURER_TECHNOMATIC	0x001e		/* Technomatic */
#define	MANUFACTURER_IRLAM	0x001f		/* Irlam Instruments */
#define	MANUFACTURER_NEXUS	0x0020		/* Nexus Electronics */
#define	MANUFACTURER_OAK	0x0021		/* Oak Solutions */
#define	MANUFACTURER_HUGHSYMONS	0x0022		/* Hugh Symons */
#define	MANUFACTURER_BEEBUG	0x0023		/* BEEBUG (RISC Developments) */
#define	MANUFACTURER_TEKNOMUSIK	0x0024		/* Teknomusik */
#define	MANUFACTURER_REELTIME	0x0025		/* Reel Time */
#define	MANUFACTURER_PRES	0x0026		/* PRES */
#define	MANUFACTURER_DIGIHURST	0x0027		/* Digihurst */
#define	MANUFACTURER_SGBCOMPSERV	0x0028		/* SGB Computer Services */
#define	MANUFACTURER_SJ	0x0029		/* SJ Research */
#define	MANUFACTURER_PHOBOX	0x002a		/* Phobox Electronics */
#define	MANUFACTURER_MORLEY	0x002b		/* Morley Electronics */
#define	MANUFACTURER_RACINGCAR	0x002c		/* Raching Car Computers */
#define	MANUFACTURER_HCCS2	0x002d		/* HCCS */
#define	MANUFACTURER_LINDIS	0x002e		/* Lindis International */
#define	MANUFACTURER_CCC	0x002f		/* Computer Control Consultants */
#define	MANUFACTURER_UNILAB	0x0030		/* Unilab */
#define	MANUFACTURER_SEFANFROHLING	0x0031		/* Sefan Frohling */
#define	MANUFACTURER_ROMBO	0x0032		/* Rombo Productions */
#define	MANUFACTURER_3SL	0x0033		/* 3SL */
#define	MANUFACTURER_DELTRONICS	0x0034		/* Deltronics */
/* RISC iX: #define XCB_COMPANY_PCARNOLDTECHNICALSERVICES 53 */
#define	MANUFACTURER_VTI	0x0035		/* Vertical Twist */
#define	MANUFACTURER_SIMIS	0x0036		/* Simis */
#define	MANUFACTURER_DTSOFT	0x0037		/* D.T. Software */
#define	MANUFACTURER_ARMINTERFACES	0x0038		/* ARM Interfaces */
#define	MANUFACTURER_BIA	0x0039		/* BIA */
#define	MANUFACTURER_CUMANA	0x003a		/* Cumana */
#define	MANUFACTURER_IOTA	0x003b		/* Iota */
#define	MANUFACTURER_ICS	0x003c		/* Ian Copestake Software */
#define	MANUFACTURER_BAILDON	0x003d		/* Baildon Electronics */
#define	MANUFACTURER_CSD	0x003e		/* CSD */
#define	MANUFACTURER_SERIALPORT	0x003f		/* Serial Port */
#define	MANUFACTURER_CADSOFT	0x0040		/* CADsoft */
#define	MANUFACTURER_ARXE	0x0041		/* ARXE */
#define	MANUFACTURER_ALEPH1	0x0042		/* Aleph 1 */
#define	MANUFACTURER_ICUBED	0x0046		/* I-Cubed */
#define	MANUFACTURER_BRINI	0x0050		/* Brini */
#define	MANUFACTURER_ANT	0x0053		/* ANT */
#define	MANUFACTURER_CASTLE	0x0055		/* Castle Technology */
#define	MANUFACTURER_ALSYSTEMS	0x005b		/* Alsystems */
#define	MANUFACTURER_SIMTEC	0x005f		/* Simtec Electronics */
#define	MANUFACTURER_YES	0x0060		/* Yellowstone Educational Solutions */
#define	MANUFACTURER_MCS	0x0063		/* MCS */
#define	MANUFACTURER_EESOX	0x0064		/* EESOX */

/*
 * List of known podules.
 */

#define	PODULE_HOSTTUBE	0x0000		/* Host TUBE (to BBC) */
#define	PODULE_PARASITETUBE	0x0001		/* Parastite TUBE (to 2nd processor) */
#define	PODULE_ACORN_SCSI	0x0002		/* Acorn SCSI interface */
#define	PODULE_ETHER1	0x0003		/* Ether1 interface */
#define	PODULE_IBMDISC	0x0004		/* IBM disc */
#define	PODULE_ROMRAM	0x0005		/* ROM/RAM podule */
#define	PODULE_BBCIO	0x0006		/* BBC I/O podule */
#define	PODULE_FAXPACK	0x0007		/* FaxPack modem */
#define	PODULE_TELETEXT	0x0008		/* Teletext */
#define	PODULE_CDROM	0x0009		/* CD-ROM */
#define	PODULE_IEEE488	0x000a		/* IEEE 488 interface */
#define	PODULE_ST506	0x000b		/* ST506 HD interface */
#define	PODULE_ESDI	0x000c		/* ESDI interface */
#define	PODULE_SMD	0x000d		/* SMD interface */
#define	PODULE_LASERPRINTER	0x000e		/* laser printer */
#define	PODULE_SCANNER	0x000f		/* scanner */
#define	PODULE_FASTRING	0x0010		/* Fast Ring interface */
#define	PODULE_FASTRING2	0x0011		/* Fast Ring II interface */
#define	PODULE_PROMPROGRAMMER	0x0012		/* PROM programmer */
#define	PODULE_ACORN_MIDI	0x0013		/* MIDI interface */
/* RISC iX: #define XCB_PRODUCT_MONOVPU 20 */
#define	PODULE_LASERDIRECT	0x0014		/* LaserDirect (Canon LBP-4) */
#define	PODULE_FRAMEGRABBER	0x0015		/* frame grabber */
#define	PODULE_A448	0x0016		/* A448 sound sampler */
#define	PODULE_VIDEODIGITISER	0x0017		/* video digitiser */
#define	PODULE_GENLOCK	0x0018		/* genlock */
#define	PODULE_CODECSAMPLER	0x0019		/* codec sampler */
#define	PODULE_IMAGEANALYSER	0x001a		/* image analyser */
#define	PODULE_ANALOGUEINPUT	0x001b		/* analogue input */
#define	PODULE_CDSOUNDSAMPLER	0x001c		/* CD sound sampler */
#define	PODULE_6MIPSSIGPROC	0x001d		/* 6 MIPS signal processor */
#define	PODULE_12MIPSSIGPROC	0x001e		/* 12 MIPS signal processor */
#define	PODULE_33MIPSSIGPROC	0x001f		/* 33 MIPS signal processor */
#define	PODULE_TOUCHSCREEN	0x0020		/* touch screen */
#define	PODULE_TRANSPUTERLINK	0x0021		/* Transputer link */
/* RISC iX: #define XCB_PRODUCT_INTERACTIVEVIDEO 34 */
#define	PODULE_HCCS_IDESCSI	0x0022		/* HCCS IDE or SCSI interface */
#define	PODULE_LASERSCANNER	0x0023		/* laser scanner */
#define	PODULE_GNOME_TRANSPUTERLINK	0x0024		/* Transputer link */
#define	PODULE_VMEBUS	0x0025		/* VME bus interface */
#define	PODULE_TAPESTREAMER	0x0026		/* tape streamer */
#define	PODULE_LASERTEST	0x0027		/* laser test */
#define	PODULE_COLOURDIGITISER	0x0028		/* colour digitiser */
#define	PODULE_WEATHERSATELLITE	0x0029		/* weather satellite */
#define	PODULE_AUTOCUE	0x002a		/* autocue */
#define	PODULE_PARALLELIO16BIT	0x002b		/* 16-bit parallel I/O */
#define	PODULE_12BITATOD	0x002c		/* 12-bit ADC */
#define	PODULE_SERIALPORTSRS423	0x002d		/* RS423 serial ports */
#define	PODULE_MINI	0x002e		/* mini */
#define	PODULE_FRAMEGRABBER2	0x002f		/* frame grabber II */
#define	PODULE_INTERACTIVEVIDEO2	0x0030		/* interactive video II */
#define	PODULE_WILDVISION_ATOD	0x0031		/* ADC */
#define	PODULE_WILDVISION_DTOA	0x0032		/* DAC */
#define	PODULE_EMR_MIDI4	0x0033		/* MIDI 4 */
#define	PODULE_FPCP	0x0034		/* floating-point co-processor */
#define	PODULE_PRISMA3	0x0035		/* Prisma 3 */
#define	PODULE_ARVIS	0x0036		/* ARVIS */
#define	PODULE_4BY4MIDI	0x0037		/* 4x4 MIDI */
#define	PODULE_BISERIALPARALLEL	0x0038		/* Bi-directional serial/parallel */
#define	PODULE_CHROMA300	0x0039		/* Chroma 300 genlock */
/* RISC iX: #define XCB_PRODUCT_CHROMA400GENLOCK 58 */
#define	PODULE_CUMANA_SCSI2	0x003a		/* SCSI II interface */
#define	PODULE_COLOURCONVERTER	0x003b		/* Colour Converter */
#define	PODULE_8BITSAMPLER	0x003c		/* 8-bit sampler */
#define	PODULE_PLUTO	0x003d		/* Pluto interface */
#define	PODULE_LOGICANALYSER	0x003e		/* Logic Analyser */
#define	PODULE_ACORN_USERMIDI	0x003f		/* User Port/MIDI interface */
#define	PODULE_LINGENUITY_SCSI8	0x0040		/* 8 bit SCSI interface */
/* RISC iX: #define XCB_PRODUCT_SIPLANADCANDDAC 65 */
#define	PODULE_ARXE_SCSI	0x0041		/* 16 bit SCSI interface */
#define	PODULE_DUALUSERPORT	0x0042		/* dual User Port */
#define	PODULE_EMR_SAMPLER8	0x0043		/* Sampler8 */
#define	PODULE_EMR_SMTP	0x0044		/* SMTP */
#define	PODULE_EMR_MIDI2	0x0045		/* MIDI2 */
#define	PODULE_PINEAPPLE_DIGITISER	0x0046		/* digitiser */
#define	PODULE_VIDEOFRAMECAPTURE	0x0047		/* video frame capture */
#define	PODULE_MONOOVERLAYFRSTORE	0x0048		/* mono overlay frame store */
#define	PODULE_MARKETBUFFER	0x0049		/* market buffer */
#define	PODULE_PAGESTORE	0x004a		/* page store */
#define	PODULE_TRAMMOTHERBOARD	0x004b		/* TRAM motherboard */
#define	PODULE_TRANSPUTER	0x004c		/* Transputer */
#define	PODULE_OPTICALSCANNER	0x004d		/* optical scanner */
#define	PODULE_DIGITISINGTABLET	0x004e		/* digitising tablet */
#define	PODULE_200DPISCANNER	0x004f		/* 200-dpi scanner */
/* RISC iX: #define XCB_PRODUCT_DIGITALIO 80 */
#define	PODULE_COLOURCARD	0x0050		/* ColourCard */
#define	PODULE_PRESENTERGENLOCK	0x0051		/* Presenter Genlock */
#define	PODULE_HAWKV9	0x0052		/* Hawk v9 mark2 */
#define	PODULE_CROMA200	0x0053		/* Chroma 200 genlock */
#define	PODULE_WILDVISION_SOUNDSAMPLER	0x0054		/* Wild Vision Sound Sampler */
/* RISC iX: #define XCB_PRODUCT_SMTPEINTERFACE 85 */
#define	PODULE_DTSOFT_IDE	0x0055		/* IDE interface */
#define	PODULE_8BITATOD	0x0056		/* 8-bit ADC */
#define	PODULE_MFMHDCONTROLLER	0x0057		/* MFM hard disc controller */
/* XXX ID 0x0058 is used by Oak ClassNet (EtherO) Ethernet cards */
#define	PODULE_OAK_SCSI	0x0058		/* 16 bit SCSI interface */
#define	PODULE_QUADSERIAL	0x0059		/* quad serial */
#define	PODULE_PALPROGRAMMER	0x005a		/* PAL programmer */
#define	PODULE_I2CBUS	0x005b		/* I^2C bus */
#define	PODULE_BEEBUG_SCANNER	0x005c		/* scanner interface */
#define	PODULE_PANDORA_QUADMIDI	0x005d		/* quad MIDI */
#define	PODULE_PRES_DISCBUFFER	0x005e		/* disc buffer */
#define	PODULE_PRES_USERPORT	0x005f		/* User Port */
#define	PODULE_MICROYEAI	0x0060		/* Micro YEAI */
#define	PODULE_ETHER2	0x0061		/* Ether2 interface */
#define	PODULE_SGB_EXPANSIONBOX	0x0062		/* SGB expansion box */
/* RISC iX: #define XCB_PRODUCT_SGBFASTPORT 99 */
#define	PODULE_ULTIMATE	0x0063		/* Ultimate micropodule carrier */
#define	PODULE_NEXUS	0x0064		/* Nexus interface (Podule) */
#define	PODULE_PHOBOX_USERANALOGUE	0x0065		/* User and Analogue ports */
#define	PODULE_MORLEY_STATICRAM	0x0066		/* static RAM */
#define	PODULE_MORLEY_SCSI	0x0067		/* SCSI interface */
#define	PODULE_MORLEY_TELETEXT	0x0068		/* teletext interface */
#define	PODULE_TECHNOMATIC_SCANNER	0x0069		/* scanner */
#define	PODULE_BEEBUG_QUADRANT	0x006a		/* Quadrant */
#define	PODULE_RCC_VOICEPROCESSOR	0x006b		/* voice processor */
#define	PODULE_RCC_UHFLINK	0x006c		/* UHF link */
#define	PODULE_MORLEY_USERANALOGUE	0x006d		/* User and Analogue ports */
#define	PODULE_HCCS_USERANALOGUE	0x006e		/* User and Analogue ports */
#define	PODULE_WILDVISION_CENTRONICS	0x006f		/* Bi-directional Centronics */
#define	PODULE_HCCS_A3000SCSI	0x0070		/* A3000 SCSI interface */
#define	PODULE_LINDIS_DIGITISER	0x0071		/* digitiser */
#define	PODULE_CCC_PEAKPROGMETER	0x0072		/* peak prog. meter */
#define	PODULE_LASERLIGHTCONTROL	0x0073		/* laser light control */
#define	PODULE_HARDDISCINTERFACE	0x0074		/* hard disc interface */
#define	PODULE_EXTRAMOUSE	0x0075		/* extra mouse */
#define	PODULE_STEBUSINTERFACE	0x0076		/* STE bus interface */
#define	PODULE_MORLEY_ST506	0x0077		/* ST506 disc interface */
#define	PODULE_BRAINSOFT_MULTI1	0x0078		/* Multi_1 */
#define	PODULE_BRAINSOFT_MULTI2	0x0079		/* Multi_2 */
#define	PODULE_BRAINSOFT_24DIGITISER	0x007a		/* 24-bit digitiser */
#define	PODULE_BRAINSOFT_24GRAPHICS	0x007b		/* 24-bit graphics */
#define	PODULE_SYNTEC_SPECTRON	0x007c		/* Spectron */
#define	PODULE_SYNTEC_QUAD16DTOA	0x007d		/* Quad 16-bit DAC */
#define	PODULE_ROMBO_4BITDIGIISER	0x007e		/* 4-bit digitiser */
#define	PODULE_DONGLEANDKEYPAD	0x007f		/* dongle and keypad */
#define	PODULE_3SL_SCSI	0x0080		/* SCSI interface */
#define	PODULE_ARMADILLO_BTM1	0x0081		/* BTM1 */
#define	PODULE_ARMADILLO_DSO1	0x0082		/* DSO1 */
#define	PODULE_DELTRONICS_USER	0x0083		/* User Port */
#define	PODULE_JPEGCOMPRESSOR	0x0084		/* JPEG compressor */
#define	PODULE_BEEBUG_A3000SCSI	0x0085		/* A3000 SCSI */
#define	PODULE_BEEBUG_COLOURSCAN	0x0086		/* colour scanner interface */
#define	PODULE_EXTENSIONROM	0x0087		/* extension ROM */
#define	PODULE_GRAPHICSENHANCER	0x0088		/* Graphics Enhancer */
#define	PODULE_SIMIS_AFB300	0x0089		/* AFB300 */
#define	PODULE_FAXPACKSENIOR	0x008a		/* FaxPack Senior */
#define	PODULE_FAXPACKJUNIOR	0x008b		/* FaxPack Junior */
#define	PODULE_LINGENUITY_SCSI8SHARE	0x008c		/* 8 bit SCSIShare interface */
#define	PODULE_VTI_SCSI	0x008d		/* SCSI interface */
#define	PODULE_ATOMWIDE_PIA	0x008e		/* PIA */
#define	PODULE_NEXUSNS	0x008f		/* Nexus interface (A3020/RiscPC netslot) */
/* RISC iX: #define XCB_PRODUCT_XCB_DTSOFTWAREPCCONNECT 144 */
#define	PODULE_ATOMWIDE_SERIAL	0x0090		/* multiport serial interface */
#define	PODULE_WATFORD_IDE	0x0091		/* IDE interface */
#define	PODULE_ATOMWIDE_IDE	0x0092		/* IDE interface */
#define	PODULE_ARMADILLO_RSI	0x0093		/* RSI */
#define	PODULE_ARMADILLO_TCR	0x0094		/* TCR */
#define	PODULE_LINGENUITY_SCSI	0x0095		/* 16 bit SCSI interface */
#define	PODULE_LINGENUITY_SCSISHARE	0x0096		/* 16 bit SCSIShare interface */
#define	PODULE_BEEBUG_IDE	0x0097		/* IDE interface */
#define	PODULE_WATFORD_PRISMRT	0x0098		/* Prism RT */
#define	PODULE_HCCS_VIDEODIGITISER	0x0099		/* video digitiser */
#define	PODULE_DTSOFT_SCANPORT	0x009a		/* ScanPort */
#define	PODULE_DTSOFT_PACCEL	0x009b		/* Paccel */
#define	PODULE_DTSOFT_CANONION	0x009c		/* Canon ION interface */
#define	PODULE_BIA_AUDIO	0x009d		/* BIA audio */
#define	PODULE_IRLAM_FAXIM	0x009e		/* FaxIm */
#define	PODULE_IRLAM_MOVINGIMAGE	0x009f		/* Moving Image */
#define	PODULE_CUMANA_SCSI1	0x00a0		/* SCSI I interface */
#define	PODULE_NEXUS_A3000ETHERNET	0x00a1		/* A3000 Ethernet */
#define	PODULE_NEXUS_PCEMACCELL	0x00a2		/* PC Emulator accelerator */
#define	PODULE_NEXUS_64CANSERIAL	0x00a3		/* 64-channel serial */
#define	PODULE_ETHER3	0x00a4		/* Ether3/Ether5 interface */
#define	PODULE_IOTA_SCANNER	0x00a5		/* scanner interface */
#define	PODULE_NEXUS_I860MATHACCELL	0x00a6		/* i860 floating-point accelerator */
#define	PODULE_II_QUADSERIAL	0x00a7		/* quad serial port */
#define	PODULE_WATFORD_SCANNERGREY	0x00a8		/* grey-scale scanner */
#define	PODULE_WATFORD_SCANNERRGB	0x00a9		/* RGB scanner */
#define	PODULE_WATFORD_PRISMCOLOUR	0x00aa		/* Prism Colour */
#define	PODULE_WATFORD_USERANALOGUE	0x00ab		/* Analogue and User Ports */
#define	PODULE_BAILDON_DISCBUFFER	0x00ac		/* disc buffer */
#define	PODULE_BAILDON_A3000UPBUS	0x00ad		/* A3000 UP bus */
#define	PODULE_ICS_IDE	0x00ae		/* IDE Interface */
#define	PODULE_HCCS_BWDIGITISER	0x00af		/* b/w digitiser */
#define	PODULE_CSD_IDE8	0x00b0		/* 8-bit IDE interface */
#define	PODULE_CSD_IDE16	0x00b1		/* 16-bit IDE interface */
#define	PODULE_SERIALPORT_IDE	0x00b2		/* IDE interface */
#define	PODULE_SERIALPORT_4MFLOPPY	0x00b3		/* 4 MB floppy */
#define	PODULE_CADSOFT_MAESTROINTER	0x00b4		/* Maestro Inter */
#define	PODULE_ARXE_QUADFS	0x00b5		/* Quad-density floppy interface */
#define	PODULE_SERIALPORT_DUALSERIAL	0x00b9		/* Serial interface */
#define	PODULE_ETHERLAN200	0x00bd		/* EtherLan 200-series */
#define	PODULE_SCANLIGHTV256	0x00cb		/* ScanLight Video 256 */
#define	PODULE_EAGLEM2	0x00cc		/* Eagle M2 */
#define	PODULE_LARKA16	0x00ce		/* Lark A16 */
#define	PODULE_ETHERLAN100	0x00cf		/* EtherLan 100-series */
#define	PODULE_ETHERLAN500	0x00d4		/* EtherLan 500-series */
#define	PODULE_ETHERM	0x00d8		/* EtherM dual interface NIC */
#define	PODULE_CUMANA_SLCD	0x00dd		/* CDFS & SLCD expansion card */
#define	PODULE_BRINILINK	0x00df		/* BriniLink transputer link adapter */
#define	PODULE_ETHERB	0x00e4		/* EtherB network slot interface */
#define	PODULE_24I16	0x00e6		/* 24i16 digitiser */
#define	PODULE_PCCARD	0x00ea		/* PC card */
#define	PODULE_ETHERLAN600	0x00ec		/* EtherLan 600-series */
#define	PODULE_CASTLE_SCSI16SHARE	0x00f3		/* 8 or 16 bit SCSI2Share interface */
#define	PODULE_CASTLE_ETHERSCSISHARE	0x00f4		/* 8 or 16 bit SCSI2Share interface, possibly with Ethernet */
#define	PODULE_CASTLE_ETHERSCSI	0x00f5		/* EtherSCSI */
#define	PODULE_CASTLE_SCSI16	0x00f6		/* 8 or 16 bit SCSI2 interface */
#define	PODULE_ALSYSTEMS_SCSI	0x0107		/* SCSI II host adapter */
#define	PODULE_RAPIDE	0x0114		/* RapIDE32 interface */
#define	PODULE_ETHERLAN100AEH	0x011c		/* AEH77 (EtherLan 102) */
#define	PODULE_ETHERLAN200AEH	0x011d		/* AEH79 (EtherLan 210) */
#define	PODULE_ETHERLAN600AEH	0x011e		/* AEH62/78/99 (EtherLan 602) */
#define	PODULE_ETHERLAN500AEH	0x011f		/* AEH75 (EtherLan 512) */
#define	PODULE_CONNECT32	0x0125		/* Connect32 SCSI II interface */
#define	PODULE_CASTLE_SCSI32	0x012b		/* 32 bit SCSI2 + DMA interface */
#define	PODULE_ETHERLAN700AEH	0x012e		/* AEH98 (EtherLan 700-series) */
#define	PODULE_ETHERLAN700	0x012f		/* EtherLan 700-series */
#define	PODULE_SIMTEC_IDE8	0x0130		/* 8 bit IDE interface */
#define	PODULE_SIMTEC_IDE	0x0131		/* 16 bit IDE interface */
#define	PODULE_MIDICONNECT	0x0133		/* Midi-Connect */
#define	PODULE_ETHERI	0x0139		/* EtherI interface */
#define	PODULE_SIMTEC_USB	0x0145		/* USB interface */
#define	PODULE_SIMTEC_NET100	0x0150		/* NET100 interface */
#define	PODULE_MIDIMAX	0x0200		/* MIDI max */
#define	PODULE_MMETHERV	0x1234		/* Multi-media/EtherV */
#define	PODULE_ETHERN	0x5678		/* EtherN interface */
