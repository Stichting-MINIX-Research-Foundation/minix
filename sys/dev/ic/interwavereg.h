#ifndef INTERWAVEREG_H
#define INTERWAVEREG_H

/*	$NetBSD: interwavereg.h,v 1.9 2008/04/28 20:23:50 martin Exp $	*/

/*
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Kari Mettinen
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


#define IW_LINELEVEL_MAX	((1L << 10) - 1)
#define IW_LINELEVEL_CODEC_MAX	((1L << 10) - 1)

#define IW_OUTPUT_CLASS		10
#define IW_INPUT_CLASS		11
#define IW_RECORD_CLASS		12


#define IW_MIC_IN		11
#define IW_MIC_IN_LVL		0

/* these 2 are hw dependent values */
#define IW_RIGHT_MIC_IN_PORT	0x16
#define IW_LEFT_MIC_IN_PORT	0x17

#define IW_AUX1			12
#define IW_AUX1_LVL		1

#define IW_RIGHT_AUX1_PORT	0x02
#define IW_LEFT_AUX1_PORT	0x03

#define IW_AUX2			13
#define IW_AUX2_LVL		2

#define IW_RIGHT_AUX2_PORT	0x04
#define IW_LEFT_AUX2_PORT	0x05

#define IW_LINE_IN		14
#define IW_LINE_IN_LVL		3

#define IW_RIGHT_LINE_IN_PORT	0x12
#define IW_LEFT_LINE_IN_PORT	0x13

#define IW_LINE_OUT		15
#define IW_LINE_OUT_LVL		4

#define IW_RIGHT_LINE_OUT_PORT	0x19
#define IW_LEFT_LINE_OUT_PORT	0x1b

#define IW_RECORD_SOURCE	5

#define IW_REC			16
#define IW_REC_LVL		6
#define IW_REC_LEFT_PORT	0x00
#define IW_REC_RIGHT_PORT	0x01

#define IW_DAC			18
#define IW_DAC_LVL		7
#define IW_LEFT_DAC_PORT	0x06
#define IW_RIGHT_DAC_PORT	0x07

#define IW_LOOPBACK		19
#define IW_LOOPBACK_LVL		8
#define IW_LOOPBACK_PORT	0x0d

#define IW_MONO_IN		20
#define IW_MONO_IN_LVL		9
#define IW_MONO_IN_PORT		0x1a

#define IW_LINE_IN_SRC		0
#define IW_AUX1_SRC		1
#define IW_MIC_IN_SRC		2
#define IW_MIX_OUT_SRC		3


/* DMA flags */

#define IW_PLAYBACK 1L
#define IW_RECORD   2L

#define ADDR_HIGH(a)  (u_short)((a) >> 7)
#define ADDR_LOW(a)   (u_short)((a) << 9)

#define MIDI_TX_IRQ	  0x01
#define MIDI_RX_IRQ	  0x02
#define ALIB_TIMER1_IRQ	  0x04
#define ALIB_TIMER2_IRQ	  0x08
#define UASBCI		  0x45		/* UASBCI index */
#define SAMPLE_CONTROL	  0x49		/* Not used by IW */
#define SET_VOICES	  0x0E
#define SAVI_WR		  0x0E
#define WAVETABLE_IRQ	  0x20
#define ENVELOPE_IRQ	  0x40
#define DMA_TC_IRQ	  0x80

#define GEN_INDEX	  0x03		 /* IGIDX offset into p3xr */
#define VOICE_SELECT	  0x02		 /* SVSR offset into p3xr */
#define VOICE_IRQS	  0x8F		 /* SVII index (read) */
#define URSTI		  0x4C		 /* URSTI index */
#define GF1_SET		  0x01		 /* URSTI[0] */
#define GF1_OUT_ENABLE	  0x02		 /* URSTI[1] */
#define GF1_IRQ_ENABLE	  0x04		 /* URSTI[2] */
#define GF1_RESET	  0xFE		 /* URSTI[0]=0 */
#define VOICE_VOLUME_IRQ  0x04		 /* SVII[2] */
#define VOICE_WAVE_IRQ	  0x08		 /* SVII[3] */
#define VC_IRQ_ENABLE	  0x20		 /* SACI[5] or SVCI[5]*/
#define VOICE_NUMBER	  0x1F		 /* Mask for SVII[4:0] */
#define VC_IRQ_PENDING	  0x80		 /* SACI[7] or SVCI[7] */
#define VC_DIRECT	  0x40		 /* SACI[6] or SVCI[6]*/
#define VC_DATA_WIDTH	  0x04		 /* SACI[2] */
#define VOICE_STOP	  0x02		 /* SACI[1] */
#define VOICE_STOPPED	  0x01		 /* SACI[0] */
#define VOLUME_STOP	  0x02		 /* SVCI[1] */
#define VOLUME_STOPPED	  0x01		 /* SVCI[0] */
#define VC_ROLLOVER	  0x04		 /* SVCI[2] */
#define VC_LOOP_ENABLE	  0x08		 /* SVCI[3] or SACI[3]*/
#define VC_BI_LOOP	  0x10		 /* SVCI[4] or SACI[4]*/
#define VOICE_OFFSET	  0x20		 /* SMSI[5] */
#define VOLUME_RATE0	  0x00		 /* SVRI[7:6]=(0,0) */
#define VOLUME_RATE1	  0x40		 /* SVRI[7:6]=(0,1) */
#define VOLUME_RATE2	  0x80		 /* SVRI[7:6]=(1,0) */
#define VOLUME_RATE3	  0xC0		 /* SVRI[7:6]=(1,1) */

#define CSR1R		  0x02
#define CPDR		  0x03
#define CRDR		  0x03

#define SHUT_DOWN	  0x7E		 /* shuts InterWave down */
#define POWER_UP	  0xFE		 /* enables all modules */
#define CODEC_PWR_UP	  0x81		 /* enables Codec Analog Ckts */
#define CODEC_PWR_DOWN	  0x01		 /* disables Codec Analog Ckts */
#define CODEC_REC_UP	  0x82		 /* Enables Record Path */
#define CODEC_REC_DOWN	  0x02		 /* Disables Record Path */
#define CODEC_PLAY_UP	  0x84		 /* Enables Playback Path */
#define CODEC_PLAY_DOWN	  0x04		 /* Disables Playback Path */
#define CODEC_IRQ_ENABLE  0x02		 /* CEXTI[2] */
#define CODEC_TIMER_IRQ	  0x40		 /* CSR3I[6] */
#define CODEC_REC_IRQ	  0x20		 /* CSR3I[5] */
#define CODEC_PLAY_IRQ	  0x10		 /* CSR3I[4] */
#define CODEC_INT	  0x01		 /* CSR1R[0] */
#define MONO_INPUT	  0x80		 /* CMONOI[7] */
#define MONO_OUTPUT	  0x40		 /* CMONOI[6] */
#define MIDI_UP		  0x88		 /* Enables MIDI ports */
#define MIDI_DOWN	  0x08		 /* Disables MIDI ports */
#define SYNTH_UP	  0x90		 /* Enables Synthesizer */
#define SYNTH_DOWN	  0x10		 /* Disables Synthesizer */
#define LMC_UP		  0xA0		 /* Enables LM Module */
#define LMC_DOWN	  0x20		 /* Disbales LM Module */
#define XTAL24_UP	  0xC0		 /* Enables 24MHz Osc */
#define XTAL24_DOWN	  0x40		 /* Disables 24MHz Osc */
#define PPWRI		  0xF2		 /* PPWRI index */
#define PLAY		  0x0F
#define REC		  0x1F
#define LEFT_AUX1_INPUT	  0x02
#define RIGHT_AUX1_INPUT  0x03
#define LEFT_AUX2_INPUT	  0x04
#define RIGHT_AUX2_INPUT  0x05
#define LEFT_LINE_IN	  0x12
#define RIGHT_LINE_IN	  0x13
#define LEFT_LINE_OUT	  0x19
#define RIGHT_LINE_OUT	  0x1B
#define LEFT_SOURCE	  0x00
#define RIGHT_SOURCE	  0x01
#define LINE_IN		  0x00
#define AUX1_IN		  0x40
#define MIC_IN		  0x80
#define MIX_IN		  0xC0
#define LEFT_DAC	  0x06
#define RIGHT_DAC	  0x07
#define LEFT_MIC_IN	  0x16
#define RIGHT_MIC_IN	  0x17
#define CUPCTI		  0x0E
#define CLPCTI		  0x0F
#define CURCTI		  0x1E
#define CLRCTI		  0x1F
#define CLAX1I		  0x02
#define CRAX1I		  0x03
#define CLAX2I		  0x04
#define CRAX2I		  0x05
#define CLLICI		  0x12
#define CRLICI		  0x13
#define CLOAI		  0x19
#define CROAI		  0x1B
#define CLICI		  0x00
#define CRICI		  0x01
#define CLDACI		  0x06
#define CRDACI		  0x07
#define CPVFI		  0x1D

#define MAX_DMA		  0x07
#define DMA_DECREMENT	  0x20
#define AUTO_INIT	  0x10
#define DMA_READ	  0x01
#define DMA_WRITE	  0x02
#define AUTO_READ	  0x03
#define AUTO_WRITE	  0x04
#define IDMA_INV	  0x0400
#define IDMA_WIDTH_16	  0x0100

#define LDMACI		  0x41	/* Index */
#define DMA_INV		  0x80
#define DMA_IRQ_ENABLE	  0x20
#define DMA_IRQ_PENDING	  0x40	/* on reads of LDMACI[6] */
#define DMA_DATA_16	  0x40	/* on writes to LDMACI[6] */
#define DMA_WIDTH_16	  0x04	/* 1=16-bit, 0=8-bit (DMA channel) */
#define DMA_RATE	  0x18	/* 00=fastest,...,11=slowest */
#define DMA_UPLOAD	  0x02	/* From LM to PC */
#define DMA_ENABLE	  0x01

#define GUS_MODE	  0x00	/* SGMI[0]=0 */
#define ENH_MODE	  0x01	/* SGMI[0]=1 */
#define ENABLE_LFOS	  0x02	/* SGMI[1] */
#define NO_WAVETABLE	  0x04	/* SGMI[2] */
#define RAM_TEST	  0x08	/* SGMI[3] */

#define DMA_SET_MASK	  0x04

#define VOICE_STOP	  0x02		 /* SACI[1] */
#define VOICE_STOPPED	  0x01		 /* SACI[0] */

#define LDSALI		  0x42
#define LDSAHI		  0x50
#define LMALI		  0x43
#define LMAHI		  0x44
#define LMCFI		  0x52
#define LMCI		  0x53
#define LMFSI		  0x56
#define LDIBI		  0x58
#define LDICI		  0x57
#define LMSBAI		  0x51
#define LMRFAI		  0x54
#define LMPFAI		  0x55
#define SVCI_RD		  0x8D
#define SVCI_WR		  0x0D
#define SACI_RD		  0x80
#define SACI_WR		  0x00
#define SALI_RD		  0x8B
#define SALI_WR		  0x0B
#define SAHI_RD		  0x8A
#define SAHI_WR		  0x0A
#define SASHI_RD	  0x82
#define SASHI_WR	  0x02
#define SASLI_RD	  0x83
#define SASLI_WR	  0x03
#define SAEHI_RD	  0x84
#define SAEHI_WR	  0x04
#define SAELI_RD	  0x85
#define SAELI_WR	  0x05
#define SVRI_RD		  0x86
#define SVRI_WR		  0x06
#define SVSI_RD		  0x87
#define SVSI_WR		  0x07
#define SVEI_RD		  0x88
#define SVEI_WR		  0x08
#define SVLI_RD		  0x89
#define SVLI_WR		  0x09
#define SROI_RD		  0x8C
#define SROI_WR		  0x0C
#define SLOI_RD		  0x93
#define SLOI_WR		  0x13
#define SMSI_RD		  0x95
#define SMSI_WR		  0x15
#define SGMI_RD		  0x99
#define SGMI_WR		  0x19
#define SFCI_RD		  0x81
#define SFCI_WR		  0x01
#define SUAI_RD		  0x90
#define SUAI_WR		  0x10
#define SVII		  0x8F
#define CMODEI		  0x0C	      /* index for CMODEI */
#define CMONOI		  0x1A
#define CFIG3I		  0x11
#define CFIG2I		  0x10
#define CLTIMI		  0x14
#define CUTIMI		  0x15
#define CSR3I		  0x18	      /* Index to CSR3I (Interrupt Status) */
#define CEXTI		  0x0A	      /* Index to External Control Register */
#define CFIG1I		  0x09	      /* Index to Codec Conf Reg 1 */
#define CSR2I		  0x0B	      /* Index to Codec Stat Reg 2 */
#define CPDFI		  0x08	      /* Index to Play Data Format Reg */
#define CRDFI		  0x1C	      /* Index to Rec Data Format Reg */
#define CLMICI		  0x16	      /* Index to Left Mic Input Ctrl Register */
#define CRMICI		  0x17	      /* Index to Right Mic Input Ctrl Register */
#define CLCI		  0x0D	      /* Index to Loopback Ctrl Register */
#define IVERI		  0x5B	      /* Index to register IVERI */
#define IDECI		  0x5A
#define ICMPTI		  0x59
#define CODEC_MODE1	  0x00
#define CODEC_MODE2	  0x40
#define CODEC_MODE3	  0x6C	      /* Enhanced Mode */
#define CODEC_STATUS1	  0x01
#define CODEC_STATUS2	  0x0B	      /* Index to CSR2I */
#define CODEC_STATUS3	  0x18	      /* Index to CSR3I */
#define PLAYBACK	  0x01	      /* Enable playback path CFIG1I[0]=1*/
#define RECORD		  0x02	      /* Enable Record path CFIG1I[1]=1*/
#define TIMER_ENABLE	  0x40	      /* CFIG2I[6] */
#define CODEC_MCE	  0x40	      /* CIDXR[6] */
#define CALIB_IN_PROGRESS 0x20	      /* CSR2I[5] */
#define CODEC_INIT	  0x80	      /* CIDXR[7] */
#define BIT16_BIG	  0xC0	      /* 16-bit signed, big endian */
#define IMA_ADPCM	  0xA0	      /* IMA-compliant ADPCM */
#define BIT8_ALAW	  0x60	      /* 8-bit A-law */
#define BIT16_LITTLE	  0x40	      /* 16-bit signed, little endian */
#define BIT8_ULAW	  0x20	      /* 8-bit mu-law */
#define BIT8_LINEAR	  0x00	      /* 8-bit unsigned */
#define REC_DFORMAT	  0x1C
#define PLAY_DFORMAT	  0x08
#define DMA_ACCESS	  0x00
#define PIO_ACCESS	  0xC0
#define DMA_SIMPLEX	  0x04
#define STEREO		  0x10	      /* CxDFI[4] */
#define AUTOCALIB	  0x08	      /* CFIG1I[3] */
#define ROM_IO		  0x02	      /* ROM I/O cycles - LMCI[1]=1 */
#define DRAM_IO		  0x4D	      /* DRAM I/O cycles - LMCI[1]=0 */
#define AUTOI		  0x01	      /* LMCI[0]=1 */
#define PLDNI		  0x07
#define ACTIVATE_DEV	  0x30
#define PWAKEI		  0x03	      /* Index for PWAKEI */
#define PISOCI		  0x01	      /* Index for PISOCI */
#define PSECI		  0xF1	      /* Index for PSECI */
#define RANGE_IOCHK	  0x31	      /* PURCI or PRRCI Index */
#define MIDI_RESET	  0x03

#define IW_DMA_RECORD	  0x02
#define IW_DMA_PLAYBACK	  0x01

#define IW_MCE		  0x40

#define IN		  0
#define OUT		  1

/* codec indirect register access */

#define IW_WRITE_CODEC_1(reg, val) \
do {\
	bus_space_write_1(sc->sc_iot, sc->codec_index_h, 0, (u_char)(reg));\
	bus_space_write_1(sc->sc_iot, sc->codec_index_h, sc->cdatap, (u_char)val);\
	bus_space_write_1(sc->sc_iot, sc->codec_index_h, 0, 0);\
} while (0)\

#define IW_READ_CODEC_1(reg, ret) \
do {\
	bus_space_write_1(sc->sc_iot, sc->codec_index_h, sc->codec_index, (u_char)(reg));\
	ret = bus_space_read_1(sc->sc_iot, sc->codec_index_h, sc->cdatap);\
	bus_space_write_1(sc->sc_iot, sc->codec_index_h, 0, 0);\
} while (0)\

/* iw direct register access */

#define IW_WRITE_DIRECT_1(reg, h, val) \
do {\
	bus_space_write_1(sc->sc_iot, h, reg, (u_char)val);\
} while (0)\

#define IW_READ_DIRECT_1(reg, h, ret) \
do {\
	ret = bus_space_read_1(sc->sc_iot, h, (u_char)reg);\
} while (0)\

/* general indexed regs access */

#define IW_WRITE_GENERAL_1(reg, val) \
do {\
	bus_space_write_1(sc->sc_iot, sc->p3xr_h, 3, (u_char)reg);\
	bus_space_write_1(sc->sc_iot, sc->p3xr_h, 5, (u_char)val);\
} while (0)\

#define IW_WRITE_GENERAL_2(reg, val) \
do {\
	bus_space_write_1(sc->sc_iot, sc->p3xr_h, 3, (u_char)reg);\
	bus_space_write_2(sc->sc_iot, sc->p3xr_h, 4, (u_short)val);\
} while (0)\

#define IW_READ_GENERAL_1(reg, ret) \
do{\
	bus_space_write_1(sc->sc_iot, sc->p3xr_h, 3, (u_char)reg);\
	ret = bus_space_read_1(sc->sc_iot, sc->p3xr_h, 5);\
} while (0)\

#define IW_READ_GENERAL_2(reg, ret) \
do{\
	bus_space_write_1(sc->sc_iot, sc->p3xr_h, 3, (u_char)reg);\
	ret = bus_space_read_2(sc->sc_iot, sc->p3xr_h, 4);\
} while (0)\


#endif /* INTERWAVEREG_H */
