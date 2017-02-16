/*	$NetBSD: confpars.c,v 1.2 2014/07/12 12:09:38 spz Exp $	*/
/* confpars.c

   Parser for dhcpd config file... */

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
__RCSID("$NetBSD: confpars.c,v 1.2 2014/07/12 12:09:38 spz Exp $");

/*! \file server/confpars.c */

#include "dhcpd.h"

static unsigned char global_host_once = 1;

static int parse_binding_value(struct parse *cfile,
				struct binding_value *value);

#if defined (TRACING)
trace_type_t *trace_readconf_type;
trace_type_t *trace_readleases_type;

void parse_trace_setup ()
{
	trace_readconf_type = trace_type_register ("readconf", (void *)0,
						   trace_conf_input,
						   trace_conf_stop, MDL);
	trace_readleases_type = trace_type_register ("readleases", (void *)0,
						     trace_conf_input,
						     trace_conf_stop, MDL);
}
#endif

/* conf-file :== parameters declarations END_OF_FILE
   parameters :== <nil> | parameter | parameters parameter
   declarations :== <nil> | declaration | declarations declaration */

isc_result_t readconf ()
{
	isc_result_t res;

	res = read_conf_file (path_dhcpd_conf, root_group, ROOT_GROUP, 0);
#if defined(LDAP_CONFIGURATION)
	if (res != ISC_R_SUCCESS)
		return (res);

	return ldap_read_config ();
#else
	return (res);
#endif
}

isc_result_t read_conf_file (const char *filename, struct group *group,
			     int group_type, int leasep)
{
	int file;
	struct parse *cfile;
	isc_result_t status;
#if defined (TRACING)
	char *fbuf, *dbuf;
	off_t flen;
	int result;
	unsigned tflen, ulen;
	trace_type_t *ttype;

	if (leasep)
		ttype = trace_readleases_type;
	else
		ttype = trace_readconf_type;

	/* If we're in playback, we need to snarf the contents of the
	   named file out of the playback file rather than trying to
	   open and read it. */
	if (trace_playback ()) {
		dbuf = (char *)0;
		tflen = 0;
		status = trace_get_file (ttype, filename, &tflen, &dbuf);
		if (status != ISC_R_SUCCESS)
			return status;
		ulen = tflen;

		/* What we get back is filename\0contents, where contents is
		   terminated just by the length.  So we figure out the length
		   of the filename, and subtract that and the NUL from the
		   total length to get the length of the contents of the file.
		   We make fbuf a pointer to the contents of the file, and
		   leave dbuf as it is so we can free it later. */
		tflen = strlen (dbuf);
		ulen = ulen - tflen - 1;
		fbuf = dbuf + tflen + 1;
		goto memfile;
	}
#endif

	if ((file = open (filename, O_RDONLY)) < 0) {
		if (leasep) {
			log_error ("Can't open lease database %s: %m --",
				   path_dhcpd_db);
			log_error ("  check for failed database %s!",
				   "rewrite attempt");
			log_error ("Please read the dhcpd.leases manual%s",
				   " page if you");
			log_fatal ("don't know what to do about this.");
		} else {
			log_fatal ("Can't open %s: %m", filename);
		}
	}

	cfile = (struct parse *)0;
#if defined (TRACING)
	flen = lseek (file, (off_t)0, SEEK_END);
	if (flen < 0) {
	      boom:
		log_fatal ("Can't lseek on %s: %m", filename);
	}
	if (lseek (file, (off_t)0, SEEK_SET) < 0)
		goto boom;
	/* Can't handle files greater than 2^31-1. */
	if (flen > 0x7FFFFFFFUL)
		log_fatal ("%s: file is too long to buffer.", filename);
	ulen = flen;

	/* Allocate a buffer that will be what's written to the tracefile,
	   and also will be what we parse from. */
	tflen = strlen (filename);
	dbuf = dmalloc (ulen + tflen + 1, MDL);
	if (!dbuf)
		log_fatal ("No memory for %s (%d bytes)",
			   filename, ulen);

	/* Copy the name into the beginning, nul-terminated. */
	strcpy (dbuf, filename);

	/* Load the file in after the NUL. */
	fbuf = dbuf + tflen + 1;
	result = read (file, fbuf, ulen);
	if (result < 0)
		log_fatal ("Can't read in %s: %m", filename);
	if (result != ulen)
		log_fatal ("%s: short read of %d bytes instead of %d.",
			   filename, ulen, result);
	close (file);
      memfile:
	/* If we're recording, write out the filename and file contents. */
	if (trace_record ())
		trace_write_packet (ttype, ulen + tflen + 1, dbuf, MDL);
	status = new_parse(&cfile, -1, fbuf, ulen, filename, 0); /* XXX */
#else
	status = new_parse(&cfile, file, NULL, 0, filename, 0);
#endif
	if (status != ISC_R_SUCCESS || cfile == NULL)
		return status;

	if (leasep)
		status = lease_file_subparse (cfile);
	else
		status = conf_file_subparse (cfile, group, group_type);
	end_parse (&cfile);
#if defined (TRACING)
	dfree (dbuf, MDL);
#endif
	return status;
}

#if defined (TRACING)
void trace_conf_input (trace_type_t *ttype, unsigned len, char *data)
{
	char *fbuf;
	unsigned flen;
	unsigned tflen;
	struct parse *cfile = (struct parse *)0;
	static int postconf_initialized;
	static int leaseconf_initialized;
	isc_result_t status;
	
	/* Do what's done above, except that we don't have to read in the
	   data, because it's already been read for us. */
	tflen = strlen (data);
	flen = len - tflen - 1;
	fbuf = data + tflen + 1;

	/* If we're recording, write out the filename and file contents. */
	if (trace_record ())
		trace_write_packet (ttype, len, data, MDL);

	status = new_parse(&cfile, -1, fbuf, flen, data, 0);
	if (status == ISC_R_SUCCESS || cfile != NULL) {
		if (ttype == trace_readleases_type)
			lease_file_subparse (cfile);
		else
			conf_file_subparse (cfile, root_group, ROOT_GROUP);
		end_parse (&cfile);
	}

	/* Postconfiguration needs to be done after the config file
	   has been loaded. */
	if (!postconf_initialized && ttype == trace_readconf_type) {
		postconf_initialization (0);
		postconf_initialized = 1;
	}

	if (!leaseconf_initialized && ttype == trace_readleases_type) {
		db_startup (0);
		leaseconf_initialized = 1;
		postdb_startup ();
	}
}

void trace_conf_stop (trace_type_t *ttype) { }
#endif

/* conf-file :== parameters declarations END_OF_FILE
   parameters :== <nil> | parameter | parameters parameter
   declarations :== <nil> | declaration | declarations declaration */

isc_result_t conf_file_subparse (struct parse *cfile, struct group *group,
				 int group_type)
{
	const char *val;
	enum dhcp_token token;
	int declaration = 0;
	int status;

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE)
			break;
		declaration = parse_statement (cfile, group, group_type,
					       (struct host_decl *)0,
					       declaration);
	} while (1);
	skip_token(&val, (unsigned *)0, cfile);

	status = cfile->warnings_occurred ? DHCP_R_BADPARSE : ISC_R_SUCCESS;
	return status;
}

/* lease-file :== lease-declarations END_OF_FILE
   lease-statements :== <nil>
   		     | lease-declaration
		     | lease-declarations lease-declaration */

isc_result_t lease_file_subparse (struct parse *cfile)
{
	const char *val;
	enum dhcp_token token;
	isc_result_t status;

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == END_OF_FILE)
			break;
		if (token == LEASE) {
			struct lease *lease = (struct lease *)0;
			if (parse_lease_declaration (&lease, cfile)) {
				enter_lease (lease);
				lease_dereference (&lease, MDL);
			} else
				parse_warn (cfile,
					    "possibly corrupt lease file");
		} else if (token == IA_NA) {
			parse_ia_na_declaration(cfile);
		} else if (token == IA_TA) {
			parse_ia_ta_declaration(cfile);
		} else if (token == IA_PD) {
			parse_ia_pd_declaration(cfile);
		} else if (token == CLASS) {
			parse_class_declaration(0, cfile, root_group,
						CLASS_TYPE_CLASS);
		} else if (token == SUBCLASS) {
			parse_class_declaration(0, cfile, root_group,
						CLASS_TYPE_SUBCLASS);
		} else if (token == HOST) {
			parse_host_declaration (cfile, root_group);
		} else if (token == GROUP) {
			parse_group_declaration (cfile, root_group);
#if defined (FAILOVER_PROTOCOL)
		} else if (token == FAILOVER) {
			parse_failover_state_declaration
				(cfile, (dhcp_failover_state_t *)0);
#endif
#ifdef DHCPv6
		} else if (token == SERVER_DUID) {
			parse_server_duid(cfile);
#endif /* DHCPv6 */
		} else {
			log_error ("Corrupt lease file - possible data loss!");
			skip_to_semi (cfile);
		}

	} while (1);

	status = cfile->warnings_occurred ? DHCP_R_BADPARSE : ISC_R_SUCCESS;
	return status;
}

/* statement :== parameter | declaration

   parameter :== DEFAULT_LEASE_TIME lease_time
	       | MAX_LEASE_TIME lease_time
	       | DYNAMIC_BOOTP_LEASE_CUTOFF date
	       | DYNAMIC_BOOTP_LEASE_LENGTH lease_time
	       | BOOT_UNKNOWN_CLIENTS boolean
	       | ONE_LEASE_PER_CLIENT boolean
	       | GET_LEASE_HOSTNAMES boolean
	       | USE_HOST_DECL_NAME boolean
	       | NEXT_SERVER ip-addr-or-hostname SEMI
	       | option_parameter
	       | SERVER-IDENTIFIER ip-addr-or-hostname SEMI
	       | FILENAME string-parameter
	       | SERVER_NAME string-parameter
	       | hardware-parameter
	       | fixed-address-parameter
	       | ALLOW allow-deny-keyword
	       | DENY allow-deny-keyword
	       | USE_LEASE_ADDR_FOR_DEFAULT_ROUTE boolean
	       | AUTHORITATIVE
	       | NOT AUTHORITATIVE

   declaration :== host-declaration
		 | group-declaration
		 | shared-network-declaration
		 | subnet-declaration
		 | VENDOR_CLASS class-declaration
		 | USER_CLASS class-declaration
		 | RANGE address-range-declaration */

int parse_statement (cfile, group, type, host_decl, declaration)
	struct parse *cfile;
	struct group *group;
	int type;
	struct host_decl *host_decl;
	int declaration;
{
	enum dhcp_token token;
	const char *val;
	struct shared_network *share;
	char *n;
	struct hardware hardware;
	struct executable_statement *et, *ep;
	struct option *option = NULL;
	struct option_cache *cache;
	int lose;
	int known;
	isc_result_t status;
	unsigned code;

	token = peek_token (&val, (unsigned *)0, cfile);

	switch (token) {
	      case INCLUDE:
		skip_token(&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STRING) {
			parse_warn (cfile, "filename string expected.");
			skip_to_semi (cfile);
		} else {
			status = read_conf_file (val, group, type, 0);
			if (status != ISC_R_SUCCESS)
				parse_warn (cfile, "%s: bad parse.", val);
			parse_semi (cfile);
		}
		return 1;
		
	      case HOST:
		skip_token(&val, (unsigned *)0, cfile);
		if (type != HOST_DECL && type != CLASS_DECL) {
			if (global_host_once &&
			    (type == SUBNET_DECL || type == SHARED_NET_DECL)) {
				global_host_once = 0;
				log_error("WARNING: Host declarations are "
					  "global.  They are not limited to "
					  "the scope you declared them in.");
			}

			parse_host_declaration (cfile, group);
		} else {
			parse_warn (cfile,
				    "host declarations not allowed here.");
			skip_to_semi (cfile);
		}
		return 1;

	      case GROUP:
		skip_token(&val, (unsigned *)0, cfile);
		if (type != HOST_DECL && type != CLASS_DECL)
			parse_group_declaration (cfile, group);
		else {
			parse_warn (cfile,
				    "group declarations not allowed here.");
			skip_to_semi (cfile);
		}
		return 1;

	      case SHARED_NETWORK:
		skip_token(&val, (unsigned *)0, cfile);
		if (type == SHARED_NET_DECL ||
		    type == HOST_DECL ||
		    type == SUBNET_DECL ||
		    type == CLASS_DECL) {
			parse_warn (cfile, "shared-network parameters not %s.",
				    "allowed here");
			skip_to_semi (cfile);
			break;
		}

		parse_shared_net_declaration (cfile, group);
		return 1;

	      case SUBNET:
	      case SUBNET6:
		skip_token(&val, (unsigned *)0, cfile);
		if (type == HOST_DECL || type == SUBNET_DECL ||
		    type == CLASS_DECL) {
			parse_warn (cfile,
				    "subnet declarations not allowed here.");
			skip_to_semi (cfile);
			return 1;
		}

		/* If we're in a subnet declaration, just do the parse. */
		if (group->shared_network != NULL) {
			if (token == SUBNET) {
				parse_subnet_declaration(cfile,
							 group->shared_network);
			} else {
				parse_subnet6_declaration(cfile,
							 group->shared_network);
			}
			break;
		}

		/*
		 * Otherwise, cons up a fake shared network structure
		 * and populate it with the lone subnet...because the
		 * intention most likely is to refer to the entire link
		 * by shorthand, any configuration inside the subnet is
		 * actually placed in the shared-network's group.
		 */

		share = NULL;
		status = shared_network_allocate (&share, MDL);
		if (status != ISC_R_SUCCESS)
			log_fatal ("Can't allocate shared subnet: %s",
				   isc_result_totext (status));
		if (!clone_group (&share -> group, group, MDL))
			log_fatal ("Can't allocate group for shared net");
		shared_network_reference (&share -> group -> shared_network,
					  share, MDL);

		/*
		 * This is an implicit shared network, not explicit in
		 * the config.
		 */
		share->flags |= SHARED_IMPLICIT;

		if (token == SUBNET) {
			parse_subnet_declaration(cfile, share);
		} else {
			parse_subnet6_declaration(cfile, share);
		}

		/* share -> subnets is the subnet we just parsed. */
		if (share->subnets) {
			interface_reference(&share->interface,
					    share->subnets->interface,
					    MDL);

			/* Make the shared network name from network number. */
			if (token == SUBNET) {
				n = piaddrmask(&share->subnets->net,
					       &share->subnets->netmask);
			} else {
				n = piaddrcidr(&share->subnets->net,
					       share->subnets->prefix_len);
			}

			share->name = strdup(n);

			if (share->name == NULL)
				log_fatal("Out of memory allocating default "
					  "shared network name (\"%s\").", n);

			/* Copy the authoritative parameter from the subnet,
			   since there is no opportunity to declare it here. */
			share->group->authoritative =
				share->subnets->group->authoritative;
			enter_shared_network(share);
		}
		shared_network_dereference(&share, MDL);
		return 1;

	      case VENDOR_CLASS:
		skip_token(&val, (unsigned *)0, cfile);
		if (type == CLASS_DECL) {
			parse_warn (cfile,
				    "class declarations not allowed here.");
			skip_to_semi (cfile);
			break;
		}
		parse_class_declaration(NULL, cfile, group, CLASS_TYPE_VENDOR);
		return 1;

	      case USER_CLASS:
		skip_token(&val, (unsigned *)0, cfile);
		if (type == CLASS_DECL) {
			parse_warn (cfile,
				    "class declarations not allowed here.");
			skip_to_semi (cfile);
			break;
		}
		parse_class_declaration(NULL, cfile, group, CLASS_TYPE_USER);
		return 1;

	      case CLASS:
		skip_token(&val, (unsigned *)0, cfile);
		if (type == CLASS_DECL) {
			parse_warn (cfile,
				    "class declarations not allowed here.");
			skip_to_semi (cfile);
			break;
		}
		parse_class_declaration(NULL, cfile, group, CLASS_TYPE_CLASS);
		return 1;

	      case SUBCLASS:
		skip_token(&val, (unsigned *)0, cfile);
		if (type == CLASS_DECL) {
			parse_warn (cfile,
				    "class declarations not allowed here.");
			skip_to_semi (cfile);
			break;
		}
		parse_class_declaration(NULL, cfile, group,
					CLASS_TYPE_SUBCLASS);
		return 1;

	      case HARDWARE:
		skip_token(&val, (unsigned *)0, cfile);
		memset (&hardware, 0, sizeof hardware);
		if (host_decl && memcmp(&hardware, &(host_decl->interface),
					sizeof(hardware)) != 0) {
			parse_warn(cfile, "Host %s hardware address already "
					  "configured.", host_decl->name);
			break;
		}

		parse_hardware_param (cfile, &hardware);
		if (host_decl)
			host_decl -> interface = hardware;
		else
			parse_warn (cfile, "hardware address parameter %s",
				    "not allowed here.");
		break;

	      case FIXED_ADDR:
	      case FIXED_ADDR6:
		skip_token(&val, NULL, cfile);
		cache = NULL;
		if (parse_fixed_addr_param(&cache, cfile, token)) {
			if (host_decl) {
				if (host_decl->fixed_addr) {
					option_cache_dereference(&cache, MDL);
					parse_warn(cfile,
						   "Only one fixed address "
						   "declaration per host.");
				} else {
					host_decl->fixed_addr = cache;
				}
			} else {
				parse_warn(cfile,
					   "fixed-address parameter not "
					   "allowed here.");
				option_cache_dereference(&cache, MDL);
			}
		}
		break;

	      case POOL:
		skip_token(&val, (unsigned *)0, cfile);
		if (type == POOL_DECL) {
			parse_warn (cfile, "pool declared within pool.");
			skip_to_semi(cfile);
		} else if (type != SUBNET_DECL && type != SHARED_NET_DECL) {
			parse_warn (cfile, "pool declared outside of network");
			skip_to_semi(cfile);
		} else 
			parse_pool_statement (cfile, group, type);

		return declaration;

	      case RANGE:
		skip_token(&val, (unsigned *)0, cfile);
		if (type != SUBNET_DECL || !group -> subnet) {
			parse_warn (cfile,
				    "range declaration not allowed here.");
			skip_to_semi (cfile);
			return declaration;
		}
		parse_address_range (cfile, group, type, (struct pool *)0,
				     (struct lease **)0);
		return declaration;

#ifdef DHCPv6
	      case RANGE6:
		skip_token(NULL, NULL, cfile);
	        if ((type != SUBNET_DECL) || (group->subnet == NULL)) {
			parse_warn (cfile,
				    "range6 declaration not allowed here.");
			skip_to_semi(cfile);
			return declaration;
		}
	      	parse_address_range6(cfile, group, NULL);
		return declaration;

	      case PREFIX6:
		skip_token(NULL, NULL, cfile);
		if ((type != SUBNET_DECL) || (group->subnet == NULL)) {
			parse_warn (cfile,
				    "prefix6 declaration not allowed here.");
			skip_to_semi(cfile);
			return declaration;
		}
	      	parse_prefix6(cfile, group, NULL);
		return declaration;

	      case FIXED_PREFIX6:
		skip_token(&val, NULL, cfile);
		if (!host_decl) {
			parse_warn (cfile,
				    "fixed-prefix6 declaration not "
				    "allowed here.");
			skip_to_semi(cfile);
			break;
		}
		parse_fixed_prefix6(cfile, host_decl);
		break;

	      case POOL6:
		skip_token(&val, NULL, cfile);
		if (type == POOL_DECL) {
			parse_warn (cfile, "pool declared within pool.");
			skip_to_semi(cfile);
		} else if (type != SUBNET_DECL) {
			parse_warn (cfile, "pool declared outside of network");
			skip_to_semi(cfile);
		} else 
			parse_pool6_statement (cfile, group, type);

		return declaration;

#endif /* DHCPv6 */

	      case TOKEN_NOT:
		skip_token(&val, (unsigned *)0, cfile);
		token = next_token (&val, (unsigned *)0, cfile);
		switch (token) {
		      case AUTHORITATIVE:
			group -> authoritative = 0;
			goto authoritative;
		      default:
			parse_warn (cfile, "expecting assertion");
			skip_to_semi (cfile);
			break;
		}
		break;
	      case AUTHORITATIVE:
		skip_token(&val, (unsigned *)0, cfile);
		group -> authoritative = 1;
	      authoritative:
		if (type == HOST_DECL)
			parse_warn (cfile, "authority makes no sense here."); 
		parse_semi (cfile);
		break;

		/* "server-identifier" is a special hack, equivalent to
		   "option dhcp-server-identifier". */
	      case SERVER_IDENTIFIER:
		code = DHO_DHCP_SERVER_IDENTIFIER;
		if (!option_code_hash_lookup(&option, dhcp_universe.code_hash,
					     &code, 0, MDL))
			log_fatal("Server identifier not in hash (%s:%d).",
				  MDL);
		skip_token(&val, (unsigned *)0, cfile);
		goto finish_option;

	      case OPTION:
		skip_token(&val, (unsigned *)0, cfile);
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == SPACE) {
			if (type != ROOT_GROUP) {
				parse_warn (cfile,
					    "option space definitions %s",
					    "may not be scoped.");
				skip_to_semi (cfile);
				break;
			}
			parse_option_space_decl (cfile);
			return declaration;
		}

		known = 0;
		status = parse_option_name(cfile, 1, &known, &option);
		if (status == ISC_R_SUCCESS) {
			token = peek_token (&val, (unsigned *)0, cfile);
			if (token == CODE) {
				if (type != ROOT_GROUP) {
					parse_warn (cfile,
						    "option definitions%s",
						    " may not be scoped.");
					skip_to_semi (cfile);
					option_dereference(&option, MDL);
					break;
				}
				skip_token(&val, (unsigned *)0, cfile);

				/*
				 * If the option was known, remove it from the
				 * code and name hashes before redefining it.
				 */
				if (known) {
					option_name_hash_delete(
						option->universe->name_hash,
							option->name, 0, MDL);
					option_code_hash_delete(
						option->universe->code_hash,
							&option->code, 0, MDL);
				}

				parse_option_code_definition(cfile, option);
				option_dereference(&option, MDL);
				return declaration;
			}

			/* If this wasn't an option code definition, don't
			   allow an unknown option. */
			if (!known) {
				parse_warn (cfile, "unknown option %s.%s",
					    option -> universe -> name,
					    option -> name);
				skip_to_semi (cfile);
				option_dereference(&option, MDL);
				return declaration;
			}

		      finish_option:
			et = (struct executable_statement *)0;
			if (!parse_option_statement
				(&et, cfile, 1, option,
				 supersede_option_statement))
				return declaration;
			option_dereference(&option, MDL);
			goto insert_statement;
		} else
			return declaration;

		break;

	      case FAILOVER:
		if (type != ROOT_GROUP && type != SHARED_NET_DECL) {
			parse_warn (cfile, "failover peers may only be %s",
				    "defined in shared-network");
			log_error ("declarations and the outer scope.");
			skip_to_semi (cfile);
			break;
		}
		token = next_token (&val, (unsigned *)0, cfile);
#if defined (FAILOVER_PROTOCOL)
		parse_failover_peer (cfile, group, type);
#else
		parse_warn (cfile, "No failover support.");
		skip_to_semi (cfile);
#endif
		break;
			
#ifdef DHCPv6 
	      case SERVER_DUID:
		parse_server_duid_conf(cfile);
		break;
#endif /* DHCPv6 */

	      default:
		et = (struct executable_statement *)0;
		lose = 0;
		if (!parse_executable_statement (&et, cfile, &lose,
						 context_any)) {
			if (!lose) {
				if (declaration)
					parse_warn (cfile,
						    "expecting a declaration");
				else
					parse_warn (cfile,
						    "expecting a parameter %s",
						    "or declaration");
				skip_to_semi (cfile);
			}
			return declaration;
		}
		if (!et)
			return declaration;
	      insert_statement:
		if (group -> statements) {
			int multi = 0;

			/* If this set of statements is only referenced
			   by this group, just add the current statement
			   to the end of the chain. */
			for (ep = group -> statements; ep -> next;
			     ep = ep -> next)
				if (ep -> refcnt > 1) /* XXX */
					multi = 1;
			if (!multi) {
				executable_statement_reference (&ep -> next,
								et, MDL);
				executable_statement_dereference (&et, MDL);
				return declaration;
			}

			/* Otherwise, make a parent chain, and put the
			   current group statements first and the new
			   statement in the next pointer. */
			ep = (struct executable_statement *)0;
			if (!executable_statement_allocate (&ep, MDL))
				log_fatal ("No memory for statements.");
			ep -> op = statements_statement;
			executable_statement_reference (&ep -> data.statements,
							group -> statements,
							MDL);
			executable_statement_reference (&ep -> next, et, MDL);
			executable_statement_dereference (&group -> statements,
							  MDL);
			executable_statement_reference (&group -> statements,
							ep, MDL);
			executable_statement_dereference (&ep, MDL);
		} else {
			executable_statement_reference (&group -> statements,
							et, MDL);
		}
		executable_statement_dereference (&et, MDL);
		return declaration;
	}

	return 0;
}

#if defined (FAILOVER_PROTOCOL)
void parse_failover_peer (cfile, group, type)
	struct parse *cfile;
	struct group *group;
	int type;
{
	enum dhcp_token token;
	const char *val;
	dhcp_failover_state_t *peer;
	u_int32_t *tp;
	char *name;
	u_int32_t split;
	u_int8_t hba [32];
	unsigned hba_len = sizeof hba;
	int i;
	struct expression *expr;
	isc_result_t status;
	dhcp_failover_config_t *cp;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != PEER) {
		parse_warn (cfile, "expecting \"peer\"");
		skip_to_semi (cfile);
		return;
	}

	token = next_token (&val, (unsigned *)0, cfile);
	if (is_identifier (token) || token == STRING) {
		name = dmalloc (strlen (val) + 1, MDL);
		if (!name)
			log_fatal ("no memory for peer name %s", name);
		strcpy (name, val);
	} else {
		parse_warn (cfile, "expecting failover peer name.");
		skip_to_semi (cfile);
		return;
	}

	/* See if there's a peer declaration by this name. */
	peer = (dhcp_failover_state_t *)0;
	find_failover_peer (&peer, name, MDL);

	token = next_token (&val, (unsigned *)0, cfile);
	if (token == SEMI) {
		dfree (name, MDL);
		if (type != SHARED_NET_DECL)
			parse_warn (cfile, "failover peer reference not %s",
				    "in shared-network declaration");
		else {
			if (!peer) {
				parse_warn (cfile, "reference to unknown%s%s",
					    " failover peer ", name);
				return;
			}
			dhcp_failover_state_reference
				(&group -> shared_network -> failover_peer,
				 peer, MDL);
		}
		dhcp_failover_state_dereference (&peer, MDL);
		return;
	} else if (token == STATE) {
		if (!peer) {
			parse_warn (cfile, "state declaration for unknown%s%s",
				    " failover peer ", name);
			return;
		}
		parse_failover_state_declaration (cfile, peer);
		dhcp_failover_state_dereference (&peer, MDL);
		return;
	} else if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace");
		skip_to_semi (cfile);
	}

	/* Make sure this isn't a redeclaration. */
	if (peer) {
		parse_warn (cfile, "redeclaration of failover peer %s", name);
		skip_to_rbrace (cfile, 1);
		dhcp_failover_state_dereference (&peer, MDL);
		return;
	}

	status = dhcp_failover_state_allocate (&peer, MDL);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't allocate failover peer %s: %s",
			   name, isc_result_totext (status));

	/* Save the name. */
	peer -> name = name;

	do {
		cp = &peer -> me;
	      peer:
		token = next_token (&val, (unsigned *)0, cfile);
		switch (token) {
		      case RBRACE:
			break;

		      case PRIMARY:
			peer -> i_am = primary;
			break;

		      case SECONDARY:
			peer -> i_am = secondary;
			if (peer -> hba)
				parse_warn (cfile,
					    "secondary may not define %s",
					    "load balance settings.");
			break;

		      case PEER:
			cp = &peer -> partner;
			goto peer;

		      case ADDRESS:
			expr = (struct expression *)0;
			if (!parse_ip_addr_or_hostname (&expr, cfile, 0)) {
				skip_to_rbrace (cfile, 1);
				dhcp_failover_state_dereference (&peer, MDL);
				return;
			}
			option_cache (&cp -> address,
				      (struct data_string *)0, expr,
				      (struct option *)0, MDL);
			expression_dereference (&expr, MDL);
			break;

		      case PORT:
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != NUMBER) {
				parse_warn (cfile, "expecting number");
				skip_to_rbrace (cfile, 1);
			}
			cp -> port = atoi (val);
			break;

		      case MAX_LEASE_MISBALANCE:
			tp = &peer->max_lease_misbalance;
			goto parse_idle;

		      case MAX_LEASE_OWNERSHIP:
			tp = &peer->max_lease_ownership;
			goto parse_idle;

		      case MAX_BALANCE:
			tp = &peer->max_balance;
			goto parse_idle;

		      case MIN_BALANCE:
			tp = &peer->min_balance;
			goto parse_idle;

		      case AUTO_PARTNER_DOWN:
			tp = &peer->auto_partner_down;
			goto parse_idle;

		      case MAX_RESPONSE_DELAY:
			tp = &cp -> max_response_delay;
		      parse_idle:
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != NUMBER) {
				parse_warn (cfile, "expecting number.");
				skip_to_rbrace (cfile, 1);
				dhcp_failover_state_dereference (&peer, MDL);
				return;
			}
			*tp = atoi (val);
			break;

		      case MAX_UNACKED_UPDATES:
			tp = &cp -> max_flying_updates;
			goto parse_idle;

		      case MCLT:
			tp = &peer -> mclt;
			goto parse_idle;

		      case HBA:
			hba_len = 32;
			if (peer -> i_am == secondary)
				parse_warn (cfile,
					    "secondary may not define %s",
					    "load balance settings.");
			if (!parse_numeric_aggregate (cfile, hba, &hba_len,
						      COLON, 16, 8)) {
				skip_to_rbrace (cfile, 1);
				dhcp_failover_state_dereference (&peer, MDL);
				return;
			}
			if (hba_len != 32) {
				parse_warn (cfile,
					    "HBA must be exactly 32 bytes.");
				break;
			}
		      make_hba:
			peer -> hba = dmalloc (32, MDL);
			if (!peer -> hba) {
				dfree (peer -> name, MDL);
				dfree (peer, MDL);
			}
			memcpy (peer -> hba, hba, 32);
			break;

		      case SPLIT:
			token = next_token (&val, (unsigned *)0, cfile);
			if (peer -> i_am == secondary)
				parse_warn (cfile,
					    "secondary may not define %s",
					    "load balance settings.");
			if (token != NUMBER) {
				parse_warn (cfile, "expecting number");
				skip_to_rbrace (cfile, 1);
				dhcp_failover_state_dereference (&peer, MDL);
				return;
			}
			split = atoi (val);
			if (split > 255) {
				parse_warn (cfile, "split must be < 256");
			} else {
				memset (hba, 0, sizeof hba);
				for (i = 0; i < split; i++) {
					if (i < split)
						hba [i / 8] |= (1 << (i & 7));
				}
				goto make_hba;
			}
			break;
			
		      case LOAD:
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != BALANCE) {
				parse_warn (cfile, "expecting 'balance'");
			      badload:
				skip_to_rbrace (cfile, 1);
				break;
			}
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != TOKEN_MAX) {
				parse_warn (cfile, "expecting 'max'");
				goto badload;
			}
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != SECONDS) {
				parse_warn (cfile, "expecting 'secs'");
				goto badload;
			}
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != NUMBER) {
				parse_warn (cfile, "expecting number");
				goto badload;
			}
			peer -> load_balance_max_secs = atoi (val);
			break;
			
		      default:
			parse_warn (cfile,
				    "invalid statement in peer declaration");
			skip_to_rbrace (cfile, 1);
			dhcp_failover_state_dereference (&peer, MDL);
			return;
		}
		if (token != RBRACE && !parse_semi (cfile)) {
			skip_to_rbrace (cfile, 1);
			dhcp_failover_state_dereference (&peer, MDL);
			return;
		}
	} while (token != RBRACE);

	/* me.address can be null; the failover link initiate code tries to
	 * derive a reasonable address to use.
	 */
	if (!peer -> partner.address)
		parse_warn (cfile, "peer address may not be omitted");

	if (!peer->me.port)
		peer->me.port = DEFAULT_FAILOVER_PORT;
	if (!peer->partner.port)
		peer->partner.port = DEFAULT_FAILOVER_PORT;

	if (peer -> i_am == primary) {
	    if (!peer -> hba) {
		parse_warn (cfile,
			    "primary failover server must have hba or split.");
	    } else if (!peer -> mclt) {
		parse_warn (cfile,
			    "primary failover server must have mclt.");
	    }
	}

	if (!peer->max_lease_misbalance)
		peer->max_lease_misbalance = DEFAULT_MAX_LEASE_MISBALANCE;
	if (!peer->max_lease_ownership)
		peer->max_lease_ownership = DEFAULT_MAX_LEASE_OWNERSHIP;
	if (!peer->max_balance)
		peer->max_balance = DEFAULT_MAX_BALANCE_TIME;
	if (!peer->min_balance)
		peer->min_balance = DEFAULT_MIN_BALANCE_TIME;
	if (!peer->me.max_flying_updates)
		peer->me.max_flying_updates = DEFAULT_MAX_FLYING_UPDATES;
	if (!peer->me.max_response_delay)
		peer->me.max_response_delay = DEFAULT_MAX_RESPONSE_DELAY;

	if (type == SHARED_NET_DECL)
		group->shared_network->failover_peer = peer;

	/* Set the initial state. */
	if (peer -> i_am == primary) {
		peer -> me.state = recover;
		peer -> me.stos = cur_time;
		peer -> partner.state = unknown_state;
		peer -> partner.stos = cur_time;
	} else {
		peer -> me.state = recover;
		peer -> me.stos = cur_time;
		peer -> partner.state = unknown_state;
		peer -> partner.stos = cur_time;
	}

	status = enter_failover_peer (peer);
	if (status != ISC_R_SUCCESS)
		parse_warn (cfile, "failover peer %s: %s",
			    peer -> name, isc_result_totext (status));
	dhcp_failover_state_dereference (&peer, MDL);
}

void parse_failover_state_declaration (struct parse *cfile,
				       dhcp_failover_state_t *peer)
{
	enum dhcp_token token;
	const char *val;
	char *name;
	dhcp_failover_state_t *state;
	dhcp_failover_config_t *cp;

	if (!peer) {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token != PEER) {
			parse_warn (cfile, "expecting \"peer\"");
			skip_to_semi (cfile);
			return;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (is_identifier (token) || token == STRING) {
			name = dmalloc (strlen (val) + 1, MDL);
			if (!name)
				log_fatal ("failover peer name %s: no memory",
					   name);
			strcpy (name, val);
		} else {
			parse_warn (cfile, "expecting failover peer name.");
			skip_to_semi (cfile);
			return;
		}

		/* See if there's a peer declaration by this name. */
		state = (dhcp_failover_state_t *)0;
		find_failover_peer (&state, name, MDL);
		if (!state) {
			parse_warn (cfile, "unknown failover peer: %s", name);
			skip_to_semi (cfile);
			return;
		}

		token = next_token (&val, (unsigned *)0, cfile);
		if (token != STATE) {
			parse_warn (cfile, "expecting 'state'");
			if (token != SEMI)
				skip_to_semi (cfile);
			return;
		}
	} else {
		state = (dhcp_failover_state_t *)0;
		dhcp_failover_state_reference (&state, peer, MDL);
	}
	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace");
		if (token != SEMI)
			skip_to_semi (cfile);
		dhcp_failover_state_dereference (&state, MDL);
		return;
	}
	do {
		token = next_token (&val, (unsigned *)0, cfile);
		switch (token) {
		      case RBRACE:
			break;
		      case MY:
			cp = &state -> me;
		      do_state:
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != STATE) {
				parse_warn (cfile, "expecting 'state'");
				goto bogus;
			}
			parse_failover_state (cfile,
					      &cp -> state, &cp -> stos);
			break;

		      case PARTNER:
			cp = &state -> partner;
			goto do_state;

		      case MCLT:
			if (state -> i_am == primary) {
				parse_warn (cfile,
					    "mclt not valid for primary");
				goto bogus;
			}
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != NUMBER) {
				parse_warn (cfile, "expecting a number.");
				goto bogus;
			}
			state -> mclt = atoi (val);
			parse_semi (cfile);
			break;
			
		      default:
			parse_warn (cfile, "expecting state setting.");
		      bogus:
			skip_to_rbrace (cfile, 1);	
			dhcp_failover_state_dereference (&state, MDL);
			return;
		}
	} while (token != RBRACE);
	dhcp_failover_state_dereference (&state, MDL);
}

void parse_failover_state (cfile, state, stos)
	struct parse *cfile;
	enum failover_state *state;
	TIME *stos;
{
	enum dhcp_token token;
	const char *val;
	enum failover_state state_in;
	TIME stos_in;

	token = next_token (&val, (unsigned *)0, cfile);
	switch (token) {
	      case UNKNOWN_STATE:
		state_in = unknown_state;
		break;

	      case PARTNER_DOWN:
		state_in = partner_down;
		break;

	      case NORMAL:
		state_in = normal;
		break;

	      case COMMUNICATIONS_INTERRUPTED:
		state_in = communications_interrupted;
		break;

	      case CONFLICT_DONE:
		state_in = conflict_done;
		break;

	      case RESOLUTION_INTERRUPTED:
		state_in = resolution_interrupted;
		break;

	      case POTENTIAL_CONFLICT:
		state_in = potential_conflict;
		break;

	      case RECOVER:
		state_in = recover;
		break;
		
	      case RECOVER_WAIT:
		state_in = recover_wait;
		break;
		
	      case RECOVER_DONE:
		state_in = recover_done;
		break;
		
	      case SHUTDOWN:
		state_in = shut_down;
		break;
		
	      case PAUSED:
		state_in = paused;
		break;
		
	      case STARTUP:
		state_in = startup;
		break;

	      default:
		parse_warn (cfile, "unknown failover state");
		skip_to_semi (cfile);
		return;
	}

	token = next_token (&val, (unsigned *)0, cfile);
	if (token == SEMI) {
		stos_in = cur_time;
	} else {
		if (token != AT) {
			parse_warn (cfile, "expecting \"at\"");
			skip_to_semi (cfile);
			return;
		}
		
		stos_in = parse_date (cfile);
		if (!stos_in)
			return;
	}

	/* Now that we've apparently gotten a clean parse, we
	   can trust that this is a state that was fully committed to
	   disk, so we can install it. */
	*stos = stos_in;
	*state = state_in;
}
#endif /* defined (FAILOVER_PROTOCOL) */

/*! 
 * 
 * \brief Parse allow and deny statements
 *
 * This function handles the common processing code for permit and deny
 * statements in the parse_pool_statement and parse_pool6_statement functions.
 * It reads in the configuration and constructs a new permit structure that it
 * attachs to the permit_head passed in from the caller.
 * 
 * The allow or deny token should already be consumed, this function expects
 * one of the following:
 *   known-clients;
 *   unknown-clients;
 *   known clients;
 *   unknown clients;
 *   authenticated clients;
 *   unauthenticated clients;
 *   all clients;
 *   dynamic bootp clients;
 *   members of <class name>;
 *   after <date>;
 *
 * \param[in] cfile       = the configuration file being parsed
 * \param[in] permit_head = the head of the permit list (permit or prohibit)
 *			    to which to attach the newly created  permit structure
 * \param[in] is_allow    = 1 if this is being invoked for an allow statement
 *			  = 0 if this is being invoked for a deny statement
 * \param[in] valid_from   = pointers to the time values from the enclosing pool
 * \param[in] valid_until    or pond structure. One of them will be filled in if
 *			     the configuration includes an "after" clause
 */

static void get_permit(struct parse *cfile, struct permit **permit_head,
	        int is_allow, TIME *valid_from, TIME *valid_until)
{
	enum dhcp_token token;
	struct permit *permit;
	const char *val;
	int need_clients = 1;
	TIME t;

	/* Create our permit structure */
	permit = new_permit(MDL);
	if (!permit)
		log_fatal ("no memory for permit");

	token = next_token(&val, NULL, cfile);
	switch (token) {
	      case UNKNOWN:
		permit->type = permit_unknown_clients;
		break;
				
	      case KNOWN_CLIENTS:
		need_clients = 0;
		permit->type = permit_known_clients;
		break;

	      case UNKNOWN_CLIENTS:
		need_clients = 0;
		permit->type = permit_unknown_clients;
		break;

	      case KNOWN:
		permit->type = permit_known_clients;
		break;
				
	      case AUTHENTICATED:
		permit->type = permit_authenticated_clients;
		break;
				
	      case UNAUTHENTICATED:
		permit->type = permit_unauthenticated_clients;
		break;

	      case ALL:
		permit->type = permit_all_clients;
		break;
				
	      case DYNAMIC:
		permit->type = permit_dynamic_bootp_clients;
		if (next_token (&val, NULL, cfile) != TOKEN_BOOTP) {
			parse_warn (cfile, "expecting \"bootp\"");
			skip_to_semi (cfile);
			free_permit (permit, MDL);
			return;
		}
		break;

	      case MEMBERS:
		need_clients = 0;
		if (next_token (&val, NULL, cfile) != OF) {
			parse_warn (cfile, "expecting \"of\"");
			skip_to_semi (cfile);
			free_permit (permit, MDL);
			return;
		}
		if (next_token (&val, NULL, cfile) != STRING) {
			parse_warn (cfile, "expecting class name.");
			skip_to_semi (cfile);
			free_permit (permit, MDL);
			return;
		}
		permit->type = permit_class;
		permit->class = NULL;
		find_class(&permit->class, val, MDL);
		if (!permit->class)
			parse_warn(cfile, "no such class: %s", val);
		break;

	      case AFTER:
		need_clients = 0;
		if (*valid_from || *valid_until) {
			parse_warn(cfile, "duplicate \"after\" clause.");
			skip_to_semi(cfile);
			free_permit(permit, MDL);
			return;
		}
		t = parse_date_core(cfile);
		permit->type = permit_after;
		permit->after = t;
		if (is_allow) {
			*valid_from = t;
		} else {
			*valid_until = t;
		}
		break;

	      default:
		parse_warn (cfile, "expecting permit type.");
		skip_to_semi (cfile);
		free_permit (permit, MDL);
		return;
	}

	/*
	 * The need_clients flag is set if we are expecting the
	 * CLIENTS token
	 */
	if ((need_clients != 0)  &&
	    (next_token (&val, NULL, cfile) != CLIENTS)) {
		parse_warn (cfile, "expecting \"clients\"");
		skip_to_semi (cfile);
		free_permit (permit, MDL);
		return;
	}

	while (*permit_head)
		permit_head = &((*permit_head)->next);
	*permit_head = permit;
	parse_semi (cfile);

	return;
}

/* Permit_list_match returns 1 if every element of the permit list in lhs
   also appears in rhs.   Note that this doesn't by itself mean that the
   two lists are equal - to check for equality, permit_list_match has to
   return 1 with (list1, list2) and with (list2, list1). */

int permit_list_match (struct permit *lhs, struct permit *rhs)
{
	struct permit *plp, *prp;
	int matched;

	if (!lhs)
		return 1;
	if (!rhs)
		return 0;
	for (plp = lhs; plp; plp = plp -> next) {
		matched = 0;
		for (prp = rhs; prp; prp = prp -> next) {
			if (prp -> type == plp -> type &&
			    (prp -> type != permit_class ||
			     prp -> class == plp -> class)) {
				matched = 1;
				break;
			}
		}
		if (!matched)
			return 0;
	}
	return 1;
}

/*!
 *
 * \brief Parse a pool statement
 *
 * Pool statements are used to group declarations and permit & deny information
 * with a specific address range.  They must be declared within a shared network
 * or subnet and there may be multiple pools withing a shared network or subnet.
 * Each pool may have a different set of permit or deny options.
 *
 * \param[in] cfile = the configuration file being parsed
 * \param[in] group = the group structure for this pool
 * \param[in] type  = the type of the enclosing statement.  This must be
 *		      SHARED_NET_DECL or SUBNET_DECL for this function.
 *
 * \return
 * void - This function either parses the statement and updates the structures
 *        or it generates an error message and possible halts the program if
 *        it encounters a problem.
 */
void parse_pool_statement (cfile, group, type)
	struct parse *cfile;
	struct group *group;
	int type;
{
	enum dhcp_token token;
	const char *val;
	int done = 0;
	struct pool *pool, **p, *pp;
	int declaration = 0;
	isc_result_t status;
	struct lease *lpchain = NULL, *lp;

	pool = NULL;
	status = pool_allocate(&pool, MDL);
	if (status != ISC_R_SUCCESS)
		log_fatal ("no memory for pool: %s",
			   isc_result_totext (status));

	if (type == SUBNET_DECL)
		shared_network_reference(&pool->shared_network,
					 group->subnet->shared_network,
					 MDL);
	else if (type == SHARED_NET_DECL)
		shared_network_reference(&pool->shared_network,
					 group->shared_network, MDL);
	else {
		parse_warn(cfile, "Dynamic pools are only valid inside "
				  "subnet or shared-network statements.");
		skip_to_semi(cfile);
		return;
	}

	if (pool->shared_network == NULL ||
            !clone_group(&pool->group, pool->shared_network->group, MDL))
		log_fatal("can't clone pool group.");

#if defined (FAILOVER_PROTOCOL)
	/* Inherit the failover peer from the shared network. */
	if (pool->shared_network->failover_peer)
	    dhcp_failover_state_reference
		    (&pool->failover_peer, 
		     pool->shared_network->failover_peer, MDL);
#endif

	if (!parse_lbrace(cfile)) {
		pool_dereference(&pool, MDL);
		return;
	}

	do {
		token = peek_token(&val, NULL, cfile);
		switch (token) {
		      case TOKEN_NO:
			skip_token(&val, NULL, cfile);
			token = next_token(&val, NULL, cfile);
			if (token != FAILOVER ||
			    (token = next_token(&val, NULL, cfile)) != PEER) {
				parse_warn(cfile,
					   "expecting \"failover peer\".");
				skip_to_semi(cfile);
				continue;
			}
#if defined (FAILOVER_PROTOCOL)
			if (pool->failover_peer)
				dhcp_failover_state_dereference
					(&pool->failover_peer, MDL);
#endif
			break;
				
#if defined (FAILOVER_PROTOCOL)
		      case FAILOVER:
			skip_token(&val, NULL, cfile);
			token = next_token (&val, NULL, cfile);
			if (token != PEER) {
				parse_warn(cfile, "expecting 'peer'.");
				skip_to_semi(cfile);
				break;
			}
			token = next_token(&val, NULL, cfile);
			if (token != STRING) {
				parse_warn(cfile, "expecting string.");
				skip_to_semi(cfile);
				break;
			}
			if (pool->failover_peer)
				dhcp_failover_state_dereference
					(&pool->failover_peer, MDL);
			status = find_failover_peer(&pool->failover_peer,
						    val, MDL);
			if (status != ISC_R_SUCCESS)
				parse_warn(cfile,
					   "failover peer %s: %s", val,
					   isc_result_totext (status));
			else
				pool->failover_peer->pool_count++;
			parse_semi(cfile);
			break;
#endif

		      case RANGE:
			skip_token(&val, NULL, cfile);
			parse_address_range (cfile, group, type,
					     pool, &lpchain);
			break;
		      case ALLOW:
			skip_token(&val, NULL, cfile);
			get_permit(cfile, &pool->permit_list, 1,
				   &pool->valid_from, &pool->valid_until);
			break;

		      case DENY:
			skip_token(&val, NULL, cfile);
			get_permit(cfile, &pool->prohibit_list, 0,
				   &pool->valid_from, &pool->valid_until);
			break;
			
		      case RBRACE:
			skip_token(&val, NULL, cfile);
			done = 1;
			break;

		      case END_OF_FILE:
			/*
			 * We can get to END_OF_FILE if, for instance,
			 * the parse_statement() reads all available tokens
			 * and leaves us at the end.
			 */
			parse_warn(cfile, "unexpected end of file");
			goto cleanup;

		      default:
			declaration = parse_statement(cfile, pool->group,
						      POOL_DECL, NULL,
						       declaration);
			break;
		}
	} while (!done);

	/* See if there's already a pool into which we can merge this one. */
	for (pp = pool->shared_network->pools; pp; pp = pp->next) {
		if (pp->group->statements != pool->group->statements)
			continue;
#if defined (FAILOVER_PROTOCOL)
		if (pool->failover_peer != pp->failover_peer)
			continue;
#endif
		if (!permit_list_match(pp->permit_list,
				       pool->permit_list) ||
		    !permit_list_match(pool->permit_list,
				       pp->permit_list) ||
		    !permit_list_match(pp->prohibit_list,
				       pool->prohibit_list) ||
		    !permit_list_match(pool->prohibit_list,
				       pp->prohibit_list))
			continue;

		/* Okay, we can merge these two pools.    All we have to
		   do is fix up the leases, which all point to their pool. */
		for (lp = lpchain; lp; lp = lp->next) {
			pool_dereference(&lp->pool, MDL);
			pool_reference(&lp->pool, pp, MDL);
		}
		break;
	}

	/* If we didn't succeed in merging this pool into another, put
	   it on the list. */
	if (!pp) {
		p = &pool->shared_network->pools;
		for (; *p; p = &((*p)->next))
			;
		pool_reference(p, pool, MDL);
	}

	/* Don't allow a pool declaration with no addresses, since it is
	   probably a configuration error. */
	if (!lpchain) {
		parse_warn(cfile, "Pool declaration with no address range.");
		log_error("Pool declarations must always contain at least");
		log_error("one range statement.");
	}

cleanup:
	/* Dereference the lease chain. */
	lp = NULL;
	while (lpchain) {
		lease_reference(&lp, lpchain, MDL);
		lease_dereference(&lpchain, MDL);
		if (lp->next) {
			lease_reference(&lpchain, lp->next, MDL);
			lease_dereference(&lp->next, MDL);
			lease_dereference(&lp, MDL);
		}
	}
	pool_dereference(&pool, MDL);
}

/* Expect a left brace; if there isn't one, skip over the rest of the
   statement and return zero; otherwise, return 1. */

int parse_lbrace (cfile)
	struct parse *cfile;
{
	enum dhcp_token token;
	const char *val;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != LBRACE) {
		parse_warn (cfile, "expecting left brace.");
		skip_to_semi (cfile);
		return 0;
	}
	return 1;
}


/* host-declaration :== hostname RBRACE parameters declarations LBRACE */

void parse_host_declaration (cfile, group)
	struct parse *cfile;
	struct group *group;
{
	const char *val;
	enum dhcp_token token;
	struct host_decl *host;
	char *name;
	int declaration = 0;
	int dynamicp = 0;
	int deleted = 0;
	isc_result_t status;
	int known;
	struct option *option;
	struct expression *expr = NULL;

	name = parse_host_name (cfile);
	if (!name) {
		parse_warn (cfile, "expecting a name for host declaration.");
		skip_to_semi (cfile);
		return;
	}

	host = (struct host_decl *)0;
	status = host_allocate (&host, MDL);
	if (status != ISC_R_SUCCESS)
		log_fatal ("can't allocate host decl struct %s: %s",
			   name, isc_result_totext (status));
	host -> name = name;
	if (!clone_group (&host -> group, group, MDL)) {
		log_fatal ("can't clone group for host %s", name);
	      boom:
		host_dereference (&host, MDL);
		return;
	}

	if (!parse_lbrace (cfile))
		goto boom;

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == RBRACE) {
			skip_token(&val, (unsigned *)0, cfile);
			break;
		}
		if (token == END_OF_FILE) {
			skip_token(&val, (unsigned *)0, cfile);
			parse_warn (cfile, "unexpected end of file");
			break;
		}
		/* If the host declaration was created by the server,
		   remember to save it. */
		if (token == DYNAMIC) {
			dynamicp = 1;
			skip_token(&val, (unsigned *)0, cfile);
			if (!parse_semi (cfile))
				break;
			continue;
		}
		/* If the host declaration was created by the server,
		   remember to save it. */
		if (token == TOKEN_DELETED) {
			deleted = 1;
			skip_token(&val, (unsigned *)0, cfile);
			if (!parse_semi (cfile))
				break;
			continue;
		}

		if (token == GROUP) {
			struct group_object *go;
			skip_token(&val, (unsigned *)0, cfile);
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != STRING && !is_identifier (token)) {
				parse_warn (cfile,
					    "expecting string or identifier.");
				skip_to_rbrace (cfile, 1);
				break;
			}
			go = (struct group_object *)0;
			if (!group_hash_lookup (&go, group_name_hash,
						val, strlen (val), MDL)) {
			    parse_warn (cfile, "unknown group %s in host %s",
					val, host -> name);
			} else {
				if (host -> named_group)
					group_object_dereference
						(&host -> named_group, MDL);
				group_object_reference (&host -> named_group,
							go, MDL);
				group_object_dereference (&go, MDL);
			}
			if (!parse_semi (cfile))
				break;
			continue;
		}

		if (token == UID) {
			const char *s;
			unsigned char *t = 0;
			unsigned len;

			skip_token(&val, (unsigned *)0, cfile);
			data_string_forget (&host -> client_identifier, MDL);

			if (host->client_identifier.len != 0) {
				parse_warn(cfile, "Host %s already has a "
						  "client identifier.",
					   host->name);
				break;
			}

			/* See if it's a string or a cshl. */
			token = peek_token (&val, (unsigned *)0, cfile);
			if (token == STRING) {
				skip_token(&val, &len, cfile);
				s = val;
				host -> client_identifier.terminated = 1;
			} else {
				len = 0;
				t = parse_numeric_aggregate
					(cfile,
					 (unsigned char *)0, &len, ':', 16, 8);
				if (!t) {
					parse_warn (cfile,
						    "expecting hex list.");
					skip_to_semi (cfile);
				}
				s = (const char *)t;
			}
			if (!buffer_allocate
			    (&host -> client_identifier.buffer,
			     len + host -> client_identifier.terminated, MDL))
				log_fatal ("no memory for uid for host %s.",
					   host -> name);
			host -> client_identifier.data =
				host -> client_identifier.buffer -> data;
			host -> client_identifier.len = len;
			memcpy (host -> client_identifier.buffer -> data, s,
				len + host -> client_identifier.terminated);
			if (t)
				dfree (t, MDL);

			if (!parse_semi (cfile))
				break;
			continue;
		}

		if (token == HOST_IDENTIFIER) {
			if (host->host_id_option != NULL) {
				parse_warn(cfile,
					   "only one host-identifier allowed "
					   "per host");
				skip_to_rbrace(cfile, 1);
				break;
			}
	      		skip_token(&val, NULL, cfile);
			token = next_token(&val, NULL, cfile);
			if (token == V6RELOPT) {
				token = next_token(&val, NULL, cfile);				
				if (token != NUMBER) {
					parse_warn(cfile,
						   "host-identifier v6relopt "
						   "must have a number");
					skip_to_rbrace(cfile, 1);
					break;
				}
				host->relays = atoi(val);
				if (host->relays < 0) {
					parse_warn(cfile,
						   "host-identifier v6relopt "
						   "must have a number >= 0");
					skip_to_rbrace(cfile, 1);
					break;
				}
			} else if (token != OPTION) {
				parse_warn(cfile, 
					   "host-identifier must be an option"
					   " or v6relopt");
				skip_to_rbrace(cfile, 1);
				break;
			}
			known = 0;
			option = NULL;
			status = parse_option_name(cfile, 1, &known, &option);
			if ((status != ISC_R_SUCCESS) || (option == NULL)) {
				break;
			}
			if (!known) {
				parse_warn(cfile, "unknown option %s.%s",
					   option->universe->name, 
					   option->name);
				skip_to_rbrace(cfile, 1);
				break;
			}

                        if (! parse_option_data(&expr, cfile, 1, option)) {
		        	skip_to_rbrace(cfile, 1);
		        	option_dereference(&option, MDL);
		        	break;
                        }
                        
			if (!parse_semi(cfile)) {
				skip_to_rbrace(cfile, 1);
				expression_dereference(&expr, MDL);
				option_dereference(&option, MDL);
				break;
			}

			option_reference(&host->host_id_option, option, MDL);
			option_dereference(&option, MDL);
			data_string_copy(&host->host_id, 
					 &expr->data.const_data, MDL);
			expression_dereference(&expr, MDL);
			continue;
		}

		declaration = parse_statement(cfile, host->group, HOST_DECL,
                                              host, declaration);
	} while (1);

	if (deleted) {
		struct host_decl *hp = (struct host_decl *)0;
		if (host_hash_lookup (&hp, host_name_hash,
				      (unsigned char *)host -> name,
				      strlen (host -> name), MDL)) {
			delete_host (hp, 0);
			host_dereference (&hp, MDL);
		}
	} else {
		if (host -> named_group && host -> named_group -> group) {
			if (host -> group -> statements ||
			    (host -> group -> authoritative !=
			     host -> named_group -> group -> authoritative)) {
				if (host -> group -> next)
				    group_dereference (&host -> group -> next,
						       MDL);
				group_reference (&host -> group -> next,
						 host -> named_group -> group,
						 MDL);
			} else {
				group_dereference (&host -> group, MDL);
				group_reference (&host -> group,
						 host -> named_group -> group,
						 MDL);
			}
		}
				
		if (dynamicp)
			host -> flags |= HOST_DECL_DYNAMIC;
		else
			host -> flags |= HOST_DECL_STATIC;

		status = enter_host (host, dynamicp, 0);
		if (status != ISC_R_SUCCESS)
			parse_warn (cfile, "host %s: %s", host -> name,
				    isc_result_totext (status));
	}
	host_dereference (&host, MDL);
}

/* class-declaration :== STRING LBRACE parameters declarations RBRACE
*/

int parse_class_declaration (cp, cfile, group, type)
	struct class **cp;
	struct parse *cfile;
	struct group *group;
	int type;
{
	const char *val;
	enum dhcp_token token;
	struct class *class = NULL, *pc = NULL;
	int declaration = 0;
	int lose = 0;
	struct data_string data;
	char *name;
	const char *tname;
	struct executable_statement *stmt = NULL;
	int new = 1;
	isc_result_t status = ISC_R_FAILURE;
	int matchedonce = 0;
	int submatchedonce = 0;
	unsigned code;

	token = next_token (&val, NULL, cfile);
	if (token != STRING) {
		parse_warn (cfile, "Expecting class name");
		skip_to_semi (cfile);
		return 0;
	}

	/* See if there's already a class with the specified name. */
	find_class (&pc, val, MDL);

	/* If it is a class, we're updating it.  If it's any of the other
	 * types (subclass, vendor or user class), the named class is a
	 * reference to the parent class so its mandatory.
	 */
	if (pc && (type == CLASS_TYPE_CLASS)) {
		class_reference(&class, pc, MDL);
		new = 0;
		class_dereference(&pc, MDL);
	} else if (!pc && (type != CLASS_TYPE_CLASS)) {
		parse_warn(cfile, "no class named %s", val);
		skip_to_semi(cfile);
		return 0;
	}

	/* The old vendor-class and user-class declarations had an implicit
	   match.   We don't do the implicit match anymore.   Instead, for
	   backward compatibility, we have an implicit-vendor-class and an
	   implicit-user-class.   vendor-class and user-class declarations
	   are turned into subclasses of the implicit classes, and the
	   submatch expression of the implicit classes extracts the contents of
	   the vendor class or user class. */
	if ((type == CLASS_TYPE_VENDOR) || (type == CLASS_TYPE_USER)) {
		data.len = strlen (val);
		data.buffer = NULL;
		if (!buffer_allocate (&data.buffer, data.len + 1, MDL))
			log_fatal ("no memory for class name.");
		data.data = &data.buffer -> data [0];
		data.terminated = 1;

		tname = type ? "implicit-vendor-class" : "implicit-user-class";
	} else if (type == CLASS_TYPE_CLASS) {
		tname = val;
	} else {
		tname = NULL;
	}

	if (tname) {
		name = dmalloc (strlen (tname) + 1, MDL);
		if (!name)
			log_fatal ("No memory for class name %s.", tname);
		strcpy (name, val);
	} else
		name = NULL;

	/* If this is a straight subclass, parse the hash string. */
	if (type == CLASS_TYPE_SUBCLASS) {
		token = peek_token (&val, NULL, cfile);
		if (token == STRING) {
			skip_token(&val, &data.len, cfile);
			data.buffer = NULL;

			if (!buffer_allocate (&data.buffer,
					      data.len + 1, MDL)) {
				if (pc)
					class_dereference (&pc, MDL);
				
				return 0;
			}
			data.terminated = 1;
			data.data = &data.buffer -> data [0];
			memcpy ((char *)data.buffer -> data, val,
				data.len + 1);
		} else if (token == NUMBER_OR_NAME || token == NUMBER) {
			memset (&data, 0, sizeof data);
			if (!parse_cshl (&data, cfile)) {
				if (pc)
					class_dereference (&pc, MDL);
				return 0;
			}
		} else {
			parse_warn (cfile, "Expecting string or hex list.");
			if (pc)
				class_dereference (&pc, MDL);
			return 0;
		}
	}

	/* See if there's already a class in the hash table matching the
	   hash data. */
	if (type != CLASS_TYPE_CLASS)
		class_hash_lookup (&class, pc -> hash,
				   (const char *)data.data, data.len, MDL);

	/* If we didn't find an existing class, allocate a new one. */
	if (!class) {
		/* Allocate the class structure... */
		if (type == CLASS_TYPE_SUBCLASS) {
			status = subclass_allocate (&class, MDL);
		} else {
			status = class_allocate (&class, MDL);
		}
		if (pc) {
			group_reference (&class -> group, pc -> group, MDL);
			class_reference (&class -> superclass, pc, MDL);
			class -> lease_limit = pc -> lease_limit;
			if (class -> lease_limit) {
				class -> billed_leases =
					dmalloc (class -> lease_limit *
						 sizeof (struct lease *), MDL);
				if (!class -> billed_leases)
					log_fatal ("no memory for billing");
				memset (class -> billed_leases, 0,
					(class -> lease_limit *
					 sizeof (struct lease *)));
			}
			data_string_copy (&class -> hash_string, &data, MDL);
			if (!pc -> hash &&
			    !class_new_hash (&pc->hash, SCLASS_HASH_SIZE, MDL))
				log_fatal ("No memory for subclass hash.");
			class_hash_add (pc -> hash,
					(const char *)class -> hash_string.data,
					class -> hash_string.len,
					(void *)class, MDL);
		} else {
			if (class->group)
				group_dereference(&class->group, MDL);
			if (!clone_group (&class -> group, group, MDL))
				log_fatal ("no memory to clone class group.");
		}

		/* If this is an implicit vendor or user class, add a
		   statement that causes the vendor or user class ID to
		   be sent back in the reply. */
		if (type == CLASS_TYPE_VENDOR || type == CLASS_TYPE_USER) {
			stmt = NULL;
			if (!executable_statement_allocate (&stmt, MDL))
				log_fatal ("no memory for class statement.");
			stmt -> op = supersede_option_statement;
			if (option_cache_allocate (&stmt -> data.option,
						   MDL)) {
				stmt -> data.option -> data = data;
				code = (type == CLASS_TYPE_VENDOR)
					? DHO_VENDOR_CLASS_IDENTIFIER
					: DHO_USER_CLASS;
				option_code_hash_lookup(
						&stmt->data.option->option,
							dhcp_universe.code_hash,
							&code, 0, MDL);
			}
			class -> statements = stmt;
		}

		/* Save the name, if there is one. */
		if (class->name != NULL)
			dfree(class->name, MDL);
		class->name = name;
	}

	if (type != CLASS_TYPE_CLASS)
		data_string_forget(&data, MDL);

	/* Spawned classes don't have to have their own settings. */
	if (class -> superclass) {
		token = peek_token (&val, NULL, cfile);
		if (token == SEMI) {
			skip_token(&val, NULL, cfile);

			if (cp)
				status = class_reference (cp, class, MDL);
			class_dereference (&class, MDL);
			if (pc)
				class_dereference (&pc, MDL);
			return cp ? (status == ISC_R_SUCCESS) : 1;
		}
		/* Give the subclass its own group. */
		if (!clone_group (&class -> group, class -> group, MDL))
			log_fatal ("can't clone class group.");

	}

	if (!parse_lbrace (cfile)) {
		class_dereference (&class, MDL);
		if (pc)
			class_dereference (&pc, MDL);
		return 0;
	}

	do {
		token = peek_token (&val, NULL, cfile);
		if (token == RBRACE) {
			skip_token(&val, NULL, cfile);
			break;
		} else if (token == END_OF_FILE) {
			skip_token(&val, NULL, cfile);
			parse_warn (cfile, "unexpected end of file");
			break;
		} else if (token == DYNAMIC) {
			class->flags |= CLASS_DECL_DYNAMIC;
			skip_token(&val, NULL, cfile);
			if (!parse_semi (cfile))
				break;
			continue;
		} else if (token == TOKEN_DELETED) {
			class->flags |= CLASS_DECL_DELETED;
			skip_token(&val, NULL, cfile);
			if (!parse_semi (cfile))
				break;
			continue;
		} else if (token == MATCH) {
			if (pc) {
				parse_warn (cfile,
					    "invalid match in subclass.");
				skip_to_semi (cfile);
				break;
			}
			skip_token(&val, NULL, cfile);
			token = peek_token (&val, NULL, cfile);
			if (token != IF)
				goto submatch;
			skip_token(&val, NULL, cfile);
			if (matchedonce) {
				parse_warn(cfile, "A class may only have "
						  "one 'match if' clause.");
				skip_to_semi(cfile);
				break;
			}
			matchedonce = 1;
			if (class->expr)
				expression_dereference(&class->expr, MDL);
			if (!parse_boolean_expression (&class->expr, cfile,
						       &lose)) {
				if (!lose) {
					parse_warn (cfile,
						    "expecting boolean expr.");
					skip_to_semi (cfile);
				}
			} else {
#if defined (DEBUG_EXPRESSION_PARSE)
				print_expression ("class match",
						  class -> expr);
#endif
				parse_semi (cfile);
			}
		} else if (token == SPAWN) {
			skip_token(&val, NULL, cfile);
			if (pc) {
				parse_warn (cfile,
					    "invalid spawn in subclass.");
				skip_to_semi (cfile);
				break;
			}
			class -> spawning = 1;
			token = next_token (&val, NULL, cfile);
			if (token != WITH) {
				parse_warn (cfile,
					    "expecting with after spawn");
				skip_to_semi (cfile);
				break;
			}
		      submatch:
			if (submatchedonce) {
				parse_warn (cfile,
					    "can't override existing %s.",
					    "submatch/spawn");
				skip_to_semi (cfile);
				break;
			}
			submatchedonce = 1;
			if (class->submatch)
				expression_dereference(&class->submatch, MDL);
			if (!parse_data_expression (&class -> submatch,
						    cfile, &lose)) {
				if (!lose) {
					parse_warn (cfile,
						    "expecting data expr.");
					skip_to_semi (cfile);
				}
			} else {
#if defined (DEBUG_EXPRESSION_PARSE)
				print_expression ("class submatch",
						  class -> submatch);
#endif
				parse_semi (cfile);
			}
		} else if (token == LEASE) {
			skip_token(&val, NULL, cfile);
			token = next_token (&val, NULL, cfile);
			if (token != LIMIT) {
				parse_warn (cfile, "expecting \"limit\"");
				if (token != SEMI)
					skip_to_semi (cfile);
				break;
			}
			token = next_token (&val, NULL, cfile);
			if (token != NUMBER) {
				parse_warn (cfile, "expecting a number");
				if (token != SEMI)
					skip_to_semi (cfile);
				break;
			}
			class -> lease_limit = atoi (val);
			if (class->billed_leases)
				dfree(class->billed_leases, MDL);
			class -> billed_leases =
				dmalloc (class -> lease_limit *
					 sizeof (struct lease *), MDL);
			if (!class -> billed_leases)
				log_fatal ("no memory for billed leases.");
			memset (class -> billed_leases, 0,
				(class -> lease_limit *
				 sizeof (struct lease *)));
			have_billing_classes = 1;
			parse_semi (cfile);
		} else {
			declaration = parse_statement (cfile, class -> group,
						       CLASS_DECL, NULL,
						       declaration);
		}
	} while (1);

	if (class->flags & CLASS_DECL_DELETED) {
		if (type == CLASS_TYPE_CLASS) {
			struct class *theclass = NULL;
		
			status = find_class(&theclass, class->name, MDL);
			if (status == ISC_R_SUCCESS) {
				delete_class(theclass, 0);
				class_dereference(&theclass, MDL);
			}
		} else {
			class_hash_delete(pc->hash,
					  (char *)class->hash_string.data,
					  class->hash_string.len, MDL);
		}
	} else if (type == CLASS_TYPE_CLASS && new) {
		if (!collections -> classes)
			class_reference (&collections -> classes, class, MDL);
		else {
			struct class *c;
			for (c = collections -> classes;
			     c -> nic; c = c -> nic)
				;
			class_reference (&c -> nic, class, MDL);
		}
	}

	if (cp)				/* should always be 0??? */
		status = class_reference (cp, class, MDL);
	class_dereference (&class, MDL);
	if (pc)
		class_dereference (&pc, MDL);
	return cp ? (status == ISC_R_SUCCESS) : 1;
}

/* shared-network-declaration :==
			hostname LBRACE declarations parameters RBRACE */

void parse_shared_net_declaration (cfile, group)
	struct parse *cfile;
	struct group *group;
{
	const char *val;
	enum dhcp_token token;
	struct shared_network *share;
	char *name;
	int declaration = 0;
	isc_result_t status;

	share = (struct shared_network *)0;
	status = shared_network_allocate (&share, MDL);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't allocate shared subnet: %s",
			   isc_result_totext (status));
	if (clone_group (&share -> group, group, MDL) == 0) {
		log_fatal ("Can't clone group for shared net");
	}
	shared_network_reference (&share -> group -> shared_network,
				  share, MDL);

	/* Get the name of the shared network... */
	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == STRING) {
		skip_token(&val, (unsigned *)0, cfile);

		if (val [0] == 0) {
			parse_warn (cfile, "zero-length shared network name");
			val = "<no-name-given>";
		}
		name = dmalloc (strlen (val) + 1, MDL);
		if (!name)
			log_fatal ("no memory for shared network name");
		strcpy (name, val);
	} else {
		name = parse_host_name (cfile);
		if (!name) {
			parse_warn (cfile,
				     "expecting a name for shared-network");
			skip_to_semi (cfile);
			shared_network_dereference (&share, MDL);
			return;
		}
	}
	share -> name = name;

	if (!parse_lbrace (cfile)) {
		shared_network_dereference (&share, MDL);
		return;
	}

	do {
		token = peek_token (&val, (unsigned *)0, cfile);
		if (token == RBRACE) {
			skip_token(&val, (unsigned *)0, cfile);
			if (!share -> subnets)
				parse_warn (cfile,
					    "empty shared-network decl");
			else
				enter_shared_network (share);
			shared_network_dereference (&share, MDL);
			return;
		} else if (token == END_OF_FILE) {
			skip_token(&val, (unsigned *)0, cfile);
			parse_warn (cfile, "unexpected end of file");
			break;
		} else if (token == INTERFACE) {
			skip_token(&val, (unsigned *)0, cfile);
			token = next_token (&val, (unsigned *)0, cfile);
			new_shared_network_interface (cfile, share, val);
			if (!parse_semi (cfile))
				break;
			continue;
		}

		declaration = parse_statement (cfile, share -> group,
					       SHARED_NET_DECL,
					       (struct host_decl *)0,
					       declaration);
	} while (1);
	shared_network_dereference (&share, MDL);
}


static int
common_subnet_parsing(struct parse *cfile, 
		      struct shared_network *share,
		      struct subnet *subnet) {
	enum dhcp_token token;
	struct subnet *t, *u;
	const char *val;
	int declaration = 0;

	enter_subnet(subnet);

	if (!parse_lbrace(cfile)) {
		subnet_dereference(&subnet, MDL);
		return 0;
	}

	do {
		token = peek_token(&val, NULL, cfile);
		if (token == RBRACE) {
			skip_token(&val, NULL, cfile);
			break;
		} else if (token == END_OF_FILE) {
			skip_token(&val, NULL, cfile);
			parse_warn (cfile, "unexpected end of file");
			break;
		} else if (token == INTERFACE) {
			skip_token(&val, NULL, cfile);
			token = next_token(&val, NULL, cfile);
			new_shared_network_interface(cfile, share, val);
			if (!parse_semi(cfile))
				break;
			continue;
		}
		declaration = parse_statement(cfile, subnet->group,
					      SUBNET_DECL,
					      NULL,
					      declaration);
	} while (1);

	/* Add the subnet to the list of subnets in this shared net. */
	if (share->subnets == NULL) {
		subnet_reference(&share->subnets, subnet, MDL);
	} else {
		u = NULL;
		for (t = share->subnets; t->next_sibling; t = t->next_sibling) {
			if (subnet_inner_than(subnet, t, 0)) {
				subnet_reference(&subnet->next_sibling, t, MDL);
				if (u) {
					subnet_dereference(&u->next_sibling,
							   MDL);
					subnet_reference(&u->next_sibling,
							 subnet, MDL);
				} else {
					subnet_dereference(&share->subnets,
							   MDL);
					subnet_reference(&share->subnets,
							 subnet, MDL);
				}
				subnet_dereference(&subnet, MDL);
				return 1;
			}
			u = t;
		}
		subnet_reference(&t->next_sibling, subnet, MDL);
	}
	subnet_dereference(&subnet, MDL);
	return 1;
}

/* subnet-declaration :==
	net NETMASK netmask RBRACE parameters declarations LBRACE */

void parse_subnet_declaration (cfile, share)
	struct parse *cfile;
	struct shared_network *share;
{
	const char *val;
	enum dhcp_token token;
	struct subnet *subnet;
	struct iaddr iaddr;
	unsigned char addr [4];
	unsigned len = sizeof addr;
	isc_result_t status;

	subnet = (struct subnet *)0;
	status = subnet_allocate (&subnet, MDL);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Allocation of new subnet failed: %s",
			   isc_result_totext (status));
	shared_network_reference (&subnet -> shared_network, share, MDL);

	/*
	 * If our parent shared network was implicitly created by the software,
	 * and not explicitly configured by the user, then we actually put all
	 * configuration scope in the parent (the shared network and subnet
	 * share the same {}-level scope).
	 *
	 * Otherwise, we clone the parent group and continue as normal.
	 */
	if (share->flags & SHARED_IMPLICIT) {
		group_reference(&subnet->group, share->group, MDL);
	} else {
		if (!clone_group(&subnet->group, share->group, MDL)) {
			log_fatal("Allocation of group for new subnet failed.");
		}
	}
	subnet_reference (&subnet -> group -> subnet, subnet, MDL);

	/* Get the network number... */
	if (!parse_numeric_aggregate (cfile, addr, &len, DOT, 10, 8)) {
		subnet_dereference (&subnet, MDL);
		return;
	}
	memcpy (iaddr.iabuf, addr, len);
	iaddr.len = len;
	subnet -> net = iaddr;

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != NETMASK) {
		parse_warn (cfile, "Expecting netmask");
		skip_to_semi (cfile);
		return;
	}

	/* Get the netmask... */
	if (!parse_numeric_aggregate (cfile, addr, &len, DOT, 10, 8)) {
		subnet_dereference (&subnet, MDL);
		return;
	}
	memcpy (iaddr.iabuf, addr, len);
	iaddr.len = len;
	subnet -> netmask = iaddr;

	/* Validate the network number/netmask pair. */
	if (host_addr (subnet -> net, subnet -> netmask)) {
		char *maskstr;

		maskstr = strdup (piaddr (subnet -> netmask));
		parse_warn (cfile,
		   "subnet %s netmask %s: bad subnet number/mask combination.",
			    piaddr (subnet -> net), maskstr);
		free(maskstr);
		subnet_dereference (&subnet, MDL);
		skip_to_semi (cfile);
		return;
	}

	common_subnet_parsing(cfile, share, subnet);
}

/* subnet6-declaration :==
	net / bits RBRACE parameters declarations LBRACE */

void
parse_subnet6_declaration(struct parse *cfile, struct shared_network *share) {
#if !defined(DHCPv6)
	parse_warn(cfile, "No DHCPv6 support.");
	skip_to_semi(cfile);
#else /* defined(DHCPv6) */
	struct subnet *subnet;
	isc_result_t status;
	enum dhcp_token token;
	const char *val;
	char *endp;
	int ofs;
	const static int mask[] = { 0x00, 0x80, 0xC0, 0xE0, 
				    0xF0, 0xF8, 0xFC, 0xFE };
	struct iaddr iaddr;

        if (local_family != AF_INET6) {
                parse_warn(cfile, "subnet6 statement is only supported "
				  "in DHCPv6 mode.");
                skip_to_semi(cfile);
                return;
        }

	subnet = NULL;
	status = subnet_allocate(&subnet, MDL);
	if (status != ISC_R_SUCCESS) {
		log_fatal("Allocation of new subnet failed: %s",
			  isc_result_totext(status));
	}
	shared_network_reference(&subnet->shared_network, share, MDL);

	/*
	 * If our parent shared network was implicitly created by the software,
	 * and not explicitly configured by the user, then we actually put all
	 * configuration scope in the parent (the shared network and subnet
	 * share the same {}-level scope).
	 *
	 * Otherwise, we clone the parent group and continue as normal.
	 */
	if (share->flags & SHARED_IMPLICIT) {
		group_reference(&subnet->group, share->group, MDL);
	} else {
		if (!clone_group(&subnet->group, share->group, MDL)) {
			log_fatal("Allocation of group for new subnet failed.");
		}
	}
	subnet_reference(&subnet->group->subnet, subnet, MDL);

	if (!parse_ip6_addr(cfile, &subnet->net)) {
		subnet_dereference(&subnet, MDL);
		return;
	}

	token = next_token(&val, NULL, cfile);
	if (token != SLASH) {
		parse_warn(cfile, "Expecting a '/'.");
		skip_to_semi(cfile);
		return;
	}

	token = next_token(&val, NULL, cfile);
	if (token != NUMBER) {
		parse_warn(cfile, "Expecting a number.");
		skip_to_semi(cfile);
		return;
	}

	subnet->prefix_len = strtol(val, &endp, 10);
	if ((subnet->prefix_len < 0) || 
	    (subnet->prefix_len > 128) || 
	    (*endp != '\0')) {
	    	parse_warn(cfile, "Expecting a number between 0 and 128.");
		skip_to_semi(cfile);
		return;
	}

	if (!is_cidr_mask_valid(&subnet->net, subnet->prefix_len)) {
		parse_warn(cfile, "New subnet mask too short.");
		skip_to_semi(cfile);
		return;
	}

	/* 
	 * Create a netmask. 
	 */
	subnet->netmask.len = 16;
	ofs = subnet->prefix_len / 8;
	if (ofs < subnet->netmask.len) {
		subnet->netmask.iabuf[ofs] = mask[subnet->prefix_len % 8];
	}
	while (--ofs >= 0) {
		subnet->netmask.iabuf[ofs] = 0xFF;
	}

	/* Validate the network number/netmask pair. */
	iaddr = subnet_number(subnet->net, subnet->netmask);
	if (memcmp(&iaddr, &subnet->net, 16) != 0) {
		parse_warn(cfile,
		   "subnet %s/%d: prefix not long enough for address.",
			    piaddr(subnet->net), subnet->prefix_len);
		subnet_dereference(&subnet, MDL);
		skip_to_semi(cfile);
		return;
	}

	if (!common_subnet_parsing(cfile, share, subnet)) {
		return;
	}
#endif /* defined(DHCPv6) */
}

/* group-declaration :== RBRACE parameters declarations LBRACE */

void parse_group_declaration (cfile, group)
	struct parse *cfile;
	struct group *group;
{
	const char *val;
	enum dhcp_token token;
	struct group *g;
	int declaration = 0;
	struct group_object *t = NULL;
	isc_result_t status;
	char *name = NULL;
	int deletedp = 0;
	int dynamicp = 0;
	int staticp = 0;

	g = NULL;
	if (!clone_group(&g, group, MDL))
		log_fatal("no memory for explicit group.");

	token = peek_token(&val, NULL, cfile);
	if (is_identifier (token) || token == STRING) {
		skip_token(&val, NULL, cfile);
		
		name = dmalloc(strlen(val) + 1, MDL);
		if (!name)
			log_fatal("no memory for group decl name %s", val);
		strcpy(name, val);
	}		

	if (!parse_lbrace(cfile)) {
		group_dereference(&g, MDL);
		return;
	}

	do {
		token = peek_token(&val, NULL, cfile);
		if (token == RBRACE) {
			skip_token(&val, NULL, cfile);
			break;
		} else if (token == END_OF_FILE) {
			skip_token(&val, NULL, cfile);
			parse_warn(cfile, "unexpected end of file");
			break;
		} else if (token == TOKEN_DELETED) {
			skip_token(&val, NULL, cfile);
			parse_semi(cfile);
			deletedp = 1;
		} else if (token == DYNAMIC) {
			skip_token(&val, NULL, cfile);
			parse_semi(cfile);
			dynamicp = 1;
		} else if (token == STATIC) {
			skip_token(&val, NULL, cfile);
			parse_semi(cfile);
			staticp = 1;
		}
		declaration = parse_statement(cfile, g, GROUP_DECL,
					      NULL, declaration);
	} while (1);

	if (name) {
		if (deletedp) {
			if (group_name_hash) {
				t = NULL;
				if (group_hash_lookup(&t, group_name_hash,
						      name,
						      strlen(name), MDL)) {
					delete_group(t, 0);
				}
			}
		} else {
			t = NULL;
			status = group_object_allocate(&t, MDL);
			if (status != ISC_R_SUCCESS)
				log_fatal("no memory for group decl %s: %s",
					  val, isc_result_totext(status));
			group_reference(&t->group, g, MDL);
			t->name = name;
			/* no need to include deletedp as it's handled above */
			t->flags = ((staticp ? GROUP_OBJECT_STATIC : 0) |
				    (dynamicp ? GROUP_OBJECT_DYNAMIC : 0));
			supersede_group(t, 0);
		}
		if (t != NULL)
			group_object_dereference(&t, MDL);
	}
}

/* fixed-addr-parameter :== ip-addrs-or-hostnames SEMI
   ip-addrs-or-hostnames :== ip-addr-or-hostname
			   | ip-addrs-or-hostnames ip-addr-or-hostname */

int
parse_fixed_addr_param(struct option_cache **oc, 
		       struct parse *cfile, 
		       enum dhcp_token type) {
	int parse_ok;
	const char *val;
	enum dhcp_token token;
	struct expression *expr = NULL;
	struct expression *tmp, *new;
	int status;

	do {
		tmp = NULL;
		if (type == FIXED_ADDR) {
			parse_ok = parse_ip_addr_or_hostname(&tmp, cfile, 1);
		} else {
			/* INSIST(type == FIXED_ADDR6); */
			parse_ok = parse_ip6_addr_expr(&tmp, cfile);
		}
		if (parse_ok) {
			if (expr != NULL) {
				new = NULL;
				status = make_concat(&new, expr, tmp);
				expression_dereference(&expr, MDL);
				expression_dereference(&tmp, MDL);
				if (!status) {
					return 0;
				}
				expr = new;
			} else {
				expr = tmp;
			}
		} else {
			if (expr != NULL) {
				expression_dereference (&expr, MDL);
			}
			return 0;
		}
		token = peek_token(&val, NULL, cfile);
		if (token == COMMA) {
			token = next_token(&val, NULL, cfile);
		}
	} while (token == COMMA);

	if (!parse_semi(cfile)) {
		if (expr) {
			expression_dereference (&expr, MDL);
		}
		return 0;
	}

	status = option_cache(oc, NULL, expr, NULL, MDL);
	expression_dereference(&expr, MDL);
	return status;
}

/* lease_declaration :== LEASE ip_address LBRACE lease_parameters RBRACE

   lease_parameters :== <nil>
		      | lease_parameter
		      | lease_parameters lease_parameter

   lease_parameter :== STARTS date
		     | ENDS date
		     | TIMESTAMP date
		     | HARDWARE hardware-parameter
		     | UID hex_numbers SEMI
		     | HOSTNAME hostname SEMI
		     | CLIENT_HOSTNAME hostname SEMI
		     | CLASS identifier SEMI
		     | DYNAMIC_BOOTP SEMI */

int parse_lease_declaration (struct lease **lp, struct parse *cfile)
{
	const char *val;
	enum dhcp_token token;
	unsigned char addr [4];
	unsigned len = sizeof addr;
	int seenmask = 0;
	int seenbit;
	char tbuf [32];
	struct lease *lease;
	struct executable_statement *on;
	int lose;
	TIME t;
	int noequal, newbinding;
	struct binding *binding;
	struct binding_value *nv;
	isc_result_t status;
	struct option_cache *oc;
	pair *p;
	binding_state_t new_state;
	unsigned buflen = 0;
	struct class *class;

	lease = (struct lease *)0;
	status = lease_allocate (&lease, MDL);
	if (status != ISC_R_SUCCESS)
		return 0;

	/* Get the address for which the lease has been issued. */
	if (!parse_numeric_aggregate (cfile, addr, &len, DOT, 10, 8)) {
		lease_dereference (&lease, MDL);
		return 0;
	}
	memcpy (lease -> ip_addr.iabuf, addr, len);
	lease -> ip_addr.len = len;

	if (!parse_lbrace (cfile)) {
		lease_dereference (&lease, MDL);
		return 0;
	}

	do {
		token = next_token (&val, (unsigned *)0, cfile);
		if (token == RBRACE)
			break;
		else if (token == END_OF_FILE) {
			parse_warn (cfile, "unexpected end of file");
			break;
		}
		strncpy (tbuf, val, sizeof tbuf);
		tbuf [(sizeof tbuf) - 1] = 0;

		/* Parse any of the times associated with the lease. */
		switch (token) {
		      case STARTS:
		      case ENDS:
		      case TIMESTAMP:
		      case TSTP:
		      case TSFP:
		      case ATSFP:
		      case CLTT:
			t = parse_date (cfile);
			switch (token) {
			      case STARTS:
				seenbit = 1;
				lease -> starts = t;
				break;
			
			      case ENDS:
				seenbit = 2;
				lease -> ends = t;
				break;
				
			      case TSTP:
				seenbit = 65536;
				lease -> tstp = t;
				break;
				
			      case TSFP:
				seenbit = 131072;
				lease -> tsfp = t;
				break;

			      case ATSFP:
				seenbit = 262144;
				lease->atsfp = t;
				break;
				
			      case CLTT:
				seenbit = 524288;
				lease -> cltt = t;
				break;
				
			      default: /* for gcc, we'll never get here. */
				log_fatal ("Impossible error at %s:%d.", MDL);
				return 0;
			}
			break;

			/* Colon-separated hexadecimal octets... */
		      case UID:
			seenbit = 8;
			token = peek_token (&val, (unsigned *)0, cfile);
			if (token == STRING) {
				unsigned char *tuid;
				skip_token(&val, &buflen, cfile);
				if (buflen < sizeof lease -> uid_buf) {
					tuid = lease -> uid_buf;
					lease -> uid_max =
						sizeof lease -> uid_buf;
				} else {
					tuid = ((unsigned char *)
						dmalloc (buflen, MDL));
					if (!tuid) {
						log_error ("no space for uid");
						lease_dereference (&lease,
								   MDL);
						return 0;
					}
					lease -> uid_max = buflen;
				}
				lease -> uid_len = buflen;
				memcpy (tuid, val, lease -> uid_len);
				lease -> uid = tuid;
			} else {
				buflen = 0;
				lease -> uid = (parse_numeric_aggregate
						(cfile, (unsigned char *)0,
						 &buflen, ':', 16, 8));
				if (!lease -> uid) {
					lease_dereference (&lease, MDL);
					return 0;
				}
				lease -> uid_len = buflen;
				lease -> uid_max = buflen;
				if (lease -> uid_len == 0) {
					lease -> uid = (unsigned char *)0;
					parse_warn (cfile, "zero-length uid");
					seenbit = 0;
					parse_semi (cfile);
					break;
				}
			}
			parse_semi (cfile);
			if (!lease -> uid) {
				log_fatal ("No memory for lease uid");
			}
			break;

		      case CLASS:
			seenbit = 32;
			token = next_token (&val, (unsigned *)0, cfile);
			if (!is_identifier (token)) {
				if (token != SEMI)
					skip_to_rbrace (cfile, 1);
				lease_dereference (&lease, MDL);
				return 0;
			}
			parse_semi (cfile);
			/* for now, we aren't using this. */
			break;

		      case HARDWARE:
			seenbit = 64;
			parse_hardware_param (cfile,
					      &lease -> hardware_addr);
			break;

		      case TOKEN_RESERVED:
			seenbit = 0;
			lease->flags |= RESERVED_LEASE;
			parse_semi(cfile);
			break;

		      case DYNAMIC_BOOTP:
			seenbit = 0;
			lease -> flags |= BOOTP_LEASE;
			parse_semi (cfile);
			break;

			/* XXX: Reverse compatibility? */
		      case TOKEN_ABANDONED:
			seenbit = 256;
			lease -> binding_state = FTS_ABANDONED;
			lease -> next_binding_state = FTS_ABANDONED;
			parse_semi (cfile);
			break;

		      case TOKEN_NEXT:
			seenbit = 128;
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != BINDING) {
				parse_warn (cfile, "expecting 'binding'");
				skip_to_semi (cfile);
				break;
			}
			goto do_binding_state;

		      case REWIND:
			seenbit = 512;
			token = next_token(&val, NULL, cfile);
			if (token != BINDING) {
				parse_warn(cfile, "expecting 'binding'");
				skip_to_semi(cfile);
				break;
			}
			goto do_binding_state;

		      case BINDING:
			seenbit = 256;

		      do_binding_state:
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != STATE) {
				parse_warn (cfile, "expecting 'state'");
				skip_to_semi (cfile);
				break;
			}
			token = next_token (&val, (unsigned *)0, cfile);
			switch (token) {
			      case TOKEN_ABANDONED:
				new_state = FTS_ABANDONED;
				break;
			      case TOKEN_FREE:
				new_state = FTS_FREE;
				break;
			      case TOKEN_ACTIVE:
				new_state = FTS_ACTIVE;
				break;
			      case TOKEN_EXPIRED:
				new_state = FTS_EXPIRED;
				break;
			      case TOKEN_RELEASED:
				new_state = FTS_RELEASED;
				break;
			      case TOKEN_RESET:
				new_state = FTS_RESET;
				break;
			      case TOKEN_BACKUP:
				new_state = FTS_BACKUP;
				break;

				/* RESERVED and BOOTP states preserved for
				 * compatibleness with older versions.
				 */
			      case TOKEN_RESERVED:
				new_state = FTS_ACTIVE;
				lease->flags |= RESERVED_LEASE;
				break;
			      case TOKEN_BOOTP:
				new_state = FTS_ACTIVE;
				lease->flags |= BOOTP_LEASE;
				break;

			      default:
				parse_warn (cfile,
					    "%s: expecting a binding state.",
					    val);
				skip_to_semi (cfile);
				return 0;
			}

			if (seenbit == 256) {
				lease -> binding_state = new_state;

				/*
				 * Apply default/conservative next/rewind
				 * binding states if they haven't been set
				 * yet.  These defaults will be over-ridden if
				 * they are set later in parsing.
				 */
				if (!(seenmask & 128))
				    lease->next_binding_state = new_state;

				/* The most conservative rewind state. */
				if (!(seenmask & 512))
				    lease->rewind_binding_state = new_state;
			} else if (seenbit == 128)
				lease -> next_binding_state = new_state;
			else if (seenbit == 512)
				lease->rewind_binding_state = new_state;
			else
				log_fatal("Impossible condition at %s:%d.",
					  MDL);

			parse_semi (cfile);
			break;

		      case CLIENT_HOSTNAME:
			seenbit = 1024;
			token = peek_token (&val, (unsigned *)0, cfile);
			if (token == STRING) {
				if (!parse_string (cfile,
						   &lease -> client_hostname,
						   (unsigned *)0)) {
					lease_dereference (&lease, MDL);
					return 0;
				}
			} else {
				lease -> client_hostname =
					parse_host_name (cfile);
				if (lease -> client_hostname)
					parse_semi (cfile);
				else {
					parse_warn (cfile,
						    "expecting a hostname.");
					skip_to_semi (cfile);
					lease_dereference (&lease, MDL);
					return 0;
				}
			}
			break;
			
		      case BILLING:
			seenbit = 2048;
			class = (struct class *)0;
			token = next_token (&val, (unsigned *)0, cfile);
			if (token == CLASS) {
				token = next_token (&val,
						    (unsigned *)0, cfile);
				if (token != STRING) {
					parse_warn (cfile, "expecting string");
					if (token != SEMI)
						skip_to_semi (cfile);
					token = BILLING;
					break;
				}
				if (lease -> billing_class)
				    class_dereference (&lease -> billing_class,
						       MDL);
				find_class (&class, val, MDL);
				if (!class)
					parse_warn (cfile,
						    "unknown class %s", val);
				parse_semi (cfile);
			} else if (token == SUBCLASS) {
				if (lease -> billing_class)
				    class_dereference (&lease -> billing_class,
						       MDL);
				parse_class_declaration(&class, cfile, NULL,
							CLASS_TYPE_SUBCLASS);
			} else {
				parse_warn (cfile, "expecting \"class\"");
				if (token != SEMI)
					skip_to_semi (cfile);
			}
			if (class) {
				class_reference (&lease -> billing_class,
						 class, MDL);
				class_dereference (&class, MDL);
			}
			break;

		      case ON:
			on = (struct executable_statement *)0;
			lose = 0;
			if (!parse_on_statement (&on, cfile, &lose)) {
				skip_to_rbrace (cfile, 1);
				lease_dereference (&lease, MDL);
				return 0;
			}
			seenbit = 0;
			if ((on->data.on.evtypes & ON_EXPIRY) &&
			    on->data.on.statements) {
				seenbit |= 16384;
				executable_statement_reference
					(&lease->on_star.on_expiry,
					 on->data.on.statements, MDL);
			}
			if ((on->data.on.evtypes & ON_RELEASE) &&
			    on->data.on.statements) {
				seenbit |= 32768;
				executable_statement_reference
					(&lease->on_star.on_release,
					 on->data.on.statements, MDL);
			}
			executable_statement_dereference (&on, MDL);
			break;

		      case OPTION:
		      case SUPERSEDE:
			noequal = 0;
			seenbit = 0;
			oc = (struct option_cache *)0;
			if (parse_option_decl (&oc, cfile)) {
			    if (oc -> option -> universe !=
				&agent_universe) {
				    parse_warn (cfile,
						"agent option expected.");
				    option_cache_dereference (&oc, MDL);
				    break;
			    }
			    if (!lease -> agent_options &&
				!(option_chain_head_allocate
				  (&lease -> agent_options, MDL))) {
				log_error ("no memory to stash agent option");
				break;
			    }
			    for (p = &lease -> agent_options -> first;
				 *p; p = &((*p) -> cdr))
				    ;
			    *p = cons (0, 0);
			    option_cache_reference (((struct option_cache **)
						     &((*p) -> car)), oc, MDL);
			    option_cache_dereference (&oc, MDL);
			}
			break;

		      case TOKEN_SET:
			noequal = 0;
			
			token = next_token (&val, (unsigned *)0, cfile);
			if (token != NAME && token != NUMBER_OR_NAME) {
				parse_warn (cfile,
					    "%s can't be a variable name",
					    val);
			      badset:
				skip_to_semi (cfile);
				lease_dereference (&lease, MDL);
				return 0;
			}
			
			seenbit = 0;
		      special_set:
			if (lease -> scope)
				binding = find_binding (lease -> scope, val);
			else
				binding = (struct binding *)0;

			if (!binding) {
			    if (!lease -> scope)
				if (!(binding_scope_allocate
				      (&lease -> scope, MDL)))
					log_fatal ("no memory for scope");
			    binding = dmalloc (sizeof *binding, MDL);
			    if (!binding)
				    log_fatal ("No memory for lease %s.",
					       "binding");
			    memset (binding, 0, sizeof *binding);
			    binding -> name =
				    dmalloc (strlen (val) + 1, MDL);
			    if (!binding -> name)
				    log_fatal ("No memory for binding %s.",
					       "name");
			    strcpy (binding -> name, val);
			    newbinding = 1;
			} else  {
			    newbinding = 0;
			}

			nv = NULL;
			if (!binding_value_allocate(&nv, MDL))
				log_fatal("no memory for binding value.");

			if (!noequal) {
			    token = next_token (&val, (unsigned *)0, cfile);
			    if (token != EQUAL) {
				parse_warn (cfile,
					    "expecting '=' in set statement.");
				goto badset;
			    }
			}

			if (!parse_binding_value(cfile, nv)) {
				binding_value_dereference(&nv, MDL);
				lease_dereference(&lease, MDL);
				return 0;
			}

			if (newbinding) {
				binding_value_reference(&binding->value,
							nv, MDL);
				binding->next = lease->scope->bindings;
				lease->scope->bindings = binding;
			} else {
				binding_value_dereference(&binding->value, MDL);
				binding_value_reference(&binding->value,
							nv, MDL);
			}

			binding_value_dereference(&nv, MDL);
			parse_semi(cfile);
			break;

			/* case NAME: */
		      default:
			if (!strcasecmp (val, "ddns-fwd-name")) {
				seenbit = 4096;
				noequal = 1;
				goto special_set;
			} else if (!strcasecmp (val, "ddns-rev-name")) {
				seenbit = 8192;
				noequal = 1;
				goto special_set;
			} else
				parse_warn(cfile, "Unexpected configuration "
						  "directive.");
			skip_to_semi (cfile);
			seenbit = 0;
			lease_dereference (&lease, MDL);
			return 0;
		}

		if (seenmask & seenbit) {
			parse_warn (cfile,
				    "Too many %s parameters in lease %s\n",
				    tbuf, piaddr (lease -> ip_addr));
		} else
			seenmask |= seenbit;

	} while (1);

	/* If no binding state is specified, make one up. */
	if (!(seenmask & 256)) {
		if (lease->ends > cur_time ||
		    lease->on_star.on_expiry || lease->on_star.on_release)
			lease->binding_state = FTS_ACTIVE;
#if defined (FAILOVER_PROTOCOL)
		else if (lease->pool && lease->pool->failover_peer)
			lease->binding_state = FTS_EXPIRED;
#endif
		else
			lease->binding_state = FTS_FREE;
		if (lease->binding_state == FTS_ACTIVE) {
#if defined (FAILOVER_PROTOCOL)
			if (lease->pool && lease->pool->failover_peer)
				lease->next_binding_state = FTS_EXPIRED;
			else
#endif
				lease->next_binding_state = FTS_FREE;
		} else
			lease->next_binding_state = lease->binding_state;

		/* The most conservative rewind state implies no rewind. */
		lease->rewind_binding_state = lease->binding_state;
	}

	if (!(seenmask & 65536))
		lease->tstp = lease->ends;

	lease_reference (lp, lease, MDL);
	lease_dereference (&lease, MDL);
	return 1;
}

/* Parse the right side of a 'binding value'.
 *
 * set foo = "bar"; is a string
 * set foo = false; is a boolean
 * set foo = %31; is a numeric value.
 */
static int
parse_binding_value(struct parse *cfile, struct binding_value *value)
{
	struct data_string *data;
	unsigned char *s;
	const char *val;
	unsigned buflen;
	int token;

	if ((cfile == NULL) || (value == NULL))
		log_fatal("Invalid arguments at %s:%d.", MDL);

	token = peek_token(&val, NULL, cfile);
	if (token == STRING) {
		skip_token(&val, &buflen, cfile);

		value->type = binding_data;
		value->value.data.len = buflen;

		data = &value->value.data;

		if (!buffer_allocate(&data->buffer, buflen + 1, MDL))
			log_fatal ("No memory for binding.");

		memcpy(data->buffer->data, val, buflen + 1);

		data->data = data->buffer->data;
		data->terminated = 1;
	} else if (token == NUMBER_OR_NAME) {
		value->type = binding_data;

		data = &value->value.data;
		s = parse_numeric_aggregate(cfile, NULL, &data->len,
					    ':', 16, 8);
		if (s == NULL) {
			skip_to_semi(cfile);
			return 0;
		}

		if (data->len) {
			if (!buffer_allocate(&data->buffer, data->len + 1,
					     MDL))
				log_fatal("No memory for binding.");

			memcpy(data->buffer->data, s, data->len);
			data->data = data->buffer->data;

			dfree (s, MDL);
		}
	} else if (token == PERCENT) {
		skip_token(&val, NULL, cfile);
		token = next_token(&val, NULL, cfile);
		if (token != NUMBER) {
			parse_warn(cfile, "expecting decimal number.");
			if (token != SEMI)
				skip_to_semi(cfile);
			return 0;
		}
		value->type = binding_numeric;
		value->value.intval = atol(val);
	} else if (token == NAME) {
		token = next_token(&val, NULL, cfile);
		value->type = binding_boolean;
		if (!strcasecmp(val, "true"))
			value->value.boolean = 1;
		else if (!strcasecmp(val, "false"))
			value->value.boolean = 0;
		else {
			parse_warn(cfile, "expecting true or false");
			if (token != SEMI)
				skip_to_semi(cfile);
			return 0;
		}
	} else {
		parse_warn (cfile, "expecting a constant value.");
		if (token != SEMI)
			skip_to_semi (cfile);
		return 0;
	}

	return 1;
}

/* address-range-declaration :== ip-address ip-address SEMI
			       | DYNAMIC_BOOTP ip-address ip-address SEMI */

void parse_address_range (cfile, group, type, inpool, lpchain)
	struct parse *cfile;
	struct group *group;
	int type;
	struct pool *inpool;
	struct lease **lpchain;
{
	struct iaddr low, high, net;
	unsigned char addr [4];
	unsigned len = sizeof addr;
	enum dhcp_token token;
	const char *val;
	int dynamic = 0;
	struct subnet *subnet;
	struct shared_network *share;
	struct pool *pool;
	isc_result_t status;

	if ((token = peek_token (&val,
				 (unsigned *)0, cfile)) == DYNAMIC_BOOTP) {
		skip_token(&val, (unsigned *)0, cfile);
		dynamic = 1;
	}

	/* Get the bottom address in the range... */
	if (!parse_numeric_aggregate (cfile, addr, &len, DOT, 10, 8))
		return;
	memcpy (low.iabuf, addr, len);
	low.len = len;

	/* Only one address? */
	token = peek_token (&val, (unsigned *)0, cfile);
	if (token == SEMI)
		high = low;
	else {
	/* Get the top address in the range... */
		if (!parse_numeric_aggregate (cfile, addr, &len, DOT, 10, 8))
			return;
		memcpy (high.iabuf, addr, len);
		high.len = len;
	}

	token = next_token (&val, (unsigned *)0, cfile);
	if (token != SEMI) {
		parse_warn (cfile, "semicolon expected.");
		skip_to_semi (cfile);
		return;
	}

	if (type == SUBNET_DECL) {
		subnet = group -> subnet;
		share = subnet -> shared_network;
	} else {
		share = group -> shared_network;
		for (subnet = share -> subnets;
		     subnet; subnet = subnet -> next_sibling) {
			net = subnet_number (low, subnet -> netmask);
			if (addr_eq (net, subnet -> net))
				break;
		}
		if (!subnet) {
			parse_warn (cfile, "address range not on network %s",
				    group -> shared_network -> name);
			log_error ("Be sure to place pool statement after %s",
				   "related subnet declarations.");
			return;
		}
	}

	if (!inpool) {
		struct pool *last = (struct pool *)0;

		/* If we're permitting dynamic bootp for this range,
		   then look for a pool with an empty prohibit list and
		   a permit list with one entry that permits all clients. */
		for (pool = share -> pools; pool; pool = pool -> next) {
			if ((!dynamic && !pool -> permit_list && 
			     pool -> prohibit_list &&
			     !pool -> prohibit_list -> next &&
			     (pool -> prohibit_list -> type ==
			      permit_dynamic_bootp_clients)) ||
			    (dynamic && !pool -> prohibit_list &&
			     pool -> permit_list &&
			     !pool -> permit_list -> next &&
			     (pool -> permit_list -> type ==
			      permit_all_clients))) {
  				break;
			}
			last = pool;
		}

		/* If we didn't get a pool, make one. */
		if (!pool) {
			struct permit *p;
			status = pool_allocate (&pool, MDL);
			if (status != ISC_R_SUCCESS)
				log_fatal ("no memory for ad-hoc pool: %s",
					   isc_result_totext (status));
			p = new_permit (MDL);
			if (!p)
				log_fatal ("no memory for ad-hoc permit.");

			/* Dynamic pools permit all clients.   Otherwise
			   we prohibit BOOTP clients. */
			if (dynamic) {
				p -> type = permit_all_clients;
				pool -> permit_list = p;
			} else {
				p -> type = permit_dynamic_bootp_clients;
				pool -> prohibit_list = p;
			}

			if (share -> pools)
				pool_reference (&last -> next, pool, MDL);
			else
				pool_reference (&share -> pools, pool, MDL);
			shared_network_reference (&pool -> shared_network,
						  share, MDL);
			if (!clone_group (&pool -> group, share -> group, MDL))
				log_fatal ("no memory for anon pool group.");
		} else {
			pool = (struct pool *)0;
			if (last)
				pool_reference (&pool, last, MDL);
			else
				pool_reference (&pool, share -> pools, MDL);
		}
	} else {
		pool = (struct pool *)0;
		pool_reference (&pool, inpool, MDL);
	}

#if defined (FAILOVER_PROTOCOL)
	if (pool -> failover_peer && dynamic) {
		/* Doctor, do you think I'm overly sensitive
		   about getting bug reports I can't fix? */
		parse_warn (cfile, "dynamic-bootp flag is %s",
			    "not permitted for address");
		log_error ("range declarations where there is a failover");
		log_error ("peer in scope.   If you wish to declare an");
		log_error ("address range from which dynamic bootp leases");
		log_error ("can be allocated, please declare it within a");
		log_error ("pool declaration that also contains the \"no");
		log_error ("failover\" statement.   The failover protocol");
		log_error ("itself does not permit dynamic bootp - this");
		log_error ("is not a limitation specific to the ISC DHCP");
		log_error ("server.   Please don't ask me to defend this");
		log_error ("until you have read and really tried %s",
			   "to understand");
		log_error ("the failover protocol specification.");

		/* We don't actually bomb at this point - instead,
		   we let parse_lease_file notice the error and
		   bomb at that point - it's easier. */
	}
#endif /* FAILOVER_PROTOCOL */

	/* Create the new address range... */
	new_address_range (cfile, low, high, subnet, pool, lpchain);
	pool_dereference (&pool, MDL);
}

#ifdef DHCPv6
static void
add_ipv6_pool_to_subnet(struct subnet *subnet, u_int16_t type,
			struct iaddr *lo_addr, int bits, int units,
			struct ipv6_pond *pond) {
	struct ipv6_pool *pool;
	struct in6_addr tmp_in6_addr;
	int num_pools;
	struct ipv6_pool **tmp;

	/*
	 * Create our pool.
	 */
	if (lo_addr->len != sizeof(tmp_in6_addr)) {
		log_fatal("Internal error: Attempt to add non-IPv6 address "
			  "to IPv6 shared network.");
	}
	memcpy(&tmp_in6_addr, lo_addr->iabuf, sizeof(tmp_in6_addr));
	pool = NULL;
	if (ipv6_pool_allocate(&pool, type, &tmp_in6_addr,
			       bits, units, MDL) != ISC_R_SUCCESS) {
		log_fatal("Out of memory");
	}

	/*
	 * Add to our global IPv6 pool set.
	 */
	if (add_ipv6_pool(pool) != ISC_R_SUCCESS) {
		log_fatal ("Out of memory");
	}

	/*
	 * Link the pool to its network.
	 */
	pool->subnet = NULL;
	subnet_reference(&pool->subnet, subnet, MDL);
	pool->shared_network = NULL;
	shared_network_reference(&pool->shared_network,
				 subnet->shared_network, MDL);
	pool->ipv6_pond = NULL;
	ipv6_pond_reference(&pool->ipv6_pond, pond, MDL);

	/* 
	 * Increase our array size for ipv6_pools in the pond
	 */
	if (pond->ipv6_pools == NULL) {
		num_pools = 0;
	} else {
		num_pools = 0;
		while (pond->ipv6_pools[num_pools] != NULL) {
			num_pools++;
		}
	}
	tmp = dmalloc(sizeof(struct ipv6_pool *) * (num_pools + 2), MDL);
	if (tmp == NULL) {
		log_fatal("Out of memory");
	}
	if (num_pools > 0) {
		memcpy(tmp, pond->ipv6_pools, 
		       sizeof(struct ipv6_pool *) * num_pools);
	}
	if (pond->ipv6_pools != NULL) {
		dfree(pond->ipv6_pools, MDL);
	}
	pond->ipv6_pools = tmp;

	/* 
	 * Record this pool in our array of pools for this shared network.
	 */
	ipv6_pool_reference(&pond->ipv6_pools[num_pools], pool, MDL);
	pond->ipv6_pools[num_pools+1] = NULL;
}

/*!
 *
 * \brief Find or create a default pond
 *
 * Find or create an ipv6_pond on which to attach the ipv6_pools.  We
 * check the shared network to see if there is a general purpose
 * entry - this will have an empty prohibit list and a permit list
 * with a single entry that permits all clients.  If the shared
 * network doesn't have one of them create it and attach it to
 * the shared network and the return argument.
 * 
 * This function is used when we have a range6 or prefix6 statement
 * inside a subnet6 statement but outside of a pool6 statement.
 * This routine constructs the missing ipv6_pond structure so
 * we always have 
 * shared_network -> ipv6_pond -> ipv6_pool
 *
 * \param[in] group     = a pointer to the group structure from which
 *                        we can find the subnet and shared netowrk
 *                        structures
 * \param[out] ret_pond = a pointer to space for the pointer to
 *                        the structure to return
 *
 * \return
 * void
 */
static void
add_ipv6_pond_to_network(struct group *group,
			 struct ipv6_pond **ret_pond) {

	struct ipv6_pond *pond = NULL, *last = NULL;
	struct permit *p;
	isc_result_t status;
	struct shared_network *shared = group->subnet->shared_network;

	for (pond = shared->ipv6_pond; pond; pond = pond->next) {
		if ((pond->group->statements == group->statements) &&
		    (pond->prohibit_list == NULL) &&
		    (pond->permit_list != NULL) &&
		    (pond->permit_list->next == NULL) &&
		    (pond->permit_list->type == permit_all_clients)) {
			ipv6_pond_reference(ret_pond, pond, MDL);
			return;
		}
		last = pond;
	}

	/* no pond available, make one */
	status = ipv6_pond_allocate(&pond, MDL);
	if (status != ISC_R_SUCCESS) 
		log_fatal ("no memory for ad-hoc ipv6 pond: %s",
			   isc_result_totext (status));
	p = new_permit (MDL);
	if (p == NULL)
		log_fatal ("no memory for ad-hoc ipv6 permit.");

	/* we permit all clients */
	p->type = permit_all_clients;
	pond->permit_list = p;

	/* and attach the pond to the return argument and the shared network */
	ipv6_pond_reference(ret_pond, pond, MDL);

	if (shared->ipv6_pond)
		ipv6_pond_reference(&last->next, pond, MDL);
	else 
		ipv6_pond_reference(&shared->ipv6_pond, pond, MDL);

	shared_network_reference(&pond->shared_network, shared, MDL);
	if (!clone_group (&pond->group, group, MDL))
		log_fatal ("no memory for anon pool group.");

	ipv6_pond_dereference(&pond, MDL);
	return;
}


/* address-range6-declaration :== ip-address6 ip-address6 SEMI
			       | ip-address6 SLASH number SEMI
			       | ip-address6 [SLASH number] TEMPORARY SEMI */

void 
parse_address_range6(struct parse *cfile,
		     struct group *group,
		     struct ipv6_pond *inpond) {
	struct iaddr lo, hi;
	int bits;
	enum dhcp_token token;
	const char *val;
	struct iaddrcidrnetlist *nets, net;
	struct iaddrcidrnetlist *p;
	u_int16_t type = D6O_IA_NA;
	struct ipv6_pond *pond = NULL;

        if (local_family != AF_INET6) {
                parse_warn(cfile, "range6 statement is only supported "
				  "in DHCPv6 mode.");
                skip_to_semi(cfile);
                return;
        }

	/* This is enforced by the caller, this is just a sanity check. */
	if (group->subnet == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	/*
	 * Read starting address.
	 */
	if (!parse_ip6_addr(cfile, &lo)) {
		return;
	}

	/*
	 * zero out the net entry in case we use it
	 */
	memset(&net, 0, sizeof(net));
	net.cidrnet.lo_addr = lo;

	/* 
	 * See if we we're using range or CIDR notation or TEMPORARY
	 */
	token = peek_token(&val, NULL, cfile);
	if (token == SLASH) {
		/*
		 * '/' means CIDR notation, so read the bits we want.
		 */
		skip_token(NULL, NULL, cfile);
		token = next_token(&val, NULL, cfile);
		if (token != NUMBER) { 
			parse_warn(cfile, "expecting number");
			skip_to_semi(cfile);
			return;
		}
		net.cidrnet.bits = atoi(val);
		bits = net.cidrnet.bits;
		if ((bits < 0) || (bits > 128)) {
			parse_warn(cfile, "networks have 0 to 128 bits");
			skip_to_semi(cfile);
			return;
		}

		if (!is_cidr_mask_valid(&net.cidrnet.lo_addr, bits)) {
			parse_warn(cfile, "network mask too short");
			skip_to_semi(cfile);
			return;
		}

		/*
		 * can be temporary (RFC 4941 like)
		 */
		token = peek_token(&val, NULL, cfile);
		if (token == TEMPORARY) {
			if (bits < 64)
				parse_warn(cfile, "temporary mask too short");
			if (bits == 128)
				parse_warn(cfile, "temporary singleton?");
			skip_token(NULL, NULL, cfile);
			type = D6O_IA_TA;
		}

		nets = &net;

	} else if (token == TEMPORARY) {
		/*
		 * temporary (RFC 4941)
		 */
		type = D6O_IA_TA;
		skip_token(NULL, NULL, cfile);
		net.cidrnet.bits = 64;
		if (!is_cidr_mask_valid(&net.cidrnet.lo_addr,
					net.cidrnet.bits)) {
			parse_warn(cfile, "network mask too short");
			skip_to_semi(cfile);
			return;
		}

		nets = &net;

	} else {
		/*
		 * No '/', so we are looking for the end address of 
		 * the IPv6 pool.
		 */
		if (!parse_ip6_addr(cfile, &hi)) {
			return;
		}

		/*
		 * Convert our range to a set of CIDR networks.
		 */
		nets = NULL;
		if (range2cidr(&nets, &lo, &hi) != ISC_R_SUCCESS) {
			log_fatal("Error converting range to CIDR networks");
		}

	}

	/*
	 * See if we have a pond for this set of pools.
	 * If the caller supplied one we use it, otherwise
	 * check the shared network
	 */

	if (inpond != NULL) {
		ipv6_pond_reference(&pond, inpond, MDL);
	} else {
		add_ipv6_pond_to_network(group, &pond);
	}

	/* Now that we have a pond add the nets we have parsed */
	for (p=nets; p != NULL; p=p->next) {
		add_ipv6_pool_to_subnet(group->subnet, type,
					&p->cidrnet.lo_addr, 
					p->cidrnet.bits, 128, pond);
	}

	/* if we allocated a list free it now */
	if (nets != &net) 
		free_iaddrcidrnetlist(&nets);

	ipv6_pond_dereference(&pond, MDL);

	token = next_token(NULL, NULL, cfile);
	if (token != SEMI) {
		parse_warn(cfile, "semicolon expected.");
		skip_to_semi(cfile);
		return;
	}
}

/* prefix6-declaration :== ip-address6 ip-address6 SLASH number SEMI */

void 
parse_prefix6(struct parse *cfile,
	      struct group *group,
	      struct ipv6_pond *inpond) {
	struct iaddr lo, hi;
	int bits;
	enum dhcp_token token;
	const char *val;
	struct iaddrcidrnetlist *nets;
	struct iaddrcidrnetlist *p;
	struct ipv6_pond *pond = NULL;

	if (local_family != AF_INET6) {
		parse_warn(cfile, "prefix6 statement is only supported "
				  "in DHCPv6 mode.");
		skip_to_semi(cfile);
		return;
	}

	/* This is enforced by the caller, so it's just a sanity check. */
	if (group->subnet == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	/*
	 * Read starting and ending address.
	 */
	if (!parse_ip6_addr(cfile, &lo)) {
		return;
	}
	if (!parse_ip6_addr(cfile, &hi)) {
		return;
	}

	/*
	 * Next is '/' number ';'.
	 */
	token = next_token(NULL, NULL, cfile);
	if (token != SLASH) {
		parse_warn(cfile, "expecting '/'");
		if (token != SEMI)
			skip_to_semi(cfile);
		return;
	}
	token = next_token(&val, NULL, cfile);
	if (token != NUMBER) {
		parse_warn(cfile, "expecting number");
		if (token != SEMI)
			skip_to_semi(cfile);
		return;
	}
	bits = atoi(val);
	if ((bits <= 0) || (bits >= 128)) {
		parse_warn(cfile, "networks have 0 to 128 bits (exclusive)");
		return;
	}
	if (!is_cidr_mask_valid(&lo, bits) ||
	    !is_cidr_mask_valid(&hi, bits)) {
		parse_warn(cfile, "network mask too short");
		return;
	}
	token = next_token(NULL, NULL, cfile);
	if (token != SEMI) {
		parse_warn(cfile, "semicolon expected.");
		skip_to_semi(cfile);
		return;
	}

	/*
	 * Convert our range to a set of CIDR networks.
	 */
	nets = NULL;
	if (range2cidr(&nets, &lo, &hi) != ISC_R_SUCCESS) {
		log_fatal("Error converting prefix to CIDR");
	}

	/*
	 * See if we have a pond for this set of pools.
	 * If the caller supplied one we use it, otherwise
	 * check the shared network
	 */

	if (inpond != NULL) {
		ipv6_pond_reference(&pond, inpond, MDL);
	} else {
		add_ipv6_pond_to_network(group, &pond);
	}
		
	for (p = nets; p != NULL; p = p->next) {
		/* Normalize and check. */
		if (p->cidrnet.bits == 128) {
			p->cidrnet.bits = bits;
		}
		if (p->cidrnet.bits > bits) {
			parse_warn(cfile, "impossible mask length");
			continue;
		}
		add_ipv6_pool_to_subnet(group->subnet, D6O_IA_PD,
					&p->cidrnet.lo_addr,
					p->cidrnet.bits, bits, pond);
	}

	free_iaddrcidrnetlist(&nets);
}

/* fixed-prefix6 :== ip6-address SLASH number SEMI */

void
parse_fixed_prefix6(struct parse *cfile, struct host_decl *host_decl) {
	struct iaddrcidrnetlist *ia, **h;
	enum dhcp_token token;
	const char *val;

	/*
	 * Get the head of the fixed-prefix list.
	 */
	h = &host_decl->fixed_prefix;

	/*
	 * Walk to the end.
	 */
	while (*h != NULL) {
		h = &((*h)->next);
	}

	/*
	 * Allocate a new iaddrcidrnetlist structure.
	 */
	ia = dmalloc(sizeof(*ia), MDL);
	if (!ia) {
		log_fatal("Out of memory");
	}

	/*
	 * Parse it.
	 */
	if (!parse_ip6_addr(cfile, &ia->cidrnet.lo_addr)) {
		dfree(ia, MDL);
		return;
	}
	token = next_token(NULL, NULL, cfile);
	if (token != SLASH) {
		dfree(ia, MDL);
		parse_warn(cfile, "expecting '/'");
		if (token != SEMI)
			skip_to_semi(cfile);
		return;
	}
	token = next_token(&val, NULL, cfile);
	if (token != NUMBER) {
		dfree(ia, MDL);
		parse_warn(cfile, "expecting number");
		if (token != SEMI)
			skip_to_semi(cfile);
		return;
	}
	token = next_token(NULL, NULL, cfile);
	if (token != SEMI) {
		dfree(ia, MDL);
		parse_warn(cfile, "semicolon expected.");
		skip_to_semi(cfile);
		return;
	}

	/*
	 * Fill it.
	 */
	ia->cidrnet.bits = atoi(val);
	if ((ia->cidrnet.bits < 0) || (ia->cidrnet.bits > 128)) {
		dfree(ia, MDL);
		parse_warn(cfile, "networks have 0 to 128 bits");
		return;
	}
	if (!is_cidr_mask_valid(&ia->cidrnet.lo_addr, ia->cidrnet.bits)) {
		dfree(ia, MDL);
		parse_warn(cfile, "network mask too short");
		return;
	}

	/*
	 * Store it.
	 */
	*h = ia;
	return;
}

/*!
 *
 * \brief Parse a pool6 statement
 *
 * Pool statements are used to group declarations and permit & deny information
 * with a specific address range.  They must be declared within a shared network
 * or subnet and there may be multiple pools withing a shared network or subnet.
 * Each pool may have a different set of permit or deny options.
 *
 * \param[in] cfile = the configuration file being parsed
 * \param[in] group = the group structure for this pool
 * \param[in] type  = the type of the enclosing statement.  This must be
 *		      SUBNET_DECL for this function.
 *
 * \return
 * void - This function either parses the statement and updates the structures
 *        or it generates an error message and possible halts the program if
 *        it encounters a problem.
 */
void parse_pool6_statement (cfile, group, type)
	struct parse *cfile;
	struct group *group;
	int type;
{
	enum dhcp_token token;
	const char *val;
	int done = 0;
	struct ipv6_pond *pond, **p;
	int declaration = 0;
	isc_result_t status;

	pond = NULL;
	status = ipv6_pond_allocate(&pond, MDL);
	if (status != ISC_R_SUCCESS)
		log_fatal("no memory for pool6: %s",
			  isc_result_totext (status));

	if (type == SUBNET_DECL)
		shared_network_reference(&pond->shared_network,
					 group->subnet->shared_network,
					 MDL);
	else {
		parse_warn(cfile, "Dynamic pool6s are only valid inside "
				  "subnet statements.");
		skip_to_semi(cfile);
		return;
	}

	if (clone_group(&pond->group, group, MDL) == 0)
		log_fatal("can't clone pool6 group.");

	if (parse_lbrace(cfile) == 0) {
		ipv6_pond_dereference(&pond, MDL);
		return;
	}

	do {
		token = peek_token(&val, NULL, cfile);
		switch (token) {
		      case RANGE6:
			skip_token(NULL, NULL, cfile);
			parse_address_range6(cfile, group, pond);
			break;

		      case PREFIX6:
			skip_token(NULL, NULL, cfile);
			parse_prefix6(cfile, group, pond);
			break;

		      case ALLOW:
			skip_token(NULL, NULL, cfile);
			get_permit(cfile, &pond->permit_list, 1,
				   &pond->valid_from, &pond->valid_until);
			break;

		      case DENY:
			skip_token(NULL, NULL, cfile);
			get_permit(cfile, &pond->prohibit_list, 0,
				   &pond->valid_from, &pond->valid_until);
			break;
			
		      case RBRACE:
			skip_token(&val, NULL, cfile);
			done = 1;
			break;

		      case END_OF_FILE:
			/*
			 * We can get to END_OF_FILE if, for instance,
			 * the parse_statement() reads all available tokens
			 * and leaves us at the end.
			 */
			parse_warn(cfile, "unexpected end of file");
			goto cleanup;

		      default:
			declaration = parse_statement(cfile, pond->group,
						      POOL_DECL, NULL,
						      declaration);
			break;
		}
	} while (!done);

	/*
	 * A possible optimization is to see if this pond can be merged into
	 * an already existing pond.  But I'll pass on that for now as we need
	 * to repoint the leases to the other pond which is annoying. SAR
	 */

	/* 
	 * Add this pond to the list (will need updating if we add the
	 * optimization).
	 */

	p = &pond->shared_network->ipv6_pond;
	for (; *p; p = &((*p)->next))
		;
	ipv6_pond_reference(p, pond, MDL);

	/* Don't allow a pool6 declaration with no addresses or
	   prefixes, since it is probably a configuration error. */
	if (pond->ipv6_pools == NULL) {
		parse_warn (cfile, "Pool6 declaration with no %s.",
			    "address range6 or prefix6");
		log_error ("Pool6 declarations must always contain at least");
		log_error ("one range6 or prefix6 statement.");
	}

cleanup:
	ipv6_pond_dereference(&pond, MDL);
}



#endif /* DHCPv6 */

/* allow-deny-keyword :== BOOTP
   			| BOOTING
			| DYNAMIC_BOOTP
			| UNKNOWN_CLIENTS */

int parse_allow_deny (oc, cfile, flag)
	struct option_cache **oc;
	struct parse *cfile;
	int flag;
{
	enum dhcp_token token;
	const char *val;
	unsigned char rf = flag;
	unsigned code;
	struct option *option = NULL;
	struct expression *data = (struct expression *)0;
	int status;

	if (!make_const_data (&data, &rf, 1, 0, 1, MDL))
		return 0;

	token = next_token (&val, (unsigned *)0, cfile);
	switch (token) {
	      case TOKEN_BOOTP:
		code = SV_ALLOW_BOOTP;
		break;

	      case BOOTING:
		code = SV_ALLOW_BOOTING;
		break;

	      case DYNAMIC_BOOTP:
		code = SV_DYNAMIC_BOOTP;
		break;

	      case UNKNOWN_CLIENTS:
		code = SV_BOOT_UNKNOWN_CLIENTS;
		break;

	      case DUPLICATES:
		code = SV_DUPLICATES;
		break;

	      case DECLINES:
		code= SV_DECLINES;
		break;

	      case CLIENT_UPDATES:
		code = SV_CLIENT_UPDATES;
		break;

	      case LEASEQUERY:
		code = SV_LEASEQUERY;
		break;

	      default:
		parse_warn (cfile, "expecting allow/deny key");
		skip_to_semi (cfile);
		return 0;
	}
	/* Reference on option is passed to option cache. */
	if (!option_code_hash_lookup(&option, server_universe.code_hash,
				     &code, 0, MDL))
		log_fatal("Unable to find server option %u (%s:%d).",
			  code, MDL);
	status = option_cache(oc, NULL, data, option, MDL);
	expression_dereference (&data, MDL);
	parse_semi (cfile);
	return status;
}

void
parse_ia_na_declaration(struct parse *cfile) {
#if !defined(DHCPv6)
	parse_warn(cfile, "No DHCPv6 support.");
	skip_to_semi(cfile);
#else /* defined(DHCPv6) */
	enum dhcp_token token;
	struct ia_xx *ia;
	const char *val;
	struct ia_xx *old_ia;
	unsigned int len;
	u_int32_t iaid;
	struct iaddr iaddr;
	binding_state_t state;
	u_int32_t prefer;
	u_int32_t valid;
	TIME end_time;
	struct iasubopt *iaaddr;
	struct ipv6_pool *pool;
	char addr_buf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
	isc_boolean_t newbinding;
	struct binding_scope *scope = NULL;
	struct binding *bnd;
	struct binding_value *nv = NULL;
	struct executable_statement *on_star[2] = {NULL, NULL};
	int lose, i;

        if (local_family != AF_INET6) {
                parse_warn(cfile, "IA_NA is only supported in DHCPv6 mode.");
                skip_to_semi(cfile);
                return;
        }

	token = next_token(&val, &len, cfile);
	if (token != STRING) {
		parse_warn(cfile, "corrupt lease file; "
				  "expecting an iaid+ia_na string");
		skip_to_semi(cfile);
		return;
	}
	if (len < 5) {
		parse_warn(cfile, "corrupt lease file; "
				  "iaid+ia_na string too short");
		skip_to_semi(cfile);
		return;
	}

	memcpy(&iaid, val, 4);
	ia = NULL;
	if (ia_allocate(&ia, iaid, val+4, len-4, MDL) != ISC_R_SUCCESS) {
		log_fatal("Out of memory.");
	}
	ia->ia_type = D6O_IA_NA;

	token = next_token(&val, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "corrupt lease file; expecting left brace");
		skip_to_semi(cfile);
		return;
	}

	for (;;) {
		token = next_token(&val, NULL, cfile);
		if (token == RBRACE) break;

		if (token == CLTT) {
			ia->cltt = parse_date (cfile);
			continue;
		}

		if (token != IAADDR) {
			parse_warn(cfile, "corrupt lease file; "
					  "expecting IAADDR or right brace");
			skip_to_semi(cfile);
			return;
		}

		if (!parse_ip6_addr(cfile, &iaddr)) {
			parse_warn(cfile, "corrupt lease file; "
					  "expecting IPv6 address");
			skip_to_semi(cfile);
			return;
		}

		token = next_token(&val, NULL, cfile);
		if (token != LBRACE) {
			parse_warn(cfile, "corrupt lease file; "
					  "expecting left brace");
			skip_to_semi(cfile);
			return;
		}

		state = FTS_LAST+1;
		prefer = valid = 0;
		end_time = -1;
		for (;;) {
			token = next_token(&val, NULL, cfile);
			if (token == RBRACE) break;

			switch(token) {
				/* Lease binding state. */
			     case BINDING:
				token = next_token(&val, NULL, cfile);
				if (token != STATE) {
					parse_warn(cfile, "corrupt lease file; "
							  "expecting state");
					skip_to_semi(cfile);
					return;
				}
				token = next_token(&val, NULL, cfile);
				switch (token) {
					case TOKEN_ABANDONED:
						state = FTS_ABANDONED;
						break;
					case TOKEN_FREE:
						state = FTS_FREE;
						break;
					case TOKEN_ACTIVE:
						state = FTS_ACTIVE;
						break;
					case TOKEN_EXPIRED:
						state = FTS_EXPIRED;
						break;
					case TOKEN_RELEASED:
						state = FTS_RELEASED;
						break;
					default:
						parse_warn(cfile,
							   "corrupt lease "
							   "file; "
					    		   "expecting a "
							   "binding state.");
						skip_to_semi(cfile);
						return;
				}

				token = next_token(&val, NULL, cfile);
				if (token != SEMI) {
					parse_warn(cfile, "corrupt lease file; "
							  "expecting "
							  "semicolon.");
				}
				break;

				/* Lease preferred lifetime. */
			      case PREFERRED_LIFE:
				token = next_token(&val, NULL, cfile);
				if (token != NUMBER) {
					parse_warn(cfile, "%s is not a valid "
							  "preferred time",
						   val);
					skip_to_semi(cfile);
					continue;
				}
				prefer = atoi (val);

				/*
				 * Currently we peek for the semi-colon to 
				 * allow processing of older lease files that
				 * don't have the semi-colon.  Eventually we
				 * should remove the peeking code.
				 */
				token = peek_token(&val, NULL, cfile);
				if (token == SEMI) {
					skip_token(&val, NULL, cfile);
				} else {
					parse_warn(cfile,
						   "corrupt lease file; "
						   "expecting semicolon.");
				}
				break;

				/* Lease valid lifetime. */
			      case MAX_LIFE:
				token = next_token(&val, NULL, cfile);
				if (token != NUMBER) {
					parse_warn(cfile, "%s is not a valid "
							  "max time",
						   val);
					skip_to_semi(cfile);
					continue;
				}
				valid = atoi (val);

				/*
				 * Currently we peek for the semi-colon to 
				 * allow processing of older lease files that
				 * don't have the semi-colon.  Eventually we
				 * should remove the peeking code.
				 */
				token = peek_token(&val, NULL, cfile);
				if (token == SEMI) {
					skip_token(&val, NULL, cfile);
				} else {
					parse_warn(cfile,
						   "corrupt lease file; "
						   "expecting semicolon.");
				}
				break;

				/* Lease expiration time. */
			      case ENDS:
				end_time = parse_date(cfile);
				break;

				/* Lease binding scopes. */
			      case TOKEN_SET:
				token = next_token(&val, NULL, cfile);
				if ((token != NAME) &&
				    (token != NUMBER_OR_NAME)) {
					parse_warn(cfile, "%s is not a valid "
							  "variable name",
						   val);
					skip_to_semi(cfile);
					continue;
				}

				if (scope != NULL)
					bnd = find_binding(scope, val);
				else {
					if (!binding_scope_allocate(&scope,
								    MDL)) {
						log_fatal("Out of memory for "
							  "lease binding "
							  "scope.");
					}

					bnd = NULL;
				}

				if (bnd == NULL) {
					bnd = dmalloc(sizeof(*bnd),
							  MDL);
					if (bnd == NULL) {
						log_fatal("No memory for "
							  "lease binding.");
					}

					bnd->name = dmalloc(strlen(val) + 1,
							    MDL);
					if (bnd->name == NULL) {
						log_fatal("No memory for "
							  "binding name.");
					}
					strcpy(bnd->name, val);

					newbinding = ISC_TRUE;
				} else {
					newbinding = ISC_FALSE;
				}

				if (!binding_value_allocate(&nv, MDL)) {
					log_fatal("no memory for binding "
						  "value.");
				}

				token = next_token(NULL, NULL, cfile);
				if (token != EQUAL) {
					parse_warn(cfile, "expecting '=' in "
							  "set statement.");
					goto binding_err;
				}

				if (!parse_binding_value(cfile, nv)) {
				      binding_err:
					binding_value_dereference(&nv, MDL);
					binding_scope_dereference(&scope, MDL);
					return;
				}

				if (newbinding) {
					binding_value_reference(&bnd->value,
								nv, MDL);
					bnd->next = scope->bindings;
					scope->bindings = bnd;
				} else {
					binding_value_dereference(&bnd->value,
								  MDL);
					binding_value_reference(&bnd->value,
								nv, MDL);
				}

				binding_value_dereference(&nv, MDL);
				parse_semi(cfile);
				break;

			      case ON:
				lose = 0;
				/*
				 * Depending on the user config we may
				 * have one or two on statements.  We
				 * need to save information about both
				 * of them until we allocate the
				 * iasubopt to hold them.
				 */
				if (on_star[0] == NULL) {
					if (!parse_on_statement (&on_star[0],
								 cfile,
								 &lose)) {
						parse_warn(cfile,
							   "corrupt lease "
							   "file; bad ON "
							   "statement");
						skip_to_rbrace (cfile, 1);
						return;
					}
				} else {
					if (!parse_on_statement (&on_star[1],
								 cfile,
								 &lose)) {
						parse_warn(cfile,
							   "corrupt lease "
							   "file; bad ON "
							   "statement");
						skip_to_rbrace (cfile, 1);
						return;
					}
				}

				break;
			  
			      default:
				parse_warn(cfile, "corrupt lease file; "
						  "expecting ia_na contents, "
						  "got '%s'", val);
				skip_to_semi(cfile);
				continue;
			}
		}

		if (state == FTS_LAST+1) {
			parse_warn(cfile, "corrupt lease file; "
					  "missing state in iaaddr");
			return;
		}
		if (end_time == -1) {
			parse_warn(cfile, "corrupt lease file; "
					  "missing end time in iaaddr");
			return;
		}

		iaaddr = NULL;
		if (iasubopt_allocate(&iaaddr, MDL) != ISC_R_SUCCESS) {
			log_fatal("Out of memory.");
		}
		memcpy(&iaaddr->addr, iaddr.iabuf, sizeof(iaaddr->addr));
		iaaddr->plen = 0;
		iaaddr->state = state;
		iaaddr->prefer = prefer;
		iaaddr->valid = valid;
		if (iaaddr->state == FTS_RELEASED)
			iaaddr->hard_lifetime_end_time = end_time;

		if (scope != NULL) {
			binding_scope_reference(&iaaddr->scope, scope, MDL);
			binding_scope_dereference(&scope, MDL);
		}

		/*
		 * Check on both on statements.  Because of how we write the
		 * lease file we know which is which if we have two but it's
		 * easier to write the code to be independent.  We do assume
		 * that the statements won't overlap.
		 */
		for (i = 0;
		     (i < 2) && on_star[i] != NULL ;
		     i++) {
			if ((on_star[i]->data.on.evtypes & ON_EXPIRY) &&
			    on_star[i]->data.on.statements) {
				executable_statement_reference
					(&iaaddr->on_star.on_expiry,
					 on_star[i]->data.on.statements, MDL);
			}
			if ((on_star[i]->data.on.evtypes & ON_RELEASE) &&
			    on_star[i]->data.on.statements) {
				executable_statement_reference
					(&iaaddr->on_star.on_release,
					 on_star[i]->data.on.statements, MDL);
			}
			executable_statement_dereference (&on_star[i], MDL);
		}
			
		/* find the pool this address is in */
		pool = NULL;
		if (find_ipv6_pool(&pool, D6O_IA_NA,
				   &iaaddr->addr) != ISC_R_SUCCESS) {
			inet_ntop(AF_INET6, &iaaddr->addr,
				  addr_buf, sizeof(addr_buf));
			parse_warn(cfile, "no pool found for address %s",
				   addr_buf);
			return;
		}

		/* remove old information */
		if (cleanup_lease6(ia_na_active, pool,
				   iaaddr, ia) != ISC_R_SUCCESS) {
			inet_ntop(AF_INET6, &iaaddr->addr,
				  addr_buf, sizeof(addr_buf));
			parse_warn(cfile, "duplicate na lease for address %s",
				   addr_buf);
		}

		/*
		 * if we like the lease we add it to our various structues
		 * otherwise we leave it and it will get cleaned when we
		 * do the iasubopt_dereference.
		 */
		if ((state == FTS_ACTIVE) || (state == FTS_ABANDONED)) {
			ia_add_iasubopt(ia, iaaddr, MDL);
			ia_reference(&iaaddr->ia, ia, MDL);
			add_lease6(pool, iaaddr, end_time);
		}

		iasubopt_dereference(&iaaddr, MDL);
		ipv6_pool_dereference(&pool, MDL);
	}

	/*
	 * If we have an existing record for this IA_NA, remove it.
	 */
	old_ia = NULL;
	if (ia_hash_lookup(&old_ia, ia_na_active,
			   (unsigned char *)ia->iaid_duid.data,
			   ia->iaid_duid.len, MDL)) {
		ia_hash_delete(ia_na_active, 
			       (unsigned char *)ia->iaid_duid.data,
			       ia->iaid_duid.len, MDL);
		ia_dereference(&old_ia, MDL);
	}

	/*
	 * If we have addresses, add this, otherwise don't bother.
	 */
	if (ia->num_iasubopt > 0) {
		ia_hash_add(ia_na_active, 
			    (unsigned char *)ia->iaid_duid.data,
			    ia->iaid_duid.len, ia, MDL);
	}
	ia_dereference(&ia, MDL);
#endif /* defined(DHCPv6) */
}

void
parse_ia_ta_declaration(struct parse *cfile) {
#if !defined(DHCPv6)
	parse_warn(cfile, "No DHCPv6 support.");
	skip_to_semi(cfile);
#else /* defined(DHCPv6) */
	enum dhcp_token token;
	struct ia_xx *ia;
	const char *val;
	struct ia_xx *old_ia;
	unsigned int len;
	u_int32_t iaid;
	struct iaddr iaddr;
	binding_state_t state;
	u_int32_t prefer;
	u_int32_t valid;
	TIME end_time;
	struct iasubopt *iaaddr;
	struct ipv6_pool *pool;
	char addr_buf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
	isc_boolean_t newbinding;
	struct binding_scope *scope = NULL;
	struct binding *bnd;
	struct binding_value *nv = NULL;
	struct executable_statement *on_star[2] = {NULL, NULL};
	int lose, i;

        if (local_family != AF_INET6) {
                parse_warn(cfile, "IA_TA is only supported in DHCPv6 mode.");
                skip_to_semi(cfile);
                return;
        }

	token = next_token(&val, &len, cfile);
	if (token != STRING) {
		parse_warn(cfile, "corrupt lease file; "
				  "expecting an iaid+ia_ta string");
		skip_to_semi(cfile);
		return;
	}
	if (len < 5) {
		parse_warn(cfile, "corrupt lease file; "
				  "iaid+ia_ta string too short");
		skip_to_semi(cfile);
		return;
	}

	memcpy(&iaid, val, 4);
	ia = NULL;
	if (ia_allocate(&ia, iaid, val+4, len-4, MDL) != ISC_R_SUCCESS) {
		log_fatal("Out of memory.");
	}
	ia->ia_type = D6O_IA_TA;

	token = next_token(&val, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "corrupt lease file; expecting left brace");
		skip_to_semi(cfile);
		return;
	}

	for (;;) {
		token = next_token(&val, NULL, cfile);
		if (token == RBRACE) break;

		if (token == CLTT) {
			ia->cltt = parse_date (cfile);
			continue;
		}

		if (token != IAADDR) {
			parse_warn(cfile, "corrupt lease file; "
					  "expecting IAADDR or right brace");
			skip_to_semi(cfile);
			return;
		}

		if (!parse_ip6_addr(cfile, &iaddr)) {
			parse_warn(cfile, "corrupt lease file; "
					  "expecting IPv6 address");
			skip_to_semi(cfile);
			return;
		}

		token = next_token(&val, NULL, cfile);
		if (token != LBRACE) {
			parse_warn(cfile, "corrupt lease file; "
					  "expecting left brace");
			skip_to_semi(cfile);
			return;
		}

		state = FTS_LAST+1;
		prefer = valid = 0;
		end_time = -1;
		for (;;) {
			token = next_token(&val, NULL, cfile);
			if (token == RBRACE) break;

			switch(token) {
				/* Lease binding state. */
			     case BINDING:
				token = next_token(&val, NULL, cfile);
				if (token != STATE) {
					parse_warn(cfile, "corrupt lease file; "
							  "expecting state");
					skip_to_semi(cfile);
					return;
				}
				token = next_token(&val, NULL, cfile);
				switch (token) {
					case TOKEN_ABANDONED:
						state = FTS_ABANDONED;
						break;
					case TOKEN_FREE:
						state = FTS_FREE;
						break;
					case TOKEN_ACTIVE:
						state = FTS_ACTIVE;
						break;
					case TOKEN_EXPIRED:
						state = FTS_EXPIRED;
						break;
					case TOKEN_RELEASED:
						state = FTS_RELEASED;
						break;
					default:
						parse_warn(cfile,
							   "corrupt lease "
							   "file; "
					    		   "expecting a "
							   "binding state.");
						skip_to_semi(cfile);
						return;
				}

				token = next_token(&val, NULL, cfile);
				if (token != SEMI) {
					parse_warn(cfile, "corrupt lease file; "
							  "expecting "
							  "semicolon.");
				}
				break;

				/* Lease preferred lifetime. */
			      case PREFERRED_LIFE:
				token = next_token(&val, NULL, cfile);
				if (token != NUMBER) {
					parse_warn(cfile, "%s is not a valid "
							  "preferred time",
						   val);
					skip_to_semi(cfile);
					continue;
				}
				prefer = atoi (val);

				/*
				 * Currently we peek for the semi-colon to 
				 * allow processing of older lease files that
				 * don't have the semi-colon.  Eventually we
				 * should remove the peeking code.
				 */
				token = peek_token(&val, NULL, cfile);
				if (token == SEMI) {
					skip_token(&val, NULL, cfile);
				} else {
					parse_warn(cfile,
						   "corrupt lease file; "
						   "expecting semicolon.");
				}
				break;

				/* Lease valid lifetime. */
			      case MAX_LIFE:
				token = next_token(&val, NULL, cfile);
				if (token != NUMBER) {
					parse_warn(cfile, "%s is not a valid "
							  "max time",
						   val);
					skip_to_semi(cfile);
					continue;
				}
				valid = atoi (val);

				/*
				 * Currently we peek for the semi-colon to 
				 * allow processing of older lease files that
				 * don't have the semi-colon.  Eventually we
				 * should remove the peeking code.
				 */
				token = peek_token(&val, NULL, cfile);
				if (token == SEMI) {
					skip_token(&val, NULL, cfile);
				} else {
					parse_warn(cfile,
						   "corrupt lease file; "
						   "expecting semicolon.");
				}
				break;

				/* Lease expiration time. */
			      case ENDS:
				end_time = parse_date(cfile);
				break;

				/* Lease binding scopes. */
			      case TOKEN_SET:
				token = next_token(&val, NULL, cfile);
				if ((token != NAME) &&
				    (token != NUMBER_OR_NAME)) {
					parse_warn(cfile, "%s is not a valid "
							  "variable name",
						   val);
					skip_to_semi(cfile);
					continue;
				}

				if (scope != NULL)
					bnd = find_binding(scope, val);
				else {
					if (!binding_scope_allocate(&scope,
								    MDL)) {
						log_fatal("Out of memory for "
							  "lease binding "
							  "scope.");
					}

					bnd = NULL;
				}

				if (bnd == NULL) {
					bnd = dmalloc(sizeof(*bnd),
							  MDL);
					if (bnd == NULL) {
						log_fatal("No memory for "
							  "lease binding.");
					}

					bnd->name = dmalloc(strlen(val) + 1,
							    MDL);
					if (bnd->name == NULL) {
						log_fatal("No memory for "
							  "binding name.");
					}
					strcpy(bnd->name, val);

					newbinding = ISC_TRUE;
				} else {
					newbinding = ISC_FALSE;
				}

				if (!binding_value_allocate(&nv, MDL)) {
					log_fatal("no memory for binding "
						  "value.");
				}

				token = next_token(NULL, NULL, cfile);
				if (token != EQUAL) {
					parse_warn(cfile, "expecting '=' in "
							  "set statement.");
					goto binding_err;
				}

				if (!parse_binding_value(cfile, nv)) {
				      binding_err:
					binding_value_dereference(&nv, MDL);
					binding_scope_dereference(&scope, MDL);
					return;
				}

				if (newbinding) {
					binding_value_reference(&bnd->value,
								nv, MDL);
					bnd->next = scope->bindings;
					scope->bindings = bnd;
				} else {
					binding_value_dereference(&bnd->value,
								  MDL);
					binding_value_reference(&bnd->value,
								nv, MDL);
				}

				binding_value_dereference(&nv, MDL);
				parse_semi(cfile);
				break;

			      case ON:
				lose = 0;
				/*
				 * Depending on the user config we may
				 * have one or two on statements.  We
				 * need to save information about both
				 * of them until we allocate the
				 * iasubopt to hold them.
				 */
				if (on_star[0] == NULL) {
					if (!parse_on_statement (&on_star[0],
								 cfile,
								 &lose)) {
						parse_warn(cfile,
							   "corrupt lease "
							   "file; bad ON "
							   "statement");
						skip_to_rbrace (cfile, 1);
						return;
					}
				} else {
					if (!parse_on_statement (&on_star[1],
								 cfile,
								 &lose)) {
						parse_warn(cfile,
							   "corrupt lease "
							   "file; bad ON "
							   "statement");
						skip_to_rbrace (cfile, 1);
						return;
					}
				}
					
				break;
			  
			      default:
				parse_warn(cfile, "corrupt lease file; "
						  "expecting ia_ta contents, "
						  "got '%s'", val);
				skip_to_semi(cfile);
				continue;
			}
		}

		if (state == FTS_LAST+1) {
			parse_warn(cfile, "corrupt lease file; "
					  "missing state in iaaddr");
			return;
		}
		if (end_time == -1) {
			parse_warn(cfile, "corrupt lease file; "
					  "missing end time in iaaddr");
			return;
		}

		iaaddr = NULL;
		if (iasubopt_allocate(&iaaddr, MDL) != ISC_R_SUCCESS) {
			log_fatal("Out of memory.");
		}
		memcpy(&iaaddr->addr, iaddr.iabuf, sizeof(iaaddr->addr));
		iaaddr->plen = 0;
		iaaddr->state = state;
		iaaddr->prefer = prefer;
		iaaddr->valid = valid;
		if (iaaddr->state == FTS_RELEASED)
			iaaddr->hard_lifetime_end_time = end_time;

		if (scope != NULL) {
			binding_scope_reference(&iaaddr->scope, scope, MDL);
			binding_scope_dereference(&scope, MDL);
		}

		/*
		 * Check on both on statements.  Because of how we write the
		 * lease file we know which is which if we have two but it's
		 * easier to write the code to be independent.  We do assume
		 * that the statements won't overlap.
		 */
		for (i = 0;
		     (i < 2) && on_star[i] != NULL ;
		     i++) {
			if ((on_star[i]->data.on.evtypes & ON_EXPIRY) &&
			    on_star[i]->data.on.statements) {
				executable_statement_reference
					(&iaaddr->on_star.on_expiry,
					 on_star[i]->data.on.statements, MDL);
			}
			if ((on_star[i]->data.on.evtypes & ON_RELEASE) &&
			    on_star[i]->data.on.statements) {
				executable_statement_reference
					(&iaaddr->on_star.on_release,
					 on_star[i]->data.on.statements, MDL);
			}
			executable_statement_dereference (&on_star[i], MDL);
		}
			
		/* find the pool this address is in */
		pool = NULL;
		if (find_ipv6_pool(&pool, D6O_IA_TA,
				   &iaaddr->addr) != ISC_R_SUCCESS) {
			inet_ntop(AF_INET6, &iaaddr->addr,
				  addr_buf, sizeof(addr_buf));
			parse_warn(cfile, "no pool found for address %s",
				   addr_buf);
			return;
		}

		/* remove old information */
		if (cleanup_lease6(ia_ta_active, pool,
				   iaaddr, ia) != ISC_R_SUCCESS) {
			inet_ntop(AF_INET6, &iaaddr->addr,
				  addr_buf, sizeof(addr_buf));
			parse_warn(cfile, "duplicate ta lease for address %s",
				   addr_buf);
		}

		/*
		 * if we like the lease we add it to our various structues
		 * otherwise we leave it and it will get cleaned when we
		 * do the iasubopt_dereference.
		 */
		if ((state == FTS_ACTIVE) || (state == FTS_ABANDONED)) {
			ia_add_iasubopt(ia, iaaddr, MDL);
			ia_reference(&iaaddr->ia, ia, MDL);
			add_lease6(pool, iaaddr, end_time);
		}

		ipv6_pool_dereference(&pool, MDL);
		iasubopt_dereference(&iaaddr, MDL);
	}

	/*
	 * If we have an existing record for this IA_TA, remove it.
	 */
	old_ia = NULL;
	if (ia_hash_lookup(&old_ia, ia_ta_active,
			   (unsigned char *)ia->iaid_duid.data,
			   ia->iaid_duid.len, MDL)) {
		ia_hash_delete(ia_ta_active, 
			       (unsigned char *)ia->iaid_duid.data,
			       ia->iaid_duid.len, MDL);
		ia_dereference(&old_ia, MDL);
	}

	/*
	 * If we have addresses, add this, otherwise don't bother.
	 */
	if (ia->num_iasubopt > 0) {
		ia_hash_add(ia_ta_active, 
			    (unsigned char *)ia->iaid_duid.data,
			    ia->iaid_duid.len, ia, MDL);
	}
	ia_dereference(&ia, MDL);
#endif /* defined(DHCPv6) */
}

void
parse_ia_pd_declaration(struct parse *cfile) {
#if !defined(DHCPv6)
	parse_warn(cfile, "No DHCPv6 support.");
	skip_to_semi(cfile);
#else /* defined(DHCPv6) */
	enum dhcp_token token;
	struct ia_xx *ia;
	const char *val;
	struct ia_xx *old_ia;
	unsigned int len;
	u_int32_t iaid;
	struct iaddr iaddr;
	u_int8_t plen;
	binding_state_t state;
	u_int32_t prefer;
	u_int32_t valid;
	TIME end_time;
	struct iasubopt *iapref;
	struct ipv6_pool *pool;
	char addr_buf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
	isc_boolean_t newbinding;
	struct binding_scope *scope = NULL;
	struct binding *bnd;
	struct binding_value *nv = NULL;
	struct executable_statement *on_star[2] = {NULL, NULL};
	int lose, i;

        if (local_family != AF_INET6) {
                parse_warn(cfile, "IA_PD is only supported in DHCPv6 mode.");
                skip_to_semi(cfile);
                return;
        }

	token = next_token(&val, &len, cfile);
	if (token != STRING) {
		parse_warn(cfile, "corrupt lease file; "
				  "expecting an iaid+ia_pd string");
		skip_to_semi(cfile);
		return;
	}
	if (len < 5) {
		parse_warn(cfile, "corrupt lease file; "
				  "iaid+ia_pd string too short");
		skip_to_semi(cfile);
		return;
	}

	memcpy(&iaid, val, 4);
	ia = NULL;
	if (ia_allocate(&ia, iaid, val+4, len-4, MDL) != ISC_R_SUCCESS) {
		log_fatal("Out of memory.");
	}
	ia->ia_type = D6O_IA_PD;

	token = next_token(&val, NULL, cfile);
	if (token != LBRACE) {
		parse_warn(cfile, "corrupt lease file; expecting left brace");
		skip_to_semi(cfile);
		return;
	}

	for (;;) {
		token = next_token(&val, NULL, cfile);
		if (token == RBRACE) break;

		if (token == CLTT) {
			ia->cltt = parse_date (cfile);
			continue;
		}

		if (token != IAPREFIX) {
			parse_warn(cfile, "corrupt lease file; expecting "
				   "IAPREFIX or right brace");
			skip_to_semi(cfile);
			return;
		}

		if (!parse_ip6_prefix(cfile, &iaddr, &plen)) {
			parse_warn(cfile, "corrupt lease file; "
					  "expecting IPv6 prefix");
			skip_to_semi(cfile);
			return;
		}

		token = next_token(&val, NULL, cfile);
		if (token != LBRACE) {
			parse_warn(cfile, "corrupt lease file; "
					  "expecting left brace");
			skip_to_semi(cfile);
			return;
		}

		state = FTS_LAST+1;
		prefer = valid = 0;
		end_time = -1;
		for (;;) {
			token = next_token(&val, NULL, cfile);
			if (token == RBRACE) break;

			switch(token) {
				/* Prefix binding state. */
			     case BINDING:
				token = next_token(&val, NULL, cfile);
				if (token != STATE) {
					parse_warn(cfile, "corrupt lease file; "
							  "expecting state");
					skip_to_semi(cfile);
					return;
				}
				token = next_token(&val, NULL, cfile);
				switch (token) {
					case TOKEN_ABANDONED:
						state = FTS_ABANDONED;
						break;
					case TOKEN_FREE:
						state = FTS_FREE;
						break;
					case TOKEN_ACTIVE:
						state = FTS_ACTIVE;
						break;
					case TOKEN_EXPIRED:
						state = FTS_EXPIRED;
						break;
					case TOKEN_RELEASED:
						state = FTS_RELEASED;
						break;
					default:
						parse_warn(cfile,
							   "corrupt lease "
							   "file; "
					    		   "expecting a "
							   "binding state.");
						skip_to_semi(cfile);
						return;
				}

				token = next_token(&val, NULL, cfile);
				if (token != SEMI) {
					parse_warn(cfile, "corrupt lease file; "
							  "expecting "
							  "semicolon.");
				}
				break;

				/* Lease preferred lifetime. */
			      case PREFERRED_LIFE:
				token = next_token(&val, NULL, cfile);
				if (token != NUMBER) {
					parse_warn(cfile, "%s is not a valid "
							  "preferred time",
						   val);
					skip_to_semi(cfile);
					continue;
				}
				prefer = atoi (val);

				/*
				 * Currently we peek for the semi-colon to 
				 * allow processing of older lease files that
				 * don't have the semi-colon.  Eventually we
				 * should remove the peeking code.
				 */
				token = peek_token(&val, NULL, cfile);
				if (token == SEMI) {
					skip_token(&val, NULL, cfile);
				} else {
					parse_warn(cfile,
						   "corrupt lease file; "
						   "expecting semicolon.");
				}
				break;

				/* Lease valid lifetime. */
			      case MAX_LIFE:
				token = next_token(&val, NULL, cfile);
				if (token != NUMBER) {
					parse_warn(cfile, "%s is not a valid "
							  "max time",
						   val);
					skip_to_semi(cfile);
					continue;
				}
				valid = atoi (val);

				/*
				 * Currently we peek for the semi-colon to 
				 * allow processing of older lease files that
				 * don't have the semi-colon.  Eventually we
				 * should remove the peeking code.
				 */
				token = peek_token(&val, NULL, cfile);
				if (token == SEMI) {
					skip_token(&val, NULL, cfile);
				} else {
					parse_warn(cfile,
						   "corrupt lease file; "
						   "expecting semicolon.");
				}
				break;

				/* Prefix expiration time. */
			      case ENDS:
				end_time = parse_date(cfile);
				break;

				/* Prefix binding scopes. */
			      case TOKEN_SET:
				token = next_token(&val, NULL, cfile);
				if ((token != NAME) &&
				    (token != NUMBER_OR_NAME)) {
					parse_warn(cfile, "%s is not a valid "
							  "variable name",
						   val);
					skip_to_semi(cfile);
					continue;
				}

				if (scope != NULL)
					bnd = find_binding(scope, val);
				else {
					if (!binding_scope_allocate(&scope,
								    MDL)) {
						log_fatal("Out of memory for "
							  "lease binding "
							  "scope.");
					}

					bnd = NULL;
				}

				if (bnd == NULL) {
					bnd = dmalloc(sizeof(*bnd),
							  MDL);
					if (bnd == NULL) {
						log_fatal("No memory for "
							  "prefix binding.");
					}

					bnd->name = dmalloc(strlen(val) + 1,
							    MDL);
					if (bnd->name == NULL) {
						log_fatal("No memory for "
							  "binding name.");
					}
					strcpy(bnd->name, val);

					newbinding = ISC_TRUE;
				} else {
					newbinding = ISC_FALSE;
				}

				if (!binding_value_allocate(&nv, MDL)) {
					log_fatal("no memory for binding "
						  "value.");
				}

				token = next_token(NULL, NULL, cfile);
				if (token != EQUAL) {
					parse_warn(cfile, "expecting '=' in "
							  "set statement.");
					goto binding_err;
				}

				if (!parse_binding_value(cfile, nv)) {
				      binding_err:
					binding_value_dereference(&nv, MDL);
					binding_scope_dereference(&scope, MDL);
					return;
				}

				if (newbinding) {
					binding_value_reference(&bnd->value,
								nv, MDL);
					bnd->next = scope->bindings;
					scope->bindings = bnd;
				} else {
					binding_value_dereference(&bnd->value,
								  MDL);
					binding_value_reference(&bnd->value,
								nv, MDL);
				}

				binding_value_dereference(&nv, MDL);
				parse_semi(cfile);
				break;

			      case ON:
				lose = 0;
				/*
				 * Depending on the user config we may
				 * have one or two on statements.  We
				 * need to save information about both
				 * of them until we allocate the
				 * iasubopt to hold them.
				 */
				if (on_star[0] == NULL) {
					if (!parse_on_statement (&on_star[0],
								 cfile,
								 &lose)) {
						parse_warn(cfile,
							   "corrupt lease "
							   "file; bad ON "
							   "statement");
						skip_to_rbrace (cfile, 1);
						return;
					}
				} else {
					if (!parse_on_statement (&on_star[1],
								 cfile,
								 &lose)) {
						parse_warn(cfile,
							   "corrupt lease "
							   "file; bad ON "
							   "statement");
						skip_to_rbrace (cfile, 1);
						return;
					}
				}

				break;
			  
			      default:
				parse_warn(cfile, "corrupt lease file; "
						  "expecting ia_pd contents, "
						  "got '%s'", val);
				skip_to_semi(cfile);
				continue;
			}
		}

		if (state == FTS_LAST+1) {
			parse_warn(cfile, "corrupt lease file; "
					  "missing state in iaprefix");
			return;
		}
		if (end_time == -1) {
			parse_warn(cfile, "corrupt lease file; "
					  "missing end time in iaprefix");
			return;
		}

		iapref = NULL;
		if (iasubopt_allocate(&iapref, MDL) != ISC_R_SUCCESS) {
			log_fatal("Out of memory.");
		}
		memcpy(&iapref->addr, iaddr.iabuf, sizeof(iapref->addr));
		iapref->plen = plen;
		iapref->state = state;
		iapref->prefer = prefer;
		iapref->valid = valid;
		if (iapref->state == FTS_RELEASED)
			iapref->hard_lifetime_end_time = end_time;

		if (scope != NULL) {
			binding_scope_reference(&iapref->scope, scope, MDL);
			binding_scope_dereference(&scope, MDL);
		}

		/*
		 * Check on both on statements.  Because of how we write the
		 * lease file we know which is which if we have two but it's
		 * easier to write the code to be independent.  We do assume
		 * that the statements won't overlap.
		 */
		for (i = 0;
		     (i < 2) && on_star[i] != NULL ;
		     i++) {
			if ((on_star[i]->data.on.evtypes & ON_EXPIRY) &&
			    on_star[i]->data.on.statements) {
				executable_statement_reference
					(&iapref->on_star.on_expiry,
					 on_star[i]->data.on.statements, MDL);
			}
			if ((on_star[i]->data.on.evtypes & ON_RELEASE) &&
			    on_star[i]->data.on.statements) {
				executable_statement_reference
					(&iapref->on_star.on_release,
					 on_star[i]->data.on.statements, MDL);
			}
			executable_statement_dereference (&on_star[i], MDL);
		}
			
		/* find the pool this address is in */
		pool = NULL;
		if (find_ipv6_pool(&pool, D6O_IA_PD,
				   &iapref->addr) != ISC_R_SUCCESS) {
			inet_ntop(AF_INET6, &iapref->addr,
				  addr_buf, sizeof(addr_buf));
			parse_warn(cfile, "no pool found for address %s",
				   addr_buf);
			return;
		}

		/* remove old information */
		if (cleanup_lease6(ia_pd_active, pool,
				   iapref, ia) != ISC_R_SUCCESS) {
			inet_ntop(AF_INET6, &iapref->addr,
				  addr_buf, sizeof(addr_buf));
			parse_warn(cfile, "duplicate pd lease for address %s",
				   addr_buf);
		}

		/*
		 * if we like the lease we add it to our various structues
		 * otherwise we leave it and it will get cleaned when we
		 * do the iasubopt_dereference.
		 */
		if ((state == FTS_ACTIVE) || (state == FTS_ABANDONED)) {
			ia_add_iasubopt(ia, iapref, MDL);
			ia_reference(&iapref->ia, ia, MDL);
			add_lease6(pool, iapref, end_time);
		}

		ipv6_pool_dereference(&pool, MDL);
		iasubopt_dereference(&iapref, MDL);
	}

	/*
	 * If we have an existing record for this IA_PD, remove it.
	 */
	old_ia = NULL;
	if (ia_hash_lookup(&old_ia, ia_pd_active,
			   (unsigned char *)ia->iaid_duid.data,
			   ia->iaid_duid.len, MDL)) {
		ia_hash_delete(ia_pd_active,
			       (unsigned char *)ia->iaid_duid.data,
			       ia->iaid_duid.len, MDL);
		ia_dereference(&old_ia, MDL);
	}

	/*
	 * If we have prefixes, add this, otherwise don't bother.
	 */
	if (ia->num_iasubopt > 0) {
		ia_hash_add(ia_pd_active, 
			    (unsigned char *)ia->iaid_duid.data,
			    ia->iaid_duid.len, ia, MDL);
	}
	ia_dereference(&ia, MDL);
#endif /* defined(DHCPv6) */
}

#ifdef DHCPv6 
/*
 * When we parse a server-duid statement in a lease file, we are 
 * looking at the saved server DUID from a previous run. In this case
 * we expect it to be followed by the binary representation of the
 * DUID stored in a string:
 *
 * server-duid "\000\001\000\001\015\221\034JRT\000\0224Y";
 */
void 
parse_server_duid(struct parse *cfile) {
	enum dhcp_token token;
	const char *val;
	unsigned int len;
	struct data_string duid;

	token = next_token(&val, &len, cfile);
	if (token != STRING) {
		parse_warn(cfile, "corrupt lease file; expecting a DUID");
		skip_to_semi(cfile);
		return;
	}

	memset(&duid, 0, sizeof(duid));
	duid.len = len;
	if (!buffer_allocate(&duid.buffer, duid.len, MDL)) {
		log_fatal("Out of memory storing DUID");
	}
	duid.data = (unsigned char *)duid.buffer->data;
	memcpy(duid.buffer->data, val, len);

	set_server_duid(&duid);

	data_string_forget(&duid, MDL);

	token = next_token(&val, &len, cfile);
	if (token != SEMI) {
		parse_warn(cfile, "corrupt lease file; expecting a semicolon");
		skip_to_semi(cfile);
		return;
	}
}

/*
 * When we parse a server-duid statement in a config file, we will
 * have the type of the server DUID to generate, and possibly the
 * actual value defined.
 *
 * server-duid llt;
 * server-duid llt ethernet|ieee802|fddi 213982198 00:16:6F:49:7D:9B;
 * server-duid ll;
 * server-duid ll ethernet|ieee802|fddi 00:16:6F:49:7D:9B;
 * server-duid en 2495 "enterprise-specific-identifier-1234";
 */
void 
parse_server_duid_conf(struct parse *cfile) {
	enum dhcp_token token;
	const char *val;
	unsigned int len;
	u_int32_t enterprise_number;
	int ll_type;
	struct data_string ll_addr;
	u_int32_t llt_time;
	struct data_string duid;
	int duid_type_num;

	/*
	 * Consume the SERVER_DUID token.
	 */
	skip_token(NULL, NULL, cfile);

	/*
	 * Obtain the DUID type.
	 */
	token = next_token(&val, NULL, cfile);

	/* 
	 * Enterprise is the easiest - enterprise number and raw data
	 * are required.
	 */
	if (token == EN) {
		/*
		 * Get enterprise number and identifier.
		 */
		token = next_token(&val, NULL, cfile);
		if (token != NUMBER) {
			parse_warn(cfile, "enterprise number expected");
			skip_to_semi(cfile);
			return;
		}
		enterprise_number = atoi(val);

		token = next_token(&val, &len, cfile);
		if (token != STRING) {
			parse_warn(cfile, "identifier expected");
			skip_to_semi(cfile);
			return;
		}

		/*
		 * Save the DUID.
		 */
		memset(&duid, 0, sizeof(duid));
        	duid.len = 2 + 4 + len;
        	if (!buffer_allocate(&duid.buffer, duid.len, MDL)) {
			log_fatal("Out of memory storing DUID");
		}
		duid.data = (unsigned char *)duid.buffer->data;
		putUShort(duid.buffer->data, DUID_EN);
 		putULong(duid.buffer->data + 2, enterprise_number);
		memcpy(duid.buffer->data + 6, val, len);

		set_server_duid(&duid);
		data_string_forget(&duid, MDL);
	}

	/* 
	 * Next easiest is the link-layer DUID. It consists only of
	 * the LL directive, or optionally the specific value to use.
	 *
	 * If we have LL only, then we set the type. If we have the
	 * value, then we set the actual DUID.
	 */
	else if (token == LL) {
		if (peek_token(NULL, NULL, cfile) == SEMI) {
			set_server_duid_type(DUID_LL);
		} else {
			/*
			 * Get our hardware type and address.
			 */
			token = next_token(NULL, NULL, cfile);
			switch (token) {
			      case ETHERNET:
				ll_type = HTYPE_ETHER;
				break;
			      case TOKEN_RING:
				ll_type = HTYPE_IEEE802;
				break;
			      case TOKEN_FDDI:
				ll_type = HTYPE_FDDI;
				break;
			      default:
				parse_warn(cfile, "hardware type expected");
				skip_to_semi(cfile);
				return;
			} 
			memset(&ll_addr, 0, sizeof(ll_addr));
			if (!parse_cshl(&ll_addr, cfile)) {
				return;
			}

			/*
			 * Save the DUID.
			 */
			memset(&duid, 0, sizeof(duid));
			duid.len = 2 + 2 + ll_addr.len;
        		if (!buffer_allocate(&duid.buffer, duid.len, MDL)) {
				log_fatal("Out of memory storing DUID");
			}
			duid.data = (unsigned char *)duid.buffer->data;
			putUShort(duid.buffer->data, DUID_LL);
 			putULong(duid.buffer->data + 2, ll_type);
			memcpy(duid.buffer->data + 4, 
			       ll_addr.data, ll_addr.len);

			set_server_duid(&duid);
			data_string_forget(&duid, MDL);
			data_string_forget(&ll_addr, MDL);
		}
	}

	/* 
	 * Finally the link-layer DUID plus time. It consists only of
	 * the LLT directive, or optionally the specific value to use.
	 *
	 * If we have LLT only, then we set the type. If we have the
	 * value, then we set the actual DUID.
	 */
	else if (token == LLT) {
		if (peek_token(NULL, NULL, cfile) == SEMI) {
			set_server_duid_type(DUID_LLT);
		} else {
			/*
			 * Get our hardware type, timestamp, and address.
			 */
			token = next_token(NULL, NULL, cfile);
			switch (token) {
			      case ETHERNET:
				ll_type = HTYPE_ETHER;
				break;
			      case TOKEN_RING:
				ll_type = HTYPE_IEEE802;
				break;
			      case TOKEN_FDDI:
				ll_type = HTYPE_FDDI;
				break;
			      default:
				parse_warn(cfile, "hardware type expected");
				skip_to_semi(cfile);
				return;
			} 
			
			token = next_token(&val, NULL, cfile);
			if (token != NUMBER) {
				parse_warn(cfile, "timestamp expected");
				skip_to_semi(cfile);
				return;
			}
			llt_time = atoi(val);

			memset(&ll_addr, 0, sizeof(ll_addr));
			if (!parse_cshl(&ll_addr, cfile)) {
				return;
			}

			/*
			 * Save the DUID.
			 */
			memset(&duid, 0, sizeof(duid));
			duid.len = 2 + 2 + 4 + ll_addr.len;
        		if (!buffer_allocate(&duid.buffer, duid.len, MDL)) {
				log_fatal("Out of memory storing DUID");
			}
			duid.data = (unsigned char *)duid.buffer->data;
			putUShort(duid.buffer->data, DUID_LLT);
 			putULong(duid.buffer->data + 2, ll_type);
 			putULong(duid.buffer->data + 4, llt_time);
			memcpy(duid.buffer->data + 8, 
			       ll_addr.data, ll_addr.len);

			set_server_duid(&duid);
			data_string_forget(&duid, MDL);
			data_string_forget(&ll_addr, MDL);
		}
	}

	/*
	 * If users want they can use a number for DUID types.
	 * This is useful for supporting future, not-yet-defined
	 * DUID types.
	 *
	 * In this case, they have to put in the complete value.
	 *
	 * This also works for existing DUID types of course. 
	 */
	else if (token == NUMBER) {
		duid_type_num = atoi(val);

		token = next_token(&val, &len, cfile);
		if (token != STRING) {
			parse_warn(cfile, "identifier expected");
			skip_to_semi(cfile);
			return;
		}

		/*
		 * Save the DUID.
		 */
		memset(&duid, 0, sizeof(duid));
        	duid.len = 2 + len;
        	if (!buffer_allocate(&duid.buffer, duid.len, MDL)) {
			log_fatal("Out of memory storing DUID");
		}
		duid.data = (unsigned char *)duid.buffer->data;
		putUShort(duid.buffer->data, duid_type_num);
		memcpy(duid.buffer->data + 2, val, len);

		set_server_duid(&duid);
		data_string_forget(&duid, MDL);
	}

	/*
	 * Anything else is an error.
	 */
	else {
		parse_warn(cfile, "DUID type of LLT, EN, or LL expected");
		skip_to_semi(cfile);
		return;
	}

	/*
	 * Finally consume our trailing semicolon.
	 */
	token = next_token(NULL, NULL, cfile);
	if (token != SEMI) {
		parse_warn(cfile, "semicolon expected");
		skip_to_semi(cfile);
	}
}

#endif /* DHCPv6 */

