/* dhcp.h

   Protocol structures... */

/*
 * Copyright (c) 2004-2009 by Internet Systems Consortium, Inc. ("ISC")
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
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises.  To learn more
 * about Internet Systems Consortium, see ``https://www.isc.org''.
 * To learn more about Vixie Enterprises, see ``http://www.vix.com''.
 */

#ifndef DHCP_H
#define DHCP_H

#define DHCP_UDP_OVERHEAD	(20 + /* IP header */			\
			        8)   /* UDP header */
#define DHCP_SNAME_LEN		64
#define DHCP_FILE_LEN		128
#define DHCP_FIXED_NON_UDP	236
#define DHCP_FIXED_LEN		(DHCP_FIXED_NON_UDP + DHCP_UDP_OVERHEAD)
						/* Everything but options. */
#define BOOTP_MIN_LEN		300

#define DHCP_MTU_MAX		1500
#define DHCP_MTU_MIN            576

#define DHCP_MAX_OPTION_LEN	(DHCP_MTU_MAX - DHCP_FIXED_LEN)
#define DHCP_MIN_OPTION_LEN     (DHCP_MTU_MIN - DHCP_FIXED_LEN)

struct dhcp_packet {
 u_int8_t  op;		/* 0: Message opcode/type */
	u_int8_t  htype;	/* 1: Hardware addr type (net/if_types.h) */
	u_int8_t  hlen;		/* 2: Hardware addr length */
	u_int8_t  hops;		/* 3: Number of relay agent hops from client */
	u_int32_t xid;		/* 4: Transaction ID */
	u_int16_t secs;		/* 8: Seconds since client started looking */
	u_int16_t flags;	/* 10: Flag bits */
	struct in_addr ciaddr;	/* 12: Client IP address (if already in use) */
	struct in_addr yiaddr;	/* 16: Client IP address */
	struct in_addr siaddr;	/* 18: IP address of next server to talk to */
	struct in_addr giaddr;	/* 20: DHCP relay agent IP address */
	unsigned char chaddr [16];	/* 24: Client hardware address */
	char sname [DHCP_SNAME_LEN];	/* 40: Server name */
	char file [DHCP_FILE_LEN];	/* 104: Boot filename */
	unsigned char options [DHCP_MAX_OPTION_LEN];
				/* 212: Optional parameters
			  (actual length dependent on MTU). */
};

/* BOOTP (rfc951) message types */
#define	BOOTREQUEST	1
#define BOOTREPLY	2

/* Possible values for flags field... */
#define BOOTP_BROADCAST 32768L

/* Possible values for hardware type (htype) field... */
#define HTYPE_ETHER	1               /* Ethernet 10Mbps              */
#define HTYPE_IEEE802	6               /* IEEE 802.2 Token Ring...	*/
#define HTYPE_FDDI	8		/* FDDI...			*/

/* Magic cookie validating dhcp options field (and bootp vendor
   extensions field). */
#define DHCP_OPTIONS_COOKIE	"\143\202\123\143"

/* DHCP Option codes: */

#define DHO_PAD					0
#define DHO_SUBNET_MASK				1
#define DHO_TIME_OFFSET				2
#define DHO_ROUTERS				3
#define DHO_TIME_SERVERS			4
#define DHO_NAME_SERVERS			5
#define DHO_DOMAIN_NAME_SERVERS			6
#define DHO_LOG_SERVERS				7
#define DHO_COOKIE_SERVERS			8
#define DHO_LPR_SERVERS				9
#define DHO_IMPRESS_SERVERS			10
#define DHO_RESOURCE_LOCATION_SERVERS		11
#define DHO_HOST_NAME				12
#define DHO_BOOT_SIZE				13
#define DHO_MERIT_DUMP				14
#define DHO_DOMAIN_NAME				15
#define DHO_SWAP_SERVER				16
#define DHO_ROOT_PATH				17
#define DHO_EXTENSIONS_PATH			18
#define DHO_IP_FORWARDING			19
#define DHO_NON_LOCAL_SOURCE_ROUTING		20
#define DHO_POLICY_FILTER			21
#define DHO_MAX_DGRAM_REASSEMBLY		22
#define DHO_DEFAULT_IP_TTL			23
#define DHO_PATH_MTU_AGING_TIMEOUT		24
#define DHO_PATH_MTU_PLATEAU_TABLE		25
#define DHO_INTERFACE_MTU			26
#define DHO_ALL_SUBNETS_LOCAL			27
#define DHO_BROADCAST_ADDRESS			28
#define DHO_PERFORM_MASK_DISCOVERY		29
#define DHO_MASK_SUPPLIER			30
#define DHO_ROUTER_DISCOVERY			31
#define DHO_ROUTER_SOLICITATION_ADDRESS		32
#define DHO_STATIC_ROUTES			33
#define DHO_TRAILER_ENCAPSULATION		34
#define DHO_ARP_CACHE_TIMEOUT			35
#define DHO_IEEE802_3_ENCAPSULATION		36
#define DHO_DEFAULT_TCP_TTL			37
#define DHO_TCP_KEEPALIVE_INTERVAL		38
#define DHO_TCP_KEEPALIVE_GARBAGE		39
#define DHO_NIS_DOMAIN				40
#define DHO_NIS_SERVERS				41
#define DHO_NTP_SERVERS				42
#define DHO_VENDOR_ENCAPSULATED_OPTIONS		43
#define DHO_NETBIOS_NAME_SERVERS		44
#define DHO_NETBIOS_DD_SERVER			45
#define DHO_NETBIOS_NODE_TYPE			46
#define DHO_NETBIOS_SCOPE			47
#define DHO_FONT_SERVERS			48
#define DHO_X_DISPLAY_MANAGER			49
#define DHO_DHCP_REQUESTED_ADDRESS		50
#define DHO_DHCP_LEASE_TIME			51
#define DHO_DHCP_OPTION_OVERLOAD		52
#define DHO_DHCP_MESSAGE_TYPE			53
#define DHO_DHCP_SERVER_IDENTIFIER		54
#define DHO_DHCP_PARAMETER_REQUEST_LIST		55
#define DHO_DHCP_MESSAGE			56
#define DHO_DHCP_MAX_MESSAGE_SIZE		57
#define DHO_DHCP_RENEWAL_TIME			58
#define DHO_DHCP_REBINDING_TIME			59
#define DHO_VENDOR_CLASS_IDENTIFIER		60
#define DHO_DHCP_CLIENT_IDENTIFIER		61
#define DHO_NWIP_DOMAIN_NAME			62
#define DHO_NWIP_SUBOPTIONS			63
#define DHO_USER_CLASS				77
#define DHO_FQDN				81
#define DHO_DHCP_AGENT_OPTIONS			82
#define DHO_AUTHENTICATE			90  /* RFC3118, was 210 */
#define DHO_CLIENT_LAST_TRANSACTION_TIME	91
#define DHO_ASSOCIATED_IP			92
#define DHO_SUBNET_SELECTION			118 /* RFC3011! */
#define DHO_DOMAIN_SEARCH			119 /* RFC3397 */
#define DHO_VIVCO_SUBOPTIONS			124
#define DHO_VIVSO_SUBOPTIONS			125

#define DHO_END					255

/* DHCP message types. */
#define DHCPDISCOVER		1
#define DHCPOFFER		2
#define DHCPREQUEST		3
#define DHCPDECLINE		4
#define DHCPACK			5
#define DHCPNAK			6
#define DHCPRELEASE		7
#define DHCPINFORM		8
#define DHCPLEASEQUERY		10
#define DHCPLEASEUNASSIGNED	11
#define DHCPLEASEUNKNOWN	12
#define DHCPLEASEACTIVE		13


/* Relay Agent Information option subtypes: */
#define RAI_CIRCUIT_ID	1
#define RAI_REMOTE_ID	2
#define RAI_AGENT_ID	3
#define RAI_LINK_SELECT	5

/* FQDN suboptions: */
#define FQDN_NO_CLIENT_UPDATE		1
#define FQDN_SERVER_UPDATE		2
#define FQDN_ENCODED			3
#define FQDN_RCODE1			4
#define FQDN_RCODE2			5
#define FQDN_HOSTNAME			6
#define FQDN_DOMAINNAME			7
#define FQDN_FQDN			8
#define FQDN_SUBOPTION_COUNT		8

/* Enterprise Suboptions: */
#define VENDOR_ISC_SUBOPTIONS		2495

#endif /* DHCP_H */

