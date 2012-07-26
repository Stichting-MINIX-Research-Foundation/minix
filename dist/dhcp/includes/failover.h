/* failover.h

   Definitions for address trees... */

/*
 * Copyright (c) 2004,2005,2007,2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2000-2003 by Internet Software Consortium
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
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``https://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#if defined (FAILOVER_PROTOCOL)
struct failover_option_info {
	int code;
	const char *name;
	enum { FT_UINT8, FT_IPADDR, FT_UINT32, FT_BYTES, FT_TEXT_OR_BYTES,
	       FT_DDNS, FT_DDNS1, FT_UINT16, FT_TEXT,
	       FT_UNDEF, FT_DIGEST } type;
	int num_present;
	int offset;
	u_int32_t bit;
};

typedef struct {
	unsigned count;
	u_int8_t *data;
} failover_option_t;

/* Failover configuration defaults. */
#ifndef  DEFAULT_MAX_BALANCE_TIME
# define DEFAULT_MAX_BALANCE_TIME	3600
#endif

#ifndef  DEFAULT_MIN_BALANCE_TIME
# define DEFAULT_MIN_BALANCE_TIME	60
#endif

#ifndef  DEFAULT_MAX_LEASE_MISBALANCE
# define DEFAULT_MAX_LEASE_MISBALANCE   15
#endif

#ifndef  DEFAULT_MAX_LEASE_OWNERSHIP
# define DEFAULT_MAX_LEASE_OWNERSHIP    10
#endif

#ifndef  DEFAULT_MAX_FLYING_UPDATES
# define DEFAULT_MAX_FLYING_UPDATES	100
#endif

#ifndef  DEFAULT_MAX_RESPONSE_DELAY
# define DEFAULT_MAX_RESPONSE_DELAY	20
#endif

/*
 * IANA has assigned ports 647 ("dhcp-failover") and 847 ("dhcp-failover2").
 * Of these, only port 647 is mentioned in the -12 draft revision.  We're not
 * sure if they are supposed to indicate primary and secondary?  No matter,
 * we'll stick to the -12 draft revision level.
 */
#ifndef  DEFAULT_FAILOVER_PORT
# define DEFAULT_FAILOVER_PORT		647
#endif

#define FM_OFFSET(x) (long)(&(((failover_message_t *)0) -> x))

/* All of the below definitions are mandated by draft-ietf-dhc-failover-12.
 * The Sections referenced are Sections within that document of that
 * version, and may be different in other documents of other versions.
 */

/* Failover message options from Section 12: */
#define FTO_ADDRESSES_TRANSFERRED	1
#define FTB_ADDRESSES_TRANSFERRED		0x00000002
#define FTO_ASSIGNED_IP_ADDRESS		2
#define FTB_ASSIGNED_IP_ADDRESS			0x00000004
#define FTO_BINDING_STATUS		3
#define FTB_BINDING_STATUS			0x00000008
#define FTO_CLIENT_IDENTIFIER		4
#define FTB_CLIENT_IDENTIFIER			0x00000010
#define FTO_CHADDR			5
#define FTB_CHADDR				0x00000020
#define FTO_CLTT			6
#define FTB_CLTT				0x00000040
#define FTO_REPLY_OPTIONS		7
#define FTB_REPLY_OPTIONS			0x00000080
#define FTO_REQUEST_OPTIONS		8
#define FTB_REQUEST_OPTIONS			0x00000100
#define FTO_DDNS			9
#define FTB_DDNS				0x00000200
#define FTO_DELAYED_SERVICE		10
#define FTB_DELAYED_SERVICE			0x00000400
#define FTO_HBA				11
#define FTB_HBA					0x00000800
#define FTO_IP_FLAGS			12
#define FTB_IP_FLAGS				0x00001000
#define FTO_LEASE_EXPIRY		13
#define FTB_LEASE_EXPIRY			0x00002000
#define FTO_MAX_UNACKED			14
#define FTB_MAX_UNACKED				0x00004000
#define FTO_MCLT			15
#define FTB_MCLT				0x00008000
#define FTO_MESSAGE			16
#define FTB_MESSAGE				0x00010000
#define FTO_MESSAGE_DIGEST		17
#define FTB_MESSAGE_DIGEST			0x00020000
#define FTO_POTENTIAL_EXPIRY		18
#define FTB_POTENTIAL_EXPIRY			0x00040000
#define FTO_RECEIVE_TIMER		19
#define FTB_RECEIVE_TIMER			0x00080000
#define FTO_PROTOCOL_VERSION		20
#define FTB_PROTOCOL_VERSION			0x00100000
#define FTO_REJECT_REASON		21
#define FTB_REJECT_REASON			0x00200000
#define FTO_RELATIONSHIP_NAME		22
#define FTB_RELATIONSHIP_NAME			0x00400000
#define FTO_SERVER_FLAGS		23
#define FTB_SERVER_FLAGS			0x00800000
#define FTO_SERVER_STATE		24
#define FTB_SERVER_STATE			0x01000000
#define FTO_STOS			25
#define FTB_STOS				0x02000000
#define FTO_TLS_REPLY			26
#define FTB_TLS_REPLY				0x04000000
#define FTO_TLS_REQUEST			27
#define FTB_TLS_REQUEST				0x08000000
#define FTO_VENDOR_CLASS		28
#define FTB_VENDOR_CLASS			0x10000000
#define FTO_VENDOR_OPTIONS		29
#define FTB_VENDOR_OPTIONS			0x20000000

#define FTO_MAX				FTO_VENDOR_OPTIONS

/* Failover protocol message types from Section 6.1: */
#define FTM_POOLREQ		1
#define FTM_POOLRESP		2
#define FTM_BNDUPD		3
#define FTM_BNDACK		4
#define FTM_CONNECT		5
#define FTM_CONNECTACK		6
#define FTM_UPDREQALL		7
#define FTM_UPDDONE		8
#define FTM_UPDREQ		9
#define FTM_STATE		10
#define FTM_CONTACT		11
#define FTM_DISCONNECT		12

/* Reject reasons from Section 12.21: */
#define FTR_ILLEGAL_IP_ADDR	1
#define FTR_FATAL_CONFLICT	2
#define FTR_MISSING_BINDINFO	3
#define FTR_TIMEMISMATCH	4
#define FTR_INVALID_MCLT	5
#define FTR_MISC_REJECT		6
#define FTR_DUP_CONNECTION	7
#define FTR_INVALID_PARTNER	8
#define FTR_TLS_UNSUPPORTED	9
#define FTR_TLS_UNCONFIGURED	10
#define FTR_TLS_REQUIRED	11
#define FTR_DIGEST_UNSUPPORTED	12
#define FTR_DIGEST_UNCONFIGURED	13
#define FTR_VERSION_MISMATCH	14
#define FTR_OUTDATED_BIND_INFO	15
#define FTR_LESS_CRIT_BIND_INFO	16
#define FTR_NO_TRAFFIC		17
#define FTR_HBA_CONFLICT	18
#define FTR_IP_NOT_RESERVED	19
#define FTR_IP_DIGEST_FAILURE	20
#define FTR_IP_MISSING_DIGEST	21
#define FTR_UNKNOWN		254

/* Message size limitations defined in Section 6.1: */
#define DHCP_FAILOVER_MIN_MESSAGE_SIZE    12
#define DHCP_FAILOVER_MAX_MESSAGE_SIZE	2048

/* Failover server flags from Section 12.23: */
#define FTF_SERVER_STARTUP	1

/* DDNS flags from Section 12.9.  These are really their names. */
#define FTF_DDNS_C		0x0001
#define FTF_DDNS_A		0x0002
#define FTF_DDNS_D		0x0004
#define FTF_DDNS_P		0x0008

/* FTO_IP_FLAGS contents from Section 12.12: */
#define FTF_IP_FLAG_RESERVE	0x0001
#define FTF_IP_FLAG_BOOTP	0x0002

/* FTO_MESSAGE_DIGEST Type Codes from Section 12.17: */
#define FTT_MESSAGE_DIGEST_HMAC_MD5	0x01

typedef struct failover_message {
	int refcnt;
	struct failover_message *next;

	int options_present;

	u_int32_t time;
	u_int32_t xid;
	u_int8_t type;

	/* One-byte options. */
	u_int8_t binding_status;
	u_int8_t delayed_service;
	u_int8_t protocol_version;
	u_int8_t reject_reason;
	u_int8_t server_flags;
	u_int8_t server_state;
	u_int8_t tls_reply;
	u_int8_t tls_request;

	/* Two-byte options. */
	u_int16_t ip_flags;

	/* Four-byte options. */
	u_int32_t addresses_transferred;
	u_int32_t assigned_addr;
	u_int32_t cltt;
	u_int32_t expiry;
	u_int32_t max_unacked;
	u_int32_t mclt;
	u_int32_t potential_expiry;
	u_int32_t receive_timer;
	u_int32_t stos;

	/* Arbitrary field options. */
	failover_option_t chaddr;
	failover_option_t client_identifier;
	failover_option_t hba;
	failover_option_t message;
	failover_option_t message_digest;
	failover_option_t relationship_name;
	failover_option_t reply_options;
	failover_option_t request_options;
	failover_option_t vendor_class;
	failover_option_t vendor_options;

	/* Special contents options. */
	ddns_fqdn_t ddns;
} failover_message_t;

typedef struct {
	OMAPI_OBJECT_PREAMBLE;
	struct option_cache *peer_address;
	unsigned peer_port;
	int options_present;
	enum dhcp_flink_state {
		dhcp_flink_start,
		dhcp_flink_message_length_wait,
		dhcp_flink_message_wait,
		dhcp_flink_disconnected,
		dhcp_flink_state_max
	} state;
	failover_message_t *imsg;
	struct _dhcp_failover_state *state_object;
	u_int16_t imsg_len;
	unsigned imsg_count;
	u_int8_t imsg_payoff; /* Pay*load* offset. :') */
	u_int32_t xid;
} dhcp_failover_link_t;

typedef struct _dhcp_failover_listener {
	OMAPI_OBJECT_PREAMBLE;
	struct _dhcp_failover_listener *next;
	omapi_addr_t address;
} dhcp_failover_listener_t;
#endif /* FAILOVER_PROTOCOL */

/* A failover peer's running state. */
enum failover_state {
	unknown_state			=  0, /* XXX: Not a standard state. */
	startup				=  1,
	normal				=  2,
	communications_interrupted	=  3,
	partner_down			=  4,
	potential_conflict		=  5,
	recover				=  6,
	paused				=  7,
	shut_down			=  8,
	recover_done			=  9,
	resolution_interrupted		= 10,
	conflict_done			= 11,

	/* Draft revision 12 of the failover protocol documents a RECOVER-WAIT
	 * state, but does not enumerate its value in the section 12.24
	 * table.  ISC DHCP 3.0.x used value 254 even though the state was
	 * not documented at all.  For the time being, we will continue to use
	 * this value.
	 */
	recover_wait			= 254
};

/* Service states are simplifications of failover states, particularly
   useful because the startup state isn't actually implementable as a
   separate failover state without maintaining a state stack. */

enum service_state {
	unknown_service_state,
	cooperating,
	not_cooperating,
	service_partner_down,
	not_responding,
	service_startup
};

#if defined (FAILOVER_PROTOCOL)
typedef struct _dhcp_failover_config {
	struct option_cache *address;
	int port;
	u_int32_t max_flying_updates;
	enum failover_state state;
	TIME stos;
	u_int32_t max_response_delay;
} dhcp_failover_config_t;

typedef struct _dhcp_failover_state {
	OMAPI_OBJECT_PREAMBLE;
	struct _dhcp_failover_state *next;
	char *name;			/* Name of this failover instance. */
	dhcp_failover_config_t me;	/* My configuration. */
	dhcp_failover_config_t partner;	/* Partner's configuration. */
	enum failover_state saved_state; /* Saved state during startup. */
	struct data_string server_identifier; /* Server identifier (IP addr) */
	u_int32_t mclt;

	u_int8_t *hba;	/* Hash bucket array for load balancing. */
	int load_balance_max_secs;

	u_int32_t max_lease_misbalance, max_lease_ownership;
	u_int32_t max_balance, min_balance;
	TIME last_balance, sched_balance;

	u_int32_t auto_partner_down;

	enum service_state service_state;
	const char *nrr;	/* Printable reason why we're in the
				   not_responding service state (empty
				   string if we are responding. */

	dhcp_failover_link_t *link_to_peer;	/* Currently-established link
						   to peer. */

	enum {
		primary, secondary
	} i_am;		/* We are primary or secondary in this relationship. */

	TIME last_packet_sent;		/* Timestamp on last packet we sent. */
	TIME last_timestamp_received;	/* The last timestamp we sent that
					   has been returned by our partner. */
	TIME skew;	/* The skew between our clock and our partner's. */
	struct lease *update_queue_head; /* List of leases we haven't sent
					    to peer. */
	struct lease *update_queue_tail;

	struct lease *ack_queue_head;	/* List of lease updates the peer
					   hasn't yet acked. */
	struct lease *ack_queue_tail;

	struct lease *send_update_done;	/* When we get a BNDACK for this
					   lease, send an UPDDONE message. */
	int cur_unacked_updates;	/* Number of updates we've sent
					   that have not yet been acked. */

					/* List of messages which we haven't
					   acked yet. */
	failover_message_t *toack_queue_head;
	failover_message_t *toack_queue_tail;
	int pending_acks;		/* Number of messages in the toack
					   queue. */
	int pool_count;			/* Number of pools referencing this
					   failover state object. */
	int curUPD;			/* If an UPDREQ* message is in motion,
					   this value indicates which one. */
	u_int32_t updxid;		/* XID of UPDREQ* message in action. */
} dhcp_failover_state_t;

#define DHCP_FAILOVER_VERSION		1
#endif /* FAILOVER_PROTOCOL */
