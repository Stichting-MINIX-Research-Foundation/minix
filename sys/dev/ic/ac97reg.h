/*	$NetBSD: ac97reg.h,v 1.13 2005/12/11 12:21:25 christos Exp $	*/

/*
 * Copyright (c) 1999 Constantine Sapuntzakis
 *
 * Author:        Constantine Sapuntzakis <csapuntz@stanford.edu>
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
 * THIS SOFTWARE IS PROVIDED BY CONSTANTINE SAPUNTZAKIS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	AC97_REG_RESET			0x00
#define		AC97_CAPS_MICIN			0x0001
#define		AC97_CAPS_MODEMLINECODEC	0x0002 /* 1.x only! */
#define		AC97_CAPS_TONECTRL		0x0004
#define		AC97_CAPS_SIMSTEREO		0x0008
#define		AC97_CAPS_HEADPHONES		0x0010
#define		AC97_CAPS_LOUDNESS		0x0020
#define		AC97_CAPS_DAC18			0x0040
#define		AC97_CAPS_DAC20			0x0080
#define		AC97_CAPS_ADC18			0x0100
#define		AC97_CAPS_ADC20			0x0200
#define		AC97_CAPS_ENHANCEMENT_MASK	0xfc00
#define		AC97_CAPS_ENHANCEMENT_SHIFT	10
#define		AC97_CAPS_ENHANCEMENT(reg)	(((reg) >> 10) & 0x1f)
#define	AC97_REG_MASTER_VOLUME		0x02
#define	AC97_REG_HEADPHONE_VOLUME	0x04
#define	AC97_REG_MASTER_VOLUME_MONO	0x06
#define	AC97_REG_MASTER_TONE		0x08
#define	AC97_REG_PCBEEP_VOLUME		0x0a
#define	AC97_REG_PHONE_VOLUME		0x0c
#define	AC97_REG_MIC_VOLUME		0x0e
#define	AC97_REG_LINEIN_VOLUME		0x10
#define	AC97_REG_CD_VOLUME		0x12
#define	AC97_REG_VIDEO_VOLUME		0x14
#define	AC97_REG_AUX_VOLUME		0x16
#define	AC97_REG_PCMOUT_VOLUME		0x18
#define	AC97_REG_RECORD_SELECT		0x1a
#define	AC97_REG_RECORD_GAIN		0x1c
#define	AC97_REG_RECORD_GAIN_MIC	0x1e /* for dedicated mic */
#define	AC97_REG_GP			0x20
#define	AC97_REG_3D_CONTROL		0x22
				/*	0x24	Modem sample rate in AC97 1.03
						Reserved in AC97 2.0
						Interrupt/paging in AC97 2.3 */
#define	AC97_REG_POWER			0x26
#define		AC97_POWER_ADC			0x0001
#define		AC97_POWER_DAC			0x0002
#define		AC97_POWER_ANL			0x0004
#define		AC97_POWER_REF			0x0008
#define		AC97_POWER_IN			0x0100
#define		AC97_POWER_OUT			0x0200
#define		AC97_POWER_MIXER		0x0400
#define		AC97_POWER_MIXER_VREF		0x0800
#define		AC97_POWER_ACLINK		0x1000
#define		AC97_POWER_CLK			0x2000
#define		AC97_POWER_AUX			0x4000
#define		AC97_POWER_EAMP			0x8000

/* AC'97 2.0 extensions -- 0x28-0x3a */
#define AC97_REG_EXT_AUDIO_ID		0x28
#define AC97_REG_EXT_AUDIO_CTRL		0x2a
#define		AC97_EXT_AUDIO_VRA		0x0001
#define		AC97_EXT_AUDIO_DRA		0x0002
#define		AC97_EXT_AUDIO_SPDIF		0x0004
#define		AC97_EXT_AUDIO_VRM		0x0008 /* for dedicated mic */
#define		AC97_EXT_AUDIO_DSA_MASK		0x0030 /* for EXT ID */
#define		 AC97_EXT_AUDIO_DSA00		0x0000 /* for EXT ID */
#define		 AC97_EXT_AUDIO_DSA01		0x0010 /* for EXT ID */
#define		 AC97_EXT_AUDIO_DSA10		0x0020 /* for EXT ID */
#define		 AC97_EXT_AUDIO_DSA11		0x0030 /* for EXT ID */
#define		AC97_EXT_AUDIO_SPSA_MASK	0x0030 /* for EXT CTRL */
#define		 AC97_EXT_AUDIO_SPSA34		0x0000 /* for EXT CTRL */
#define		 AC97_EXT_AUDIO_SPSA78		0x0010 /* for EXT CTRL */
#define		 AC97_EXT_AUDIO_SPSA69		0x0020 /* for EXT CTRL */
#define		 AC97_EXT_AUDIO_SPSAAB		0x0030 /* for EXT CTRL */
#define		AC97_EXT_AUDIO_CDAC		0x0040
#define		AC97_EXT_AUDIO_SDAC		0x0080
#define		AC97_EXT_AUDIO_LDAC		0x0100
#define		AC97_EXT_AUDIO_AMAP		0x0200 /* for EXT ID */
#define		AC97_EXT_AUDIO_REV_MASK		0x0C00 /* for EXT ID */
#define		 AC97_EXT_AUDIO_REV_11		0x0000 /* for EXT ID */
#define		 AC97_EXT_AUDIO_REV_22		0x0400 /* for EXT ID */
#define		 AC97_EXT_AUDIO_REV_23		0x0800 /* for EXT ID */
#define		 AC97_EXT_AUDIO_REV_RESERVED11	0x0c00 /* for EXT ID */
#define		AC97_EXT_AUDIO_ID_MASK		0xC000 /* for EXT ID */
#define		 AC97_EXT_AUDIO_ID_PRIMARY	0x0000 /* for EXT ID */
#define		 AC97_EXT_AUDIO_ID_SECONDARY01	0x4000 /* for EXT ID */
#define		 AC97_EXT_AUDIO_ID_SECONDARY10	0x8000 /* for EXT ID */
#define		 AC97_EXT_AUDIO_ID_SECONDARY11	0xc000 /* for EXT ID */
#define		AC97_EXT_AUDIO_MADC		0x0200 /* for EXT CTRL */
#define		AC97_EXT_AUDIO_SPCV		0x0400 /* for EXT CTRL */
#define		AC97_EXT_AUDIO_PRI		0x0800 /* for EXT CTRL */
#define		AC97_EXT_AUDIO_PRJ		0x1000 /* for EXT CTRL */
#define		AC97_EXT_AUDIO_PRK		0x2000 /* for EXT CTRL */
#define		AC97_EXT_AUDIO_PRL		0x4000 /* for EXT CTRL */
#define		AC97_EXT_AUDIO_VCFG		0x8000 /* for EXT CTRL */

#define		AC97_SINGLE_RATE		48000
#define	AC97_REG_PCM_FRONT_DAC_RATE	0x2c
#define	AC97_REG_PCM_SURR_DAC_RATE	0x2e
#define	AC97_REG_PCM_LFE_DAC_RATE	0x30
#define	AC97_REG_PCM_LR_ADC_RATE	0x32
#define	AC97_REG_PCM_MIC_ADC_RATE	0x34	/* dedicated mic */
#define	AC97_REG_CENTER_LFE_MASTER	0x36	/* center + LFE master volume */
#define	AC97_REG_SURR_MASTER		0x38	/* surround (rear) master vol */
#define AC97_REG_SPDIF_CTRL		0x3a
#define		AC97_SPDIF_V			0x8000
#define		AC97_SPDIF_DRS			0x4000
#define		AC97_SPDIF_SPSR_MASK		0x3000
#define		 AC97_SPDIF_SPSR_44K		0x0000
#define		 AC97_SPDIF_SPSR_48K		0x2000
#define		 AC97_SPDIF_SPSR_32K		0x1000
#define		AC97_SPDIF_L			0x0800
#define		AC97_SPDIF_CC_MASK		0x07f0
#define		AC97_SPDIF_PRE			0x0008
#define		AC97_SPDIF_COPY			0x0004
#define		AC97_SPDIF_NONAUDIO		0x0002
#define		AC97_SPDIF_PRO			0x0001

/* Modem -- 0x3c-0x58 */
#define	AC97_REG_EXT_MODEM_ID		0x3c	/* extended modem id */
#define		AC97_EXT_MODEM_LINE1		0x0001
#define		AC97_EXT_MODEM_LINE2		0x0002
#define		AC97_EXT_MODEM_HANDSET		0x0004
#define		AC97_EXT_MODEM_CID1		0x0008
#define		AC97_EXT_MODEM_CID2		0x0010
#define		AC97_EXT_MODEM_ID0		0x4000
#define		AC97_EXT_MODEM_ID1		0x8000
#define		AC97_EXT_MODEM_ID_MASK		0xc000
#define	AC97_REG_EXT_MODEM_CTRL		0x3e	/* extended modem ctrl */
#define		AC97_EXT_MODEM_CTRL_GPIO	0x0001	/* gpio is ready */
#define		AC97_EXT_MODEM_CTRL_MREF	0x0002	/* vref up */
#define		AC97_EXT_MODEM_CTRL_ADC1	0x0004	/* line1 adc ready */
#define		AC97_EXT_MODEM_CTRL_DAC1	0x0008	/* line1 dac ready */
#define		AC97_EXT_MODEM_CTRL_ADC2	0x0010	/* line2 adc ready */
#define		AC97_EXT_MODEM_CTRL_DAC2	0x0020	/* line2 dac ready */
#define		AC97_EXT_MODEM_CTRL_HADC	0x0040	/* handset adc ready */
#define		AC97_EXT_MODEM_CTRL_HDAC	0x0080	/* handset dac ready */
#define		AC97_EXT_MODEM_CTRL_PRA		0x0100	/* gpio off */
#define		AC97_EXT_MODEM_CTRL_PRB		0x0200	/* vref off */
#define		AC97_EXT_MODEM_CTRL_PRC		0x0400	/* line1 adc off */
#define		AC97_EXT_MODEM_CTRL_PRD		0x0800	/* line1 dac off */
#define		AC97_EXT_MODEM_CTRL_PRE		0x1000	/* line2 adc off */
#define		AC97_EXT_MODEM_CTRL_PRF		0x2000	/* line2 dac off */
#define		AC97_EXT_MODEM_CTRL_PRG		0x4000	/* handset adc off */
#define		AC97_EXT_MODEM_CTRL_PRH		0x8000	/* handset dac off */
#define AC97_REG_LINE1_RATE		0x40
#define	AC97_REG_LINE2_RATE		0x42
#define	AC97_REG_HANDSET_RATE		0x44
#define	AC97_REG_LINE1_LEVEL		0x46
#define	AC97_REG_LINE2_LEVEL		0x48
#define	AC97_REG_HANDSET_LEVEL		0x4a
#define	AC97_REG_GPIO_CFG		0x4c	/* gpio config */
#define	AC97_REG_GPIO_POLARITY		0x4e	/* gpio pin polarity */
#define	AC97_REG_GPIO_STICKY		0x50	/* gpio pin sticky */
#define	AC97_REG_GPIO_WAKEUP		0x52	/* gpio pin wakeup */
#define	AC97_REG_GPIO_STATUS		0x54
#define		AC97_GPIO_LINE1_OH		0x0001	/* off-hook */
#define		AC97_GPIO_LINE1_RI		0x0002	/* ring detect */
#define		AC97_GPIO_LINE1_CID		0x0004	/* caller-id */
#define		AC97_GPIO_LINE2_OH		0x0400	/* off-hook */
#define		AC97_GPIO_LINE2_RI		0x0800	/* ring detect */
#define		AC97_GPIO_LINE2_CID		0x1000	/* caller-id */
#define	AC97_REG_MISC_AFE		0x56	/* misc modem afe status & control */
#define		AC97_MISC_AFE_L1B_MASK		0x0007	/* line1 loopback */
#define		AC97_MISC_AFE_L2B_MASK		0x0070	/* line2 loopback */
#define		AC97_MISC_AFE_HSB_MASK		0x0700	/* handset loopback */
#define		AC97_MISC_AFE_MLNK		0x1000	/* ac-link status */
#define		AC97_MISC_AFE_CID1		0x2000	/* line1 cid decode */
#define		AC97_MISC_AFE_CID2		0x4000	/* line2 cid decide */
#define		AC97_MISC_AFE_CIDR		0x8000	/* raw cid data */

/* Modem loopback modes */
#define		AC97_LOOPBACK_DISABLE		0
#define		AC97_LOOPBACK_ADC		1
#define		AC97_LOOPBACK_LOCAL_ANALOG	2
#define		AC97_LOOPBACK_DAC		3
#define		AC97_LOOPBACK_REMOTE_ANALOG	4
#define		AC97_LOOPBACK_VENDOR1		5
#define		AC97_LOOPBACK_VENDOR2		6
#define		AC97_LOOPBACK_VENDOR3		7



/* Vendor specific -- 0x5a-0x7b */

#define	AC97_REG_VENDOR_ID1		0x7c
#define	AC97_REG_VENDOR_ID2		0x7e
#define		AC97_VENDOR_ID_MASK		0xffffff00

#define	AC97_CODEC_ID(a0, a1, a2, x)					\
	(((a0) << 24) | ((a1) << 16) | ((a2) << 8) | (x))

#define	AC97_GET_CODEC_ID(id, cp)					\
do {									\
	(cp)[0] = ((id) >> 24) & 0xff;					\
	(cp)[1] = ((id) >> 16) & 0xff;					\
	(cp)[2] = ((id) >> 8)  & 0xff;					\
	(cp)[3] = (id) & 0xff;						\
} while (0)
