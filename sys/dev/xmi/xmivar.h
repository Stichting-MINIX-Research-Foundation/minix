/*	$NetBSD: xmivar.h,v 1.3 2009/05/12 14:48:08 cegger Exp $	*/

/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * per-XMI-adapter state.
 */
struct xmi_softc {
	device_t sc_dev;
	bus_space_tag_t sc_iot;		/* Space tag for the XMI bus */
	bus_dma_tag_t sc_dmat;
	bus_addr_t sc_addr;		/* Address base address for this bus */
	int sc_busnr;			/* (Physical) number of this bus */
	int sc_lastiv;			/* last available interrupt vector */
	int sc_intcpu;
};

/*
 * Struct used for autoconfiguration; attaching of XMI nodes.
 */
struct xmi_attach_args {
	bus_space_tag_t xa_iot;
	bus_space_handle_t xa_ioh;	/* Base address for this node */
	bus_dma_tag_t xa_dmat;
	int xa_busnr;
	int xa_nodenr;
	int xa_intcpu;	/* Mask of which cpus to interrupt */
	int xa_ivec;	/* Interrupt vector to use */
	void *xa_icookie;
};

/*
 * XMI node list.
 */
struct xmi_list {
	uint16_t xl_nr;		/* Unit ID# */
	uint16_t xl_havedriver;	/* Have device driver (informal) */
	const char *xl_name;	/* DEC name */
};

/* Prototype */
void xmi_attach(struct xmi_softc *);
void xmi_intr_establish(void *, int, void (*)(void *), void *, struct evcnt *);
