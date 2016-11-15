/*	$NetBSD: pciide_geode_reg.h,v 1.6 2009/10/19 18:41:15 bouyer Exp $	*/

/*
 * Copyright (c) 2004 Manuel Bouyer.
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

#define CS5530_PIO_REG(chan, drv)	(0x20 + (chan) * 0x10 + (drv) * 0x8)
#define CS5530_DMA_REG(chan, drv)	(0x24 + (chan) * 0x10 + (drv) * 0x8)
#define CS5530_DMA_REG_PIO_FORMAT	0x80000000 /* select PIO format 1 */
#define CS5530_DMA_REG_UDMA		0x00100000 /* enable Ultra-DMA */

/*
 * Recommended values from the cs5530 data sheet.
 * Note that the udma values include CS5530_DMA_REG_UDMA.
 * However, from the datasheet bits 30-21 should be reserved, yet
 * geode_udma sets bit 23 to 1. I don't know if it's the definition of
 * DMA_REG_UDMA which is wrong (bit 23 instead of 20) or these values.
 */
static const int32_t geode_cs5530_pio[] __unused =
    {0x9172d132, 0x21717121, 0x00803020, 0x20102010, 0x00100010};
static const int32_t geode_cs5530_dma[] __unused =
    {0x00077771, 0x00012121, 0x00002020};
static const int32_t geode_cs5530_udma[] __unused =
    {0x00921250, 0x00911140, 0x00911030};

#define SC1100_PIO_REG(chan, drv)	(0x40 + (chan) * 0x10 + (drv) * 0x8)
#define SC1100_DMA_REG(chan, drv)	(0x44 + (chan) * 0x10 + (drv) * 0x8)

/* Timings from FreeBSD */
static const int32_t geode_sc1100_pio[] __unused =
    {0x9172d132, 0x21717121, 0x00803020, 0x20102010, 0x00100010, 0x00803020,
     0x20102010, 0x00100010, 0x00100010, 0x00100010, 0x00100010};
static const int32_t geode_sc1100_dma[] __unused =
    {0x80077771, 0x80012121, 0x80002020};
static const int32_t geode_sc1100_udma[] __unused =
    {0x80921250, 0x80911140, 0x80911030};
