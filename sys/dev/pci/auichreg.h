/*	$NetBSD: auichreg.h,v 1.12 2009/09/03 14:29:42 sborrill Exp $	*/

/*
 * Copyright (c) 2000 Michael Shalayeff
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from OpenBSD: ich.c,v 1.3 2000/08/11 06:17:18 mickey Exp
 */

#ifndef _DEV_PCI_AUICHREG_H_
#define	_DEV_PCI_AUICHREG_H_

/*
 * AC'97 audio found on Intel 810/820/440MX chipsets.
 *	http://developer.intel.com/design/chipsets/datashts/290655.htm
 *	http://developer.intel.com/design/chipsets/manuals/298028.htm
 */

/* 12.1.10 NAMBAR - native audio mixer base address register */
#define	ICH_NAMBAR	0x10
/* 12.1.11 NABMBAR - native audio bus mastering base address register */
#define	ICH_NABMBAR	0x14
#define ICH_MMBAR	0x18	/* ICH4/ICH5/ICH6/ICH7 native audio mixer BAR */
#define ICH_MBBAR	0x1c	/* ICH4/ICH5/ICH6/ICH7 native bus mastering BAR */
#define ICH_CFG		0x40
#define		ICH_CFG_IOSE	0x0100

/* table 12-3. native audio bus master control registers */
#define	ICH_BDBAR	0x00	/* 8-byte aligned address */
#define	ICH_CIV		0x04	/* 5 bits current index value */
#define	ICH_LVI		0x05	/* 5 bits last valid index value */
#define		ICH_LVI_MASK	0x1f
#define	ICH_STS		0x06	/* 16 bits status */
#define		ICH_FIFOE	0x10	/* fifo error */
#define		ICH_BCIS	0x08	/* r- buf cmplt int sts; wr ack */
#define		ICH_LVBCI	0x04	/* r- last valid bci, wr ack */
#define		ICH_CELV	0x02	/* current equals last valid */
#define		ICH_DCH		0x01	/* DMA halted */
#define		ICH_ISTS_BITS	"\020\01dch\02celv\03lvbci\04bcis\05fifoe"
#define	ICH_PICB	0x08	/* 16 bits */
#define	ICH_PIV		0x0a	/* 5 bits prefetched index value */
#define	ICH_CTRL	0x0b	/* control */
#define		ICH_IOCE	0x10	/* int on completion enable */
#define		ICH_FEIE	0x08	/* fifo error int enable */
#define		ICH_LVBIE	0x04	/* last valid buf int enable */
#define		ICH_RR		0x02	/* 1 - reset regs */
#define		ICH_RPBM	0x01	/* 1 - run, 0 - pause */

#define	ICH_CODEC_OFFSET	0x80

#define	ICH_PCMI	0x00
#define	ICH_PCMO	0x10
#define	ICH_MICI	0x20

#define	ICH_GCTRL	0x2c
#define		ICH_SSM_78	0x40000000 /* S/PDIF slots 7 and 8 */
#define		ICH_SSM_69	0x80000000 /* S/PDIF slots 6 and 9 */
#define		ICH_SSM_1011	0xc0000000 /* S/PDIF slots 10 and 11 */
#define		ICH_POM16	0x000000 /* PCM out precision 16bit */
#define		ICH_POM20	0x400000 /* PCM out precision 20bit */
#define		ICH_PCM246_MASK	0x300000
#define		 ICH_PCM2	0x000000 /* 2ch output */
#define		 ICH_PCM4	0x100000 /* 4ch output */
#define		 ICH_PCM6	0x200000 /* 6ch output */
#define		ICH_SIS_PCM246_MASK	0x0000c0
#define		 ICH_SIS_PCM2	0x000000 /* 2ch output */
#define		 ICH_SIS_PCM4	0x000040 /* 4ch output */
#define		 ICH_SIS_PCM6	0x000080 /* 6ch output */
#define		ICH_S2RIE	0x40	/* int when tertiary codec resume */
#define		ICH_SRIE	0x20	/* int when 2ndary codec resume */
#define		ICH_PRIE	0x10	/* int when primary codec resume */
#define		ICH_ACLSO	0x08	/* aclink shut off */
#define		ICH_WRESET	0x04	/* warm reset */
#define		ICH_CRESET	0x02	/* cold reset */
#define		ICH_GIE		0x01	/* gpi int enable */
#define	ICH_GSTS	0x30
#define		ICH_S2RI	0x20000000 /* tertiary resume int */
#define		ICH_S2CR	0x10000000 /* tertiary codec ready */
#define		ICH_BCS		0x08000000 /* bit clock stopped */
#define		ICH_SPINT	0x04000000 /* S/PDIF int */
#define		ICH_P2INT	0x02000000 /* PCM-In 2 int */
#define		ICH_M2INT	0x01000000 /* mic 2 int */
#define		ICH_SAMPLE_CAP	0x00c00000 /* sampling precision capability */
#define		ICH_CHAN_CAP	0x00300000 /* multi-channel capability */
#define		ICH_MD3		0x20000	/* pwr-dn semaphore for modem */
#define		ICH_AD3		0x10000	/* pwr-dn semaphore for audio */
#define		ICH_RCS		0x08000	/* read completion status */
#define		ICH_B3S12	0x04000	/* bit 3 of slot 12 */
#define		ICH_B2S12	0x02000	/* bit 2 of slot 12 */
#define		ICH_B1S12	0x01000	/* bit 1 of slot 12 */
#define		ICH_SRI		0x00800	/* secondary resume int */
#define		ICH_PRI		0x00400	/* primary resume int */
#define		ICH_SCR		0x00200	/* secondary codec ready */
#define		ICH_PCR		0x00100	/* primary codec ready */
#define		ICH_MINT	0x00080	/* mic in int */
#define		ICH_POINT	0x00040	/* pcm out int */
#define		ICH_PIINT	0x00020	/* pcm in int */
#define		ICH_MOINT	0x00004	/* modem out int */
#define		ICH_MIINT	0x00002	/* modem in int */
#define		ICH_GSCI	0x00001	/* gpi status change */
#define		ICH_GSTS_BITS	"\020\01gsci\02miict\03moint\06piint\07point\010mint\011pcr\012scr\013pri\014sri\015b1s12\016b2s12\017b3s12\020rcs\021ad3\022md3"
#define	ICH_CAS		0x34	/* 1/8 bit */
#define	ICH_SEMATIMO	1000	/* us */

#define	ICH_SIS_NV_CTL	0x4c	/* some SiS/nVidia register.  From Linux */
#define		ICH_SIS_CTL_UNMUTE	0x01	/* un-mute the output */

/*
 * according to the dev/audiovar.h AU_RING_SIZE is 2^16, what fits
 * in our limits perfectly, i.e. setting it to higher value
 * in your kernel config would improve perfomance, still 2^21 is the max
 */
#define	ICH_DMALIST_MAX	32
#define	ICH_DMASEG_MAX	(65536*2)	/* 64k samples, 2x16 bit samples */
struct auich_dmalist {
	u_int32_t	base;
	u_int32_t	len;
#define	ICH_DMAF_IOC	0x80000000	/* 1-int on complete */
#define	ICH_DMAF_BUP	0x40000000	/* 0-retrans last, 1-transmit 0 */
};

#endif /* _DEV_PCI_AUICHREG_H_ */
