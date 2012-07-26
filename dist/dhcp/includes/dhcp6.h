/* dhcp6.h

   DHCPv6 Protocol structures... */

/*
 * Copyright (c) 2006-2010 by Internet Systems Consortium, Inc. ("ISC")
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


/* DHCPv6 Option codes: */

#define D6O_CLIENTID				1 /* RFC3315 */
#define D6O_SERVERID				2
#define D6O_IA_NA				3
#define D6O_IA_TA				4
#define D6O_IAADDR				5
#define D6O_ORO					6
#define D6O_PREFERENCE				7
#define D6O_ELAPSED_TIME			8
#define D6O_RELAY_MSG				9
/* Option code 10 unassigned. */
#define D6O_AUTH				11
#define D6O_UNICAST				12
#define D6O_STATUS_CODE				13
#define D6O_RAPID_COMMIT			14
#define D6O_USER_CLASS				15
#define D6O_VENDOR_CLASS			16
#define D6O_VENDOR_OPTS				17
#define D6O_INTERFACE_ID			18
#define D6O_RECONF_MSG				19
#define D6O_RECONF_ACCEPT			20
#define D6O_SIP_SERVERS_DNS			21 /* RFC3319 */
#define D6O_SIP_SERVERS_ADDR			22 /* RFC3319 */
#define D6O_NAME_SERVERS			23 /* RFC3646 */
#define D6O_DOMAIN_SEARCH			24 /* RFC3646 */
#define D6O_IA_PD				25 /* RFC3633 */
#define D6O_IAPREFIX				26 /* RFC3633 */
#define D6O_NIS_SERVERS				27 /* RFC3898 */
#define D6O_NISP_SERVERS			28 /* RFC3898 */
#define D6O_NIS_DOMAIN_NAME			29 /* RFC3898 */
#define D6O_NISP_DOMAIN_NAME			30 /* RFC3898 */
#define D6O_SNTP_SERVERS			31 /* RFC4075 */
#define D6O_INFORMATION_REFRESH_TIME		32 /* RFC4242 */
#define D6O_BCMCS_SERVER_D			33 /* RFC4280 */
#define D6O_BCMCS_SERVER_A			34 /* RFC4280 */
/* 35 is unassigned */
#define D6O_GEOCONF_CIVIC			36 /* RFC4776 */
#define D6O_REMOTE_ID				37 /* RFC4649 */
#define D6O_SUBSCRIBER_ID			38 /* RFC4580 */
#define D6O_CLIENT_FQDN				39 /* RFC4704 */
#define D6O_PANA_AGENT				40 /* paa-option */
#define D6O_NEW_POSIX_TIMEZONE			41 /* RFC4833 */
#define D6O_NEW_TZDB_TIMEZONE			42 /* RFC4833 */
#define D6O_ERO					43 /* RFC4994 */
#define D6O_LQ_QUERY				44 /* RFC5007 */
#define D6O_CLIENT_DATA				45 /* RFC5007 */
#define D6O_CLT_TIME				46 /* RFC5007 */
#define D6O_LQ_RELAY_DATA			47 /* RFC5007 */
#define D6O_LQ_CLIENT_LINK			48 /* RFC5007 */

/* 
 * Status Codes, from RFC 3315 section 24.4, and RFC 3633, 5007.
 */
#define STATUS_Success		 0
#define STATUS_UnspecFail	 1
#define STATUS_NoAddrsAvail	 2
#define STATUS_NoBinding	 3
#define STATUS_NotOnLink	 4 
#define STATUS_UseMulticast	 5 
#define STATUS_NoPrefixAvail	 6
#define STATUS_UnknownQueryType	 7
#define STATUS_MalformedQuery	 8
#define STATUS_NotConfigured	 9
#define STATUS_NotAllowed	10

/* 
 * DHCPv6 message types, defined in section 5.3 of RFC 3315 
 */
#define DHCPV6_SOLICIT		    1
#define DHCPV6_ADVERTISE	    2
#define DHCPV6_REQUEST		    3
#define DHCPV6_CONFIRM		    4
#define DHCPV6_RENEW		    5
#define DHCPV6_REBIND		    6
#define DHCPV6_REPLY		    7
#define DHCPV6_RELEASE		    8
#define DHCPV6_DECLINE		    9
#define DHCPV6_RECONFIGURE	   10
#define DHCPV6_INFORMATION_REQUEST 11
#define DHCPV6_RELAY_FORW	   12
#define DHCPV6_RELAY_REPL	   13
#define DHCPV6_LEASEQUERY	   14
#define DHCPV6_LEASEQUERY_REPLY    15

extern const char *dhcpv6_type_names[];
extern const int dhcpv6_type_name_max;

/* DUID type definitions (RFC3315 section 9).
 */
#define DUID_LLT	1
#define DUID_EN		2
#define DUID_LL		3

/* Offsets into IA_*'s where Option spaces commence.  */
#define IA_NA_OFFSET 12 /* IAID, T1, T2, all 4 octets each */
#define IA_TA_OFFSET  4 /* IAID only, 4 octets */
#define IA_PD_OFFSET 12 /* IAID, T1, T2, all 4 octets each */

/* Offset into IAADDR's where Option spaces commence. */
#define IAADDR_OFFSET 24

/* Offset into IAPREFIX's where Option spaces commence. */
#define IAPREFIX_OFFSET 25

/* Offset into LQ_QUERY's where Option spaces commence. */
#define LQ_QUERY_OFFSET 17

/* 
 * DHCPv6 well-known multicast addressess, from section 5.1 of RFC 3315 
 */
#define All_DHCP_Relay_Agents_and_Servers "FF02::1:2"
#define All_DHCP_Servers "FF05::1:3"

/*
 * DHCPv6 Retransmission Constants (RFC3315 section 5.5, RFC 5007)
 */

#define SOL_MAX_DELAY     1
#define SOL_TIMEOUT       1
#define SOL_MAX_RT      120
#define REQ_TIMEOUT       1
#define REQ_MAX_RT       30
#define REQ_MAX_RC       10
#define CNF_MAX_DELAY     1
#define CNF_TIMEOUT       1
#define CNF_MAX_RT        4
#define CNF_MAX_RD       10
#define REN_TIMEOUT      10
#define REN_MAX_RT      600
#define REB_TIMEOUT      10
#define REB_MAX_RT      600
#define INF_MAX_DELAY     1
#define INF_TIMEOUT       1
#define INF_MAX_RT      120
#define REL_TIMEOUT       1
#define REL_MAX_RC        5
#define DEC_TIMEOUT       1
#define DEC_MAX_RC        5
#define REC_TIMEOUT       2
#define REC_MAX_RC        8
#define HOP_COUNT_LIMIT  32
#define LQ6_TIMEOUT       1
#define LQ6_MAX_RT       10
#define LQ6_MAX_RC        5

/* 
 * Normal packet format, defined in section 6 of RFC 3315 
 */
struct dhcpv6_packet {
	unsigned char msg_type;
	unsigned char transaction_id[3];
	unsigned char options[FLEXIBLE_ARRAY_MEMBER];
};

/* Offset into DHCPV6 Reply packets where Options spaces commence. */
#define REPLY_OPTIONS_INDEX 4

/* 
 * Relay packet format, defined in section 7 of RFC 3315 
 */
struct dhcpv6_relay_packet {
	unsigned char msg_type;
	unsigned char hop_count;
	unsigned char link_address[16];
	unsigned char peer_address[16];
	unsigned char options[FLEXIBLE_ARRAY_MEMBER];
};

/* Leasequery query-types (RFC 5007) */

#define LQ6QT_BY_ADDRESS	1
#define LQ6QT_BY_CLIENTID	2

/*
 * DUID time starts 2000-01-01.
 * This constant is the number of seconds since 1970-01-01,
 * when the Unix epoch began.
 */
#define DUID_TIME_EPOCH 946684800

/* Information-Request Time option (RFC 4242) */

#define IRT_DEFAULT	86400
#define IRT_MINIMUM	600

