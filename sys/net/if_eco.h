/*	$NetBSD: if_eco.h,v 1.8 2008/02/20 17:05:52 matt Exp $	*/

/*-
 * Copyright (c) 2001 Ben Harris
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _NET_IF_ECO_H_
#define _NET_IF_ECO_H_

#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/queue.h>

#include <net/if.h>

/*
 * Econet headers come in two forms.  The initial frame of an exchange
 * has source and destination addresses, a control byte and a port.
 * Later frames just have source and destination addresses.
 *
 * Complete packets are generally passed around with the full header on,
 * even if this means assembling them from two separate frames.
 */
#define ECO_ADDR_LEN	2	/* Length of an Econet address */
#define ECO_HDR_LEN	6	/* Two addresses, a port and a control byte */
#define ECO_SHDR_LEN	4	/* "Short" Econet header: just two addresses */
/* #define ECO_MTU	8192	 * Default MTU */
#define ECO_IPMTU	1280	/* MTU for IP used by RISC iX */
#define ECO_MTU		ECO_IPMTU

struct eco_header {
	uint8_t	eco_dhost[ECO_ADDR_LEN];
	uint8_t	eco_shost[ECO_ADDR_LEN];
	uint8_t	eco_control;
	uint8_t	eco_port;
} __packed;

#define ECO_PORT_IMMEDIATE	0x00
#define ECO_PORT_DSTAPE		0x54 /* DigitalServicesTapeStore */
#define ECO_PORT_FS		0x99 /* FileServerCommand */
#define ECO_PORT_BRIDGE		0x9C /* Bridge */
#define ECO_PORT_PSINQREP	0x9E /* PrinterServerInquiryReply */
#define ECO_PORT_PSINQ		0x9F /* PrinterServerInquiry */
#define ECO_PORT_FAST		0xA0 /* SJ *FAST protocol */
#define ECO_PORT_NEXNETFIND	0xA1 /* SJ Nexus net find reply port */
#define ECO_PORT_FINDSRV	0xB0 /* FindServer */
#define ECO_PORT_FINDSRVREP	0xB1 /* FindServerReply */
#define ECO_PORT_TTXTCMD	0xB2 /* TeletextServerCommand */
#define ECO_PORT_TTXTPAGE	0xB3 /* TeletextServerPage */
#define ECO_PORT_OLDPSDATA	0xD0 /* OldPrinterServer */
#define ECO_PORT_PSDATA		0xD1 /* PrinterServer */
#define ECO_PORT_IP		0xD2 /* TCPIPProtocolSuite */
#define ECO_PORT_SIDSLAVE	0xD3 /* SIDFrameSlave */
#define ECO_PORT_SCROLLARAMA	0xD4 /* Scrollarama */
#define ECO_PORT_PHONE		0xD5 /* Phone */
#define ECO_PORT_BCASTCTL	0xD6 /* BroadcastControl */
#define ECO_PORT_BCASTDATA	0xD7 /* BroadcastData */
#define ECO_PORT_IMPLICENCE	0xD8 /* ImpressionLicenceChecker */
#define ECO_PORT_SQUIRREL	0xD9 /* DigitalServicesSquirrel */
#define ECO_PORT_SID2NDARY	0xDA /* SIDSecondary */
#define ECO_PORT_SQUIRREL2	0xDB /* DigitalServicesSquirrel2 */
#define ECO_PORT_DDCTL		0xDC /* DataDistributionControl */
#define ECO_PORT_DDDATA		0xDD /* DataDistributionData */
#define ECO_PORT_CLASSROM	0xDE /* ClassROM */
#define ECO_PORT_PSCMD		0xDF /* PrinterSpoolerCommand */

/* Control bytes for immediate operations. */
#define ECO_CTL_PEEK		0x81
#define ECO_CTL_POKE		0x82
#define ECO_CTL_JSR		0x83
#define ECO_CTL_USERPROC	0x84
#define ECO_CTL_OSPROC		0x85
#define ECO_CTL_HALT		0x86
#define ECO_CTL_CONTINUE	0x87
#define ECO_CTL_MACHINEPEEK	0x88
#define ECO_CTL_GETREGISTERS	0x89

/* Control bytes for IP */
#define ECO_CTL_IP		0x81
#define ECO_CTL_IPBCAST_REPLY	0x8E
#define ECO_CTL_IPBCAST_REQUEST	0x8F
#define ECO_CTL_ARP_REQUEST	0xA1
#define ECO_CTL_ARP_REPLY	0xA2

struct eco_arp {
	uint8_t ecar_spa[4];
	uint8_t ecar_tpa[4];
};

enum eco_state {
	ECO_UNKNOWN, ECO_IDLE, ECO_SCOUT_RCVD,
	ECO_SCOUT_SENT, ECO_DATA_SENT, ECO_IMMED_SENT,
	ECO_DONE
};


/*
 * This structure contains a packet that might need retransmitting,
 * together with a callout to trigger retransmission.  They're kept on
 * a per-interface list so they can be freed when an interface is
 * downed.
 */
struct eco_retry {
	LIST_ENTRY(eco_retry)	er_link;
	struct	callout er_callout;
	struct	mbuf *er_packet;
	struct	ifnet *er_ifp;
};

/*
 * Common structure used to store state about an Econet interface.
 */
struct ecocom {
	struct ifnet	ec_if;
	int	(*ec_claimwire)(struct ifnet *);
	void	(*ec_txframe)(struct ifnet *, struct mbuf *);
	enum eco_state	ec_state;
	struct mbuf	*ec_scout;
	struct mbuf	*ec_packet;
	LIST_HEAD(, eco_retry)	ec_retries;
};

#ifdef _KERNEL
void	eco_ifattach(struct ifnet *, const uint8_t *);
void	eco_ifdetach(struct ifnet *);
int	eco_init(struct ifnet *);
void	eco_stop(struct ifnet *, int);

char	*eco_sprintf(const uint8_t *);

struct mbuf *	eco_inputframe(struct ifnet *, struct mbuf *);
void	eco_inputidle(struct ifnet *);
#endif

#endif /* !_NET_IF_ECO_H_ */
