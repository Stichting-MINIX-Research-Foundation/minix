/*	$NetBSD: pcidevs.h,v 1.1229 2015/08/28 13:10:42 nonaka Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: pcidevs,v 1.1235 2015/08/28 13:09:48 nonaka Exp
 */

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
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
 * NOTE: a fairly complete list of PCI codes can be found at:
 *
 *	http://www.pcidatabase.com/
 *
 * (but it doesn't always seem to match vendor documentation)
 *
 * NOTE: As per tron@NetBSD.org, the proper update procedure is
 *
 * 1.) Change "src/sys/dev/pci/pcidevs".
 * 2.) Commit "src/sys/dev/pci/pcidevs".
 * 3.) Execute "make -f Makefile.pcidevs" in "src/sys/dev/pci".
 * 4.) Commit "src/sys/dev/pci/pcidevs.h" and "src/sys/dev/pci/pcidevs_data.h".
 */

/*
 * Use "make -f Makefile.pcidevs" to regenerate pcidevs.h and pcidevs_data.h
 */

/*
 * List of known PCI vendors
 */

#define	PCI_VENDOR_PEAK	0x001c		/* Peak System Technik */
#define	PCI_VENDOR_MARTINMARIETTA	0x003d		/* Martin-Marietta */
#define	PCI_VENDOR_HAUPPAUGE	0x0070		/* Hauppauge Computer Works */
#define	PCI_VENDOR_DYNALINK	0x0675		/* Dynalink */
#define	PCI_VENDOR_COMPAQ	0x0e11		/* Compaq */
#define	PCI_VENDOR_SYMBIOS	0x1000		/* Symbios Logic */
#define	PCI_VENDOR_ATI	0x1002		/* ATI Technologies */
#define	PCI_VENDOR_ULSI	0x1003		/* ULSI Systems */
#define	PCI_VENDOR_VLSI	0x1004		/* VLSI Technology */
#define	PCI_VENDOR_AVANCE	0x1005		/* Avance Logic */
#define	PCI_VENDOR_REPLY	0x1006		/* Reply Group */
#define	PCI_VENDOR_NETFRAME	0x1007		/* NetFrame Systems */
#define	PCI_VENDOR_EPSON	0x1008		/* Epson */
#define	PCI_VENDOR_PHOENIX	0x100a		/* Phoenix Technologies */
#define	PCI_VENDOR_NS	0x100b		/* National Semiconductor */
#define	PCI_VENDOR_TSENG	0x100c		/* Tseng Labs */
#define	PCI_VENDOR_AST	0x100d		/* AST Research */
#define	PCI_VENDOR_WEITEK	0x100e		/* Weitek */
#define	PCI_VENDOR_VIDEOLOGIC	0x1010		/* Video Logic */
#define	PCI_VENDOR_DEC	0x1011		/* Digital Equipment */
#define	PCI_VENDOR_MICRONICS	0x1012		/* Micronics Computers */
#define	PCI_VENDOR_CIRRUS	0x1013		/* Cirrus Logic */
#define	PCI_VENDOR_IBM	0x1014		/* IBM */
#define	PCI_VENDOR_LSIL	0x1015		/* LSI Logic of Canada */
#define	PCI_VENDOR_ICLPERSONAL	0x1016		/* ICL Personal Systems */
#define	PCI_VENDOR_SPEA	0x1017		/* SPEA Software */
#define	PCI_VENDOR_UNISYS	0x1018		/* Unisys Systems */
#define	PCI_VENDOR_ELITEGROUP	0x1019		/* Elitegroup Computer Systems */
#define	PCI_VENDOR_NCR	0x101a		/* AT&T Global Information Systems */
#define	PCI_VENDOR_VITESSE	0x101b		/* Vitesse Semiconductor */
#define	PCI_VENDOR_WD	0x101c		/* Western Digital */
#define	PCI_VENDOR_AMI	0x101e		/* American Megatrends */
#define	PCI_VENDOR_PICTURETEL	0x101f		/* PictureTel */
#define	PCI_VENDOR_HITACHICOMP	0x1020		/* Hitachi Computer Products */
#define	PCI_VENDOR_OKI	0x1021		/* OKI Electric Industry */
#define	PCI_VENDOR_AMD	0x1022		/* AMD */
#define	PCI_VENDOR_TRIDENT	0x1023		/* Trident Microsystems */
#define	PCI_VENDOR_ZENITH	0x1024		/* Zenith Data Systems */
#define	PCI_VENDOR_ACER	0x1025		/* Acer */
#define	PCI_VENDOR_DELL	0x1028		/* Dell Computer */
#define	PCI_VENDOR_SNI	0x1029		/* Siemens Nixdorf AG */
#define	PCI_VENDOR_LSILOGIC	0x102a		/* LSI Logic, Headland div. */
#define	PCI_VENDOR_MATROX	0x102b		/* Matrox */
#define	PCI_VENDOR_CHIPS	0x102c		/* Chips and Technologies */
#define	PCI_VENDOR_WYSE	0x102d		/* WYSE Technology */
#define	PCI_VENDOR_OLIVETTI	0x102e		/* Olivetti Advanced Technology */
#define	PCI_VENDOR_TOSHIBA	0x102f		/* Toshiba America */
#define	PCI_VENDOR_TMCRESEARCH	0x1030		/* TMC Research */
#define	PCI_VENDOR_MIRO	0x1031		/* Miro Computer Products */
#define	PCI_VENDOR_COMPAQ2	0x1032		/* Compaq (2nd PCI Vendor ID) */
#define	PCI_VENDOR_NEC	0x1033		/* NEC */
#define	PCI_VENDOR_BURNDY	0x1034		/* Burndy */
#define	PCI_VENDOR_COMPCOMM	0x1035		/* Comp. & Comm. Research Lab */
#define	PCI_VENDOR_FUTUREDOMAIN	0x1036		/* Future Domain */
#define	PCI_VENDOR_HITACHIMICRO	0x1037		/* Hitach Microsystems */
#define	PCI_VENDOR_AMP	0x1038		/* AMP */
#define	PCI_VENDOR_SIS	0x1039		/* Silicon Integrated System */
#define	PCI_VENDOR_SEIKOEPSON	0x103a		/* Seiko Epson */
#define	PCI_VENDOR_TATUNGAMERICA	0x103b		/* Tatung of America */
#define	PCI_VENDOR_HP	0x103c		/* Hewlett-Packard */
#define	PCI_VENDOR_SOLLIDAY	0x103e		/* Solliday Engineering */
#define	PCI_VENDOR_LOGICMODELLING	0x103f		/* Logic Modeling */
#define	PCI_VENDOR_KPC	0x1040		/* Kubota Pacific */
#define	PCI_VENDOR_COMPUTREND	0x1041		/* Computrend */
#define	PCI_VENDOR_PCTECH	0x1042		/* PC Technology */
#define	PCI_VENDOR_ASUSTEK	0x1043		/* Asustek Computer */
#define	PCI_VENDOR_DPT	0x1044		/* Distributed Processing Technology */
#define	PCI_VENDOR_OPTI	0x1045		/* Opti */
#define	PCI_VENDOR_IPCCORP	0x1046		/* IPC */
#define	PCI_VENDOR_GENOA	0x1047		/* Genoa Systems */
#define	PCI_VENDOR_ELSA	0x1048		/* Elsa */
#define	PCI_VENDOR_FOUNTAINTECH	0x1049		/* Fountain Technology */
#define	PCI_VENDOR_SGSTHOMSON	0x104a		/* SGS-Thomson Microelectronics */
#define	PCI_VENDOR_BUSLOGIC	0x104b		/* BusLogic */
#define	PCI_VENDOR_TI	0x104c		/* Texas Instruments */
#define	PCI_VENDOR_SONY	0x104d		/* Sony */
#define	PCI_VENDOR_OAKTECH	0x104e		/* Oak Technology */
#define	PCI_VENDOR_COTIME	0x104f		/* Co-time Computer */
#define	PCI_VENDOR_WINBOND	0x1050		/* Winbond Electronics */
#define	PCI_VENDOR_ANIGMA	0x1051		/* Anigma */
#define	PCI_VENDOR_YOUNGMICRO	0x1052		/* Young Micro Systems */
#define	PCI_VENDOR_HITACHI	0x1054		/* Hitachi */
#define	PCI_VENDOR_EFARMICRO	0x1055		/* Efar Microsystems */
#define	PCI_VENDOR_ICL	0x1056		/* ICL */
#define	PCI_VENDOR_MOT	0x1057		/* Motorola */
#define	PCI_VENDOR_ETR	0x1058		/* Electronics & Telec. RSH */
#define	PCI_VENDOR_TEKNOR	0x1059		/* Teknor Microsystems */
#define	PCI_VENDOR_PROMISE	0x105a		/* Promise Technology */
#define	PCI_VENDOR_FOXCONN	0x105b		/* Foxconn International */
#define	PCI_VENDOR_WIPRO	0x105c		/* Wipro Infotech */
#define	PCI_VENDOR_NUMBER9	0x105d		/* Number 9 Computer Company */
#define	PCI_VENDOR_VTECH	0x105e		/* Vtech Computers */
#define	PCI_VENDOR_INFOTRONIC	0x105f		/* Infotronic America */
#define	PCI_VENDOR_UMC	0x1060		/* United Microelectronics */
#define	PCI_VENDOR_ITT	0x1061		/* I. T. T. */
#define	PCI_VENDOR_MASPAR	0x1062		/* MasPar Computer */
#define	PCI_VENDOR_OCEANOA	0x1063		/* Ocean Office Automation */
#define	PCI_VENDOR_ALCATEL	0x1064		/* Alcatel CIT */
#define	PCI_VENDOR_TEXASMICRO	0x1065		/* Texas Microsystems */
#define	PCI_VENDOR_PICOPOWER	0x1066		/* Picopower Technology */
#define	PCI_VENDOR_MITSUBISHI	0x1067		/* Mitsubishi Electronics */
#define	PCI_VENDOR_DIVERSIFIED	0x1068		/* Diversified Technology */
#define	PCI_VENDOR_MYLEX	0x1069		/* Mylex */
#define	PCI_VENDOR_ATEN	0x106a		/* Aten Research */
#define	PCI_VENDOR_APPLE	0x106b		/* Apple Computer */
#define	PCI_VENDOR_HYUNDAI	0x106c		/* Hyundai Electronics America */
#define	PCI_VENDOR_SEQUENT	0x106d		/* Sequent */
#define	PCI_VENDOR_DFI	0x106e		/* DFI */
#define	PCI_VENDOR_CITYGATE	0x106f		/* City Gate Development */
#define	PCI_VENDOR_DAEWOO	0x1070		/* Daewoo Telecom */
#define	PCI_VENDOR_MITAC	0x1071		/* Mitac */
#define	PCI_VENDOR_GIT	0x1072		/* GIT */
#define	PCI_VENDOR_YAMAHA	0x1073		/* Yamaha */
#define	PCI_VENDOR_NEXGEN	0x1074		/* NexGen Microsystems */
#define	PCI_VENDOR_AIR	0x1075		/* Advanced Integration Research */
#define	PCI_VENDOR_CHAINTECH	0x1076		/* Chaintech Computer */
#define	PCI_VENDOR_QLOGIC	0x1077		/* QLogic */
#define	PCI_VENDOR_CYRIX	0x1078		/* Cyrix */
#define	PCI_VENDOR_IBUS	0x1079		/* I-Bus */
#define	PCI_VENDOR_NETWORTH	0x107a		/* NetWorth */
#define	PCI_VENDOR_GATEWAY	0x107b		/* Gateway 2000 */
#define	PCI_VENDOR_GOLDSTAR	0x107c		/* Goldstar */
#define	PCI_VENDOR_LEADTEK	0x107d		/* LeadTek Research */
#define	PCI_VENDOR_INTERPHASE	0x107e		/* Interphase */
#define	PCI_VENDOR_DATATECH	0x107f		/* Data Technology */
#define	PCI_VENDOR_CONTAQ	0x1080		/* Contaq Microsystems */
#define	PCI_VENDOR_SUPERMAC	0x1081		/* Supermac Technology */
#define	PCI_VENDOR_EFA	0x1082		/* EFA of America */
#define	PCI_VENDOR_FOREX	0x1083		/* Forex Computer */
#define	PCI_VENDOR_PARADOR	0x1084		/* Parador */
#define	PCI_VENDOR_TULIP	0x1085		/* Tulip Computers */
#define	PCI_VENDOR_JBOND	0x1086		/* J. Bond Computer Systems */
#define	PCI_VENDOR_CACHECOMP	0x1087		/* Cache Computer */
#define	PCI_VENDOR_MICROCOMP	0x1088		/* Microcomputer Systems */
#define	PCI_VENDOR_DG	0x1089		/* Data General */
#define	PCI_VENDOR_BIT3	0x108a		/* Bit3 Computer */
#define	PCI_VENDOR_ELONEX	0x108c		/* Elonex PLC c/o Oakleigh Systems */
#define	PCI_VENDOR_OLICOM	0x108d		/* Olicom */
#define	PCI_VENDOR_SUN	0x108e		/* Sun Microsystems */
#define	PCI_VENDOR_SYSTEMSOFT	0x108f		/* Systemsoft */
#define	PCI_VENDOR_ENCORE	0x1090		/* Encore Computer */
#define	PCI_VENDOR_INTERGRAPH	0x1091		/* Intergraph */
#define	PCI_VENDOR_DIAMOND	0x1092		/* Diamond Computer Systems */
#define	PCI_VENDOR_NATIONALINST	0x1093		/* National Instruments */
#define	PCI_VENDOR_FICOMP	0x1094		/* First Int'l Computers */
#define	PCI_VENDOR_CMDTECH	0x1095		/* CMD Technology */
#define	PCI_VENDOR_ALACRON	0x1096		/* Alacron */
#define	PCI_VENDOR_APPIAN	0x1097		/* Appian Technology */
#define	PCI_VENDOR_QUANTUMDESIGNS	0x1098		/* Quantum Designs */
#define	PCI_VENDOR_SAMSUNGELEC	0x1099		/* Samsung Electronics */
#define	PCI_VENDOR_PACKARDBELL	0x109a		/* Packard Bell */
#define	PCI_VENDOR_GEMLIGHT	0x109b		/* Gemlight Computer */
#define	PCI_VENDOR_MEGACHIPS	0x109c		/* Megachips */
#define	PCI_VENDOR_ZIDA	0x109d		/* Zida Technologies */
#define	PCI_VENDOR_BROOKTREE	0x109e		/* Brooktree */
#define	PCI_VENDOR_TRIGEM	0x109f		/* Trigem Computer */
#define	PCI_VENDOR_MEIDENSHA	0x10a0		/* Meidensha */
#define	PCI_VENDOR_JUKO	0x10a1		/* Juko Electronics */
#define	PCI_VENDOR_QUANTUM	0x10a2		/* Quantum */
#define	PCI_VENDOR_EVEREX	0x10a3		/* Everex Systems */
#define	PCI_VENDOR_GLOBE	0x10a4		/* Globe Manufacturing Sales */
#define	PCI_VENDOR_RACAL	0x10a5		/* Racal Interlan */
#define	PCI_VENDOR_INFORMTECH	0x10a6		/* Informtech Industrial */
#define	PCI_VENDOR_BENCHMARQ	0x10a7		/* Benchmarq Microelectronics */
#define	PCI_VENDOR_SIERRA	0x10a8		/* Sierra Semiconductor */
#define	PCI_VENDOR_SGI	0x10a9		/* Silicon Graphics */
#define	PCI_VENDOR_ACC	0x10aa		/* ACC Microelectronics */
#define	PCI_VENDOR_DIGICOM	0x10ab		/* Digicom */
#define	PCI_VENDOR_HONEYWELL	0x10ac		/* Honeywell IASD */
#define	PCI_VENDOR_SYMPHONY	0x10ad		/* Symphony Labs */
#define	PCI_VENDOR_CORNERSTONE	0x10ae		/* Cornerstone Technology */
#define	PCI_VENDOR_MICROCOMPSON	0x10af		/* Micro Computer Sysytems (M) SON */
#define	PCI_VENDOR_CARDEXPER	0x10b0		/* CardExpert Technology */
#define	PCI_VENDOR_CABLETRON	0x10b1		/* Cabletron Systems */
#define	PCI_VENDOR_RAYETHON	0x10b2		/* Raytheon */
#define	PCI_VENDOR_DATABOOK	0x10b3		/* Databook */
#define	PCI_VENDOR_STB	0x10b4		/* STB Systems */
#define	PCI_VENDOR_PLX	0x10b5		/* PLX Technology */
#define	PCI_VENDOR_MADGE	0x10b6		/* Madge Networks */
#define	PCI_VENDOR_3COM	0x10b7		/* 3Com */
#define	PCI_VENDOR_SMC	0x10b8		/* Standard Microsystems */
#define	PCI_VENDOR_ALI	0x10b9		/* Acer Labs */
#define	PCI_VENDOR_MITSUBISHIELEC	0x10ba		/* Mitsubishi Electronics */
#define	PCI_VENDOR_DAPHA	0x10bb		/* Dapha Electronics */
#define	PCI_VENDOR_ALR	0x10bc		/* Advanced Logic Research */
#define	PCI_VENDOR_SURECOM	0x10bd		/* Surecom Technology */
#define	PCI_VENDOR_TSENGLABS	0x10be		/* Tseng Labs International */
#define	PCI_VENDOR_MOST	0x10bf		/* Most */
#define	PCI_VENDOR_BOCA	0x10c0		/* Boca Research */
#define	PCI_VENDOR_ICM	0x10c1		/* ICM */
#define	PCI_VENDOR_AUSPEX	0x10c2		/* Auspex Systems */
#define	PCI_VENDOR_SAMSUNGSEMI	0x10c3		/* Samsung Semiconductors */
#define	PCI_VENDOR_AWARD	0x10c4		/* Award Software Int'l */
#define	PCI_VENDOR_XEROX	0x10c5		/* Xerox */
#define	PCI_VENDOR_RAMBUS	0x10c6		/* Rambus */
#define	PCI_VENDOR_MEDIAVIS	0x10c7		/* Media Vision */
#define	PCI_VENDOR_NEOMAGIC	0x10c8		/* Neomagic */
#define	PCI_VENDOR_DATAEXPERT	0x10c9		/* Dataexpert */
#define	PCI_VENDOR_FUJITSU	0x10ca		/* Fujitsu */
#define	PCI_VENDOR_OMRON	0x10cb		/* Omron */
#define	PCI_VENDOR_MENTOR	0x10cc		/* Mentor ARC */
#define	PCI_VENDOR_ADVSYS	0x10cd		/* Advanced System Products */
#define	PCI_VENDOR_RADIUS	0x10ce		/* Radius */
#define	PCI_VENDOR_FUJITSU4	0x10cf		/* Fujitsu (4th PCI Vendor ID) */
#define	PCI_VENDOR_FUJITSU2	0x10d0		/* Fujitsu (2nd PCI Vendor ID) */
#define	PCI_VENDOR_FUTUREPLUS	0x10d1		/* Future+ Systems */
#define	PCI_VENDOR_MOLEX	0x10d2		/* Molex */
#define	PCI_VENDOR_JABIL	0x10d3		/* Jabil Circuit */
#define	PCI_VENDOR_HAULON	0x10d4		/* Hualon Microelectronics */
#define	PCI_VENDOR_AUTOLOGIC	0x10d5		/* Autologic */
#define	PCI_VENDOR_CETIA	0x10d6		/* Cetia */
#define	PCI_VENDOR_BCM	0x10d7		/* BCM Advanced */
#define	PCI_VENDOR_APL	0x10d8		/* Advanced Peripherals Labs */
#define	PCI_VENDOR_MACRONIX	0x10d9		/* Macronix */
#define	PCI_VENDOR_THOMASCONRAD	0x10da		/* Thomas-Conrad */
#define	PCI_VENDOR_ROHM	0x10db		/* Rohm Research */
#define	PCI_VENDOR_CERN	0x10dc		/* CERN/ECP/EDU */
#define	PCI_VENDOR_ES	0x10dd		/* Evans & Sutherland */
#define	PCI_VENDOR_NVIDIA	0x10de		/* NVIDIA */
#define	PCI_VENDOR_EMULEX	0x10df		/* Emulex */
#define	PCI_VENDOR_IMS	0x10e0		/* Integrated Micro Solutions */
#define	PCI_VENDOR_TEKRAM	0x10e1		/* Tekram Technology (1st PCI Vendor ID) */
#define	PCI_VENDOR_APTIX	0x10e2		/* Aptix */
#define	PCI_VENDOR_NEWBRIDGE	0x10e3		/* Newbridge Microsystems / Tundra Semiconductor */
#define	PCI_VENDOR_TANDEM	0x10e4		/* Tandem Computers */
#define	PCI_VENDOR_MICROINDUSTRIES	0x10e5		/* Micro Industries */
#define	PCI_VENDOR_GAINBERY	0x10e6		/* Gainbery Computer Products */
#define	PCI_VENDOR_VADEM	0x10e7		/* Vadem */
#define	PCI_VENDOR_AMCIRCUITS	0x10e8		/* Applied Micro Circuits */
#define	PCI_VENDOR_ALPSELECTIC	0x10e9		/* Alps Electric */
#define	PCI_VENDOR_INTEGRAPHICS	0x10ea		/* Integraphics Systems */
#define	PCI_VENDOR_ARTISTSGRAPHICS	0x10eb		/* Artists Graphics */
#define	PCI_VENDOR_REALTEK	0x10ec		/* Realtek Semiconductor */
#define	PCI_VENDOR_ASCIICORP	0x10ed		/* ASCII */
#define	PCI_VENDOR_XILINX	0x10ee		/* Xilinx */
#define	PCI_VENDOR_RACORE	0x10ef		/* Racore Computer Products */
#define	PCI_VENDOR_PERITEK	0x10f0		/* Peritek */
#define	PCI_VENDOR_TYAN	0x10f1		/* Tyan Computer */
#define	PCI_VENDOR_ACHME	0x10f2		/* Achme Computer */
#define	PCI_VENDOR_ALARIS	0x10f3		/* Alaris */
#define	PCI_VENDOR_SMOS	0x10f4		/* S-MOS Systems */
#define	PCI_VENDOR_NKK	0x10f5		/* NKK */
#define	PCI_VENDOR_CREATIVE	0x10f6		/* Creative Electronic Systems */
#define	PCI_VENDOR_MATSUSHITA	0x10f7		/* Matsushita */
#define	PCI_VENDOR_ALTOS	0x10f8		/* Altos India */
#define	PCI_VENDOR_PCDIRECT	0x10f9		/* PC Direct */
#define	PCI_VENDOR_TRUEVISIO	0x10fa		/* Truevision */
#define	PCI_VENDOR_THESYS	0x10fb		/* Thesys Ges. F. Mikroelektronik */
#define	PCI_VENDOR_IODATA	0x10fc		/* I-O Data Device */
#define	PCI_VENDOR_SOYO	0x10fd		/* Soyo Technology */
#define	PCI_VENDOR_FAST	0x10fe		/* Fast Electronic */
#define	PCI_VENDOR_NCUBE	0x10ff		/* NCube */
#define	PCI_VENDOR_JAZZ	0x1100		/* Jazz Multimedia */
#define	PCI_VENDOR_INITIO	0x1101		/* Initio */
#define	PCI_VENDOR_CREATIVELABS	0x1102		/* Creative Labs */
#define	PCI_VENDOR_TRIONES	0x1103		/* Triones Technologies */
#define	PCI_VENDOR_RASTEROPS	0x1104		/* RasterOps */
#define	PCI_VENDOR_SIGMA	0x1105		/* Sigma Designs */
#define	PCI_VENDOR_VIATECH	0x1106		/* VIA Technologies */
#define	PCI_VENDOR_STRATIS	0x1107		/* Stratus Computer */
#define	PCI_VENDOR_PROTEON	0x1108		/* Proteon */
#define	PCI_VENDOR_COGENT	0x1109		/* Cogent Data Technologies */
#define	PCI_VENDOR_SIEMENS	0x110a		/* Siemens AG / Siemens Nixdorf AG */
#define	PCI_VENDOR_XENON	0x110b		/* Xenon Microsystems */
#define	PCI_VENDOR_MINIMAX	0x110c		/* Mini-Max Technology */
#define	PCI_VENDOR_ZNYX	0x110d		/* Znyx Advanced Systems */
#define	PCI_VENDOR_CPUTECH	0x110e		/* CPU Technology */
#define	PCI_VENDOR_ROSS	0x110f		/* Ross Technology */
#define	PCI_VENDOR_POWERHOUSE	0x1110		/* Powerhouse Systems */
#define	PCI_VENDOR_SCO	0x1111		/* Santa Cruz Operation */
#define	PCI_VENDOR_RNS	0x1112		/* RNS */
#define	PCI_VENDOR_ACCTON	0x1113		/* Accton Technology */
#define	PCI_VENDOR_ATMEL	0x1114		/* Atmel */
#define	PCI_VENDOR_DUPONT	0x1115		/* DuPont Pixel Systems */
#define	PCI_VENDOR_DATATRANSLATION	0x1116		/* Data Translation */
#define	PCI_VENDOR_DATACUBE	0x1117		/* Datacube */
#define	PCI_VENDOR_BERG	0x1118		/* Berg Electronics */
#define	PCI_VENDOR_VORTEX	0x1119		/* Vortex Computer Systems */
#define	PCI_VENDOR_EFFICIENTNETS	0x111a		/* Efficent Networks */
#define	PCI_VENDOR_TELEDYNE	0x111b		/* Teledyne Electronic Systems */
#define	PCI_VENDOR_TRICORD	0x111c		/* Tricord Systems */
#define	PCI_VENDOR_IDT	0x111d		/* IDT */
#define	PCI_VENDOR_ELDEC	0x111e		/* Eldec */
#define	PCI_VENDOR_PDI	0x111f		/* Prescision Digital Images */
#define	PCI_VENDOR_EMC	0x1120		/* Emc */
#define	PCI_VENDOR_ZILOG	0x1121		/* Zilog */
#define	PCI_VENDOR_MULTITECH	0x1122		/* Multi-tech Systems */
#define	PCI_VENDOR_LEUTRON	0x1124		/* Leutron Vision */
#define	PCI_VENDOR_EUROCORE	0x1125		/* Eurocore/Vigra */
#define	PCI_VENDOR_VIGRA	0x1126		/* Vigra */
#define	PCI_VENDOR_FORE	0x1127		/* FORE Systems */
#define	PCI_VENDOR_FIRMWORKS	0x1129		/* Firmworks */
#define	PCI_VENDOR_HERMES	0x112a		/* Hermes Electronics */
#define	PCI_VENDOR_LINOTYPE	0x112b		/* Linotype */
#define	PCI_VENDOR_RAVICAD	0x112d		/* Ravicad */
#define	PCI_VENDOR_INFOMEDIA	0x112e		/* Infomedia Microelectronics */
#define	PCI_VENDOR_IMAGINGTECH	0x112f		/* Imaging Technlogy */
#define	PCI_VENDOR_COMPUTERVISION	0x1130		/* Computervision */
#define	PCI_VENDOR_PHILIPS	0x1131		/* Philips */
#define	PCI_VENDOR_MITEL	0x1132		/* Mitel */
#define	PCI_VENDOR_EICON	0x1133		/* Eicon Technology */
#define	PCI_VENDOR_MCS	0x1134		/* Mercury Computer Systems */
#define	PCI_VENDOR_FUJIXEROX	0x1135		/* Fuji Xerox */
#define	PCI_VENDOR_MOMENTUM	0x1136		/* Momentum Data Systems */
#define	PCI_VENDOR_CISCO	0x1137		/* Cisco Systems */
#define	PCI_VENDOR_ZIATECH	0x1138		/* Ziatech */
#define	PCI_VENDOR_DYNPIC	0x1139		/* Dynamic Pictures */
#define	PCI_VENDOR_FWB	0x113a		/* FWB */
#define	PCI_VENDOR_CYCLONE	0x113c		/* Cyclone Micro */
#define	PCI_VENDOR_LEADINGEDGE	0x113d		/* Leading Edge */
#define	PCI_VENDOR_SANYO	0x113e		/* Sanyo Electric */
#define	PCI_VENDOR_EQUINOX	0x113f		/* Equinox Systems */
#define	PCI_VENDOR_INTERVOICE	0x1140		/* Intervoice */
#define	PCI_VENDOR_CREST	0x1141		/* Crest Microsystem */
#define	PCI_VENDOR_ALLIANCE	0x1142		/* Alliance Semiconductor */
#define	PCI_VENDOR_NETPOWER	0x1143		/* NetPower */
#define	PCI_VENDOR_CINMILACRON	0x1144		/* Cincinnati Milacron */
#define	PCI_VENDOR_WORKBIT	0x1145		/* Workbit */
#define	PCI_VENDOR_FORCE	0x1146		/* Force Computers */
#define	PCI_VENDOR_INTERFACE	0x1147		/* Interface */
#define	PCI_VENDOR_SCHNEIDERKOCH	0x1148		/* Schneider & Koch */
#define	PCI_VENDOR_WINSYSTEM	0x1149		/* Win System */
#define	PCI_VENDOR_VMIC	0x114a		/* VMIC */
#define	PCI_VENDOR_CANOPUS	0x114b		/* Canopus */
#define	PCI_VENDOR_ANNABOOKS	0x114c		/* Annabooks */
#define	PCI_VENDOR_IC	0x114d		/* IC */
#define	PCI_VENDOR_NIKON	0x114e		/* Nikon Systems */
#define	PCI_VENDOR_DIGI	0x114f		/* Digi International */
#define	PCI_VENDOR_TMC	0x1150		/* Thinking Machines */
#define	PCI_VENDOR_JAE	0x1151		/* JAE Electronics */
#define	PCI_VENDOR_MEGATEK	0x1152		/* Megatek */
#define	PCI_VENDOR_LANDWIN	0x1153		/* Land Win Electronic */
#define	PCI_VENDOR_MELCO	0x1154		/* Melco */
#define	PCI_VENDOR_PINETECH	0x1155		/* Pine Technology */
#define	PCI_VENDOR_PERISCOPE	0x1156		/* Periscope Engineering */
#define	PCI_VENDOR_AVSYS	0x1157		/* Avsys */
#define	PCI_VENDOR_VOARX	0x1158		/* Voarx R & D */
#define	PCI_VENDOR_MUTECH	0x1159		/* Mutech */
#define	PCI_VENDOR_HARLEQUIN	0x115a		/* Harlequin */
#define	PCI_VENDOR_PARALLAX	0x115b		/* Parallax Graphics */
#define	PCI_VENDOR_XIRCOM	0x115d		/* Xircom */
#define	PCI_VENDOR_PEERPROTO	0x115e		/* Peer Protocols */
#define	PCI_VENDOR_MAXTOR	0x115f		/* Maxtor */
#define	PCI_VENDOR_MEGASOFT	0x1160		/* Megasoft */
#define	PCI_VENDOR_PFU	0x1161		/* PFU Limited */
#define	PCI_VENDOR_OALAB	0x1162		/* OA Laboratory */
#define	PCI_VENDOR_RENDITION	0x1163		/* Rendition */
#define	PCI_VENDOR_APT	0x1164		/* Advanced Peripherals Technologies */
#define	PCI_VENDOR_IMAGRAPH	0x1165		/* Imagraph */
#define	PCI_VENDOR_SERVERWORKS	0x1166		/* ServerWorks */
#define	PCI_VENDOR_MUTOH	0x1167		/* Mutoh Industries */
#define	PCI_VENDOR_THINE	0x1168		/* Thine Electronics */
#define	PCI_VENDOR_CDAC	0x1169		/* Centre for Dev. of Advanced Computing */
#define	PCI_VENDOR_POLARIS	0x116a		/* Polaris Communications */
#define	PCI_VENDOR_CONNECTWARE	0x116b		/* Connectware */
#define	PCI_VENDOR_WSTECH	0x116f		/* Workstation Technology */
#define	PCI_VENDOR_INVENTEC	0x1170		/* Inventec */
#define	PCI_VENDOR_LOUGHSOUND	0x1171		/* Loughborough Sound Images */
#define	PCI_VENDOR_ALTERA	0x1172		/* Altera */
#define	PCI_VENDOR_ADOBE	0x1173		/* Adobe Systems */
#define	PCI_VENDOR_BRIDGEPORT	0x1174		/* Bridgeport Machines */
#define	PCI_VENDOR_MIRTRON	0x1175		/* Mitron Computer */
#define	PCI_VENDOR_SBE	0x1176		/* SBE */
#define	PCI_VENDOR_SILICONENG	0x1177		/* Silicon Engineering */
#define	PCI_VENDOR_ALFA	0x1178		/* Alfa */
#define	PCI_VENDOR_TOSHIBA2	0x1179		/* Toshiba */
#define	PCI_VENDOR_ATREND	0x117a		/* A-Trend Technology */
#define	PCI_VENDOR_ATTO	0x117c		/* Atto Technology */
#define	PCI_VENDOR_TR	0x117e		/* T/R Systems */
#define	PCI_VENDOR_RICOH	0x1180		/* Ricoh */
#define	PCI_VENDOR_TELEMATICS	0x1181		/* Telematics International */
#define	PCI_VENDOR_FUJIKURA	0x1183		/* Fujikura */
#define	PCI_VENDOR_FORKS	0x1184		/* Forks */
#define	PCI_VENDOR_DATAWORLD	0x1185		/* Dataworld */
#define	PCI_VENDOR_DLINK	0x1186		/* D-Link Systems */
#define	PCI_VENDOR_ATL	0x1187		/* Advanced Techonoloy Labratories */
#define	PCI_VENDOR_SHIMA	0x1188		/* Shima Seiki Manufacturing */
#define	PCI_VENDOR_MATSUSHITA2	0x1189		/* Matsushita Electronics (2nd PCI Vendor ID) */
#define	PCI_VENDOR_HILEVEL	0x118a		/* HiLevel Technology */
#define	PCI_VENDOR_COROLLARY	0x118c		/* Corrollary */
#define	PCI_VENDOR_BITFLOW	0x118d		/* BitFlow */
#define	PCI_VENDOR_HERMSTEDT	0x118e		/* Hermstedt */
#define	PCI_VENDOR_ACARD	0x1191		/* Acard */
#define	PCI_VENDOR_DENSAN	0x1192		/* Densan */
#define	PCI_VENDOR_ZEINET	0x1193		/* Zeinet */
#define	PCI_VENDOR_TOUCAN	0x1194		/* Toucan Technology */
#define	PCI_VENDOR_RATOC	0x1195		/* Ratoc Systems */
#define	PCI_VENDOR_HYTEC	0x1196		/* Hytec Electronic */
#define	PCI_VENDOR_GAGE	0x1197		/* Gage Applied Sciences */
#define	PCI_VENDOR_LAMBDA	0x1198		/* Lambda Systems */
#define	PCI_VENDOR_DCA	0x1199		/* Digital Communications Associates */
#define	PCI_VENDOR_MINDSHARE	0x119a		/* Mind Share */
#define	PCI_VENDOR_OMEGA	0x119b		/* Omega Micro */
#define	PCI_VENDOR_ITI	0x119c		/* Information Technology Institute */
#define	PCI_VENDOR_BUG	0x119d		/* Bug Sapporo */
#define	PCI_VENDOR_FUJITSU3	0x119e		/* Fujitsu (3th PCI Vendor ID) */
#define	PCI_VENDOR_BULL	0x119f		/* Bull Hn Information Systems */
#define	PCI_VENDOR_CONVEX	0x11a0		/* Convex Computer */
#define	PCI_VENDOR_HAMAMATSU	0x11a1		/* Hamamatsu Photonics */
#define	PCI_VENDOR_SIERRA2	0x11a2		/* Sierra Research & Technology (2nd PCI Vendor ID) */
#define	PCI_VENDOR_BARCO	0x11a4		/* Barco */
#define	PCI_VENDOR_MICROUNITY	0x11a5		/* MicroUnity Systems Engineering */
#define	PCI_VENDOR_PUREDATA	0x11a6		/* Pure Data */
#define	PCI_VENDOR_POWERCC	0x11a7		/* Power Computing */
#define	PCI_VENDOR_INNOSYS	0x11a9		/* InnoSys */
#define	PCI_VENDOR_ACTEL	0x11aa		/* Actel */
#define	PCI_VENDOR_MARVELL	0x11ab		/* Marvell */
#define	PCI_VENDOR_CANNON	0x11ac		/* Cannon IS */
#define	PCI_VENDOR_LITEON	0x11ad		/* Lite-On Communications */
#define	PCI_VENDOR_SCITEX	0x11ae		/* Scitex */
#define	PCI_VENDOR_AVID	0x11af		/* Avid Technology */
#define	PCI_VENDOR_V3	0x11b0		/* V3 Semiconductor */
#define	PCI_VENDOR_APRICOT	0x11b1		/* Apricot Computer */
#define	PCI_VENDOR_KODAK	0x11b2		/* Eastman Kodak */
#define	PCI_VENDOR_BARR	0x11b3		/* Barr Systems */
#define	PCI_VENDOR_LEITECH	0x11b4		/* Leitch Technology */
#define	PCI_VENDOR_RADSTONE	0x11b5		/* Radstone Technology */
#define	PCI_VENDOR_UNITEDVIDEO	0x11b6		/* United Video */
#define	PCI_VENDOR_MOT2	0x11b7		/* Motorola (2nd PCI Vendor ID) */
#define	PCI_VENDOR_XPOINT	0x11b8		/* Xpoint Technologies */
#define	PCI_VENDOR_PATHLIGHT	0x11b9		/* Pathlight Technology */
#define	PCI_VENDOR_VIDEOTRON	0x11ba		/* VideoTron */
#define	PCI_VENDOR_PYRAMID	0x11bb		/* Pyramid Technologies */
#define	PCI_VENDOR_NETPERIPH	0x11bc		/* Network Peripherals */
#define	PCI_VENDOR_PINNACLE	0x11bd		/* Pinnacle Systems */
#define	PCI_VENDOR_IMI	0x11be		/* International Microcircuts */
#define	PCI_VENDOR_LUCENT	0x11c1		/* Lucent Technologies */
#define	PCI_VENDOR_NEC2	0x11c3		/* NEC (2nd PCI Vendor ID) */
#define	PCI_VENDOR_DOCTECH	0x11c4		/* Document Technologies */
#define	PCI_VENDOR_SHIVA	0x11c5		/* Shiva */
#define	PCI_VENDOR_DCMDATA	0x11c7		/* DCM Data Systems */
#define	PCI_VENDOR_DOLPHIN	0x11c8		/* Dolphin Interconnect Solutions */
#define	PCI_VENDOR_MAGMA	0x11c9		/* Mesa Ridge Technologies (MAGMA) */
#define	PCI_VENDOR_LSISYS	0x11ca		/* LSI Systems */
#define	PCI_VENDOR_SPECIALIX	0x11cb		/* Specialix Research */
#define	PCI_VENDOR_MKC	0x11cc		/* Michels & Kleberhoff Computer */
#define	PCI_VENDOR_HAL	0x11cd		/* HAL Computer Systems */
#define	PCI_VENDOR_AURAVISION	0x11d1		/* Auravision */
#define	PCI_VENDOR_ANALOG	0x11d4		/* Analog Devices */
#define	PCI_VENDOR_SEGA	0x11db		/* SEGA Enterprises */
#define	PCI_VENDOR_ZORAN	0x11de		/* Zoran */
#define	PCI_VENDOR_QUICKLOGIC	0x11e3		/* QuickLogic */
#define	PCI_VENDOR_COMPEX	0x11f6		/* Compex */
#define	PCI_VENDOR_PMCSIERRA	0x11f8		/* PMC-Sierra */
#define	PCI_VENDOR_COMTROL	0x11fe		/* Comtrol */
#define	PCI_VENDOR_CYCLADES	0x120e		/* Cyclades */
#define	PCI_VENDOR_ESSENTIAL	0x120f		/* Essential Communications */
#define	PCI_VENDOR_O2MICRO	0x1217		/* O2 Micro */
#define	PCI_VENDOR_3DFX	0x121a		/* 3Dfx Interactive */
#define	PCI_VENDOR_ARIEL	0x1220		/* Ariel */
#define	PCI_VENDOR_HEURICON	0x1223		/* Heurikon/Computer Products */
#define	PCI_VENDOR_AZTECH	0x122d		/* Aztech */
#define	PCI_VENDOR_3DO	0x1239		/* The 3D0 Company */
#define	PCI_VENDOR_CCUBE	0x123f		/* C-Cube Microsystems */
#define	PCI_VENDOR_JNI	0x1242		/* JNI */
#define	PCI_VENDOR_AVM	0x1244		/* AVM */
#define	PCI_VENDOR_SAMSUNGELEC2	0x1249		/* Samsung Electronics (2nd vendor ID) */
#define	PCI_VENDOR_STALLION	0x124d		/* Stallion Technologies */
#define	PCI_VENDOR_LINEARSYS	0x1254		/* Linear Systems */
#define	PCI_VENDOR_COREGA	0x1259		/* Corega */
#define	PCI_VENDOR_ASIX	0x125b		/* ASIX Electronics */
#define	PCI_VENDOR_AURORA	0x125c		/* Aurora Technologies */
#define	PCI_VENDOR_ESSTECH	0x125d		/* ESS Technology */
#define	PCI_VENDOR_INTERSIL	0x1260		/* Intersil */
#define	PCI_VENDOR_NORTEL	0x126c		/* Nortel Networks (Northern Telecom) */
#define	PCI_VENDOR_SILMOTION	0x126f		/* Silicon Motion */
#define	PCI_VENDOR_ENSONIQ	0x1274		/* Ensoniq */
#define	PCI_VENDOR_NETAPP	0x1275		/* Network Appliance */
#define	PCI_VENDOR_TRANSMETA	0x1279		/* Transmeta */
#define	PCI_VENDOR_ROCKWELL	0x127a		/* Rockwell Semiconductor Systems */
#define	PCI_VENDOR_DAVICOM	0x1282		/* Davicom Semiconductor */
#define	PCI_VENDOR_ITE	0x1283		/* Integrated Technology Express */
#define	PCI_VENDOR_ESSTECH2	0x1285		/* ESS Technology */
#define	PCI_VENDOR_TRITECH	0x1292		/* TriTech Microelectronics */
#define	PCI_VENDOR_KOFAX	0x1296		/* Kofax Image Products */
#define	PCI_VENDOR_ALTEON	0x12ae		/* Alteon */
#define	PCI_VENDOR_RISCOM	0x12aa		/* RISCom */
#define	PCI_VENDOR_USR	0x12b9		/* US Robotics (3Com) */
#define	PCI_VENDOR_USR2	0x16ec		/* US Robotics */
#define	PCI_VENDOR_PICTUREEL	0x12c5		/* Picture Elements */
#define	PCI_VENDOR_NVIDIA_SGS	0x12d2		/* Nvidia & SGS-Thomson Microelectronics */
#define	PCI_VENDOR_PERICOM	0x12d8		/* Pericom Semiconductors */
#define	PCI_VENDOR_RAINBOW	0x12de		/* Rainbow Technologies */
#define	PCI_VENDOR_DATUM	0x12e2		/* Datum Inc. Bancomm-Timing Division */
#define	PCI_VENDOR_AUREAL	0x12eb		/* Aureal Semiconductor */
#define	PCI_VENDOR_JUNIPER	0x1304		/* Juniper Networks */
#define	PCI_VENDOR_ADMTEK	0x1317		/* ADMtek */
#define	PCI_VENDOR_PACKETENGINES	0x1318		/* Packet Engines */
#define	PCI_VENDOR_FORTEMEDIA	0x1319		/* Forte Media */
#define	PCI_VENDOR_SIIG	0x131f		/* Siig */
#define	PCI_VENDOR_MICROMEMORY	0x1332		/* Micro Memory */
#define	PCI_VENDOR_DOMEX	0x134a		/* Domex */
#define	PCI_VENDOR_QUATECH	0x135c		/* Quatech */
#define	PCI_VENDOR_LMC	0x1376		/* LAN Media */
#define	PCI_VENDOR_NETGEAR	0x1385		/* Netgear */
#define	PCI_VENDOR_MOXA	0x1393		/* Moxa Technologies */
#define	PCI_VENDOR_LEVELONE	0x1394		/* Level One */
#define	PCI_VENDOR_COLOGNECHIP	0x1397		/* Cologne Chip Designs */
#define	PCI_VENDOR_ALACRITECH	0x139a		/* Alacritech */
#define	PCI_VENDOR_HIFN	0x13a3		/* Hifn */
#define	PCI_VENDOR_EXAR	0x13a8		/* EXAR */
#define	PCI_VENDOR_3WARE	0x13c1		/* 3ware */
#define	PCI_VENDOR_ABOCOM	0x13d1		/* AboCom Systems */
#define	PCI_VENDOR_PHOBOS	0x13d8		/* Phobos */
#define	PCI_VENDOR_NETBOOST	0x13dc		/* NetBoost */
#define	PCI_VENDOR_SUNDANCETI	0x13f0		/* Sundance Technology */
#define	PCI_VENDOR_CMEDIA	0x13f6		/* C-Media Electronics */
#define	PCI_VENDOR_ADVANTECH	0x13fe		/* Advantech */
#define	PCI_VENDOR_LAVA	0x1407		/* Lava Semiconductor Manufacturing */
#define	PCI_VENDOR_SUNIX	0x1409		/* SUNIX */
#define	PCI_VENDOR_ICENSEMBLE	0x1412		/* IC Ensemble / VIA Technologies */
#define	PCI_VENDOR_MICROSOFT	0x1414		/* Microsoft */
#define	PCI_VENDOR_OXFORDSEMI	0x1415		/* Oxford Semiconductor */
#define	PCI_VENDOR_CHELSIO	0x1425		/* Chelsio Communications */
#define	PCI_VENDOR_TAMARACK	0x143d		/* Tamarack Microelectronics */
#define	PCI_VENDOR_SAMSUNGELEC3	0x144d		/* Samsung Electronics (3rd vendor ID) */
#define	PCI_VENDOR_ASKEY	0x144f		/* Askey Computer */
#define	PCI_VENDOR_AVERMEDIA	0x1461		/* Avermedia Technologies */
#define	PCI_VENDOR_SYSTEMBASE	0x14a1		/* System Base */
#define	PCI_VENDOR_MARVELL2	0x1b4b		/* Marvell */
#define	PCI_VENDOR_AIRONET	0x14b9		/* Aironet Wireless Communications */
#define	PCI_VENDOR_COMPAL	0x14c0		/* COMPAL Electronics */
#define	PCI_VENDOR_MYRICOM	0x14c1		/* Myricom */
#define	PCI_VENDOR_TITAN	0x14d2		/* Titan Electronics */
#define	PCI_VENDOR_AVLAB	0x14db		/* Avlab Technology */
#define	PCI_VENDOR_INVERTEX	0x14e1		/* Invertex */
#define	PCI_VENDOR_BROADCOM	0x14e4		/* Broadcom */
#define	PCI_VENDOR_PLANEX	0x14ea		/* Planex Communications */
#define	PCI_VENDOR_CONEXANT	0x14f1		/* Conexant Systems */
#define	PCI_VENDOR_DELTA	0x1500		/* Delta Electronics */
#define	PCI_VENDOR_ENE	0x1524		/* ENE Technology */
#define	PCI_VENDOR_TERRATEC	0x153b		/* TerraTec Electronic */
#define	PCI_VENDOR_PERLE	0x155f		/* Perle Systems */
#define	PCI_VENDOR_SOLIDUM	0x1588		/* Solidum Systems */
#define	PCI_VENDOR_HP2	0x1590		/* Hewlett-Packard */
#define	PCI_VENDOR_SYBA	0x1592		/* Syba */
#define	PCI_VENDOR_FARADAY	0x159b		/* Faraday Technology */
#define	PCI_VENDOR_GEOCAST	0x15a1		/* Geocast Network Systems */
#define	PCI_VENDOR_BLUESTEEL	0x15ab		/* Bluesteel Networks */
#define	PCI_VENDOR_VMWARE	0x15ad		/* VMware */
#define	PCI_VENDOR_AGILENT	0x15bc		/* Agilent Technologies */
#define	PCI_VENDOR_EUMITCOM	0x1638		/* Eumitcom */
#define	PCI_VENDOR_NETSEC	0x1660		/* NetSec */
#define	PCI_VENDOR_SIBYTE	0x166d		/* Broadcom (SiByte) */
#define	PCI_VENDOR_MYSON	0x1516		/* Myson-Century Technology */
#define	PCI_VENDOR_MELLANOX	0x15b3		/* Mellanox Technologies */
#define	PCI_VENDOR_NDC	0x15e8		/* National Datacomm */
#define	PCI_VENDOR_ACTIONTEC	0x1668		/* Action Tec Electronics */
#define	PCI_VENDOR_ATHEROS	0x168c		/* Atheros Communications */
#define	PCI_VENDOR_GLOBALSUN	0x16ab		/* Global Sun Tech */
#define	PCI_VENDOR_SAFENET	0x16ae		/* SafeNet */
#define	PCI_VENDOR_MICREL	0x16c6		/* Micrel */
#define	PCI_VENDOR_NETOCTAVE	0x170b		/* Netoctave */
#define	PCI_VENDOR_LINKSYS	0x1737		/* Linksys */
#define	PCI_VENDOR_ALTIMA	0x173b		/* Altima */
#define	PCI_VENDOR_ANTARES	0x1754		/* Antares Microsystems */
#define	PCI_VENDOR_CAVIUM	0x177d		/* Cavium */
#define	PCI_VENDOR_FZJZEL	0x1796		/* FZ Juelich / ZEL */
#define	PCI_VENDOR_BELKIN	0x1799		/* Belkin */
#define	PCI_VENDOR_HAWKING	0x17b3		/* Hawking Technology */
#define	PCI_VENDOR_SANDBURST	0x17ba		/* Sandburst */
#define	PCI_VENDOR_NETCHIP	0x17cc		/* PLX Technology (NetChip) */
#define	PCI_VENDOR_I4	0x17cf		/* I4 */
#define	PCI_VENDOR_ARECA	0x17d3		/* Areca */
#define	PCI_VENDOR_S2IO	0x17d5		/* S2io Technologies */
#define	PCI_VENDOR_RDC	0x17F3		/* RDC Semiconductor */
#define	PCI_VENDOR_LINKSYS2	0x17fe		/* Linksys */
#define	PCI_VENDOR_RALINK	0x1814		/* Ralink Technologies */
#define	PCI_VENDOR_RMI	0x182e		/* Raza Microelectronics Inc. (Broadcom) */
#define	PCI_VENDOR_NETLOGIC	0x184e		/* Netlogic Microsystems (Broadcom) */
#define	PCI_VENDOR_BBELEC	0x1896		/* B & B Electronics */
#define	PCI_VENDOR_XGI	0x18ca		/* XGI Technology */
#define	PCI_VENDOR_RENESAS	0x1912		/* Renesas Technologies */
#define	PCI_VENDOR_FREESCALE	0x1957		/* Freescale Semiconductor */
#define	PCI_VENDOR_ATTANSIC	0x1969		/* Attansic Technologies */
#define	PCI_VENDOR_JMICRON	0x197b		/* JMicron Technology */
#define	PCI_VENDOR_ASPEED	0x1a03		/* ASPEED Technology */
#define	PCI_VENDOR_EVE	0x1adb		/* EVE */
#define	PCI_VENDOR_QUMRANET	0x1af4		/* Qumranet */
#define	PCI_VENDOR_ASMEDIA	0x1b21		/* ASMedia */
#define	PCI_VENDOR_REDHAT	0x1b36		/* Red Hat */
#define	PCI_VENDOR_FRESCO	0x1b73		/* Fresco Logic */
#define	PCI_VENDOR_QINHENG2	0x1c00		/* Nanjing QinHeng Electronics (PCIe) */
#define	PCI_VENDOR_SYMPHONY2	0x1c1c		/* Symphony Labs (2nd PCI Vendor ID) */
#define	PCI_VENDOR_TEKRAM2	0x1de1		/* Tekram Technology (2nd PCI Vendor ID) */
#define	PCI_VENDOR_SUNIX2	0x1fd4		/* SUNIX Co */
#define	PCI_VENDOR_HINT	0x3388		/* HiNT */
#define	PCI_VENDOR_3DLABS	0x3d3d		/* 3D Labs */
#define	PCI_VENDOR_AVANCE2	0x4005		/* Avance Logic (2nd PCI Vendor ID) */
#define	PCI_VENDOR_ADDTRON	0x4033		/* Addtron Technology */
#define	PCI_VENDOR_QINHENG	0x4348		/* Nanjing QinHeng Electronics */
#define	PCI_VENDOR_ICOMPRESSION	0x4444		/* Conexant (iCompression) */
#define	PCI_VENDOR_INDCOMPSRC	0x494f		/* Industrial Computer Source */
#define	PCI_VENDOR_NETVIN	0x4a14		/* NetVin */
#define	PCI_VENDOR_BUSLOGIC2	0x4b10		/* Buslogic (2nd PCI Vendor ID) */
#define	PCI_VENDOR_MEDIAQ	0x4d51		/* MediaQ */
#define	PCI_VENDOR_GUILLEMOT	0x5046		/* Guillemot */
#define	PCI_VENDOR_TURTLE_BEACH	0x5053		/* Turtle Beach */
#define	PCI_VENDOR_S3	0x5333		/* S3 */
#define	PCI_VENDOR_NETPOWER2	0x5700		/* NetPower (2nd PCI Vendor ID) */
#define	PCI_VENDOR_XENSOURCE	0x5853		/* XenSource, Inc. */
#define	PCI_VENDOR_C4T	0x6374		/* c't Magazin */
#define	PCI_VENDOR_DCI	0x6666		/* Decision Computer */
#define	PCI_VENDOR_KURUSUGAWA	0x6809		/* Kurusugawa Electronics */
#define	PCI_VENDOR_PCHDTV	0x7063		/* pcHDTV */
#define	PCI_VENDOR_QUANCOM	0x8008		/* QUANCOM Electronic GmbH */
#define	PCI_VENDOR_INTEL	0x8086		/* Intel */
#define	PCI_VENDOR_VIRTUALBOX	0x80ee		/* VirtualBox */
#define	PCI_VENDOR_TRIGEM2	0x8800		/* Trigem Computer (2nd PCI Vendor ID) */
#define	PCI_VENDOR_PROLAN	0x8c4a		/* ProLAN */
#define	PCI_VENDOR_COMPUTONE	0x8e0e		/* Computone */
#define	PCI_VENDOR_KTI	0x8e2e		/* KTI */
#define	PCI_VENDOR_ADP	0x9004		/* Adaptec */
#define	PCI_VENDOR_ADP2	0x9005		/* Adaptec (2nd PCI Vendor ID) */
#define	PCI_VENDOR_ATRONICS	0x907f		/* Atronics */
#define	PCI_VENDOR_NETMOS	0x9710		/* Netmos */
#define	PCI_VENDOR_PARALLELS	0xaaaa		/* Parallels */
#define	PCI_VENDOR_CHRYSALIS	0xcafe		/* Chrysalis-ITS */
#define	PCI_VENDOR_MIDDLE_DIGITAL	0xdeaf		/* Middle Digital */
#define	PCI_VENDOR_ARC	0xedd8		/* ARC Logic */
#define	PCI_VENDOR_INVALID	0xffff		/* INVALID VENDOR ID */

/*
 * List of known products. Grouped by vendor.
 */

/* 3COM Products */
#define	PCI_PRODUCT_3COM_3C985	0x0001		/* 3c985 Gigabit Ethernet */
#define	PCI_PRODUCT_3COM_3C996	0x0003		/* 3c996 10/100/1000 Ethernet */
#define	PCI_PRODUCT_3COM_3C556MODEM	0x1007		/* 3c556 V.90 Mini-PCI Modem */
#define	PCI_PRODUCT_3COM_3C940	0x1700		/* 3c940 Gigabit Ethernet */
#define	PCI_PRODUCT_3COM_3C339	0x3390		/* 3c339 TokenLink Velocity */
#define	PCI_PRODUCT_3COM_3C359	0x3590		/* 3c359 TokenLink Velocity XL */
#define	PCI_PRODUCT_3COM_3C450TX	0x4500		/* 3c450-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C575TX	0x5057		/* 3c575-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C575BTX	0x5157		/* 3CCFE575BT 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C575CTX	0x5257		/* 3CCFE575CT 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C590	0x5900		/* 3c590 Ethernet */
#define	PCI_PRODUCT_3COM_3C595TX	0x5950		/* 3c595-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C595T4	0x5951		/* 3c595-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C595MII	0x5952		/* 3c595-MII 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C555	0x5055		/* 3c555 10/100 Mini-PCI Ethernet */
#define	PCI_PRODUCT_3COM_3C154G72	0x6001		/* 3CRWE154G72 Wireless LAN Adapter */
#define	PCI_PRODUCT_3COM_3C556	0x6055		/* 3c556 10/100 Mini-PCI Ethernet */
#define	PCI_PRODUCT_3COM_3C556B	0x6056		/* 3c556B 10/100 Mini-PCI Ethernet */
#define	PCI_PRODUCT_3COM_3C656_E	0x6560		/* 3CCFEM656 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C656_M	0x6561		/* 3CCFEM656 56k Modem */
#define	PCI_PRODUCT_3COM_3C656B_E	0x6562		/* 3CCFEM656B 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C656B_M	0x6563		/* 3CCFEM656B 56k Modem */
#define	PCI_PRODUCT_3COM_3C656C_E	0x6564		/* 3CXFEM656C 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C656C_M	0x6565		/* 3CXFEM656C 56k Modem */
#define	PCI_PRODUCT_3COM_3CSOHO100TX	0x7646		/* 3cSOHO100-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3CRWE777A	0x7770		/* 3crwe777a AirConnect */
#define	PCI_PRODUCT_3COM_3C804	0x7980		/* 3c804 FDDILink SAS */
#define	PCI_PRODUCT_3COM_TOKEN	0x8811		/* Token Ring */
#define	PCI_PRODUCT_3COM_3C900TPO	0x9000		/* 3c900-TPO Ethernet */
#define	PCI_PRODUCT_3COM_3C900COMBO	0x9001		/* 3c900-COMBO Ethernet */
#define	PCI_PRODUCT_3COM_3C905TX	0x9050		/* 3c905-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905T4	0x9051		/* 3c905-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C900BTPO	0x9004		/* 3c900B-TPO Ethernet */
#define	PCI_PRODUCT_3COM_3C900BCOMBO	0x9005		/* 3c900B-COMBO Ethernet */
#define	PCI_PRODUCT_3COM_3C900BTPC	0x9006		/* 3c900B-TPC Ethernet */
#define	PCI_PRODUCT_3COM_3C905BTX	0x9055		/* 3c905B-TX 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BT4	0x9056		/* 3c905B-T4 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BCOMBO	0x9058		/* 3c905B-COMBO 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905BFX	0x905a		/* 3c905B-FX 100 Ethernet */
#define	PCI_PRODUCT_3COM_3C905CTX	0x9200		/* 3c905C-TX 10/100 Ethernet w/ mngmt */
#define	PCI_PRODUCT_3COM_3C905CXTX	0x9201		/* 3c905CX-TX 10/100 Ethernet w/ mngmt */
#define	PCI_PRODUCT_3COM_3C920BEMBW	0x9202		/* 3c920B-EMB-WNM Integrated Fast Ethernet */
#define	PCI_PRODUCT_3COM_3C910SOHOB	0x9300		/* 3c910 OfficeConnect 10/100B Ethernet */
#define	PCI_PRODUCT_3COM_3C980SRV	0x9800		/* 3c980 Server Adapter 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3C980CTXM	0x9805		/* 3c980C-TXM 10/100 Ethernet */
#define	PCI_PRODUCT_3COM_3CR990	0x9900		/* 3c990-TX 10/100 Ethernet w/ 3XP */
#define	PCI_PRODUCT_3COM_3CR990TX95	0x9902		/* 3CR990-TX-95 10/100 Ethernet w/ 3XP */
#define	PCI_PRODUCT_3COM_3CR990TX97	0x9903		/* 3CR990-TX-97 10/100 Ethernet w/ 3XP */
#define	PCI_PRODUCT_3COM_3C990B	0x9904		/* 3c990B 10/100 Ethernet w/ 3XP */
#define	PCI_PRODUCT_3COM_3CR990FX	0x9905		/* 3CR990-FX 100 Ethernet w/ 3XP */
#define	PCI_PRODUCT_3COM_3CR990SVR95	0x9908		/* 3CR990-SVR-95 10/100 Ethernet w/ 3XP */
#define	PCI_PRODUCT_3COM_3CR990SVR97	0x9909		/* 3CR990-SVR-97 10/100 Ethernet w/ 3XP */
#define	PCI_PRODUCT_3COM_3C990BSVR	0x990a		/* 3c990BSVR 10/100 Ethernet w/ 3XP */

/* 3Dfx Interactive products */
#define	PCI_PRODUCT_3DFX_VOODOO	0x0001		/* Voodoo */
#define	PCI_PRODUCT_3DFX_VOODOO2	0x0002		/* Voodoo2 */
#define	PCI_PRODUCT_3DFX_BANSHEE	0x0003		/* Banshee */
#define	PCI_PRODUCT_3DFX_VOODOO3	0x0005		/* Voodoo3 */
#define	PCI_PRODUCT_3DFX_VOODOO5	0x0009		/* Voodoo 4/5 */

/* 3D Labs products */
#define	PCI_PRODUCT_3DLABS_300SX	0x0001		/* GLINT 300SX */
#define	PCI_PRODUCT_3DLABS_500TX	0x0002		/* GLINT 500TX */
#define	PCI_PRODUCT_3DLABS_DELTA	0x0003		/* GLINT DELTA */
#define	PCI_PRODUCT_3DLABS_PERMEDIA	0x0004		/* GLINT Permedia */
#define	PCI_PRODUCT_3DLABS_500MX	0x0006		/* GLINT 500MX */
#define	PCI_PRODUCT_3DLABS_PERMEDIA2	0x0007		/* GLINT Permedia 2 */
#define	PCI_PRODUCT_3DLABS_GAMMA	0x0008		/* GLINT GAMMA */
#define	PCI_PRODUCT_3DLABS_PERMEDIA2V	0x0009		/* GLINT Permedia 2V */
#define	PCI_PRODUCT_3DLABS_PERMEDIA3	0x000a		/* GLINT Permedia 3 */
#define	PCI_PRODUCT_3DLABS_WILDCAT5110	0x07a2		/* WILDCAT 5110 */

/* 3ware products */
#define	PCI_PRODUCT_3WARE_ESCALADE	0x1000		/* Escalade ATA RAID Controller */
#define	PCI_PRODUCT_3WARE_ESCALADE_ASIC	0x1001		/* Escalade ATA RAID 7000/8000 Series Controller */
#define	PCI_PRODUCT_3WARE_9000	0x1002		/* 9000 Series RAID */
#define	PCI_PRODUCT_3WARE_9550	0x1003		/* 9550 Series RAID */
#define	PCI_PRODUCT_3WARE_9650	0x1004		/* 9650 Series RAID */
#define	PCI_PRODUCT_3WARE_9690	0x1005		/* 9690 Series RAID */
#define	PCI_PRODUCT_3WARE_9750	0x1010		/* 9750 Series RAID */

/* AboCom products */
#define	PCI_PRODUCT_ABOCOM_FE2500	0xab02		/* FE2500 10/100 Ethernet */
#define	PCI_PRODUCT_ABOCOM_PCM200	0xab03		/* PCM200 10/100 Ethernet */
#define	PCI_PRODUCT_ABOCOM_FE2000VX	0xab06		/* FE2000VX 10/100 Ethernet (OEM) */
#define	PCI_PRODUCT_ABOCOM_FE2500MX	0xab08		/* FE2500MX 10/100 Ethernet */

/* ACC Products */
#define	PCI_PRODUCT_ACC_2188	0x0000		/* ACCM 2188 VL-PCI Bridge */
#define	PCI_PRODUCT_ACC_2051_HB	0x2051		/* 2051 PCI Single Chip Solution (host Bridge) */
#define	PCI_PRODUCT_ACC_2051_ISA	0x5842		/* 2051 PCI Single Chip Solution (ISA Bridge) */

/* Acard products */
#define	PCI_PRODUCT_ACARD_ATP850U	0x0005		/* ATP850U/UF UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_ATP860	0x0006		/* ATP860 UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_ATP860A	0x0007		/* ATP860-A UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_ATP865	0x0008		/* ATP865 UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_ATP865A	0x0009		/* ATP865-A UDMA IDE Controller */
#define	PCI_PRODUCT_ACARD_AEC6710	0x8002		/* AEC6710 SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712UW	0x8010		/* AEC6712UW SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712U	0x8020		/* AEC6712U SCSI */
#define	PCI_PRODUCT_ACARD_AEC6712S	0x8030		/* AEC6712S SCSI */
#define	PCI_PRODUCT_ACARD_AEC6710D	0x8040		/* AEC6710D SCSI */
#define	PCI_PRODUCT_ACARD_AEC6715UW	0x8050		/* AEC6715UW SCSI */

/* Accton products */
#define	PCI_PRODUCT_ACCTON_MPX5030	0x1211		/* MPX 5030/5038 Ethernet */
#define	PCI_PRODUCT_ACCTON_EN2242	0x1216		/* EN2242 10/100 Ethernet */

/* Acer products */
#define	PCI_PRODUCT_ACER_M1435	0x1435		/* M1435 VL-PCI Bridge */

/* Advantech products */
#define	PCI_PRODUCT_ADVANTECH_PCI1600	0x1600		/* PCI-16[12]0 serial */
#define	PCI_PRODUCT_ADVANTECH_PCI1604	0x1604		/* PCI-1604 serial */
#define	PCI_PRODUCT_ADVANTECH_PCI1610	0x1610		/* PCI-1610 4 port serial */
#define	PCI_PRODUCT_ADVANTECH_PCI1612	0x1612		/* PCI-1612 4 port serial */
#define	PCI_PRODUCT_ADVANTECH_PCI1620	0x1620		/* PCI-1620 8 port serial (1-4) */
#define	PCI_PRODUCT_ADVANTECH_PCI1620_1	0x16ff		/* PCI-1620 8 port serial (5-8) */

/* Acer Labs products */
#define	PCI_PRODUCT_ALI_M1445	0x1445		/* M1445 VL-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1449	0x1449		/* M1449 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M1451	0x1451		/* M1451 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1461	0x1461		/* M1461 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1531	0x1531		/* M1531 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1533	0x1533		/* M1533 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M1541	0x1541		/* M1541 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1543	0x1543		/* M1543 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M1563	0x1563		/* M1563 PCI-ISA Bridge */
#define	PCI_PRODUCT_ALI_M1647	0x1647		/* M1647 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M1689	0x1689		/* M1689 Host-PCI Bridge */
#define	PCI_PRODUCT_ALI_M3309	0x3309		/* M3309 MPEG Decoder */
#define	PCI_PRODUCT_ALI_M4803	0x5215		/* M4803 */
#define	PCI_PRODUCT_ALI_M5257	0x5257		/* M5257 PCI Software Modem */
#define	PCI_PRODUCT_ALI_M5229	0x5229		/* M5229 UDMA IDE Controller */
#define	PCI_PRODUCT_ALI_M5237	0x5237		/* M5237 USB 1.1 Host Controller */
#define	PCI_PRODUCT_ALI_M5239	0x5239		/* M5239 USB 2.0 Host Controller */
#define	PCI_PRODUCT_ALI_M5243	0x5243		/* M5243 PCI-AGP Bridge */
#define	PCI_PRODUCT_ALI_M5247	0x5247		/* M5247 PCI-AGP Bridge */
#define	PCI_PRODUCT_ALI_M5249	0x5249		/* M5249 Hypertransport to PCI Bridge */
#define	PCI_PRODUCT_ALI_M5261	0x5261		/* M5261 Tulip Ethernet Controller */
#define	PCI_PRODUCT_ALI_M5288	0x5288		/* M5288 SATA/Raid Controller */
#define	PCI_PRODUCT_ALI_M5451	0x5451		/* M5451 AC-Link Controller Audio Device */
#define	PCI_PRODUCT_ALI_M5453	0x5453		/* M5453 AC-Link Controller Modem Device */
#define	PCI_PRODUCT_ALI_M5455	0x5455		/* M5455 AC-Link Controller Audio Device */
#define	PCI_PRODUCT_ALI_M7101	0x7101		/* M7101 Power Management Controller */

/* Adaptec products */
#define	PCI_PRODUCT_ADP_AIC1160	0x1160		/* AIC-1160 */
#define	PCI_PRODUCT_ADP_AIC7850	0x5078		/* AIC-7850 */
#define	PCI_PRODUCT_ADP_AIC7855	0x5578		/* AIC-7855 */
#define	PCI_PRODUCT_ADP_AIC5900	0x5900		/* AIC-5900 ATM */
#define	PCI_PRODUCT_ADP_AIC5905	0x5905		/* AIC-5905 ATM */
#define	PCI_PRODUCT_ADP_AIC6915	0x6915		/* AIC-6915 10/100 Ethernet */
#define	PCI_PRODUCT_ADP_AIC7860	0x6078		/* AIC-7860 */
#define	PCI_PRODUCT_ADP_APA1480	0x6075		/* APA-1480 Ultra */
#define	PCI_PRODUCT_ADP_2940AU	0x6178		/* AHA-2940A Ultra */
#define	PCI_PRODUCT_ADP_AIC7870	0x7078		/* AIC-7870 */
#define	PCI_PRODUCT_ADP_2940	0x7178		/* AHA-2940 */
#define	PCI_PRODUCT_ADP_3940	0x7278		/* AHA-3940 */
#define	PCI_PRODUCT_ADP_3985	0x7378		/* AHA-3985 */
#define	PCI_PRODUCT_ADP_2944	0x7478		/* AHA-2944 */
#define	PCI_PRODUCT_ADP_AIC7895	0x7895		/* AIC-7895 Ultra */
#define	PCI_PRODUCT_ADP_AIC7880	0x8078		/* AIC-7880 Ultra */
#define	PCI_PRODUCT_ADP_2940U	0x8178		/* AHA-2940 Ultra */
#define	PCI_PRODUCT_ADP_3940U	0x8278		/* AHA-3940 Ultra */
#define	PCI_PRODUCT_ADP_389XU	0x8378		/* AHA-389X Ultra */
#define	PCI_PRODUCT_ADP_2944U	0x8478		/* AHA-2944 Ultra */
#define	PCI_PRODUCT_ADP_2940UP	0x8778		/* AHA-2940 Ultra Pro */

#define	PCI_PRODUCT_ADP2_2940U2	0x0010		/* AHA-2940U2 U2 */
#define	PCI_PRODUCT_ADP2_2930U2	0x0011		/* AHA-2930U2 U2 */
#define	PCI_PRODUCT_ADP2_AIC7890	0x001f		/* AIC-7890/1 U2 */
#define	PCI_PRODUCT_ADP2_3950U2B	0x0050		/* AHA-3950U2B U2 */
#define	PCI_PRODUCT_ADP2_3950U2D	0x0051		/* AHA-3950U2D U2 */
#define	PCI_PRODUCT_ADP2_AIC7896	0x005f		/* AIC-7896/7 U2 */
#define	PCI_PRODUCT_ADP2_AIC7892A	0x0080		/* AIC-7892A U160 */
#define	PCI_PRODUCT_ADP2_AIC7892B	0x0081		/* AIC-7892B U160 */
#define	PCI_PRODUCT_ADP2_AIC7892D	0x0083		/* AIC-7892D U160 */
#define	PCI_PRODUCT_ADP2_AIC7892P	0x008f		/* AIC-7892P U160 */
#define	PCI_PRODUCT_ADP2_AIC7899A	0x00c0		/* AIC-7899A U160 */
#define	PCI_PRODUCT_ADP2_AIC7899B	0x00c1		/* AIC-7899B U160 */
#define	PCI_PRODUCT_ADP2_AIC7899D	0x00c3		/* AIC-7899D U160 */
#define	PCI_PRODUCT_ADP2_AIC7899F	0x00c5		/* AIC-7899F RAID */
#define	PCI_PRODUCT_ADP2_AIC7899P	0x00cf		/* AIC-7899P U160 */
#define	PCI_PRODUCT_ADP2_1420SA	0x0241		/* RAID 1420SA */
#define	PCI_PRODUCT_ADP2_1430SA	0x0243		/* RAID 1430SA */
#define	PCI_PRODUCT_ADP2_AAC2622	0x0282		/* AAC-2622 */
#define	PCI_PRODUCT_ADP2_ASR2200S	0x0285		/* ASR-2200S */
#define	PCI_PRODUCT_ADP2_ASR2120S	0x0286		/* ASR-2120S */
#define	PCI_PRODUCT_ADP2_ASR2200S_SUB2M	0x0287		/* ASR-2200S */
#define	PCI_PRODUCT_ADP2_ASR2410SA	0x0290		/* ASR-2410SA */
#define	PCI_PRODUCT_ADP2_AAR2810SA	0x0292		/* AAR-2810SA */
#define	PCI_PRODUCT_ADP2_3405	0x02bb		/* RAID 3405 */
#define	PCI_PRODUCT_ADP2_3805	0x02bc		/* RAID 3805 */
#define	PCI_PRODUCT_ADP2_2405	0x02d5		/* RAID 2405 */
#define	PCI_PRODUCT_ADP2_AAC364	0x0364		/* AAC-364 */
#define	PCI_PRODUCT_ADP2_ASR5400S	0x0365		/* ASR-5400S */
#define	PCI_PRODUCT_ADP2_PERC_2QC	0x1364		/* Dell PERC 2/QC */
/* XXX guess */
#define	PCI_PRODUCT_ADP2_PERC_3QC	0x1365		/* Dell PERC 3/QC */
#define	PCI_PRODUCT_ADP2_HP_M110_G2	0x3227		/* HP M110 G2 / ASR-2610SA */
#define	PCI_PRODUCT_ADP2_SERVERAID	0x0250		/* ServeRAID 6/7 (marco) */

/* Addtron Products */
#define	PCI_PRODUCT_ADDTRON_8139	0x1360		/* 8139 Ethernet */
#define	PCI_PRODUCT_ADDTRON_RHINEII	0x1320		/* Rhine II 10/100 Ethernet */

/* ADMtek products */
#define	PCI_PRODUCT_ADMTEK_AL981	0x0981		/* AL981 (Comet) 10/100 Ethernet */
#define	PCI_PRODUCT_ADMTEK_AN983	0x0985		/* AN983 (Centaur-P) 10/100 Ethernet */
#define	PCI_PRODUCT_ADMTEK_AN985	0x1985		/* AN985 (Centaur-C) 10/100 Ethernet */
#define	PCI_PRODUCT_ADMTEK_ADM5120	0x5120		/* Infineon ADM5120 PCI Host Bridge */
#define	PCI_PRODUCT_ADMTEK_ADM8211	0x8201		/* ADM8211 11Mbps 802.11b WLAN */
#define	PCI_PRODUCT_ADMTEK_ADM9511	0x9511		/* ADM9511 (Centaur-II) 10/100 Ethernet */
#define	PCI_PRODUCT_ADMTEK_ADM9513	0x9513		/* ADM9513 (Centaur-II) 10/100 Ethernet */

/* Advanced System Products */
#define	PCI_PRODUCT_ADVSYS_1200A	0x1100
#define	PCI_PRODUCT_ADVSYS_1200B	0x1200
#define	PCI_PRODUCT_ADVSYS_ULTRA	0x1300		/* ABP-930/40UA */
#define	PCI_PRODUCT_ADVSYS_WIDE	0x2300		/* ABP-940UW */
#define	PCI_PRODUCT_ADVSYS_U2W	0x2500		/* ASB-3940U2W */
#define	PCI_PRODUCT_ADVSYS_U3W	0x2700		/* ASB-3940U3W */

/* Agilent Technologies Products */
#define	PCI_PRODUCT_AGILENT_TACHYON_DX2	0x0100		/* Tachyon DX2 FC Controller */

/* Aironet Wireless Communicasions products */
#define	PCI_PRODUCT_AIRONET_PC4xxx	0x0001		/* PC4500/PC4800 Wireless LAN Adapter */
#define	PCI_PRODUCT_AIRONET_PCI350	0x0350		/* PCI350 Wireless LAN Adapter */
#define	PCI_PRODUCT_AIRONET_MPI350	0xa504		/* MPI350 Mini-PCI Wireless LAN Adapter */
#define	PCI_PRODUCT_AIRONET_PC4500	0x4500		/* PC4500 Wireless LAN Adapter */
#define	PCI_PRODUCT_AIRONET_PC4800	0x4800		/* PC4800 Wireless LAN Adapter */

/* Alacritech products */
#define	PCI_PRODUCT_ALACRITECH_SES1001T	0x0005		/* SES1001T iSCSI Accelerator */

/* Alliance products */
#define	PCI_PRODUCT_ALLIANCE_AT24	0x6424		/* AT24 */
#define	PCI_PRODUCT_ALLIANCE_AT25	0x643d		/* AT25 */

/* Alteon products */
#define	PCI_PRODUCT_ALTEON_ACENIC	0x0001		/* ACEnic 1000baseSX Ethernet */
#define	PCI_PRODUCT_ALTEON_ACENIC_COPPER	0x0002		/* ACEnic 1000baseT Ethernet */
#define	PCI_PRODUCT_ALTEON_BCM5700	0x0003		/* ACEnic BCM5700 10/100/1000 Ethernet */
#define	PCI_PRODUCT_ALTEON_BCM5701	0x0004		/* ACEnic BCM5701 10/100/1000 Ethernet */

/* Altera products */
#define	PCI_PRODUCT_ALTERA_EP4CGX15BF14C8N	0x4c15		/* EP4CGX15BF14C8N */

/* Altima products */
#define	PCI_PRODUCT_ALTIMA_AC1000	0x03e8		/* AC1000 Gigabit Ethernet */
#define	PCI_PRODUCT_ALTIMA_AC1001	0x03e9		/* AC1001 Gigabit Ethernet */
#define	PCI_PRODUCT_ALTIMA_AC9100	0x03ea		/* AC9100 Gigabit Ethernet */
#define	PCI_PRODUCT_ALTIMA_AC1003	0x03eb		/* AC1003 Gigabit Ethernet */

/* AMD products */
#define	PCI_PRODUCT_AMD_AMD64_HT	0x1100		/* K8 AMD64 HyperTransport Configuration */
#define	PCI_PRODUCT_AMD_AMD64_ADDR	0x1101		/* K8 AMD64 Address Map Configuration */
#define	PCI_PRODUCT_AMD_AMD64_DRAM	0x1102		/* K8 AMD64 DRAM Configuration */
#define	PCI_PRODUCT_AMD_AMD64_MISC	0x1103		/* K8 AMD64 Miscellaneous Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F10_HT	0x1200		/* AMD64 Family10h HyperTransport Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F10_ADDR	0x1201		/* AMD64 Family10h Address Map Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F10_DRAM	0x1202		/* AMD64 Family10h DRAM Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F10_MISC	0x1203		/* AMD64 Family10h Miscellaneous Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F10_LINK	0x1204		/* AMD64 Family10h Link Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F11_HT	0x1300		/* AMD64 Family11h HyperTransport Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F11_ADDR	0x1301		/* AMD64 Family11h Address Map Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F11_DRAM	0x1302		/* AMD64 Family11h DRAM Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F11_MISC	0x1303		/* AMD64 Family11h Miscellaneous Configuration */
#define	PCI_PRODUCT_AMD_AMD64_F11_LINK	0x1304		/* AMD64 Family11h Link Configuration */
#define	PCI_PRODUCT_AMD_F14_RC	0x1510		/* Family14h Root Complex */
#define	PCI_PRODUCT_AMD_F15_HT	0x1600		/* Family15h HyperTransport Configuration */
#define	PCI_PRODUCT_AMD_F15_ADDR	0x1601		/* Family15h Address Map Configuration */
#define	PCI_PRODUCT_AMD_F15_DRAM	0x1602		/* Family15h DRAM Configuration */
#define	PCI_PRODUCT_AMD_F15_MISC	0x1603		/* Family15h Miscellaneous Configuration */
#define	PCI_PRODUCT_AMD_F15_LINK	0x1604		/* Family15h Link Configuration */
#define	PCI_PRODUCT_AMD_F15_NB	0x1605		/* Family15h North Bridge Configuration */
#define	PCI_PRODUCT_AMD_F14_HT	0x1700		/* Family12h/14h HyperTransport Configuration */
#define	PCI_PRODUCT_AMD_F14_ADDR	0x1701		/* Family12h/14h Address Map Configuration */
#define	PCI_PRODUCT_AMD_F14_DRAM	0x1702		/* Family12h/14h DRAM Configuration */
#define	PCI_PRODUCT_AMD_F14_NB	0x1703		/* Family12h/14h North Bridge Configuration */
#define	PCI_PRODUCT_AMD_F14_CSTATE	0x1704		/* Family12h/14h CPU C-state Configuration */
#define	PCI_PRODUCT_AMD_F12_RC	0x1705		/* Family12h Root Complex */
#define	PCI_PRODUCT_AMD_F12_GPP0	0x1709		/* Family12h GPP0 Root Port */
#define	PCI_PRODUCT_AMD_F14_MISC	0x1716		/* Family12h/14h Misc. Configuration */
#define	PCI_PRODUCT_AMD_F14_HB18	0x1718		/* Family12h/14h Host Bridge */
#define	PCI_PRODUCT_AMD_F14_HB19	0x1719		/* Family12h/14h Host Bridge */
#define	PCI_PRODUCT_AMD_PCNET_PCI	0x2000		/* PCnet-PCI Ethernet */
#define	PCI_PRODUCT_AMD_PCNET_HOME	0x2001		/* PCnet-Home HomePNA Ethernet */
#define	PCI_PRODUCT_AMD_AM_1771_MBW	0x2003		/* Alchemy AM 1771 MBW */
#define	PCI_PRODUCT_AMD_PCSCSI_PCI	0x2020		/* PCscsi-PCI SCSI */
#define	PCI_PRODUCT_AMD_GEODELX_PCHB	0x2080		/* Geode LX Host-PCI Bridge */
#define	PCI_PRODUCT_AMD_GEODELX_VGA	0x2081		/* Geode LX VGA Controller */
#define	PCI_PRODUCT_AMD_GEODELX_AES	0x2082		/* Geode LX AES Security Block */
#define	PCI_PRODUCT_AMD_CS5536_PCISB	0x208f		/* CS5536 GeodeLink PCI South Bridge */
#define	PCI_PRODUCT_AMD_CS5536_PCIB	0x2090		/* CS5536 PCI-ISA Bridge */
#define	PCI_PRODUCT_AMD_CS5536_FLASH	0x2091		/* CS5536 Flash */
#define	PCI_PRODUCT_AMD_CS5536_AUDIO	0x2093		/* CS5536 Audio */
#define	PCI_PRODUCT_AMD_CS5536_OHCI	0x2094		/* CS5536 OHCI USB Controller */
#define	PCI_PRODUCT_AMD_CS5536_EHCI	0x2095		/* CS5536 EHCI USB Controller */
#define	PCI_PRODUCT_AMD_CS5536_UDC	0x2096		/* CS5536 UDC */
#define	PCI_PRODUCT_AMD_CS5536_UOC	0x2097		/* CS5536 UOC */
#define	PCI_PRODUCT_AMD_CS5536_IDE	0x209a		/* CS5536 IDE Controller */
#define	PCI_PRODUCT_AMD_SC520_SC	0x3000		/* Elan SC520 System Controller */
#define	PCI_PRODUCT_AMD_SC751_SC	0x7006		/* AMD751 System Controller */
#define	PCI_PRODUCT_AMD_SC751_PPB	0x7007		/* AMD751 PCI-PCI Bridge */
#define	PCI_PRODUCT_AMD_IGR4_AGP	0x700a		/* AMD IGR4 AGP Bridge */
#define	PCI_PRODUCT_AMD_IGR4_PPB	0x700b		/* AMD IGR4 PCI-PCI Bridge */
#define	PCI_PRODUCT_AMD_SC762_NB	0x700c		/* AMD762 North Bridge */
#define	PCI_PRODUCT_AMD_SC762_PPB	0x700d		/* AMD762 AGP Bridge */
#define	PCI_PRODUCT_AMD_SC761_SC	0x700e		/* AMD761 System Controller */
#define	PCI_PRODUCT_AMD_SC761_PPB	0x700f		/* AMD761 PCI-PCI Bridge */
#define	PCI_PRODUCT_AMD_PBC755_ISA	0x7400		/* AMD755 PCI-ISA Bridge */
#define	PCI_PRODUCT_AMD_PBC755_IDE	0x7401		/* AMD755 IDE Controller */
#define	PCI_PRODUCT_AMD_PBC755_PMC	0x7403		/* AMD755 ACPI Controller */
#define	PCI_PRODUCT_AMD_PBC755_USB	0x7404		/* AMD755 USB Host Controller */
#define	PCI_PRODUCT_AMD_PBC756_ISA	0x7408		/* AMD756 PCI-ISA Bridge */
#define	PCI_PRODUCT_AMD_PBC756_IDE	0x7409		/* AMD756 IDE Controller */
#define	PCI_PRODUCT_AMD_PBC756_PMC	0x740b		/* AMD756 Power Management Controller */
#define	PCI_PRODUCT_AMD_PBC756_USB	0x740c		/* AMD756 USB Host Controller */
#define	PCI_PRODUCT_AMD_PBC766_ISA	0x7410		/* AMD766 South Bridge */
#define	PCI_PRODUCT_AMD_PBC766_IDE	0x7411		/* AMD766 IDE Controller */
#define	PCI_PRODUCT_AMD_PBC766_PMC	0x7413		/* AMD766 Power Management Controller */
#define	PCI_PRODUCT_AMD_PBC766_USB	0x7414		/* AMD766 USB Host Controller */
#define	PCI_PRODUCT_AMD_PBC768_ISA	0x7440		/* AMD768 PCI-ISA/LPC Bridge */
#define	PCI_PRODUCT_AMD_PBC768_IDE	0x7441		/* AMD768 EIDE Controller */
#define	PCI_PRODUCT_AMD_PBC768_PMC	0x7443		/* AMD768 Power Management Controller */
#define	PCI_PRODUCT_AMD_PBC768_AC	0x7445		/* AMD768 AC97 Audio */
#define	PCI_PRODUCT_AMD_PBC768_MD	0x7446		/* AMD768 AC97 Modem */
#define	PCI_PRODUCT_AMD_PBC768_PPB	0x7448		/* AMD768 PCI-PCI Bridge */
#define	PCI_PRODUCT_AMD_PBC768_USB	0x7449		/* AMD768 USB Controller */
#define	PCI_PRODUCT_AMD_PCIX8131_PPB	0x7450		/* AMD8131 PCI-X Tunnel */
#define	PCI_PRODUCT_AMD_PCIX8131_APIC	0x7451		/* AMD8131 IO Apic */
#define	PCI_PRODUCT_AMD_AGP8151_DEV	0x7454		/* AMD8151 AGP Device */
#define	PCI_PRODUCT_AMD_AGP8151_PPB	0x7455		/* AMD8151 AGP Bridge */
#define	PCI_PRODUCT_AMD_PCIX_PPB	0x7458		/* AMD8123 PCI-X Bridge */
#define	PCI_PRODUCT_AMD_PCIX_APIC	0x7459		/* AMD8132 PCI-X IOAPIC */
#define	PCI_PRODUCT_AMD_PBC8111	0x7460		/* AMD8111 I/O Hub */
#define	PCI_PRODUCT_AMD_PBC8111_USB_7461	0x7461		/* AMD8111 7461 USB Host Controller */
#define	PCI_PRODUCT_AMD_PBC8111_ETHER	0x7462		/* AMD8111 Ethernet */
#define	PCI_PRODUCT_AMD_PBC8111_USB	0x7464		/* AMD8111 USB Host Controller */
#define	PCI_PRODUCT_AMD_PBC8111_LPC	0x7468		/* AMD8111 LPC Controller */
#define	PCI_PRODUCT_AMD_PBC8111_IDE	0x7469		/* AMD8111 IDE Controller */
#define	PCI_PRODUCT_AMD_PBC8111_SMB	0x746a		/* AMD8111 SMBus Controller */
#define	PCI_PRODUCT_AMD_PBC8111_ACPI	0x746b		/* AMD8111 ACPI Controller */
#define	PCI_PRODUCT_AMD_PBC8111_AC	0x746d		/* AMD8111 AC97 Audio */
#define	PCI_PRODUCT_AMD_PBC8111_MC97	0x746e		/* AMD8111 MC97 Modem */
#define	PCI_PRODUCT_AMD_PBC8111_AC_756b	0x756b		/* AMD8111 756b ACPI Controller */
#define	PCI_PRODUCT_AMD_HUDSON_SATA	0x7800		/* Hudson SATA Controller */
#define	PCI_PRODUCT_AMD_HUDSON_SATA_AHCI	0x7801		/* Hudson AHCI SATA Controller */
#define	PCI_PRODUCT_AMD_HUDSON_SDHC	0x7806		/* Hudson SD Flash Controller */
#define	PCI_PRODUCT_AMD_HUDSON_OHCI	0x7807		/* Hudson USB OHCI Controller */
#define	PCI_PRODUCT_AMD_HUDSON_EHCI	0x7808		/* Hudson USB EHCI Controller */
#define	PCI_PRODUCT_AMD_HUDSON_OHCI_2	0x7809		/* Hudson USB OHCI Controller */
#define	PCI_PRODUCT_AMD_HUDSON_SMB	0x780b		/* Hudson SMBus Controller */
#define	PCI_PRODUCT_AMD_HUDSON_IDE	0x780c		/* Hudson IDE Controller */
#define	PCI_PRODUCT_AMD_HUDSON_HDAUDIO	0x780d		/* Hudson HD Audio Controller */
#define	PCI_PRODUCT_AMD_HUDSON_LPC	0x780e		/* Hudson LPC Bridge */
#define	PCI_PRODUCT_AMD_HUDSON_PCI	0x780f		/* Hudson PCI Bridge */
#define	PCI_PRODUCT_AMD_HUDSON_XHCI	0x7812		/* Hudson USB xHCI Controller */
#define	PCI_PRODUCT_AMD_RS780_HB	0x9600		/* RS780 Host Bridge */
#define	PCI_PRODUCT_AMD_RS880_HB	0x9601		/* RS785/RS880 Host Bridge */
#define	PCI_PRODUCT_AMD_RS780_PPB_GFX	0x9602		/* RS780/RS880 PCI-PCI Bridge (int gfx) */
#define	PCI_PRODUCT_AMD_RS780_PPB_GFX0	0x9603		/* RS780 PCI-PCI Bridge (ext gfx port 0) */
#define	PCI_PRODUCT_AMD_RS780_PPB0	0x9604		/* RS780/RS880 PCI-PCIE Bridge (port 0) */
#define	PCI_PRODUCT_AMD_RS780_PPB1	0x9605		/* RS780/RS880 PCI-PCIE Bridge (port 1) */
#define	PCI_PRODUCT_AMD_RS780_PPB2	0x9606		/* RS780 PCI-PCIE Bridge (port 2) */
#define	PCI_PRODUCT_AMD_RS780_PPB3	0x9607		/* RS780 PCI-PCIE Bridge (port 3) */
#define	PCI_PRODUCT_AMD_RS780_PPB4	0x9608		/* RS780/RS880 PCI-PCIE Bridge (port 4) */
#define	PCI_PRODUCT_AMD_RS780_PPB5	0x9609		/* RS780/RS880 PCI-PCIE Bridge (port 5) */
#define	PCI_PRODUCT_AMD_RS780_PPB6	0x960a		/* RS780 PCI-PCIE Bridge (NB-SB link) */
#define	PCI_PRODUCT_AMD_RS780_PPB7	0x960b		/* RS780 PCI-PCIE Bridge (ext gfx port 1) */

/* American Megatrends products */
#define	PCI_PRODUCT_AMI_MEGARAID	0x9010		/* MegaRAID */
#define	PCI_PRODUCT_AMI_MEGARAID2	0x9060		/* MegaRAID 2 */
#define	PCI_PRODUCT_AMI_MEGARAID3	0x1960		/* MegaRAID 3 */

/* Analog Devices products */
#define	PCI_PRODUCT_ANALOG_AD1889	0x1889		/* AD1889 PCI SoundMAX Controller */
#define	PCI_PRODUCT_ANALOG_SAFENET	0x2f44		/* SafeNet Crypto Accelerator ADSP-2141 */

/* Antares Microsystems products */
#define	PCI_PRODUCT_ANTARES_TC9021	0x1021		/* Antares Gigabit Ethernet */

/* Apple products */
#define	PCI_PRODUCT_APPLE_BANDIT	0x0001		/* Bandit Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_GC	0x0002		/* Grand Central I/O Controller */
#define	PCI_PRODUCT_APPLE_CONTROL	0x0003		/* Control */
#define	PCI_PRODUCT_APPLE_PLANB	0x0004		/* PlanB */
#define	PCI_PRODUCT_APPLE_OHARE	0x0007		/* OHare I/O Controller */
#define	PCI_PRODUCT_APPLE_BANDIT2	0x0008		/* Bandit Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_HEATHROW	0x0010		/* Heathrow I/O Controller */
#define	PCI_PRODUCT_APPLE_PADDINGTON	0x0017		/* Paddington I/O Controller */
#define	PCI_PRODUCT_APPLE_PBG3_FW	0x0018		/* PowerBook G3 Firewire */
#define	PCI_PRODUCT_APPLE_KEYLARGO_USB	0x0019		/* KeyLargo USB Controller */
#define	PCI_PRODUCT_APPLE_UNINORTH1	0x001e		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH2	0x001f		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP	0x0020		/* UniNorth AGP Interface */
#define	PCI_PRODUCT_APPLE_GMAC	0x0021		/* GMAC Ethernet */
#define	PCI_PRODUCT_APPLE_KEYLARGO	0x0022		/* KeyLargo I/O Controller */
#define	PCI_PRODUCT_APPLE_GMAC2	0x0024		/* GMAC Ethernet */
#define	PCI_PRODUCT_APPLE_PANGEA_MACIO	0x0025		/* Pangea I/O Controller */
#define	PCI_PRODUCT_APPLE_PANGEA_USB	0x0026		/* Pangea USB Controller */
#define	PCI_PRODUCT_APPLE_PANGEA_AGP	0x0027		/* Pangea AGP Interface */
#define	PCI_PRODUCT_APPLE_PANGEA_PCI1	0x0028		/* Pangea Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_PANGEA_PCI2	0x0029		/* Pangea Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP2	0x002d		/* UniNorth AGP Interface */
#define	PCI_PRODUCT_APPLE_UNINORTH3	0x002e		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH4	0x002f		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_PANGEA_FW	0x0030		/* Pangea Firewire */
#define	PCI_PRODUCT_APPLE_UNINORTH_FW	0x0031		/* UniNorth Firewire */
#define	PCI_PRODUCT_APPLE_GMAC3	0x0032		/* GMAC Ethernet */
#define	PCI_PRODUCT_APPLE_UNINORTH_ATA	0x0033		/* UniNorth ATA/100 Controller */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP3	0x0034		/* UniNorth AGP Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH5	0x0035		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_UNINORTH6	0x0036		/* UniNorth Host-PCI Bridge */
#define	PCI_PRODUCT_APPLE_KAUAI	0x003b		/* Kauai ATA Controller */
#define	PCI_PRODUCT_APPLE_INTREPID	0x003e		/* Intrepid I/O Controller */
#define	PCI_PRODUCT_APPLE_INTREPID_USB	0x003f		/* Intrepid USB Controller */
#define	PCI_PRODUCT_APPLE_K2_USB	0x0040		/* K2 USB Controller */
#define	PCI_PRODUCT_APPLE_K2	0x0041		/* K2 MAC-IO Controller */
#define	PCI_PRODUCT_APPLE_K2_FW	0x0042		/* K2 Firewire */
#define	PCI_PRODUCT_APPLE_K2_UATA	0x0043		/* K2 UATA Controller */
#define	PCI_PRODUCT_APPLE_U3_PPB1	0x0045		/* U3 PCI-PCI Bridge */
#define	PCI_PRODUCT_APPLE_U3_PPB2	0x0046		/* U3 PCI-PCI Bridge */
#define	PCI_PRODUCT_APPLE_U3_PPB3	0x0047		/* U3 PCI-PCI Bridge */
#define	PCI_PRODUCT_APPLE_U3_PPB4	0x0048		/* U3 PCI-PCI Bridge */
#define	PCI_PRODUCT_APPLE_U3_PPB5	0x0049		/* U3 PCI-PCI Bridge */
#define	PCI_PRODUCT_APPLE_U3_AGP	0x004b		/* U3 AGP Interface */
#define	PCI_PRODUCT_APPLE_K2_GMAC	0x004c		/* GMAC Ethernet */
#define	PCI_PRODUCT_APPLE_SHASTA	0x004f		/* Shasta */
#define	PCI_PRODUCT_APPLE_SHASTA_ATA	0x0050		/* Shasta ATA */
#define	PCI_PRODUCT_APPLE_SHASTA_GMAC	0x0051		/* Shasta GMAC */
#define	PCI_PRODUCT_APPLE_SHASTA_FW	0x0052		/* Shasta Firewire */
#define	PCI_PRODUCT_APPLE_SHASTA_PCI1	0x0053		/* Shasta PCI */
#define	PCI_PRODUCT_APPLE_SHASTA_PCI2	0x0054		/* Shasta PCI */
#define	PCI_PRODUCT_APPLE_SHASTA_PCI3	0x0055		/* Shasta PCI */
#define	PCI_PRODUCT_APPLE_SHASTA_HT	0x0056		/* Shasta HyperTransport */
#define	PCI_PRODUCT_APPLE_INTREPID2_AGP	0x0066		/* Intrepid 2 AGP */
#define	PCI_PRODUCT_APPLE_INTREPID2_PCI1	0x0067		/* Intrepid 2 PCI */
#define	PCI_PRODUCT_APPLE_INTREPID2_PCI2	0x0068		/* Intrepid 2 PCI */
#define	PCI_PRODUCT_APPLE_INTREPID2_ATA	0x0069		/* Intrepid 2 ATA */
#define	PCI_PRODUCT_APPLE_INTREPID2_FW	0x006a		/* Intrepid 2 FireWire */
#define	PCI_PRODUCT_APPLE_INTREPID2_GMAC	0x006b		/* Intrepid 2 GMAC */
#define	PCI_PRODUCT_APPLE_BCM5701	0x1645		/* BCM5701 */

/* ARC Logic products */
#define	PCI_PRODUCT_ARC_1000PV	0xa091		/* 1000PV */
#define	PCI_PRODUCT_ARC_2000PV	0xa099		/* 2000PV */
#define	PCI_PRODUCT_ARC_2000MT	0xa0a1		/* 2000MT */

/* Areca products */
#define	PCI_PRODUCT_ARECA_ARC1110	0x1110		/* ARC-1110 */
#define	PCI_PRODUCT_ARECA_ARC1120	0x1120		/* ARC-1120 */
#define	PCI_PRODUCT_ARECA_ARC1130	0x1130		/* ARC-1130 */
#define	PCI_PRODUCT_ARECA_ARC1160	0x1160		/* ARC-1160 */
#define	PCI_PRODUCT_ARECA_ARC1170	0x1170		/* ARC-1170 */
#define	PCI_PRODUCT_ARECA_ARC1200	0x1200		/* ARC-1200 */
#define	PCI_PRODUCT_ARECA_ARC1200_B	0x1201		/* ARC-1200 rev B */
#define	PCI_PRODUCT_ARECA_ARC1202	0x1202		/* ARC-1202 */
#define	PCI_PRODUCT_ARECA_ARC1210	0x1210		/* ARC-1210 */
#define	PCI_PRODUCT_ARECA_ARC1220	0x1220		/* ARC-1220 */
#define	PCI_PRODUCT_ARECA_ARC1230	0x1230		/* ARC-1230 */
#define	PCI_PRODUCT_ARECA_ARC1260	0x1260		/* ARC-1260 */
#define	PCI_PRODUCT_ARECA_ARC1270	0x1270		/* ARC-1270 */
#define	PCI_PRODUCT_ARECA_ARC1280	0x1280		/* ARC-1280 */
#define	PCI_PRODUCT_ARECA_ARC1380	0x1380		/* ARC-1380 */
#define	PCI_PRODUCT_ARECA_ARC1381	0x1381		/* ARC-1381 */
#define	PCI_PRODUCT_ARECA_ARC1680	0x1680		/* ARC-1680 */
#define	PCI_PRODUCT_ARECA_ARC1681	0x1681		/* ARC-1681 */

/* ASIX Electronics products */
#define	PCI_PRODUCT_ASIX_AX88140A	0x1400		/* AX88140A 10/100 Ethernet */

/* ASMedia products */
#define	PCI_PRODUCT_ASMEDIA_ASM1061_01	0x0601		/* ASM1061 AHCI SATA III Controller */
#define	PCI_PRODUCT_ASMEDIA_ASM1061_02	0x0602		/* ASM1061 AHCI SATA III Controller */
#define	PCI_PRODUCT_ASMEDIA_ASM1061_11	0x0611		/* ASM1061 AHCI SATA III Controller */
#define	PCI_PRODUCT_ASMEDIA_ASM1061_12	0x0612		/* ASM1061 AHCI SATA III Controller */
#define	PCI_PRODUCT_ASMEDIA_ASM1042	0x1042		/* ASM1042 USB 3.0 Host Controller */
#define	PCI_PRODUCT_ASMEDIA_ASM1042A	0x1142		/* ASM1042A USB 3.0 Host Controller */

/* Asustek products */
#define	PCI_PRODUCT_ASUSTEK_HFCPCI	0x0675		/* ISDN */

/* Attansic Technology Corp. */
#define	PCI_PRODUCT_ATTANSIC_ETHERNET_L1E	0x1026		/* L1E Gigabit Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_ETHERNET_GIGA	0x1048		/* L1 Gigabit Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_AR8132	0x1062		/* AR8132 Fast Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_AR8131	0x1063		/* AR8131 Gigabit Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_AR8151	0x1073		/* AR8151 v1.0 Gigabit Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_AR8151_V2	0x1083		/* AR8151 v2.0 Gigabit Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_AR8162	0x1090		/* AR8162 */
#define	PCI_PRODUCT_ATTANSIC_AR8161	0x1091		/* AR8161 */
#define	PCI_PRODUCT_ATTANSIC_AR8172	0x10a0		/* AR8172 */
#define	PCI_PRODUCT_ATTANSIC_AR8171	0x10a1		/* AR8171 */
#define	PCI_PRODUCT_ATTANSIC_ETHERNET_100	0x2048		/* L2 100 Mbit Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_AR8152_B	0x2060		/* AR8152 v1.1 Fast Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_AR8152_B2	0x2062		/* AR8152 v2.0 Fast Ethernet Adapter */
#define	PCI_PRODUCT_ATTANSIC_E2200	0xe091		/* E2200 */

/* ATI products */
/* See http://www.x.org/wiki/Radeon%20ASICs */
#define	PCI_PRODUCT_ATI_RADEON_WRESTLER_HDMI	0x1314		/* Wrestler HDMI Audio */
#define	PCI_PRODUCT_ATI_RADEON_BEAVERCREEK_HDMI	0x1714		/* BeaverCreek HDMI Audio */
#define	PCI_PRODUCT_ATI_RADEON_RV380_3150	0x3150		/* Radeon Mobility X600 (M24) 3150 */
#define	PCI_PRODUCT_ATI_RADEON_RV380_3154	0x3154		/* FireGL M24 GL 3154 */
#define	PCI_PRODUCT_ATI_RADEON_RV380_3E50	0x3e50		/* Radeon X600 (RV380) 3E50 */
#define	PCI_PRODUCT_ATI_RADEON_RV380_3E54	0x3e54		/* FireGL V3200 (RV380) 3E54 */
#define	PCI_PRODUCT_ATI_RADEON_RS100_4136	0x4136		/* Radeon IGP320 (A3) 4136 */
#define	PCI_PRODUCT_ATI_RADEON_RS200_A7	0x4137		/* Radeon IGP330/340/350 (A4) 4137 */
#define	PCI_PRODUCT_ATI_RADEON_R300_AD	0x4144		/* Radeon 9500 AD */
#define	PCI_PRODUCT_ATI_RADEON_R300_AE	0x4145		/* Radeon 9500 AE */
#define	PCI_PRODUCT_ATI_RADEON_R300_AF	0x4146		/* Radeon 9600TX AF */
#define	PCI_PRODUCT_ATI_RADEON_R300_AG	0x4147		/* FireGL Z1 AG */
#define	PCI_PRODUCT_ATI_RADEON_R350_AH	0x4148		/* Radeon 9800SE AH */
#define	PCI_PRODUCT_ATI_RADEON_R350_AI	0x4149		/* Radeon 9800 AI */
#define	PCI_PRODUCT_ATI_RADEON_R350_AJ	0x414a		/* Radeon 9800 AJ */
#define	PCI_PRODUCT_ATI_RADEON_R350_AK	0x414b		/* FireGL X2 AK */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AP	0x4150		/* Radeon 9600 AP */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AQ	0x4151		/* Radeon 9600SE AQ */
#define	PCI_PRODUCT_ATI_RADEON_RV360_AR	0x4152		/* Radeon 9600XT AR */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AS	0x4153		/* Radeon 9600 AS */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AT	0x4154		/* FireGL T2 AT */
/* RV350 and RV360 FireFL T2 have same PCI id */
#define	PCI_PRODUCT_ATI_RADEON_RV350_AV	0x4154		/* FireGL RV360 AV */
#define	PCI_PRODUCT_ATI_MACH32	0x4158		/* Mach32 */
#define	PCI_PRODUCT_ATI_RADEON_9600_LE_S	0x4171		/* Radeon 9600 LE Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9600_XT_S	0x4172		/* Radeon 9600 XT Secondary */
#define	PCI_PRODUCT_ATI_RADEON_RS250_B7	0x4237		/* Radeon 7000 IGP (A4+) */
#define	PCI_PRODUCT_ATI_RADEON_R200_BB	0x4242		/* Radeon 8500 AIW BB */
#define	PCI_PRODUCT_ATI_RADEON_R200_BC	0x4243		/* Radeon 8500 AIW BC */
#define	PCI_PRODUCT_ATI_RADEON_RS100_4336	0x4336		/* Radeon IGP320M (U1) 4336 */
#define	PCI_PRODUCT_ATI_RADEON_RS200_4337	0x4337		/* Radeon IGP330M/340M/350M (U2) 4337 */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_200	0x4341		/* IXP AC'97 Audio Controller */
#define	PCI_PRODUCT_ATI_SB200_PPB	0x4342		/* SB200 PCI-PCI Bridge */
#define	PCI_PRODUCT_ATI_SB200_EHCI	0x4345		/* SB200 USB2 Host Controller */
#define	PCI_PRODUCT_ATI_SB200_OHCI_1	0x4347		/* SB200 USB Host Controller */
#define	PCI_PRODUCT_ATI_SB200_OHCI_2	0x4348		/* SB200 USB Host Controller */
#define	PCI_PRODUCT_ATI_IXP_IDE_200	0x4349		/* SB200 IXP IDE Controller */
#define	PCI_PRODUCT_ATI_SB200_ISA	0x434c		/* SB200 PCI-ISA Bridge */
#define	PCI_PRODUCT_ATI_SB200_MODEM	0x434d		/* SB200 Modem */
#define	PCI_PRODUCT_ATI_SB200_SMB	0x4353		/* SB200 SMBus Controller */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_300	0x4361		/* IXP AC'97 Audio Controller */
#define	PCI_PRODUCT_ATI_SB300_SMB	0x4363		/* SB300 SMBus Controller */
#define	PCI_PRODUCT_ATI_IXP_IDE_300	0x4369		/* SB300 IXP IDE Controller */
#define	PCI_PRODUCT_ATI_IXP_SATA_300	0x436e		/* IXP300 SATA Controller */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_400	0x4370		/* IXP AC'97 Audio Controller */
#define	PCI_PRODUCT_ATI_SB400_PPB	0x4371		/* SB400 PCI-PCI Bridge */
#define	PCI_PRODUCT_ATI_SB400_SMB	0x4372		/* SB400 SMBus Controller */
#define	PCI_PRODUCT_ATI_SB400_EHCI	0x4373		/* SB400 USB2 Host Controller */
#define	PCI_PRODUCT_ATI_SB400_OHCI_1	0x4374		/* SB400 USB Host Controller */
#define	PCI_PRODUCT_ATI_SB400_OHCI_2	0x4375		/* SB400 USB Host Controller */
#define	PCI_PRODUCT_ATI_IXP_IDE_400	0x4376		/* SB400 IXP IDE Controller */
#define	PCI_PRODUCT_ATI_SB400_ISA	0x4377		/* SB400 PCI-ISA Bridge */
#define	PCI_PRODUCT_ATI_SB400_MODEM	0x4378		/* SB400 Modem */
#define	PCI_PRODUCT_ATI_SB400_SATA_1	0x4379		/* SB400 SATA Controller */
#define	PCI_PRODUCT_ATI_SB400_SATA_2	0x437a		/* SB400 SATA Controller */
#define	PCI_PRODUCT_ATI_SB600_SATA_1	0x4380		/* SB600 SATA Controller */
#define	PCI_PRODUCT_ATI_SB600_SATA_2	0x4381		/* SB600 SATA Controller */
#define	PCI_PRODUCT_ATI_SB600_AC97_AUDIO	0x4382		/* SB600 AC97 Audio */
#define	PCI_PRODUCT_ATI_SB600_AZALIA	0x4383		/* SBx00 Azalia */
#define	PCI_PRODUCT_ATI_SB600_PPB	0x4384		/* SBx00 PCI to PCI Bridge */
#define	PCI_PRODUCT_ATI_SB600_SMB	0x4385		/* SBx00 SMBus Controller */
#define	PCI_PRODUCT_ATI_SB600_USB_EHCI	0x4386		/* SB600 USB EHCI Controller */
#define	PCI_PRODUCT_ATI_SB600_USB_OHCI0	0x4387		/* SB600 USB OHCI0 Controller */
#define	PCI_PRODUCT_ATI_SB600_USB_OHCI1	0x4388		/* SB600 USB OHCI1 Controller */
#define	PCI_PRODUCT_ATI_SB600_USB_OHCI2	0x4389		/* SB600 USB OHCI2 Controller */
#define	PCI_PRODUCT_ATI_SB600_USB_OHCI3	0x438a		/* SB600 USB OHCI3 Controller */
#define	PCI_PRODUCT_ATI_SB600_USB_OHCI4	0x438b		/* SB600 USB OHCI4 Controller */
#define	PCI_PRODUCT_ATI_SB600_AC97_MODEM	0x438e		/* SB600 AC97 Modem */
#define	PCI_PRODUCT_ATI_IXP_IDE_600	0x438c		/* SB600 IXP IDE Controller */
#define	PCI_PRODUCT_ATI_SB600_PLB_438D	0x438d		/* SB600 PCI to LPC Bridge */
#define	PCI_PRODUCT_ATI_SB700_SATA_IDE	0x4390		/* SB700-SB900 SATA Controller (IDE mode) */
#define	PCI_PRODUCT_ATI_SB700_SATA_AHCI	0x4391		/* SB700-SB900 SATA Controller (AHCI mode) */
#define	PCI_PRODUCT_ATI_SB700_SATA_RAID	0x4392		/* SB700-SB900 RAID SATA Controller */
#define	PCI_PRODUCT_ATI_SB700_SATA_RAID5	0x4393		/* SB700-SB900 RAID5 SATA Controller */
#define	PCI_PRODUCT_ATI_SB700_SATA_FC	0x4394		/* SB700-SB900 FC SATA Controller */
#define	PCI_PRODUCT_ATI_SB700_SATA_AHCI2	0x4395		/* SB700-SB900 SATA Controller (AHCI mode) */
#define	PCI_PRODUCT_ATI_SB700_USB_EHCI	0x4396		/* SB700-SB900 USB EHCI Controller */
#define	PCI_PRODUCT_ATI_SB800_SATA	0x4395		/* SB800/SB900 SATA Controller */
#define	PCI_PRODUCT_ATI_SB700_USB_OHCI0	0x4397		/* SB700-SB900 USB OHCI Controller */
#define	PCI_PRODUCT_ATI_SB700_USB_OHCI1	0x4398		/* SB700-SB900 USB OHCI Controller */
#define	PCI_PRODUCT_ATI_SB700_USB_OHCI2	0x4399		/* SB700-SB900 USB OHCI Controller */
#define	PCI_PRODUCT_ATI_SB700_IDE	0x439c		/* SB700-SB900 IDE Controller */
#define	PCI_PRODUCT_ATI_SB700_LPC	0x439d		/* SB700-SB900 LPC Host Controller */
#define	PCI_PRODUCT_ATI_SB700_PCIE0	0x43a0		/* SB700-SB900 PCI to PCI bridge (PCIe 0) */
#define	PCI_PRODUCT_ATI_SB700_PCIE1	0x43a1		/* SB700-SB900 PCI to PCI bridge (PCIe 1) */
#define	PCI_PRODUCT_ATI_SB900_PCIE2	0x43a2		/* SB900 PCI to PCI bridge (PCIe 2) */
#define	PCI_PRODUCT_ATI_SB900_PCIE3	0x43a3		/* SB900 PCI to PCI bridge (PCIe 3) */
#define	PCI_PRODUCT_ATI_MACH64_CT	0x4354		/* Mach64 CT */
#define	PCI_PRODUCT_ATI_MACH64_CX	0x4358		/* Mach64 CX */
#define	PCI_PRODUCT_ATI_RADEON_RS250_D7	0x4437		/* Radeon Mobility 7000 IGP */
#define	PCI_PRODUCT_ATI_RAGE_PRO_AGP	0x4742		/* 3D Rage Pro (AGP) */
#define	PCI_PRODUCT_ATI_RAGE_PRO_AGP1X	0x4744		/* 3D Rage Pro (AGP 1x) */
#define	PCI_PRODUCT_ATI_RAGE_PRO_PCI_B	0x4749		/* 3D Rage Pro Turbo */
#define	PCI_PRODUCT_ATI_RAGE_XC_PCI66	0x474c		/* Rage XC (PCI66) */
#define	PCI_PRODUCT_ATI_RAGE_XL_AGP	0x474d		/* Rage XL (AGP) */
#define	PCI_PRODUCT_ATI_RAGE_XC_AGP	0x474e		/* Rage XC (AGP) */
#define	PCI_PRODUCT_ATI_RAGE_XL_PCI66	0x474f		/* Rage XL (PCI66) */
#define	PCI_PRODUCT_ATI_RAGE_PRO_PCI_P	0x4750		/* 3D Rage Pro */
#define	PCI_PRODUCT_ATI_RAGE_PRO_PCI_L	0x4751		/* 3D Rage Pro (limited 3D) */
#define	PCI_PRODUCT_ATI_RAGE_XL_PCI	0x4752		/* Rage XL */
#define	PCI_PRODUCT_ATI_RAGE_XC_PCI	0x4753		/* Rage XC */
#define	PCI_PRODUCT_ATI_RAGE_II	0x4754		/* 3D Rage I/II */
#define	PCI_PRODUCT_ATI_RAGE_IIP	0x4755		/* 3D Rage II+ */
#define	PCI_PRODUCT_ATI_RAGE_IIC_PCI	0x4756		/* 3D Rage IIC */
#define	PCI_PRODUCT_ATI_RAGE_IIC_AGP_B	0x4757		/* 3D Rage IIC (AGP) */
#define	PCI_PRODUCT_ATI_MACH64_GX	0x4758		/* Mach64 GX */
#define	PCI_PRODUCT_ATI_RAGE_IIC	0x4759		/* 3D Rage IIC */
#define	PCI_PRODUCT_ATI_RAGE_IIC_AGP_P	0x475a		/* 3D Rage IIC (AGP) */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4966	0x4966		/* Radeon 9000/PRO If */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4967	0x4967		/* Radeon 9000 Ig */
#define	PCI_PRODUCT_ATI_RADEON_R420_JH	0x4a48		/* Radeon X800 (R420) JH */
#define	PCI_PRODUCT_ATI_RADEON_R420_JI	0x4a49		/* Radeon X800PRO (R420) JI */
/* XXX 4a4a is generic, the SE should be 4a4f */
#define	PCI_PRODUCT_ATI_RADEON_R420_JJ	0x4a4a		/* Radeon X800SE (R420) JJ */
/* XXX 4a4b should be the XT */
#define	PCI_PRODUCT_ATI_RADEON_R420_JK	0x4a4b		/* Radeon X800 (R420) JK */
#define	PCI_PRODUCT_ATI_RADEON_R420_JL	0x4a4c		/* Radeon X800 (R420) JL */
#define	PCI_PRODUCT_ATI_RADEON_R420_JM	0x4a4d		/* FireGL X3 (R420) JM */
#define	PCI_PRODUCT_ATI_RADEON_R420_JN	0x4a4e		/* Radeon Mobility 9800 (M18) JN */
/* Duplicate, maybe this entry should be 4a50, XT Platinum */
#define	PCI_PRODUCT_ATI_RADEON_R420_JP	0x4a4e		/* Radeon X800XT (R420) JP */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO_AGP	0x4c42		/* 3D Rage LT Pro (AGP 133MHz) */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO_AGP66	0x4c44		/* 3D Rage LT Pro (AGP 66MHz) */
#define	PCI_PRODUCT_ATI_RAGE_MOB_M3_PCI	0x4c45		/* Rage Mobility M3 */
#define	PCI_PRODUCT_ATI_RAGE_MOB_M3_AGP	0x4c46		/* Rage Mobility M3 (AGP) */
#define	PCI_PRODUCT_ATI_RAGE_LT	0x4c47		/* 3D Rage LT */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO_PCI	0x4c49		/* 3D Rage LT Pro */
#define	PCI_PRODUCT_ATI_RAGE_MOBILITY	0x4c4d		/* Rage Mobility */
#define	PCI_PRODUCT_ATI_RAGE_L_MOBILITY	0x4c4e		/* Rage L Mobility */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO	0x4c50		/* 3D Rage LT Pro */
#define	PCI_PRODUCT_ATI_RAGE_LT_PRO2	0x4c51		/* 3D Rage LT Pro */
#define	PCI_PRODUCT_ATI_RAGE_MOB_M1_PCI	0x4c52		/* Rage Mobility M1 (PCI) */
#define	PCI_PRODUCT_ATI_RAGE_L_MOB_M1_PCI	0x4c53		/* Rage L Mobility (PCI) */
#define	PCI_PRODUCT_ATI_RADEON_RV200_LW	0x4c57		/* Radeon Mobility M7 LW */
#define	PCI_PRODUCT_ATI_RADEON_RV200_LX	0x4c58		/* FireGL Mobility 7800 M7 LX */
#define	PCI_PRODUCT_ATI_RADEON_RV100_LY	0x4c59		/* Radeon Mobility M6 LY */
#define	PCI_PRODUCT_ATI_RADEON_RV100_LZ	0x4c5a		/* Radeon Mobility M6 LZ */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4C64	0x4c64		/* FireGL Mobility 9000 (M9) Ld */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4C66	0x4c66		/* Radeon Mobility 9000 (M9) Lf */
#define	PCI_PRODUCT_ATI_RADEON_RV250_4C67	0x4c67		/* Radeon Mobility 9000 (M9) Lg */
#define	PCI_PRODUCT_ATI_RADEON_128_AGP4X	0x4d46		/* Radeon Mobility 128 AGP 4x */
#define	PCI_PRODUCT_ATI_RADEON_128_AGP2X	0x4d4c		/* Radeon Mobility 128 AGP 2x */
#define	PCI_PRODUCT_ATI_RADEON_R300_ND	0x4e44		/* Radeon 9700 ND */
#define	PCI_PRODUCT_ATI_RADEON_R300_NE	0x4e45		/* Radeon 9700/9500Pro NE */
#define	PCI_PRODUCT_ATI_RADEON_R300_NF	0x4e46		/* Radeon 9700 NF */
#define	PCI_PRODUCT_ATI_RADEON_R300_NG	0x4e47		/* FireGL X1 NG */
#define	PCI_PRODUCT_ATI_RADEON_R350_NH	0x4e48		/* Radeon 9800PRO NH */
#define	PCI_PRODUCT_ATI_RADEON_R350_NI	0x4e49		/* Radeon 9800 NI */
#define	PCI_PRODUCT_ATI_RADEON_R360_NJ	0x4e4a		/* Radeon 9800XT NJ */
#define	PCI_PRODUCT_ATI_RADEON_R350_NK	0x4e4b		/* FireGL X2 NK */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NP	0x4e50		/* Radeon Mobility 9600/9700 (M10/11) NP */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NQ	0x4e41		/* Radeon Mobility 9600 (M10) NQ */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NR	0x4e52		/* Radeon Mobility 9600 (M11) NR */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NS	0x4e53		/* Radeon Mobility 9600 (M10) NS */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NT	0x4e54		/* FireGL Mobility T2 (M10) NT */
#define	PCI_PRODUCT_ATI_RADEON_RV350_NV	0x4e56		/* FireGL Mobility T2e (M11) NV */
#define	PCI_PRODUCT_ATI_RADEON_9700_9500_S	0x4e64		/* Radeon 9700/9500 Series Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9700_9500_S2	0x4e65		/* Radeon 9700/9500 Series Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9600_2	0x4e66		/* Radeon 9600TX Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9800_PRO_2	0x4e68		/* Radeon 9800 Pro Secondary */
#define	PCI_PRODUCT_ATI_RAGE1PCI	0x5041		/* Rage 128 Pro PCI */
#define	PCI_PRODUCT_ATI_RAGE1AGP2X	0x5042		/* Rage 128 Pro AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE1AGP4X	0x5043		/* Rage 128 Pro AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE1PCIT	0x5044		/* Rage 128 Pro PCI (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE1AGP2XT	0x5045		/* Rage 128 Pro AGP 2x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE1AGP4XT	0x5046		/* Rage Fury MAXX AGP 4x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE2PCI	0x5047		/* Rage 128 Pro PCI */
#define	PCI_PRODUCT_ATI_RAGE2AGP2X	0x5048		/* Rage 128 Pro AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE2AGP4X	0x5049		/* Rage 128 Pro AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE2PCIT	0x504a		/* Rage 128 Pro PCI (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE2AGP2XT	0x504b		/* Rage 128 Pro AGP 2x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE2AGP4XT	0x504c		/* Rage 128 Pro AGP 4x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE3PCI	0x504d		/* Rage 128 Pro PCI */
#define	PCI_PRODUCT_ATI_RAGE3AGP2X	0x504e		/* Rage 128 Pro AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE3AGP4X	0x504f		/* Rage 128 Pro AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE3PCIT	0x5050		/* Rage 128 Pro PCI (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE3AGP2XT	0x5051		/* Rage 128 Pro AGP 2x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE3AGP4XT	0x5052		/* Rage 128 Pro AGP 4x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE4PCI	0x5053		/* Rage 128 Pro PCI */
#define	PCI_PRODUCT_ATI_RAGE4AGP2X	0x5054		/* Rage 128 Pro AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE4AGP4X	0x5055		/* Rage 128 Pro AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE4PCIT	0x5056		/* Rage 128 Pro PCI (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE4AGP2XT	0x5057		/* Rage 128 Pro AGP 2x (TMDS) */
#define	PCI_PRODUCT_ATI_RAGE4AGP4XT	0x5058		/* Rage 128 Pro AGP 4x (TMDS) */
#define	PCI_PRODUCT_ATI_RADEON_R100_QD	0x5144		/* Radeon QD */
#define	PCI_PRODUCT_ATI_RADEON_R100_QE	0x5145		/* Radeon QE */
#define	PCI_PRODUCT_ATI_RADEON_R100_QF	0x5146		/* Radeon QF */
#define	PCI_PRODUCT_ATI_RADEON_R100_QG	0x5147		/* Radeon QG */
#define	PCI_PRODUCT_ATI_RADEON_R200_QH	0x5148		/* FireGL 8700/8800 QH */
#define	PCI_PRODUCT_ATI_RADEON_R200_QL	0x514c		/* Radeon 8500 QL */
#define	PCI_PRODUCT_ATI_RADEON_R200_QM	0x514d		/* Radeon 9100 QM */
#define	PCI_PRODUCT_ATI_RADEON_RV200_QW	0x5157		/* Radeon 7500 QW */
#define	PCI_PRODUCT_ATI_RADEON_RV200_QX	0x5158		/* Radeon 7500 QX */
#define	PCI_PRODUCT_ATI_RADEON_RV100_QY	0x5159		/* Radeon 7000/VE QY */
#define	PCI_PRODUCT_ATI_RADEON_RV100_QZ	0x515a		/* Radeon 7000/VE QZ */
#define	PCI_PRODUCT_ATI_ES1000	0x515e		/* ES1000 */
#define	PCI_PRODUCT_ATI_RADEON_9100_S	0x516d		/* Radeon 9100 Series Secondary */
#define	PCI_PRODUCT_ATI_RAGEGLPCI	0x5245		/* Rage 128 GL PCI */
#define	PCI_PRODUCT_ATI_RAGEGLAGP	0x5246		/* Rage 128 GL AGP 2x */
#define	PCI_PRODUCT_ATI_RAGEVRPCI	0x524b		/* Rage 128 VR PCI */
#define	PCI_PRODUCT_ATI_RAGEVRAGP	0x524c		/* Rage 128 VR AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE4XPCI	0x5345		/* Rage 128 4x PCI */
#define	PCI_PRODUCT_ATI_RAGE4XA2X	0x5346		/* Rage 128 4x AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE4XA4X	0x5347		/* Rage 128 4x AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE4X	0x5348		/* Rage 128 4x */
#define	PCI_PRODUCT_ATI_RAGE24XPCI	0x534b		/* Rage 128 4x PCI */
#define	PCI_PRODUCT_ATI_RAGE24XA2X	0x534c		/* Rage 128 4x AGP 2x */
#define	PCI_PRODUCT_ATI_RAGE24XA4X	0x534d		/* Rage 128 4x AGP 4x */
#define	PCI_PRODUCT_ATI_RAGE24X	0x534e		/* Rage 128 4x */
#define	PCI_PRODUCT_ATI_RAGE128PROULTRATF	0x5446		/* Rage 128 Pro Ultra TF AGP */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5460	0x5460		/* Radeon Mobility M300 (M22) 5460 */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5464	0x5464		/* FireGL M22 GL 5464 */
#define	PCI_PRODUCT_ATI_RADEON_R423_UH	0x5548		/* Radeon X800 (R423) UH */
#define	PCI_PRODUCT_ATI_RADEON_R423_UI	0x5549		/* Radeon X800PRO (R423) UI */
#define	PCI_PRODUCT_ATI_RADEON_R423_UJ	0x554a		/* Radeon X800LE (R423) UJ */
#define	PCI_PRODUCT_ATI_RADEON_R423_UK	0x554b		/* Radeon X800SE (R423) UK */
#define	PCI_PRODUCT_ATI_RADEON_R430_554F	0x554f		/* Radeon X800 GTO (R430) 554F */
#define	PCI_PRODUCT_ATI_RADEON_R423_UQ	0x5551		/* FireGL V7200 (R423) UQ */
#define	PCI_PRODUCT_ATI_RADEON_R423_UR	0x5552		/* FireGL V5100 (R423) UR */
#define	PCI_PRODUCT_ATI_RADEON_R423_UT	0x5554		/* FireGL V7100 (R423) UT */
#define	PCI_PRODUCT_ATI_RADEON_R430_556F	0x556f		/* Radeon X800 GTO (R430) Secondary */
#define	PCI_PRODUCT_ATI_MACH64_VT	0x5654		/* Mach64 VT */
#define	PCI_PRODUCT_ATI_MACH64_VTB	0x5655		/* Mach64 VTB */
#define	PCI_PRODUCT_ATI_MACH64_VT4	0x5656		/* Mach64 VT4 */
#define	PCI_PRODUCT_ATI_RS300_HB	0x5833		/* RS300 Host Bridge */
#define	PCI_PRODUCT_ATI_RADEON_RS300_X4	0x5834		/* Radeon 9100 IGP (A4) */
#define	PCI_PRODUCT_ATI_RADEON_RS300_X5	0x5835		/* Radeon Mobility 9100 IGP (U3) */
#define	PCI_PRODUCT_ATI_RS300_AGP	0x5838		/* RS300 AGP Interface */
#define	PCI_PRODUCT_ATI_RADEON_9200_PRO_S	0x5940		/* Radeon 9200 Pro Secondary */
#define	PCI_PRODUCT_ATI_RADEON_9200_S	0x5941		/* Radeon 9200 Secondary */
#define	PCI_PRODUCT_ATI_RS480_HB	0x5950		/* RS480 Host Bridge */
#define	PCI_PRODUCT_ATI_RD580	0x5952		/* RD580 CrossFire Xpress 3200 Host Bridge */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5954	0x5954		/* Radeon Xpress 200G Series */
#define	PCI_PRODUCT_ATI_RD790_NB	0x5956		/* RD790 North Bridge (Dual Slot) */
#define	PCI_PRODUCT_ATI_RX780_790_HB	0x5957		/* RX780/RX790 Chipset Host Bridge */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5960	0x5960		/* Radeon 9200PRO 5960 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5961	0x5961		/* Radeon 9200 5961 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5962	0x5962		/* Radeon 9200 5962 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5963	0x5963		/* Radeon 9200 5963 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5964	0x5964		/* Radeon 9200SE 5964 */
#define	PCI_PRODUCT_ATI_RS482M	0x5975		/* Radeon Xpress Series (RS482M) */
#define	PCI_PRODUCT_ATI_RD790_PPB_GFX0_A	0x5978		/* RD790 PCI Bridge GFX0 Port A */
#define	PCI_PRODUCT_ATI_RD790_PPB_GFX0_B	0x5979		/* RD790 PCI Bridge GFX0 Port B */
#define	PCI_PRODUCT_ATI_RD790_PPB_GPP_A	0x597a		/* RD790 PCI Express Bridge GPP Port A */
#define	PCI_PRODUCT_ATI_RD790_PPB_GPP_B	0x597b		/* RD790 PCI Express Bridge GPP Port B */
#define	PCI_PRODUCT_ATI_RD790_PPB_GPP_C	0x597c		/* RD790 PCI Express Bridge GPP Port C */
#define	PCI_PRODUCT_ATI_RD790_PPB_GPP_D	0x597d		/* RD790 PCI Express Bridge GPP Port D */
#define	PCI_PRODUCT_ATI_RD790_PPB_GPP_E	0x597e		/* RD790 PCI Express Bridge GPP Port E */
#define	PCI_PRODUCT_ATI_RD790_PPB_GPP_F	0x597f		/* RD790 PCI Express Bridge GPP Port F */
#define	PCI_PRODUCT_ATI_RD790_PPB_GFX1_A	0x5980		/* RD790 PCI Bridge GFX1 Port A */
#define	PCI_PRODUCT_ATI_RD790_PPB_GFX1_B	0x5981		/* RD790 PCI Bridge GFX1 Port B */
#define	PCI_PRODUCT_ATI_RD790_PPB_NBSB	0x5982		/* RD790 PCI Bridge (NB-SB Link) */
#define	PCI_PRODUCT_ATI_RD890_NB_DS16	0x5a10		/* RD890 North Bridge Dual Slot 2x16 GFX */
#define	PCI_PRODUCT_ATI_RD890_NB_SS	0x5a11		/* RD890 North Bridge Single Slot GFX */
#define	PCI_PRODUCT_ATI_RD890_NB_DS8	0x5a12		/* RD890 North Bridge Dual Slot 2x8 GFX */
#define	PCI_PRODUCT_ATI_RD890_PPB_GFX0_A	0x5a13		/* RD890 PCI Bridge GFX0 Port A */
#define	PCI_PRODUCT_ATI_RD890_PPB_GFX0_B	0x5a14		/* RD890 PCI Bridge GFX0 Port B */
#define	PCI_PRODUCT_ATI_RD890_PPB_GPP_A	0x5a15		/* RD890 PCI Express Bridge GPP Port A */
#define	PCI_PRODUCT_ATI_RD890_PPB_GPP_B	0x5a16		/* RD890 PCI Express Bridge GPP Port B */
#define	PCI_PRODUCT_ATI_RD890_PPB_GPP_C	0x5a17		/* RD890 PCI Express Bridge GPP Port C */
#define	PCI_PRODUCT_ATI_RD890_PPB_GPP_D	0x5a18		/* RD890 PCI Express Bridge GPP Port D */
#define	PCI_PRODUCT_ATI_RD890_PPB_GPP_E	0x5a19		/* RD890 PCI Express Bridge GPP Port E */
#define	PCI_PRODUCT_ATI_RD890_PPB_GPP_F	0x5a1a		/* RD890 PCI Express Bridge GPP Port F */
#define	PCI_PRODUCT_ATI_RD890_PPB_GPP_G	0x5a1b		/* RD890 PCI Express Bridge GPP Port G */
#define	PCI_PRODUCT_ATI_RD890_PPB_GPP_H	0x5a1c		/* RD890 PCI Express Bridge GPP Port H */
#define	PCI_PRODUCT_ATI_RD890_PPB_GFX1_A	0x5a1d		/* RD890 PCI Bridge GFX1 Port A */
#define	PCI_PRODUCT_ATI_RD890_PPB_GFX1_B	0x5a1e		/* RD890 PCI Bridge GFX1 Port B */
#define	PCI_PRODUCT_ATI_RD890_PPB_NBSB	0x5a1f		/* RD890 PCI Bridge (NB-SB Link) */
#define	PCI_PRODUCT_ATI_RD890_IOMMU	0x5a23		/* RD890 IOMMU */
#define	PCI_PRODUCT_ATI_RADEON_XPRESS_200	0x5a33		/* Radeon Xpress 200 */
#define	PCI_PRODUCT_ATI_RS480_XRP	0x5a34		/* RS480 PCI Express Root Port */
#define	PCI_PRODUCT_ATI_RS480_PPB_5A36	0x5a36		/* RS480 PCI Express Bridge */
#define	PCI_PRODUCT_ATI_RS480_PPB_5A37	0x5a37		/* RS480 PCI Express Bridge */
#define	PCI_PRODUCT_ATI_RS480_PPB_5A38	0x5a38		/* RS480 PCI Express Bridge */
#define	PCI_PRODUCT_ATI_RS480_PPB_5A3F	0x5a3f		/* RS480 PCI Express Bridge */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5B60	0x5b60		/* Radeon X300 (RV370) 5B60 */
#define	PCI_PRODUCT_ATI_RADEON_RV380_5B62	0x5b62		/* Radeon X600 PCI Express */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5B63	0x5b63		/* Radeon Sapphire X550 Silent */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5B64	0x5b64		/* FireGL V3100 (RV370) 5B64 */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5B65	0x5b65		/* FireGL D1100 (RV370) 5B65 */
#define	PCI_PRODUCT_ATI_RADEON_X300_S	0x5b70		/* Radeon X300 Series Secondary */
#define	PCI_PRODUCT_ATI_RADEON_RV370_5B73	0x5b73		/* Radeon RV370 Secondary */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5C61	0x5c61		/* Radeon Mobility 9200 (M9+) */
#define	PCI_PRODUCT_ATI_RADEON_RV280_5C63	0x5c63		/* Radeon Mobility 9200 (M9+) */
#define	PCI_PRODUCT_ATI_RADEON_9200SE_S	0x5d44		/* Radeon 9200SE Secondary */
#define	PCI_PRODUCT_ATI_RADEON_X850XT	0x5d52		/* Radeon X850 XT */
#define	PCI_PRODUCT_ATI_RADEON_R423_5D57	0x5d57		/* Radeon X800XT (R423) 5D57 */
#define	PCI_PRODUCT_ATI_RADEON_X850XT_S	0x5d72		/* Radeon X850 XT Secondary */
#define	PCI_PRODUCT_ATI_RADEON_X700	0x5e4b		/* Radeon X700 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X700_S	0x5e6b		/* Radeon X700 Pro Secondary */
#define	PCI_PRODUCT_ATI_RADEON_HD5870	0x6898		/* Radeon HD 5870 Cypress */
#define	PCI_PRODUCT_ATI_RADEON_HD5600_RD	0x68c1		/* Radeon HD 5600 Redwood */
#define	PCI_PRODUCT_ATI_RADEON_HD5450	0x68f9		/* Radeon HD 5450 */
#define	PCI_PRODUCT_ATI_RADEON_X1300	0x7146		/* Radeon X1300 Series (RV515) */
#define	PCI_PRODUCT_ATI_RADEON_X1300_S	0x7166		/* Radeon X1300 Series (RV515) Secondary */
#define	PCI_PRODUCT_ATI_RADEON_X1600XT	0x71c0		/* Radeon X1600 XT */
#define	PCI_PRODUCT_ATI_RADEON_X1600	0x71c5		/* Radeon Mobility X1600 */
#define	PCI_PRODUCT_ATI_RADEON_X1600XT_S	0x71e0		/* Radeon X1600 XT Secondary */
#define	PCI_PRODUCT_ATI_RADEON_X1950	0x7280		/* Radeon X1950 PRO */
#define	PCI_PRODUCT_ATI_RADEON_X1950_S	0x72a0		/* Radeon X1950 PRO Secondary */
#define	PCI_PRODUCT_ATI_RADEON_RS300_7834	0x7834		/* Radeon 9100 PRO IGP */
#define	PCI_PRODUCT_ATI_RADEON_RS300_7835	0x7835		/* Radeon 9200 IGP */
#define	PCI_PRODUCT_ATI_RS690_HB_7910	0x7910		/* RS690 Host Bridge */
#define	PCI_PRODUCT_ATI_RS690_HB_7911	0x7911		/* RS740 Host Bridge */
#define	PCI_PRODUCT_ATI_RS690_PPB_7912	0x7912		/* RS690 GFX Bridge */
#define	PCI_PRODUCT_ATI_RS690_PPB_7913	0x7913		/* RS690 PCI Express Bridge GFX */
#define	PCI_PRODUCT_ATI_RS690_PPB_7914	0x7914		/* RS690 PCI Express Bridge GPP Port A */
#define	PCI_PRODUCT_ATI_RS690_PPB_7915	0x7915		/* RS690 PCI Express Bridge GPP Port B */
#define	PCI_PRODUCT_ATI_RS690_PPB_7916	0x7916		/* RS690 PCI Express Bridge GPP Port C */
#define	PCI_PRODUCT_ATI_RS690_PPB_7917	0x7917		/* RS690 PCI Express Bridge GPP Port D */
#define	PCI_PRODUCT_ATI_RADEON_HD4850	0x9442		/* Radeon HD4850 */
#define	PCI_PRODUCT_ATI_RADEON_HD4650	0x9498		/* Radeon HD4650 */
#define	PCI_PRODUCT_ATI_RADEON_HD2400_XT	0x94c1		/* Radeon HD2400 XT */
#define	PCI_PRODUCT_ATI_RADEON_HD2400_PRO	0x94c3		/* Radeon HD2400 Pro */
#define	PCI_PRODUCT_ATI_RADEON_HD2400_M72	0x94c9		/* Mobility Radeon HD 2400 */
#define	PCI_PRODUCT_ATI_RADEON_HD3870	0x9501		/* Radeon HD3870 */
#define	PCI_PRODUCT_ATI_RADEON_HD4350	0x954f		/* Radeon HD4350 */
#define	PCI_PRODUCT_ATI_RADEON_HD4500_M93	0x9555		/* Mobility Radeon HD 4500 */
#define	PCI_PRODUCT_ATI_RADEON_HD2600_M76	0x9581		/* Mobility Radeon HD 2600 */
#define	PCI_PRODUCT_ATI_RADEON_HD2600PROAGP	0x9587		/* Radeon HD2600 Pro AGP */
#define	PCI_PRODUCT_ATI_RADEON_HD2600_XT	0x9588		/* Radeon HD2600 XT GDDR3 */
#define	PCI_PRODUCT_ATI_RADEON_HD2600_PRO	0x9589		/* Radeon HD 2600 Pro */
#define	PCI_PRODUCT_ATI_RADEON_HD3650_M	0x9591		/* Mobility Radeon HD 3650 */
#define	PCI_PRODUCT_ATI_RADEON_HD3650_AGP	0x9596		/* Radeon HD3650 AGP */
#define	PCI_PRODUCT_ATI_RADEON_HD3650	0x9598		/* Radeon HD3650 */
#define	PCI_PRODUCT_ATI_RADEON_HD3400_M82	0x95c4		/* Mobility Radeon HD 3400 Series (M82) */
#define	PCI_PRODUCT_ATI_RADEON_HD4250_S	0x95c5		/* Radeon HD4250 GPU (RV610) Secondary */
#define	PCI_PRODUCT_ATI_RADEON_HD6520G	0x9647		/* Radeon HD6520G */
#define	PCI_PRODUCT_ATI_RADEON_HD4200	0x9712		/* Radeon HD4200 Mobility */
#define	PCI_PRODUCT_ATI_RADEON_HD4250	0x9715		/* Radeon HD4250 GPU (RS880) */
#define	PCI_PRODUCT_ATI_RADEON_HD6310	0x9802		/* Radeon HD6310 Graphics */
#define	PCI_PRODUCT_ATI_RADEON_HD6320	0x9806		/* Radeon HD6320 Graphics */
#define	PCI_PRODUCT_ATI_RADEON_HD7340	0x9808		/* Radeon HD7340 Graphics */
#define	PCI_PRODUCT_ATI_RADEON_HD2600_HD	0xaa08		/* Radeon HD2600 HD Audio Controller */
#define	PCI_PRODUCT_ATI_RADEON_HD4350_HD	0xaa38		/* Radeon HD4350 HD Audio Controller */
#define	PCI_PRODUCT_ATI_RADEON_HD5600_HDMI	0xaa60		/* Redwood HDMI Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD54XX_HDA	0xaa68		/* Radeon HD 54xx Audio */

/* Auravision products */
#define	PCI_PRODUCT_AURAVISION_VXP524	0x01f7		/* VxP524 PCI Video Processor */

/* Aureal Semiconductor */
#define	PCI_PRODUCT_AUREAL_AU8820	0x0001		/* AU8820 Vortex Digital Audio Processor */
#define	PCI_PRODUCT_AUREAL_AU8830	0x0002		/* AU8830 Vortex 3D Digital Audio Processor */

/* Applied Micro Circuts products */
#define	PCI_PRODUCT_AMCIRCUITS_S5933	0x4750		/* S5933 PCI Matchmaker */
#define	PCI_PRODUCT_AMCIRCUITS_LANAI	0x8043		/* Myrinet LANai Interface */
#define	PCI_PRODUCT_AMCIRCUITS_CAMAC	0x812d		/* FZJ/ZEL CAMAC Controller */
#define	PCI_PRODUCT_AMCIRCUITS_VICBUS	0x812e		/* FZJ/ZEL VICBUS Interface */
#define	PCI_PRODUCT_AMCIRCUITS_PCISYNC	0x812f		/* FZJ/ZEL Synchronisation Module */
#define	PCI_PRODUCT_AMCIRCUITS_ADDI7800	0x818e		/* ADDI-DATA APCI-7800 8-port Serial */
#define	PCI_PRODUCT_AMCIRCUITS_S5920	0x5920		/* S5920 PCI Target */

/* ASPEED Technology products */
#define	PCI_PRODUCT_ASPEED_AST1150	0x1150		/* AST1150 PCIe-to-PCI bridge */
#define	PCI_PRODUCT_ASPEED_AST1180	0x1180		/* AST1180 */
#define	PCI_PRODUCT_ASPEED_AST2000	0x2000		/* ASPEED Graphics Family */

/* Atheros Communications products */
#define	PCI_PRODUCT_ATHEROS_AR5201	0x0007		/* AR5201 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5311	0x0011		/* AR5211 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5211	0x0012		/* AR5211 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5212	0x0013		/* AR5212 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5212_2	0x0014		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_3	0x0015		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_4	0x0016		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_5	0x0017		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_6	0x0018		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_7	0x0019		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR2413	0x001a		/* AR2413 */
#define	PCI_PRODUCT_ATHEROS_AR5413	0x001b		/* AR5413 */
#define	PCI_PRODUCT_ATHEROS_AR5424	0x001c		/* AR5424 */
#define	PCI_PRODUCT_ATHEROS_AR5416	0x0023		/* AR5416 */
#define	PCI_PRODUCT_ATHEROS_AR5418	0x0024		/* AR5418 */
#define	PCI_PRODUCT_ATHEROS_AR9160	0x0027		/* AR9160 */
#define	PCI_PRODUCT_ATHEROS_AR9280	0x0029		/* AR9280 */
#define	PCI_PRODUCT_ATHEROS_AR9281	0x002a		/* AR9281 */
#define	PCI_PRODUCT_ATHEROS_AR9285	0x002b		/* AR9285 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR2427	0x002c		/* AR2427 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR9227	0x002d		/* AR9227 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR9287	0x002e		/* AR9287 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR9300	0x0030		/* AR9300 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR9485	0x0032		/* AR9485 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR9462	0x0034		/* AR9462 Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_QCA988X	0x003c		/* QCA986x/988x Wireless LAN */
#define	PCI_PRODUCT_ATHEROS_AR5201_AP	0x0207		/* AR5201 Wireless LAN Reference Card (Early AP11) */
#define	PCI_PRODUCT_ATHEROS_AR5201_DEFAULT	0x1107		/* AR5201 Wireless LAN (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5212_DEFAULT	0x1113		/* AR5212 Wireless LAN (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5211_DEFAULT	0x1112		/* AR5211 Wireless LAN (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5212_FPGA	0xf013		/* AR5212 Wireless LAN Reference Card (emulation board) */
#define	PCI_PRODUCT_ATHEROS_AR5211_FPGA11B	0xf11b		/* AR5211 Wireless LAN Reference Card (11b emulation board) */
#define	PCI_PRODUCT_ATHEROS_AR5211_LEGACY	0xff12		/* AR5211 Wireless LAN Reference Card (original emulation board) */

/* Atronics products */
#define	PCI_PRODUCT_ATRONICS_IDE_2015PL	0x2015		/* IDE-2015PL */

/* Avance Logic products */
#define	PCI_PRODUCT_AVANCE_AVL2301	0x2301		/* AVL2301 */
#define	PCI_PRODUCT_AVANCE_AVG2302	0x2302		/* AVG2302 */
#define	PCI_PRODUCT_AVANCE2_ALG2301	0x2301		/* ALG2301 */
#define	PCI_PRODUCT_AVANCE2_ALG2302	0x2302		/* ALG2302 */
#define	PCI_PRODUCT_AVANCE2_ALS4000	0x4000		/* ALS4000 Audio */

/* Avlab Technology products */
#define	PCI_PRODUCT_AVLAB_PCI2S	0x2130		/* Low Profile PCI 4 Serial */
#define	PCI_PRODUCT_AVLAB_LPPCI4S	0x2150		/* Low Profile PCI 4 Serial */
#define	PCI_PRODUCT_AVLAB_LPPCI4S_2	0x2152		/* Low Profile PCI 4 Serial */

/* CCUBE products */
#define	PCI_PRODUCT_CCUBE_CINEMASTER	0x8888		/* Cinemaster C 3.0 DVD Decoder */

/* AVM products */
#define	PCI_PRODUCT_AVM_FRITZ_CARD	0x0a00		/* Fritz! Card ISDN Interface */
#define	PCI_PRODUCT_AVM_FRITZ_PCI_V2_ISDN	0x0e00		/* Fritz!PCI v2.0 ISDN Interface */
#define	PCI_PRODUCT_AVM_B1	0x0700		/* Basic Rate B1 ISDN Interface */
#define	PCI_PRODUCT_AVM_T1	0x1200		/* Primary Rate T1 ISDN Interface */

/* RMI products */
#define	PCI_PRODUCT_RMI_XLR_PCIX	0x000b		/* XLR PCI-X bridge */
#define	PCI_PRODUCT_RMI_XLS_PCIE	0xabcd		/* XLS PCIe-PCIe bridge */

/* B & B Electronics Products */
#define	PCI_PRODUCT_BBELEC_NON_ISOLATED_1_PORT	0x4201		/* single-channel RS-485 PCI UART */
#define	PCI_PRODUCT_BBELEC_NON_ISOLATED_2_PORT	0x4202		/* dual-channel RS-485 PCI UART */
#define	PCI_PRODUCT_BBELEC_NON_ISOLATED_4_PORT	0x4204		/* quad-channel RS-485 PCI UART */
#define	PCI_PRODUCT_BBELEC_NON_ISOLATED_8_PORT	0x4208		/* octal-channel RS-485 PCI UART */
#define	PCI_PRODUCT_BBELEC_ISOLATED_1_PORT	0x4211		/* single-channel Isolated RS-485 PCI UART */
#define	PCI_PRODUCT_BBELEC_ISOLATED_2_PORT	0x4212		/* dual-channel Isolated RS-485 PCI UART */
#define	PCI_PRODUCT_BBELEC_ISOLATED_4_PORT	0x4214		/* quad-channel Isolated RS-485 PCI UART */
#define	PCI_PRODUCT_BBELEC_ISOLATED_8_PORT	0x4218		/* octal-channel Isolated RS-485 PCI UART */

/* Belkin products */
#define	PCI_PRODUCT_BELKIN_F5D6001	0x6001		/* F5D6001 */
#define	PCI_PRODUCT_BELKIN_F5D6020V3	0x6020		/* F5D6020v3 802.11b */
#define	PCI_PRODUCT_BELKIN_F5D7010	0x701f		/* F5D7010 */

/* Stallion products */
#define	PCI_PRODUCT_STALLION_EC8_32	0x0000		/* EC8/32 */
#define	PCI_PRODUCT_STALLION_EC8_64	0x0002		/* EC8/64 */
#define	PCI_PRODUCT_STALLION_EASYIO	0x0003		/* EasyIO */

/* Bit3 products */
#define	PCI_PRODUCT_BIT3_PCIVME617	0x0001		/* PCI-VME Interface Mod. 617 */
#define	PCI_PRODUCT_BIT3_PCIVME618	0x0010		/* PCI-VME Interface Mod. 618 */
#define	PCI_PRODUCT_BIT3_PCIVME2706	0x0300		/* PCI-VME Interface Mod. 2706 */

/* Bluesteel Networks */
#define	PCI_PRODUCT_BLUESTEEL_5501	0x0000		/* 5501 */
#define	PCI_PRODUCT_BLUESTEEL_5601	0x5601		/* 5601 */

/* Broadcom products */
#define	PCI_PRODUCT_BROADCOM_BCM5752	0x1600		/* BCM5752 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5752M	0x1601		/* BCM5752M NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5709	0x1639		/* BCM5709 NetXtreme II 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5709S	0x163a		/* BCM5709 NetXtreme II 1000baseSX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5716	0x163b		/* BCM5716 NetXtreme II 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5716S	0x163c		/* BCM5716 NetXtreme II 1000baseSX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5700	0x1644		/* BCM5700 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5701	0x1645		/* BCM5701 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5702	0x1646		/* BCM5702 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5703	0x1647		/* BCM5703 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5704C	0x1648		/* BCM5704C 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5704S_ALT	0x1649		/* BCM5704S 1000baseSX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5706	0x164a		/* BCM5706 NetXtreme II 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5708	0x164c		/* BCM5708 NetXtreme II 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5702FE	0x164d		/* BCM5702FE 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57710	0x164e		/* BCM57710 NetXtreme II 10Gb Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57711	0x164f		/* BCM57711 NetXtreme II 10Gb Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57711E	0x1650		/* BCM57711E NetXtreme II 10Gb Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705	0x1653		/* BCM5705 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705K	0x1654		/* BCM5705K 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5717	0x1655		/* BCM5717 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5718	0x1656		/* BCM5718 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5719	0x1657		/* BCM5719 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5721	0x1659		/* BCM5721 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5722	0x165a		/* BCM5722 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5723	0x165b		/* BCM5723 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5724	0x165c		/* BCM5724 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705M	0x165d		/* BCM5705M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705M_ALT	0x165e		/* BCM5705M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5720	0x165f		/* BCM5720 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57712	0x1662		/* BCM57712 NetXtreme II 10Gb Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57712E	0x1663		/* BCM57712E NetXtreme II 10Gb Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5714	0x1668		/* BCM5714 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5714S	0x1669		/* BCM5714S 1000baseSX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5780	0x166a		/* BCM5780 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5780S	0x166b		/* BCM5780S NetXtreme 1000baseSX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5705F	0x166e		/* BCM5705F 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5754M	0x1672		/* BCM5754M NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5755M	0x1673		/* BCM5755M NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5756	0x1674		/* BCM5756 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5750	0x1676		/* BCM5750 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5751	0x1677		/* BCM5751 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5715	0x1678		/* BCM5715 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5715S	0x1679		/* BCM5715S 1000baseSX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5754	0x167a		/* BCM5754 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5755	0x167b		/* BCM5755 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5750M	0x167c		/* BCM5750M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5751M	0x167d		/* BCM5751M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5751F	0x167e		/* BCM5751F 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5787F	0x167f		/* BCM5787F 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5761E	0x1680		/* BCM5761E 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5761	0x1681		/* BCM5761 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57762	0x1682		/* BCM57762 Gigabit Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5764	0x1684		/* BCM5764 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57766	0x1686		/* BCM57766 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5761S	0x1688		/* BCM5761S 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5761SE	0x1689		/* BCM5761SE 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57760	0x1690		/* BCM57760 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57788	0x1691		/* BCM57788 NetLink 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57780	0x1692		/* BCM57780 NetXtreme 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5787M	0x1693		/* BCM5787M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57790	0x1694		/* BCM57790 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5782	0x1696		/* BCM5782 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5784M	0x1698		/* BCM5784M NetLink 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5785G	0x1699		/* BCM5785G 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5786	0x169a		/* BCM5786 NetLink 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5787	0x169b		/* BCM5787 NetLink 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5788	0x169c		/* BCM5788 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5789	0x169d		/* BCM5789 NetLink 1000baseT Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5785F	0x16a0		/* BCM5785F 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5702X	0x16a6		/* BCM5702X 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5703X	0x16a7		/* BCM5703X 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5704S	0x16a8		/* BCM5704S 1000baseSX Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5706S	0x16aa		/* BCM5706 NetXtreme II 1000baseSX */
#define	PCI_PRODUCT_BROADCOM_BCM5708S	0x16ac		/* BCM5708 NetXtreme II 1000baseSX */
#define	PCI_PRODUCT_BROADCOM_BCM57761	0x16b0		/* BCM57761 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57781	0x16b1		/* BCM57781 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57791	0x16b2		/* BCM57791 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57786	0x16b3		/* BCM57786 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57765	0x16b4		/* BCM57765 Integrated Gigabit Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57785	0x16b5		/* BCM57785 Integrated Gigabit Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57795	0x16b6		/* BCM57795 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM57782	0x16b7		/* BCM57782 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5702_ALT	0x16c6		/* BCM5702 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5703_ALT	0x16c7		/* BCM5703 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5781	0x16dd		/* BCM5781 Integrated Gigabit Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5753	0x16f7		/* BCM5753 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5753M	0x16fd		/* BCM5753M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5753F	0x16fe		/* BCM5753F 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5903M	0x16ff		/* BCM5903M 10/100/1000 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM4401_B0	0x170c		/* BCM4401-B0 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5901	0x170d		/* BCM5901 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5901A2	0x170e		/* BCM5901A 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5906	0x1712		/* BCM5906 NetLink Fast Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM5906M	0x1713		/* BCM5906M NetLink Fast Ethernet */
#define	PCI_PRODUCT_BROADCOM_BCM4303	0x4301		/* BCM4303 */
#define	PCI_PRODUCT_BROADCOM_BCM4307	0x4307		/* BCM4307 */
#define	PCI_PRODUCT_BROADCOM_BCM4311	0x4311		/* BCM4311 2.4GHz */
#define	PCI_PRODUCT_BROADCOM_BCM4312	0x4312		/* BCM4312 Dualband */
#define	PCI_PRODUCT_BROADCOM_BCM4313	0x4313		/* BCM4313 5GHz */
#define	PCI_PRODUCT_BROADCOM_BCM4315	0x4315		/* BCM4315 2.4GHz */
#define	PCI_PRODUCT_BROADCOM_BCM4318	0x4318		/* BCM4318 AirForce One 54g */
#define	PCI_PRODUCT_BROADCOM_BCM4319	0x4319		/* BCM4319 */
#define	PCI_PRODUCT_BROADCOM_BCM4306	0x4320		/* BCM4306 */
#define	PCI_PRODUCT_BROADCOM_BCM4306_2	0x4321		/* BCM4306 */
#define	PCI_PRODUCT_BROADCOM_BCM4322	0x4322		/* BCM4322 */
#define	PCI_PRODUCT_BROADCOM_BCM4309	0x4324		/* BCM4309 */
#define	PCI_PRODUCT_BROADCOM_BCM43XG	0x4325		/* BCM43XG */
#define	PCI_PRODUCT_BROADCOM_BCM4328	0x4328		/* BCM4328 802.11a/b/g/n */
#define	PCI_PRODUCT_BROADCOM_BCM4329	0x4329		/* BCM4329 802.11b/g/n */
#define	PCI_PRODUCT_BROADCOM_BCM432A	0x432a		/* BCM432A 802.11 */
#define	PCI_PRODUCT_BROADCOM_BCM432B	0x432b		/* BCM432B 802.11a/b/g/n */
#define	PCI_PRODUCT_BROADCOM_BCM432C	0x432c		/* BCM432C 802.11b/g/n */
#define	PCI_PRODUCT_BROADCOM_BCM432D	0x432d		/* BCM432D 802.11 */
#define	PCI_PRODUCT_BROADCOM_BCM43224	0x4353		/* BCM43224 Dualband 802.11 */
#define	PCI_PRODUCT_BROADCOM_BCM43225	0x4357		/* BCM43225 2.4GHz 802.11 */
#define	PCI_PRODUCT_BROADCOM_BCM43227	0x4358		/* BCM43227 2.4GHz 802.11 */
#define	PCI_PRODUCT_BROADCOM_BCM43228	0x4359		/* BCM43228 Dualband 802.11 */
#define	PCI_PRODUCT_BROADCOM_BCM4401	0x4401		/* BCM4401 10/100 Ethernet */
#define	PCI_PRODUCT_BROADCOM_5801	0x5801		/* 5801 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5802	0x5802		/* 5802 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5805	0x5805		/* 5805 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5820	0x5820		/* 5820 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5821	0x5821		/* 5821 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5822	0x5822		/* 5822 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5823	0x5823		/* 5823 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5825	0x5825		/* 5825 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5860	0x5860		/* 5860 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5861	0x5861		/* 5861 Security Processor */
#define	PCI_PRODUCT_BROADCOM_5862	0x5862		/* 5862 Security Processor */

/* Brooktree products */
#define	PCI_PRODUCT_BROOKTREE_BT848	0x0350		/* Bt848 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT849	0x0351		/* Bt849 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT878	0x036e		/* Bt878 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT879	0x036f		/* Bt879 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT880	0x0370		/* Bt880 Video Capture */
#define	PCI_PRODUCT_BROOKTREE_BT878A	0x0878		/* Bt878 Video Capture (Audio Section) */
#define	PCI_PRODUCT_BROOKTREE_BT879A	0x0879		/* Bt879 Video Capture (Audio Section) */
#define	PCI_PRODUCT_BROOKTREE_BT880A	0x0880		/* Bt880 Video Capture (Audio Section) */
#define	PCI_PRODUCT_BROOKTREE_BT8474	0x8474		/* Bt8474 Multichannel HDLC Controller */

/* BusLogic products */
#define	PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC	0x0140		/* MultiMaster NC */
#define	PCI_PRODUCT_BUSLOGIC_MULTIMASTER	0x1040		/* MultiMaster */
#define	PCI_PRODUCT_BUSLOGIC_FLASHPOINT	0x8130		/* FlashPoint */

/* c't Magazin products */
#define	PCI_PRODUCT_C4T_GPPCI	0x6773		/* GPPCI */

/* Cavium products */
#define	PCI_PRODUCT_CAVIUM_NITROX	0x0001		/* Nitrox XL */

/* Chelsio products */
#define	PCI_PRODUCT_CHELSIO_T302E	0x0021		/* T302e */
#define	PCI_PRODUCT_CHELSIO_T310E	0x0022		/* T310e */
#define	PCI_PRODUCT_CHELSIO_T320X	0x0023		/* T320x */
#define	PCI_PRODUCT_CHELSIO_T302X	0x0024		/* T302x */
#define	PCI_PRODUCT_CHELSIO_T320E	0x0025		/* T320e */
#define	PCI_PRODUCT_CHELSIO_T310X	0x0026		/* T310x */
#define	PCI_PRODUCT_CHELSIO_T3B10	0x0030		/* T3B10 */
#define	PCI_PRODUCT_CHELSIO_T3B20	0x0031		/* T3B20 */
#define	PCI_PRODUCT_CHELSIO_T3B02	0x0032		/* T3B02 */

/* Chips and Technologies products */
#define	PCI_PRODUCT_CHIPS_64310	0x00b8		/* 64310 */
#define	PCI_PRODUCT_CHIPS_69000	0x00c0		/* 69000 */
#define	PCI_PRODUCT_CHIPS_65545	0x00d8		/* 65545 */
#define	PCI_PRODUCT_CHIPS_65548	0x00dc		/* 65548 */
#define	PCI_PRODUCT_CHIPS_65550	0x00e0		/* 65550 */
#define	PCI_PRODUCT_CHIPS_65554	0x00e4		/* 65554 */
#define	PCI_PRODUCT_CHIPS_69030	0x0c30		/* 69030 */

/* Chrysalis products */
#define	PCI_PRODUCT_CHRYSALIS_LUNAVPN	0x0001		/* LunaVPN */

/* Cirrus Logic products */
#define	PCI_PRODUCT_CIRRUS_CL_GD7548	0x0038		/* CL-GD7548 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5430	0x00a0		/* CL-GD5430 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5434_4	0x00a4		/* CL-GD5434-4 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5434_8	0x00a8		/* CL-GD5434-8 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5436	0x00ac		/* CL-GD5436 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5446	0x00b8		/* CL-GD5446 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5480	0x00bc		/* CL-GD5480 */
#define	PCI_PRODUCT_CIRRUS_CL_PD6729	0x1100		/* CL-PD6729 */
#define	PCI_PRODUCT_CIRRUS_CL_PD6832	0x1110		/* CL-PD6832 PCI-CardBus Bridge */
#define	PCI_PRODUCT_CIRRUS_CL_PD6833	0x1113		/* CL-PD6833 PCI-CardBus Bridge */
#define	PCI_PRODUCT_CIRRUS_CL_GD7542	0x1200		/* CL-GD7542 */
#define	PCI_PRODUCT_CIRRUS_CL_GD7543	0x1202		/* CL-GD7543 */
#define	PCI_PRODUCT_CIRRUS_CL_GD7541	0x1204		/* CL-GD7541 */
#define	PCI_PRODUCT_CIRRUS_CL_CD4400	0x4400		/* CL-CD4400 Communications Controller */
#define	PCI_PRODUCT_CIRRUS_CS4610	0x6001		/* CS4610 SoundFusion Audio Accelerator */
#define	PCI_PRODUCT_CIRRUS_CS4280	0x6003		/* CS4280 CrystalClear Audio Interface */
#define	PCI_PRODUCT_CIRRUS_CS4615	0x6004		/* CS4615 */
#define	PCI_PRODUCT_CIRRUS_CS4281	0x6005		/* CS4281 CrystalClear Audio Interface */

/* Adaptec's AAR-1210SA serial ATA RAID controller uses the CMDTECH chip */
#define	PCI_PRODUCT_CMDTECH_AAR_1210SA	0x0240		/* AAR-1210SA SATA RAID Controller */
/* CMD Technology products -- info gleaned from their web site */
#define	PCI_PRODUCT_CMDTECH_640	0x0640		/* PCI0640 */
/* No data on the CMD Tech. web site for the following as of Mar. 3 '98 */
#define	PCI_PRODUCT_CMDTECH_642	0x0642		/* PCI0642 */
/* datasheets available from www.cmd.com for the followings */
#define	PCI_PRODUCT_CMDTECH_643	0x0643		/* PCI0643 */
#define	PCI_PRODUCT_CMDTECH_646	0x0646		/* PCI0646 */
#define	PCI_PRODUCT_CMDTECH_647	0x0647		/* PCI0647 */
#define	PCI_PRODUCT_CMDTECH_648	0x0648		/* PCI0648 */
#define	PCI_PRODUCT_CMDTECH_649	0x0649		/* PCI0649 */

/* Inclusion of 'A' in the following entry is probably wrong. */
/* No data on the CMD Tech. web site for the following as of Mar. 3 '98 */
#define	PCI_PRODUCT_CMDTECH_240	0x0240		/* Sil240 SATALink */
#define	PCI_PRODUCT_CMDTECH_650A	0x0650		/* PCI0650A */
#define	PCI_PRODUCT_CMDTECH_670	0x0670		/* USB0670 */
#define	PCI_PRODUCT_CMDTECH_673	0x0673		/* USB0673 */
#define	PCI_PRODUCT_CMDTECH_680	0x0680		/* SiI0680 */
#define	PCI_PRODUCT_CMDTECH_3112	0x3112		/* SiI3112 SATALink */
#define	PCI_PRODUCT_CMDTECH_3114	0x3114		/* SiI3114 SATALink */
#define	PCI_PRODUCT_CMDTECH_3124	0x3124		/* SiI3124 SATALink */
#define	PCI_PRODUCT_CMDTECH_3132	0x3132		/* SiI3132 SATALink */
#define	PCI_PRODUCT_CMDTECH_3512	0x3512		/* SiI3512 SATALink */
#define	PCI_PRODUCT_CMDTECH_3531	0x3531		/* SiI3531 SATALink */

/* C-Media products */
#define	PCI_PRODUCT_CMEDIA_CMI8338A	0x0100		/* CMI8338A PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_CMI8338B	0x0101		/* CMI8338B PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_CMI8738	0x0111		/* CMI8738/C3DX PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_CMI8738B	0x0112		/* CMI8738B PCI Audio Device */
#define	PCI_PRODUCT_CMEDIA_HSP56	0x0211		/* HSP56 Audiomodem Riser */

/* Cogent Data Technologies products */
#define	PCI_PRODUCT_COGENT_EM110TX	0x1400		/* EX110TX PCI Fast Ethernet Adapter */

/* Cologne Chip Designs */
#define	PCI_PRODUCT_COLOGNECHIP_HFC	0x2bd0		/* HFC-S */

/* COMPAL products */
#define	PCI_PRODUCT_COMPAL_38W2	0x0011		/* 38W2 OEM Notebook */

/* Compaq products */
#define	PCI_PRODUCT_COMPAQ_PCI_EISA_BRIDGE	0x0001		/* PCI-EISA Bridge */
#define	PCI_PRODUCT_COMPAQ_PCI_ISA_BRIDGE	0x0002		/* PCI-ISA Bridge */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX1	0x1000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX2	0x2000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_QVISION_V0	0x3032		/* QVision */
#define	PCI_PRODUCT_COMPAQ_QVISION_1280P	0x3033		/* QVision 1280/p */
#define	PCI_PRODUCT_COMPAQ_QVISION_V2	0x3034		/* QVision */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX4	0x4000		/* Triflex Host-PCI Bridge */
#define	PCI_PRODUCT_COMPAQ_CSA5300	0x4070		/* Smart Array 5300 */
#define	PCI_PRODUCT_COMPAQ_CSA5i	0x4080		/* Smart Array 5i */
#define	PCI_PRODUCT_COMPAQ_CSA532	0x4082		/* Smart Array 532 */
#define	PCI_PRODUCT_COMPAQ_CSA5312	0x4083		/* Smart Array 5312 */
#define	PCI_PRODUCT_COMPAQ_CSA6i	0x4091		/* Smart Array 6i */
#define	PCI_PRODUCT_COMPAQ_CSA641	0x409a		/* Smart Array 641 */
#define	PCI_PRODUCT_COMPAQ_CSA642	0x409b		/* Smart Array 642 */
#define	PCI_PRODUCT_COMPAQ_CSA6400	0x409c		/* Smart Array 6400 */
#define	PCI_PRODUCT_COMPAQ_CSA6400EM	0x409d		/* Smart Array 6400 EM */
#define	PCI_PRODUCT_COMPAQ_CSA6422	0x409e		/* Smart Array 6422 */
#define	PCI_PRODUCT_COMPAQ_CSA64XX	0x0046		/* Smart Array 64xx */
#define	PCI_PRODUCT_COMPAQ_USB	0x7020		/* USB Controller */
#define	PCI_PRODUCT_COMPAQ_ASMC	0xa0f0		/* Advanced Systems Management Controller */
/* MediaGX Cx55x0 built-in OHCI seems to have this ID */
#define	PCI_PRODUCT_COMPAQ_USB_MEDIAGX	0xa0f8		/* USB Controller */
#define	PCI_PRODUCT_COMPAQ_SMART2P	0xae10		/* SMART2P RAID */
#define	PCI_PRODUCT_COMPAQ_N100TX	0xae32		/* Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_N10T2	0xb012		/* Netelligent 10 T/2 UTP/Coax */
#define	PCI_PRODUCT_COMPAQ_INT100TX	0xb030		/* Integrated Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_N10T	0xae34		/* Netelligent 10 T */
#define	PCI_PRODUCT_COMPAQ_IntNF3P	0xae35		/* Integrated NetFlex 3/P */
#define	PCI_PRODUCT_COMPAQ_DPNet100TX	0xae40		/* Dual Port Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_IntPL100TX	0xae43		/* ProLiant Integrated Netelligent 10/100 TX */
#define	PCI_PRODUCT_COMPAQ_DP4000	0xb011		/* Deskpro 4000 5233MMX */
#define	PCI_PRODUCT_COMPAQ_CSA5300_2	0xb060		/* Smart Array 5300 rev. 2 */
#define	PCI_PRODUCT_COMPAQ_PRESARIO56XX	0xb0b8		/* Presario 56xx */
#define	PCI_PRODUCT_COMPAQ_M700	0xb112		/* Armada M700 */
#define	PCI_PRODUCT_COMPAQ_CSA5i_2	0xb178		/* Smart Array 5i/532 rev. 2 */
#define	PCI_PRODUCT_COMPAQ_ILO_1	0xb203		/* iLO */
#define	PCI_PRODUCT_COMPAQ_ILO_2	0xb204		/* iLO */
#define	PCI_PRODUCT_COMPAQ_NF3P_BNC	0xf150		/* NetFlex 3/P w/ BNC */
#define	PCI_PRODUCT_COMPAQ_NF3P	0xf130		/* NetFlex 3/P */

/* Compex products - XXX better descriptions */
#define	PCI_PRODUCT_COMPEX_NE2KETHER	0x1401		/* Ethernet */
#define	PCI_PRODUCT_COMPEX_RL100ATX	0x2011		/* RL100-ATX 10/100 Ethernet */
#define	PCI_PRODUCT_COMPEX_RL100TX	0x9881		/* RL100-TX 10/100 Ethernet */

/* Comtrol products */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT32EXT	0x0001		/* RocketPort 32 Port External */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT8EXT	0x0002		/* RocketPort 8 Port External */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT16EXT	0x0003		/* RocketPort 16 Port External */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT4QUAD	0x0004		/* RocketPort 4 Port w/ Quad Cable */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT8OCTA	0x0005		/* RocketPort 8 Port w/ Octa Cable */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT8RJ	0x0006		/* RocketPort 8 Port w/ RJ11s */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT4RJ	0x0007		/* RocketPort 4 Port w/ RJ11s */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT8DB	0x0008		/* RocketPort 8 Port w/ DB78 */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT16DB	0x0009		/* RocketPort 16 Port w/ DB78 */
#define	PCI_PRODUCT_COMTROL_ROCKETPORTP4	0x000a		/* RocketPort Plus 4 Port */
#define	PCI_PRODUCT_COMTROL_ROCKETPORTP8	0x000b		/* RocketPort Plus 8 Port */
#define	PCI_PRODUCT_COMTROL_ROCKETMODEM6	0x000c		/* RocketModem 6 Port */
#define	PCI_PRODUCT_COMTROL_ROCKETMODEM4	0x000d		/* RocketModem 4 Port */
#define	PCI_PRODUCT_COMTROL_ROCKETPORTP232	0x000e		/* RocketPort 2 Port RS232 */
#define	PCI_PRODUCT_COMTROL_ROCKETPORTP422	0x000f		/* RocketPort 2 Port RS422 */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT550811A	0x8010		/* RocketPort 550/8 RJ11 part A */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT550811B	0x8011		/* RocketPort 550/8 RJ11 part B */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT5508OA	0x8012		/* RocketPort 550/8 Octa part A */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT5508OB	0x8013		/* RocketPort 550/8 Octa part B */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT5504	0x8014		/* RocketPort 550/4 */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT550Q	0x8015		/* RocketPort 550/Quad */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT55016A	0x8016		/* RocketPort 550/16 part A */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT55016B	0x8017		/* RocketPort 550/16 part B */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT5508A	0x8018		/* RocketPort 550/8 part A */
#define	PCI_PRODUCT_COMTROL_ROCKETPORT5508B	0x8019		/* RocketPort 550/8 part B */

/* Conexant Systems products */
#define	PCI_PRODUCT_CONEXANT_SOFTK56	0x2443		/* SoftK56 PCI Software Modem */
#define	PCI_PRODUCT_CONEXANT_56KFAXMODEM	0x1085		/* HW 56K Fax Modem */
#define	PCI_PRODUCT_CONEXANT_LANFINITY	0x1803		/* LANfinity MiniPCI 10/100 Ethernet */
#define	PCI_PRODUCT_CONEXANT_CX2388X	0x8800		/* CX23880/1/2/3 PCI Video/Audio Decoder */
#define	PCI_PRODUCT_CONEXANT_CX2388XAUDIO	0x8801		/* CX23880/1/2/3 PCI Audio Port */
#define	PCI_PRODUCT_CONEXANT_CX2388XMPEG	0x8802		/* CX23880/1/2/3 PCI MPEG Port */
#define	PCI_PRODUCT_CONEXANT_CX2388XIR	0x8804		/* CX23880/1/2/3 PCI IR Port */
#define	PCI_PRODUCT_CONEXANT_CX23885	0x8852		/* CX23885 */

/* Contaq Microsystems products */
#define	PCI_PRODUCT_CONTAQ_82C599	0x0600		/* 82C599 PCI-VLB Bridge */
#define	PCI_PRODUCT_CONTAQ_82C693	0xc693		/* 82C693 PCI-ISA Bridge */

/* Corega products */
#define	PCI_PRODUCT_COREGA_CB_TXD	0xa117		/* FEther CB-TXD 10/100 Ethernet */
#define	PCI_PRODUCT_COREGA_2CB_TXD	0xa11e		/* FEther II CB-TXD 10/100 Ethernet */
#define	PCI_PRODUCT_COREGA_LAPCIGT	0xc107		/* CG-LAPCIGT */

/* Corollary Products */
#define	PCI_PRODUCT_COROLLARY_CBUSII_PCIB	0x0014		/* \"C-Bus II\"-PCI Bridge */

/* Creative Labs products */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE	0x0002		/* SBLive! EMU 10000 */
#define	PCI_PRODUCT_CREATIVELABS_AWE64D	0x0003		/* SoundBlaster AWE64D */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGY	0x0004		/* SB Audigy EMU 10000 */
#define	PCI_PRODUCT_CREATIVELABS_XFI	0x0005		/* SoundBlaster X-Fi */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE2	0x0006		/* SBLive! EMU 10000 */
#define	PCI_PRODUCT_CREATIVELABS_SBAUDIGYLS	0x0007		/* SB Audigy LS */
#define	PCI_PRODUCT_CREATIVELABS_SBAUDIGY4	0x0008		/* SB Audigy 4 */
#define	PCI_PRODUCT_CREATIVELABS_FIWIRE	0x4001		/* Firewire */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY	0x7002		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGIN	0x7003		/* SoundBlaster Audigy Digital */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY2	0x7004		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY3	0x7005		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_EV1938	0x8938		/* Ectiva 1938 */

/* Cyclades products */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_1	0x0100		/* Cyclom-Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_2	0x0101		/* Cyclom-Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM4Y_1	0x0102		/* Cyclom-4Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM4Y_2	0x0103		/* Cyclom-4Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM8Y_1	0x0104		/* Cyclom-8Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM8Y_2	0x0105		/* Cyclom-8Y above 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMZ_1	0x0200		/* Cyclom-Z below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMZ_2	0x0201		/* Cyclom-Z above 1M */

/* Cyclone Microsystems products */
#define	PCI_PRODUCT_CYCLONE_PCI_700	0x0700		/* IQ80310 (PCI-700) */

/* Cyrix (now National) products */
#define	PCI_PRODUCT_CYRIX_MEDIAGX_PCHB	0x0001		/* MediaGX Built-in PCI Host Controller */
#define	PCI_PRODUCT_CYRIX_CX5520_PCIB	0x0002		/* Cx5520 I/O Companion */
#define	PCI_PRODUCT_CYRIX_CX5530_PCIB	0x0100		/* Cx5530 I/O Companion Multi-Function South Bridge */
#define	PCI_PRODUCT_CYRIX_CX5530_SMI	0x0101		/* Cx5530 I/O Companion (SMI Status and ACPI Timer) */
#define	PCI_PRODUCT_CYRIX_CX5530_IDE	0x0102		/* Cx5530 I/O Companion (IDE Controller) */
#define	PCI_PRODUCT_CYRIX_CX5530_AUDIO	0x0103		/* Cx5530 I/O Companion (XpressAUDIO) */
#define	PCI_PRODUCT_CYRIX_CX5530_VIDEO	0x0104		/* Cx5530 I/O Companion (Video Controller) */

/* Datum Inc. Bancomm-Timing Division products */
#define	PCI_PRODUCT_DATUM_BC635PCI_U	0x4013		/* BC635PCI-U TC & FREQ. Processor */

/* Davicom Semiconductor products */
#define	PCI_PRODUCT_DAVICOM_DM9102	0x9102		/* DM9102 10/100 Ethernet */

/* Decision Computer Inc */
#define	PCI_PRODUCT_DCI_APCI4	0x0001		/* PCCOM 4-port */
#define	PCI_PRODUCT_DCI_APCI8	0x0002		/* PCCOM 8-port */
#define	PCI_PRODUCT_DCI_APCI2	0x0004		/* PCCOM 2-port */

/* DEC products */
#define	PCI_PRODUCT_DEC_21050	0x0001		/* DC21050 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21040	0x0002		/* DC21040 (\"Tulip\") Ethernet */
#define	PCI_PRODUCT_DEC_21030	0x0004		/* DC21030 (\"TGA\") */
#define	PCI_PRODUCT_DEC_NVRAM	0x0007		/* Zephyr NV-RAM */
#define	PCI_PRODUCT_DEC_KZPSA	0x0008		/* KZPSA */
#define	PCI_PRODUCT_DEC_21140	0x0009		/* DC21140 (\"FasterNet\") 10/100 Ethernet */
#define	PCI_PRODUCT_DEC_PBXGB	0x000d		/* TGA2 */
#define	PCI_PRODUCT_DEC_DEFPA	0x000f		/* DEFPA */
/* product DEC ???	0x0010	??? VME Interface */
#define	PCI_PRODUCT_DEC_21041	0x0014		/* DC21041 (\"Tulip Plus\") Ethernet */
#define	PCI_PRODUCT_DEC_DGLPB	0x0016		/* DGLPB (\"OPPO\") */
#define	PCI_PRODUCT_DEC_21142	0x0019		/* DC21142/21143 10/100 Ethernet */
#define	PCI_PRODUCT_DEC_21052	0x0021		/* DC21052 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21150	0x0022		/* DC21150 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21152	0x0024		/* DC21152 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21153	0x0025		/* DC21153 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21154	0x0026		/* DC21154 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21554	0x0046		/* DC21554 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_SWXCR	0x1065		/* SWXCR RAID */

/* Dell Computer products */
#define	PCI_PRODUCT_DELL_PERC_2SI	0x0001		/* PERC 2/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI	0x0002		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3SI	0x0003		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3SI_2	0x0004		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI_2	0x0008		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3	0x000a		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_4DI	0x000e		/* PERC 4/Di */
#define	PCI_PRODUCT_DELL_PERC_4DI_2	0x000f		/* PERC 4/Di */
#define	PCI_PRODUCT_DELL_DRAC_4	0x0011		/* DRAC 4 */
#define	PCI_PRODUCT_DELL_DRAC_4_VUART	0x0012		/* DRAC 4 Virtual UART */
#define	PCI_PRODUCT_DELL_PERC_4ESI	0x0013		/* PERC 4e/Si */
#define	PCI_PRODUCT_DELL_DRAC_4_SMIC	0x0014		/* DRAC 4 SMIC */
#define	PCI_PRODUCT_DELL_PERC_5	0x0015		/* PERC 5 */
#define	PCI_PRODUCT_DELL_PERC_6	0x0060		/* PERC 6 */
#define	PCI_PRODUCT_DELL_PERC_3DI_2_SUB	0x00cf		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3SI_2_SUB	0x00d0		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI_SUB2	0x00d1		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_SUB3	0x00d9		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB	0x0106		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB2	0x011b		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB3	0x0121		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_5_1	0x1f01		/* PERC 5/e */
#define	PCI_PRODUCT_DELL_PERC_5_2	0x1f02		/* PERC 5/i */
#define	PCI_PRODUCT_DELL_CERC_1_5	0x0291		/* CERC SATA RAID 1.5/6ch */

/* Delta products */
#define	PCI_PRODUCT_DELTA_8139	0x1360		/* 8139 Ethernet */
#define	PCI_PRODUCT_DELTA_RHINEII	0x1320		/* Rhine II 10/100 Ethernet */

/* Diamond products */
#define	PCI_PRODUCT_DIAMOND_VIPER	0x9001		/* Viper/PCI */

/* Digi International Products */
#define	PCI_PRODUCT_DIGI_ACCELEPORT8R920	0x0027		/* AccelePort 8r 920 8-port serial */
#define	PCI_PRODUCT_DIGI_NEO4	0x00b0		/* Neo 4-port */
#define	PCI_PRODUCT_DIGI_NEO8	0x00b1		/* Neo 8-port */
#define	PCI_PRODUCT_DIGI_NEO8_PCIE	0x00f0		/* Neo 8-port (PCIe) */

/* D-Link Systems products */
#define	PCI_PRODUCT_DLINK_DL1002	0x1002		/* DL-1002 10/100 Ethernet */
#define	PCI_PRODUCT_DLINK_DFE530TXPLUS	0x1300		/* DFE-530TXPLUS 10/100 Ethernet */
#define	PCI_PRODUCT_DLINK_DFE690TXD	0x1340		/* DFE-690TXD 10/100 Ethernet */
#define	PCI_PRODUCT_DLINK_DWL610	0x3300		/* DWL-610 802.11b WLAN */
#define	PCI_PRODUCT_DLINK_DL4000	0x4000		/* DL-4000 Gigabit Ethernet */
#define	PCI_PRODUCT_DLINK_DGE550SX	0x4001		/* DGE-550SX */
#define	PCI_PRODUCT_DLINK_DFE520TX	0x4200		/* DFE-520TX 10/100 Ethernet */
#define	PCI_PRODUCT_DLINK_DGE528T	0x4300		/* DGE-528T Gigabit Ethernet */
#define	PCI_PRODUCT_DLINK_DGE560T	0x4b00		/* DGE-560T Gigabit Ethernet */
#define	PCI_PRODUCT_DLINK_DGE560T_2	0x4b01		/* DGE-560T_2 Gigabit Ethernet */
#define	PCI_PRODUCT_DLINK_DGE560SX	0x4b02		/* DGE-560SX */
#define	PCI_PRODUCT_DLINK_DGE530T	0x4c00		/* DGE-530T Gigabit Ethernet */

/* Distributed Processing Technology products */
#define	PCI_PRODUCT_DPT_SC_RAID	0xa400		/* SmartCache/SmartRAID (EATA) */
#define	PCI_PRODUCT_DPT_I960_PPB	0xa500		/* PCI-PCI Bridge */
#define	PCI_PRODUCT_DPT_RAID_I2O	0xa501		/* SmartRAID (I2O) */
#define	PCI_PRODUCT_DPT_RAID_2005S	0xa511		/* Zero Channel SmartRAID (I2O) */
#define	PCI_PRODUCT_DPT_MEMCTLR	0x1012		/* Memory Controller */

/* Dolphin products */
#define	PCI_PRODUCT_DOLPHIN_PCISCI32	0x0658		/* PCI-SCI Bridge (32-bit, 33 MHz) */
#define	PCI_PRODUCT_DOLPHIN_PCISCI64	0xd665		/* PCI-SCI Bridge (64-bit, 33 MHz) */
#define	PCI_PRODUCT_DOLPHIN_PCISCI66	0xd667		/* PCI-SCI Bridge (64-bit, 66 MHz) */

/* Domex products */
#define	PCI_PRODUCT_DOMEX_PCISCSI	0x0001		/* DMX-3191D */

/* Dynalink products */
#define	PCI_PRODUCT_DYNALINK_IS64PH	0x1702		/* IS64PH ISDN Adapter */

/* ELSA products */
#define	PCI_PRODUCT_ELSA_QS1PCI	0x1000		/* QuickStep 1000 ISDN Card */
#define	PCI_PRODUCT_ELSA_GLORIAXL	0x8901		/* Gloria XL 1624 */

/* Emulex products */
#define	PCI_PRODUCT_EMULEX_LP6000	0x1ae5		/* LP6000 FibreChannel Adapter */
#define	PCI_PRODUCT_EMULEX_LP952	0xf095		/* LP952 FibreChannel Adapter */
#define	PCI_PRODUCT_EMULEX_LP982	0xf098		/* LP982 FibreChannel Adapter */
#define	PCI_PRODUCT_EMULEX_LP101	0xf0a1		/* LP101 FibreChannel Adapter */
#define	PCI_PRODUCT_EMULEX_LP7000	0xf700		/* LP7000 FibreChannel Adapter */
#define	PCI_PRODUCT_EMULEX_LP8000	0xf800		/* LP8000 FibreChannel Adapter */
#define	PCI_PRODUCT_EMULEX_LP9000	0xf900		/* LP9000 FibreChannel Adapter */
#define	PCI_PRODUCT_EMULEX_LP9802	0xf980		/* LP9802 FibreChannel Adapter */
#define	PCI_PRODUCT_EMULEX_LP10000	0xfa00		/* LP10000 FibreChannel Adapter */

/* ENE Technology products */
#define	PCI_PRODUCT_ENE_MCR510	0x0510		/* MCR510 PCI Memory Card Reader Controller */
#define	PCI_PRODUCT_ENE_CB712	0x0550		/* CB712/714/810 PCI SD Card Reader Controller */
#define	PCI_PRODUCT_ENE_CB1211	0x1211		/* CB1211 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB1225	0x1225		/* CB1225 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB1410	0x1410		/* CB1410 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB710	0x1411		/* CB710 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB1420	0x1420		/* CB1420 CardBus Controller */
#define	PCI_PRODUCT_ENE_CB720	0x1421		/* CB720 CardBus Controller */

/* Ensoniq products */
#define	PCI_PRODUCT_ENSONIQ_AUDIOPCI	0x5000		/* AudioPCI */
#define	PCI_PRODUCT_ENSONIQ_AUDIOPCI97	0x1371		/* AudioPCI 97 */
#define	PCI_PRODUCT_ENSONIQ_CT5880	0x5880		/* CT5880 */

/* Equinox Systems product */
#define	PCI_PRODUCT_EQUINOX_SST64P	0x0808		/* SST-64P Adapter */
#define	PCI_PRODUCT_EQUINOX_SST128P	0x1010		/* SST-128P Adapter */
#define	PCI_PRODUCT_EQUINOX_SST16P_1	0x80c0		/* SST-16P Adapter */
#define	PCI_PRODUCT_EQUINOX_SST16P_2	0x80c4		/* SST-16P Adapter */
#define	PCI_PRODUCT_EQUINOX_SST16P_3	0x80c8		/* SST-16P Adapter */
#define	PCI_PRODUCT_EQUINOX_SST4P	0x8888		/* SST-4P Adapter */
#define	PCI_PRODUCT_EQUINOX_SST8P	0x9090		/* SST-8P Adapter */

/* Essential Communications products */
#define	PCI_PRODUCT_ESSENTIAL_RR_HIPPI	0x0001		/* RoadRunner HIPPI Interface */
#define	PCI_PRODUCT_ESSENTIAL_RR_GIGE	0x0005		/* RoadRunner Gig-E Interface */

/* ESS Technology products */
#define	PCI_PRODUCT_ESSTECH_MAESTRO1	0x0100		/* Maestro 1 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO2	0x1968		/* Maestro 2 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_SOLO1	0x1969		/* Solo-1 PCI AudioDrive */
#define	PCI_PRODUCT_ESSTECH_MAESTRO2E	0x1978		/* Maestro 2E PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_ALLEGRO1	0x1988		/* Allegro-1 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3	0x1998		/* Maestro 3 PCI Audio Accelerator */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3MODEM	0x1999		/* Maestro 3 Modem */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3_2	0x199a		/* Maestro 3 PCI Audio Accelerator */

/* ESS Technology products */
#define	PCI_PRODUCT_ESSTECH2_MAESTRO1	0x0100		/* Maestro 1 PCI Audio Accelerator */

/* Eumitcom products */
#define	PCI_PRODUCT_EUMITCOM_WL11000P	0x1100		/* WL11000P PCI WaveLAN/IEEE 802.11 */

/* O2 Micro */
#define	PCI_PRODUCT_O2MICRO_00F7	0x00f7		/* Integrated OHCI IEEE 1394 Host Controller */
#define	PCI_PRODUCT_O2MICRO_OZ6729	0x6729		/* OZ6729 PCI-PCMCIA Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6730	0x673A		/* OZ6730 PCI-PCMCIA Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6832	0x6832		/* OZ6832/OZ6833 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6836	0x6836		/* OZ6836/OZ6860 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6872	0x6872		/* OZ6812/OZ6872 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6922	0x6925		/* OZ6922 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6933	0x6933		/* OZ6933 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_OZ6972	0x6972		/* OZ6912/OZ6972 PCI-CardBus Bridge */
#define	PCI_PRODUCT_O2MICRO_7120	0x7120		/* OZ7120 Integrated MMC/SD Controller */
#define	PCI_PRODUCT_O2MICRO_7130	0x7130		/* OZ7130 Integrated MS/xD/SM Controller */
#define	PCI_PRODUCT_O2MICRO_7223	0x7223		/* OZ711E0 PCI-CardBus Bridge */

/* Evans & Sutherland products */
#define	PCI_PRODUCT_ES_FREEDOM	0x0001		/* Freedom PCI-GBus Interface */

/* EXAR products */
#define	PCI_PRODUCT_EXAR_XR17D152	0x0152		/* dual-channel Universal PCI UART */
#define	PCI_PRODUCT_EXAR_XR17D154	0x0154		/* quad-channel Universal PCI UART */
#define	PCI_PRODUCT_EXAR_XR17D158	0x0158		/* octal-channel Universal PCI UART */

/* FORE products */
#define	PCI_PRODUCT_FORE_PCA200	0x0210		/* ATM PCA-200 */
#define	PCI_PRODUCT_FORE_PCA200E	0x0300		/* ATM PCA-200e */

/* Forte Media products */
#define	PCI_PRODUCT_FORTEMEDIA_FM801	0x0801		/* 801 Sound */
#define	PCI_PRODUCT_FORTEMEDIA_PCIJOY	0x0802		/* PCI Gameport Joystick */

/* Fresco Logic products */
#define	PCI_PRODUCT_FRESCO_FL1000	0x1000		/* FL1000 USB3 Host Controller */
#define	PCI_PRODUCT_FRESCO_FL1009	0x1009		/* FL1009 USB3 Host Controller */

/* Future Domain products */
#define	PCI_PRODUCT_FUTUREDOMAIN_TMC_18C30	0x0000		/* TMC-18C30 (36C70) */

/* Fujitsu products */
#define	PCI_PRODUCT_FUJITSU4_PW008GE5	0x11a1		/* PW008GE5 */
#define	PCI_PRODUCT_FUJITSU4_PW008GE4	0x11a2		/* PW008GE4 */
#define	PCI_PRODUCT_FUJITSU4_PP250_450_LAN	0x11cc		/* PRIMEPOWER250/450 LAN */

/* FZ Juelich / ZEL products */
#define	PCI_PRODUCT_FZJZEL_GIGALINK	0x0001		/* Gigabit Link / STR1100 */
#define	PCI_PRODUCT_FZJZEL_PLXHOTLINK	0x0002		/* HOTlink Interface */
#define	PCI_PRODUCT_FZJZEL_COUNTTIME	0x0003		/* Counter / Timer */
#define	PCI_PRODUCT_FZJZEL_PLXCAMAC	0x0004		/* CAMAC Controller */
#define	PCI_PRODUCT_FZJZEL_PROFIBUS	0x0005		/* PROFIBUS Interface */
#define	PCI_PRODUCT_FZJZEL_AMCCHOTLINK	0x0006		/* old HOTlink Interface */

/* Efficient Networks products */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PF	0x0000		/* 155P-MF1 ATM (FPGA) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PA	0x0002		/* 155P-MF1 ATM (ASIC) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI25P	0x0003		/* SpeedStream ENI-25p */
#define	PCI_PRODUCT_EFFICIENTNETS_SS3000	0x0005		/* SpeedStream 3000 */

/* Freescale Semiconductor products */
#define	PCI_PRODUCT_FREESCALE_MPC8548E	0x0012		/* MPC8548E */
#define	PCI_PRODUCT_FREESCALE_MPC8548	0x0013		/* MPC8548 */
#define	PCI_PRODUCT_FREESCALE_MPC8543E	0x0014		/* MPC8543E */
#define	PCI_PRODUCT_FREESCALE_MPC8543	0x0015		/* MPC8543 */
#define	PCI_PRODUCT_FREESCALE_MPC8547E	0x0018		/* MPC8547E */
#define	PCI_PRODUCT_FREESCALE_MPC8545E	0x0019		/* MPC8545E */
#define	PCI_PRODUCT_FREESCALE_MPC8545	0x001a		/* MPC8545 */
#define	PCI_PRODUCT_FREESCALE_MPC8544E	0x0032		/* MPC8544E */
#define	PCI_PRODUCT_FREESCALE_MPC8544	0x0033		/* MPC8544 */
#define	PCI_PRODUCT_FREESCALE_MPC8572E	0x0040		/* MPC8572E */
#define	PCI_PRODUCT_FREESCALE_MPC8572	0x0041		/* MPC8572 */
#define	PCI_PRODUCT_FREESCALE_MPC8536E	0x0050		/* MPC8536E */
#define	PCI_PRODUCT_FREESCALE_MPC8536	0x0051		/* MPC8536 */
#define	PCI_PRODUCT_FREESCALE_P2020E	0x0070		/* P2020E */
#define	PCI_PRODUCT_FREESCALE_P2020	0x0071		/* P2020 */
#define	PCI_PRODUCT_FREESCALE_P2010E	0x0078		/* P2010E */
#define	PCI_PRODUCT_FREESCALE_P2010	0x0079		/* P2010 */
#define	PCI_PRODUCT_FREESCALE_P1020E	0x0100		/* P1021E */
#define	PCI_PRODUCT_FREESCALE_P1020	0x0101		/* P1020 */
#define	PCI_PRODUCT_FREESCALE_P1021E	0x0102		/* P1021E */
#define	PCI_PRODUCT_FREESCALE_P1021	0x0103		/* P1021 */
#define	PCI_PRODUCT_FREESCALE_P1024E	0x0104		/* P1024E */
#define	PCI_PRODUCT_FREESCALE_P1024	0x0105		/* P1024 */
#define	PCI_PRODUCT_FREESCALE_P1025E	0x0106		/* P1025E */
#define	PCI_PRODUCT_FREESCALE_P1025	0x0107		/* P1025 */
#define	PCI_PRODUCT_FREESCALE_P1011E	0x0108		/* P1011E */
#define	PCI_PRODUCT_FREESCALE_P1011	0x0109		/* P1011 */
#define	PCI_PRODUCT_FREESCALE_P1022E	0x0110		/* P1022E */
#define	PCI_PRODUCT_FREESCALE_P1022	0x0111		/* P1022 */
#define	PCI_PRODUCT_FREESCALE_P1013E	0x0118		/* P1013E */
#define	PCI_PRODUCT_FREESCALE_P1013	0x0119		/* P1013 */
#define	PCI_PRODUCT_FREESCALE_P4080E	0x0400		/* P4080E */
#define	PCI_PRODUCT_FREESCALE_P4080	0x0401		/* P4080 */
#define	PCI_PRODUCT_FREESCALE_P4040E	0x0408		/* P4040E */
#define	PCI_PRODUCT_FREESCALE_P4040	0x0409		/* P4040 */
#define	PCI_PRODUCT_FREESCALE_P2040E	0x0410		/* P2040E */
#define	PCI_PRODUCT_FREESCALE_P2040	0x0411		/* P2040 */
#define	PCI_PRODUCT_FREESCALE_P3041E	0x041e		/* P3041E */
#define	PCI_PRODUCT_FREESCALE_P3041	0x041f		/* P3041 */
#define	PCI_PRODUCT_FREESCALE_P5020E	0x0420		/* P5020E */
#define	PCI_PRODUCT_FREESCALE_P5020	0x0421		/* P5020 */
#define	PCI_PRODUCT_FREESCALE_P5010E	0x0428		/* P5010E */
#define	PCI_PRODUCT_FREESCALE_P5010	0x0429		/* P5010 */

/* Marvell products */
#define	PCI_PRODUCT_MARVELL_GT64010A	0x0146		/* GT-64010A System Controller */
#define	PCI_PRODUCT_MARVELL_88F1181	0x1181		/* 88F1181 */
#define	PCI_PRODUCT_MARVELL_88F1281	0x1281		/* 88F1281 SoC Orion2 */
#define	PCI_PRODUCT_MARVELL_88W8300_1	0x1fa6		/* Libertas 88W8300 */
#define	PCI_PRODUCT_MARVELL_88W8310	0x1fa7		/* Libertas 88W8310 */
#define	PCI_PRODUCT_MARVELL_88W8335_1	0x1faa		/* Libertas 88W8335 */
#define	PCI_PRODUCT_MARVELL_88W8335_2	0x1fab		/* Libertas 88W8335 */
#define	PCI_PRODUCT_MARVELL_88SB2211	0x2211		/* 88SB2211 x1 PCIe-PCI Bridge */
#define	PCI_PRODUCT_MARVELL_88W8300_2	0x2a01		/* Libertas 88W8300 */
#define	PCI_PRODUCT_MARVELL_GT64115	0x4111		/* GT-64115 System Controller */
#define	PCI_PRODUCT_MARVELL_GT64011	0x4146		/* GT-64011 System Controller */
#define	PCI_PRODUCT_MARVELL_SKNET	0x4320		/* SK-NET Gigabit Ethernet */
#define	PCI_PRODUCT_MARVELL_YUKONII_8021CU	0x4340		/* Yukon-II 88E8021CU */
#define	PCI_PRODUCT_MARVELL_YUKONII_8022CU	0x4341		/* Yukon-II 88E8022CU */
#define	PCI_PRODUCT_MARVELL_YUKONII_8061CU	0x4342		/* Yukon-II 88E8061CU */
#define	PCI_PRODUCT_MARVELL_YUKONII_8062CU	0x4343		/* Yukon-II 88E8062CU */
#define	PCI_PRODUCT_MARVELL_YUKONII_8021X	0x4344		/* Yukon-II 88E8021X */
#define	PCI_PRODUCT_MARVELL_YUKONII_8022X	0x4345		/* Yukon-II 88E8022X */
#define	PCI_PRODUCT_MARVELL_YUKONII_8061X	0x4346		/* Yukon-II 88E8061X */
#define	PCI_PRODUCT_MARVELL_YUKONII_8062X	0x4347		/* Yukon-II 88E8062X */
#define	PCI_PRODUCT_MARVELL_YUKON_8035	0x4350		/* Yukon 88E8035 */
#define	PCI_PRODUCT_MARVELL_YUKON_8036	0x4351		/* Yukon 88E8036 */
#define	PCI_PRODUCT_MARVELL_YUKON_8038	0x4352		/* Yukon 88E8038 */
#define	PCI_PRODUCT_MARVELL_YUKON_8039	0x4353		/* Yukon 88E8039 */
#define	PCI_PRODUCT_MARVELL_YUKON_8040	0x4354		/* Yukon 88E8040 */
#define	PCI_PRODUCT_MARVELL_YUKON_C033	0x4356		/* Yukon 88EC033 */
#define	PCI_PRODUCT_MARVELL_YUKON_8052	0x4360		/* Yukon 88E8052 */
#define	PCI_PRODUCT_MARVELL_YUKON_8050	0x4361		/* Yukon 88E8050 */
#define	PCI_PRODUCT_MARVELL_YUKON_8053	0x4362		/* Yukon 88E8053 */
#define	PCI_PRODUCT_MARVELL_YUKON_8055	0x4363		/* Yukon 88E8055 */
#define	PCI_PRODUCT_MARVELL_YUKON_8056	0x4364		/* Yukon 88E8056 */
#define	PCI_PRODUCT_MARVELL_YUKON_1	0x4365		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_C036	0x4366		/* Yukon 88EC036 */
#define	PCI_PRODUCT_MARVELL_YUKON_C032	0x4367		/* Yukon 88EC032 */
#define	PCI_PRODUCT_MARVELL_YUKON_C034	0x4368		/* Yukon 88EC034 */
#define	PCI_PRODUCT_MARVELL_YUKON_C042	0x4369		/* Yukon 88EC042 */
#define	PCI_PRODUCT_MARVELL_YUKON_C055	0x436a		/* Yukon 88EC055 */
#define	PCI_PRODUCT_MARVELL_GT64120	0x4620		/* GT-64120 System Controller */
#define	PCI_PRODUCT_MARVELL_BELKIN	0x5005		/* Belkin Gigabit Ethernet */
#define	PCI_PRODUCT_MARVELL_88SX5040	0x5040		/* 88SX5040 SATA */
#define	PCI_PRODUCT_MARVELL_88SX5041	0x5041		/* 88SX5041 SATA */
#define	PCI_PRODUCT_MARVELL_88SX5080	0x5080		/* 88SX5080 SATA */
#define	PCI_PRODUCT_MARVELL_88SX5081	0x5081		/* 88SX5081 SATA */
#define	PCI_PRODUCT_MARVELL_88F5082	0x5082		/* 88F5082 SoC Orion1 */
#define	PCI_PRODUCT_MARVELL_88F5180N	0x5180		/* 88F5180N SoC Orion1 */
#define	PCI_PRODUCT_MARVELL_88F5181	0x5181		/* 88F5181 SoC Orion1 */
#define	PCI_PRODUCT_MARVELL_88F5182	0x5182		/* 88F5182 SoC Orion1 */
#define	PCI_PRODUCT_MARVELL_88F5281	0x5281		/* 88F5281 SoC Orion2 */
#define	PCI_PRODUCT_MARVELL_88SX6040	0x6040		/* 88SX6040 SATA II */
#define	PCI_PRODUCT_MARVELL_88SX6041	0x6041		/* 88SX6041 SATA II */
#define	PCI_PRODUCT_MARVELL_88SX6042	0x6042		/* 88SX6042 SATA IIe */
#define	PCI_PRODUCT_MARVELL_88SX6080	0x6080		/* 88SX6080 SATA II */
#define	PCI_PRODUCT_MARVELL_88SX6081	0x6081		/* 88SX6081 SATA II */
#define	PCI_PRODUCT_MARVELL_88F6082	0x6082		/* 88F6082 SoC Orion1 */
#define	PCI_PRODUCT_MARVELL_88SE6101	0x6101		/* 88SE6101 Single Port PATA133 Controller */
#define	PCI_PRODUCT_MARVELL_88SE6121	0x6121		/* 88SE6121 SATA II Controller */
#define	PCI_PRODUCT_MARVELL_88SE614X	0x6141		/* 88SE614X SATA II PCI-E Controller */
#define	PCI_PRODUCT_MARVELL_88SE6145	0x6145		/* 88SE6145 SATA II PCI-E Controller */
#define	PCI_PRODUCT_MARVELL_88F6180	0x6180		/* 88F6180 SoC Kirkwood */
#define	PCI_PRODUCT_MARVELL_88F6183	0x6183		/* 88F6183 SoC Orion1 */
#define	PCI_PRODUCT_MARVELL_88F6192	0x6192		/* 88F6192 SoC Kirkwood */
#define	PCI_PRODUCT_MARVELL_88F6281	0x6281		/* 88F6281 SoC Kirkwood */
#define	PCI_PRODUCT_MARVELL_88F6282	0x6282		/* 88F6282 SoC Kirkwood */
#define	PCI_PRODUCT_MARVELL_GT64130	0x6320		/* GT-64130 System Controller */
#define	PCI_PRODUCT_MARVELL_GT64260	0x6430		/* GT-64260 System Controller */
#define	PCI_PRODUCT_MARVELL_MV64360	0x6460		/* MV6436x System Controller */
#define	PCI_PRODUCT_MARVELL_MV64460	0x6480		/* MV6446x System Controller */
#define	PCI_PRODUCT_MARVELL_MV6707	0x6707		/* MV6707 SoC Armada 370 */
#define	PCI_PRODUCT_MARVELL_MV6710	0x6710		/* MV6710 SoC Armada 370 */
#define	PCI_PRODUCT_MARVELL_MV6W11	0x6711		/* MV6W11 SoC Armada 370 */
#define	PCI_PRODUCT_MARVELL_88SX7042	0x7042		/* 88SX7042 SATA IIe */
#define	PCI_PRODUCT_MARVELL_MV78100	0x7810		/* MV78100 SoC Discovery Innovation */
#define	PCI_PRODUCT_MARVELL_MV78130	0x7813		/* MV78130 SoC Armada XP */
#define	PCI_PRODUCT_MARVELL_MV78160	0x7816		/* MV78160 SoC Armada XP */
#define	PCI_PRODUCT_MARVELL_MV78200	0x7820		/* MV78200 SoC Discovery Innovation */
#define	PCI_PRODUCT_MARVELL_MV78230	0x7823		/* MV78230 SoC Armada XP */
#define	PCI_PRODUCT_MARVELL_MV78260	0x7826		/* MV78260 SoC Armada XP */
#define	PCI_PRODUCT_MARVELL_MV78460	0x7846		/* MV78460 SoC Armada XP */
#define	PCI_PRODUCT_MARVELL_88F6810	0x6810		/* 88F6810 SoC Armada 38x */
#define	PCI_PRODUCT_MARVELL_88F6820	0x6820		/* 88F6820 SoC Armada 38x */
#define	PCI_PRODUCT_MARVELL_88F6828	0x6828		/* 88F6828 SoC Armada 38x */
#define	PCI_PRODUCT_MARVELL_88W8660	0x8660		/* 88W8660 SoC Orion1 */

#define	PCI_PRODUCT_MARVELL2_88SE9120	0x9120		/* 88SE9120 SATA */
#define	PCI_PRODUCT_MARVELL2_88SE912X	0x9123		/* 88SE912[38] SATA II or III PCI-E AHCI Controller */
#define	PCI_PRODUCT_MARVELL2_88SE9125	0x9125		/* 88SE9125 SATA III PCI-E AHCI Controller */
#define	PCI_PRODUCT_MARVELL2_88SE9172	0x9172		/* 88SE9172 SATA */
#define	PCI_PRODUCT_MARVELL2_88SE9182	0x9182		/* 88SE9182 SATA */
#define	PCI_PRODUCT_MARVELL2_88SE9183	0x9183		/* 88SE9183 SATA */
#define	PCI_PRODUCT_MARVELL2_88SE91XX	0x91a3		/* 88SE91XX SATA */
#define	PCI_PRODUCT_MARVELL2_88SE9215	0x9215		/* 88SE9215 SATA */
#define	PCI_PRODUCT_MARVELL2_88SE9220	0x9220		/* 88SE9220 SATA */
#define	PCI_PRODUCT_MARVELL2_88SE9230	0x9230		/* 88SE9230 SATA */
#define	PCI_PRODUCT_MARVELL2_88SE9235	0x9235		/* 88SE9235 SATA */

/* Global Sun Tech products */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P	0x1101		/* GL24110P PCI IEEE 802.11b */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P02	0x1102		/* GL24110P PCI IEEE 802.11b */

/* Guillemot products */
#define	PCI_PRODUCT_GUILLEMOT_MAXIRADIO	0x1001		/* MAXIRADIO */

/* Hawking products */
#define	PCI_PRODUCT_HAWKING_PN672TX	0xab08		/* PN672TX 10/100 Ethernet */

/* Heuricon products */
#define	PCI_PRODUCT_HEURICON_PMPPC	0x000e		/* PM/PPC */

/* Hewlett-Packard products */
#define	PCI_PRODUCT_HP_VISUALIZE_EG	0x1005		/* A4977A Visualize EG */
#define	PCI_PRODUCT_HP_VISUALIZE_FX6	0x1006		/* Visualize FX6 */
#define	PCI_PRODUCT_HP_VISUALIZE_FX4	0x1008		/* Visualize FX4 */
#define	PCI_PRODUCT_HP_VISUALIZE_FX2	0x100a		/* Visualize FX2 */
#define	PCI_PRODUCT_HP_TACHYON_TL	0x1028		/* Tachyon TL FC Controller */
#define	PCI_PRODUCT_HP_TACHYON_XL2	0x1029		/* Tachyon XL2 FC Controller */
#define	PCI_PRODUCT_HP_TACHYON_TS	0x102A		/* Tachyon TS FC Controller */
#define	PCI_PRODUCT_HP_J2585A	0x1030		/* J2585A */
#define	PCI_PRODUCT_HP_J2585B	0x1031		/* J2585B */
#define	PCI_PRODUCT_HP_DIVA	0x1048		/* Diva Serial Multiport */
#define	PCI_PRODUCT_HP_ELROY	0x1054		/* Elroy Ropes-PCI */
#define	PCI_PRODUCT_HP_VISUALIZE_FXE	0x108b		/* Visualize FXe */
#define	PCI_PRODUCT_HP_TOPTOOLS	0x10c1		/* TopTools Communications Port */
#define	PCI_PRODUCT_HP_NETRAID_4M	0x10c2		/* NetRaid-4M */
#define	PCI_PRODUCT_HP_SMARTIRQ	0x10ed		/* NetServer SmartIRQ */
#define	PCI_PRODUCT_HP_82557B	0x1200		/* 82557B 10/100 NIC */
#define	PCI_PRODUCT_HP_PLUTO	0x1229		/* Pluto MIO */
#define	PCI_PRODUCT_HP_ZX1_IOC	0x122a		/* zx1 IOC */
#define	PCI_PRODUCT_HP_MERCURY	0x122e		/* Mercury Ropes-PCI */
#define	PCI_PRODUCT_HP_QUICKSILVER	0x12b4		/* QuickSilver Ropes-PCI */

#define	PCI_PRODUCT_HP_HPSAV100	0x3210		/* Smart Array V100 */
#define	PCI_PRODUCT_HP_HPSAE200I_1	0x3211		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200	0x3212		/* Smart Array E200 */
#define	PCI_PRODUCT_HP_HPSAE200I_2	0x3213		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200I_3	0x3214		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200I_4	0x3215		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSA_1	0x3220		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_2	0x3222		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSAP800	0x3223		/* Smart Array P600 */
#define	PCI_PRODUCT_HP_HPSAP600	0x3225		/* Smart Array P600 */
#define	PCI_PRODUCT_HP_HPSA_3	0x3230		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_4	0x3231		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_5	0x3232		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_6	0x3233		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSAP400	0x3234		/* Smart Array P400 */
#define	PCI_PRODUCT_HP_HPSAP400I	0x3235		/* Smart Array P400i */
#define	PCI_PRODUCT_HP_HPSA_7	0x3236		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_8	0x3237		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_9	0x3238		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_10	0x3239		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_11	0x323a		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_12	0x323b		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_13	0x323c		/* Smart Array */
#define	PCI_PRODUCT_HP_USB	0x3300		/* iLO3 Virtual USB */
#define	PCI_PRODUCT_HP_IPMI	0x3302		/* IPMI */
#define	PCI_PRODUCT_HP_ILO3_SLAVE	0x3306		/* iLO3 Slave */
#define	PCI_PRODUCT_HP_ILO3_MGMT	0x3307		/* iLO3 Management */
#define	PCI_PRODUCT_HP_RS780_PPB_GFX	0x9602		/* (AMD) RS780 PCI-PCI Bridge (int gfx) */

/* Hifn products */
#define	PCI_PRODUCT_HIFN_7751	0x0005		/* 7751 */
#define	PCI_PRODUCT_HIFN_6500	0x0006		/* 6500 */
#define	PCI_PRODUCT_HIFN_7811	0x0007		/* 7811 */
#define	PCI_PRODUCT_HIFN_7951	0x0012		/* 7951 */
#define	PCI_PRODUCT_HIFN_7955	0x0020		/* 7954/7955 */
#define	PCI_PRODUCT_HIFN_7956	0x001d		/* 7956 */
#define	PCI_PRODUCT_HIFN_78XX	0x0014		/* 7814/7851/7854 */
#define	PCI_PRODUCT_HIFN_8065	0x0016		/* 8065 */
#define	PCI_PRODUCT_HIFN_8165	0x0017		/* 8165 */
#define	PCI_PRODUCT_HIFN_8154	0x0018		/* 8154 */

/* HiNT products */
#define	PCI_PRODUCT_HINT_HB1	0x0021		/* HB1 PCI-PCI Bridge */
#define	PCI_PRODUCT_HINT_HB4	0x0022		/* HB4 PCI-PCI Bridge */

/* Hitachi products */
#define	PCI_PRODUCT_HITACHI_SWC	0x0101		/* MSVCC01/02/03/04 Video Capture Cards */
#define	PCI_PRODUCT_HITACHI_SH7751	0x3505		/* SH7751 PCI Controller */
#define	PCI_PRODUCT_HITACHI_SH7751R	0x350e		/* SH7751R PCI Controller */

/* IBM products */
#define	PCI_PRODUCT_IBM_MCABRIDGE	0x0002		/* MCA Bridge */
#define	PCI_PRODUCT_IBM_ALTALITE	0x0005		/* CPU Bridge - Alta Lite */
#define	PCI_PRODUCT_IBM_ALTAMP	0x0007		/* CPU Bridge - Alta MP */
#define	PCI_PRODUCT_IBM_ISABRIDGE	0x000a		/* Fire Coral ISA Bridge w/ PnP */
#define	PCI_PRODUCT_IBM_POWERWAVE	0x0013		/* PowerWave Graphics Adapter */
#define	PCI_PRODUCT_IBM_IDAHO	0x0015		/* Idaho PCI Bridge */
#define	PCI_PRODUCT_IBM_CPUBRIDGE	0x0017		/* CPU Bridge */
#define	PCI_PRODUCT_IBM_LANSTREAMER	0x0018		/* Auto LANStreamer */
#define	PCI_PRODUCT_IBM_GXT150P	0x001b		/* GXT-150P 2D Accelerator */
#define	PCI_PRODUCT_IBM_CARRERA	0x001c		/* Carrera PCI Bridge */
#define	PCI_PRODUCT_IBM_82G2675	0x001d		/* 82G2675 SCSI-2 Fast Controller */
#define	PCI_PRODUCT_IBM_MCABRIDGE2	0x0020		/* MCA Bridge */
#define	PCI_PRODUCT_IBM_82351	0x0022		/* 82351 PCI-PCI Bridge */
#define	PCI_PRODUCT_IBM_MONNAV	0x002c		/* Montana/Nevada PCI Bridge and Memory Controller */
#define	PCI_PRODUCT_IBM_PYTHON	0x002d		/* Python PCI-PCI Bridge */
#define	PCI_PRODUCT_IBM_SERVERAID	0x002e		/* ServeRAID (copperhead) */
#define	PCI_PRODUCT_IBM_GXT250P	0x003c		/* GXT-250P Graphics Adapter */
#define	PCI_PRODUCT_IBM_OLYMPIC	0x003e		/* 16/4 Token Ring */
#define	PCI_PRODUCT_IBM_MIAMI	0x0036		/* Miami/PCI */
#define	PCI_PRODUCT_IBM_82660	0x0037		/* 82660 PowerPC to PCI Bridge and Memory Controller */
#define	PCI_PRODUCT_IBM_MPIC	0x0046		/* MPIC */
#define	PCI_PRODUCT_IBM_TURBOWAYS25	0x0053		/* Turboways 25 ATM */
#define	PCI_PRODUCT_IBM_GXT500P	0x0054		/* GXT-500P/GXT550P Graphics Adapter */
#define	PCI_PRODUCT_IBM_I82557B	0x005c		/* i82557B 10/100 Ethernet */
#define	PCI_PRODUCT_IBM_GXT800P	0x005e		/* GXT-800P Graphics Adapter */
#define	PCI_PRODUCT_IBM_EADSPCI	0x008b		/* EADS PCI-PCI Bridge */
#define	PCI_PRODUCT_IBM_GXT3000P	0x008e		/* GXT-3000P Graphics Adapter */
#define	PCI_PRODUCT_IBM_GXT3000P2	0x0090		/* GXT-3000P Graphics Adapter(2) */
#define	PCI_PRODUCT_IBM_GXT2000P	0x00b8		/* GXT-2000P Graphics Adapter */
#define	PCI_PRODUCT_IBM_OLYMPIC2	0x00ce		/* Olympic 2 Token Ring */
#define	PCI_PRODUCT_IBM_CPC71064	0x00fc		/* CPC710 Dual Bridge and Memory Controller (PCI64) */
#define	PCI_PRODUCT_IBM_CPC71032	0x0105		/* CPC710 Dual Bridge and Memory Controller (PCI32) */
#define	PCI_PRODUCT_IBM_TPAUDIO	0x0153		/* ThinkPad 600X/A20/T20/T22 Audio */
#define	PCI_PRODUCT_IBM_405GP	0x0156		/* PPC 405GP PCI Bridge */
#define	PCI_PRODUCT_IBM_GXT4000P	0x016e		/* GXT-4000P Graphics Adapter */
#define	PCI_PRODUCT_IBM_GXT6000P	0x0170		/* GXT-6000P Graphics Adapter */
#define	PCI_PRODUCT_IBM_GXT300P	0x017d		/* GXT-300P Graphics Adapter */
#define	PCI_PRODUCT_IBM_133PCIX	0x01a7		/* 133 PCI-X Bridge */
#define	PCI_PRODUCT_IBM_SERVERAID4	0x01bd		/* ServeRAID 4/5 (morpheus) */
#define	PCI_PRODUCT_IBM_440GP	0x01ef		/* PPC 440GP PCI Bridge */
#define	PCI_PRODUCT_IBM_IBMETHER	0x01ff		/* 10/100 Ethernet */
#define	PCI_PRODUCT_IBM_GXT6500P	0x021b		/* GXT-6500P Graphics Adapter */
#define	PCI_PRODUCT_IBM_GXT4500P	0x021c		/* GXT-4500P Graphics Adapter */
#define	PCI_PRODUCT_IBM_GXT135P	0x0233		/* GXT-135P Graphics Adapter */
#define	PCI_PRODUCT_IBM_4810_BSP	0x0295		/* 4810 BSP */
#define	PCI_PRODUCT_IBM_4810_SCC	0x0297		/* 4810 SCC */
#define	PCI_PRODUCT_IBM_SERVERAID8K	0x9580		/* ServeRAID 8k */
#define	PCI_PRODUCT_IBM_MPIC2	0xffff		/* MPIC-II */

/* IC Ensemble / VIA Technologies products */
#define	PCI_PRODUCT_ICENSEMBLE_ICE1712	0x1712		/* Envy24 Multichannel Audio Controller */
#define	PCI_PRODUCT_ICENSEMBLE_VT1720	0x1724		/* Envy24PT/HT Multi-Channel Audio Controller */

/* Conexant (iCompression, GlobeSpan) products */
#define	PCI_PRODUCT_ICOMPRESSION_ITVC15	0x0803		/* iTVC15 MPEG2 Codec */

/* IDT products */
#define	PCI_PRODUCT_IDT_77201	0x0001		/* 77201/77211 ATM (\"NICStAR\") */
#define	PCI_PRODUCT_IDT_RC32334	0x0204		/* RC32334 System Controller */
#define	PCI_PRODUCT_IDT_RC32332	0x0205		/* RC32332 System Controller */

/* Industrial Computer Source */
#define	PCI_PRODUCT_INDCOMPSRC_WDT50x	0x22c0		/* PCI-WDT50x Watchdog Timer */

/* Initio products */
#define	PCI_PRODUCT_INITIO_1622	0x1622		/* INIC-1622 SATA */
#define	PCI_PRODUCT_INITIO_I920	0x0002		/* INIC-920 SCSI */
#define	PCI_PRODUCT_INITIO_I850	0x0850		/* INIC-850 SCSI */
#define	PCI_PRODUCT_INITIO_I1060	0x1060		/* INIC-1060 SCSI */
#define	PCI_PRODUCT_INITIO_I940	0x9400		/* INIC-940 SCSI */
#define	PCI_PRODUCT_INITIO_I935	0x9401		/* INIC-935 SCSI */
#define	PCI_PRODUCT_INITIO_I950	0x9500		/* INIC-950 SCSI */

/* Integraphics Systems products */
#define	PCI_PRODUCT_INTEGRAPHICS_IGA1680	0x1680		/* IGA 1680 */
#define	PCI_PRODUCT_INTEGRAPHICS_IGA1682	0x1682		/* IGA 1682 */
#define	PCI_PRODUCT_INTEGRAPHICS_CYBERPRO2000	0x2000		/* CyberPro 2000 */
#define	PCI_PRODUCT_INTEGRAPHICS_CYBERPRO2010	0x2010		/* CyberPro 2010 */

/* Integrated Micro Solutions products */
#define	PCI_PRODUCT_IMS_8849	0x8849		/* 8849 */
#define	PCI_PRODUCT_IMS_TT128M	0x9128		/* TwinTurbo 128M */

/* Intel products */
#define	PCI_PRODUCT_INTEL_IRONLAKE_D_HB	0x0040		/* Iron Lake Host Bridge */
#define	PCI_PRODUCT_INTEL_IRONLAKE_D_IGD	0x0042		/* Iron Lake Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_IRONLAKE_M_HB	0x0044		/* Iron Lake Host Bridge */
#define	PCI_PRODUCT_INTEL_IRONLAKE_M_IGD	0x0046		/* Iron Lake Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_IRONLAKE_MA_HB	0x0062		/* Iron Lake Host Bridge */
#define	PCI_PRODUCT_INTEL_IRONLAKE_MC2_HB	0x006a		/* Iron Lake Host Bridge */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6005_2X2_1	0x0082		/* Centrino Advanced-N 6205 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_1000_1	0x0083		/* WiFi Link 1000 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_1000_2	0x0084		/* WiFi Link 1000 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6005_2X2_2	0x0085		/* Centrino Advanced-N 6205 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6050_2X2_1	0x0087		/* Centrino Advanced-N 6250 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6050_2X2_2	0x0089		/* Centrino Advanced-N 6250 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_1030_1	0x008a		/* Centrino Wireless-N 1030 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_1030_2	0x008b		/* Centrino Wireless-N 1030 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6230_1	0x0090		/* Centrino Advanced-N 6230 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6230_2	0x0091		/* Centrino Advanced-N 6230 */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_HB	0x0100		/* Sandy Bridge Host Bridge */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_PCIE	0x0101		/* Sandy Bridge PCIe Root port */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_IGD	0x0102		/* Sandy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_PCIE_1	0x0105		/* Sandy Bridge PCIe Root port */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_PCIE_2	0x0109		/* Sandy Bridge PCIe Root port */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_IGD_1	0x0112		/* Sandy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_IGD_2	0x0122		/* Sandy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_M_HB	0x0104		/* Sandy Bridge Host Bridge */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_M_IGD	0x0106		/* Sandy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_M_IGD_1	0x0116		/* Sandy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_M_IGD_2	0x0126		/* Sandy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_S_HB	0x0108		/* Sandy Bridge Host Bridge */
#define	PCI_PRODUCT_INTEL_SANDYBRIDGE_S_IGD	0x010A		/* Sandy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_HB	0x0150		/* Ivy Bridge Host Bridge */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_PCIE	0x0151		/* Ivy Bridge PCI Express Root Port */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_IGD	0x0152		/* Ivy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_M_HB	0x0154		/* Ivy Bridge Host Bridge */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_PCIE_1	0x0155		/* Ivy Bridge PCI Express Root Port */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_M_IGD	0x0156		/* Ivy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_S_HB	0x0158		/* Ivy Bridge Host Bridge */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_PCIE_2	0x0159		/* Ivy Bridge PCI Express Root Port */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_S_IGD	0x015a		/* Ivy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_PCIE_3	0x015d		/* Ivy Bridge PCI Express Root Port */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_IGD_1	0x0162		/* Ivy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_M_IGD_1	0x0166		/* Ivy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_IVYBRIDGE_S_IGD_1	0x016a		/* Ivy Bridge Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_80312	0x030d		/* 80312 I/O Companion Chip */
#define	PCI_PRODUCT_INTEL_80321	0x0319		/* 80321 I/O Processor */
#define	PCI_PRODUCT_INTEL_6700PXH_IOXAPIC	0x0326		/* 6700PXH IOxAPIC */
#define	PCI_PRODUCT_INTEL_6700PXH_PCIE0	0x0329		/* 6700PXH PCI Express-to-PCI Bridge #0 */
#define	PCI_PRODUCT_INTEL_6700PXH_PCIE1	0x032a		/* 6700PXH PCI Express-to-PCI Bridge #1 */
#define	PCI_PRODUCT_INTEL_6702PXH_PCIX	0x032c		/* 6702PXH PCI Express-to-PCIX */
#define	PCI_PRODUCT_INTEL_IOP332_A	0x0330		/* IOP332 PCI Express-to-PCI Bridge #0 */
#define	PCI_PRODUCT_INTEL_IOP332_B	0x0332		/* IOP332 PCI Express-to-PCI Bridge #1 */
#define	PCI_PRODUCT_INTEL_80331	0x0335		/* Lindsay I/O Processor PCI-X Bridge */
#define	PCI_PRODUCT_INTEL_41210A	0x0340		/* Serial to Parallel PCI Bridge A */
#define	PCI_PRODUCT_INTEL_41210B	0x0341		/* Serial to Parallel PCI Bridge B */
#define	PCI_PRODUCT_INTEL_IOP333_A	0x0370		/* IOP333 PCI Express-to-PCI Bridge #0 */
#define	PCI_PRODUCT_INTEL_IOP333_B	0x0372		/* IOP333 PCI Express-to-PCI Bridge #1 */
#define	PCI_PRODUCT_INTEL_SRCZCRX	0x0407		/* RAID Controller */
#define	PCI_PRODUCT_INTEL_SRCU42E	0x0408		/* SCSI RAID Controller */
#define	PCI_PRODUCT_INTEL_SRCS28X	0x0409		/* SATA RAID Controller */
#define	PCI_PRODUCT_INTEL_HASWELL_IGD	0x0402		/* Haswell Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_HASWELL_IGD_1	0x0412		/* Haswell Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_DH89XXCC_IQIA	0x0434		/* DH89xxCC PCIe Endpoint and QuickAssist */
#define	PCI_PRODUCT_INTEL_DH89XXCL_IQIA	0x0435		/* DH89xxCL PCIe Endpoint and QuickAssist */
#define	PCI_PRODUCT_INTEL_DH89XXCC_SGMII	0x0438		/* DH89XXCC SGMII */
#define	PCI_PRODUCT_INTEL_DH89XXCC_SERDES	0x043a		/* DH89XXCC SerDes */
#define	PCI_PRODUCT_INTEL_DH89XXCC_BPLANE	0x043c		/* DH89XXCC backplane */
#define	PCI_PRODUCT_INTEL_DH89XXCC_SFP	0x0440		/* DH89XXCC SFP */
#define	PCI_PRODUCT_INTEL_DH89XXCC_IQIA_VF	0x0442		/* DH89XXCC QuickAssist Virtual Function */
#define	PCI_PRODUCT_INTEL_DH89XXCL_IQIA_VF	0x0443		/* DH89XXCL QuickAssist Virtual Function */
#define	PCI_PRODUCT_INTEL_PCEB	0x0482		/* 82375EB/SB PCI-EISA Bridge */
#define	PCI_PRODUCT_INTEL_CDC	0x0483		/* 82424ZX Cache and DRAM Controller */
#define	PCI_PRODUCT_INTEL_SIO	0x0484		/* 82378ZB System I/O */
#define	PCI_PRODUCT_INTEL_82426EX	0x0486		/* 82426EX PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_PCMC	0x04a3		/* 82434LX/NX PCI, Cache and Memory Controller (PCMC) */
#define	PCI_PRODUCT_INTEL_GDT_RAID1	0x0600		/* GDT RAID */
#define	PCI_PRODUCT_INTEL_GDT_RAID2	0x061f		/* GDT RAID */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6150_1	0x0885		/* Centrino Wireless-N 6150 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6150_2	0x0886		/* Centrino Wireless-N 6150 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_2230_1	0x0887		/* Centrino Wireless-N 2230 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_2230_2	0x0888		/* Centrino Wireless-N 2230 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6235	0x088e		/* Centrino Advanced-N 6235 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6235_2	0x088f		/* Centrino Advanced-N 6235 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_2200_1	0x0890		/* Centrino Wireless-N 2200 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_2200_2	0x0891		/* Centrino Wireless-N 2200 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_135_1	0x0892		/* Centrino Wireless-N 135 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_135_2	0x0893		/* Centrino Wireless-N 135 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_105_1	0x0894		/* Centrino Wireless-N 105 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_105_2	0x0895		/* Centrino Wireless-N 105 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_130_1	0x0896		/* Centrino Wireless-N 130 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_130_2	0x0897		/* Centrino Wireless-N 130 */
#define	PCI_PRODUCT_INTEL_X1000_SDIO_EMMC	0x08a7		/* Quark X1000 SDIO/eMMC */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_100_1	0x08ae		/* Centrino Wireless-N 100 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_100_2	0x08af		/* Centrino Wireless-N 100 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_7260_1	0x08b1		/* Dual Band Wireless AC 7260 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_7260_2	0x08b2		/* Dual Band Wireless AC 7260 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_3160_1	0x08b3		/* Dual Band Wireless AC 3160 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_3160_2	0x08b4		/* Dual Band Wireless AC 3160 */
#define	PCI_PRODUCT_INTEL_X1000_I2C_GPIO	0x0934		/* Quark X1000 I2C and GPIO */
#define	PCI_PRODUCT_INTEL_X1000_SPI	0x0935		/* Quark X1000 SPI */
#define	PCI_PRODUCT_INTEL_X1000_HS_UART	0x0936		/* Quark X1000 HS-UART */
#define	PCI_PRODUCT_INTEL_X1000_MAC	0x0937		/* Quark X1000 10/100 Ethernet MAC */
#define	PCI_PRODUCT_INTEL_X1000_EHCI	0x0939		/* Quark X1000 EHCI */
#define	PCI_PRODUCT_INTEL_X1000_OHCI	0x093a		/* Quark X1000 OHCI */
#define	PCI_PRODUCT_INTEL_PCIE_NVME_SSD	0x0953		/* PCIe NVMe SSD */
#define	PCI_PRODUCT_INTEL_X1000_HB	0x0958		/* Quark X1000 Host Bridge */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_7265_1	0x095a		/* Dual Band Wireless AC 7265 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_7265_2	0x095b		/* Dual Band Wireless AC 7265 */
#define	PCI_PRODUCT_INTEL_X1000_LB	0x095e		/* Quark X1000 Legacy Bridge */
#define	PCI_PRODUCT_INTEL_80960RM	0x0962		/* i960 RM PCI-PCI */
#define	PCI_PRODUCT_INTEL_80960RN	0x0964		/* i960 RN PCI-PCI */
#define	PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT1	0x0a02		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_M_HOST_DRAM	0x0a04		/* Core 4G (mobile) Host Bridge, DRAM */
#define	PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT1	0x0a06		/* HD Graphics (GT1) */
#define	PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT1	0x0a0a		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT1_1	0x0a0b		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_M_MINI_HDA	0x0a0c		/* Core 4G (mobile) Mini HD audio */
#define	PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT1_2	0x0a0e		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT2	0x0a12		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT2	0x0a16		/* HD Graphics (GT2) */
#define	PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT2	0x0a1a		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT2_1	0x0a1b		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT2_2	0x0a1e		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_D_ULT_GT3	0x0a22		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_M_ULT_GT3	0x0a26		/* HD Graphics 5000 (GT3) */
#define	PCI_PRODUCT_INTEL_CORE4G_S_ULT_GT3	0x0a2a		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT3_1	0x0a2b		/* HD Graphics */
#define	PCI_PRODUCT_INTEL_CORE4G_R_ULT_GT3_2	0x0a2e		/* Iris Graphics 5100 */
#define	PCI_PRODUCT_INTEL_HASWELL_HOST_DRAM	0x0c00		/* Haswell Host Bridge, DRAM */
#define	PCI_PRODUCT_INTEL_HASWELL_PCIE16	0x0c01		/* Haswell PCI-E x16 Controller */
#define	PCI_PRODUCT_INTEL_HASWELL_PCIE8	0x0c05		/* Haswell PCI-E x8 Controller */
#define	PCI_PRODUCT_INTEL_XE3_12KV3_HOST_DRAM	0x0c08		/* Xeon E3-1200 v3 Host Bridge, DRAM */
#define	PCI_PRODUCT_INTEL_HASWELL_PCIE4	0x0c09		/* Haswell PCI-E x4 Controller */
#define	PCI_PRODUCT_INTEL_HASWELL_MINI_HDA	0x0c0c		/* Haswell Mini HD Audio Controller */
#define	PCI_PRODUCT_INTEL_S1200_PCIE_1	0x0c46		/* Atom S1200 PCIe Root Port 1 */
#define	PCI_PRODUCT_INTEL_S1200_PCIE_2	0x0c47		/* Atom S1200 PCIe Root Port 2 */
#define	PCI_PRODUCT_INTEL_S1200_PCIE_3	0x0c48		/* Atom S1200 PCIe Root Port 3 */
#define	PCI_PRODUCT_INTEL_S1200_PCIE_4	0x0c49		/* Atom S1200 PCIe Root Port 4 */
#define	PCI_PRODUCT_INTEL_S1200_INTERNALMNG	0x0c54		/* Atom S1200 Internal management */
#define	PCI_PRODUCT_INTEL_S1200_DFX1	0x0c55		/* Atom S1200 Debug function 1 */
#define	PCI_PRODUCT_INTEL_S1200_DFX2	0x0c56		/* Atom S1200 Debug function 2 */
#define	PCI_PRODUCT_INTEL_S1200_SMBUS_0	0x0c59		/* Atom S1200 SMBus 0 (PCIe mass-storage) */
#define	PCI_PRODUCT_INTEL_S1200_SMBUS_1	0x0c5a		/* Atom S1200 SMBus 1 (enclosure maintain) */
#define	PCI_PRODUCT_INTEL_S1200_SMBUS_2	0x0c5b		/* Atom S1200 SMBus 2 */
#define	PCI_PRODUCT_INTEL_S1200_SMBUS_3	0x0c5c		/* Atom S1200 SMBus 3 */
#define	PCI_PRODUCT_INTEL_S1200_SMBUS_4	0x0c5d		/* Atom S1200 SMBus 4 */
#define	PCI_PRODUCT_INTEL_S1200_SMBUS_5	0x0c5e		/* Atom S1200 SMBus 5 */
#define	PCI_PRODUCT_INTEL_S1200_UART	0x0c5f		/* Atom S1200 High-Speed UART */
#define	PCI_PRODUCT_INTEL_S1200_ILB	0x0c60		/* Atom S1200 LPC bridge */
#define	PCI_PRODUCT_INTEL_S1200_S1220	0x0c72		/* Atom S1220 Internal */
#define	PCI_PRODUCT_INTEL_S1200_S1240	0x0c73		/* Atom S1240 Internal */
#define	PCI_PRODUCT_INTEL_S1200_S1260	0x0c75		/* Atom S1260 Internal */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_HB	0x0f00		/* Bay Trail Processor Transaction Router */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_HDA	0x0f04		/* Bay Trail HD Audio */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO2_DMA	0x0f06		/* Bay Trail Serial IO (DMA) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO2_PWM1	0x0f08		/* Bay Trail Serial IO (PWM) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO2_PWM2	0x0f09		/* Bay Trail Serial IO (PWM) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO2_UART1	0x0f0a		/* Bay Trail Serial IO (HSUART) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO2_UART2	0x0f0c		/* Bay Trail Serial IO (HSUART) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO2_SPI	0x0f0e		/* Bay Trail Serial IO (SPI) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_PCU_SMB	0x0f12		/* Bay Trail PCU SMBus */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SCC_MMC	0x0f14		/* Bay Trail Storage Control Cluster(MMC) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SCC_SDIO	0x0f15		/* Bay Trail Storage Control Cluster(SDIO) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SCC	0x0f16		/* Bay Trail Storage Control Cluster(SD) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_TXE	0x0f18		/* Bay Trail Trusted Execution Engine */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_PCU_LPC	0x0f1c		/* Bay Trail LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SATA_IDE_0	0x0f20		/* Bay Trail SATA (IDE) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SATA_IDE_1	0x0f21		/* Bay Trail SATA (IDE) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SATA_AHCI_0	0x0f22		/* Bay Trail SATA (AHCI) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SATA_AHCI_1	0x0f23		/* Bay Trail SATA (AHCI) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_LPEA	0x0f28		/* Bay Trail Low Power Engine Audio */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_IGD	0x0f31		/* Bay Trail Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_EHCI	0x0f34		/* Bay Trail USB EHCI */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_XHCI	0x0f35		/* Bay Trail USB xHCI */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_USB_DEV	0x0f37		/* Bay Trail USB device */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_CISP	0x0f38		/* Bay Trail Camera Image Signal Processor */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO_DMA	0x0f40		/* Bay Trail Serial IO (DMA) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO_I2C1	0x0f41		/* Bay Trail Serial IO (I2C) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO_I2C2	0x0f42		/* Bay Trail Serial IO (I2C) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO_I2C3	0x0f43		/* Bay Trail Serial IO (I2C) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO_I2C4	0x0f44		/* Bay Trail Serial IO (I2C) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO_I2C5	0x0f45		/* Bay Trail Serial IO (I2C) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO_I2C6	0x0f46		/* Bay Trail Serial IO (I2C) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_SIO_I2C7	0x0f47		/* Bay Trail Serial IO (I2C) */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_PCIE_1	0x0f48		/* Bay Trail PCIE Root Port */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_PCIE_2	0x0f4a		/* Bay Trail PCIE Root Port */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_PCIE_3	0x0f4c		/* Bay Trail PCIE Root Port */
#define	PCI_PRODUCT_INTEL_BAYTRAIL_PCIE_4	0x0f4e		/* Bay Trail PCIE Root Port */
#define	PCI_PRODUCT_INTEL_82542	0x1000		/* i82542 Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82543GC_FIBER	0x1001		/* i82453GC 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_MODEM56	0x1002		/* 56k Modem */
#define	PCI_PRODUCT_INTEL_82543GC_COPPER	0x1004		/* i82543GC 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82544EI_COPPER	0x1008		/* i82544EI 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82544EI_FIBER	0x1009		/* i82544EI 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82544GC_COPPER	0x100c		/* i82544GC 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82544GC_LOM	0x100d		/* i82544GC (LOM) Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82540EM	0x100e		/* i82540EM 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82545EM_COPPER	0x100f		/* i82545EM 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82546EB_COPPER	0x1010		/* i82546EB 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82545EM_FIBER	0x1011		/* i82545EM 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82546EB_FIBER	0x1012		/* i82546EB 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82541EI	0x1013		/* i82541EI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541ER_LOM	0x1014		/* i82541ER (LOM) Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82540EM_LOM	0x1015		/* i82540EM (LOM) Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82540EP_LOM	0x1016		/* i82540EP (LOM) Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82540EP	0x1017		/* i82540EP Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541EI_MOBILE	0x1018		/* i82541EI Mobile Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82547EI	0x1019		/* i82547EI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82547EI_MOBILE	0x101a		/* i82547EI Mobile Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82546EB_QUAD	0x101d		/* i82546EB 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82540EP_LP	0x101e		/* i82540EP Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82545GM_COPPER	0x1026		/* i82545GM 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82545GM_FIBER	0x1027		/* i82545GM 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82545GM_SERDES	0x1028		/* i82545GM Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_PRO_100	0x1029		/* PRO/100 Ethernet */
#define	PCI_PRODUCT_INTEL_IN_BUSINESS	0x1030		/* InBusiness Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_0	0x1031		/* PRO/100 VE Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_1	0x1032		/* PRO/100 VE Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_0	0x1033		/* PRO/100 VM Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_1	0x1034		/* PRO/100 VM Network Controller */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_0	0x1035		/* 82562EH HomePNA Network Controller */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_1	0x1036		/* 82562EH HomePNA Network Controller */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_2	0x1037		/* 82562EH HomePNA Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_2	0x1038		/* PRO/100 VM Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_2	0x1039		/* PRO/100 VE Network Controller w/ 82562ET/EZ PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_3	0x103a		/* PRO/100 VE Network Controller w/ 82562ET/EZ (CNR) PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_3	0x103b		/* PRO/100 VM Network Controller w/ 82562EM/EX PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_4	0x103c		/* PRO/100 VM Network Controller w/ 82562EM/EX (CNR) PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_4	0x103d		/* PRO/100 VE (MOB) Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_5	0x103e		/* PRO/100 VM (MOB) Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_WL_2100	0x1043		/* PRO/Wireless LAN 2100 3B Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_82597EX	0x1048		/* PRO/10GbE LR Server Adapter */
#define	PCI_PRODUCT_INTEL_82801H_M_AMT	0x1049		/* i82801H (M_AMT) LAN Controller */
#define	PCI_PRODUCT_INTEL_82801H_AMT	0x104a		/* i82801H (AMT) LAN Controller */
#define	PCI_PRODUCT_INTEL_82801H_LAN	0x104b		/* i82801H LAN Controller */
#define	PCI_PRODUCT_INTEL_82801H_IFE_LAN	0x104c		/* i82801H (IFE) LAN Controller */
#define	PCI_PRODUCT_INTEL_82801H_M_LAN	0x104d		/* i82801H (M) LAN Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_6	0x1050		/* PRO/100 VM Network Controller w/ 82562ET/EZ PHY */
#define	PCI_PRODUCT_INTEL_82801EB_LAN	0x1051		/* 82801EB/ER 10/100 Ethernet */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_7	0x1052		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_8	0x1053		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_9	0x1054		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_10	0x1055		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_11	0x1056		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_12	0x1057		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_M	0x1059		/* PRO/100 M Network Controller */
#define	PCI_PRODUCT_INTEL_82571EB_COPPER	0x105e		/* i82571EB 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82571EB_FIBER	0x105f		/* i82571EB 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82571EB_SERDES	0x1060		/* i82571EB Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82801FB_LAN_2	0x1064		/* 82801FB 10/100 Ethernet */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_9	0x1065		/* PRO/100 VE Ethernet */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_13	0x1066		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_14	0x1067		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_5	0x1068		/* PRO/100 VE (LOM) Network Controller */
#define	PCI_PRODUCT_INTEL_82801GB_LAN	0x1069		/* 82801GB 10/100 Ethernet */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_10	0x106a		/* PRO/100 VE Ethernet */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_11	0x106b		/* PRO/100 VE Ethernet */
#define	PCI_PRODUCT_INTEL_82547GI	0x1075		/* i82547GI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541GI	0x1076		/* i82541GI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541GI_MOBILE	0x1077		/* i82541GI Mobile Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82541ER	0x1078		/* i82541ER Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82546GB_COPPER	0x1079		/* i82546GB 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82546GB_FIBER	0x107a		/* i82546GB 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82546GB_SERDES	0x107b		/* i82546GB Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82541PI	0x107c		/* i82541PI Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82572EI_COPPER	0x107d		/* i82572EI 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82572EI_FIBER	0x107e		/* i82572EI 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_82572EI_SERDES	0x107f		/* i82572EI Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82546GB_PCIE	0x108a		/* PRO/1000MT (82546GB) */
#define	PCI_PRODUCT_INTEL_82573E	0x108b		/* i82573E Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82573E_IAMT	0x108c		/* i82573E Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_15	0x1091		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_6	0x1092		/* PRO/100 VE Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_8	0x1093		/* PRO/100 VE Network Controller */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_7	0x1094		/* PRO/100 VE Network Controller w/ 82562G PHY */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_16	0x1095		/* PRO/100 VM Network Connection */
#define	PCI_PRODUCT_INTEL_80K3LAN_CPR_DPT	0x1096		/* i80003 Dual 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_80K3LAN_FIB_DPT	0x1097		/* i80003 Dual 1000baseX Ethernet */
#define	PCI_PRODUCT_INTEL_80K3LAN_SDS_DPT	0x1098		/* i80003 Dual Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82546GB_QUAD_COPPER	0x1099		/* i82546GB Quad Port Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82573L	0x109a		/* i82573L Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82597EX_CX4	0x109e		/* 82597EX CX4 */
#define	PCI_PRODUCT_INTEL_82571EB_QUAD_COPPER	0x10a4		/* i82571EB Quad Port Gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82571EB_QUAD_FIBER	0x10a5		/* i82571EB Quad Port Gigabit Fiber Ethernet */
#define	PCI_PRODUCT_INTEL_82575EB_COPPER	0x10a7		/* i82575EB dual-1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82575EB_FIBER_SERDES	0x10a9		/* i82575EB dual-1000baseX Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82546GB_QUAD_COPPER_KSP3	0x10b5		/* i82546GB Quad Port Gigabit Ethernet (KSP3) */
#define	PCI_PRODUCT_INTEL_82598	0x10b6		/* 82598 10G Ethernet */
#define	PCI_PRODUCT_INTEL_82572EI	0x10b9		/* i82572EI 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_80K3LAN_CPR_SPT	0x10ba		/* i80003 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_80K3LAN_SDS_SPT	0x10bb		/* i80003 Gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82571GB_QUAD_COPPER	0x10bc		/* i82571GB Quad 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82801I_IGP_AMT	0x10bd		/* 82801I (AMT) LAN Controller */
#define	PCI_PRODUCT_INTEL_82801I_IGP_M	0x10bf		/* 82801I Mobile LAN Controller */
#define	PCI_PRODUCT_INTEL_82801I_IFE	0x10c0		/* 82801I LAN Controller */
#define	PCI_PRODUCT_INTEL_82801I_IFE_G	0x10c2		/* 82801I (G) LAN Controller */
#define	PCI_PRODUCT_INTEL_82801I_IFE_GT	0x10c3		/* 82801I (GT) LAN Controller */
#define	PCI_PRODUCT_INTEL_82801H_IFE_GT	0x10c4		/* i82801H IFE (GT) LAN Controller */
#define	PCI_PRODUCT_INTEL_82801H_IFE_G	0x10c5		/* i82801H IFE (G) LAN Controller */
#define	PCI_PRODUCT_INTEL_82598AF_DUAL	0x10c6		/* 82598 10 Gigabit AF Dual Port */
#define	PCI_PRODUCT_INTEL_82598AF	0x10c7		/* 82598 10 Gigabit AF */
#define	PCI_PRODUCT_INTEL_82598AT	0x10c8		/* 82598 10 Gigabit AT */
#define	PCI_PRODUCT_INTEL_82576_COPPER	0x10c9		/* 82576 1000BaseT Ethernet */
#define	PCI_PRODUCT_INTEL_82576_VF	0x10ca		/* 82576 1000BaseT Ethernet Virtual Function */
#define	PCI_PRODUCT_INTEL_82801H_IGP_M_V	0x10cb		/* i82801H IGP (MV) LAN Controller */
#define	PCI_PRODUCT_INTEL_82801J_R_BM_LM	0x10cc		/* i82567LM-2 LAN Controller */
#define	PCI_PRODUCT_INTEL_82801J_R_BM_LF	0x10cd		/* i82567LF-2 LAN Controller */
#define	PCI_PRODUCT_INTEL_82801J_R_BM_V	0x10ce		/* i82567V-2 LAN Controller */
#define	PCI_PRODUCT_INTEL_82574L	0x10d3		/* i82574L 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82571PT_QUAD_COPPER	0x10d5		/* i82571PT quad-1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82575GB_QUAD_COPPER	0x10d6		/* i82575GB quad-1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82571EB_DUAL_SERDES	0x10d9		/* i82571EB dual giabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82571EB_QUAD_SERDES	0x10da		/* i82571EB qual giabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82598EB_SFP	0x10db		/* 82598EB 10 Gigabit SFP */
#define	PCI_PRODUCT_INTEL_82598EB_CX4	0x10dd		/* 82598EB 10 Gigabit CX4 */
#define	PCI_PRODUCT_INTEL_82801J_D_BM_LM	0x10de		/* i82567LM-3 LAN Controller */
#define	PCI_PRODUCT_INTEL_82801J_D_BM_LF	0x10df		/* i82567LF-3 LAN Controller */
#define	PCI_PRODUCT_INTEL_82598_SR_DUAL_EM	0x10e1		/* 82598 10 Gigabit SR Dual Port */
#define	PCI_PRODUCT_INTEL_82575GB_QUAD_COPPER_PM	0x10e2		/* i82575GB Quad-1000baseT Ethernet (PM) */
#define	PCI_PRODUCT_INTEL_82801I_BM	0x10e5		/* i82567LM-4 LAN Controller */
#define	PCI_PRODUCT_INTEL_82576_FIBER	0x10e6		/* 82576 1000BaseX Ethernet */
#define	PCI_PRODUCT_INTEL_82576_SERDES	0x10e7		/* 82576 gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82576_QUAD_COPPER	0x10e8		/* 82576 quad-1000BaseT Ethernet */
#define	PCI_PRODUCT_INTEL_PCH_M_LM	0x10ea		/* PCH LAN (82577LM) Controller */
#define	PCI_PRODUCT_INTEL_PCH_M_LC	0x10eb		/* PCH LAN (82577LC) Controller */
#define	PCI_PRODUCT_INTEL_82598_CX4_DUAL	0x10ec		/* 82598 10 Gigabit CX4 Dual Port */
#define	PCI_PRODUCT_INTEL_82599_VF	0x10ed		/* 82599 10 Gigabit Ethernet Virtual Function */
#define	PCI_PRODUCT_INTEL_PCH_D_DM	0x10ef		/* PCH LAN (82578DM) Controller */
#define	PCI_PRODUCT_INTEL_PCH_D_DC	0x10f0		/* PCH LAN (82578DC) Controller */
#define	PCI_PRODUCT_INTEL_82598_DA_DUAL	0x10f1		/* 82598 10 Gigabit DA Dual Port */
#define	PCI_PRODUCT_INTEL_82598EB_XF_LR	0x10f4		/* 82598EB 10 Gigabit XF LR */
#define	PCI_PRODUCT_INTEL_82801I_IGP_M_AMT	0x10f5		/* 82801I Mobile (AMT) LAN Controller */
#define	PCI_PRODUCT_INTEL_82574LA	0x10f6		/* 82574L 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82599_KX4	0x10F7		/* 82599 (KX/KX4) 10 GbE Controller */
#define	PCI_PRODUCT_INTEL_82599_COMBO_BACKPLANE	0x10F8		/* 82599 (combined backplane; KR/KX4/KX) 10 GbE Controller */
#define	PCI_PRODUCT_INTEL_82599_CX4	0x10F9		/* 82599 (CX4) 10 GbE Controller */
#define	PCI_PRODUCT_INTEL_82599_SFP	0x10FB		/* 82599 (SFI/SFP+) 10 GbE Controller */
#define	PCI_PRODUCT_INTEL_82599_XAUI_LOM	0x10FC		/* 82599 (XAUI/BX4) 10 GbE Controller */
#define	PCI_PRODUCT_INTEL_82552	0x10fe		/* 82552 10/100 Network Connection */
#define	PCI_PRODUCT_INTEL_82815_DC100_HUB	0x1100		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_DC100_AGP	0x1101		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_DC100_GRAPH	0x1102		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82815_NOAGP_HUB	0x1110		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_NOAGP_GRAPH	0x1112		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82815_NOGRAPH_HUB	0x1120		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_NOGRAPH_AGP	0x1121		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_FULL_HUB	0x1130		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_FULL_AGP	0x1131		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_FULL_GRAPH	0x1132		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82806AA	0x1161		/* 82806AA PCI64 Hub Advanced Programmable Interrupt Controller */
#define	PCI_PRODUCT_INTEL_ADI_BECC	0x1162		/* ADI i80200 Big Endian Companion Chip */
#define	PCI_PRODUCT_INTEL_X1000_PCIE_0	0x11c3		/* Quark X1000 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_X1000_PCIE_1	0x11c4		/* Quark X1000 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_IXP1200	0x1200		/* IXP1200 Network Processor */
#define	PCI_PRODUCT_INTEL_82559ER	0x1209		/* 82559ER Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_82092AA	0x1222		/* 82092AA IDE Controller */
#define	PCI_PRODUCT_INTEL_SAA7116	0x1223		/* SAA7116 */
#define	PCI_PRODUCT_INTEL_82452_PB	0x1225		/* 82452KX/GX Orion Extended Express Processor to PCI Bridge */
#define	PCI_PRODUCT_INTEL_82596	0x1226		/* 82596 LAN Controller */
#define	PCI_PRODUCT_INTEL_EEPRO100	0x1227		/* EE Pro 100 10/100 Fast Ethernet */
#define	PCI_PRODUCT_INTEL_EEPRO100S	0x1228		/* EE Pro 100 Smart 10/100 Fast Ethernet */
#define	PCI_PRODUCT_INTEL_8255X	0x1229		/* 8255x Fast Ethernet LAN Controller */
#define	PCI_PRODUCT_INTEL_82437FX	0x122d		/* 82437FX (TSC) System Controller */
#define	PCI_PRODUCT_INTEL_82371FB_ISA	0x122e		/* 82371FB (PIIX) PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82371FB_IDE	0x1230		/* 82371FB (PIIX) IDE Controller */
#define	PCI_PRODUCT_INTEL_82371MX	0x1234		/* 82371MX (MPIIX) Mobile PCI I/O IDE Xcelerator */
#define	PCI_PRODUCT_INTEL_82437MX	0x1235		/* 82437MX (MTSC) Mobile System Controller */
#define	PCI_PRODUCT_INTEL_82441FX	0x1237		/* 82441FX (PMC) PCI and Memory Controller */
#define	PCI_PRODUCT_INTEL_82380AB	0x123c		/* 82380AB (MISA) Mobile PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82380FB	0x124b		/* 82380FB (MPCI2) Mobile PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82439HX	0x1250		/* 82439HX (TXC) System Controller */
#define	PCI_PRODUCT_INTEL_82870P2_PPB	0x1460		/* 82870P2 P64H2 PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82870P2_IOxAPIC	0x1461		/* 82870P2 P64H2 IOxAPIC */
#define	PCI_PRODUCT_INTEL_82870P2_HPLUG	0x1462		/* 82870P2 P64H2 Hot Plug Controller */
#define	PCI_PRODUCT_INTEL_82801I_82567V_3	0x1501		/* i82567V-3 LAN Controller */
#define	PCI_PRODUCT_INTEL_PCH2_LV_LM	0x1502		/* 82579LM Gigabit Network Connection */
#define	PCI_PRODUCT_INTEL_PCH2_LV_V	0x1503		/* 82579V Gigabit Network Connection */
#define	PCI_PRODUCT_INTEL_82599_SFP_EM	0x1507		/* 82599 10G Ethernet Express Module */
#define	PCI_PRODUCT_INTEL_82598_BX	0x1508		/* 82598 10G Ethernet BX */
#define	PCI_PRODUCT_INTEL_82576_NS	0x150a		/* 82576 gigabit Ethernet */
#define	PCI_PRODUCT_INTEL_82598AT2	0x150b		/* 82598 10G AT2 Ethernet */
#define	PCI_PRODUCT_INTEL_82583V	0x150c		/* i82583V 1000baseT Ethernet */
#define	PCI_PRODUCT_INTEL_82576_SERDES_QUAD	0x150d		/* 82576 quad-gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82580_COPPER	0x150e		/* 82580 1000BaseT Ethernet */
#define	PCI_PRODUCT_INTEL_82580_FIBER	0x150f		/* 82580 1000BaseX Ethernet */
#define	PCI_PRODUCT_INTEL_82580_SERDES	0x1510		/* 82580 1000BaseT Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82580_SGMII	0x1511		/* 82580 gigabit Ethernet (SGMII) */
#define	PCI_PRODUCT_INTEL_82599_KX4_MEZZ	0x1514		/* 82599 10G KX4 Ethernet Mezzanine */
#define	PCI_PRODUCT_INTEL_X540_VF	0x1515		/* X540 10G Ethernet Virtual Function */
#define	PCI_PRODUCT_INTEL_82580_COPPER_DUAL	0x1516		/* 82580 dual-1000BaseT Ethernet */
#define	PCI_PRODUCT_INTEL_82599_KR	0x1517		/* 82599 10G Ethernet KR */
#define	PCI_PRODUCT_INTEL_82576_NS_SERDES	0x1518		/* 82576 gigabit Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_82599_T3_LOM	0x151c		/* 82599 10G Ethernet */
#define	PCI_PRODUCT_INTEL_82580_ER	0x151d		/* 82580 1000BaseT Ethernet */
#define	PCI_PRODUCT_INTEL_82580_ER_DUAL	0x151e		/* 82580 dual-1000BaseT Ethernet */
#define	PCI_PRODUCT_INTEL_I350_VF	0x1520		/* I350 Gigabit Network Connection Virtual Function */
#define	PCI_PRODUCT_INTEL_I350_COPPER	0x1521		/* I350 Gigabit Network Connection */
#define	PCI_PRODUCT_INTEL_I350_FIBER	0x1522		/* I350 Gigabit Fiber Network Connection */
#define	PCI_PRODUCT_INTEL_I350_SERDES	0x1523		/* I350 Gigabit Backplane Connection */
#define	PCI_PRODUCT_INTEL_I350_SGMII	0x1524		/* I350 Gigabit Connection */
#define	PCI_PRODUCT_INTEL_82801J_D_BM_V	0x1525		/* 82567V LAN Controller */
#define	PCI_PRODUCT_INTEL_82576_QUAD_COPPER_ET2	0x1526		/* 82576 quad-1000BaseT Ethernet */
#define	PCI_PRODUCT_INTEL_82580_QUAD_FIBER	0x1527		/* 82580 quad-1000BaseX Ethernet */
#define	PCI_PRODUCT_INTEL_X540_AT2	0x1528		/* X540-AT2 10Gbase-T Ethernet */
#define	PCI_PRODUCT_INTEL_82599_SFP_FCOE	0x1529		/* 82599 10 GbE FCoE */
#define	PCI_PRODUCT_INTEL_82599_BPLANE_FCOE	0x152a		/* 82599 10 GbE Backplane FCoE */
#define	PCI_PRODUCT_INTEL_82576_VF_HV	0x152d		/* 82576 1000BaseT Ethernet Virtual Function */
#define	PCI_PRODUCT_INTEL_82599_VF_HV	0x152e		/* 82599 10 GbE Virtual Function */
#define	PCI_PRODUCT_INTEL_I350_VF_HV	0x152f		/* I350 Gigabit Network Connection Virtual Function */
#define	PCI_PRODUCT_INTEL_X540_VF_HV	0x1530		/* X540 10 GbE Virtual Function */
#define	PCI_PRODUCT_INTEL_I210_T1	0x1533		/* I210-T1 Ethernet Server Adapter */
#define	PCI_PRODUCT_INTEL_I210_COPPER_OEM1	0x1534		/* I210 Ethernet (COPPER OEM) */
#define	PCI_PRODUCT_INTEL_I210_COPPER_IT	0x1535		/* I210 Ethernet (COPPER IT) */
#define	PCI_PRODUCT_INTEL_I210_FIBER	0x1536		/* I210 Ethernet (FIBER) */
#define	PCI_PRODUCT_INTEL_I210_SERDES	0x1537		/* I210 Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_I210_SGMII	0x1538		/* I210 Ethernet (SGMII) */
#define	PCI_PRODUCT_INTEL_I211_COPPER	0x1539		/* I211 Ethernet (COPPER) */
#define	PCI_PRODUCT_INTEL_I217_LM	0x153a		/* I217-LM Ethernet Connection */
#define	PCI_PRODUCT_INTEL_I217_V	0x153b		/* I217-V Ethernet Connection */
#define	PCI_PRODUCT_INTEL_I350_DA4	0x1546		/* I350 Quad port Gigabit Connection */
#define	PCI_PRODUCT_INTEL_82599_SFP_SF_QP	0x154a		/* 82599 10 GbE Controller */
#define	PCI_PRODUCT_INTEL_XL710_VF	0x154c		/* XL710 Ethernet Virtual Function */
#define	PCI_PRODUCT_INTEL_82599_SFP_SF2	0x154d		/* 82599 (SFP+) 10 GbE Controller */
#define	PCI_PRODUCT_INTEL_82599EN_SFP	0x1557		/* 82599 10 GbE Controller */
#define	PCI_PRODUCT_INTEL_I218_V	0x1559		/* I218-V Ethernet Connection */
#define	PCI_PRODUCT_INTEL_I218_LM	0x155a		/* I218-LM Ethernet Connection */
#define	PCI_PRODUCT_INTEL_X540_BYPASS	0x155c		/* X540 10 GbE Bypass */
#define	PCI_PRODUCT_INTEL_82599_BYPASS	0x155d		/* 82599 10 GbE Bypass */
#define	PCI_PRODUCT_INTEL_XL710_VF_HV	0x1571		/* XL710 Ethernet Virtual Function */
#define	PCI_PRODUCT_INTEL_XL710_SFP	0x1572		/* XL710 SFP+ Ethernet */
#define	PCI_PRODUCT_INTEL_I210_COPPER_WOF	0x157b		/* I210 Ethernet (COPPER) */
#define	PCI_PRODUCT_INTEL_I210_SERDES_WOF	0x157c		/* I210 Ethernet (SERDES) */
#define	PCI_PRODUCT_INTEL_XL710_KX_A	0x157f		/* XL710 KX Ethernet */
#define	PCI_PRODUCT_INTEL_XL710_KX_B	0x1580		/* XL710 KX Ethernet */
#define	PCI_PRODUCT_INTEL_XL710_KX_C	0x1581		/* XL710 KX Ethernet */
#define	PCI_PRODUCT_INTEL_XL710_QSFP_A	0x1583		/* XL710 40GbE QSFP+ */
#define	PCI_PRODUCT_INTEL_XL710_QSFP_B	0x1584		/* XL710 40GbE QSFP+ */
#define	PCI_PRODUCT_INTEL_XL710_QSFP_C	0x1585		/* XL710 40GbE QSFP+ */
#define	PCI_PRODUCT_INTEL_X710_10G_T	0x1586		/* X710 10GBaseT Ethernet */
#define	PCI_PRODUCT_INTEL_I218_LM2	0x15a0		/* I218-LM Ethernet Connection */
#define	PCI_PRODUCT_INTEL_I218_V2	0x15a1		/* I218-V Ethernet Connection */
#define	PCI_PRODUCT_INTEL_I218_LM3	0x15a2		/* I218-LM Ethernet Connection */
#define	PCI_PRODUCT_INTEL_I218_V3	0x15a3		/* I218-V Ethernet Connection */
#define	PCI_PRODUCT_INTEL_CORE5G_HB_1	0x1604		/* Core 5G Host Bridge */
#define	PCI_PRODUCT_INTEL_CORE5G_HDA_1	0x160c		/* Core 5G HD Audio */
#define	PCI_PRODUCT_INTEL_CORE5G_M_GT1	0x1606		/* HD Graphics (GT1) */
#define	PCI_PRODUCT_INTEL_CORE5G_M_GT2_1	0x1616		/* HD Graphics 5500 */
#define	PCI_PRODUCT_INTEL_CORE5G_M_GT2_2	0x161e		/* HD Graphics 5300 */
#define	PCI_PRODUCT_INTEL_CORE5G_M_GT3_15W	0x1626		/* HD Graphics 6000 */
#define	PCI_PRODUCT_INTEL_CORE5G_M_GT3_28W	0x162b		/* Iris Graphics 6100 */
#define	PCI_PRODUCT_INTEL_80960_RP	0x1960		/* ROB-in i960RP Microprocessor */
#define	PCI_PRODUCT_INTEL_80960RM_2	0x1962		/* i960 RM PCI-PCI */
#define	PCI_PRODUCT_INTEL_82840_HB	0x1a21		/* 82840 Host */
#define	PCI_PRODUCT_INTEL_82840_AGP	0x1a23		/* 82840 AGP */
#define	PCI_PRODUCT_INTEL_82840_PCI	0x1a24		/* 82840 PCI */
#define	PCI_PRODUCT_INTEL_82845_HB	0x1a30		/* 82845 Host */
#define	PCI_PRODUCT_INTEL_82845_AGP	0x1a31		/* 82845 AGP */
#define	PCI_PRODUCT_INTEL_5000_DMA	0x1a38		/* 5000 Series Chipset DMA Engine */
#define	PCI_PRODUCT_INTEL_6SERIES_SATA_1	0x1c00		/* 6 Series SATA */
#define	PCI_PRODUCT_INTEL_6SERIES_SATA_2	0x1c01		/* 6 Series SATA */
#define	PCI_PRODUCT_INTEL_6SERIES_AHCI_1	0x1c02		/* 6 Series AHCI */
#define	PCI_PRODUCT_INTEL_6SERIES_AHCI_2	0x1c03		/* 6 Series AHCI */
#define	PCI_PRODUCT_INTEL_6SERIES_RAID_1	0x1c04		/* 6 Series RAID */
#define	PCI_PRODUCT_INTEL_6SERIES_RAID_2	0x1c05		/* 6 Series RAID */
#define	PCI_PRODUCT_INTEL_6SERIES_RAID_3	0x1c06		/* 6 Series RAID */
#define	PCI_PRODUCT_INTEL_6SERIES_SATA_3	0x1c08		/* 6 Series SATA */
#define	PCI_PRODUCT_INTEL_6SERIES_SATA_4	0x1c09		/* 6 Series SATA */
#define	PCI_PRODUCT_INTEL_6SERIES_PCIE_1	0x1c10		/* 6 Series PCIE */
#define	PCI_PRODUCT_INTEL_6SERIES_PCIE_2	0x1c12		/* 6 Series PCIE */
#define	PCI_PRODUCT_INTEL_6SERIES_PCIE_3	0x1c14		/* 6 Series PCIE */
#define	PCI_PRODUCT_INTEL_6SERIES_PCIE_4	0x1c16		/* 6 Series PCIE */
#define	PCI_PRODUCT_INTEL_6SERIES_PCIE_5	0x1c18		/* 6 Series PCIE */
#define	PCI_PRODUCT_INTEL_6SERIES_PCIE_6	0x1c1a		/* 6 Series PCIE */
#define	PCI_PRODUCT_INTEL_6SERIES_PCIE_7	0x1c1c		/* 6 Series PCIE */
#define	PCI_PRODUCT_INTEL_6SERIES_PCIE_8	0x1c1e		/* 6 Series PCIE */
#define	PCI_PRODUCT_INTEL_6SERIES_HDA	0x1c20		/* 6 Series HD Audio */
#define	PCI_PRODUCT_INTEL_6SERIES_SMB	0x1c22		/* 6 Series SMBus Controller */
#define	PCI_PRODUCT_INTEL_6SERIES_THERM	0x1c24		/* 6 Series Thermal */
#define	PCI_PRODUCT_INTEL_6SERIES_DMI	0x1c25		/* 6 Series DMI-PCI */
#define	PCI_PRODUCT_INTEL_6SERIES_EHCI_1	0x1c26		/* 6 Series USB */
#define	PCI_PRODUCT_INTEL_6SERIES_EHCI_2	0x1c2d		/* 6 Series USB */
#define	PCI_PRODUCT_INTEL_6SERIES_MEI	0x1c3a		/* 6 Series MEI */
#define	PCI_PRODUCT_INTEL_6SERIES_KT	0x1c3d		/* 6 Series KT */
#define	PCI_PRODUCT_INTEL_Z68_LPC	0x1c44		/* Z68 LPC */
#define	PCI_PRODUCT_INTEL_P67_LPC	0x1c46		/* P67 LPC */
#define	PCI_PRODUCT_INTEL_UM67_LPC	0x1c47		/* UM67 LPC */
#define	PCI_PRODUCT_INTEL_HM65_LPC	0x1c49		/* HM65 LPC */
#define	PCI_PRODUCT_INTEL_H67_LPC	0x1c4a		/* H67 LPC */
#define	PCI_PRODUCT_INTEL_HM67_LPC	0x1c4b		/* HM67 LPC */
#define	PCI_PRODUCT_INTEL_Q65_LPC	0x1c4c		/* Q65 LPC */
#define	PCI_PRODUCT_INTEL_QS67_LPC	0x1c4d		/* QS67 LPC */
#define	PCI_PRODUCT_INTEL_Q67_LPC	0x1c4e		/* Q67 LPC */
#define	PCI_PRODUCT_INTEL_QM67_LPC	0x1c4f		/* QM67 LPC */
#define	PCI_PRODUCT_INTEL_B65_LPC	0x1c50		/* B65 LPC */
#define	PCI_PRODUCT_INTEL_C202_LPC	0x1c52		/* C202 LPC */
#define	PCI_PRODUCT_INTEL_C204_LPC	0x1c54		/* C204 LPC */
#define	PCI_PRODUCT_INTEL_C206_LPC	0x1c56		/* C206 LPC */
#define	PCI_PRODUCT_INTEL_H61_LPC	0x1c5c		/* H61 LPC */
#define	PCI_PRODUCT_INTEL_C600_SATA_1	0x1d00		/* C600/X79 SATA */
#define	PCI_PRODUCT_INTEL_C600_AHCI	0x1d02		/* C600/X79 AHCI */
#define	PCI_PRODUCT_INTEL_C600_RAID_1	0x1d04		/* C600/X79 RAID */
#define	PCI_PRODUCT_INTEL_C600_RAID_2	0x1d06		/* C600/X79 Premium RAID */
#define	PCI_PRODUCT_INTEL_C600_SATA_2	0x1d08		/* C600/X79 SATA */
#define	PCI_PRODUCT_INTEL_C600_PCIE_1	0x1d10		/* C600/X79 PCIE */
#define	PCI_PRODUCT_INTEL_C600_PCIE_2	0x1d12		/* C600/X79 PCIE */
#define	PCI_PRODUCT_INTEL_C600_PCIE_3	0x1d14		/* C600/X79 PCIE */
#define	PCI_PRODUCT_INTEL_C600_PCIE_4	0x1d16		/* C600/X79 PCIE */
#define	PCI_PRODUCT_INTEL_C600_PCIE_5	0x1d18		/* C600/X79 PCIE */
#define	PCI_PRODUCT_INTEL_C600_PCIE_6	0x1d1a		/* C600/X79 PCIE */
#define	PCI_PRODUCT_INTEL_C600_PCIE_7	0x1d1c		/* C600/X79 PCIE */
#define	PCI_PRODUCT_INTEL_C600_PCIE_8	0x1d1e		/* C600/X79 PCIE */
#define	PCI_PRODUCT_INTEL_C600_HDA	0x1d20		/* C600 HD Audio */
#define	PCI_PRODUCT_INTEL_C600_SMBUS	0x1d22		/* C600 SMBus Controller */
#define	PCI_PRODUCT_INTEL_C600_THERM	0x1d24		/* C600 Thermal Management Controller */
#define	PCI_PRODUCT_INTEL_C600_EHCI_1	0x1d26		/* C600 USB */
#define	PCI_PRODUCT_INTEL_C600_EHCI_2	0x1d2d		/* C600 USB */
#define	PCI_PRODUCT_INTEL_C600_LAN	0x1d33		/* C600 LAN */
#define	PCI_PRODUCT_INTEL_C600_MEI_1	0x1d3a		/* C600 MEI */
#define	PCI_PRODUCT_INTEL_C600_MEI_2	0x1d3b		/* C600 MEI */
#define	PCI_PRODUCT_INTEL_C600_KT	0x1d3d		/* C600 KT */
#define	PCI_PRODUCT_INTEL_C600_VPCIE	0x1d3e		/* C600 Virtual PCIE */
#define	PCI_PRODUCT_INTEL_C600_LPC	0x1d41		/* C600 LPC */
#define	PCI_PRODUCT_INTEL_C600_SAS_1	0x1d60		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_SATA_1	0x1d61		/* C600 SAS Controller (SATA) */
#define	PCI_PRODUCT_INTEL_C600_SAS_2	0x1d62		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_3	0x1d63		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_4	0x1d64		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_5	0x1d65		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_6	0x1d66		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_7	0x1d67		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_8	0x1d68		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_9	0x1d69		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_SATA_2	0x1d6a		/* C600 SAS Controller (SATA) */
#define	PCI_PRODUCT_INTEL_C600_SAS_SATA_3	0x1d6b		/* C600/X79 SAS Controller (SATA) */
#define	PCI_PRODUCT_INTEL_C600_SAS_10	0x1d6c		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_11	0x1d6d		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_12	0x1d6e		/* C600 SAS Controller */
#define	PCI_PRODUCT_INTEL_C600_SAS_SATA_4	0x1d6f		/* C600 SAS Controller (SATA) */
#define	PCI_PRODUCT_INTEL_C600_SMB_0	0x1d70		/* C600/X79 Series SMBus Controller */
#define	PCI_PRODUCT_INTEL_C600_SMB_1	0x1d71		/* C606/C608 SMBus Controller */
#define	PCI_PRODUCT_INTEL_C600_SMB_2	0x1d72		/* C608 SMBus Controller */
#define	PCI_PRODUCT_INTEL_7SER_DT_SATA_1	0x1e00		/* 7 Series (desktop) SATA Controller */
#define	PCI_PRODUCT_INTEL_7SER_MO_SATA_1	0x1e01		/* 7 Series (mobile) SATA Controller */
#define	PCI_PRODUCT_INTEL_7SER_DT_SATA_AHCI	0x1e02		/* 7 Series (desktop) SATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_7SER_MO_SATA_AHCI	0x1e03		/* 7 Series (mobile) SATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_7SER_DT_SATA_RAID_2	0x1e04		/* 7 Series (desktop) SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_7SER_DT_SATA_RAID_3	0x1e06		/* 7 Series (desktop) SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_7SER_MO_SATA_RAID	0x1e07		/* 7 Series (mobile) SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_7SER_DT_SATA_2	0x1e08		/* 7 Series (desktop) SATA Controller */
#define	PCI_PRODUCT_INTEL_7SER_MO_SATA_2	0x1e09		/* 7 Series (mobile) SATA Controller */
#define	PCI_PRODUCT_INTEL_7SER_DT_SATA_RAID_1	0x1e0e		/* 7 Series (desktop) SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_7SERIES_PCIE_1	0x1e10		/* 7 Series PCIE */
#define	PCI_PRODUCT_INTEL_7SERIES_PCIE_2	0x1e12		/* 7 Series PCIE */
#define	PCI_PRODUCT_INTEL_7SERIES_PCIE_3	0x1e14		/* 7 Series PCIE */
#define	PCI_PRODUCT_INTEL_7SERIES_PCIE_4	0x1e16		/* 7 Series PCIE */
#define	PCI_PRODUCT_INTEL_7SERIES_PCIE_5	0x1e18		/* 7 Series PCIE */
#define	PCI_PRODUCT_INTEL_7SERIES_PCIE_6	0x1e1a		/* 7 Series PCIE */
#define	PCI_PRODUCT_INTEL_7SERIES_PCIE_7	0x1e1c		/* 7 Series PCIE */
#define	PCI_PRODUCT_INTEL_7SERIES_PCIE_8	0x1e1e		/* 7 Series PCIE */
#define	PCI_PRODUCT_INTEL_7SERIES_HDA	0x1e20		/* 7 Series HD Audio */
#define	PCI_PRODUCT_INTEL_7SERIES_SMB	0x1e22		/* 7 Series SMBus Controller */
#define	PCI_PRODUCT_INTEL_7SERIES_PPB	0x1e25		/* 7 Series PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_7SERIES_EHCI_1	0x1e26		/* 7 Series USB EHCI */
#define	PCI_PRODUCT_INTEL_7SERIES_EHCI_2	0x1e2d		/* 7 Series USB EHCI */
#define	PCI_PRODUCT_INTEL_7SERIES_XHCI	0x1e31		/* 7 Series USB xHCI */
#define	PCI_PRODUCT_INTEL_7SERIES_MEI_1	0x1e3a		/* 7 Series MEI Controller */
#define	PCI_PRODUCT_INTEL_7SERIES_MEI_2	0x1e3b		/* 7 Series MEI Controller */
#define	PCI_PRODUCT_INTEL_7SERIES_IDE_R	0x1e3c		/* 7 Series IDE-R */
#define	PCI_PRODUCT_INTEL_7SERIES_KT	0x1e3d		/* 7 Series KT */
#define	PCI_PRODUCT_INTEL_Z77_LPC	0x1e44		/* Z77 LPC */
#define	PCI_PRODUCT_INTEL_Z75_LPC	0x1e46		/* Z75 LPC */
#define	PCI_PRODUCT_INTEL_Q77_LPC	0x1e47		/* Q77 LPC */
#define	PCI_PRODUCT_INTEL_Q75_LPC	0x1e48		/* Q75 LPC */
#define	PCI_PRODUCT_INTEL_B75_LPC	0x1e49		/* B75 LPC */
#define	PCI_PRODUCT_INTEL_H77_LPC	0x1e4a		/* H77 LPC */
#define	PCI_PRODUCT_INTEL_C216_LPC	0x1e53		/* C216 LPC */
#define	PCI_PRODUCT_INTEL_MOBILE_QM77_LPC	0x1e55		/* Mobile QM77 LPC */
#define	PCI_PRODUCT_INTEL_MOBILE_QS77_LPC	0x1e56		/* Mobile QS77 LPC */
#define	PCI_PRODUCT_INTEL_MOBILE_HM77_LPC	0x1e57		/* Mobile HM77 LPC */
#define	PCI_PRODUCT_INTEL_MOBILE_UM77_LPC	0x1e58		/* Mobile UM77 LPC */
#define	PCI_PRODUCT_INTEL_MOBILE_HM76_LPC	0x1e59		/* Mobile HM76 LPC */
#define	PCI_PRODUCT_INTEL_MOBILE_HM75_LPC	0x1e5d		/* Mobile HM75 LPC */
#define	PCI_PRODUCT_INTEL_MOBILE_HM70_LPC	0x1e5e		/* Mobile HM70 LPC */
#define	PCI_PRODUCT_INTEL_NM70_LPC	0x1e5f		/* NM70 LPC */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_0	0x1f00		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_1	0x1f01		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_2	0x1f02		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_3	0x1f03		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_4	0x1f04		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_5	0x1f05		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_6	0x1f06		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_7	0x1f07		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_8	0x1f08		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_9	0x1f09		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_A	0x1f0a		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_B	0x1f0b		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_C	0x1f0c		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_D	0x1f0d		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_E	0x1f0e		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_TROUTER_F	0x1f0f		/* C2000 Transaction Router */
#define	PCI_PRODUCT_INTEL_C2000_PCIE_1	0x1f10		/* C2000 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_C2000_PCIE_2	0x1f11		/* C2000 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_C2000_PCIE_3	0x1f12		/* C2000 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_C2000_PCIE_4	0x1f13		/* C2000 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_C2000_RAS	0x1f14		/* C2000 RAS */
#define	PCI_PRODUCT_INTEL_C2000_SMBUS	0x1f15		/* C2000 SMBus 2.0 */
#define	PCI_PRODUCT_INTEL_C2000_RCEC	0x1f16		/* C2000 RCEC */
#define	PCI_PRODUCT_INTEL_C2000_IQIA_PHYS	0x1f18		/* C2000 IQIA Physical Function */
#define	PCI_PRODUCT_INTEL_C2000_IQIA_VF	0x1f19		/* C2000 IQIA Virtual Function */
#define	PCI_PRODUCT_INTEL_C2000_SATA2	0x1f22		/* C2000 SATA2 */
#define	PCI_PRODUCT_INTEL_C2000_USB	0x1f2c		/* C2000 USB 2.0 */
#define	PCI_PRODUCT_INTEL_C2000_SATA3	0x1f32		/* C2000 SATA3 */
#define	PCI_PRODUCT_INTEL_C2000_PCU_1	0x1f38		/* C2000 PCU */
#define	PCI_PRODUCT_INTEL_C2000_PCU_2	0x1f39		/* C2000 PCU */
#define	PCI_PRODUCT_INTEL_C2000_PCU_3	0x1f3a		/* C2000 PCU */
#define	PCI_PRODUCT_INTEL_C2000_PCU_4	0x1f3b		/* C2000 PCU */
#define	PCI_PRODUCT_INTEL_C2000_PCU_SMBUS	0x1f3c		/* C2000 PCU SMBus */
#define	PCI_PRODUCT_INTEL_C2000_1000KX	0x1f40		/* C2000 Ethernet(1000BASE-KX) */
#define	PCI_PRODUCT_INTEL_C2000_SGMII	0x1f41		/* C2000 Ethernet(SGMII) */
#define	PCI_PRODUCT_INTEL_C2000_DUMMYGBE	0x1f42		/* C2000 Ethernet(Dummy function) */
#define	PCI_PRODUCT_INTEL_C2000_25GBE	0x1f45		/* C2000 Ethernet(2.5Gbe) */
#define	PCI_PRODUCT_INTEL_DH89XXCC_LPC	0x2310		/* DH89xxCC LPC Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCC_SATA_1	0x2323		/* DH89xxCC SATA Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCC_SATA_2	0x2326		/* DH89xxCC SATA Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCC_SMB	0x2330		/* DH89xxCC SMBus Host Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCC_THERMAL	0x2332		/* DH89xxCC Thermal Subsystem */
#define	PCI_PRODUCT_INTEL_DH89XXCC_USB_1	0x2334		/* DH89xxCC USB EHCI */
#define	PCI_PRODUCT_INTEL_DH89XXCC_USB_2	0x2335		/* DH89xxCC USB EHCI */
#define	PCI_PRODUCT_INTEL_DH89XXCC_PCIE_1_1	0x2342		/* DH89xxCC PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCC_PCIE_1_2	0x2343		/* DH89xxCC PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCC_PCIE_2_1	0x2344		/* DH89xxCC PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCC_PCIE_2_2	0x2345		/* DH89xxCC PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCC_PCIE_3_1	0x2346		/* DH89xxCC PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCC_PCIE_3_2	0x2347		/* DH89xxCC PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCC_PCIE_4_1	0x2348		/* DH89xxCC PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCC_PCIE_4_2	0x2349		/* DH89xxCC PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCC_WDT	0x2360		/* DH89xxCC Watchdog Timer for Core Reset */
#define	PCI_PRODUCT_INTEL_DH89XXCC_MEI_1	0x2364		/* DH89xxCC MEI Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCC_MEI_2	0x2365		/* DH89xxCC MEI Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCL_LPC	0x2390		/* DH89xxCL LPC Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCL_SATA_1	0x23a3		/* DH89xxCL SATA Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCL_SATA_2	0x23a6		/* DH89xxCL SATA Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCL_SMB	0x23b0		/* DH89xxCL SMBus Host Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCL_THERMAL	0x23b2		/* DH89xxCL Thermal Subsystem */
#define	PCI_PRODUCT_INTEL_DH89XXCL_USB_1	0x23b4		/* DH89xxCL USB EHCI */
#define	PCI_PRODUCT_INTEL_DH89XXCL_USB_2	0x23b4		/* DH89xxCL USB EHCI */
#define	PCI_PRODUCT_INTEL_DH89XXCL_PCIE_1_1	0x23c2		/* DH89xxCL PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCL_PCIE_1_2	0x23c3		/* DH89xxCL PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCL_PCIE_2_1	0x23c4		/* DH89xxCL PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCL_PCIE_2_2	0x23c5		/* DH89xxCL PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCL_PCIE_3_1	0x23c6		/* DH89xxCL PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCL_PCIE_3_2	0x23c7		/* DH89xxCL PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCL_PCIE_4_1	0x23c8		/* DH89xxCL PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCL_PCIE_4_2	0x23c9		/* DH89xxCL PCIe Root Port */
#define	PCI_PRODUCT_INTEL_DH89XXCL_WDT	0x23e0		/* DH89xxCL Watchdog Timer for Core Reset */
#define	PCI_PRODUCT_INTEL_DH89XXCL_MEI_1	0x23e4		/* DH89xxCL MEI Controller */
#define	PCI_PRODUCT_INTEL_DH89XXCL_MEI_2	0x23e5		/* DH89xxCL MEI Controller */
#define	PCI_PRODUCT_INTEL_82801AA_LPC	0x2410		/* 82801AA LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801AA_IDE	0x2411		/* 82801AA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801AA_USB	0x2412		/* 82801AA USB Controller */
#define	PCI_PRODUCT_INTEL_82801AA_SMB	0x2413		/* 82801AA SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801AA_ACA	0x2415		/* 82801AA AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801AA_ACM	0x2416		/* 82801AA AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801AA_HPB	0x2418		/* 82801AA Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801AB_LPC	0x2420		/* 82801AB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801AB_IDE	0x2421		/* 82801AB IDE Controller */
#define	PCI_PRODUCT_INTEL_82801AB_USB	0x2422		/* 82801AB USB Controller */
#define	PCI_PRODUCT_INTEL_82801AB_SMB	0x2423		/* 82801AB SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801AB_ACA	0x2425		/* 82801AB AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801AB_ACM	0x2426		/* 82801AB AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801AB_HPB	0x2428		/* 82801AB Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_LPC	0x2440		/* 82801BA LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_USB1	0x2442		/* 82801BA USB Controller */
#define	PCI_PRODUCT_INTEL_82801BA_SMB	0x2443		/* 82801BA SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801BA_USB2	0x2444		/* 82801BA USB Controller */
#define	PCI_PRODUCT_INTEL_82801BA_ACA	0x2445		/* 82801BA AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801BA_ACM	0x2446		/* 82801BA AC-97 PCI Modem */
#define	PCI_PRODUCT_INTEL_82801BAM_HPB	0x2448		/* 82801BAM Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_LAN	0x2449		/* 82801BA LAN Controller */
#define	PCI_PRODUCT_INTEL_82801BAM_IDE	0x244a		/* 82801BAM IDE Controller */
#define	PCI_PRODUCT_INTEL_82801BA_IDE	0x244b		/* 82801BA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801BAM_LPC	0x244c		/* 82801BAM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801BA_HPB	0x244e		/* 82801BA Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82801E_LPC	0x2450		/* 82801E LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801E_SMB	0x2453		/* 82801E SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801E_LAN_1	0x2459		/* 82801E LAN Controller */
#define	PCI_PRODUCT_INTEL_82801E_LAN_2	0x245d		/* 82801E LAN Controller */
#define	PCI_PRODUCT_INTEL_82801CA_LPC	0x2480		/* 82801CA LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801CA_USB_1	0x2482		/* 82801CA USB Controller */
#define	PCI_PRODUCT_INTEL_82801CA_SMB	0x2483		/* 82801CA SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801CA_USB_2	0x2484		/* 82801CA USB Controller */
#define	PCI_PRODUCT_INTEL_82801CA_AC	0x2485		/* 82801CA AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801CA_MOD	0x2486		/* 82801CA AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801CA_USBC	0x2487		/* 82801CA USB Controller */
#define	PCI_PRODUCT_INTEL_82801CA_IDE_1	0x248A		/* 82801CA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801CA_IDE_2	0x248B		/* 82801CA IDE Controller */
#define	PCI_PRODUCT_INTEL_82801CAM_LPC	0x248C		/* 82801CAM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801DB_LPC	0x24C0		/* 82801DB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801DB_USB_1	0x24C2		/* 82801DB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801DB_SMB	0x24C3		/* 82801DB SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801DB_USB_2	0x24C4		/* 82801DB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801DB_AC	0x24C5		/* 82801DB AC97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801DB_MOD	0x24C6		/* 82801DB AC97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801DB_USB_3	0x24C7		/* 82801DB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801DBM_IDE	0x24CA		/* 82801DBM IDE Controller */
#define	PCI_PRODUCT_INTEL_82801DB_IDE	0x24CB		/* 82801DB IDE Controller (UltraATA/100) */
#define	PCI_PRODUCT_INTEL_82801DBM_LPC	0x24CC		/* 82801DB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801DB_USBC	0x24CD		/* 82801DB USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_LPC	0x24D0		/* 82801EB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801EB_SATA	0x24D1		/* 82801EB Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82801EB_USB_0	0x24D2		/* 82801EB/ER USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_SMB	0x24D3		/* 82801EB/ER SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801EB_USB_1	0x24D4		/* 82801EB/ER USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_AC	0x24D5		/* 82801EB/ER AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801EB_MOD	0x24D6		/* 82801EB/ER AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801EB_USB_2	0x24D7		/* 82801EB/ER USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_IDE	0x24DB		/* 82801EB/ER IDE Controller */
#define	PCI_PRODUCT_INTEL_82801EB_EHCI	0x24DD		/* 82801EB/ER USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801EB_USB_3	0x24DE		/* 82801EB/ER USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801ER_SATA	0x24DF		/* 82801ER Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82820_MCH	0x2501		/* 82820 MCH (Camino) */
#define	PCI_PRODUCT_INTEL_82820_AGP	0x250f		/* 82820 AGP */
#define	PCI_PRODUCT_INTEL_82850_HB	0x2530		/* 82850 Host */
#define	PCI_PRODUCT_INTEL_82860_HB	0x2531		/* 82860 Host */
#define	PCI_PRODUCT_INTEL_82850_AGP	0x2532		/* 82850/82860 AGP */
#define	PCI_PRODUCT_INTEL_82860_PCI1	0x2533		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI2	0x2534		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI3	0x2535		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI4	0x2536		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_E7500_HB	0x2540		/* E7500 MCH Host */
#define	PCI_PRODUCT_INTEL_E7500_DRAM	0x2541		/* E7500 MCH DRAM Controller */
#define	PCI_PRODUCT_INTEL_E7500_HI_B1	0x2543		/* E7500 MCH HI_B vppb 1 */
#define	PCI_PRODUCT_INTEL_E7500_HI_B2	0x2544		/* E7500 MCH HI_B vppb 2 */
#define	PCI_PRODUCT_INTEL_E7500_HI_C1	0x2545		/* E7500 MCH HI_C vppb 1 */
#define	PCI_PRODUCT_INTEL_E7500_HI_C2	0x2546		/* E7500 MCH HI_C vppb 2 */
#define	PCI_PRODUCT_INTEL_E7500_HI_D1	0x2547		/* E7500 MCH HI_D vppb 1 */
#define	PCI_PRODUCT_INTEL_E7500_HI_D2	0x2548		/* E7500 MCH HI_D vppb 2 */
#define	PCI_PRODUCT_INTEL_E7501_HB	0x254c		/* E7501 MCH Host */
#define	PCI_PRODUCT_INTEL_E7505_HB	0x2550		/* E7505 MCH Host */
#define	PCI_PRODUCT_INTEL_E7505_RAS	0x2551		/* E7505 MCH RAS Controller */
#define	PCI_PRODUCT_INTEL_E7505_AGP	0x2552		/* E7505 MCH Host-AGP Bridge */
#define	PCI_PRODUCT_INTEL_E7505_HI_B1	0x2553		/* E7505 MCH HI_B PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_E7505_HI_B2	0x2554		/* E7505 MCH HI_B PCI-PCI Error Reporting */
#define	PCI_PRODUCT_INTEL_82845G_DRAM	0x2560		/* 82845G/GL DRAM Controller / Host-Hub I/F Bridge */
#define	PCI_PRODUCT_INTEL_82845G_AGP	0x2561		/* 82845G/GL Host-AGP Bridge */
#define	PCI_PRODUCT_INTEL_82845G_IGD	0x2562		/* 82845G/GL Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82865_HB	0x2570		/* 82865 Host */
#define	PCI_PRODUCT_INTEL_82865_AGP	0x2571		/* 82865 AGP */
#define	PCI_PRODUCT_INTEL_82865_IGD	0x2572		/* 82865G Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82801EB_HPB	0x2573		/* 82801EB Hub-PCI Bridge */
#define	PCI_PRODUCT_INTEL_82875P_HB	0x2578		/* 82875P Host */
#define	PCI_PRODUCT_INTEL_82875P_AGP	0x2579		/* 82875P AGP */
#define	PCI_PRODUCT_INTEL_82875P_CSA	0x257b		/* 82875P PCI-CSA Bridge */
#define	PCI_PRODUCT_INTEL_82915G_HB	0x2580		/* 82915P/G/GL Host */
#define	PCI_PRODUCT_INTEL_82915G_EX	0x2581		/* 82915P/G/GL PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82915G_IGD	0x2582		/* 82915G/GL Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82925X_HB	0x2584		/* 82925X Host */
#define	PCI_PRODUCT_INTEL_82925X_EX	0x2585		/* 82925X PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_E7221_HB	0x2588		/* E7221 Host Bridge */
#define	PCI_PRODUCT_INTEL_E7221_IGD	0x258a		/* E7221 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82915GM_HB	0x2590		/* 82915PM/GM/GMS,82910GML Host Bridge */
#define	PCI_PRODUCT_INTEL_82915GM_EX	0x2591		/* 82915PM/GM PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82915GM_IGD	0x2592		/* 82915GM/GMS,82910GML Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_6300ESB_LPC	0x25a1		/* 6300ESB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_6300ESB_IDE	0x25a2		/* 6300ESB IDE Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_SATA	0x25a3		/* 6300ESB SATA Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_SMB	0x25a4		/* 6300ESB SMBus Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_ACA	0x25a6		/* 6300ESB AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_ACM	0x25a7		/* 6300ESB AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_USB_0	0x25a9		/* 6300ESB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_USB_1	0x25aa		/* 6300ESB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_WDT	0x25ab		/* 6300ESB Watchdog Timer */
#define	PCI_PRODUCT_INTEL_6300ESB_APIC	0x25ac		/* 6300ESB Advanced Interrupt Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_EHCI	0x25ad		/* 6300ESB USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_6300ESB_PCIX	0x25ae		/* 6300ESB PCI-X Bridge */
#define	PCI_PRODUCT_INTEL_6300ESB_RAID	0x25b0		/* 6300ESB SATA RAID Controller */
#define	PCI_PRODUCT_INTEL_5000X_MCH	0x25c0		/* 5000X Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_5000Z_HB	0x25d0		/* 5000Z ESI */
#define	PCI_PRODUCT_INTEL_5000V_HB	0x25d4		/* 5000V ESI */
#define	PCI_PRODUCT_INTEL_5000P_HB	0x25d8		/* 5000P ESI */
#define	PCI_PRODUCT_INTEL_5000_PCIE_1	0x25e2		/* 5000 Series Chipset PCI Express x4 Port 2 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_2	0x25e3		/* 5000 Series Chipset PCI Express x4 Port 3 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_3	0x25e4		/* 5000 Series Chipset PCI Express x4 Port 4 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_4	0x25e5		/* 5000 Series Chipset PCI Express x4 Port 5 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_5	0x25e6		/* 5000 Series Chipset PCI Express x4 Port 6 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_6	0x25e7		/* 5000 Series Chipset PCI Express x4 Port 7 */
#define	PCI_PRODUCT_INTEL_5000_FSB_REG	0x25f0		/* 5000 Series Chipset FSB Registers */
#define	PCI_PRODUCT_INTEL_5000_RESERVED_1	0x25f1		/* 5000 Series Chipset Reserved Registers */
#define	PCI_PRODUCT_INTEL_5000_RESERVED_2	0x25f3		/* 5000 Series Chipset Reserved Registers */
#define	PCI_PRODUCT_INTEL_5000_FBD_1	0x25f5		/* 5000 Series Chipset FBD Registers */
#define	PCI_PRODUCT_INTEL_5000_FBD_2	0x25f6		/* 5000 Series Chipset FBD Registers */
#define	PCI_PRODUCT_INTEL_5000_PCIE_7	0x25f7		/* 5000 Series Chipset PCI Express x8 Port 2-3 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_8	0x25f8		/* 5000 Series Chipset PCI Express x8 Port 4-5 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_9	0x25f9		/* 5000 Series Chipset PCI Express x8 Port 6-7 */
#define	PCI_PRODUCT_INTEL_5000X_PCIE	0x25fa		/* 5000X PCI Express x16 Port 4-7 */
#define	PCI_PRODUCT_INTEL_82801FB_LPC	0x2640		/* 82801FB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801FBM_LPC	0x2641		/* 82801FBM ICH6M LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801FB_SATA	0x2651		/* 82801FB Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82801FR_SATA	0x2652		/* 82801FR Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82801FBM_SATA	0x2653		/* 82801FBM Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82801FB_USB_0	0x2658		/* 82801FB/FR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_USB_1	0x2659		/* 82801FB/FR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_USB_2	0x265a		/* 82801FB/FR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_USB_3	0x265b		/* 82801FB/FR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_EHCI	0x265c		/* 82801FB/FR USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801FB_EXP_0	0x2660		/* 82801FB/FR PCI Express Port #0 */
#define	PCI_PRODUCT_INTEL_82801FB_EXP_1	0x2662		/* 82801FB/FR PCI Express Port #1 */
#define	PCI_PRODUCT_INTEL_82801FB_EXP_2	0x2664		/* 82801FB/FR PCI Express Port #2 */
#define	PCI_PRODUCT_INTEL_82801FB_HDA	0x2668		/* 82801FB/FR High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_82801FB_SMB	0x266a		/* 82801FB/FR SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801FB_LAN	0x266c		/* 82801FB LAN Controller */
#define	PCI_PRODUCT_INTEL_82801FB_ACM	0x266d		/* 82801FB/FR AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801FB_AC	0x266e		/* 82801FB/FR AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801FB_IDE	0x266f		/* 82801FB/FR IDE Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_LPC	0x2670		/* 63xxESB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_63XXESB_SATA	0x2680		/* 63xxESB Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_SATA_AHCI	0x2681		/* 63xxESB AHCI Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_USB_0	0x2688		/* 63xxESB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_USB_1	0x2689		/* 63xxESB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_USB_2	0x268a		/* 63xxESB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_USB_3	0x268b		/* 63xxESB USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_EHCI	0x268c		/* 63xxESB USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_EXP_1	0x2690		/* 63xxESB PCI Express Port #1 */
#define	PCI_PRODUCT_INTEL_63XXESB_EXP_2	0x2692		/* 63xxESB PCI Express Port #2 */
#define	PCI_PRODUCT_INTEL_63XXESB_EXP_3	0x2694		/* 63xxESB PCI Express Port #3 */
#define	PCI_PRODUCT_INTEL_63XXESB_EXP_4	0x2696		/* 63xxESB PCI Express Port #4 */
#define	PCI_PRODUCT_INTEL_63XXESB_ACA	0x2698		/* 63xxESB AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_HDA	0x269a		/* 63xxESB High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_SMB	0x269b		/* 63xxESB SMBus Controller */
#define	PCI_PRODUCT_INTEL_63XXESB_IDE	0x269e		/* 63xxESB IDE Controller */
#define	PCI_PRODUCT_INTEL_82945P_MCH	0x2770		/* 82945G/P Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82945P_EXP	0x2771		/* 82945G/P PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82945P_IGD	0x2772		/* 82945G/P Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82955X_HB	0x2774		/* 82955X Host */
#define	PCI_PRODUCT_INTEL_82955X_EXP	0x2775		/* 82955X PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_E7230_HB	0x2778		/* E7230 Host */
#define	PCI_PRODUCT_INTEL_E7230_EXP	0x2779		/* E7230 PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82975X_EXP_2	0x277a		/* 82975X PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82975X_HB	0x277c		/* 82975X Host */
#define	PCI_PRODUCT_INTEL_82975X_EXP	0x277d		/* 82975X PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82915G_IGDC	0x2782		/* 82915G/GL IGD Companion */
#define	PCI_PRODUCT_INTEL_82915GM_IGDC	0x2792		/* 82915GM/GMS IGD Companion */
#define	PCI_PRODUCT_INTEL_82945GM_HB	0x27a0		/* 82945GM/PM/GMS Host Bridge */
#define	PCI_PRODUCT_INTEL_82945GM_IGD	0x27a2		/* 82945GM/PM/GMS Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82945GM_IGD_1	0x27a6		/* 82945GM/PM/GMS Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82945GME_HB	0x27ac		/* 82945GME Host Bridge */
#define	PCI_PRODUCT_INTEL_82945GME_IGD	0x27ae		/* 82945GME Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82801GH_LPC	0x27b0		/* 82801GH LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801G_LPC	0x27b8		/* 82801GB/GR LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801GBM_LPC	0x27b9		/* 82801GBM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_NM10_LPC	0x27bc		/* NM10 Family LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801GHM_LPC	0x27bd		/* 82801GHM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801G_SATA	0x27c0		/* 82801GB/GR SATA Controller */
#define	PCI_PRODUCT_INTEL_82801G_SATA_AHCI	0x27c1		/* 82801GB/GR AHCI SATA Controller */
#define	PCI_PRODUCT_INTEL_82801G_SATA_RAID	0x27c3		/* 82801GB/GR RAID SATA Controller */
#define	PCI_PRODUCT_INTEL_82801GBM_SATA	0x27c4		/* 82801GBM/GHM SATA Controller */
#define	PCI_PRODUCT_INTEL_82801GBM_AHCI	0x27c5		/* 82801GBM AHCI SATA Controller */
#define	PCI_PRODUCT_INTEL_82801GHM_RAID	0x27c6		/* 82801GHM SATA RAID Controller */
#define	PCI_PRODUCT_INTEL_82801G_USB_1	0x27c8		/* 82801GB/GR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_USB_2	0x27c9		/* 82801GB/GR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_USB_3	0x27ca		/* 82801GB/GR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_USB_4	0x27cb		/* 82801GB/GR USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_EHCI	0x27cc		/* 82801GB/GR USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801G_EXP_1	0x27d0		/* 82801GB/GR PCI Express Port #1 */
#define	PCI_PRODUCT_INTEL_82801G_EXP_2	0x27d2		/* 82801GB/GR PCI Express Port #2 */
#define	PCI_PRODUCT_INTEL_82801G_EXP_3	0x27d4		/* 82801GB/GR PCI Express Port #3 */
#define	PCI_PRODUCT_INTEL_82801G_EXP_4	0x27d6		/* 82801GB/GR PCI Express Port #4 */
#define	PCI_PRODUCT_INTEL_82801G_HDA	0x27d8		/* 82801GB/GR High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_82801G_SMB	0x27da		/* 82801GB/GR SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801G_LAN	0x27dc		/* 82801GB/GR LAN Controller */
#define	PCI_PRODUCT_INTEL_82801G_ACM	0x27dd		/* 82801GB/GR AC'97 Modem Controller */
#define	PCI_PRODUCT_INTEL_82801G_ACA	0x27de		/* 82801GB/GR AC'97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82801G_IDE	0x27df		/* 82801GB/GR IDE Controller */
#define	PCI_PRODUCT_INTEL_82801G_EXP_5	0x27e0		/* 82801GB/GR PCI Express Port #5 */
#define	PCI_PRODUCT_INTEL_82801G_EXP_6	0x27e2		/* 82801GB/GR PCI Express Port #6 */
#define	PCI_PRODUCT_INTEL_82801H_LPC	0x2810		/* 82801H LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801HEM_LPC	0x2811		/* 82801HEM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801HH_LPC	0x2812		/* 82801HH LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801HO_LPC	0x2814		/* 82801HO LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801HBM_LPC	0x2815		/* 82801HBM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801H_SATA_1	0x2820		/* 82801H SATA Controller */
#define	PCI_PRODUCT_INTEL_82801H_SATA_AHCI6	0x2821		/* 82801H AHCI SATA Controller w/ 6 ports */
#define	PCI_PRODUCT_INTEL_82801H_SATA_RAID	0x2822		/* 82801H RAID SATA Controller */
#define	PCI_PRODUCT_INTEL_82801H_SATA_AHCI4	0x2824		/* 82801H AHCI SATA Controller w/ 4 ports */
#define	PCI_PRODUCT_INTEL_82801H_SATA_2	0x2825		/* 82801H SATA Controller */
#define	PCI_PRODUCT_INTEL_82801HEM_SATA	0x2828		/* 82801HEM SATA Controller */
#define	PCI_PRODUCT_INTEL_82801HBM_SATA_1	0x2829		/* 82801HBM SATA Controller */
#define	PCI_PRODUCT_INTEL_82801HBM_SATA_2	0x282a		/* 82081HBM SATA Controller */
#define	PCI_PRODUCT_INTEL_82801H_USB_1	0x2830		/* 82801H USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801H_USB_2	0x2831		/* 82801H USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801H_USB_3	0x2832		/* 82801H USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801H_USB_4	0x2834		/* 82801H USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801H_USB_5	0x2835		/* 82801H USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801H_EHCI_1	0x2836		/* 82801H USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801H_EHCI_2	0x283a		/* 82801H USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801H_SMB	0x283e		/* 82801H SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801H_EXP_1	0x283f		/* 82801H PCI Express Port #1 */
#define	PCI_PRODUCT_INTEL_82801H_EXP_2	0x2841		/* 82801H PCI Express Port #2 */
#define	PCI_PRODUCT_INTEL_82801H_EXP_3	0x2843		/* 82801H PCI Express Port #3 */
#define	PCI_PRODUCT_INTEL_82801H_EXP_4	0x2845		/* 82801H PCI Express Port #4 */
#define	PCI_PRODUCT_INTEL_82801H_EXP_5	0x2847		/* 82801H PCI Express Port #5 */
#define	PCI_PRODUCT_INTEL_82801H_EXP_6	0x2849		/* 82801H PCI Express Port #6 */
#define	PCI_PRODUCT_INTEL_82801H_HDA	0x284b		/* 82801H High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_82801H_THERMAL	0x284f		/* 82801H Thermal Controller */
#define	PCI_PRODUCT_INTEL_82801HBM_IDE	0x2850		/* 82801H IDE Controller */
#define	PCI_PRODUCT_INTEL_82801IH_LPC	0x2912		/* 82801IH LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801IO_LPC	0x2914		/* 82801IO LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801IR_LPC	0x2916		/* 82801IR LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801IEM_LPC	0x2917		/* 82801IEM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801IB_LPC	0x2918		/* 82801IB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801IM_LPC	0x2919		/* 82801IM LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801I_SATA_1	0x2920		/* 82801I SATA Controller w/ 4 ports */
#define	PCI_PRODUCT_INTEL_82801I_SATA_2	0x2921		/* 82801I SATA Controller w/ 2 ports */
#define	PCI_PRODUCT_INTEL_82801I_SATA_AHCI6	0x2922		/* 82801I AHCI SATA Controller w/ 6 ports */
#define	PCI_PRODUCT_INTEL_82801I_SATA_AHCI4	0x2923		/* 82801I AHCI SATA Controller w/ 4 ports */
#define	PCI_PRODUCT_INTEL_82801I_SATA_3	0x2926		/* 82801I SATA Controller w/ 2 ports */
#define	PCI_PRODUCT_INTEL_82801I_SATA_4	0x2928		/* 82801I Mobile AHCI SATA Controller with 2 ports */
#define	PCI_PRODUCT_INTEL_82801I_SATA_5	0x2929		/* 82801I Mobile AHCI SATA Controller with 4 ports */
#define	PCI_PRODUCT_INTEL_82801I_SATA_6	0x292d		/* 82801I Mobile AHCI SATA Controller with 2 ports */
#define	PCI_PRODUCT_INTEL_82801I_SATA_7	0x292e		/* 82801I Mobile AHCI SATA Controller */
#define	PCI_PRODUCT_INTEL_82801I_SMB	0x2930		/* 82801I SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801I_THERMAL	0x2932		/* 82801I Thermal Controller */
#define	PCI_PRODUCT_INTEL_82801I_USB_1	0x2934		/* 82801I USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801I_USB_2	0x2935		/* 82801I USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801I_USB_3	0x2936		/* 82801I USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801I_USB_4	0x2937		/* 82801I USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801I_USB_5	0x2938		/* 82801I USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801I_USB_6	0x2939		/* 82801I USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801I_EHCI_1	0x293a		/* 82801I USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801I_EHCI_2	0x293c		/* 82801I USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801I_HDA	0x293e		/* 82801I High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_82801I_EXP_1	0x2940		/* 82801I PCI Express Port #1 */
#define	PCI_PRODUCT_INTEL_82801I_EXP_2	0x2942		/* 82801I PCI Express Port #2 */
#define	PCI_PRODUCT_INTEL_82801I_EXP_3	0x2944		/* 82801I PCI Express Port #3 */
#define	PCI_PRODUCT_INTEL_82801I_EXP_4	0x2946		/* 82801I PCI Express Port #4 */
#define	PCI_PRODUCT_INTEL_82801I_EXP_5	0x2948		/* 82801I PCI Express Port #5 */
#define	PCI_PRODUCT_INTEL_82801I_EXP_6	0x294a		/* 82801I PCI Express Port #6 */
#define	PCI_PRODUCT_INTEL_82801I_IGP_C	0x294c		/* 82801I (C) LAN Controller */
#define	PCI_PRODUCT_INTEL_82946GZ_HB	0x2970		/* 82946GZ Host Bridge */
#define	PCI_PRODUCT_INTEL_82946GZ_IGD	0x2972		/* 82946GZ Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82946GZ_KT	0x2977		/* 82946GZ KT */
#define	PCI_PRODUCT_INTEL_82G35_HB	0x2980		/* 82G35 Host Bridge */
#define	PCI_PRODUCT_INTEL_82G35_IGD	0x2982		/* 82G35 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82G35_IGD_1	0x2983		/* 82G35 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82965Q_HB	0x2990		/* 82965Q Host Bridge */
#define	PCI_PRODUCT_INTEL_82965Q_EXP	0x2991		/* 82965Q PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82965Q_IGD	0x2992		/* 82965Q Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82965Q_IGD_1	0x2993		/* 82965Q Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82Q965_KT	0x2997		/* 82Q965 KT */
#define	PCI_PRODUCT_INTEL_82965G_HB	0x29a0		/* 82965G Host Bridge */
#define	PCI_PRODUCT_INTEL_82965G_EXP	0x29a1		/* 82965G PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82965G_IGD	0x29a2		/* 82965G Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82965G_IGD_1	0x29a3		/* 82965G Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82P965_KT	0x29a7		/* 82P965/G965 KT */
#define	PCI_PRODUCT_INTEL_82Q35_HB	0x29b0		/* 82Q35 Host Bridge */
#define	PCI_PRODUCT_INTEL_82Q35_EXP	0x29b1		/* 82Q35 PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82Q35_IGD	0x29b2		/* 82Q35 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82Q35_IGD_1	0x29b3		/* 82Q35 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82Q35_KT	0x29b7		/* 82Q35 KT */
#define	PCI_PRODUCT_INTEL_82G33_HB	0x29c0		/* 82G33/P35 Host Bridge */
#define	PCI_PRODUCT_INTEL_82G33_EXP	0x29c1		/* 82G33 PCI Express Port */
#define	PCI_PRODUCT_INTEL_82G33_IGD	0x29c2		/* 82G33 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82G33_IGD_1	0x29c3		/* 82G33 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82801I_LAN	0x29c4		/* 82801I LAN Controller */
#define	PCI_PRODUCT_INTEL_82G33_KT	0x29c7		/* 82G33/G31/P35/P31 KT */
#define	PCI_PRODUCT_INTEL_82Q33_HB	0x29d0		/* 82Q35 Host Bridge */
#define	PCI_PRODUCT_INTEL_82Q33_EXP	0x29d1		/* 82Q35 PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82Q33_IGD	0x29d2		/* 82Q35 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82Q33_IGD_1	0x29d3		/* 82Q35 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82Q33_KT	0x29d7		/* 82Q33 KT */
#define	PCI_PRODUCT_INTEL_82X38_HB	0x29e0		/* 82X38 Host Bridge */
#define	PCI_PRODUCT_INTEL_82X38_PCIE_1	0x29e1		/* 82X38 Host-Primary PCIe Bridge */
#define	PCI_PRODUCT_INTEL_82X38_HECI	0x29e4		/* 82X38 HECI */
#define	PCI_PRODUCT_INTEL_82X38_KT	0x29e7		/* 82X38 KT */
#define	PCI_PRODUCT_INTEL_82X38_PCIE_2	0x29e9		/* 82X38 Host-Secondary PCIe Bridge */
#define	PCI_PRODUCT_INTEL_3200_HB	0x29f0		/* 3200/3210 Host */
#define	PCI_PRODUCT_INTEL_3200_PCIE	0x29f1		/* 3200/3210 PCIE */
#define	PCI_PRODUCT_INTEL_3200_KT	0x29f7		/* 3200 KT */
#define	PCI_PRODUCT_INTEL_82965PM_HB	0x2a00		/* 82965PM Host Bridge */
#define	PCI_PRODUCT_INTEL_80862A01	0x2a01		/* 80862A01 Mobile PCI Express Root Port */
#define	PCI_PRODUCT_INTEL_82965PM_IGD	0x2a02		/* 82965PM Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82965PM_IGD_1	0x2a03		/* 82965PM Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82965PM_MEI	0x2a04		/* 82965PM MEI Controller */
#define	PCI_PRODUCT_INTEL_82965PM_IDE	0x2a06		/* 82965PM IDE Interface */
#define	PCI_PRODUCT_INTEL_82965PM_KT	0x2a07		/* 82965PM/GM KT */
#define	PCI_PRODUCT_INTEL_82965GME_HB	0x2a10		/* 82965GME Host Bridge */
#define	PCI_PRODUCT_INTEL_82965GME_IGD	0x2a12		/* 82965GME Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82965GME_KT	0x2a17		/* 82965GME KT */
#define	PCI_PRODUCT_INTEL_82GM45_HB	0x2a40		/* 82GM45 Host Bridge */
#define	PCI_PRODUCT_INTEL_82GM45_IGD	0x2a42		/* 82GM45 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82GM45_IGD_1	0x2a43		/* 82GM45 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82GM45_KT	0x2a47		/* 82GM45 KT */
#define	PCI_PRODUCT_INTEL_82IGD_E_HB	0x2e00		/* 82IGD_E Host Bridge */
#define	PCI_PRODUCT_INTEL_82IGD_E_IGD	0x2e02		/* 82IGD_E Integrated Graphics */
#define	PCI_PRODUCT_INTEL_82Q45_KT	0x2e07		/* 82Q45 KT */
#define	PCI_PRODUCT_INTEL_82Q45_HB	0x2e10		/* 82Q45 Host Bridge */
#define	PCI_PRODUCT_INTEL_82Q45_EXP	0x2e11		/* 82Q45 PCI Express Bridge */
#define	PCI_PRODUCT_INTEL_82Q45_IGD	0x2e12		/* 82Q45 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82Q45_IGD_1	0x2e13		/* 82Q45 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82Q45_KT_1	0x2e17		/* 82Q45 KT */
#define	PCI_PRODUCT_INTEL_82G45_HB	0x2e20		/* 82G45 Host Bridge */
#define	PCI_PRODUCT_INTEL_82G45_IGD	0x2e22		/* 82G45 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82G41_HB	0x2e30		/* 82G41 Host Bridge */
#define	PCI_PRODUCT_INTEL_82G41_IGD	0x2e32		/* 82G41 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82B43_HB	0x2e40		/* 82B43 Host Bridge */
#define	PCI_PRODUCT_INTEL_82B43_IGD	0x2e42		/* 82B43 Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_XE5_V3_DMI2	0x2f00		/* Xeon E5 v3 DMI2 */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_1	0x2f01		/* Xeon E5 v3 PCIe Root Port in DMI2 Mode */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_2_1	0x2f04		/* Xeon E5 v3 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_2_2	0x2f05		/* Xeon E5 v3 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_2_3	0x2f06		/* Xeon E5 v3 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_2_4	0x2f07		/* Xeon E5 v3 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_3_1	0x2f08		/* Xeon E5 v3 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_3_2	0x2f09		/* Xeon E5 v3 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_3_3	0x2f0a		/* Xeon E5 v3 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCIE_3_4	0x2f0b		/* Xeon E5 v3 PCIe Root Port */
#define	PCI_PRODUCT_INTEL_XE5_V3_R2PCIE_1	0x2f1d		/* Xeon E5 v3 Integrated IO Ring Interface */
#define	PCI_PRODUCT_INTEL_XE5_V3_UBOX_1	0x2f1e		/* Xeon E5 v3 Scratchpad and Semaphores */
#define	PCI_PRODUCT_INTEL_XE5_V3_UBOX_3	0x2f1f		/* Xeon E5 v3 Scratchpad and Semaphores */
#define	PCI_PRODUCT_INTEL_XE5_V3_QDT_CH0	0x2f20		/* Xeon E5 v3 QDT DMA Channel 0 */
#define	PCI_PRODUCT_INTEL_XE5_V3_QDT_CH1	0x2f21		/* Xeon E5 v3 QDT DMA Channel 1 */
#define	PCI_PRODUCT_INTEL_XE5_V3_QDT_CH2	0x2f22		/* Xeon E5 v3 QDT DMA Channel 2 */
#define	PCI_PRODUCT_INTEL_XE5_V3_QDT_CH3	0x2f23		/* Xeon E5 v3 QDT DMA Channel 3 */
#define	PCI_PRODUCT_INTEL_XE5_V3_QDT_CH4	0x2f24		/* Xeon E5 v3 QDT DMA Channel 4 */
#define	PCI_PRODUCT_INTEL_XE5_V3_QDT_CH5	0x2f25		/* Xeon E5 v3 QDT DMA Channel 5 */
#define	PCI_PRODUCT_INTEL_XE5_V3_QDT_CH6	0x2f26		/* Xeon E5 v3 QDT DMA Channel 6 */
#define	PCI_PRODUCT_INTEL_XE5_V3_QDT_CH7	0x2f27		/* Xeon E5 v3 QDT DMA Channel 7 */
#define	PCI_PRODUCT_INTEL_XE5_V3_IIO_AM	0x2f28		/* Xeon E5 v3 Address Map, VTd, SMM */
#define	PCI_PRODUCT_INTEL_XE5_V3_IIO_RAM	0x2f2a		/* Xeon E5 v3 RAS, CS, Global Errors */
#define	PCI_PRODUCT_INTEL_XE5_V3_IIO_IOAPIC	0x2f2c		/* Xeon E5 v3 I/O APIC */
#define	PCI_PRODUCT_INTEL_XE5_V3_R2PCIE_2	0x2f34		/* Xeon E5 v3 PCIe Ring Performance Monitoring */
#define	PCI_PRODUCT_INTEL_XE5_V3_RQPI_PM_1	0x2f36		/* Xeon E5 v3 QPI Ring Performance Monitoring */
#define	PCI_PRODUCT_INTEL_XE5_V3_RQPI_PM_2	0x2f37		/* Xeon E5 v3 QPI Ring Interface Monitoring */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_MAIN	0x2f68		/* Xeon E5 v3 IMC Main */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_TADR1	0x2f6a		/* Xeon E5 v3 IMC Ch 0-1 Target Address Decode Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_TADR2	0x2f6b		/* Xeon E5 v3 IMC Ch 0-1 Target Address Decode Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_RAS	0x2f71		/* Xeon E5 v3 IMC RAS Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_RAS	0x2f79		/* Xeon E5 v3 IMC Ras Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_UBOX_2	0x2f7d		/* Xeon E5 v3 Scratchpad and Semaphores */
#define	PCI_PRODUCT_INTEL_XE5_V3_QPI_LINK0	0x2f80		/* Xeon E5 v3 QPI Link 0 */
#define	PCI_PRODUCT_INTEL_XE5_V3_RQPI_RING	0x2f81		/* Xeon E5 v3 QPI Ring Interface */
#define	PCI_PRODUCT_INTEL_XE5_V3_QPI_LINK1	0x2f90		/* Xeon E5 v3 QPI Link 1 */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCU_1	0x2f98		/* Xeon E5 v3 Power Control Unit */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCU_2	0x2f99		/* Xeon E5 v3 Power Control Unit */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCU_3	0x2f9a		/* Xeon E5 v3 Power Control Unit */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCU_5	0x2f9c		/* Xeon E5 v3 Power Control Unit */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_MAIN	0x2fa8		/* Xeon E5 v3 IMC Main */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_TADR1	0x2faa		/* Xeon E5 v3 IMC Ch 0-1 Target Address Decode Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_TADR2	0x2fab		/* Xeon E5 v3 IMC Ch 0-1 Target Address Decode Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_TADR3	0x2fac		/* Xeon E5 v3 IMC Ch 2-3 Target Address Decode Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_TADR4	0x2fad		/* Xeon E5 v3 IMC Ch 2-3 Target Address Decode Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_REG1	0x2fb0		/* Xeon E5 v3 IMC Ch 0-1 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_REG2	0x2fb1		/* Xeon E5 v3 IMC Ch 0-1 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_REG3	0x2fb2		/* Xeon E5 v3 IMC Ch 2-3 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_REG4	0x2fb3		/* Xeon E5 v3 IMC Ch 2-3 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_REG5	0x2fb4		/* Xeon E5 v3 IMC Ch 0-1 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_REG6	0x2fb5		/* Xeon E5 v3 IMC Ch 0-1 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_REG7	0x2fb6		/* Xeon E5 v3 IMC Ch 2-3 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_REG8	0x2fb7		/* Xeon E5 v3 IMC Ch 2-3 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_DDRIO_1	0x2fba		/* Xeon E5 v3 IMC DDRIO Multicast */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_DDRIO_2	0x2fbb		/* Xeon E5 v3 IMC DDRIO Multicast */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_DDRIO_5	0x2fbc		/* Xeon E5 v3 IMC DDRIO */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_DDRIO_6	0x2fbd		/* Xeon E5 v3 IMC DDRIO */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_DDRIO_7	0x2fbe		/* Xeon E5 v3 IMC DDRIO Multicast */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_DDRIO_8	0x2fbf		/* Xeon E5 v3 IMC DDRIO Multicast */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_DDRIO_3	0x2fbe		/* Xeon E5 v3 IMC DDRIO Multicast */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_DDRIO_4	0x2fbf		/* Xeon E5 v3 IMC DDRIO Multicast */
#define	PCI_PRODUCT_INTEL_XE5_V3_PCU_4	0x2fc0		/* Xeon E5 v3 Power Control Unit */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_REG5	0x2fd4		/* Xeon E5 v3 IMC Ch 0-1 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_REG6	0x2fd5		/* Xeon E5 v3 IMC Ch 0-1 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_REG7	0x2fd6		/* Xeon E5 v3 IMC Ch 0-1 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC1_REG8	0x2fd7		/* Xeon E5 v3 IMC Ch 0-1 Registers */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_DDRIO_9	0x2fd8		/* Xeon E5 v3 IMC DDRIO */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_DDRIO_A	0x2fd9		/* Xeon E5 v3 IMC DDRIO */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_DDRIO_B	0x2fda		/* Xeon E5 v3 IMC DDRIO */
#define	PCI_PRODUCT_INTEL_XE5_V3_IMC0_DDRIO_C	0x2fdb		/* Xeon E5 v3 IMC DDRIO */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_3165_1	0x3165		/* Dual Band Wireless AC 3165 */
#define	PCI_PRODUCT_INTEL_31244	0x3200		/* 31244 Serial ATA Controller */
#define	PCI_PRODUCT_INTEL_82855PM_DDR	0x3340		/* 82855PM MCH Host Controller */
#define	PCI_PRODUCT_INTEL_82855PM_AGP	0x3341		/* 82855PM Host-AGP Bridge */
#define	PCI_PRODUCT_INTEL_82855PM_PM	0x3342		/* 82855PM Power Management Controller */
#define	PCI_PRODUCT_INTEL_3400_HB	0x3403		/* X58 DMI port */
#define	PCI_PRODUCT_INTEL_5500_HB	0x3403		/* 5500 ESI Port */
#define	PCI_PRODUCT_INTEL_82X58_HB	0x3405		/* X58 Host */
#define	PCI_PRODUCT_INTEL_825520_HB	0x3406		/* 5520 ESI Port */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_1	0x3408		/* 5520/5500/X58 PCIE Root Port 1 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_2	0x3409		/* 5520/5500/X58 PCIE Root Port 2 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_3	0x340a		/* 5520/5500/X58 PCIE Root Port 3 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_4	0x340b		/* 5520/5500/X58 PCIE Root Port 4 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_5	0x340c		/* 5520/5500/X58 PCIE Root Port 5 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_6	0x340d		/* 5520/5500/X58 PCIE Root Port 6 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_7	0x340e		/* 5520/5500/X58 PCIE Root Port 7 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_8	0x340f		/* 5520/5500/X58 PCIE Root Port 8 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_9	0x3410		/* 5520/5500/X58 PCIE Root Port 9 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_10	0x3411		/* 5520/5500/X58 PCIE Root Port 10 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_0_0	0x3420		/* 5520/5500/X58 PCIE Root Port 0 */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_0_1	0x3420		/* 5520/5500/X58 PCIE Root Port 0 */
#define	PCI_PRODUCT_INTEL_82X58_GPIO	0x3422		/* 5520/5500/X58 Scratchpads and GPIO */
#define	PCI_PRODUCT_INTEL_82X58_RAS	0x3423		/* 5520/5500/X58 Control Status and RAS */
#define	PCI_PRODUCT_INTEL_82X58_QP0_P0	0x3425		/* 5520/5500/X58 QuickPath Port 0 */
#define	PCI_PRODUCT_INTEL_82X58_QP0_P1	0x3426		/* 5520/5500/X58 QuickPath Port 0 */
#define	PCI_PRODUCT_INTEL_82X58_QP1_P0	0x3427		/* 5520/5500/X58 QuickPath Port 1 */
#define	PCI_PRODUCT_INTEL_82X58_QP1_P1	0x3428		/* 5520/5500/X58 QuickPath Port 1 */
#define	PCI_PRODUCT_INTEL_82X58_IOXAPIC	0x342d		/* 5520/5500/X58 IOxAPIC */
#define	PCI_PRODUCT_INTEL_82X58_MISC	0x342e		/* 5520/5500/X58 Misc */
#define	PCI_PRODUCT_INTEL_82X58_THROTTLE	0x3438		/* 5520/5500/X58 Throttling */
#define	PCI_PRODUCT_INTEL_63XXESB_EXP_UP	0x3500		/* 63xxESB PCI Express Upstream Port */
#define	PCI_PRODUCT_INTEL_63XXESB_PCIX	0x350c		/* 63xxESB PCI Express to PCI-X Bridge */
#define	PCI_PRODUCT_INTEL_63XXESB_EXP_DN_1	0x3510		/* 63xxESB PCI Express Downstream Port #1 */
#define	PCI_PRODUCT_INTEL_63XXESB_EXP_DN_2	0x3514		/* 63xxESB PCI Express Downstream Port #2 */
#define	PCI_PRODUCT_INTEL_63XXESB_EXP_DN_3	0x3518		/* 63xxESB PCI Express Downstream Port #3 */
#define	PCI_PRODUCT_INTEL_82830MP_IO_1	0x3575		/* 82830MP CPU to I/O Bridge 1 */
#define	PCI_PRODUCT_INTEL_82830MP_AGP	0x3576		/* 82830MP CPU to AGP Bridge */
#define	PCI_PRODUCT_INTEL_82830MP_IV	0x3577		/* 82830MP Integrated Video */
#define	PCI_PRODUCT_INTEL_82830MP_IO_2	0x3578		/* 82830MP CPU to I/O Bridge 2 */
#define	PCI_PRODUCT_INTEL_82855GM_MCH	0x3580		/* 82855GM Host-Hub Controller */
#define	PCI_PRODUCT_INTEL_82855GM_AGP	0x3581		/* 82855GM Host-AGP Bridge */
#define	PCI_PRODUCT_INTEL_82855GM_IGD	0x3582		/* 82855GM GMCH Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_82855GM_MC	0x3584		/* 82855GM GMCH Memory Controller */
#define	PCI_PRODUCT_INTEL_82855GM_CP	0x3585		/* 82855GM GMCH Configuration Process */
#define	PCI_PRODUCT_INTEL_E7525_MCH	0x3590		/* E7525 Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_E7525_MCHER	0x3591		/* E7525 Error Reporting Device */
#define	PCI_PRODUCT_INTEL_E7520_DMA	0x3594		/* E7520 DMA Controller */
#define	PCI_PRODUCT_INTEL_E7525_PCIE_A	0x3595		/* E7525 PCI Express Port A */
#define	PCI_PRODUCT_INTEL_E7525_PCIE_A1	0x3596		/* E7525 PCI Express Port A1 */
#define	PCI_PRODUCT_INTEL_E7525_PCIE_B	0x3597		/* E7525 PCI Express Port B */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_B1	0x3598		/* E7520 PCI Express Port B1 */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_C	0x3599		/* E7520 PCI Express Port C */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_C1	0x359a		/* E7520 PCI Express Port C1 */
#define	PCI_PRODUCT_INTEL_E7520_CFG	0x359b		/* E7520 Extended Configuration */
#define	PCI_PRODUCT_INTEL_82801JD_SATA_IDE	0x3a00		/* 82801JD SATA Controller (IDE mode) */
#define	PCI_PRODUCT_INTEL_82801JD_SATA_AHCI	0x3a02		/* 82801JD SATA Controller (AHCI mode) */
#define	PCI_PRODUCT_INTEL_82801JD_SATA_RAID	0x3a02		/* 82801JD SATA Controller (RAID mode) */
#define	PCI_PRODUCT_INTEL_82801JD_SATA_IDE2	0x3a06		/* 82801JD SATA Controller (IDE mode) */
#define	PCI_PRODUCT_INTEL_82801JDO_LPC	0x3a14		/* 82801JDO LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801JIR_LPC	0x3a16		/* 82801JIR LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801JIB_LPC	0x3a18		/* 82801JIB LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801JD_LPC	0x3a1a		/* 82801JD LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_82801JI_SATA_IDE	0x3a20		/* 82801JI SATA Controller (IDE mode) */
#define	PCI_PRODUCT_INTEL_82801JI_SATA_AHCI	0x3a22		/* 82801JI SATA Controller (AHCI mode) */
#define	PCI_PRODUCT_INTEL_82801JI_SATA_RAID	0x3a25		/* 82801JI SATA Controller (RAID mode) */
#define	PCI_PRODUCT_INTEL_82801JI_SATA_IDE2	0x3a26		/* 82801JI SATA Controller (IDE mode) */
#define	PCI_PRODUCT_INTEL_82801JI_SMB	0x3a30		/* 82801JI SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801JI_THERMAL	0x3a30		/* 82801JI Thermal Controller */
#define	PCI_PRODUCT_INTEL_82801JI_USB_1	0x3a34		/* 82801JI USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JI_USB_2	0x3a35		/* 82801JI USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JI_USB_3	0x3a36		/* 82801JI USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JI_USB_4	0x3a37		/* 82801JI USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JI_USB_5	0x3a38		/* 82801JI USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JI_USB_6	0x3a39		/* 82801JI USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JI_EHCI_1	0x3a3a		/* 82801JI USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JI_EHCI_2	0x3a3c		/* 82801JI USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JI_HDA	0x3a3e		/* 82801JI High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_1	0x3a40		/* 82801JI PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_2	0x3a42		/* 82801JI PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_3	0x3a44		/* 82801JI PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_4	0x3a46		/* 82801JI PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_5	0x3a48		/* 82801JI PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_6	0x3a4a		/* 82801JI PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JI_LAN	0x3a4c		/* 82801JI LAN Controller */
#define	PCI_PRODUCT_INTEL_82801JD_SMB	0x3a60		/* 82801JD SMBus Controller */
#define	PCI_PRODUCT_INTEL_82801JD_THERMAL	0x3a62		/* 82801JD Thermal Controller */
#define	PCI_PRODUCT_INTEL_82801JD_USB_1	0x3a64		/* 82801JD USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JD_USB_2	0x3a65		/* 82801JD USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JD_USB_3	0x3a66		/* 82801JD USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JD_USB_4	0x3a67		/* 82801JD USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JD_USB_5	0x3a68		/* 82801JD USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JD_USB_6	0x3a69		/* 82801JD USB UHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JD_EHCI_1	0x3a6a		/* 82801JD USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JD_EHCI_2	0x3a6c		/* 82801JD USB EHCI Controller */
#define	PCI_PRODUCT_INTEL_82801JD_HDA	0x3a6e		/* 82801JD High Definition Audio Controller */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_1	0x3a70		/* 82801JD PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_2	0x3a72		/* 82801JD PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_3	0x3a74		/* 82801JD PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_4	0x3a76		/* 82801JD PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_5	0x3a78		/* 82801JD PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_6	0x3a7a		/* 82801JD PCI Express Port */
#define	PCI_PRODUCT_INTEL_82801JD_LAN	0x3a7c		/* 82801JD LAN Controller */
#define	PCI_PRODUCT_INTEL_P55_LPC	0x3b02		/* P55 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_PM55_LPC	0x3b03		/* PM55 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_H55_LPC	0x3b06		/* H55 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_QM57_LPC	0x3b07		/* QM57 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_H57_LPC	0x3b08		/* H57 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_HM55_LPC	0x3b09		/* HM55 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_Q57_LPC	0x3b0a		/* Q57 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_HM57_LPC	0x3b0b		/* HM57 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_QS57_LPC	0x3b0f		/* QS57 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_3400_LPC	0x3b12		/* 3400 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_3420_LPC	0x3b14		/* 3420 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_3450_LPC	0x3b16		/* 3450 LPC Interface Bridge */
#define	PCI_PRODUCT_INTEL_3400_SATA_1	0x3b20		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_SATA_2	0x3b21		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_AHCI_1	0x3b22		/* 3400 AHCI */
#define	PCI_PRODUCT_INTEL_3400_AHCI_2	0x3b23		/* 3400 AHCI */
#define	PCI_PRODUCT_INTEL_3400_RAID_1	0x3b25		/* 3400 RAID */
#define	PCI_PRODUCT_INTEL_3400_SATA_3	0x3b26		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_SATA_4	0x3b28		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_AHCI_3	0x3b29		/* 3400 AHCI */
#define	PCI_PRODUCT_INTEL_3400_RAID_2	0x3b2c		/* 3400 RAID */
#define	PCI_PRODUCT_INTEL_3400_SATA_5	0x3b2d		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_SATA_6	0x3b2e		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_AHCI_4	0x3b2f		/* 3400 AHCI */
#define	PCI_PRODUCT_INTEL_3400_SMB	0x3b30		/* 3400 SMBus */
#define	PCI_PRODUCT_INTEL_3400_THERMAL	0x3b32		/* 3400 Thermal */
#define	PCI_PRODUCT_INTEL_3400_EHCI_1	0x3b34		/* 3400 USB EHCI */
#define	PCI_PRODUCT_INTEL_3400_UHCI_1	0x3b36		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_2	0x3b37		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_3	0x3b38		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_4	0x3b39		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_5	0x3b3a		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_6	0x3b3b		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_EHCI_2	0x3b3c		/* 3400 USB ECHI */
#define	PCI_PRODUCT_INTEL_3400_UHCI_7	0x3b3e		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_8	0x3b3f		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_PCIE_1	0x3b42		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_2	0x3b44		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_3	0x3b46		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_4	0x3b48		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_5	0x3b4a		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_6	0x3b4c		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_7	0x3b4e		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_8	0x3b50		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_HDA	0x3b56		/* 3400 HD Audio */
#define	PCI_PRODUCT_INTEL_QS57_HDA	0x3b57		/* QS57 HD Audio */
#define	PCI_PRODUCT_INTEL_3400_MEI_1	0x3b64		/* 3400 MEI */
#define	PCI_PRODUCT_INTEL_3400_MEI_2	0x3b65		/* 3400 MEI */
#define	PCI_PRODUCT_INTEL_3400_PT_IDER	0x3b66		/* 3400 PT IDER */
#define	PCI_PRODUCT_INTEL_3400_KT	0x3b67		/* 3400 KT */
#define	PCI_PRODUCT_INTEL_E5_HB	0x3c00		/* E5 Host */
#define	PCI_PRODUCT_INTEL_E5_PCIE_1	0x3c02		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_2	0x3c03		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_3	0x3c04		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_4	0x3c05		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_5	0x3c06		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_6	0x3c07		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_7	0x3c08		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_8	0x3c09		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_9	0x3c0a		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_PCIE_10	0x3c0b		/* E5 PCIE */
#define	PCI_PRODUCT_INTEL_E5_DMA_1	0x3c20		/* E5 DMA */
#define	PCI_PRODUCT_INTEL_E5_DMA_2	0x3c21		/* E5 DMA */
#define	PCI_PRODUCT_INTEL_E5_DMA_3	0x3c22		/* E5 DMA */
#define	PCI_PRODUCT_INTEL_E5_DMA_4	0x3c23		/* E5 DMA */
#define	PCI_PRODUCT_INTEL_E5_DMA_5	0x3c24		/* E5 DMA */
#define	PCI_PRODUCT_INTEL_E5_DMA_6	0x3c25		/* E5 DMA */
#define	PCI_PRODUCT_INTEL_E5_DMA_7	0x3c26		/* E5 DMA */
#define	PCI_PRODUCT_INTEL_E5_DMA_8	0x3c27		/* E5 DMA */
#define	PCI_PRODUCT_INTEL_E5_ADDRMAP	0x3c28		/* E5 Address Map */
#define	PCI_PRODUCT_INTEL_E5_ERR	0x3c2a		/* E5 Error Reporting */
#define	PCI_PRODUCT_INTEL_E5_IOAPIC	0x3c2c		/* E5 I/O APIC */
#define	PCI_PRODUCT_INTEL_5400_HB	0x4000		/* 5400 Host */
#define	PCI_PRODUCT_INTEL_5400A_HB	0x4001		/* 5400A Host */
#define	PCI_PRODUCT_INTEL_5400B_HB	0x4003		/* 5400B Host */
#define	PCI_PRODUCT_INTEL_5400_PCIE_1	0x4021		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_5400_PCIE_2	0x4022		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_5400_PCIE_3	0x4023		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_5400_PCIE_4	0x4024		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_5400_PCIE_5	0x4025		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_5400_PCIE_6	0x4026		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_5400_PCIE_7	0x4027		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_5400_PCIE_8	0x4028		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_5400_PCIE_9	0x4029		/* 5400 PCIE */
#define	PCI_PRODUCT_INTEL_IOAT_SNB	0x402f		/* I/OAT SNB */
#define	PCI_PRODUCT_INTEL_5400_FSBINT	0x4030		/* 5400 FSB/Boot/Interrupt */
#define	PCI_PRODUCT_INTEL_5400_CE	0x4031		/* 5400 Coherency Engine */
#define	PCI_PRODUCT_INTEL_5400_IOAPIC	0x4032		/* 5400 IOAPIC */
#define	PCI_PRODUCT_INTEL_5400_RAS_0	0x4035		/* 5400 RAS */
#define	PCI_PRODUCT_INTEL_5400_RAS_1	0x4036		/* 5400 RAS */
#define	PCI_PRODUCT_INTEL_E600_VGA	0x4108		/* E600 Integrated VGA */
#define	PCI_PRODUCT_INTEL_E600_HB	0x4114		/* E600 Host */
#define	PCI_PRODUCT_INTEL_PRO_WL_2200BG	0x4220		/* PRO/Wireless LAN 2200BG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_PRO_WL_2225BG	0x4221		/* PRO/Wireless LAN 2225BG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_PRO_WL_3945ABG_1	0x4222		/* PRO/Wireless LAN 3945ABG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_PRO_WL_2915ABG_1	0x4223		/* PRO/Wireless LAN 2915ABG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_PRO_WL_2915ABG_2	0x4224		/* PRO/Wireless LAN 2915ABG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_PRO_WL_3945ABG_2	0x4227		/* PRO/Wireless LAN 3945ABG Mini-PCI Adapter */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_4965_1	0x4229		/* Wireless WiFi Link 4965 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6000_3X3_1	0x422b		/* Centrino Ultimate-N 6300 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6000_IPA_1	0x422c		/* Centrino Advanced-N 6200 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_4965_3	0x422d		/* Wireless WiFi Link 4965 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_4965_2	0x4230		/* Wireless WiFi Link 4965 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5100_1	0x4232		/* WiFi Link 5100 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_4965_4	0x4233		/* Wireless WiFi Link 4965 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5300_1	0x4235		/* WiFi Link 5300 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5300_2	0x4236		/* WiFi Link 5300 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5100_2	0x4237		/* WiFi Link 5100 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6000_3X3_2	0x4238		/* Centrino Ultimate-N 6300 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6000_IPA_2	0x4239		/* Centrino Advanced-N 6200 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5350_1	0x423a		/* WiFi Link 5350 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5350_2	0x423b		/* WiFi Link 5350 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5150_1	0x423c		/* WiFi Link 5150 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5150_2	0x423d		/* WiFi Link 5150 */
#define	PCI_PRODUCT_INTEL_EP80579_HB	0x5020		/* EP80579 Host */
#define	PCI_PRODUCT_INTEL_EP80579_MEM	0x5021		/* EP80579 Memory */
#define	PCI_PRODUCT_INTEL_EP80579_EDMA	0x5023		/* EP80579 EDMA */
#define	PCI_PRODUCT_INTEL_EP80579_PCIE_1	0x5024		/* EP80579 PCIE */
#define	PCI_PRODUCT_INTEL_EP80579_PCIE_2	0x5025		/* EP80579 PCIE */
#define	PCI_PRODUCT_INTEL_EP80579_SATA	0x5028		/* EP80579 SATA */
#define	PCI_PRODUCT_INTEL_EP80579_AHCI	0x5029		/* EP80579 AHCI */
#define	PCI_PRODUCT_INTEL_EP80579_ASU	0x502c		/* EP80579 ASU */
#define	PCI_PRODUCT_INTEL_EP80579_RESERVED1	0x5030		/* EP80579 Reserved */
#define	PCI_PRODUCT_INTEL_EP80579_LPC	0x5031		/* EP80579 LPC */
#define	PCI_PRODUCT_INTEL_EP80579_SMB	0x5032		/* EP80579 SMBus */
#define	PCI_PRODUCT_INTEL_EP80579_UHCI	0x5033		/* EP80579 USB */
#define	PCI_PRODUCT_INTEL_EP80579_EHCI	0x5035		/* EP80579 USB */
#define	PCI_PRODUCT_INTEL_EP80579_PPB	0x5037		/* EP80579 PCI-PCI bridge */
#define	PCI_PRODUCT_INTEL_EP80579_CAN_1	0x5039		/* EP80579 CANbus */
#define	PCI_PRODUCT_INTEL_EP80579_CAN_2	0x503a		/* EP80579 CANbus */
#define	PCI_PRODUCT_INTEL_EP80579_SERIAL	0x503b		/* EP80579 Serial */
#define	PCI_PRODUCT_INTEL_EP80579_1588	0x503c		/* EP80579 1588 */
#define	PCI_PRODUCT_INTEL_EP80579_LEB	0x503d		/* EP80579 LEB */
#define	PCI_PRODUCT_INTEL_EP80579_GCU	0x503e		/* EP80579 GCU */
#define	PCI_PRODUCT_INTEL_EP80579_RESERVED2	0x503f		/* EP80579 Reserved */
#define	PCI_PRODUCT_INTEL_EP80579_LAN_1	0x5040		/* EP80579 LAN */
#define	PCI_PRODUCT_INTEL_EP80579_LAN_2	0x5044		/* EP80579 LAN */
#define	PCI_PRODUCT_INTEL_EP80579_LAN_3	0x5048		/* EP80579 LAN */
#define	PCI_PRODUCT_INTEL_80312_ATU	0x530d		/* 80310 ATU */
#define	PCI_PRODUCT_INTEL_82371SB_ISA	0x7000		/* 82371SB (PIIX3) PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82371SB_IDE	0x7010		/* 82371SB (PIIX3) IDE Interface */
#define	PCI_PRODUCT_INTEL_82371SB_USB	0x7020		/* 82371SB (PIIX3) USB Host Controller */
#define	PCI_PRODUCT_INTEL_82437VX	0x7030		/* 82437VX (TVX) System Controller */
#define	PCI_PRODUCT_INTEL_82439TX	0x7100		/* 82439TX (MTXC) System Controller */
#define	PCI_PRODUCT_INTEL_82371AB_ISA	0x7110		/* 82371AB (PIIX4) PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82371AB_IDE	0x7111		/* 82371AB (PIIX4) IDE Controller */
#define	PCI_PRODUCT_INTEL_82371AB_USB	0x7112		/* 82371AB (PIIX4) USB Host Controller */
#define	PCI_PRODUCT_INTEL_82371AB_PMC	0x7113		/* 82371AB (PIIX4) Power Management Controller */
#define	PCI_PRODUCT_INTEL_82810_MCH	0x7120		/* 82810 Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810_GC	0x7121		/* 82810 Graphics Controller */
#define	PCI_PRODUCT_INTEL_82810_DC100_MCH	0x7122		/* 82810-DC100 Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810_DC100_GC	0x7123		/* 82810-DC100 Graphics Controller */
#define	PCI_PRODUCT_INTEL_82810E_MCH	0x7124		/* 82810E Memory Controller Hub */
#define	PCI_PRODUCT_INTEL_82810E_GC	0x7125		/* 82810E Graphics Controller */
#define	PCI_PRODUCT_INTEL_82443LX	0x7180		/* 82443LX PCI AGP Controller */
#define	PCI_PRODUCT_INTEL_82443LX_AGP	0x7181		/* 82443LX AGP Interface */
#define	PCI_PRODUCT_INTEL_82443BX	0x7190		/* 82443BX Host Bridge/Controller */
#define	PCI_PRODUCT_INTEL_82443BX_AGP	0x7191		/* 82443BX AGP Interface */
#define	PCI_PRODUCT_INTEL_82443BX_NOAGP	0x7192		/* 82443BX Host Bridge/Controller (AGP disabled) */
#define	PCI_PRODUCT_INTEL_82440MX	0x7194		/* 82443MX Host Bridge/Controller */
#define	PCI_PRODUCT_INTEL_82440MX_ACA	0x7195		/* 82443MX AC-97 Audio Controller */
#define	PCI_PRODUCT_INTEL_82440MX_ISA	0x7198		/* 82443MX PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_82440MX_IDE	0x7199		/* 82443MX IDE Controller */
#define	PCI_PRODUCT_INTEL_82440MX_USB	0x719a		/* 82443MX USB Host Controller */
#define	PCI_PRODUCT_INTEL_82440MX_PMC	0x719b		/* 82443MX Power Management Controller */
#define	PCI_PRODUCT_INTEL_82443GX	0x71a0		/* 82443GX Host Bridge/Controller */
#define	PCI_PRODUCT_INTEL_82443GX_AGP	0x71a1		/* 82443GX AGP Interface */
#define	PCI_PRODUCT_INTEL_82443GX_NOAGP	0x71a2		/* 82443GX Host Bridge/Controller (AGP disabled) */
#define	PCI_PRODUCT_INTEL_I740	0x7800		/* i740 Graphics Accelerator */
#define	PCI_PRODUCT_INTEL_SCH_IDE	0x811a		/* SCH IDE Controller */
#define	PCI_PRODUCT_INTEL_E600_HDA	0x811b		/* E600 HD Audio */
#define	PCI_PRODUCT_INTEL_E600_PCIB_0	0x8180		/* E600 Virtual PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_E600_PCIB_1	0x8181		/* E600 Virtual PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_E600_GVD	0x8182		/* E600 Integrated Graphic Video Display */
#define	PCI_PRODUCT_INTEL_E600_PCIB_2	0x8184		/* E600 Virtual PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_E600_PCIB_3	0x8185		/* E600 Virtual PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_E600_LPC	0x8186		/* Atom Processor E6xx LPC Bridge */
#define	PCI_PRODUCT_INTEL_PCI450_PB	0x84c4		/* 82454KX/GX PCI Bridge (PB) */
#define	PCI_PRODUCT_INTEL_PCI450_MC	0x84c5		/* 82451KX/GX Memory Controller (MC) */
#define	PCI_PRODUCT_INTEL_82451NX_MIOC	0x84ca		/* 82451NX Memory & I/O Controller (MIOC) */
#define	PCI_PRODUCT_INTEL_82451NX_PXB	0x84cb		/* 82451NX PCI Expander Bridge (PXB) */

#define	PCI_PRODUCT_INTEL_EG20T_PCIB	0x8800		/* EG20T PCH PCIExpress Bridge */
#define	PCI_PRODUCT_INTEL_EG20T_PCTHUB	0x8801		/* EG20T PCH Packet Hub */
#define	PCI_PRODUCT_INTEL_EG20T_GBE	0x8802		/* EG20T PCH Gigabit Ether */
#define	PCI_PRODUCT_INTEL_EG20T_GPIO	0x8803		/* EG20T PCH GPIO */
#define	PCI_PRODUCT_INTEL_EG20T_OHCI1_0	0x8804		/* EG20T PCH USB OHCI Host Controller #1 */
#define	PCI_PRODUCT_INTEL_EG20T_OHCI1_1	0x8805		/* EG20T PCH USB OHCI Host Controller #1 */
#define	PCI_PRODUCT_INTEL_EG20T_OHCI1_2	0x8806		/* EG20T PCH USB OHCI Host Controller #1 */
#define	PCI_PRODUCT_INTEL_EG20T_EHCI1	0x8807		/* EG20T PCH USB EHCI Host Controller #1 */
#define	PCI_PRODUCT_INTEL_EG20T_USB_DEV	0x8808		/* EG20T PCH USB Device */
#define	PCI_PRODUCT_INTEL_EG20T_SDIO_0	0x8809		/* EG20T PCH SDIO Controller #0 */
#define	PCI_PRODUCT_INTEL_EG20T_SDIO_1	0x880a		/* EG20T PCH SDIO Controller #1 */
#define	PCI_PRODUCT_INTEL_EG20T_AHCI	0x880b		/* EG20T PCH AHCI SATA Controller */
#define	PCI_PRODUCT_INTEL_EG20T_OHCI0_0	0x880c		/* EG20T PCH USB OHCI Host Controller #0 */
#define	PCI_PRODUCT_INTEL_EG20T_OHCI0_1	0x880d		/* EG20T PCH USB OHCI Host Controller #0 */
#define	PCI_PRODUCT_INTEL_EG20T_OHCI0_2	0x880e		/* EG20T PCH USB OHCI Host Controller #0 */
#define	PCI_PRODUCT_INTEL_EG20T_EHCI0	0x880f		/* EG20T PCH USB EHCI Host Controller #0 */
#define	PCI_PRODUCT_INTEL_EG20T_DMA_0	0x8810		/* EG20T PCH DMAC #0 */
#define	PCI_PRODUCT_INTEL_EG20T_UART_0	0x8811		/* EG20T PCH UART #0 */
#define	PCI_PRODUCT_INTEL_EG20T_UART_1	0x8812		/* EG20T PCH UART #1 */
#define	PCI_PRODUCT_INTEL_EG20T_UART_2	0x8813		/* EG20T PCH UART #2 */
#define	PCI_PRODUCT_INTEL_EG20T_UART_3	0x8814		/* EG20T PCH UART #3 */
#define	PCI_PRODUCT_INTEL_EG20T_DMA_1	0x8815		/* EG20T PCH DMAC #1 */
#define	PCI_PRODUCT_INTEL_EG20T_SPI	0x8816		/* EG20T PCH SPI */
#define	PCI_PRODUCT_INTEL_EG20T_I2C	0x8817		/* EG20T PCH I2C Interface */
#define	PCI_PRODUCT_INTEL_EG20T_CAN	0x8818		/* EG20T PCH CAN Controller */
#define	PCI_PRODUCT_INTEL_EG20T_IEEE1588	0x8819		/* EG20T PCH IEEE1588 */
#define	PCI_PRODUCT_INTEL_8SER_DT_SATA	0x8c00		/* 8 Series (desktop) SATA Controller */
#define	PCI_PRODUCT_INTEL_8SER_MO_SATA	0x8c01		/* 8 Series (mobile) SATA Controller */
#define	PCI_PRODUCT_INTEL_8SER_DT_SATA_AHCI	0x8c02		/* 8 Series (desktop) SATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_8SER_MO_SATA_AHCI	0x8c03		/* 8 Series (mobile) SATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_8SER_DT_SATA_RAID	0x8c04		/* 8 Series (desktop) SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_8SER_MO_SATA_RAID	0x8c05		/* 8 Series (mobile) SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_8SER_DT_SATA_RAID_SR	0x8c06		/* 8 Series (desktop) SATA Controller (RAID) + Smart Response */
#define	PCI_PRODUCT_INTEL_8SER_MO_SATA_RAID_SR	0x8c07		/* 8 Series (mobile) SATA Controller (RAID) + Smart Response */
#define	PCI_PRODUCT_INTEL_8SER_DT_SATA_2	0x8c08		/* 8 Series (desktop) SATA Controller */
#define	PCI_PRODUCT_INTEL_8SER_MO_SATA_2	0x8c09		/* 8 Series (mobile) SATA Controller */
#define	PCI_PRODUCT_INTEL_8SER_DT_SATA_RAID1	0x8c0e		/* 8 Series (desktop) SATA Controller (RAID1) */
#define	PCI_PRODUCT_INTEL_8SER_MO_SATA_RAID1	0x8c0f		/* 8 Series (mobile) SATA Controller (RAID1) */
#define	PCI_PRODUCT_INTEL_8SERIES_PCIE_1	0x8c10		/* 8 Series PCIE */
#define	PCI_PRODUCT_INTEL_8SERIES_PCIE_2	0x8c12		/* 8 Series PCIE */
#define	PCI_PRODUCT_INTEL_8SERIES_PCIE_3	0x8c14		/* 8 Series PCIE */
#define	PCI_PRODUCT_INTEL_8SERIES_PCIE_4	0x8c16		/* 8 Series PCIE */
#define	PCI_PRODUCT_INTEL_8SERIES_PCIE_5	0x8c18		/* 8 Series PCIE */
#define	PCI_PRODUCT_INTEL_8SERIES_PCIE_6	0x8c1a		/* 8 Series PCIE */
#define	PCI_PRODUCT_INTEL_8SERIES_PCIE_7	0x8c1c		/* 8 Series PCIE */
#define	PCI_PRODUCT_INTEL_8SERIES_PCIE_8	0x8c1e		/* 8 Series PCIE */
#define	PCI_PRODUCT_INTEL_8SERIES_HDA	0x8c20		/* 8 Series HD Audio */
#define	PCI_PRODUCT_INTEL_8SERIES_SMB	0x8c22		/* 8 Series SMBus Controller */
#define	PCI_PRODUCT_INTEL_8SERIES_THERM	0x8c24		/* 8 Series Thermal */
#define	PCI_PRODUCT_INTEL_8SERIES_EHCI_1	0x8c26		/* 8 Series USB EHCI */
#define	PCI_PRODUCT_INTEL_8SERIES_EHCI_2	0x8c2d		/* 8 Series USB EHCI */
#define	PCI_PRODUCT_INTEL_8SERIES_XHCI	0x8c31		/* 8 Series USB xHCI */
#define	PCI_PRODUCT_INTEL_8SERIES_LAN	0x8c33		/* 8 Series LAN */
#define	PCI_PRODUCT_INTEL_8SERIES_MEI_1	0x8c3a		/* 8 Series MEI Controller */
#define	PCI_PRODUCT_INTEL_8SERIES_MEI_2	0x8c3b		/* 8 Series MEI Controller */
#define	PCI_PRODUCT_INTEL_8SERIES_IDE_R	0x8c3c		/* 8 Series IDE-R */
#define	PCI_PRODUCT_INTEL_8SERIES_KT	0x8c3d		/* 8 Series KT */
#define	PCI_PRODUCT_INTEL_8SERIES_M_LPC	0x8c41		/* 8 Series Mobile Full Featured ES LPC */
#define	PCI_PRODUCT_INTEL_8SERIES_D_LPC	0x8c42		/* 8 Series Desktop Full Featured ES LPC */
#define	PCI_PRODUCT_INTEL_Z87_LPC	0x8c44		/* Z87 LPC */
#define	PCI_PRODUCT_INTEL_Z85_LPC	0x8c46		/* Z85 LPC */
#define	PCI_PRODUCT_INTEL_HM86_LPC	0x8c49		/* HM86 LPC */
#define	PCI_PRODUCT_INTEL_H87_LPC	0x8c4a		/* H87 LPC */
#define	PCI_PRODUCT_INTEL_HM87_LPC	0x8c4b		/* HM87 LPC */
#define	PCI_PRODUCT_INTEL_Q85_LPC	0x8c4c		/* Q85 LPC */
#define	PCI_PRODUCT_INTEL_Q87_LPC	0x8c4e		/* Q87 LPC */
#define	PCI_PRODUCT_INTEL_QM87_LPC	0x8c4f		/* QM87 LPC */
#define	PCI_PRODUCT_INTEL_B85_LPC	0x8c50		/* B85 LPC */
#define	PCI_PRODUCT_INTEL_C222_LPC	0x8c52		/* C222 LPC */
#define	PCI_PRODUCT_INTEL_C224_LPC	0x8c54		/* C224 LPC */
#define	PCI_PRODUCT_INTEL_C226_LPC	0x8c56		/* C226 LPC */
#define	PCI_PRODUCT_INTEL_H81_LPC	0x8c5c		/* H81 LPC */
#define	PCI_PRODUCT_INTEL_9SERIES_SATA	0x8c80		/* 9 Series SATA Controller */
#define	PCI_PRODUCT_INTEL_9SERIES_SATA_AHCI	0x8c82		/* 9 Series SATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_9SERIES_SATA_RAID	0x8c84		/* 9 Series SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_9SERIES_SATA_RAID_SR	0x8c86		/* 9 Series SATA Controller (RAID) + Smart Response */
#define	PCI_PRODUCT_INTEL_9SERIES_SATA_2	0x8c88		/* 9 Series SATA Controller */
#define	PCI_PRODUCT_INTEL_9SERIES_SATA_RAID1	0x8c8e		/* 9 Series SATA Controller (RAID1) */
#define	PCI_PRODUCT_INTEL_9SERIES_PCIE_1	0x8c90		/* 9 Series PCIE */
#define	PCI_PRODUCT_INTEL_9SERIES_PCIE_2	0x8c92		/* 9 Series PCIE */
#define	PCI_PRODUCT_INTEL_9SERIES_PCIE_3	0x8c94		/* 9 Series PCIE */
#define	PCI_PRODUCT_INTEL_9SERIES_PCIE_4	0x8c96		/* 9 Series PCIE */
#define	PCI_PRODUCT_INTEL_9SERIES_PCIE_5	0x8c98		/* 9 Series PCIE */
#define	PCI_PRODUCT_INTEL_9SERIES_PCIE_6	0x8c9a		/* 9 Series PCIE */
#define	PCI_PRODUCT_INTEL_9SERIES_PCIE_7	0x8c9c		/* 9 Series PCIE */
#define	PCI_PRODUCT_INTEL_9SERIES_PCIE_8	0x8c9e		/* 9 Series PCIE */
#define	PCI_PRODUCT_INTEL_9SERIES_HDA	0x8ca0		/* 9 Series HD Audio */
#define	PCI_PRODUCT_INTEL_9SERIES_SMB	0x8ca2		/* 9 Series SMBus Controller */
#define	PCI_PRODUCT_INTEL_9SERIES_THERM	0x8ca4		/* 9 Series Thermal */
#define	PCI_PRODUCT_INTEL_9SERIES_EHCI_1	0x8ca6		/* 9 Series USB EHCI */
#define	PCI_PRODUCT_INTEL_9SERIES_EHCI_2	0x8cad		/* 9 Series USB EHCI */
#define	PCI_PRODUCT_INTEL_9SERIES_XHCI	0x8cb1		/* 9 Series USB xHCI */
#define	PCI_PRODUCT_INTEL_9SERIES_LAN	0x8cb3		/* 9 Series LAN */
#define	PCI_PRODUCT_INTEL_9SERIES_MEI_1	0x8cba		/* 9 Series MEI Controller */
#define	PCI_PRODUCT_INTEL_9SERIES_MEI_2	0x8cbb		/* 9 Series MEI Controller */
#define	PCI_PRODUCT_INTEL_9SERIES_IDE_R	0x8cbc		/* 9 Series IDE-R */
#define	PCI_PRODUCT_INTEL_9SERIES_KT	0x8cbd		/* 9 Series KT */
#define	PCI_PRODUCT_INTEL_9SERIES_LPC_ES	0x8cc2		/* 9 Series Full Featured ES LPC */
#define	PCI_PRODUCT_INTEL_Z97_LPC	0x8cc4		/* Z97 LPC */
#define	PCI_PRODUCT_INTEL_H97_LPC	0x8cc6		/* H97 LPC */
#define	PCI_PRODUCT_INTEL_C610_SATA	0x8d00		/* C61x/X99 SATA Controller */
#define	PCI_PRODUCT_INTEL_C610_SATA_AHCI	0x8d02		/* C61x/X99 SATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_C610_SATA_RAID	0x2822		/* C61x/X99 SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_C610_SATA_RAID_2	0x8d06		/* C61x/X99 SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_C610_SATA_RAID_3	0x2826		/* C61x/X99 SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_C610_SATA_2	0x8d08		/* C61x/X99 SATA Controller */
#define	PCI_PRODUCT_INTEL_C610_SSATA	0x8d60		/* C61x/X99 sSATA Controller */
#define	PCI_PRODUCT_INTEL_C610_SSATA_AHCI	0x8d62		/* C61x/X99 sSATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_C610_SSATA_RAID	0x8d66		/* C61x/X99 sSATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_C610_SSATA_RAID_2	0x2827		/* C61x/X99 sSATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_C610_PCIE_1_1	0x8d10		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_1_2	0x8d11		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_1_3	0x244e		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_2_1	0x8d12		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_2_2	0x8d13		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_3_1	0x8d14		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_3_2	0x8d15		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_4_1	0x8d16		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_4_2	0x8d17		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_5_1	0x8d18		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_5_2	0x8d19		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_6_1	0x8d1a		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_6_2	0x8d1b		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_7_1	0x8d1c		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_7_2	0x8d1d		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_8_1	0x8d1e		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_PCIE_8_2	0x8d1f		/* C61x/X99 PCIE */
#define	PCI_PRODUCT_INTEL_C610_HDA	0x8d20		/* C61x/X99 HD Audio */
#define	PCI_PRODUCT_INTEL_C610_HDA_2	0x8d21		/* C61x/X99 HD Audio */
#define	PCI_PRODUCT_INTEL_C610_SMB	0x8d22		/* C61x/X99 SMBus Controller */
#define	PCI_PRODUCT_INTEL_C610_THERM	0x8d24		/* C61x/X99 Thermal */
#define	PCI_PRODUCT_INTEL_C610_EHCI	0x8d26		/* C61x/X99 USB EHCI */
#define	PCI_PRODUCT_INTEL_C610_EHCI_2	0x8d2d		/* C61x/X99 USB EHCI */
#define	PCI_PRODUCT_INTEL_C610_XHCI	0x8d31		/* C61x/X99 USB xHCI */
#define	PCI_PRODUCT_INTEL_C610_LAN	0x8d33		/* C61x/X99 LAN */
#define	PCI_PRODUCT_INTEL_C610_MEI	0x8d3a		/* C61x/X99 MEI Controller */
#define	PCI_PRODUCT_INTEL_C610_MEI_2	0x8d3b		/* C61x/X99 MEI Controller */
#define	PCI_PRODUCT_INTEL_C610_IDE_R	0x8d3c		/* C61x/X99 IDE-R */
#define	PCI_PRODUCT_INTEL_C610_KT	0x8d3d		/* C61x/X99 KT */
#define	PCI_PRODUCT_INTEL_X99_LPC	0x8d44		/* X99 LPC */
#define	PCI_PRODUCT_INTEL_X99_LPC_2	0x8d47		/* X99 LPC */
#define	PCI_PRODUCT_INTEL_C610_SPSR	0x8d7c		/* C61x/X99 SPSR */
#define	PCI_PRODUCT_INTEL_C610_MS_SMB0	0x8d7d		/* C61x/X99 MS SMbus */
#define	PCI_PRODUCT_INTEL_C610_MS_SMB1	0x8d7e		/* C61x/X99 MS SMbus */
#define	PCI_PRODUCT_INTEL_C610_MS_SMB2	0x8d7f		/* C61x/X99 MS SMbus */
#define	PCI_PRODUCT_INTEL_CORE4G_M_AHCI	0x9c03		/* Core 4G (mobile) SATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_CORE4G_M_RAID_1	0x9c05		/* Core 4G (mobile) SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_CORE4G_M_RAID_2	0x9c07		/* Core 4G (mobile) SATA Controller (RAID) Premium */
#define	PCI_PRODUCT_INTEL_CORE4G_M_RAID_3	0x9c0f		/* Core 4G (mobile) SATA Controller (RAID) Premium */
#define	PCI_PRODUCT_INTEL_CORE4G_M_PCIE_1	0x9c10		/* Core 4G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE4G_M_PCIE_2	0x9c12		/* Core 4G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE4G_M_PCIE_3	0x9c14		/* Core 4G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE4G_M_PCIE_4	0x9c16		/* Core 4G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE4G_M_PCIE_5	0x9c18		/* Core 4G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE4G_M_PCIE_6	0x9c1a		/* Core 4G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE4G_M_HDA	0x9c20		/* Core 4G (mobile) HD Audio */
#define	PCI_PRODUCT_INTEL_CORE4G_M_SMB	0x9c22		/* Core 4G (mobile) SMBus */
#define	PCI_PRODUCT_INTEL_CORE4G_M_THERM	0x9c24		/* Core 4G (mobile) Thermal */
#define	PCI_PRODUCT_INTEL_CORE4G_M_EHCI	0x9c26		/* Core 4G (mobile) USB EHCI */
#define	PCI_PRODUCT_INTEL_CORE4G_M_XHCI	0x9c31		/* Core 4G (mobile) USB xHCI */
#define	PCI_PRODUCT_INTEL_CORE4G_M_SDIO	0x9c35		/* Core 4G (mobile) SDIO */
#define	PCI_PRODUCT_INTEL_CORE4G_M_SSOUND	0x9c36		/* Core 4G (mobile) Smart Sound */
#define	PCI_PRODUCT_INTEL_CORE4G_M_MEI_1	0x9c3a		/* Core 4G (mobile) MEI */
#define	PCI_PRODUCT_INTEL_CORE4G_M_MEI_2	0x9c3b		/* Core 4G (mobile) MEI */
#define	PCI_PRODUCT_INTEL_CORE4G_M_IDE_R	0x9c3c		/* Core 4G (mobile) IDE-R */
#define	PCI_PRODUCT_INTEL_CORE4G_M_KT	0x9c3d		/* Core 4G (mobile) KT */
#define	PCI_PRODUCT_INTEL_CORE4G_M_LPC_1	0x9c41		/* Core 4G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE4G_M_LPC_2	0x9c43		/* Core 4G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE4G_M_LPC_3	0x9c45		/* Core 4G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE4G_M_S_DMA	0x9c60		/* Core 4G (mobile) Serial I/O DMA */
#define	PCI_PRODUCT_INTEL_CORE4G_M_S_I2C_0	0x9c61		/* Core 4G (mobile) Serial I/O I2C */
#define	PCI_PRODUCT_INTEL_CORE4G_M_S_I2C_1	0x9c62		/* Core 4G (mobile) Serial I/O I2C */
#define	PCI_PRODUCT_INTEL_CORE4G_M_S_UART_0	0x9c63		/* Core 4G (mobile) Serial I/O UART */
#define	PCI_PRODUCT_INTEL_CORE4G_M_S_UART_1	0x9c64		/* Core 4G (mobile) Serial I/O UART */
#define	PCI_PRODUCT_INTEL_CORE4G_M_S_GSPI_0	0x9c65		/* Core 4G (mobile) Serial I/O GSPI */
#define	PCI_PRODUCT_INTEL_CORE4G_M_S_GSPI_1	0x9c66		/* Core 4G (mobile) Serial I/O GSPI */
#define	PCI_PRODUCT_INTEL_CORE5G_M_AHCI	0x9c83		/* Core 5G (mobile) SATA Controller (AHCI) */
#define	PCI_PRODUCT_INTEL_CORE5G_M_RAID_1	0x9c85		/* Core 5G (mobile) SATA Controller (RAID) */
#define	PCI_PRODUCT_INTEL_CORE5G_M_RAID_2	0x9c87		/* Core 5G (mobile) SATA Controller (RAID) Premium */
#define	PCI_PRODUCT_INTEL_CORE5G_M_RAID_3	0x9c8f		/* Core 5G (mobile) SATA Controller (RAID) RRT Only */
#define	PCI_PRODUCT_INTEL_CORE5G_M_PCIE_1	0x9c90		/* Core 5G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE5G_M_PCIE_2	0x9c92		/* Core 5G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE5G_M_PCIE_3	0x9c94		/* Core 5G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE5G_M_PCIE_4	0x9c96		/* Core 5G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE5G_M_PCIE_5	0x9c98		/* Core 5G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE5G_M_PCIE_6	0x9c9a		/* Core 5G (mobile) PCIE */
#define	PCI_PRODUCT_INTEL_CORE5G_M_HDA	0x9ca0		/* Core 5G (mobile) HD Audio */
#define	PCI_PRODUCT_INTEL_CORE5G_M_SMB	0x9ca2		/* Core 5G (mobile) SMBus */
#define	PCI_PRODUCT_INTEL_CORE5G_M_THERM	0x9ca4		/* Core 5G (mobile) Thermal */
#define	PCI_PRODUCT_INTEL_CORE5G_M_EHCI	0x9ca6		/* Core 5G (mobile) USB EHCI */
#define	PCI_PRODUCT_INTEL_CORE5G_M_XHCI	0x9cb1		/* Core 5G (mobile) USB xHCI */
#define	PCI_PRODUCT_INTEL_CORE5G_M_SDIO	0x9cb5		/* Core 5G (mobile) SDIO */
#define	PCI_PRODUCT_INTEL_CORE5G_M_SSOUND	0x9cb6		/* Core 5G (mobile) Smart Sound */
#define	PCI_PRODUCT_INTEL_CORE5G_M_MEI_1	0x9cba		/* Core 5G (mobile) ME Interface */
#define	PCI_PRODUCT_INTEL_CORE5G_M_MEI_2	0x9cbb		/* Core 5G (mobile) ME Interface */
#define	PCI_PRODUCT_INTEL_CORE5G_M_IDE_R	0x9cbc		/* Core 5G (mobile) IDE-R */
#define	PCI_PRODUCT_INTEL_CORE5G_M_KT	0x9cbd		/* Core 5G (mobile) KT */
#define	PCI_PRODUCT_INTEL_CORE5G_M_LPC_1	0x9cc1		/* Core 5G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE5G_M_LPC_2	0x9cc2		/* Core 5G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE5G_M_LPC_3	0x9cc3		/* Core 5G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE5G_M_LPC_4	0x9cc5		/* Core 5G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE5G_M_LPC_5	0x9cc6		/* Core 5G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE5G_M_LPC_6	0x9cc7		/* Core 5G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE5G_M_LPC_7	0x9cc9		/* Core 5G (mobile) LPC */
#define	PCI_PRODUCT_INTEL_CORE5G_M_S_DMA	0x9ce0		/* Core 5G (mobile) Serial I/O DMA */
#define	PCI_PRODUCT_INTEL_CORE5G_M_S_I2C_0	0x9ce1		/* Core 5G (mobile) Serial I/O I2C */
#define	PCI_PRODUCT_INTEL_CORE5G_M_S_I2C_1	0x9ce2		/* Core 5G (mobile) Serial I/O I2C */
#define	PCI_PRODUCT_INTEL_CORE5G_M_S_UART_0	0x9ce3		/* Core 5G (mobile) Serial I/O UART */
#define	PCI_PRODUCT_INTEL_CORE5G_M_S_UART_1	0x9ce4		/* Core 5G (mobile) Serial I/O UART */
#define	PCI_PRODUCT_INTEL_CORE5G_M_S_GSPI_0	0x9ce5		/* Core 5G (mobile) Serial I/O GSPI */
#define	PCI_PRODUCT_INTEL_CORE5G_M_S_GSPI_1	0x9ce6		/* Core 5G (mobile) Serial I/O GSPI */
#define	PCI_PRODUCT_INTEL_PINEVIEW_HB	0xa000		/* Pineview Host Bridge */
#define	PCI_PRODUCT_INTEL_PINEVIEW_IGD	0xa001		/* Pineview Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_PINEVIEW_IGD_1	0xa002		/* Pineview Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_PINEVIEW_M_HB	0xa010		/* Pineview Host Bridge */
#define	PCI_PRODUCT_INTEL_PINEVIEW_M_IGD	0xa011		/* Pineview Integrated Graphics Device */
#define	PCI_PRODUCT_INTEL_21152	0xb152		/* S21152BB PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_21154	0xb154		/* S21152BA,S21154AE/BE PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_21555	0xb555		/* 21555 Non-Transparent PCI-PCI Bridge */
#define	PCI_PRODUCT_INTEL_CP_DMI_1	0xd131		/* Core Processor DMI */
#define	PCI_PRODUCT_INTEL_CP_DMI_2	0xd132		/* Core Processor DMI */
#define	PCI_PRODUCT_INTEL_CP_PCIE_1	0xd138		/* Core Processor PCIe Root Port (x16 or x8 max) */
#define	PCI_PRODUCT_INTEL_CP_PCIE_2	0xd13a		/* Core Processor PCIe Root Port (x8 max) */
#define	PCI_PRODUCT_INTEL_CP_QPI_LINK	0xd150		/* Core Processor QPI Link */
#define	PCI_PRODUCT_INTEL_CP_QPI_RPREGS	0xd151		/* Core Processor QPI Routing and Protocol Registers */
#define	PCI_PRODUCT_INTEL_CP_SYS_MREGS	0xd155		/* Core Processor System Management Registers */
#define	PCI_PRODUCT_INTEL_CP_SS_REGS	0xd156		/* Core Processor Semaphore and Scratchpad Registers */
#define	PCI_PRODUCT_INTEL_CP_SCS_REGS	0xd157		/* Core Processor System Control and Status Registers */
#define	PCI_PRODUCT_INTEL_CP_MISC_REGS	0xd158		/* Core Processor Miscellaneous Registers */
#define	PCI_PRODUCT_INTEL_HANKSVILLE	0xf0fe		/* HANKSVILLE LAN Controller */


/* Intergraph products */
#define	PCI_PRODUCT_INTERGRAPH_4D50T	0x00e4		/* Powerstorm 4D50T */
#define	PCI_PRODUCT_INTERGRAPH_4D60T	0x00e3		/* Powerstorm 4D60T */

/* Intersil products */
#define	PCI_PRODUCT_INTERSIL_MINI_PCI_WLAN	0x3873		/* PRISM2.5 Mini-PCI WLAN */
#define	PCI_PRODUCT_INTERSIL_MINI_PCI_3877	0x3877		/* PRISM Indigo Mini-PCI WLAN */
#define	PCI_PRODUCT_INTERSIL_MINI_PCI_3890	0x3890		/* PRISM Duette Mini-PCI WLAN */

/* Invertex */
#define	PCI_PRODUCT_INVERTEX_AEON	0x0005		/* AEON */

/* IO Data products */
#define	PCI_PRODUCT_IODATA_CBIDE2	0x0003		/* CBIDE2/CI-iCN NinjaATA-32Bi IDE */
#define	PCI_PRODUCT_IODATA_CBSCII	0x0005		/* CBSCII NinjaSCSI-32Bi SCSI */
#define	PCI_PRODUCT_IODATA_RSAPCI	0x0007		/* RSA-PCI 2-port Serial */
#define	PCI_PRODUCT_IODATA_GVBCTV5DL	0xd012		/* GV-BCTV5DL/PCI TV tuner */

/* ITE products */
#define	PCI_PRODUCT_ITE_IT8152	0x8152		/* IT8152 Host Bridge */
#define	PCI_PRODUCT_ITE_IT8211	0x8211		/* IT8211 IDE Controller */
#define	PCI_PRODUCT_ITE_IT8212	0x8212		/* IT8212 IDE Controller */
#define	PCI_PRODUCT_ITE_IT8213	0x8213		/* IT8213 IDE Controller */
#define	PCI_PRODUCT_ITE_IT8888	0x8888		/* PCI-ISA Bridge */
#define	PCI_PRODUCT_ITE_IT8892	0x8892		/* PCIe-PCI Bridge */

/* I. T. T. products */
#define	PCI_PRODUCT_ITT_AGX016	0x0001		/* AGX016 */
#define	PCI_PRODUCT_ITT_ITT3204	0x0002		/* ITT3204 MPEG Decoder */

/* JMicron products */
#define	PCI_PRODUCT_JMICRON_JMB360	0x2360		/* JMB360 SATA Controller */
#define	PCI_PRODUCT_JMICRON_JMB361	0x2361		/* JMB361 SATA/PATA Controller */
#define	PCI_PRODUCT_JMICRON_JMB362	0x2362		/* JMB362 SATA Controller */
#define	PCI_PRODUCT_JMICRON_JMB363	0x2363		/* JMB363 SATA/PATA Controller */
#define	PCI_PRODUCT_JMICRON_JMB365	0x2365		/* JMB365 SATA/PATA Controller */
#define	PCI_PRODUCT_JMICRON_JMB366	0x2366		/* JMB366 SATA/PATA Controller */
#define	PCI_PRODUCT_JMICRON_JMB368	0x2368		/* JMB368 PATA Controller */
#define	PCI_PRODUCT_JMICRON_JMB38X_SD	0x2381		/* JMB38X SD Host Controller */
#define	PCI_PRODUCT_JMICRON_JMB38X_MMC	0x2382		/* JMB38X SD/MMC Host Controller */
#define	PCI_PRODUCT_JMICRON_JMB38X_MS	0x2383		/* JMB38X Memory Stick Host Controller */
#define	PCI_PRODUCT_JMICRON_JMB38X_XD	0x2384		/* JMB38X xD Host Controller */
#define	PCI_PRODUCT_JMICRON_JMB388_SD	0x2391		/* JMB388 SD Host Controller */
#define	PCI_PRODUCT_JMICRON_JMB388_MMC	0x2392		/* JMB388 SD/MMC Host Controller */
#define	PCI_PRODUCT_JMICRON_JMB388_MS	0x2393		/* JMB388 Memory Stick Host Controller */
#define	PCI_PRODUCT_JMICRON_JMB388_XD	0x2394		/* JMB388 xD Host Controller */
#define	PCI_PRODUCT_JMICRON_JMC250	0x0250		/* JMC250 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_JMICRON_JMC260	0x0260		/* JMC260 Fast Ethernet Controller */

/* JNI products */
#define	PCI_PRODUCT_JNI_JNIC1460	0x1460		/* JNIC-1460 Fibre-Channel Adapter */
#define	PCI_PRODUCT_JNI_JNIC1560	0x1560		/* JNIC-1560 Dual Fibre-Channel Adapter */
#define	PCI_PRODUCT_JNI_FCI1063	0x4643		/* FCI-1063 Fibre-Channel Adapter */
#define	PCI_PRODUCT_JNI_FCX26562	0x6562		/* FCX2-6562 Dual Fibre-Channel Adapter */
#define	PCI_PRODUCT_JNI_FCX6562	0x656a		/* FCX-6562 Fibre-Channel Adapter */

/* Juniper Networks products */
#define	PCI_PRODUCT_JUNIPER_XCLK0	0x0030		/* Experimental Clock Version 0 */

/* KTI products - XXX better descriptions */
#define	PCI_PRODUCT_KTI_NE2KETHER	0x3000		/* Ethernet */

/* LAN Media */
#define	PCI_PRODUCT_LMC_HSSI	0x0003		/* HSSI Interface */
#define	PCI_PRODUCT_LMC_DS3	0x0004		/* DS3 Interface */
#define	PCI_PRODUCT_LMC_SSI	0x0005		/* SSI */
#define	PCI_PRODUCT_LMC_DS1	0x0006		/* DS1 */

/* Lava products */
#define	PCI_PRODUCT_LAVA_TWOSP_2S	0x0100		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_QUATTRO_AB	0x0101		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_QUATTRO_CD	0x0102		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_IOFLEX_2S_0	0x0110		/* Serial */
#define	PCI_PRODUCT_LAVA_IOFLEX_2S_1	0x0111		/* Serial */
#define	PCI_PRODUCT_LAVA_QUATTRO_AB2	0x0120		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_QUATTRO_CD2	0x0121		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_OCTOPUS550_0	0x0180		/* Quad Serial */
#define	PCI_PRODUCT_LAVA_OCTOPUS550_1	0x0181		/* Quad Serial */
#define	PCI_PRODUCT_LAVA_LAVAPORT_2	0x0200		/* Serial */
#define	PCI_PRODUCT_LAVA_LAVAPORT_0	0x0201		/* Serial */
#define	PCI_PRODUCT_LAVA_LAVAPORT_1	0x0202		/* Serial */
#define	PCI_PRODUCT_LAVA_SSERIAL	0x0500		/* Single Serial */
#define	PCI_PRODUCT_LAVA_650	0x0600		/* Serial */
#define	PCI_PRODUCT_LAVA_TWOSP_1P	0x8000		/* Parallel */
#define	PCI_PRODUCT_LAVA_PARALLEL2	0x8001		/* Dual Parallel */
#define	PCI_PRODUCT_LAVA_PARALLEL2A	0x8002		/* Dual Parallel */
#define	PCI_PRODUCT_LAVA_PARALLELB	0x8003		/* Dual Parallel */

/* LeadTek Research */
#define	PCI_PRODUCT_LEADTEK_S3_805	0x0000		/* S3 805 */

/* Level One products */
#define	PCI_PRODUCT_LEVELONE_LXT1001	0x0001		/* LXT-1001 10/100/1000 Ethernet */

/* Linear Systems / CompuModules */
#define	PCI_PRODUCT_LINEARSYS_DVB_TX	0x7629		/* DVB Transmitter */
#define	PCI_PRODUCT_LINEARSYS_DVB_RX	0x7630		/* DVB Receiver */

/* Linksys products */
#define	PCI_PRODUCT_LINKSYS_EG1032	0x1032		/* EG1032 v2 Instant Gigabit Network Adapter */
#define	PCI_PRODUCT_LINKSYS_EG1064	0x1064		/* EG1064 v2 Instant Gigabit Network Adapter */
#define	PCI_PRODUCT_LINKSYS_PCMPC200	0xab08		/* PCMPC200 */
#define	PCI_PRODUCT_LINKSYS_PCM200	0xab09		/* PCM200 */
#define	PCI_PRODUCT_LINKSYS2_IPN2220	0x2220		/* IPN 2220 Wireless LAN Adapter (rev 01) */

/* Lite-On products */
#define	PCI_PRODUCT_LITEON_82C168	0x0002		/* 82C168/82C169 (PNIC) 10/100 Ethernet */
#define	PCI_PRODUCT_LITEON_82C115	0xc115		/* 82C115 (PNIC II) 10/100 Ethernet */

/* Lucent Technologies products */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0440	0x0440		/* K56flex DSVD LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0441	0x0441		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0442	0x0442		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0443	0x0443		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0444	0x0444		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0445	0x0445		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0446	0x0446		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0447	0x0447		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0448	0x0448		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0449	0x0449		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044a	0x044a		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044b	0x044b		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044c	0x044c		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044d	0x044d		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044e	0x044e		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0450	0x0450		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0451	0x0451		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0452	0x0452		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0453	0x0453		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0454	0x0454		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0455	0x0455		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0456	0x0456		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0457	0x0457		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0458	0x0458		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0459	0x0459		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_045a	0x045a		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_VENUSMODEM	0x0480		/* Venus Modem */
#define	PCI_PRODUCT_LUCENT_OR3LP26	0x5400		/* ORCA FPGA w/ 32-bit PCI ASIC Core */
#define	PCI_PRODUCT_LUCENT_OR3TP12	0x5401		/* ORCA FPGA w/ 64-bit PCI ASIC Core */
#define	PCI_PRODUCT_LUCENT_USBHC	0x5801		/* USB Host Controller */
#define	PCI_PRODUCT_LUCENT_USBHC2	0x5802		/* 2-port USB Host Controller */
#define	PCI_PRODUCT_LUCENT_FW322_323	0x5811		/* FW322/323 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_LUCENT_FW643_PCIE	0x5901		/* FW643 PCIE IEEE 1394b Host Controller */
#define	PCI_PRODUCT_LUCENT_ET1310	0xed00		/* ET1310 10/100/1000 Ethernet */
#define	PCI_PRODUCT_LUCENT_ET1301	0xed01		/* ET1301 10/100 Ethernet */

/* Macronix */
#define	PCI_PRODUCT_MACRONIX_MX98713	0x0512		/* MX98713 (PMAC) 10/100 Ethernet */
#define	PCI_PRODUCT_MACRONIX_MX987x5	0x0531		/* MX987x5 (PMAC) 10/100 Ethernet */

/* Madge Networks products */
#define	PCI_PRODUCT_MADGE_SMARTRN2	0x0002		/* Smart 16/4 PCI Ringnode Mk2 */
#define	PCI_PRODUCT_MADGE_COLLAGE25	0x1000		/* Collage 25 ATM Adapter */
#define	PCI_PRODUCT_MADGE_COLLAGE155	0x1001		/* Collage 155 ATM Adapter */

/* MAGMA products */
#define	PCI_PRODUCT_MAGMA_SERIAL16	0x0010		/* 16 DMA PCI-SLRS */
#define	PCI_PRODUCT_MAGMA_SERIAL4	0x0011		/* 4 DMA PCI-SLRS */

/* Matrox products */
#define	PCI_PRODUCT_MATROX_ATLAS	0x0518		/* MGA PX2085 (\"Atlas\") */
#define	PCI_PRODUCT_MATROX_MILLENNIUM	0x0519		/* MGA Millennium 2064W (\"Storm\") */
#define	PCI_PRODUCT_MATROX_MYSTIQUE	0x051a		/* MGA Mystique 1064SG */
#define	PCI_PRODUCT_MATROX_MILLENNIUM2	0x051b		/* MGA Millennium II 2164W */
#define	PCI_PRODUCT_MATROX_MILLENNIUM2_AGP	0x051f		/* MGA Millennium II 2164WA-B AGP */
#define	PCI_PRODUCT_MATROX_G200_PCI	0x0520		/* MGA G200 PCI */
#define	PCI_PRODUCT_MATROX_G200_AGP	0x0521		/* MGA G200 AGP */
#define	PCI_PRODUCT_MATROX_G200E_SE	0x0522		/* MGA G200e (ServerEngines) */
#define	PCI_PRODUCT_MATROX_G400_AGP	0x0525		/* MGA G400 AGP */
#define	PCI_PRODUCT_MATROX_G200EW	0x0532		/* MGA G200eW */
#define	PCI_PRODUCT_MATROX_G200EH	0x0533		/* MGA G200eH */
#define	PCI_PRODUCT_MATROX_IMPRESSION	0x0d10		/* MGA Impression */
#define	PCI_PRODUCT_MATROX_G100_PCI	0x1000		/* MGA G100 PCI */
#define	PCI_PRODUCT_MATROX_G100_AGP	0x1001		/* MGA G100 AGP */
#define	PCI_PRODUCT_MATROX_G550_AGP	0x2527		/* MGA G550 AGP */

/* MediaQ products */
#define	PCI_PRODUCT_MEDIAQ_MQ200	0x0200		/* MQ200 */

/* Mellanox Technologies */
#define	PCI_PRODUCT_MELLANOX_MT23108	0x5a44		/* InfiniHost (Tavor) */
#define	PCI_PRODUCT_MELLANOX_MT23108_PCI	0x5a46		/* InfiniHost PCI Bridge (Tavor) */
#define	PCI_PRODUCT_MELLANOX_MT25204_OLD	0x5e8c		/* InfiniHost III Lx (old Sinai) */
#define	PCI_PRODUCT_MELLANOX_MT25204	0x6274		/* InfiniHost III Lx (Sinai) */
#define	PCI_PRODUCT_MELLANOX_MT25208_COMPAT	0x6278		/* InfiniHost III Ex (Arbel in Tavor compatility) */
#define	PCI_PRODUCT_MELLANOX_MT25208	0x6282		/* InfiniHost III Ex (Arbel) */
#define	PCI_PRODUCT_MELLANOX_MT25408_SDR	0x6340		/* ConnectX SDR (Hermon) */
#define	PCI_PRODUCT_MELLANOX_MT25408_DDR	0x634a		/* ConnectX DDR (Hermon) */
#define	PCI_PRODUCT_MELLANOX_MT25408_QDR	0x6354		/* ConnectX QDR PCIe 2.0 2.5GT/s (Hermon) */
#define	PCI_PRODUCT_MELLANOX_MT25408_EN	0x6368		/* ConnectX EN 10GigE PCIe 2.0 2.5GT/s (Hermon) */
#define	PCI_PRODUCT_MELLANOX_MT25408_DDR_2	0x6732		/* ConnectX DDR PCIe 2.0 5GT/s (Hermon) */
#define	PCI_PRODUCT_MELLANOX_MT25408_QDR_2	0x673c		/* ConnectX QDR PCIe 2.0 5GT/s (Hermon) */
#define	PCI_PRODUCT_MELLANOX_MT25408_EN_2	0x6750		/* ConnectX EN 10GigE PCIe 2.0 5GT/s (Hermon) */

/* Micro Memory products */
#define	PCI_PRODUCT_MICROMEMORY_5415CN	0x5415		/* MM-5415CN Memory Module */
#define	PCI_PRODUCT_MICROMEMORY_5425CN	0x5425		/* MM-5425CN Memory Module */

/* Microsoft products */
#define	PCI_PRODUCT_MICROSOFT_MN120	0x0001		/* MN-120 10/100 Ethernet Notebook Adapter */

/* Micrel products */
#define	PCI_PRODUCT_MICREL_KSZ8841	0x8841		/* 10/100 Ethernet */
#define	PCI_PRODUCT_MICREL_KSZ8842	0x8842		/* Switched 2 Port 10/100 Ethernet */

/* Middle Digital products */
#define	PCI_PRODUCT_MIDDLE_DIGITAL_WEASEL_VGA	0x9050		/* Weasel Virtual VGA */
#define	PCI_PRODUCT_MIDDLE_DIGITAL_WEASEL_SERIAL	0x9051		/* Weasel Serial Port */
#define	PCI_PRODUCT_MIDDLE_DIGITAL_WEASEL_CONTROL	0x9052		/* Weasel Control */

/* Mitsubishi products */
#define	PCI_PRODUCT_MITSUBISHIELEC_TORNADO	0x0308		/* Tornado 3000 AGP */

/* Motorola products */
#define	PCI_PRODUCT_MOT_MPC105	0x0001		/* MPC105 \"Eagle\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC106	0x0002		/* MPC106 \"Grackle\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC8240	0x0003		/* MPC8240 \"Kahlua\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC107	0x0004		/* MPC107 \"Chaparral\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC8245	0x0006		/* MPC8245 \"Kahlua II\" Host Bridge */
#define	PCI_PRODUCT_MOT_MPC8555E	0x000a		/* MPC8555E */
#define	PCI_PRODUCT_MOT_MPC8541	0x000c		/* MPC8541 */
#define	PCI_PRODUCT_MOT_MPC8548E	0x0012		/* MPC8548E */
#define	PCI_PRODUCT_MOT_MPC8548	0x0013		/* MPC8548 */
#define	PCI_PRODUCT_MOT_RAVEN	0x4801		/* Raven Host Bridge & Multi-Processor Interrupt Controller */
#define	PCI_PRODUCT_MOT_FALCON	0x4802		/* Falcon ECC Memory Controller Chip Set */
#define	PCI_PRODUCT_MOT_HAWK	0x4803		/* Hawk System Memory Controller & PCI Host Bridge */
#define	PCI_PRODUCT_MOT_MPC5200B	0x5809		/* MPC5200B Host Bridge */

/* Moxa Technologies products */
#define	PCI_PRODUCT_MOXA_CP102U	0x1022		/* CP102U */
#define	PCI_PRODUCT_MOXA_C104H	0x1040		/* C104H */
#define	PCI_PRODUCT_MOXA_CP104	0x1041		/* CP104UL */
#define	PCI_PRODUCT_MOXA_CP104V2	0x1042		/* CP104V2 */
#define	PCI_PRODUCT_MOXA_CP104EL	0x1043		/* CP104EL */
#define	PCI_PRODUCT_MOXA_CP114	0x1141		/* CP114 */
#define	PCI_PRODUCT_MOXA_C168H	0x1680		/* C168H */
#define	PCI_PRODUCT_MOXA_C168U	0x1681		/* C168U */
#define	PCI_PRODUCT_MOXA_C168EL	0x1682		/* C168EL */
#define	PCI_PRODUCT_MOXA_C168ELA	0x1683		/* C168EL A */

/* Mutech products */
#define	PCI_PRODUCT_MUTECH_MV1000	0x0001		/* MV1000 */

/* Mylex products */
#define	PCI_PRODUCT_MYLEX_RAID_V2	0x0001		/* DAC960 RAID (v2 Interface) */
#define	PCI_PRODUCT_MYLEX_RAID_V3	0x0002		/* DAC960 RAID (v3 Interface) */
#define	PCI_PRODUCT_MYLEX_RAID_V4	0x0010		/* DAC960 RAID (v4 Interface) */
#define	PCI_PRODUCT_MYLEX_RAID_V5	0x0020		/* DAC960 RAID (v5 Interface) */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID_3000	0x0030		/* eXtremeRAID 3000 */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID_2000	0x0040		/* eXtremeRAID 2000 */
#define	PCI_PRODUCT_MYLEX_ACCELERAID	0x0050		/* AcceleRAID 352 */
#define	PCI_PRODUCT_MYLEX_ACCELERAID_170	0x0052		/* AcceleRAID 170 */
#define	PCI_PRODUCT_MYLEX_ACCELERAID_160	0x0054		/* AcceleRAID 160 */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID1100	0xba55		/* eXtremeRAID 1100 */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID	0xba56		/* eXtremeRAID 2000/3000 */

/* Myricom products */
#define	PCI_PRODUCT_MYRICOM_MYRINET	0x8043		/* Myrinet */

/* Myson-Century Technology products */
#define	PCI_PRODUCT_MYSON_MTD803	0x0803		/* MTD803 3-in-1 Fast Ethernet Controller */

/* National Datacomm products */
#define	PCI_PRODUCT_NDC_NCP130	0x0130		/* NCP130 Wireless NIC */
#define	PCI_PRODUCT_NDC_NCP130A2	0x0131		/* NCP130 rev A2 Wireless NIC */

/* Netoctave */
#define	PCI_PRODUCT_NETOCTAVE_NSP2K	0x0100		/* NSP2K */

/* NetBoost (now Intel) products */
#define	PCI_PRODUCT_NETBOOST_POLICY	0x0000		/* Policy Accelerator */

/* NetLogic (now Broadcom?) products */
#define	PCI_PRODUCT_NETLOGIC_XLP_SBC	0x1001		/* XLP System Bridge controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_ICI	0x1002		/* XLP Inter-Chip interconnect */
#define	PCI_PRODUCT_NETLOGIC_XLP_PIC	0x1003		/* XLP Programmable Interrupt controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_PCIROOT	0x1004		/* XLP PCI-Express RootComplex/Endpoint port */
#define	PCI_PRODUCT_NETLOGIC_XLP_INTERLAKEN	0x1005		/* XLP Interlaken LA interface */
#define	PCI_PRODUCT_NETLOGIC_XLP_DEVUSB	0x1006		/* XLP Device USB controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_EHCIUSB	0x1007		/* XLP EHCI USB controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_OHCIUSB	0x1008		/* XLP OHCI USB controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_NAE	0x1009		/* XLP Network Acceleration engine */
#define	PCI_PRODUCT_NETLOGIC_XLP_POE	0x100A		/* XLP Packet Ordering engine */
#define	PCI_PRODUCT_NETLOGIC_XLP_FMN	0x100B		/* XLP Fast Messaging Network */
#define	PCI_PRODUCT_NETLOGIC_XLP_DMA	0x100C		/* XLP Data Transfer and RAID engine */
#define	PCI_PRODUCT_NETLOGIC_XLP_SAE	0x100D		/* XLP Security accelerator */
#define	PCI_PRODUCT_NETLOGIC_XLP_PKE	0x100E		/* XLP RSA/ECC accelerator */
#define	PCI_PRODUCT_NETLOGIC_XLP_CDE	0x100F		/* XLP Compress/Decompression engine */
#define	PCI_PRODUCT_NETLOGIC_XLP_UART	0x1010		/* XLP UART controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_I2C	0x1011		/* XLP I2C controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_GPIO	0x1012		/* XLP GPIO controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_SYSTEM	0x1013		/* XLP System controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_JTAG	0x1014		/* XLP JTAG interface */
#define	PCI_PRODUCT_NETLOGIC_XLP_NOR	0x1015		/* XLP NOR flash controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_NAND	0x1016		/* XLP NAND flash controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_SPI	0x1017		/* XLP SPI controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_SDHC	0x1018		/* XLP eMMC/SD/SDIO controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_RXE	0x1019		/* XLP Regular Expression accelerator */
#define	PCI_PRODUCT_NETLOGIC_XLP_AHCISATA	0x101a		/* XLP AHCI SATA controller */
#define	PCI_PRODUCT_NETLOGIC_XLP_SRIO	0x101b		/* XLP SRIO (Serial Rapid IO) controller */

/* NetVin products - XXX better descriptions */
#define	PCI_PRODUCT_NETVIN_5000	0x5000		/* 5000 Ethernet */

/* Newbridge / Tundra products */
#define	PCI_PRODUCT_NEWBRIDGE_CA91CX42	0x0000		/* Universe VME Bridge */
#define	PCI_PRODUCT_NEWBRIDGE_CA91L826A	0x0826		/* QSpan II PCI Bridge */
#define	PCI_PRODUCT_NEWBRIDGE_CA91L8260	0x8260		/* PowerSpan PCI Bridge */
#define	PCI_PRODUCT_NEWBRIDGE_CA91L8261	0x8261		/* PowerSpan II PCI Bridge */

/* National Instruments products */
#define	PCI_PRODUCT_NATIONALINST_MXI3	0x2c30		/* MXI-3 PCI Extender */

/* National Semiconductor products */
#define	PCI_PRODUCT_NS_DP83810	0x0001		/* DP83810 10/100 Ethernet */
#define	PCI_PRODUCT_NS_PC87415	0x0002		/* PC87415 IDE */
#define	PCI_PRODUCT_NS_PC87560	0x000e		/* 87560 Legacy I/O */
#define	PCI_PRODUCT_NS_USB	0x0012		/* USB */
#define	PCI_PRODUCT_NS_DP83815	0x0020		/* DP83815 10/100 Ethernet */
#define	PCI_PRODUCT_NS_DP83820	0x0022		/* DP83820 10/100/1000 Ethernet */
#define	PCI_PRODUCT_NS_CS5535_HB	0x0028		/* CS5535 Host-PCI Bridge */
#define	PCI_PRODUCT_NS_CS5535_ISA	0x002b		/* CS5535 PCI-ISA Bridge */
#define	PCI_PRODUCT_NS_CS5535_IDE	0x002d		/* CS5535 IDE Controller */
#define	PCI_PRODUCT_NS_CS5535_AUDIO	0x002e		/* CS5535 Audio Controller */
#define	PCI_PRODUCT_NS_CS5535_USB	0x002f		/* CS5535 USB Host Controller */
#define	PCI_PRODUCT_NS_CS5535_VIDEO	0x0030		/* CS5535 Video Controller */
#define	PCI_PRODUCT_NS_SATURN	0x0035		/* Saturn */
#define	PCI_PRODUCT_NS_SC1100_IDE	0x0502		/* SC1100 PCI IDE */
#define	PCI_PRODUCT_NS_SC1100_AUDIO	0x0503		/* SC1100 XpressAUDIO */
#define	PCI_PRODUCT_NS_SC1100_ISA	0x0510		/* SC1100 PCI-ISA Bridge */
#define	PCI_PRODUCT_NS_SC1100_ACPI	0x0511		/* SC1100 SMI/ACPI */
#define	PCI_PRODUCT_NS_SC1100_XBUS	0x0515		/* SC1100 X-Bus */
#define	PCI_PRODUCT_NS_NS87410	0xd001		/* NS87410 */

/* Philips products */
#define	PCI_PRODUCT_PHILIPS_SAA7130HL	0x7130		/* SAA7130HL PCI Video Broadcast Decoder */
#define	PCI_PRODUCT_PHILIPS_SAA7133HL	0x7133		/* SAA7133HL PCI A/V Broadcast Decoder */
#define	PCI_PRODUCT_PHILIPS_SAA7134HL	0x7134		/* SAA7134HL PCI A/V Broadcast Decoder */
#define	PCI_PRODUCT_PHILIPS_SAA7135HL	0x7135		/* SAA7135HL PCI A/V Broadcast Decoder */
#define	PCI_PRODUCT_PHILIPS_SAA7146AH	0x7146		/* SAA7146AH PCI Multimedia Bridge */

/* NCR/Symbios Logic products */
#define	PCI_PRODUCT_SYMBIOS_810	0x0001		/* 53c810 */
#define	PCI_PRODUCT_SYMBIOS_820	0x0002		/* 53c820 */
#define	PCI_PRODUCT_SYMBIOS_825	0x0003		/* 53c825 */
#define	PCI_PRODUCT_SYMBIOS_815	0x0004		/* 53c815 */
#define	PCI_PRODUCT_SYMBIOS_810AP	0x0005		/* 53c810AP */
#define	PCI_PRODUCT_SYMBIOS_860	0x0006		/* 53c860 */
#define	PCI_PRODUCT_SYMBIOS_1510D	0x000a		/* 53c1510D */
#define	PCI_PRODUCT_SYMBIOS_896	0x000b		/* 53c896 */
#define	PCI_PRODUCT_SYMBIOS_895	0x000c		/* 53c895 */
#define	PCI_PRODUCT_SYMBIOS_885	0x000d		/* 53c885 */
#define	PCI_PRODUCT_SYMBIOS_875	0x000f		/* 53c875/876 */
#define	PCI_PRODUCT_SYMBIOS_1510	0x0010		/* 53c1510 */
#define	PCI_PRODUCT_SYMBIOS_895A	0x0012		/* 53c895A */
#define	PCI_PRODUCT_SYMBIOS_875A	0x0013		/* 53c875A */
#define	PCI_PRODUCT_SYMBIOS_1010	0x0020		/* 53c1010 */
#define	PCI_PRODUCT_SYMBIOS_1010_2	0x0021		/* 53c1010 (66MHz) */
#define	PCI_PRODUCT_SYMBIOS_1030	0x0030		/* 53c1020/53c1030 */
#define	PCI_PRODUCT_SYMBIOS_1030R	0x1030		/* 53c1030R */
#define	PCI_PRODUCT_SYMBIOS_1030ZC	0x0031		/* 53c1030ZC */
#define	PCI_PRODUCT_SYMBIOS_1035	0x0040		/* 53c1035 */
#define	PCI_PRODUCT_SYMBIOS_1035ZC	0x0041		/* 53c1035ZC */
#define	PCI_PRODUCT_SYMBIOS_SAS1064	0x0050		/* SAS1064 */
#define	PCI_PRODUCT_SYMBIOS_SAS1068	0x0054		/* SAS1068 */
#define	PCI_PRODUCT_SYMBIOS_SAS1068_2	0x0055		/* SAS1068 */
#define	PCI_PRODUCT_SYMBIOS_SAS1064E	0x0056		/* SAS1064E */
#define	PCI_PRODUCT_SYMBIOS_SAS1064E_2	0x0057		/* SAS1064E */
#define	PCI_PRODUCT_SYMBIOS_SAS1068E	0x0058		/* SAS1068E */
#define	PCI_PRODUCT_SYMBIOS_SAS1068E_2	0x0059		/* SAS1068E */
#define	PCI_PRODUCT_SYMBIOS_SAS1066E	0x005A		/* SAS1066E */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_2208	0x005B		/* MegaRAID SAS2208 */
#define	PCI_PRODUCT_SYMBIOS_SAS1064A	0x005C		/* SAS1064A */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_3108	0x005d		/* MegaRAID SAS3108 */
#define	PCI_PRODUCT_SYMBIOS_SAS1066	0x005E		/* SAS1066 */
#define	PCI_PRODUCT_SYMBIOS_SAS1078	0x0060		/* SAS1078 PCI */
#define	PCI_PRODUCT_SYMBIOS_SAS1078_PCIE	0x0062		/* SAS1078 PCI Express */
#define	PCI_PRODUCT_SYMBIOS_SAS2116_1	0x0064		/* SAS2116 */
#define	PCI_PRODUCT_SYMBIOS_SAS2116_2	0x0065		/* SAS2116 */
#define	PCI_PRODUCT_SYMBIOS_SAS2308_3	0x006e		/* SAS2308 */
#define	PCI_PRODUCT_SYMBIOS_SAS2004	0x0070		/* SAS2004 */
#define	PCI_PRODUCT_SYMBIOS_SAS2008	0x0072		/* SAS2008 */
#define	PCI_PRODUCT_SYMBIOS_SAS2008_1	0x0073		/* MegaRAID SAS2008 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_3	0x0074		/* SAS2108 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_4	0x0076		/* SAS2108 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_5	0x0077		/* SAS2108 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_1	0x0078		/* MegaRAID SAS2108 CRYPTO GEN2 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_2	0x0079		/* MegaRAID SAS2108 GEN2 */
#define	PCI_PRODUCT_SYMBIOS_SAS1078DE	0x007c		/* SAS1078DE */
#define	PCI_PRODUCT_SYMBIOS_SAS2208_1	0x0080		/* SAS2208 */
#define	PCI_PRODUCT_SYMBIOS_SAS2208_2	0x0081		/* SAS2208 */
#define	PCI_PRODUCT_SYMBIOS_SAS2208_3	0x0082		/* SAS2208 */
#define	PCI_PRODUCT_SYMBIOS_SAS2208_4	0x0083		/* SAS2208 */
#define	PCI_PRODUCT_SYMBIOS_SAS2208_5	0x0084		/* SAS2208 */
#define	PCI_PRODUCT_SYMBIOS_SAS2208_6	0x0085		/* SAS2208 */
#define	PCI_PRODUCT_SYMBIOS_SAS2308_1	0x0086		/* SAS2308 */
#define	PCI_PRODUCT_SYMBIOS_SAS2308_2	0x0087		/* SAS2308 */
#define	PCI_PRODUCT_SYMBIOS_875J	0x008f		/* 53c875J */
#define	PCI_PRODUCT_SYMBIOS_FC909	0x0620		/* FC909 */
#define	PCI_PRODUCT_SYMBIOS_FC909A	0x0621		/* FC909A */
#define	PCI_PRODUCT_SYMBIOS_FC929	0x0622		/* FC929 */
#define	PCI_PRODUCT_SYMBIOS_FC929_1	0x0623		/* FC929 */
#define	PCI_PRODUCT_SYMBIOS_FC919	0x0624		/* FC919 */
#define	PCI_PRODUCT_SYMBIOS_FC919_1	0x0625		/* FC919 */
#define	PCI_PRODUCT_SYMBIOS_FC929X	0x0626		/* FC929X */
#define	PCI_PRODUCT_SYMBIOS_FC919X	0x0628		/* FC919X */
#define	PCI_PRODUCT_SYMBIOS_FC949X	0x0640		/* FC949X */
#define	PCI_PRODUCT_SYMBIOS_FC939X	0x0642		/* FC939X */
#define	PCI_PRODUCT_SYMBIOS_FC949E	0x0646		/* FC949E */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_320X	0x0407		/* LSI Megaraid SCSI 320-X */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_320E	0x0408		/* LSI Megaraid SCSI 320-E */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_300X	0x0409		/* LSI Megaraid SATA (300-6X/300-8X) */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_SAS	0x0411		/* MegaRAID SAS */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_VERDE_ZCR	0x0413		/* MegaRAID Verde ZCR */
#define	PCI_PRODUCT_SYMBIOS_PERC_4SC	0x1960		/* PERC 4/SC */

/* Packet Engines products */
#define	PCI_PRODUCT_SYMBIOS_PE_GNIC	0x0702		/* Packet Engines G-NIC Ethernet */

/* Parallels products */
#define	PCI_PRODUCT_PARALLELS_TOOLS	0x1112		/* Tools */
#define	PCI_PRODUCT_PARALLELS_VIDEO	0x1121		/* Video */
#define	PCI_PRODUCT_PARALLELS_VIDEO2	0x1131		/* Video II */

/* NEC products */
#define	PCI_PRODUCT_NEC_USB	0x0035		/* USB Host Controller */
#define	PCI_PRODUCT_NEC_VRC4173_CARDU	0x003e		/* VRC4173 PC-Card Unit */
#define	PCI_PRODUCT_NEC_POWERVR2	0x0046		/* PowerVR PCX2 */
#define	PCI_PRODUCT_NEC_PD72872	0x0063		/* uPD72872 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_NEC_PKUGX001	0x0074		/* PK-UG-X001 K56flex Modem */
#define	PCI_PRODUCT_NEC_PKUGX008	0x007d		/* PK-UG-X008 */
#define	PCI_PRODUCT_NEC_VRC4173_BCU	0x00a5		/* VRC4173 Bus Control Unit */
#define	PCI_PRODUCT_NEC_VRC4173_AC97U	0x00a6		/* VRC4173 AC97 Unit */
#define	PCI_PRODUCT_NEC_PD72870	0x00cd		/* uPD72870 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_NEC_PD72871	0x00ce		/* uPD72871 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_NEC_PD720100A	0x00e0		/* USB2 Host Controller */
#define	PCI_PRODUCT_NEC_PD720400	0x0125		/* uPD720400 PCI Express - PCI/PCI-X Bridge */
#define	PCI_PRODUCT_NEC_PD720200	0x0194		/* USB3 Host Controller */
#define	PCI_PRODUCT_NEC_VA26D	0x803c		/* Versa Pro LX VA26D */
#define	PCI_PRODUCT_NEC_VERSALX	0x8058		/* Versa LX */

/* Neomagic products */
#define	PCI_PRODUCT_NEOMAGIC_NMMG2070	0x0001		/* MagicGraph NM2070 */
#define	PCI_PRODUCT_NEOMAGIC_NMMG128V	0x0002		/* MagicGraph 128V */
#define	PCI_PRODUCT_NEOMAGIC_NMMG128ZV	0x0003		/* MagicGraph 128ZV */
#define	PCI_PRODUCT_NEOMAGIC_NMMG2160	0x0004		/* MagicGraph 128XD */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256AV_VGA	0x0005		/* MagicMedia 256AV VGA */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256ZX_VGA	0x0006		/* MagicMedia 256ZX VGA */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256XLP_AU	0x0016		/* MagicMedia 256XL+ Audio */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256AV_AU	0x8005		/* MagicMedia 256AV Audio */
#define	PCI_PRODUCT_NEOMAGIC_NMMM256ZX_AU	0x8006		/* MagicMedia 256ZX Audio */

/* NetChip (now PLX) products */
#define	PCI_PRODUCT_NETCHIP_NET2280	0x2280		/* NET2280 USB Device Controller */
#define	PCI_PRODUCT_NETCHIP_NET2282	0x2282		/* NET2282 USB Device Controller */

/* Netgear products */
#define	PCI_PRODUCT_NETGEAR_GA620	0x620a		/* GA620 1000baseSX Ethernet */
#define	PCI_PRODUCT_NETGEAR_GA620T	0x630a		/* GA620 1000baseT Ethernet */
#define	PCI_PRODUCT_NETGEAR_MA301	0x4100		/* MA301 PCI IEEE 802.11b */

/* Netmos products */
#define	PCI_PRODUCT_NETMOS_NM9805	0x9805		/* 1284 Printer Port */
#define	PCI_PRODUCT_NETMOS_NM9815	0x9815		/* Dual 1284 Printer Port */
#define	PCI_PRODUCT_NETMOS_NM9820	0x9820		/* Single UART */
#define	PCI_PRODUCT_NETMOS_NM9835	0x9835		/* Dual UART and 1284 Printer Port */
#define	PCI_PRODUCT_NETMOS_NM9845	0x9845		/* Quad UART and 1284 Printer Port */
#define	PCI_PRODUCT_NETMOS_NM9855	0x9855		/* 9855 Quad UART and 1284 Printer Port */
#define	PCI_PRODUCT_NETMOS_NM9865	0x9865		/* 9865 Quad UART and 1284 Printer Port */
#define	PCI_PRODUCT_NETMOS_MCS9990	0x9990		/* MCS9990 Quad USB 2.0 Port */
#define	PCI_PRODUCT_NETMOS_NM9900	0x9900		/* Single PCI-E UART */
#define	PCI_PRODUCT_NETMOS_NM9901	0x9901		/* Dual PCI-E UART */
#define	PCI_PRODUCT_NETMOS_NM9904	0x9904		/* Quad PCI-E UART */
#define	PCI_PRODUCT_NETMOS_NM9912	0x9912		/* Dual PCI-E UART and 1284 Printer Port */
#define	PCI_PRODUCT_NETMOS_NM9922	0x9922		/* Dual PCI-E UART */

/* Network Security Technologies */
#define	PCI_PRODUCT_NETSEC_7751	0x7751		/* 7751 */

/* NexGen products */
#define	PCI_PRODUCT_NEXGEN_NX82C501	0x4e78		/* NX82C501 Host-PCI Bridge */

/* NKK products */
#define	PCI_PRODUCT_NKK_NDR4600	0xa001		/* NDR4600 Host-PCI Bridge */

/* Nortel products */
#define	PCI_PRODUCT_NORTEL_BAYSTACK_21	0x1211		/* Baystack 21 (Accton MPX EN5038) */

/* Number Nine products */
#define	PCI_PRODUCT_NUMBER9_I128	0x2309		/* Imagine-128 */
#define	PCI_PRODUCT_NUMBER9_I128_2	0x2339		/* Imagine-128 II */

/* Nvidia products */
#define	PCI_PRODUCT_NVIDIA_RIVATNT	0x0020		/* RIVA TNT */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2	0x0028		/* RIVA TNT2 */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2U	0x0029		/* RIVA TNT2 Ultra */
#define	PCI_PRODUCT_NVIDIA_VANTA	0x002c		/* Vanta */
#define	PCI_PRODUCT_NVIDIA_RIVATNT2M64	0x002d		/* RIVA TNT2 Model 64 */
#define	PCI_PRODUCT_NVIDIA_MCP04_PCIB	0x0030		/* MCP04 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP04_SMBUS	0x0034		/* MCP04 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP04_IDE	0x0035		/* MCP04 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP04_SATA	0x0036		/* MCP04 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP04_LAN1	0x0037		/* MCP04 Ethernet */
#define	PCI_PRODUCT_NVIDIA_MCP04_LAN2	0x0038		/* MCP04 Ethernet */
#define	PCI_PRODUCT_NVIDIA_MCP04_SATA2	0x003e		/* MCP04 SATA */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6800U	0x0040		/* GeForce 6800 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6800	0x0041		/* GeForce 6800 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6800LE	0x0042		/* GeForce 6800 LE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6800GT	0x0045		/* GeForce 6800 GT */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PCIB1	0x0050		/* nForce4 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PCIB2	0x0051		/* nForce4 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SMBUS	0x0052		/* nForce4 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_ATA133	0x0053		/* nForce4 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SATA1	0x0054		/* nForce4 Serial ATA 1 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SATA2	0x0055		/* nForce4 Serial ATA 2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_LAN1	0x0056		/* nForce4 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_LAN2	0x0057		/* nForce4 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_AC	0x0059		/* nForce4 AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_USB	0x005a		/* nForce4 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_USB2	0x005b		/* nForce4 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PCI	0x005c		/* nForce4 PCI Host Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PCIE	0x005d		/* nForce4 PCIe Host Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_MEM	0x005e		/* nForce4 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PCIB	0x0060		/* nForce2 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_SMBUS	0x0064		/* nForce2 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_ATA133	0x0065		/* nForce2 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_LAN	0x0066		/* nForce2 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_OHCI	0x0067		/* nForce2 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_EHCI	0x0068		/* nForce2 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MCPT_AC	0x006a		/* nForce2 MCP-T AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MCPT_AP	0x006b		/* nForce2 MCP-T Audio Processing Unit */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PPB	0x006c		/* nForce2 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_FW	0x006e		/* nForce2 Firewire Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_PCIB	0x0080		/* nForce2 Ultra 400 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_SMBUS	0x0084		/* nForce2 Ultra 400 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_ATA133	0x0085		/* nForce2 Ultra 400 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_LAN1	0x0086		/* nForce2 Ultra 400 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_OHCI	0x0087		/* nForce2 Ultra 400 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_EHCI	0x0088		/* nForce2 Ultra 400 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_MCPT_AC	0x008a		/* nForce2 Ultra 400 AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_PPB	0x008b		/* nForce2 Ultra 400 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_LAN2	0x008c		/* nForce2 Ultra 400 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_SATA	0x008e		/* nForce2 Ultra 400 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_ALADDINTNT2	0x00a0		/* Aladdin TNT2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PCIB	0x00d0		/* nForce3 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PCHB	0x00d1		/* nForce3 Host-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PPB2	0x00d2		/* nForce3 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_SMBUS	0x00d4		/* nForce3 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_ATA133	0x00d5		/* nForce3 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN1	0x00d6		/* nForce3 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_OHCI	0x00d7		/* nForce3 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_EHCI	0x00d8		/* nForce3 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_MCPT_AC	0x00da		/* nForce3 MCP-T AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PPB	0x00dd		/* nForce3 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN4	0x00df		/* nForce3 Ethernet #4 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PCIB	0x00e0		/* nForce3 250 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PCHB	0x00e1		/* nForce3 250 Host-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_AGP	0x00e2		/* nForce3 250 AGP */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_SATA	0x00e3		/* nForce3 250 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_SMBUS	0x00e4		/* nForce3 250 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_ATA133	0x00e5		/* nForce3 250 ATA133 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_LAN	0x00e6		/* nForce3 250 Ethernet */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_OHCI	0x00e7		/* nForce3 250 USB Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_EHCI	0x00e8		/* nForce3 250 USB2 Host Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_MCPT_AC	0x00ea		/* nForce3 250 MCP-T AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PPB	0x00ed		/* nForce3 250 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_SATA2	0x00ee		/* nForce3 250 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_QUADROFX3400	0x00f8		/* Quadro FX 3400 */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_280NVS2	0x00fd		/* Quadro4 280 NVS */
#define	PCI_PRODUCT_NVIDIA_QUADROFX1300	0x00fe		/* Quadro FX 1300 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEPCX4300	0x00ff		/* GeForce PCX 4300 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE256	0x0100		/* GeForce 256 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEDDR	0x0101		/* GeForce DDR */
#define	PCI_PRODUCT_NVIDIA_QUADRO	0x0103		/* Quadro */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2MX	0x0110		/* GeForce2 MX */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2MX200	0x0111		/* GeForce2 MX 100/200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2GO	0x0112		/* GeForce2 Go */
#define	PCI_PRODUCT_NVIDIA_QUADRO2_MXR	0x0113		/* Quadro2 MXR/EX */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GT	0x0140		/* GeForce 6600 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600	0x0141		/* GeForce 6600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600_2	0x0142		/* GeForce 6600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GO	0x0144		/* GeForce 6600 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6610XL	0x0145		/* GeForce 6610 XL */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GO_2	0x0146		/* GeForce 6600 Go */
#define	PCI_PRODUCT_NVIDIA_QUADROFX5500	0x014d		/* Quadro FX 5500 */
#define	PCI_PRODUCT_NVIDIA_QUADROFX540	0x014e		/* Quadro FX 540 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6200	0x014f		/* GeForce 6200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2	0x0150		/* GeForce2 GTS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2DDR	0x0151		/* GeForce2 GTS (DDR) */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2BR	0x0152		/* GeForce2 GTS */
#define	PCI_PRODUCT_NVIDIA_QUADRO2	0x0153		/* Quadro2 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6200TC	0x0161		/* GeForce 6200TC */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6200LE	0x0163		/* GeForce 6200LE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_MX460	0x0170		/* GeForce4 MX 460 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_MX440	0x0171		/* GeForce4 MX 440 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_MX420	0x0172		/* GeForce4 MX 420 */
#define	PCI_PRODUCT_NVIDIA_GF4_MX440_SE	0x0173		/* GeForce4 MX 440 SE */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_500XGL	0x0178		/* Quadro4 500XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_200NVS	0x017a		/* Quadro4 200/400NVS */
#define	PCI_PRODUCT_NVIDIA_GF4_MX440_8X	0x0181		/* GeForce4 MX 440 (AGP8X) */
#define	PCI_PRODUCT_NVIDIA_GF4_MX440_SE_8X	0x0182		/* GeForce4 MX 440 SE (AGP8X) */
#define	PCI_PRODUCT_NVIDIA_GF4_MX420_8X	0x0183		/* GeForce4 MX 420 (AGP8X) */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_580XGL	0x0188		/* Quadro4 580 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_280NVS	0x018a		/* Quadro4 280 NVS */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_380XGL	0x018b		/* Quadro4 380 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADROFX4600	0x019e		/* Quadro FX 4600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2_IGP	0x01a0		/* GeForce2 Integrated GPU */
#define	PCI_PRODUCT_NVIDIA_NFORCE_PCHB	0x01a4		/* nForce PCI Host */
#define	PCI_PRODUCT_NVIDIA_NFORCE_DDR2	0x01aa		/* nForce 220 DDR */
#define	PCI_PRODUCT_NVIDIA_NFORCE_DDR	0x01ab		/* nForce 420 DDR */
#define	PCI_PRODUCT_NVIDIA_NFORCE_MEM	0x01ac		/* nForce 220/420 */
#define	PCI_PRODUCT_NVIDIA_NFORCE_MEM1	0x01ad		/* nForce 220/420 */
#define	PCI_PRODUCT_NVIDIA_NFORCE_APU	0x01b0		/* nForce Audio Processing Unit */
#define	PCI_PRODUCT_NVIDIA_NFORCE_MCP_AC	0x01b1		/* nForce MCP AC-97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE_ISA	0x01b2		/* nForce ISA */
#define	PCI_PRODUCT_NVIDIA_XBOX_SMBUS	0x01b4		/* Xbox nForce SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE_AGP	0x01b7		/* nForce AGP */
#define	PCI_PRODUCT_NVIDIA_NFORCE_PPB	0x01b8		/* nForce PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE_ATA100	0x01bc		/* nForce ATA100 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE_USB	0x01c2		/* nForce USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE_LAN	0x01c3		/* nForce Ethernet */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_7300LE	0x01d1		/* GeForce 7300 LE */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PCHB	0x01e0		/* nForce2 Host-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PPB2	0x01e8		/* nForce2 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM1	0x01eb		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM2	0x01ec		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM3	0x01ed		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM4	0x01ee		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM5	0x01ef		/* nForce2 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_GF4_MX_IGP	0x01f0		/* GeForce4 MX Integrated GPU */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3	0x0200		/* GeForce3 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3_TI200	0x0201		/* GeForce3 Ti 200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3_TI500	0x0202		/* GeForce3 Ti 500 */
#define	PCI_PRODUCT_NVIDIA_QUADRO_DCC	0x0203		/* Quadro DCC */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6150	0x0240		/* GeForce 6150 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_6150LE	0x0241		/* GeForce 6150 LE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_TI4600	0x0250		/* GeForce4 Ti 4600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_TI4400	0x0251		/* GeForce4 Ti 4400 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4_TI4200	0x0253		/* GeForce4 Ti 4200 */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_900XGL	0x0258		/* Quadro4 900XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_750XGL	0x0259		/* Quadro4 750XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_700XGL	0x025b		/* Quadro4 700XGL */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_PCIB	0x0260		/* nForce430 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_SMBUS	0x0264		/* nForce430 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_ATA133	0x0265		/* nForce430 ATA133 IDE Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_SATA1	0x0266		/* nForce430 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_SATA2	0x0267		/* nForce430 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_LAN1	0x0268		/* nForce430 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_LAN2	0x0269		/* nForce430 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_AC	0x026b		/* nForce430 AC-97 Audio Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_HDA	0x026c		/* nForce430 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_OHCI	0x026d		/* nForce430 USB Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_EHCI	0x026e		/* nForce430 USB2 Controller */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_PPB	0x026f		/* nForce430 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_HB	0x0270		/* nForce430 Host Bridge */
#define	PCI_PRODUCT_NVIDIA_NFORCE430_MC	0x0272		/* nForce430 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_C51_MC2	0x027e		/* C51 Memory Controller 2 */
#define	PCI_PRODUCT_NVIDIA_C51_MC3	0x027f		/* C51 Memory Controller 3 */
#define	PCI_PRODUCT_NVIDIA_GF4_TI_4800	0x0280		/* GeForce4 Ti 4800 */
#define	PCI_PRODUCT_NVIDIA_GF4_TI_4200_8X	0x0281		/* GeForce4 Ti 4200 (AGP8X) */
#define	PCI_PRODUCT_NVIDIA_GF4_TI_4800_SE	0x0282		/* GeForce4 Ti 4800 SE */
#define	PCI_PRODUCT_NVIDIA_GF4_TI_4200_GO	0x0286		/* GeForce4 Ti 4200 Go AGP 8x */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_980_XGL	0x0288		/* Quadro4 980 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_780_XGL	0x0289		/* Quadro4 780 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_1500	0x029e		/* Quadro FX 1500 */
#define	PCI_PRODUCT_NVIDIA_XBOXFB	0x02a0		/* Xbox Frame Buffer */
#define	PCI_PRODUCT_NVIDIA_XBOX_PCHB	0x02a5		/* Xbox nForce Host-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02F0	0x02f0		/* C51 Host Bridge (0x02f0) */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02F1	0x02f1		/* C51 Host Bridge (0x02f1) */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02F2	0x02f2		/* C51 Host Bridge (0x02f2) */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02F3	0x02f3		/* C51 Host Bridge (0x02f3) */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02F4	0x02f4		/* C51 Host Bridge (0x02f4) */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02F5	0x02f5		/* C51 Host Bridge (0x02f5) */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02F6	0x02f6		/* C51 Host Bridge (0x02f6) */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02F7	0x02f7		/* C51 Host Bridge (0x02f7) */
#define	PCI_PRODUCT_NVIDIA_C51_MC5	0x02f8		/* C51 Memory Controller 5 */
#define	PCI_PRODUCT_NVIDIA_C51_MC4	0x02f9		/* C51 Memory Controller 4 */
#define	PCI_PRODUCT_NVIDIA_C51_MC0	0x02fa		/* C51 Memory Controller 0 */
#define	PCI_PRODUCT_NVIDIA_C51_PPB_02FB	0x02fb		/* C51 PCI Express Bridge (0x02fb) */
#define	PCI_PRODUCT_NVIDIA_C51_PPB_02FC	0x02fc		/* C51 PCI Express Bridge (0x02fc) */
#define	PCI_PRODUCT_NVIDIA_C51_PPB_02FD	0x02fd		/* C51 PCI Express Bridge (0x02fd) */
#define	PCI_PRODUCT_NVIDIA_C51_MC1	0x02fe		/* C51 Memory Controller 1 */
#define	PCI_PRODUCT_NVIDIA_C51_HB_02FF	0x02ff		/* C51 Host Bridge (0x02ff) */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_FX5800U	0x0301		/* GeForce FX 5800 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_FX5800	0x0302		/* GeForce FX 5800 */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_2000	0x0308		/* Quadro FX 2000 */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_1000	0x0309		/* Quadro FX 1000 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5600U	0x0311		/* GeForce FX 5600 Ultra */
#define	PCI_PRODUCT_NVIDIA_GF_FX5600	0x0312		/* GeForce FX 5600 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5600_SE	0x0314		/* GeForce FX 5600 SE */
#define	PCI_PRODUCT_NVIDIA_GF_FX5200U	0x0321		/* GeForce FX 5200 Ultra */
#define	PCI_PRODUCT_NVIDIA_GF_FX5200	0x0322		/* GeForce FX 5200 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5200SE	0x0323		/* GeForce FX 5200SE */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_500	0x032B		/* Quadro FX 500 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5900U	0x0330		/* GeForce FX 5900 Ultra */
#define	PCI_PRODUCT_NVIDIA_GF_FX5900	0x0331		/* GeForce FX 5900 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5900XT	0x0332		/* GeForce FX 5900XT */
#define	PCI_PRODUCT_NVIDIA_GF_FX5950U	0x0333		/* GeForce FX 5950 Ultra */
#define	PCI_PRODUCT_NVIDIA_QUADRO_FX_3000	0x0338		/* Quadro FX 3000 */
#define	PCI_PRODUCT_NVIDIA_GF_FX5700_LE	0x0343		/* GeForce FX 5700 LE */
#define	PCI_PRODUCT_NVIDIA_MCP55_LPC2	0x0361		/* nForce MCP55 LPC Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA	0x0362		/* nForce MCP55 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP55_LPC	0x0364		/* nForce MCP55 LPC Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP55_SMB	0x0368		/* nForce MCP55 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_MEM	0x0369		/* nForce MCP55 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_MEM2	0x036a		/* nForce MCP55 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_IDE	0x036e		/* nForce MCP55 ATA133 IDE Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_OHCI	0x036c		/* nForce MCP55 OHCI USB Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_EHCI	0x036d		/* nForce MCP55 EHCI USB Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_PPB	0x0370		/* nForce MCP55 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP55_HDA	0x0371		/* nForce MCP55 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_LAN1	0x0372		/* nForce MCP55 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_LAN2	0x0373		/* nForce MCP55 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_PCIE	0x0377		/* nForce MCP55 PCI-Express 16x Port */
#define	PCI_PRODUCT_NVIDIA_MCP55_PCIE2	0x0378		/* nForce MCP55 PCI-Express 16x Port */
#define	PCI_PRODUCT_NVIDIA_MCP55_SATA	0x037e		/* nForce MCP55 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP55_SATA2	0x037f		/* nForce MCP55 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_GF_GO_7600	0x0398		/* GeForce Go 7600 */
#define	PCI_PRODUCT_NVIDIA_MCP61_ISA	0x03e0		/* nForce MCP61 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP61_HDA	0x03e4		/* nForce MCP61 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_LAN1	0x03e5		/* nForce MCP61 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_LAN2	0x03e6		/* nForce MCP61 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_SATA	0x03e7		/* nForce MCP61 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_PPB_1	0x03e8		/* nForce MCP61 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP61_PPB_2	0x03e9		/* nForce MCP61 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP61_MEM	0x03ea		/* nForce MCP61 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_SMB	0x03eb		/* nForce MCP61 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_IDE	0x03ec		/* nForce MCP61 ATA133 IDE Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_LAN3	0x03ee		/* nForce MCP61 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_LAN4	0x03ef		/* nForce MCP61 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_HDA2	0x03f0		/* nForce MCP61 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_OHCI	0x03f1		/* nForce MCP61 OHCI USB Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_EHCI	0x03f2		/* nForce MCP61 EHCI USB Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_PCI	0x03f3		/* nForce MCP61 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP61_SMC	0x03f4		/* nForce MCP61 System Management Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_MEM2	0x03f5		/* nForce MCP61 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_SATA2	0x03f6		/* nForce MCP61 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP61_SATA3	0x03f7		/* nForce MCP61 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_8600GTS	0x0400		/* GeForce 8600 GTS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_8600GT	0x0402		/* GeForce 8600 GT */
#define	PCI_PRODUCT_NVIDIA_GF_8500_GT	0x0421		/* GeForce 8500 GT */
#define	PCI_PRODUCT_NVIDIA_GF_8400M_GS	0x0427		/* GeForce 8400M GS */
#define	PCI_PRODUCT_NVIDIA_QUADRO_NVS140M	0x0429		/* Quadro NVS 140M */
#define	PCI_PRODUCT_NVIDIA_MCP65_ISA	0x0440		/* nForce MCP65 PCI-ISA Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP65_LPC1	0x0441		/* nForce MCP65 PCI-LPC Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP65_LPC2	0x0442		/* nForce MCP65 PCI-LPC Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP65_LPC3	0x0443		/* nForce MCP65 PCI-LPC Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP65_MEM	0x0444		/* nForce MCP65 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_MEM2	0x0445		/* nForce MCP65 Memory Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_SMB	0x0446		/* nForce MCP65 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_SMU	0x0447		/* nForce MCP65 System Management Unit */
#define	PCI_PRODUCT_NVIDIA_MCP65_IDE	0x0448		/* nForce MCP65 ATA133 IDE Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_PCI	0x0449		/* nForce MCP65 PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP65_HDA_1	0x044a		/* nForce MCP65 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_HDA_2	0x044b		/* nForce MCP65 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_AHCI_1	0x044c		/* nForce MCP65 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_AHCI_2	0x044d		/* nForce MCP65 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_AHCI_3	0x044e		/* nForce MCP65 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_AHCI_4	0x044f		/* nForce MCP65 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_LAN1	0x0450		/* nForce MCP65 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_LAN2	0x0451		/* nForce MCP65 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_LAN3	0x0452		/* nForce MCP65 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_LAN4	0x0453		/* nForce MCP65 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_USB_1	0x0454		/* nForce MCP65 USB Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_USB_2	0x0455		/* nForce MCP65 USB Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_USB_3	0x0456		/* nForce MCP65 USB Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_USB_4	0x0457		/* nForce MCP65 USB Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_PPB_1	0x0458		/* nForce MCP65 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP65_PPB_2	0x0459		/* nForce MCP65 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP65_PPB_3	0x045a		/* nForce MCP65 PCI-PCI Bridge */
#define	PCI_PRODUCT_NVIDIA_MCP65_SATA	0x045c		/* nForce MCP65 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_SATA2	0x045d		/* nForce MCP65 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_SATA3	0x045e		/* nForce MCP65 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP65_SATA4	0x045f		/* nForce MCP65 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_SMB	0x0542		/* nForce MCP67 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_LAN1	0x054c		/* nForce MCP67 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_LAN2	0x054d		/* nForce MCP67 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_LAN3	0x054e		/* nForce MCP67 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_LAN4	0x054f		/* nForce MCP67 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_SATA	0x0550		/* nForce MCP67 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_SATA2	0x0551		/* nForce MCP67 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_SATA3	0x0552		/* nForce MCP67 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_SATA4	0x0553		/* nForce MCP67 Serial ATA Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_1	0x0554		/* nForce MCP67 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_2	0x0555		/* nForce MCP67 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_3	0x0556		/* nForce MCP67 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_4	0x0557		/* nForce MCP67 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_5	0x0558		/* nForce MCP67 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_6	0x0559		/* nForce MCP67 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_7	0x055a		/* nForce MCP67 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_8	0x055b		/* nForce MCP67 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_HDA_1	0x055c		/* nForce MCP67 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_HDA_2	0x055d		/* nForce MCP67 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP67_IDE	0x0560		/* nForce MCP67 ATA133 IDE Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_IDE	0x056c		/* nForce MCP73 ATA133 IDE Controller */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_8800_GT	0x0611		/* GeForce 8800 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9800_GT	0x0614		/* GeForce 9800 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9600_GT	0x0622		/* GeForce 9600 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9500_GT	0x0640		/* GeForce 9500 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9300_GE_1	0x06e0		/* GeForce 9300 GE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8400GS	0x06e4		/* GeForce 8400 GS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE9300M_GS	0x06e9		/* GeForce 9300M GS */
#define	PCI_PRODUCT_NVIDIA_QUADRONVS150	0x06ea		/* Quadro NVS 150m */
#define	PCI_PRODUCT_NVIDIA_QUADRONVS160	0x06eb		/* Quadro NVS 160m */
#define	PCI_PRODUCT_NVIDIA_MCP77_IDE	0x0759		/* nForce MCP77 ATA133 IDE Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_LAN1	0x0760		/* nForce MCP77 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_LAN2	0x0761		/* nForce MCP77 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_LAN3	0x0762		/* nForce MCP77 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_LAN4	0x0763		/* nForce MCP77 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_HDA_1	0x0774		/* nForce MCP77 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_HDA_2	0x0775		/* nForce MCP77 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_HDA_3	0x0776		/* nForce MCP77 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_HDA_4	0x0777		/* nForce MCP77 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_SMB	0x07d8		/* nForce MCP73 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_LAN1	0x07dc		/* nForce MCP73 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_LAN2	0x07dd		/* nForce MCP73 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_LAN3	0x07de		/* nForce MCP73 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_LAN4	0x07df		/* nForce MCP73 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_1	0x07f0		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_2	0x07f1		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_3	0x07f2		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_4	0x07f3		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_5	0x07f4		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_6	0x07f5		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_7	0x07f6		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_8	0x07f7		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_9	0x07f8		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_10	0x07f9		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_11	0x07fa		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_12	0x07fb		/* nForce MCP73 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_HDA_1	0x07fc		/* nForce MCP73 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP73_HDA_2	0x07fd		/* nForce MCP73 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_MCP78S_SMB	0x0752		/* nForce MCP78S SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_210	0x0a65		/* GeForce 210 */
#define	PCI_PRODUCT_NVIDIA_MCP79_SMB	0x0aa2		/* nForce MCP79 SMBus Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_LAN1	0x0ab0		/* nForce MCP79 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_LAN2	0x0ab1		/* nForce MCP79 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_LAN3	0x0ab2		/* nForce MCP79 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_LAN4	0x0ab3		/* nForce MCP79 Gigabit Ethernet Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_1	0x0ad0		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_2	0x0ad1		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_3	0x0ad2		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_4	0x0ad3		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_5	0x0ad4		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_6	0x0ad5		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_7	0x0ad6		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_8	0x0ad7		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_9	0x0ad8		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_10	0x0ad9		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_11	0x0ada		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_12	0x0adb		/* nForce MCP77 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_1	0x0ab4		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_2	0x0ab5		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_3	0x0ab6		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_4	0x0ab7		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_5	0x0ab8		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_6	0x0ab9		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_7	0x0aba		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_8	0x0abb		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_9	0x0abc		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_10	0x0abd		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_11	0x0abe		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_12	0x0abf		/* nForce MCP79 AHCI Controller */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_210_HDA	0x0be3		/* GeForce 210 High Definition Audio Controller */
#define	PCI_PRODUCT_NVIDIA_GF_GT640M	0x0fd2		/* GeForce GT 640M */
#define	PCI_PRODUCT_NVIDIA_GF116	0x1244		/* GeForce GTX 550 Ti */

/* Nvidia & SGS-Thomson Microelectronics */
#define	PCI_PRODUCT_NVIDIA_SGS_RIVA128	0x0018		/* Riva 128 */

/* Oak Technologies products */
#define	PCI_PRODUCT_OAKTECH_OTI1007	0x0107		/* OTI107 */

/* Olicom products */
#define	PCI_PRODUCT_OLICOM_OC3136	0x0001		/* OC-3136/3137 Token-Ring 16/4 */
#define	PCI_PRODUCT_OLICOM_OC3139	0x0002		/* OC-3139f Fastload 16/4 Token Ring */
#define	PCI_PRODUCT_OLICOM_OC3140	0x0004		/* OC-3139/3140 RapidFire Token Ring 16/4 */
#define	PCI_PRODUCT_OLICOM_OC3250	0x0005		/* OC-3250 GoCard Token Ring 16/4 */
#define	PCI_PRODUCT_OLICOM_OC3530	0x0006		/* OC-3530 RapidFire Token Ring 100 */
#define	PCI_PRODUCT_OLICOM_OC3141	0x0007		/* OC-3141 RapidFire Token Ring 16/4 */
#define	PCI_PRODUCT_OLICOM_OC3540	0x0008		/* OC-3540 RapidFire HSTR 100/16/4 */
#define	PCI_PRODUCT_OLICOM_OC3150	0x000a		/* OC-3150 RapidFire Token-Ring 16/4 */
#define	PCI_PRODUCT_OLICOM_OC2805	0x0011		/* OC-2805 Ethernet */
#define	PCI_PRODUCT_OLICOM_OC2325	0x0012		/* OC-2325 Ethernet 10/100 */
#define	PCI_PRODUCT_OLICOM_OC2183	0x0013		/* OC-2183/2185 Ethernet */
#define	PCI_PRODUCT_OLICOM_OC2326	0x0014		/* OC-2326 10/100-TX Ethernet */
#define	PCI_PRODUCT_OLICOM_OC2327	0x0019		/* OC-2327/2350 10/100 Ethernet */
#define	PCI_PRODUCT_OLICOM_OC6151	0x0021		/* OC-6151/6152 155 Mbit ATM */
#define	PCI_PRODUCT_OLICOM_OCATM	0x0022		/* ATM */

/* Opti products */
#define	PCI_PRODUCT_OPTI_82C557	0xc557		/* 82C557 */
#define	PCI_PRODUCT_OPTI_82C558	0xc558		/* 82C558 */
#define	PCI_PRODUCT_OPTI_82C568	0xc568		/* 82C568 */
#define	PCI_PRODUCT_OPTI_82D568	0xd568		/* 82D568 */
#define	PCI_PRODUCT_OPTI_82C621	0xc621		/* 82C621 */
#define	PCI_PRODUCT_OPTI_82C822	0xc822		/* 82C822 */
#define	PCI_PRODUCT_OPTI_82C861	0xc861		/* 82C861 */
#define	PCI_PRODUCT_OPTI_82C700	0xc700		/* 82C700 */
#define	PCI_PRODUCT_OPTI_82C701	0xc701		/* 82C701 */

/* Oxford Semiconductor products */
#define	PCI_PRODUCT_OXFORDSEMI_VSCOM_PCI011H	0x8403		/* 011H */
#define	PCI_PRODUCT_OXFORDSEMI_OX16PCI954	0x9501		/* OX16PCI954 */
#define	PCI_PRODUCT_OXFORDSEMI_OX16PCI954K	0x9504		/* OX16PCI954K */
#define	PCI_PRODUCT_OXFORDSEMI_OXUPCI952	0x9505		/* OXuPCI952 */
#define	PCI_PRODUCT_OXFORDSEMI_EXSYS_EX41092	0x950a		/* Exsys EX-41092 */
#define	PCI_PRODUCT_OXFORDSEMI_OXCB950	0x950b		/* OXCB950 */
#define	PCI_PRODUCT_OXFORDSEMI_OXMPCI954	0x950c		/* OXmPCI954 */
#define	PCI_PRODUCT_OXFORDSEMI_OXMPCI954D	0x9510		/* OXmPCI954 Disabled */
#define	PCI_PRODUCT_OXFORDSEMI_EXSYS_EX41098	0x9511		/* Exsys EX-41098 */
#define	PCI_PRODUCT_OXFORDSEMI_OX16PCI954P	0x9513		/* OX16PCI954 Parallel */
#define	PCI_PRODUCT_OXFORDSEMI_OX16PCI952	0x9521		/* OX16PCI952 */
#define	PCI_PRODUCT_OXFORDSEMI_OX16PCI952P	0x9523		/* OX16PCI952 Parallel */
#define	PCI_PRODUCT_OXFORDSEMI_OX16PCI958	0x9538		/* OX16PCI958 */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952_0	0xc101		/* OXPCIe952 */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952_1	0xc105		/* OXPCIe952 */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952P	0xc110		/* OXPCIe952 Parallel */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952_2S	0xc120		/* OXPCIe952 2 Serial */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952_2	0xc124		/* OXPCIe952 */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952_3	0xc140		/* OXPCIe952 */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952_4	0xc141		/* OXPCIe952 */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952_5	0xc144		/* OXPCIe952 */
#define	PCI_PRODUCT_OXFORDSEMI_OXPCIE952_6	0xc145		/* OXPCIe952 */

/* Packet Engines products */
#define	PCI_PRODUCT_PACKETENGINES_GNICII	0x0911		/* G-NIC II Ethernet */

/* pcHDTV products */
#define	PCI_PRODUCT_PCHDTV_HD2000	0x2000		/* HD-2000 HDTV Video Capture */
#define	PCI_PRODUCT_PCHDTV_HD5500	0x5500		/* HD-5500 HDTV Video Capture */

/* PC Tech products */
#define	PCI_PRODUCT_PCTECH_RZ1000	0x1000		/* RZ1000 */

/* Peak System Technik products */
#define	PCI_PRODUCT_PEAK_PCAN	0x0001		/* PCAN CAN Controller */

/* Pericom Semiconductor products */
#define	PCI_PRODUCT_PERICOM_P17C9X110	0xe110		/* P17C9X110 PCIe to PCI Bridge */
#define	PCI_PRODUCT_PERICOM_P17C9X	0xe111		/* P17C9X PCIe to PCI Bridge */

/* Phobos products */
#define	PCI_PRODUCT_PHOBOS_P1000	0x1000		/* P1000 Gigabit Ethernet */

/* Planex products */
#define	PCI_PRODUCT_PLANEX_FNW_3603_TX	0xab06		/* FNW-3603-TX 10/100 Ethernet */
#define	PCI_PRODUCT_PLANEX_FNW_3800_TX	0xab07		/* FNW-3800-TX 10/100 Ethernet */

/* PLX Technology products */
#define	PCI_PRODUCT_PLX_PCI_400	0x1077		/* VScom PCI-400 4 port serial */
#define	PCI_PRODUCT_PLX_PCI_800	0x1076		/* VScom PCI-800 8 port serial */
#define	PCI_PRODUCT_PLX_PCI_200	0x1103		/* VScom PCI-200 2 port serial */
#define	PCI_PRODUCT_PLX_PEX_8111	0x8111		/* PEX 8111 PCIe-to-PCI Bridge */
#define	PCI_PRODUCT_PLX_PEX_8112	0x8112		/* PEX 8112 PCIe-to-PCI Bridge */
#define	PCI_PRODUCT_PLX_PEX_8114	0x8114		/* PEX 8114 PCIe-to-PCI/PCI-X Bridge */
#define	PCI_PRODUCT_PLX_9030	0x9030		/* 9030 I/O Accelrator */
#define	PCI_PRODUCT_PLX_9050	0x9050		/* 9050 I/O Accelrator */
#define	PCI_PRODUCT_PLX_9054	0x9054		/* 9054 I/O Accelerator */
#define	PCI_PRODUCT_PLX_9060ES	0x906e		/* 9060ES PCI Bus Controller */
#define	PCI_PRODUCT_PLX_9656	0x9656		/* 9656 I/O Accelerator */
#define	PCI_PRODUCT_PLX_9656FPBGA	0x5601		/* 9656 I/O Accelerator FPBGA */

/* Powerhouse Systems products */
#define	PCI_PRODUCT_POWERHOUSE_POWERTOP	0x6037		/* PowerTop PowerPC System Controller */
#define	PCI_PRODUCT_POWERHOUSE_POWERPRO	0x6073		/* PowerPro PowerPC System Controller */

/* ProLAN products - XXX better descriptions */
#define	PCI_PRODUCT_PROLAN_NE2KETHER	0x1980		/* Ethernet */

/* Promise products */
#define	PCI_PRODUCT_PROMISE_PDC20265	0x0d30		/* PDC20265 Ultra/66 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20263	0x0d38		/* PDC20263 Ultra/66 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20275	0x1275		/* PDC20275 Ultra/133 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20318	0x3318		/* PDC20318 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20319	0x3319		/* PDC20319 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20371	0x3371		/* PDC20371 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20379	0x3372		/* PDC20379 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20378	0x3373		/* PDC20378 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20375	0x3375		/* PDC20375 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20376	0x3376		/* PDC20376 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20377	0x3377		/* PDC20377 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC40719	0x3515		/* PDC40719 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC40519	0x3519		/* PDC40519 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20771	0x3570		/* PDC20771 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20571	0x3571		/* PDC20571 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20579	0x3574		/* PDC20579 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC40779	0x3577		/* PDC40779 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC40718	0x3d17		/* PDC40718 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC40518	0x3d18		/* PDC40518 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20775	0x3d73		/* PDC20775 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20575	0x3d75		/* PDC20575 Serial ATA Controller */
#define	PCI_PRODUCT_PROMISE_PDC20267	0x4d30		/* PDC20267 Ultra/100 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20246	0x4d33		/* PDC20246 Ultra/33 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20262	0x4d38		/* PDC20262 Ultra/66 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20268	0x4d68		/* PDC20268 Ultra/100 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20269	0x4d69		/* PDC20269 Ultra/133 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20276	0x5275		/* PDC20276 Ultra/133 IDE Controller */
#define	PCI_PRODUCT_PROMISE_DC5030	0x5300		/* DC5030 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20270	0x6268		/* PDC20270 Ultra/100 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20271	0x6269		/* PDC20271 Ultra/133 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20617	0x6617		/* PDC20617 Dual Ultra/133 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20620	0x6620		/* PDC20620 Dual Ultra/133 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20621	0x6621		/* PDC20621 Dual Ultra/133 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20618	0x6626		/* PDC20618 Dual Ultra/133 IDE Controler */
#define	PCI_PRODUCT_PROMISE_PDC20619	0x6629		/* PDC20619 Dual Ultra/133 IDE Controller */
#define	PCI_PRODUCT_PROMISE_PDC20277	0x7275		/* PDC20277 Ultra/133 IDE Controller */

/* Nanjing QinHeng Electronics */
#define	PCI_PRODUCT_QINHENG_CH352_2S	0x3253		/* CH352 2S */
#define	PCI_PRODUCT_QINHENG_CH353_4S	0x3453		/* CH353 4S */
#define	PCI_PRODUCT_QINHENG_CH356_8S	0x3853		/* CH356 8S */
#define	PCI_PRODUCT_QINHENG_CH356_6S	0x3873		/* CH356 6S */
#define	PCI_PRODUCT_QINHENG_CH353_2S1PAR	0x5046		/* CH353 2S, 1P (fixed address) */
#define	PCI_PRODUCT_QINHENG_CH352_1S1P	0x5053		/* CH352 1S, 1P */
#define	PCI_PRODUCT_QINHENG_CH357_4S	0x5334		/* CH357 4S */
#define	PCI_PRODUCT_QINHENG_CH358_4S1P	0x5334		/* CH358 4S, 1P */
#define	PCI_PRODUCT_QINHENG_CH358_8S	0x5338		/* CH358 8S */
#define	PCI_PRODUCT_QINHENG_CH359_16S	0x5838		/* CH359 16S */
#define	PCI_PRODUCT_QINHENG_CH353_2S1P	0x7053		/* CH353 2S, 1P */
#define	PCI_PRODUCT_QINHENG_CH356_4S1P	0x7073		/* CH356 4S, 1P */
#define	PCI_PRODUCT_QINHENG_CH355_4S	0x7173		/* CH355 4S */

/* Nanjing QinHeng Electronics (PCIe) */
#define	PCI_PRODUCT_QINHENG2_CH384_4S1P	0x3450		/* CH384 4S, 1P */
#define	PCI_PRODUCT_QINHENG2_CH384_4S	0x3470		/* CH384 4S */
#define	PCI_PRODUCT_QINHENG2_CH382_2S1P	0x3250		/* CH382 2S, 1P */
#define	PCI_PRODUCT_QINHENG2_CH382_2S	0x3253		/* CH382 2S */
#define	PCI_PRODUCT_QINHENG2_CH384_8S	0x3853		/* CH384 8S */
#define	PCI_PRODUCT_QINHENG2_CH384_28S	0x4353		/* CH384 28S */

/* QLogic products */
#define	PCI_PRODUCT_QLOGIC_QLA200	0x0119		/* QLA200 */
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020		/* ISP1020 */
#define	PCI_PRODUCT_QLOGIC_ISP1022	0x1022		/* ISP1022 */
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080		/* ISP1080 */
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240		/* ISP1240 */
#define	PCI_PRODUCT_QLOGIC_ISP1280	0x1280		/* ISP1280 */
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100		/* ISP2100 */
#define	PCI_PRODUCT_QLOGIC_ISP3022	0x3022		/* ISP4022 iSCSI TOE */
#define	PCI_PRODUCT_QLOGIC_ISP4022	0x4022		/* ISP4022 iSCSI TOE */

/* QUANCOM Electronic GmbH products */
#define	PCI_PRODUCT_QUANCOM_PWDOG1	0x0010		/* PWDOG1 */

/* Quantum Designs products */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8500	0x0001		/* 8500 */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8580	0x0002		/* 8580 */

/* QuickLogic products */
#define	PCI_PRODUCT_QUICKLOGIC_PCWATCHDOG	0x5030		/* PC Watchdog */

/* Qumranet products */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1000	0x1000		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1001	0x1001		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1002	0x1002		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1003	0x1003		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1004	0x1004		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1005	0x1005		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1006	0x1006		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1007	0x1007		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1008	0x1008		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1009	0x1009		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_100A	0x100a		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_100B	0x100b		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_100C	0x100c		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_100D	0x100d		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_100E	0x100e		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_100F	0x100f		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1010	0x1010		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1011	0x1011		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1012	0x1012		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1013	0x1013		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1014	0x1014		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1015	0x1015		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1016	0x1016		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1017	0x1017		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1018	0x1018		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1019	0x1019		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_101A	0x101a		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_101B	0x101b		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_101C	0x101c		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_101D	0x101d		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_101E	0x101e		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_101F	0x101f		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1020	0x1020		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1021	0x1021		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1022	0x1022		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1023	0x1023		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1024	0x1024		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1025	0x1025		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1026	0x1026		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1027	0x1027		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1028	0x1028		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1029	0x1029		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_102A	0x102a		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_102B	0x102b		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_102C	0x102c		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_102D	0x102d		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_102E	0x102e		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_102F	0x102f		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1030	0x1030		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1031	0x1031		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1032	0x1032		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1033	0x1033		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1034	0x1034		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1035	0x1035		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1036	0x1036		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1037	0x1037		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1038	0x1038		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_1039	0x1039		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_103A	0x103a		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_103B	0x103b		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_103C	0x103c		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_103D	0x103d		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_103E	0x103e		/* Virtio */
#define	PCI_PRODUCT_QUMRANET_VIRTIO_103F	0x103f		/* Virtio */

/* Rainbow Technologies products */
#define	PCI_PRODUCT_RAINBOW_CS200	0x0200		/* CryptoSwift 200 PKI Accelerator */

/* Ralink Technologies products */
#define	PCI_PRODUCT_RALINK_RT2460A	0x0101		/* RT2460A 802.11b */
#define	PCI_PRODUCT_RALINK_RT2560	0x0201		/* RT2560 802.11b/g */
#define	PCI_PRODUCT_RALINK_RT2561S	0x0301		/* RT2561S 802.11b/g */
#define	PCI_PRODUCT_RALINK_RT2561	0x0302		/* RT2561 802.11b/g */
#define	PCI_PRODUCT_RALINK_RT2661	0x0401		/* RT2661 802.11b/g/n */
#define	PCI_PRODUCT_RALINK_RT3090	0x3090		/* RT3090 802.11b/g/n */

/* RATOC Systems products */
#define	PCI_PRODUCT_RATOC_REXPCI31	0x0853		/* REX PCI-31/33 SCSI */

/* RDC Semiconductor products */
#define	PCI_PRODUCT_RDC_R1010_IDE	0x1010		/* R1010 IDE controller */
#define	PCI_PRODUCT_RDC_R1011_IDE	0x1011		/* R1011 IDE controller */
#define	PCI_PRODUCT_RDC_R1012_IDE	0x1012		/* R1012 IDE controller */
#define	PCI_PRODUCT_RDC_R6021_HB	0x6021		/* R6021 Host */
#define	PCI_PRODUCT_RDC_R6025_HB	0x6025		/* R6025 Host */
#define	PCI_PRODUCT_RDC_R6031_ISA	0x6031		/* R6031 PCI-ISA bridge */
#define	PCI_PRODUCT_RDC_PCIB	0x6036		/* R6036 PCI-ISA bridge */
#define	PCI_PRODUCT_RDC_R6040	0x6040		/* R6040 10/100 Ethernet */
#define	PCI_PRODUCT_RDC_R6060_OHCI	0x6060		/* R6060 USB OHCI */
#define	PCI_PRODUCT_RDC_R6061_EHCI	0x6061		/* R6061 USB EHCI */

/* Realtek products */
#define	PCI_PRODUCT_REALTEK_RTS5209	0x5209		/* RTS5209 PCI-E Card Reader */
#define	PCI_PRODUCT_REALTEK_RTS5227	0x5227		/* RTS5227 PCI-E Card Reader */
#define	PCI_PRODUCT_REALTEK_RTS5229	0x5229		/* RTS5229 PCI-E Card Reader */
#define	PCI_PRODUCT_REALTEK_RTS5249	0x5249		/* RTS5249 PCI-E Card Reader */
#define	PCI_PRODUCT_REALTEK_RTL8402	0x5286		/* RTL8402 PCI-E Card Reader */
#define	PCI_PRODUCT_REALTEK_RTL8411B	0x5287		/* RTL8411B PCI-E Card Reader */
#define	PCI_PRODUCT_REALTEK_RTL8411	0x5289		/* RTL8411 PCI-E Card Reader */
#define	PCI_PRODUCT_REALTEK_RT8029	0x8029		/* 8029 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8139D	0x8039		/* 8139D 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8100	0x8100		/* 8100 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8129	0x8129		/* 8129 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8101E	0x8136		/* 8100E/8101E/8102E 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8138	0x8138		/* 8138 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8139	0x8139		/* 8139 10/100 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8169SC	0x8167		/* 8169SC/8110SC 10/100/1000 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8168	0x8168		/* 8168/8111 10/100/1000 Ethernet */
#define	PCI_PRODUCT_REALTEK_RT8169	0x8169		/* 8169/8110 10/100/1000 Ethernet */
#define	PCI_PRODUCT_REALTEK_RTL8188CE	0x8176		/* RTL8188CE Wireless LAN 802.11n PCI-E NIC */
#define	PCI_PRODUCT_REALTEK_RTL8192CE	0x8178		/* RTL8192CE Wireless LAN 802.11n PCI-E NIC */
#define	PCI_PRODUCT_REALTEK_RTL8188EE	0x8179		/* RTL8188EE Wireless LAN 802.11n PCI-E NIC */
#define	PCI_PRODUCT_REALTEK_RT8180	0x8180		/* 8180 802.11b */
#define	PCI_PRODUCT_REALTEK_RT8185	0x8185		/* 8185 802.11a/b/g */

/* Red Hat products */
#define	PCI_PRODUCT_REDHAT_PPB	0x0001		/* Qemu PCI-PCI */
#define	PCI_PRODUCT_REDHAT_QXL	0x0100		/* QXL Video */

/* Renesas products */
#define	PCI_PRODUCT_RENESAS_SH7780	0x0002		/* SH7780 PCI Controller */
#define	PCI_PRODUCT_RENESAS_SH7785	0x0007		/* SH7785 PCI Controller */
#define	PCI_PRODUCT_RENESAS_SH7757_PBI	0x0011		/* SH7757 PCIe End-Point [PBI] */
#define	PCI_PRODUCT_RENESAS_SH7757_PPB	0x0012		/* SH7757 PCIe-PCI Bridge [PPB] */
#define	PCI_PRODUCT_RENESAS_SH7757_PS	0x0013		/* SH7757 PCIe Switch [PS] */
#define	PCI_PRODUCT_RENESAS_PD720201	0x0014		/* uPD720201 USB 3.0 Host Controller */
#define	PCI_PRODUCT_RENESAS_PD720202	0x0015		/* uPD720202 USB 3.0 Host Controller */

/* Ricoh products */
#define	PCI_PRODUCT_RICOH_Rx5C465	0x0465		/* 5C465 PCI-CardBus Bridge */
#define	PCI_PRODUCT_RICOH_Rx5C466	0x0466		/* 5C466 PCI-CardBus Bridge */
#define	PCI_PRODUCT_RICOH_Rx5C475	0x0475		/* 5C475 PCI-CardBus Bridge */
#define	PCI_PRODUCT_RICOH_RL5C476	0x0476		/* 5C476 PCI-CardBus Bridge */
#define	PCI_PRODUCT_RICOH_Rx5C477	0x0477		/* 5C477 PCI-CardBus Bridge */
#define	PCI_PRODUCT_RICOH_Rx5C478	0x0478		/* 5C478 PCI-CardBus Bridge */
#define	PCI_PRODUCT_RICOH_Rx5C551	0x0551		/* 5C551 PCI-CardBus Bridge/Firewire */
#define	PCI_PRODUCT_RICOH_Rx5C552	0x0552		/* 5C552 PCI-CardBus Bridge/Firewire */
#define	PCI_PRODUCT_RICOH_Rx5C592	0x0592		/* 5C592 PCI-CardBus Bridge/MS/SD/Firewire */
#define	PCI_PRODUCT_RICOH_Rx5C593	0x0593		/* 5C593 PCI-CardBus Bridge/MS/SD/Firewire */
#define	PCI_PRODUCT_RICOH_Rx5C821	0x0821		/* 5C821 PCI-CardBus Bridge/MS/SD/MMC/SC */
#define	PCI_PRODUCT_RICOH_Rx5C822	0x0822		/* 5C822 PCI-CardBus Bridge/MS/SD/MMC/SC */
#define	PCI_PRODUCT_RICOH_Rx5C832	0x0832		/* 5C832 PCI-SD/MMC/MMC+/MS/xD/Firewire */
#define	PCI_PRODUCT_RICOH_Rx5C843	0x0843		/* 5C843 PCI-CardBus Bridge/SD/MMC/MMC+/MS/xD/Firewire */
#define	PCI_PRODUCT_RICOH_Rx5C847	0x0847		/* 5C847 PCI-CardBus Bridge/SD/MMC/MMC+/MS/xD/Firewire */
#define	PCI_PRODUCT_RICOH_RxDPCC	0x0852		/* xD-Picture Card Controller */
#define	PCI_PRODUCT_RICOH_Rx5C853	0x0853		/* 5C853 PCI-CardBus Bridge/SD/MMC/MMC+/MS/xD/SC/Firewire */
#define	PCI_PRODUCT_RICOH_Rx5U230	0xe230		/* 5U230 FireWire/SD/MMC/xD/MS Controller */
#define	PCI_PRODUCT_RICOH_Rx5U822	0xe822		/* 5U822 SD/MMC Controller */
#define	PCI_PRODUCT_RICOH_Rx5U823	0xe823		/* 5U823 SD/MMC Controller */
#define	PCI_PRODUCT_RICOH_Rx5U832	0xe832		/* 5U832 Firewire Controller */
#define	PCI_PRODUCT_RICOH_Rx5C852	0xe852		/* 5C852 xD Controller */

/* RISCom (SDL Communications?) products */
#define	PCI_PRODUCT_RISCOM_N2	0x5568		/* N2 */

/* RNS products */
#define	PCI_PRODUCT_RNS_FDDI	0x2200		/* 2200 FDDI */

/* S2io products */
#define	PCI_PRODUCT_S2IO_XFRAME	0x5831		/* Xframe 10 Gigabit Ethernet Adapter */
#define	PCI_PRODUCT_S2IO_XFRAME2	0x5832		/* Xframe2 10 Gigabit Ethernet Adapter */
#define	PCI_PRODUCT_S2IO_XFRAME3	0x5833		/* Xframe3 10 Gigabit Ethernet Adapter */

/* S3 products */
#define	PCI_PRODUCT_S3_VIRGE	0x5631		/* ViRGE */
#define	PCI_PRODUCT_S3_TRIO32	0x8810		/* Trio32 */
#define	PCI_PRODUCT_S3_TRIO64	0x8811		/* Trio32/64 */
#define	PCI_PRODUCT_S3_AURORA64P	0x8812		/* Aurora64V+ */
#define	PCI_PRODUCT_S3_TRIO64UVP	0x8814		/* Trio64UV+ */
#define	PCI_PRODUCT_S3_VIRGE_VX	0x883d		/* ViRGE/VX */
#define	PCI_PRODUCT_S3_868	0x8880		/* 868 */
#define	PCI_PRODUCT_S3_928	0x88b0		/* 86C928 */
#define	PCI_PRODUCT_S3_864_0	0x88c0		/* 86C864-0 (\"Vision864\") */
#define	PCI_PRODUCT_S3_864_1	0x88c1		/* 86C864-1 (\"Vision864\") */
#define	PCI_PRODUCT_S3_864_2	0x88c2		/* 86C864-2 (\"Vision864\") */
#define	PCI_PRODUCT_S3_864_3	0x88c3		/* 86C864-3 (\"Vision864\") */
#define	PCI_PRODUCT_S3_964_0	0x88d0		/* 86C964-0 (\"Vision964\") */
#define	PCI_PRODUCT_S3_964_1	0x88d1		/* 86C964-1 (\"Vision964\") */
#define	PCI_PRODUCT_S3_964_2	0x88d2		/* 86C964-2 (\"Vision964\") */
#define	PCI_PRODUCT_S3_964_3	0x88d3		/* 86C964-3 (\"Vision964\") */
#define	PCI_PRODUCT_S3_968_0	0x88f0		/* 86C968-0 (\"Vision968\") */
#define	PCI_PRODUCT_S3_968_1	0x88f1		/* 86C968-1 (\"Vision968\") */
#define	PCI_PRODUCT_S3_968_2	0x88f2		/* 86C968-2 (\"Vision968\") */
#define	PCI_PRODUCT_S3_968_3	0x88f3		/* 86C968-3 (\"Vision968\") */
#define	PCI_PRODUCT_S3_TRIO64V2_DX	0x8901		/* Trio64V2/DX */
/* pcidatbase.com has this as 0x0551, I'd rather believe linux's 8902 */
#define	PCI_PRODUCT_S3_PLATO_PX	0x8902		/* Plato/PX */
#define	PCI_PRODUCT_S3_TRIO3D	0x8904		/* 86C365 Trio3D */
#define	PCI_PRODUCT_S3_VIRGE_DX	0x8a01		/* ViRGE/DX */
#define	PCI_PRODUCT_S3_VIRGE_GX2	0x8a10		/* ViRGE/GX2 */
#define	PCI_PRODUCT_S3_TRIO3D2X	0x8a13		/* Trio3D/2X */
#define	PCI_PRODUCT_S3_SAVAGE3D	0x8a20		/* Savage3D */
#define	PCI_PRODUCT_S3_SAVAGE3D_MV	0x8a21		/* Savage3D+MV */
#define	PCI_PRODUCT_S3_SAVAGE4	0x8a22		/* Savage4 */
#define	PCI_PRODUCT_S3_PROSAVAGE_KM133	0x8a26		/* ProSavage KM133 */
#define	PCI_PRODUCT_S3_VIRGE_MX	0x8c01		/* ViRGE/MX */
#define	PCI_PRODUCT_S3_VIRGE_MXP	0x8c03		/* ViRGE/MXP */
#define	PCI_PRODUCT_S3_SAVAGE_MX_MV	0x8c10		/* Savage/MX+MV */
#define	PCI_PRODUCT_S3_SAVAGE_MX	0x8c11		/* Savage/MX */
#define	PCI_PRODUCT_S3_SAVAGE_IX_MV	0x8c12		/* Savage/IX+MV */
#define	PCI_PRODUCT_S3_SAVAGE_IX	0x8c13		/* Savage/IX */
#define	PCI_PRODUCT_S3_SAVAGE_IXC	0x8c2e		/* Savage/IXC */
#define	PCI_PRODUCT_S3_SAVAGE2000	0x9102		/* Savage2000 */
#define	PCI_PRODUCT_S3_SONICVIBES	0xca00		/* SonicVibes */

/* SafeNet products */
#define	PCI_PRODUCT_SAFENET_SAFEXCEL	0x1141		/* SafeXcel */

/* Samsung Electronics products */
#define	PCI_PRODUCT_SAMSUNGELEC3_XP941	0xa800		/* XP941 M.2 SSD */
#define	PCI_PRODUCT_SAMSUNGELEC3_SM951	0xa801		/* SM951 M.2 SSD */

/* Samsung Semiconductor products */
#define	PCI_PRODUCT_SAMSUNGSEMI_KS8920	0x8920		/* KS8920 10/100 Ethernet */

/* Sandburst products */
#define	PCI_PRODUCT_SANDBURST_QE1000	0x0180		/* QE1000 */
#define	PCI_PRODUCT_SANDBURST_FE1000	0x0200		/* FE1000 */
/*product SANDBURST	SE1600	0x0100	SE1600*/

/* SEGA Enterprises products */
#define	PCI_PRODUCT_SEGA_BROADBAND	0x1234		/* Broadband Adapter */

/* ServerWorks products */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_LE_AGP	0x0005		/* CNB20-LE PCI/AGP Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB30_LE_PCI	0x0006		/* CNB30-LE PCI Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_LE_PCI	0x0007		/* CNB20-LE PCI Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_HE_PCI	0x0008		/* CNB20-HE PCI Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_HE_AGP	0x0009		/* CNB20-HE PCI/AGP Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CIOB_X	0x0010		/* CIOB-X PCI-X Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CMIC_HE	0x0011		/* CMIC-HE PCI/AGP Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB30_HE	0x0012		/* CNB30-HE PCI Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CNB20_HE_PCI2	0x0013		/* CNB20-HE PCI/AGP Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CMIC_LE	0x0014		/* CMIC-LE PCI/AGP Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CMIC_SL	0x0017		/* CMIC-SL PCI/AGP Bridge */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_PPB0	0x0036		/* HT1000 PCI/PCI-X Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CIOB_X2	0x0101		/* CIOB-X2 PCI-X Bridge */
#define	PCI_PRODUCT_SERVERWORKS_BCM5714	0x0103		/* BCM5714/BCM5715 Integral PCI-E to PCI-X Bridge */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_PPB1	0x0104		/* HT1000 PCI/PCI-X Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CIOB_E	0x0110		/* CIOB-E PCI-X Bridge */
#define	PCI_PRODUCT_SERVERWORKS_HT2100_PPB0	0x0140		/* HT2100 PCI-Express Bridge */
#define	PCI_PRODUCT_SERVERWORKS_HT2100_PPB1	0x0141		/* HT2100 PCI-Express Bridge */
#define	PCI_PRODUCT_SERVERWORKS_HT2100_PPB2	0x0142		/* HT2100 PCI-Express Bridge */
#define	PCI_PRODUCT_SERVERWORKS_HT2100_PPB3	0x0144		/* HT2100 PCI-Express Bridge */
#define	PCI_PRODUCT_SERVERWORKS_OSB4	0x0200		/* OSB4 South Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CSB5	0x0201		/* CSB5 South Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CSB6	0x0203		/* CSB6 South Bridge */
#define	PCI_PRODUCT_SERVERWORKS_HT1000SB	0x0205		/* HT1000SB South Bridge */
#define	PCI_PRODUCT_SERVERWORKS_OSB4_IDE	0x0211		/* OSB4 IDE */
#define	PCI_PRODUCT_SERVERWORKS_CSB5_IDE	0x0212		/* CSB5 IDE */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_IDE	0x0214		/* HT-1000 IDE Controller */
#define	PCI_PRODUCT_SERVERWORKS_CSB6_RAID	0x0213		/* CSB6 IDE/RAID */
#define	PCI_PRODUCT_SERVERWORKS_CSB6_IDE	0x0217		/* CSB6 IDE/RAID */
#define	PCI_PRODUCT_SERVERWORKS_OSB4_USB	0x0220		/* OSB4/CSB5 USB Host Controller */
#define	PCI_PRODUCT_SERVERWORKS_CSB6_USB	0x0221		/* CSB6 USB Host Controller */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_USB	0x0223		/* HT1000 USB */
#define	PCI_PRODUCT_SERVERWORKS_CSB5_LPC	0x0225		/* CSB5 ISA/LPC Bridge */
#define	PCI_PRODUCT_SERVERWORKS_CSB6_LPC	0x0227		/* CSB6 ISA/LPC Bridge */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_LPC	0x0234		/* HT1000 LPC */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_XIOAPIC	0x0235		/* HT1000 XIOAPIC */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_WDTIMER	0x0238		/* HT1000 Watchdog Timer */
#define	PCI_PRODUCT_SERVERWORKS_K2_SATA	0x0240		/* K2 SATA */
#define	PCI_PRODUCT_SERVERWORKS_FRODO4_SATA	0x0241		/* Frodo4 SATA */
#define	PCI_PRODUCT_SERVERWORKS_FRODO8_SATA	0x0242		/* Frodo8 SATA */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_SATA_1	0x024a		/* HT-1000 SATA */
#define	PCI_PRODUCT_SERVERWORKS_HT1000_SATA_2	0x024b		/* HT-1000 SATA */

/* SGI products */
#define	PCI_PRODUCT_SGI_IOC3	0x0003		/* IOC3 */
#define	PCI_PRODUCT_SGI_RAD1	0x0005		/* PsiTech RAD1 */
#define	PCI_PRODUCT_SGI_TIGON	0x0009		/* Tigon Gigabit Ethernet */

/* SGS-Thomson products */
#define	PCI_PRODUCT_SGSTHOMSON_2000	0x0008		/* STG 2000X */
#define	PCI_PRODUCT_SGSTHOMSON_1764	0x1746		/* STG 1764X */

/* Broadcom (SiByte) products */
#define	PCI_PRODUCT_SIBYTE_BCM1250_PCIHB	0x0001		/* BCM1250 PCI Host Bridge */
#define	PCI_PRODUCT_SIBYTE_BCM1250_LDTHB	0x0002		/* BCM1250 LDT Host Bridge */

/* Sigma Designs products */
#define	PCI_PRODUCT_SIGMA_HOLLYWOODPLUS	0x8300		/* REALmagic Hollywood-Plus MPEG-2 Decoder */

/* SIIG Inc products */
#define	PCI_PRODUCT_SIIG_CYBER10_S550	0x1000		/* Cyber10x Serial 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_S650	0x1001		/* Cyber10x Serial 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_S850	0x1002		/* Cyber10x Serial 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_IO550	0x1010		/* Cyber10x I/O 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_IO650	0x1011		/* Cyber10x I/O 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_IO850	0x1012		/* Cyber10x I/O 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_P	0x1020		/* Cyber10x Parallel PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2P	0x1021		/* Cyber10x Parallel Dual PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S550	0x1030		/* Cyber10x Serial Dual 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S650	0x1031		/* Cyber10x Serial Dual 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S850	0x1032		/* Cyber10x Serial Dual 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S1P550	0x1034		/* Cyber10x 2S1P 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S1P650	0x1035		/* Cyber10x 2S1P 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_2S1P850	0x1036		/* Cyber10x 2S1P 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_4S550	0x1050		/* Cyber10x 4S 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_4S650	0x1051		/* Cyber10x 4S 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER10_4S850	0x1052		/* Cyber10x 4S 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_S550	0x2000		/* Cyber20x Serial 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_S650	0x2001		/* Cyber20x Serial 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_S850	0x2002		/* Cyber20x Serial 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_IO550	0x2010		/* Cyber20x I/O 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_IO650	0x2011		/* Cyber20x I/O 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_IO850	0x2012		/* Cyber20x I/O 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_P	0x2020		/* Cyber20x Parallel PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2P	0x2021		/* Cyber20x Parallel Dual PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S550	0x2030		/* Cyber20x Serial Dual 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S650	0x2031		/* Cyber20x Serial Dual 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S850	0x2032		/* Cyber20x Serial Dual 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2P1S550	0x2040		/* Cyber20x 2P1S 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2P1S650	0x2041		/* Cyber20x 2P1S 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2P1S850	0x2042		/* Cyber20x 2P1S 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_4S550	0x2050		/* Cyber20x 4S 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_4S650	0x2051		/* Cyber20x 4S 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_4S850	0x2052		/* Cyber20x 4S 16850 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S1P550	0x2060		/* Cyber20x 2S1P 16550 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S1P650	0x2061		/* Cyber20x 2S1P 16650 PCI */
#define	PCI_PRODUCT_SIIG_CYBER20_2S1P850	0x2062		/* Cyber20x 2S1P 16850 PCI */
#define	PCI_PRODUCT_SIIG_PS8000P550	0x2080		/* PCI Serial 8000 Plus 16550 */
#define	PCI_PRODUCT_SIIG_PS8000P650	0x2081		/* PCI Serial 8000 Plus 16650 */
#define	PCI_PRODUCT_SIIG_PS8000P850	0x2082		/* PCI Serial 8000 Plus 16850 */

/* Silicon Integrated System products */
#define	PCI_PRODUCT_SIS_86C201	0x0001		/* 86C201 */
#define	PCI_PRODUCT_SIS_86C202	0x0002		/* 86C202 */
#define	PCI_PRODUCT_SIS_86C205	0x0005		/* 86C205 */
#define	PCI_PRODUCT_SIS_85C503	0x0008		/* 85C503 or 5597/5598 ISA Bridge */
#define	PCI_PRODUCT_SIS_600PMC	0x0009		/* 600 Power Mngmt Controller */
#define	PCI_PRODUCT_SIS_180_SATA	0x0180		/* 180 SATA Controller */
#define	PCI_PRODUCT_SIS_181_SATA	0x0181		/* 181 SATA Controller */
#define	PCI_PRODUCT_SIS_182_SATA	0x0182		/* 182 SATA Controller */
#define	PCI_PRODUCT_SIS_183_SATA	0x0183		/* 183 SATA controller */
#define	PCI_PRODUCT_SIS_190	0x0190		/* 190 Ethernet */
#define	PCI_PRODUCT_SIS_191	0x0191		/* 191 Gigabit Ethernet */
#define	PCI_PRODUCT_SIS_5597_VGA	0x0200		/* 5597/5598 Integrated VGA */
#define	PCI_PRODUCT_SIS_300	0x0300		/* 300/305 AGP VGA */
#define	PCI_PRODUCT_SIS_315PRO_VGA	0x0325		/* 315 Pro VGA */
#define	PCI_PRODUCT_SIS_85C501	0x0406		/* 85C501 */
#define	PCI_PRODUCT_SIS_85C496	0x0496		/* 85C496 */
#define	PCI_PRODUCT_SIS_530HB	0x0530		/* 530 Host-PCI Bridge */
#define	PCI_PRODUCT_SIS_540HB	0x0540		/* 540 Host-PCI Bridge */
#define	PCI_PRODUCT_SIS_550HB	0x0550		/* 550 Host-PCI Bridge */
#define	PCI_PRODUCT_SIS_85C601	0x0601		/* 85C601 */
#define	PCI_PRODUCT_SIS_620	0x0620		/* 620 Host Bridge */
#define	PCI_PRODUCT_SIS_630	0x0630		/* 630 Host Bridge */
#define	PCI_PRODUCT_SIS_633	0x0633		/* 633 Host Bridge */
#define	PCI_PRODUCT_SIS_635	0x0635		/* 635 Host Bridge */
#define	PCI_PRODUCT_SIS_640	0x0640		/* 640 Host Bridge */
#define	PCI_PRODUCT_SIS_645	0x0645		/* 645 Host Bridge */
#define	PCI_PRODUCT_SIS_646	0x0646		/* 646 Host Bridge */
#define	PCI_PRODUCT_SIS_648	0x0648		/* 648 Host Bridge */
#define	PCI_PRODUCT_SIS_650	0x0650		/* 650 Host Bridge */
#define	PCI_PRODUCT_SIS_651	0x0651		/* 651 Host Bridge */
#define	PCI_PRODUCT_SIS_652	0x0652		/* 652 Host Bridge */
#define	PCI_PRODUCT_SIS_655	0x0655		/* 655 Host Bridge */
#define	PCI_PRODUCT_SIS_658	0x0658		/* 658 Host Bridge */
#define	PCI_PRODUCT_SIS_661	0x0661		/* 661 Host Bridge */
#define	PCI_PRODUCT_SIS_671	0x0671		/* 671 Host Bridge */
#define	PCI_PRODUCT_SIS_730	0x0730		/* 730 Host Bridge */
#define	PCI_PRODUCT_SIS_733	0x0733		/* 733 Host Bridge */
#define	PCI_PRODUCT_SIS_735	0x0735		/* 735 Host Bridge */
#define	PCI_PRODUCT_SIS_740	0x0740		/* 740 Host Bridge */
#define	PCI_PRODUCT_SIS_741	0x0741		/* 741 Host Bridge */
#define	PCI_PRODUCT_SIS_745	0x0745		/* 745 Host Bridge */
#define	PCI_PRODUCT_SIS_746	0x0746		/* 746 Host Bridge */
#define	PCI_PRODUCT_SIS_748	0x0748		/* 748 Host Bridge */
#define	PCI_PRODUCT_SIS_750	0x0750		/* 750 Host Bridge */
#define	PCI_PRODUCT_SIS_751	0x0751		/* 751 Host Bridge */
#define	PCI_PRODUCT_SIS_752	0x0752		/* 752 Host Bridge */
#define	PCI_PRODUCT_SIS_755	0x0755		/* 755 Host Bridge */
#define	PCI_PRODUCT_SIS_756	0x0756		/* 756 Host Bridge */
#define	PCI_PRODUCT_SIS_760	0x0760		/* 760 Host Bridge */
#define	PCI_PRODUCT_SIS_761	0x0761		/* 761 Host Bridge */
#define	PCI_PRODUCT_SIS_900	0x0900		/* 900 10/100 Ethernet */
#define	PCI_PRODUCT_SIS_961	0x0961		/* 961 Host Bridge */
#define	PCI_PRODUCT_SIS_962	0x0962		/* 962 Host Bridge */
#define	PCI_PRODUCT_SIS_963	0x0963		/* 963 Host Bridge */
#define	PCI_PRODUCT_SIS_964	0x0964		/* 964 Host Bridge */
#define	PCI_PRODUCT_SIS_965	0x0965		/* 965 Host Bridge */
#define	PCI_PRODUCT_SIS_966	0x0966		/* 966 Host Bridge */
#define	PCI_PRODUCT_SIS_968	0x0968		/* 968 Host Bridge */
#define	PCI_PRODUCT_SIS_5597_IDE	0x5513		/* 5597/5598 IDE Controller */
#define	PCI_PRODUCT_SIS_5597_HB	0x5597		/* 5597/5598 Host Bridge */
#define	PCI_PRODUCT_SIS_530VGA	0x6306		/* 530 GUI Accelerator+3D */
#define	PCI_PRODUCT_SIS_6325	0x6325		/* 6325 AGP VGA */
#define	PCI_PRODUCT_SIS_6326	0x6326		/* 6326 AGP VGA */
#define	PCI_PRODUCT_SIS_5597_USB	0x7001		/* 5597/5598 USB Host Controller */
#define	PCI_PRODUCT_SIS_7002	0x7002		/* 7002 USB 2.0 Host Controller */
#define	PCI_PRODUCT_SIS_7012_AC	0x7012		/* 7012 AC-97 Sound */
#define	PCI_PRODUCT_SIS_7016	0x7016		/* 7016 10/100 Ethernet */
#define	PCI_PRODUCT_SIS_7018	0x7018		/* 7018 Sound */
#define	PCI_PRODUCT_SIS_7502	0x7502		/* 7502 HD audio */

/* Silicon Motion products */
#define	PCI_PRODUCT_SILMOTION_SM502	0x0501		/* Voyager GX */
#define	PCI_PRODUCT_SILMOTION_SM710	0x0710		/* LynxEM */
#define	PCI_PRODUCT_SILMOTION_SM712	0x0712		/* LynxEM+ */
#define	PCI_PRODUCT_SILMOTION_SM720	0x0720		/* Lynx3DM */
#define	PCI_PRODUCT_SILMOTION_SM810	0x0810		/* LynxE */
#define	PCI_PRODUCT_SILMOTION_SM811	0x0811		/* LynxE */
#define	PCI_PRODUCT_SILMOTION_SM820	0x0820		/* Lynx3D */
#define	PCI_PRODUCT_SILMOTION_SM910	0x0910		/* Lynx */

/* SMC products */
#define	PCI_PRODUCT_SMC_37C665	0x1000		/* FDC37C665 */
#define	PCI_PRODUCT_SMC_37C922	0x1001		/* FDC37C922 */
#define	PCI_PRODUCT_SMC_83C170	0x0005		/* 83C170 (\"EPIC/100\") Fast Ethernet */
#define	PCI_PRODUCT_SMC_83C175	0x0006		/* 83C175 (\"EPIC/100\") Fast Ethernet */

/* Solidum Systems */
#define	PCI_PRODUCT_SOLIDUM_AMD971	0x2000		/* SNP8023: AMD 971 */
#define	PCI_PRODUCT_SOLIDUM_CLASS802	0x8023		/* SNP8023: Classifier Engine */
#define	PCI_PRODUCT_SOLIDUM_PAXWARE1100	0x1100		/* PAX.ware 1100 Dual Gb Classifier Engine */

/* Sony products */
#define	PCI_PRODUCT_SONY_CXD1947A	0x8009		/* CXD1947A IEEE 1394 Host Controller */
#define	PCI_PRODUCT_SONY_CXD3222	0x8039		/* CXD3222 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_SONY_MEMSTICK	0x808a		/* Memory Stick I/F Controller */

/* Sun Microsystems products */
#define	PCI_PRODUCT_SUN_EBUS	0x1000		/* PCIO Ebus2 */
#define	PCI_PRODUCT_SUN_HMENETWORK	0x1001		/* PCIO Happy Meal Ethernet */
#define	PCI_PRODUCT_SUN_EBUSIII	0x1100		/* PCIO Ebus2 (US III) */
#define	PCI_PRODUCT_SUN_ERINETWORK	0x1101		/* ERI Ethernet */
#define	PCI_PRODUCT_SUN_FIREWIRE	0x1102		/* FireWire Controller */
#define	PCI_PRODUCT_SUN_USB	0x1103		/* USB Controller */
#define	PCI_PRODUCT_SUN_GEMNETWORK	0x2bad		/* GEM Gigabit Ethernet */
#define	PCI_PRODUCT_SUN_SIMBA	0x5000		/* Simba PCI Bridge */
#define	PCI_PRODUCT_SUN_5821	0x5454		/* BCM5821 */
#define	PCI_PRODUCT_SUN_SCA1K	0x5455		/* Crypto Accelerator 1000 */
#define	PCI_PRODUCT_SUN_PSYCHO	0x8000		/* psycho PCI Controller */
#define	PCI_PRODUCT_SUN_MS_IIep	0x9000		/* microSPARC IIep PCI */
#define	PCI_PRODUCT_SUN_US_IIi	0xa000		/* UltraSPARC IIi PCI */
#define	PCI_PRODUCT_SUN_US_IIe	0xa001		/* UltraSPARC IIe PCI */
#define	PCI_PRODUCT_SUN_CASSINI	0xabba		/* Cassini Gigabit Ethernet */

/* Sundance Technology products */
#define	PCI_PRODUCT_SUNDANCETI_IP100A	0x0200		/* IP100A 10/100 Ethernet */
#define	PCI_PRODUCT_SUNDANCETI_ST201	0x0201		/* ST201 10/100 Ethernet */
#define	PCI_PRODUCT_SUNDANCETI_ST1023	0x1023		/* ST1023 Gigabit Ethernet */
#define	PCI_PRODUCT_SUNDANCETI_ST2021	0x2021		/* ST2021 Gigabit Ethernet */

/* SUNIX products */
#define	PCI_PRODUCT_SUNIX2_0001	0x0001		/* Matrix serial adapter */
#define	PCI_PRODUCT_SUNIX2_SER5XXXX	0x1999		/* SER5xxx multiport serial */
#define	PCI_PRODUCT_SUNIX_PCI2S550	0x7168		/* PCI2S550 multiport serial */
#define	PCI_PRODUCT_SUNIX_SUN1888	0x7268		/* SUN1888 multiport parallel */

/* Surecom Technology products */
#define	PCI_PRODUCT_SURECOM_NE34	0x0e34		/* NE-34 Ethernet */

/* Syba */
#define	PCI_PRODUCT_SYBA_4S2P	0x0781		/* 4S2P */
#define	PCI_PRODUCT_SYBA_4S	0x0786		/* 4S */

/* Symphony Labs products */
#define	PCI_PRODUCT_SYMPHONY_82C101	0x0001		/* 82C101 */
#define	PCI_PRODUCT_SYMPHONY_82C103	0x0103		/* 82C103 */
#define	PCI_PRODUCT_SYMPHONY_82C105	0x0105		/* 82C105 */
#define	PCI_PRODUCT_SYMPHONY2_82C101	0x0001		/* 82C101 */
#define	PCI_PRODUCT_SYMPHONY_83C553	0x0565		/* 83C553 PCI-ISA Bridge */

/* System Base products */
#define	PCI_PRODUCT_SYSTEMBASE_SB16C1054	0x0004		/* SB16C1054 UARTs */
#define	PCI_PRODUCT_SYSTEMBASE_SB16C1058	0x0008		/* SB16C1058 UARTs */
#define	PCI_PRODUCT_SYSTEMBASE_SB16C1050	0x4d02		/* SB16C1050 UARTs */

/* Schneider & Koch (really SysKonnect) products */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SKNET_FDDI	0x4000		/* SK-NET FDDI-xP */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SKNET_GE	0x4300		/* SK-NET GE */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9821v2	0x4320		/* SK-9821 v2.0 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9DX1	0x4400		/* SK-NET SK-9DX1 Gigabit Ethernet */
/* These next two are are really subsystem IDs */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9D21	0x4421		/* SK-9D21 1000BASE-T */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9D41	0x4441		/* SK-9D41 1000BASE-X */

#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9SXX	0x9000		/* SK-9Sxx Gigabit Ethernet */
/* This next entry is used for both single-port (SK-9E21D) and dual-port
 * (SK-9E22) gig-e based on Marvell Yukon-2, with PCI revision	0x17 for
 * the single-port and 0x12 for the	dual-port.
 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK_9E21	0x9e00		/* SK-9E21D/SK-9E22 1000base-T */

/* Tamarack Microelectronics */
#define	PCI_PRODUCT_TAMARACK_TC9021	0x1021		/* TC9021 Gigabit Ethernet */
#define	PCI_PRODUCT_TAMARACK_TC9021_ALT	0x9021		/* TC9021 Gigabit Ethernet (alt ID) */

/* Tandem Computers */
#define	PCI_PRODUCT_TANDEM_SERVERNETII	0x0005		/* ServerNet II VIA Adapter */

/* Tekram Technology products (1st PCI Vendor ID) */
#define	PCI_PRODUCT_TEKRAM_DC290	0xdc29		/* DC-290(M) */

/* Tekram Technology products (2nd PCI Vendor ID) */
#define	PCI_PRODUCT_TEKRAM2_DC690C	0x690c		/* DC-690C */
#define	PCI_PRODUCT_TEKRAM2_DC315	0x0391		/* DC-315/DC-395 */

/* Texas Instruments products */
#define	PCI_PRODUCT_TI_TLAN	0x0500		/* TLAN */
#define	PCI_PRODUCT_TI_TVP4020	0x3d07		/* TVP4020 Permedia 2 */
#define	PCI_PRODUCT_TI_TSB12LV21	0x8000		/* TSB12LV21 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB12LV22	0x8009		/* TSB12LV22 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4450LYNX	0x8011		/* PCI4450 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI4410LYNX	0x8017		/* PCI4410 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_TSB12LV23	0x8019		/* TSB12LV23 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB12LV26	0x8020		/* TSB12LV26 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB43AA22	0x8021		/* TSB43AA22 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB43AA22A	0x8023		/* TSB43AA22/A IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB43AA23	0x8024		/* TSB43AA23 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_TSB43AB21	0x8026		/* TSB43AA21 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4451LYNX	0x8027		/* PCI4451 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI4510LYNX	0x8029		/* PCI4510 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI4520LYNX	0x802A		/* PCI4520 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI7410LYNX	0x802B		/* PCI7[4-6]10 IEEE 1394 Host Controller w/ PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI72111CB	0x8031		/* PCI7x21/7x11 Cardbus Controller */
#define	PCI_PRODUCT_TI_PCI72111FW	0x8032		/* PCI7x21/7x11 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI72111FM	0x8033		/* PCI7x21/7x11 Integrated FlashMedia Controller */
#define	PCI_PRODUCT_TI_PCI72111SD	0x8034		/* PCI7x21/7x11 SD Card Controller */
#define	PCI_PRODUCT_TI_PCI72111SM	0x8035		/* PCI7x21/7x11 SM Card Controller */
#define	PCI_PRODUCT_TI_PCI6515A	0x8036		/* PCI6515A Cardbus Controller */
#define	PCI_PRODUCT_TI_PCI6515ASM	0x8038		/* PCI6515A Cardbus Controller (Smart Card mode) */
#define	PCI_PRODUCT_TI_PCIXX12CB	0x8039		/* PCIXX12 Cardbus Controller */
#define	PCI_PRODUCT_TI_PCIXX12FW	0x803a		/* PCIXX12 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCIXX12FM	0x803b		/* PCIXX12 Integrated FlashMedia Controller */
#define	PCI_PRODUCT_TI_PCIXX12SD	0x803c		/* PCIXX12 Secure Digital Host Controller */
#define	PCI_PRODUCT_TI_PCIXX12SM	0x803d		/* PCIXX12 Smart Card */
#define	PCI_PRODUCT_TI_ACX100A	0x8400		/* ACX100A 802.11b */
#define	PCI_PRODUCT_TI_ACX100B	0x8401		/* ACX100B 802.11b */
#define	PCI_PRODUCT_TI_ACX111	0x9066		/* ACX111 802.11b/g */
#define	PCI_PRODUCT_TI_PCI1130	0xac12		/* PCI1130 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1031	0xac13		/* PCI1031 PCI-PCMCIA Bridge */
#define	PCI_PRODUCT_TI_PCI1131	0xac15		/* PCI1131 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1250	0xac16		/* PCI1250 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1220	0xac17		/* PCI1220 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1221	0xac19		/* PCI1221 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1210	0xac1a		/* PCI1210 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1450	0xac1b		/* PCI1450 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1225	0xac1c		/* PCI1225 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1251	0xac1d		/* PCI1251 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1211	0xac1e		/* PCI1211 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1251B	0xac1f		/* PCI1251B PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI2030	0xac20		/* PCI2030 PCI-PCI Bridge */
#define	PCI_PRODUCT_TI_PCI2050	0xac28		/* PCI2050 PCI-PCI Bridge */
#define	PCI_PRODUCT_TI_PCI4450YENTA	0xac40		/* PCI4450 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4410YENTA	0xac41		/* PCI4410 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4451YENTA	0xac42		/* PCI4451 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4510YENTA	0xac44		/* PCI4510 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI4520YENTA	0xac46		/* PCI4520 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI7510YENTA	0xac47		/* PCI7510 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI7610YENTA	0xac48		/* PCI7610 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI7410YENTA	0xac49		/* PCI7410 PCI-CardBus Bridge w/ IEEE 1394 Host Controller */
#define	PCI_PRODUCT_TI_PCI7610SM	0xac4A		/* PCI7610 PCI-CardBus Bridge (Smart Card Mode) */
#define	PCI_PRODUCT_TI_PCI7410SD	0xac4B		/* PCI7[46]10 PCI-CardBus Bridge (SD/MMC Mode) */
#define	PCI_PRODUCT_TI_PCI7410MS	0xac4C		/* PCI7[46]10 PCI-CardBus Bridge (Memory Stick Mode) */
#define	PCI_PRODUCT_TI_PCI1410	0xac50		/* PCI1410 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1420	0xac51		/* PCI1420 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1451	0xac52		/* PCI1451 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1421	0xac53		/* PCI1421 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1620	0xac54		/* PCI1620 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1520	0xac55		/* PCI1520 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1510	0xac56		/* PCI1510 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1530	0xac57		/* PCI1530 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI1515	0xac58		/* PCI1515 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TI_PCI2040	0xac60		/* PCI2040 PCI-DSP Bridge */
#define	PCI_PRODUCT_TI_PCI7420YENTA	0xac8e		/* PCI7420 PCI-Cardbus Bridge w/ IEEE 1394 Host Controller */

/* Titan Electronics products */

#define	PCI_PRODUCT_TITAN_VSCOM_PCI010L	0x8001		/* PCI-010L */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI100L	0x8010		/* PCI-100L */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI110L	0x8011		/* PCI-110L */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI200L	0x8020		/* PCI-200L */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI210L	0x8021		/* PCI-210L */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI200LI	0x8028		/* PCI-200Li */
#define	PCI_PRODUCT_MOLEX_VSCOM_PCI400L	0x8040		/* PCI-400L */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI800L	0x8080		/* PCI-800L */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI011H	0x8403		/* PCI-011H */
#define	PCI_PRODUCT_TITAN_VSCOM_PCIx10H	0xa000		/* PCI-x10H */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI100H	0xa001		/* PCI-100H */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI800H	0xa003		/* PCI-800H */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI800H_1	0xa004		/* PCI-800H_1 */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI200H	0xa005		/* PCI-200H */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI010HV2	0xe001		/* PCI-010HV2 */
#define	PCI_PRODUCT_TITAN_VSCOM_PCI200HV2	0xe020		/* PCI-200HV2 */


/* Toshiba America products */
#define	PCI_PRODUCT_TOSHIBA_R4X00	0x0009		/* R4x00 Host-PCI Bridge */
#define	PCI_PRODUCT_TOSHIBA_TC35856F	0x0020		/* TC35856F ATM (\"Meteor\") */

/* Toshiba products */
#define	PCI_PRODUCT_TOSHIBA2_PORTEGE	0x0001		/* Portege Notebook */
#define	PCI_PRODUCT_TOSHIBA2_PICCOLO	0x0101		/* Piccolo IDE Controller */
#define	PCI_PRODUCT_TOSHIBA2_PICCOLO2	0x0102		/* Piccolo 2 IDE Controller */
#define	PCI_PRODUCT_TOSHIBA2_PICCOLO3	0x0103		/* Piccolo 3 IDE Controller */
#define	PCI_PRODUCT_TOSHIBA2_PICCOLO5	0x0105		/* Piccolo 5 IDE Controller */
#define	PCI_PRODUCT_TOSHIBA2_HOST	0x0601		/* Host Bridge/Controller */
#define	PCI_PRODUCT_TOSHIBA2_ISA	0x0602		/* PCI-ISA Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC95	0x0603		/* ToPIC95 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC95B	0x060a		/* ToPIC95B PCI-CardBus Bridge */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC97	0x060f		/* ToPIC97 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TOSHIBA2_SANREMO	0x0618		/* SanRemo? Triangle Host Bridge */
#define	PCI_PRODUCT_TOSHIBA2_SMCARD	0x0804		/* Smart Media Controller */
#define	PCI_PRODUCT_TOSHIBA2_SDCARD	0x0805		/* Secure Digital Card Controller Type-A */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC100	0x0617		/* ToPIC100 PCI-CardBus Bridge */
#define	PCI_PRODUCT_TOSHIBA2_OBOE	0x0701		/* Fast Infrared Type O */
#define	PCI_PRODUCT_TOSHIBA2_DONAUOBOE	0x0d01		/* Fast Infrared Type DO */

/* Transmeta products */
#define	PCI_PRODUCT_TRANSMETA_TM8000NB	0x0061		/* TM8000 Integrated North Bridge */
#define	PCI_PRODUCT_TRANSMETA_NORTHBRIDGE	0x0295		/* Virtual North Bridge */
#define	PCI_PRODUCT_TRANSMETA_LONGRUN	0x0395		/* LongRun North Bridge */
#define	PCI_PRODUCT_TRANSMETA_SDRAM	0x0396		/* SDRAM Controller */
#define	PCI_PRODUCT_TRANSMETA_BIOS_SCRATCH	0x0397		/* BIOS Scratchpad */

/* Trident products */
#define	PCI_PRODUCT_TRIDENT_4DWAVE_DX	0x2000		/* 4DWAVE DX */
#define	PCI_PRODUCT_TRIDENT_4DWAVE_NX	0x2001		/* 4DWAVE NX */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADE_I7	0x8420		/* CyberBlade i7 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9320	0x9320		/* TGUI 9320 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9350	0x9350		/* TGUI 9350 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9360	0x9360		/* TGUI 9360 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9397	0x9397		/* CYBER 9397 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9397DVD	0x939a		/* CYBER 9397DVD */
#define	PCI_PRODUCT_TRIDENT_CYBER_9525	0x9525		/* CYBER 9525 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9420	0x9420		/* TGUI 9420 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9440	0x9440		/* TGUI 9440 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9660	0x9660		/* TGUI 9660 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9680	0x9680		/* TGUI 9680 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9682	0x9682		/* TGUI 9682 */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADE	0x9910		/* CyberBlade */

/* Triones Technologies products */
/* The 366 and 370 controllers have the same product ID */
#define	PCI_PRODUCT_TRIONES_HPT343	0x0003		/* HPT343/345 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT366	0x0004		/* HPT366/370/372 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT372A	0x0005		/* HPT372A IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT302	0x0006		/* HPT302 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT371	0x0007		/* HPT371 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT374	0x0008		/* HPT374 IDE Controller */
#define	PCI_PRODUCT_TRIONES_HPT372N	0x0009		/* HPT372N IDE Controller */
#define	PCI_PRODUCT_TRIONES_ROCKETRAID_2310	0x2310		/* RocketRAID 2310 RAID card */
#define	PCI_PRODUCT_TRIONES_ROCKETRAID_2720	0x2720		/* RocketRAID 2720 RAID card */

/* TriTech Microelectronics products*/
#define	PCI_PRODUCT_TRITECH_TR25202	0xfc02		/* Pyramid3D TR25202 */

/* Tseng Labs products */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_A	0x3202		/* ET4000w32p rev A */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_B	0x3205		/* ET4000w32p rev B */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_C	0x3206		/* ET4000w32p rev C */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_D	0x3207		/* ET4000w32p rev D */
#define	PCI_PRODUCT_TSENG_ET6000	0x3208		/* ET6000 */

/* Turtle Beach products */
#define	PCI_PRODUCT_TURTLE_BEACH_SANTA_CRUZ	0x3357		/* Santa Cruz */

/* UMC products */
#define	PCI_PRODUCT_UMC_UM82C881	0x0001		/* UM82C881 486 Chipset */
#define	PCI_PRODUCT_UMC_UM82C886	0x0002		/* UM82C886 PCI-ISA Bridge */
#define	PCI_PRODUCT_UMC_UM8673F	0x0101		/* UM8673F EIDE Controller */
#define	PCI_PRODUCT_UMC_UM8881	0x0881		/* UM8881 HB4 486 PCI Chipset */
#define	PCI_PRODUCT_UMC_UM82C891	0x0891		/* UM82C891 */
#define	PCI_PRODUCT_UMC_UM886A	0x1001		/* UM886A */
#define	PCI_PRODUCT_UMC_UM8886BF	0x673a		/* UM8886BF */
#define	PCI_PRODUCT_UMC_UM8710	0x8710		/* UM8710 */
#define	PCI_PRODUCT_UMC_UM8886	0x886a		/* UM8886 */
#define	PCI_PRODUCT_UMC_UM8881F	0x8881		/* UM8881F PCI-Host Bridge */
#define	PCI_PRODUCT_UMC_UM8886F	0x8886		/* UM8886F PCI-ISA Bridge */
#define	PCI_PRODUCT_UMC_UM8886A	0x888a		/* UM8886A */
#define	PCI_PRODUCT_UMC_UM8891A	0x8891		/* UM8891A */
#define	PCI_PRODUCT_UMC_UM9017F	0x9017		/* UM9017F */
#define	PCI_PRODUCT_UMC_UM8886N	0xe88a		/* UM8886N */
#define	PCI_PRODUCT_UMC_UM8891N	0xe891		/* UM8891N */

/* ULSI Systems products */
#define	PCI_PRODUCT_ULSI_US201	0x0201		/* US201 */

/* US Robotics products */
#define	PCI_PRODUCT_USR_3C2884A	0x1007		/* 56K Voice Internal PCI Modem (WinModem) */
#define	PCI_PRODUCT_USR_3CP5609	0x1008		/* 3CP5609 PCI 16550 Modem */
#define	PCI_PRODUCT_USR2_USR997902	0x0116		/* USR997902 Gigabit Ethernet */
#define	PCI_PRODUCT_USR2_2415	0x3685		/* Wireless PCI-PCMCIA Adapter */

/* V3 Semiconductor products */
#define	PCI_PRODUCT_V3_V292PBCPSC	0x0010		/* V292PBCPSC Am29K Local Bus to PCI Bridge */
#define	PCI_PRODUCT_V3_V292PBC	0x0292		/* V292PBC AMD290x0 Host-PCI Bridge */
#define	PCI_PRODUCT_V3_V960PBC	0x0960		/* V960PBC i960 Host-PCI Bridge */
#define	PCI_PRODUCT_V3_V96DPC	0xc960		/* V96DPC i960 (Dual) Host-PCI Bridge */

/* VIA Technologies products, from http://www.via.com.tw/ */
#define	PCI_PRODUCT_VIATECH_VT6305	0x0130		/* VT6305 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_VIATECH_K8M800_0	0x0204		/* K8M800 Host */
#define	PCI_PRODUCT_VIATECH_K8T890_0	0x0238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_KT880	0x0269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_K8HTB_0	0x0282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT8363_HB	0x0305		/* VT8363 (Apollo KT133) Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT3351_HB_0351	0x0351		/* VT3351 Host Bridge */
#define	PCI_PRODUCT_VIATECH_P4M900	0x0364		/* CN896/P4M900 Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT8371_HB	0x0391		/* VT8371 (Apollo KX133) Host Bridge */
#define	PCI_PRODUCT_VIATECH_VX900_HB	0x0410		/* VX900 Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT8501_MVP4	0x0501		/* VT8501 (Apollo MVP4) Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C505	0x0505		/* VT82C505 (Pluto) */
#define	PCI_PRODUCT_VIATECH_VT82C561	0x0561		/* VT82C561 */
#define	PCI_PRODUCT_VIATECH_VT82C586A_IDE	0x0571		/* VT82C586A IDE Controller */
#define	PCI_PRODUCT_VIATECH_VT82C576	0x0576		/* VT82C576 3V */
#define	PCI_PRODUCT_VIATECH_CX700_IDE	0x0581		/* CX700 IDE Controller */
#define	PCI_PRODUCT_VIATECH_VT82C580VP	0x0585		/* VT82C580 (Apollo VP) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C586_ISA	0x0586		/* VT82C586 PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237A_SATA	0x0591		/* VT8237A Integrated SATA Controller */
#define	PCI_PRODUCT_VIATECH_VT82C595	0x0595		/* VT82C595 (Apollo VP2) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C596A	0x0596		/* VT82C596A PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C597	0x0597		/* VT82C597 (Apollo VP3) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C598PCI	0x0598		/* VT82C598 (Apollo MVP3) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8605PCI	0x0605		/* VT8605 (Apollo ProMedia 133) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C686A_ISA	0x0686		/* VT82C686A PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C691	0x0691		/* VT82C691 (Apollo Pro) Host-PCI */
#define	PCI_PRODUCT_VIATECH_VT82C693	0x0693		/* VT82C693 (Apollo Pro Plus) Host-PCI */
#define	PCI_PRODUCT_VIATECH_VT86C926	0x0926		/* VT86C926 Amazon PCI-Ethernet Controller */
#define	PCI_PRODUCT_VIATECH_VT82C570M	0x1000		/* VT82C570M (Apollo) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C570MV	0x1006		/* VT82C570M (Apollo) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_KT880_1	0x1269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT3351_HB_1351	0x1351		/* VT3351 Host Bridge */
#define	PCI_PRODUCT_VIATECH_P4M900_1	0x1364		/* CN896/P4M900 Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C586_IDE	0x1571		/* VT82C586 IDE Controller */
#define	PCI_PRODUCT_VIATECH_VT82C595_2	0x1595		/* VT82C595 (Apollo VP2) Host-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_KT880_2	0x2269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT3351_HB_2351	0x2351		/* VT3351 Host Bridge */
#define	PCI_PRODUCT_VIATECH_P4M900_2	0x2364		/* CN896/P4M900 Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT8251_PPB_287A	0x287a		/* VT8251 PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8251_PCIE1	0x287c		/* VT8251 PCIE Root Port1 */
#define	PCI_PRODUCT_VIATECH_VT8251_PCIE2	0x287d		/* VT8251 PCIE Root Port2 */
#define	PCI_PRODUCT_VIATECH_VT8251_VLINK	0x287e		/* VT8251 Ultra VLINK Controller */
#define	PCI_PRODUCT_VIATECH_VT83C572	0x3038		/* VT83C572 USB Controller */
#define	PCI_PRODUCT_VIATECH_VT82C586_PWR	0x3040		/* VT82C586 Power Management Controller */
#define	PCI_PRODUCT_VIATECH_VT3043	0x3043		/* VT3043 (Rhine) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT6306	0x3044		/* VT6306 IEEE 1394 Host Controller */
#define	PCI_PRODUCT_VIATECH_VT6105M	0x3053		/* VT6105M (Rhine III) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT82C686A_SMB	0x3057		/* VT82C686A SMBus Controller */
#define	PCI_PRODUCT_VIATECH_VT82C686A_AC97	0x3058		/* VT82C686A AC-97 Audio Controller */
#define	PCI_PRODUCT_VIATECH_VT8233_AC97	0x3059		/* VT8233/VT8235 AC-97 Audio Controller */
#define	PCI_PRODUCT_VIATECH_VT6102	0x3065		/* VT6102 (Rhine II) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT82C686A_MC97	0x3068		/* VT82C686A MC-97 Modem Controller */
#define	PCI_PRODUCT_VIATECH_VT8233	0x3074		/* VT8233 PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8366	0x3099		/* VT8366 (Apollo KT266) CPU-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8653	0x3101		/* VT8653 (Apollo Pro 266T) CPU-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237_EHCI	0x3104		/* VT8237 EHCI USB Controller */
#define	PCI_PRODUCT_VIATECH_VT6105	0x3106		/* VT6105 (Rhine III) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT612X	0x3119		/* VT612X (Velocity) 10/100/1000 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT8623_VGA	0x3122		/* VT8623 (Apollo CLE266) VGA Controller */
#define	PCI_PRODUCT_VIATECH_VT8623	0x3123		/* VT8623 (Apollo CLE266) CPU-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8233A	0x3147		/* VT8233A PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237_SATA	0x3149		/* VT8237 Integrated SATA Controller */
#define	PCI_PRODUCT_VIATECH_VT6410_RAID	0x3164		/* VT6410 ATA133 RAID Controller */
#define	PCI_PRODUCT_VIATECH_VT8235	0x3177		/* VT8235 (Apollo KT400) PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_K8HTB	0x3188		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT8377	0x3189		/* VT8377 Apollo KT400 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8378	0x3205		/* VT8378 Apollo KM400 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237	0x3227		/* VT8237 PCI-LPC Bridge */
#define	PCI_PRODUCT_VIATECH_VT6421_RAID	0x3249		/* VT6421 Serial RAID Controller */
#define	PCI_PRODUCT_VIATECH_KT880_3	0x3269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8251	0x3287		/* VT8251 PCI-LPC Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237A_HDA	0x3288		/* VT8237A/VT8251 High Definition Audio Controller */
#define	PCI_PRODUCT_VIATECH_VT8237A_ISA	0x3337		/* VT8237A/VT82C586A PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT3314_IG	0x3344		/* VT3314 CN900 UniChrome Integrated Graphics */
#define	PCI_PRODUCT_VIATECH_VT8237R_SATA	0x3349		/* VT8237R Integrated SATA Controller */
#define	PCI_PRODUCT_VIATECH_VT3351_HB_3351	0x3351		/* VT3351 Host Bridge */
#define	PCI_PRODUCT_VIATECH_P4M900_3	0x3364		/* CN896/P4M900 Host Bridge */
#define	PCI_PRODUCT_VIATECH_CHROME9_HC	0x3371		/* Chrome9 HC IGP */
#define	PCI_PRODUCT_VIATECH_VT8237S_ISA	0x3372		/* VT8237S PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237A_PPB	0x337a		/* VT8237A PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8237A_HB	0x337b		/* VT8237A Host Bridge */
#define	PCI_PRODUCT_VIATECH_KT880_4	0x4269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT3351_HB_4351	0x4351		/* VT3351 Host Bridge */
#define	PCI_PRODUCT_VIATECH_P4M900_4	0x4364		/* CN896/P4M900 Host Bridge */
#define	PCI_PRODUCT_VIATECH_CX700M2_IDE	0x5324		/* CX700M2/VX700 IDE Controller */
#define	PCI_PRODUCT_VIATECH_VT8237A_SATA_2	0x5337		/* VT8237A Integrated SATA Controller */
#define	PCI_PRODUCT_VIATECH_VT3351_IOAPIC	0x5351		/* VT3351 I/O APIC Interrupt Controller */
#define	PCI_PRODUCT_VIATECH_P4M900_IOAPIC	0x5364		/* CN896/P4M900 IOAPIC */
#define	PCI_PRODUCT_VIATECH_VT8237S_SATA	0x5372		/* VT8237S Integrated SATA Controller */
#define	PCI_PRODUCT_VIATECH_VT86C100A	0x6100		/* VT86C100A (Rhine-II) 10/100 Ethernet */
#define	PCI_PRODUCT_VIATECH_VT8251_SATA	0x6287		/* VT8251 Integrated SATA Controller */
#define	PCI_PRODUCT_VIATECH_P4M900_6	0x6364		/* CN896/P4M900 Security Device */
#define	PCI_PRODUCT_VIATECH_VT8378_IG	0x7205		/* VT8378 KM400 UniChrome Integrated Graphics */
#define	PCI_PRODUCT_VIATECH_KT880_5	0x7269		/* KT880 CPU to PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT3351_HB_7351	0x7351		/* VT3351 Host Bridge */
#define	PCI_PRODUCT_VIATECH_P4M900_7	0x7364		/* CN896/P4M900 Host Bridge */
#define	PCI_PRODUCT_VIATECH_VT8231	0x8231		/* VT8231 PCI-ISA Bridge */
#define	PCI_PRODUCT_VIATECH_VT8231_PWR	0x8235		/* VT8231 Power Management Controller */
#define	PCI_PRODUCT_VIATECH_VT8363_PPB	0x8305		/* VT8363 (Apollo KT133) PCI to AGP Bridge */
#define	PCI_PRODUCT_VIATECH_CX700	0x8324		/* CX700 PCI-LPC Bridge */
#define	PCI_PRODUCT_VIATECH_VX800	0x8353		/* VX800/VX820 PCI-LPC Bridge */
#define	PCI_PRODUCT_VIATECH_VT8371_PPB	0x8391		/* VT8371 (Apollo KX133) PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8501AGP	0x8501		/* VT8501 (Apollo MVP4) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C597AGP	0x8597		/* VT82C597 (Apollo VP3) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT82C598AGP	0x8598		/* VT82C598 (Apollo MVP3) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8605AGP	0x8605		/* VT8605 (Apollo ProMedia 133) Host-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VX900_IDE	0x9001		/* VX900 IDE Controller */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_A238	0xa238		/* K8T890 PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_P4M900_PPB_1	0xa364		/* CN896/P4M900 PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_VT8633AGP	0xb091		/* VT8633 (Apollo Pro 266) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8366AGP	0xb099		/* VT8366 (Apollo KT266) CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT8377AGP	0xb168		/* VT8377 CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_K8HTB_AGP	0xb188		/* K8HTB AGP */
#define	PCI_PRODUCT_VIATECH_VT8377CEAGP	0xb198		/* VT8377CE CPU-AGP Bridge */
#define	PCI_PRODUCT_VIATECH_VT3237_PPB	0xb999		/* K8T890 North / VT8237 South PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_C238	0xc238		/* K8T890 PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_P4M900_PPB_2	0xc364		/* CN896/P4M900 PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_D238	0xd238		/* K8T890 PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_E238	0xe238		/* K8T890 PCI-PCI Bridge */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_F238	0xf238		/* K8T890 PCI-PCI Bridge */

/* VirtualBox products */
#define	PCI_PRODUCT_VIRTUALBOX_GRAPHICS	0xbeef		/* Graphics */
#define	PCI_PRODUCT_VIRTUALBOX_GUEST	0xcafe		/* Guest Service */

/* Vortex Computer Systems products */
#define	PCI_PRODUCT_VORTEX_GDT_60x0	0x0000		/* GDT6000/6020/6050 */
#define	PCI_PRODUCT_VORTEX_GDT_6000B	0x0001		/* GDT6000B/6010 */
#define	PCI_PRODUCT_VORTEX_GDT_6x10	0x0002		/* GDT6110/6510 */
#define	PCI_PRODUCT_VORTEX_GDT_6x20	0x0003		/* GDT6120/6520 */
#define	PCI_PRODUCT_VORTEX_GDT_6530	0x0004		/* GDT6530 */
#define	PCI_PRODUCT_VORTEX_GDT_6550	0x0005		/* GDT6550 */
#define	PCI_PRODUCT_VORTEX_GDT_6x17	0x0006		/* GDT6117/6517 */
#define	PCI_PRODUCT_VORTEX_GDT_6x27	0x0007		/* GDT6127/6527 */
#define	PCI_PRODUCT_VORTEX_GDT_6537	0x0008		/* GDT6537 */
#define	PCI_PRODUCT_VORTEX_GDT_6557	0x0009		/* GDT6557/6557-ECC */
#define	PCI_PRODUCT_VORTEX_GDT_6x15	0x000a		/* GDT6115/6515 */
#define	PCI_PRODUCT_VORTEX_GDT_6x25	0x000b		/* GDT6125/6525 */
#define	PCI_PRODUCT_VORTEX_GDT_6535	0x000c		/* GDT6535 */
#define	PCI_PRODUCT_VORTEX_GDT_6555	0x000d		/* GDT6555/6555-ECC */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RP	0x0100		/* GDT6[15]17RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RP	0x0101		/* GDT6[15]27RP */
#define	PCI_PRODUCT_VORTEX_GDT_6537RP	0x0102		/* GDT6537RP */
#define	PCI_PRODUCT_VORTEX_GDT_6557RP	0x0103		/* GDT6557RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RP	0x0104		/* GDT6[15]11RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RP	0x0105		/* GDT6[15]21RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RD	0x0110		/* GDT6[15]17RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RD	0x0111		/* GDT6[5]127RD */
#define	PCI_PRODUCT_VORTEX_GDT_6537RD	0x0112		/* GDT6537RD */
#define	PCI_PRODUCT_VORTEX_GDT_6557RD	0x0113		/* GDT6557RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RD	0x0114		/* GDT6[15]11RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RD	0x0115		/* GDT6[15]21RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x18RD	0x0118		/* GDT6[156]18RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x28RD	0x0119		/* GDT6[156]28RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x38RD	0x011a		/* GDT6[56]38RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x58RD	0x011b		/* GDT6[56]58RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RP2	0x0120		/* GDT6[15]17RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RP2	0x0121		/* GDT6[15]27RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6537RP2	0x0123		/* GDT6537RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RP2	0x0124		/* GDT6[15]11RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RP2	0x0125		/* GDT6[15]21RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x13RS	0x0136		/* GDT6513RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x23RS	0x0137		/* GDT6523RS */
#define	PCI_PRODUCT_VORTEX_GDT_6518RS	0x0138		/* GDT6518RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x28RS	0x0139		/* GDT6x28RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x38RS	0x013a		/* GDT6x38RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x58RS	0x013b		/* GDT6x58RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x33RS	0x013c		/* GDT6x33RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x43RS	0x013d		/* GDT6x43RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x53RS	0x013e		/* GDT6x53RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x63RS	0x013f		/* GDT6x63RS */
#define	PCI_PRODUCT_VORTEX_GDT_7x13RN	0x0166		/* GDT7x13RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x23RN	0x0167		/* GDT7x23RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x18RN	0x0168		/* GDT7[156]18RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x28RN	0x0169		/* GDT7[156]28RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x38RN	0x016a		/* GDT7[56]38RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x58RN	0x016b		/* GDT7[56]58RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x43RN	0x016d		/* GDT7[56]43RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x53RN	0x016E		/* GDT7x53RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x63RN	0x016F		/* GDT7x63RN */
#define	PCI_PRODUCT_VORTEX_GDT_4x13RZ	0x01D6		/* GDT4x13RZ */
#define	PCI_PRODUCT_VORTEX_GDT_4x23RZ	0x01D7		/* GDT4x23RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x13RZ	0x01F6		/* GDT8x13RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x23RZ	0x01F7		/* GDT8x23RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x33RZ	0x01FC		/* GDT8x33RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x43RZ	0x01FD		/* GDT8x43RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x53RZ	0x01FE		/* GDT8x53RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x63RZ	0x01FF		/* GDT8x63RZ */
#define	PCI_PRODUCT_VORTEX_GDT_6x19RD	0x0210		/* GDT6[56]19RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x29RD	0x0211		/* GDT6[56]29RD */
#define	PCI_PRODUCT_VORTEX_GDT_7x19RN	0x0260		/* GDT7[56]19RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x29RN	0x0261		/* GDT7[56]29RN */
#define	PCI_PRODUCT_VORTEX_GDT_ICP	0x0300		/* ICP */

/* VLSI products */
#define	PCI_PRODUCT_VLSI_82C592	0x0005		/* 82C592 CPU Bridge */
#define	PCI_PRODUCT_VLSI_82C593	0x0006		/* 82C593 ISA Bridge */
#define	PCI_PRODUCT_VLSI_82C594	0x0007		/* 82C594 Wildcat System Controller */
#define	PCI_PRODUCT_VLSI_82C596597	0x0008		/* 82C596/597 Wildcat ISA Bridge */
#define	PCI_PRODUCT_VLSI_82C541	0x000c		/* 82C541 */
#define	PCI_PRODUCT_VLSI_82C543	0x000d		/* 82C543 */
#define	PCI_PRODUCT_VLSI_82C532	0x0101		/* 82C532 */
#define	PCI_PRODUCT_VLSI_82C534	0x0102		/* 82C534 */
#define	PCI_PRODUCT_VLSI_82C535	0x0104		/* 82C535 */
#define	PCI_PRODUCT_VLSI_82C147	0x0105		/* 82C147 */
#define	PCI_PRODUCT_VLSI_82C975	0x0200		/* 82C975 */
#define	PCI_PRODUCT_VLSI_82C925	0x0280		/* 82C925 */

/* VMware products */
#define	PCI_PRODUCT_VMWARE_VIRTUAL2	0x0405		/* Virtual SVGA II */
#define	PCI_PRODUCT_VMWARE_VIRTUAL	0x0710		/* Virtual SVGA */
#define	PCI_PRODUCT_VMWARE_VMXNET	0x0720		/* Virtual Network */
#define	PCI_PRODUCT_VMWARE_VMSCSI	0x0730		/* Virtual SCSI */
#define	PCI_PRODUCT_VMWARE_VMCI	0x0740		/* Virtual Machine Communication Interface */
#define	PCI_PRODUCT_VMWARE_VMEM	0x0750		/* Virtual 82545EM */
#define	PCI_PRODUCT_VMWARE_VMEB	0x0760		/* Virtual 82546EB */
#define	PCI_PRODUCT_VMWARE_VMUSB	0x0770		/* Virtual USB */
#define	PCI_PRODUCT_VMWARE_VM1394	0x0780		/* Virtual Firewire */
#define	PCI_PRODUCT_VMWARE_VMPCIB	0x0790		/* Virtual PCI Bridge */
#define	PCI_PRODUCT_VMWARE_VMPCIE	0x07a0		/* Virtual PCI Express Root Port */
#define	PCI_PRODUCT_VMWARE_VMXNET3	0x07b0		/* Virtual Network 3 */
#define	PCI_PRODUCT_VMWARE_PVSCSI	0x07c0		/* PVSCSI */
#define	PCI_PRODUCT_VMWARE_VMI3	0x0801		/* VMI option ROM */

/* Weitek products */
#define	PCI_PRODUCT_WEITEK_P9000	0x9001		/* P9000 */
#define	PCI_PRODUCT_WEITEK_P9100	0x9100		/* P9100 */

/* Western Digital products */
#define	PCI_PRODUCT_WD_WD33C193A	0x0193		/* WD33C193A */
#define	PCI_PRODUCT_WD_WD33C196A	0x0196		/* WD33C196A */
#define	PCI_PRODUCT_WD_WD33C197A	0x0197		/* WD33C197A */
#define	PCI_PRODUCT_WD_WD7193	0x3193		/* WD7193 */
#define	PCI_PRODUCT_WD_WD7197	0x3197		/* WD7197 */
#define	PCI_PRODUCT_WD_WD33C296A	0x3296		/* WD33C296A */
#define	PCI_PRODUCT_WD_WD34C296	0x4296		/* WD34C296 */
#define	PCI_PRODUCT_WD_90C	0xc24a		/* 90C */

/* Winbond Electronics products */
#define	PCI_PRODUCT_WINBOND_W83769F	0x0001		/* W83769F */
#define	PCI_PRODUCT_WINBOND_W83C553F_0	0x0565		/* W83C553F PCI-ISA Bridge */
#define	PCI_PRODUCT_WINBOND_W83628F	0x0628		/* W83628F PCI-ISA Bridge */
#define	PCI_PRODUCT_WINBOND_W83C553F_1	0x0105		/* W83C553F IDE Controller */
#define	PCI_PRODUCT_WINBOND_W89C840F	0x0840		/* W89C840F 10/100 Ethernet */
#define	PCI_PRODUCT_WINBOND_W89C940F	0x0940		/* W89C940F Ethernet */
#define	PCI_PRODUCT_WINBOND_W89C940F_1	0x5a5a		/* W89C940F Ethernet */
#define	PCI_PRODUCT_WINBOND_W6692	0x6692		/* W6692 ISDN */

/* Workbit products */
#define	PCI_PRODUCT_WORKBIT_NJSC32BI	0x8007		/* NinjaSCSI-32Bi SCSI */
#define	PCI_PRODUCT_WORKBIT_NJATA32BI	0x8008		/* NinjaATA-32Bi IDE */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE	0x8009		/* NinjaSCSI-32UDE SCSI */
#define	PCI_PRODUCT_WORKBIT_NJSC32BI_KME	0xf007		/* NinjaSCSI-32Bi SCSI (KME) */
#define	PCI_PRODUCT_WORKBIT_NJATA32BI_KME	0xf008		/* NinjaATA-32Bi IDE (KME) */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE_IODATA	0xf010		/* NinjaSCSI-32UDE SCSI (IODATA) */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE_LOGITEC	0xf012		/* NinjaSCSI-32UDE SCSI (LOGITEC) */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE_LOGITEC2	0xf013		/* NinjaSCSI-32UDE SCSI (LOGITEC2) */
#define	PCI_PRODUCT_WORKBIT_NJSC32UDE_BUFFALO	0xf015		/* NinjaSCSI-32UDE SCSI (BUFFALO) */
#define	PCI_PRODUCT_WORKBIT_NPATA32_CF32A	0xf021		/* CF32A CompactFlash Adapter */
#define	PCI_PRODUCT_WORKBIT_NPATA32_CF32A_BUFFALO	0xf024		/* CF32A CF Adapter (BUFFALO) */
#define	PCI_PRODUCT_WORKBIT_NPATA32_KME	0xf02c		/* NPATA-32 IDE (KME) */

/* XenSource products */
#define	PCI_PRODUCT_XENSOURCE_XENPLATFORM	0x0001		/* Xen Platform Device */

/* XGI Technology products */
#define	PCI_PRODUCT_XGI_VOLARI_Z7	0x0020		/* Volari Z7/Z9/Z9s */
#define	PCI_PRODUCT_XGI_VOLARI_Z9M	0x0021		/* Volari Z9m */
#define	PCI_PRODUCT_XGI_VOLARI_Z11	0x0027		/* Volari Z11/Z11M */
#define	PCI_PRODUCT_XGI_VOLARI_V3XT	0x0040		/* Volari V3XT/V5/V8 */
#define	PCI_PRODUCT_XGI_VOLARI_XP10	0x0047		/* Volari XP10 */

/* Xircom products */
/* is the `-3' here just indicating revision 3, or is it really part
   of the device name? */
#define	PCI_PRODUCT_XIRCOM_X3201_3	0x0002		/* X3201-3 Fast Ethernet Controller */
/* this is the device id `indicating 21143 driver compatibility' */
#define	PCI_PRODUCT_XIRCOM_X3201_3_21143	0x0003		/* X3201-3 Fast Ethernet Controller (21143) */
#define	PCI_PRODUCT_XIRCOM_WINGLOBAL	0x000c		/* WinGlobal Modem */
#define	PCI_PRODUCT_XIRCOM_MODEM56	0x0103		/* 56k Modem */

/* Yamaha products */
#define	PCI_PRODUCT_YAMAHA_YMF724	0x0004		/* 724 Audio */
#define	PCI_PRODUCT_YAMAHA_YMF740	0x000a		/* 740 Audio */
#define	PCI_PRODUCT_YAMAHA_YMF740C	0x000c		/* 740C (DS-1) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF724F	0x000d		/* 724F (DS-1) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF744B	0x0010		/* 744 (DS-1S) Audio */
#define	PCI_PRODUCT_YAMAHA_YMF754	0x0012		/* 754 (DS-1E) Audio */

/* Zeinet products */
#define	PCI_PRODUCT_ZEINET_1221	0x0001		/* 1221 */

/* Ziatech products */
#define	PCI_PRODUCT_ZIATECH_ZT8905	0x8905		/* PCI-ST32 Bridge */

/* Zoran products */
#define	PCI_PRODUCT_ZORAN_ZR36057	0x6057		/* ZR36057 Multimedia Controller */
#define	PCI_PRODUCT_ZORAN_ZR36120	0x6120		/* ZR36120 Video Controller */
