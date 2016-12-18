/*	$NetBSD: ydsreg.h,v 1.6 2005/12/11 12:22:51 christos Exp $	*/

/*
 * Copyright (c) 2000, 2001 Kazuki Sakamoto and Minoura Makoto.
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
 * YMF724/740/744/754 registers
 */

#ifndef _DEV_PCI_YDSREG_H_
#define	_DEV_PCI_YDSREG_H_

/*
 * PCI Config Registers
 */
#define	YDS_PCI_MBA		0x10
#define	YDS_PCI_LEGACY		0x40
# define YDS_PCI_LEGACY_SBEN	0x0001
# define YDS_PCI_LEGACY_FMEN	0x0002
# define YDS_PCI_LEGACY_JPEN	0x0004
# define YDS_PCI_LEGACY_MEN	0x0008
# define YDS_PCI_LEGACY_MIEN	0x0010
# define YDS_PCI_LEGACY_IO	0x0020
# define YDS_PCI_LEGACY_SDMA0	0x0000
# define YDS_PCI_LEGACY_SDMA1	0x0040
# define YDS_PCI_LEGACY_SDMA3	0x00c0
# define YDS_PCI_LEGACY_SBIRQ5	0x0000
# define YDS_PCI_LEGACY_SBIRQ7	0x0100
# define YDS_PCI_LEGACY_SBIRQ9	0x0200
# define YDS_PCI_LEGACY_SBIRQ10	0x0300
# define YDS_PCI_LEGACY_SBIRQ11	0x0400
# define YDS_PCI_LEGACY_MPUIRQ5	0x0000
# define YDS_PCI_LEGACY_MPUIRQ7	0x0800
# define YDS_PCI_LEGACY_MPUIRQ9	0x1000
# define YDS_PCI_LEGACY_MPUIRQ10 0x1800
# define YDS_PCI_LEGACY_MPUIRQ11 0x2000
# define YDS_PCI_LEGACY_SIEN	0x4000
# define YDS_PCI_LEGACY_LAD	0x8000

# define YDS_PCI_EX_LEGACY_FMIO_388	(0x0000 << 16)
# define YDS_PCI_EX_LEGACY_FMIO_398	(0x0001 << 16)
# define YDS_PCI_EX_LEGACY_FMIO_3A0	(0x0002 << 16)
# define YDS_PCI_EX_LEGACY_FMIO_3A8	(0x0003 << 16)
# define YDS_PCI_EX_LEGACY_SBIO_220	(0x0000 << 16)
# define YDS_PCI_EX_LEGACY_SBIO_240	(0x0004 << 16)
# define YDS_PCI_EX_LEGACY_SBIO_260	(0x0008 << 16)
# define YDS_PCI_EX_LEGACY_SBIO_280	(0x000c << 16)
# define YDS_PCI_EX_LEGACY_MPUIO_330	(0x0000 << 16)
# define YDS_PCI_EX_LEGACY_MPUIO_300	(0x0010 << 16)
# define YDS_PCI_EX_LEGACY_MPUIO_332	(0x0020 << 16)
# define YDS_PCI_EX_LEGACY_MPUIO_334	(0x0030 << 16)
# define YDS_PCI_EX_LEGACY_JSIO_201	(0x0000 << 16)
# define YDS_PCI_EX_LEGACY_JSIO_202	(0x0040 << 16)
# define YDS_PCI_EX_LEGACY_JSIO_204	(0x0080 << 16)
# define YDS_PCI_EX_LEGACY_JSIO_205	(0x00c0 << 16)
# define YDS_PCI_EX_LEGACY_MAIM		(0x0100 << 16)
# define YDS_PCI_EX_LEGACY_SMOD_PCI	(0x0000 << 16)
# define YDS_PCI_EX_LEGACY_SMOD_DISABLE	(0x0800 << 16)
# define YDS_PCI_EX_LEGACY_SMOD_DDMA	(0x1000 << 16)
# define YDS_PCI_EX_LEGACY_SBVER_3	(0x0000 << 16)
# define YDS_PCI_EX_LEGACY_SBVER_2	(0x2000 << 16)
# define YDS_PCI_EX_LEGACY_SBVER_1	(0x4000 << 16)
# define YDS_PCI_EX_LEGACY_IMOD		(0x8000 << 16)

#define	YDS_PCI_DSCTRL		0x48
# define YDS_DSCTRL_CRST	 0x0001
# define YDS_DSCTRL_WRST	 0x0004
# define YDS_DSCTRL_ACLS	 0x0008
#define YDS_PCI_DSPOWER1	0x4a
# define YDS_DSPOWER1_DMC	 0x0001
# define YDS_DSPOWER1_DPLL	 0x0002
# define YDS_DSPOWER1_JSR	 0x0040
#define YDS_PCI_DISTDMA		0x4c
#define YDS_PCI_DSPOWER2	0x4e
# define YDS_DSPOWER2_CMCD	 0x0001
# define YDS_DSPOWER2_PSFM	 0x0002
# define YDS_DSPOWER2_PSSB	 0x0004
# define YDS_DSPOWER2_PSMPU	 0x0008
# define YDS_DSPOWER2_PSJOY	 0x0010
# define YDS_DSPOWER2_PSPCA	 0x0020
# define YDS_DSPOWER2_PSSRC	 0x0040
# define YDS_DSPOWER2_PSZV	 0x0080
# define YDS_DSPOWER2_PSDIT	 0x0100
# define YDS_DSPOWER2_PSDIR	 0x0200
# define YDS_DSPOWER2_PSACL	 0x0400
# define YDS_DSPOWER2_PSIO	 0x0800
# define YDS_DSPOWER2_PSHWV	 0x1000

#define YDS_PCI_FM_BA		0x60
#define YDS_PCI_SB_BA		0x62
#define YDS_PCI_MPU_BA		0x64
#define YDS_PCI_JS_BA		0x66

/*
 * DS-1 PCI Audio part registers
 */
#define YDS_INTERRUPT_FLAGS	0x0004
#define YDS_INTERRUPT_FLAGS_TI	0x0001
#define YDS_ACTIVITY		0x0006
# define YDS_ACTIVITY_DOCKA	0x0010
#define	YDS_GLOBAL_CONTROL	0x0008
# define YDS_GLCTRL_HVE		0x0001
# define YDS_GLCTRL_HVIE	0x0002

#define YDS_GPIO_IIF		0x0050
# define YDS_GPIO_GIO0		0x0001
# define YDS_GPIO_GIO1		0x0002
# define YDS_GPIO_GIO2		0x0004
#define YDS_GPIO_IIE		0x0052
# define YDS_GPIO_GIE0		0x0001
# define YDS_GPIO_GIE1		0x0002
# define YDS_GPIO_GIE2		0x0004
#define YDS_GPIO_ISTAT		0x0054
# define YDS_GPIO_GPI0		0x0001
# define YDS_GPIO_GPI1		0x0002
# define YDS_GPIO_GPI2		0x0004
#define YDS_GPIO_OCTRL		0x0056
# define YDS_GPIO_GPO0		0x0001
# define YDS_GPIO_GPO1		0x0002
# define YDS_GPIO_GPO2		0x0004
#define YDS_GPIO_FUNCE		0x0058
# define YDS_GPIO_GPC0		0x0001
# define YDS_GPIO_GPC1		0x0002
# define YDS_GPIO_GPC2		0x0004
# define YDS_GPIO_GPE0		0x0010
# define YDS_GPIO_GPE1		0x0020
# define YDS_GPIO_GPE2		0x0040
#define YDS_GPIO_ITYPE		0x005a
# define YDS_GPIO_GPT0_LEVEL	0x0000
# define YDS_GPIO_GPT0_RISE	0x0001
# define YDS_GPIO_GPT0_FALL	0x0002
# define YDS_GPIO_GPT0_BOTH	0x0003
# define YDS_GPIO_GPT0_MASK	0x0003
# define YDS_GPIO_GPT1_LEVEL	0x0004
# define YDS_GPIO_GPT1_RISE	0x0005
# define YDS_GPIO_GPT1_FALL	0x0006
# define YDS_GPIO_GPT1_BOTH	0x0007
# define YDS_GPIO_GPT1_MASK	0x0007
# define YDS_GPIO_GPT2_LEVEL	0x0000
# define YDS_GPIO_GPT2_RISE	0x0010
# define YDS_GPIO_GPT2_FALL	0x0020
# define YDS_GPIO_GPT2_BOTH	0x0030
# define YDS_GPIO_GPT2_MASK	0x0030

#define	YDS_GLOBAL_CONTROL	0x0008
# define YDS_GLCTRL_HVE		0x0001
# define YDS_GLCTRL_HVIE	0x0002

#define	AC97_CMD_DATA		0x0060
#define	AC97_CMD_ADDR		0x0062
# define AC97_ID(id)		((id) << 8)
# define AC97_CMD_READ		0x8000
# define AC97_CMD_WRITE		0x0000
#define	AC97_STAT_DATA1		0x0064
#define	AC97_STAT_ADDR1		0x0066
#define	AC97_STAT_DATA2		0x0068
#define	AC97_STAT_ADDR2		0x006a
# define AC97_BUSY		0x8000
#define AC97_SECONDARY_CONF	0x0070
# define AC97_SECONDARY_RSOC	0x0001
# define AC97_SECONDARY_PHWV	0x0002
# define AC97_SECONDARY_SHWV	0x0004
# define AC97_SECONDARY_4CHEN	0x0010
# define AC97_SECONDARY_4CHSEL	0x0020

#define	YDS_LEGACY_OUT_VOLUME	0x0080
#define	YDS_DAC_OUT_VOLUME	0x0084
#define	YDS_DAC_OUT_VOL_L	0x0084
#define	YDS_DAC_OUT_VOL_R	0x0086
#define	YDS_ZV_OUT_VOLUME	0x0088
#define	YDS_2ND_OUT_VOLUME	0x008C
#define	YDS_ADC_OUT_VOLUME	0x0090
#define	YDS_LEGACY_REC_VOLUME	0x0094
#define	YDS_DAC_REC_VOLUME	0x0098
#define	YDS_ZV_REC_VOLUME	0x009C
#define	YDS_2ND_REC_VOLUME	0x00A0
#define	YDS_ADC_REC_VOLUME	0x00A4
#define	YDS_ADC_IN_VOLUME	0x00A8
#define	YDS_REC_IN_VOLUME	0x00AC
#define	YDS_P44_OUT_VOLUME	0x00B0
#define	YDS_P44_REC_VOLUME	0x00B4
#define	YDS_SPDIFIN_OUT_VOLUME	0x00B8
#define	YDS_SPDIFIN_REC_VOLUME	0x00BC

#define	YDS_ADC_SAMPLE_RATE	0x00c0
#define	YDS_REC_SAMPLE_RATE	0x00c4
#define	YDS_ADC_FORMAT		0x00c8
#define	YDS_REC_FORMAT		0x00cc
# define YDS_FORMAT_8BIT	0x01
# define YDS_FORMAT_STEREO	0x02

#define	YDS_STATUS		0x0100
# define YDS_STAT_ACT		0x00000001
# define YDS_STAT_WORK		0x00000002
# define YDS_STAT_TINT		0x00008000
# define YDS_STAT_INT		0x80000000
#define	YDS_CONTROL_SELECT	0x0104
# define YDS_CSEL		0x00000001
#define	YDS_MODE		0x0108
# define YDS_MODE_ACTV		0x00000001
# define YDS_MODE_ACTV2		0x00000002
# define YDS_MODE_TOUT		0x00008000
# define YDS_MODE_RESET		0x00010000
# define YDS_MODE_AC3		0x40000000
# define YDS_MODE_MUTE		0x80000000

#define	YDS_CONFIG		0x0114
# define YDS_DSP_DISABLE	0
# define YDS_DSP_SETUP		0x00000001

#define	YDS_PLAY_CTRLSIZE	0x0140
#define	YDS_REC_CTRLSIZE	0x0144
#define	YDS_EFFECT_CTRLSIZE	0x0148
#define	YDS_WORK_SIZE		0x014c
#define	YDS_MAPOF_REC		0x0150
# define YDS_RECSLOT_VALID	0x00000001
# define YDS_ADCSLOT_VALID	0x00000002
#define	YDS_MAPOF_EFFECT	0x0154
# define YDS_DL_VALID		0x00000001
# define YDS_DR_VALID		0x00000002
# define YDS_EFFECT1_VALID	0x00000004
# define YDS_EFFECT2_VALID	0x00000008
# define YDS_EFFECT3_VALID	0x00000010

#define	YDS_PLAY_CTRLBASE	0x0158
#define	YDS_REC_CTRLBASE	0x015c
#define	YDS_EFFECT_CTRLBASE	0x0160
#define	YDS_WORK_BASE		0x0164

#define	YDS_DSP_INSTRAM		0x1000
#define	YDS_CTRL_INSTRAM	0x4000

typedef enum {
	YDS_DS_1,
	YDS_DS_1E
} yds_dstype_t;

#define	AC97_TIMEOUT		1000
#define	YDS_WORK_TIMEOUT	250000

/* slot control data structures */
#define	MAX_PLAY_SLOT_CTRL	64
#define	N_PLAY_SLOT_CTRL_BANK	2
#define	N_REC_SLOT_CTRL		2
#define	N_REC_SLOT_CTRL_BANK	2

/*
 * play slot
 */
union play_slot_table {
	uint32_t numofplay;
	uint32_t slotbase;
};

struct play_slot_ctrl_bank {
	uint32_t format;
#define	PSLT_FORMAT_STEREO	0x00010000
#define	PSLT_FORMAT_8BIT	0x80000000
#define	PSLT_FORMAT_SRC441	0x10000000
#define PSLT_FORMAT_RCH		0x00000001
	uint32_t loopdefault;
	uint32_t pgbase;
	uint32_t pgloop;
	uint32_t pgloopend;
	uint32_t pgloopfrac;
	uint32_t pgdeltaend;
	uint32_t lpfkend;
	uint32_t eggainend;
	uint32_t lchgainend;
	uint32_t rchgainend;
	uint32_t effect1gainend;
	uint32_t effect2gainend;
	uint32_t effect3gainend;
	uint32_t lpfq;
	uint32_t status;
#define	PSLT_STATUS_DEND	0x00000001
	uint32_t numofframes;
	uint32_t loopcount;
	uint32_t pgstart;
	uint32_t pgstartfrac;
	uint32_t pgdelta;
	uint32_t lpfk;
	uint32_t eggain;
	uint32_t lchgain;
	uint32_t rchgain;
	uint32_t effect1gain;
	uint32_t effect2gain;
	uint32_t effect3gain;
	uint32_t lpfd1;
	uint32_t lpfd2;
};

/*
 * rec slot
 */
struct rec_slot_ctrl_bank {
	uint32_t pgbase;
	uint32_t pgloopendadr;
	uint32_t pgstartadr;
	uint32_t numofloops;
};

struct rec_slot {
	struct rec_slot_ctrl {
		struct rec_slot_ctrl_bank bank[N_REC_SLOT_CTRL_BANK];
	} ctrl[N_REC_SLOT_CTRL];
};

/*
 * effect slot
 */
struct effect_slot_ctrl_bank {
	uint32_t pgbase;
	uint32_t pgloopend;
	uint32_t pgstart;
	uint32_t temp;
};

#endif /* _DEV_PCI_YDSREG_H_ */
