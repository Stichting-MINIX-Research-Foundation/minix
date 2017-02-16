/*	$NetBSD: dhcpd.h,v 1.6 2014/07/12 12:09:37 spz Exp $	*/
/* dhcpd.h

   Definitions for dhcpd... */

/*
 * Copyright (c) 2004-2014 by Internet Systems Consortium, Inc. ("ISC")
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
 *
 */

/*! \file includes/dhcpd.h */

#include "config.h"

#ifndef __CYGWIN32__
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>

#include <netdb.h>
#else
#define fd_set cygwin_fd_set
#include <sys/types.h>
#endif
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <ctype.h>
#include <time.h>

#include <net/if.h>
#undef FDDI
#include <net/route.h>
#include <net/if_arp.h>
#if HAVE_NET_IF_DL_H
# include <net/if_dl.h>
#endif

#include <setjmp.h>

#include "cdefs.h"
#include "osdep.h"

#include "arpa/nameser.h"

#include "minires.h"

struct hash_table;
typedef struct hash_table group_hash_t;
typedef struct hash_table universe_hash_t;
typedef struct hash_table option_name_hash_t;
typedef struct hash_table option_code_hash_t;
typedef struct hash_table dns_zone_hash_t;
typedef struct hash_table lease_ip_hash_t;
typedef struct hash_table lease_id_hash_t;
typedef struct hash_table host_hash_t;
typedef struct hash_table class_hash_t;

typedef time_t TIME;

#ifndef EOL
#define EOL '\n'
#endif

#include <omapip/isclib.h>
#include <omapip/result.h>

#include "dhcp.h"
#include "dhcp6.h"
#include "statement.h"
#include "tree.h"
#include "inet.h"
#include "dhctoken.h"

#include <omapip/omapip_p.h>

#if defined(LDAP_CONFIGURATION)
# include <ldap.h>
# include <sys/utsname.h> /* for uname() */
#endif

#if !defined (BYTE_NAME_HASH_SIZE)
# define BYTE_NAME_HASH_SIZE	401	/* Default would be ridiculous. */
#endif
#if !defined (BYTE_CODE_HASH_SIZE)
# define BYTE_CODE_HASH_SIZE	254	/* Default would be ridiculous. */
#endif

/* Although it is highly improbable that a 16-bit option space might
 * actually use 2^16 actual defined options, it is the worst case
 * scenario we must prepare for.  Having 4 options per bucket in this
 * case is pretty reasonable.
 */
#if !defined (WORD_NAME_HASH_SIZE)
# define WORD_NAME_HASH_SIZE	20479
#endif
#if !defined (WORD_CODE_HASH_SIZE)
# define WORD_CODE_HASH_SIZE	16384
#endif

/* Not only is it improbable that the 32-bit spaces might actually use 2^32
 * defined options, it is infeasible.  It would be best for this kind of
 * space to be dynamically sized.  Instead we size it at the word hash's
 * level.
 */
#if !defined (QUAD_NAME_HASH_SIZE)
# define QUAD_NAME_HASH_SIZE	WORD_NAME_HASH_SIZE
#endif
#if !defined (QUAD_CODE_HASH_SIZE)
# define QUAD_CODE_HASH_SIZE	WORD_CODE_HASH_SIZE
#endif

#if !defined (DNS_HASH_SIZE)
# define DNS_HASH_SIZE		0	/* Default. */
#endif

/* Default size to use for name/code hashes on user-defined option spaces. */
#if !defined (DEFAULT_SPACE_HASH_SIZE)
# define DEFAULT_SPACE_HASH_SIZE	11
#endif

#if !defined (NWIP_HASH_SIZE)
# define NWIP_HASH_SIZE		17	/* A really small table. */
#endif

#if !defined (FQDN_HASH_SIZE)
# define FQDN_HASH_SIZE		13	/* A ridiculously small table. */
#endif

/* I really doubt a given installation is going to have more than a few
 * hundred vendors involved.
 */
#if !defined (VIVCO_HASH_SIZE)
# define VIVCO_HASH_SIZE	127
#endif

#if !defined (VIVSO_HASH_SIZE)
# define VIVSO_HASH_SIZE	VIVCO_HASH_SIZE
#endif

#if !defined (VSIO_HASH_SIZE)
# define VSIO_HASH_SIZE         VIVCO_HASH_SIZE
#endif

#if !defined (VIV_ISC_HASH_SIZE)
# define VIV_ISC_HASH_SIZE	3	/* An incredulously small table. */
#endif

#if !defined (UNIVERSE_HASH_SIZE)
# define UNIVERSE_HASH_SIZE	13	/* A really small table. */
#endif

#if !defined (GROUP_HASH_SIZE)
# define GROUP_HASH_SIZE	0	/* Default. */
#endif

/* At least one person has indicated they use ~20k host records.
 */
#if !defined (HOST_HASH_SIZE)
# define HOST_HASH_SIZE		22501
#endif

/* We have user reports of use of ISC DHCP numbering leases in the 200k's.
 *
 * We also have reports of folks using 10.0/8 as a dynamic range.  The
 * following is something of a compromise between the two.  At the ~2-3
 * hundred thousand leases, there's ~2-3 leases to search in each bucket.
 */
#if !defined (LEASE_HASH_SIZE)
# define LEASE_HASH_SIZE	100003
#endif

/* It is not known what the worst case subclass hash size is.  We estimate
 * high, I think.
 */
#if !defined (SCLASS_HASH_SIZE)
# define SCLASS_HASH_SIZE	12007
#endif

#if !defined (AGENT_HASH_SIZE)
# define AGENT_HASH_SIZE	11	/* A really small table. */
#endif

/* The server hash size is used for both names and codes.  There aren't
 * many (roughly 50 at the moment), so we use a smaller table.  If we
 * use a 1:1 table size, then we get name collisions due to poor name
 * hashing.  So we use double the space we need, which drastically
 * reduces collisions.
 */
#if !defined (SERVER_HASH_SIZE)
# define SERVER_HASH_SIZE (2*(sizeof(server_options) / sizeof(struct option)))
#endif


/* How many options are likely to appear in a single packet? */
#if !defined (OPTION_HASH_SIZE)
# define OPTION_HASH_SIZE 17
# define OPTION_HASH_PTWO 32	/* Next power of two above option hash. */
# define OPTION_HASH_EXP 5	/* The exponent for that power of two. */
#endif

#define compute_option_hash(x) \
	(((x) & (OPTION_HASH_PTWO - 1)) + \
	 (((x) >> OPTION_HASH_EXP) & \
	  (OPTION_HASH_PTWO - 1))) % OPTION_HASH_SIZE;

enum dhcp_shutdown_state {
	shutdown_listeners,
	shutdown_omapi_connections,
	shutdown_drop_omapi_connections,
	shutdown_dhcp,
	shutdown_done
};

/* Client FQDN option, failover FQDN option, etc. */
typedef struct {
	u_int8_t codes [2];
	unsigned length;
	u_int8_t *data;
} ddns_fqdn_t;

#include "failover.h"

/* A parsing context. */

struct parse {
	int lexline;
	int lexchar;
	char *token_line;
	char *prev_line;
	char *cur_line;
	const char *tlname;
	int eol_token;

	/*
	 * In order to give nice output when we have a parsing error
	 * in our file, we keep track of where we are in the line so
	 * that we can show the user.
	 *
	 * We need to keep track of two lines, because we can look
	 * ahead, via the "peek" function, to the next line sometimes.
	 *
	 * The "line1" and "line2" variables act as buffers for this
	 * information. The "lpos" variable tells us where we are in the
	 * line.
	 *
	 * When we "put back" a character from the parsing context, we
	 * do not want to have the character appear twice in the error
	 * output. So, we set a flag, the "ugflag", which the
	 * get_char() function uses to check for this condition.
	 */
	char line1 [81];
	char line2 [81];
	int lpos;
	int line;
	int tlpos;
	int tline;
	enum dhcp_token token;
	int ugflag;
	char *tval;
	int tlen;
	char tokbuf [1500];

	int warnings_occurred;
	int file;
	char *inbuf;
	size_t bufix, buflen;
	size_t bufsiz;

	struct parse *saved_state;

#if defined(LDAP_CONFIGURATION)
	/*
	 * LDAP configuration uses a call-back to iteratively read config
	 * off of the LDAP repository.
	 * XXX: The token stream can not be rewound reliably, so this must
	 * be addressed for DHCPv6 support.
	 */
	int (*read_function)(struct parse *);
#endif
};

/* Variable-length array of data. */

struct string_list {
	struct string_list *next;
	char string [1];
};

/* A name server, from /etc/resolv.conf. */
struct name_server {
	struct name_server *next;
	struct sockaddr_in addr;
	TIME rcdate;
};

/* A domain search list element. */
struct domain_search_list {
	struct domain_search_list *next;
	char *domain;
	TIME rcdate;
};

/* Option tag structures are used to build chains of option tags, for
   when we're sure we're not going to have enough of them to justify
   maintaining an array. */

struct option_tag {
	struct option_tag *next;
	u_int8_t data [1];
};

/* An agent option structure.   We need a special structure for the
   Relay Agent Information option because if more than one appears in
   a message, we have to keep them separate. */

struct agent_options {
	struct agent_options *next;
	int length;
	struct option_tag *first;
};

struct option_cache {
	int refcnt;
	struct option_cache *next;
	struct expression *expression;
	struct option *option;
	struct data_string data;

	#define OPTION_HAD_NULLS	0x00000001
	u_int32_t flags;
};

struct option_state {
	int refcnt;
	int universe_count;
	int site_universe;
	int site_code_min;
	void *universes [1];
};

/* A dhcp packet and the pointers to its option values. */
struct packet {
	struct dhcp_packet *raw;
	int refcnt;
	unsigned packet_length;
	int packet_type;

	unsigned char dhcpv6_msg_type;		/* DHCPv6 message type */

	/* DHCPv6 transaction ID */
	unsigned char dhcpv6_transaction_id[3];

	/* DHCPv6 relay information */
	unsigned char dhcpv6_hop_count;
	struct in6_addr dhcpv6_link_address;
	struct in6_addr dhcpv6_peer_address;

	/* DHCPv6 packet containing this one, or NULL if none */
	struct packet *dhcpv6_container_packet;

	int options_valid;
	int client_port;
	struct iaddr client_addr;
	struct interface_info *interface;	/* Interface on which packet
						   was received. */
	struct hardware *haddr;		/* Physical link address
					   of local sender (maybe gateway). */

	/* Information for relay agent options (see
	   draft-ietf-dhc-agent-options-xx.txt). */
	u_int8_t *circuit_id;		/* Circuit ID of client connection. */
	int circuit_id_len;
	u_int8_t *remote_id;		/* Remote ID of client. */
	int remote_id_len;

	int got_requested_address;	/* True if client sent the
					   dhcp-requested-address option. */

	struct shared_network *shared_network;
	struct option_state *options;

#if !defined (PACKET_MAX_CLASSES)
# define PACKET_MAX_CLASSES 5
#endif
	int class_count;
	struct class *classes [PACKET_MAX_CLASSES];

	int known;
	int authenticated;

	/* If we stash agent options onto the packet option state, to pretend
	 * options we got in a previous exchange were still there, we need
	 * to signal this in a reliable way.
	 */
	isc_boolean_t agent_options_stashed;

	/*
	 * ISC_TRUE if packet received unicast (as opposed to multicast).
	 * Only used in DHCPv6.
	 */
	isc_boolean_t unicast;
};

/*
 * A network interface's MAC address.
 * 20 bytes for the hardware address
 * and 1 byte for the type tag
 */

#define HARDWARE_ADDR_LEN 20

struct hardware {
	u_int8_t hlen;
	u_int8_t hbuf[HARDWARE_ADDR_LEN + 1];
};

#if defined(LDAP_CONFIGURATION)
# define LDAP_BUFFER_SIZE		8192
# define LDAP_METHOD_STATIC		0
# define LDAP_METHOD_DYNAMIC	1
#if defined (LDAP_USE_SSL)
# define LDAP_SSL_OFF			0
# define LDAP_SSL_ON			1
# define LDAP_SSL_TLS			2
# define LDAP_SSL_LDAPS			3
#endif

/* This is a tree of the current configuration we are building from LDAP */
struct ldap_config_stack {
	LDAPMessage * res;	/* Pointer returned from ldap_search */
	LDAPMessage * ldent;	/* Current item in LDAP that we're processing.
							in res */
	int close_brace;	/* Put a closing } after we're through with
						this item */
	int processed;	/* We set this flag if this base item has been
					processed. After this base item is processed,
					we can start processing the children */
	struct ldap_config_stack *children;
	struct ldap_config_stack *next;
};
#endif

typedef enum {
	server_startup = 0,
	server_running = 1,
	server_shutdown = 2,
	server_hibernate = 3,
	server_awaken = 4
} control_object_state_t;

typedef struct {
	OMAPI_OBJECT_PREAMBLE;
	control_object_state_t state;
} dhcp_control_object_t;

/* Lease states: */
#define FTS_FREE	1
#define FTS_ACTIVE	2
#define FTS_EXPIRED	3
#define FTS_RELEASED	4
#define FTS_ABANDONED	5
#define FTS_RESET	6
#define FTS_BACKUP	7
typedef u_int8_t binding_state_t;

/* FTS_LAST is the highest value that is valid for a lease binding state. */
#define FTS_LAST FTS_BACKUP

/*
 * A block for the on statements so we can share the structure
 * between v4 and v6
 */
struct on_star {
	struct executable_statement *on_expiry;
	struct executable_statement *on_commit;
	struct executable_statement *on_release;
};

/* A dhcp lease declaration structure. */
struct lease {
	OMAPI_OBJECT_PREAMBLE;
	struct lease *next;
	struct lease *n_uid, *n_hw;

	struct iaddr ip_addr;
	TIME starts, ends, sort_time;
	char *client_hostname;
	struct binding_scope *scope;
	struct host_decl *host;
	struct subnet *subnet;
	struct pool *pool;
	struct class *billing_class;
	struct option_chain_head *agent_options;

	/* insert the structure directly */
	struct on_star on_star;

	unsigned char *uid;
	unsigned short uid_len;
	unsigned short uid_max;
	unsigned char uid_buf [7];
	struct hardware hardware_addr;

	u_int8_t flags;
#       define STATIC_LEASE		1
#	define BOOTP_LEASE		2
#	define RESERVED_LEASE		4
#	define MS_NULL_TERMINATION	8
#	define ON_UPDATE_QUEUE		16
#	define ON_ACK_QUEUE		32
#	define ON_QUEUE			(ON_UPDATE_QUEUE | ON_ACK_QUEUE)
#	define UNICAST_BROADCAST_HACK	64
#	define ON_DEFERRED_QUEUE	128

/* Persistent flags are to be preserved on a given lease structure. */
#       define PERSISTENT_FLAGS		(ON_ACK_QUEUE | ON_UPDATE_QUEUE)
/* Ephemeral flags are to be preserved on a given lease (copied etc). */
#	define EPHEMERAL_FLAGS		(MS_NULL_TERMINATION | \
					 UNICAST_BROADCAST_HACK | \
					 RESERVED_LEASE | \
					 BOOTP_LEASE)

	/*
	 * The lease's binding state is its current state.  The next binding
	 * state is the next state this lease will move into by expiration,
	 * or timers in general.  The desired binding state is used on lease
	 * updates; the caller is attempting to move the lease to the desired
	 * binding state (and this may either succeed or fail, so the binding
	 * state must be preserved).
	 *
	 * The 'rewind' binding state is used in failover processing.  It
	 * is used for an optimization when out of communications; it allows
	 * the server to "rewind" a lease to the previous state acknowledged
	 * by the peer, and progress forward from that point.
	 */
	binding_state_t binding_state;
	binding_state_t next_binding_state;
	binding_state_t desired_binding_state;
	binding_state_t rewind_binding_state;

	struct lease_state *state;

	/*
	 * 'tsfp' is more of an 'effective' tsfp.  It may be calculated from
	 * stos+mclt for example if it's an expired lease and the server is
	 * in partner-down state.  'atsfp' is zeroed whenever a lease is
	 * updated - and only set when the peer acknowledges it.  This
	 * ensures every state change is transmitted.
	 */
	TIME tstp;	/* Time sent to partner. */
	TIME tsfp;	/* Time sent from partner. */
	TIME atsfp;	/* Actual time sent from partner. */
	TIME cltt;	/* Client last transaction time. */
	u_int32_t last_xid; /* XID we sent in this lease's BNDUPD */
	struct lease *next_pending;

	/*
	 * A pointer to the state of the ddns update for this lease.
	 * It should be set while the update is in progress and cleared
	 * when the update finishes.  It can be used to cancel the
	 * update if we want to do a different update.
	 */
	struct dhcp_ddns_cb *ddns_cb;
};

struct lease_state {
	struct lease_state *next;

	struct interface_info *ip;

	struct packet *packet;	/* The incoming packet. */

	TIME offered_expiry;

	struct option_state *options;
	struct data_string parameter_request_list;
	int max_message_size;
	unsigned char expiry[4], renewal[4], rebind[4];
	struct data_string filename, server_name;
	int got_requested_address;
	int got_server_identifier;
	struct shared_network *shared_network;	/* Shared network of interface
						   on which request arrived. */

	u_int32_t xid;
	u_int16_t secs;
	u_int16_t bootp_flags;
	struct in_addr ciaddr;
	struct in_addr siaddr;
	struct in_addr giaddr;
	u_int8_t hops;
	u_int8_t offer;
	struct iaddr from;
};

#define	ROOT_GROUP	0
#define HOST_DECL	1
#define SHARED_NET_DECL	2
#define SUBNET_DECL	3
#define CLASS_DECL	4
#define	GROUP_DECL	5
#define POOL_DECL	6

/* Possible modes in which discover_interfaces can run. */

#define DISCOVER_RUNNING	0
#define DISCOVER_SERVER		1
#define DISCOVER_UNCONFIGURED	2
#define DISCOVER_RELAY		3
#define DISCOVER_REQUESTED	4

/* DDNS_UPDATE_STYLE enumerations. */
#define DDNS_UPDATE_STYLE_NONE		0
#define DDNS_UPDATE_STYLE_AD_HOC	1
#define DDNS_UPDATE_STYLE_INTERIM	2
#define DDNS_UPDATE_STYLE_STANDARD	3

/* Server option names. */

#define SV_DEFAULT_LEASE_TIME		1
#define SV_MAX_LEASE_TIME		2
#define SV_MIN_LEASE_TIME		3
#define SV_BOOTP_LEASE_CUTOFF		4
#define SV_BOOTP_LEASE_LENGTH		5
#define SV_BOOT_UNKNOWN_CLIENTS		6
#define SV_DYNAMIC_BOOTP		7
#define	SV_ALLOW_BOOTP			8
#define	SV_ALLOW_BOOTING		9
#define	SV_ONE_LEASE_PER_CLIENT		10
#define	SV_GET_LEASE_HOSTNAMES		11
#define	SV_USE_HOST_DECL_NAMES		12
#define	SV_USE_LEASE_ADDR_FOR_DEFAULT_ROUTE	13
#define	SV_MIN_SECS			14
#define	SV_FILENAME			15
#define SV_SERVER_NAME			16
#define	SV_NEXT_SERVER			17
#define SV_AUTHORITATIVE		18
#define SV_VENDOR_OPTION_SPACE		19
#define SV_ALWAYS_REPLY_RFC1048		20
#define SV_SITE_OPTION_SPACE		21
#define SV_ALWAYS_BROADCAST		22
#define SV_DDNS_DOMAIN_NAME		23
#define SV_DDNS_HOST_NAME		24
#define SV_DDNS_REV_DOMAIN_NAME		25
#define SV_LEASE_FILE_NAME		26
#define SV_PID_FILE_NAME		27
#define SV_DUPLICATES			28
#define SV_DECLINES			29
#define SV_DDNS_UPDATES			30
#define SV_OMAPI_PORT			31
#define SV_LOCAL_PORT			32
#define SV_LIMITED_BROADCAST_ADDRESS	33
#define SV_REMOTE_PORT			34
#define SV_LOCAL_ADDRESS		35
#define SV_OMAPI_KEY			36
#define SV_STASH_AGENT_OPTIONS		37
#define SV_DDNS_TTL			38
#define SV_DDNS_UPDATE_STYLE		39
#define SV_CLIENT_UPDATES		40
#define SV_UPDATE_OPTIMIZATION		41
#define SV_PING_CHECKS			42
#define SV_UPDATE_STATIC_LEASES		43
#define SV_LOG_FACILITY			44
#define SV_DO_FORWARD_UPDATES		45
#define SV_PING_TIMEOUT			46
#define SV_RESERVE_INFINITE		47
#define SV_DDNS_CONFLICT_DETECT		48
#define SV_LEASEQUERY			49
#define SV_ADAPTIVE_LEASE_TIME_THRESHOLD	50
#define SV_DO_REVERSE_UPDATES		51
#define SV_FQDN_REPLY			52
#define SV_PREFER_LIFETIME		53
#define SV_DHCPV6_LEASE_FILE_NAME       54
#define SV_DHCPV6_PID_FILE_NAME         55
#define SV_LIMIT_ADDRS_PER_IA		56
#define SV_LIMIT_PREFS_PER_IA		57
#define SV_DELAYED_ACK			58
#define SV_MAX_ACK_DELAY		59
#if defined(LDAP_CONFIGURATION)
# define SV_LDAP_SERVER                 60
# define SV_LDAP_PORT                   61
# define SV_LDAP_USERNAME               62
# define SV_LDAP_PASSWORD               63
# define SV_LDAP_BASE_DN                64
# define SV_LDAP_METHOD                 65
# define SV_LDAP_DEBUG_FILE             66
# define SV_LDAP_DHCP_SERVER_CN         67
# define SV_LDAP_REFERRALS              68
#if defined (LDAP_USE_SSL)
# define SV_LDAP_SSL                    69
# define SV_LDAP_TLS_REQCERT            70
# define SV_LDAP_TLS_CA_FILE            71
# define SV_LDAP_TLS_CA_DIR             72
# define SV_LDAP_TLS_CERT               73
# define SV_LDAP_TLS_KEY                74
# define SV_LDAP_TLS_CRLCHECK           75
# define SV_LDAP_TLS_CIPHERS            76
# define SV_LDAP_TLS_RANDFILE           77
#endif
#endif
#define SV_CACHE_THRESHOLD		78
#define SV_DONT_USE_FSYNC		79
#define SV_DDNS_LOCAL_ADDRESS4		80
#define SV_DDNS_LOCAL_ADDRESS6		81
#define SV_IGNORE_CLIENT_UIDS		82

#if !defined (DEFAULT_PING_TIMEOUT)
# define DEFAULT_PING_TIMEOUT 1
#endif

#if !defined (DEFAULT_DELAYED_ACK)
# define DEFAULT_DELAYED_ACK 28  /* default SO_SNDBUF size / 576 bytes */
#endif

#if !defined (DEFAULT_ACK_DELAY_SECS)
# define DEFAULT_ACK_DELAY_SECS 0
#endif

#if !defined (DEFAULT_ACK_DELAY_USECS)
# define DEFAULT_ACK_DELAY_USECS 250000 /* 1/4 of a second */
#endif

#if !defined (DEFAULT_MIN_ACK_DELAY_USECS)
# define DEFAULT_MIN_ACK_DELAY_USECS 10000 /* 1/100 second */
#endif

#if !defined (DEFAULT_CACHE_THRESHOLD)
# define DEFAULT_CACHE_THRESHOLD 25
#endif

#if !defined (DEFAULT_DEFAULT_LEASE_TIME)
# define DEFAULT_DEFAULT_LEASE_TIME 43200
#endif

#if !defined (DEFAULT_MIN_LEASE_TIME)
# define DEFAULT_MIN_LEASE_TIME 300
#endif

#if !defined (DEFAULT_MAX_LEASE_TIME)
# define DEFAULT_MAX_LEASE_TIME 86400
#endif

#if !defined (DEFAULT_DDNS_TTL)
# define DEFAULT_DDNS_TTL 3600
#endif
#if !defined (MAX_DEFAULT_DDNS_TTL)
# define MAX_DEFAULT_DDNS_TTL 3600
#endif

#if !defined (MIN_LEASE_WRITE)
# define MIN_LEASE_WRITE 15
#endif

/* Client option names */

#define	CL_TIMEOUT		1
#define	CL_SELECT_INTERVAL	2
#define CL_REBOOT_TIMEOUT	3
#define CL_RETRY_INTERVAL	4
#define CL_BACKOFF_CUTOFF	5
#define CL_INITIAL_INTERVAL	6
#define CL_BOOTP_POLICY		7
#define	CL_SCRIPT_NAME		8
#define CL_REQUESTED_OPTIONS	9
#define CL_REQUESTED_LEASE_TIME	10
#define CL_SEND_OPTIONS		11
#define CL_MEDIA		12
#define	CL_REJECT_LIST		13

#ifndef CL_DEFAULT_TIMEOUT
# define CL_DEFAULT_TIMEOUT	60
#endif

#ifndef CL_DEFAULT_SELECT_INTERVAL
# define CL_DEFAULT_SELECT_INTERVAL 0
#endif

#ifndef CL_DEFAULT_REBOOT_TIMEOUT
# define CL_DEFAULT_REBOOT_TIMEOUT 10
#endif

#ifndef CL_DEFAULT_RETRY_INTERVAL
# define CL_DEFAULT_RETRY_INTERVAL 300
#endif

#ifndef CL_DEFAULT_BACKOFF_CUTOFF
# define CL_DEFAULT_BACKOFF_CUTOFF 120
#endif

#ifndef CL_DEFAULT_INITIAL_INTERVAL
# define CL_DEFAULT_INITIAL_INTERVAL 10
#endif

#ifndef CL_DEFAULT_BOOTP_POLICY
# define CL_DEFAULT_BOOTP_POLICY P_ACCEPT
#endif

#ifndef CL_DEFAULT_REQUESTED_OPTIONS
# define CL_DEFAULT_REQUESTED_OPTIONS \
	{ DHO_SUBNET_MASK, \
	  DHO_BROADCAST_ADDRESS, \
	  DHO_TIME_OFFSET, \
	  DHO_ROUTERS, \
	  DHO_DOMAIN_NAME, \
	  DHO_DOMAIN_NAME_SERVERS, \
	  DHO_HOST_NAME }
#endif

struct group_object {
	OMAPI_OBJECT_PREAMBLE;

	struct group_object *n_dynamic;
	struct group *group;
	char *name;
	int flags;
#define GROUP_OBJECT_DELETED	1
#define GROUP_OBJECT_DYNAMIC	2
#define GROUP_OBJECT_STATIC	4
};

/* Group of declarations that share common parameters. */
struct group {
	struct group *next;

	int refcnt;
	struct group_object *object;
	struct subnet *subnet;
	struct shared_network *shared_network;
	int authoritative;
	struct executable_statement *statements;
};

/* A dhcp host declaration structure. */
struct host_decl {
	OMAPI_OBJECT_PREAMBLE;
	struct host_decl *n_ipaddr;
	struct host_decl *n_dynamic;
	char *name;
	struct hardware interface;
	struct data_string client_identifier;
	struct option *host_id_option;
	struct data_string host_id;
	/* XXXSK: fixed_addr should be an array of iaddr values,
		  not an option_cache, but it's referenced in a lot of
		  places, so we'll leave it for now. */
	struct option_cache *fixed_addr;
	struct iaddrcidrnetlist *fixed_prefix;
	struct group *group;
	struct group_object *named_group;
	struct data_string auth_key_id;
	int flags;
#define HOST_DECL_DELETED	1
#define HOST_DECL_DYNAMIC	2
#define HOST_DECL_STATIC	4
	/* For v6 the host-identifer option can specify which relay
	   to use when trying to look up an option.  We store the
	   value here. */
	int relays;
};

struct permit {
	struct permit *next;
	enum {
		permit_unknown_clients,
		permit_known_clients,
		permit_authenticated_clients,
		permit_unauthenticated_clients,
		permit_all_clients,
		permit_dynamic_bootp_clients,
		permit_class,
		permit_after
	} type;
	struct class *class;
	TIME after;	/* date after which this clause applies */
};

struct pool {
	OMAPI_OBJECT_PREAMBLE;
	struct pool *next;
	struct group *group;
	struct shared_network *shared_network;
	struct permit *permit_list;
	struct permit *prohibit_list;
	struct lease *active;
	struct lease *expired;
	struct lease *free;
	struct lease *backup;
	struct lease *abandoned;
	struct lease *reserved;
	TIME next_event_time;
	int lease_count;
	int free_leases;
	int backup_leases;
	int index;
	TIME valid_from;        /* deny pool use before this date */
	TIME valid_until;       /* deny pool use after this date */

#if defined (FAILOVER_PROTOCOL)
	dhcp_failover_state_t *failover_peer;
#endif
};

struct shared_network {
	OMAPI_OBJECT_PREAMBLE;
	struct shared_network *next;
	char *name;

#define SHARED_IMPLICIT	  1 /* This network was synthesized. */
	int flags;

	struct subnet *subnets;
	struct interface_info *interface;
	struct pool *pools;
	struct ipv6_pond *ipv6_pond;
	struct group *group;
#if defined (FAILOVER_PROTOCOL)
	dhcp_failover_state_t *failover_peer;
#endif
};

struct subnet {
	OMAPI_OBJECT_PREAMBLE;
	struct subnet *next_subnet;
	struct subnet *next_sibling;
	struct shared_network *shared_network;
	struct interface_info *interface;
	struct iaddr interface_address;
	struct iaddr net;
	struct iaddr netmask;
	int prefix_len;			/* XXX: currently for IPv6 only */
	struct group *group;
};

struct collection {
	struct collection *next;

	const char *name;
	struct class *classes;
};

/* Used as an argument to parse_clasS_decl() */
#define CLASS_TYPE_VENDOR	0
#define CLASS_TYPE_USER		1
#define CLASS_TYPE_CLASS	2
#define CLASS_TYPE_SUBCLASS	3

/* XXX classes must be reference-counted. */
struct class {
	OMAPI_OBJECT_PREAMBLE;
	struct class *nic;		/* Next in collection. */
	struct class *superclass;	/* Set for spawned classes only. */
	char *name;			/* Not set for spawned classes. */

	/* A class may be configured to permit a limited number of leases. */
	int lease_limit;
	int leases_consumed;
	struct lease **billed_leases;

	/* If nonzero, class has not been saved since it was last
	   modified. */
	int dirty;

	/* Hash table containing subclasses. */
	class_hash_t *hash;
	struct data_string hash_string;

	/* Expression used to match class. */
	struct expression *expr;

	/* Expression used to compute subclass identifiers for spawning
	   and to do subclass matching. */
	struct expression *submatch;
	int spawning;

	struct group *group;

	/* Statements to execute if class matches. */
	struct executable_statement *statements;

#define CLASS_DECL_DELETED	1
#define CLASS_DECL_DYNAMIC	2
#define CLASS_DECL_STATIC	4
#define CLASS_DECL_SUBCLASS	8

	int flags;
};

/* DHCP client lease structure... */
struct client_lease {
	struct client_lease *next;		      /* Next lease in list. */
	TIME expiry, renewal, rebind;			  /* Lease timeouts. */
	struct iaddr address;			    /* Address being leased. */
	char *server_name;			     /* Name of boot server. */
	char *filename;		     /* Name of file we're supposed to boot. */
	struct string_list *medium;			  /* Network medium. */
	struct auth_key *key;      /* Key used in basic DHCP authentication. */

	unsigned int is_static : 1;    /* If set, lease is from config file. */
	unsigned int is_bootp: 1;  /* If set, lease was acquired with BOOTP. */

	struct option_state *options;	     /* Options supplied with lease. */
};

/* DHCPv6 lease structures */
struct dhc6_addr {
	struct dhc6_addr *next;
	struct iaddr address;
	u_int8_t plen;

	/* Address state flags. */
	#define DHC6_ADDR_DEPREFFED	0x01
	#define DHC6_ADDR_EXPIRED	0x02
	u_int8_t flags;

	TIME starts;
	u_int32_t preferred_life;
	u_int32_t max_life;

	struct option_state *options;
};

struct dhc6_ia {
	struct dhc6_ia *next;
	unsigned char iaid[4];
	u_int16_t ia_type;

	TIME starts;
	u_int32_t renew;
	u_int32_t rebind;
	struct dhc6_addr *addrs;

	struct option_state *options;
};

struct dhc6_lease {
	struct dhc6_lease *next;
	struct data_string server_id;

	isc_boolean_t released;
	int score;
	u_int8_t pref;

	unsigned char dhcpv6_transaction_id[3];
	struct dhc6_ia *bindings;

	struct option_state *options;
};

/* Possible states in which the client can be. */
enum dhcp_state {
	S_REBOOTING = 1,
	S_INIT = 2,
	S_SELECTING = 3,
	S_REQUESTING = 4,
	S_BOUND = 5,
	S_RENEWING = 6,
	S_REBINDING = 7,
	S_STOPPED = 8
};

/* Authentication and BOOTP policy possibilities (not all values work
   for each). */
enum policy { P_IGNORE, P_ACCEPT, P_PREFER, P_REQUIRE, P_DONT };

/* Configuration information from the config file... */
struct client_config {
	/*
	 * When a message has been received, run these statements
	 * over it.
	 */
	struct group *on_receipt;

	/*
	 * When a message is sent, run these statements.
	 */
	struct group *on_transmission;

	struct option **required_options;  /* Options that MUST be present. */
	struct option **requested_options; /* Options to request (ORO/PRL). */

	TIME timeout;			/* Start to panic if we don't get a
					   lease in this time period when
					   SELECTING. */
	TIME initial_delay;             /* Set initial delay before first
					   transmission. */
	TIME initial_interval;		/* All exponential backoff intervals
					   start here. */
	TIME retry_interval;		/* If the protocol failed to produce
					   an address before the timeout,
					   try the protocol again after this
					   many seconds. */
	TIME select_interval;		/* Wait this many seconds from the
					   first DHCPDISCOVER before
					   picking an offered lease. */
	TIME reboot_timeout;		/* When in INIT-REBOOT, wait this
					   long before giving up and going
					   to INIT. */
	TIME backoff_cutoff;		/* When doing exponential backoff,
					   never back off to an interval
					   longer than this amount. */
	u_int32_t requested_lease;	/* Requested lease time, if user
					   doesn't configure one. */
	struct string_list *media;	/* Possible network media values. */
	char *script_name;		/* Name of config script. */
	char *vendor_space_name;	/* Name of config script. */
	enum policy bootp_policy;
					/* Ignore, accept or prefer BOOTP
					   responses. */
	enum policy auth_policy;
					/* Require authentication, prefer
					   authentication, or don't try to
					   authenticate. */
	struct string_list *medium;	/* Current network medium. */

	struct iaddrmatchlist *reject_list;	/* Servers to reject. */

	int omapi_port;			/* port on which to accept OMAPI
					   connections, or -1 for no
					   listener. */
	int do_forward_update;		/* If nonzero, and if we have the
					   information we need, update the
					   A record for the address we get. */
};

/* Per-interface state used in the dhcp client... */
/* XXX: consider union {}'ing this for v4/v6. */
struct client_state {
	struct client_state *next;
	struct interface_info *interface;
	char *name;

	/* Common values. */
	struct client_config *config;		    /* Client configuration. */
	struct string_list *env;	       /* Client script environment. */
	int envc;			/* Number of entries in environment. */
	struct option_state *sent_options;		 /* Options we sent. */
	enum dhcp_state state;          /* Current state for this interface. */
	TIME last_write;		/* Last time this state was written. */

	/* DHCPv4 values. */
	struct client_lease *active;		  /* Currently active lease. */
	struct client_lease *new;			       /* New lease. */
	struct client_lease *offered_leases;	    /* Leases offered to us. */
	struct client_lease *leases;		/* Leases we currently hold. */
	struct client_lease *alias;			     /* Alias lease. */

	struct iaddr destination;		    /* Where to send packet. */
	u_int32_t xid;					  /* Transaction ID. */
	u_int16_t secs;			    /* secs value from DHCPDISCOVER. */
	TIME first_sending;			/* When was first copy sent? */
	TIME interval;		      /* What's the current resend interval? */
	struct string_list *medium;		   /* Last media type tried. */
	struct dhcp_packet packet;		    /* Outgoing DHCP packet. */
	unsigned packet_length;	       /* Actual length of generated packet. */

	struct iaddr requested_address;	    /* Address we would like to get. */

	/* DHCPv6 values. */
	unsigned char dhcpv6_transaction_id[3];
	u_int8_t refresh_type;

	struct dhc6_lease *active_lease;
	struct dhc6_lease *old_lease;
	struct dhc6_lease *advertised_leases;
	struct dhc6_lease *selected_lease;
	struct dhc6_lease *held_leases;

	struct timeval start_time;
	u_int16_t elapsed;
	int txcount;

	/* See RFC3315 section 14. */
	TIME RT;		/* In hundredths of seconds. */
	TIME IRT;		/* In hundredths of seconds. */
	TIME MRC;		/* Count. */
	TIME MRT;		/* In hundredths of seconds. */
	TIME MRD;		/* In seconds, relative. */
	TIME next_MRD;		/* In seconds, absolute. */

	/* Rather than a state, we use a function that shifts around
	 * depending what stage of life the v6 state machine is in.
	 * This is where incoming packets are dispatched to (sometimes
	 * a no-op).
	 */
	void (*v6_handler)(struct packet *, struct client_state *);

	/*
	 * A pointer to the state of the ddns update for this lease.
	 * It should be set while the update is in progress and cleared
	 * when the update finishes.  It can be used to cancel the
	 * update if we want to do a different update.
	 */
	struct dhcp_ddns_cb *ddns_cb;
};

struct envadd_state {
	struct client_state *client;
	const char *prefix;
};

struct dns_update_state {
	struct client_state *client;
	struct iaddr address;
	int dns_update_timeout;
};

/* Information about each network interface. */

struct interface_info {
	OMAPI_OBJECT_PREAMBLE;
	struct interface_info *next;	/* Next interface in list... */
	struct shared_network *shared_network;
				/* Networks connected to this interface. */
	struct hardware hw_address;	/* Its physical address. */
	struct in_addr *addresses;	/* Addresses associated with this
					 * interface.
					 */
	int address_count;		/* Number of addresses stored. */
	int address_max;		/* Size of addresses buffer. */
	struct in6_addr *v6addresses;	/* IPv6 addresses associated with
					   this interface. */
	int v6address_count;		/* Number of IPv6 addresses associated
					   with this interface. */
	int v6address_max;		/* Maximum number of IPv6 addresses
					   we can store in current buffer. */

	u_int8_t *circuit_id;		/* Circuit ID associated with this
					   interface. */
	unsigned circuit_id_len;	/* Length of Circuit ID, if there
					   is one. */
	u_int8_t *remote_id;		/* Remote ID associated with this
					   interface (if any). */
	unsigned remote_id_len;		/* Length of Remote ID. */

	char name [IFNAMSIZ];		/* Its name... */
	int index;			/* Its if_nametoindex(). */
	int rfdesc;			/* Its read file descriptor. */
	int wfdesc;			/* Its write file descriptor, if
					   different. */
	unsigned char *rbuf;		/* Read buffer, if required. */
	unsigned int rbuf_max;		/* Size of read buffer. */
	size_t rbuf_offset;		/* Current offset into buffer. */
	size_t rbuf_len;		/* Length of data in buffer. */

	struct ifreq *ifp;		/* Pointer to ifreq struct. */
	int configured;			/* If set to 1, interface has at least
					 * one valid IP address.
					 */
	u_int32_t flags;		/* Control flags... */
#define INTERFACE_REQUESTED 1
#define INTERFACE_AUTOMATIC 2
#define INTERFACE_RUNNING 4
#define INTERFACE_DOWNSTREAM 8
#define INTERFACE_UPSTREAM 16
#define INTERFACE_STREAMS (INTERFACE_DOWNSTREAM | INTERFACE_UPSTREAM)

	/* Only used by DHCP client code. */
	struct client_state *client;
# if defined(USE_DLPI_SEND) || defined(USE_DLPI_RECEIVE) || \
     defined(USE_DLPI_HWADDR)
	int dlpi_sap_length;
	struct hardware dlpi_broadcast_addr;
# endif /* DLPI_SEND || DLPI_RECEIVE */
	struct hardware anycast_mac_addr;
};

struct hardware_link {
	struct hardware_link *next;
	char name [IFNAMSIZ];
	struct hardware address;
};

struct leasequeue {
	struct leasequeue *prev;
	struct leasequeue *next;
	struct lease *lease;
};

typedef void (*tvref_t)(void *, void *, const char *, int);
typedef void (*tvunref_t)(void *, const char *, int);
struct timeout {
	struct timeout *next;
	struct timeval when;
	void (*func) (void *);
	void *what;
	tvref_t ref;
	tvunref_t unref;
	isc_timer_t *isc_timeout;
};

struct eventqueue {
	struct eventqueue *next;
	void (*handler)(void *);
};

struct protocol {
	struct protocol *next;
	int fd;
	void (*handler) (struct protocol *);
	void *local;
};

struct dns_query; /* forward */

struct dns_wakeup {
	struct dns_wakeup *next;	/* Next wakeup in chain. */
	void (*func) (struct dns_query *);
};

struct dns_question {
	u_int16_t type;			/* Type of query. */
	u_int16_t class;		/* Class of query. */
	unsigned char data [1];		/* Query data. */
};

struct dns_answer {
	u_int16_t type;			/* Type of answer. */
	u_int16_t class;		/* Class of answer. */
	int count;			/* Number of answers. */
	unsigned char *answers[1];	/* Pointers to answers. */
};

struct dns_query {
	struct dns_query *next;		/* Next query in hash bucket. */
	u_int32_t hash;			/* Hash bucket index. */
	TIME expiry;			/* Query expiry time (zero if not yet
					   answered. */
	u_int16_t id;			/* Query ID (also hash table index) */
	caddr_t waiters;		/* Pointer to list of things waiting
					   on this query. */

	struct dns_question *question;	/* Question, internal format. */
	struct dns_answer *answer;	/* Answer, internal format. */

	unsigned char *query;		/* Query formatted for DNS server. */
	unsigned len;			/* Length of entire query. */
	int sent;			/* The query has been sent. */
	struct dns_wakeup *wakeups;	/* Wakeups to call if this query is
					   answered. */
	struct name_server *next_server;	/* Next server to try. */
	int backoff;			/* Current backoff, in seconds. */
};

#define DNS_ZONE_ACTIVE  0
#define DNS_ZONE_INACTIVE 1
struct dns_zone {
	int refcnt;
	TIME timeout;
	char *name;
	struct option_cache *primary;
	struct option_cache *secondary;
	struct option_cache *primary6;
	struct option_cache *secondary6;
	struct auth_key *key;
	u_int16_t flags;
};

struct icmp_state {
	OMAPI_OBJECT_PREAMBLE;
	int socket;
	void (*icmp_handler) (struct iaddr, u_int8_t *, int);
};

#include "ctrace.h"

/* Bitmask of dhcp option codes. */
typedef unsigned char option_mask [16];

/* DHCP Option mask manipulation macros... */
#define OPTION_ZERO(mask)	(memset (mask, 0, 16))
#define OPTION_SET(mask, bit)	(mask [bit >> 8] |= (1 << (bit & 7)))
#define OPTION_CLR(mask, bit)	(mask [bit >> 8] &= ~(1 << (bit & 7)))
#define OPTION_ISSET(mask, bit)	(mask [bit >> 8] & (1 << (bit & 7)))
#define OPTION_ISCLR(mask, bit)	(!OPTION_ISSET (mask, bit))

/* An option occupies its length plus two header bytes (code and
    length) for every 255 bytes that must be stored. */
#define OPTION_SPACE(x)		((x) + 2 * ((x) / 255 + 1))

/* Default path to dhcpd config file. */
#ifdef DEBUG
#undef _PATH_DHCPD_CONF
#define _PATH_DHCPD_CONF	"dhcpd.conf"
#undef _PATH_DHCPD_DB
#define _PATH_DHCPD_DB		"dhcpd.leases"
#undef _PATH_DHCPD6_DB
#define _PATH_DHCPD6_DB		"dhcpd6.leases"
#undef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID		"dhcpd.pid"
#undef _PATH_DHCPD6_PID
#define _PATH_DHCPD6_PID	"dhcpd6.pid"
#else /* !DEBUG */

#ifndef _PATH_DHCPD_CONF
#define _PATH_DHCPD_CONF	"/etc/dhcpd.conf"
#endif /* DEBUG */

#ifndef _PATH_DHCPD_DB
#define _PATH_DHCPD_DB		LOCALSTATEDIR"/db/dhcpd.leases"
#endif

#ifndef _PATH_DHCPD6_DB
#define _PATH_DHCPD6_DB		LOCALSTATEDIR"/db/dhcpd6.leases"
#endif

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID		LOCALSTATEDIR"/run/dhcpd.pid"
#endif

#ifndef _PATH_DHCPD6_PID
#define _PATH_DHCPD6_PID	LOCALSTATEDIR"/run/dhcpd6.pid"
#endif

#endif /* DEBUG */

#ifndef _PATH_DHCLIENT_CONF
#define _PATH_DHCLIENT_CONF	"/etc/dhclient.conf"
#endif

#ifndef _PATH_DHCLIENT_SCRIPT
#define _PATH_DHCLIENT_SCRIPT	"/sbin/dhclient-script"
#endif

#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID	LOCALSTATEDIR"/run/dhclient.pid"
#endif

#ifndef _PATH_DHCLIENT6_PID
#define _PATH_DHCLIENT6_PID	LOCALSTATEDIR"/run/dhclient6.pid"
#endif

#ifndef _PATH_DHCLIENT_DB
#define _PATH_DHCLIENT_DB	LOCALSTATEDIR"/db/dhclient.leases"
#endif

#ifndef _PATH_DHCLIENT6_DB
#define _PATH_DHCLIENT6_DB	LOCALSTATEDIR"/db/dhclient6.leases"
#endif

#ifndef _PATH_RESOLV_CONF
#define _PATH_RESOLV_CONF	"/etc/resolv.conf"
#endif

#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID	LOCALSTATEDIR"/run/dhcrelay.pid"
#endif

#ifndef _PATH_DHCRELAY6_PID
#define _PATH_DHCRELAY6_PID	LOCALSTATEDIR"/run/dhcrelay6.pid"
#endif

#ifndef DHCPD_LOG_FACILITY
#define DHCPD_LOG_FACILITY	LOG_DAEMON
#endif

#define MAX_TIME 0x7fffffff
#define MIN_TIME 0

						/* these are referenced */
typedef struct hash_table ia_hash_t;
typedef struct hash_table iasubopt_hash_t;

						/* IAADDR/IAPREFIX lease */

struct iasubopt {
	int refcnt;				/* reference count */
	struct in6_addr addr;			/* IPv6 address/prefix */
	u_int8_t plen;				/* iaprefix prefix length */
	binding_state_t state;			/* state */
	struct binding_scope *scope;		/* "set var = value;" */
	time_t hard_lifetime_end_time;		/* time address expires */
	time_t soft_lifetime_end_time;		/* time ephemeral expires */
	u_int32_t prefer;			/* cached preferred lifetime */
	u_int32_t valid;			/* cached valid lifetime */
	struct ia_xx *ia;			/* IA for this lease */
	struct ipv6_pool *ipv6_pool;		/* pool for this lease */
/*
 * For now, just pick an arbitrary time to keep old hard leases
 * around (value in seconds).
 */
#define EXPIRED_IPV6_CLEANUP_TIME (60*60)

	int heap_index;				/* index into heap, or -1
						   (internal use only) */

	/*
	 * A pointer to the state of the ddns update for this lease.
	 * It should be set while the update is in progress and cleared
	 * when the update finishes.  It can be used to cancel the
	 * update if we want to do a different update.
	 */
	struct dhcp_ddns_cb *ddns_cb;

	/* space for the on * executable statements */
	struct on_star on_star;
};

struct ia_xx {
	int refcnt;			/* reference count */
	struct data_string iaid_duid;	/* from the client */
	u_int16_t ia_type;		/* IA_XX */
	int num_iasubopt;		/* number of IAADDR/PREFIX */
	int max_iasubopt;		/* space available for IAADDR/PREFIX */
	time_t cltt;			/* client last transaction time */
	struct iasubopt **iasubopt;	/* pointers to the IAADDR/IAPREFIXs */
};

extern ia_hash_t *ia_na_active;
extern ia_hash_t *ia_ta_active;
extern ia_hash_t *ia_pd_active;

/*!
 *
 * \brief ipv6_pool structure
 *
 * This structure is part of a range of addresses or prefixes.
 * A range6 or prefix6 statement will map to one or more of these
 * with each pool being a simple block of the form xxxx/yyy and
 * all the pools adding up to comprise the entire range.  When
 * choosing an address or prefix the code will walk through the
 * pools until it finds one that is available.
 *
 * The naming for this structure is unfortunate as there is also
 * a v4 pool structure and the two are not equivalent.  The v4
 * pool matches the ipv6_pond structure.  I considered changing the
 * name of this structure but concluded that doing so would be worse
 * than leaving it as is.  Changing it adds some risk and makes for
 * larger differences between the 4.1 & 4.2 code and the 4.3 code.
 *
 */

struct ipv6_pool {
	int refcnt;				/* reference count */
	u_int16_t pool_type;			/* IA_xx */
	struct in6_addr start_addr;		/* first IPv6 address */
	int bits;				/* number of bits, CIDR style */
	int units;				/* allocation unit in bits */
	iasubopt_hash_t *leases;		/* non-free leases */
	int num_active;				/* count of active leases */
	isc_heap_t *active_timeouts;		/* timeouts for active leases */
	int num_inactive;			/* count of inactive leases */
	isc_heap_t *inactive_timeouts;		/* timeouts for expired or
						   released leases */
	struct shared_network *shared_network;	/* shared_network for
						   this pool */
	struct subnet *subnet;			/* subnet for this pool */
	struct ipv6_pond *ipv6_pond;		/* pond for this pool */
};

/*!
 *
 * \brief ipv6_pond structure
 *
 * This structure is the ipv6 version of the v4 pool structure.
 * It contains the address and prefix information via the pointers
 * to the ipv6_pools and the allowability of this pool for a given
 * client via the permit lists and the valid TIMEs.
 *
 */

struct ipv6_pond {
	int refcnt;
	struct ipv6_pond *next;
	struct group *group;
	struct shared_network *shared_network; /* backpointer to the enclosing
						  shared network */
	struct permit *permit_list;	/* allow clients from this list */
	struct permit *prohibit_list;	/* deny clients from this list */
	TIME valid_from;		/* deny pool use before this date */
	TIME valid_until;		/* deny pool use after this date */

	struct ipv6_pool **ipv6_pools;	/* NULL-terminated array */
	int last_ipv6_pool;		/* offset of last IPv6 pool
					   used to issue a lease */
};

/* Flags and state for dhcp_ddns_cb_t */
#define DDNS_UPDATE_ADDR        0x01
#define DDNS_UPDATE_PTR         0x02
#define DDNS_INCLUDE_RRSET      0x04
#define DDNS_CONFLICT_OVERRIDE  0x08
#define DDNS_CLIENT_DID_UPDATE  0x10
#define DDNS_EXECUTE_NEXT       0x20
#define DDNS_ABORT              0x40
#define DDNS_STATIC_LEASE       0x80
#define DDNS_ACTIVE_LEASE	0x100
/*
 * The following two groups are separate and we could reuse
 * values but not reusing them may be useful in the future.
 */
#define DDNS_STATE_CLEANUP          0 // The previous step failed, cleanup

#define DDNS_STATE_ADD_FW_NXDOMAIN  1
#define DDNS_STATE_ADD_FW_YXDHCID   2
#define DDNS_STATE_ADD_PTR          3

#define DDNS_STATE_REM_FW_YXDHCID  17
#define DDNS_STATE_REM_FW_NXRR     18
#define DDNS_STATE_REM_PTR         19

/*
 * Flags for the dns print function
 */
#define DDNS_PRINT_INBOUND  1
#define DDNS_PRINT_OUTBOUND 0

struct dhcp_ddns_cb;

typedef void (*ddns_action_t)(struct dhcp_ddns_cb *ddns_cb,
			      isc_result_t result);

typedef struct dhcp_ddns_cb {
	struct data_string fwd_name;
	struct data_string rev_name;
	struct data_string dhcid;
	struct iaddr address;
	int address_type;

	unsigned long ttl;

	unsigned char zone_name[DHCP_MAXDNS_WIRE];
	isc_sockaddrlist_t zone_server_list;
	isc_sockaddr_t zone_addrs[DHCP_MAXNS];
	int zone_addr_count;
	struct dns_zone *zone;

	u_int16_t flags;
	TIME timeout;
	int state;
	ddns_action_t cur_func;

	struct dhcp_ddns_cb * next_op;

	/* Lease or client state that triggered the ddns operation */
	void *lease;
	struct binding_scope **scope;

	void *transaction;
	void *dataspace;

	dns_rdataclass_t dhcid_class;
	char *lease_tag;
} dhcp_ddns_cb_t;

extern struct ipv6_pool **pools;
extern int num_pools;

/* External definitions... */

HASH_FUNCTIONS_DECL (group, const char *, struct group_object, group_hash_t)
HASH_FUNCTIONS_DECL (universe, const char *, struct universe, universe_hash_t)
HASH_FUNCTIONS_DECL (option_name, const char *, struct option,
		     option_name_hash_t)
HASH_FUNCTIONS_DECL (option_code, const unsigned *, struct option,
		     option_code_hash_t)
HASH_FUNCTIONS_DECL (dns_zone, const char *, struct dns_zone, dns_zone_hash_t)
HASH_FUNCTIONS_DECL(lease_ip, const unsigned char *, struct lease,
		    lease_ip_hash_t)
HASH_FUNCTIONS_DECL(lease_id, const unsigned char *, struct lease,
		    lease_id_hash_t)
HASH_FUNCTIONS_DECL (host, const unsigned char *, struct host_decl, host_hash_t)
HASH_FUNCTIONS_DECL (class, const char *, struct class, class_hash_t)

/* options.c */

extern struct option *vendor_cfg_option;
int parse_options (struct packet *);
int parse_option_buffer (struct option_state *, const unsigned char *,
			 unsigned, struct universe *);
struct universe *find_option_universe (struct option *, const char *);
int parse_encapsulated_suboptions (struct option_state *, struct option *,
				   const unsigned char *, unsigned,
				   struct universe *, const char *);
int cons_options (struct packet *, struct dhcp_packet *, struct lease *,
		  struct client_state *,
		  int, struct option_state *, struct option_state *,
		  struct binding_scope **,
		  int, int, int, struct data_string *, const char *);
int fqdn_universe_decode (struct option_state *,
			  const unsigned char *, unsigned, struct universe *);
struct option_cache *
lookup_fqdn6_option(struct universe *universe, struct option_state *options,
		    unsigned code);
void
save_fqdn6_option(struct universe *universe, struct option_state *options,
		  struct option_cache *oc, isc_boolean_t appendp);
void
delete_fqdn6_option(struct universe *universe, struct option_state *options,
		    int code);
void
fqdn6_option_space_foreach(struct packet *packet, struct lease *lease,
			   struct client_state *client_state,
			   struct option_state *in_options,
			   struct option_state *cfg_options,
			   struct binding_scope **scope,
			   struct universe *u, void *stuff,
			   void (*func)(struct option_cache *,
					struct packet *,
					struct lease *,
					struct client_state *,
					struct option_state *,
					struct option_state *,
					struct binding_scope **,
					struct universe *, void *));
int
fqdn6_option_space_encapsulate(struct data_string *result,
			       struct packet *packet, struct lease *lease,
			       struct client_state *client_state,
			       struct option_state *in_options,
			       struct option_state *cfg_options,
			       struct binding_scope **scope,
			       struct universe *universe);
int
fqdn6_universe_decode(struct option_state *options,
		      const unsigned char *buffer, unsigned length,
		      struct universe *u);
int append_option(struct data_string *dst, struct universe *universe,
		  struct option *option, struct data_string *src);
int
store_options(int *ocount,
	      unsigned char *buffer, unsigned buflen, unsigned index,
	      struct packet *packet, struct lease *lease,
	      struct client_state *client_state,
	      struct option_state *in_options,
	      struct option_state *cfg_options,
	      struct binding_scope **scope,
	      unsigned *priority_list, int priority_len,
	      unsigned first_cutoff, int second_cutoff, int terminate,
	      const char *vuname);
int store_options6(char *, int, struct option_state *, struct packet *,
		   const int *, struct data_string *);
int format_has_text(const char *);
int format_min_length(const char *, struct option_cache *);
const char *pretty_print_option (struct option *, const unsigned char *,
				 unsigned, int, int);
int pretty_escape(char **, char *, const unsigned char **,
		  const unsigned char *);
int get_option (struct data_string *, struct universe *,
		struct packet *, struct lease *, struct client_state *,
		struct option_state *, struct option_state *,
		struct option_state *, struct binding_scope **, unsigned,
		const char *, int);
void set_option (struct universe *, struct option_state *,
		 struct option_cache *, enum statement_op);
struct option_cache *lookup_option (struct universe *,
				    struct option_state *, unsigned);
struct option_cache *lookup_hashed_option (struct universe *,
					   struct option_state *,
					   unsigned);
struct option_cache *next_hashed_option(struct universe *,
					struct option_state *,
					struct option_cache *);
int save_option_buffer (struct universe *, struct option_state *,
			struct buffer *, unsigned char *, unsigned,
			unsigned, int);
int append_option_buffer(struct universe *, struct option_state *,
			 struct buffer *, unsigned char *, unsigned,
			 unsigned, int);
void build_server_oro(struct data_string *, struct option_state *,
		      const char *, int);
void save_option(struct universe *, struct option_state *,
		 struct option_cache *);
void also_save_option(struct universe *, struct option_state *,
		      struct option_cache *);
void save_hashed_option(struct universe *, struct option_state *,
			struct option_cache *, isc_boolean_t appendp);
void delete_option (struct universe *, struct option_state *, int);
void delete_hashed_option (struct universe *,
			   struct option_state *, int);
int option_cache_dereference (struct option_cache **,
			      const char *, int);
int hashed_option_state_dereference (struct universe *,
				     struct option_state *,
				     const char *, int);
int store_option (struct data_string *,
		  struct universe *, struct packet *, struct lease *,
		  struct client_state *,
		  struct option_state *, struct option_state *,
		  struct binding_scope **, struct option_cache *);
int option_space_encapsulate (struct data_string *,
			      struct packet *, struct lease *,
			      struct client_state *,
			      struct option_state *,
			      struct option_state *,
			      struct binding_scope **,
			      struct data_string *);
int hashed_option_space_encapsulate (struct data_string *,
				     struct packet *, struct lease *,
				     struct client_state *,
				     struct option_state *,
				     struct option_state *,
				     struct binding_scope **,
				     struct universe *);
int nwip_option_space_encapsulate (struct data_string *,
				   struct packet *, struct lease *,
				   struct client_state *,
				   struct option_state *,
				   struct option_state *,
				   struct binding_scope **,
				   struct universe *);
int fqdn_option_space_encapsulate (struct data_string *,
				   struct packet *, struct lease *,
				   struct client_state *,
				   struct option_state *,
				   struct option_state *,
				   struct binding_scope **,
				   struct universe *);
void suboption_foreach (struct packet *, struct lease *, struct client_state *,
			struct option_state *, struct option_state *,
			struct binding_scope **, struct universe *, void *,
			void (*) (struct option_cache *, struct packet *,
				  struct lease *, struct client_state *,
				  struct option_state *, struct option_state *,
				  struct binding_scope **,
				  struct universe *, void *),
			struct option_cache *, const char *);
void option_space_foreach (struct packet *, struct lease *,
			   struct client_state *,
			   struct option_state *,
			   struct option_state *,
			   struct binding_scope **,
			   struct universe *, void *,
			   void (*) (struct option_cache *,
				     struct packet *,
				     struct lease *, struct client_state *,
				     struct option_state *,
				     struct option_state *,
				     struct binding_scope **,
				     struct universe *, void *));
void hashed_option_space_foreach (struct packet *, struct lease *,
				  struct client_state *,
				  struct option_state *,
				  struct option_state *,
				  struct binding_scope **,
				  struct universe *, void *,
				  void (*) (struct option_cache *,
					    struct packet *,
					    struct lease *,
					    struct client_state *,
					    struct option_state *,
					    struct option_state *,
					    struct binding_scope **,
					    struct universe *, void *));
int linked_option_get (struct data_string *, struct universe *,
		       struct packet *, struct lease *,
		       struct client_state *,
		       struct option_state *, struct option_state *,
		       struct option_state *, struct binding_scope **,
		       unsigned);
int linked_option_state_dereference (struct universe *,
				     struct option_state *,
				     const char *, int);
void save_linked_option(struct universe *, struct option_state *,
			struct option_cache *, isc_boolean_t appendp);
void linked_option_space_foreach (struct packet *, struct lease *,
				  struct client_state *,
				  struct option_state *,
				  struct option_state *,
				  struct binding_scope **,
				  struct universe *, void *,
				  void (*) (struct option_cache *,
					    struct packet *,
					    struct lease *,
					    struct client_state *,
					    struct option_state *,
					    struct option_state *,
					    struct binding_scope **,
					    struct universe *, void *));
int linked_option_space_encapsulate (struct data_string *, struct packet *,
				     struct lease *, struct client_state *,
				     struct option_state *,
				     struct option_state *,
				     struct binding_scope **,
				     struct universe *);
void delete_linked_option (struct universe *, struct option_state *, int);
struct option_cache *lookup_linked_option (struct universe *,
					   struct option_state *, unsigned);
void do_packet (struct interface_info *,
		struct dhcp_packet *, unsigned,
		unsigned int, struct iaddr, struct hardware *);
void do_packet6(struct interface_info *, const char *,
		int, int, const struct iaddr *, isc_boolean_t);
int packet6_len_okay(const char *, int);

int validate_packet(struct packet *);

int add_option(struct option_state *options,
	       unsigned int option_num,
	       void *data,
	       unsigned int data_len);

/* dhcpd.c */
extern struct timeval cur_tv;
#define cur_time cur_tv.tv_sec

extern int ddns_update_style;
extern int dont_use_fsync;

extern const char *path_dhcpd_conf;
extern const char *path_dhcpd_db;
extern const char *path_dhcpd_pid;

extern int dhcp_max_agent_option_packet_length;
extern struct eventqueue *rw_queue_empty;

int main(int, char **);
void postconf_initialization(int);
void postdb_startup(void);
void cleanup (void);
void lease_pinged (struct iaddr, u_int8_t *, int);
void lease_ping_timeout (void *);
int dhcpd_interface_setup_hook (struct interface_info *ip, struct iaddr *ia);
extern enum dhcp_shutdown_state shutdown_state;
isc_result_t dhcp_io_shutdown (omapi_object_t *, void *);
isc_result_t dhcp_set_control_state (control_object_state_t oldstate,
				     control_object_state_t newstate);
#if defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void relinquish_ackqueue(void);
#endif

/* conflex.c */
isc_result_t new_parse (struct parse **, int,
			char *, unsigned, const char *, int);
isc_result_t end_parse (struct parse **);
isc_result_t save_parse_state(struct parse *cfile);
isc_result_t restore_parse_state(struct parse *cfile);
enum dhcp_token next_token (const char **, unsigned *, struct parse *);
enum dhcp_token peek_token (const char **, unsigned *, struct parse *);
enum dhcp_token next_raw_token(const char **rval, unsigned *rlen,
			       struct parse *cfile);
enum dhcp_token peek_raw_token(const char **rval, unsigned *rlen,
			       struct parse *cfile);
/*
 * Use skip_token when we are skipping a token we have previously
 * used peek_token on as we know what the result will be in this case.
 */
#define skip_token(a,b,c) ((void) next_token((a),(b),(c)))


/* confpars.c */
void parse_trace_setup (void);
isc_result_t readconf (void);
isc_result_t read_conf_file (const char *, struct group *, int, int);
#if defined (TRACING)
void trace_conf_input (trace_type_t *, unsigned, char *);
void trace_conf_stop (trace_type_t *ttype);
#endif
isc_result_t conf_file_subparse (struct parse *, struct group *, int);
isc_result_t lease_file_subparse (struct parse *);
int parse_statement (struct parse *, struct group *, int,
		     struct host_decl *, int);
#if defined (FAILOVER_PROTOCOL)
void parse_failover_peer (struct parse *, struct group *, int);
void parse_failover_state_declaration (struct parse *,
				       dhcp_failover_state_t *);
void parse_failover_state (struct parse *,
				  enum failover_state *, TIME *);
#endif
int permit_list_match (struct permit *, struct permit *);
void parse_pool_statement (struct parse *, struct group *, int);
int parse_lbrace (struct parse *);
void parse_host_declaration (struct parse *, struct group *);
int parse_class_declaration (struct class **, struct parse *,
			     struct group *, int);
void parse_shared_net_declaration (struct parse *, struct group *);
void parse_subnet_declaration (struct parse *,
			       struct shared_network *);
void parse_subnet6_declaration (struct parse *,
				struct shared_network *);
void parse_group_declaration (struct parse *, struct group *);
int parse_fixed_addr_param (struct option_cache **,
			    struct parse *, enum dhcp_token);
int parse_lease_declaration (struct lease **, struct parse *);
int parse_ip6_addr(struct parse *, struct iaddr *);
int parse_ip6_addr_expr(struct expression **, struct parse *);
int parse_ip6_prefix(struct parse *, struct iaddr *, u_int8_t *);
void parse_address_range (struct parse *, struct group *, int,
			  struct pool *, struct lease **);
void parse_address_range6(struct parse *cfile, struct group *group,
			  struct ipv6_pond *);
void parse_prefix6(struct parse *cfile, struct group *group,
			  struct ipv6_pond *);
void parse_fixed_prefix6(struct parse *cfile, struct host_decl *host_decl);
void parse_ia_na_declaration(struct parse *);
void parse_ia_ta_declaration(struct parse *);
void parse_ia_pd_declaration(struct parse *);
void parse_server_duid(struct parse *cfile);
void parse_server_duid_conf(struct parse *cfile);
void parse_pool6_statement (struct parse *, struct group *, int);

/* ddns.c */
int ddns_updates(struct packet *, struct lease *, struct lease *,
		 struct iasubopt *, struct iasubopt *, struct option_state *);
isc_result_t ddns_removals(struct lease *, struct iasubopt *,
			   struct dhcp_ddns_cb *, isc_boolean_t);
#if defined (TRACING)
void trace_ddns_init(void);
#endif

/* parse.c */
void add_enumeration (struct enumeration *);
struct enumeration *find_enumeration (const char *, int);
struct enumeration_value *find_enumeration_value (const char *, int,
						  unsigned *,
						  const char *);
void skip_to_semi (struct parse *);
void skip_to_rbrace (struct parse *, int);
int parse_semi (struct parse *);
int parse_string (struct parse *, char **, unsigned *);
char *parse_host_name (struct parse *);
int parse_ip_addr_or_hostname (struct expression **,
			       struct parse *, int);
void parse_hardware_param (struct parse *, struct hardware *);
void parse_lease_time (struct parse *, TIME *);
unsigned char *parse_numeric_aggregate (struct parse *,
					unsigned char *, unsigned *,
					int, int, unsigned);
void convert_num (struct parse *, unsigned char *, const char *,
		  int, unsigned);
TIME parse_date (struct parse *);
TIME parse_date_core(struct parse *);
isc_result_t parse_option_name (struct parse *, int, int *,
				struct option **);
void parse_option_space_decl (struct parse *);
int parse_option_code_definition (struct parse *, struct option *);
int parse_base64 (struct data_string *, struct parse *);
int parse_cshl (struct data_string *, struct parse *);
int parse_executable_statement (struct executable_statement **,
				struct parse *, int *,
				enum expression_context);
int parse_executable_statements (struct executable_statement **,
				 struct parse *, int *,
				 enum expression_context);
int parse_zone (struct dns_zone *, struct parse *);
int parse_key (struct parse *);
int parse_on_statement (struct executable_statement **,
			struct parse *, int *);
int parse_switch_statement (struct executable_statement **,
			    struct parse *, int *);
int parse_case_statement (struct executable_statement **,
			  struct parse *, int *,
			  enum expression_context);
int parse_if_statement (struct executable_statement **,
			struct parse *, int *);
int parse_boolean_expression (struct expression **,
			      struct parse *, int *);
int parse_boolean (struct parse *);
int parse_data_expression (struct expression **,
			   struct parse *, int *);
int parse_numeric_expression (struct expression **,
			      struct parse *, int *);
int parse_dns_expression (struct expression **, struct parse *, int *);
int parse_non_binary (struct expression **, struct parse *, int *,
		      enum expression_context);
int parse_expression (struct expression **, struct parse *, int *,
		      enum expression_context,
		      struct expression **, enum expr_op);
int parse_option_data(struct expression **expr, struct parse *cfile,
		      int lookups, struct option *option);
int parse_option_statement (struct executable_statement **,
			    struct parse *, int,
			    struct option *, enum statement_op);
int parse_option_token (struct expression **, struct parse *,
			const char **, struct expression *, int, int);
int parse_allow_deny (struct option_cache **, struct parse *, int);
int parse_auth_key (struct data_string *, struct parse *);
int parse_warn (struct parse *, const char *, ...)
	__attribute__((__format__(__printf__,2,3)));
struct expression *parse_domain_list(struct parse *cfile, int);


/* tree.c */
extern struct binding_scope *global_scope;
pair cons (caddr_t, pair);
int make_const_option_cache (struct option_cache **, struct buffer **,
			     u_int8_t *, unsigned, struct option *,
			     const char *, int);
int make_host_lookup (struct expression **, const char *);
int enter_dns_host (struct dns_host_entry **, const char *);
int make_const_data (struct expression **,
		     const unsigned char *, unsigned, int, int,
		     const char *, int);
int make_const_int (struct expression **, unsigned long);
int make_concat (struct expression **,
		 struct expression *, struct expression *);
int make_encapsulation (struct expression **, struct data_string *);
int make_substring (struct expression **, struct expression *,
		    struct expression *, struct expression *);
int make_limit (struct expression **, struct expression *, int);
int make_let (struct executable_statement **, const char *);
int option_cache (struct option_cache **, struct data_string *,
		  struct expression *, struct option *,
		  const char *, int);
int evaluate_expression (struct binding_value **, struct packet *,
			 struct lease *, struct client_state *,
			 struct option_state *, struct option_state *,
			 struct binding_scope **, struct expression *,
			 const char *, int);
int binding_value_dereference (struct binding_value **, const char *, int);
int evaluate_boolean_expression (int *,
				 struct packet *,  struct lease *,
				 struct client_state *,
				 struct option_state *,
				 struct option_state *,
				 struct binding_scope **,
				 struct expression *);
int evaluate_data_expression (struct data_string *,
			      struct packet *, struct lease *,
			      struct client_state *,
			      struct option_state *,
			      struct option_state *,
			      struct binding_scope **,
			      struct expression *,
			      const char *, int);
int evaluate_numeric_expression (unsigned long *, struct packet *,
				 struct lease *, struct client_state *,
				 struct option_state *, struct option_state *,
				 struct binding_scope **,
				 struct expression *);
int evaluate_option_cache (struct data_string *,
			   struct packet *, struct lease *,
			   struct client_state *,
			   struct option_state *, struct option_state *,
			   struct binding_scope **,
			   struct option_cache *,
			   const char *, int);
int evaluate_boolean_option_cache (int *,
				   struct packet *, struct lease *,
				   struct client_state *,
				   struct option_state *,
				   struct option_state *,
				   struct binding_scope **,
				   struct option_cache *,
				   const char *, int);
int evaluate_boolean_expression_result (int *,
					struct packet *, struct lease *,
					struct client_state *,
					struct option_state *,
					struct option_state *,
					struct binding_scope **,
					struct expression *);
void expression_dereference (struct expression **, const char *, int);
int is_dns_expression (struct expression *);
int is_boolean_expression (struct expression *);
int is_data_expression (struct expression *);
int is_numeric_expression (struct expression *);
int is_compound_expression (struct expression *);
int op_precedence (enum expr_op, enum expr_op);
enum expression_context expression_context (struct expression *);
enum expression_context op_context (enum expr_op);
int write_expression (FILE *, struct expression *, int, int, int);
struct binding *find_binding (struct binding_scope *, const char *);
int free_bindings (struct binding_scope *, const char *, int);
int binding_scope_dereference (struct binding_scope **,
			       const char *, int);
int fundef_dereference (struct fundef **, const char *, int);
int data_subexpression_length (int *, struct expression *);
int expr_valid_for_context (struct expression *, enum expression_context);
struct binding *create_binding (struct binding_scope **, const char *);
int bind_ds_value (struct binding_scope **,
		   const char *, struct data_string *);
int find_bound_string (struct data_string *,
		       struct binding_scope *, const char *);
int unset (struct binding_scope *, const char *);
int data_string_sprintfa(struct data_string *ds, const char *fmt, ...);

/* dhcp.c */
extern int outstanding_pings;
extern int max_outstanding_acks;
extern int max_ack_delay_secs;
extern int max_ack_delay_usecs;

void dhcp (struct packet *);
void dhcpdiscover (struct packet *, int);
void dhcprequest (struct packet *, int, struct lease *);
void dhcprelease (struct packet *, int);
void dhcpdecline (struct packet *, int);
void dhcpinform (struct packet *, int);
void nak_lease (struct packet *, struct iaddr *cip);
void ack_lease (struct packet *, struct lease *,
		unsigned int, TIME, char *, int, struct host_decl *);
void delayed_ack_enqueue(struct lease *);
void commit_leases_readerdry(void *);
void flush_ackqueue(void *);
void dhcp_reply (struct lease *);
int find_lease (struct lease **, struct packet *,
		struct shared_network *, int *, int *, struct lease *,
		const char *, int);
int mockup_lease (struct lease **, struct packet *,
		  struct shared_network *,
		  struct host_decl *);
void static_lease_dereference (struct lease *, const char *, int);

int allocate_lease (struct lease **, struct packet *,
		    struct pool *, int *);
int permitted (struct packet *, struct permit *);
int locate_network (struct packet *);
int parse_agent_information_option (struct packet *, int, u_int8_t *);
unsigned cons_agent_information_options (struct option_state *,
					 struct dhcp_packet *,
					 unsigned, unsigned);
void get_server_source_address(struct in_addr *from,
			       struct option_state *options,
			       struct option_state *out_options,
			       struct packet *packet);
void setup_server_source_address(struct in_addr *from,
				 struct option_state *options,
				 struct packet *packet);

/* dhcpleasequery.c */
void dhcpleasequery (struct packet *, int);
void dhcpv6_leasequery (struct data_string *, struct packet *);

/* dhcpv6.c */
isc_boolean_t server_duid_isset(void);
void copy_server_duid(struct data_string *ds, const char *file, int line);
void set_server_duid(struct data_string *new_duid);
isc_result_t set_server_duid_from_option(void);
void set_server_duid_type(int type);
isc_result_t generate_new_server_duid(void);
isc_result_t get_client_id(struct packet *, struct data_string *);
void dhcpv6(struct packet *);

/* bootp.c */
void bootp (struct packet *);

/* memory.c */
extern int (*group_write_hook) (struct group_object *);
extern struct group *root_group;
extern group_hash_t *group_name_hash;
isc_result_t delete_group (struct group_object *, int);
isc_result_t supersede_group (struct group_object *, int);
int clone_group (struct group **, struct group *, const char *, int);
int write_group (struct group_object *);

/* salloc.c */
void relinquish_lease_hunks (void);
struct lease *new_leases (unsigned, const char *, int);
#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void relinquish_free_lease_states (void);
#endif
OMAPI_OBJECT_ALLOC_DECL (lease, struct lease, dhcp_type_lease)
OMAPI_OBJECT_ALLOC_DECL (class, struct class, dhcp_type_class)
OMAPI_OBJECT_ALLOC_DECL (subclass, struct class, dhcp_type_subclass)
OMAPI_OBJECT_ALLOC_DECL (pool, struct pool, dhcp_type_pool)
OMAPI_OBJECT_ALLOC_DECL (host, struct host_decl, dhcp_type_host)

/* alloc.c */
OMAPI_OBJECT_ALLOC_DECL (subnet, struct subnet, dhcp_type_subnet)
OMAPI_OBJECT_ALLOC_DECL (shared_network, struct shared_network,
			 dhcp_type_shared_network)
OMAPI_OBJECT_ALLOC_DECL (group_object, struct group_object, dhcp_type_group)
OMAPI_OBJECT_ALLOC_DECL (dhcp_control,
			 dhcp_control_object_t, dhcp_type_control)

#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void relinquish_free_pairs (void);
void relinquish_free_expressions (void);
void relinquish_free_binding_values (void);
void relinquish_free_option_caches (void);
void relinquish_free_packets (void);
#endif

int option_chain_head_allocate (struct option_chain_head **,
				const char *, int);
int option_chain_head_reference (struct option_chain_head **,
				 struct option_chain_head *,
				 const char *, int);
int option_chain_head_dereference (struct option_chain_head **,
				   const char *, int);
int group_allocate (struct group **, const char *, int);
int group_reference (struct group **, struct group *, const char *, int);
int group_dereference (struct group **, const char *, int);
struct dhcp_packet *new_dhcp_packet (const char *, int);
struct protocol *new_protocol (const char *, int);
struct lease_state *new_lease_state (const char *, int);
struct domain_search_list *new_domain_search_list (const char *, int);
struct name_server *new_name_server (const char *, int);
void free_name_server (struct name_server *, const char *, int);
struct option *new_option (const char *, const char *, int);
int option_reference(struct option **dest, struct option *src,
		     const char * file, int line);
int option_dereference(struct option **dest, const char *file, int line);
struct universe *new_universe (const char *, int);
void free_universe (struct universe *, const char *, int);
void free_domain_search_list (struct domain_search_list *,
			      const char *, int);
void free_lease_state (struct lease_state *, const char *, int);
void free_protocol (struct protocol *, const char *, int);
void free_dhcp_packet (struct dhcp_packet *, const char *, int);
struct client_lease *new_client_lease (const char *, int);
void free_client_lease (struct client_lease *, const char *, int);
struct permit *new_permit (const char *, int);
void free_permit (struct permit *, const char *, int);
pair new_pair (const char *, int);
void free_pair (pair, const char *, int);
int expression_allocate (struct expression **, const char *, int);
int expression_reference (struct expression **,
			  struct expression *, const char *, int);
void free_expression (struct expression *, const char *, int);
int binding_value_allocate (struct binding_value **,
			    const char *, int);
int binding_value_reference (struct binding_value **,
			     struct binding_value *,
			     const char *, int);
void free_binding_value (struct binding_value *, const char *, int);
int fundef_allocate (struct fundef **, const char *, int);
int fundef_reference (struct fundef **,
		      struct fundef *, const char *, int);
int option_cache_allocate (struct option_cache **, const char *, int);
int option_cache_reference (struct option_cache **,
			    struct option_cache *, const char *, int);
int buffer_allocate (struct buffer **, unsigned, const char *, int);
int buffer_reference (struct buffer **, struct buffer *,
		      const char *, int);
int buffer_dereference (struct buffer **, const char *, int);
int dns_host_entry_allocate (struct dns_host_entry **,
			     const char *, const char *, int);
int dns_host_entry_reference (struct dns_host_entry **,
			      struct dns_host_entry *,
			      const char *, int);
int dns_host_entry_dereference (struct dns_host_entry **,
				const char *, int);
int option_state_allocate (struct option_state **, const char *, int);
int option_state_reference (struct option_state **,
			    struct option_state *, const char *, int);
int option_state_dereference (struct option_state **,
			      const char *, int);
void data_string_copy(struct data_string *, const struct data_string *,
		      const char *, int);
void data_string_forget (struct data_string *, const char *, int);
void data_string_truncate (struct data_string *, int);
int executable_statement_allocate (struct executable_statement **,
				   const char *, int);
int executable_statement_reference (struct executable_statement **,
				    struct executable_statement *,
				    const char *, int);
int packet_allocate (struct packet **, const char *, int);
int packet_reference (struct packet **,
		      struct packet *, const char *, int);
int packet_dereference (struct packet **, const char *, int);
int binding_scope_allocate (struct binding_scope **,
			    const char *, int);
int binding_scope_reference (struct binding_scope **,
			     struct binding_scope *,
			     const char *, int);
int dns_zone_allocate (struct dns_zone **, const char *, int);
int dns_zone_reference (struct dns_zone **,
			struct dns_zone *, const char *, int);

/* print.c */
#define DEFAULT_TIME_FORMAT 0
#define LOCAL_TIME_FORMAT   1
extern int db_time_format;
char *quotify_string (const char *, const char *, int);
char *quotify_buf (const unsigned char *, unsigned, const char *, int);
char *print_base64 (const unsigned char *, unsigned, const char *, int);
char *print_hw_addr (const int, const int, const unsigned char *);
void print_lease (struct lease *);
void dump_raw (const unsigned char *, unsigned);
void dump_packet_option (struct option_cache *, struct packet *,
			 struct lease *, struct client_state *,
			 struct option_state *, struct option_state *,
			 struct binding_scope **, struct universe *, void *);
void dump_packet (struct packet *);
void hash_dump (struct hash_table *);
char *print_hex (unsigned, const u_int8_t *, unsigned, unsigned);
void print_hex_only (unsigned, const u_int8_t *, unsigned, char *);
void print_hex_or_string (unsigned, const u_int8_t *, unsigned, char *);
#define print_hex_1(len, data, limit) print_hex(len, data, limit, 0)
#define print_hex_2(len, data, limit) print_hex(len, data, limit, 1)
#define print_hex_3(len, data, limit) print_hex(len, data, limit, 2)
char *print_dotted_quads (unsigned, const u_int8_t *);
char *print_dec_1 (unsigned long);
char *print_dec_2 (unsigned long);
void print_expression (const char *, struct expression *);
int token_print_indent_concat (FILE *, int, int,
			       const char *, const char *, ...);
int token_indent_data_string (FILE *, int, int, const char *, const char *,
			      struct data_string *);
int token_print_indent (FILE *, int, int,
			const char *, const char *, const char *);
void indent_spaces (FILE *, int);
#if defined (NSUPDATE)
void print_dns_status (int, struct dhcp_ddns_cb *, isc_result_t);
#endif
const char *print_time(TIME);

void get_hw_addr(const char *name, struct hardware *hw);

/* socket.c */
#if defined (USE_SOCKET_SEND) || defined (USE_SOCKET_RECEIVE) \
	|| defined (USE_SOCKET_FALLBACK)
int if_register_socket(struct interface_info *, int, int *, struct in6_addr *);
#endif

#if defined (USE_SOCKET_FALLBACK) && !defined (USE_SOCKET_SEND)
void if_reinitialize_fallback (struct interface_info *);
void if_register_fallback (struct interface_info *);
ssize_t send_fallback (struct interface_info *,
		       struct packet *, struct dhcp_packet *, size_t,
		       struct in_addr,
		       struct sockaddr_in *, struct hardware *);
ssize_t send_fallback6(struct interface_info *, struct packet *,
		       struct dhcp_packet *, size_t, struct in6_addr *,
		       struct sockaddr_in6 *, struct hardware *);
#endif

#ifdef USE_SOCKET_SEND
void if_reinitialize_send (struct interface_info *);
void if_register_send (struct interface_info *);
void if_deregister_send (struct interface_info *);
ssize_t send_packet (struct interface_info *,
		     struct packet *, struct dhcp_packet *, size_t,
		     struct in_addr,
		     struct sockaddr_in *, struct hardware *);
#endif
ssize_t send_packet6(struct interface_info *, const unsigned char *, size_t,
		     struct sockaddr_in6 *);
#ifdef USE_SOCKET_RECEIVE
void if_reinitialize_receive (struct interface_info *);
void if_register_receive (struct interface_info *);
void if_deregister_receive (struct interface_info *);
ssize_t receive_packet (struct interface_info *,
			unsigned char *, size_t,
			struct sockaddr_in *, struct hardware *);
#endif

#if defined (USE_SOCKET_FALLBACK)
isc_result_t fallback_discard (omapi_object_t *);
#endif

#if defined (USE_SOCKET_SEND)
int can_unicast_without_arp (struct interface_info *);
int can_receive_unicast_unconfigured (struct interface_info *);
int supports_multiple_interfaces (struct interface_info *);
void maybe_setup_fallback (void);
#endif

void if_register6(struct interface_info *info, int do_multicast);
void if_register_linklocal6(struct interface_info *info);
ssize_t receive_packet6(struct interface_info *interface,
			unsigned char *buf, size_t len,
			struct sockaddr_in6 *from, struct in6_addr *to_addr,
			unsigned int *if_index);
void if_deregister6(struct interface_info *info);


/* bpf.c */
#if defined (USE_BPF_SEND) || defined (USE_BPF_RECEIVE)
int if_register_bpf (struct interface_info *);
#endif
#ifdef USE_BPF_SEND
void if_reinitialize_send (struct interface_info *);
void if_register_send (struct interface_info *);
void if_deregister_send (struct interface_info *);
ssize_t send_packet (struct interface_info *,
		     struct packet *, struct dhcp_packet *, size_t,
		     struct in_addr,
		     struct sockaddr_in *, struct hardware *);
#endif
#ifdef USE_BPF_RECEIVE
void if_reinitialize_receive (struct interface_info *);
void if_register_receive (struct interface_info *);
void if_deregister_receive (struct interface_info *);
ssize_t receive_packet (struct interface_info *,
			unsigned char *, size_t,
			struct sockaddr_in *, struct hardware *);
#endif
#if defined (USE_BPF_SEND)
int can_unicast_without_arp (struct interface_info *);
int can_receive_unicast_unconfigured (struct interface_info *);
int supports_multiple_interfaces (struct interface_info *);
void maybe_setup_fallback (void);
#endif

/* lpf.c */
#if defined (USE_LPF_SEND) || defined (USE_LPF_RECEIVE)
int if_register_lpf (struct interface_info *);
#endif
#ifdef USE_LPF_SEND
void if_reinitialize_send (struct interface_info *);
void if_register_send (struct interface_info *);
void if_deregister_send (struct interface_info *);
ssize_t send_packet (struct interface_info *,
		     struct packet *, struct dhcp_packet *, size_t,
		     struct in_addr,
		     struct sockaddr_in *, struct hardware *);
#endif
#ifdef USE_LPF_RECEIVE
void if_reinitialize_receive (struct interface_info *);
void if_register_receive (struct interface_info *);
void if_deregister_receive (struct interface_info *);
ssize_t receive_packet (struct interface_info *,
			unsigned char *, size_t,
			struct sockaddr_in *, struct hardware *);
#endif
#if defined (USE_LPF_SEND)
int can_unicast_without_arp (struct interface_info *);
int can_receive_unicast_unconfigured (struct interface_info *);
int supports_multiple_interfaces (struct interface_info *);
void maybe_setup_fallback (void);
#endif

/* nit.c */
#if defined (USE_NIT_SEND) || defined (USE_NIT_RECEIVE)
int if_register_nit (struct interface_info *);
#endif

#ifdef USE_NIT_SEND
void if_reinitialize_send (struct interface_info *);
void if_register_send (struct interface_info *);
void if_deregister_send (struct interface_info *);
ssize_t send_packet (struct interface_info *,
		     struct packet *, struct dhcp_packet *, size_t,
		     struct in_addr,
		     struct sockaddr_in *, struct hardware *);
#endif
#ifdef USE_NIT_RECEIVE
void if_reinitialize_receive (struct interface_info *);
void if_register_receive (struct interface_info *);
void if_deregister_receive (struct interface_info *);
ssize_t receive_packet (struct interface_info *,
			unsigned char *, size_t,
			struct sockaddr_in *, struct hardware *);
#endif
#if defined (USE_NIT_SEND)
int can_unicast_without_arp (struct interface_info *);
int can_receive_unicast_unconfigured (struct interface_info *);
int supports_multiple_interfaces (struct interface_info *);
void maybe_setup_fallback (void);
#endif

/* dlpi.c */
#if defined (USE_DLPI_SEND) || defined (USE_DLPI_RECEIVE)
int if_register_dlpi (struct interface_info *);
#endif

#ifdef USE_DLPI_SEND
int can_unicast_without_arp (struct interface_info *);
int can_receive_unicast_unconfigured (struct interface_info *);
void if_reinitialize_send (struct interface_info *);
void if_register_send (struct interface_info *);
void if_deregister_send (struct interface_info *);
ssize_t send_packet (struct interface_info *,
		     struct packet *, struct dhcp_packet *, size_t,
		     struct in_addr,
		     struct sockaddr_in *, struct hardware *);
int supports_multiple_interfaces (struct interface_info *);
void maybe_setup_fallback (void);
#endif
#ifdef USE_DLPI_RECEIVE
void if_reinitialize_receive (struct interface_info *);
void if_register_receive (struct interface_info *);
void if_deregister_receive (struct interface_info *);
ssize_t receive_packet (struct interface_info *,
			unsigned char *, size_t,
			struct sockaddr_in *, struct hardware *);
#endif


/* raw.c */
#ifdef USE_RAW_SEND
void if_reinitialize_send (struct interface_info *);
void if_register_send (struct interface_info *);
void if_deregister_send (struct interface_info *);
ssize_t send_packet (struct interface_info *, struct packet *,
		     struct dhcp_packet *, size_t, struct in_addr,
		     struct sockaddr_in *, struct hardware *);
int can_unicast_without_arp (struct interface_info *);
int can_receive_unicast_unconfigured (struct interface_info *);
int supports_multiple_interfaces (struct interface_info *);
void maybe_setup_fallback (void);
#endif

/* discover.c */
extern struct interface_info *interfaces,
	*dummy_interfaces, *fallback_interface;
extern struct protocol *protocols;
extern int quiet_interface_discovery;
isc_result_t interface_setup (void);
void interface_trace_setup (void);

extern struct in_addr limited_broadcast;
extern int local_family;
extern struct in_addr local_address;

extern u_int16_t local_port;
extern u_int16_t remote_port;
extern int (*dhcp_interface_setup_hook) (struct interface_info *,
					 struct iaddr *);
extern int (*dhcp_interface_discovery_hook) (struct interface_info *);
extern isc_result_t (*dhcp_interface_startup_hook) (struct interface_info *);

extern void (*bootp_packet_handler) (struct interface_info *,
				     struct dhcp_packet *, unsigned,
				     unsigned int,
				     struct iaddr, struct hardware *);
extern void (*dhcpv6_packet_handler)(struct interface_info *,
				     const char *, int,
				     int, const struct iaddr *, isc_boolean_t);
extern struct timeout *timeouts;
extern omapi_object_type_t *dhcp_type_interface;
#if defined (TRACING)
extern trace_type_t *interface_trace;
extern trace_type_t *inpacket_trace;
extern trace_type_t *outpacket_trace;
#endif
extern struct interface_info **interface_vector;
extern int interface_count;
extern int interface_max;
isc_result_t interface_initialize(omapi_object_t *, const char *, int);
void discover_interfaces(int);
int setup_fallback (struct interface_info **, const char *, int);
int if_readsocket (omapi_object_t *);
void reinitialize_interfaces (void);

/* dispatch.c */
void set_time(TIME);
struct timeval *process_outstanding_timeouts (struct timeval *);
void dispatch (void);
isc_result_t got_one(omapi_object_t *);
isc_result_t got_one_v6(omapi_object_t *);
isc_result_t interface_set_value (omapi_object_t *, omapi_object_t *,
				  omapi_data_string_t *, omapi_typed_data_t *);
isc_result_t interface_get_value (omapi_object_t *, omapi_object_t *,
				  omapi_data_string_t *, omapi_value_t **);
isc_result_t interface_destroy (omapi_object_t *, const char *, int);
isc_result_t interface_signal_handler (omapi_object_t *,
				       const char *, va_list);
isc_result_t interface_stuff_values (omapi_object_t *,
				     omapi_object_t *,
				     omapi_object_t *);

void add_timeout (struct timeval *, void (*) (void *), void *,
	tvref_t, tvunref_t);
void cancel_timeout (void (*) (void *), void *);
void cancel_all_timeouts (void);
void relinquish_timeouts (void);

OMAPI_OBJECT_ALLOC_DECL (interface,
			 struct interface_info, dhcp_type_interface)

/* tables.c */
extern char *default_option_format;
extern struct universe dhcp_universe;
extern struct universe dhcpv6_universe;
extern struct universe nwip_universe;
extern struct universe fqdn_universe;
extern struct universe vsio_universe;
extern int dhcp_option_default_priority_list [];
extern int dhcp_option_default_priority_list_count;
extern const char *hardware_types [256];
extern int universe_count, universe_max;
extern struct universe **universes;
extern universe_hash_t *universe_hash;
void initialize_common_option_spaces (void);
extern struct universe *config_universe;

/* stables.c */
#if defined (FAILOVER_PROTOCOL)
extern failover_option_t null_failover_option;
extern failover_option_t skip_failover_option;
extern struct failover_option_info ft_options [];
extern u_int32_t fto_allowed [];
extern int ft_sizes [];
extern const char *dhcp_flink_state_names [];
#endif
extern const char *binding_state_names [];

extern struct universe agent_universe;
extern struct universe server_universe;

extern struct enumeration ddns_styles;
extern struct enumeration syslog_enum;
void initialize_server_option_spaces (void);

/* inet.c */
struct iaddr subnet_number (struct iaddr, struct iaddr);
struct iaddr ip_addr (struct iaddr, struct iaddr, u_int32_t);
struct iaddr broadcast_addr (struct iaddr, struct iaddr);
u_int32_t host_addr (struct iaddr, struct iaddr);
int addr_eq (struct iaddr, struct iaddr);
int addr_match(struct iaddr *, struct iaddrmatch *);
int addr_cmp(const struct iaddr *a1, const struct iaddr *a2);
int addr_or(struct iaddr *result,
	    const struct iaddr *a1, const struct iaddr *a2);
int addr_and(struct iaddr *result,
	     const struct iaddr *a1, const struct iaddr *a2);
isc_boolean_t is_cidr_mask_valid(const struct iaddr *addr, int bits);
isc_result_t range2cidr(struct iaddrcidrnetlist **result,
			const struct iaddr *lo, const struct iaddr *hi);
isc_result_t free_iaddrcidrnetlist(struct iaddrcidrnetlist **result);
const char *piaddr (struct iaddr);
char *piaddrmask(struct iaddr *, struct iaddr *);
char *piaddrcidr(const struct iaddr *, unsigned int);
u_int16_t validate_port(char *);

/* dhclient.c */
extern int nowait;

extern int wanted_ia_na;
extern int wanted_ia_ta;
extern int wanted_ia_pd;

extern const char *path_dhclient_conf;
extern const char *path_dhclient_db;
extern const char *path_dhclient_pid;
extern char *path_dhclient_script;
extern int interfaces_requested;
extern struct data_string default_duid;
extern int duid_type;

extern struct client_config top_level_config;

void dhcpoffer (struct packet *);
void dhcpack (struct packet *);
void dhcpnak (struct packet *);

void send_discover (void *);
void send_request (void *);
void send_release (void *);
void send_decline (void *);

void state_reboot (void *);
void state_init (void *);
void state_selecting (void *);
void state_requesting (void *);
void state_bound (void *);
void state_stop (void *);
void state_panic (void *);

void bind_lease (struct client_state *);

void make_client_options (struct client_state *,
			  struct client_lease *, u_int8_t *,
			  struct option_cache *, struct iaddr *,
			  struct option **, struct option_state **);
void make_discover (struct client_state *, struct client_lease *);
void make_request (struct client_state *, struct client_lease *);
void make_decline (struct client_state *, struct client_lease *);
void make_release (struct client_state *, struct client_lease *);

void destroy_client_lease (struct client_lease *);
void rewrite_client_leases (void);
void write_lease_option (struct option_cache *, struct packet *,
			 struct lease *, struct client_state *,
			 struct option_state *, struct option_state *,
			 struct binding_scope **, struct universe *, void *);
int write_client_lease (struct client_state *, struct client_lease *, int, int);
isc_result_t write_client6_lease(struct client_state *client,
				 struct dhc6_lease *lease,
				 int rewrite, int sync);
int dhcp_option_ev_name (char *, size_t, struct option *);

void script_init (struct client_state *, const char *,
		  struct string_list *);
void client_option_envadd (struct option_cache *, struct packet *,
			   struct lease *, struct client_state *,
			   struct option_state *, struct option_state *,
			   struct binding_scope **, struct universe *, void *);
void script_write_params (struct client_state *, const char *,
			  struct client_lease *);
void script_write_requested (struct client_state *);
int script_go (struct client_state *);
void client_envadd (struct client_state *,
		    const char *, const char *, const char *, ...)
	__attribute__((__format__(__printf__,4,5)));

struct client_lease *packet_to_lease (struct packet *, struct client_state *);
void go_daemon (void);
void finish_daemon (void);
void write_client_pid_file (void);
void client_location_changed (void);
void do_release (struct client_state *);
int dhclient_interface_shutdown_hook (struct interface_info *);
int dhclient_interface_discovery_hook (struct interface_info *);
isc_result_t dhclient_interface_startup_hook (struct interface_info *);
void dhclient_schedule_updates(struct client_state *client,
			       struct iaddr *addr, int offset);
void client_dns_update_timeout (void *cp);
isc_result_t client_dns_update(struct client_state *client,
			       dhcp_ddns_cb_t *ddns_cb);
void client_dns_remove(struct client_state *client, struct iaddr *addr);

void dhcpv4_client_assignments(void);
void dhcpv6_client_assignments(void);
void form_duid(struct data_string *duid, const char *file, int line);

/* dhc6.c */
void dhc6_lease_destroy(struct dhc6_lease **src, const char *file, int line);
void start_init6(struct client_state *client);
void start_info_request6(struct client_state *client);
void start_confirm6(struct client_state *client);
void start_release6(struct client_state *client);
void start_selecting6(struct client_state *client);
void unconfigure6(struct client_state *client, const char *reason);

/* db.c */
int write_lease (struct lease *);
int write_host (struct host_decl *);
int write_server_duid(void);
#if defined (FAILOVER_PROTOCOL)
int write_failover_state (dhcp_failover_state_t *);
#endif
int db_printable (const unsigned char *);
int db_printable_len (const unsigned char *, unsigned);
isc_result_t write_named_billing_class(const void *, unsigned, void *);
void write_billing_classes (void);
int write_billing_class (struct class *);
void commit_leases_timeout (void *);
void commit_leases_readerdry(void *);
int commit_leases (void);
int commit_leases_timed (void);
void db_startup (int);
int new_lease_file (void);
int group_writer (struct group_object *);
int write_ia(const struct ia_xx *);

/* packet.c */
u_int32_t checksum (unsigned char *, unsigned, u_int32_t);
u_int32_t wrapsum (u_int32_t);
void assemble_hw_header (struct interface_info *, unsigned char *,
			 unsigned *, struct hardware *);
void assemble_udp_ip_header (struct interface_info *, unsigned char *,
			     unsigned *, u_int32_t, u_int32_t,
			     u_int32_t, unsigned char *, unsigned);
ssize_t decode_hw_header (struct interface_info *, unsigned char *,
			  unsigned, struct hardware *);
ssize_t decode_udp_ip_header (struct interface_info *, unsigned char *,
			      unsigned, struct sockaddr_in *,
			      unsigned, unsigned *);

/* ethernet.c */
void assemble_ethernet_header (struct interface_info *, unsigned char *,
			       unsigned *, struct hardware *);
ssize_t decode_ethernet_header (struct interface_info *,
				unsigned char *,
				unsigned, struct hardware *);

/* tr.c */
void assemble_tr_header (struct interface_info *, unsigned char *,
			 unsigned *, struct hardware *);
ssize_t decode_tr_header (struct interface_info *,
			  unsigned char *,
			  unsigned, struct hardware *);

/* dhxpxlt.c */
void convert_statement (struct parse *);
void convert_host_statement (struct parse *, jrefproto);
void convert_host_name (struct parse *, jrefproto);
void convert_class_statement (struct parse *, jrefproto, int);
void convert_class_decl (struct parse *, jrefproto);
void convert_lease_time (struct parse *, jrefproto, char *);
void convert_shared_net_statement (struct parse *, jrefproto);
void convert_subnet_statement (struct parse *, jrefproto);
void convert_subnet_decl (struct parse *, jrefproto);
void convert_host_decl (struct parse *, jrefproto);
void convert_hardware_decl (struct parse *, jrefproto);
void convert_hardware_addr (struct parse *, jrefproto);
void convert_filename_decl (struct parse *, jrefproto);
void convert_servername_decl (struct parse *, jrefproto);
void convert_ip_addr_or_hostname (struct parse *, jrefproto, int);
void convert_fixed_addr_decl (struct parse *, jrefproto);
void convert_option_decl (struct parse *, jrefproto);
void convert_lease_statement (struct parse *, jrefproto);
void convert_address_range (struct parse *, jrefproto);
void convert_date (struct parse *, jrefproto, char *);
void convert_numeric_aggregate (struct parse *, jrefproto, int, int, int, int);
void indent (int);

/* route.c */
void add_route_direct (struct interface_info *, struct in_addr);
void add_route_net (struct interface_info *, struct in_addr, struct in_addr);
void add_route_default_gateway (struct interface_info *, struct in_addr);
void remove_routes (struct in_addr);
void remove_if_route (struct interface_info *, struct in_addr);
void remove_all_if_routes (struct interface_info *);
void set_netmask (struct interface_info *, struct in_addr);
void set_broadcast_addr (struct interface_info *, struct in_addr);
void set_ip_address (struct interface_info *, struct in_addr);

/* clparse.c */
isc_result_t read_client_conf (void);
int read_client_conf_file (const char *,
			   struct interface_info *, struct client_config *);
void read_client_leases (void);
void parse_client_statement (struct parse *, struct interface_info *,
			     struct client_config *);
int parse_X (struct parse *, u_int8_t *, unsigned);
int parse_option_list (struct parse *, struct option ***);
void parse_interface_declaration (struct parse *,
				  struct client_config *, char *);
int interface_or_dummy (struct interface_info **, const char *);
void make_client_state (struct client_state **);
void make_client_config (struct client_state *, struct client_config *);
void parse_client_lease_statement (struct parse *, int);
void parse_client_lease_declaration (struct parse *,
				     struct client_lease *,
				     struct interface_info **,
				     struct client_state **);
int parse_option_decl (struct option_cache **, struct parse *);
void parse_string_list (struct parse *, struct string_list **, int);
int parse_ip_addr (struct parse *, struct iaddr *);
int parse_ip_addr_with_subnet(struct parse *, struct iaddrmatch *);
void parse_reject_statement (struct parse *, struct client_config *);

/* icmp.c */
OMAPI_OBJECT_ALLOC_DECL (icmp_state, struct icmp_state, dhcp_type_icmp)
extern struct icmp_state *icmp_state;
void icmp_startup (int, void (*) (struct iaddr, u_int8_t *, int));
int icmp_readsocket (omapi_object_t *);
int icmp_echorequest (struct iaddr *);
isc_result_t icmp_echoreply (omapi_object_t *);

/* dns.c */
isc_result_t enter_dns_zone (struct dns_zone *);
isc_result_t dns_zone_lookup (struct dns_zone **, const char *);
int dns_zone_dereference (struct dns_zone **, const char *, int);
#if defined (NSUPDATE)
#define FIND_FORWARD 0
#define FIND_REVERSE 1
isc_result_t find_tsig_key (ns_tsig_key **, const char *, struct dns_zone *);
void tkey_free (ns_tsig_key **);
isc_result_t find_cached_zone (dhcp_ddns_cb_t *, int);
void forget_zone (struct dns_zone **);
void repudiate_zone (struct dns_zone **);
int get_dhcid (dhcp_ddns_cb_t *, int, const u_int8_t *, unsigned);
void dhcid_tolease (struct data_string *, struct data_string *);
isc_result_t dhcid_fromlease (struct data_string *, struct data_string *);
isc_result_t ddns_update_fwd(struct data_string *, struct iaddr,
			     struct data_string *, unsigned long, unsigned,
			     unsigned);
isc_result_t ddns_remove_fwd(struct data_string *,
			     struct iaddr, struct data_string *);
#endif /* NSUPDATE */

dhcp_ddns_cb_t *ddns_cb_alloc(const char *file, int line);
void ddns_cb_free (dhcp_ddns_cb_t *ddns_cb, const char *file, int line);
void ddns_cb_forget_zone (dhcp_ddns_cb_t *ddns_cb);
isc_result_t
ddns_modify_fwd(dhcp_ddns_cb_t *ddns_cb, const char *file, int line);
isc_result_t
ddns_modify_ptr(dhcp_ddns_cb_t *ddns_cb, const char *file, int line);
void
ddns_cancel(dhcp_ddns_cb_t *ddns_cb, const char *file, int line);

/* resolv.c */
extern char path_resolv_conf [];
extern struct name_server *name_servers;
extern struct domain_search_list *domains;

void read_resolv_conf (TIME);
struct name_server *first_name_server (void);

/* inet_addr.c */
#ifdef NEED_INET_ATON
int inet_aton (const char *, struct in_addr *);
#endif

/* class.c */
extern int have_billing_classes;
struct class unknown_class;
struct class known_class;
struct collection default_collection;
struct collection *collections;
extern struct executable_statement *default_classification_rules;

void classification_setup (void);
void classify_client (struct packet *);
int check_collection (struct packet *, struct lease *, struct collection *);
void classify (struct packet *, struct class *);
isc_result_t unlink_class (struct class **class);
isc_result_t find_class (struct class **, const char *,
			 const char *, int);
int unbill_class (struct lease *, struct class *);
int bill_class (struct lease *, struct class *);

/* execute.c */
int execute_statements (struct binding_value **result,
			struct packet *, struct lease *,
			struct client_state *,
			struct option_state *, struct option_state *,
			struct binding_scope **,
			struct executable_statement *,
			struct on_star *);
void execute_statements_in_scope (struct binding_value **result,
				  struct packet *, struct lease *,
				  struct client_state *,
				  struct option_state *,
				  struct option_state *,
				  struct binding_scope **,
				  struct group *, struct group *,
				  struct on_star *);
int executable_statement_dereference (struct executable_statement **,
				      const char *, int);
void write_statements (FILE *, struct executable_statement *, int);
int find_matching_case (struct executable_statement **,
			struct packet *, struct lease *, struct client_state *,
			struct option_state *, struct option_state *,
			struct binding_scope **,
			struct expression *, struct executable_statement *);
int executable_statement_foreach (struct executable_statement *,
				  int (*) (struct executable_statement *,
					   void *, int), void *, int);

/* comapi.c */
extern omapi_object_type_t *dhcp_type_group;
extern omapi_object_type_t *dhcp_type_shared_network;
extern omapi_object_type_t *dhcp_type_subnet;
extern omapi_object_type_t *dhcp_type_control;
extern dhcp_control_object_t *dhcp_control_object;

void dhcp_common_objects_setup (void);

isc_result_t dhcp_group_set_value  (omapi_object_t *, omapi_object_t *,
				    omapi_data_string_t *,
				    omapi_typed_data_t *);
isc_result_t dhcp_group_get_value (omapi_object_t *, omapi_object_t *,
				   omapi_data_string_t *,
				   omapi_value_t **);
isc_result_t dhcp_group_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_group_signal_handler (omapi_object_t *,
					const char *, va_list);
isc_result_t dhcp_group_stuff_values (omapi_object_t *,
				      omapi_object_t *,
				      omapi_object_t *);
isc_result_t dhcp_group_lookup (omapi_object_t **,
				omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_group_create (omapi_object_t **,
				omapi_object_t *);
isc_result_t dhcp_group_remove (omapi_object_t *,
				omapi_object_t *);

isc_result_t dhcp_control_set_value  (omapi_object_t *, omapi_object_t *,
				      omapi_data_string_t *,
				      omapi_typed_data_t *);
isc_result_t dhcp_control_get_value (omapi_object_t *, omapi_object_t *,
				     omapi_data_string_t *,
				     omapi_value_t **);
isc_result_t dhcp_control_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_control_signal_handler (omapi_object_t *,
					  const char *, va_list);
isc_result_t dhcp_control_stuff_values (omapi_object_t *,
					omapi_object_t *,
					omapi_object_t *);
isc_result_t dhcp_control_lookup (omapi_object_t **,
				  omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_control_create (omapi_object_t **,
				  omapi_object_t *);
isc_result_t dhcp_control_remove (omapi_object_t *,
				  omapi_object_t *);

isc_result_t dhcp_subnet_set_value  (omapi_object_t *, omapi_object_t *,
				     omapi_data_string_t *,
				     omapi_typed_data_t *);
isc_result_t dhcp_subnet_get_value (omapi_object_t *, omapi_object_t *,
				    omapi_data_string_t *,
				    omapi_value_t **);
isc_result_t dhcp_subnet_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_subnet_signal_handler (omapi_object_t *,
					 const char *, va_list);
isc_result_t dhcp_subnet_stuff_values (omapi_object_t *,
				       omapi_object_t *,
				       omapi_object_t *);
isc_result_t dhcp_subnet_lookup (omapi_object_t **,
				 omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_subnet_create (omapi_object_t **,
				 omapi_object_t *);
isc_result_t dhcp_subnet_remove (omapi_object_t *,
				 omapi_object_t *);

isc_result_t dhcp_shared_network_set_value  (omapi_object_t *,
					     omapi_object_t *,
					     omapi_data_string_t *,
					     omapi_typed_data_t *);
isc_result_t dhcp_shared_network_get_value (omapi_object_t *,
					    omapi_object_t *,
					    omapi_data_string_t *,
					    omapi_value_t **);
isc_result_t dhcp_shared_network_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_shared_network_signal_handler (omapi_object_t *,
						 const char *, va_list);
isc_result_t dhcp_shared_network_stuff_values (omapi_object_t *,
					       omapi_object_t *,
					       omapi_object_t *);
isc_result_t dhcp_shared_network_lookup (omapi_object_t **,
					 omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_shared_network_create (omapi_object_t **,
					 omapi_object_t *);
isc_result_t dhcp_shared_network_remove (omapi_object_t *,
					 omapi_object_t *);

/* omapi.c */
extern int (*dhcp_interface_shutdown_hook) (struct interface_info *);

extern omapi_object_type_t *dhcp_type_lease;
extern omapi_object_type_t *dhcp_type_pool;
extern omapi_object_type_t *dhcp_type_class;
extern omapi_object_type_t *dhcp_type_subclass;

#if defined (FAILOVER_PROTOCOL)
extern omapi_object_type_t *dhcp_type_failover_state;
extern omapi_object_type_t *dhcp_type_failover_link;
extern omapi_object_type_t *dhcp_type_failover_listener;
#endif

void dhcp_db_objects_setup (void);

isc_result_t dhcp_lease_set_value  (omapi_object_t *, omapi_object_t *,
				    omapi_data_string_t *,
				    omapi_typed_data_t *);
isc_result_t dhcp_lease_get_value (omapi_object_t *, omapi_object_t *,
				   omapi_data_string_t *,
				   omapi_value_t **);
isc_result_t dhcp_lease_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_lease_signal_handler (omapi_object_t *,
					const char *, va_list);
isc_result_t dhcp_lease_stuff_values (omapi_object_t *,
				      omapi_object_t *,
				      omapi_object_t *);
isc_result_t dhcp_lease_lookup (omapi_object_t **,
				omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_lease_create (omapi_object_t **,
				omapi_object_t *);
isc_result_t dhcp_lease_remove (omapi_object_t *,
				omapi_object_t *);
isc_result_t dhcp_host_set_value  (omapi_object_t *, omapi_object_t *,
				   omapi_data_string_t *,
				   omapi_typed_data_t *);
isc_result_t dhcp_host_get_value (omapi_object_t *, omapi_object_t *,
				  omapi_data_string_t *,
				  omapi_value_t **);
isc_result_t dhcp_host_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_host_signal_handler (omapi_object_t *,
				       const char *, va_list);
isc_result_t dhcp_host_stuff_values (omapi_object_t *,
				     omapi_object_t *,
				     omapi_object_t *);
isc_result_t dhcp_host_lookup (omapi_object_t **,
			       omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_host_create (omapi_object_t **,
			       omapi_object_t *);
isc_result_t dhcp_host_remove (omapi_object_t *,
			       omapi_object_t *);
isc_result_t dhcp_pool_set_value  (omapi_object_t *, omapi_object_t *,
				   omapi_data_string_t *,
				   omapi_typed_data_t *);
isc_result_t dhcp_pool_get_value (omapi_object_t *, omapi_object_t *,
				  omapi_data_string_t *,
				  omapi_value_t **);
isc_result_t dhcp_pool_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_pool_signal_handler (omapi_object_t *,
				       const char *, va_list);
isc_result_t dhcp_pool_stuff_values (omapi_object_t *,
				     omapi_object_t *,
				     omapi_object_t *);
isc_result_t dhcp_pool_lookup (omapi_object_t **,
			       omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_pool_create (omapi_object_t **,
			       omapi_object_t *);
isc_result_t dhcp_pool_remove (omapi_object_t *,
			       omapi_object_t *);
isc_result_t dhcp_class_set_value  (omapi_object_t *, omapi_object_t *,
				    omapi_data_string_t *,
				    omapi_typed_data_t *);
isc_result_t dhcp_class_get_value (omapi_object_t *, omapi_object_t *,
				   omapi_data_string_t *,
				   omapi_value_t **);
isc_result_t dhcp_class_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_class_signal_handler (omapi_object_t *,
					const char *, va_list);
isc_result_t dhcp_class_stuff_values (omapi_object_t *,
				      omapi_object_t *,
				      omapi_object_t *);
isc_result_t dhcp_class_lookup (omapi_object_t **,
				omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_class_create (omapi_object_t **,
				omapi_object_t *);
isc_result_t dhcp_class_remove (omapi_object_t *,
				omapi_object_t *);
isc_result_t dhcp_subclass_set_value  (omapi_object_t *, omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_typed_data_t *);
isc_result_t dhcp_subclass_get_value (omapi_object_t *, omapi_object_t *,
				      omapi_data_string_t *,
				      omapi_value_t **);
isc_result_t dhcp_subclass_destroy (omapi_object_t *, const char *, int);
isc_result_t dhcp_subclass_signal_handler (omapi_object_t *,
					   const char *, va_list);
isc_result_t dhcp_subclass_stuff_values (omapi_object_t *,
					 omapi_object_t *,
					 omapi_object_t *);
isc_result_t dhcp_subclass_lookup (omapi_object_t **,
				   omapi_object_t *, omapi_object_t *);
isc_result_t dhcp_subclass_create (omapi_object_t **,
				   omapi_object_t *);
isc_result_t dhcp_subclass_remove (omapi_object_t *,
				   omapi_object_t *);
isc_result_t dhcp_interface_set_value (omapi_object_t *,
				       omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_typed_data_t *);
isc_result_t dhcp_interface_get_value (omapi_object_t *,
				       omapi_object_t *,
				       omapi_data_string_t *,
				       omapi_value_t **);
isc_result_t dhcp_interface_destroy (omapi_object_t *,
				     const char *, int);
isc_result_t dhcp_interface_signal_handler (omapi_object_t *,
					    const char *,
					    va_list ap);
isc_result_t dhcp_interface_stuff_values (omapi_object_t *,
					  omapi_object_t *,
					  omapi_object_t *);
isc_result_t dhcp_interface_lookup (omapi_object_t **,
				    omapi_object_t *,
				    omapi_object_t *);
isc_result_t dhcp_interface_create (omapi_object_t **,
				    omapi_object_t *);
isc_result_t dhcp_interface_remove (omapi_object_t *,
				    omapi_object_t *);
void interface_stash (struct interface_info *);
void interface_snorf (struct interface_info *, int);

isc_result_t binding_scope_set_value (struct binding_scope *, int,
				      omapi_data_string_t *,
				      omapi_typed_data_t *);
isc_result_t binding_scope_get_value (omapi_value_t **,
				      struct binding_scope *,
				      omapi_data_string_t *);
isc_result_t binding_scope_stuff_values (omapi_object_t *,
					 struct binding_scope *);

void register_eventhandler(struct eventqueue **, void (*handler)(void *));
void unregister_eventhandler(struct eventqueue **, void (*handler)(void *));
void trigger_event(struct eventqueue **);

/* mdb.c */

extern struct subnet *subnets;
extern struct shared_network *shared_networks;
extern host_hash_t *host_hw_addr_hash;
extern host_hash_t *host_uid_hash;
extern host_hash_t *host_name_hash;
extern lease_id_hash_t *lease_uid_hash;
extern lease_ip_hash_t *lease_ip_addr_hash;
extern lease_id_hash_t *lease_hw_addr_hash;

extern omapi_object_type_t *dhcp_type_host;

extern int numclasseswritten;


isc_result_t enter_class (struct class *, int, int);
isc_result_t delete_class (struct class *, int);
isc_result_t enter_host (struct host_decl *, int, int);
isc_result_t delete_host (struct host_decl *, int);
void change_host_uid(struct host_decl *host, const char *data, int len);
int find_hosts_by_haddr (struct host_decl **, int,
			 const unsigned char *, unsigned,
			 const char *, int);
int find_hosts_by_uid (struct host_decl **, const unsigned char *,
		       unsigned, const char *, int);
int find_hosts_by_option(struct host_decl **, struct packet *,
			 struct option_state *, const char *, int);
int find_host_for_network (struct subnet **, struct host_decl **,
			   struct iaddr *, struct shared_network *);
void new_address_range (struct parse *, struct iaddr, struct iaddr,
			struct subnet *, struct pool *,
			struct lease **);
isc_result_t dhcp_lease_free (omapi_object_t *, const char *, int);
isc_result_t dhcp_lease_get (omapi_object_t **, const char *, int);
int find_grouped_subnet (struct subnet **, struct shared_network *,
			 struct iaddr, const char *, int);
int find_subnet(struct subnet **, struct iaddr, const char *, int);
void enter_shared_network (struct shared_network *);
void new_shared_network_interface (struct parse *,
				   struct shared_network *,
				   const char *);
int subnet_inner_than(const struct subnet *, const struct subnet *, int);
void enter_subnet (struct subnet *);
void enter_lease (struct lease *);
int supersede_lease (struct lease *, struct lease *, int, int, int);
void make_binding_state_transition (struct lease *);
int lease_copy (struct lease **, struct lease *, const char *, int);
void release_lease (struct lease *, struct packet *);
void abandon_lease (struct lease *, const char *);
#if 0
/* this appears to be unused and I plan to remove it SAR */
void dissociate_lease (struct lease *);
#endif
void pool_timer (void *);
int find_lease_by_uid (struct lease **, const unsigned char *,
		       unsigned, const char *, int);
int find_lease_by_hw_addr (struct lease **, const unsigned char *,
			   unsigned, const char *, int);
int find_lease_by_ip_addr (struct lease **, struct iaddr,
			   const char *, int);
void uid_hash_add (struct lease *);
void uid_hash_delete (struct lease *);
void hw_hash_add (struct lease *);
void hw_hash_delete (struct lease *);
int write_leases (void);
int write_leases6(void);
int lease_enqueue (struct lease *);
isc_result_t lease_instantiate(const void *, unsigned, void *);
void expire_all_pools (void);
void dump_subnets (void);
#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void free_everything (void);
#endif

/* failover.c */
#if defined (FAILOVER_PROTOCOL)
extern dhcp_failover_state_t *failover_states;
void dhcp_failover_startup (void);
int dhcp_failover_write_all_states (void);
isc_result_t enter_failover_peer (dhcp_failover_state_t *);
isc_result_t find_failover_peer (dhcp_failover_state_t **,
				 const char *, const char *, int);
isc_result_t dhcp_failover_link_initiate (omapi_object_t *);
isc_result_t dhcp_failover_link_signal (omapi_object_t *,
					const char *, va_list);
isc_result_t dhcp_failover_link_set_value (omapi_object_t *,
					   omapi_object_t *,
					   omapi_data_string_t *,
					   omapi_typed_data_t *);
isc_result_t dhcp_failover_link_get_value (omapi_object_t *,
					   omapi_object_t *,
					   omapi_data_string_t *,
					   omapi_value_t **);
isc_result_t dhcp_failover_link_destroy (omapi_object_t *,
					 const char *, int);
isc_result_t dhcp_failover_link_stuff_values (omapi_object_t *,
					      omapi_object_t *,
					      omapi_object_t *);
isc_result_t dhcp_failover_listen (omapi_object_t *);

isc_result_t dhcp_failover_listener_signal (omapi_object_t *,
					    const char *,
					    va_list);
isc_result_t dhcp_failover_listener_set_value (omapi_object_t *,
					       omapi_object_t *,
					       omapi_data_string_t *,
					       omapi_typed_data_t *);
isc_result_t dhcp_failover_listener_get_value (omapi_object_t *,
					       omapi_object_t *,
					       omapi_data_string_t *,
					       omapi_value_t **);
isc_result_t dhcp_failover_listener_destroy (omapi_object_t *,
					     const char *, int);
isc_result_t dhcp_failover_listener_stuff (omapi_object_t *,
					   omapi_object_t *,
					   omapi_object_t *);
isc_result_t dhcp_failover_register (omapi_object_t *);
isc_result_t dhcp_failover_state_signal (omapi_object_t *,
					 const char *, va_list);
isc_result_t dhcp_failover_state_transition (dhcp_failover_state_t *,
					     const char *);
isc_result_t dhcp_failover_set_service_state (dhcp_failover_state_t *state);
isc_result_t dhcp_failover_set_state (dhcp_failover_state_t *,
				      enum failover_state);
isc_result_t dhcp_failover_peer_state_changed (dhcp_failover_state_t *,
					       failover_message_t *);
void dhcp_failover_pool_rebalance (void *);
void dhcp_failover_pool_check (struct pool *);
int dhcp_failover_state_pool_check (dhcp_failover_state_t *);
void dhcp_failover_timeout (void *);
void dhcp_failover_send_contact (void *);
isc_result_t dhcp_failover_send_state (dhcp_failover_state_t *);
isc_result_t dhcp_failover_send_updates (dhcp_failover_state_t *);
int dhcp_failover_queue_update (struct lease *, int);
int dhcp_failover_send_acks (dhcp_failover_state_t *);
void dhcp_failover_toack_queue_timeout (void *);
int dhcp_failover_queue_ack (dhcp_failover_state_t *, failover_message_t *msg);
void dhcp_failover_ack_queue_remove (dhcp_failover_state_t *, struct lease *);
isc_result_t dhcp_failover_state_set_value (omapi_object_t *,
					    omapi_object_t *,
					    omapi_data_string_t *,
					    omapi_typed_data_t *);
void dhcp_failover_keepalive (void *);
void dhcp_failover_reconnect (void *);
void dhcp_failover_startup_timeout (void *);
void dhcp_failover_link_startup_timeout (void *);
void dhcp_failover_listener_restart (void *);
void dhcp_failover_auto_partner_down(void *vs);
isc_result_t dhcp_failover_state_get_value (omapi_object_t *,
					    omapi_object_t *,
					    omapi_data_string_t *,
					    omapi_value_t **);
isc_result_t dhcp_failover_state_destroy (omapi_object_t *,
					  const char *, int);
isc_result_t dhcp_failover_state_stuff (omapi_object_t *,
					omapi_object_t *,
					omapi_object_t *);
isc_result_t dhcp_failover_state_lookup (omapi_object_t **,
					 omapi_object_t *,
					 omapi_object_t *);
isc_result_t dhcp_failover_state_create (omapi_object_t **,
					 omapi_object_t *);
isc_result_t dhcp_failover_state_remove (omapi_object_t *,
					 omapi_object_t *);
int dhcp_failover_state_match (dhcp_failover_state_t *, u_int8_t *, unsigned);
int dhcp_failover_state_match_by_name(dhcp_failover_state_t *,
				      failover_option_t *);
const char *dhcp_failover_reject_reason_print (int);
const char *dhcp_failover_state_name_print (enum failover_state);
const char *dhcp_failover_message_name (unsigned);
const char *dhcp_failover_option_name (unsigned);
failover_option_t *dhcp_failover_option_printf (unsigned, char *,
						unsigned *,
						unsigned,
						const char *, ...)
	__attribute__((__format__(__printf__,5,6)));
failover_option_t *dhcp_failover_make_option (unsigned, char *,
					      unsigned *, unsigned, ...);
isc_result_t dhcp_failover_put_message (dhcp_failover_link_t *,
					omapi_object_t *, int, u_int32_t, ...);
isc_result_t dhcp_failover_send_connect (omapi_object_t *);
isc_result_t dhcp_failover_send_connectack (omapi_object_t *,
					    dhcp_failover_state_t *,
					    int, const char *);
isc_result_t dhcp_failover_send_disconnect (omapi_object_t *,
					    int, const char *);
isc_result_t dhcp_failover_send_bind_update (dhcp_failover_state_t *,
					     struct lease *);
isc_result_t dhcp_failover_send_bind_ack (dhcp_failover_state_t *,
					  failover_message_t *,
					  int, const char *);
isc_result_t dhcp_failover_send_poolreq (dhcp_failover_state_t *);
isc_result_t dhcp_failover_send_poolresp (dhcp_failover_state_t *, int);
isc_result_t dhcp_failover_send_update_request (dhcp_failover_state_t *);
isc_result_t dhcp_failover_send_update_request_all (dhcp_failover_state_t *);
isc_result_t dhcp_failover_send_update_done (dhcp_failover_state_t *);
isc_result_t dhcp_failover_process_bind_update (dhcp_failover_state_t *,
						failover_message_t *);
isc_result_t dhcp_failover_process_bind_ack (dhcp_failover_state_t *,
					     failover_message_t *);
isc_result_t dhcp_failover_generate_update_queue (dhcp_failover_state_t *,
						  int);
isc_result_t dhcp_failover_process_update_request (dhcp_failover_state_t *,
						   failover_message_t *);
isc_result_t dhcp_failover_process_update_request_all (dhcp_failover_state_t *,
						       failover_message_t *);
isc_result_t dhcp_failover_process_update_done (dhcp_failover_state_t *,
						failover_message_t *);
void ia_remove_all_lease(struct ia_xx *ia, const char *file, int line);
void dhcp_failover_recover_done (void *);
void failover_print (char *, unsigned *, unsigned, const char *);
void update_partner (struct lease *);
int load_balance_mine (struct packet *, dhcp_failover_state_t *);
int peer_wants_lease (struct lease *);
binding_state_t normal_binding_state_transition_check (struct lease *,
						       dhcp_failover_state_t *,
						       binding_state_t,
						       u_int32_t);
binding_state_t
conflict_binding_state_transition_check (struct lease *,
					 dhcp_failover_state_t *,
					 binding_state_t, u_int32_t);
int lease_mine_to_reallocate (struct lease *);

OMAPI_OBJECT_ALLOC_DECL (dhcp_failover_state, dhcp_failover_state_t,
			 dhcp_type_failover_state)
OMAPI_OBJECT_ALLOC_DECL (dhcp_failover_listener, dhcp_failover_listener_t,
			 dhcp_type_failover_listener)
OMAPI_OBJECT_ALLOC_DECL (dhcp_failover_link, dhcp_failover_link_t,
			 dhcp_type_failover_link)
#endif /* FAILOVER_PROTOCOL */

const char *binding_state_print (enum failover_state);

/* ldap.c */
#if defined(LDAP_CONFIGURATION)
extern struct enumeration ldap_methods;
#if defined (LDAP_USE_SSL)
extern struct enumeration ldap_ssl_usage_enum;
extern struct enumeration ldap_tls_reqcert_enum;
extern struct enumeration ldap_tls_crlcheck_enum;
#endif
isc_result_t ldap_read_config (void);
int find_haddr_in_ldap (struct host_decl **, int, unsigned,
			const unsigned char *, const char *, int);
int find_subclass_in_ldap (struct class *, struct class **,
			   struct data_string *);
#endif

/* mdb6.c */
HASH_FUNCTIONS_DECL(ia, unsigned char *, struct ia_xx, ia_hash_t)
HASH_FUNCTIONS_DECL(iasubopt, struct in6_addr *, struct iasubopt,
		    iasubopt_hash_t)

isc_result_t iasubopt_allocate(struct iasubopt **iasubopt,
			       const char *file, int line);
isc_result_t iasubopt_reference(struct iasubopt **iasubopt,
				struct iasubopt *src,
				const char *file, int line);
isc_result_t iasubopt_dereference(struct iasubopt **iasubopt,
				  const char *file, int line);

isc_result_t ia_make_key(struct data_string *key, u_int32_t iaid,
			 const char *duid, unsigned int duid_len,
			 const char *file, int line);
isc_result_t ia_allocate(struct ia_xx **ia, u_int32_t iaid,
			 const char *duid, unsigned int duid_len,
			 const char *file, int line);
isc_result_t ia_reference(struct ia_xx **ia, struct ia_xx *src,
			  const char *file, int line);
isc_result_t ia_dereference(struct ia_xx **ia,
			    const char *file, int line);
isc_result_t ia_add_iasubopt(struct ia_xx *ia, struct iasubopt *iasubopt,
			     const char *file, int line);
void ia_remove_iasubopt(struct ia_xx *ia, struct iasubopt *iasubopt,
			const char *file, int line);
isc_boolean_t ia_equal(const struct ia_xx *a, const struct ia_xx *b);

isc_result_t ipv6_pool_allocate(struct ipv6_pool **pool, u_int16_t type,
				const struct in6_addr *start_addr,
				int bits, int units,
				const char *file, int line);
isc_result_t ipv6_pool_reference(struct ipv6_pool **pool,
				 struct ipv6_pool *src,
				 const char *file, int line);
isc_result_t ipv6_pool_dereference(struct ipv6_pool **pool,
				   const char *file, int line);
isc_result_t create_lease6(struct ipv6_pool *pool,
			   struct iasubopt **addr,
			   unsigned int *attempts,
			   const struct data_string *uid,
			   time_t soft_lifetime_end_time);
isc_result_t add_lease6(struct ipv6_pool *pool,
			struct iasubopt *lease,
			time_t valid_lifetime_end_time);
isc_result_t renew_lease6(struct ipv6_pool *pool, struct iasubopt *lease);
isc_result_t expire_lease6(struct iasubopt **leasep,
			   struct ipv6_pool *pool, time_t now);
isc_result_t release_lease6(struct ipv6_pool *pool, struct iasubopt *lease);
isc_result_t decline_lease6(struct ipv6_pool *pool, struct iasubopt *lease);
isc_boolean_t lease6_exists(const struct ipv6_pool *pool,
			    const struct in6_addr *addr);
isc_boolean_t lease6_usable(struct iasubopt *lease);
isc_result_t cleanup_lease6(ia_hash_t *ia_table,
			    struct ipv6_pool *pool,
			    struct iasubopt *lease,
			    struct ia_xx *ia);
isc_result_t mark_lease_unavailble(struct ipv6_pool *pool,
				   const struct in6_addr *addr);
isc_result_t create_prefix6(struct ipv6_pool *pool,
			    struct iasubopt **pref,
			    unsigned int *attempts,
			    const struct data_string *uid,
			    time_t soft_lifetime_end_time);
isc_boolean_t prefix6_exists(const struct ipv6_pool *pool,
			     const struct in6_addr *pref, u_int8_t plen);

isc_result_t add_ipv6_pool(struct ipv6_pool *pool);
isc_result_t find_ipv6_pool(struct ipv6_pool **pool, u_int16_t type,
			    const struct in6_addr *addr);
isc_boolean_t ipv6_in_pool(const struct in6_addr *addr,
			   const struct ipv6_pool *pool);
isc_result_t ipv6_pond_allocate(struct ipv6_pond **pond,
				const char *file, int line);
isc_result_t ipv6_pond_reference(struct ipv6_pond **pond,
				 struct ipv6_pond *src,
				 const char *file, int line);
isc_result_t ipv6_pond_dereference(struct ipv6_pond **pond,
				   const char *file, int line);

isc_result_t renew_leases(struct ia_xx *ia);
isc_result_t release_leases(struct ia_xx *ia);
isc_result_t decline_leases(struct ia_xx *ia);
void schedule_lease_timeout(struct ipv6_pool *pool);
void schedule_all_ipv6_lease_timeouts(void);

void mark_hosts_unavailable(void);
void mark_phosts_unavailable(void);
void mark_interfaces_unavailable(void);

#define MAX_ADDRESS_STRING_LEN \
   (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"))
