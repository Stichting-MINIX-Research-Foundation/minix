/*	$NetBSD: ether_bpf.c,v 1.10 2008/12/14 18:46:33 christos Exp $	*/

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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/bpf.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <kvm.h>
#include <nlist.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include <netif/netif_small.h>
#include <netif/etherdrv.h>

#define BPFDEV "/dev/bpf0"

#define MAXPKT 1536

/*
 * Allows to use any configured interface with
 * standalone network code. Provides the interface used
 * by i386/stand/lib/netif/netif_small.c.
 */

static int bpf = -1;

static struct nlist nl[] = {
	{"_ifnet"},
	{NULL}
};

int
EtherInit(char *ha)
{
	int res;
	u_int val;
	struct ifreq ifr;
	kvm_t *kvm;
	char errbuf[_POSIX2_LINE_MAX];
	struct ifnet_head ifh;
	struct ifnet *ifp;
	struct ifaddr *ifap = 0;
	struct sockaddr_dl *sdlp;
	int sdllen;

	bpf = open(BPFDEV, O_RDWR, 0);
	if (bpf < 0) {
		warn("open %s", BPFDEV);
		return 0;
	}

	val = MAXPKT;
	res = ioctl(bpf, BIOCSBLEN, &val);
	if (res < 0) {
		warn("ioctl BIOCSBLEN");
		return 0;
	}

	val = 1;
	res = ioctl(bpf, BIOCIMMEDIATE, &val);
	if (res < 0) {
		warn("ioctl BIOCIMMEDIATE");
		return 0;
	}

	val = 1;
	res = ioctl(bpf, FIONBIO, &val);
	if (res < 0) {
		warn("ioctl FIONBIO");
		return 0;
	}

	memcpy(ifr.ifr_name, BPF_IFNAME, IFNAMSIZ);
	res = ioctl(bpf, BIOCSETIF, &ifr);
	if (res < 0) {
		warn("ioctl BIOCSETIF %s", BPF_IFNAME);
		return 0;
	}

	kvm = kvm_openfiles(0, 0, 0, O_RDONLY, errbuf);
	if (!kvm) {
		warnx(errbuf);
		return 0;
	}
	if (kvm_nlist(kvm, nl) < 0) {
		warnx("nlist failed (%s)", kvm_geterr(kvm));
		kvm_close(kvm);
		return 0;
	}

	kvm_read(kvm, nl[0].n_value, &ifh, sizeof(struct ifnet_head));
	ifp = TAILQ_FIRST(&ifh);
	while (ifp) {
		struct ifnet ifnet;
		kvm_read(kvm, (u_long)ifp, &ifnet, sizeof(struct ifnet));
		if (!strcmp(ifnet.if_xname, BPF_IFNAME)) {
			ifap = IFADDR_FIRST(&ifnet);
			break;
		}
		ifp = IFNET_NEXT(&ifnet);
	}
	if (!ifp) {
		warnx("interface not found");
		kvm_close(kvm);
		return 0;
	}

#define _offsetof(t, m) ((int)((void *)&((t *)0)->m))
	sdllen = _offsetof(struct sockaddr_dl,
			   sdl_data[0]) + strlen(BPF_IFNAME) + 6;
	sdlp = malloc(sdllen);

	while (ifap) {
		struct ifaddr ifaddr;
		kvm_read(kvm, (u_long)ifap, &ifaddr, sizeof(struct ifaddr));
		kvm_read(kvm, (u_long)ifaddr.ifa_addr, sdlp, sdllen);
		if (sdlp->sdl_family == AF_LINK) {
			memcpy(ha, CLLADDR(sdlp), 6);
			break;
		}
		ifap = IFADDR_NEXT(&ifaddr);
	}
	free(sdlp);
	kvm_close(kvm);
	if (!ifap) {
		warnx("interface hw addr not found");
		return 0;
	}
	return 1;
}

void
EtherStop(void)
{

	if (bpf != -1)
		close(bpf);
}

int
EtherSend(char *pkt, int len)
{

	if (write(bpf, pkt, len) != len) {
		warn("EtherSend");
		return -1;
	}
	return len;
}

static union {
	struct bpf_hdr h;
	u_char buf[MAXPKT];
} rbuf;

int
EtherReceive(char *pkt, int maxlen)
{
	int res;

	res = read(bpf, &rbuf, MAXPKT);
	if (res > 0) {
#if 0
		int i;
		fprintf(stderr, "got packet, len=%d\n", rbuf.h.bh_caplen);
		if (rbuf.h.bh_caplen < rbuf.h.bh_datalen)
			printf("(truncated)\n");
		for (i = 0; i < 20; i++)
			fprintf(stderr, "%02x ", rbuf.buf[rbuf.h.bh_hdrlen + i]);
		fprintf(stderr, "\n");
#endif
		if (rbuf.h.bh_caplen > maxlen)
			return 0;
		memcpy(pkt, &rbuf.buf[rbuf.h.bh_hdrlen], rbuf.h.bh_caplen);
		return rbuf.h.bh_caplen;
	}

	return 0;
}
