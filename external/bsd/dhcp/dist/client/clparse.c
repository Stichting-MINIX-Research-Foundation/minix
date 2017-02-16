/*	$NetBSD: clparse.c,v 1.1.1.3 2014/07/12 11:57:35 spz Exp $	*/
/* clparse.c

   Parser for dhclient config and lease files... */

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
__RCSID("$NetBSD: clparse.c,v 1.1.1.3 2014/07/12 11:57:35 spz Exp $");

#include "dhcpd.h"
#include <errno.h>

struct client_config top_level_config;

#define NUM_DEFAULT_REQUESTED_OPTS	9
struct option *default_requested_options[NUM_DEFAULT_REQUESTED_OPTS + 1];

static void parse_client_default_duid(struct parse *cfile);
static void parse_client6_lease_statement(struct parse *cfile);
#ifdef DHCPv6
static struct dhc6_ia *parse_client6_ia_na_statement(struct parse *cfile);
static struct dhc6_ia *parse_client6_ia_ta_statement(struct parse *cfile);
static struct dhc6_ia *parse_client6_ia_pd_statement(struct parse *cfile);
static struct dhc6_addr *parse_client6_iaaddr_statement(struct parse *cfile);
static struct dhc6_addr *parse_client6_iaprefix_statement(struct parse *cfile);
#endif /* DHCPv6 */

/* client-conf-file :== client-declarations END_OF_FILE
   client-declarations :== <nil>
			 | client-declaration
			 | client-declarations client-declaration */

isc_result_t read_client_conf ()
{
	struct client_config *config;
	struct interface_info *ip;
	isc_result_t status;
	unsigned code;

        /* 
         * TODO: LATER constant is very undescriptive. We should review it and
         * change it to something more descriptive or even better remove it
         * completely as it is currently not used.
         */
#ifdef LATER
        struct parse *parse = NULL;
#endif

	/* Initialize the default request list. */
	memset(default_requested_options, 0, sizeof(default_requested_options));

	/* 1 */
	code = DHO_SUBNET_MASK;
	option_code_hash_lookup(&default_requested_options[0],
				dhcp_universe.code_hash, &code, 0, MDL);

	/* 2 */
	code = DHO_BROADCAST_ADDRESS;
	option_code_hash_lookup(&default_requested_options[1],
				dhcp_universe.code_hash, &code, 0, MDL);

	/* 3 */
	code = DHO_TIME_OFFSET;
	option_code_hash_lookup(&default_requested_options[2],
				dhcp_universe.code_hash, &code, 0, MDL);

	/* 4 */
	code = DHO_ROUTERS;
	option_code_hash_lookup(&default_requested_options[3],
				dhcp_universe.code_hash, &code, 0, MDL);

	/* 5 */
	code = DHO_DOMAIN_NAME;
	option_code_hash_lookup(&default_requested_options[4],
				dhcp_universe.code_hash, &code, 0, MDL);

	/* 6 */
	code = DHO_DOMAIN_NAME_SERVERS;
	option_code_hash_lookup(&default_requested_options[5],
				dhcp_universe.code_hash, &code, 0, MDL);

	/* 7 */
	code = DHO_HOST_NAME;
	option_code_hash_lookup(&default_requested_options[6],
				dhcp_universe.code_hash, &code, 0, MDL);

	/* 8 */
	code = D6O_NAME_SERVERS;
	option_code_hash_lookup(&default_requested_options[7],
				dhcpv6_universe.code_hash, &code, 0, MDL);

	/* 9 */
	code = D6O_DOMAIN_SEARCH;
	option_code_hash_lookup(&default_requested_options[8],
				dhcpv6_universe.code_hash, &code, 0, MDL);

	for (code = 0 ; code < NUM_DEFAULT_REQUESTED_OPTS ; code++) {
		if (default_requested_options[code] == NULL)
			log_fatal("Unable to find option definition for "
				  "index %u during default parameter request "
				  "assembly.", code);
	}

	/* Initialize the top level client configuration. */
	memset (&top_level_config, 0, sizeof top_level_config);

	/* Set some defaults... */
	top_level_config.timeout = 60;
	top_level_config.select_interval = 0;
	top_level_config.reboot_timeout = 10;
	top_level_config.retry_interval = 300;
	top_level_config.backoff_cutoff = 15;
	top_level_config.initial_interval = 3;

	/*
	 * RFC 2131, section 4.4.1 specifies that the client SHOULD wait a
	 * random time between 1 and 10 seconds. However, we choose to not
	 * implement this default. If user is inclined to really have that
	 * delay, he is welcome to do so, using 'initial-delay X;' parameter
	 * in config file.
	 */
	top_level_config.initial_delay = 0;

	top_level_config.bootp_policy = P_ACCEPT;
	top_level_config.script_name = path_dhclient_script;
	top_level_config.requested_options = default_requested_options;
	top_level_config.omapi_port = -1;
	top_level_config.do_forward_update = 1;
	/* Requested lease time, used by DHCPv6 (DHCPv4 uses the option cache)
	 */
	top_level_config.requested_lease = 7200;

	group_allocate (&top_level_config.on_receipt, MDL);
	if (!top_level_config.on_receipt)
		log_fatal ("no memory for top-level on_receipt group");

	group_allocate (&top_level_config.on_transmission, MDL);
	if (!top_level_config.on_transmission)
		log_fatal ("no memory for top-level on_transmission group");

	status = read_client_conf_file (path_dhclient_conf,
					(struct interface_info *)0,
					&top_level_config);

	if (status != ISC_R_SUCCESS) {
		;
#ifdef LATER
		/* Set up the standard name service updater routine. */
		status = new_parse(&parse, -1, default_client_config,
				   sizeof(default_client_config) - 1,
				   "default client configuration", 0);
		if (status != ISC_R_SUCCESS)
			log_fatal ("can't begin default client config!");
	}

	if (parse != NULL) {
		do {
			token = peek_token(&val, NULL, cfile);
			if (token == END_OF_FILE)
				break;
			parse_client_statement(cfile, NULL, &top_level_config);
		} while (1);
		end_parse(&parse);
#endif
	}

	/* Set up state and config structures for clients that don't
	   have per-interface configuration statements. */
	config = (struct client_config *)0;
	for (ip = interfaces; ip; ip = ip -> next) {
		if (!ip -> client) {
			ip -> client = (struct client_state *)
				dmalloc (sizeof (struct client_state), MDL);
			if (!ip -> client)
				log_fatal ("no memory for client state.");
			memset (ip -> client, 0, sizeof *(ip -> client));
			ip -> client -> interface = ip;
		}

		if (!ip -> client -> config) {
			if (!config) {
				config = (struct client_config *)
					dmalloc (sizeof (struct client_config),
						 MDL);
				if (!config)
				    log_fatal ("no memory for client config.");
				memcpy (config, &top_level_config,
					sizeof top_level_config);
			}
			ip -> client -> config = config;
		}
	}
	return status;
}

int read_client_conf_file (const char *name, struct interface_info *ip,
			   struct client_config *client)
{
	int file;
	struct parse *cfile;
	const char *val;
	int token;
	isc_result_t status;

	if ((file = open (name, O_RDONLY)) < 0)
		return uerr2isc (errno);

	cfile = NULL;
	status = new_parse(&cfile, file, NULL, 0, path_dhclient_conf, 0);
	if (status != ISC_R_SUCCESS || cfile == NULL)
		return status;

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE)
			break;
		parse_client_statement (cfile, ip, client);
	} while (1);
	skip_token(&val, (unsigned *)0, cfile);
	status = (cfile -> warnings_occurred
		  ? DHCP_R_BADPARSE
		  : ISC_R_SUCCESS);
	end_parse (&cfile);
	return status;
}


/* lease-file :== client-lease-statements END_OF_FILE
   client-lease-statements :== <nil>
		     | client-lease-statements LEASE client-lease-statement */

void read_client_leases ()
{
	int file;
	isc_result_t status;
	struct parse *cfile;
	const char *val;
	int token;

	/* Open the lease file.   If we can't open it, just return -
	   we can safely trust the server to remember our state. */
	if ((file = open (path_dhclient_db, O_RDONLY)) < 0)
		return;

	cfile = NULL;
	status = new_parse(&cfile, file, NULL, 0, path_dhclient_db, 0);
	if (status != ISC_R_SUCCESS || cfile == NULL)
		return;

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE)
			break;

		switch (token) {
		      case DEFAULT_DUID:
			parse_client_default_duid(cfile);
			break;

		      case LEASE:
			parse_client_lease_statement(cfile, 0);
			break;

		      case LEASE6:
			parse_client6_lease_statement(cfile);
			break;

		      default:
			log_error ("Corrupt lease file - possible data loss!");
			skip_to_semi (cfile);
			break;
		}
	} while (1);

	end_parse (&cfile);
}

/* client-declaration :== 
	SEND option-decl |
	DEFAULT option-decl |
	SUPERSEDE option-decl |
	PREPEND option-decl |
	APPEND option-decl |
	hardware-declaration |
	ALSO REQUEST option-list |
	ALSO REQUIRE option-list |
	REQUEST option-list |
	REQUIRE option-list |
	TIMEOUT number |
	RETRY number |
	REBOOT number |
	SELECT_TIMEOUT number |
	SCRIPT string |
	VENDOR_SPACE string |
	interface-declaration |
	LEASE client-lease-statement |
	ALIAS client-lease-statement |
	KEY key-definition */

void parse_client_statement (cfile, ip, config)
	struct parse *cfile;
	struct interface_info *ip;
	struct client_config *config;
{
	int token;
	const char *val;
	struct option *option = NULL;
	struct executable_statement *stmt;
	int lose;
	char *name;
	enum policy policy;
	int known;
	int tmp, i;
	isc_result_t status;
	struct option ***append_list, **new_list, **cat_list;

	switch (peek_token (&val, (unsigned *)0, cfile)) {
	      case INCLUDE:
		skip_token(&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile, "filename string expected.");
			skip_to_semi (cfile);
		} else {
			status = read_client_conf_file (val, ip, config);
			if (status != ISC_R_SUCCESS)
				parse_warn (cfile, "%s: bad parse.", val);
			parse_semi (cfile);
		}
		return;
		
	      case KEY:
		skip_token(&val, (unsigned *)0, cfile);
		if (ip) {
			/* This may seem arbitrary, but there's a reason for
			   doing it: the authentication key database is not
			   scoped.  If we allow the user to declare a key other
			   than in the outer scope, the user is very likely to
			   believe that the key will only be used in that
			   scope.  If the user only wants the key to be used on
			   one interface, because it's known that the other
			   interface may be connected to an insecure net and
			   the secret key is considered sensitive, we don't
			   want to lull them into believing they've gotten
			   their way.   This is a bit contrived, but people
			   tend not to be entirely rational about security. */
			parse_warn (cfile, "key definition not allowed here.");
			skip_to_semi (cfile);
			break;
		}
		parse_key (cfile);
		return;

	      case TOKEN_ALSO:
		/* consume ALSO */
		skip_token(&val, NULL, cfile);

		/* consume type of ALSO list. */
		token = next_token(&val, NULL, cfile);

		if (token == REQUEST) {
			append_list = &config->requested_options;
		} else if (token == REQUIRE) {
			append_list = &config->required_options;
		} else {
			parse_warn(cfile, "expected REQUEST or REQUIRE list");
			skip_to_semi(cfile);
			return;
		}

		/* If there is no list, cut the concat short. */
		if (*append_list == NULL) {
			parse_option_list(cfile, append_list);
			return;
		}

		/* Count the length of the existing list. */
		for (i = 0 ; (*append_list)[i] != NULL ; i++)
			; /* This space intentionally left blank. */

		/* If there's no codes on the list, cut the concat short. */
		if (i == 0) {
			parse_option_list(cfile, append_list);
			return;
		}

		tmp = parse_option_list(cfile, &new_list);

		if (tmp == 0 || new_list == NULL)
			return;

		/* Allocate 'i + tmp' buckets plus a terminator. */
		cat_list = dmalloc(sizeof(struct option *) * (i + tmp + 1),
				   MDL);

		if (cat_list == NULL) {
			log_error("Unable to allocate memory for new "
				  "request list.");
			skip_to_semi(cfile);
			return;
		}

		for (i = 0 ; (*append_list)[i] != NULL ; i++)
			option_reference(&cat_list[i], (*append_list)[i], MDL);

		tmp = i;

		for (i = 0 ; new_list[i] != 0 ; i++)
			option_reference(&cat_list[tmp++], new_list[i], MDL);

		cat_list[tmp] = 0;

		/* XXX: We cannot free the old list, because it may have been
		 * XXX: assigned from an outer configuration scope (or may be
		 * XXX: the static default setting).
		 */
		*append_list = cat_list;

		return;

		/* REQUIRE can either start a policy statement or a
		   comma-separated list of names of required options. */
	      case REQUIRE:
		skip_token(&val, (unsigned *)0, cfile);
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == AUTHENTICATION) {
			policy = P_REQUIRE;
			goto do_policy;
		}
		parse_option_list (cfile, &config -> required_options);
		return;

	      case IGNORE:
		skip_token(&val, (unsigned *)0, cfile);
		policy = P_IGNORE;
		goto do_policy;

	      case ACCEPT:
		skip_token(&val, (unsigned *)0, cfile);
		policy = P_ACCEPT;
		goto do_policy;

	      case PREFER:
		skip_token(&val, (unsigned *)0, cfile);
		policy = P_PREFER;
		goto do_policy;

	      case DONT:
		skip_token(&val, (unsigned *)0, cfile);
		policy = P_DONT;
		goto do_policy;

	      do_policy:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == AUTHENTICATION) {
			if (policy != P_PREFER &&
			    policy != P_REQUIRE &&
			    policy != P_DONT) {
				parse_warn (cfile,
					    "invalid authentication policy.");
				skip_to_semi (cfile);
				return;
			}
			config -> auth_policy = policy;
		} else if (token != TOKEN_BOOTP) {
			if (policy != P_PREFER &&
			    policy != P_IGNORE &&
			    policy != P_ACCEPT) {
				parse_warn (cfile, "invalid bootp policy.");
				skip_to_semi (cfile);
				return;
			}
			config -> bootp_policy = policy;
		} else {
			parse_warn (cfile, "expecting a policy type.");
			skip_to_semi (cfile);
			return;
		} 
		break;

	      case OPTION:
		skip_token(&val, (unsigned *)0, cfile);
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == SPACE) {
			if (ip) {
				parse_warn (cfile,
					    "option space definitions %s",
					    " may not be scoped.");
				skip_to_semi (cfile);
				break;
			}
			parse_option_space_decl (cfile);
			return;
		}

		known = 0;
		status = parse_option_name(cfile, 1, &known, &option);
		if (status != ISC_R_SUCCESS || option == NULL)
			return;

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != CODE) {
			parse_warn (cfile, "expecting \"code\" keyword.");
			skip_to_semi (cfile);
			option_dereference(&option, MDL);
			return;
		}
		if (ip) {
			parse_warn (cfile,
				    "option definitions may only appear in %s",
				    "the outermost scope.");
			skip_to_semi (cfile);
			option_dereference(&option, MDL);
			return;
		}

		/*
		 * If the option was known, remove it from the code and name
		 * hash tables before redefining it.
		 */
		if (known) {
			option_name_hash_delete(option->universe->name_hash,
						option->name, 0, MDL);
			option_code_hash_delete(option->universe->code_hash,
						&option->code, 0, MDL);
		}

		parse_option_code_definition(cfile, option);
		option_dereference(&option, MDL);
		return;

	      case MEDIA:
		skip_token(&val, (unsigned *)0, cfile);
		parse_string_list (cfile, &config -> media, 1);
		return;

	      case HARDWARE:
		skip_token(&val, (unsigned *)0, cfile);
		if (ip) {
			parse_hardware_param (cfile, &ip -> hw_address);
		} else {
			parse_warn (cfile, "hardware address parameter %s",
				    "not allowed here.");
			skip_to_semi (cfile);
		}
		return;

	      case ANYCAST_MAC:
		skip_token(&val, NULL, cfile);
		if (ip != NULL) {
			parse_hardware_param(cfile, &ip->anycast_mac_addr);
		} else {
			parse_warn(cfile, "anycast mac address parameter "
				   "not allowed here.");
			skip_to_semi (cfile);
		}
		return;

	      case REQUEST:
		skip_token(&val, (unsigned *)0, cfile);
		if (config -> requested_options == default_requested_options)
			config -> requested_options = NULL;
		parse_option_list (cfile, &config -> requested_options);
		return;

	      case TIMEOUT:
		skip_token(&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> timeout);
		return;

	      case RETRY:
		skip_token(&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> retry_interval);
		return;

	      case SELECT_TIMEOUT:
		skip_token(&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> select_interval);
		return;

	      case OMAPI:
		skip_token(&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != PORT) {
			parse_warn (cfile,
				    "unexpected omapi subtype: %s", val);
			skip_to_semi (cfile);
			return;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != NUMBER) {
			parse_warn (cfile, "invalid port number: `%s'", val);
			skip_to_semi (cfile);
			return;
		}
		tmp = atoi (val);
		if (tmp < 0 || tmp > 65535)
			parse_warn (cfile, "invalid omapi port %d.", tmp);
		else if (config != &top_level_config)
			parse_warn (cfile,
				    "omapi port only works at top level.");
		else
			config -> omapi_port = tmp;
		parse_semi (cfile);
		return;
		
	      case DO_FORWARD_UPDATE:
		skip_token(&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (!strcasecmp (val, "on") ||
		    !strcasecmp (val, "true"))
			config -> do_forward_update = 1;
		else if (!strcasecmp (val, "off") ||
			 !strcasecmp (val, "false"))
			config -> do_forward_update = 0;
		else {
			parse_warn (cfile, "expecting boolean value.");
			skip_to_semi (cfile);
			return;
		}
		parse_semi (cfile);
		return;

	      case REBOOT:
		skip_token(&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> reboot_timeout);
		return;

	      case BACKOFF_CUTOFF:
		skip_token(&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> backoff_cutoff);
		return;

	      case INITIAL_INTERVAL:
		skip_token(&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> initial_interval);
		return;

	      case INITIAL_DELAY:
		skip_token(&val, (unsigned *)0, cfile);
		parse_lease_time (cfile, &config -> initial_delay);
		return;

	      case SCRIPT:
		skip_token(&val, (unsigned *)0, cfile);
		parse_string (cfile, &config -> script_name, (unsigned *)0);
		return;

	      case VENDOR:
		skip_token(&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != OPTION) {
			parse_warn (cfile, "expecting 'vendor option space'");
			skip_to_semi (cfile);
			return;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != SPACE) {
			parse_warn (cfile, "expecting 'vendor option space'");
			skip_to_semi (cfile);
			return;
		}
		token = next_token (&val, (unsigned *)0, cfile);
		if (!is_identifier (token)) {
			parse_warn (cfile, "expecting an identifier.");
			skip_to_semi (cfile);
			return;
		}
		config -> vendor_space_name = dmalloc (strlen (val) + 1, MDL);
		if (!config -> vendor_space_name)
			log_fatal ("no memory for vendor option space name.");
		strcpy (config -> vendor_space_name, val);
		for (i = 0; i < universe_count; i++)
			if (!strcmp (universes [i] -> name,
				     config -> vendor_space_name))
				break;
		if (i == universe_count) {
			log_error ("vendor option space %s not found.",
				   config -> vendor_space_name);
		}
		parse_semi (cfile);
		return;

	      case INTERFACE:
		skip_token(&val, (unsigned *)0, cfile);
		if (ip)
			parse_warn (cfile, "nested interface declaration.");
		parse_interface_declaration (cfile, config, (char *)0);
		return;

	      case PSEUDO:
		skip_token(&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		name = dmalloc (strlen (val) + 1, MDL);
		if (!name)
			log_fatal ("no memory for pseudo interface name");
		strcpy (name, val);
		parse_interface_declaration (cfile, config, name);
		return;
		
	      case LEASE:
		skip_token(&val, (unsigned *)0, cfile);
		parse_client_lease_statement (cfile, 1);
		return;

	      case ALIAS:
		skip_token(&val, (unsigned *)0, cfile);
		parse_client_lease_statement (cfile, 2);
		return;

	      case REJECT:
		skip_token(&val, (unsigned *)0, cfile);
		parse_reject_statement (cfile, config);
		return;

	      default:
		lose = 0;
		stmt = (struct executable_statement *)0;
		if (!parse_executable_statement (&stmt,
						 cfile, &lose, context_any)) {
			if (!lose) {
				parse_warn (cfile, "expecting a statement.");
				skip_to_semi (cfile);
			}
		} else {
			struct executable_statement **eptr, *sptr;
			if (stmt &&
			    (stmt -> op == send_option_statement ||
			     (stmt -> op == on_statement &&
			      (stmt -> data.on.evtypes & ON_TRANSMISSION)))) {
			    eptr = &config -> on_transmission -> statements;
			    if (stmt -> op == on_statement) {
				    sptr = (struct executable_statement *)0;
				    executable_statement_reference
					    (&sptr,
					     stmt -> data.on.statements, MDL);
				    executable_statement_dereference (&stmt,
								      MDL);
				    executable_statement_reference (&stmt,
								    sptr,
								    MDL);
				    executable_statement_dereference (&sptr,
								      MDL);
			    }
			} else
			    eptr = &config -> on_receipt -> statements;

			if (stmt) {
				for (; *eptr; eptr = &(*eptr) -> next)
					;
				executable_statement_reference (eptr,
								stmt, MDL);
			}
			return;
		}
		break;
	}
	parse_semi (cfile);
}

/* option-list :== option_name |
   		   option_list COMMA option_name */

int
parse_option_list(struct parse *cfile, struct option ***list)
{
	int ix;
	int token;
	const char *val;
	pair p = (pair)0, q = (pair)0, r;
	struct option *option = NULL;
	isc_result_t status;

	ix = 0;
	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == SEMI) {
			token = next_token (&val, (unsigned *)0, cfile);
			break;
		}
		if (!is_identifier (token)) {
			parse_warn (cfile, "%s: expected option name.", val);
			skip_token(&val, (unsigned *)0, cfile);
			skip_to_semi (cfile);
			return 0;
		}
		status = parse_option_name(cfile, 0, NULL, &option);
		if (status != ISC_R_SUCCESS || option == NULL) {
			parse_warn (cfile, "%s: expected option name.", val);
			return 0;
		}
		r = new_pair (MDL);
		if (!r)
			log_fatal ("can't allocate pair for option code.");
		/* XXX: we should probably carry a reference across this */
		r->car = (caddr_t)option;
		option_dereference(&option, MDL);
		r -> cdr = (pair)0;
		if (p)
			q -> cdr = r;
		else
			p = r;
		q = r;
		++ix;
		token = next_token (&val, (unsigned *)0, cfile);
	} while (token == COMMA);
	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
		return 0;
	}
	/* XXX we can't free the list here, because we may have copied
	   XXX it from an outer config state. */
	*list = NULL;
	if (ix) {
		*list = dmalloc ((ix + 1) * sizeof(struct option *), MDL);
		if (!*list)
			log_error ("no memory for option list.");
		else {
			ix = 0;
			for (q = p; q; q = q -> cdr)
				option_reference(&(*list)[ix++],
						 (struct option *)q->car, MDL);
			(*list)[ix] = NULL;
		}
		while (p) {
			q = p -> cdr;
			free_pair (p, MDL);
			p = q;
		}
	}

	return ix;
}

/* interface-declaration :==
   	INTERFACE string LBRACE client-declarations RBRACE */

void parse_interface_declaration (cfile, outer_config, name)
	struct parse *cfile;
	struct client_config *outer_config;
	char *name;
{
	int token;
	const char *val;
	struct client_state *client, **cp;
	struct interface_info *ip = (struct interface_info *)0;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != STRING) {
		parse_warn (cfile, "expecting interface name (in quotes).");
		skip_to_semi (cfile);
		return;
	}

	if (!interface_or_dummy (&ip, val))
		log_fatal ("Can't allocate interface %s.", val);

	/* If we were given a name, this is a pseudo-interface. */
	if (name) {
		make_client_state (&client);
		client -> name = name;
		client -> interface = ip;
		for (cp = &ip -> client; *cp; cp = &((*cp) -> next))
			;
		*cp = client;
	} else {
		if (!ip -> client) {
			make_client_state (&ip -> client);
			ip -> client -> interface = ip;
		}
		client = ip -> client;
	}

	if (!client -> config)
		make_client_config (client, outer_config);

	ip -> flags &= ~INTERFACE_AUTOMATIC;
	interfaces_requested = 1;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace.");
		skip_to_semi (cfile);
		return;
	}

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE) {
			parse_warn (cfile,
				    "unterminated interface declaration.");
			return;
		}
		if (token == RBRACE)
			break;
		parse_client_statement (cfile, ip, client -> config);
	} while (1);
	skip_token(&val, (unsigned *)0, cfile);
}

int interface_or_dummy (struct interface_info **pi, const char *name)
{
	struct interface_info *i;
	struct interface_info *ip = (struct interface_info *)0;
	isc_result_t status;

	/* Find the interface (if any) that matches the name. */
	for (i = interfaces; i; i = i -> next) {
		if (!strcmp (i -> name, name)) {
			interface_reference (&ip, i, MDL);
			break;
		}
	}

	/* If it's not a real interface, see if it's on the dummy list. */
	if (!ip) {
		for (ip = dummy_interfaces; ip; ip = ip -> next) {
			if (!strcmp (ip -> name, name)) {
				interface_reference (&ip, i, MDL);
				break;
			}
		}
	}

	/* If we didn't find an interface, make a dummy interface as
	   a placeholder. */
	if (!ip) {
		if ((status = interface_allocate (&ip, MDL)) != ISC_R_SUCCESS)
			log_fatal ("Can't record interface %s: %s",
				   name, isc_result_totext (status));

		if (strlen(name) >= sizeof(ip->name)) {
			interface_dereference(&ip, MDL);
			return 0;
		}
		strcpy(ip->name, name);

		if (dummy_interfaces) {
			interface_reference (&ip -> next,
					     dummy_interfaces, MDL);
			interface_dereference (&dummy_interfaces, MDL);
		}
		interface_reference (&dummy_interfaces, ip, MDL);
	}
	if (pi)
		status = interface_reference (pi, ip, MDL);
	else
		status = ISC_R_FAILURE;
	interface_dereference (&ip, MDL);
	if (status != ISC_R_SUCCESS)
		return 0;
	return 1;
}

void make_client_state (state)
	struct client_state **state;
{
	*state = ((struct client_state *)dmalloc (sizeof **state, MDL));
	if (!*state)
		log_fatal ("no memory for client state\n");
	memset (*state, 0, sizeof **state);
}

void make_client_config (client, config)
	struct client_state *client;
	struct client_config *config;
{
	client -> config = (((struct client_config *)
			     dmalloc (sizeof (struct client_config), MDL)));
	if (!client -> config)
		log_fatal ("no memory for client config\n");
	memcpy (client -> config, config, sizeof *config);
	if (!clone_group (&client -> config -> on_receipt,
			  config -> on_receipt, MDL) ||
	    !clone_group (&client -> config -> on_transmission,
			  config -> on_transmission, MDL))
		log_fatal ("no memory for client state groups.");
}

/* client-lease-statement :==
	LBRACE client-lease-declarations RBRACE

	client-lease-declarations :==
		<nil> |
		client-lease-declaration |
		client-lease-declarations client-lease-declaration */


void parse_client_lease_statement (cfile, is_static)
	struct parse *cfile;
	int is_static;
{
	struct client_lease *lease, *lp, *pl, *next;
	struct interface_info *ip = (struct interface_info *)0;
	int token;
	const char *val;
	struct client_state *client = (struct client_state *)0;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace.");
		skip_to_semi (cfile);
		return;
	}

	lease = ((struct client_lease *)
		 dmalloc (sizeof (struct client_lease), MDL));
	if (!lease)
		log_fatal ("no memory for lease.\n");
	memset (lease, 0, sizeof *lease);
	lease -> is_static = is_static;
	if (!option_state_allocate (&lease -> options, MDL))
		log_fatal ("no memory for lease options.\n");

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE) {
			parse_warn (cfile, "unterminated lease declaration.");
			return;
		}
		if (token == RBRACE)
			break;
		parse_client_lease_declaration (cfile, lease, &ip, &client);
	} while (1);
	skip_token(&val, (unsigned *)0, cfile);

	/* If the lease declaration didn't include an interface
	   declaration that we recognized, it's of no use to us. */
	if (!ip) {
		destroy_client_lease (lease);
		return;
	}

	/* Make sure there's a client state structure... */
	if (!ip -> client) {
		make_client_state (&ip -> client);
		ip -> client -> interface = ip;
	}
	if (!client)
		client = ip -> client;

	/* If this is an alias lease, it doesn't need to be sorted in. */
	if (is_static == 2) {
		ip -> client -> alias = lease;
		return;
	}

	/* The new lease may supersede a lease that's not the
	   active lease but is still on the lease list, so scan the
	   lease list looking for a lease with the same address, and
	   if we find it, toss it. */
	pl = (struct client_lease *)0;
	for (lp = client -> leases; lp; lp = next) {
		next = lp -> next;
		if (lp -> address.len == lease -> address.len &&
		    !memcmp (lp -> address.iabuf, lease -> address.iabuf,
			     lease -> address.len)) {
			if (pl)
				pl -> next = next;
			else
				client -> leases = next;
			destroy_client_lease (lp);
			break;
		} else
			pl = lp;
	}

	/* If this is a preloaded lease, just put it on the list of recorded
	   leases - don't make it the active lease. */
	if (is_static) {
		lease -> next = client -> leases;
		client -> leases = lease;
		return;
	}
		
	/* The last lease in the lease file on a particular interface is
	   the active lease for that interface.    Of course, we don't know
	   what the last lease in the file is until we've parsed the whole
	   file, so at this point, we assume that the lease we just parsed
	   is the active lease for its interface.   If there's already
	   an active lease for the interface, and this lease is for the same
	   ip address, then we just toss the old active lease and replace
	   it with this one.   If this lease is for a different address,
	   then if the old active lease has expired, we dump it; if not,
	   we put it on the list of leases for this interface which are
	   still valid but no longer active. */
	if (client -> active) {
		if (client -> active -> expiry < cur_time)
			destroy_client_lease (client -> active);
		else if (client -> active -> address.len ==
			 lease -> address.len &&
			 !memcmp (client -> active -> address.iabuf,
				  lease -> address.iabuf,
				  lease -> address.len))
			destroy_client_lease (client -> active);
		else {
			client -> active -> next = client -> leases;
			client -> leases = client -> active;
		}
	}
	client -> active = lease;

	/* phew. */
}

/* client-lease-declaration :==
	BOOTP |
	INTERFACE string |
	FIXED_ADDR ip_address |
	FILENAME string |
	SERVER_NAME string |
	OPTION option-decl |
	RENEW time-decl |
	REBIND time-decl |
	EXPIRE time-decl |
	KEY id */

void parse_client_lease_declaration (cfile, lease, ipp, clientp)
	struct parse *cfile;
	struct client_lease *lease;
	struct interface_info **ipp;
	struct client_state **clientp;
{
	int token;
	const char *val;
	struct interface_info *ip;
	struct option_cache *oc;
	struct client_state *client = (struct client_state *)0;

	switch (next_token (&val, (unsigned *)0, cfile)) {
	      case KEY:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING && !is_identifier (token)) {
			parse_warn (cfile, "expecting key name.");
			skip_to_semi (cfile);
			break;
		}
		if (omapi_auth_key_lookup_name (&lease -> key, val) !=
		    ISC_R_SUCCESS)
			parse_warn (cfile, "unknown key %s", val);
		parse_semi (cfile);
		break;
	      case TOKEN_BOOTP:
		lease -> is_bootp = 1;
		break;

	      case INTERFACE:
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile,
				    "expecting interface name (in quotes).");
			skip_to_semi (cfile);
			break;
		}
		if (!interface_or_dummy (ipp, val))
			log_fatal ("Can't allocate interface %s.", val);
		break;

	      case NAME:
		token = next_token (&val, (unsigned *)0, cfile);
		ip = *ipp;
		if (!ip) {
			parse_warn (cfile, "state name precedes interface.");
			break;
		}
		for (client = ip -> client; client; client = client -> next)
			if (client -> name && !strcmp (client -> name, val))
				break;
		if (!client)
			parse_warn (cfile,
				    "lease specified for unknown pseudo.");
		*clientp = client;
		break;

	      case FIXED_ADDR:
		if (!parse_ip_addr (cfile, &lease -> address))
			return;
		break;

	      case MEDIUM:
		parse_string_list (cfile, &lease -> medium, 0);
		return;

	      case FILENAME:
		parse_string (cfile, &lease -> filename, (unsigned *)0);
		return;

	      case SERVER_NAME:
		parse_string (cfile, &lease -> server_name, (unsigned *)0);
		return;

	      case RENEW:
		lease -> renewal = parse_date (cfile);
		return;

	      case REBIND:
		lease -> rebind = parse_date (cfile);
		return;

	      case EXPIRE:
		lease -> expiry = parse_date (cfile);
		return;

	      case OPTION:
		oc = (struct option_cache *)0;
		if (parse_option_decl (&oc, cfile)) {
			save_option(oc->option->universe, lease->options, oc);
			option_cache_dereference (&oc, MDL);
		}
		return;

	      default:
		parse_warn (cfile, "expecting lease declaration.");
		skip_to_semi (cfile);
		break;
	}
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
	}
}

/* Parse a default-duid ""; statement.
 */
static void
parse_client_default_duid(struct parse *cfile)
{
	struct data_string new_duid;
	const char *val = NULL;
	unsigned len;
	int token;

	memset(&new_duid, 0, sizeof(new_duid));

	token = next_token(&val, &len, cfile);
	if (token != STRING) {
		parse_warn(cfile, "Expected DUID string.");
		skip_to_semi(cfile);
		return;
	}

	if (len <= 2) {
		parse_warn(cfile, "Invalid DUID contents.");
		skip_to_semi(cfile);
		return;
	}

	if (!buffer_allocate(&new_duid.buffer, len, MDL)) {
		parse_warn(cfile, "Out of memory parsing default DUID.");
		skip_to_semi(cfile);
		return;
	}
	new_duid.data = new_duid.buffer->data;
	new_duid.len = len;

	memcpy(new_duid.buffer->data, val, len);

	/* Rotate the last entry into place. */
	if (default_duid.buffer != NULL)
		data_string_forget(&default_duid, MDL);
	data_string_copy(&default_duid, &new_duid, MDL);
	data_string_forget(&new_duid, MDL);

	parse_semi(cfile);
}

/* Parse a lease6 {} construct.  The v6 client is a little different
 * than the v4 client today, in that it only retains one lease, the
 * active lease, and discards any less recent information.  It may
 * be useful in the future to cache additional information, but it
 * is not worth the effort for the moment.
 */
static void
parse_client6_lease_statement(struct parse *cfile)
{
#if !defined(DHCPv6)
	parse_warn(cfile, "No DHCPv6 support.");
	skip_to_semi(cfile);
#else /* defined(DHCPv6) */
	struct option_cache *oc = NULL;
	struct dhc6_lease *lease;
	struct dhc6_ia **ia;
	struct client_state *client = NULL;
	struct interface_info *iface = NULL;
	struct data_string ds;
	const char *val;
	unsigned len;
	int token, has_ia, no_semi, has_name;

	token = next_token(NULL, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "Expecting open curly brace.");
		skip_to_semi(cfile);
		return;
	}

	lease = dmalloc(sizeof(*lease), MDL);
	if (lease == NULL) {
		parse_warn(cfile, "Unable to allocate lease state.");
		skip_to_rbrace(cfile, 1);
		return;
	}

	option_state_allocate(&lease->options, MDL);
	if (lease->options == NULL) {
		parse_warn(cfile, "Unable to allocate option cache.");
		skip_to_rbrace(cfile, 1);
		dfree(lease, MDL);
		return;
	}

	has_ia = 0;
	has_name = 0;
	ia = &lease->bindings;
	token = next_token(&val, NULL, cfile);
	while (token != RBRACE) {
		no_semi = 0;

		switch(token) {
		      case IA_NA:
			*ia = parse_client6_ia_na_statement(cfile);
			if (*ia != NULL) {
				ia = &(*ia)->next;
				has_ia = 1;
			}

			no_semi = 1;

			break;

		      case IA_TA:
			*ia = parse_client6_ia_ta_statement(cfile);
			if (*ia != NULL) {
				ia = &(*ia)->next;
				has_ia = 1;
			}

			no_semi = 1;

			break;

		      case IA_PD:
			*ia = parse_client6_ia_pd_statement(cfile);
			if (*ia != NULL) {
				ia = &(*ia)->next;
				has_ia = 1;
			}

			no_semi = 1;

			break;

		      case INTERFACE:
			if (iface != NULL) {
				parse_warn(cfile, "Multiple interface names?");
				skip_to_semi(cfile);
				no_semi = 1;
				break;
			}

			token = next_token(&val, &len, cfile);
			if (token != STRING) {
			      strerror:
				parse_warn(cfile, "Expecting a string.");
				skip_to_semi(cfile);
				no_semi = 1;
				break;
			}

			for (iface = interfaces ; iface != NULL ;
			     iface = iface->next) {
				if (strcmp(iface->name, val) == 0)
					break;
			}

			if (iface == NULL) {
				parse_warn(cfile, "Unknown interface.");
				break;
			}

			break;

		      case NAME:
			has_name = 1;

			if (client != NULL) {
				parse_warn(cfile, "Multiple state names?");
				skip_to_semi(cfile);
				no_semi = 1;
				break;
			}

			if (iface == NULL) {
				parse_warn(cfile, "Client name without "
						  "interface.");
				skip_to_semi(cfile);
				no_semi = 1;
				break;
			}

			token = next_token(&val, &len, cfile);
			if (token != STRING)
				goto strerror;

			for (client = iface->client ; client != NULL ;
			     client = client->next) {
				if ((client->name != NULL) &&
				    (strcmp(client->name, val) == 0))
					break;
			}

			if (client == NULL) {
				parse_warn(cfile, "Unknown client state %s.",
					   val);
				break;
			}

			break;

		      case OPTION:
			if (parse_option_decl(&oc, cfile)) {
				save_option(oc->option->universe,
					    lease->options, oc);
				option_cache_dereference(&oc, MDL);
			}
			no_semi = 1;
			break;

		      case TOKEN_RELEASED:
		      case TOKEN_ABANDONED:
			lease->released = ISC_TRUE;
			break;

		      default:
			parse_warn(cfile, "Unexpected token, %s.", val);
			no_semi = 1;
			skip_to_semi(cfile);
			break;
		}

		if (!no_semi)
			parse_semi(cfile);

		token = next_token(&val, NULL, cfile);

		if (token == END_OF_FILE) {
			parse_warn(cfile, "Unexpected end of file.");
			break;
		}
	}

	if (!has_ia) {
		log_debug("Lease with no IA's discarded from lease db.");
		dhc6_lease_destroy(&lease, MDL);
		return;
	}

	if (iface == NULL)
		parse_warn(cfile, "Lease has no interface designation.");
	else if (!has_name && (client == NULL)) {
		for (client = iface->client ; client != NULL ;
		     client = client->next) {
			if (client->name == NULL)
				break;
		}
	}

	if (client == NULL) {
		parse_warn(cfile, "No matching client state.");
		dhc6_lease_destroy(&lease, MDL);
		return;
	}

	/* Fetch Preference option from option cache. */
	memset(&ds, 0, sizeof(ds));
	oc = lookup_option(&dhcpv6_universe, lease->options, D6O_PREFERENCE);
	if ((oc != NULL) &&
	    evaluate_option_cache(&ds, NULL, NULL, NULL, lease->options,
				  NULL, &global_scope, oc, MDL)) {
		if (ds.len != 1) {
			log_error("Invalid length of DHCPv6 Preference option "
				  "(%d != 1)", ds.len);
			data_string_forget(&ds, MDL);
			dhc6_lease_destroy(&lease, MDL);
			return;
		} else
			lease->pref = ds.data[0];

		data_string_forget(&ds, MDL);
	}

	/* Fetch server-id option from option cache. */
	oc = lookup_option(&dhcpv6_universe, lease->options, D6O_SERVERID);
	if ((oc == NULL) ||
	    !evaluate_option_cache(&lease->server_id, NULL, NULL, NULL,
				   lease->options, NULL, &global_scope, oc,
				   MDL) ||
	    (lease->server_id.len == 0)) {
		/* This should be impossible... */
		log_error("Invalid SERVERID option cache.");
		dhc6_lease_destroy(&lease, MDL);
		return;
	}

	if (client->active_lease != NULL)
		dhc6_lease_destroy(&client->active_lease, MDL);

	client->active_lease = lease;
#endif /* defined(DHCPv6) */
}

/* Parse an ia_na object from the client lease.
 */
#ifdef DHCPv6
static struct dhc6_ia *
parse_client6_ia_na_statement(struct parse *cfile)
{
	struct option_cache *oc = NULL;
	struct dhc6_ia *ia;
	struct dhc6_addr **addr;
	const char *val;
	int token, no_semi, len;
	u_int8_t buf[5];

	ia = dmalloc(sizeof(*ia), MDL);
	if (ia == NULL) {
		parse_warn(cfile, "Out of memory allocating IA_NA state.");
		skip_to_semi(cfile);
		return NULL;
	}
	ia->ia_type = D6O_IA_NA;

	/* Get IAID. */
	len = parse_X(cfile, buf, 5);
	if (len == 4) {
		memcpy(ia->iaid, buf, 4);
	} else {
		parse_warn(cfile, "Expecting IAID of length 4, got %d.", len);
		skip_to_semi(cfile);
		dfree(ia, MDL);
		return NULL;
	}

	token = next_token(NULL, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "Expecting open curly brace.");
		skip_to_semi(cfile);
		dfree(ia, MDL);
		return NULL;
	}

	option_state_allocate(&ia->options, MDL);
	if (ia->options == NULL) {
		parse_warn(cfile, "Unable to allocate option state.");
		skip_to_rbrace(cfile, 1);
		dfree(ia, MDL);
		return NULL;
	}

	addr = &ia->addrs;
	token = next_token(&val, NULL, cfile);
	while (token != RBRACE) {
		no_semi = 0;

		switch (token) {
		      case STARTS:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				ia->starts = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case RENEW:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				ia->renew = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case REBIND:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				ia->rebind = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case IAADDR:
			*addr = parse_client6_iaaddr_statement(cfile);

			if (*addr != NULL)
				addr = &(*addr)->next;

			no_semi = 1;

			break;

		      case OPTION:
			if (parse_option_decl(&oc, cfile)) {
				save_option(oc->option->universe,
					    ia->options, oc);
				option_cache_dereference(&oc, MDL);
			}
			no_semi = 1;
			break;

		      default:
			parse_warn(cfile, "Unexpected token.");
			no_semi = 1;
			skip_to_semi(cfile);
			break;
		}

		if (!no_semi)
			parse_semi(cfile);

		token = next_token(&val, NULL, cfile);

		if (token == END_OF_FILE) {
			parse_warn(cfile, "Unexpected end of file.");
			break;
		}
	}

	return ia;
}
#endif /* DHCPv6 */

/* Parse an ia_ta object from the client lease.
 */
#ifdef DHCPv6
static struct dhc6_ia *
parse_client6_ia_ta_statement(struct parse *cfile)
{
	struct option_cache *oc = NULL;
	struct dhc6_ia *ia;
	struct dhc6_addr **addr;
	const char *val;
	int token, no_semi, len;
	u_int8_t buf[5];

	ia = dmalloc(sizeof(*ia), MDL);
	if (ia == NULL) {
		parse_warn(cfile, "Out of memory allocating IA_TA state.");
		skip_to_semi(cfile);
		return NULL;
	}
	ia->ia_type = D6O_IA_TA;

	/* Get IAID. */
	len = parse_X(cfile, buf, 5);
	if (len == 4) {
		memcpy(ia->iaid, buf, 4);
	} else {
		parse_warn(cfile, "Expecting IAID of length 4, got %d.", len);
		skip_to_semi(cfile);
		dfree(ia, MDL);
		return NULL;
	}

	token = next_token(NULL, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "Expecting open curly brace.");
		skip_to_semi(cfile);
		dfree(ia, MDL);
		return NULL;
	}

	option_state_allocate(&ia->options, MDL);
	if (ia->options == NULL) {
		parse_warn(cfile, "Unable to allocate option state.");
		skip_to_rbrace(cfile, 1);
		dfree(ia, MDL);
		return NULL;
	}

	addr = &ia->addrs;
	token = next_token(&val, NULL, cfile);
	while (token != RBRACE) {
		no_semi = 0;

		switch (token) {
		      case STARTS:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				ia->starts = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

			/* No RENEW or REBIND */

		      case IAADDR:
			*addr = parse_client6_iaaddr_statement(cfile);

			if (*addr != NULL)
				addr = &(*addr)->next;

			no_semi = 1;

			break;

		      case OPTION:
			if (parse_option_decl(&oc, cfile)) {
				save_option(oc->option->universe,
					    ia->options, oc);
				option_cache_dereference(&oc, MDL);
			}
			no_semi = 1;
			break;

		      default:
			parse_warn(cfile, "Unexpected token.");
			no_semi = 1;
			skip_to_semi(cfile);
			break;
		}

		if (!no_semi)
			parse_semi(cfile);

		token = next_token(&val, NULL, cfile);

		if (token == END_OF_FILE) {
			parse_warn(cfile, "Unexpected end of file.");
			break;
		}
	}

	return ia;
}
#endif /* DHCPv6 */

/* Parse an ia_pd object from the client lease.
 */
#ifdef DHCPv6
static struct dhc6_ia *
parse_client6_ia_pd_statement(struct parse *cfile)
{
	struct option_cache *oc = NULL;
	struct dhc6_ia *ia;
	struct dhc6_addr **pref;
	const char *val;
	int token, no_semi, len;
	u_int8_t buf[5];

	ia = dmalloc(sizeof(*ia), MDL);
	if (ia == NULL) {
		parse_warn(cfile, "Out of memory allocating IA_PD state.");
		skip_to_semi(cfile);
		return NULL;
	}
	ia->ia_type = D6O_IA_PD;

	/* Get IAID. */
	len = parse_X(cfile, buf, 5);
	if (len == 4) {
		memcpy(ia->iaid, buf, 4);
	} else {
		parse_warn(cfile, "Expecting IAID of length 4, got %d.", len);
		skip_to_semi(cfile);
		dfree(ia, MDL);
		return NULL;
	}

	token = next_token(NULL, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "Expecting open curly brace.");
		skip_to_semi(cfile);
		dfree(ia, MDL);
		return NULL;
	}

	option_state_allocate(&ia->options, MDL);
	if (ia->options == NULL) {
		parse_warn(cfile, "Unable to allocate option state.");
		skip_to_rbrace(cfile, 1);
		dfree(ia, MDL);
		return NULL;
	}

	pref = &ia->addrs;
	token = next_token(&val, NULL, cfile);
	while (token != RBRACE) {
		no_semi = 0;

		switch (token) {
		      case STARTS:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				ia->starts = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case RENEW:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				ia->renew = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case REBIND:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				ia->rebind = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case IAPREFIX:
			*pref = parse_client6_iaprefix_statement(cfile);

			if (*pref != NULL)
				pref = &(*pref)->next;

			no_semi = 1;

			break;

		      case OPTION:
			if (parse_option_decl(&oc, cfile)) {
				save_option(oc->option->universe,
					    ia->options, oc);
				option_cache_dereference(&oc, MDL);
			}
			no_semi = 1;
			break;

		      default:
			parse_warn(cfile, "Unexpected token.");
			no_semi = 1;
			skip_to_semi(cfile);
			break;
		}

		if (!no_semi)
			parse_semi(cfile);

		token = next_token(&val, NULL, cfile);

		if (token == END_OF_FILE) {
			parse_warn(cfile, "Unexpected end of file.");
			break;
		}
	}

	return ia;
}
#endif /* DHCPv6 */

/* Parse an iaaddr {} structure. */
#ifdef DHCPv6
static struct dhc6_addr *
parse_client6_iaaddr_statement(struct parse *cfile)
{
	struct option_cache *oc = NULL;
	struct dhc6_addr *addr;
	const char *val;
	int token, no_semi;

	addr = dmalloc(sizeof(*addr), MDL);
	if (addr == NULL) {
		parse_warn(cfile, "Unable to allocate IAADDR state.");
		skip_to_semi(cfile);
		return NULL;
	}

	/* Get IP address. */
	if (!parse_ip6_addr(cfile, &addr->address)) {
		skip_to_semi(cfile);
		dfree(addr, MDL);
		return NULL;
	}

	token = next_token(NULL, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "Expecting open curly bracket.");
		skip_to_semi(cfile);
		dfree(addr, MDL);
		return NULL;
	}

	option_state_allocate(&addr->options, MDL);
	if (addr->options == NULL) {
		parse_warn(cfile, "Unable to allocate option state.");
		skip_to_semi(cfile);
		dfree(addr, MDL);
		return NULL;
	}

	token = next_token(&val, NULL, cfile);
	while (token != RBRACE) {
		no_semi = 0;

		switch (token) {
		      case STARTS:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				addr->starts = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case PREFERRED_LIFE:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				addr->preferred_life = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case MAX_LIFE:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				addr->max_life = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case OPTION:
			if (parse_option_decl(&oc, cfile)) {
				save_option(oc->option->universe,
					    addr->options, oc);
				option_cache_dereference(&oc, MDL);
			}
			no_semi = 1;
			break;

		      default:
			parse_warn(cfile, "Unexpected token.");
			skip_to_rbrace(cfile, 1);
			no_semi = 1;
			break;
		}

		if (!no_semi)
			parse_semi(cfile);

		token = next_token(&val, NULL, cfile);
		if (token == END_OF_FILE) {
			parse_warn(cfile, "Unexpected end of file.");
			break;
		}
	}

	return addr;
}
#endif /* DHCPv6 */

/* Parse an iaprefix {} structure. */
#ifdef DHCPv6
static struct dhc6_addr *
parse_client6_iaprefix_statement(struct parse *cfile)
{
	struct option_cache *oc = NULL;
	struct dhc6_addr *pref;
	const char *val;
	int token, no_semi;

	pref = dmalloc(sizeof(*pref), MDL);
	if (pref == NULL) {
		parse_warn(cfile, "Unable to allocate IAPREFIX state.");
		skip_to_semi(cfile);
		return NULL;
	}

	/* Get IP prefix. */
	if (!parse_ip6_prefix(cfile, &pref->address, &pref->plen)) {
		skip_to_semi(cfile);
		dfree(pref, MDL);
		return NULL;
	}

	token = next_token(NULL, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "Expecting open curly bracket.");
		skip_to_semi(cfile);
		dfree(pref, MDL);
		return NULL;
	}

	option_state_allocate(&pref->options, MDL);
	if (pref->options == NULL) {
		parse_warn(cfile, "Unable to allocate option state.");
		skip_to_semi(cfile);
		dfree(pref, MDL);
		return NULL;
	}

	token = next_token(&val, NULL, cfile);
	while (token != RBRACE) {
		no_semi = 0;

		switch (token) {
		      case STARTS:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				pref->starts = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case PREFERRED_LIFE:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				pref->preferred_life = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case MAX_LIFE:
			token = next_token(&val, NULL, cfile);
			if (token == NUMBER) {
				pref->max_life = atoi(val);
			} else {
				parse_warn(cfile, "Expecting a number.");
				skip_to_semi(cfile);
				no_semi = 1;
			}
			break;

		      case OPTION:
			if (parse_option_decl(&oc, cfile)) {
				save_option(oc->option->universe,
					    pref->options, oc);
				option_cache_dereference(&oc, MDL);
			}
			no_semi = 1;
			break;

		      default:
			parse_warn(cfile, "Unexpected token.");
			skip_to_rbrace(cfile, 1);
			no_semi = 1;
			break;
		}

		if (!no_semi)
			parse_semi(cfile);

		token = next_token(&val, NULL, cfile);
		if (token == END_OF_FILE) {
			parse_warn(cfile, "Unexpected end of file.");
			break;
		}
	}

	return pref;
}
#endif /* DHCPv6 */

void parse_string_list (cfile, lp, multiple)
	struct parse *cfile;
	struct string_list **lp;
	int multiple;
{
	int token;
	const char *val;
	struct string_list *cur, *tmp;

	/* Find the last medium in the media list. */
	if (*lp) {
		for (cur = *lp; cur -> next; cur = cur -> next)
			;
	} else {
		cur = (struct string_list *)0;
	}

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile, "Expecting media options.");
			skip_to_semi (cfile);
			return;
		}

		tmp = ((struct string_list *)
		       dmalloc (strlen (val) + sizeof (struct string_list),
				MDL));
		if (!tmp)
			log_fatal ("no memory for string list entry.");

		strcpy (tmp -> string, val);
		tmp -> next = (struct string_list *)0;

		/* Store this medium at the end of the media list. */
		if (cur)
			cur -> next = tmp;
		else
			*lp = tmp;
		cur = tmp;

		token = next_token (&val, (unsigned *)0, cfile);
	} while (multiple && token == COMMA);

	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
	}
}

void parse_reject_statement (cfile, config)
	struct parse *cfile;
	struct client_config *config;
{
	int token;
	const char *val;
	struct iaddrmatch match;
	struct iaddrmatchlist *list;
	int i;

	do {
		if (!parse_ip_addr_with_subnet (cfile, &match)) {
			/* no warn: parser will have reported what's wrong */
			skip_to_semi (cfile);
			return;
		}

		/* check mask is not all zeros (because that would
		 * reject EVERY address).  This check could be
		 * simplified if we assume that the mask *always*
		 * represents a prefix .. but perhaps it might be
		 * useful to have a mask which is not a proper prefix
		 * (perhaps for ipv6?).  The following is almost as
		 * efficient as inspection of match.mask.iabuf[0] when
		 * it IS a true prefix, and is more general when it is
		 * not.
		 */

		for (i=0 ; i < match.mask.len ; i++) {
		    if (match.mask.iabuf[i]) {
			break;
		    }
		}

		if (i == match.mask.len) {
		    /* oops we found all zeros */
		    parse_warn(cfile, "zero-length prefix is not permitted "
				      "for reject statement");
		    skip_to_semi(cfile);
		    return;
		} 

		list = dmalloc(sizeof(struct iaddrmatchlist), MDL);
		if (!list)
			log_fatal ("no memory for reject list!");

		list->match = match;
		list->next = config->reject_list;
		config->reject_list = list;

		token = next_token (&val, (unsigned *)0, cfile);
	} while (token == COMMA);

	if (token != SEMI) {
		parse_warn (cfile, "expecting semicolon.");
		skip_to_semi (cfile);
	}
}	

/* allow-deny-keyword :== BOOTP
   			| BOOTING
			| DYNAMIC_BOOTP
			| UNKNOWN_CLIENTS */

int parse_allow_deny (oc, cfile, flag)
	struct option_cache **oc;
	struct parse *cfile;
	int flag;
{
	parse_warn (cfile, "allow/deny/ignore not permitted here.");
	skip_to_semi (cfile);
	return 0;
}
