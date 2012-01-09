/*	$NetBSD: pcnet_isapnp.c,v 1.8 2008/12/14 18:46:33 christos Exp $	*/

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


#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <libi386.h>
#include <isapnpvar.h>
#include <isadmavar.h>
#include <bootinfo.h>

#include "etherdrv.h"
#include "lance.h"

#ifndef ISAPNPID
#define ISAPNPID 0x516e0010 /* TKN0010 */
#endif

int lance_rap, lance_rdp;

u_char eth_myaddr[6];

extern void am7990_init(void);
extern void am7990_stop(void);

static struct btinfo_netif bi_netif;

int
EtherInit(unsigned char *myadr)
{
	int iobase, dmachan, i;

	if (isapnp_finddev(ISAPNPID, &iobase, &dmachan)) {
		printf("cannot find PCNET\n");
		return 0;
	}

	printf("printf using PCNET @ %x\n", iobase);

	lance_rap = iobase + 0x12;
	lance_rdp = iobase + 0x10;

	/* make sure it's stopped */
	am7990_stop();

	for (i = 0; i < 6; i++)
		myadr[i] = eth_myaddr[i] = inb(iobase + i);

	isa_dmacascade(dmachan);

	am7990_init();

	strncpy(bi_netif.ifname, "le", sizeof(bi_netif.ifname));
	bi_netif.bus = BI_BUS_ISA;
	bi_netif.addr.iobase = iobase;

	BI_ADD(&bi_netif, BTINFO_NETIF, sizeof(bi_netif));

	return 1;
}
