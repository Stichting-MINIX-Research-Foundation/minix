/*	net/gen/dhcp.h - DHCP packet format		Author: Kees J. Bot
 *								1 Dec 2000
 */

#ifndef __NET__GEN__DHCP_H__
#define __NET__GEN__DHCP_H__

typedef struct dhcp {
	u8_t		op;		/* Message opcode/type. */
	u8_t		htype;		/* Hardware address type. */
	u8_t		hlen;		/* Hardware address length. */
	u8_t		hops;		/* Hop count when relaying. */
	u32_t		xid;		/* Transaction ID. */
	u16_t		secs;		/* Seconds past since client began. */
	u16_t		flags;		/* Flags. */
	ipaddr_t	ciaddr;		/* Client IP address. */
	ipaddr_t	yiaddr;		/* "Your" IP address. */
	ipaddr_t	siaddr;		/* Boot server IP address. */
	ipaddr_t	giaddr;		/* Relay agent (gateway) IP address. */
	u8_t		chaddr[16];	/* Client hardware address. */
	u8_t		sname[64];	/* Server host name. */
	u8_t		file[128];	/* Boot file. */
	u32_t		magic;		/* Magic number. */
	u8_t		options[308];	/* Optional parameters. */
} dhcp_t;

/* DHCP operations and stuff: */
#define DHCP_BOOTREQUEST	 1	/* Boot request message. */
#define DHCP_BOOTREPLY		 2	/* Boot reply message. */
#define DHCP_HTYPE_ETH		 1	/* Ethernet hardware type. */
#define DHCP_HLEN_ETH		 6	/* Ethernet hardware address length. */
#define DHCP_FLAGS_BCAST    0x8000U	/* Reply must be broadcast to client. */

					/* "Magic" first four option bytes. */
#ifdef __NBSD_LIBC
#define DHCP_MAGIC	htonl(0x63825363UL)
#else
#define DHCP_MAGIC	HTONL(0x63825363UL)
#endif

/* DHCP common tags: */
#define DHCP_TAG_NETMASK	 1	/* Netmask. */
#define DHCP_TAG_GATEWAY	 3	/* Gateway list. */
#define DHCP_TAG_DNS		 6	/* DNS Nameserver list. */
#define DHCP_TAG_HOSTNAME	12	/* Host name. */
#define DHCP_TAG_DOMAIN		15	/* Domain. */
#define DHCP_TAG_IPMTU		26	/* Interface MTU. */

/* DHCP protocol tags: */
#define DHCP_TAG_REQIP		50	/* Request this IP. */
#define DHCP_TAG_LEASE		51	/* Lease time requested/offered. */
#define DHCP_TAG_OVERLOAD	52	/* Options continued in file/sname. */
#define DHCP_TAG_TYPE		53	/* DHCP message (values below). */
#define DHCP_TAG_SERVERID	54	/* Server identifier. */
#define DHCP_TAG_REQPAR		55	/* Parameters requested. */
#define DHCP_TAG_MESSAGE	56	/* Error message. */
#define DHCP_TAG_MAXDHCP	57	/* Max DHCP packet size. */
#define DHCP_TAG_RENEWAL	58	/* Time to go into renewal state. */
#define DHCP_TAG_REBINDING	59	/* Time to go into rebinding state. */
#define DHCP_TAG_CLASSID	60	/* Class identifier. */
#define DHCP_TAG_CLIENTID	61	/* Client identifier. */

/* DHCP messages: */
#define DHCP_DISCOVER		 1	/* Locate available servers. */
#define DHCP_OFFER		 2	/* Parameters offered to client. */
#define DHCP_REQUEST		 3	/* (Re)request offered parameters. */
#define DHCP_DECLINE		 4	/* Client declines offer. */
#define DHCP_ACK		 5	/* Server acknowlegdes request. */
#define DHCP_NAK		 6	/* Server denies request. */
#define DHCP_RELEASE		 7	/* Client relinguishes address. */
#define DHCP_INFORM		 8	/* Client requests just local config. */

void dhcp_init(dhcp_t *_dp);
int dhcp_settag(dhcp_t *_dp, int _tag, void *_data, size_t _len);
int dhcp_gettag(dhcp_t *_dp, int _searchtag, u8_t **_pdata, size_t *_plen);

#endif /* __NET__GEN__DHCP_H__ */
