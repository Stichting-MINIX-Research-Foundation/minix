/*	$NetBSD: pciide_piccolo.h_reg.h,v 1.0 2008/04/28 00:00:00 djb  	*/

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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
 * Register definitions for the Toshiba PICCOLO, by SWAG!
 */

#define PICCOLO_PIO_TIMING 0x50
#define PICCOLO_DMA_TIMING 0x5c

#define PICCOLO_PIO_MASK 0xffffe088
#define PICCOLO_DMA_MASK 0xffffe088
#define PICCOLO_UDMA_MASK 0x78ffe088

/* TIMING SWAG!!! */

/* 
 * first digit is command active, next two are front porch and back porch 
 * command active >= minimum for mode 
 * front porch + back porch + command active >= cycle time for mode
 * values below may need adjustment 
 */
static const u_int32_t piccolo_pio_times[]
    __unused = {
/*        programmed               actual       */
	0x0566,             
	0x0433,
	0x0311,
	0x0201,
	0x0200,		/* PIO 4, 150ns cycle (120ns is spec), 90ns command active (70ns is spec), 30ns setup and hold */
	0x0100
	   
};

static const u_int32_t piccolo_sw_dma_times[]
    __unused = {
/*        programmed               actual       */
	0x0f77		
};

static const u_int32_t piccolo_mw_dma_times[]
     __unused = {
/*        programmed               actual       */
	0x0655,
	0x0200,
	0x0200,
	0x0100 
};

/* XXX Is MSB UDMA enable? Can't set it. Seems to work without being set. */
static const u_int32_t piccolo_udma_times[]
    __unused = {
/*        programmed               actual       */
	0x84000222,
	0x83000111,
	0x82000000	/* UDMA 2, 120ns cycle (117ns is spec), 60ns command active (55ns is spec), 30ns setup and hold */  
};
