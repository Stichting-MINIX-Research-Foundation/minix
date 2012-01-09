/*	$NetBSD: netif_small.c,v 1.12 2009/10/21 23:12:09 snj Exp $	*/

/* minimal netif - for boot ROMs we don't have to select between
  several interfaces, and we have to save space

  hacked from netbsd:sys/arch/mvme68k/stand/libsa/netif.c
 */

/*
 * Copyright (c) 1995 Gordon W. Ross
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

#include <sys/types.h>
#include <sys/socket.h>
#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include <net/if.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/net.h>

#include "netif_small.h"
#include "etherdrv.h"

#ifdef NETIF_DEBUG
int netif_debug = 1;
#endif

/* we allow for one socket only */
static struct iodesc iosocket;

struct iodesc *
socktodesc(int sock)
{
	if (sock != 0) {
		return NULL;
	}
	return &iosocket;
}

int
netif_open(void)
{
	struct iodesc *io;

	io = &iosocket;
	if (io->io_netif) {
#ifdef NETIF_DEBUG
		printf("netif_open: device busy\n");
#endif
		return -1;
	}
	memset(io, 0, sizeof(*io));

	if (!EtherInit(io->myea)) {
		printf("EtherInit failed\n");
		return -1;
	}

	io->io_netif = (void*)1; /* mark busy */

	return 0;
}

void
netif_close(int fd)
{
	struct iodesc *io;

	if (fd != 0) {
		return;
	}

	io = &iosocket;
	if (io->io_netif) {
		EtherStop();
		io->io_netif = NULL;
	}
}

/*
 * Send a packet.  The ether header is already there.
 * Return the length sent (or -1 on error).
 */
int
netif_put(struct iodesc *desc, void *pkt, size_t len)
{
#ifdef NETIF_DEBUG
	if (netif_debug) {
		struct ether_header *eh;

		printf("netif_put: desc=%p pkt=%p len=%d\n",
			   desc, pkt, len);
		eh = pkt;
		printf("dst: %s ", ether_sprintf(eh->ether_dhost));
		printf("src: %s ", ether_sprintf(eh->ether_shost));
		printf("type: 0x%x\n", eh->ether_type & 0xFFFF);
	}
#endif
	return EtherSend(pkt, len);
}

/*
 * Receive a packet, including the ether header.
 * Return the total length received (or -1 on error).
 */
int
netif_get(struct iodesc *desc, void *pkt, size_t maxlen, saseconds_t timo)
{
	int len;
	satime_t t;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("netif_get: pkt=%p, maxlen=%d, tmo=%d\n",
			   pkt, maxlen, timo);
#endif

	t = getsecs();
	len = 0;
	while (((getsecs() - t) < timo) && !len) {
		len = EtherReceive(pkt, maxlen);
	}

#ifdef NETIF_DEBUG
	if (netif_debug) {
		struct ether_header *eh = pkt;

		printf("dst: %s ", ether_sprintf(eh->ether_dhost));
		printf("src: %s ", ether_sprintf(eh->ether_shost));
		printf("type: 0x%x\n", eh->ether_type & 0xFFFF);
	}
#endif

	return len;
}
