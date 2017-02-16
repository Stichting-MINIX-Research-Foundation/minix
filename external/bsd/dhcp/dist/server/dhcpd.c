/*	$NetBSD: dhcpd.c,v 1.4 2014/07/12 12:09:38 spz Exp $	*/
/* dhcpd.c

   DHCP Server Daemon. */

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

#include <sys/cdefs.h>
__RCSID("$NetBSD: dhcpd.c,v 1.4 2014/07/12 12:09:38 spz Exp $");

static const char copyright[] =
"Copyright 2004-2014 Internet Systems Consortium.";
static const char arr [] = "All rights reserved.";
static const char message [] = "Internet Systems Consortium DHCP Server";
static const char url [] =
"For info, please visit https://www.isc.org/software/dhcp/";

#include "dhcpd.h"
#include <omapip/omapip_p.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>

#if defined (PARANOIA)
#  include <sys/types.h>
#  include <unistd.h>
#  include <pwd.h>
/* get around the ISC declaration of group */
#  define group real_group 
#    include <grp.h>
#  undef group
#endif /* PARANOIA */

#ifndef UNIT_TEST
static void usage(void);
#endif

struct iaddr server_identifier;
int server_identifier_matched;

#if defined (NSUPDATE)

/* This stuff is always executed to figure the default values for certain
   ddns variables. */

char std_nsupdate [] = "						    \n\
option server.ddns-hostname =						    \n\
  pick (option fqdn.hostname, option host-name);			    \n\
option server.ddns-domainname =	config-option domain-name;		    \n\
option server.ddns-rev-domainname = \"in-addr.arpa.\";";

#endif /* NSUPDATE */
int ddns_update_style;
int dont_use_fsync = 0; /* 0 = default, use fsync, 1 = don't use fsync */

const char *path_dhcpd_conf = _PATH_DHCPD_CONF;
const char *path_dhcpd_db = _PATH_DHCPD_DB;
const char *path_dhcpd_pid = _PATH_DHCPD_PID;
/* False (default) => we write and use a pid file */
isc_boolean_t no_pid_file = ISC_FALSE;

int dhcp_max_agent_option_packet_length = DHCP_MTU_MAX;

static omapi_auth_key_t *omapi_key = (omapi_auth_key_t *)0;
int omapi_port;

#if defined (TRACING)
trace_type_t *trace_srandom;
#endif

static isc_result_t verify_addr (omapi_object_t *l, omapi_addr_t *addr) {
	return ISC_R_SUCCESS;
}

static isc_result_t verify_auth (omapi_object_t *p, omapi_auth_key_t *a) {
	if (a != omapi_key)
		return DHCP_R_INVALIDKEY;
	return ISC_R_SUCCESS;
}

static void omapi_listener_start (void *foo)
{
	omapi_object_t *listener;
	isc_result_t result;
	struct timeval tv;

	listener = (omapi_object_t *)0;
	result = omapi_generic_new (&listener, MDL);
	if (result != ISC_R_SUCCESS)
		log_fatal ("Can't allocate new generic object: %s",
			   isc_result_totext (result));
	result = omapi_protocol_listen (listener,
					(unsigned)omapi_port, 1);
	if (result == ISC_R_SUCCESS && omapi_key)
		result = omapi_protocol_configure_security
			(listener, verify_addr, verify_auth);
	if (result != ISC_R_SUCCESS) {
		log_error ("Can't start OMAPI protocol: %s",
			   isc_result_totext (result));
		tv.tv_sec = cur_tv.tv_sec + 5;
		tv.tv_usec = cur_tv.tv_usec;
		add_timeout (&tv, omapi_listener_start, 0, 0, 0);
	}
	omapi_object_dereference (&listener, MDL);
}

#if defined (PARANOIA)
/* to be used in one of two possible scenarios */
static void setup_chroot (char *chroot_dir) {
  if (geteuid())
    log_fatal ("you must be root to use chroot");

  if (chroot(chroot_dir)) {
    log_fatal ("chroot(\"%s\"): %m", chroot_dir);
  }
  if (chdir ("/")) {
    /* probably permission denied */
    log_fatal ("chdir(\"/\"): %m");
  }
}
#endif /* PARANOIA */

#ifndef UNIT_TEST
int 
main(int argc, char **argv) {
	int fd;
	int i, status;
	struct servent *ent;
	char *s;
	int cftest = 0;
	int lftest = 0;
#ifndef DEBUG
	int pid;
	char pbuf [20];
	int daemon = 1;
#endif
	int quiet = 0;
	char *server = (char *)0;
	isc_result_t result;
	unsigned seed;
	struct interface_info *ip;
#if defined (NSUPDATE)
	struct parse *parse;
	int lose;
#endif
	int no_dhcpd_conf = 0;
	int no_dhcpd_db = 0;
	int no_dhcpd_pid = 0;
#ifdef DHCPv6
	int local_family_set = 0;
#endif /* DHCPv6 */
#if defined (TRACING)
	char *traceinfile = (char *)0;
	char *traceoutfile = (char *)0;
#endif

#if defined (PARANOIA)
	char *set_user   = 0;
	char *set_group  = 0;
	char *set_chroot = 0;

	uid_t set_uid = 0;
	gid_t set_gid = 0;
#endif /* PARANOIA */

        /* Make sure that file descriptors 0 (stdin), 1, (stdout), and
           2 (stderr) are open. To do this, we assume that when we
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

	/* Initialize the omapi system. */
	result = omapi_init ();
	if (result != ISC_R_SUCCESS)
		log_fatal ("Can't initialize OMAPI: %s",
			   isc_result_totext (result));

	/* Set up the OMAPI wrappers for common objects. */
	dhcp_db_objects_setup ();
	/* Set up the OMAPI wrappers for various server database internal
	   objects. */
	dhcp_common_objects_setup ();

	/* Initially, log errors to stderr as well as to syslogd. */
	openlog ("dhcpd", LOG_NDELAY, DHCPD_LOG_FACILITY);

	for (i = 1; i < argc; i++) {
		if (!strcmp (argv [i], "-p")) {
			if (++i == argc)
				usage ();
			local_port = validate_port (argv [i]);
			log_debug ("binding to user-specified port %d",
			       ntohs (local_port));
		} else if (!strcmp (argv [i], "-f")) {
#ifndef DEBUG
			daemon = 0;
#endif
		} else if (!strcmp (argv [i], "-d")) {
#ifndef DEBUG
			daemon = 0;
#endif
			log_perror = -1;
		} else if (!strcmp (argv [i], "-s")) {
			if (++i == argc)
				usage ();
			server = argv [i];
#if defined (PARANOIA)
		} else if (!strcmp (argv [i], "-user")) {
			if (++i == argc)
				usage ();
			set_user = argv [i];
		} else if (!strcmp (argv [i], "-group")) {
			if (++i == argc)
				usage ();
			set_group = argv [i];
		} else if (!strcmp (argv [i], "-chroot")) {
			if (++i == argc)
				usage ();
			set_chroot = argv [i];
#endif /* PARANOIA */
		} else if (!strcmp (argv [i], "-cf")) {
			if (++i == argc)
				usage ();
			path_dhcpd_conf = argv [i];
			no_dhcpd_conf = 1;
		} else if (!strcmp (argv [i], "-lf")) {
			if (++i == argc)
				usage ();
			path_dhcpd_db = argv [i];
			no_dhcpd_db = 1;
		} else if (!strcmp (argv [i], "-pf")) {
			if (++i == argc)
				usage ();
			path_dhcpd_pid = argv [i];
			no_dhcpd_pid = 1;
		} else if (!strcmp(argv[i], "--no-pid")) {
			no_pid_file = ISC_TRUE;
                } else if (!strcmp (argv [i], "-t")) {
			/* test configurations only */
#ifndef DEBUG
			daemon = 0;
#endif
			cftest = 1;
			log_perror = -1;
                } else if (!strcmp (argv [i], "-T")) {
			/* test configurations and lease file only */
#ifndef DEBUG
			daemon = 0;
#endif
			cftest = 1;
			lftest = 1;
			log_perror = -1;
		} else if (!strcmp (argv [i], "-q")) {
			quiet = 1;
			quiet_interface_discovery = 1;
#ifdef DHCPv6
		} else if (!strcmp(argv[i], "-4")) {
			if (local_family_set && (local_family != AF_INET)) {
				log_fatal("Server cannot run in both IPv4 and "
					  "IPv6 mode at the same time.");
			}
			local_family = AF_INET;
			local_family_set = 1;
		} else if (!strcmp(argv[i], "-6")) {
			if (local_family_set && (local_family != AF_INET6)) {
				log_fatal("Server cannot run in both IPv4 and "
					  "IPv6 mode at the same time.");
			}
			local_family = AF_INET6;
			local_family_set = 1;
#endif /* DHCPv6 */
		} else if (!strcmp (argv [i], "--version")) {
			log_info("isc-dhcpd-%s", PACKAGE_VERSION);
			exit (0);
#if defined (TRACING)
		} else if (!strcmp (argv [i], "-tf")) {
			if (++i == argc)
				usage ();
			traceoutfile = argv [i];
		} else if (!strcmp (argv [i], "-play")) {
			if (++i == argc)
				usage ();
			traceinfile = argv [i];
			trace_replay_init ();
#endif /* TRACING */
		} else if (argv [i][0] == '-') {
			usage ();
		} else {
			struct interface_info *tmp =
				(struct interface_info *)0;
			if (strlen(argv[i]) >= sizeof(tmp->name))
				log_fatal("%s: interface name too long "
					  "(is %ld)",
					  argv[i], (long)strlen(argv[i]));
			result = interface_allocate (&tmp, MDL);
			if (result != ISC_R_SUCCESS)
				log_fatal ("Insufficient memory to %s %s: %s",
					   "record interface", argv [i],
					   isc_result_totext (result));
			strcpy (tmp -> name, argv [i]);
			if (interfaces) {
				interface_reference (&tmp -> next,
						     interfaces, MDL);
				interface_dereference (&interfaces, MDL);
			}
			interface_reference (&interfaces, tmp, MDL);
			tmp -> flags = INTERFACE_REQUESTED;
		}
	}

	if (!no_dhcpd_conf && (s = getenv ("PATH_DHCPD_CONF"))) {
		path_dhcpd_conf = s;
	}

#ifdef DHCPv6
        if (local_family == AF_INET6) {
                /* DHCPv6: override DHCPv4 lease and pid filenames */
	        if (!no_dhcpd_db) {
                        if ((s = getenv ("PATH_DHCPD6_DB")))
		                path_dhcpd_db = s;
                        else
		                path_dhcpd_db = _PATH_DHCPD6_DB;
	        }
	        if (!no_dhcpd_pid) {
                        if ((s = getenv ("PATH_DHCPD6_PID")))
		                path_dhcpd_pid = s;
                        else
		                path_dhcpd_pid = _PATH_DHCPD6_PID;
	        }
        } else
#else /* !DHCPv6 */
        {
	        if (!no_dhcpd_db && (s = getenv ("PATH_DHCPD_DB"))) {
		        path_dhcpd_db = s;
	        }
	        if (!no_dhcpd_pid && (s = getenv ("PATH_DHCPD_PID"))) {
		        path_dhcpd_pid = s;
	        }
        }
#endif /* DHCPv6 */

        /*
         * convert relative path names to absolute, for files that need
         * to be reopened after chdir() has been called
         */
        if (path_dhcpd_db[0] != '/') {
		const char *path = path_dhcpd_db;
                path_dhcpd_db = realpath(path_dhcpd_db, NULL);
                if (path_dhcpd_db == NULL)
                        log_fatal("Failed to get realpath for %s: %s", path, 
                                   strerror(errno));
        }

	if (!quiet) {
		log_info("%s %s", message, PACKAGE_VERSION);
		log_info (copyright);
		log_info (arr);
		log_info (url);
	} else {
		quiet = 0;
		log_perror = 0;
	}

#ifndef DEBUG
	/*
	 * We need to fork before we call the context create
	 * call that creates the worker threads!
	 */
	if (daemon) {
		/* First part of becoming a daemon... */
		if ((pid = fork ()) < 0)
			log_fatal ("Can't fork daemon: %m");
		else if (pid)
			exit (0);
	}
#endif

	/* Set up the isc and dns library managers */
	status = dhcp_context_create(DHCP_CONTEXT_PRE_DB, NULL, NULL);
	if (status != ISC_R_SUCCESS)
		log_fatal("Can't initialize context: %s",
		          isc_result_totext(status));

	/* Set up the client classification system. */
	classification_setup ();
 
#if defined (TRACING)
	trace_init (set_time, MDL);
	if (traceoutfile) {
		result = trace_begin (traceoutfile, MDL);
		if (result != ISC_R_SUCCESS)
			log_fatal ("Unable to begin trace: %s",
				isc_result_totext (result));
	}
	interface_trace_setup ();
	parse_trace_setup ();
	trace_srandom = trace_type_register ("random-seed", (void *)0,
					     trace_seed_input,
					     trace_seed_stop, MDL);
	trace_ddns_init();
#endif

#if defined (PARANOIA)
	/* get user and group info if those options were given */
	if (set_user) {
		struct passwd *tmp_pwd;

		if (geteuid())
			log_fatal ("you must be root to set user");

		if (!(tmp_pwd = getpwnam(set_user)))
			log_fatal ("no such user: %s", set_user);

		set_uid = tmp_pwd->pw_uid;

		/* use the user's group as the default gid */
		if (!set_group)
			set_gid = tmp_pwd->pw_gid;
	}

	if (set_group) {
/* get around the ISC declaration of group */
#define group real_group
		struct group *tmp_grp;

		if (geteuid())
			log_fatal ("you must be root to set group");

		if (!(tmp_grp = getgrnam(set_group)))
			log_fatal ("no such group: %s", set_group);

		set_gid = tmp_grp->gr_gid;
#undef group
	}

#  if defined (EARLY_CHROOT)
	if (set_chroot) setup_chroot (set_chroot);
#  endif /* EARLY_CHROOT */
#endif /* PARANOIA */

	/* Default to the DHCP/BOOTP port. */
	if (!local_port)
	{
		if ((s = getenv ("DHCPD_PORT"))) {
			local_port = validate_port (s);
			log_debug ("binding to environment-specified port %d",
				   ntohs (local_port));
		} else {
			if (local_family == AF_INET) {
				ent = getservbyname("dhcp", "udp");
				if (ent == NULL) {
					local_port = htons(67);
				} else {
					local_port = ent->s_port;
				}
			} else {
				/* INSIST(local_family == AF_INET6); */
				ent = getservbyname("dhcpv6-server", "udp");
				if (ent == NULL) {
					local_port = htons(547);
				} else {
					local_port = ent->s_port;
				}
			}
#ifndef __CYGWIN32__ /* XXX */
			endservent ();
#endif
		}
	}
  
  	if (local_family == AF_INET) {
		remote_port = htons(ntohs(local_port) + 1);
	} else {
		/* INSIST(local_family == AF_INET6); */
		ent = getservbyname("dhcpv6-client", "udp");
		if (ent == NULL) {
			remote_port = htons(546);
		} else {
			remote_port = ent->s_port;
		}
	}

	if (server) {
		if (local_family != AF_INET) {
			log_fatal("You can only specify address to send "
			          "replies to when running an IPv4 server.");
		}
		if (!inet_aton (server, &limited_broadcast)) {
			struct hostent *he;
			he = gethostbyname (server);
			if (he) {
				memcpy (&limited_broadcast,
					he -> h_addr_list [0],
					sizeof limited_broadcast);
			} else
				limited_broadcast.s_addr = INADDR_BROADCAST;
		}
	} else {
		limited_broadcast.s_addr = INADDR_BROADCAST;
	}

	/* Get the current time... */
	gettimeofday(&cur_tv, NULL);

	/* Set up the initial dhcp option universe. */
	initialize_common_option_spaces ();
	initialize_server_option_spaces ();

	/* Add the ddns update style enumeration prior to parsing. */
	add_enumeration (&ddns_styles);
	add_enumeration (&syslog_enum);
#if defined (LDAP_CONFIGURATION)
	add_enumeration (&ldap_methods);
#if defined (LDAP_USE_SSL)
	add_enumeration (&ldap_ssl_usage_enum);
	add_enumeration (&ldap_tls_reqcert_enum);
	add_enumeration (&ldap_tls_crlcheck_enum);
#endif
#endif

	if (!group_allocate (&root_group, MDL))
		log_fatal ("Can't allocate root group!");
	root_group -> authoritative = 0;

	/* Set up various hooks. */
	dhcp_interface_setup_hook = dhcpd_interface_setup_hook;
	bootp_packet_handler = do_packet;
#ifdef DHCPv6
	dhcpv6_packet_handler = do_packet6;
#endif /* DHCPv6 */

#if defined (NSUPDATE)
	/* Set up the standard name service updater routine. */
	parse = NULL;
	status = new_parse(&parse, -1, std_nsupdate, sizeof(std_nsupdate) - 1,
			    "standard name service update routine", 0);
	if (status != ISC_R_SUCCESS)
		log_fatal ("can't begin parsing name service updater!");

	if (parse != NULL) {
		lose = 0;
		if (!(parse_executable_statements(&root_group->statements,
						  parse, &lose, context_any))) {
			end_parse(&parse);
			log_fatal("can't parse standard name service updater!");
		}
		end_parse(&parse);
	}
#endif

	/* Initialize icmp support... */
	if (!cftest && !lftest)
		icmp_startup (1, lease_pinged);

#if defined (TRACING)
	if (traceinfile) {
	    if (!no_dhcpd_db) {
		    log_error ("%s", "");
		    log_error ("** You must specify a lease file with -lf.");
		    log_error ("   Dhcpd will not overwrite your default");
		    log_fatal ("   lease file when playing back a trace. **");
	    }		
	    trace_file_replay (traceinfile);

#if defined (DEBUG_MEMORY_LEAKAGE) && \
                defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
            free_everything ();
            omapi_print_dmalloc_usage_by_caller (); 
#endif

	    exit (0);
	}
#endif

#ifdef DHCPv6
	/* set up DHCPv6 hashes */
	if (!ia_new_hash(&ia_na_active, DEFAULT_HASH_SIZE, MDL)) {
		log_fatal("Out of memory creating hash for active IA_NA.");
	}
	if (!ia_new_hash(&ia_ta_active, DEFAULT_HASH_SIZE, MDL)) {
		log_fatal("Out of memory creating hash for active IA_TA.");
	}
	if (!ia_new_hash(&ia_pd_active, DEFAULT_HASH_SIZE, MDL)) {
		log_fatal("Out of memory creating hash for active IA_PD.");
	}
#endif /* DHCPv6 */

	/* Read the dhcpd.conf file... */
	if (readconf () != ISC_R_SUCCESS)
		log_fatal ("Configuration file errors encountered -- exiting");

	postconf_initialization (quiet);
 
#if defined (PARANOIA) && !defined (EARLY_CHROOT)
	if (set_chroot) setup_chroot (set_chroot);
#endif /* PARANOIA && !EARLY_CHROOT */

        /* test option should cause an early exit */
 	if (cftest && !lftest) 
 		exit(0);

	group_write_hook = group_writer;

	/* Start up the database... */
	db_startup (lftest);

	if (lftest)
		exit (0);

	/* Discover all the network interfaces and initialize them. */
	discover_interfaces(DISCOVER_SERVER);

#ifdef DHCPv6
	/*
	 * Remove addresses from our pools that we should not issue
	 * to clients.
	 *
	 * We currently have no support for this in IPv4. It is not 
	 * as important in IPv4, as making pools with ranges that 
	 * leave out interfaces and hosts is fairly straightforward
	 * using range notation, but not so handy with CIDR notation.
	 */
	if (local_family == AF_INET6) {
		mark_hosts_unavailable();
		mark_phosts_unavailable();
		mark_interfaces_unavailable();
	}
#endif /* DHCPv6 */


	/* Make up a seed for the random number generator from current
	   time plus the sum of the last four bytes of each
	   interface's hardware address interpreted as an integer.
	   Not much entropy, but we're booting, so we're not likely to
	   find anything better. */
	seed = 0;
	for (ip = interfaces; ip; ip = ip -> next) {
		int junk;
		memcpy (&junk,
			&ip -> hw_address.hbuf [ip -> hw_address.hlen -
					       sizeof seed], sizeof seed);
		seed += junk;
	}
	srandom (seed + cur_time);
#if defined (TRACING)
	trace_seed_stash (trace_srandom, seed + cur_time);
#endif
	postdb_startup ();

#ifdef DHCPv6
	/*
	 * Set server DHCPv6 identifier.
	 * See dhcpv6.c for discussion of setting DUID.
	 */
	if (set_server_duid_from_option() == ISC_R_SUCCESS) {
		write_server_duid();
	} else {
		if (!server_duid_isset()) {
			if (generate_new_server_duid() != ISC_R_SUCCESS) {
				log_fatal("Unable to set server identifier.");
			}
			write_server_duid();
		}
	}
#endif /* DHCPv6 */

#ifndef DEBUG
 
#if defined (PARANOIA)
	/* change uid to the specified one */

	if (set_gid) {
		if (setgroups (0, (void *)0))
			log_fatal ("setgroups: %m");
		if (setgid (set_gid))
			log_fatal ("setgid(%d): %m", (int) set_gid);
	}	

	if (set_uid) {
		if (setuid (set_uid))
			log_fatal ("setuid(%d): %m", (int) set_uid);
	}
#endif /* PARANOIA */

	/*
	 * Deal with pid files.  If the user told us
	 * not to write a file we don't read one either
	 */
	if (no_pid_file == ISC_FALSE) {
		/*Read previous pid file. */
		if ((i = open (path_dhcpd_pid, O_RDONLY)) >= 0) {
			status = read(i, pbuf, (sizeof pbuf) - 1);
			close (i);
			if (status > 0) {
				pbuf[status] = 0;
				pid = atoi(pbuf);

				/*
				 * If there was a previous server process and
				 * it is still running, abort
				 */
				if (!pid ||
				    (pid != getpid() && kill(pid, 0) == 0))
					log_fatal("There's already a "
						  "DHCP server running.");
			}
		}

		/* Write new pid file. */
		i = open(path_dhcpd_pid, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if (i >= 0) {
			sprintf(pbuf, "%d\n", (int) getpid());
			IGNORE_RET (write(i, pbuf, strlen(pbuf)));
			close(i);
		} else {
			log_error("Can't create PID file %s: %m.",
				  path_dhcpd_pid);
		}
	}

	/* If we were requested to log to stdout on the command line,
	   keep doing so; otherwise, stop. */
	if (log_perror == -1)
		log_perror = 1;
	else
		log_perror = 0;

	if (daemon) {
		/* Become session leader and get pid... */
		(void) setsid();

                /* Close standard I/O descriptors. */
                (void) close(0);
                (void) close(1);
                (void) close(2);

                /* Reopen them on /dev/null. */
                (void) open("/dev/null", O_RDWR);
                (void) open("/dev/null", O_RDWR);
                (void) open("/dev/null", O_RDWR);
                log_perror = 0; /* No sense logging to /dev/null. */

       		IGNORE_RET (chdir("/"));
	}
#endif /* !DEBUG */

#if defined (DEBUG_MEMORY_LEAKAGE) || defined (DEBUG_MALLOC_POOL) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	dmalloc_cutoff_generation = dmalloc_generation;
	dmalloc_longterm = dmalloc_outstanding;
	dmalloc_outstanding = 0;
#endif

	omapi_set_int_value ((omapi_object_t *)dhcp_control_object,
			     (omapi_object_t *)0, "state", server_running);

        /* install signal handlers */
	signal(SIGINT, dhcp_signal_handler);   /* control-c */
	signal(SIGTERM, dhcp_signal_handler);  /* kill */

	/* Log that we are about to start working */
	log_info("Server starting service.");

	/*
	 * Receive packets and dispatch them...
	 * dispatch() will never return.
	 */
	dispatch ();

	/* Let's return status code */
	return 0;
}
#endif /* !UNIT_TEST */

void postconf_initialization (int quiet)
{
	struct option_state *options = NULL;
	struct data_string db;
	struct option_cache *oc;
	char *s;
	isc_result_t result;
	int tmp;
#if defined (NSUPDATE)
	struct in_addr  local4, *local4_ptr = NULL;
	struct in6_addr local6, *local6_ptr = NULL;
#endif

	/* Now try to get the lease file name. */
	option_state_allocate(&options, MDL);

	execute_statements_in_scope(NULL, NULL, NULL, NULL, NULL,
				    options, &global_scope, root_group,
				    NULL, NULL);
	memset(&db, 0, sizeof db);
	oc = lookup_option(&server_universe, options, SV_LEASE_FILE_NAME);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		s = dmalloc(db.len + 1, MDL);
		if (!s)
			log_fatal("no memory for lease db filename.");
		memcpy(s, db.data, db.len);
		s[db.len] = 0;
		data_string_forget(&db, MDL);
		path_dhcpd_db = s;
	}

	oc = lookup_option(&server_universe, options, SV_PID_FILE_NAME);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		s = dmalloc(db.len + 1, MDL);
		if (!s)
			log_fatal("no memory for pid filename.");
		memcpy(s, db.data, db.len);
		s[db.len] = 0;
		data_string_forget(&db, MDL);
		path_dhcpd_pid = s;
	}

#ifdef DHCPv6
        if (local_family == AF_INET6) {
                /*
                 * Override lease file name with dhcpv6 lease file name,
                 * if it was set; then, do the same with the pid file name
                 */
                oc = lookup_option(&server_universe, options,
                                   SV_DHCPV6_LEASE_FILE_NAME);
                if (oc &&
                    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
					  &global_scope, oc, MDL)) {
                        s = dmalloc(db.len + 1, MDL);
                        if (!s)
                                log_fatal("no memory for lease db filename.");
                        memcpy(s, db.data, db.len);
                        s[db.len] = 0;
                        data_string_forget(&db, MDL);
                        path_dhcpd_db = s;
                }

                oc = lookup_option(&server_universe, options,
                                   SV_DHCPV6_PID_FILE_NAME);
                if (oc &&
                    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
					  &global_scope, oc, MDL)) {
                        s = dmalloc(db.len + 1, MDL);
                        if (!s)
                                log_fatal("no memory for pid filename.");
                        memcpy(s, db.data, db.len);
                        s[db.len] = 0;
                        data_string_forget(&db, MDL);
                        path_dhcpd_pid = s;
                }
        }
#endif /* DHCPv6 */

	omapi_port = -1;
	oc = lookup_option(&server_universe, options, SV_OMAPI_PORT);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		if (db.len == 2) {
			omapi_port = getUShort(db.data);
		} else
			log_fatal("invalid omapi port data length");
		data_string_forget(&db, MDL);
	}

	oc = lookup_option(&server_universe, options, SV_OMAPI_KEY);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		s = dmalloc(db.len + 1, MDL);
		if (!s)
			log_fatal("no memory for OMAPI key filename.");
		memcpy(s, db.data, db.len);
		s[db.len] = 0;
		data_string_forget(&db, MDL);
		result = omapi_auth_key_lookup_name(&omapi_key, s);
		dfree(s, MDL);
		if (result != ISC_R_SUCCESS)
			log_fatal("OMAPI key %s: %s",
				  s, isc_result_totext (result));
	}

	oc = lookup_option(&server_universe, options, SV_LOCAL_PORT);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		if (db.len == 2) {
			local_port = htons(getUShort (db.data));
		} else
			log_fatal("invalid local port data length");
		data_string_forget(&db, MDL);
	}

	oc = lookup_option(&server_universe, options, SV_REMOTE_PORT);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		if (db.len == 2) {
			remote_port = htons(getUShort (db.data));
		} else
			log_fatal("invalid remote port data length");
		data_string_forget(&db, MDL);
	}

	oc = lookup_option(&server_universe, options,
			   SV_LIMITED_BROADCAST_ADDRESS);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		if (db.len == 4) {
			memcpy(&limited_broadcast, db.data, 4);
		} else
			log_fatal("invalid broadcast address data length");
		data_string_forget(&db, MDL);
	}

	oc = lookup_option(&server_universe, options, SV_LOCAL_ADDRESS);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		if (db.len == 4) {
			memcpy(&local_address, db.data, 4);
		} else
			log_fatal("invalid local address data length");
		data_string_forget(&db, MDL);
	}

	oc = lookup_option(&server_universe, options, SV_DDNS_UPDATE_STYLE);
	if (oc) {
		if (evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
					  &global_scope, oc, MDL)) {
			if (db.len == 1) {
				ddns_update_style = db.data[0];
			} else
				log_fatal("invalid dns update type");
			data_string_forget(&db, MDL);
		}
	} else {
		ddns_update_style = DDNS_UPDATE_STYLE_NONE;
	}
#if defined (NSUPDATE)
	/* We no longer support ad_hoc, tell the user */
	if (ddns_update_style == DDNS_UPDATE_STYLE_AD_HOC) {
		log_fatal("ddns-update-style ad_hoc no longer supported");
	}

	oc = lookup_option(&server_universe, options, SV_DDNS_LOCAL_ADDRESS4);
	if (oc) {
		if (evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
					  &global_scope, oc, MDL)) {
			if (db.len == 4) {
				memcpy(&local4, db.data, 4);
				local4_ptr = &local4;
			}
			data_string_forget(&db, MDL);
		}
	}

	oc = lookup_option(&server_universe, options, SV_DDNS_LOCAL_ADDRESS6);
	if (oc) {
		if (evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
					  &global_scope, oc, MDL)) {
			if (db.len == 16) {
				memcpy(&local6, db.data, 16);
				local6_ptr = &local6;
			}
			data_string_forget(&db, MDL);
		}
	}

	if (dhcp_context_create(DHCP_CONTEXT_POST_DB, local4_ptr, local6_ptr)
	    != ISC_R_SUCCESS)
		log_fatal("Unable to complete ddns initialization");

#else
	/* If we don't have support for updates compiled in tell the user */
	if (ddns_update_style != DDNS_UPDATE_STYLE_NONE) {
		log_fatal("Support for ddns-update-style not compiled in");
	}
#endif

	oc = lookup_option(&server_universe, options, SV_LOG_FACILITY);
	if (oc) {
		if (evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
					  &global_scope, oc, MDL)) {
			if (db.len == 1) {
				closelog ();
				openlog("dhcpd", LOG_NDELAY, db.data[0]);
				/* Log the startup banner into the new
				   log file. */
				if (!quiet) {
					/* Don't log to stderr twice. */
					tmp = log_perror;
					log_perror = 0;
					log_info("%s %s",
						 message, PACKAGE_VERSION);
					log_info(copyright);
					log_info(arr);
					log_info(url);
					log_perror = tmp;
				}
			} else
				log_fatal("invalid log facility");
			data_string_forget(&db, MDL);
		}
	}
	
	oc = lookup_option(&server_universe, options, SV_DELAYED_ACK);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		if (db.len == 2) {
			max_outstanding_acks = htons(getUShort(db.data));
		} else {
			log_fatal("invalid max delayed ACK count ");
		}
		data_string_forget(&db, MDL);
	}

	oc = lookup_option(&server_universe, options, SV_MAX_ACK_DELAY);
	if (oc &&
	    evaluate_option_cache(&db, NULL, NULL, NULL, options, NULL,
				  &global_scope, oc, MDL)) {
		u_int32_t timeval;

		if (db.len != 4)
			log_fatal("invalid max ack delay configuration");

		timeval = getULong(db.data);
		max_ack_delay_secs  = timeval / 1000000;
		max_ack_delay_usecs = timeval % 1000000;

		data_string_forget(&db, MDL);
	}

	oc = lookup_option(&server_universe, options, SV_DONT_USE_FSYNC);
	if ((oc != NULL) &&
	    evaluate_boolean_option_cache(NULL, NULL, NULL, NULL, options, NULL,
					  &global_scope, oc, MDL)) {
		dont_use_fsync = 1;
		log_error("Not using fsync() to flush lease writes");
	}

	/* Don't need the options anymore. */
	option_state_dereference(&options, MDL);
}

void postdb_startup (void)
{
	/* Initialize the omapi listener state. */
	if (omapi_port != -1) {
		omapi_listener_start (0);
	}

#if defined (FAILOVER_PROTOCOL)
	/* Initialize the failover listener state. */
	dhcp_failover_startup ();
#endif

	/*
	 * Begin our lease timeout background task.
	 */
	schedule_all_ipv6_lease_timeouts();
}

/* Print usage message. */
#ifndef UNIT_TEST
static void
usage(void) {
	log_info("%s %s", message, PACKAGE_VERSION);
	log_info(copyright);
	log_info(arr);

	log_fatal("Usage: dhcpd [-p <UDP port #>] [-f] [-d] [-q] [-t|-T]\n"
#ifdef DHCPv6
		  "             [-4|-6] [-cf config-file] [-lf lease-file]\n"
#else /* !DHCPv6 */
		  "             [-cf config-file] [-lf lease-file]\n"
#endif /* DHCPv6 */
#if defined (PARANOIA)
		   /* meld into the following string */
		  "             [-user user] [-group group] [-chroot dir]\n"
#endif /* PARANOIA */
#if defined (TRACING)
		  "             [-tf trace-output-file]\n"
		  "             [-play trace-input-file]\n"
#endif /* TRACING */
		  "             [-pf pid-file] [--no-pid] [-s server]\n"
		  "             [if0 [...ifN]]");
}
#endif

void lease_pinged (from, packet, length)
	struct iaddr from;
	u_int8_t *packet;
	int length;
{
	struct lease *lp;

	/* Don't try to look up a pinged lease if we aren't trying to
	   ping one - otherwise somebody could easily make us churn by
	   just forging repeated ICMP EchoReply packets for us to look
	   up. */
	if (!outstanding_pings)
		return;

	lp = (struct lease *)0;
	if (!find_lease_by_ip_addr (&lp, from, MDL)) {
		log_debug ("unexpected ICMP Echo Reply from %s",
			   piaddr (from));
		return;
	}

	if (!lp -> state) {
#if defined (FAILOVER_PROTOCOL)
		if (!lp -> pool ||
		    !lp -> pool -> failover_peer)
#endif
			log_debug ("ICMP Echo Reply for %s late or spurious.",
				   piaddr (from));
		goto out;
	}

	if (lp -> ends > cur_time) {
		log_debug ("ICMP Echo reply while lease %s valid.",
			   piaddr (from));
	}

	/* At this point it looks like we pinged a lease and got a
	   response, which shouldn't have happened. */
	data_string_forget (&lp -> state -> parameter_request_list, MDL);
	free_lease_state (lp -> state, MDL);
	lp -> state = (struct lease_state *)0;

	abandon_lease (lp, "pinged before offer");
	cancel_timeout (lease_ping_timeout, lp);
	--outstanding_pings;
      out:
	lease_dereference (&lp, MDL);
}

void lease_ping_timeout (vlp)
	void *vlp;
{
	struct lease *lp = vlp;

#if defined (DEBUG_MEMORY_LEAKAGE)
	unsigned long previous_outstanding = dmalloc_outstanding;
#endif

	--outstanding_pings;
	dhcp_reply (lp);

#if defined (DEBUG_MEMORY_LEAKAGE)
	log_info ("generation %ld: %ld new, %ld outstanding, %ld long-term",
		  dmalloc_generation,
		  dmalloc_outstanding - previous_outstanding,
		  dmalloc_outstanding, dmalloc_longterm);
#endif
#if defined (DEBUG_MEMORY_LEAKAGE)
	dmalloc_dump_outstanding ();
#endif
}

int dhcpd_interface_setup_hook (struct interface_info *ip, struct iaddr *ia)
{
	struct subnet *subnet;
	struct shared_network *share;
	isc_result_t status;

	/* Special case for fallback network - not sure why this is
	   necessary. */
	if (!ia) {
		const char *fnn = "fallback-net";
		status = shared_network_allocate (&ip -> shared_network, MDL);
		if (status != ISC_R_SUCCESS)
			log_fatal ("No memory for shared subnet: %s",
				   isc_result_totext (status));
		ip -> shared_network -> name = dmalloc (strlen (fnn) + 1, MDL);
		strcpy (ip -> shared_network -> name, fnn);
		return 1;
	}

	/* If there's a registered subnet for this address,
	   connect it together... */
	subnet = (struct subnet *)0;
	if (find_subnet (&subnet, *ia, MDL)) {
		/* If this interface has multiple aliases on the same
		   subnet, ignore all but the first we encounter. */
		if (!subnet -> interface) {
			interface_reference (&subnet -> interface, ip, MDL);
			subnet -> interface_address = *ia;
		} else if (subnet -> interface != ip) {
			log_error ("Multiple interfaces match the %s: %s %s", 
				   "same subnet",
				   subnet -> interface -> name, ip -> name);
		}
		share = subnet -> shared_network;
		if (ip -> shared_network &&
		    ip -> shared_network != share) {
			log_fatal ("Interface %s matches multiple shared %s",
				   ip -> name, "networks");
		} else {
			if (!ip -> shared_network)
				shared_network_reference
					(&ip -> shared_network, share, MDL);
		}
		
		if (!share -> interface) {
			interface_reference (&share -> interface, ip, MDL);
		} else if (share -> interface != ip) {
			log_error ("Multiple interfaces match the %s: %s %s", 
				   "same shared network",
				   share -> interface -> name, ip -> name);
		}
		subnet_dereference (&subnet, MDL);
	}
	return 1;
}

static TIME shutdown_time;
static int omapi_connection_count;
enum dhcp_shutdown_state shutdown_state;

isc_result_t dhcp_io_shutdown (omapi_object_t *obj, void *foo)
{
	/* Shut down all listeners. */
	if (shutdown_state == shutdown_listeners &&
	    obj -> type == omapi_type_listener &&
	    obj -> inner &&
	    obj -> inner -> type == omapi_type_protocol_listener) {
		omapi_listener_destroy (obj, MDL);
		return ISC_R_SUCCESS;
	}

	/* Shut down all existing omapi connections. */
	if (obj -> type == omapi_type_connection &&
	    obj -> inner &&
	    obj -> inner -> type == omapi_type_protocol) {
		if (shutdown_state == shutdown_drop_omapi_connections) {
			omapi_disconnect (obj, 1);
		}
		omapi_connection_count++;
		if (shutdown_state == shutdown_omapi_connections) {
			omapi_disconnect (obj, 0);
			return ISC_R_SUCCESS;
		}
	}

	/* Shutdown all DHCP interfaces. */
	if (obj -> type == dhcp_type_interface &&
	    shutdown_state == shutdown_dhcp) {
		dhcp_interface_remove (obj, (omapi_object_t *)0);
		return ISC_R_SUCCESS;
	}
	return ISC_R_SUCCESS;
}

static isc_result_t dhcp_io_shutdown_countdown (void *vlp)
{
#if defined (FAILOVER_PROTOCOL)
	dhcp_failover_state_t *state;
	int failover_connection_count = 0;
#endif
	struct timeval tv;

      oncemore:
	if (shutdown_state == shutdown_listeners ||
	    shutdown_state == shutdown_omapi_connections ||
	    shutdown_state == shutdown_drop_omapi_connections ||
	    shutdown_state == shutdown_dhcp) {
		omapi_connection_count = 0;
		omapi_io_state_foreach (dhcp_io_shutdown, 0);
	}

	if ((shutdown_state == shutdown_listeners ||
	     shutdown_state == shutdown_omapi_connections ||
	     shutdown_state == shutdown_drop_omapi_connections) &&
	    omapi_connection_count == 0) {
		shutdown_state = shutdown_dhcp;
		shutdown_time = cur_time;
		goto oncemore;
	} else if (shutdown_state == shutdown_listeners &&
		   cur_time - shutdown_time > 4) {
		shutdown_state = shutdown_omapi_connections;
		shutdown_time = cur_time;
	} else if (shutdown_state == shutdown_omapi_connections &&
		   cur_time - shutdown_time > 4) {
		shutdown_state = shutdown_drop_omapi_connections;
		shutdown_time = cur_time;
	} else if (shutdown_state == shutdown_drop_omapi_connections &&
		   cur_time - shutdown_time > 4) {
		shutdown_state = shutdown_dhcp;
		shutdown_time = cur_time;
		goto oncemore;
	} else if (shutdown_state == shutdown_dhcp &&
		   cur_time - shutdown_time > 4) {
		shutdown_state = shutdown_done;
		shutdown_time = cur_time;
	}

#if defined (FAILOVER_PROTOCOL)
	/* Set all failover peers into the shutdown state. */
	if (shutdown_state == shutdown_dhcp) {
	    for (state = failover_states; state; state = state -> next) {
		if (state -> me.state == normal) {
		    dhcp_failover_set_state (state, shut_down);
		    failover_connection_count++;
		}
		if (state -> me.state == shut_down &&
		    state -> partner.state != partner_down)
			failover_connection_count++;
	    }
	}

	if (shutdown_state == shutdown_done) {
	    for (state = failover_states; state; state = state -> next) {
		if (state -> me.state == shut_down) {
		    if (state -> link_to_peer)
			dhcp_failover_link_dereference (&state -> link_to_peer,
							MDL);
		    dhcp_failover_set_state (state, recover);
		}
	    }
#if defined (DEBUG_MEMORY_LEAKAGE) && \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	    free_everything ();
	    omapi_print_dmalloc_usage_by_caller ();
#endif
	    exit (0);
	}		
#else
	if (shutdown_state == shutdown_done) {
#if defined (DEBUG_MEMORY_LEAKAGE) && \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
		free_everything ();
		omapi_print_dmalloc_usage_by_caller (); 
#endif
		exit (0);
	}
#endif
	if (shutdown_state == shutdown_dhcp &&
#if defined(FAILOVER_PROTOCOL)
	    !failover_connection_count &&
#endif
	    ISC_TRUE) {
		shutdown_state = shutdown_done;
		shutdown_time = cur_time;
		goto oncemore;
	}
	tv.tv_sec = cur_tv.tv_sec + 1;
	tv.tv_usec = cur_tv.tv_usec;
	add_timeout (&tv,
		     (void (*)(void *))dhcp_io_shutdown_countdown, 0, 0, 0);
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_set_control_state (control_object_state_t oldstate,
				     control_object_state_t newstate)
{
	struct timeval tv;

	if (newstate != server_shutdown)
		return DHCP_R_INVALIDARG;
	/* Re-entry. */
	if (shutdown_signal == SIGUSR1)
		return ISC_R_SUCCESS;
	shutdown_time = cur_time;
	shutdown_state = shutdown_listeners;
	/* Called by user. */
	if (shutdown_signal == 0) {
		shutdown_signal = SIGUSR1;
		dhcp_io_shutdown_countdown (0);
		return ISC_R_SUCCESS;
	}
	/* Called on signal. */
	log_info("Received signal %d, initiating shutdown.", shutdown_signal);
	shutdown_signal = SIGUSR1;
	
	/*
	 * Prompt the shutdown event onto the timer queue
	 * and return to the dispatch loop.
	 */
	tv.tv_sec = cur_tv.tv_sec;
	tv.tv_usec = cur_tv.tv_usec + 1;
	add_timeout(&tv,
		    (void (*)(void *))dhcp_io_shutdown_countdown, 0, 0, 0);
	return ISC_R_SUCCESS;
}
