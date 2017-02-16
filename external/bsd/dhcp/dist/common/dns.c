/*	$NetBSD: dns.c,v 1.5 2014/07/12 12:09:37 spz Exp $	*/
/* dns.c

   Domain Name Service subroutines. */

/*
 * Copyright (c) 2009-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004-2007 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2001-2003 by Internet Software Consortium
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
__RCSID("$NetBSD: dns.c,v 1.5 2014/07/12 12:09:37 spz Exp $");

/*! \file common/dns.c
 */
#include "dhcpd.h"
#include "arpa/nameser.h"
#include <isc/md5.h>
#include <isc/sha2.h>
#include <dns/result.h>

/*
 * This file contains code to connect the DHCP code to the libdns modules.
 * As part of that function it maintains a database of zone cuts that can
 * be used to figure out which server should be contacted to update any
 * given domain name.  Included in the zone information may be a pointer
 * to a key in which case that key is used for the update.  If no zone
 * is found then the DNS code determines the zone on its own.
 *
 * The way this works is that you define the domain name to which an
 * SOA corresponds, and the addresses of some primaries for that domain name:
 *
 *	zone FOO.COM {
 *	  primary 10.0.17.1;
 *	  secondary 10.0.22.1, 10.0.23.1;
 *	  key "FOO.COM Key";
 * 	}
 *
 * If an update is requested for GAZANGA.TOPANGA.FOO.COM, then the name
 * server looks in its database for a zone record for "GAZANGA.TOPANGA.FOO.COM",
 * doesn't find it, looks for one for "TOPANGA.FOO.COM", doesn't find *that*,
 * looks for "FOO.COM", finds it. So it
 * attempts the update to the primary for FOO.COM.   If that times out, it
 * tries the secondaries.   You can list multiple primaries if you have some
 * kind of magic name server that supports that.   You shouldn't list
 * secondaries that don't know how to forward updates (e.g., BIND 8 doesn't
 * support update forwarding, AFAIK).   If no TSIG key is listed, the update
 * is attempted without TSIG.
 *
 * You can also include IPv6 addresses via the primary6 and secondary6
 * options.  The search order for the addresses is primary, primary6,
 * secondary and lastly secondary6, with a limit on the number of 
 * addresses used.  Currently this limit is 3.
 *
 * The DHCP server tries to find an existing zone for any given name by
 * trying to look up a local zone structure for each domain containing
 * that name, all the way up to '.'.   If it finds one cached, it tries
 * to use that one to do the update.   That's why it tries to update
 * "FOO.COM" above, even though theoretically it should try GAZANGA...
 * and TOPANGA... first.
 *
 * If the update fails with a predefined zone the zone is marked as bad
 * and another search of the predefined zones is done.  If no predefined
 * zone is found finding a zone is left to the DNS module via examination
 * of SOA records.  If the DNS module finds a zone it may cache the zone
 * but the zone won't be cached here.
 *
 * TSIG updates are not performed on zones found by the DNS module - if
 * you want TSIG updates you _must_ write a zone definition linking the
 * key to the zone.   In cases where you know for sure what the key is
 * but do not want to hardcode the IP addresses of the primary or
 * secondaries, a zone declaration can be made that doesn't include any
 * primary or secondary declarations.   When the DHCP server encounters
 * this while hunting up a matching zone for a name, it looks up the SOA,
 * fills in the IP addresses, and uses that record for the update.
 * If the SOA lookup returns NXRRSET, a warning is printed and the zone is
 * discarded, TSIG key and all.   The search for the zone then continues 
 * as if the zone record hadn't been found.   Zones without IP addresses 
 * don't match when initially hunting for a zone to update.
 *
 * When an update is attempted and no predefined zone is found
 * that matches any enclosing domain of the domain being updated, the DHCP
 * server goes through the same process that is done when the update to a
 * predefined zone fails - starting with the most specific domain
 * name (GAZANGA.TOPANGA.FOO.COM) and moving to the least specific (the root),
 * it tries to look up an SOA record.
 *
 * TSIG keys are defined like this:
 *
 *	key "FOO.COM Key" {
 *		algorithm HMAC-MD5.SIG-ALG.REG.INT;
 *		secret <Base64>;
 *	}
 *
 * <Base64> is a number expressed in base64 that represents the key.
 * It's also permissible to use a quoted string here - this will be
 * translated as the ASCII bytes making up the string, and will not
 * include any NUL termination.  The key name can be any text string,
 * and the key type must be one of the key types defined in the draft
 * or by the IANA.  Currently only the HMAC-MD5... key type is
 * supported.
 *
 * The DDNS processing has been split into two areas.  One is the
 * control code that determines what should be done.  That code is found
 * in the client or server directories.  The other is the common code
 * that performs functions such as properly formatting the arguments.
 * That code is found in this file.  The basic processing flow for a
 * DDNS update is:
 * In the client or server code determine what needs to be done and
 * collect the necesary information then pass it to a function from
 * this file.
 * In this code lookup the zone and extract the zone and key information
 * (if available) and prepare the arguments for the DNS module.
 * When the DNS module completes its work (times out or gets a reply)
 * it will trigger another function here which does generic processing
 * and then passes control back to the code from the server or client.
 * The server or client code then determines the next step which may
 * result in another call to this module in which case the process repeats.
 */

dns_zone_hash_t *dns_zone_hash;

/*
 * DHCP dns structures
 * Normally the relationship between these structures isn't one to one
 * but in the DHCP case it (mostly) is.  To make the allocations, frees,
 * and passing of the memory easier we make a single structure with all
 * the pieces.
 *
 * The maximum size of the data buffer should be large enough for any
 * items DHCP will generate
 */

typedef struct dhcp_ddns_rdata {
	dns_rdata_t	rdata;
	dns_rdatalist_t rdatalist;
	dns_rdataset_t  rdataset;
} dhcp_ddns_data_t;

#if defined (NSUPDATE)
#if defined (DNS_ZONE_LOOKUP)

/*
 * The structure used to find a nameserver if there wasn't a zone entry.
 * Currently we assume we won't have many of these outstanding at any
 * time so we go with a simple linked list.
 * In use find_zone_start() will fill in the oname with the name
 * requested by the DDNS code.  zname will point to it and be
 * advanced as labels are removed.  If the DNS client code returns
 * a set of name servers eventp and rdataset will be set.  Then
 * the code will walk through the nameservers in namelist and
 * find addresses that are stored in addrs and addrs6.
 */

typedef struct dhcp_ddns_ns {
	struct dhcp_ddns_ns *next;      
	struct data_string oname;     /* the original name for DDNS */
	char *zname;                  /* a pointer into the original name for
					 the zone we are checking */
	dns_clientresevent_t *eventp; /* pointer to the event that provided the
					 namelist, we can't free the eventp
					 until we free the namelist */
	dns_name_t *ns_name;          /* current name server we are examining */
	dns_rdataset_t *rdataset; 
	dns_rdatatype_t rdtype;       /* type of address we want */

	struct in_addr addrs[DHCP_MAXNS];   /* space for v4 addresses */
	struct in6_addr addrs6[DHCP_MAXNS]; /* space for v6 addresses */
	int num_addrs;
	int num_addrs6;
	int ttl;

	void *transaction;             /* transaction id for DNS calls */
} dhcp_ddns_ns_t;

/*
 * The list of DDNS names for which we are attempting to find a name server.
 * This list is used for finding the name server, it doesn't include the
 * information necessary to do the DDNS request after finding a name server.
 * The code attempts to minimize duplicate requests by examining the list
 * to see if we are already trying to find a substring of the new request.
 * For example imagine the first request is "a.b.c.d.e." and the server has
 * already discarded the first two lables and is trying "c.d.e.".  If the
 * next request is for "x.y.c.d.e." the code assumes the in progress
 * request is sufficient and doesn't add a new request for the second name.
 * If the next request was for "x.y.z.d.e." the code doesn't assume they
 * will use the same nameserver and starts a second request.
 * This strategy will not eliminate all duplicates but is simple and
 * should be sufficient.
 */
dhcp_ddns_ns_t *dns_outstanding_ns = NULL;

/*
 * Routines to manipulate the list of outstanding searches
 *
 * add_to_ns_queue() - adds the given control block to the queue
 *
 * remove_from_ns_queue() - removes the given control block from
 * the queue
 *
 * find_in_ns_queue() compares the name from the given control
 * block with the control blocks in the queue.  It returns
 * success if a matching entry is found.  In order to match
 * the entry already on the queue must be shorter than the
 * incoming name must match the ending substring of the name.
 */

static void
add_to_ns_queue(dhcp_ddns_ns_t *ns_cb)
{
	ns_cb->next = dns_outstanding_ns;
	dns_outstanding_ns = ns_cb;
}


static void
remove_from_ns_queue(dhcp_ddns_ns_t *ns_cb)
{
	dhcp_ddns_ns_t **foo;

	foo = &dns_outstanding_ns;
	while (*foo) {
		if (*foo == ns_cb) {
			*foo = ns_cb->next;
			break;
		}
		foo = &((*foo)->next);
	}
	ns_cb->next = NULL;
}

static isc_result_t
find_in_ns_queue(dhcp_ddns_ns_t *ns_cb)
{
	dhcp_ddns_ns_t *temp_cb;
	int in_len, temp_len;

	in_len = strlen(ns_cb->zname);

	for(temp_cb = dns_outstanding_ns;
	    temp_cb != NULL;
	    temp_cb = temp_cb->next) {
		temp_len = strlen(temp_cb->zname);
		if (temp_len > in_len)
			continue;
		if (strcmp(temp_cb->zname,
			   ns_cb->zname + (in_len - temp_len)) == 0)
			return(ISC_R_SUCCESS);
	}
	return(ISC_R_NOTFOUND);
}

void cache_found_zone (dhcp_ddns_ns_t *);
#endif

void ddns_interlude(isc_task_t *, isc_event_t *);

#if defined (TRACING)
/*
 * Code to support tracing DDNS packets.  We trace packets going to and
 * coming from the libdns code but don't try to track the packets
 * exchanged between the libdns code and the dns server(s) it contacts.
 *
 * The code is split into two sets of routines
 *  input refers to messages received from the dns module
 *  output refers to messages sent to the dns module
 * Currently there are three routines in each set
 *  write is used to write information about the message to the trace file
 *        this routine is called directly from the proper place in the code.
 *  read is used to read information about a message from the trace file
 *       this routine is called from the trace loop as it reads through
 *       the file and is registered via the trace_type_register routine.
 *       When playing back a trace file we shall absorb records of output
 *       messages as part of processing the write function, therefore
 *       any output messages we encounter are flagged as errors.
 *  stop isn't currently used in this code but is needed for the register
 *       routine.
 *
 * We pass a pointer to a control block to the dns module which it returns
 * to use as part of the result.  As the pointer may vary between traces
 * we need to map between those from the trace file and the new ones during
 * playback.
 *
 * The mapping is complicated a little as a pointer could be 4 or 8 bytes
 * long.  We treat the old pointer as an 8 byte quantity and pad and compare
 * as necessary.
 */

/*
 * Structure used to map old pointers to new pointers.
 * Old pointers are 8 bytes long as we don't know if the trace was 
 * done on a 64 bit or 32 bit machine.  
 */
#define TRACE_PTR_LEN 8

typedef struct dhcp_ddns_map {
	char  old_pointer[TRACE_PTR_LEN];
	void *new_pointer;
	struct dhcp_ddns_map *next;
} dhcp_ddns_map_t;

/* The starting point for the map structure */
static dhcp_ddns_map_t *ddns_map;

trace_type_t *trace_ddns_input;
trace_type_t *trace_ddns_output;

/*
 * The data written to the trace file is:
 * 32 bits result from dns
 * 64 bits pointer of cb
 */

static void
trace_ddns_input_write(dhcp_ddns_cb_t *ddns_cb, isc_result_t result)
{
	trace_iov_t iov[2];
	u_int32_t old_result;
	char old_pointer[TRACE_PTR_LEN];
	
	old_result = htonl((u_int32_t)result);
	memset(old_pointer, 0, TRACE_PTR_LEN);
	memcpy(old_pointer, &ddns_cb, sizeof(ddns_cb));

	iov[0].len = sizeof(old_result);
	iov[0].buf = (char *)&old_result;
	iov[1].len = TRACE_PTR_LEN;
	iov[1].buf = old_pointer;
	trace_write_packet_iov(trace_ddns_input, 2, iov, MDL);
}

/*
 * Process the result and pointer from the trace file.
 * We use the pointer map to find the proper pointer for this instance.
 * Then we need to construct an event to pass along to the interlude
 * function.
 */
static void
trace_ddns_input_read(trace_type_t *ttype, unsigned length,
				  char *buf)
{
	u_int32_t old_result;
	char old_pointer[TRACE_PTR_LEN];
	dns_clientupdateevent_t *eventp;
	void *new_pointer;
	dhcp_ddns_map_t *ddns_map_ptr;

	if (length < (sizeof(old_result) + TRACE_PTR_LEN)) {
		log_error("trace_ddns_input_read: data too short");
		return;
	}

	memcpy(&old_result, buf, sizeof(old_result));
	memcpy(old_pointer, buf + sizeof(old_result), TRACE_PTR_LEN);

	/* map the old pointer to a new pointer */
	for (ddns_map_ptr = ddns_map;
	     ddns_map_ptr != NULL;
	     ddns_map_ptr = ddns_map_ptr->next) {
		if ((ddns_map_ptr->new_pointer != NULL) &&
		    memcmp(ddns_map_ptr->old_pointer,
			   old_pointer, TRACE_PTR_LEN) == 0) {
			new_pointer = ddns_map_ptr->new_pointer;
			ddns_map_ptr->new_pointer = NULL;
			memset(ddns_map_ptr->old_pointer, 0, TRACE_PTR_LEN);
			break;
		}
	}
	if (ddns_map_ptr == NULL) {
		log_error("trace_dns_input_read: unable to map cb pointer");
		return;
	}		

	eventp = (dns_clientupdateevent_t *)
		isc_event_allocate(dhcp_gbl_ctx.mctx,
				   dhcp_gbl_ctx.task,
				   0,
				   ddns_interlude,
				   new_pointer,
				   sizeof(dns_clientupdateevent_t));
	if (eventp == NULL) {
		log_error("trace_ddns_input_read: unable to allocate event");
		return;
	}
	eventp->result = ntohl(old_result);


	ddns_interlude(dhcp_gbl_ctx.task, (isc_event_t *)eventp);

	return;
}

static void
trace_ddns_input_stop(trace_type_t *ttype)
{
}

/*
 * We use the same arguments as for the dns startupdate function to
 * allows us to choose between the two via a macro.  If tracing isn't
 * in use we simply call the dns function directly.
 *
 * If we are doing playback we read the next packet from the file
 * and compare the type.  If it matches we extract the results and pointer
 * from the trace file.  The results are returned to the caller as if
 * they had called the dns routine.  The pointer is used to construct a 
 * map for when the "reply" is processed.
 *
 * The data written to trace file is:
 * 32 bits result
 * 64 bits pointer of cb (DDNS Control block)
 * contents of cb
 */

static isc_result_t
trace_ddns_output_write(dns_client_t *client, dns_rdataclass_t rdclass,
			dns_name_t *zonename, dns_namelist_t *prerequisites,
			dns_namelist_t *updates, isc_sockaddrlist_t *servers,
			dns_tsec_t *tsec, unsigned int options,
			isc_task_t *task, isc_taskaction_t action, void *arg,
			dns_clientupdatetrans_t **transp)
{
	isc_result_t result;
	u_int32_t old_result;
	char old_pointer[TRACE_PTR_LEN];
	dhcp_ddns_map_t *ddns_map_ptr;
	
	if (trace_playback() != 0) {
		/* We are doing playback, extract the entry from the file */
		unsigned buflen = 0;
		char *inbuf = NULL;

		result = trace_get_packet(&trace_ddns_output,
					  &buflen, &inbuf);
		if (result != ISC_R_SUCCESS) {
			log_error("trace_ddns_output_write: no input found");
			return (ISC_R_FAILURE);
		}
		if (buflen < (sizeof(old_result) + TRACE_PTR_LEN)) {
			log_error("trace_ddns_output_write: data too short");
			dfree(inbuf, MDL);
			return (ISC_R_FAILURE);
		}
		memcpy(&old_result, inbuf, sizeof(old_result));
		result = ntohl(old_result);
		memcpy(old_pointer, inbuf + sizeof(old_result), TRACE_PTR_LEN);
		dfree(inbuf, MDL);

		/* add the pointer to the pointer map */
		for (ddns_map_ptr = ddns_map;
		     ddns_map_ptr != NULL;
		     ddns_map_ptr = ddns_map_ptr->next) {
			if (ddns_map_ptr->new_pointer == NULL) {
				break;
			}
		}

		/*
		 * If we didn't find an empty entry, allocate an entry and
		 * link it into the list.  The list isn't ordered.
		 */
		if (ddns_map_ptr == NULL) {
			ddns_map_ptr = dmalloc(sizeof(*ddns_map_ptr), MDL);
			if (ddns_map_ptr == NULL) {
				log_error("trace_ddns_output_write: " 
					  "unable to allocate map entry");
				return(ISC_R_FAILURE);
				}
			ddns_map_ptr->next = ddns_map;
			ddns_map = ddns_map_ptr;
		}

		memcpy(ddns_map_ptr->old_pointer, old_pointer, TRACE_PTR_LEN);
		ddns_map_ptr->new_pointer = arg;
	}
	else {
		/* We aren't doing playback, make the actual call */
		result = dns_client_startupdate(client, rdclass, zonename,
						prerequisites, updates,
						servers, tsec, options,
						task, action, arg, transp);
	}

	if (trace_record() != 0) {
		/* We are recording, save the information to the file */
		trace_iov_t iov[3];
		old_result = htonl((u_int32_t)result);
		memset(old_pointer, 0, TRACE_PTR_LEN);
		memcpy(old_pointer, &arg, sizeof(arg));
		iov[0].len = sizeof(old_result);
		iov[0].buf = (char *)&old_result;
		iov[1].len = TRACE_PTR_LEN;
		iov[1].buf = old_pointer;

		/* Write out the entire cb, in case we want to look at it */
		iov[2].len = sizeof(dhcp_ddns_cb_t);
		iov[2].buf = (char *)arg;

		trace_write_packet_iov(trace_ddns_output, 3, iov, MDL);
	}

	return(result);
}

static void
trace_ddns_output_read(trace_type_t *ttype, unsigned length,
				   char *buf)
{
	log_error("unaccounted for ddns output.");
}

static void
trace_ddns_output_stop(trace_type_t *ttype)
{
}

void
trace_ddns_init()
{
	trace_ddns_output = trace_type_register("ddns-output", NULL,
						trace_ddns_output_read,
						trace_ddns_output_stop, MDL);
	trace_ddns_input  = trace_type_register("ddns-input", NULL,
						trace_ddns_input_read,
						trace_ddns_input_stop, MDL);
	ddns_map = NULL;
}

#define ddns_update trace_ddns_output_write
#else
#define ddns_update dns_client_startupdate
#endif /* TRACING */

#define zone_resolve dns_client_startresolve

/*
 * Code to allocate and free a dddns control block.  This block is used
 * to pass and track the information associated with a DDNS update request.
 */
dhcp_ddns_cb_t *
ddns_cb_alloc(const char *file, int line)
{
	dhcp_ddns_cb_t *ddns_cb;
	int i;

	ddns_cb = dmalloc(sizeof(*ddns_cb), file, line);
	if (ddns_cb != NULL) {
		ISC_LIST_INIT(ddns_cb->zone_server_list);
		for (i = 0; i < DHCP_MAXNS; i++) {
			ISC_LINK_INIT(&ddns_cb->zone_addrs[i], link);
		}
	}

#if defined (DEBUG_DNS_UPDATES)
	log_info("%s(%d): Allocating ddns_cb=%p", file, line, ddns_cb);
#endif

	return(ddns_cb);
}
		
void
ddns_cb_free(dhcp_ddns_cb_t *ddns_cb, const char *file, int line)
{
#if defined (DEBUG_DNS_UPDATES)
	log_info("%s(%d): freeing ddns_cb=%p", file, line, ddns_cb);
#endif

  	data_string_forget(&ddns_cb->fwd_name, file, line);
	data_string_forget(&ddns_cb->rev_name, file, line);
	data_string_forget(&ddns_cb->dhcid, file, line);
	
	if (ddns_cb->zone != NULL) {
		forget_zone((struct dns_zone **)&ddns_cb->zone);
	}

	/* Should be freed by now, check just in case. */
	if (ddns_cb->transaction != NULL)
		log_error("Impossible memory leak at %s:%d (attempt to free "
			  "DDNS Control Block before transaction).", MDL);

	dfree(ddns_cb, file, line);
}

void
ddns_cb_forget_zone(dhcp_ddns_cb_t *ddns_cb)
{
	int i;

	forget_zone(&ddns_cb->zone);
	ddns_cb->zone_name[0] = 0;
	ISC_LIST_INIT(ddns_cb->zone_server_list);
	for (i = 0; i < DHCP_MAXNS; i++) {
		ISC_LINK_INIT(&ddns_cb->zone_addrs[i], link);
	}
}

isc_result_t find_tsig_key (ns_tsig_key **key, const char *zname,
			    struct dns_zone *zone)
{
	ns_tsig_key *tkey;

	if (!zone)
		return ISC_R_NOTFOUND;

	if (!zone -> key) {
		return DHCP_R_KEY_UNKNOWN;
	}
	
	if ((!zone -> key -> name ||
	     strlen (zone -> key -> name) > NS_MAXDNAME) ||
	    (!zone -> key -> algorithm ||
	     strlen (zone -> key -> algorithm) > NS_MAXDNAME) ||
	    (!zone -> key) ||
	    (!zone -> key -> key) ||
	    (zone -> key -> key -> len == 0)) {
		return DHCP_R_INVALIDKEY;
	}
	tkey = dmalloc (sizeof *tkey, MDL);
	if (!tkey) {
	      nomem:
		return ISC_R_NOMEMORY;
	}
	memset (tkey, 0, sizeof *tkey);
	tkey -> data = dmalloc (zone -> key -> key -> len, MDL);
	if (!tkey -> data) {
		dfree (tkey, MDL);
		goto nomem;
	}
	strcpy (tkey -> name, zone -> key -> name);
	strcpy (tkey -> alg, zone -> key -> algorithm);
	memcpy (tkey -> data,
		zone -> key -> key -> value, zone -> key -> key -> len);
	tkey -> len = zone -> key -> key -> len;
	*key = tkey;
	return ISC_R_SUCCESS;
}

void tkey_free (ns_tsig_key **key)
{
	if ((*key) -> data)
		dfree ((*key) -> data, MDL);
	dfree ((*key), MDL);
	*key = (ns_tsig_key *)0;
}
#endif

static isc_result_t remove_dns_zone (struct dns_zone *zone)
{
	struct dns_zone *tz = NULL;

	if (dns_zone_hash) {
		dns_zone_hash_lookup(&tz, dns_zone_hash, zone->name, 0, MDL);
		if (tz != NULL) {
			dns_zone_hash_delete(dns_zone_hash, tz->name, 0, MDL);
			dns_zone_dereference(&tz, MDL);
		}
	}

	return (ISC_R_SUCCESS);
}

isc_result_t enter_dns_zone (struct dns_zone *zone)
{
	struct dns_zone *tz = (struct dns_zone *)0;

	if (dns_zone_hash) {
		dns_zone_hash_lookup (&tz,
				      dns_zone_hash, zone -> name, 0, MDL);
		if (tz == zone) {
			dns_zone_dereference (&tz, MDL);
			return ISC_R_SUCCESS;
		}
		if (tz) {
			dns_zone_hash_delete (dns_zone_hash,
					      zone -> name, 0, MDL);
			dns_zone_dereference (&tz, MDL);
		}
	} else {
		if (!dns_zone_new_hash(&dns_zone_hash, DNS_HASH_SIZE, MDL))
			return ISC_R_NOMEMORY;
	}

	dns_zone_hash_add (dns_zone_hash, zone -> name, 0, zone, MDL);
	return ISC_R_SUCCESS;
}

isc_result_t dns_zone_lookup (struct dns_zone **zone, const char *name)
{
	int len;
	char *tname = (char *)0;
	isc_result_t status;

	if (!dns_zone_hash)
		return ISC_R_NOTFOUND;

	len = strlen (name);
	if (name [len - 1] != '.') {
		tname = dmalloc ((unsigned)len + 2, MDL);
		if (!tname)
			return ISC_R_NOMEMORY;
		strcpy (tname, name);
		tname [len] = '.';
		tname [len + 1] = 0;
		name = tname;
	}
	if (!dns_zone_hash_lookup (zone, dns_zone_hash, name, 0, MDL))
		status = ISC_R_NOTFOUND;
	else if ((*zone)->timeout && (*zone)->timeout < cur_time) {
		dns_zone_hash_delete(dns_zone_hash, (*zone)->name, 0, MDL);
		dns_zone_dereference(zone, MDL);
		status = ISC_R_NOTFOUND;
	} else 
		status = ISC_R_SUCCESS;

	if (tname)
		dfree (tname, MDL);
	return status;
}

int dns_zone_dereference (ptr, file, line)
	struct dns_zone **ptr;
	const char *file;
	int line;
{
	struct dns_zone *dns_zone;

	if ((ptr == NULL) || (*ptr == NULL)) {
		log_error("%s(%d): null pointer", file, line);
#if defined (POINTER_DEBUG)
		abort();
#else
		return (0);
#endif
	}

	dns_zone = *ptr;
	*ptr = NULL;
	--dns_zone->refcnt;
	rc_register(file, line, ptr, dns_zone, dns_zone->refcnt, 1, RC_MISC);
	if (dns_zone->refcnt > 0)
		return (1);

	if (dns_zone->refcnt < 0) {
		log_error("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history(dns_zone);
#endif
#if defined (POINTER_DEBUG)
		abort();
#else
		return (0);
#endif
	}

	if (dns_zone->name)
		dfree(dns_zone->name, file, line);
	if (dns_zone->key)
		omapi_auth_key_dereference(&dns_zone->key, file, line);
	if (dns_zone->primary)
		option_cache_dereference(&dns_zone->primary, file, line);
	if (dns_zone->secondary)
		option_cache_dereference(&dns_zone->secondary, file, line);
	if (dns_zone->primary6)
		option_cache_dereference(&dns_zone->primary6, file, line);
	if (dns_zone->secondary6)
		option_cache_dereference(&dns_zone->secondary6, file, line);
	dfree(dns_zone, file, line);
	return (1);
}

#if defined (NSUPDATE)
#if defined (DNS_ZONE_LOOKUP)

/* Helper function to copy the address from an rdataset to 
 * the nameserver control block.  Mostly to avoid really long
 * lines in the nested for loops
 */
static void
zone_addr_to_ns(dhcp_ddns_ns_t *ns_cb,
		dns_rdataset_t *rdataset)
{
	dns_rdata_t rdata;
	dns_rdata_in_a_t a;
	dns_rdata_in_aaaa_t aaaa;
	
	dns_rdata_init(&rdata);
	dns_rdataset_current(rdataset, &rdata);
	switch (rdataset->type) {
	case dns_rdatatype_a:
		(void) dns_rdata_tostruct(&rdata, &a, NULL);
		memcpy(&ns_cb->addrs[ns_cb->num_addrs], &a.in_addr, 4);
		ns_cb->num_addrs++;
		dns_rdata_freestruct(&a);
		break;
	case dns_rdatatype_aaaa:
		(void) dns_rdata_tostruct(&rdata, &aaaa, NULL);
		memcpy(&ns_cb->addrs6[ns_cb->num_addrs6], &aaaa.in6_addr, 16);
		ns_cb->num_addrs6++;
		dns_rdata_freestruct(&aaaa);
		break;
	default:
		break;
	}

	if ((ns_cb->ttl == 0) || (ns_cb->ttl > rdataset->ttl))
		ns_cb->ttl = rdataset->ttl;
}

/*
 * The following three routines co-operate to find the addresses of
 * the nameservers to use for a zone if we don't have a zone statement.
 * We strongly suggest the use of a zone statement to avoid problmes
 * and to allow for the use of TSIG and therefore better security, but
 * include this functionality for those that don't want such statements.
 *
 * find_zone_start(ddns_cb, direction)
 * This is the first of the routines, it is called from the rest of
 * the ddns code when we have received a request for DDNS for a name
 * and don't have a zone entry that would cover that name.  The name
 * is in the ddns_cb as specified by the direction (forward or reverse).
 * The start function pulls the name out and constructs the name server
 * block then starts the process by calling the DNS client code.
 *
 * find_zone_ns(taskp, eventp)
 * This is the second step of the process.  The DNS client code will
 * call this when it has gotten a response or timed out.  If the response
 * doesn't have a list of nameservers we remove another label from the
 * zone name and try again.  If the response does include a list of
 * nameservers we start walking through the list attempting to get
 * addresses for the nameservers.
 *
 * find_zone_addrs(taskp, eventp)
 * This is the third step of the process.  In find_zone_ns we got
 * a list of nameserves and started walking through them.  This continues
 * the walk and if we get back any addresses it adds them to our list.
 * When we get enough addresses or run out of nameservers we construct
 * a zone entry and insert it into the zone hash for the rest of the
 * DDNS code to use.
 */
static void
find_zone_addrs(isc_task_t *taskp,
		isc_event_t *eventp)
{
	dns_clientresevent_t *ddns_event = (dns_clientresevent_t *)eventp;
	dhcp_ddns_ns_t *ns_cb = (dhcp_ddns_ns_t *)eventp->ev_arg;
	dns_name_t *ns_name = NULL;
	dns_rdataset_t *rdataset;
	isc_result_t result;
	dns_name_t *name;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_ns_t ns;
	

	/* the transaction is done, get rid of the tag */
	dns_client_destroyrestrans(&ns_cb->transaction);

	/* If we succeeded we try and extract the addresses, if we can
	 * and we have enough we are done.  If we didn't succeed or
	 * we don't have enough addresses afterwards we drop through
	 * and try the next item on the list.
	 */
	if (ddns_event->result == ISC_R_SUCCESS) {

		for (name = ISC_LIST_HEAD(ddns_event->answerlist);
		     name != NULL;
		     name = ISC_LIST_NEXT(name, link)) {

			for (rdataset = ISC_LIST_HEAD(name->list);
			     rdataset != NULL;
			     rdataset = ISC_LIST_NEXT(rdataset, link)) {

				for (result = dns_rdataset_first(rdataset);
				     result == ISC_R_SUCCESS;
				     result = dns_rdataset_next(rdataset)) {

					/* add address to cb */
					zone_addr_to_ns(ns_cb, rdataset);

					/* We are done if we have
					 * enough addresses
					 */
					if (ns_cb->num_addrs +
					    ns_cb->num_addrs6 >= DHCP_MAXNS)
						goto done;
				}
			}
		}
	}

	/* We need more addresses.
	 * We restart the loop we were in before.
	 */

	for (ns_name = ns_cb->ns_name;
	     ns_name != NULL;
	     ns_name = ISC_LIST_NEXT(ns_name, link)) {

		if (ns_name == ns_cb->ns_name) {
			/* first time through, use saved state */
			rdataset = ns_cb->rdataset;
		} else {
			rdataset = ISC_LIST_HEAD(ns_name->list);
		}

		for (;
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			
			if (rdataset->type != dns_rdatatype_ns)
				continue;
			dns_rdata_init(&rdata);

			if (rdataset == ns_cb->rdataset) {
				/* first time through use the saved state */
				if (ns_cb->rdtype == dns_rdatatype_a) {
					ns_cb->rdtype = dns_rdatatype_aaaa;
				} else {
					ns_cb->rdtype = dns_rdatatype_a;
					if (dns_rdataset_next(rdataset) !=
					    ISC_R_SUCCESS)
						continue;
				}
			} else {
				if ((!dns_rdataset_isassociated(rdataset)) ||
				    (dns_rdataset_first(rdataset) != 
				     ISC_R_SUCCESS))
					continue;
			}				

			dns_rdataset_current(rdataset, &rdata);
			if (dns_rdata_tostruct(&rdata, &ns, NULL) !=
			    ISC_R_SUCCESS)
				continue;

			/* Save our current state */
			ns_cb->ns_name = ns_name;
			ns_cb->rdataset = rdataset;

			/* And call out to DNS */
			result = zone_resolve(dhcp_gbl_ctx.dnsclient, &ns.name,
					      dns_rdataclass_in,
					      ns_cb->rdtype,
					      DNS_CLIENTRESOPT_NODNSSEC,
					      dhcp_gbl_ctx.task,
					      find_zone_addrs,
					      (void *)ns_cb,
					      &ns_cb->transaction);

			/* do we need to clean this? */
			dns_rdata_freestruct(&ns);

			if (result == ISC_R_SUCCESS)
				/* we have started the next step, cleanup
				 * the structures associated with this call
				 * but leave the cb for the next round
				 */
				goto cleanup;

			log_error("find_zone_addrs: unable to continue "
				  "resolve: %s %s",
				  ns_cb->zname,
				  isc_result_totext(result));

			/* The call to start a resolve transaction failed,
			 * should we try to continue with any other names?
			 * For now let's not, but let's use whatever we
			 * may already have.
			 */
			goto done;
		}
	}

 done:
	/* we've either gotten our max number of addresses or
	 * run out of nameservers to try.  Convert the cb into
	 * a zone and insert it into the zone hash.  Then
	 * we need to clean up the saved state.
	 */
	if ((ns_cb->num_addrs != 0) ||
	    (ns_cb->num_addrs6 != 0))
		cache_found_zone(ns_cb);

	dns_client_freeresanswer(dhcp_gbl_ctx.dnsclient,
				 &ns_cb->eventp->answerlist);
	isc_event_free((isc_event_t **)&ns_cb->eventp);

	remove_from_ns_queue(ns_cb);
	data_string_forget(&ns_cb->oname, MDL);
	dfree(ns_cb, MDL);

 cleanup:
	/* cleanup any of the new state information */

	dns_client_freeresanswer(dhcp_gbl_ctx.dnsclient,
				 &ddns_event->answerlist);
	isc_event_free(&eventp);

	return;
	 
}

/*
 * Routine to continue the process of finding a nameserver via the DNS
 * This is routine is called when we are still trying to get a list
 * of nameservers to process.
 */
 
static void
find_zone_ns(isc_task_t *taskp,
	     isc_event_t *eventp)
{
	dns_clientresevent_t *ddns_event = (dns_clientresevent_t *)eventp;
	dhcp_ddns_ns_t *ns_cb = (dhcp_ddns_ns_t *)eventp->ev_arg;
	dns_fixedname_t zname0;
	dns_name_t *zname = NULL, *ns_name = NULL;
	dns_rdataset_t *rdataset;
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_ns_t ns;

	/* the transaction is done, get rid of the tag */
	dns_client_destroyrestrans(&ns_cb->transaction);

	if (ddns_event->result != ISC_R_SUCCESS) {
		/* We didn't find any nameservers, try again */

		/* Remove a label and continue */
		ns_cb->zname = strchr(ns_cb->zname, '.');
		if ((ns_cb->zname == NULL) ||
		    (ns_cb->zname[1] == 0)) {
			/* No more labels, all done */
			goto cleanup;
		}
		ns_cb->zname++;

		/* Create a DNS version of the zone name and call the
		 * resolver code */
		if (((result = dhcp_isc_name((unsigned char *)ns_cb->zname,
					     &zname0, &zname))
		     != ISC_R_SUCCESS) ||
		    ((result = zone_resolve(dhcp_gbl_ctx.dnsclient,
					    zname, dns_rdataclass_in,
					    dns_rdatatype_ns,
					    DNS_CLIENTRESOPT_NODNSSEC,
					    dhcp_gbl_ctx.task,
					    find_zone_ns,
					    (void *)ns_cb,
					    &ns_cb->transaction))
		     != ISC_R_SUCCESS)) {
			log_error("find_zone_ns: Unable to build "
				  "name or start resolve: %s %s",
				  ns_cb->zname,
				  isc_result_totext(result));
			goto cleanup;
		}
		
		/* we have successfully started the next iteration
		 * of this step, clean up from the call and continue */
                dns_client_freeresanswer(dhcp_gbl_ctx.dnsclient,
                                         &ddns_event->answerlist);
		isc_event_free(&eventp);
		return;
	}

	/* We did get a set of nameservers, save the information and
	 * start trying to get addresses
	 */
	ns_cb->eventp = ddns_event;
	for (ns_name = ISC_LIST_HEAD(ddns_event->answerlist);
	     ns_name != NULL;
	     ns_name = ISC_LIST_NEXT(ns_name, link)) {

		for (rdataset = ISC_LIST_HEAD(ns_name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {

			if (rdataset->type != dns_rdatatype_ns)
				continue;

			if ((!dns_rdataset_isassociated(rdataset)) ||
			    (dns_rdataset_first(rdataset) != 
			     ISC_R_SUCCESS))
				continue;

			dns_rdataset_current(rdataset, &rdata);
			if (dns_rdata_tostruct(&rdata, &ns, NULL) !=
			    ISC_R_SUCCESS)
				continue;

			/* Save our current state */
			ns_cb->ns_name = ns_name;
			ns_cb->rdataset = rdataset;

			/* And call out to DNS */
			result = zone_resolve(dhcp_gbl_ctx.dnsclient, &ns.name,
					      dns_rdataclass_in,
					      ns_cb->rdtype,
					      DNS_CLIENTRESOPT_NODNSSEC,
					      dhcp_gbl_ctx.task,
					      find_zone_addrs,
					      (void *)ns_cb,
					      &ns_cb->transaction);

			/* do we need to clean this? */
			dns_rdata_freestruct(&ns);

			if (result == ISC_R_SUCCESS)
				/* We have successfully started the next step
				 * we don't cleanup the eventp block as we are
				 * still using it.
				 */
				return;

			log_error("find_zone_ns: unable to continue "
				  "resolve: %s %s",
				  ns_cb->zname,
				  isc_result_totext(result));

			/* The call to start a resolve transaction failed,
			 * should we try to continue with any other names?
			 * For now let's not
			 */
			goto cleanup;
		}
	}

 cleanup:
	/* When we add a queue to manage the DDNS
	 * requests we will need to remove any that
	 * were waiting for this resolution */

	dns_client_freeresanswer(dhcp_gbl_ctx.dnsclient,
				 &ddns_event->answerlist);
	isc_event_free(&eventp);

	remove_from_ns_queue(ns_cb);

	data_string_forget(&ns_cb->oname, MDL);
	dfree(ns_cb, MDL);
	return;
	
}

/*
 * Start the process of finding nameservers via the DNS because
 * we don't have a zone entry already.
 * We construct a control block and fill in the DDNS name.  As
 * the process continues we shall move the zname pointer to
 * indicate which labels we are still using.  The rest of
 * the control block will be filled in as we continue processing.
 */
static isc_result_t
find_zone_start(dhcp_ddns_cb_t *ddns_cb, int direction) 
{
	isc_result_t status = ISC_R_NOTFOUND;
	dhcp_ddns_ns_t *ns_cb;
	dns_fixedname_t zname0;
	dns_name_t *zname = NULL;

	/*
	 * We don't validate np as that was already done in find_cached_zone()
	 */

	/* Allocate the control block for this request */
	ns_cb = dmalloc(sizeof(*ns_cb), MDL);
	if (ns_cb == NULL) {
		log_error("find_zone_start: unable to allocate cb");
		return(ISC_R_FAILURE);
	}
	ns_cb->rdtype = dns_rdatatype_a;

	/* Copy the data string so the NS lookup is independent of the DDNS */
	if (direction == FIND_FORWARD) {
		data_string_copy(&ns_cb->oname,  &ddns_cb->fwd_name, MDL);
	} else {
		data_string_copy(&ns_cb->oname,  &ddns_cb->rev_name, MDL);
	}
	ns_cb->zname = (char *)ns_cb->oname.data;

	/*
	 * Check the dns_outstanding_ns queue to see if we are
	 * already processing something that would cover this name
	 */
	if (find_in_ns_queue(ns_cb) == ISC_R_SUCCESS) {
		data_string_forget(&ns_cb->oname, MDL);
		dfree(ns_cb, MDL);
		return (ISC_R_SUCCESS);
	}

	/* Create a DNS version of the zone name and call the
	 * resolver code */
	if (((status = dhcp_isc_name((unsigned char *)ns_cb->zname,
				     &zname0, &zname))
	     != ISC_R_SUCCESS) ||
	    ((status = zone_resolve(dhcp_gbl_ctx.dnsclient,
				    zname, dns_rdataclass_in,
				    dns_rdatatype_ns,
				    DNS_CLIENTRESOPT_NODNSSEC,
				    dhcp_gbl_ctx.task,
				    find_zone_ns,
				    (void *)ns_cb,
				    &ns_cb->transaction))
	     != ISC_R_SUCCESS)) {
		log_error("find_zone_start: Unable to build "
			  "name or start resolve: %s %s",
			  ns_cb->zname,
			  isc_result_totext(status));

		/* We failed to start the process, clean up */
		data_string_forget(&ns_cb->oname, MDL);
		dfree(ns_cb, MDL);
	} else {
		/* We started the process, attach the control block
		 * to the queue */
		add_to_ns_queue(ns_cb);
	}

	return (status);
}
#endif

isc_result_t
find_cached_zone(dhcp_ddns_cb_t *ddns_cb, int direction) 
{
	isc_result_t status = ISC_R_NOTFOUND;
	const char *np;
	struct dns_zone *zone = NULL;
	struct data_string nsaddrs;
	struct in_addr zone_addr;
	struct in6_addr zone_addr6;
	int ix;

	if (direction == FIND_FORWARD) {
		np = (const char *)ddns_cb->fwd_name.data;
	} else {
		np = (const char *)ddns_cb->rev_name.data;
	}

	/* We can't look up a null zone. */
	if ((np == NULL) || (*np == '\0')) {
		return (DHCP_R_INVALIDARG);
	}

	/*
	 * For each subzone, try to find a cached zone.
	 */
	for (;;) {
		status = dns_zone_lookup(&zone, np);
		if (status == ISC_R_SUCCESS)
			break;

		np = strchr(np, '.');
		if (np == NULL)
			break;
		np++;
	}

	if (status != ISC_R_SUCCESS)
		return (status);

	/* Make sure the zone is valid, we've already gotten
	 * rid of expired dynamic zones.  Check to see if
	 * we repudiated this zone.  If so give up.
	 */
	if ((zone->flags & DNS_ZONE_INACTIVE) != 0) {
		dns_zone_dereference(&zone, MDL);
		return (ISC_R_FAILURE);
	}

	/* Make sure the zone name will fit. */
	if (strlen(zone->name) > sizeof(ddns_cb->zone_name)) {
		dns_zone_dereference(&zone, MDL);
		return (ISC_R_NOSPACE);
	}
	strcpy((char *)&ddns_cb->zone_name[0], zone->name);

	memset (&nsaddrs, 0, sizeof nsaddrs);
	ix = 0;

	if (zone->primary) {
		if (evaluate_option_cache(&nsaddrs, NULL, NULL, NULL,
					  NULL, NULL, &global_scope,
					  zone->primary, MDL)) {
			int ip = 0;
			while (ix < DHCP_MAXNS) {
				if (ip + 4 > nsaddrs.len)
					break;
				memcpy(&zone_addr, &nsaddrs.data[ip], 4);
				isc_sockaddr_fromin(&ddns_cb->zone_addrs[ix],
						    &zone_addr,
						    NS_DEFAULTPORT);
				ISC_LIST_APPEND(ddns_cb->zone_server_list,
						&ddns_cb->zone_addrs[ix],
						link);
				ip += 4;
				ix++;
			}
			data_string_forget(&nsaddrs, MDL);
		}
	}

	if (zone->primary6) {
		if (evaluate_option_cache(&nsaddrs, NULL, NULL, NULL,
					  NULL, NULL, &global_scope,
					  zone->primary6, MDL)) {
			int ip = 0;
			while (ix < DHCP_MAXNS) {
				if (ip + 16 > nsaddrs.len)
					break;
				memcpy(&zone_addr6, &nsaddrs.data[ip], 16);
				isc_sockaddr_fromin6(&ddns_cb->zone_addrs[ix],
						    &zone_addr6,
						    NS_DEFAULTPORT);
				ISC_LIST_APPEND(ddns_cb->zone_server_list,
						&ddns_cb->zone_addrs[ix],
						link);
				ip += 16;
				ix++;
			}
			data_string_forget(&nsaddrs, MDL);
		}
	}

	if (zone->secondary) {
		if (evaluate_option_cache(&nsaddrs, NULL, NULL, NULL,
					  NULL, NULL, &global_scope,
					  zone->secondary, MDL)) {
			int ip = 0;
			while (ix < DHCP_MAXNS) {
				if (ip + 4 > nsaddrs.len)
					break;
				memcpy(&zone_addr, &nsaddrs.data[ip], 4);
				isc_sockaddr_fromin(&ddns_cb->zone_addrs[ix],
						    &zone_addr,
						    NS_DEFAULTPORT);
				ISC_LIST_APPEND(ddns_cb->zone_server_list,
						&ddns_cb->zone_addrs[ix],
						link);
				ip += 4;
				ix++;
			}
			data_string_forget (&nsaddrs, MDL);
		}
	}

	if (zone->secondary6) {
		if (evaluate_option_cache(&nsaddrs, NULL, NULL, NULL,
					  NULL, NULL, &global_scope,
					  zone->secondary6, MDL)) {
			int ip = 0;
			while (ix < DHCP_MAXNS) {
				if (ip + 16 > nsaddrs.len)
					break;
				memcpy(&zone_addr6, &nsaddrs.data[ip], 16);
				isc_sockaddr_fromin6(&ddns_cb->zone_addrs[ix],
						    &zone_addr6,
						    NS_DEFAULTPORT);
				ISC_LIST_APPEND(ddns_cb->zone_server_list,
						&ddns_cb->zone_addrs[ix],
						link);
				ip += 16;
				ix++;
			}
			data_string_forget (&nsaddrs, MDL);
		}
	}

	dns_zone_reference(&ddns_cb->zone, zone, MDL);
	dns_zone_dereference (&zone, MDL);
	return ISC_R_SUCCESS;
}

void forget_zone (struct dns_zone **zone)
{
	dns_zone_dereference (zone, MDL);
}

void repudiate_zone (struct dns_zone **zone)
{
	/* verify that we have a pointer at least */
	if ((zone == NULL) || (*zone == NULL)) {
		log_info("Null argument to repudiate zone");
		return;
	}

	(*zone)->flags |= DNS_ZONE_INACTIVE;
	dns_zone_dereference(zone, MDL);
}

#if defined (DNS_ZONE_LOOKUP)
void cache_found_zone(dhcp_ddns_ns_t *ns_cb)
{
	struct dns_zone *zone = NULL;
	int len, remove_zone = 0;

	/* See if there's already such a zone. */
	if (dns_zone_lookup(&zone, ns_cb->zname) == ISC_R_SUCCESS) {
		/* If it's not a dynamic zone, leave it alone. */
		if (zone->timeout == 0)
			return;

		/* Remove any old addresses in case they've changed */
		if (zone->primary)
			option_cache_dereference(&zone->primary, MDL);
		if (zone->primary6)
			option_cache_dereference(&zone->primary6, MDL);

		/* Set the flag to remove the zone from the hash if
		   we have problems */
		remove_zone = 1;
	} else if (dns_zone_allocate(&zone, MDL) == 0) {
		return;
	} else {
		/* We've just allocated the zone, now we need
		 * to allocate space for the name and addresses
		 */

		/* allocate space for the name */
		len = strlen(ns_cb->zname);
		zone->name = dmalloc(len + 2, MDL);
		if (zone->name == NULL) {
			goto cleanup;
		}

		/* Copy the name and add a trailing '.' if necessary */
		strcpy(zone->name, ns_cb->zname);
		if (zone->name[len-1] != '.') {
			zone->name[len] = '.';
			zone->name[len+1] = 0;
		}
	}

	zone->timeout = cur_time + ns_cb->ttl;

	if (ns_cb->num_addrs != 0) {
		len = ns_cb->num_addrs * sizeof(struct in_addr);
		if ((!option_cache_allocate(&zone->primary, MDL)) ||
		    (!buffer_allocate(&zone->primary->data.buffer,
				      len, MDL))) {
			if (remove_zone == 1)
				remove_dns_zone(zone);
			goto cleanup;
		}
		memcpy(zone->primary->data.buffer->data, ns_cb->addrs, len);
		zone->primary->data.data = 
			&zone->primary->data.buffer->data[0];
		zone->primary->data.len = len;
	}
	if (ns_cb->num_addrs6 != 0) {
		len = ns_cb->num_addrs6 * sizeof(struct in6_addr);
		if ((!option_cache_allocate(&zone->primary6, MDL)) ||
		    (!buffer_allocate(&zone->primary6->data.buffer,
				      len, MDL))) {
			if (remove_zone == 1)
				remove_dns_zone(zone);
			goto cleanup;
		}
		memcpy(zone->primary6->data.buffer->data, ns_cb->addrs6, len);
		zone->primary6->data.data = 
			&zone->primary6->data.buffer->data[0];
		zone->primary6->data.len = len;
	}

	enter_dns_zone(zone);

 cleanup:
	dns_zone_dereference(&zone, MDL);
	return;
}
#endif

/*!
 * \brief Create an id for a client
 *
 * This function is used to create an id for a client to use with DDNS
 * This version of the function is for the standard style, RFC 4701
 *
 * This function takes information from the type and data fields and
 * mangles it into a dhcid string which it places in ddns_cb.  It also
 * sets a field in ddns_cb to specify the class that should be used
 * when sending the dhcid, in this case it is a DHCID record so we use
 * dns_rdatatype_dhcid
 *
 * The DHCID we construct is:
 *  2 bytes - identifier type (see 4701 and IANA)
 *  1 byte  - digest type, currently only SHA256 (1)
 *  n bytes - digest, length depends on digest type, currently 32 for
 *            SHA256
 *
 * What we base the digest on is up to the calling code for an id type of
 * 0 - 1 octet htype followed by hlen octets of chaddr from v4 client request
 * 1 - data octets from a dhcpv4 client's client identifier option
 * 2 - the client DUID from a v4 or v6 client's client id option
 * This identifier is concatenated with the fqdn and the result is digested.
 */
static int get_std_dhcid(dhcp_ddns_cb_t *ddns_cb,
		  int type,
		  const u_int8_t *identifier,
		  unsigned id_len)
{
	struct data_string *id = &ddns_cb->dhcid;
	isc_sha256_t sha256;
	unsigned char buf[ISC_SHA256_DIGESTLENGTH];
	unsigned char fwd_buf[256];
	unsigned fwd_buflen = 0;

	/* Types can only be 0..(2^16)-1. */
	if (type < 0 || type > 65535)
		return (0);

	/* We need to convert the fwd name to wire representation */
	if (MRns_name_pton((char *)ddns_cb->fwd_name.data, fwd_buf, 256) == -1)
		return (0);
	while(fwd_buf[fwd_buflen] != 0) {
		fwd_buflen += fwd_buf[fwd_buflen] + 1;
	}
	fwd_buflen++;

	if (!buffer_allocate(&id->buffer,
			     ISC_SHA256_DIGESTLENGTH + 2 + 1,
			     MDL))
		return (0);
	id->data = id->buffer->data;

	/* The two first bytes contain the type identifier. */
	putUShort(id->buffer->data, (unsigned)type);

	/* The next is the digest type, SHA-256 is 1 */
	putUChar(id->buffer->data + 2, 1u);

	/* Computing the digest */
	isc_sha256_init(&sha256);
	isc_sha256_update(&sha256, identifier, id_len);
	isc_sha256_update(&sha256, fwd_buf, fwd_buflen);
	isc_sha256_final(buf, &sha256);

	memcpy(id->buffer->data + 3, &buf, ISC_SHA256_DIGESTLENGTH);

	id->len = ISC_SHA256_DIGESTLENGTH + 2 + 1;

	return (1);
}

/*!
 *
 * \brief Create an id for a client
 *
 * This function is used to create an id for a client to use with DDNS
 * This version of the function is for the interim style.  It is retained
 * to allow users to continue using the interim style but they should
 * switch to the standard style (which uses get_std_dhcid) for better
 * interoperability.  
 *
 * This function takes information from the type and data fields and
 * mangles it into a dhcid string which it places in ddns_cb.  It also
 * sets a field in ddns_cb to specify the class that should be used
 * when sending the dhcid, in this case it is a txt record so we use
 * dns_rdata_type_txt
 *
 * NOTE WELL: this function has issues with how it calculates the
 * dhcid, they can't be changed now as that would break the records
 * already in use.
 */

static int get_int_dhcid (dhcp_ddns_cb_t *ddns_cb,
		   int type,
		   const u_int8_t *data,
		   unsigned len)
{
	struct data_string *id = &ddns_cb->dhcid;
	unsigned char buf[ISC_MD5_DIGESTLENGTH];
	isc_md5_t md5;
	int i;

	/* Types can only be 0..(2^16)-1. */
	if (type < 0 || type > 65535)
		return (0);

	/*
	 * Hexadecimal MD5 digest plus two byte type, NUL,
	 * and one byte for length for dns.
	 */
	if (!buffer_allocate(&id -> buffer,
			     (ISC_MD5_DIGESTLENGTH * 2) + 4, MDL))
		return (0);
	id->data = id->buffer->data;

	/*
	 * We put the length into the first byte to turn 
	 * this into a dns text string.  This avoid needing to
	 * copy the string to add the byte later.
	 */
	id->buffer->data[0] = ISC_MD5_DIGESTLENGTH * 2 + 2;

	/* Put the type in the next two bytes. */
	id->buffer->data[1] = "0123456789abcdef"[(type >> 4) & 0xf];
	/* This should have been [type & 0xf] but now that
	 * it is in use we need to leave it this way in order
	 * to avoid disturbing customer's lease files
	 */
	id->buffer->data[2] = "0123456789abcdef"[type % 15];
  
	/* Mash together an MD5 hash of the identifier. */
	isc_md5_init(&md5);
	isc_md5_update(&md5, data, len);
	isc_md5_final(&md5, buf);

	/* Convert into ASCII. */
	for (i = 0; i < ISC_MD5_DIGESTLENGTH; i++) {
		id->buffer->data[i * 2 + 3] =
			"0123456789abcdef"[(buf[i] >> 4) & 0xf];
		id->buffer->data[i * 2 + 4] =
			"0123456789abcdef"[buf[i] & 0xf];
	}

	id->len = ISC_MD5_DIGESTLENGTH * 2 + 3;
	id->buffer->data[id->len] = 0;
	id->terminated = 1;

	return (1);
}

int get_dhcid(dhcp_ddns_cb_t *ddns_cb,
	      int type,
	      const u_int8_t *identifier,
	      unsigned id_len)
{
	if (ddns_cb->dhcid_class == dns_rdatatype_dhcid)
		return get_std_dhcid(ddns_cb, type, identifier, id_len);
	else 
		return get_int_dhcid(ddns_cb, type, identifier, id_len);
}

/*
 * The dhcid (text version) that we pass to DNS includes a length byte
 * at the start but the text we store in the lease doesn't include the
 * length byte.  The following routines are to convert between the two
 * styles.
 *
 * When converting from a dhcid to a leaseid we reuse the buffer and
 * simply adjust the data pointer and length fields in the data string.
 * This avoids any prolems with allocating space.
 */

void
dhcid_tolease(struct data_string *dhcid,
	      struct data_string *leaseid)
{
	/* copy the data string then update the fields */
	data_string_copy(leaseid, dhcid, MDL);
	leaseid->data++;
	leaseid->len--;
}

isc_result_t
dhcid_fromlease(struct data_string *dhcid,
		struct data_string *leaseid)
{
	if (!buffer_allocate(&dhcid->buffer, leaseid->len + 2, MDL)) {
		return(ISC_R_FAILURE);
	}

	dhcid->data = dhcid->buffer->data;

	dhcid->buffer->data[0] = leaseid->len;
	memcpy(dhcid->buffer->data + 1, leaseid->data, leaseid->len);
	dhcid->len = leaseid->len + 1;
	if (leaseid->terminated == 1) {
		dhcid->buffer->data[dhcid->len] = 0;
		dhcid->terminated = 1;
	}

	return(ISC_R_SUCCESS);
}

/* 
 * Construct the dataset for this item.
 * This is a fairly simple arrangement as the operations we do are simple.
 * If there is data we simply have the rdata point to it - the formatting
 * must be correct already.  We then link the rdatalist to the rdata and
 * create a rdataset from the rdatalist.
 */

static isc_result_t
make_dns_dataset(dns_rdataclass_t  dataclass,
		 dns_rdatatype_t   datatype,
		 dhcp_ddns_data_t *dataspace,
		 unsigned char    *data,
		 int               datalen,
		 int               ttl)
{
	dns_rdata_t *rdata = &dataspace->rdata;
	dns_rdatalist_t *rdatalist = &dataspace->rdatalist;
	dns_rdataset_t *rdataset = &dataspace->rdataset;

	isc_region_t region;

	/* set up the rdata */
	dns_rdata_init(rdata);

	if (data == NULL) {
		/* No data, set up the rdata fields we care about */
		rdata->flags = DNS_RDATA_UPDATE;
		rdata->type = datatype;
		rdata->rdclass = dataclass;
	} else {
		switch(datatype) {
		case dns_rdatatype_a:
		case dns_rdatatype_aaaa:
		case dns_rdatatype_txt:
		case dns_rdatatype_dhcid:
		case dns_rdatatype_ptr:
			/* The data must be in the right format we simply
			 * need to supply it via the correct structure */
			region.base   = data;
			region.length = datalen;
			dns_rdata_fromregion(rdata, dataclass, datatype,
					     &region);
			break;
		default:
			return(DHCP_R_INVALIDARG);
			break;
		}
	}

	/* setup the datalist and attach the rdata to it */
	dns_rdatalist_init(rdatalist);
	rdatalist->type = datatype;
	rdatalist->rdclass = dataclass;
	rdatalist->ttl = ttl;
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);

	/* convert the datalist to a dataset */
	dns_rdataset_init(rdataset);
	dns_rdatalist_tordataset(rdatalist, rdataset);

	return(ISC_R_SUCCESS);
}

/*
 * When a DHCP client or server intends to update an A RR, it first
 * prepares a DNS UPDATE query which includes as a prerequisite the
 * assertion that the name does not exist.  The update section of the
 * query attempts to add the new name and its IP address mapping (an A
 * RR), and the DHCID RR with its unique client-identity.
 *   -- "Interaction between DHCP and DNS"
 *
 * There are two cases, one for the server and one for the client.
 *
 * For the server the first step will have a request of:
 * The name is not in use
 * Add an A RR
 * Add a DHCID RR
 *
 * For the client the first step will have a request of:
 * The A RR does not exist
 * Add an A RR
 * Add a DHCID RR
 */

static isc_result_t
ddns_modify_fwd_add1(dhcp_ddns_cb_t   *ddns_cb,
		     dhcp_ddns_data_t *dataspace,
		     dns_name_t       *pname,
		     dns_name_t       *uname)
{
	isc_result_t result;

	/* Construct the prerequisite list */
	if ((ddns_cb->flags & DDNS_INCLUDE_RRSET) != 0) {
		/* The A RR shouldn't exist */
		result = make_dns_dataset(dns_rdataclass_none,
					  ddns_cb->address_type,
					  dataspace, NULL, 0, 0);
	} else {
		/* The name is not in use */
		result = make_dns_dataset(dns_rdataclass_none,
					  dns_rdatatype_any,
					  dataspace, NULL, 0, 0);
	}
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(pname->list, &dataspace->rdataset, link);
	dataspace++;

	/* Construct the update list */
	/* Add the A RR */
	result = make_dns_dataset(dns_rdataclass_in, ddns_cb->address_type,
				  dataspace,
				  (unsigned char *)ddns_cb->address.iabuf,
				  ddns_cb->address.len, ddns_cb->ttl);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(uname->list, &dataspace->rdataset, link);
	dataspace++;

	/* Add the DHCID RR */
	result = make_dns_dataset(dns_rdataclass_in, ddns_cb->dhcid_class,
				  dataspace, 
				  (unsigned char *)ddns_cb->dhcid.data,
				  ddns_cb->dhcid.len, ddns_cb->ttl);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(uname->list, &dataspace->rdataset, link);

	return(ISC_R_SUCCESS);
}

/*
 * If the first update operation fails with YXDOMAIN, the updater can
 * conclude that the intended name is in use.  The updater then
 * attempts to confirm that the DNS name is not being used by some
 * other host. The updater prepares a second UPDATE query in which the
 * prerequisite is that the desired name has attached to it a DHCID RR
 * whose contents match the client identity.  The update section of
 * this query deletes the existing A records on the name, and adds the
 * A record that matches the DHCP binding and the DHCID RR with the
 * client identity.
 *   -- "Interaction between DHCP and DNS"
 *
 * The message for the second step depends on if we are doing conflict
 * resolution.  If we are we include a prerequisite.  If not we delete
 * the DHCID in addition to all A rrsets.
 *
 * Conflict resolution:
 * DHCID RR exists, and matches client identity.
 * Delete A RRset.
 * Add A RR.
 *
 * Conflict override:
 * Delete DHCID RRs.
 * Add DHCID RR
 * Delete A RRset.
 * Add A RR.
 */

static isc_result_t
ddns_modify_fwd_add2(dhcp_ddns_cb_t   *ddns_cb,
		     dhcp_ddns_data_t *dataspace,
		     dns_name_t       *pname,
		     dns_name_t       *uname)
{
	isc_result_t result = ISC_R_SUCCESS;

	/*
	 * If we are doing conflict resolution (unset) we use a prereq list.
	 * If not we delete the DHCID in addition to all A rrsets.
	 */
	if ((ddns_cb->flags & DDNS_CONFLICT_OVERRIDE) == 0) {
		/* Construct the prereq list */
		/* The DHCID RR exists and matches the client identity */
		result = make_dns_dataset(dns_rdataclass_in, ddns_cb->dhcid_class,
					  dataspace, 
					  (unsigned char *)ddns_cb->dhcid.data,
					  ddns_cb->dhcid.len, 0);
		if (result != ISC_R_SUCCESS) {
			return(result);
		}
		ISC_LIST_APPEND(pname->list, &dataspace->rdataset, link);
		dataspace++;
	} else {
		/* Start constructing the update list.
		 * Conflict detection override: delete DHCID RRs */
		result = make_dns_dataset(dns_rdataclass_any,
					  ddns_cb->dhcid_class,
					  dataspace, NULL, 0, 0);
		if (result != ISC_R_SUCCESS) {
			return(result);
		}
		ISC_LIST_APPEND(uname->list, &dataspace->rdataset, link);
		dataspace++;

		/* Add current DHCID RR */
		result = make_dns_dataset(dns_rdataclass_in, ddns_cb->dhcid_class,
					  dataspace, 
					  (unsigned char *)ddns_cb->dhcid.data,
					  ddns_cb->dhcid.len, ddns_cb->ttl);
		if (result != ISC_R_SUCCESS) {
			return(result);
		}
		ISC_LIST_APPEND(uname->list, &dataspace->rdataset, link);
		dataspace++;
	}

	/* Start or continue constructing the update list */
	/* Delete the A RRset */
	result = make_dns_dataset(dns_rdataclass_any, ddns_cb->address_type,
				  dataspace, NULL, 0, 0);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(uname->list, &dataspace->rdataset, link);
	dataspace++;

	/* Add the A RR */
	result = make_dns_dataset(dns_rdataclass_in, ddns_cb->address_type,
				  dataspace, 
				  (unsigned char *)ddns_cb->address.iabuf,
				  ddns_cb->address.len, ddns_cb->ttl);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(uname->list, &dataspace->rdataset, link);

	return(ISC_R_SUCCESS);
}

/*
 * The entity chosen to handle the A record for this client (either the
 * client or the server) SHOULD delete the A record that was added when
 * the lease was made to the client.
 *
 * In order to perform this delete, the updater prepares an UPDATE
 * query which contains two prerequisites.  The first prerequisite
 * asserts that the DHCID RR exists whose data is the client identity
 * described in Section 4.3. The second prerequisite asserts that the
 * data in the A RR contains the IP address of the lease that has
 * expired or been released.
 *   -- "Interaction between DHCP and DNS"
 *
 * RFC 4703 has relaxed the prereqisites to only checking the DHCID RR
 * and we have adopted that to minizmie problems due to interruptions
 * when doing a deletion.  
 *
 * First try has:
 * DHCID RR exists, and matches client identity.
 * Delete appropriate A RR.
 */

static isc_result_t
ddns_modify_fwd_rem1(dhcp_ddns_cb_t   *ddns_cb,
		     dhcp_ddns_data_t *dataspace,
		     dns_name_t       *pname,
		     dns_name_t       *uname)
{
	isc_result_t result = ISC_R_SUCCESS;

	/* Consruct the prereq list */
	/* The DHCID RR exists and matches the client identity */
	result = make_dns_dataset(dns_rdataclass_in, ddns_cb->dhcid_class,
				  dataspace, 
				  (unsigned char *)ddns_cb->dhcid.data,
				  ddns_cb->dhcid.len, 0);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(pname->list, &dataspace->rdataset, link);
	dataspace++;

	/* Construct the update list */
	/* Delete A RRset */
	result = make_dns_dataset(dns_rdataclass_none, ddns_cb->address_type,
				  dataspace,
				  (unsigned char *)ddns_cb->address.iabuf,
				  ddns_cb->address.len, 0);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(uname->list, &dataspace->rdataset, link);

	return(ISC_R_SUCCESS);
}

/*
 * If the deletion of the A succeeded, and there are no A or AAAA
 * records left for this domain, then we can blow away the DHCID
 * record as well.   We can't blow away the DHCID record above
 * because it's possible that more than one record has been added
 * to this domain name.
 *
 * Second query has:
 * A RR does not exist.
 * AAAA RR does not exist.
 * Delete appropriate DHCID RR.
 */

static isc_result_t
ddns_modify_fwd_rem2(dhcp_ddns_cb_t   *ddns_cb,
		     dhcp_ddns_data_t *dataspace,
		     dns_name_t       *pname,
		     dns_name_t       *uname)
{
	isc_result_t result;

	/* Construct the prereq list */
	/* The A RR does not exist */
	result = make_dns_dataset(dns_rdataclass_none, dns_rdatatype_a,
				  dataspace, NULL, 0, 0);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(pname->list, &dataspace->rdataset, link);
	dataspace++;

	/* The AAAA RR does not exist */
	result = make_dns_dataset(dns_rdataclass_none, dns_rdatatype_aaaa,
				  dataspace, NULL, 0, 0);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(pname->list, &dataspace->rdataset, link);
	dataspace++;

	/* Construct the update list */
	/* Delete DHCID RR */
	result = make_dns_dataset(dns_rdataclass_none, ddns_cb->dhcid_class,
				  dataspace,
				  (unsigned char *)ddns_cb->dhcid.data,
				  ddns_cb->dhcid.len, 0);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}
	ISC_LIST_APPEND(uname->list, &dataspace->rdataset, link);

	return(ISC_R_SUCCESS);
}

/*
 * This routine converts from the task action call into something
 * easier to work with.  It also handles the common case of a signature
 * or zone not being correct.
 */
void ddns_interlude(isc_task_t  *taskp,
		    isc_event_t *eventp)
{
	dhcp_ddns_cb_t *ddns_cb = (dhcp_ddns_cb_t *)eventp->ev_arg;
	dns_clientupdateevent_t *ddns_event = (dns_clientupdateevent_t *)eventp;
	isc_result_t eresult = ddns_event->result;
	isc_result_t result;

	/* We've extracted the information we want from it, get rid of
	 * the event block.*/
	isc_event_free(&eventp);

#if defined (TRACING)
	if (trace_record()) {
		trace_ddns_input_write(ddns_cb, eresult);
	}
#endif

#if defined (DEBUG_DNS_UPDATES)
	print_dns_status(DDNS_PRINT_INBOUND, ddns_cb, eresult);
#endif

	/* This transaction is complete, clear the value */
	dns_client_destroyupdatetrans(&ddns_cb->transaction);

	/* If we cancelled or tried to cancel the operation we just
	 * need to clean up. */
	if ((eresult == ISC_R_CANCELED) ||
	    ((ddns_cb->flags & DDNS_ABORT) != 0)) {
#if defined (DEBUG_DNS_UPDATES)
		log_info("DDNS: completeing transaction cancellation cb=%p, "
			 "flags=%x, %s",
			 ddns_cb, ddns_cb->flags, isc_result_totext(eresult));
#endif
		if ((ddns_cb->flags & DDNS_ABORT) == 0) {
			log_info("DDNS: cleaning up lease pointer for a cancel "
				 "cb=%p", ddns_cb);
			/* 
			 * We shouldn't actually be able to get here but
			 * we are.  This means we haven't cleaned up
			 * the lease pointer so we need to do that before
			 * freeing the cb.  
			 */
			ddns_cb->cur_func(ddns_cb, eresult);
			return;
		}

		if (ddns_cb->next_op != NULL) {
			/* if necessary cleanup up next op block */
			ddns_cb_free(ddns_cb->next_op, MDL);
		}
		ddns_cb_free(ddns_cb, MDL);
		return;
	}

	/* If we had a problem with our key or zone try again */
	if ((eresult == DNS_R_NOTAUTH) ||
	    (eresult == DNS_R_NOTZONE)) {
		int i;
		/* Our zone information was questionable,
		 * repudiate it and try again */
		log_error("DDNS: bad zone information, repudiating zone %s",
			  ddns_cb->zone_name);
		repudiate_zone(&ddns_cb->zone);
		ddns_cb->zone_name[0]    = 0;
		ISC_LIST_INIT(ddns_cb->zone_server_list);
		for (i = 0; i < DHCP_MAXNS; i++) {
			ISC_LINK_INIT(&ddns_cb->zone_addrs[i], link);
		}

		if ((ddns_cb->state == DDNS_STATE_ADD_PTR) ||
		    (ddns_cb->state == DDNS_STATE_REM_PTR)) {
			result = ddns_modify_ptr(ddns_cb, MDL);
		} else {
			result = ddns_modify_fwd(ddns_cb, MDL);
		}

		if (result != ISC_R_SUCCESS) {
			/* if we couldn't redo the query log it and
			 * let the next function clean it up */
			log_info("DDNS: Failed to retry after zone failure");
			ddns_cb->cur_func(ddns_cb, result);
		}
		return;
	} else {
		/* pass it along to be processed */
		ddns_cb->cur_func(ddns_cb, eresult);
	}
	
	return;
}

/*
 * This routine does the generic work for sending a ddns message to
 * modify the forward record (A or AAAA) and calls one of a set of
 * routines to build the specific message.
 */

isc_result_t
ddns_modify_fwd(dhcp_ddns_cb_t *ddns_cb, const char *file, int line)
{
	isc_result_t result;
	dns_tsec_t *tsec_key = NULL;

	unsigned char *clientname;
	dhcp_ddns_data_t *dataspace = NULL;
	dns_namelist_t prereqlist, updatelist;
	dns_fixedname_t zname0, pname0, uname0;
	dns_name_t *zname = NULL, *pname, *uname;

	isc_sockaddrlist_t *zlist = NULL;

	/* Get a pointer to the clientname to make things easier. */
	clientname = (unsigned char *)ddns_cb->fwd_name.data;

	/* Extract and validate the type of the address. */
	if (ddns_cb->address.len == 4) {
		ddns_cb->address_type = dns_rdatatype_a;
	} else if (ddns_cb->address.len == 16) {
		ddns_cb->address_type = dns_rdatatype_aaaa;
	} else {
		return DHCP_R_INVALIDARG;
	}

	/*
	 * If we already have a zone use it, otherwise try to lookup the
	 * zone in our cache.  If we find one we will have a pointer to
	 * the zone that needs to be dereferenced when we are done with it.
	 * If we don't find one that is okay we'll let the DNS code try and
	 * find the information for us.
	 */

	if (ddns_cb->zone == NULL) {
		result = find_cached_zone(ddns_cb, FIND_FORWARD);
#if defined (DNS_ZONE_LOOKUP)
		if (result == ISC_R_NOTFOUND) {
			/*
			 * We didn't find a cached zone, see if we can
			 * can find a nameserver and create a zone.
			 */
			if (find_zone_start(ddns_cb, FIND_FORWARD)
			    == ISC_R_SUCCESS) {
				/*
				 * We have started the process to find a zone
				 * queue the ddns_cb for processing after we
				 * create the zone
				 */
				/* sar - not yet implemented, currently we just
				 * arrange for things to get cleaned up
				 */
				goto cleanup;
			}
		}
#endif
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	/*
	 * If we have a zone try to get any information we need
	 * from it - name, addresses and the key.  The address 
	 * and key may be empty the name can't be.
	 */
	if (ddns_cb->zone) {
		/* Set up the zone name for use by DNS */
		result = dhcp_isc_name(ddns_cb->zone_name, &zname0, &zname);
		if (result != ISC_R_SUCCESS) {
			log_error("Unable to build name for zone for "
				  "fwd update: %s %s",
				  ddns_cb->zone_name,
				  isc_result_totext(result));
			goto cleanup;
		}

		if (!(ISC_LIST_EMPTY(ddns_cb->zone_server_list))) {
			/* If we have any addresses get them */
			zlist = &ddns_cb->zone_server_list;
		}
		

		if (ddns_cb->zone->key != NULL) {
			/*
			 * Not having a key is fine, having a key
			 * but not a tsec is odd so we warn the user.
			 */
			/*sar*/
			/* should we do the warning? */
			tsec_key = ddns_cb->zone->key->tsec_key;
			if (tsec_key == NULL) {
				log_error("No tsec for use with key %s",
					  ddns_cb->zone->key->name);
			}
		}
	}

	/* Set up the DNS names for the prereq and update lists */
	if (((result = dhcp_isc_name(clientname, &pname0, &pname))
	     != ISC_R_SUCCESS) ||
	    ((result = dhcp_isc_name(clientname, &uname0, &uname))
	     != ISC_R_SUCCESS)) {
		log_error("Unable to build name for fwd update: %s %s",
			  clientname, isc_result_totext(result));
		goto cleanup;
	}

	/* Allocate the various isc dns library structures we may require. */
	dataspace = isc_mem_get(dhcp_gbl_ctx.mctx, sizeof(*dataspace) * 4);
	if (dataspace == NULL) {
		log_error("Unable to allocate memory for fwd update");
		result = ISC_R_NOMEMORY; 
		goto cleanup;
	}

	ISC_LIST_INIT(prereqlist);
	ISC_LIST_INIT(updatelist);

	switch(ddns_cb->state) {
	case DDNS_STATE_ADD_FW_NXDOMAIN:
		result = ddns_modify_fwd_add1(ddns_cb, dataspace,
					      pname, uname);
		if (result != ISC_R_SUCCESS) {
			goto cleanup;
		}
		ISC_LIST_APPEND(prereqlist, pname, link);
		break;
	case DDNS_STATE_ADD_FW_YXDHCID:
		result = ddns_modify_fwd_add2(ddns_cb, dataspace,
					       pname, uname);
		if (result != ISC_R_SUCCESS) {
			goto cleanup;
		}

		/* If we aren't doing conflict override we have entries
		 * in the pname list and we need to attach it to the
		 * prereqlist */

		if ((ddns_cb->flags & DDNS_CONFLICT_OVERRIDE) == 0) {
			ISC_LIST_APPEND(prereqlist, pname, link);
		}

		break;
	case DDNS_STATE_REM_FW_YXDHCID:
		result = ddns_modify_fwd_rem1(ddns_cb, dataspace,
					      pname, uname);
		if (result != ISC_R_SUCCESS) {
			goto cleanup;
		}
		ISC_LIST_APPEND(prereqlist, pname, link);
		break;
	case DDNS_STATE_REM_FW_NXRR:
		result = ddns_modify_fwd_rem2(ddns_cb, dataspace,
					      pname, uname);
		if (result != ISC_R_SUCCESS) {
			goto cleanup;
		}
		ISC_LIST_APPEND(prereqlist, pname, link);
		break;

	default:
		log_error("Invalid operation in ddns code.");
		result = DHCP_R_INVALIDARG;
		goto cleanup;
		break;
	}

	/*
	 * We always have an update list but may not have a prereqlist
	 * if we are doing conflict override.
	 */
	ISC_LIST_APPEND(updatelist, uname, link);

	/* send the message, cleanup and return the result */
	result = ddns_update(dhcp_gbl_ctx.dnsclient,
			     dns_rdataclass_in, zname,
			     &prereqlist, &updatelist,
			     zlist, tsec_key,
			     DNS_CLIENTRESOPT_ALLOWRUN,
			     dhcp_gbl_ctx.task,
			     ddns_interlude,
			     (void *)ddns_cb,
			     &ddns_cb->transaction);
	if (result == ISC_R_FAMILYNOSUPPORT) {
		log_info("Unable to perform DDNS update, "
			 "address family not supported");
	}

#if defined (DEBUG_DNS_UPDATES)
	print_dns_status(DDNS_PRINT_OUTBOUND, ddns_cb, result);
#endif

 cleanup:
#if defined (DEBUG_DNS_UPDATES)
	if (result != ISC_R_SUCCESS) {
		log_info("DDNS: %s(%d): error in ddns_modify_fwd %s for %p",
			 file, line, isc_result_totext(result), ddns_cb);
	}
#endif

	if (dataspace != NULL) {
		isc_mem_put(dhcp_gbl_ctx.mctx, dataspace,
			    sizeof(*dataspace) * 4);
	}
	return(result);
}


isc_result_t
ddns_modify_ptr(dhcp_ddns_cb_t *ddns_cb, const char *file, int line)
{
	isc_result_t result;
	dns_tsec_t *tsec_key  = NULL;
	unsigned char *ptrname;
	dhcp_ddns_data_t *dataspace = NULL;
	dns_namelist_t updatelist;
	dns_fixedname_t zname0, uname0;
	dns_name_t *zname = NULL, *uname;
	isc_sockaddrlist_t *zlist = NULL;
	unsigned char buf[256];
	int buflen;

	/*
	 * Try to lookup the zone in the zone cache.  As with the forward
	 * case it's okay if we don't have one, the DNS code will try to
	 * find something also if we succeed we will need to dereference
	 * the zone later.  Unlike with the forward case we assume we won't
	 * have a pre-existing zone.
	 */
	result = find_cached_zone(ddns_cb, FIND_REVERSE);

#if defined (DNS_ZONE_LOOKUP)
	if (result == ISC_R_NOTFOUND) {
		/*
		 * We didn't find a cached zone, see if we can
		 * can find a nameserver and create a zone.
		 */
		if (find_zone_start(ddns_cb, FIND_REVERSE) == ISC_R_SUCCESS) {
			/*
			 * We have started the process to find a zone
			 * queue the ddns_cb for processing after we
			 * create the zone
			 */
			/* sar - not yet implemented, currently we just
			 * arrange for things to get cleaned up
			 */
			goto cleanup;
		}
	}
#endif
	if (result != ISC_R_SUCCESS)
		goto cleanup;


	if ((result == ISC_R_SUCCESS) &&
	    !(ISC_LIST_EMPTY(ddns_cb->zone_server_list))) {
		/* Set up the zone name for use by DNS */
		result = dhcp_isc_name(ddns_cb->zone_name, &zname0, &zname);
		if (result != ISC_R_SUCCESS) {
			log_error("Unable to build name for zone for "
				  "fwd update: %s %s",
				  ddns_cb->zone_name,
				  isc_result_totext(result));
			goto cleanup;
		}
		/* If we have any addresses get them */
		if (!(ISC_LIST_EMPTY(ddns_cb->zone_server_list))) {
			zlist = &ddns_cb->zone_server_list;
		}

		/*
		 * If we now have a zone try to get the key, NULL is okay,
		 * having a key but not a tsec is odd so we warn.
		 */
		/*sar*/
		/* should we do the warning if we have a key but no tsec? */
		if ((ddns_cb->zone != NULL) && (ddns_cb->zone->key != NULL)) {
			tsec_key = ddns_cb->zone->key->tsec_key;
			if (tsec_key == NULL) {
				log_error("No tsec for use with key %s",
					  ddns_cb->zone->key->name);
			}
		}
	}

	/* We must have a name for the update list */
	/* Get a pointer to the ptrname to make things easier. */
	ptrname = (unsigned char *)ddns_cb->rev_name.data;

	if ((result = dhcp_isc_name(ptrname, &uname0, &uname))
	     != ISC_R_SUCCESS) {
		log_error("Unable to build name for fwd update: %s %s",
			  ptrname, isc_result_totext(result));
		goto cleanup;
	}

	/*
	 * Allocate the various isc dns library structures we may require.
	 * Allocating one blob avoids being halfway through the process
	 * and being unable to allocate as well as making the free easy.
	 */
	dataspace = isc_mem_get(dhcp_gbl_ctx.mctx, sizeof(*dataspace) * 2);
	if (dataspace == NULL) {
		log_error("Unable to allocate memory for fwd update");
		result = ISC_R_NOMEMORY; 
		goto cleanup;
	}

	ISC_LIST_INIT(updatelist);

	/*
	 * Construct the update list
	 * We always delete what's currently there
	 * Delete PTR RR.
	 */
	result = make_dns_dataset(dns_rdataclass_any, dns_rdatatype_ptr,
				  &dataspace[0], NULL, 0, 0);
	if (result != ISC_R_SUCCESS) {
		goto cleanup;
	}
	ISC_LIST_APPEND(uname->list, &dataspace[0].rdataset, link);

	/*
	 * If we are updating the pointer we then add the new one 
	 * Add PTR RR.
	 */
	if (ddns_cb->state == DDNS_STATE_ADD_PTR) {
#if 0
		/*
		 * I've left this dead code in the file  for now in case
		 * we decide to try and get rid of the ns_name functions.
		 * sar
		 */

		/*
		 * Need to convert pointer into on the wire representation
		 * We replace the '.' characters with the lengths of the
		 * next name and add a length to the beginning for the first
		 * name.
		 */
		if (ddns_cb->fwd_name.len == 1) {
			/* the root */
			buf[0] = 0;
			buflen = 1;
		} else {
			unsigned char *cp;
			buf[0] = '.';
			memcpy(&buf[1], ddns_cb->fwd_name.data,
			       ddns_cb->fwd_name.len);
			for(cp = buf + ddns_cb->fwd_name.len, buflen = 0;
			    cp != buf;
			    cp--) {
				if (*cp == '.') {
					*cp = buflen;
					buflen = 0;
				} else {
					buflen++;
				}
			}
			*cp = buflen;
			buflen = ddns_cb->fwd_name.len + 1;
		}
#endif
		/*
		 * Need to convert pointer into on the wire representation
		 */
		if (MRns_name_pton((char *)ddns_cb->fwd_name.data,
				   buf, 256) == -1) {
			goto cleanup;
		}
		buflen = 0;
		while (buf[buflen] != 0) {
			buflen += buf[buflen] + 1;
		}
		buflen++;

		result = make_dns_dataset(dns_rdataclass_in,
					  dns_rdatatype_ptr,
					  &dataspace[1],
					  buf, buflen, ddns_cb->ttl);
		if (result != ISC_R_SUCCESS) {
			goto cleanup;
		}
		ISC_LIST_APPEND(uname->list, &dataspace[1].rdataset, link);
	}

	ISC_LIST_APPEND(updatelist, uname, link);

	/*sar*/
	/*
	 * for now I'll cleanup the dataset immediately, it would be
	 * more efficient to keep it around in case the signaturure failed
	 * and we wanted to retry it.
	 */
	/* send the message, cleanup and return the result */
	result = ddns_update((dns_client_t *)dhcp_gbl_ctx.dnsclient,
			     dns_rdataclass_in, zname,
			     NULL, &updatelist,
			     zlist, tsec_key,
			     DNS_CLIENTRESOPT_ALLOWRUN,
			     dhcp_gbl_ctx.task,
			     ddns_interlude, (void *)ddns_cb,
			     &ddns_cb->transaction);
	if (result == ISC_R_FAMILYNOSUPPORT) {
		log_info("Unable to perform DDNS update, "
			 "address family not supported");
	}

#if defined (DEBUG_DNS_UPDATES)
	print_dns_status(DDNS_PRINT_OUTBOUND, ddns_cb, result);
#endif

 cleanup:
#if defined (DEBUG_DNS_UPDATES)
	if (result != ISC_R_SUCCESS) {
		log_info("DDNS: %s(%d): error in ddns_modify_ptr %s for %p",
			 file, line, isc_result_totext(result), ddns_cb);
	}
#endif

	if (dataspace != NULL) {
		isc_mem_put(dhcp_gbl_ctx.mctx, dataspace,
			    sizeof(*dataspace) * 2);
	}
	return(result);
}

void
ddns_cancel(dhcp_ddns_cb_t *ddns_cb, const char *file, int line) {
	ddns_cb->flags |= DDNS_ABORT;
	if (ddns_cb->transaction != NULL) {
		dns_client_cancelupdate((dns_clientupdatetrans_t *)
					ddns_cb->transaction);
	}
	ddns_cb->lease = NULL;

#if defined (DEBUG_DNS_UPDATES)
	log_info("DDNS: %s(%d): cancelling transaction for  %p",
		 file, line,  ddns_cb);
#endif
}

#endif /* NSUPDATE */

HASH_FUNCTIONS (dns_zone, const char *, struct dns_zone, dns_zone_hash_t,
		dns_zone_reference, dns_zone_dereference, do_case_hash)
