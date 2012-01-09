/*	$NetBSD: pci_user.c,v 1.4 2008/12/14 18:46:33 christos Exp $	*/

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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

#include "sanamespace.h"

#include <sys/types.h>
#include <machine/pio.h>

#include <pcivar.h>

extern int mapio(void);

/*
 * Replacement for i386/stand/lib/biospci.c.
 * Very simple functions to access PCI config space from
 * userland. Works with configuration mode 1 only, can
 * only access bus number 0.
 */

#define	PCI_MODE1_ENABLE	0x80000000UL
#define	PCI_MODE1_ADDRESS_REG	0x0cf8
#define	PCI_MODE1_DATA_REG	0x0cfc

static int
maketag(int bus, int dev, int fcn)
{

	return PCI_MODE1_ENABLE |
	    (bus << 16) | (dev << 11) | (fcn << 8);
}

int
pcicheck(void)
{

	return mapio() ? -1 : 0;
}

int
pcifinddev(int vid, int did, pcihdl_t *handle)
{
	int i;

	for (i = 0; i < 32; i++) {
		pcihdl_t h;
		int id;
		h = maketag(0, i, 0);
		pcicfgread(&h, 0, &id);
		if (id == (vid | (did << 16))) {
			*handle = h;
			return 0;
		}
	}
	return -1;
}

int
pcicfgread(pcihdl_t *handle, int off, int *val)
{
	int data;

	outl(PCI_MODE1_ADDRESS_REG, *handle | off);
	data = inl(PCI_MODE1_DATA_REG);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	*val = data;
	return 0;
}

int
pcicfgwrite(pcihdl_t *handle, int off, int val)
{
	outl(PCI_MODE1_ADDRESS_REG, *handle | off);
	outl(PCI_MODE1_DATA_REG, val);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	return 0;
}
