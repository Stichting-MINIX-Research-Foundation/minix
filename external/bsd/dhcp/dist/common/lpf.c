/*	$NetBSD: lpf.c,v 1.1.1.4 2014/07/12 11:57:44 spz Exp $	*/
/* lpf.c

   Linux packet filter code, contributed by Brian Murrel at Interlinx
   Support Services in Vancouver, B.C. */

/*
 * Copyright (c) 2009,2012 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2007 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: lpf.c,v 1.1.1.4 2014/07/12 11:57:44 spz Exp $");

#include "dhcpd.h"
#if defined (USE_LPF_SEND) || defined (USE_LPF_RECEIVE)
#include <sys/uio.h>
#include <errno.h>

#include <asm/types.h>
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <netinet/in_systm.h>
#include <net/if_packet.h>
#include "includes/netinet/ip.h"
#include "includes/netinet/udp.h"
#include "includes/netinet/if_ether.h"
#endif

#if defined (USE_LPF_RECEIVE) || defined (USE_LPF_HWADDR)
#include <sys/ioctl.h>
#include <net/if.h>
#endif

#if defined (USE_LPF_SEND) || defined (USE_LPF_RECEIVE)
/* Reinitializes the specified interface after an address change.   This
   is not required for packet-filter APIs. */

#ifdef USE_LPF_SEND
void if_reinitialize_send (info)
	struct interface_info *info;
{
}
#endif

#ifdef USE_LPF_RECEIVE
void if_reinitialize_receive (info)
	struct interface_info *info;
{
}
#endif

/* Called by get_interface_list for each interface that's discovered.
   Opens a packet filter for each interface and adds it to the select
   mask. */

int if_register_lpf (info)
	struct interface_info *info;
{
	int sock;
	struct sockaddr sa;

	/* Make an LPF socket. */
	if ((sock = socket(PF_PACKET, SOCK_PACKET,
			   htons((short)ETH_P_ALL))) < 0) {
		if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT ||
		    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
		    errno == EAFNOSUPPORT || errno == EINVAL) {
			log_error ("socket: %m - make sure");
			log_error ("CONFIG_PACKET (Packet socket) %s",
				   "and CONFIG_FILTER");
			log_error ("(Socket Filtering) are enabled %s",
				   "in your kernel");
			log_fatal ("configuration!");
		}
		log_fatal ("Open a socket for LPF: %m");
	}

	/* Bind to the interface name */
	memset (&sa, 0, sizeof sa);
	sa.sa_family = AF_PACKET;
	strncpy (sa.sa_data, (const char *)info -> ifp, sizeof sa.sa_data);
	if (bind (sock, &sa, sizeof sa)) {
		if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT ||
		    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
		    errno == EAFNOSUPPORT || errno == EINVAL) {
			log_error ("socket: %m - make sure");
			log_error ("CONFIG_PACKET (Packet socket) %s",
				   "and CONFIG_FILTER");
			log_error ("(Socket Filtering) are enabled %s",
				   "in your kernel");
			log_fatal ("configuration!");
		}
		log_fatal ("Bind socket to interface: %m");
	}

	get_hw_addr(info->name, &info->hw_address);

	return sock;
}
#endif /* USE_LPF_SEND || USE_LPF_RECEIVE */

#ifdef USE_LPF_SEND
void if_register_send (info)
	struct interface_info *info;
{
	/* If we're using the lpf API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_LPF_RECEIVE
	info -> wfdesc = if_register_lpf (info);
#else
	info -> wfdesc = info -> rfdesc;
#endif
	if (!quiet_interface_discovery)
		log_info ("Sending on   LPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}

void if_deregister_send (info)
	struct interface_info *info;
{
	/* don't need to close twice if we are using lpf for sending and
	   receiving */
#ifndef USE_LPF_RECEIVE
	/* for LPF this is simple, packet filters are removed when sockets
	   are closed */
	close (info -> wfdesc);
#endif
	info -> wfdesc = -1;
	if (!quiet_interface_discovery)
		log_info ("Disabling output on LPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_LPF_SEND */

#ifdef USE_LPF_RECEIVE
/* Defined in bpf.c.   We can't extern these in dhcpd.h without pulling
   in bpf includes... */
extern struct sock_filter dhcp_bpf_filter [];
extern int dhcp_bpf_filter_len;

#if defined (HAVE_TR_SUPPORT)
extern struct sock_filter dhcp_bpf_tr_filter [];
extern int dhcp_bpf_tr_filter_len;
static void lpf_tr_filter_setup (struct interface_info *);
#endif

static void lpf_gen_filter_setup (struct interface_info *);

void if_register_receive (info)
	struct interface_info *info;
{
	/* Open a LPF device and hang it on this interface... */
	info -> rfdesc = if_register_lpf (info);

#if defined (HAVE_TR_SUPPORT)
	if (info -> hw_address.hbuf [0] == HTYPE_IEEE802)
		lpf_tr_filter_setup (info);
	else
#endif
		lpf_gen_filter_setup (info);

	if (!quiet_interface_discovery)
		log_info ("Listening on LPF/%s/%s%s%s",
			  info -> name,
			  print_hw_addr (info -> hw_address.hbuf [0],
					 info -> hw_address.hlen - 1,
					 &info -> hw_address.hbuf [1]),
			  (info -> shared_network ? "/" : ""),
			  (info -> shared_network ?
			   info -> shared_network -> name : ""));
}

void if_deregister_receive (info)
	struct interface_info *info;
{
	/* for LPF this is simple, packet filters are removed when sockets
	   are closed */
	close (info -> rfdesc);
	info -> rfdesc = -1;
	if (!quiet_interface_discovery)
		log_info ("Disabling input on LPF/%s/%s%s%s",
			  info -> name,
			  print_hw_addr (info -> hw_address.hbuf [0],
					 info -> hw_address.hlen - 1,
					 &info -> hw_address.hbuf [1]),
			  (info -> shared_network ? "/" : ""),
			  (info -> shared_network ?
			   info -> shared_network -> name : ""));
}

static void lpf_gen_filter_setup (info)
	struct interface_info *info;
{
	struct sock_fprog p;

	memset(&p, 0, sizeof(p));

	/* Set up the bpf filter program structure.    This is defined in
	   bpf.c */
	p.len = dhcp_bpf_filter_len;
	p.filter = dhcp_bpf_filter;

        /* Patch the server port into the LPF  program...
	   XXX changes to filter program may require changes
	   to the insn number(s) used below! XXX */
	dhcp_bpf_filter [8].k = ntohs ((short)local_port);

	if (setsockopt (info -> rfdesc, SOL_SOCKET, SO_ATTACH_FILTER, &p,
			sizeof p) < 0) {
		if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT ||
		    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
		    errno == EAFNOSUPPORT) {
			log_error ("socket: %m - make sure");
			log_error ("CONFIG_PACKET (Packet socket) %s",
				   "and CONFIG_FILTER");
			log_error ("(Socket Filtering) are enabled %s",
				   "in your kernel");
			log_fatal ("configuration!");
		}
		log_fatal ("Can't install packet filter program: %m");
	}
}

#if defined (HAVE_TR_SUPPORT)
static void lpf_tr_filter_setup (info)
	struct interface_info *info;
{
	struct sock_fprog p;

	memset(&p, 0, sizeof(p));

	/* Set up the bpf filter program structure.    This is defined in
	   bpf.c */
	p.len = dhcp_bpf_tr_filter_len;
	p.filter = dhcp_bpf_tr_filter;

        /* Patch the server port into the LPF  program...
	   XXX changes to filter program may require changes
	   XXX to the insn number(s) used below!
	   XXX Token ring filter is null - when/if we have a filter 
	   XXX that's not, we'll need this code.
	   XXX dhcp_bpf_filter [?].k = ntohs (local_port); */

	if (setsockopt (info -> rfdesc, SOL_SOCKET, SO_ATTACH_FILTER, &p,
			sizeof p) < 0) {
		if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT ||
		    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
		    errno == EAFNOSUPPORT) {
			log_error ("socket: %m - make sure");
			log_error ("CONFIG_PACKET (Packet socket) %s",
				   "and CONFIG_FILTER");
			log_error ("(Socket Filtering) are enabled %s",
				   "in your kernel");
			log_fatal ("configuration!");
		}
		log_fatal ("Can't install packet filter program: %m");
	}
}
#endif /* HAVE_TR_SUPPORT */
#endif /* USE_LPF_RECEIVE */

#ifdef USE_LPF_SEND
ssize_t send_packet (interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	unsigned hbufp = 0, ibufp = 0;
	double hh [16];
	double ih [1536 / sizeof (double)];
	unsigned char *buf = (unsigned char *)ih;
	struct sockaddr_pkt sa;
	int result;
	int fudge;

	if (!strcmp (interface -> name, "fallback"))
		return send_fallback (interface, packet, raw,
				      len, from, to, hto);

	if (hto == NULL && interface->anycast_mac_addr.hlen)
		hto = &interface->anycast_mac_addr;

	/* Assemble the headers... */
	assemble_hw_header (interface, (unsigned char *)hh, &hbufp, hto);
	fudge = hbufp % 4;	/* IP header must be word-aligned. */
	memcpy (buf + fudge, (unsigned char *)hh, hbufp);
	ibufp = hbufp + fudge;
	assemble_udp_ip_header (interface, buf, &ibufp, from.s_addr,
				to -> sin_addr.s_addr, to -> sin_port,
				(unsigned char *)raw, len);
	memcpy (buf + ibufp, raw, len);

	/* For some reason, SOCK_PACKET sockets can't be connected,
	   so we have to do a sentdo every time. */
	memset (&sa, 0, sizeof sa);
	sa.spkt_family = AF_PACKET;
	strncpy ((char *)sa.spkt_device,
		 (const char *)interface -> ifp, sizeof sa.spkt_device);
	sa.spkt_protocol = htons(ETH_P_IP);

	result = sendto (interface -> wfdesc,
			 buf + fudge, ibufp + len - fudge, 0, 
			 (const struct sockaddr *)&sa, sizeof sa);
	if (result < 0)
		log_error ("send_packet: %m");
	return result;
}
#endif /* USE_LPF_SEND */

#ifdef USE_LPF_RECEIVE
ssize_t receive_packet (interface, buf, len, from, hfrom)
	struct interface_info *interface;
	unsigned char *buf;
	size_t len;
	struct sockaddr_in *from;
	struct hardware *hfrom;
{
	int length = 0;
	int offset = 0;
	unsigned char ibuf [1536];
	unsigned bufix = 0;
	unsigned paylen;

	length = read (interface -> rfdesc, ibuf, sizeof ibuf);
	if (length <= 0)
		return length;

	bufix = 0;
	/* Decode the physical header... */
	offset = decode_hw_header (interface, ibuf, bufix, hfrom);

	/* If a physical layer checksum failed (dunno of any
	   physical layer that supports this, but WTH), skip this
	   packet. */
	if (offset < 0) {
		return 0;
	}

	bufix += offset;
	length -= offset;

	/* Decode the IP and UDP headers... */
	offset = decode_udp_ip_header (interface, ibuf, bufix, from,
				       (unsigned)length, &paylen);

	/* If the IP or UDP checksum was bad, skip the packet... */
	if (offset < 0)
		return 0;

	bufix += offset;
	length -= offset;

	if (length < paylen)
		log_fatal("Internal inconsistency at %s:%d.", MDL);

	/* Copy out the data in the packet... */
	memcpy(buf, &ibuf[bufix], paylen);
	return paylen;
}

int can_unicast_without_arp (ip)
	struct interface_info *ip;
{
	return 1;
}

int can_receive_unicast_unconfigured (ip)
	struct interface_info *ip;
{
	return 1;
}

int supports_multiple_interfaces (ip)
	struct interface_info *ip;
{
	return 1;
}

void maybe_setup_fallback ()
{
	isc_result_t status;
	struct interface_info *fbi = (struct interface_info *)0;
	if (setup_fallback (&fbi, MDL)) {
		if_register_fallback (fbi);
		status = omapi_register_io_object ((omapi_object_t *)fbi,
						   if_readsocket, 0,
						   fallback_discard, 0, 0);
		if (status != ISC_R_SUCCESS)
			log_fatal ("Can't register I/O handle for \"%s\": %s",
				   fbi -> name, isc_result_totext (status));
		interface_dereference (&fbi, MDL);
	}
}
#endif

#if defined (USE_LPF_RECEIVE) || defined (USE_LPF_HWADDR)
void
get_hw_addr(const char *name, struct hardware *hw) {
	int sock;
	struct ifreq tmp;
	struct sockaddr *sa;

	if (strlen(name) >= sizeof(tmp.ifr_name)) {
		log_fatal("Device name too long: \"%s\"", name);
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		log_fatal("Can't create socket for \"%s\": %m", name);
	}

	memset(&tmp, 0, sizeof(tmp));
	strcpy(tmp.ifr_name, name);
	if (ioctl(sock, SIOCGIFHWADDR, &tmp) < 0) {
		log_fatal("Error getting hardware address for \"%s\": %m", 
			  name);
	}

	sa = &tmp.ifr_hwaddr;
	switch (sa->sa_family) {
		case ARPHRD_ETHER:
			hw->hlen = 7;
			hw->hbuf[0] = HTYPE_ETHER;
			memcpy(&hw->hbuf[1], sa->sa_data, 6);
			break;
		case ARPHRD_IEEE802:
#ifdef ARPHRD_IEEE802_TR
		case ARPHRD_IEEE802_TR:
#endif /* ARPHRD_IEEE802_TR */
			hw->hlen = 7;
			hw->hbuf[0] = HTYPE_IEEE802;
			memcpy(&hw->hbuf[1], sa->sa_data, 6);
			break;
		case ARPHRD_FDDI:
			hw->hlen = 7;
			hw->hbuf[0] = HTYPE_FDDI;
			memcpy(&hw->hbuf[1], sa->sa_data, 6);
			break;
		default:
			log_fatal("Unsupported device type %ld for \"%s\"",
				  (long int)sa->sa_family, name);
	}

	close(sock);
}
#endif
