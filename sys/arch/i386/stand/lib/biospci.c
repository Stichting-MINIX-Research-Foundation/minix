/*	$NetBSD: biospci.c,v 1.5 2008/12/14 17:03:43 christos Exp $	 */

/*
 * Copyright (c) 1996
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

/*
 * basic PCI functions for libsa needs lowlevel parts from bios_pci.S
 */

#include <lib/libsa/stand.h>

#include "pcivar.h"

extern int pcibios_present(int *);
extern int pcibios_finddev(int, int, int, unsigned int *);
extern int pcibios_cfgread(unsigned int, int, int *);
extern int pcibios_cfgwrite(unsigned int, int, int);

#define PCISIG ('P' | ('C' << 8) | ('I' << 16) | (' ' << 24))

int
pcicheck(void)
{
	int             ret, sig;

	ret = pcibios_present(&sig);

	if ((ret & 0xff00) || (sig != PCISIG))
		return -1;

	return 0;
}

int
pcifinddev(int vid, int did, pcihdl_t *handle)
{
	int             ret;

	*handle = 0;

	ret = pcibios_finddev(vid, did, 0, handle);

	if (ret)
		return -1;

	return 0;
}

int
pcicfgread(pcihdl_t *handle, int off, int *val)
{
	int             ret;

	ret = pcibios_cfgread(*handle, off, val);

	if (ret)
		return -1;

	return 0;
}

int
pcicfgwrite(pcihdl_t *handle, int off, int val)
{
	int             ret;

	ret = pcibios_cfgwrite(*handle, off, val);

	if (ret)
		return -1;

	return 0;
}
