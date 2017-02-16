/*	$NetBSD: socket.c,v 1.1.1.3 2014/07/12 11:57:46 spz Exp $	*/
/* socket.c

   BSD socket interface code... */

/*
 * Copyright (c) 2004-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
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
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: socket.c,v 1.1.1.3 2014/07/12 11:57:46 spz Exp $");

/* SO_BINDTODEVICE support added by Elliot Poger (poger@leland.stanford.edu).
 * This sockopt allows a socket to be bound to a particular interface,
 * thus enabling the use of DHCPD on a multihomed host.
 * If SO_BINDTODEVICE is defined in your system header files, the use of
 * this sockopt will be automatically enabled. 
 * I have implemented it under Linux; other systems should be doable also.
 */

#include "dhcpd.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/uio.h>

#if defined(sun) && defined(USE_V4_PKTINFO)
#include <sys/sysmacros.h>
#include <net/if.h>
#include <sys/sockio.h>
#include <net/if_dl.h>
#include <sys/dlpi.h>
#endif

#ifdef USE_SOCKET_FALLBACK
# if !defined (USE_SOCKET_SEND)
#  define if_register_send if_register_fallback
#  define send_packet send_fallback
#  define if_reinitialize_send if_reinitialize_fallback
# endif
#endif

#if defined(DHCPv6)
/*
 * XXX: this is gross.  we need to go back and overhaul the API for socket
 * handling.
 */
static int no_global_v6_socket = 0;
static unsigned int global_v6_socket_references = 0;
static int global_v6_socket = -1;

static void if_register_multicast(struct interface_info *info);
#endif

/*
 * We can use a single socket for AF_INET (similar to AF_INET6) on all
 * interfaces configured for DHCP if the system has support for IP_PKTINFO
 * and IP_RECVPKTINFO (for example Solaris 11).
 */
#if defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && defined(USE_V4_PKTINFO)
static unsigned int global_v4_socket_references = 0;
static int global_v4_socket = -1;
#endif

/*
 * If we can't bind() to a specific interface, then we can only have
 * a single socket. This variable insures that we don't try to listen
 * on two sockets.
 */
#if !defined(SO_BINDTODEVICE) && !defined(USE_FALLBACK)
static int once = 0;
#endif /* !defined(SO_BINDTODEVICE) && !defined(USE_FALLBACK) */

/* Reinitializes the specified interface after an address change.   This
   is not required for packet-filter APIs. */

#if defined (USE_SOCKET_SEND) || defined (USE_SOCKET_FALLBACK)
void if_reinitialize_send (info)
	struct interface_info *info;
{
#if 0
#ifndef USE_SOCKET_RECEIVE
	once = 0;
	close (info -> wfdesc);
#endif
	if_register_send (info);
#endif
}
#endif

#ifdef USE_SOCKET_RECEIVE
void if_reinitialize_receive (info)
	struct interface_info *info;
{
#if 0
	once = 0;
	close (info -> rfdesc);
	if_register_receive (info);
#endif
}
#endif

#if defined (USE_SOCKET_SEND) || \
	defined (USE_SOCKET_RECEIVE) || \
		defined (USE_SOCKET_FALLBACK)
/* Generic interface registration routine... */
int
if_register_socket(struct interface_info *info, int family,
		   int *do_multicast, struct in6_addr *linklocal6)
{
	struct sockaddr_storage name;
	int name_len;
	int sock;
	int flag;
	int domain;
#ifdef DHCPv6
	struct sockaddr_in6 *addr6;
#endif
	struct sockaddr_in *addr;

	/* INSIST((family == AF_INET) || (family == AF_INET6)); */

#if !defined(SO_BINDTODEVICE) && !defined(USE_FALLBACK)
	/* Make sure only one interface is registered. */
	if (once) {
		log_fatal ("The standard socket API can only support %s",
		       "hosts with a single network interface.");
	}
	once = 1;
#endif

	/* 
	 * Set up the address we're going to bind to, depending on the
	 * address family. 
	 */ 
	memset(&name, 0, sizeof(name));
	switch (family) {
#ifdef DHCPv6
	case AF_INET6:
		addr6 = (struct sockaddr_in6 *)&name; 
		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = local_port;
		if (linklocal6) {
			memcpy(&addr6->sin6_addr,
			       linklocal6,
			       sizeof(addr6->sin6_addr));
			addr6->sin6_scope_id = if_nametoindex(info->name);
		}
#ifdef HAVE_SA_LEN
		addr6->sin6_len = sizeof(*addr6);
#endif
		name_len = sizeof(*addr6);
		domain = PF_INET6;
		if ((info->flags & INTERFACE_STREAMS) == INTERFACE_UPSTREAM) {
			*do_multicast = 0;
		}
		break;
#endif /* DHCPv6 */

	case AF_INET:
	default:
		addr = (struct sockaddr_in *)&name; 
		addr->sin_family = AF_INET;
		addr->sin_port = local_port;
		memcpy(&addr->sin_addr,
		       &local_address,
		       sizeof(addr->sin_addr));
#ifdef HAVE_SA_LEN
		addr->sin_len = sizeof(*addr);
#endif
		name_len = sizeof(*addr);
		domain = PF_INET;
		break;
	}

	/* Make a socket... */
	sock = socket(domain, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		log_fatal("Can't create dhcp socket: %m");
	}

	/* Set the REUSEADDR option so that we don't fail to start if
	   we're being restarted. */
	flag = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			(char *)&flag, sizeof(flag)) < 0) {
		log_fatal("Can't set SO_REUSEADDR option on dhcp socket: %m");
	}

	/* Set the BROADCAST option so that we can broadcast DHCP responses.
	   We shouldn't do this for fallback devices, and we can detect that
	   a device is a fallback because it has no ifp structure. */
	if (info->ifp &&
	    (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
			 (char *)&flag, sizeof(flag)) < 0)) {
		log_fatal("Can't set SO_BROADCAST option on dhcp socket: %m");
	}

#if defined(DHCPv6) && defined(SO_REUSEPORT)
	/*
	 * We only set SO_REUSEPORT on AF_INET6 sockets, so that multiple
	 * daemons can bind to their own sockets and get data for their
	 * respective interfaces.  This does not (and should not) affect
	 * DHCPv4 sockets; we can't yet support BSD sockets well, much
	 * less multiple sockets. Make sense only with multicast.
	 */
	if ((local_family == AF_INET6) && *do_multicast) {
		flag = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
			       (char *)&flag, sizeof(flag)) < 0) {
			log_fatal("Can't set SO_REUSEPORT option on dhcp "
				  "socket: %m");
		}
	}
#endif

	/* Bind the socket to this interface's IP address. */
	if (bind(sock, (struct sockaddr *)&name, name_len) < 0) {
		log_error("Can't bind to dhcp address: %m");
		log_error("Please make sure there is no other dhcp server");
		log_error("running and that there's no entry for dhcp or");
		log_error("bootp in /etc/inetd.conf.   Also make sure you");
		log_error("are not running HP JetAdmin software, which");
		log_fatal("includes a bootp server.");
	}

#if defined(SO_BINDTODEVICE)
	/* Bind this socket to this interface. */
	if ((local_family != AF_INET6) && (info->ifp != NULL) &&
	    setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
			(char *)(info -> ifp), sizeof(*(info -> ifp))) < 0) {
		log_fatal("setsockopt: SO_BINDTODEVICE: %m");
	}
#endif

	/* IP_BROADCAST_IF instructs the kernel which interface to send
	 * IP packets whose destination address is 255.255.255.255.  These
	 * will be treated as subnet broadcasts on the interface identified
	 * by ip address (info -> primary_address).  This is only known to
	 * be defined in SCO system headers, and may not be defined in all
	 * releases.
	 */
#if defined(SCO) && defined(IP_BROADCAST_IF)
        if (info->address_count &&
	    setsockopt(sock, IPPROTO_IP, IP_BROADCAST_IF, &info->addresses[0],
		       sizeof(info->addresses[0])) < 0)
		log_fatal("Can't set IP_BROADCAST_IF on dhcp socket: %m");
#endif

#if defined(IP_PKTINFO) && defined(IP_RECVPKTINFO)  && defined(USE_V4_PKTINFO)
	/*
	 * If we turn on IP_RECVPKTINFO we will be able to receive
	 * the interface index information of the received packet.
	 */
	if (family == AF_INET) {
		int on = 1;
		if (setsockopt(sock, IPPROTO_IP, IP_RECVPKTINFO, 
		               &on, sizeof(on)) != 0) {
			log_fatal("setsockopt: IPV_RECVPKTINFO: %m");
		}
	}
#endif

#ifdef DHCPv6
	/*
	 * If we turn on IPV6_PKTINFO, we will be able to receive 
	 * additional information, such as the destination IP address.
	 * We need this to spot unicast packets.
	 */
	if (family == AF_INET6) {
		int on = 1;
#ifdef IPV6_RECVPKTINFO
		/* RFC3542 */
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, 
		               &on, sizeof(on)) != 0) {
			log_fatal("setsockopt: IPV6_RECVPKTINFO: %m");
		}
#else
		/* RFC2292 */
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO, 
		               &on, sizeof(on)) != 0) {
			log_fatal("setsockopt: IPV6_PKTINFO: %m");
		}
#endif
	}

	if ((family == AF_INET6) &&
	    ((info->flags & INTERFACE_UPSTREAM) != 0)) {
		int hop_limit = 32;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
			       &hop_limit, sizeof(int)) < 0) {
			log_fatal("setsockopt: IPV6_MULTICAST_HOPS: %m");
		}
	}
#endif /* DHCPv6 */

	return sock;
}
#endif /* USE_SOCKET_SEND || USE_SOCKET_RECEIVE || USE_SOCKET_FALLBACK */

#if defined (USE_SOCKET_SEND) || defined (USE_SOCKET_FALLBACK)
void if_register_send (info)
	struct interface_info *info;
{
#ifndef USE_SOCKET_RECEIVE
	info->wfdesc = if_register_socket(info, AF_INET, 0, NULL);
	/* If this is a normal IPv4 address, get the hardware address. */
	if (strcmp(info->name, "fallback") != 0)
		get_hw_addr(info->name, &info->hw_address);
#if defined (USE_SOCKET_FALLBACK)
	/* Fallback only registers for send, but may need to receive as
	   well. */
	info->rfdesc = info->wfdesc;
#endif
#else
	info->wfdesc = info->rfdesc;
#endif
	if (!quiet_interface_discovery)
		log_info ("Sending on   Socket/%s%s%s",
		      info->name,
		      (info->shared_network ? "/" : ""),
		      (info->shared_network ?
		       info->shared_network->name : ""));
}

#if defined (USE_SOCKET_SEND)
void if_deregister_send (info)
	struct interface_info *info;
{
#ifndef USE_SOCKET_RECEIVE
	close (info -> wfdesc);
#endif
	info -> wfdesc = -1;

	if (!quiet_interface_discovery)
		log_info ("Disabling output on Socket/%s%s%s",
		      info -> name,
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_SOCKET_SEND */
#endif /* USE_SOCKET_SEND || USE_SOCKET_FALLBACK */

#ifdef USE_SOCKET_RECEIVE
void if_register_receive (info)
	struct interface_info *info;
{

#if defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && defined(USE_V4_PKTINFO)
	if (global_v4_socket_references == 0) {
		global_v4_socket = if_register_socket(info, AF_INET, 0, NULL);
		if (global_v4_socket < 0) {
			/*
			 * if_register_socket() fatally logs if it fails to
			 * create a socket, this is just a sanity check.
			 */
			log_fatal("Failed to create AF_INET socket %s:%d",
				  MDL);
		}
	}
		
	info->rfdesc = global_v4_socket;
	global_v4_socket_references++;
#else
	/* If we're using the socket API for sending and receiving,
	   we don't need to register this interface twice. */
	info->rfdesc = if_register_socket(info, AF_INET, 0, NULL);
#endif /* IP_PKTINFO... */
	/* If this is a normal IPv4 address, get the hardware address. */
	if (strcmp(info->name, "fallback") != 0)
		get_hw_addr(info->name, &info->hw_address);

	if (!quiet_interface_discovery)
		log_info ("Listening on Socket/%s%s%s",
		      info->name,
		      (info->shared_network ? "/" : ""),
		      (info->shared_network ?
		       info->shared_network->name : ""));
}

void if_deregister_receive (info)
	struct interface_info *info;
{
#if defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && defined(USE_V4_PKTINFO)
	/* Dereference the global v4 socket. */
	if ((info->rfdesc == global_v4_socket) &&
	    (info->wfdesc == global_v4_socket) &&
	    (global_v4_socket_references > 0)) {
		global_v4_socket_references--;
		info->rfdesc = -1;
	} else {
		log_fatal("Impossible condition at %s:%d", MDL);
	}

	if (global_v4_socket_references == 0) {
		close(global_v4_socket);
		global_v4_socket = -1;
	}
#else
	close(info->rfdesc);
	info->rfdesc = -1;
#endif /* IP_PKTINFO... */
	if (!quiet_interface_discovery)
		log_info ("Disabling input on Socket/%s%s%s",
		      info -> name,
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_SOCKET_RECEIVE */


#ifdef DHCPv6 
/*
 * This function joins the interface to DHCPv6 multicast groups so we will
 * receive multicast messages.
 */
static void
if_register_multicast(struct interface_info *info) {
	int sock = info->rfdesc;
	struct ipv6_mreq mreq;

	if (inet_pton(AF_INET6, All_DHCP_Relay_Agents_and_Servers,
		      &mreq.ipv6mr_multiaddr) <= 0) {
		log_fatal("inet_pton: unable to convert '%s'", 
			  All_DHCP_Relay_Agents_and_Servers);
	}
	mreq.ipv6mr_interface = if_nametoindex(info->name);
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, 
		       &mreq, sizeof(mreq)) < 0) {
		log_fatal("setsockopt: IPV6_JOIN_GROUP: %m");
	}

	/*
	 * The relay agent code sets the streams so you know which way
	 * is up and down.  But a relay agent shouldn't join to the
	 * Server address, or else you get fun loops.  So up or down
	 * doesn't matter, we're just using that config to sense this is
	 * a relay agent.
	 */
	if ((info->flags & INTERFACE_STREAMS) == 0) {
		if (inet_pton(AF_INET6, All_DHCP_Servers,
			      &mreq.ipv6mr_multiaddr) <= 0) {
			log_fatal("inet_pton: unable to convert '%s'", 
				  All_DHCP_Servers);
		}
		mreq.ipv6mr_interface = if_nametoindex(info->name);
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, 
			       &mreq, sizeof(mreq)) < 0) {
			log_fatal("setsockopt: IPV6_JOIN_GROUP: %m");
		}
	}
}

void
if_register6(struct interface_info *info, int do_multicast) {
	/* Bounce do_multicast to a stack variable because we may change it. */
	int req_multi = do_multicast;

	if (no_global_v6_socket) {
		log_fatal("Impossible condition at %s:%d", MDL);
	}

	if (global_v6_socket_references == 0) {
		global_v6_socket = if_register_socket(info, AF_INET6,
						      &req_multi, NULL);
		if (global_v6_socket < 0) {
			/*
			 * if_register_socket() fatally logs if it fails to
			 * create a socket, this is just a sanity check.
			 */
			log_fatal("Impossible condition at %s:%d", MDL);
		} else {
			log_info("Bound to *:%d", ntohs(local_port));
		}
	}
		
	info->rfdesc = global_v6_socket;
	info->wfdesc = global_v6_socket;
	global_v6_socket_references++;

	if (req_multi)
		if_register_multicast(info);

	get_hw_addr(info->name, &info->hw_address);

	if (!quiet_interface_discovery) {
		if (info->shared_network != NULL) {
			log_info("Listening on Socket/%d/%s/%s",
				 global_v6_socket, info->name, 
				 info->shared_network->name);
			log_info("Sending on   Socket/%d/%s/%s",
				 global_v6_socket, info->name,
				 info->shared_network->name);
		} else {
			log_info("Listening on Socket/%s", info->name);
			log_info("Sending on   Socket/%s", info->name);
		}
	}
}

/*
 * Register an IPv6 socket bound to the link-local address of
 * the argument interface (used by clients on a multiple interface box,
 * vs. a server or a relay using the global IPv6 socket and running
 * *only* in a single instance).
 */
void
if_register_linklocal6(struct interface_info *info) {
	int sock;
	int count;
	struct in6_addr *addr6 = NULL;
	int req_multi = 0;

	if (global_v6_socket >= 0) {
		log_fatal("Impossible condition at %s:%d", MDL);
	}
		
	no_global_v6_socket = 1;

	/* get the (?) link-local address */
	for (count = 0; count < info->v6address_count; count++) {
		addr6 = &info->v6addresses[count];
		if (IN6_IS_ADDR_LINKLOCAL(addr6))
			break;
	}

	if (!addr6) {
		log_fatal("no link-local IPv6 address for %s", info->name);
	}

	sock = if_register_socket(info, AF_INET6, &req_multi, addr6);

	if (sock < 0) {
		log_fatal("if_register_socket for %s fails", info->name);
	}

	info->rfdesc = sock;
	info->wfdesc = sock;

	get_hw_addr(info->name, &info->hw_address);

	if (!quiet_interface_discovery) {
		if (info->shared_network != NULL) {
			log_info("Listening on Socket/%d/%s/%s",
				 global_v6_socket, info->name, 
				 info->shared_network->name);
			log_info("Sending on   Socket/%d/%s/%s",
				 global_v6_socket, info->name,
				 info->shared_network->name);
		} else {
			log_info("Listening on Socket/%s", info->name);
			log_info("Sending on   Socket/%s", info->name);
		}
	}
}

void 
if_deregister6(struct interface_info *info) {
	/* client case */
	if (no_global_v6_socket) {
		close(info->rfdesc);
		info->rfdesc = -1;
		info->wfdesc = -1;
	} else if ((info->rfdesc == global_v6_socket) &&
		   (info->wfdesc == global_v6_socket) &&
		   (global_v6_socket_references > 0)) {
		/* Dereference the global v6 socket. */
		global_v6_socket_references--;
		info->rfdesc = -1;
		info->wfdesc = -1;
	} else {
		log_fatal("Impossible condition at %s:%d", MDL);
	}

	if (!quiet_interface_discovery) {
		if (info->shared_network != NULL) {
			log_info("Disabling input on  Socket/%s/%s", info->name,
		       		 info->shared_network->name);
			log_info("Disabling output on Socket/%s/%s", info->name,
		       		 info->shared_network->name);
		} else {
			log_info("Disabling input on  Socket/%s", info->name);
			log_info("Disabling output on Socket/%s", info->name);
		}
	}

	if (!no_global_v6_socket &&
	    (global_v6_socket_references == 0)) {
		close(global_v6_socket);
		global_v6_socket = -1;

		log_info("Unbound from *:%d", ntohs(local_port));
	}
}
#endif /* DHCPv6 */

#if defined (USE_SOCKET_SEND) || defined (USE_SOCKET_FALLBACK)
ssize_t send_packet (interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	int result;
#ifdef IGNORE_HOSTUNREACH
	int retry = 0;
	do {
#endif
#if defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && defined(USE_V4_PKTINFO)
		struct in_pktinfo pktinfo;

		if (interface->ifp != NULL) {
			memset(&pktinfo, 0, sizeof (pktinfo));
			pktinfo.ipi_ifindex = interface->ifp->ifr_index;
			if (setsockopt(interface->wfdesc, IPPROTO_IP,
				       IP_PKTINFO, (char *)&pktinfo,
				       sizeof(pktinfo)) < 0) 
				log_fatal("setsockopt: IP_PKTINFO: %m");
		}
#endif
		result = sendto (interface -> wfdesc, (char *)raw, len, 0,
				 (struct sockaddr *)to, sizeof *to);
#ifdef IGNORE_HOSTUNREACH
	} while (to -> sin_addr.s_addr == htonl (INADDR_BROADCAST) &&
		 result < 0 &&
		 (errno == EHOSTUNREACH ||
		  errno == ECONNREFUSED) &&
		 retry++ < 10);
#endif
	if (result < 0) {
		log_error ("send_packet: %m");
		if (errno == ENETUNREACH)
			log_error ("send_packet: please consult README file%s",
				   " regarding broadcast address.");
	}
	return result;
}

#endif /* USE_SOCKET_SEND || USE_SOCKET_FALLBACK */

#ifdef DHCPv6
/*
 * Solaris 9 is missing the CMSG_LEN and CMSG_SPACE macros, so we will 
 * synthesize them (based on the BIND 9 technique).
 */

#ifndef CMSG_LEN
static size_t CMSG_LEN(size_t len) {
	size_t hdrlen;
	/*
	 * Cast NULL so that any pointer arithmetic performed by CMSG_DATA
	 * is correct.
	 */
	hdrlen = (size_t)CMSG_DATA(((struct cmsghdr *)NULL));
	return hdrlen + len;
}
#endif /* !CMSG_LEN */

#ifndef CMSG_SPACE
static size_t CMSG_SPACE(size_t len) {
	struct msghdr msg;
	struct cmsghdr *cmsgp;

	/*
	 * XXX: The buffer length is an ad-hoc value, but should be enough
	 * in a practical sense.
	 */
	union {
		struct cmsghdr cmsg_sizer;
		u_int8_t pktinfo_sizer[sizeof(struct cmsghdr) + 1024];
	} dummybuf;

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = &dummybuf;
	msg.msg_controllen = sizeof(dummybuf);

	cmsgp = (struct cmsghdr *)&dummybuf;
	cmsgp->cmsg_len = CMSG_LEN(len);

	cmsgp = CMSG_NXTHDR(&msg, cmsgp);
	if (cmsgp != NULL) {
		return (char *)cmsgp - (char *)msg.msg_control;
	} else {
		return 0;
	}
}
#endif /* !CMSG_SPACE */

#endif /* DHCPv6 */

#if defined(DHCPv6) || \
	(defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && \
	 defined(USE_V4_PKTINFO))
/*
 * For both send_packet6() and receive_packet6() we need to allocate
 * space for the cmsg header information.  We do this once and reuse
 * the buffer.  We also need the control buf for send_packet() and
 * receive_packet() when we use a single socket and IP_PKTINFO to
 * send the packet out the correct interface.
 */
static void   *control_buf = NULL;
static size_t  control_buf_len = 0;

static void
allocate_cmsg_cbuf(void) {
	control_buf_len = CMSG_SPACE(sizeof(struct in6_pktinfo));
	control_buf = dmalloc(control_buf_len, MDL);
	return;
}
#endif /* DHCPv6, IP_PKTINFO ... */

#ifdef DHCPv6
/* 
 * For both send_packet6() and receive_packet6() we need to use the 
 * sendmsg()/recvmsg() functions rather than the simpler send()/recv()
 * functions.
 *
 * In the case of send_packet6(), we need to do this in order to insure
 * that the reply packet leaves on the same interface that it arrived 
 * on. 
 *
 * In the case of receive_packet6(), we need to do this in order to 
 * get the IP address the packet was sent to. This is used to identify
 * whether a packet is multicast or unicast.
 *
 * Helpful man pages: recvmsg, readv (talks about the iovec stuff), cmsg.
 *
 * Also see the sections in RFC 3542 about IPV6_PKTINFO.
 */

/* Send an IPv6 packet */
ssize_t send_packet6(struct interface_info *interface,
		     const unsigned char *raw, size_t len,
		     struct sockaddr_in6 *to) {
	struct msghdr m;
	struct iovec v;
	struct sockaddr_in6 dst;
	int result;
	struct in6_pktinfo *pktinfo;
	struct cmsghdr *cmsg;
	unsigned int ifindex;

	/*
	 * If necessary allocate space for the control message header.
	 * The space is common between send and receive.
	 */

	if (control_buf == NULL) {
		allocate_cmsg_cbuf();
		if (control_buf == NULL) {
			log_error("send_packet6: unable to allocate cmsg header");
			return(ENOMEM);
		}
	}
	memset(control_buf, 0, control_buf_len);

	/*
	 * Initialize our message header structure.
	 */
	memset(&m, 0, sizeof(m));

	/*
	 * Set the target address we're sending to.
	 * Enforce the scope ID for bogus BSDs.
	 */
	memcpy(&dst, to, sizeof(dst));
	m.msg_name = &dst;
	m.msg_namelen = sizeof(dst);
	ifindex = if_nametoindex(interface->name);
	if (no_global_v6_socket)
		dst.sin6_scope_id = ifindex;

	/*
	 * Set the data buffer we're sending. (Using this wacky 
	 * "scatter-gather" stuff... we only have a single chunk 
	 * of data to send, so we declare a single vector entry.)
	 */
	v.iov_base = (char *)raw;
	v.iov_len = len;
	m.msg_iov = &v;
	m.msg_iovlen = 1;

	/*
	 * Setting the interface is a bit more involved.
	 * 
	 * We have to create a "control message", and set that to 
	 * define the IPv6 packet information. We could set the
	 * source address if we wanted, but we can safely let the
	 * kernel decide what that should be. 
	 */
	m.msg_control = control_buf;
	m.msg_controllen = control_buf_len;
	cmsg = CMSG_FIRSTHDR(&m);
	INSIST(cmsg != NULL);
	cmsg->cmsg_level = IPPROTO_IPV6;
	cmsg->cmsg_type = IPV6_PKTINFO;
	cmsg->cmsg_len = CMSG_LEN(sizeof(*pktinfo));
	pktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsg);
	memset(pktinfo, 0, sizeof(*pktinfo));
	pktinfo->ipi6_ifindex = ifindex;
	m.msg_controllen = cmsg->cmsg_len;

	result = sendmsg(interface->wfdesc, &m, 0);
	if (result < 0) {
		log_error("send_packet6: %m");
	}
	return result;
}
#endif /* DHCPv6 */

#ifdef USE_SOCKET_RECEIVE
ssize_t receive_packet (interface, buf, len, from, hfrom)
	struct interface_info *interface;
	unsigned char *buf;
	size_t len;
	struct sockaddr_in *from;
	struct hardware *hfrom;
{
#if !(defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && defined(USE_V4_PKTINFO))
	SOCKLEN_T flen = sizeof *from;
#endif
	int result;

	/*
	 * The normal Berkeley socket interface doesn't give us any way
	 * to know what hardware interface we received the message on,
	 * but we should at least make sure the structure is emptied.
	 */
	memset(hfrom, 0, sizeof(*hfrom));

#ifdef IGNORE_HOSTUNREACH
	int retry = 0;
	do {
#endif

#if defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && defined(USE_V4_PKTINFO)
	struct msghdr m;
	struct iovec v;
	struct cmsghdr *cmsg;
	struct in_pktinfo *pktinfo;
	unsigned int ifindex;

	/*
	 * If necessary allocate space for the control message header.
	 * The space is common between send and receive.
	 */
	if (control_buf == NULL) {
		allocate_cmsg_cbuf();
		if (control_buf == NULL) {
			log_error("receive_packet: unable to allocate cmsg "
				  "header");
			return(ENOMEM);
		}
	}
	memset(control_buf, 0, control_buf_len);

	/*
	 * Initialize our message header structure.
	 */
	memset(&m, 0, sizeof(m));

	/*
	 * Point so we can get the from address.
	 */
	m.msg_name = from;
	m.msg_namelen = sizeof(*from);

	/*
	 * Set the data buffer we're receiving. (Using this wacky 
	 * "scatter-gather" stuff... but we that doesn't really make
	 * sense for us, so we use a single vector entry.)
	 */
	v.iov_base = buf;
	v.iov_len = len;
	m.msg_iov = &v;
	m.msg_iovlen = 1;

	/*
	 * Getting the interface is a bit more involved.
	 *
	 * We set up some space for a "control message". We have 
	 * previously asked the kernel to give us packet 
	 * information (when we initialized the interface), so we
	 * should get the interface index from that.
	 */
	m.msg_control = control_buf;
	m.msg_controllen = control_buf_len;

	result = recvmsg(interface->rfdesc, &m, 0);

	if (result >= 0) {
		/*
		 * If we did read successfully, then we need to loop
		 * through the control messages we received and 
		 * find the one with our inteface index.
		 */
		cmsg = CMSG_FIRSTHDR(&m);
		while (cmsg != NULL) {
			if ((cmsg->cmsg_level == IPPROTO_IP) && 
			    (cmsg->cmsg_type == IP_PKTINFO)) {
				pktinfo = (struct in_pktinfo *)CMSG_DATA(cmsg);
				ifindex = pktinfo->ipi_ifindex;
				/*
				 * We pass the ifindex back to the caller 
				 * using the unused hfrom parameter avoiding
				 * interface changes between sockets and 
				 * the discover code.
				 */
				memcpy(hfrom->hbuf, &ifindex, sizeof(ifindex));
				return (result);
			}
			cmsg = CMSG_NXTHDR(&m, cmsg);
		}

		/*
		 * We didn't find the necessary control message
		 * flag it as an error
		 */
		result = -1;
		errno = EIO;
	}
#else
		result = recvfrom(interface -> rfdesc, (char *)buf, len, 0,
				  (struct sockaddr *)from, &flen);
#endif /* IP_PKTINFO ... */
#ifdef IGNORE_HOSTUNREACH
	} while (result < 0 &&
		 (errno == EHOSTUNREACH ||
		  errno == ECONNREFUSED) &&
		 retry++ < 10);
#endif
	return (result);
}

#endif /* USE_SOCKET_RECEIVE */

#ifdef DHCPv6
ssize_t 
receive_packet6(struct interface_info *interface, 
		unsigned char *buf, size_t len, 
		struct sockaddr_in6 *from, struct in6_addr *to_addr,
		unsigned int *if_idx)
{
	struct msghdr m;
	struct iovec v;
	int result;
	struct cmsghdr *cmsg;
	struct in6_pktinfo *pktinfo;

	/*
	 * If necessary allocate space for the control message header.
	 * The space is common between send and receive.
	 */
	if (control_buf == NULL) {
		allocate_cmsg_cbuf();
		if (control_buf == NULL) {
			log_error("receive_packet6: unable to allocate cmsg "
				  "header");
			return(ENOMEM);
		}
	}
	memset(control_buf, 0, control_buf_len);

	/*
	 * Initialize our message header structure.
	 */
	memset(&m, 0, sizeof(m));

	/*
	 * Point so we can get the from address.
	 */
	m.msg_name = from;
	m.msg_namelen = sizeof(*from);

	/*
	 * Set the data buffer we're receiving. (Using this wacky 
	 * "scatter-gather" stuff... but we that doesn't really make
	 * sense for us, so we use a single vector entry.)
	 */
	v.iov_base = buf;
	v.iov_len = len;
	m.msg_iov = &v;
	m.msg_iovlen = 1;

	/*
	 * Getting the interface is a bit more involved.
	 *
	 * We set up some space for a "control message". We have 
	 * previously asked the kernel to give us packet 
	 * information (when we initialized the interface), so we
	 * should get the destination address from that.
	 */
	m.msg_control = control_buf;
	m.msg_controllen = control_buf_len;

	result = recvmsg(interface->rfdesc, &m, 0);

	if (result >= 0) {
		/*
		 * If we did read successfully, then we need to loop
		 * through the control messages we received and 
		 * find the one with our destination address.
		 */
		cmsg = CMSG_FIRSTHDR(&m);
		while (cmsg != NULL) {
			if ((cmsg->cmsg_level == IPPROTO_IPV6) && 
			    (cmsg->cmsg_type == IPV6_PKTINFO)) {
				pktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsg);
				*to_addr = pktinfo->ipi6_addr;
				*if_idx = pktinfo->ipi6_ifindex;

				return (result);
			}
			cmsg = CMSG_NXTHDR(&m, cmsg);
		}

		/*
		 * We didn't find the necessary control message
		 * flag is as an error
		 */
		result = -1;
		errno = EIO;
	}

	return (result);
}
#endif /* DHCPv6 */

#if defined (USE_SOCKET_FALLBACK)
/* This just reads in a packet and silently discards it. */

isc_result_t fallback_discard (object)
	omapi_object_t *object;
{
	char buf [1540];
	struct sockaddr_in from;
	SOCKLEN_T flen = sizeof from;
	int status;
	struct interface_info *interface;

	if (object -> type != dhcp_type_interface)
		return DHCP_R_INVALIDARG;
	interface = (struct interface_info *)object;

	status = recvfrom (interface -> wfdesc, buf, sizeof buf, 0,
			   (struct sockaddr *)&from, &flen);
#if defined (DEBUG)
	/* Only report fallback discard errors if we're debugging. */
	if (status < 0) {
		log_error ("fallback_discard: %m");
		return ISC_R_UNEXPECTED;
	}
#else
        /* ignore the fact that status value is never used */
        IGNORE_UNUSED(status);
#endif
	return ISC_R_SUCCESS;
}
#endif /* USE_SOCKET_FALLBACK */

#if defined (USE_SOCKET_SEND)
int can_unicast_without_arp (ip)
	struct interface_info *ip;
{
	return 0;
}

int can_receive_unicast_unconfigured (ip)
	struct interface_info *ip;
{
#if defined (SOCKET_CAN_RECEIVE_UNICAST_UNCONFIGURED)
	return 1;
#else
	return 0;
#endif
}

int supports_multiple_interfaces (ip)
	struct interface_info *ip;
{
#if defined(SO_BINDTODEVICE) || \
	(defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && \
	 defined(USE_V4_PKTINFO))
	return(1);
#else
	return(0);
#endif
}

/* If we have SO_BINDTODEVICE, set up a fallback interface; otherwise,
   do not. */

void maybe_setup_fallback ()
{
#if defined (USE_SOCKET_FALLBACK)
	isc_result_t status;
	struct interface_info *fbi = (struct interface_info *)0;
	if (setup_fallback (&fbi, MDL)) {
		fbi -> wfdesc = if_register_socket (fbi, AF_INET, 0, NULL);
		fbi -> rfdesc = fbi -> wfdesc;
		log_info ("Sending on   Socket/%s%s%s",
		      fbi -> name,
		      (fbi -> shared_network ? "/" : ""),
		      (fbi -> shared_network ?
		       fbi -> shared_network -> name : ""));
	
		status = omapi_register_io_object ((omapi_object_t *)fbi,
						   if_readsocket, 0,
						   fallback_discard, 0, 0);
		if (status != ISC_R_SUCCESS)
			log_fatal ("Can't register I/O handle for %s: %s",
				   fbi -> name, isc_result_totext (status));
		interface_dereference (&fbi, MDL);
	}
#endif
}


#if defined(sun) && defined(USE_V4_PKTINFO)
/* This code assumes the existence of SIOCGLIFHWADDR */
void
get_hw_addr(const char *name, struct hardware *hw) {
	struct sockaddr_dl *dladdrp;
	int sock, i;
	struct lifreq lifr;

	memset(&lifr, 0, sizeof (lifr));
	(void) strlcpy(lifr.lifr_name, name, sizeof (lifr.lifr_name));
	/*
	 * Check if the interface is a virtual or IPMP interface - in those
	 * cases it has no hw address, so generate a random one.
	 */
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ||
	    ioctl(sock, SIOCGLIFFLAGS, &lifr) < 0) {
		if (sock != -1)
			(void) close(sock);

#ifdef DHCPv6
		/*
		 * If approrpriate try this with an IPv6 socket
		 */
		if ((sock = socket(AF_INET6, SOCK_DGRAM, 0)) >= 0 &&
		    ioctl(sock, SIOCGLIFFLAGS, &lifr) >= 0) {
			goto flag_check;
		}
		if (sock != -1)
			(void) close(sock);
#endif
		log_fatal("Couldn't get interface flags for %s: %m", name);

	}

 flag_check:
	if (lifr.lifr_flags & (IFF_VIRTUAL|IFF_IPMP)) {
		hw->hlen = sizeof (hw->hbuf);
		srandom((long)gethrtime());

		hw->hbuf[0] = HTYPE_IPMP;
		for (i = 1; i < hw->hlen; ++i) {
			hw->hbuf[i] = random() % 256;
		}

		if (sock != -1)
			(void) close(sock);
		return;
	}

	if (ioctl(sock, SIOCGLIFHWADDR, &lifr) < 0)
		log_fatal("Couldn't get interface hardware address for %s: %m",
			  name);
	dladdrp = (struct sockaddr_dl *)&lifr.lifr_addr;
	hw->hlen = dladdrp->sdl_alen+1;
	switch (dladdrp->sdl_type) {
		case DL_CSMACD: /* IEEE 802.3 */
		case DL_ETHER:
			hw->hbuf[0] = HTYPE_ETHER;
			break;
		case DL_TPR:
			hw->hbuf[0] = HTYPE_IEEE802;
			break;
		case DL_FDDI:
			hw->hbuf[0] = HTYPE_FDDI;
			break;
		case DL_IB:
			hw->hbuf[0] = HTYPE_INFINIBAND;
			break;
		default:
			log_fatal("%s: unsupported DLPI MAC type %lu", name,
				  (unsigned long)dladdrp->sdl_type);
	}

	memcpy(hw->hbuf+1, LLADDR(dladdrp), hw->hlen-1);

	if (sock != -1)
		(void) close(sock);
}
#endif /* defined(sun) */

#endif /* USE_SOCKET_SEND */
