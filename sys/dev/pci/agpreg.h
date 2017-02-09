/*	$NetBSD: agpreg.h,v 1.22 2014/06/12 18:46:32 riastradh Exp $	*/

/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/sys/pci/agpreg.h,v 1.3 2000/07/12 10:13:04 dfr Exp $
 */

#ifndef _PCI_AGPREG_H_
#define _PCI_AGPREG_H_

/*
 * Offsets for various AGP configuration registers.
 */
#define AGP_APBASE		0x10

#define AGP_STATUS		0x4
#define AGP_COMMAND		0x8

/*
 * Config registers for Intel AGP chipsets.
 */
/* i845/855PM */
#define	AGP_I845_AGPMISC	0x51
# define AGPMISC_AAGN		(1U << 1)  /* Aperture AccessEN */

/* i840/850/850E */
#define AGP_I840_MCHCFG		0x50
# define MCHCFG_AAGN		(1U << 9)  /* Aperture AccessEN */

/* i82443LX/BX/GX */
#define AGP_INTEL_NBXCFG	0x50
# define NBXCFG_APAE		(1U << 10) /* AGPtoPCI AccessDIS */
# define NBXCFG_AAGN		(1U << 9)  /* Aperture AccessEN */

/* Error Status for i8XX Chipset */
#define	AGP_INTEL_I8XX_ERRSTS	0xc8

/* Common register */
#define	AGP_INTEL_ERRSTS	0x90 /* bytes at 0x91 and 0x92, not i8XX */
#define AGP_INTEL_AGPCMD	0xa8
# define AGPCMD_SBA		(1U << 9)
# define AGPCMD_AGPEN		(1U << 8)
# define AGPCMD_FWEN		(1U << 4)
# define AGPCMD_RATE_1X		(1U << 1)
# define AGPCMD_RATE_2X		(1U << 2)
# define AGPCMD_RATE_4X		(1U << 3)

#define AGP_INTEL_AGPCTRL	0xb0
# define AGPCTRL_AGPRSE		(1U << 13) /* AGPRSE (82443 only)*/
# define AGPCTRL_GTLB		(1U << 7)  /* GTLB EN */

#define AGP_INTEL_APSIZE	0xb4
# define APSIZE_MASK		0x3f

#define AGP_INTEL_ATTBASE	0xb8

/*
 * Config offsets for VIA AGP 2.x chipsets.
 */
#define AGP_VIA_GARTCTRL	0x80
#define AGP_VIA_APSIZE		0x84
#define AGP_VIA_ATTBASE		0x88

/*
 * Config offsets for VIA AGP 3.0 chipsets.
 */
#define AGP3_VIA_GARTCTRL	0x90
#define AGP3_VIA_APSIZE		0x94
#define AGP3_VIA_ATTBASE	0x98
#define AGP_VIA_AGPSEL		0xfc

/*
 * Config offsets for SiS AGP chipsets.
 */
#define AGP_SIS_ATTBASE		0x90
#define AGP_SIS_WINCTRL		0x94
#define AGP_SIS_TLBCTRL		0x97
#define AGP_SIS_TLBFLUSH	0x98

/*
 * Config offsets for Ali AGP chipsets.
 */
#define AGP_ALI_AGPCTRL		0xb8
#define AGP_ALI_ATTBASE		0xbc
#define AGP_ALI_TLBCTRL		0xc0

/*
 * Config offsets for the AMD 751 chipset.
 */
#define AGP_AMD751_REGISTERS	0x14
#define AGP_AMD751_APCTRL	0xac
#define AGP_AMD751_MODECTRL	0xb0
#define AGP_AMD751_MODECTRL_SYNEN	0x80
#define AGP_AMD751_MODECTRL2	0xb2
#define AGP_AMD751_MODECTRL2_G1LM	0x01
#define AGP_AMD751_MODECTRL2_GPDCE	0x02
#define AGP_AMD751_MODECTRL2_NGSE	0x08

/*
 * Memory mapped register offsets for AMD 751 chipset.
 */
#define AGP_AMD751_CAPS		0x00
#define AGP_AMD751_CAPS_EHI		0x0800
#define AGP_AMD751_CAPS_P2P		0x0400
#define AGP_AMD751_CAPS_MPC		0x0200
#define AGP_AMD751_CAPS_VBE		0x0100
#define AGP_AMD751_CAPS_REV		0x00ff
#define AGP_AMD751_STATUS	0x02
#define AGP_AMD751_STATUS_P2PS		0x0800
#define AGP_AMD751_STATUS_GCS		0x0400
#define AGP_AMD751_STATUS_MPS		0x0200
#define AGP_AMD751_STATUS_VBES		0x0100
#define AGP_AMD751_STATUS_P2PE		0x0008
#define AGP_AMD751_STATUS_GCE		0x0004
#define AGP_AMD751_STATUS_VBEE		0x0001
#define AGP_AMD751_ATTBASE	0x04
#define AGP_AMD751_TLBCTRL	0x0c

/*
 * Config registers for i810 device 0
 */
#define AGP_I810_SMRAM		0x70
#define AGP_I810_SMRAM_GMS		0xc0
#define AGP_I810_SMRAM_GMS_DISABLED	0x00
#define AGP_I810_SMRAM_GMS_ENABLED_0	0x40
#define AGP_I810_SMRAM_GMS_ENABLED_512	0x80
#define AGP_I810_SMRAM_GMS_ENABLED_1024	0xc0
#define AGP_I810_MISCC		0x72
#define	AGP_I810_MISCC_WINSIZE		0x0001
#define AGP_I810_MISCC_WINSIZE_64	0x0000
#define AGP_I810_MISCC_WINSIZE_32	0x0001
#define AGP_I810_MISCC_PLCK		0x0008
#define AGP_I810_MISCC_PLCK_UNLOCKED	0x0000
#define AGP_I810_MISCC_PLCK_LOCKED	0x0008
#define AGP_I810_MISCC_WPTC		0x0030
#define AGP_I810_MISCC_WPTC_NOLIMIT	0x0000
#define AGP_I810_MISCC_WPTC_62		0x0010
#define AGP_I810_MISCC_WPTC_50		0x0020
#define	AGP_I810_MISCC_WPTC_37		0x0030
#define AGP_I810_MISCC_RPTC		0x00c0
#define AGP_I810_MISCC_RPTC_NOLIMIT	0x0000
#define AGP_I810_MISCC_RPTC_62		0x0040
#define AGP_I810_MISCC_RPTC_50		0x0080
#define AGP_I810_MISCC_RPTC_37		0x00c0

/*
 * Config registers for i810 device 1
 */
#define AGP_I810_GMADR		0x10
#define AGP_I810_MMADR		0x14

/*
 * Memory mapped register offsets for i810 chipset.
 */
#define AGP_I810_PGTBL_CTL	0x2020

/**
 * This field determines the actual size of the global GTT on the 965
 * and G33
 */
#define AGP_I810_PGTBL_SIZE_MASK	0x0000000e
#define AGP_I810_PGTBL_SIZE_512KB	(0 << 1)
#define AGP_I810_PGTBL_SIZE_256KB	(1 << 1)
#define AGP_I810_PGTBL_SIZE_128KB	(2 << 1)
#define AGP_I810_DRT			0x3000
#define AGP_I810_DRT_UNPOPULATED	0x00
#define AGP_I810_DRT_POPULATED		0x01
#define AGP_I810_GTT			0x10000

/*
 * Config registers for i830MG device 0
 */
#define AGP_I830_GCC0			0x50
#define AGP_I830_GCC1			0x52
#define AGP_I830_GCC1_DEV2		0x08
#define AGP_I830_GCC1_DEV2_ENABLED	0x00
#define AGP_I830_GCC1_DEV2_DISABLED	0x08
#define AGP_I830_GCC1_GMS		0x70
#define AGP_I830_GCC1_GMS_STOLEN_512	0x20
#define AGP_I830_GCC1_GMS_STOLEN_1024	0x30
#define AGP_I830_GCC1_GMS_STOLEN_8192	0x40
#define AGP_I830_GCC1_GMASIZE		0x01
#define AGP_I830_GCC1_GMASIZE_64	0x01
#define AGP_I830_GCC1_GMASIZE_128	0x00

/*
 * Memory mapped register offsets for i830 chipset.
 */
#define	AGP_I830_HIC			0x70

/*
 * Config registers for 852GM/855GM/865G device 0
 */
#define AGP_I855_GCC1			0x50 /* upper word */
#define AGP_I855_GCC1_DEV2		0x08
#define AGP_I855_GCC1_DEV2_ENABLED	0x00
#define AGP_I855_GCC1_DEV2_DISABLED	0x08
#define AGP_I855_GCC1_GMS		0xf0	/* Top bit reserved pre-G33 */
#define AGP_I855_GCC1_GMS_STOLEN_0M	0x00
#define AGP_I855_GCC1_GMS_STOLEN_1M	0x10
#define AGP_I855_GCC1_GMS_STOLEN_4M	0x20
#define AGP_I855_GCC1_GMS_STOLEN_8M	0x30
#define AGP_I855_GCC1_GMS_STOLEN_16M	0x40
#define AGP_I855_GCC1_GMS_STOLEN_32M	0x50

/*
 * Config registers for 915G/915GM
 */
#define AGP_I915_MMADR			0x10
#define AGP_I915_GMADR			0x18
#define AGP_I915_GTTADR			0x1c
#define AGP_I915_MSAC			0x60 /* upper word */
#define AGP_I915_MSAC_APER_128M		0x02

/*
 * Config registers for 915G/915GM device 0
 */
#define AGP_I915_GCC1			0x52
#define AGP_I915_GCC1_GMS		0x70
#define AGP_I915_GCC1_GMS_STOLEN_0M	0x00
#define AGP_I915_GCC1_GMS_STOLEN_1M	0x10
#define	AGP_I915_GCC1_GMS_STOLEN_8M	0x30
#define AGP_I915_GCC1_GMS_STOLEN_16M	0x40
#define AGP_I915_GCC1_GMS_STOLEN_32M	0x50
#define AGP_I915_GCC1_GMS_STOLEN_48M	0x60
#define AGP_I915_GCC1_GMS_STOLEN_64M	0x70
#define AGP_I915_IFPADDR		0x60

/*
 * Config registers for 965G/965Q
 */
#define AGP_I965_MMADR			0x10
#define AGP_I965_GMADR			0x18

#define AGP_I965_GTT			(512*1024)

#define AGP_I965_PGTBL_SIZE_1MB		(3 << 1)
#define AGP_I965_PGTBL_SIZE_2MB		(4 << 1)
#define AGP_I965_PGTBL_SIZE_1_5MB	(5 << 1)

/*
 * Config registers for 965G/965Q device 0
 */
#define AGP_I965_IFPADDR		0x70

/*
 * Config registers for G33
 */
#define AGP_G33_PGTBL_SIZE_MASK		(3U << 8)
#define AGP_G33_PGTBL_SIZE_1M		(1U << 8)
#define AGP_G33_PGTBL_SIZE_2M		(2U << 8)

#define AGP_G33_GCC1_GMS_STOLEN_128M	0x80
#define AGP_G33_GCC1_GMS_STOLEN_256M	0x90

/*
 * Config registers for G4X
 */
#define AGP_G4X_MSAC			0x64 /* upper word */
#define AGP_G4X_MSAC_MASK		0xff
#define AGP_G4X_MSAC_APER_256M		0x02
#define AGP_G4X_MSAC_APER_512M		0x04

#define AGP_G4X_GTT			(2*1024*1024)

#define	AGP_G4X_PGTBL_SIZE_MASK		0x0000000e
#define	AGP_G4X_PGTBL_SIZE_512K		(0 << 1)
#define	AGP_G4X_PGTBL_SIZE_256K		(1 << 1)
#define	AGP_G4X_PGTBL_SIZE_128K		(2 << 1)
#define	AGP_G4X_PGTBL_SIZE_1M		(3 << 1)
#define	AGP_G4X_PGTBL_SIZE_2M		(4 << 1)
#define	AGP_G4X_PGTBL_SIZE_1_5M		(5 << 1)

#define AGP_G4X_GCC1_GMS_STOLEN_96M	0xa0
#define AGP_G4X_GCC1_GMS_STOLEN_160M	0xb0
#define AGP_G4X_GCC1_GMS_STOLEN_224M	0xc0
#define AGP_G4X_GCC1_GMS_STOLEN_352M	0xd0

/*
 * AMD64 GART registers
 */
#define	AGP_AMD64_APCTRL		0x90
#define	AGP_AMD64_APBASE		0x94
#define	AGP_AMD64_ATTBASE		0x98
#define	AGP_AMD64_CACHECTRL		0x9c
#define	AGP_AMD64_APCTRL_GARTEN		0x00000001
#define	AGP_AMD64_APCTRL_SIZE_MASK	0x0000000e
#define	AGP_AMD64_APCTRL_DISGARTCPU	0x00000010
#define	AGP_AMD64_APCTRL_DISGARTIO	0x00000020
#define	AGP_AMD64_APCTRL_DISWLKPRB	0x00000040
#define	AGP_AMD64_APBASE_MASK		0x00007fff
#define	AGP_AMD64_ATTBASE_MASK		0xfffffff0
#define	AGP_AMD64_CACHECTRL_INVGART	0x00000001
#define	AGP_AMD64_CACHECTRL_PTEERR	0x00000002

/*
 * NVIDIA nForce3 registers
 */
#define AGP_AMD64_NVIDIA_0_APBASE	0x10
#define AGP_AMD64_NVIDIA_1_APBASE1	0x50
#define AGP_AMD64_NVIDIA_1_APLIMIT1	0x54
#define AGP_AMD64_NVIDIA_1_APSIZE	0xa8
#define AGP_AMD64_NVIDIA_1_APBASE2	0xd8
#define AGP_AMD64_NVIDIA_1_APLIMIT2	0xdc

/*
 * ULi M1689 registers
 */
#define AGP_AMD64_ULI_APBASE		0x10
#define AGP_AMD64_ULI_HTT_FEATURE	0x50
#define AGP_AMD64_ULI_ENU_SCR		0x54

#endif /* !_PCI_AGPREG_H_ */
