/*	$NetBSD: bootp.c,v 1.40 2015/07/25 07:06:11 isaki Exp $	*/

/*
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Header: bootp.c,v 1.4 93/09/11 03:13:51 leres Exp  (LBL)
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"
#include "net.h"
#include "bootp.h"

struct in_addr servip;
#ifdef SUPPORT_LINUX
char linuxcmdline[256];
#ifndef TAG_LINUX_CMDLINE
#define TAG_LINUX_CMDLINE 123
#endif
#endif

static n_long	nmask, smask;

static satime_t	bot;

static	char vm_rfc1048[4] = VM_RFC1048;
#ifdef BOOTP_VEND_CMU
static	char vm_cmu[4] = VM_CMU;
#endif

/* Local forwards */
static	ssize_t bootpsend(struct iodesc *, void *, size_t);
static	ssize_t bootprecv(struct iodesc *, void *, size_t, saseconds_t);
static	int vend_rfc1048(u_char *, u_int);
#ifdef BOOTP_VEND_CMU
static	void vend_cmu(u_char *);
#endif

#ifdef SUPPORT_DHCP
static char expected_dhcpmsgtype = -1, dhcp_ok;
struct in_addr dhcp_serverip;
#endif

/*
 * Boot programs can patch this at run-time to change the behavior
 * of bootp/dhcp.
 */
int bootp_flags;

static void
bootp_addvend(u_char *area)
{
#ifdef SUPPORT_DHCP
	char vci[64];
	int vcilen;
	
	*area++ = TAG_PARAM_REQ;
	*area++ = 6;
	*area++ = TAG_SUBNET_MASK;
	*area++ = TAG_GATEWAY;
	*area++ = TAG_HOSTNAME;
	*area++ = TAG_DOMAINNAME;
	*area++ = TAG_ROOTPATH;
	*area++ = TAG_SWAPSERVER;

	/* Insert a NetBSD Vendor Class Identifier option. */
	snprintf(vci, sizeof(vci), "NetBSD:%s:libsa", MACHINE);
	vcilen = strlen(vci);
	*area++ = TAG_CLASSID;
	*area++ = vcilen;
	(void)memcpy(area, vci, vcilen);
	area += vcilen;
#endif
	*area = TAG_END;
}

/* Fetch required bootp information */
void
bootp(int sock)
{
	struct iodesc *d;
	struct bootp *bp;
	struct {
		u_char header[UDP_TOTAL_HEADER_SIZE];
		struct bootp wbootp;
	} wbuf;
	struct {
		u_char header[UDP_TOTAL_HEADER_SIZE];
		struct bootp rbootp;
	} rbuf;
	unsigned int index;

#ifdef BOOTP_DEBUG
 	if (debug)
		printf("bootp: socket=%d\n", sock);
#endif
	if (!bot)
		bot = getsecs();

	if (!(d = socktodesc(sock))) {
		printf("bootp: bad socket. %d\n", sock);
		return;
	}
#ifdef BOOTP_DEBUG
 	if (debug)
		printf("bootp: d=%lx\n", (long)d);
#endif

	bp = &wbuf.wbootp;
	(void)memset(bp, 0, sizeof(*bp));

	bp->bp_op = BOOTREQUEST;
	bp->bp_htype = 1;		/* 10Mb Ethernet (48 bits) */
	bp->bp_hlen = 6;
	bp->bp_xid = htonl(d->xid);
	MACPY(d->myea, bp->bp_chaddr);
	(void)strncpy((char *)bp->bp_file, bootfile, sizeof(bp->bp_file));
	(void)memcpy(bp->bp_vend, vm_rfc1048, sizeof(vm_rfc1048));
	index = 4;
#ifdef SUPPORT_DHCP
	bp->bp_vend[index++] = TAG_DHCP_MSGTYPE;
	bp->bp_vend[index++] = 1;
	bp->bp_vend[index++] = DHCPDISCOVER;
#endif
	bootp_addvend(&bp->bp_vend[index]);

	d->myip.s_addr = INADDR_ANY;
	d->myport = htons(IPPORT_BOOTPC);
	d->destip.s_addr = INADDR_BROADCAST;
	d->destport = htons(IPPORT_BOOTPS);

#ifdef SUPPORT_DHCP
	expected_dhcpmsgtype = DHCPOFFER;
	dhcp_ok = 0;
#endif

	if (sendrecv(d,
		    bootpsend, bp, sizeof(*bp),
		    bootprecv, &rbuf.rbootp, sizeof(rbuf.rbootp))
	   == -1) {
		printf("bootp: no reply\n");
		return;
	}

#ifdef SUPPORT_DHCP
	if (dhcp_ok) {
		u_int32_t leasetime;
		index = 6;
		bp->bp_vend[index++] = DHCPREQUEST;
		bp->bp_vend[index++] = TAG_REQ_ADDR;
		bp->bp_vend[index++] = 4;
		(void)memcpy(&bp->bp_vend[9], &rbuf.rbootp.bp_yiaddr, 4);
		index += 4;
		bp->bp_vend[index++] = TAG_SERVERID;
		bp->bp_vend[index++] = 4;
		(void)memcpy(&bp->bp_vend[index], &dhcp_serverip.s_addr, 4);
		index += 4;
		bp->bp_vend[index++] = TAG_LEASETIME;
		bp->bp_vend[index++] = 4;
		leasetime = htonl(300);
		(void)memcpy(&bp->bp_vend[index], &leasetime, 4);
		index += 4;
		bootp_addvend(&bp->bp_vend[index]);

		expected_dhcpmsgtype = DHCPACK;

		if (sendrecv(d,
			    bootpsend, bp, sizeof(*bp),
			    bootprecv, &rbuf.rbootp, sizeof(rbuf.rbootp))
		   == -1) {
			printf("DHCPREQUEST failed\n");
			return;
		}
	}
#endif

	myip = d->myip = rbuf.rbootp.bp_yiaddr;
	servip = rbuf.rbootp.bp_siaddr;
	if (rootip.s_addr == INADDR_ANY)
		rootip = servip;
	(void)memcpy(bootfile, rbuf.rbootp.bp_file, sizeof(bootfile));
	bootfile[sizeof(bootfile) - 1] = '\0';

	if (IN_CLASSA(myip.s_addr))
		nmask = IN_CLASSA_NET;
	else if (IN_CLASSB(myip.s_addr))
		nmask = IN_CLASSB_NET;
	else
		nmask = IN_CLASSC_NET;
#ifdef BOOTP_DEBUG
	if (debug)
		printf("'native netmask' is %s\n", intoa(nmask));
#endif

	/* Get subnet (or natural net) mask */
	netmask = nmask;
	if (smask)
		netmask = smask;
#ifdef BOOTP_DEBUG
	if (debug)
		printf("mask: %s\n", intoa(netmask));
#endif

	/* We need a gateway if root is on a different net */
	if (!SAMENET(myip, rootip, netmask)) {
#ifdef BOOTP_DEBUG
		if (debug)
			printf("need gateway for root ip\n");
#endif
	}

	/* Toss gateway if on a different net */
	if (!SAMENET(myip, gateip, netmask)) {
#ifdef BOOTP_DEBUG
		if (debug)
			printf("gateway ip (%s) bad\n", inet_ntoa(gateip));
#endif
		gateip.s_addr = 0;
	}

#ifdef BOOTP_DEBUG
	if (debug) {
		printf("client addr: %s\n", inet_ntoa(myip));
		if (smask)
			printf("subnet mask: %s\n", intoa(smask));
		if (gateip.s_addr != 0)
			printf("net gateway: %s\n", inet_ntoa(gateip));
		printf("server addr: %s\n", inet_ntoa(rootip));
		if (rootpath[0] != '\0')
			printf("server path: %s\n", rootpath);
		if (bootfile[0] != '\0')
			printf("file name: %s\n", bootfile);
	}
#endif

	/* Bump xid so next request will be unique. */
	++d->xid;
}

/* Transmit a bootp request */
static ssize_t
bootpsend(struct iodesc *d, void *pkt, size_t len)
{
	struct bootp *bp;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("bootpsend: d=%lx called.\n", (long)d);
#endif

	bp = pkt;
	bp->bp_secs = htons((u_short)(getsecs() - bot));

#ifdef BOOTP_DEBUG
	if (debug)
		printf("bootpsend: calling sendudp\n");
#endif

	return sendudp(d, pkt, len);
}

static ssize_t
bootprecv(struct iodesc *d, void *pkt, size_t len, saseconds_t tleft)
{
	ssize_t n;
	struct bootp *bp;

#ifdef BOOTP_DEBUGx
	if (debug)
		printf("bootp_recvoffer: called\n");
#endif

	n = readudp(d, pkt, len, tleft);
	if (n == -1 || (size_t)n < sizeof(struct bootp) - BOOTP_VENDSIZE)
		goto bad;

	bp = (struct bootp *)pkt;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("bootprecv: checked.  bp = 0x%lx, n = %d\n",
		    (long)bp, (int)n);
#endif
	if (bp->bp_xid != htonl(d->xid)) {
#ifdef BOOTP_DEBUG
		if (debug) {
			printf("bootprecv: expected xid 0x%lx, got 0x%x\n",
			    d->xid, ntohl(bp->bp_xid));
		}
#endif
		goto bad;
	}

	/* protect against bogus addresses sent by DHCP servers */
	if (bp->bp_yiaddr.s_addr == INADDR_ANY ||
	    bp->bp_yiaddr.s_addr == INADDR_BROADCAST)
		goto bad;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("bootprecv: got one!\n");
#endif

	/* Suck out vendor info */
	if (memcmp(vm_rfc1048, bp->bp_vend, sizeof(vm_rfc1048)) == 0) {
		if (vend_rfc1048(bp->bp_vend, sizeof(bp->bp_vend)) != 0)
			goto bad;
	}
#ifdef BOOTP_VEND_CMU
	else if (memcmp(vm_cmu, bp->bp_vend, sizeof(vm_cmu)) == 0)
		vend_cmu(bp->bp_vend);
#endif
	else
		printf("bootprecv: unknown vendor 0x%lx\n", (long)bp->bp_vend);

	return n;
bad:
	errno = 0;
	return -1;
}

static int
vend_rfc1048(u_char *cp, u_int len)
{
	u_char *ep;
	int size;
	u_char tag;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("vend_rfc1048 bootp info. len=%d\n", len);
#endif
	ep = cp + len;

	/* Step over magic cookie */
	cp += sizeof(int);

	while (cp < ep) {
		tag = *cp++;
		size = *cp++;
		if (tag == TAG_END)
			break;

		if (tag == TAG_SUBNET_MASK && size >= sizeof(smask)) {
			(void)memcpy(&smask, cp, sizeof(smask));
		}
		if (tag == TAG_GATEWAY && size >= sizeof(gateip.s_addr)) {
			(void)memcpy(&gateip.s_addr, cp, sizeof(gateip.s_addr));
		}
		if (tag == TAG_SWAPSERVER && size >= sizeof(rootip.s_addr)) {
			/* let it override bp_siaddr */
			(void)memcpy(&rootip.s_addr, cp, sizeof(rootip.s_addr));
		}
		if (tag == TAG_ROOTPATH && size < sizeof(rootpath)) {
			strncpy(rootpath, (char *)cp, sizeof(rootpath));
			rootpath[size] = '\0';
		}
		if (tag == TAG_HOSTNAME && size < sizeof(hostname)) {
			strncpy(hostname, (char *)cp, sizeof(hostname));
			hostname[size] = '\0';
		}
#ifdef SUPPORT_DHCP
		if (tag == TAG_DHCP_MSGTYPE) {
			if (*cp != expected_dhcpmsgtype)
				return -1;
			dhcp_ok = 1;
		}
		if (tag == TAG_SERVERID &&
		    size >= sizeof(dhcp_serverip.s_addr))
		{
			(void)memcpy(&dhcp_serverip.s_addr, cp, 
			      sizeof(dhcp_serverip.s_addr));
		}
#endif
#ifdef SUPPORT_LINUX
		if (tag == TAG_LINUX_CMDLINE && size < sizeof(linuxcmdline)) {
			strncpy(linuxcmdline, (char *)cp, sizeof(linuxcmdline));
			linuxcmdline[size] = '\0';
		}
#endif
		cp += size;
	}
	return 0;
}

#ifdef BOOTP_VEND_CMU
static void
vend_cmu(u_char *cp)
{
	struct cmu_vend *vp;

#ifdef BOOTP_DEBUG
	if (debug)
		printf("vend_cmu bootp info.\n");
#endif
	vp = (struct cmu_vend *)cp;

	if (vp->v_smask.s_addr != 0) {
		smask = vp->v_smask.s_addr;
	}
	if (vp->v_dgate.s_addr != 0) {
		gateip = vp->v_dgate;
	}
}
#endif
