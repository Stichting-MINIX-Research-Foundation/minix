/* $NetBSD: atppcvar.h,v 1.12 2011/05/26 02:29:23 jakllsch Exp $ */

/*-
 * Copyright (c) 2001 Alcove - Nicolas Souchu
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
 * FreeBSD: src/sys/isa/ppcreg.h,v 1.10.2.4 2001/10/02 05:21:45 nsouch Exp
 *
 */

#ifndef __ATPPCVAR_H
#define __ATPPCVAR_H

#include <sys/bus.h>
#include <machine/types.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

#include <dev/ppbus/ppbus_conf.h>


/* Maximum time to wait for device response */
#define MAXBUSYWAIT	(5 * (hz))

/* Poll interval when waiting for device to become ready */
#define ATPPC_POLL	((hz)/10)


/* Diagnostic and verbose printing macros */

#ifdef ATPPC_DEBUG
extern int atppc_debug;
#define ATPPC_DPRINTF(arg) if(atppc_debug) printf arg
#else
#define ATPPC_DPRINTF(arg)
#endif

#ifdef ATPPC_VERBOSE
extern int atppc_verbose;
#define ATPPC_VPRINTF(arg) if(atppc_verbose) printf arg
#else
#define ATPPC_VPRINTF(arg)
#endif


/* Flag used in DMA transfer */
#define ATPPC_DMA_MODE_READ 0x0
#define ATPPC_DMA_MODE_WRITE 0x1


/* Flags passed via config */
#define ATPPC_FLAG_DISABLE_INTR	0x01
#define ATPPC_FLAG_DISABLE_DMA	0x02

/* Single softintr callback entry */
struct atppc_handler_node {
	void (*func)(void *);
	void *arg;
	SLIST_ENTRY(atppc_handler_node) entries;
};

/* Generic structure to hold parallel port chipset info. */
struct atppc_softc {
	/* Generic device attributes */
	device_t sc_dev;

	kmutex_t sc_lock;
	kcondvar_t sc_out_cv;
	kcondvar_t sc_in_cv;

	/* Machine independent bus infrastructure */
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmapt;
	bus_size_t sc_dma_maxsize;

	/* Child device */
	device_t child;

        /* Opaque handle used for interrupt handler establishment */
	void *sc_ieh;

	/* List of soft interrupts to call */
	SLIST_HEAD(handler_list, atppc_handler_node) sc_handler_listhead;

	 /* Input buffer: working pointers, and size in bytes. */
	char * sc_inb;
	char * sc_inbstart;
	u_int32_t sc_inb_nbytes;
	int sc_inerr;

	/* Output buffer pointer, working pointer, and size in bytes. */
	char * sc_outb;
	char * sc_outbstart;
	u_int32_t sc_outb_nbytes;
	int sc_outerr;

	/* DMA functions: setup by bus specific attach code */
	int (*sc_dma_start)(struct atppc_softc *, void *, u_int, u_int8_t);
	int (*sc_dma_finish)(struct atppc_softc *);
	int (*sc_dma_abort)(struct atppc_softc *);
	int (*sc_dma_malloc)(device_t, void **, bus_addr_t *,
		bus_size_t);
	void (*sc_dma_free)(device_t, void **, bus_addr_t *,
		bus_size_t);

	/* Microsequence related members */
	char * sc_ptr;		/* microseq current pointer */
	int sc_accum;		/* microseq accumulator */

	/* Device attachment state */
#define ATPPC_ATTACHED 1
#define ATPPC_NOATTACH 0
	u_int8_t sc_dev_ok;

	/*
	 * Hardware capabilities flags: standard mode and nibble mode are
	 * assumed to always be available since if they aren't you don't
	 * HAVE a parallel port.
	 */
#define ATPPC_HAS_INTR	0x01	/* Interrupt available */
#define ATPPC_HAS_DMA	0x02	/* DMA available */
#define ATPPC_HAS_FIFO	0x04	/* FIFO available */
#define ATPPC_HAS_PS2	0x08	/* PS2 mode capable */
#define ATPPC_HAS_ECP	0x10	/* ECP mode available */
#define ATPPC_HAS_EPP	0x20	/* EPP mode available */
	u_int8_t sc_has;	/* Chipset detected capabilities */

	/* Flags specifying mode of chipset operation . */
#define ATPPC_MODE_STD	0x01	/* Use centronics-compatible mode */
#define ATPPC_MODE_PS2	0x02	/* Use PS2 mode */
#define ATPPC_MODE_EPP	0x04	/* Use EPP mode */
#define ATPPC_MODE_ECP	0x08	/* Use ECP mode */
#define ATPPC_MODE_NIBBLE 0x10	/* Use nibble mode */
#define ATPPC_MODE_FAST	0x20	/* Use Fast Centronics mode */
	u_int8_t sc_mode;	/* Current operational mode */

	/* Flags which further define chipset operation */
#define ATPPC_USE_INTR	0x01	/* Use interrupts */
#define ATPPC_USE_DMA	0x02	/* Use DMA */
	u_int8_t sc_use;	/* Capabilities to use */

	/* Parallel Port Chipset model. */
#define SMC_LIKE        0
#define SMC_37C665GT    1
#define SMC_37C666GT    2
#define NS_PC87332      3
#define NS_PC87306      4
#define INTEL_820191AA  5       /* XXX not implemented */
#define GENERIC         6
#define WINB_W83877F    7
#define WINB_W83877AF   8
#define WINB_UNKNOWN    9
#define NS_PC87334      10
#define SMC_37C935      11
#define NS_PC87303      12
	u_int8_t sc_model;	/* chipset model */

	/* EPP mode */
#define ATPPC_EPP_1_9	0x0
#define ATPPC_EPP_1_7	0x1
	u_int8_t sc_epp;

	/* Parallel Port Chipset Type. SMC versus GENERIC (others) */
#define ATPPC_TYPE_SMCLIKE 0
#define ATPPC_TYPE_GENERIC 1
	u_int8_t sc_type;	/* generic or smclike chipset type */

	/* Stored register values after an interrupt occurs */
	u_int8_t sc_ecr_intr;
	u_int8_t sc_ctr_intr;
	u_int8_t sc_str_intr;

#define ATPPC_IRQ_NONE	0x0
#define ATPPC_IRQ_nACK	0x1
#define ATPPC_IRQ_DMA	0x2
#define ATPPC_IRQ_FIFO	0x4
#define ATPPC_IRQ_nFAULT	0x8
	u_int8_t sc_irqstat;	/* Record irq settings */

#define ATPPC_DMA_INIT		0x01
#define ATPPC_DMA_STARTED	0x02
#define ATPPC_DMA_COMPLETE	0x03
#define ATPPC_DMA_INTERRUPTED	0x04
#define ATPPC_DMA_ERROR		0x05
	u_int8_t sc_dmastat;	/* Record dma state */

#define ATPPC_PWORD_MASK	0x30
#define ATPPC_PWORD_16	0x00
#define ATPPC_PWORD_8	0x10
#define ATPPC_PWORD_32	0x20
	u_int8_t sc_pword;	/* PWord size: used for FIFO DMA transfers */
	u_int8_t sc_fifo;	/* FIFO size */

	/* Indicates number of PWords in FIFO queues that generate interrupt */
	u_int8_t sc_wthr;	/* writeIntrThresold */
	u_int8_t sc_rthr;	/* readIntrThresold */
};



#ifdef _KERNEL

/* Function prototypes */

/* Soft config attach/detach routines */
void atppc_sc_attach(struct atppc_softc *);
int atppc_sc_detach(struct atppc_softc *, int);

/* Detection routines */
int atppc_detect_port(bus_space_tag_t, bus_space_handle_t);

/* Interrupt handler for atppc device */
int atppcintr(void *);

#endif /* _KERNEL */

#endif /* __ATPPCVAR_H */
