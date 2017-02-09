/*	$NetBSD: pucvar.h,v 1.11 2014/03/18 18:20:42 riastradh Exp $	*/

/*
 * Copyright (c) 1998, 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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
 * Exported (or conveniently located) PCI "universal" communications card
 * software structures.
 *
 * Author: Christopher G. Demetriou, May 14, 1998.
 */

#define	PUC_MAX_PORTS		32

struct puc_device_description {
	const char		*name;
	pcireg_t		rval[4];
	pcireg_t		rmask[4];
	struct {
		int	type;
		int	bar;
		int	offset;
		int	flags;
	}			ports[PUC_MAX_PORTS];
};

#define	PUC_REG_VEND		0
#define	PUC_REG_PROD		1
#define	PUC_REG_SVEND		2
#define	PUC_REG_SPROD		3

#define	PUC_PORT_TYPE_NONE	0
#define	PUC_PORT_TYPE_COM	1
#define	PUC_PORT_TYPE_LPT	2

#define	PUC_PORT_VALID(desc, port) \
  ((port) < PUC_MAX_PORTS && (desc)->ports[(port)].type != PUC_PORT_TYPE_NONE)
#define PUC_PORT_BAR_INDEX(bar)	(((bar) - PCI_MAPREG_START) / 4)

/* Flags for PUC_PORT_TYPE_COM */
/* * assume all clock rates have 8 lower bits to 0 - this leaves us 8 flags */
#define PUC_COM_CLOCKMASK 0xffffff00

#define PUC_COM_FLAG0	(1 << 0)
#define PUC_COM_FLAG1	(1 << 1)
#define PUC_COM_FLAG2	(1 << 2)
#define PUC_COM_FLAG3	(1 << 3)
#define PUC_COM_FLAG4	(1 << 4)
#define PUC_COM_FLAG5	(1 << 5)
#define PUC_COM_FLAG6	(1 << 6)
#define PUC_COM_FLAG7	(1 << 7)

/* Flags for SIIG Cyberserial options */
#define PUC_COM_SIIG10x	PUC_COM_FLAG7
#define PUC_COM_SIIG20x	PUC_COM_FLAG6
#define PUC_PORT_USR0	PUC_COM_FLAG0
#define PUC_PORT_USR1	PUC_COM_FLAG1
#define PUC_PORT_USR2	PUC_COM_FLAG2
#define PUC_PORT_USR3	PUC_COM_FLAG3

/* Flags for PUC_PORT_TYPE_LPT */
/* none currently */

struct puc_attach_args {
	int			port;
	int			type;
	int			flags;

	pci_chipset_tag_t	pc;
	pci_intr_handle_t	intrhandle;
	pcitag_t		tag;

	bus_addr_t		a;
	bus_space_tag_t		t;
	bus_space_handle_t	h;
	bus_dma_tag_t		dmat;
	bus_dma_tag_t		dmat64;
};

extern const struct puc_device_description puc_devices[];
extern const struct puc_device_description *
	puc_find_description(pcireg_t, pcireg_t, pcireg_t, pcireg_t);
