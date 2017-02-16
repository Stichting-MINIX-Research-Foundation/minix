/*	$NetBSD: dhcrelay.c,v 1.5 2014/07/12 12:09:37 spz Exp $	*/
/* dhcrelay.c

   DHCP/BOOTP Relay Agent. */

/*
 * Copyright(c) 2004-2014 by Internet Systems Consortium, Inc.("ISC")
 * Copyright(c) 1997-2003 by Internet Software Consortium
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
__RCSID("$NetBSD: dhcrelay.c,v 1.5 2014/07/12 12:09:37 spz Exp $");

#include "dhcpd.h"
#include <syslog.h>
#include <signal.h>
#include <sys/time.h>

TIME default_lease_time = 43200; /* 12 hours... */
TIME max_lease_time = 86400; /* 24 hours... */
struct tree_cache *global_options[256];

struct option *requested_opts[2];

/* Needed to prevent linking against conflex.c. */
int lexline;
int lexchar;
char *token_line;
char *tlname;

const char *path_dhcrelay_pid = _PATH_DHCRELAY_PID;
isc_boolean_t no_dhcrelay_pid = ISC_FALSE;
/* False (default) => we write and use a pid file */
isc_boolean_t no_pid_file = ISC_FALSE;

int bogus_agent_drops = 0;	/* Packets dropped because agent option
				   field was specified and we're not relaying
				   packets that already have an agent option
				   specified. */
int bogus_giaddr_drops = 0;	/* Packets sent to us to relay back to a
				   client, but with a bogus giaddr. */
int client_packets_relayed = 0;	/* Packets relayed from client to server. */
int server_packet_errors = 0;	/* Errors sending packets to servers. */
int server_packets_relayed = 0;	/* Packets relayed from server to client. */
int client_packet_errors = 0;	/* Errors sending packets to clients. */

int add_agent_options = 0;	/* If nonzero, add relay agent options. */

int agent_option_errors = 0;    /* Number of packets forwarded without
				   agent options because there was no room. */
int drop_agent_mismatches = 0;	/* If nonzero, drop server replies that
				   don't have matching circuit-id's. */
int corrupt_agent_options = 0;	/* Number of packets dropped because
				   relay agent information option was bad. */
int missing_agent_option = 0;	/* Number of packets dropped because no
				   RAI option matching our ID was found. */
int bad_circuit_id = 0;		/* Circuit ID option in matching RAI option
				   did not match any known circuit ID. */
int missing_circuit_id = 0;	/* Circuit ID option in matching RAI option
				   was missing. */
int max_hop_count = 10;		/* Maximum hop count */

#ifdef DHCPv6
	/* Force use of DHCPv6 interface-id option. */
isc_boolean_t use_if_id = ISC_FALSE;
#endif

	/* Maximum size of a packet with agent options added. */
int dhcp_max_agent_option_packet_length = DHCP_MTU_MIN;

	/* What to do about packets we're asked to relay that
	   already have a relay option: */
enum { forward_and_append,	/* Forward and append our own relay option. */
       forward_and_replace,	/* Forward, but replace theirs with ours. */
       forward_untouched,	/* Forward without changes. */
       discard } agent_relay_mode = forward_and_replace;

u_int16_t local_port;
u_int16_t remote_port;

/* Relay agent server list. */
struct server_list {
	struct server_list *next;
	struct sockaddr_in to;
} *servers;

#ifdef DHCPv6
struct stream_list {
	struct stream_list *next;
	struct interface_info *ifp;
	struct sockaddr_in6 link;
	int id;
} *downstreams, *upstreams;

static struct stream_list *parse_downstream(char *);
static struct stream_list *parse_upstream(char *);
static void setup_streams(void);

/*
 * A pointer to a subscriber id to add to the message we forward.
 * This is primarily for testing purposes as we only have one id
 * for the entire relay and don't determine one per client which
 * would be more useful.
 */
char *dhcrelay_sub_id = NULL;
#endif

static void do_relay4(struct interface_info *, struct dhcp_packet *,
	              unsigned int, unsigned int, struct iaddr,
		      struct hardware *);
static int add_relay_agent_options(struct interface_info *,
				   struct dhcp_packet *, unsigned,
				   struct in_addr);
static int find_interface_by_agent_option(struct dhcp_packet *,
			       struct interface_info **, u_int8_t *, int);
static int strip_relay_agent_options(struct interface_info *,
				     struct interface_info **,
				     struct dhcp_packet *, unsigned);

static const char copyright[] =
"Copyright 2004-2014 Internet Systems Consortium.";
static const char arr[] = "All rights reserved.";
static const char message[] =
"Internet Systems Consortium DHCP Relay Agent";
static const char url[] =
"For info, please visit https://www.isc.org/software/dhcp/";

#ifdef DHCPv6
#define DHCRELAY_USAGE \
"Usage: dhcrelay [-4] [-d] [-q] [-a] [-D]\n"\
"                     [-A <length>] [-c <hops>] [-p <port>]\n" \
"                     [-pf <pid-file>] [--no-pid]\n"\
"                     [-m append|replace|forward|discard]\n" \
"                     [-i interface0 [ ... -i interfaceN]\n" \
"                     server0 [ ... serverN]\n\n" \
"       dhcrelay -6   [-d] [-q] [-I] [-c <hops>] [-p <port>]\n" \
"                     [-pf <pid-file>] [--no-pid]\n" \
"                     [-s <subscriber-id>]\n" \
"                     -l lower0 [ ... -l lowerN]\n" \
"                     -u upper0 [ ... -u upperN]\n" \
"       lower (client link): [address%%]interface[#index]\n" \
"       upper (server link): [address%%]interface"
#else
#define DHCRELAY_USAGE \
"Usage: dhcrelay [-d] [-q] [-a] [-D] [-A <length>] [-c <hops>] [-p <port>]\n" \
"                [-pf <pid-file>] [--no-pid]\n" \
"                [-m append|replace|forward|discard]\n" \
"                [-i interface0 [ ... -i interfaceN]\n" \
"                server0 [ ... serverN]\n\n"
#endif

static void usage(void) {
	log_fatal(DHCRELAY_USAGE);
}

int 
main(int argc, char **argv) {
	isc_result_t status;
	struct servent *ent;
	struct server_list *sp = NULL;
	struct interface_info *tmp = NULL;
	char *service_local = NULL, *service_remote = NULL;
	u_int16_t port_local = 0, port_remote = 0;
	int no_daemon = 0, quiet = 0;
	int fd;
	int i;
#ifdef DHCPv6
	struct stream_list *sl = NULL;
	int local_family_set = 0;
#endif

	/* Make sure that file descriptors 0(stdin), 1,(stdout), and
	   2(stderr) are open. To do this, we assume that when we
	   open a file the lowest available file descriptor is used. */
	fd = open("/dev/null", O_RDWR);
	if (fd == 0)
		fd = open("/dev/null", O_RDWR);
	if (fd == 1)
		fd = open("/dev/null", O_RDWR);
	if (fd == 2)
		log_perror = 0; /* No sense logging to /dev/null. */
	else if (fd != -1)
		close(fd);

	openlog("dhcrelay", LOG_NDELAY, LOG_DAEMON);

#if !defined(DEBUG)
	setlogmask(LOG_UPTO(LOG_INFO));
#endif	

	/* Set up the isc and dns library managers */
	status = dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
				     NULL, NULL);
	if (status != ISC_R_SUCCESS)
		log_fatal("Can't initialize context: %s",
			  isc_result_totext(status));

	/* Set up the OMAPI. */
	status = omapi_init();
	if (status != ISC_R_SUCCESS)
		log_fatal("Can't initialize OMAPI: %s",
			   isc_result_totext(status));

	/* Set up the OMAPI wrappers for the interface object. */
	interface_setup();

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-4")) {
#ifdef DHCPv6
			if (local_family_set && (local_family == AF_INET6)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET;
		} else if (!strcmp(argv[i], "-6")) {
			if (local_family_set && (local_family == AF_INET)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET6;
#endif
		} else if (!strcmp(argv[i], "-d")) {
			no_daemon = 1;
		} else if (!strcmp(argv[i], "-q")) {
			quiet = 1;
			quiet_interface_discovery = 1;
		} else if (!strcmp(argv[i], "-p")) {
			if (++i == argc)
				usage();
			local_port = validate_port(argv[i]);
			log_debug("binding to user-specified port %d",
				  ntohs(local_port));
		} else if (!strcmp(argv[i], "-c")) {
			int hcount;
			if (++i == argc)
				usage();
			hcount = atoi(argv[i]);
			if (hcount <= 255)
				max_hop_count= hcount;
			else
				usage();
 		} else if (!strcmp(argv[i], "-i")) {
#ifdef DHCPv6
			if (local_family_set && (local_family == AF_INET6)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET;
#endif
			if (++i == argc) {
				usage();
			}
			if (strlen(argv[i]) >= sizeof(tmp->name)) {
				log_fatal("%s: interface name too long "
					  "(is %ld)",
					  argv[i], (long)strlen(argv[i]));
			}
			status = interface_allocate(&tmp, MDL);
			if (status != ISC_R_SUCCESS) {
				log_fatal("%s: interface_allocate: %s",
					  argv[i],
					  isc_result_totext(status));
			}
			strcpy(tmp->name, argv[i]);
			interface_snorf(tmp, INTERFACE_REQUESTED);
			interface_dereference(&tmp, MDL);
		} else if (!strcmp(argv[i], "-a")) {
#ifdef DHCPv6
			if (local_family_set && (local_family == AF_INET6)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET;
#endif
			add_agent_options = 1;
		} else if (!strcmp(argv[i], "-A")) {
#ifdef DHCPv6
			if (local_family_set && (local_family == AF_INET6)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET;
#endif
			if (++i == argc)
				usage();

			dhcp_max_agent_option_packet_length = atoi(argv[i]);

			if (dhcp_max_agent_option_packet_length > DHCP_MTU_MAX)
				log_fatal("%s: packet length exceeds "
					  "longest possible MTU\n",
					  argv[i]);
		} else if (!strcmp(argv[i], "-m")) {
#ifdef DHCPv6
			if (local_family_set && (local_family == AF_INET6)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET;
#endif
			if (++i == argc)
				usage();
			if (!strcasecmp(argv[i], "append")) {
				agent_relay_mode = forward_and_append;
			} else if (!strcasecmp(argv[i], "replace")) {
				agent_relay_mode = forward_and_replace;
			} else if (!strcasecmp(argv[i], "forward")) {
				agent_relay_mode = forward_untouched;
			} else if (!strcasecmp(argv[i], "discard")) {
				agent_relay_mode = discard;
			} else
				usage();
		} else if (!strcmp(argv[i], "-D")) {
#ifdef DHCPv6
			if (local_family_set && (local_family == AF_INET6)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET;
#endif
			drop_agent_mismatches = 1;
#ifdef DHCPv6
		} else if (!strcmp(argv[i], "-I")) {
			if (local_family_set && (local_family == AF_INET)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET6;
			use_if_id = ISC_TRUE;
		} else if (!strcmp(argv[i], "-l")) {
			if (local_family_set && (local_family == AF_INET)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET6;
			if (downstreams != NULL)
				use_if_id = ISC_TRUE;
			if (++i == argc)
				usage();
			sl = parse_downstream(argv[i]);
			sl->next = downstreams;
			downstreams = sl;
		} else if (!strcmp(argv[i], "-u")) {
			if (local_family_set && (local_family == AF_INET)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET6;
			if (++i == argc)
				usage();
			sl = parse_upstream(argv[i]);
			sl->next = upstreams;
			upstreams = sl;
		} else if (!strcmp(argv[i], "-s")) {
			if (local_family_set && (local_family == AF_INET)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET6;
			if (++i == argc)
				usage();
			dhcrelay_sub_id = argv[i];
#endif
		} else if (!strcmp(argv[i], "-pf")) {
			if (++i == argc)
				usage();
			path_dhcrelay_pid = argv[i];
			no_dhcrelay_pid = ISC_TRUE;
		} else if (!strcmp(argv[i], "--no-pid")) {
			no_pid_file = ISC_TRUE;
		} else if (!strcmp(argv[i], "--version")) {
			log_info("isc-dhcrelay-%s", PACKAGE_VERSION);
			exit(0);
		} else if (!strcmp(argv[i], "--help") ||
			   !strcmp(argv[i], "-h")) {
			log_info(DHCRELAY_USAGE);
			exit(0);
 		} else if (argv[i][0] == '-') {
			usage();
 		} else {
			struct hostent *he;
			struct in_addr ia, *iap = NULL;

#ifdef DHCPv6
			if (local_family_set && (local_family == AF_INET6)) {
				usage();
			}
			local_family_set = 1;
			local_family = AF_INET;
#endif
			if (inet_aton(argv[i], &ia)) {
				iap = &ia;
			} else {
				he = gethostbyname(argv[i]);
				if (!he) {
					log_error("%s: host unknown", argv[i]);
				} else {
					iap = ((struct in_addr *)
					       he->h_addr_list[0]);
				}
			}

			if (iap) {
				sp = ((struct server_list *)
				      dmalloc(sizeof *sp, MDL));
				if (!sp)
					log_fatal("no memory for server.\n");
				sp->next = servers;
				servers = sp;
				memcpy(&sp->to.sin_addr, iap, sizeof *iap);
			}
 		}
	}

	/*
	 * If the user didn't specify a pid file directly
	 * find one from environment variables or defaults
	 */
	if (no_dhcrelay_pid == ISC_FALSE) {
		if (local_family == AF_INET) {
			path_dhcrelay_pid = getenv("PATH_DHCRELAY_PID");
			if (path_dhcrelay_pid == NULL)
				path_dhcrelay_pid = _PATH_DHCRELAY_PID;
		}
#ifdef DHCPv6
		else {
			path_dhcrelay_pid = getenv("PATH_DHCRELAY6_PID");
			if (path_dhcrelay_pid == NULL)
				path_dhcrelay_pid = _PATH_DHCRELAY6_PID;
		}
#endif
	}

	if (!quiet) {
		log_info("%s %s", message, PACKAGE_VERSION);
		log_info(copyright);
		log_info(arr);
		log_info(url);
	} else 
		log_perror = 0;

	/* Set default port */
	if (local_family == AF_INET) {
 		service_local = "bootps";
 		service_remote = "bootpc";
		port_local = htons(67);
 		port_remote = htons(68);
	}
#ifdef DHCPv6
	else {
		service_local = "dhcpv6-server";
		service_remote = "dhcpv6-client";
		port_local = htons(547);
		port_remote = htons(546);
	}
#endif

	if (!local_port) {
		ent = getservbyname(service_local, "udp");
		if (ent)
			local_port = ent->s_port;
		else
			local_port = port_local;

		ent = getservbyname(service_remote, "udp");
		if (ent)
			remote_port = ent->s_port;
		else
			remote_port = port_remote;

		endservent();
	}

	if (local_family == AF_INET) {
		/* We need at least one server */
		if (servers == NULL) {
			log_fatal("No servers specified.");
		}


		/* Set up the server sockaddrs. */
		for (sp = servers; sp; sp = sp->next) {
			sp->to.sin_port = local_port;
			sp->to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
			sp->to.sin_len = sizeof sp->to;
#endif
		}
	}
#ifdef DHCPv6
	else {
		unsigned code;

		/* We need at least one upstream and one downstream interface */
		if (upstreams == NULL || downstreams == NULL) {
			log_info("Must specify at least one lower "
				 "and one upper interface.\n");
			usage();
		}

		/* Set up the initial dhcp option universe. */
		initialize_common_option_spaces();

		/* Check requested options. */
		code = D6O_RELAY_MSG;
		if (!option_code_hash_lookup(&requested_opts[0],
					     dhcpv6_universe.code_hash,
					     &code, 0, MDL))
			log_fatal("Unable to find the RELAY_MSG "
				  "option definition.");
		code = D6O_INTERFACE_ID;
		if (!option_code_hash_lookup(&requested_opts[1],
					     dhcpv6_universe.code_hash,
					     &code, 0, MDL))
			log_fatal("Unable to find the INTERFACE_ID "
				  "option definition.");
	}
#endif

	/* Get the current time... */
	gettimeofday(&cur_tv, NULL);

	/* Discover all the network interfaces. */
	discover_interfaces(DISCOVER_RELAY);

#ifdef DHCPv6
	if (local_family == AF_INET6)
		setup_streams();
#endif

	/* Become a daemon... */
	if (!no_daemon) {
		int pid;
		FILE *pf;
		int pfdesc;

		log_perror = 0;

		if ((pid = fork()) < 0)
			log_fatal("Can't fork daemon: %m");
		else if (pid)
			exit(0);

		if (no_pid_file == ISC_FALSE) {
			pfdesc = open(path_dhcrelay_pid,
				      O_CREAT | O_TRUNC | O_WRONLY, 0644);

			if (pfdesc < 0) {
				log_error("Can't create %s: %m",
					  path_dhcrelay_pid);
			} else {
				pf = fdopen(pfdesc, "w");
				if (!pf)
					log_error("Can't fdopen %s: %m",
						  path_dhcrelay_pid);
				else {
					fprintf(pf, "%ld\n",(long)getpid());
					fclose(pf);
				}	
			}
		}

		(void) close(0);
		(void) close(1);
		(void) close(2);
		(void) setsid();

		IGNORE_RET (chdir("/"));
	}

	/* Set up the packet handler... */
	if (local_family == AF_INET)
		bootp_packet_handler = do_relay4;
#ifdef DHCPv6
	else
		dhcpv6_packet_handler = do_packet6;
#endif

        /* install signal handlers */
	signal(SIGINT, dhcp_signal_handler);   /* control-c */
	signal(SIGTERM, dhcp_signal_handler);  /* kill */

	/* Start dispatching packets and timeouts... */
	dispatch();

	/* In fact dispatch() never returns. */
	return (0);
}

static void
do_relay4(struct interface_info *ip, struct dhcp_packet *packet,
	  unsigned int length, unsigned int from_port, struct iaddr from,
	  struct hardware *hfrom) {
	struct server_list *sp;
	struct sockaddr_in to;
	struct interface_info *out;
	struct hardware hto, *htop;

	if (packet->hlen > sizeof packet->chaddr) {
		log_info("Discarding packet with invalid hlen, received on "
			 "%s interface.", ip->name);
		return;
	}
	if (ip->address_count < 1 || ip->addresses == NULL) {
		log_info("Discarding packet received on %s interface that "
			 "has no IPv4 address assigned.", ip->name);
		return;
	}

	/* Find the interface that corresponds to the giaddr
	   in the packet. */
	if (packet->giaddr.s_addr) {
		for (out = interfaces; out; out = out->next) {
			int i;

			for (i = 0 ; i < out->address_count ; i++ ) {
				if (out->addresses[i].s_addr ==
				    packet->giaddr.s_addr) {
					i = -1;
					break;
				}
			}

			if (i == -1)
				break;
		}
	} else {
		out = NULL;
	}

	/* If it's a bootreply, forward it to the client. */
	if (packet->op == BOOTREPLY) {
		if (!(packet->flags & htons(BOOTP_BROADCAST)) &&
			can_unicast_without_arp(out)) {
			to.sin_addr = packet->yiaddr;
			to.sin_port = remote_port;

			/* and hardware address is not broadcast */
			htop = &hto;
		} else {
			to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
			to.sin_port = remote_port;

			/* hardware address is broadcast */
			htop = NULL;
		}
		to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
		to.sin_len = sizeof to;
#endif

		memcpy(&hto.hbuf[1], packet->chaddr, packet->hlen);
		hto.hbuf[0] = packet->htype;
		hto.hlen = packet->hlen + 1;

		/* Wipe out the agent relay options and, if possible, figure
		   out which interface to use based on the contents of the
		   option that we put on the request to which the server is
		   replying. */
		if (!(length =
		      strip_relay_agent_options(ip, &out, packet, length)))
			return;

		if (!out) {
			log_error("Packet to bogus giaddr %s.\n",
			      inet_ntoa(packet->giaddr));
			++bogus_giaddr_drops;
			return;
		}

		if (send_packet(out, NULL, packet, length, out->addresses[0],
				&to, htop) < 0) {
			++server_packet_errors;
		} else {
			log_debug("Forwarded BOOTREPLY for %s to %s",
			       print_hw_addr(packet->htype, packet->hlen,
					      packet->chaddr),
			       inet_ntoa(to.sin_addr));

			++server_packets_relayed;
		}
		return;
	}

	/* If giaddr matches one of our addresses, ignore the packet -
	   we just sent it. */
	if (out)
		return;

	/* Add relay agent options if indicated.   If something goes wrong,
	   drop the packet. */
	if (!(length = add_relay_agent_options(ip, packet, length,
					       ip->addresses[0])))
		return;

	/* If giaddr is not already set, Set it so the server can
	   figure out what net it's from and so that we can later
	   forward the response to the correct net.    If it's already
	   set, the response will be sent directly to the relay agent
	   that set giaddr, so we won't see it. */
	if (!packet->giaddr.s_addr)
		packet->giaddr = ip->addresses[0];
	if (packet->hops < max_hop_count)
		packet->hops = packet->hops + 1;
	else
		return;

	/* Otherwise, it's a BOOTREQUEST, so forward it to all the
	   servers. */
	for (sp = servers; sp; sp = sp->next) {
		if (send_packet((fallback_interface
				 ? fallback_interface : interfaces),
				 NULL, packet, length, ip->addresses[0],
				 &sp->to, NULL) < 0) {
			++client_packet_errors;
		} else {
			log_debug("Forwarded BOOTREQUEST for %s to %s",
			       print_hw_addr(packet->htype, packet->hlen,
					      packet->chaddr),
			       inet_ntoa(sp->to.sin_addr));
			++client_packets_relayed;
		}
	}
				 
}

/* Strip any Relay Agent Information options from the DHCP packet
   option buffer.   If there is a circuit ID suboption, look up the
   outgoing interface based upon it. */

static int
strip_relay_agent_options(struct interface_info *in,
			  struct interface_info **out,
			  struct dhcp_packet *packet,
			  unsigned length) {
	int is_dhcp = 0;
	u_int8_t *op, *nextop, *sp, *max;
	int good_agent_option = 0;
	int status;

	/* If we're not adding agent options to packets, we're not taking
	   them out either. */
	if (!add_agent_options)
		return (length);

	/* If there's no cookie, it's a bootp packet, so we should just
	   forward it unchanged. */
	if (memcmp(packet->options, DHCP_OPTIONS_COOKIE, 4))
		return (length);

	max = ((u_int8_t *)packet) + length;
	sp = op = &packet->options[4];

	while (op < max) {
		switch(*op) {
			/* Skip padding... */
		      case DHO_PAD:
			if (sp != op)
				*sp = *op;
			++op;
			++sp;
			continue;

			/* If we see a message type, it's a DHCP packet. */
		      case DHO_DHCP_MESSAGE_TYPE:
			is_dhcp = 1;
			goto skip;
			break;

			/* Quit immediately if we hit an End option. */
		      case DHO_END:
			if (sp != op)
				*sp++ = *op++;
			goto out;

		      case DHO_DHCP_AGENT_OPTIONS:
			/* We shouldn't see a relay agent option in a
			   packet before we've seen the DHCP packet type,
			   but if we do, we have to leave it alone. */
			if (!is_dhcp)
				goto skip;

			/* Do not process an agent option if it exceeds the
			 * buffer.  Fail this packet.
			 */
			nextop = op + op[1] + 2;
			if (nextop > max)
				return (0);

			status = find_interface_by_agent_option(packet,
								out, op + 2,
								op[1]);
			if (status == -1 && drop_agent_mismatches)
				return (0);
			if (status)
				good_agent_option = 1;
			op = nextop;
			break;

		      skip:
			/* Skip over other options. */
		      default:
			/* Fail if processing this option will exceed the
			 * buffer(op[1] is malformed).
			 */
			nextop = op + op[1] + 2;
			if (nextop > max)
				return (0);

			if (sp != op) {
				memmove(sp, op, op[1] + 2);
				sp += op[1] + 2;
				op = nextop;
			} else
				op = sp = nextop;

			break;
		}
	}
      out:

	/* If it's not a DHCP packet, we're not supposed to touch it. */
	if (!is_dhcp)
		return (length);

	/* If none of the agent options we found matched, or if we didn't
	   find any agent options, count this packet as not having any
	   matching agent options, and if we're relying on agent options
	   to determine the outgoing interface, drop the packet. */

	if (!good_agent_option) {
		++missing_agent_option;
		if (drop_agent_mismatches)
			return (0);
	}

	/* Adjust the length... */
	if (sp != op) {
		length = sp -((u_int8_t *)packet);

		/* Make sure the packet isn't short(this is unlikely,
		   but WTH) */
		if (length < BOOTP_MIN_LEN) {
			memset(sp, DHO_PAD, BOOTP_MIN_LEN - length);
			length = BOOTP_MIN_LEN;
		}
	}
	return (length);
}


/* Find an interface that matches the circuit ID specified in the
   Relay Agent Information option.   If one is found, store it through
   the pointer given; otherwise, leave the existing pointer alone.

   We actually deviate somewhat from the current specification here:
   if the option buffer is corrupt, we suggest that the caller not
   respond to this packet.  If the circuit ID doesn't match any known
   interface, we suggest that the caller to drop the packet.  Only if
   we find a circuit ID that matches an existing interface do we tell
   the caller to go ahead and process the packet. */

static int
find_interface_by_agent_option(struct dhcp_packet *packet,
			       struct interface_info **out,
			       u_int8_t *buf, int len) {
	int i = 0;
	u_int8_t *circuit_id = 0;
	unsigned circuit_id_len = 0;
	struct interface_info *ip;

	while (i < len) {
		/* If the next agent option overflows the end of the
		   packet, the agent option buffer is corrupt. */
		if (i + 1 == len ||
		    i + buf[i + 1] + 2 > len) {
			++corrupt_agent_options;
			return (-1);
		}
		switch(buf[i]) {
			/* Remember where the circuit ID is... */
		      case RAI_CIRCUIT_ID:
			circuit_id = &buf[i + 2];
			circuit_id_len = buf[i + 1];
			i += circuit_id_len + 2;
			continue;

		      default:
			i += buf[i + 1] + 2;
			break;
		}
	}

	/* If there's no circuit ID, it's not really ours, tell the caller
	   it's no good. */
	if (!circuit_id) {
		++missing_circuit_id;
		return (-1);
	}

	/* Scan the interface list looking for an interface whose
	   name matches the one specified in circuit_id. */

	for (ip = interfaces; ip; ip = ip->next) {
		if (ip->circuit_id &&
		    ip->circuit_id_len == circuit_id_len &&
		    !memcmp(ip->circuit_id, circuit_id, circuit_id_len))
			break;
	}

	/* If we got a match, use it. */
	if (ip) {
		*out = ip;
		return (1);
	}

	/* If we didn't get a match, the circuit ID was bogus. */
	++bad_circuit_id;
	return (-1);
}

/*
 * Examine a packet to see if it's a candidate to have a Relay
 * Agent Information option tacked onto its tail.   If it is, tack
 * the option on.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: dhcrelay.c,v 1.5 2014/07/12 12:09:37 spz Exp $");
static int
add_relay_agent_options(struct interface_info *ip, struct dhcp_packet *packet,
			unsigned length, struct in_addr giaddr) {
	int is_dhcp = 0, mms;
	unsigned optlen;
	u_int8_t *op, *nextop, *sp, *max, *end_pad = NULL;

	/* If we're not adding agent options to packets, we can skip
	   this. */
	if (!add_agent_options)
		return (length);

	/* If there's no cookie, it's a bootp packet, so we should just
	   forward it unchanged. */
	if (memcmp(packet->options, DHCP_OPTIONS_COOKIE, 4))
		return (length);

	max = ((u_int8_t *)packet) + dhcp_max_agent_option_packet_length;

	/* Commence processing after the cookie. */
	sp = op = &packet->options[4];

	while (op < max) {
		switch(*op) {
			/* Skip padding... */
		      case DHO_PAD:
			/* Remember the first pad byte so we can commandeer
			 * padded space.
			 *
			 * XXX: Is this really a good idea?  Sure, we can
			 * seemingly reduce the packet while we're looking,
			 * but if the packet was signed by the client then
			 * this padding is part of the checksum(RFC3118),
			 * and its nonpresence would break authentication.
			 */
			if (end_pad == NULL)
				end_pad = sp;

			if (sp != op)
				*sp++ = *op++;
			else
				sp = ++op;

			continue;

			/* If we see a message type, it's a DHCP packet. */
		      case DHO_DHCP_MESSAGE_TYPE:
			is_dhcp = 1;
			goto skip;

			/*
			 * If there's a maximum message size option, we
			 * should pay attention to it
			 */
		      case DHO_DHCP_MAX_MESSAGE_SIZE:
			mms = ntohs(*(op + 2));
			if (mms < dhcp_max_agent_option_packet_length &&
			    mms >= DHCP_MTU_MIN)
				max = ((u_int8_t *)packet) + mms;
			goto skip;

			/* Quit immediately if we hit an End option. */
		      case DHO_END:
			goto out;

		      case DHO_DHCP_AGENT_OPTIONS:
			/* We shouldn't see a relay agent option in a
			   packet before we've seen the DHCP packet type,
			   but if we do, we have to leave it alone. */
			if (!is_dhcp)
				goto skip;

			end_pad = NULL;

			/* There's already a Relay Agent Information option
			   in this packet.   How embarrassing.   Decide what
			   to do based on the mode the user specified. */

			switch(agent_relay_mode) {
			      case forward_and_append:
				goto skip;
			      case forward_untouched:
				return (length);
			      case discard:
				return (0);
			      case forward_and_replace:
			      default:
				break;
			}

			/* Skip over the agent option and start copying
			   if we aren't copying already. */
			op += op[1] + 2;
			break;

		      skip:
			/* Skip over other options. */
		      default:
			/* Fail if processing this option will exceed the
			 * buffer(op[1] is malformed).
			 */
			nextop = op + op[1] + 2;
			if (nextop > max)
				return (0);

			end_pad = NULL;

			if (sp != op) {
				memmove(sp, op, op[1] + 2);
				sp += op[1] + 2;
				op = nextop;
			} else
				op = sp = nextop;

			break;
		}
	}
      out:

	/* If it's not a DHCP packet, we're not supposed to touch it. */
	if (!is_dhcp)
		return (length);

	/* If the packet was padded out, we can store the agent option
	   at the beginning of the padding. */

	if (end_pad != NULL)
		sp = end_pad;

#if 0
	/* Remember where the end of the packet was after parsing
	   it. */
	op = sp;
#endif

	/* Sanity check.  Had better not ever happen. */
	if ((ip->circuit_id_len > 255) ||(ip->circuit_id_len < 1))
		log_fatal("Circuit ID length %d out of range [1-255] on "
			  "%s\n", ip->circuit_id_len, ip->name);
	optlen = ip->circuit_id_len + 2;            /* RAI_CIRCUIT_ID + len */

	if (ip->remote_id) {
		if (ip->remote_id_len > 255 || ip->remote_id_len < 1)
			log_fatal("Remote ID length %d out of range [1-255] "
				  "on %s\n", ip->circuit_id_len, ip->name);
		optlen += ip->remote_id_len + 2;    /* RAI_REMOTE_ID + len */
	}

	/* We do not support relay option fragmenting(multiple options to
	 * support an option data exceeding 255 bytes).
	 */
	if ((optlen < 3) ||(optlen > 255))
		log_fatal("Total agent option length(%u) out of range "
			   "[3 - 255] on %s\n", optlen, ip->name);

	/*
	 * Is there room for the option, its code+len, and DHO_END?
	 * If not, forward without adding the option.
	 */
	if (max - sp >= optlen + 3) {
		log_debug("Adding %d-byte relay agent option", optlen + 3);

		/* Okay, cons up *our* Relay Agent Information option. */
		*sp++ = DHO_DHCP_AGENT_OPTIONS;
		*sp++ = optlen;

		/* Copy in the circuit id... */
		*sp++ = RAI_CIRCUIT_ID;
		*sp++ = ip->circuit_id_len;
		memcpy(sp, ip->circuit_id, ip->circuit_id_len);
		sp += ip->circuit_id_len;

		/* Copy in remote ID... */
		if (ip->remote_id) {
			*sp++ = RAI_REMOTE_ID;
			*sp++ = ip->remote_id_len;
			memcpy(sp, ip->remote_id, ip->remote_id_len);
			sp += ip->remote_id_len;
		}
	} else {
		++agent_option_errors;
		log_error("No room in packet (used %d of %d) "
			  "for %d-byte relay agent option: omitted",
			   (int) (sp - ((u_int8_t *) packet)),
			   (int) (max - ((u_int8_t *) packet)),
			   optlen + 3);
	}

	/*
	 * Deposit an END option unless the packet is full (shouldn't
	 * be possible).
	 */
	if (sp < max)
		*sp++ = DHO_END;

	/* Recalculate total packet length. */
	length = sp -((u_int8_t *)packet);

	/* Make sure the packet isn't short(this is unlikely, but WTH) */
	if (length < BOOTP_MIN_LEN) {
		memset(sp, DHO_PAD, BOOTP_MIN_LEN - length);
		return (BOOTP_MIN_LEN);
	}

	return (length);
}

#ifdef DHCPv6
/*
 * Parse a downstream argument: [address%]interface[#index].
 */
static struct stream_list *
parse_downstream(char *arg) {
	struct stream_list *dp, *up;
	struct interface_info *ifp = NULL;
	char *ifname, *addr, *iid;
	isc_result_t status;

	if (!supports_multiple_interfaces(ifp) &&
	    (downstreams != NULL))
		log_fatal("No support for multiple interfaces.");

	/* Decode the argument. */
	ifname = strchr(arg, '%');
	if (ifname == NULL) {
		ifname = arg;
		addr = NULL;
	} else {
		*ifname++ = '\0';
		addr = arg;
	}
	iid = strchr(ifname, '#');
	if (iid != NULL) {
		*iid++ = '\0';
	}
	if (strlen(ifname) >= sizeof(ifp->name)) {
		log_error("Interface name '%s' too long", ifname);
		usage();
	}

	/* Don't declare twice. */
	for (dp = downstreams; dp; dp = dp->next) {
		if (strcmp(ifname, dp->ifp->name) == 0)
			log_fatal("Down interface '%s' declared twice.",
				  ifname);
	}

	/* Share with up side? */
	for (up = upstreams; up; up = up->next) {
		if (strcmp(ifname, up->ifp->name) == 0) {
			log_info("Interface '%s' is both down and up.",
				 ifname);
			ifp = up->ifp;
			break;
		}
	}

	/* New interface. */
	if (ifp == NULL) {
		status = interface_allocate(&ifp, MDL);
		if (status != ISC_R_SUCCESS)
			log_fatal("%s: interface_allocate: %s",
				  arg, isc_result_totext(status));
		strcpy(ifp->name, ifname);
		if (interfaces) {
			interface_reference(&ifp->next, interfaces, MDL);
			interface_dereference(&interfaces, MDL);
		}
		interface_reference(&interfaces, ifp, MDL);
		ifp->flags |= INTERFACE_REQUESTED | INTERFACE_DOWNSTREAM;
	}

	/* New downstream. */
	dp = (struct stream_list *) dmalloc(sizeof(*dp), MDL);
	if (!dp)
		log_fatal("No memory for downstream.");
	dp->ifp = ifp;
	if (iid != NULL) {
		dp->id = atoi(iid);
	} else {
		dp->id = -1;
	}
	/* !addr case handled by setup. */
	if (addr && (inet_pton(AF_INET6, addr, &dp->link.sin6_addr) <= 0))
		log_fatal("Bad link address '%s'", addr);

	return dp;
}

/*
 * Parse an upstream argument: [address]%interface.
 */
static struct stream_list *
parse_upstream(char *arg) {
	struct stream_list *up, *dp;
	struct interface_info *ifp = NULL;
	char *ifname, *addr;
	isc_result_t status;

	/* Decode the argument. */
	ifname = strchr(arg, '%');
	if (ifname == NULL) {
		ifname = arg;
		addr = All_DHCP_Servers;
	} else {
		*ifname++ = '\0';
		addr = arg;
	}
	if (strlen(ifname) >= sizeof(ifp->name)) {
		log_fatal("Interface name '%s' too long", ifname);
	}

	/* Shared up interface? */
	for (up = upstreams; up; up = up->next) {
		if (strcmp(ifname, up->ifp->name) == 0) {
			ifp = up->ifp;
			break;
		}
	}
	for (dp = downstreams; dp; dp = dp->next) {
		if (strcmp(ifname, dp->ifp->name) == 0) {
			ifp = dp->ifp;
			break;
		}
	}

	/* New interface. */
	if (ifp == NULL) {
		status = interface_allocate(&ifp, MDL);
		if (status != ISC_R_SUCCESS)
			log_fatal("%s: interface_allocate: %s",
				  arg, isc_result_totext(status));
		strcpy(ifp->name, ifname);
		if (interfaces) {
			interface_reference(&ifp->next, interfaces, MDL);
			interface_dereference(&interfaces, MDL);
		}
		interface_reference(&interfaces, ifp, MDL);
		ifp->flags |= INTERFACE_REQUESTED | INTERFACE_UPSTREAM;
	}

	/* New upstream. */
	up = (struct stream_list *) dmalloc(sizeof(*up), MDL);
	if (up == NULL)
		log_fatal("No memory for upstream.");

	up->ifp = ifp;

	if (inet_pton(AF_INET6, addr, &up->link.sin6_addr) <= 0)
		log_fatal("Bad address %s", addr);

	return up;
}

/*
 * Setup downstream interfaces.
 */
static void
setup_streams(void) {
	struct stream_list *dp, *up;
	int i;
	isc_boolean_t link_is_set;

	for (dp = downstreams; dp; dp = dp->next) {
		/* Check interface */
		if (dp->ifp->v6address_count == 0)
			log_fatal("Interface '%s' has no IPv6 addresses.",
				  dp->ifp->name);

		/* Check/set link. */
		if (IN6_IS_ADDR_UNSPECIFIED(&dp->link.sin6_addr))
			link_is_set = ISC_FALSE;
		else
			link_is_set = ISC_TRUE;
		for (i = 0; i < dp->ifp->v6address_count; i++) {
			if (IN6_IS_ADDR_LINKLOCAL(&dp->ifp->v6addresses[i]))
				continue;
			if (!link_is_set)
				break;
			if (!memcmp(&dp->ifp->v6addresses[i],
				    &dp->link.sin6_addr,
				    sizeof(dp->link.sin6_addr)))
				break;
		}
		if (i == dp->ifp->v6address_count)
			log_fatal("Interface %s does not have global IPv6 "
				  "address assigned.", dp->ifp->name);
		if (!link_is_set)
			memcpy(&dp->link.sin6_addr,
			       &dp->ifp->v6addresses[i],
			       sizeof(dp->link.sin6_addr));

		/* Set interface-id. */
		if (dp->id == -1)
			dp->id = dp->ifp->index;
	}

	for (up = upstreams; up; up = up->next) {
		up->link.sin6_port = local_port;
		up->link.sin6_family = AF_INET6;
#ifdef HAVE_SA_LEN
		up->link.sin6_len = sizeof(up->link);
#endif

		if (up->ifp->v6address_count == 0)
			log_fatal("Interface '%s' has no IPv6 addresses.",
				  up->ifp->name);
	}
}

/*
 * Add DHCPv6 agent options here.
 */
static const int required_forw_opts[] = {
	D6O_INTERFACE_ID,
	D6O_SUBSCRIBER_ID,
	D6O_RELAY_MSG,
	0
};

/*
 * Process a packet upwards, i.e., from client to server.
 */
static void
process_up6(struct packet *packet, struct stream_list *dp) {
	char forw_data[65535];
	unsigned cursor;
	struct dhcpv6_relay_packet *relay;
	struct option_state *opts;
	struct stream_list *up;

	/* Check if the message should be relayed to the server. */
	switch (packet->dhcpv6_msg_type) {
	      case DHCPV6_SOLICIT:
	      case DHCPV6_REQUEST:
	      case DHCPV6_CONFIRM:
	      case DHCPV6_RENEW:
	      case DHCPV6_REBIND:
	      case DHCPV6_RELEASE:
	      case DHCPV6_DECLINE:
	      case DHCPV6_INFORMATION_REQUEST:
	      case DHCPV6_RELAY_FORW:
	      case DHCPV6_LEASEQUERY:
		log_info("Relaying %s from %s port %d going up.",
			 dhcpv6_type_names[packet->dhcpv6_msg_type],
			 piaddr(packet->client_addr),
			 ntohs(packet->client_port));
		break;

	      case DHCPV6_ADVERTISE:
	      case DHCPV6_REPLY:
	      case DHCPV6_RECONFIGURE:
	      case DHCPV6_RELAY_REPL:
	      case DHCPV6_LEASEQUERY_REPLY:
		log_info("Discarding %s from %s port %d going up.",
			 dhcpv6_type_names[packet->dhcpv6_msg_type],
			 piaddr(packet->client_addr),
			 ntohs(packet->client_port));
		return;

	      default:
		log_info("Unknown %d type from %s port %d going up.",
			 packet->dhcpv6_msg_type,
			 piaddr(packet->client_addr),
			 ntohs(packet->client_port));
		return;
	}

	/* Build the relay-forward header. */
	relay = (struct dhcpv6_relay_packet *) forw_data;
	cursor = offsetof(struct dhcpv6_relay_packet, options);
	relay->msg_type = DHCPV6_RELAY_FORW;
	if (packet->dhcpv6_msg_type == DHCPV6_RELAY_FORW) {
		if (packet->dhcpv6_hop_count >= max_hop_count) {
			log_info("Hop count exceeded,");
			return;
		}
		relay->hop_count = packet->dhcpv6_hop_count + 1;
		if (dp) {
			memcpy(&relay->link_address, &dp->link.sin6_addr, 16);
		} else {
			/* On smart relay add: && !global. */
			if (!use_if_id && downstreams->next) {
				log_info("Shan't get back the interface.");
				return;
			}
			memset(&relay->link_address, 0, 16);
		}
	} else {
		relay->hop_count = 0;
		if (!dp)
			return;
		memcpy(&relay->link_address, &dp->link.sin6_addr, 16);
	}
	memcpy(&relay->peer_address, packet->client_addr.iabuf, 16);

	/* Get an option state. */
	opts = NULL;
	if (!option_state_allocate(&opts, MDL)) {
		log_fatal("No memory for upwards options.");
	}
	
	/* Add an interface-id (if used). */
	if (use_if_id) {
		int if_id;

		if (dp) {
			if_id = dp->id;
		} else if (!downstreams->next) {
			if_id = downstreams->id;
		} else {
			log_info("Don't know the interface.");
			option_state_dereference(&opts, MDL);
			return;
		}

		if (!save_option_buffer(&dhcpv6_universe, opts,
					NULL, (unsigned char *) &if_id,
					sizeof(int),
					D6O_INTERFACE_ID, 0)) {
			log_error("Can't save interface-id.");
			option_state_dereference(&opts, MDL);
			return;
		}
	}

	/* Add a subscriber-id if desired. */
	/* This is for testing rather than general use */
	if (dhcrelay_sub_id != NULL) {
		if (!save_option_buffer(&dhcpv6_universe, opts, NULL,
					(unsigned char *) dhcrelay_sub_id,
					strlen(dhcrelay_sub_id),
					D6O_SUBSCRIBER_ID, 0)) {
			log_error("Can't save subsriber-id.");
			option_state_dereference(&opts, MDL);
			return;
		}
	}
		

	/* Add the relay-msg carrying the packet. */
	if (!save_option_buffer(&dhcpv6_universe, opts,
				NULL, (unsigned char *) packet->raw,
				packet->packet_length,
				D6O_RELAY_MSG, 0)) {
		log_error("Can't save relay-msg.");
		option_state_dereference(&opts, MDL);
		return;
	}

	/* Finish the relay-forward message. */
	cursor += store_options6(forw_data + cursor,
				 sizeof(forw_data) - cursor,
				 opts, packet, 
				 required_forw_opts, NULL);
	option_state_dereference(&opts, MDL);

	/* Send it to all upstreams. */
	for (up = upstreams; up; up = up->next) {
		send_packet6(up->ifp, (unsigned char *) forw_data,
			     (size_t) cursor, &up->link);
	}
}
			     
/*
 * Process a packet downwards, i.e., from server to client.
 */
static void
process_down6(struct packet *packet) {
	struct stream_list *dp;
	struct option_cache *oc;
	struct data_string relay_msg;
	const struct dhcpv6_packet *msg;
	struct data_string if_id;
	struct sockaddr_in6 to;
	struct iaddr peer;

	/* The packet must be a relay-reply message. */
	if (packet->dhcpv6_msg_type != DHCPV6_RELAY_REPL) {
		if (packet->dhcpv6_msg_type < dhcpv6_type_name_max)
			log_info("Discarding %s from %s port %d going down.",
				 dhcpv6_type_names[packet->dhcpv6_msg_type],
				 piaddr(packet->client_addr),
				 ntohs(packet->client_port));
		else
			log_info("Unknown %d type from %s port %d going down.",
				 packet->dhcpv6_msg_type,
				 piaddr(packet->client_addr),
				 ntohs(packet->client_port));
		return;
	}

	/* Inits. */
	memset(&relay_msg, 0, sizeof(relay_msg));
	memset(&if_id, 0, sizeof(if_id));
	memset(&to, 0, sizeof(to));
	to.sin6_family = AF_INET6;
#ifdef HAVE_SA_LEN
	to.sin6_len = sizeof(to);
#endif
	to.sin6_port = remote_port;
	peer.len = 16;

	/* Get the relay-msg option (carrying the message to relay). */
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_RELAY_MSG);
	if (oc == NULL) {
		log_info("No relay-msg.");
		return;
	}
	if (!evaluate_option_cache(&relay_msg, packet, NULL, NULL,
				   packet->options, NULL,
				   &global_scope, oc, MDL) ||
	    (relay_msg.len < offsetof(struct dhcpv6_packet, options))) {
		log_error("Can't evaluate relay-msg.");
		return;
	}
	msg = (const struct dhcpv6_packet *) relay_msg.data;

	/* Get the interface-id (if exists) and the downstream. */
	oc = lookup_option(&dhcpv6_universe, packet->options,
			   D6O_INTERFACE_ID);
	if (oc != NULL) {
		int if_index;

		if (!evaluate_option_cache(&if_id, packet, NULL, NULL,
					   packet->options, NULL,
					   &global_scope, oc, MDL) ||
		    (if_id.len != sizeof(int))) {
			log_info("Can't evaluate interface-id.");
			goto cleanup;
		}
		memcpy(&if_index, if_id.data, sizeof(int));
		for (dp = downstreams; dp; dp = dp->next) {
			if (dp->id == if_index)
				break;
		}
	} else {
		if (use_if_id) {
			/* Require an interface-id. */
			log_info("No interface-id.");
			goto cleanup;
		}
		for (dp = downstreams; dp; dp = dp->next) {
			/* Get the first matching one. */
			if (!memcmp(&dp->link.sin6_addr,
				    &packet->dhcpv6_link_address,
				    sizeof(struct in6_addr)))
				break;
		}
	}
	/* Why bother when there is no choice. */
	if (!dp && downstreams && !downstreams->next)
		dp = downstreams;
	if (!dp) {
		log_info("Can't find the down interface.");
		goto cleanup;
	}
	memcpy(peer.iabuf, &packet->dhcpv6_peer_address, peer.len);
	to.sin6_addr = packet->dhcpv6_peer_address;

	/* Check if we should relay the carried message. */
	switch (msg->msg_type) {
		/* Relay-Reply of for another relay, not a client. */
	      case DHCPV6_RELAY_REPL:
		to.sin6_port = local_port;
		/* Fall into: */

	      case DHCPV6_ADVERTISE:
	      case DHCPV6_REPLY:
	      case DHCPV6_RECONFIGURE:
	      case DHCPV6_RELAY_FORW:
	      case DHCPV6_LEASEQUERY_REPLY:
		log_info("Relaying %s to %s port %d down.",
			 dhcpv6_type_names[msg->msg_type],
			 piaddr(peer),
			 ntohs(to.sin6_port));
		break;

	      case DHCPV6_SOLICIT:
	      case DHCPV6_REQUEST:
	      case DHCPV6_CONFIRM:
	      case DHCPV6_RENEW:
	      case DHCPV6_REBIND:
	      case DHCPV6_RELEASE:
	      case DHCPV6_DECLINE:
	      case DHCPV6_INFORMATION_REQUEST:
	      case DHCPV6_LEASEQUERY:
		log_info("Discarding %s to %s port %d down.",
			 dhcpv6_type_names[msg->msg_type],
			 piaddr(peer),
			 ntohs(to.sin6_port));
		goto cleanup;

	      default:
		log_info("Unknown %d type to %s port %d down.",
			 msg->msg_type,
			 piaddr(peer),
			 ntohs(to.sin6_port));
		goto cleanup;
	}

	/* Send the message to the downstream. */
	send_packet6(dp->ifp, (unsigned char *) relay_msg.data,
		     (size_t) relay_msg.len, &to);

      cleanup:
	if (relay_msg.data != NULL)
		data_string_forget(&relay_msg, MDL);
	if (if_id.data != NULL)
		data_string_forget(&if_id, MDL);
}

/*
 * Called by the dispatch packet handler with a decoded packet.
 */
void
dhcpv6(struct packet *packet) {
	struct stream_list *dp;

	/* Try all relay-replies downwards. */
	if (packet->dhcpv6_msg_type == DHCPV6_RELAY_REPL) {
		process_down6(packet);
		return;
	}
	/* Others are candidates to go up if they come from down. */
	for (dp = downstreams; dp; dp = dp->next) {
		if (packet->interface != dp->ifp)
			continue;
		process_up6(packet, dp);
		return;
	}
	/* Relay-forward could work from an unknown interface. */
	if (packet->dhcpv6_msg_type == DHCPV6_RELAY_FORW) {
		process_up6(packet, NULL);
		return;
	}

	log_info("Can't process packet from interface '%s'.",
		 packet->interface->name);
}
#endif

/* Stub routines needed for linking with DHCP libraries. */
void
bootp(struct packet *packet) {
	return;
}

void
dhcp(struct packet *packet) {
	return;
}

void
classify(struct packet *p, struct class *c) {
	return;
}

int
check_collection(struct packet *p, struct lease *l, struct collection *c) {
	return 0;
}

isc_result_t
find_class(struct class **class, const char *c1, const char *c2, int i) {
	return ISC_R_NOTFOUND;
}

int
parse_allow_deny(struct option_cache **oc, struct parse *p, int i) {
	return 0;
}

isc_result_t
dhcp_set_control_state(control_object_state_t oldstate,
		       control_object_state_t newstate) {
	if (newstate != server_shutdown)
		return ISC_R_SUCCESS;
	exit(0);
}
