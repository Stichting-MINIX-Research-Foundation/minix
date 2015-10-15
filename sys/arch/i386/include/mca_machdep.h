/*	$NetBSD: mca_machdep.h,v 1.15 2011/07/01 18:15:11 dyoung Exp $	*/

/*
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
 * Copyright (c) 1999 Scott D. Telford.  All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _I386_MCA_MACHDEP_H_
#define _I386_MCA_MACHDEP_H_

#include <sys/device.h>	/* for device_t */
#include <sys/bus.h>

/*
 * i386-specific definitions for MCA autoconfiguration.
 */

extern struct x86_bus_dma_tag mca_bus_dma_tag;

/* set to 1 if MCA bus is detected */
extern int MCA_system;

void	mca_nmi(void);

/*
 * Types provided to machine-independent MCA code.
 */
struct x86_mca_chipset {
        void */*struct mca_dma_state*/ ic_dmastate;
};

typedef struct x86_mca_chipset *mca_chipset_tag_t;
typedef int mca_intr_handle_t;

/*
 * Functions provided to machine-independent MCA code.
 */
struct mcabus_attach_args;

void	mca_attach_hook(device_t, device_t,
		struct mcabus_attach_args *);
int	mca_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_dmamap_t *, int);
void	mca_dma_set_ioport(int dma, uint16_t port);
const struct evcnt *mca_intr_evcnt(mca_chipset_tag_t, mca_intr_handle_t);
void	*mca_intr_establish(mca_chipset_tag_t, mca_intr_handle_t,
		int, int (*)(void *), void *);
void	mca_intr_disestablish(mca_chipset_tag_t, void *);
int	mca_conf_read(mca_chipset_tag_t, int, int);
void	mca_conf_write(mca_chipset_tag_t, int, int, int);
void	mca_busprobe(void);

/*
 * Flags for DMA. Avoid BUS_DMA_BUS1, we share dmamap routines with ISA and
 * that flag is used for different purpose within _isa_dmamap_*().
 */
#define MCABUS_DMA_IOPORT		BUS_DMA_BUS2	/* io-port based DMA */
#define	MCABUS_DMA_16BIT		BUS_DMA_BUS3	/* 16bit DMA */
#define	_MCABUS_DMA_USEDMACTRL		BUS_DMA_BUS4	/* internal flag */

/*
 * These two are used to light disk busy LED on PS/2 during disk operations.
 */
void	mca_disk_busy(void);
void	mca_disk_unbusy(void);

/* MCA register addresses for IBM PS/2 */

#define PS2_SYS_CTL_A		0x92	/* PS/2 System Control Port A */
#define MCA_MB_SETUP_REG	0x94	/* Motherboard setup register */
#define MCA_ADAP_SETUP_REG	0x96	/* Adapter setup register */
#define MCA_POS_REG_BASE	0x100	/* POS registers base address */
#define MCA_POS_REG_SIZE	8	/* POS registers window size */

#define MCA_POS_REG(n)		(0x100+(n))	/* POS registers 0-7 */

/* Adapter setup register bits */

#define MCA_ADAP_SET		0x08	/* Adapter setup mode */
#define MCA_ADAP_CHR		0x80	/* Adapter channel reset */

#define MCA_MAX_SLOTS		8	/* max number of slots per bus */

#endif /* _I386_MCA_MACHDEP_H_ */
