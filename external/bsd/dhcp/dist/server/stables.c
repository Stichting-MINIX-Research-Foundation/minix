/*	$NetBSD: stables.c,v 1.1.1.2 2014/07/12 11:58:16 spz Exp $	*/
/* stables.c

   Tables of information only used by server... */

/*
 * Copyright (c) 2004-2011,2013-2014 by Internet Systems Consortium, Inc. ("ISC")
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
__RCSID("$NetBSD: stables.c,v 1.1.1.2 2014/07/12 11:58:16 spz Exp $");

#include "dhcpd.h"
#include <syslog.h>

#if defined (FAILOVER_PROTOCOL)

/* This is used to indicate some kind of failure when generating a
   failover option. */
failover_option_t null_failover_option = { 0, 0 };
failover_option_t skip_failover_option = { 0, 0 };

/* Information about failover options, for printing, encoding
   and decoding. */
struct failover_option_info ft_options [] =
{
	{ 0, "unused", FT_UNDEF, 0, 0, 0 },
	{ FTO_ADDRESSES_TRANSFERRED, "addresses-transferred", FT_UINT32, 1,
	  FM_OFFSET(addresses_transferred), FTB_ADDRESSES_TRANSFERRED },
	{ FTO_ASSIGNED_IP_ADDRESS, "assigned-IP-address", FT_IPADDR, 1,
	  FM_OFFSET(assigned_addr), FTB_ASSIGNED_IP_ADDRESS },
	{ FTO_BINDING_STATUS, "binding-status", FT_UINT8, 1,
	  FM_OFFSET(binding_status), FTB_BINDING_STATUS },
	{ FTO_CLIENT_IDENTIFIER, "client-identifier", FT_BYTES, 0,
	  FM_OFFSET(client_identifier), FTB_CLIENT_IDENTIFIER },
	{ FTO_CHADDR, "client-hardware-address", FT_BYTES, 0,
	  FM_OFFSET(chaddr), FTB_CHADDR },
	{ FTO_CLTT, "client-last-transaction-time", FT_UINT32, 1,
	  FM_OFFSET(cltt), FTB_CLTT },
	{ FTO_REPLY_OPTIONS, "client-reply-options", FT_BYTES, 0,
	  FM_OFFSET(reply_options), FTB_REPLY_OPTIONS },
	{ FTO_REQUEST_OPTIONS, "client-request-options", FT_BYTES, 0,
	  FM_OFFSET(request_options), FTB_REQUEST_OPTIONS },
	{ FTO_DDNS, "DDNS", FT_DDNS, 1, FM_OFFSET(ddns), FTB_DDNS },
	{ FTO_DELAYED_SERVICE, "delayed-service", FT_UINT8, 1,
	  FM_OFFSET(delayed_service), FTB_DELAYED_SERVICE },
	{ FTO_HBA, "hash-bucket-assignment", FT_BYTES, 0,
	  FM_OFFSET(hba), FTB_HBA },
	{ FTO_IP_FLAGS, "IP-flags", FT_UINT16, 1,
	  FM_OFFSET(ip_flags), FTB_IP_FLAGS },
	{ FTO_LEASE_EXPIRY, "lease-expiration-time", FT_UINT32, 1,
	  FM_OFFSET(expiry), FTB_LEASE_EXPIRY },
	{ FTO_MAX_UNACKED, "max-unacked-bndupd", FT_UINT32, 1, 
	  FM_OFFSET(max_unacked), FTB_MAX_UNACKED },
	{ FTO_MCLT, "MCLT", FT_UINT32, 1, FM_OFFSET(mclt), FTB_MCLT },
	{ FTO_MESSAGE, "message", FT_TEXT, 0,
	  FM_OFFSET(message), FTB_MESSAGE },
	{ FTO_MESSAGE_DIGEST, "message-digest", FT_BYTES, 0,
	  FM_OFFSET(message_digest), FTB_MESSAGE_DIGEST },
	{ FTO_POTENTIAL_EXPIRY, "potential-expiration-time", FT_UINT32, 1,
	  FM_OFFSET(potential_expiry), FTB_POTENTIAL_EXPIRY },
	{ FTO_RECEIVE_TIMER, "receive-timer", FT_UINT32, 1,
	  FM_OFFSET(receive_timer), FTB_RECEIVE_TIMER },
	{ FTO_PROTOCOL_VERSION, "protocol-version", FT_UINT8, 1,
	  FM_OFFSET(protocol_version), FTB_PROTOCOL_VERSION },
	{ FTO_REJECT_REASON, "reject-reason", FT_UINT8, 1,
	  FM_OFFSET(reject_reason), FTB_REJECT_REASON },
	{ FTO_RELATIONSHIP_NAME, "relationship-name", FT_BYTES, 0,
	  FM_OFFSET(relationship_name), FTB_RELATIONSHIP_NAME },
	{ FTO_SERVER_FLAGS, "server-flags", FT_UINT8, 1,
	  FM_OFFSET(server_flags), FTB_SERVER_FLAGS },
	{ FTO_SERVER_STATE, "server-state", FT_UINT8, 1,
	  FM_OFFSET(server_state), FTB_SERVER_STATE },
	{ FTO_STOS, "start-time-of-state", FT_UINT32, 1,
	  FM_OFFSET(stos), FTB_STOS },
	{ FTO_TLS_REPLY, "TLS-reply", FT_UINT8, 1,
	  FM_OFFSET(tls_reply), FTB_TLS_REPLY },
	{ FTO_TLS_REQUEST, "TLS-request", FT_UINT8, 1,
	  FM_OFFSET(tls_request), FTB_TLS_REQUEST },
	{ FTO_VENDOR_CLASS, "vendor-class-identifier", FT_BYTES, 0,
	  FM_OFFSET(vendor_class), FTB_VENDOR_CLASS },
	{ FTO_VENDOR_OPTIONS, "vendor-specific-options", FT_BYTES, 0,
	  FM_OFFSET(vendor_options), FTB_VENDOR_OPTIONS }
};

/* These are really options that make sense for a particular request - if
   some other option comes in, we're not going to use it, so we can just
   discard it.  Note that the message-digest option is allowed for all
   message types, but is not saved - it's just used to validate the message
   and then discarded - so it's not mentioned here. */

u_int32_t fto_allowed [] = {
	0,	/* 0 unused */
	0,	/* 1 POOLREQ */
	FTB_ADDRESSES_TRANSFERRED, /* 2 POOLRESP */
	(FTB_ASSIGNED_IP_ADDRESS | FTB_BINDING_STATUS | FTB_CLIENT_IDENTIFIER |
	 FTB_CHADDR | FTB_DDNS | FTB_IP_FLAGS | FTB_LEASE_EXPIRY |
	 FTB_POTENTIAL_EXPIRY | FTB_STOS | FTB_CLTT | FTB_REQUEST_OPTIONS |
	 FTB_REPLY_OPTIONS), /* 3 BNDUPD */
	(FTB_ASSIGNED_IP_ADDRESS | FTB_BINDING_STATUS | FTB_CLIENT_IDENTIFIER |
	 FTB_CHADDR | FTB_DDNS | FTB_IP_FLAGS | FTB_LEASE_EXPIRY |
	 FTB_POTENTIAL_EXPIRY | FTB_STOS | FTB_CLTT | FTB_REQUEST_OPTIONS |
	 FTB_REPLY_OPTIONS | FTB_REJECT_REASON | FTB_MESSAGE), /* 4 BNDACK */
	(FTB_RELATIONSHIP_NAME | FTB_MAX_UNACKED | FTB_RECEIVE_TIMER |
	 FTB_VENDOR_CLASS | FTB_PROTOCOL_VERSION | FTB_TLS_REQUEST |
	 FTB_MCLT | FTB_HBA), /* 5 CONNECT */
	(FTB_RELATIONSHIP_NAME | FTB_MAX_UNACKED | FTB_RECEIVE_TIMER |
	 FTB_VENDOR_CLASS | FTB_PROTOCOL_VERSION | FTB_TLS_REPLY |
	 FTB_REJECT_REASON | FTB_MESSAGE), /* CONNECTACK */
	0, /* 7 UPDREQALL */
	0, /* 8 UPDDONE */
	0, /* 9 UPDREQ */
	(FTB_SERVER_STATE | FTB_SERVER_FLAGS | FTB_STOS), /* 10 STATE */
	0,	/* 11 CONTACT */
	(FTB_REJECT_REASON | FTB_MESSAGE) /* 12 DISCONNECT */
};

/* Sizes of the various types. */
int ft_sizes [] = {
	1, /* FT_UINT8 */
	4, /* FT_IPADDR */
	4, /* FT_UINT32 */
	1, /* FT_BYTES */
	1, /* FT_TEXT_OR_BYTES */
	0, /* FT_DDNS */
	0, /* FT_DDNS1 */
	2, /* FT_UINT16 */
	1, /* FT_TEXT */
	0, /* FT_UNDEF */
	0, /* FT_DIGEST */
};

/* Names of the various failover link states. */
const char *dhcp_flink_state_names [] = {
	"invalid state 0",
	"startup",
	"message length wait",
	"message wait",
	"disconnected"
};
#endif /* FAILOVER_PROTOCOL */

/* Failover binding state names.   These are used even if there is no
   failover protocol support. */
const char *binding_state_names [] = {
	"free", "active", "expired", "released", "abandoned",
	"reset", "backup" };

struct universe agent_universe;
static struct option agent_options[] = {
	{ "circuit-id", "X",			&agent_universe,   1, 1 },
	{ "remote-id", "X",			&agent_universe,   2, 1 },
	{ "agent-id", "I",			&agent_universe,   3, 1 },
	{ "DOCSIS-device-class", "L",		&agent_universe,   4, 1 },
	{ "link-selection", "I",		&agent_universe,   5, 1 },
	{ NULL, NULL, NULL, 0, 0 }
};

struct universe server_universe;
static struct option server_options[] = {
	{ "default-lease-time", "T",		&server_universe,   1, 1 },
	{ "max-lease-time", "T",		&server_universe,   2, 1 },
	{ "min-lease-time", "T",		&server_universe,   3, 1 },
	{ "dynamic-bootp-lease-cutoff", "T",	&server_universe,   4, 1 },
	{ "dynamic-bootp-lease-length", "L",	&server_universe,   5, 1 },
	{ "boot-unknown-clients", "f",		&server_universe,   6, 1 },
	{ "dynamic-bootp", "f",			&server_universe,   7, 1 },
	{ "allow-bootp", "f",			&server_universe,   8, 1 },
	{ "allow-booting", "f",			&server_universe,   9, 1 },
	{ "one-lease-per-client", "f",		&server_universe,  10, 1 },
	{ "get-lease-hostnames", "f",		&server_universe,  11, 1 },
	{ "use-host-decl-names", "f",		&server_universe,  12, 1 },
	{ "use-lease-addr-for-default-route", "f",
						&server_universe,  13, 1 },
	{ "min-secs", "B",			&server_universe,  14, 1 },
	{ "filename", "t",			&server_universe,  15, 1 },
	{ "server-name", "t",			&server_universe,  16, 1 },
	{ "next-server", "I",			&server_universe,  17, 1 },
	{ "authoritative", "f",			&server_universe,  18, 1 },
	{ "vendor-option-space", "U",		&server_universe,  19, 1 },
	{ "always-reply-rfc1048", "f",		&server_universe,  20, 1 },
	{ "site-option-space", "X",		&server_universe,  21, 1 },
	{ "always-broadcast", "f",		&server_universe,  22, 1 },
	{ "ddns-domainname", "t",		&server_universe,  23, 1 },
	{ "ddns-hostname", "t",			&server_universe,  24, 1 },
	{ "ddns-rev-domainname", "t",		&server_universe,  25, 1 },
	{ "lease-file-name", "t",		&server_universe,  26, 1 },
	{ "pid-file-name", "t",			&server_universe,  27, 1 },
	{ "duplicates", "f",			&server_universe,  28, 1 },
	{ "declines", "f",			&server_universe,  29, 1 },
	{ "ddns-updates", "f",			&server_universe,  30, 1 },
	{ "omapi-port", "S",			&server_universe,  31, 1 },
	{ "local-port", "S",			&server_universe,  32, 1 },
	{ "limited-broadcast-address", "I",	&server_universe,  33, 1 },
	{ "remote-port", "S",			&server_universe,  34, 1 },
	{ "local-address", "I",			&server_universe,  35, 1 },
	{ "omapi-key", "d",			&server_universe,  36, 1 },
	{ "stash-agent-options", "f",		&server_universe,  37, 1 },
	{ "ddns-ttl", "T",			&server_universe,  38, 1 },
	{ "ddns-update-style", "Nddns-styles.",	&server_universe,  39, 1 },
	{ "client-updates", "f",		&server_universe,  40, 1 },
	{ "update-optimization", "f",		&server_universe,  41, 1 },
	{ "ping-check", "f",			&server_universe,  42, 1 },
	{ "update-static-leases", "f",		&server_universe,  43, 1 },
	{ "log-facility", "Nsyslog-facilities.",
						&server_universe,  44, 1 },
	{ "do-forward-updates", "f",		&server_universe,  45, 1 },
	{ "ping-timeout", "T",			&server_universe,  46, 1 },
	{ "infinite-is-reserved", "f",		&server_universe,  47, 1 },
	{ "update-conflict-detection", "f",	&server_universe,  48, 1 },
	{ "leasequery", "f",			&server_universe,  49, 1 },
	{ "adaptive-lease-time-threshold", "B",	&server_universe,  50, 1 },
	{ "do-reverse-updates", "f",		&server_universe,  51, 1 },
	{ "fqdn-reply", "f",			&server_universe,  52, 1 },
	{ "preferred-lifetime", "T",		&server_universe,  53, 1 },
	{ "dhcpv6-lease-file-name", "t",	&server_universe,  54, 1 },
	{ "dhcpv6-pid-file-name", "t",		&server_universe,  55, 1 },
	{ "limit-addrs-per-ia", "L",		&server_universe,  56, 1 },
	{ "limit-prefs-per-ia", "L",		&server_universe,  57, 1 },
/* Assert a configuration parsing error if delayed-ack isn't compiled in. */
#if defined(DELAYED_ACK)
	{ "delayed-ack", "S",			&server_universe,  58, 1 },
	{ "max-ack-delay", "L",			&server_universe,  59, 1 },
#endif
#if defined(LDAP_CONFIGURATION)
	{ "ldap-server", "t",			&server_universe,  60, 1 },
	{ "ldap-port", "d",			&server_universe,  61, 1 },
	{ "ldap-username", "t",			&server_universe,  62, 1 },
	{ "ldap-password", "t",			&server_universe,  63, 1 },
	{ "ldap-base-dn", "t",			&server_universe,  64, 1 },
	{ "ldap-method", "Nldap-methods.",	&server_universe,  65, 1 },
	{ "ldap-debug-file", "t",		&server_universe,  66, 1 },
	{ "ldap-dhcp-server-cn", "t",		&server_universe,  67, 1 },
	{ "ldap-referrals", "f",		&server_universe,  68, 1 },
#if defined(LDAP_USE_SSL)
	{ "ldap-ssl", "Nldap-ssl-usage.",	&server_universe,  69, 1 },
	{ "ldap-tls-reqcert", "Nldap-tls-reqcert.",	&server_universe,  70, 1 },
	{ "ldap-tls-ca-file", "t",		&server_universe,  71, 1 },
	{ "ldap-tls-ca-dir", "t",		&server_universe,  72, 1 },
	{ "ldap-tls-cert", "t",			&server_universe,  73, 1 },
	{ "ldap-tls-key", "t",			&server_universe,  74, 1 },
	{ "ldap-tls-crlcheck", "Nldap-tls-crlcheck.",	&server_universe,  75, 1 },
	{ "ldap-tls-ciphers", "t",		&server_universe,  76, 1 },
	{ "ldap-tls-randfile", "t",		&server_universe,  77, 1 },
#endif /* LDAP_USE_SSL */
#endif /* LDAP_CONFIGURATION */
	{ "dhcp-cache-threshold", "B",		&server_universe,  78, 1 },
	{ "dont-use-fsync", "f",		&server_universe,  79, 1 },
	{ "ddns-local-address4", "I",		&server_universe,  80, 1 },
	{ "ddns-local-address6", "6",		&server_universe,  81, 1 },
	{ "ignore-client-uids", "f",		&server_universe,  82, 1 },
	{ NULL, NULL, NULL, 0, 0 }
};

#if defined(LDAP_CONFIGURATION)
struct enumeration_value ldap_values [] = {
	{ "static", LDAP_METHOD_STATIC },
	{ "dynamic", LDAP_METHOD_DYNAMIC },
	{ (char *) 0, 0 }
};

struct enumeration ldap_methods = {
	(struct enumeration *)0,
	"ldap-methods", 1,
	ldap_values
};

#if defined(LDAP_USE_SSL)
struct enumeration_value ldap_ssl_usage_values [] = {
	{ "off", LDAP_SSL_OFF },
	{ "on",LDAP_SSL_ON },
	{ "ldaps", LDAP_SSL_LDAPS },
	{ "start_tls", LDAP_SSL_TLS },
	{ (char *) 0, 0 }
};

struct enumeration ldap_ssl_usage_enum = {
	(struct enumeration *)0,
	"ldap-ssl-usage", 1,
	ldap_ssl_usage_values
};

struct enumeration_value ldap_tls_reqcert_values [] = {
	{ "never", LDAP_OPT_X_TLS_NEVER },
	{ "hard", LDAP_OPT_X_TLS_HARD  },
	{ "demand", LDAP_OPT_X_TLS_DEMAND},
	{ "allow", LDAP_OPT_X_TLS_ALLOW },
	{ "try", LDAP_OPT_X_TLS_TRY   },
	{ (char *) 0, 0 }
};
struct enumeration ldap_tls_reqcert_enum = {
	(struct enumeration *)0,
	"ldap-tls-reqcert", 1,
	ldap_tls_reqcert_values
};

struct enumeration_value ldap_tls_crlcheck_values [] = {
	{ "none", LDAP_OPT_X_TLS_CRL_NONE},
	{ "peer", LDAP_OPT_X_TLS_CRL_PEER},
	{ "all",  LDAP_OPT_X_TLS_CRL_ALL },
	{ (char *) 0, 0 }
};
struct enumeration ldap_tls_crlcheck_enum = {
	(struct enumeration *)0,
	"ldap-tls-crlcheck", 1,
	ldap_tls_crlcheck_values
};
#endif
#endif

struct enumeration_value ddns_styles_values [] = {
	{ "none", 0 },
	{ "ad-hoc", 1 },
	{ "interim", 2 },
	{ "standard", 3 },
	{ (char *)0, 0 }
};

struct enumeration ddns_styles = {
	(struct enumeration *)0,
	"ddns-styles", 1,
	ddns_styles_values
};

struct enumeration_value syslog_values [] = {
#if defined (LOG_KERN)
	{ "kern", LOG_KERN },
#endif
#if defined (LOG_USER)
	{ "user", LOG_USER },
#endif
#if defined (LOG_MAIL)
	{ "mail", LOG_MAIL },
#endif
#if defined (LOG_DAEMON)
	{ "daemon", LOG_DAEMON },
#endif
#if defined (LOG_AUTH)
	{ "auth", LOG_AUTH },
#endif
#if defined (LOG_SYSLOG)
	{ "syslog", LOG_SYSLOG },
#endif
#if defined (LOG_LPR)
	{ "lpr", LOG_LPR },
#endif
#if defined (LOG_NEWS)
	{ "news", LOG_NEWS },
#endif
#if defined (LOG_UUCP)
	{ "uucp", LOG_UUCP },
#endif
#if defined (LOG_CRON)
	{ "cron", LOG_CRON },
#endif
#if defined (LOG_AUTHPRIV)
	{ "authpriv", LOG_AUTHPRIV },
#endif
#if defined (LOG_FTP)
	{ "ftp", LOG_FTP },
#endif
#if defined (LOG_LOCAL0)
	{ "local0", LOG_LOCAL0 },
#endif
#if defined (LOG_LOCAL1)
	{ "local1", LOG_LOCAL1 },
#endif
#if defined (LOG_LOCAL2)
	{ "local2", LOG_LOCAL2 },
#endif
#if defined (LOG_LOCAL3)
	{ "local3", LOG_LOCAL3 },
#endif
#if defined (LOG_LOCAL4)
	{ "local4", LOG_LOCAL4 },
#endif
#if defined (LOG_LOCAL5)
	{ "local5", LOG_LOCAL5 },
#endif
#if defined (LOG_LOCAL6)
	{ "local6", LOG_LOCAL6 },
#endif
#if defined (LOG_LOCAL7)
	{ "local7", LOG_LOCAL7 },
#endif
	{ (char *)0, 0 }
};

struct enumeration syslog_enum = {
	(struct enumeration *)0,
	"syslog-facilities", 1,
	syslog_values
};

void initialize_server_option_spaces()
{
	int i;
	unsigned code;

	/* Set up the Relay Agent Information Option suboption space... */
	agent_universe.name = "agent";
	agent_universe.concat_duplicates = 0;
	agent_universe.option_state_dereference =
		linked_option_state_dereference;
	agent_universe.lookup_func = lookup_linked_option;
	agent_universe.save_func = save_linked_option;
	agent_universe.delete_func = delete_linked_option;
	agent_universe.encapsulate = linked_option_space_encapsulate;
	agent_universe.foreach = linked_option_space_foreach;
	agent_universe.decode = parse_option_buffer;
	agent_universe.index = universe_count++;
	agent_universe.length_size = 1;
	agent_universe.tag_size = 1;
	agent_universe.get_tag = getUChar;
	agent_universe.store_tag = putUChar;
	agent_universe.get_length = getUChar;
	agent_universe.store_length = putUChar;
	agent_universe.site_code_min = 0;
	agent_universe.end = 0;
	universes [agent_universe.index] = &agent_universe;
	if (!option_name_new_hash(&agent_universe.name_hash,
				  AGENT_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&agent_universe.code_hash,
				  AGENT_HASH_SIZE, MDL))
		log_fatal ("Can't allocate agent option hash table.");
	for (i = 0 ; agent_options[i].name ; i++) {
		option_code_hash_add(agent_universe.code_hash,
				     &agent_options[i].code, 0,
				     &agent_options[i], MDL);
		option_name_hash_add(agent_universe.name_hash,
				     agent_options[i].name, 0,
				     &agent_options[i], MDL);
	}
#if defined(REPORT_HASH_PERFORMANCE)
	log_info("Relay Agent name hash: %s",
		 option_name_hash_report(agent_universe.name_hash));
	log_info("Relay Agent code hash: %s",
		 option_code_hash_report(agent_universe.code_hash));
#endif
	code = DHO_DHCP_AGENT_OPTIONS;
	option_code_hash_lookup(&agent_universe.enc_opt,
				dhcp_universe.code_hash, &code, 0, MDL);

	/* Set up the server option universe... */
	server_universe.name = "server";
	server_universe.concat_duplicates = 0;
	server_universe.lookup_func = lookup_hashed_option;
	server_universe.option_state_dereference =
		hashed_option_state_dereference;
	server_universe.save_func = save_hashed_option;
	server_universe.delete_func = delete_hashed_option;
	server_universe.encapsulate = hashed_option_space_encapsulate;
	server_universe.foreach = hashed_option_space_foreach;
	server_universe.length_size = 1; /* Never used ... */
	server_universe.tag_size = 4;
	server_universe.store_tag = putUChar;
	server_universe.store_length = putUChar;
	server_universe.site_code_min = 0;
	server_universe.end = 0;
	server_universe.index = universe_count++;
	universes [server_universe.index] = &server_universe;
	if (!option_name_new_hash(&server_universe.name_hash,
				  SERVER_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&server_universe.code_hash,
				  SERVER_HASH_SIZE, MDL))
		log_fatal ("Can't allocate server option hash table.");
	for (i = 0 ; server_options[i].name ; i++) {
		option_code_hash_add(server_universe.code_hash,
				     &server_options[i].code, 0,
				     &server_options[i], MDL);
		option_name_hash_add(server_universe.name_hash,
				     server_options[i].name, 0,
				     &server_options[i], MDL);
	}
#if defined(REPORT_HASH_PERFORMANCE)
	log_info("Server-Config Option name hash: %s",
		 option_name_hash_report(server_universe.name_hash));
	log_info("Server-Config Option code hash: %s",
		 option_code_hash_report(server_universe.code_hash));
#endif

	/* Add the server and agent option spaces to the option space hash. */
	universe_hash_add (universe_hash,
			   agent_universe.name, 0, &agent_universe, MDL);
	universe_hash_add (universe_hash,
			   server_universe.name, 0, &server_universe, MDL);

	/* Make the server universe the configuration option universe. */
	config_universe = &server_universe;

	code = SV_VENDOR_OPTION_SPACE;
	option_code_hash_lookup(&vendor_cfg_option, server_universe.code_hash,
				&code, 0, MDL);
}
