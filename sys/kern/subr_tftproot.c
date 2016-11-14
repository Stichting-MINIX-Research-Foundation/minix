/*	$NetBSD: subr_tftproot.c,v 1.16 2015/05/21 02:04:22 rtr Exp $ */

/*-
 * Copyright (c) 2007 Emmanuel Dreyfus, all rights reserved.
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
 *	This product includes software developed by Emmanuel Dreyfus
 * 4. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE THE AUTHOR AND CONTRIBUTORS ``AS IS'' 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Download the root RAMdisk through TFTP at root mount time
 */

#include "opt_tftproot.h"
#include "opt_md.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_tftproot.c,v 1.16 2015/05/21 02:04:22 rtr Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/lwp.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/timevar.h>
#include <sys/socketvar.h>

#include <net/if.h>

#include <dev/md.h>

#include <netinet/in.h>

#include <nfs/rpcv2.h>

#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsdiskless.h>
#include <nfs/nfs_var.h>

extern void       mdattach(int);

/* 
 * Copied from <lib/libsa/tftp.h> 
 */

#define SEGSIZE         512             /* data segment size */

/*
 * Packet types.
 */
#define RRQ     01                      /* read request */
#define WRQ     02                      /* write request */
#define DATA    03                      /* data packet */
#define ACK     04                      /* acknowledgement */
#define ERROR   05                      /* error code */

struct  tftphdr {
        short   th_opcode;              /* packet type */
        union {
                unsigned short tu_block; /* block # */
                short   tu_code;        /* error code */
                char    tu_stuff[1];    /* request packet stuff */
        } th_u;
        char    th_data[1];             /* data or error string */
} __packed;

#define th_block        th_u.tu_block
#define th_code         th_u.tu_code
#define th_stuff        th_u.tu_stuff
#define th_msg          th_data

#define IPPORT_TFTP 69

#ifdef TFTPROOT_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

struct tftproot_handle {
	struct nfs_diskless *trh_nd;
	void *trh_base;
	size_t trh_len;
	unsigned short trh_block;
	int trh_flags;
};

#define TRH_FINISHED	1

int tftproot_dhcpboot(device_t);

static int tftproot_getfile(struct tftproot_handle *, struct lwp *);
static int tftproot_recv(struct mbuf **, void *);

int
tftproot_dhcpboot(device_t bootdv)
{
	struct nfs_diskless *nd = NULL;
	struct ifnet *ifp = NULL;
	struct lwp *l;
	struct tftproot_handle trh;
	device_t dv;
	int error = -1;

	if (rootspec != NULL) {
		IFNET_FOREACH(ifp)
			if (strcmp(rootspec, ifp->if_xname) == 0)
				break;
	} 

	if ((ifp == NULL) &&
	    (bootdv != NULL && device_class(bootdv) == DV_IFNET)) {
		IFNET_FOREACH(ifp)
			if (strcmp(device_xname(bootdv), ifp->if_xname) == 0)
				break;
	}

	if (ifp == NULL) {
		DPRINTF(("%s():%d ifp is NULL\n", __func__, __LINE__));
		goto out;
	}

	dv = device_find_by_xname(ifp->if_xname);

	if ((dv == NULL) || (device_class(dv) != DV_IFNET)) {
		DPRINTF(("%s():%d cannot find device for interface %s\n",
		    __func__, __LINE__, ifp->if_xname));
		goto out;
	}

	root_device = dv;

	l = curlwp; /* XXX */

	nd = kmem_zalloc(sizeof(*nd), KM_SLEEP);
	nd->nd_ifp = ifp;
	nd->nd_nomount = 1;

	if ((error = nfs_boot_init(nd, l)) != 0) {
		DPRINTF(("%s():%d nfs_boot_init returned %d\n", 
		    __func__, __LINE__, error));
		goto out;
	}

	/* 
	 * Strip leading "tftp:"
	 */
#define PREFIX "tftp:"
	if (strstr(nd->nd_bootfile, PREFIX) == nd->nd_bootfile)
		(void)memmove(nd->nd_bootfile,
			      nd->nd_bootfile + (sizeof(PREFIX) - 1),
			      sizeof(nd->nd_bootfile) - sizeof(PREFIX));
#undef PREFIX

	printf("tftproot: bootfile=%s\n", nd->nd_bootfile);

	memset(&trh, 0, sizeof(trh));
	trh.trh_nd = nd;
	trh.trh_block = 1;

	if ((error = tftproot_getfile(&trh, l)) != 0) {
		DPRINTF(("%s():%d tftproot_getfile returned %d\n", 
		    __func__, __LINE__, error));
		goto out;
	}

	error = 0;

out:
	if (nd)
		kmem_free(nd, sizeof(*nd));

	return error;
}

static int 
tftproot_getfile(struct tftproot_handle *trh, struct lwp *l)
{
	struct socket *so = NULL;
	struct mbuf *m_serv = NULL;
	struct mbuf *m_outbuf = NULL;
	struct sockaddr_in sin;
	struct tftphdr *tftp;
	size_t packetlen, namelen;
	int error = -1;
	const char octetstr[] = "octet";
	size_t hdrlen = sizeof(*tftp) - sizeof(tftp->th_data);
	char *cp;
	
	if ((error = socreate(AF_INET, &so, SOCK_DGRAM, 0, l, NULL)) != 0) {
		DPRINTF(("%s():%d socreate returned %d\n", 
		    __func__, __LINE__, error));
		goto out;
	}

	/*
	 * Set timeout
	 */
	if ((error = nfs_boot_setrecvtimo(so))) {
		DPRINTF(("%s():%d SO_RCVTIMEO failed %d\n", 
		    __func__, __LINE__, error));
		goto out;
	}

	/*
	 * Set server address and port
	 */
	memcpy(&sin, &trh->trh_nd->nd_root.ndm_saddr, sizeof(sin));
	sin.sin_port = htons(IPPORT_TFTP);

	/*
	 * Set send buffer, prepare the TFTP packet
	 */
	namelen = strlen(trh->trh_nd->nd_bootfile) + 1;
	packetlen = sizeof(tftp->th_opcode) + namelen + sizeof(octetstr);
	if (packetlen > MSIZE) {
		DPRINTF(("%s():%d boot filename too long (%ld bytes)\n", 
		    __func__, __LINE__, (long)namelen));
		goto out;
	}

	m_outbuf = m_gethdr(M_WAIT, MT_DATA);
	m_clget(m_outbuf, M_WAIT);
	m_outbuf->m_len = packetlen;
	m_outbuf->m_pkthdr.len = packetlen;
	m_outbuf->m_pkthdr.rcvif = NULL;

	tftp = mtod(m_outbuf, struct tftphdr *);
	memset(tftp, 0, packetlen);

	tftp->th_opcode = htons((short)RRQ);
	cp = tftp->th_stuff;
	(void)strncpy(cp,  trh->trh_nd->nd_bootfile, namelen);
	cp += namelen;
	(void)strncpy(cp, octetstr, sizeof(octetstr));

	/* 
	 * Perform the file transfer
	 */
	printf("tftproot: download %s:%s ", 
	    inet_ntoa(sin.sin_addr), trh->trh_nd->nd_bootfile);

	do {
		/*
		 * Show progress for every 200 blocks (100kB)
		 */
#ifndef TFTPROOT_PROGRESS
#define TFTPROOT_PROGRESS 200
#endif
		if ((trh->trh_block % TFTPROOT_PROGRESS) == 0)
			twiddle();

		/* 
		 * Send the packet and receive the answer. 
		 * We get the sender address here, which should be
		 * the same server with a different port
		 */
		if ((error = nfs_boot_sendrecv(so, &sin, NULL, m_outbuf,
		    tftproot_recv, NULL, &m_serv, trh, l)) != 0) {
			DPRINTF(("%s():%d sendrecv failed %d\n", 
			    __func__, __LINE__, error));
			goto out;
		}

		/* 
		 * Accomodate the packet length for acks.
		 * This is really needed only on first pass
		 */
		m_outbuf->m_len = hdrlen;
		m_outbuf->m_pkthdr.len = hdrlen;
		tftp->th_opcode = htons((short)ACK);
		tftp->th_block = htons(trh->trh_block);


		/*
		 * Check for termination
		 */
		if (trh->trh_flags & TRH_FINISHED)
			break;

		trh->trh_block++;
	} while (1/* CONSTCOND */);

	printf("\n");

	/*
	 * Ack the last block. so_send frees m_outbuf, therefore
	 * we do not want to free it ourselves.
	 * Ignore errors, as we already have the whole file.
	 */
	if ((error = (*so->so_send)(so, mtod(m_serv, struct sockaddr *), NULL,
	    m_outbuf, NULL, 0, l)) != 0)
		DPRINTF(("%s():%d tftproot: sosend returned %d\n", 
		    __func__, __LINE__, error));
	else
		m_outbuf = NULL;

	/* 
	 * And use it as the root ramdisk. 
	 */
	DPRINTF(("%s():%d RAMdisk loaded: %ld@%p\n", 
	    __func__, __LINE__, trh->trh_len, trh->trh_base));
	md_root_setconf(trh->trh_base, trh->trh_len);
	mdattach(0);

	error = 0;
out:
	if (m_serv)
		m_freem(m_serv);

	if (m_outbuf)
		m_freem(m_outbuf);

	if (so)
		soclose(so);

	return error;
}

static int
tftproot_recv(struct mbuf **mp, void *ctx)
{
	struct tftproot_handle *trh = ctx;
	struct tftphdr *tftp;
	struct mbuf *m = *mp;
	size_t newlen;
	size_t hdrlen = sizeof(*tftp) - sizeof(tftp->th_data);

	/*
	 * Check for short packet
	 */
	if (m->m_pkthdr.len < hdrlen) {
		DPRINTF(("%s():%d short reply (%d bytes)\n", 
		    __func__, __LINE__, m->m_pkthdr.len));
		return -1;
	}

	/*
	 * Check for packet too large for being a TFTP packet
	 */
	if (m->m_pkthdr.len > hdrlen + SEGSIZE) {
		DPRINTF(("%s():%d packet too big (%d bytes)\n", 
		    __func__, __LINE__, m->m_pkthdr.len));
		return -1;
	}


	/*
	 * Examine the TFTP header
	 */
	if (m->m_len > sizeof(*tftp)) {
		if ((m = *mp = m_pullup(m, sizeof(*tftp))) == NULL) {
			DPRINTF(("%s():%d m_pullup failed\n",
			    __func__, __LINE__));
			return -1;
		}
	}
	tftp = mtod(m, struct tftphdr *);

	/*
	 * We handle data or error
	 */
	switch(ntohs(tftp->th_opcode)) {
	case DATA:
		break;
		
	case ERROR: {
		char errbuf[SEGSIZE + 1];
		int i;

		for (i = 0; i < SEGSIZE; i++) {
			if ((tftp->th_data[i] < ' ') ||
			    (tftp->th_data[i] > '~')) {
				errbuf[i] = '\0';
				break;
			}
			errbuf[i] = tftp->th_data[i];
		}
		errbuf[SEGSIZE] = '\0';

		printf("tftproot: TFTP server returned error %d, %s\n",
		    ntohs(tftp->th_code), errbuf);

		return -1;
		break;
	}

	default:
		DPRINTF(("%s():%d unexpected tftp reply opcode %d\n", 
		    __func__, __LINE__, ntohs(tftp->th_opcode)));
		return -1;
		break;
	}

	/*
	 * Check for last packet, which does not fill the whole space
	 */
	if (m->m_pkthdr.len < hdrlen + SEGSIZE) {
		DPRINTF(("%s():%d last chunk (%d bytes)\n", 
		    __func__, __LINE__, m->m_pkthdr.len));
		trh->trh_flags |= TRH_FINISHED;
	}


	if (ntohs(tftp->th_block) != trh->trh_block) {
		DPRINTF(("%s():%d expected block %d, got block %d\n",
		    __func__, __LINE__, trh->trh_block, ntohs(tftp->th_block)));
		return -1;
	}

	/* 
	 * Grow the receiving buffer to accommodate new data
	 */
	newlen = trh->trh_len + (m->m_pkthdr.len - hdrlen);
	if ((trh->trh_base = realloc(trh->trh_base, 
	    newlen, M_TEMP, M_WAITOK)) == NULL) {
		DPRINTF(("%s():%d failed to realloc %ld bytes\n", 
		    __func__, __LINE__, (long)newlen));
		return -1;
	}

	/*
	 * Copy the data
	 */
	m_copydata(m, hdrlen, m->m_pkthdr.len - hdrlen,
		   (char *)trh->trh_base + trh->trh_len);
	trh->trh_len = newlen;

	return 0;
}

