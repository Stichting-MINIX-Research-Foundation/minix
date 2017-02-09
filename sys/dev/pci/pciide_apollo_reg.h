/*	$NetBSD: pciide_apollo_reg.h,v 1.20 2011/07/10 20:01:37 jakllsch Exp $	*/

/*
 * Copyright (c) 1998 Manuel Bouyer.
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
 *
 */

/*
 * Copyright (c) 2000 David Sainty.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Registers definitions for VIA technologies's Apollo controllers (VT82V580VO,
 * VT82C586A and VT82C586B). Available from http://www.via.com.tw/ or
 * http://www.viatech.com/
 */

/*
 * AMD 7x6 PCI IDE controller is a clone of the VIA apollo.
 *      http://www.amd.com/products/cpg/athlon/techdocs/pdf/22548.pdf (756)
 *      http://www.amd.com/products/cpg/athlon/techdocs/pdf/23167.pdf (766)
 */

/*
 * The nVidia nForce and nForce2 IDE controllers are compatible with
 * the AMD controllers, but their registers are offset 0x10 bytes.
 */

/* Chip revisions */
#define AMD756_CHIPREV_D2 3

/*
 * The AMD756 chip revision D2 has a bug affecting DMA (but not UDMA)
 * modes.  The workaround documented by AMD is to not use DMA on any
 * drive which does not support UDMA modes.
 *
 * See: http://www.amd.com/products/cpg/athlon/techdocs/pdf/22591.pdf
 */
#define AMD756_CHIPREV_DISABLEDMA(rev) ((rev) <= AMD756_CHIPREV_D2)

/* registers offset - vendor dependent */
#define APO_VIA_REGBASE			0x40
#define APO_AMD_REGBASE			0x40
#define APO_NVIDIA_REGBASE		0x50
#define APO_VIA_VT6421_REGBASE		0xa0

/* misc. configuration registers */
#define APO_IDECONF(sc) ((sc)->sc_apo_regbase + 0x00)
#define APO_IDECONF_EN(channel) (0x00000001 << (1 - (channel)))
#define APO_IDECONF_SERR_EN	0x00000100 /* VIA 580 only */
#define APO_IDECONF_DS_SOURCE	0x00000200 /* VIA 580 only */
#define APO_IDECONF_ALT_INTR_EN	0x00000400 /* VIA 580 only */
#define APO_IDECONF_PERR_EN	0x00000800 /* VIA 580 only */
#define APO_IDECONF_WR_BUFF_EN(channel) (0x00001000 << ((1 - (channel)) << 1))
#define APO_IDECONF_RD_PREF_EN(channel) (0x00002000 << ((1 - (channel)) << 1))
#define APO_IDECONF_DEVSEL_TME	0x00010000 /* VIA 580 only */
#define APO_IDECONF_MAS_CMD_MON	0x00020000 /* VIA 580 only */
#define APO_IDECONF_IO_NAT(channel) \
	(0x00400000 << (1 - (channel))) /* VIA 580 only */
#define APO_IDECONF_FIFO_TRSH(channel, x) \
	((x) & 0x3) << ((1 - (channel)) << 1 + 24)
#define APO_IDECONF_FIFO_CONF_MASK 0x60000000

/* Misc. controls register - VIA only */
#define APO_CTLMISC(sc) ((sc)->sc_apo_regbase + 0x04)
#define APO_CTLMISC_BM_STS_RTY	0x00000008
#define APO_CTLMISC_FIFO_HWS	0x00000010
#define APO_CTLMISC_WR_IRDY_WS	0x00000020
#define APO_CTLMISC_RD_IRDY_WS	0x00000040
#define APO_CTLMISC_INTR_SWP	0x00004000
#define APO_CTLMISC_DRDY_TIME_MASK 0x00030000
#define APO_CTLMISC_FIFO_FLSH_RD(channel) (0x00100000 << (1 - (channel)))
#define APO_CTLMISC_FIFO_FLSH_DMA(channel) (0x00400000 << (1 - (channel)))

/* data port timings controls */
#define APO_DATATIM(sc) ((sc)->sc_apo_regbase + 0x08)
#define APO_DATATIM_MASK(channel) (0xffff << ((1 - (channel)) << 4))
#define APO_DATATIM_RECOV(channel, drive, x) (((x) & 0xf) << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3)))
#define APO_DATATIM_PULSE(channel, drive, x) (((x) & 0xf) << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3) + 4))

/* misc timings control - VIA only */
#define APO_MISCTIM(sc) ((sc)->sc_apo_regbase + 0x0c)

/* Ultra-DMA control (586A/B only, amd and nvidia ) */
#define APO_UDMA(sc) ((sc)->sc_apo_regbase + 0x10)
#define APO_UDMA_MASK(channel) (0xffff << ((1 - (channel)) << 4))
#define APO_UDMA_TIME(channel, drive, x) (((x) & 0xf) << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3)))
#define APO_UDMA_PIO_MODE(channel, drive) (0x20 << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3))) /* via only */
#define APO_UDMA_EN(channel, drive) (0x40 << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3)))
#define APO_UDMA_EN_MTH(channel, drive) (0x80 << \
	(((1 - (channel)) << 4) + ((1 - (drive)) << 3)))
#define APO_UDMA_CLK66(channel) (0x08 << ((1 - (channel)) << 4)) /* via only */

/* for via */
static const int8_t via_udma133_tim[] __unused =
    {0x07, 0x07, 0x06, 0x04, 0x02, 0x01, 0x00};
static const int8_t via_udma100_tim[] __unused =
    {0x07, 0x07, 0x04, 0x02, 0x01, 0x00};
static const int8_t via_udma66_tim[] __unused =
    {0x03, 0x03, 0x02, 0x01, 0x00};
static const int8_t via_udma33_tim[] __unused =
    {0x03, 0x02, 0x00};

/* for amd and nvidia */
static const int8_t amd7x6_udma_tim[] __unused =
    {0x02, 0x01, 0x00, 0x04, 0x05, 0x06, 0x07};

/* for all */
static const int8_t apollo_pio_set[] __unused =
    {0x0a, 0x0a, 0x0a, 0x02, 0x02};
static const int8_t apollo_pio_rec[] __unused =
    {0x08, 0x08, 0x08, 0x02, 0x00};
