/*	$NetBSD: if_ie_vme.c,v 1.31 2014/08/18 04:26:38 riastradh Exp $	*/

/*
 * Copyright (c) 1995 Charles D. Cranor
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
 * Converted to SUN ie driver by Charles D. Cranor,
 *		October 1994, January 1995.
 */

/*
 * The i82586 is a very painful chip, found in sun3's, sun-4/100's
 * sun-4/200's, and VME based suns.  The byte order is all wrong for a
 * SUN, making life difficult.  Programming this chip is mostly the same,
 * but certain details differ from system to system.  This driver is
 * written so that different "ie" interfaces can be controled by the same
 * driver.
 */

/*
 * programming notes:
 *
 * the ie chip operates in a 24 bit address space.
 *
 * most ie interfaces appear to be divided into two parts:
 *	 - generic 586 stuff
 *	 - board specific
 *
 * generic:
 *	the generic stuff of the ie chip is all done with data structures
 * 	that live in the chip's memory address space.   the chip expects
 * 	its main data structure (the sys conf ptr -- SCP) to be at a fixed
 * 	address in its 24 bit space: 0xfffff4
 *
 *      the SCP points to another structure called the ISCP.
 *      the ISCP points to another structure called the SCB.
 * 	the SCB has a status field, a linked list of "commands", and
 * 	a linked list of "receive buffers".   these are data structures that
 * 	live in memory, not registers.
 *
 * board:
 * 	to get the chip to do anything, you first put a command in the
 * 	command data structure list.   then you have to signal "attention"
 * 	to the chip to get it to look at the command.   how you
 * 	signal attention depends on what board you have... on PC's
 * 	there is an i/o port number to do this, on sun's there is a
 * 	register bit you toggle.
 *
 * 	to get data from the chip you program it to interrupt...
 *
 *
 * sun issues:
 *
 *      there are 3 kinds of sun "ie" interfaces:
 *        1 - a VME/multibus card
 *        2 - an on-board interface (sun3's, sun-4/100's, and sun-4/200's)
 *        3 - another VME board called the 3E
 *
 * 	the VME boards lives in vme16 space.   only 16 and 8 bit accesses
 * 	are allowed, so functions that copy data must be aware of this.
 *
 * 	the chip is an intel chip.  this means that the byte order
 * 	on all the "short"s in the chip's data structures is wrong.
 * 	so, constants described in the intel docs are swapped for the sun.
 * 	that means that any buffer pointers you give the chip must be
 * 	swapped to intel format.   yuck.
 *
 *   VME/multibus interface:
 * 	for the multibus interface the board ignores the top 4 bits
 * 	of the chip address.   the multibus interface has its own
 * 	MMU like page map (without protections or valid bits, etc).
 * 	there are 256 pages of physical memory on the board (each page
 * 	is 1024 bytes).   There are 1024 slots in the page map.  so,
 * 	a 1024 byte page takes up 10 bits of address for the offset,
 * 	and if there are 1024 slots in the page that is another 10 bits
 * 	of the address.   That makes a 20 bit address, and as stated
 * 	earlier the board ignores the top 4 bits, so that accounts
 * 	for all 24 bits of address.
 *
 * 	Note that the last entry of the page map maps the top of the
 * 	24 bit address space and that the SCP is supposed to be at
 * 	0xfffff4 (taking into account allignment).   so,
 *	for multibus, that entry in the page map has to be used for the SCP.
 *
 * 	The page map effects BOTH how the ie chip sees the
 * 	memory, and how the host sees it.
 *
 * 	The page map is part of the "register" area of the board
 *
 *	The page map to control where ram appears in the address space.
 *	We choose to have RAM start at 0 in the 24 bit address space.
 *
 *	to get the phyiscal address of the board's RAM you must take the
 *	top 12 bits of the physical address of the register address and
 *	or in the 4 bits from the status word as bits 17-20 (remember that
 *	the board ignores the chip's top 4 address lines). For example:
 *	if the register is @ 0xffe88000, then the top 12 bits are 0xffe00000.
 *	to get the 4 bits from the status word just do status & IEVME_HADDR.
 *	suppose the value is "4".   Then just shift it left 16 bits to get
 *	it into bits 17-20 (e.g. 0x40000).    Then or it to get the
 *	address of RAM (in our example: 0xffe40000).   see the attach routine!
 *
 *
 *   on-board interface:
 *
 *	on the onboard ie interface the 24 bit address space is hardwired
 *	to be 0xff000000 -> 0xffffffff of KVA.   this means that sc_iobase
 *	will be 0xff000000.   sc_maddr will be where ever we allocate RAM
 *	in KVA.    note that since the SCP is at a fixed address it means
 *	that we have to allocate a fixed KVA for the SCP.
 *	<fill in useful info later>
 *
 *
 *   VME3E interface:
 *
 *	<fill in useful info later>
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ie_vme.c,v 1.31 2014/08/18 04:26:38 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <sys/bus.h>
#include <sys/intr.h>
#include <dev/vme/vmevar.h>

#include <dev/ic/i82586reg.h>
#include <dev/ic/i82586var.h>

#include "locators.h"

/*
 * VME/multibus definitions
 */
#define IEVME_PAGESIZE 1024	/* bytes */
#define IEVME_PAGSHIFT 10	/* bits */
#define IEVME_NPAGES   256	/* number of pages on chip */
#define IEVME_MAPSZ    1024	/* number of entries in the map */

/*
 * PTE for the page map
 */
#define IEVME_SBORDR 0x8000	/* sun byte order */
#define IEVME_IBORDR 0x0000	/* intel byte ordr */

#define IEVME_P2MEM  0x2000	/* memory is on P2 */
#define IEVME_OBMEM  0x0000	/* memory is on board */

#define IEVME_PGMASK 0x0fff	/* gives the physical page frame number */

struct ievme {
	u_int16_t	pgmap[IEVME_MAPSZ];
	u_int16_t	xxx[32];	/* prom */
	u_int16_t	status;		/* see below for bits */
	u_int16_t	xxx2;		/* filler */
	u_int16_t	pectrl;		/* parity control (see below) */
	u_int16_t	peaddr;		/* low 16 bits of address */
};

/*
 * status bits
 */
#define IEVME_RESET 0x8000	/* reset board */
#define IEVME_ONAIR 0x4000	/* go out of loopback 'on-air' */
#define IEVME_ATTEN 0x2000	/* attention */
#define IEVME_IENAB 0x1000	/* interrupt enable */
#define IEVME_PEINT 0x0800	/* parity error interrupt enable */
#define IEVME_PERR  0x0200	/* parity error flag */
#define IEVME_INT   0x0100	/* interrupt flag */
#define IEVME_P2EN  0x0020	/* enable p2 bus */
#define IEVME_256K  0x0010	/* 256kb rams */
#define IEVME_HADDR 0x000f	/* mask for bits 17-20 of address */

/*
 * parity control
 */
#define IEVME_PARACK 0x0100	/* parity error ack */
#define IEVME_PARSRC 0x0080	/* parity error source */
#define IEVME_PAREND 0x0040	/* which end of the data got the error */
#define IEVME_PARADR 0x000f	/* mask to get bits 17-20 of parity address */

/* Supported media */
static int media[] = {
	IFM_ETHER | IFM_10_2,
};
#define NMEDIA	(sizeof(media) / sizeof(media[0]))

/*
 * the 3E board not supported (yet?)
 */


static void ie_vmereset(struct ie_softc *, int);
static void ie_vmeattend(struct ie_softc *, int);
static void ie_vmerun(struct ie_softc *);
static int  ie_vmeintr(struct ie_softc *, int);

int ie_vme_match(device_t, cfdata_t, void *);
void ie_vme_attach(device_t, device_t, void *);

struct ie_vme_softc {
	struct ie_softc ie;
	bus_space_tag_t ievt;
	bus_space_handle_t ievh;
};

CFATTACH_DECL_NEW(ie_vme, sizeof(struct ie_vme_softc),
    ie_vme_match, ie_vme_attach, NULL, NULL);

#define read_iev(sc, reg) \
  bus_space_read_2(sc->ievt, sc->ievh, offsetof(struct ievme, reg))
#define write_iev(sc, reg, val) \
  bus_space_write_2(sc->ievt, sc->ievh, offsetof(struct ievme, reg), val)

/*
 * MULTIBUS/VME support routines
 */
void
ie_vmereset(struct ie_softc *sc, int what)
{
	struct ie_vme_softc *vsc = (struct ie_vme_softc *)sc;
	write_iev(vsc, status, IEVME_RESET);
	delay(100);		/* XXX could be shorter? */
	write_iev(vsc, status, 0);
}

void
ie_vmeattend(struct ie_softc *sc, int why)
{
	struct ie_vme_softc *vsc = (struct ie_vme_softc *)sc;

	/* flag! */
	write_iev(vsc, status, read_iev(vsc, status) | IEVME_ATTEN);
	/* down. */
	write_iev(vsc, status, read_iev(vsc, status) & ~IEVME_ATTEN);
}

void
ie_vmerun(struct ie_softc *sc)
{
	struct ie_vme_softc *vsc = (struct ie_vme_softc *)sc;

	write_iev(vsc, status, read_iev(vsc, status)
		  | IEVME_ONAIR | IEVME_IENAB | IEVME_PEINT);
}

int
ie_vmeintr(struct ie_softc *sc, int where)
{
	struct ie_vme_softc *vsc = (struct ie_vme_softc *)sc;

	if (where != INTR_ENTER)
		return (0);

        /*
         * check for parity error
         */
	if (read_iev(vsc, status) & IEVME_PERR) {
		aprint_error_dev(sc->sc_dev, "parity error (ctrl 0x%x @ 0x%02x%04x)\n",
		       read_iev(vsc, pectrl),
		       read_iev(vsc, pectrl) & IEVME_HADDR,
		       read_iev(vsc, peaddr));
		write_iev(vsc, pectrl, read_iev(vsc, pectrl) | IEVME_PARACK);
	}
	return (0);
}

void ie_memcopyin(struct ie_softc *, void *, int, size_t);
void ie_memcopyout(struct ie_softc *, const void *, int, size_t);

/*
 * Copy board memory to kernel.
 */
void
ie_memcopyin(struct ie_softc *sc, void *p, int offset, size_t size)
{
	size_t help;

	if ((offset & 1) && ((u_long)p & 1) && size > 0) {
		*(u_int8_t *)p = bus_space_read_1(sc->bt, sc->bh, offset);
		offset++;
		p = (u_int8_t *)p + 1;
		size--;
	}

	if ((offset & 1) || ((u_long)p & 1)) {
		bus_space_read_region_1(sc->bt, sc->bh, offset, p, size);
		return;
	}

	help = size / 2;
	bus_space_read_region_2(sc->bt, sc->bh, offset, p, help);
	if (2 * help == size)
		return;

	offset += 2 * help;
	p = (u_int16_t *)p + help;
	*(u_int8_t *)p = bus_space_read_1(sc->bt, sc->bh, offset);
}

/*
 * Copy from kernel space to board memory.
 */
void
ie_memcopyout(struct ie_softc *sc, const void *p, int offset, size_t size)
{
	size_t help;

	if ((offset & 1) && ((u_long)p & 1) && size > 0) {
		bus_space_write_1(sc->bt, sc->bh, offset, *(const u_int8_t *)p);
		offset++;
		p = (const u_int8_t *)p + 1;
		size--;
	}

	if ((offset & 1) || ((u_long)p & 1)) {
		bus_space_write_region_1(sc->bt, sc->bh, offset, p, size);
		return;
	}

	help = size / 2;
	bus_space_write_region_2(sc->bt, sc->bh, offset, p, help);
	if (2 * help == size)
		return;

	offset += 2 * help;
	p = (const u_int16_t *)p + help;
	bus_space_write_1(sc->bt, sc->bh, offset, *(const u_int8_t *)p);
}

/* read a 16-bit value at BH offset */
u_int16_t ie_vme_read16(struct ie_softc *, int offset);
/* write a 16-bit value at BH offset */
void ie_vme_write16(struct ie_softc *, int offset, u_int16_t value);
void ie_vme_write24(struct ie_softc *, int offset, int addr);

u_int16_t
ie_vme_read16(struct ie_softc *sc, int offset)
{
	u_int16_t v;

	bus_space_barrier(sc->bt, sc->bh, offset, 2, BUS_SPACE_BARRIER_READ);
	v = bus_space_read_2(sc->bt, sc->bh, offset);
	return (((v&0xff)<<8) | ((v>>8)&0xff));
}

void
ie_vme_write16(struct ie_softc *sc, int offset, u_int16_t v)
{
	int v0 = ((((v)&0xff)<<8) | (((v)>>8)&0xff));
	bus_space_write_2(sc->bt, sc->bh, offset, v0);
	bus_space_barrier(sc->bt, sc->bh, offset, 2, BUS_SPACE_BARRIER_WRITE);
}

void
ie_vme_write24(struct ie_softc *sc, int offset, int addr)
{
	u_char *f = (u_char *)&addr;
	u_int16_t v0, v1;
	u_char *t;

	t = (u_char *)&v0;
	t[0] = f[3]; t[1] = f[2];
	bus_space_write_2(sc->bt, sc->bh, offset, v0);

	t = (u_char *)&v1;
	t[0] = f[1]; t[1] = 0;
	bus_space_write_2(sc->bt, sc->bh, offset+2, v1);

	bus_space_barrier(sc->bt, sc->bh, offset, 4, BUS_SPACE_BARRIER_WRITE);
}

int
ie_vme_match(device_t parent, cfdata_t cf, void *aux)
{
	struct vme_attach_args *va = aux;
	vme_chipset_tag_t ct = va->va_vct;
	vme_am_t mod;
	int error;

	if (va->numcfranges < 2) {
		printf("ie_vme_match: need 2 ranges\n");
		return (0);
	}
	if ((va->r[1].offset & 0xff0fffff) ||
	    ((va->r[0].offset & 0xfff00000)
	     != (va->r[1].offset & 0xfff00000))) {
		printf("ie_vme_match: base address mismatch\n");
		return (0);
	}
	if (va->r[0].size != VMECF_LEN_DEFAULT &&
	    va->r[0].size != sizeof(struct ievme)) {
		printf("ie_vme_match: bad csr size\n");
		return (0);
	}
	if (va->r[1].size == VMECF_LEN_DEFAULT) {
		printf("ie_vme_match: must specify memory size\n");
		return (0);
	}

	mod = 0x3d; /* VME_AM_A24|VME_AM_MBO|VME_AM_SUPER|VME_AM_DATA */

	if (va->r[0].am != VMECF_AM_DEFAULT &&
	    va->r[0].am != mod)
		return (0);

	if (vme_space_alloc(va->va_vct, va->r[0].offset,
			    sizeof(struct ievme), mod))
		return (0);
	if (vme_space_alloc(va->va_vct, va->r[1].offset,
			    va->r[1].size, mod)) {
		vme_space_free(va->va_vct, va->r[0].offset,
			       sizeof(struct ievme), mod);
		return (0);
	}
	error = vme_probe(ct, va->r[0].offset, 2, mod, VME_D16, 0, 0);
	vme_space_free(va->va_vct, va->r[0].offset, sizeof(struct ievme), mod);
	vme_space_free(va->va_vct, va->r[1].offset, va->r[1].size, mod);

	return (error == 0);
}

void
ie_vme_attach(device_t parent, device_t self, void *aux)
{
	u_int8_t myaddr[ETHER_ADDR_LEN];
	struct ie_vme_softc *vsc = device_private(self);
	struct vme_attach_args *va = aux;
	vme_chipset_tag_t ct = va->va_vct;
	struct ie_softc *sc;
	vme_intr_handle_t ih;
	vme_addr_t rampaddr;
	vme_size_t memsize;
	vme_mapresc_t resc;
	int lcv;
	prop_data_t eaddrprop;
	vme_am_t mod;

	/*
	 * *note*: we don't detect the difference between a VME3E and
	 * a multibus/vme card.   if you want to use a 3E you'll have
	 * to fix this.
	 */
	mod = 0x3d; /* VME_AM_A24|VME_AM_MBO|VME_AM_SUPER|VME_AM_DATA */
	if (vme_space_alloc(va->va_vct, va->r[0].offset,
			    sizeof(struct ievme), mod) ||
	    vme_space_alloc(va->va_vct, va->r[1].offset,
			    va->r[1].size, mod))
		panic("if_ie: vme alloc");

	sc = &vsc->ie;
	sc->sc_dev = self;

	sc->hwreset = ie_vmereset;
	sc->hwinit = ie_vmerun;
	sc->chan_attn = ie_vmeattend;
	sc->intrhook = ie_vmeintr;
	sc->memcopyout = ie_memcopyout;
	sc->memcopyin = ie_memcopyin;

	sc->ie_bus_barrier = NULL;
	sc->ie_bus_read16 = ie_vme_read16;
	sc->ie_bus_write16 = ie_vme_write16;
	sc->ie_bus_write24 = ie_vme_write24;

	memsize = va->r[1].size;

	if (vme_space_map(ct, va->r[0].offset, sizeof(struct ievme), mod,
			  VME_D16 | VME_D8, 0,
			  &vsc->ievt, &vsc->ievh, &resc) != 0)
		panic("if_ie: vme map csr");

	rampaddr = va->r[1].offset;

	/* 4 more */
	rampaddr = rampaddr | ((read_iev(vsc, status) & IEVME_HADDR) << 16);
	if (vme_space_map(ct, rampaddr, memsize, mod, VME_D16 | VME_D8, 0,
			  &sc->bt, &sc->bh, &resc) != 0)
		panic("if_ie: vme map mem");

	write_iev(vsc, pectrl, read_iev(vsc, pectrl) | IEVME_PARACK);

	/*
	 * Set up mappings, direct map except for last page
	 * which is mapped at zero and at high address (for scp)
	 */
	for (lcv = 0; lcv < IEVME_MAPSZ - 1; lcv++)
		write_iev(vsc, pgmap[lcv], IEVME_SBORDR | IEVME_OBMEM | lcv);
	write_iev(vsc, pgmap[IEVME_MAPSZ - 1], IEVME_SBORDR | IEVME_OBMEM | 0);

	/* Clear all ram */
	bus_space_set_region_2(sc->bt, sc->bh, 0, 0, memsize/2);

	/*
	 * We use the first page to set up SCP, ICSP and SCB data
	 * structures. The remaining pages become the buffer area
	 * (managed in i82586.c).
	 * SCP is in double-mapped page, so the 586 can see it at
	 * the mandatory magic address (IE_SCP_ADDR).
	 */
	sc->scp = (IE_SCP_ADDR & (IEVME_PAGESIZE - 1));

	/* iscp at location zero */
	sc->iscp = 0;

	/* scb follows iscp */
	sc->scb = IE_ISCP_SZ;

	ie_vme_write16(sc, IE_ISCP_SCB((long)sc->iscp), sc->scb);
	ie_vme_write16(sc, IE_ISCP_BASE((u_long)sc->iscp), 0);
	ie_vme_write24(sc, IE_SCP_ISCP((u_long)sc->scp), 0);

	if (i82586_proberam(sc) == 0) {
		printf(": memory probe failed\n");
		return;
	}

	/*
	 * Rest of first page is unused; rest of ram for buffers.
	 */
	sc->buf_area = IEVME_PAGESIZE;
	sc->buf_area_sz = memsize - IEVME_PAGESIZE;

	sc->do_xmitnopchain = 0;

	printf("\n%s:", device_xname(self));

	eaddrprop = prop_dictionary_get(device_properties(self), "mac-address");
	if (eaddrprop != NULL && prop_data_size(eaddrprop) == ETHER_ADDR_LEN)
		memcpy(myaddr, prop_data_data_nocopy(eaddrprop),
			ETHER_ADDR_LEN);

	i82586_attach(sc, "multibus/vme", myaddr, media, NMEDIA, media[0]);

	vme_intr_map(ct, va->ilevel, va->ivector, &ih);
	vme_intr_establish(ct, ih, IPL_NET, i82586_intr, sc);
}
