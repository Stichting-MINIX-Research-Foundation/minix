/*	$NetBSD: dptivar.h,v 1.9 2012/10/27 17:18:17 chs Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation
 * All rights reserved.
 *
 * TERMS AND CONDITIONS OF USE
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Adaptec and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose, are disclaimed. In no
 * event shall Adaptec be liable for any direct, indirect, incidental, special,
 * exemplary or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruptions) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this driver software, even
 * if advised of the possibility of such damage.
 */

#ifndef	_I2O_DPTIVAR_H_
#define	_I2O_DPTIVAR_H_

#define	DPTI_MAX_SEGS	17

/*
 * Runtime state.
 */
struct dpti_softc {
	device_t sc_dev;
	int	sc_blinkled;
};

struct dpti_ptbuf {
	void *	db_ptr;
	struct	proc *db_proc;
	int	db_size;
	int	db_out;
	int	db_nfrag;
	struct	iovec db_frags[DPTI_MAX_SEGS];
};

/*
 * Constants used by the `signature'.
 */

/* I2O */
#define	DPTI_VERSION		1
#define	DPTI_REVISION		0
#define	DPTI_SUBREVISION	0

#define	DPTI_YEAR		1
#define	DPTI_MONTH		9
#define	DPTI_DAY		12

/* EATA */
#define	DPT_VERSION		1
#define	DPT_REVISION		0
#define	DPT_SUBREVISION		0

#define	DPT_YEAR		1
#define	DPT_MONTH		9
#define	DPT_DAY			12

/*
 * ioctls.  We define only the lower 16 bits, since the DPT utilities don't
 * seem to obey the ioctl encoding conventions of each platform - the high
 * 16 bits are relatively meaningless.
 */
#define	DPT_EATAUSRCMD		0x4441
#define	DPT_DEBUG		0x4442
#define	DPT_SIGNATURE		0x4443
#define	DPT_NUMCTRLS		0x4444
#define	DPT_CTRLINFO		0x4445
#define	DPT_STATINFO		0x4446
#define	DPT_CLRSTAT		0x4447
#define	DPT_SYSINFO		0x4448
#define	DPT_TIMEOUT		0x4449
#define	DPT_CONFIG		0x444a
#define	DPT_BLINKLED		0x444b
#define	DPT_I2OUSRCMD		0x444c
#define	DPT_I2ORESCANCMD	0x444d
#define	DPT_I2ORESETCMD		0x444e
#define	DPT_TARGET_BUSY		0x444f

/*
 * Controller and system info structures.
 */
struct dpt_ctlrinfo {
	u_int16_t	length;
	u_int16_t	drvrHBAnum;
	u_int32_t	baseAddr;
	u_int16_t	blinkState;
	u_int8_t	pciBusNum;
	u_int8_t	pciDeviceNum;
	u_int16_t	hbaFlags;
	u_int16_t	Interrupt;
	u_int32_t	reserved1;
	u_int32_t	reserved2;
	u_int32_t	reserved3;
};
#define	FLG_OSD_PCI_VALID 0x0001
#define	FLG_OSD_DMA       0x0002
#define	FLG_OSD_I2O       0x0004

struct dpt_eata_ctlrinfo {
	u_int8_t	state;
	u_int8_t	id;
	int		vect;
	int		base;
	int		njobs;
	int		qdepth;
	int		wakebase;
	u_long		sgsize;
	u_int		heads;
	u_int		sectors;
	u_int8_t	do_drive32;
	u_int8_t	busquiet;
	u_int8_t	idpal[4];
	u_int8_t	primary;
	u_int8_t	eataVersion;
	u_long		cpLength;
	u_long		spLength;
	u_int8_t	drqNum;
	u_int8_t	flag1;
	u_int8_t	flag2;
};

struct dpt_targetbusy {
	u_long		channel;
	u_long		id;
	u_long		lun;
	u_long		isbusy;
};

#if (!defined(dsDescription_size))
# define dsDescription_size 46
#endif

struct dpt_sig {
	char		dsSignature[6];      /* ALWAYS "dPtSiG" */
	u_int8_t	dsSigVersion;        /* sig version (currently 1) */
	u_int8_t	dsProcessorFamily;   /* what type of processor */
	u_int8_t	dsProcessor;         /* precise processor */
	u_int8_t	dsFiletype;          /* type of file */
	u_int8_t	dsFiletypeFlags;     /* flags to specify type, etc. */
	u_int8_t	dsOEM;               /* OEM file was created for */
	u_int32_t	dsOS;                /* which Operating systems */
	u_int16_t	dsCapabilities;      /* RAID levels, etc. */
	u_int16_t	dsDeviceSupp;        /* SCSI device types supported */
	u_int16_t	dsAdapterSupp;       /* DPT HBA families supported */
	u_int16_t	dsApplication;       /* applications file is for */
	u_int8_t	dsRequirements;      /* Other driver dependencies */
	u_int8_t	dsVersion;           /* 1 */
	u_int8_t	dsRevision;          /* 'J' */
	u_int8_t	dsSubRevision;       /* '9'   ' ' if N/A */
	u_int8_t	dsMonth;             /* creation month */
	u_int8_t	dsDay;               /* creation day */
	u_int8_t	dsYear;              /* creation year since 1980 (1993=13) */
	char		dsDescription[dsDescription_size];
};

struct dpt_dparam {
	u_int16_t	cylinders;      /* Upto 1024 */
	u_int8_t	heads;          /* Upto 255 */
	u_int8_t	sectors;        /* Upto 63 */
};

struct dpt_sysinfo {
	u_int8_t	drive0CMOS;             /* CMOS Drive 0 Type */
	u_int8_t	drive1CMOS;             /* CMOS Drive 1 Type */
	u_int8_t	numDrives;              /* 0040:0075 contents */
	u_int8_t	processorFamily;        /* Same as DPTSIG's defs */
	u_int8_t	processorType;          /* Same as DPTSIG's defs */
	u_int8_t	smartROMMajorVersion;
	u_int8_t	smartROMMinorVersion;   /* SmartROM version */
	u_int8_t	smartROMRevision;
	u_int16_t	flags;                  /* See bit definitions above */
	u_int16_t	conventionalMemSize;    /* in KB */
	u_int32_t	extendedMemSize;        /* in KB */
	u_int32_t	osType;                 /* Same as DPTSIG's defs */
	u_int8_t	osMajorVersion;
	u_int8_t	osMinorVersion;         /* The OS version */
	u_int8_t	osRevision;
	u_int8_t	osSubRevision;
	u_int8_t	busType;                /* See defininitions above */
	u_int8_t	pad[3];                 /* For alignment */
	struct	dpt_dparam drives[16];		/* SmartROM Logical Drives */
};

/*
 * Defs pertaining to dpt_sysinfo.
 */

#define	SI_CMOS_Valid		0x0001
#define	SI_NumDrivesValid	0x0002
#define	SI_ProcessorValid	0x0004
#define	SI_MemorySizeValid	0x0008
#define	SI_DriveParamsValid	0x0010
#define	SI_SmartROMverValid	0x0020
#define	SI_OSversionValid	0x0040
#define	SI_OSspecificValid	0x0080
#define	SI_BusTypeValid		0x0100
#define	SI_ALL_VALID            0x0FFF
#define	SI_NO_SmartROM          0x8000

#define	SI_ISA_BUS	0x00
#define	SI_MCA_BUS	0x01
#define	SI_EISA_BUS	0x02
#define	SI_PCI_BUS	0x04


/*
 * Defs pertaining to dpt_sig.
 */

/* Current Signature Version - u_int8_t dsSigVersion; */
/* ------------------------------------------------------------------ */
#define	SIG_VERSION 1

/* Processor Family - u_int8_t dsProcessorFamily;  DISTINCT VALUES */
/* ------------------------------------------------------------------ */
/* What type of processor the file is meant to run on. */
/* This will let us know whether to read u_int16_ts as high/low or low/high. */
#define	PROC_INTEL      0x00    /* Intel 80x86 */
#define	PROC_MOTOROLA   0x01    /* Motorola 68K */
#define	PROC_MIPS4000   0x02    /* MIPS RISC 4000 */
#define	PROC_MIPS       0x02	/* MIPS RISC */
#define	PROC_ALPHA      0x03    /* DEC Alpha */
#define	PROC_POWERPC    0x04    /* IBM Power PC */
#define	PROC_i960       0x05    /* Intel i960 */
#define	PROC_ULTRASPARC 0x06    /* SPARC processor */

/* Specific Minimim Processor - u_int8_t dsProcessor;    FLAG BITS */
/* ------------------------------------------------------------------ */
/* Different bit definitions dependent on processor_family */

/* PROC_INTEL: */
#define	PROC_8086       0x01    /* Intel 8086 */
#define	PROC_286        0x02    /* Intel 80286 */
#define	PROC_386        0x04    /* Intel 80386 */
#define	PROC_486        0x08    /* Intel 80486 */
#define	PROC_PENTIUM    0x10    /* Intel 586 aka P5 aka Pentium */
#define	PROC_SEXIUM     0x20    /* Intel 686 aka P6 aka Pentium Pro or MMX */

/* PROC_i960: */
#define	PROC_960RX      0x01    /* Intel 80960RP/RD */
#define	PROC_960HX      0x02    /* Intel 80960HA/HD/HT */
#define	PROC_960RN      0x03    /* Intel 80960RN/RM */
#define	PROC_960RS      0x04    /* Intel 80960RS */

/* PROC_MOTOROLA: */
#define	PROC_68000      0x01    /* Motorola 68000 */
#define	PROC_68010      0x02    /* Motorola 68010 */
#define	PROC_68020      0x04    /* Motorola 68020 */
#define	PROC_68030      0x08    /* Motorola 68030 */
#define	PROC_68040      0x10    /* Motorola 68040 */

/* PROC_POWERPC */
#define	PROC_PPC601             0x01    /* PowerPC 601 */
#define	PROC_PPC603             0x02    /* PowerPC 603 */
#define	PROC_PPC604             0x04    /* PowerPC 604 */

/* PROC_MIPS */
#define	PROC_R4000      0x01    /* MIPS R4000 */
#define	PROC_RM7000     0x02    /* MIPS RM7000 */

/* Filetype - u_int8_t dsFiletype;       DISTINCT VALUES */
/* ------------------------------------------------------------------ */
#define	FT_EXECUTABLE   0       /* Executable Program */
#define	FT_SCRIPT       1       /* Script/Batch File??? */
#define	FT_HBADRVR      2       /* HBA Driver */
#define	FT_OTHERDRVR    3       /* Other Driver */
#define	FT_IFS          4       /* Installable Filesystem Driver */
#define	FT_ENGINE       5       /* DPT Engine */
#define	FT_COMPDRVR     6       /* Compressed Driver Disk */
#define	FT_LANGUAGE     7       /* Foreign Language file */
#define	FT_FIRMWARE     8       /* Downloadable or actual Firmware */
#define	FT_COMMMODL     9       /* Communications Module */
#define	FT_INT13        10      /* INT 13 style HBA Driver */
#define	FT_HELPFILE     11      /* Help file */
#define	FT_LOGGER       12      /* Event Logger */
#define	FT_INSTALL      13      /* An Install Program */
#define	FT_LIBRARY      14      /* Storage Manager Real-Mode Calls */
#define	FT_RESOURCE     15      /* Storage Manager Resource File */
#define	FT_MODEM_DB     16      /* Storage Manager Modem Database */

/* Filetype flags - u_int8_t dsFiletypeFlags;    FLAG BITS */
/* ------------------------------------------------------------------ */
#define	FTF_DLL         0x01    /* Dynamic Link Library */
#define	FTF_NLM         0x02    /* Netware Loadable Module */
#define	FTF_OVERLAYS    0x04    /* Uses overlays */
#define	FTF_DEBUG       0x08    /* Debug version */
#define	FTF_TSR         0x10    /* TSR */
#define	FTF_SYS         0x20    /* DOS Loadable driver */
#define	FTF_PROTECTED   0x40    /* Runs in protected mode */
#define	FTF_APP_SPEC    0x80    /* Application Specific */
#define	FTF_ROM         (FTF_SYS|FTF_TSR)       /* Special Case */

/* OEM - u_int8_t dsOEM;         DISTINCT VALUES */
/* ------------------------------------------------------------------ */
#define	OEM_DPT         0       /* DPT */
#define	OEM_ATT         1       /* ATT */
#define	OEM_NEC         2       /* NEC */
#define	OEM_ALPHA       3       /* Alphatronix */
#define	OEM_AST         4       /* AST */
#define	OEM_OLIVETTI    5       /* Olivetti */
#define	OEM_SNI         6       /* Siemens/Nixdorf */
#define	OEM_SUN         7       /* SUN Microsystems */
#define	OEM_ADAPTEC     8       /* Adaptec */

/* Operating System  - u_int32_t dsOS;    FLAG BITS */
/* ------------------------------------------------------------------ */
#define	OS_DOS          0x00000001 /* PC/MS-DOS                         */
#define	OS_WINDOWS      0x00000002 /* Microsoft Windows 3.x             */
#define	OS_WINDOWS_NT   0x00000004 /* Microsoft Windows NT              */
#define	OS_OS2M         0x00000008 /* OS/2 1.2.x,MS 1.3.0,IBM 1.3.x - Monolithic */
#define	OS_OS2L         0x00000010 /* Microsoft OS/2 1.301 - LADDR      */
#define	OS_OS22x        0x00000020 /* IBM OS/2 2.x                      */
#define	OS_NW286        0x00000040 /* Novell NetWare 286                */
#define	OS_NW386        0x00000080 /* Novell NetWare 386                */
#define	OS_GEN_UNIX     0x00000100 /* Generic Unix                      */
#define	OS_SCO_UNIX     0x00000200 /* SCO Unix                          */
#define	OS_ATT_UNIX     0x00000400 /* ATT Unix                          */
#define	OS_UNIXWARE     0x00000800 /* USL Unix                          */
#define	OS_INT_UNIX     0x00001000 /* Interactive Unix                  */
#define	OS_SOLARIS      0x00002000 /* SunSoft Solaris                   */
#define	OS_QNX          0x00004000 /* QNX for Tom Moch                  */
#define	OS_NEXTSTEP     0x00008000 /* NeXTSTEP/OPENSTEP/MACH            */
#define	OS_BANYAN       0x00010000 /* Banyan Vines                      */
#define	OS_OLIVETTI_UNIX 0x00020000/* Olivetti Unix                     */
#define	OS_MAC_OS       0x00040000 /* Mac OS                    	*/
#define	OS_WINDOWS_95   0x00080000 /* Microsoft Windows '95             */
#define	OS_NW4x         0x00100000 /* Novell Netware 4.x        	*/
#define	OS_BSDI_UNIX    0x00200000 /* BSDi Unix BSD/OS 2.0 and up       */
#define	OS_AIX_UNIX     0x00400000 /* AIX Unix                          */
#define	OS_FREE_BSD     0x00800000 /* FreeBSD Unix              	*/
#define	OS_LINUX        0x01000000 /* Linux                    	 	*/
#define	OS_DGUX_UNIX    0x02000000 /* Data General Unix                 */
#define	OS_SINIX_N      0x04000000 /* SNI SINIX-N                       */
#define	OS_PLAN9        0x08000000 /* ATT Plan 9             		*/
#define	OS_TSX          0x10000000 /* SNH TSX-32              	  	*/
#define	OS_WINDOWS_98   0x20000000 /* Microsoft Windows '98    		*/

#define	OS_OTHER        0x80000000 /* Other                             */

/* Capabilities - u_int16_t dsCapabilities;        FLAG BITS */
/* ------------------------------------------------------------------ */
#define	CAP_RAID0       0x0001  /* RAID-0 */
#define	CAP_RAID1       0x0002  /* RAID-1 */
#define	CAP_RAID3       0x0004  /* RAID-3 */
#define	CAP_RAID5       0x0008  /* RAID-5 */
#define	CAP_SPAN        0x0010  /* Spanning */
#define	CAP_PASS        0x0020  /* Provides passthrough */
#define	CAP_OVERLAP     0x0040  /* Passthrough supports overlapped commands */
#define	CAP_ASPI        0x0080  /* Supports ASPI Command Requests */
#define	CAP_ABOVE16MB   0x0100  /* ISA Driver supports greater than 16MB */
#define	CAP_EXTEND      0x8000  /* Extended info appears after description */
#ifdef SNI_MIPS
#define	CAP_CACHEMODE   0x1000  /* dpt_force_cache is set in driver */
#endif

/* Devices Supported - u_int16_t dsDeviceSupp;    FLAG BITS */
/* ------------------------------------------------------------------ */
#define	DEV_DASD        0x0001  /* DASD (hard drives) */
#define	DEV_TAPE        0x0002  /* Tape drives */
#define	DEV_PRINTER     0x0004  /* Printers */
#define	DEV_PROC        0x0008  /* Processors */
#define	DEV_WORM        0x0010  /* WORM drives */
#define	DEV_CDROM       0x0020  /* CD-ROM drives */
#define	DEV_SCANNER     0x0040  /* Scanners */
#define	DEV_OPTICAL     0x0080  /* Optical Drives */
#define	DEV_JUKEBOX     0x0100  /* Jukebox */
#define	DEV_COMM        0x0200  /* Communications Devices */
#define	DEV_OTHER       0x0400  /* Other Devices */
#define	DEV_ALL         0xFFFF  /* All SCSI Devices */

/* Adapters Families Supported - u_int16_t dsAdapterSupp; FLAG BITS */
/* ------------------------------------------------------------------ */
#define	ADF_2001        0x0001  /* PM2001           */
#define	ADF_2012A       0x0002  /* PM2012A          */
#define	ADF_PLUS_ISA    0x0004  /* PM2011,PM2021    */
#define	ADF_PLUS_EISA   0x0008  /* PM2012B,PM2022   */
#define	ADF_SC3_ISA     0x0010  /* PM2021           */
#define	ADF_SC3_EISA    0x0020  /* PM2022,PM2122, etc */
#define	ADF_SC3_PCI     0x0040  /* SmartCache III PCI */
#define	ADF_SC4_ISA     0x0080  /* SmartCache IV ISA */
#define	ADF_SC4_EISA    0x0100  /* SmartCache IV EISA */
#define	ADF_SC4_PCI     0x0200  /* SmartCache IV PCI */
#define	ADF_SC5_PCI     0x0400  /* Fifth Generation I2O products */
/*
 *      Combinations of products
 */
#define	ADF_ALL_2000    (ADF_2001|ADF_2012A)
#define	ADF_ALL_PLUS    (ADF_PLUS_ISA|ADF_PLUS_EISA)
#define	ADF_ALL_SC3     (ADF_SC3_ISA|ADF_SC3_EISA|ADF_SC3_PCI)
#define	ADF_ALL_SC4     (ADF_SC4_ISA|ADF_SC4_EISA|ADF_SC4_PCI)
#define	ADF_ALL_SC5     (ADF_SC5_PCI)
/* All EATA Cacheing Products */
#define	ADF_ALL_CACHE   (ADF_ALL_PLUS|ADF_ALL_SC3|ADF_ALL_SC4)
/* All EATA Bus Mastering Products */
#define	ADF_ALL_MASTER  (ADF_2012A|ADF_ALL_CACHE)
/* All EATA Adapter Products */
#define	ADF_ALL_EATA    (ADF_2001|ADF_ALL_MASTER)
#define	ADF_ALL         ADF_ALL_EATA

/* Application - u_int16_t dsApplication;         FLAG BITS */
/* ------------------------------------------------------------------ */
#define	APP_DPTMGR      0x0001  /* DPT Storage Manager */
#define	APP_ENGINE      0x0002  /* DPT Engine */
#define	APP_SYTOS       0x0004  /* Sytron Sytos Plus */
#define	APP_CHEYENNE    0x0008  /* Cheyenne ARCServe + ARCSolo */
#define	APP_MSCDEX      0x0010  /* Microsoft CD-ROM extensions */
#define	APP_NOVABACK    0x0020  /* NovaStor Novaback */
#define	APP_AIM         0x0040  /* Archive Information Manager */

/* Requirements - u_int8_t dsRequirements;         FLAG BITS            */
/* ------------------------------------------------------------------   */
#define	REQ_SMARTROM    0x01    /* Requires SmartROM to be present      */
#define	REQ_DPTDDL      0x02    /* Requires DPTDDL.SYS to be loaded     */
#define	REQ_HBA_DRIVER  0x04    /* Requires an HBA driver to be loaded  */
#define	REQ_ASPI_TRAN   0x08    /* Requires an ASPI Transport Modules   */
#define	REQ_ENGINE      0x10    /* Requires a DPT Engine to be loaded   */
#define	REQ_COMM_ENG    0x20    /* Requires a DPT Communications Engine */

/* ------------------------------------------------------------------   */
/* Requirements - u_int16_t dsFirmware;         FLAG BITS               */
/* ------------------------------------------------------------------   */
#define	dsFirmware dsApplication
#define	FW_DNLDSIZE0      0x0000    /* 0..2 DownLoader Size - NONE      */
#define	FW_DNLDSIZE16     0x0001    /* 0..2 DownLoader Size 16K         */
#define	FW_DNLDSIZE32     0x0002    /* 0..2 DownLoader Size 32K         */
#define	FW_DNLDSIZE64     0x0004    /* 0..2 DownLoader Size 64K         */

#define	FW_LOAD_BTM       0x2000        /* 13 Load Offset (1=Btm, 0=Top)    */
#define	FW_LOAD_TOP       0x0000        /* 13 Load Offset (1=Btm, 0=Top)    */
#define	FW_SIG_VERSION1   0x0000        /* 15..14 Version Bits 0=Ver1       */

#endif	/* !_I2O_DPTIVAR_H_ */
