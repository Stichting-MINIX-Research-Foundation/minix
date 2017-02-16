/*	$NetBSD: omapi.c,v 1.2 2014/07/12 12:09:38 spz Exp $	*/
/* omapi.c

   OMAPI object interfaces for the DHCP server. */

/*
 * Copyright (c) 2012-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004-2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999-2003 by Internet Software Consortium
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
__RCSID("$NetBSD: omapi.c,v 1.2 2014/07/12 12:09:38 spz Exp $");

/* Many, many thanks to Brian Murrell and BCtel for this code - BCtel
   provided the funding that resulted in this code and the entire
   OMAPI support library being written, and Brian helped brainstorm
   and refine the requirements.  To the extent that this code is
   useful, you have Brian and BCtel to thank.  Any limitations in the
   code are a result of mistakes on my part.  -- Ted Lemon */

#include "dhcpd.h"
#include <omapip/omapip_p.h>

static isc_result_t class_lookup (omapi_object_t **,
				  omapi_object_t *, omapi_object_t *,
				  omapi_object_type_t *);

omapi_object_type_t *dhcp_type_lease;
omapi_object_type_t *dhcp_type_pool;
omapi_object_type_t *dhcp_type_class;
omapi_object_type_t *dhcp_type_subclass;
omapi_object_type_t *dhcp_type_host;
#if defined (FAILOVER_PROTOCOL)
omapi_object_type_t *dhcp_type_failover_state;
omapi_object_type_t *dhcp_type_failover_link;
omapi_object_type_t *dhcp_type_failover_listener;
#endif

void dhcp_db_objects_setup ()
{
	isc_result_t status;

	status = omapi_object_type_register (&dhcp_type_lease,
					     "lease",
					     dhcp_lease_set_value,
					     dhcp_lease_get_value,
					     dhcp_lease_destroy,
					     dhcp_lease_signal_handler,
					     dhcp_lease_stuff_values,
					     dhcp_lease_lookup,
					     dhcp_lease_create,
					     dhcp_lease_remove,
#if defined (COMPACT_LEASES)
					     dhcp_lease_free,
					     dhcp_lease_get,
#else
					     0, 0,
#endif
					     0,
					     sizeof (struct lease),
					     0, RC_LEASE);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register lease object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_class,
					     "class",
					     dhcp_class_set_value,
					     dhcp_class_get_value,
					     dhcp_class_destroy,
					     dhcp_class_signal_handler,
					     dhcp_class_stuff_values,
					     dhcp_class_lookup,
					     dhcp_class_create,
					     dhcp_class_remove, 0, 0, 0,
					     sizeof (struct class), 0,
					     RC_MISC);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register class object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_subclass,
					     "subclass",
					     dhcp_subclass_set_value,
					     dhcp_subclass_get_value,
					     dhcp_class_destroy,
					     dhcp_subclass_signal_handler,
					     dhcp_subclass_stuff_values,
					     dhcp_subclass_lookup,
					     dhcp_subclass_create,
					     dhcp_subclass_remove, 0, 0, 0,
					     sizeof (struct class), 0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register subclass object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_pool,
					     "pool",
					     dhcp_pool_set_value,
					     dhcp_pool_get_value,
					     dhcp_pool_destroy,
					     dhcp_pool_signal_handler,
					     dhcp_pool_stuff_values,
					     dhcp_pool_lookup,
					     dhcp_pool_create,
					     dhcp_pool_remove, 0, 0, 0,
					     sizeof (struct pool), 0, RC_MISC);

	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register pool object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_host,
					     "host",
					     dhcp_host_set_value,
					     dhcp_host_get_value,
					     dhcp_host_destroy,
					     dhcp_host_signal_handler,
					     dhcp_host_stuff_values,
					     dhcp_host_lookup,
					     dhcp_host_create,
					     dhcp_host_remove, 0, 0, 0,
					     sizeof (struct host_decl),
					     0, RC_MISC);

	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register host object type: %s",
			   isc_result_totext (status));

#if defined (FAILOVER_PROTOCOL)
	status = omapi_object_type_register (&dhcp_type_failover_state,
					     "failover-state",
					     dhcp_failover_state_set_value,
					     dhcp_failover_state_get_value,
					     dhcp_failover_state_destroy,
					     dhcp_failover_state_signal,
					     dhcp_failover_state_stuff,
					     dhcp_failover_state_lookup,
					     dhcp_failover_state_create,
					     dhcp_failover_state_remove,
					     0, 0, 0,
					     sizeof (dhcp_failover_state_t),
					     0, RC_MISC);

	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register failover state object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_failover_link,
					     "failover-link",
					     dhcp_failover_link_set_value,
					     dhcp_failover_link_get_value,
					     dhcp_failover_link_destroy,
					     dhcp_failover_link_signal,
					     dhcp_failover_link_stuff_values,
					     0, 0, 0, 0, 0, 0,
					     sizeof (dhcp_failover_link_t), 0,
					     RC_MISC);

	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register failover link object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_failover_listener,
					     "failover-listener",
					     dhcp_failover_listener_set_value,
					     dhcp_failover_listener_get_value,
					     dhcp_failover_listener_destroy,
					     dhcp_failover_listener_signal,
					     dhcp_failover_listener_stuff,
					     0, 0, 0, 0, 0, 0,
					     sizeof
					     (dhcp_failover_listener_t), 0,
					     RC_MISC);

	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register failover listener object type: %s",
			   isc_result_totext (status));
#endif /* FAILOVER_PROTOCOL */
}

isc_result_t dhcp_lease_set_value  (omapi_object_t *h,
				    omapi_object_t *id,
				    omapi_data_string_t *name,
				    omapi_typed_data_t *value)
{
	struct lease *lease;
	isc_result_t status;

	if (h -> type != dhcp_type_lease)
		return DHCP_R_INVALIDARG;
	lease = (struct lease *)h;

	/* We're skipping a lot of things it might be interesting to
	   set - for now, we just make it possible to whack the state. */
	if (!omapi_ds_strcmp (name, "state")) {
	    unsigned long bar;
	    const char *ols, *nls;
	    status = omapi_get_int_value (&bar, value);
	    if (status != ISC_R_SUCCESS)
		return status;

	    if (bar < 1 || bar > FTS_LAST)
		return DHCP_R_INVALIDARG;
	    nls = binding_state_names [bar - 1];
	    if (lease -> binding_state >= 1 &&
		lease -> binding_state <= FTS_LAST)
		ols = binding_state_names [lease -> binding_state - 1];
	    else
		ols = "unknown state";

	    if (lease -> binding_state != bar) {
		lease -> next_binding_state = bar;
		if (supersede_lease (lease, 0, 1, 1, 1)) {
			log_info ("lease %s state changed from %s to %s",
				  piaddr(lease->ip_addr), ols, nls);
			return ISC_R_SUCCESS;
		}
		log_info ("lease %s state change from %s to %s failed.",
			  piaddr (lease -> ip_addr), ols, nls);
		return ISC_R_IOERROR;
	    }
	    return DHCP_R_UNCHANGED;
	} else if (!omapi_ds_strcmp (name, "ip-address")) {
	    return ISC_R_NOPERM;
	} else if (!omapi_ds_strcmp (name, "dhcp-client-identifier")) {
	    return DHCP_R_UNCHANGED;	/* XXX take change. */
	} else if (!omapi_ds_strcmp (name, "hostname")) {
	    return DHCP_R_UNCHANGED;	/* XXX take change. */
	} else if (!omapi_ds_strcmp (name, "client-hostname")) {
	    return DHCP_R_UNCHANGED;	/* XXX take change. */
	} else if (!omapi_ds_strcmp (name, "host")) {
	    return DHCP_R_UNCHANGED;	/* XXX take change. */
	} else if (!omapi_ds_strcmp (name, "subnet")) {
	    return DHCP_R_INVALIDARG;
	} else if (!omapi_ds_strcmp (name, "pool")) {
	    return ISC_R_NOPERM;
	} else if (!omapi_ds_strcmp (name, "starts")) {
	    return ISC_R_NOPERM;
	} else if (!omapi_ds_strcmp (name, "ends")) {
	    unsigned long lease_end, old_lease_end;
	    status = omapi_get_int_value (&lease_end, value);
	    if (status != ISC_R_SUCCESS)
		return status;
	    old_lease_end = lease->ends;
	    lease->ends = lease_end;
	    if (supersede_lease (lease, 0, 1, 1, 1)) {
		log_info ("lease %s end changed from %lu to %lu",
			  piaddr(lease->ip_addr), old_lease_end, lease_end);
		return ISC_R_SUCCESS;
	    }
	    log_info ("lease %s end change from %lu to %lu failed",
		      piaddr(lease->ip_addr), old_lease_end, lease_end);
	    return ISC_R_IOERROR;
	} else if (!omapi_ds_strcmp(name, "flags")) {
	    u_int8_t oldflags;

	    if (value->type != omapi_datatype_data)
		return DHCP_R_INVALIDARG;

	    oldflags = lease->flags;
	    lease->flags = (value->u.buffer.value[0] & EPHEMERAL_FLAGS) |
			   (lease->flags & ~EPHEMERAL_FLAGS);
	    if(oldflags == lease->flags)
		return ISC_R_SUCCESS;
	    if (!supersede_lease(lease, NULL, 1, 1, 1)) {
		log_error("Failed to update flags for lease %s.",
			  piaddr(lease->ip_addr));
		return ISC_R_IOERROR;
	    }
	    return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "billing-class")) {
	    return DHCP_R_UNCHANGED;	/* XXX carefully allow change. */
	} else if (!omapi_ds_strcmp (name, "hardware-address")) {
	    return DHCP_R_UNCHANGED;	/* XXX take change. */
	} else if (!omapi_ds_strcmp (name, "hardware-type")) {
	    return DHCP_R_UNCHANGED;	/* XXX take change. */
	} else if (lease -> scope) {
	    status = binding_scope_set_value (lease -> scope, 0, name, value);
	    if (status == ISC_R_SUCCESS) {
		    if (write_lease (lease) && commit_leases ())
			    return ISC_R_SUCCESS;
		    return ISC_R_IOERROR;
	    }
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}

	if (!lease -> scope) {
		if (!binding_scope_allocate (&lease -> scope, MDL))
			return ISC_R_NOMEMORY;
	}
	status = binding_scope_set_value (lease -> scope, 1, name, value);
	if (status != ISC_R_SUCCESS)
		return status;

	if (write_lease (lease) && commit_leases ())
		return ISC_R_SUCCESS;
	return ISC_R_IOERROR;
}


isc_result_t dhcp_lease_get_value (omapi_object_t *h, omapi_object_t *id,
				   omapi_data_string_t *name,
				   omapi_value_t **value)
{
	struct lease *lease;
	isc_result_t status;

	if (h -> type != dhcp_type_lease)
		return DHCP_R_INVALIDARG;
	lease = (struct lease *)h;

	if (!omapi_ds_strcmp (name, "state"))
		return omapi_make_int_value (value, name,
					     (int)lease -> binding_state, MDL);
	else if (!omapi_ds_strcmp (name, "ip-address"))
		return omapi_make_const_value (value, name,
					       lease -> ip_addr.iabuf,
					       lease -> ip_addr.len, MDL);
	else if (!omapi_ds_strcmp (name, "dhcp-client-identifier")) {
		return omapi_make_const_value (value, name,
					       lease -> uid,
					       lease -> uid_len, MDL);
	} else if (!omapi_ds_strcmp (name, "client-hostname")) {
		if (lease -> client_hostname)
			return omapi_make_string_value
				(value, name, lease -> client_hostname, MDL);
		return ISC_R_NOTFOUND;
	} else if (!omapi_ds_strcmp (name, "host")) {
		if (lease -> host)
			return omapi_make_handle_value
				(value, name,
				 ((omapi_object_t *)lease -> host), MDL);
	} else if (!omapi_ds_strcmp (name, "subnet"))
		return omapi_make_handle_value (value, name,
						((omapi_object_t *)
						 lease -> subnet), MDL);
	else if (!omapi_ds_strcmp (name, "pool"))
		return omapi_make_handle_value (value, name,
						((omapi_object_t *)
						 lease -> pool), MDL);
	else if (!omapi_ds_strcmp (name, "billing-class")) {
		if (lease -> billing_class)
			return omapi_make_handle_value
				(value, name,
				 ((omapi_object_t *)lease -> billing_class),
				 MDL);
		return ISC_R_NOTFOUND;
	} else if (!omapi_ds_strcmp (name, "hardware-address")) {
		if (lease -> hardware_addr.hlen)
			return omapi_make_const_value
				(value, name, &lease -> hardware_addr.hbuf [1],
				 (unsigned)(lease -> hardware_addr.hlen - 1),
				 MDL);
		return ISC_R_NOTFOUND;
	} else if (!omapi_ds_strcmp (name, "hardware-type")) {
		if (lease -> hardware_addr.hlen)
			return omapi_make_int_value
				(value, name, lease -> hardware_addr.hbuf [0],
				 MDL);
		return ISC_R_NOTFOUND;
	} else if (lease -> scope) {
		status = binding_scope_get_value (value, lease -> scope, name);
		if (status != ISC_R_NOTFOUND)
			return status;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return DHCP_R_UNKNOWNATTRIBUTE;
}

isc_result_t dhcp_lease_destroy (omapi_object_t *h, const char *file, int line)
{
	struct lease *lease;

	if (h->type != dhcp_type_lease)
		return DHCP_R_INVALIDARG;
	lease = (struct lease *)h;

	if (lease-> uid)
		uid_hash_delete (lease);
	hw_hash_delete (lease);

	if (lease->on_star.on_release)
		executable_statement_dereference (&lease->on_star.on_release,
						  file, line);
	if (lease->on_star.on_expiry)
		executable_statement_dereference (&lease->on_star.on_expiry,
						  file, line);
	if (lease->on_star.on_commit)
		executable_statement_dereference (&lease->on_star.on_commit,
						  file, line);
	if (lease->scope)
		binding_scope_dereference (&lease->scope, file, line);

	if (lease->agent_options)
		option_chain_head_dereference (&lease->agent_options,
					       file, line);
	if (lease->uid && lease->uid != lease->uid_buf) {
		dfree (lease->uid, MDL);
		lease->uid = &lease->uid_buf [0];
		lease->uid_len = 0;
	}

	if (lease->client_hostname) {
		dfree (lease->client_hostname, MDL);
		lease->client_hostname = (char *)0;
	}

	if (lease->host)
		host_dereference (&lease->host, file, line);
	if (lease->subnet)
		subnet_dereference (&lease->subnet, file, line);
	if (lease->pool)
		pool_dereference (&lease->pool, file, line);

	if (lease->state) {
		free_lease_state (lease->state, file, line);
		lease->state = (struct lease_state *)0;

		cancel_timeout (lease_ping_timeout, lease);
		--outstanding_pings; /* XXX */
	}

	if (lease->billing_class)
		class_dereference
			(&lease->billing_class, file, line);

#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	/* XXX we should never be destroying a lease with a next
	   XXX pointer except on exit... */
	if (lease->next)
		lease_dereference (&lease->next, file, line);
	if (lease->n_hw)
		lease_dereference (&lease->n_hw, file, line);
	if (lease->n_uid)
		lease_dereference (&lease->n_uid, file, line);
	if (lease->next_pending)
		lease_dereference (&lease->next_pending, file, line);
#endif

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_lease_signal_handler (omapi_object_t *h,
					const char *name, va_list ap)
{
	/* h should point to (struct lease *) */
	isc_result_t status;

	if (h -> type != dhcp_type_lease)
		return DHCP_R_INVALIDARG;

	if (!strcmp (name, "updated"))
		return ISC_R_SUCCESS;

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> signal_handler) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_lease_stuff_values (omapi_object_t *c,
				      omapi_object_t *id,
				      omapi_object_t *h)
{
	u_int32_t bouncer;
	struct lease *lease;
	isc_result_t status;
	u_int8_t flagbuf;

	if (h -> type != dhcp_type_lease)
		return DHCP_R_INVALIDARG;
	lease = (struct lease *)h;

	/* Write out all the values. */

	status = omapi_connection_put_named_uint32(c, "state",
						   lease->binding_state);
	if (status != ISC_R_SUCCESS)
		return (status);

	status = omapi_connection_put_name (c, "ip-address");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, lease -> ip_addr.len);
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_copyin (c, lease -> ip_addr.iabuf,
					  lease -> ip_addr.len);
	if (status != ISC_R_SUCCESS)
		return status;

	if (lease -> uid_len) {
		status = omapi_connection_put_name (c,
						    "dhcp-client-identifier");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_uint32 (c, lease -> uid_len);
		if (status != ISC_R_SUCCESS)
			return status;
		if (lease -> uid_len) {
			status = omapi_connection_copyin (c, lease -> uid,
							  lease -> uid_len);
			if (status != ISC_R_SUCCESS)
				return status;
		}
	}

	if (lease -> client_hostname) {
		status = omapi_connection_put_name (c, "client-hostname");
		if (status != ISC_R_SUCCESS)
			return status;
		status =
			omapi_connection_put_string (c,
						     lease -> client_hostname);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	if (lease -> host) {
		status = omapi_connection_put_name (c, "host");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_handle (c,
						      (omapi_object_t *)
						      lease -> host);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	status = omapi_connection_put_name (c, "subnet");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_handle
		(c, (omapi_object_t *)lease -> subnet);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "pool");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_handle (c,
					      (omapi_object_t *)lease -> pool);
	if (status != ISC_R_SUCCESS)
		return status;

	if (lease -> billing_class) {
		status = omapi_connection_put_name (c, "billing-class");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_handle
			(c, (omapi_object_t *)lease -> billing_class);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	if (lease -> hardware_addr.hlen) {
		status = omapi_connection_put_name (c, "hardware-address");
		if (status != ISC_R_SUCCESS)
			return status;
		status = (omapi_connection_put_uint32
			  (c,
			   (unsigned long)(lease -> hardware_addr.hlen - 1)));
		if (status != ISC_R_SUCCESS)
			return status;
		status = (omapi_connection_copyin
			  (c, &lease -> hardware_addr.hbuf [1],
			   (unsigned long)(lease -> hardware_addr.hlen - 1)));

		if (status != ISC_R_SUCCESS)
			return status;

		status = omapi_connection_put_named_uint32(c, "hardware-type",
						lease->hardware_addr.hbuf[0]);
		if (status != ISC_R_SUCCESS)
			return (status);
	}

	/* TIME values may be 64-bit, depending on system architecture.
	 * OMAPI must be system independent, both in terms of transmitting
	 * bytes on the wire in network byte order, and in terms of being
	 * readable and usable by both systems.
	 *
	 * XXX: In a future feature release, a put_int64() should be made
	 * to exist, and perhaps a put_time() wrapper that selects which
	 * to use based upon sizeof(TIME).  In the meantime, use existing,
	 * 32-bit, code.
	 */
	bouncer = (u_int32_t)lease->ends;
	status = omapi_connection_put_named_uint32(c, "ends", bouncer);
	if (status != ISC_R_SUCCESS)
		return (status);

	bouncer = (u_int32_t)lease->starts;
	status = omapi_connection_put_named_uint32(c, "starts", bouncer);
	if (status != ISC_R_SUCCESS)
		return (status);

	bouncer = (u_int32_t)lease->tstp;
	status = omapi_connection_put_named_uint32(c, "tstp", bouncer);
	if (status != ISC_R_SUCCESS)
		return (status);

	bouncer = (u_int32_t)lease->tsfp;
	status = omapi_connection_put_named_uint32(c, "tsfp", bouncer);
	if (status != ISC_R_SUCCESS)
		return status;

	bouncer = (u_int32_t)lease->atsfp;
	status = omapi_connection_put_named_uint32(c, "atsfp", bouncer);
	if (status != ISC_R_SUCCESS)
		return status;

	bouncer = (u_int32_t)lease->cltt;
	status = omapi_connection_put_named_uint32(c, "cltt", bouncer);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "flags");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32(c, sizeof(flagbuf));
	if (status != ISC_R_SUCCESS)
		return status;
	flagbuf = lease->flags & EPHEMERAL_FLAGS;
	status = omapi_connection_copyin(c, &flagbuf, sizeof(flagbuf));
	if (status != ISC_R_SUCCESS)
		return status;

	if (lease -> scope) {
		status = binding_scope_stuff_values (c, lease -> scope);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_lease_lookup (omapi_object_t **lp,
				omapi_object_t *id, omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	struct lease *lease;

	if (!ref)
		return DHCP_R_NOKEYS;

	/* First see if we were sent a handle. */
	status = omapi_get_value_str (ref, id, "handle", &tv);
	if (status == ISC_R_SUCCESS) {
		status = omapi_handle_td_lookup (lp, tv -> value);

		omapi_value_dereference (&tv, MDL);
		if (status != ISC_R_SUCCESS)
			return status;

		/* Don't return the object if the type is wrong. */
		if ((*lp) -> type != dhcp_type_lease) {
			omapi_object_dereference (lp, MDL);
			return DHCP_R_INVALIDARG;
		}
	}

	/* Now look for an IP address. */
	status = omapi_get_value_str (ref, id, "ip-address", &tv);
	if (status == ISC_R_SUCCESS) {
		lease = (struct lease *)0;
		lease_ip_hash_lookup(&lease, lease_ip_addr_hash,
				     tv->value->u.buffer.value,
				     tv->value->u.buffer.len, MDL);

		omapi_value_dereference (&tv, MDL);

		/* If we already have a lease, and it's not the same one,
		   then the query was invalid. */
		if (*lp && *lp != (omapi_object_t *)lease) {
			omapi_object_dereference (lp, MDL);
			lease_dereference (&lease, MDL);
			return DHCP_R_KEYCONFLICT;
		} else if (!lease) {
			if (*lp)
				omapi_object_dereference (lp, MDL);
			return ISC_R_NOTFOUND;
		} else if (!*lp) {
			/* XXX fix so that hash lookup itself creates
			   XXX the reference. */
			omapi_object_reference (lp,
						(omapi_object_t *)lease, MDL);
			lease_dereference (&lease, MDL);
		}
	}

	/* Now look for a client identifier. */
	status = omapi_get_value_str (ref, id, "dhcp-client-identifier", &tv);
	if (status == ISC_R_SUCCESS) {
		lease = (struct lease *)0;
		lease_id_hash_lookup(&lease, lease_uid_hash,
				     tv->value->u.buffer.value,
				     tv->value->u.buffer.len, MDL);
		omapi_value_dereference (&tv, MDL);

		if (*lp && *lp != (omapi_object_t *)lease) {
			omapi_object_dereference (lp, MDL);
			lease_dereference (&lease, MDL);
			return DHCP_R_KEYCONFLICT;
		} else if (!lease) {
			if (*lp)
			    omapi_object_dereference (lp, MDL);
			return ISC_R_NOTFOUND;
		} else if (lease -> n_uid) {
			if (*lp)
			    omapi_object_dereference (lp, MDL);
			return DHCP_R_MULTIPLE;
		} else if (!*lp) {
			/* XXX fix so that hash lookup itself creates
			   XXX the reference. */
			omapi_object_reference (lp,
						(omapi_object_t *)lease, MDL);
			lease_dereference (&lease, MDL);
		}
	}

	/* Now look for a hardware address. */
	status = omapi_get_value_str (ref, id, "hardware-address", &tv);
	if (status == ISC_R_SUCCESS) {
		unsigned char *haddr;
		unsigned int len;

		len = tv -> value -> u.buffer.len + 1;
		haddr = dmalloc (len, MDL);
		if (!haddr) {
			omapi_value_dereference (&tv, MDL);
			return ISC_R_NOMEMORY;
		}

		memcpy (haddr + 1, tv -> value -> u.buffer.value, len - 1);
		omapi_value_dereference (&tv, MDL);

		status = omapi_get_value_str (ref, id, "hardware-type", &tv);
		if (status == ISC_R_SUCCESS) {
			if (tv -> value -> type == omapi_datatype_data) {
				if ((tv -> value -> u.buffer.len != 4) ||
				    (tv -> value -> u.buffer.value[0] != 0) ||
				    (tv -> value -> u.buffer.value[1] != 0) ||
				    (tv -> value -> u.buffer.value[2] != 0)) {
					omapi_value_dereference (&tv, MDL);
					dfree (haddr, MDL);
					return DHCP_R_INVALIDARG;
				}

				haddr[0] = tv -> value -> u.buffer.value[3];
			} else if (tv -> value -> type == omapi_datatype_int) {
				haddr[0] = (unsigned char)
					tv -> value -> u.integer;
			} else {
				omapi_value_dereference (&tv, MDL);
				dfree (haddr, MDL);
				return DHCP_R_INVALIDARG;
			}

			omapi_value_dereference (&tv, MDL);
		} else {
			/* If no hardware-type is specified, default to
			   ethernet.  This may or may not be a good idea,
			   but Telus is currently relying on this behavior.
			   - DPN */
			haddr[0] = HTYPE_ETHER;
		}

		lease = (struct lease *)0;
		lease_id_hash_lookup(&lease, lease_hw_addr_hash, haddr, len,
				     MDL);
		dfree (haddr, MDL);

		if (*lp && *lp != (omapi_object_t *)lease) {
			omapi_object_dereference (lp, MDL);
			lease_dereference (&lease, MDL);
			return DHCP_R_KEYCONFLICT;
		} else if (!lease) {
			if (*lp)
			    omapi_object_dereference (lp, MDL);
			return ISC_R_NOTFOUND;
		} else if (lease -> n_hw) {
			if (*lp)
			    omapi_object_dereference (lp, MDL);
			lease_dereference (&lease, MDL);
			return DHCP_R_MULTIPLE;
		} else if (!*lp) {
			/* XXX fix so that hash lookup itself creates
			   XXX the reference. */
			omapi_object_reference (lp,
						(omapi_object_t *)lease, MDL);
			lease_dereference (&lease, MDL);
		}
	}

	/* If we get to here without finding a lease, no valid key was
	   specified. */
	if (!*lp)
		return DHCP_R_NOKEYS;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_lease_create (omapi_object_t **lp,
				omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_lease_remove (omapi_object_t *lp,
				omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_host_set_value  (omapi_object_t *h,
				   omapi_object_t *id,
				   omapi_data_string_t *name,
				   omapi_typed_data_t *value)
{
	struct host_decl *host;
	isc_result_t status;

	if (h -> type != dhcp_type_host)
		return DHCP_R_INVALIDARG;
	host = (struct host_decl *)h;

	/* XXX For now, we can only set these values on new host objects.
	   XXX Soon, we need to be able to update host objects. */
	if (!omapi_ds_strcmp (name, "name")) {
		if (host -> name)
			return ISC_R_EXISTS;
		if (value && (value -> type == omapi_datatype_data ||
			      value -> type == omapi_datatype_string)) {
			host -> name = dmalloc (value -> u.buffer.len + 1,
						MDL);
			if (!host -> name)
				return ISC_R_NOMEMORY;
			memcpy (host -> name,
				value -> u.buffer.value,
				value -> u.buffer.len);
			host -> name [value -> u.buffer.len] = 0;
		} else
			return DHCP_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	if (!omapi_ds_strcmp (name, "group")) {
		if (value && (value -> type == omapi_datatype_data ||
			      value -> type == omapi_datatype_string)) {
			struct group_object *group;
			group = (struct group_object *)0;
			group_hash_lookup (&group, group_name_hash,
					   (char *)value -> u.buffer.value,
					   value -> u.buffer.len, MDL);
			if (!group || (group -> flags & GROUP_OBJECT_DELETED))
				return ISC_R_NOTFOUND;
			if (host -> group)
				group_dereference (&host -> group, MDL);
			group_reference (&host -> group, group -> group, MDL);
			if (host -> named_group)
				group_object_dereference (&host -> named_group,
							  MDL);
			group_object_reference (&host -> named_group,
						group, MDL);
			group_object_dereference (&group, MDL);
		} else
			return DHCP_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	if (!omapi_ds_strcmp (name, "hardware-address")) {
		if (host -> interface.hlen)
			return ISC_R_EXISTS;
		if (value && (value -> type == omapi_datatype_data ||
			      value -> type == omapi_datatype_string)) {
			if (value -> u.buffer.len >
			    (sizeof host -> interface.hbuf) - 1)
				return DHCP_R_INVALIDARG;
			memcpy (&host -> interface.hbuf [1],
				value -> u.buffer.value,
				value -> u.buffer.len);
			host -> interface.hlen = value -> u.buffer.len + 1;
		} else
			return DHCP_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	if (!omapi_ds_strcmp (name, "hardware-type")) {
		int type;
		if ((value != NULL) &&
		    ((value->type == omapi_datatype_data) &&
		     (value->u.buffer.len == sizeof(type)))) {
			if (value->u.buffer.len > sizeof(type))
				return (DHCP_R_INVALIDARG);
			memcpy(&type, value->u.buffer.value,
			       value->u.buffer.len);
			type = ntohl(type);
		} else if ((value != NULL) &&
			   (value->type == omapi_datatype_int))
			type = value->u.integer;
		else
			return (DHCP_R_INVALIDARG);
		host->interface.hbuf[0] = type;
		return (ISC_R_SUCCESS);
	}

	if (!omapi_ds_strcmp (name, "dhcp-client-identifier")) {
		if (host -> client_identifier.data)
			return ISC_R_EXISTS;
		if (value && (value -> type == omapi_datatype_data ||
			      value -> type == omapi_datatype_string)) {
		    if (!buffer_allocate (&host -> client_identifier.buffer,
					  value -> u.buffer.len, MDL))
			    return ISC_R_NOMEMORY;
		    host -> client_identifier.data =
			    &host -> client_identifier.buffer -> data [0];
		    memcpy (host -> client_identifier.buffer -> data,
			    value -> u.buffer.value,
			    value -> u.buffer.len);
		    host -> client_identifier.len = value -> u.buffer.len;
		} else
		    return DHCP_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	if (!omapi_ds_strcmp (name, "ip-address")) {
		if (host -> fixed_addr)
			option_cache_dereference (&host -> fixed_addr, MDL);
		if (!value)
			return ISC_R_SUCCESS;
		if (value && (value -> type == omapi_datatype_data ||
			      value -> type == omapi_datatype_string)) {
			struct data_string ds;
			memset (&ds, 0, sizeof ds);
			ds.len = value -> u.buffer.len;
			if (!buffer_allocate (&ds.buffer, ds.len, MDL))
				return ISC_R_NOMEMORY;
			ds.data = (&ds.buffer -> data [0]);
			memcpy (ds.buffer -> data,
				value -> u.buffer.value, ds.len);
			if (!option_cache (&host -> fixed_addr,
					   &ds, (struct expression *)0,
					   (struct option *)0, MDL)) {
				data_string_forget (&ds, MDL);
				return ISC_R_NOMEMORY;
			}
			data_string_forget (&ds, MDL);
		} else
			return DHCP_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	if (!omapi_ds_strcmp (name, "statements")) {
		if (!host -> group) {
			if (!clone_group (&host -> group, root_group, MDL))
				return ISC_R_NOMEMORY;
		} else {
			if (host -> group -> statements &&
			    (!host -> named_group ||
			     host -> group != host -> named_group -> group) &&
			    host -> group != root_group)
				return ISC_R_EXISTS;
			if (!clone_group (&host -> group, host -> group, MDL))
				return ISC_R_NOMEMORY;
		}
		if (!host -> group)
			return ISC_R_NOMEMORY;
		if (value && (value -> type == omapi_datatype_data ||
			      value -> type == omapi_datatype_string)) {
			struct parse *parse;
			int lose = 0;
			parse = (struct parse *)0;
			status = new_parse(&parse, -1,
					    (char *) value->u.buffer.value,
					    value->u.buffer.len,
					    "network client", 0);
			if (status != ISC_R_SUCCESS || parse == NULL)
				return status;

			if (!(parse_executable_statements
			      (&host -> group -> statements, parse, &lose,
			       context_any))) {
				end_parse (&parse);
				return DHCP_R_BADPARSE;
			}
			end_parse (&parse);
		} else
			return DHCP_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	/* The "known" flag isn't supported in the database yet, but it's
	   legitimate. */
	if (!omapi_ds_strcmp (name, "known")) {
		return ISC_R_SUCCESS;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}

	return DHCP_R_UNKNOWNATTRIBUTE;
}


isc_result_t dhcp_host_get_value (omapi_object_t *h, omapi_object_t *id,
				   omapi_data_string_t *name,
				   omapi_value_t **value)
{
	struct host_decl *host;
	isc_result_t status;
	struct data_string ip_addrs;

	if (h -> type != dhcp_type_host)
		return DHCP_R_INVALIDARG;
	host = (struct host_decl *)h;

	if (!omapi_ds_strcmp (name, "ip-addresses")) {
	    memset (&ip_addrs, 0, sizeof ip_addrs);
	    if (host -> fixed_addr &&
		evaluate_option_cache (&ip_addrs, (struct packet *)0,
				       (struct lease *)0,
				       (struct client_state *)0,
				       (struct option_state *)0,
				       (struct option_state *)0,
				       &global_scope,
				       host -> fixed_addr, MDL)) {
		    status = omapi_make_const_value (value, name,
						     ip_addrs.data,
						     ip_addrs.len, MDL);
		    data_string_forget (&ip_addrs, MDL);
		    return status;
	    }
	    return ISC_R_NOTFOUND;
	}

	if (!omapi_ds_strcmp (name, "dhcp-client-identifier")) {
		if (!host -> client_identifier.len)
			return ISC_R_NOTFOUND;
		return omapi_make_const_value (value, name,
					       host -> client_identifier.data,
					       host -> client_identifier.len,
					       MDL);
	}

	if (!omapi_ds_strcmp (name, "name"))
		return omapi_make_string_value (value, name, host -> name,
						MDL);

	if (!omapi_ds_strcmp (name, "hardware-address")) {
		if (!host -> interface.hlen)
			return ISC_R_NOTFOUND;
		return (omapi_make_const_value
			(value, name, &host -> interface.hbuf [1],
			 (unsigned long)(host -> interface.hlen - 1), MDL));
	}

	if (!omapi_ds_strcmp (name, "hardware-type")) {
		if (!host -> interface.hlen)
			return ISC_R_NOTFOUND;
		return omapi_make_int_value (value, name,
					     host -> interface.hbuf [0], MDL);
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return DHCP_R_UNKNOWNATTRIBUTE;
}

isc_result_t dhcp_host_destroy (omapi_object_t *h, const char *file, int line)
{

	if (h -> type != dhcp_type_host)
		return DHCP_R_INVALIDARG;

#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	struct host_decl *host = (struct host_decl *)h;
	if (host -> n_ipaddr)
		host_dereference (&host -> n_ipaddr, file, line);
	if (host -> n_dynamic)
		host_dereference (&host -> n_dynamic, file, line);
	if (host -> name) {
		dfree (host -> name, file, line);
		host -> name = (char *)0;
	}
	data_string_forget (&host -> client_identifier, file, line);
	if (host -> fixed_addr)
		option_cache_dereference (&host -> fixed_addr, file, line);
	if (host -> group)
		group_dereference (&host -> group, file, line);
	if (host -> named_group)
		omapi_object_dereference ((omapi_object_t **)
					  &host -> named_group, file, line);
	data_string_forget (&host -> auth_key_id, file, line);
#endif

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_host_signal_handler (omapi_object_t *h,
				       const char *name, va_list ap)
{
	struct host_decl *host;
	isc_result_t status;
	int updatep = 0;

	if (h -> type != dhcp_type_host)
		return DHCP_R_INVALIDARG;
	host = (struct host_decl *)h;

	if (!strcmp (name, "updated")) {
		/* There must be a client identifier of some sort. */
		if (host -> interface.hlen == 0 &&
		    !host -> client_identifier.len)
			return DHCP_R_INVALIDARG;

		if (!host -> name) {
			char hnbuf [64];
			sprintf (hnbuf, "nh%08lx%08lx",
				 (unsigned long)cur_time, (unsigned long)host);
			host -> name = dmalloc (strlen (hnbuf) + 1, MDL);
			if (!host -> name)
				return ISC_R_NOMEMORY;
			strcpy (host -> name, hnbuf);
		}

#ifdef DEBUG_OMAPI
		log_debug ("OMAPI added host %s", host -> name);
#endif
		status = enter_host (host, 1, 1);
		if (status != ISC_R_SUCCESS)
			return status;
		updatep = 1;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> signal_handler) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	if (updatep)
		return ISC_R_SUCCESS;
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_host_stuff_values (omapi_object_t *c,
				      omapi_object_t *id,
				      omapi_object_t *h)
{
	struct host_decl *host;
	isc_result_t status;
	struct data_string ip_addrs;

	if (h -> type != dhcp_type_host)
		return DHCP_R_INVALIDARG;
	host = (struct host_decl *)h;

	/* Write out all the values. */

	memset (&ip_addrs, 0, sizeof ip_addrs);
	if (host -> fixed_addr &&
	    evaluate_option_cache (&ip_addrs, (struct packet *)0,
				   (struct lease *)0,
				   (struct client_state *)0,
				   (struct option_state *)0,
				   (struct option_state *)0,
				   &global_scope,
				   host -> fixed_addr, MDL)) {
		status = omapi_connection_put_name (c, "ip-address");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_uint32 (c, ip_addrs.len);
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_copyin (c,
						  ip_addrs.data, ip_addrs.len);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	if (host -> client_identifier.len) {
		status = omapi_connection_put_name (c,
						    "dhcp-client-identifier");
		if (status != ISC_R_SUCCESS)
			return status;
		status = (omapi_connection_put_uint32
			  (c, host -> client_identifier.len));
		if (status != ISC_R_SUCCESS)
			return status;
		status = (omapi_connection_copyin
			  (c,
			   host -> client_identifier.data,
			   host -> client_identifier.len));
		if (status != ISC_R_SUCCESS)
			return status;
	}

	if (host -> name) {
		status = omapi_connection_put_name (c, "name");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_string (c, host -> name);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	if (host -> interface.hlen) {
		status = omapi_connection_put_name (c, "hardware-address");
		if (status != ISC_R_SUCCESS)
			return status;
		status = (omapi_connection_put_uint32
			  (c, (unsigned long)(host -> interface.hlen - 1)));
		if (status != ISC_R_SUCCESS)
			return status;
		status = (omapi_connection_copyin
			  (c, &host -> interface.hbuf [1],
			   (unsigned long)(host -> interface.hlen - 1)));
		if (status != ISC_R_SUCCESS)
			return status;

		status = omapi_connection_put_named_uint32(c, "hardware-type",
							   host->interface.hbuf[0]);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_host_lookup (omapi_object_t **lp,
			       omapi_object_t *id, omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	struct host_decl *host;

	if (!ref)
		return DHCP_R_NOKEYS;

	/* First see if we were sent a handle. */
	status = omapi_get_value_str (ref, id, "handle", &tv);
	if (status == ISC_R_SUCCESS) {
		status = omapi_handle_td_lookup (lp, tv -> value);

		omapi_value_dereference (&tv, MDL);
		if (status != ISC_R_SUCCESS)
			return status;

		/* Don't return the object if the type is wrong. */
		if ((*lp) -> type != dhcp_type_host) {
			omapi_object_dereference (lp, MDL);
			return DHCP_R_INVALIDARG;
		}
		if (((struct host_decl *)(*lp)) -> flags & HOST_DECL_DELETED) {
			omapi_object_dereference (lp, MDL);
		}
	}

	/* Now look for a client identifier. */
	status = omapi_get_value_str (ref, id, "dhcp-client-identifier", &tv);
	if (status == ISC_R_SUCCESS) {
		host = (struct host_decl *)0;
		host_hash_lookup (&host, host_uid_hash,
				  tv -> value -> u.buffer.value,
				  tv -> value -> u.buffer.len, MDL);
		omapi_value_dereference (&tv, MDL);

		if (*lp && *lp != (omapi_object_t *)host) {
			omapi_object_dereference (lp, MDL);
			if (host)
				host_dereference (&host, MDL);
			return DHCP_R_KEYCONFLICT;
		} else if (!host || (host -> flags & HOST_DECL_DELETED)) {
			if (*lp)
			    omapi_object_dereference (lp, MDL);
			if (host)
				host_dereference (&host, MDL);
			return ISC_R_NOTFOUND;
		} else if (!*lp) {
			/* XXX fix so that hash lookup itself creates
			   XXX the reference. */
			omapi_object_reference (lp,
						(omapi_object_t *)host, MDL);
			host_dereference (&host, MDL);
		}
	}

	/* Now look for a hardware address. */
	status = omapi_get_value_str (ref, id, "hardware-address", &tv);
	if (status == ISC_R_SUCCESS) {
		unsigned char *haddr;
		unsigned int len;

		len = tv -> value -> u.buffer.len + 1;
		haddr = dmalloc (len, MDL);
		if (!haddr) {
			omapi_value_dereference (&tv, MDL);
			return ISC_R_NOMEMORY;
		}

		memcpy (haddr + 1, tv -> value -> u.buffer.value, len - 1);
		omapi_value_dereference (&tv, MDL);

		status = omapi_get_value_str (ref, id, "hardware-type", &tv);
		if (status == ISC_R_SUCCESS) {
			if (tv -> value -> type == omapi_datatype_data) {
				if ((tv -> value -> u.buffer.len != 4) ||
				    (tv -> value -> u.buffer.value[0] != 0) ||
				    (tv -> value -> u.buffer.value[1] != 0) ||
				    (tv -> value -> u.buffer.value[2] != 0)) {
					omapi_value_dereference (&tv, MDL);
					dfree (haddr, MDL);
					return DHCP_R_INVALIDARG;
				}

				haddr[0] = tv -> value -> u.buffer.value[3];
			} else if (tv -> value -> type == omapi_datatype_int) {
				haddr[0] = (unsigned char)
					tv -> value -> u.integer;
			} else {
				omapi_value_dereference (&tv, MDL);
				dfree (haddr, MDL);
				return DHCP_R_INVALIDARG;
			}

			omapi_value_dereference (&tv, MDL);
		} else {
			/* If no hardware-type is specified, default to
			   ethernet.  This may or may not be a good idea,
			   but Telus is currently relying on this behavior.
			   - DPN */
			haddr[0] = HTYPE_ETHER;
		}

		host = (struct host_decl *)0;
		host_hash_lookup (&host, host_hw_addr_hash, haddr, len, MDL);
		dfree (haddr, MDL);

		if (*lp && *lp != (omapi_object_t *)host) {
			omapi_object_dereference (lp, MDL);
			if (host)
				host_dereference (&host, MDL);
			return DHCP_R_KEYCONFLICT;
		} else if (!host || (host -> flags & HOST_DECL_DELETED)) {
			if (*lp)
			    omapi_object_dereference (lp, MDL);
			if (host)
				host_dereference (&host, MDL);
			return ISC_R_NOTFOUND;
		} else if (!*lp) {
			/* XXX fix so that hash lookup itself creates
			   XXX the reference. */
			omapi_object_reference (lp,
						(omapi_object_t *)host, MDL);
			host_dereference (&host, MDL);
		}
	}

	/* Now look for an ip address. */
	status = omapi_get_value_str (ref, id, "ip-address", &tv);
	if (status == ISC_R_SUCCESS) {
		struct lease *l;

		/* first find the lease for this ip address */
		l = (struct lease *)0;
		lease_ip_hash_lookup(&l, lease_ip_addr_hash,
				     tv->value->u.buffer.value,
				     tv->value->u.buffer.len, MDL);
		omapi_value_dereference (&tv, MDL);

		if (!l && !*lp)
			return ISC_R_NOTFOUND;

		if (l) {
			/* now use that to get a host */
			host = (struct host_decl *)0;
			host_hash_lookup (&host, host_hw_addr_hash,
					  l -> hardware_addr.hbuf,
					  l -> hardware_addr.hlen, MDL);

			if (host && *lp && *lp != (omapi_object_t *)host) {
			    omapi_object_dereference (lp, MDL);
			    if (host)
				host_dereference (&host, MDL);
			    return DHCP_R_KEYCONFLICT;
			} else if (!host || (host -> flags &
					     HOST_DECL_DELETED)) {
			    if (host)
				host_dereference (&host, MDL);
			    if (!*lp)
				    return ISC_R_NOTFOUND;
			} else if (!*lp) {
				/* XXX fix so that hash lookup itself creates
				   XXX the reference. */
			    omapi_object_reference (lp, (omapi_object_t *)host,
						    MDL);
			    host_dereference (&host, MDL);
			}
			lease_dereference (&l, MDL);
		}
	}

	/* Now look for a name. */
	status = omapi_get_value_str (ref, id, "name", &tv);
	if (status == ISC_R_SUCCESS) {
		host = (struct host_decl *)0;
		host_hash_lookup (&host, host_name_hash,
				  tv -> value -> u.buffer.value,
				  tv -> value -> u.buffer.len, MDL);
		omapi_value_dereference (&tv, MDL);

		if (*lp && *lp != (omapi_object_t *)host) {
			omapi_object_dereference (lp, MDL);
			if (host)
			    host_dereference (&host, MDL);
			return DHCP_R_KEYCONFLICT;
		} else if (!host || (host -> flags & HOST_DECL_DELETED)) {
			if (host)
			    host_dereference (&host, MDL);
			return ISC_R_NOTFOUND;
		} else if (!*lp) {
			/* XXX fix so that hash lookup itself creates
			   XXX the reference. */
			omapi_object_reference (lp,
						(omapi_object_t *)host, MDL);
			host_dereference (&host, MDL);
		}
	}

	/* If we get to here without finding a host, no valid key was
	   specified. */
	if (!*lp)
		return DHCP_R_NOKEYS;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_host_create (omapi_object_t **lp,
			       omapi_object_t *id)
{
	struct host_decl *hp;
	isc_result_t status;
	hp = (struct host_decl *)0;
	status = host_allocate (&hp, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	group_reference (&hp -> group, root_group, MDL);
	hp -> flags = HOST_DECL_DYNAMIC;
	status = omapi_object_reference (lp, (omapi_object_t *)hp, MDL);
	host_dereference (&hp, MDL);
	return status;
}

isc_result_t dhcp_host_remove (omapi_object_t *lp,
			       omapi_object_t *id)
{
	struct host_decl *hp;
	if (lp -> type != dhcp_type_host)
		return DHCP_R_INVALIDARG;
	hp = (struct host_decl *)lp;

#ifdef DEBUG_OMAPI
	log_debug ("OMAPI delete host %s", hp -> name);
#endif
	delete_host (hp, 1);
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_pool_set_value  (omapi_object_t *h,
				   omapi_object_t *id,
				   omapi_data_string_t *name,
				   omapi_typed_data_t *value)
{
	/* h should point to (struct pool *) */
	isc_result_t status;

	if (h -> type != dhcp_type_pool)
		return DHCP_R_INVALIDARG;

	/* No values to set yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}

	return DHCP_R_UNKNOWNATTRIBUTE;
}


isc_result_t dhcp_pool_get_value (omapi_object_t *h, omapi_object_t *id,
				  omapi_data_string_t *name,
				  omapi_value_t **value)
{
	/* h should point to (struct pool *) */
	isc_result_t status;

	if (h -> type != dhcp_type_pool)
		return DHCP_R_INVALIDARG;

	/* No values to get yet. */

	/* Try to find some inner object that can provide the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return DHCP_R_UNKNOWNATTRIBUTE;
}

isc_result_t dhcp_pool_destroy (omapi_object_t *h, const char *file, int line)
{
#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	struct permit *pc, *pn;
#endif

	if (h -> type != dhcp_type_pool)
		return DHCP_R_INVALIDARG;

#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	struct pool *pool = (struct pool *)h;
	if (pool -> next)
		pool_dereference (&pool -> next, file, line);
	if (pool -> group)
		group_dereference (&pool -> group, file, line);
	if (pool -> shared_network)
	    shared_network_dereference (&pool -> shared_network, file, line);
	if (pool -> active)
		lease_dereference (&pool -> active, file, line);
	if (pool -> expired)
		lease_dereference (&pool -> expired, file, line);
	if (pool -> free)
		lease_dereference (&pool -> free, file, line);
	if (pool -> backup)
		lease_dereference (&pool -> backup, file, line);
	if (pool -> abandoned)
		lease_dereference (&pool -> abandoned, file, line);
#if defined (FAILOVER_PROTOCOL)
	if (pool -> failover_peer)
		dhcp_failover_state_dereference (&pool -> failover_peer,
						 file, line);
#endif
	for (pc = pool -> permit_list; pc; pc = pn) {
		pn = pc -> next;
		free_permit (pc, file, line);
	}
	pool -> permit_list = (struct permit *)0;

	for (pc = pool -> prohibit_list; pc; pc = pn) {
		pn = pc -> next;
		free_permit (pc, file, line);
	}
	pool -> prohibit_list = (struct permit *)0;
#endif

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_pool_signal_handler (omapi_object_t *h,
				       const char *name, va_list ap)
{
	/* h should point to (struct pool *) */
	isc_result_t status;

	if (h -> type != dhcp_type_pool)
		return DHCP_R_INVALIDARG;

	/* Can't write pools yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> signal_handler) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_pool_stuff_values (omapi_object_t *c,
				     omapi_object_t *id,
				     omapi_object_t *h)
{
	struct pool *pool;
	isc_result_t status;

	if (h->type != dhcp_type_pool)
		return (DHCP_R_INVALIDARG);
	pool = (struct pool *)h;

	/*
	 * I don't think we can actually find a pool yet
	 * but include the output of interesting values
	 * for when we do
	 */
	status = omapi_connection_put_named_uint32(c, "lease-count",
						   ((u_int32_t)
						    pool->lease_count));
	if (status != ISC_R_SUCCESS)
		return (status);

	status = omapi_connection_put_named_uint32(c, "free-leases",
						   ((u_int32_t)
						    pool->free_leases));
	if (status != ISC_R_SUCCESS)
		return (status);

	status = omapi_connection_put_named_uint32(c, "backup-leases", 
						   ((u_int32_t)
						    pool->backup_leases));
	if (status != ISC_R_SUCCESS)
		return (status);
	/* we could add time stamps but lets wait on those */

	/* Write out the inner object, if any. */
	if (h->inner && h->inner->type->stuff_values) {
		status = ((*(h->inner->type->stuff_values))
			  (c, id, h->inner));
		if (status == ISC_R_SUCCESS)
			return (status);
	}

	return (ISC_R_SUCCESS);
}

isc_result_t dhcp_pool_lookup (omapi_object_t **lp,
			       omapi_object_t *id, omapi_object_t *ref)
{
	/* Can't look up pools yet. */

	/* If we get to here without finding a pool, no valid key was
	   specified. */
	if (!*lp)
		return DHCP_R_NOKEYS;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_pool_create (omapi_object_t **lp,
			       omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_pool_remove (omapi_object_t *lp,
			       omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

static isc_result_t
class_set_value (omapi_object_t *h,
		 omapi_object_t *id,
		 omapi_data_string_t *name,
		 omapi_typed_data_t *value)
{
	struct class *class;
	struct class *superclass = 0;
	isc_result_t status;
	int issubclass = (h -> type == dhcp_type_subclass);

	class = (struct class *)h;

	if (!omapi_ds_strcmp(name, "name")) {
		char *tname;

		if (class->name)
			return ISC_R_EXISTS;

		if ((tname = dmalloc(value->u.buffer.len + 1, MDL)) == NULL) {
			return ISC_R_NOMEMORY;
		}

		/* tname is null terminated from dmalloc() */
		memcpy(tname, value->u.buffer.value, value->u.buffer.len);

		if (issubclass) {
			status = find_class(&superclass, tname, MDL);
			dfree(tname, MDL);

			if (status == ISC_R_NOTFOUND)
				return status;

			if (class->superclass != NULL)
				class_dereference(&class->superclass, MDL);
			class_reference(&class->superclass, superclass, MDL);

			if (class->group != NULL)
				group_dereference(&class->group, MDL);
			group_reference(&class->group, superclass->group, MDL);

			class->lease_limit = superclass->lease_limit;
			if (class->lease_limit != 0) {
				class->billed_leases =
					dmalloc(class->lease_limit *
						sizeof(struct lease *),
						MDL);
				if (class->billed_leases == NULL) {
					return ISC_R_NOMEMORY;
				}
			}

		} else if (value->type == omapi_datatype_data ||
			   value->type == omapi_datatype_string) {
			class->name = dmalloc(value->u.buffer.len + 1, MDL);
			if (!class->name)
				return ISC_R_NOMEMORY;

			/* class->name is null-terminated from dmalloc() */
			memcpy(class->name, value->u.buffer.value,
			       value->u.buffer.len);
		} else
			return DHCP_R_INVALIDARG;

		return ISC_R_SUCCESS;
	}


	if (issubclass && !omapi_ds_strcmp(name, "hashstring")) {
		if (class->hash_string.data)
			return ISC_R_EXISTS;

		if (value->type == omapi_datatype_data ||
		    value->type == omapi_datatype_string) {
			if (!buffer_allocate(&class->hash_string.buffer,
					     value->u.buffer.len, MDL))
				return ISC_R_NOMEMORY;
			class->hash_string.data =
					class->hash_string.buffer->data;
			memcpy(class->hash_string.buffer->data,
			       value->u.buffer.value, value->u.buffer.len);
			class->hash_string.len = value->u.buffer.len;
		} else
			return DHCP_R_INVALIDARG;

		return ISC_R_SUCCESS;
	}

	if (!omapi_ds_strcmp(name, "group")) {
		if (value->type == omapi_datatype_data ||
		    value->type == omapi_datatype_string) {
			struct group_object *group = NULL;

			group_hash_lookup(&group, group_name_hash,
					  (char *)value->u.buffer.value,
					  value->u.buffer.len, MDL);
			if (!group || (group->flags & GROUP_OBJECT_DELETED))
				return ISC_R_NOTFOUND;
			if (class->group)
				group_dereference(&class->group, MDL);
			group_reference(&class->group, group->group, MDL);
			group_object_dereference(&group, MDL);
		} else
			return DHCP_R_INVALIDARG;

		return ISC_R_SUCCESS;
	}


	/* note we do not support full expressions via omapi because the
	   expressions parser needs to be re-done to support parsing from
	   strings and not just files. */

	if (!omapi_ds_strcmp(name, "match")) {
		if (value->type == omapi_datatype_data ||
		    value->type == omapi_datatype_string) {
			unsigned minlen = (value->u.buffer.len > 8 ?
						8 : value->u.buffer.len);

			if (!strncmp("hardware",
				     (char *)value->u.buffer.value, minlen))
			{
				if (!expression_allocate(&class->submatch, MDL))
					return ISC_R_NOMEMORY;

				class->submatch->op = expr_hardware;
			} else
				return DHCP_R_INVALIDARG;
		} else
			return DHCP_R_INVALIDARG;

		return ISC_R_SUCCESS;
	}


	if (!omapi_ds_strcmp(name, "option")) {
		if (value->type == omapi_datatype_data ||
		    value->type == omapi_datatype_string) {
			/* XXXJAB support 'options' here. */
			/* XXXJAB specifically 'bootfile-name' */
			return DHCP_R_INVALIDARG; /* XXX tmp */
		} else
			return DHCP_R_INVALIDARG;

		/*
		 * Currently no way to get here, if we update the above
		 * code so that we do get here this return needs to be
		 * uncommented.
		 * return ISC_R_SUCCESS;
		 */
	}


	/* Try to find some inner object that can take the value. */
	if (h->inner && h->inner->type->set_value) {
		status = ((*(h->inner->type->set_value))
			  (h->inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}

	return DHCP_R_UNKNOWNATTRIBUTE;
}



isc_result_t dhcp_class_set_value  (omapi_object_t *h,
				    omapi_object_t *id,
				    omapi_data_string_t *name,
				    omapi_typed_data_t *value)
{
	if (h -> type != dhcp_type_class)
		return DHCP_R_INVALIDARG;

	return class_set_value(h, id, name, value);
}

isc_result_t dhcp_class_get_value (omapi_object_t *h, omapi_object_t *id,
				   omapi_data_string_t *name,
				   omapi_value_t **value)
{
	struct class *class;
	isc_result_t status;

	if (h -> type != dhcp_type_class)
		return DHCP_R_INVALIDARG;
	class = (struct class *)h;

	if (!omapi_ds_strcmp (name, "name"))
		return omapi_make_string_value (value, name, class -> name,
						MDL);

	/* Try to find some inner object that can provide the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return DHCP_R_UNKNOWNATTRIBUTE;
}

isc_result_t dhcp_class_destroy (omapi_object_t *h, const char *file, int line)
{

	if (h -> type != dhcp_type_class && h -> type != dhcp_type_subclass)
		return DHCP_R_INVALIDARG;
	struct class *class = (struct class *)h;

	if (class -> nic)
		class_dereference (&class -> nic, file, line);
	if (class -> superclass)
		class_dereference (&class -> superclass, file, line);
	if (class -> name) {
		dfree (class -> name, file, line);
		class -> name = (char *)0;
	}
	if (class -> billed_leases) {
		int i;
		for (i = 0; i < class -> lease_limit; i++) {
			if (class -> billed_leases [i]) {
				lease_dereference (&class -> billed_leases [i],
						   file, line);
			}
		}
		dfree (class -> billed_leases, file, line);
		class -> billed_leases = (struct lease **)0;
	}
	if (class -> hash) {
		class_free_hash_table (&class -> hash, file, line);
		class -> hash = (class_hash_t *)0;
	}
	data_string_forget (&class -> hash_string, file, line);

	if (class -> expr)
		expression_dereference (&class -> expr, file, line);
	if (class -> submatch)
		expression_dereference (&class -> submatch, file, line);
	if (class -> group)
		group_dereference (&class -> group, file, line);
	if (class -> statements)
		executable_statement_dereference (&class -> statements,
						  file, line);
	if (class -> superclass)
		class_dereference (&class -> superclass, file, line);

	return ISC_R_SUCCESS;
}

static isc_result_t
class_signal_handler(omapi_object_t *h,
		     const char *name, va_list ap)
{
	struct class *class = (struct class *)h;
	isc_result_t status;
	int updatep = 0;
	int issubclass;

	issubclass = (h->type == dhcp_type_subclass);

	if (!strcmp (name, "updated")) {

		if (!issubclass) {
			if (class->name == 0 || strlen(class->name) == 0) {
				return DHCP_R_INVALIDARG;
			}
		} else {
			if (class->superclass == 0) {
				return DHCP_R_INVALIDARG; /* didn't give name */
			}

			if (class->hash_string.data == NULL) {
				return DHCP_R_INVALIDARG;
			}
		}


		if (issubclass) {
			if (!class->superclass->hash)
				class_new_hash(&class->superclass->hash,
					       SCLASS_HASH_SIZE, MDL);

			class_hash_add(class->superclass->hash,
				       (const char *)class->hash_string.data,
				       class->hash_string.len,
				       (void *)class, MDL);
		}

#ifdef DEBUG_OMAPI
		if (issubclass) {
			log_debug ("OMAPI added subclass %s",
				   class->superclass->name);
		} else {
			log_debug ("OMAPI added class %s", class->name);
		}
#endif

		status = enter_class (class, 1, 1);
		if (status != ISC_R_SUCCESS)
			return status;
		updatep = 1;
	}

	/* Try to find some inner object that can take the value. */
	if (h->inner && h->inner->type->signal_handler) {
		status = ((*(h->inner->type->signal_handler))
			  (h->inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	if (updatep)
		return ISC_R_SUCCESS;

	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_class_signal_handler (omapi_object_t *h,
					const char *name, va_list ap)
{
	if (h -> type != dhcp_type_class)
		return DHCP_R_INVALIDARG;

	return class_signal_handler(h, name, ap);
}


/*
 * Routine to put out generic class & subclass information
 */
static isc_result_t class_stuff_values (omapi_object_t *c,
				 omapi_object_t *id,
				 omapi_object_t *h)
{
	struct class *class;
	isc_result_t status;

	class = (struct class *)h;

	status = omapi_connection_put_named_uint32(c, "lease-limit",
						   ((u_int32_t)
						    class->lease_limit));
	if (status != ISC_R_SUCCESS)
		return (status);

	status = omapi_connection_put_named_uint32(c, "leases-used",
						   ((u_int32_t)
						    class->leases_consumed));
	if (status != ISC_R_SUCCESS)
		return (status);

	/* Write out the inner object, if any. */
	if (h->inner && h->inner->type->stuff_values) {
		status = ((*(h->inner->type->stuff_values))
			  (c, id, h->inner));
		if (status == ISC_R_SUCCESS)
			return (status);
	}

	return (ISC_R_SUCCESS);
}


isc_result_t dhcp_class_stuff_values (omapi_object_t *c,
				      omapi_object_t *id,
				      omapi_object_t *h)
{
	if (h->type != dhcp_type_class)
		return (DHCP_R_INVALIDARG);

	/* add any class specific items here */

	return (class_stuff_values(c, id, h));
}

static isc_result_t class_lookup (omapi_object_t **lp,
				  omapi_object_t *id, omapi_object_t *ref,
				  omapi_object_type_t *typewanted)
{
	omapi_value_t *nv = NULL;
	omapi_value_t *hv = NULL;
	isc_result_t status;
	struct class *class = 0;
	struct class *subclass = 0;

	*lp = NULL;

	if (ref == NULL)
		return (DHCP_R_NOKEYS);

	/* see if we have a name */
	status = omapi_get_value_str(ref, id, "name", &nv);
	if (status == ISC_R_SUCCESS) {
		char *name = dmalloc(nv->value->u.buffer.len + 1, MDL);
		memcpy (name,
			nv->value->u.buffer.value,
			nv->value->u.buffer.len);

		omapi_value_dereference(&nv, MDL);

		find_class(&class, name, MDL);

		dfree(name, MDL);

		if (class == NULL) {
			return (ISC_R_NOTFOUND);
		}

		if (typewanted == dhcp_type_subclass) {
			status = omapi_get_value_str(ref, id,
						     "hashstring", &hv);
			if (status != ISC_R_SUCCESS) {
				class_dereference(&class, MDL);
				return (DHCP_R_NOKEYS);
			}

			if (hv->value->type != omapi_datatype_data &&
			    hv->value->type != omapi_datatype_string) {
				class_dereference(&class, MDL);
				omapi_value_dereference(&hv, MDL);
				return (DHCP_R_NOKEYS);
			}

			class_hash_lookup(&subclass, class->hash,
					  (const char *)
					  hv->value->u.buffer.value,
					  hv->value->u.buffer.len, MDL);

			omapi_value_dereference(&hv, MDL);

			class_dereference(&class, MDL);

			if (subclass == NULL) {
				return (ISC_R_NOTFOUND);
			}

			class_reference(&class, subclass, MDL);
			class_dereference(&subclass, MDL);
		}

		/* Don't return the object if the type is wrong. */
		if (class->type != typewanted) {
			class_dereference(&class, MDL);
			return (DHCP_R_INVALIDARG);
		}

		if (class->flags & CLASS_DECL_DELETED) {
			class_dereference(&class, MDL);
			return (ISC_R_NOTFOUND);
		}

		omapi_object_reference(lp, (omapi_object_t *)class, MDL);
		class_dereference(&class, MDL);

		return (ISC_R_SUCCESS);
	}

	return (DHCP_R_NOKEYS);
}


isc_result_t dhcp_class_lookup (omapi_object_t **lp,
				omapi_object_t *id, omapi_object_t *ref)
{
	return class_lookup(lp, id, ref, dhcp_type_class);
}

isc_result_t dhcp_class_create (omapi_object_t **lp,
				omapi_object_t *id)
{
	struct class *cp = 0;
	isc_result_t status;

	status = class_allocate(&cp, MDL);
	if (status != ISC_R_SUCCESS)
		return (status);

	clone_group(&cp->group, root_group, MDL);
	cp->flags = CLASS_DECL_DYNAMIC;
	status = omapi_object_reference(lp, (omapi_object_t *)cp, MDL);
	class_dereference(&cp, MDL);
	return (status);
}

isc_result_t dhcp_class_remove (omapi_object_t *lp,
				omapi_object_t *id)
{
	struct class *cp;
	if (lp -> type != dhcp_type_class)
		return DHCP_R_INVALIDARG;
	cp = (struct class *)lp;

#ifdef DEBUG_OMAPI
	log_debug ("OMAPI delete class %s", cp -> name);
#endif

	delete_class (cp, 1);
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_subclass_set_value  (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_typed_data_t *value)
{
	if (h -> type != dhcp_type_subclass)
		return DHCP_R_INVALIDARG;

	return class_set_value(h, id, name, value);
}


isc_result_t dhcp_subclass_get_value (omapi_object_t *h, omapi_object_t *id,
				      omapi_data_string_t *name,
				      omapi_value_t **value)
{
	struct class *subclass;
	isc_result_t status;

	if (h -> type != dhcp_type_class)
		return DHCP_R_INVALIDARG;
	subclass = (struct class *)h;
	if (subclass -> name != 0)
		return DHCP_R_INVALIDARG;

	/* XXXJAB No values to get yet. */

	/* Try to find some inner object that can provide the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return DHCP_R_UNKNOWNATTRIBUTE;
}

isc_result_t dhcp_subclass_signal_handler (omapi_object_t *h,
					   const char *name, va_list ap)
{
	if (h -> type != dhcp_type_subclass)
		return DHCP_R_INVALIDARG;

	return class_signal_handler(h, name, ap);
}


isc_result_t dhcp_subclass_stuff_values (omapi_object_t *c,
					 omapi_object_t *id,
					 omapi_object_t *h)
{
	struct class *subclass;

	if (h->type != dhcp_type_subclass)
		return (DHCP_R_INVALIDARG);
	subclass = (struct class *)h;
	if (subclass->name != 0)
		return (DHCP_R_INVALIDARG);

	/* add any subclass specific items here */

	return (class_stuff_values(c, id, h));
}

isc_result_t dhcp_subclass_lookup (omapi_object_t **lp,
				   omapi_object_t *id, omapi_object_t *ref)
{
	return class_lookup(lp, id, ref, dhcp_type_subclass);
}




isc_result_t dhcp_subclass_create (omapi_object_t **lp,
				   omapi_object_t *id)
{
	struct class *cp = 0;
	isc_result_t status;

	status = subclass_allocate(&cp, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	group_reference (&cp->group, root_group, MDL);

	cp->flags = CLASS_DECL_DYNAMIC;

	status = omapi_object_reference (lp, (omapi_object_t *)cp, MDL);
	subclass_dereference (&cp, MDL);
	return status;
}

isc_result_t dhcp_subclass_remove (omapi_object_t *lp,
				   omapi_object_t *id)
{
	struct class *cp;
	if (lp -> type != dhcp_type_subclass)
		return DHCP_R_INVALIDARG;
	cp = (struct class *)lp;

#ifdef DEBUG_OMAPI
	log_debug ("OMAPI delete subclass %s", cp -> name);
#endif

	delete_class (cp, 1);

	return ISC_R_SUCCESS;
}

isc_result_t binding_scope_set_value (struct binding_scope *scope, int createp,
				      omapi_data_string_t *name,
				      omapi_typed_data_t *value)
{
	struct binding *bp;
	char *nname;
	struct binding_value *nv;
	nname = dmalloc (name -> len + 1, MDL);
	if (!nname)
		return ISC_R_NOMEMORY;
	memcpy (nname, name -> value, name -> len);
	nname [name -> len] = 0;
	bp = find_binding (scope, nname);
	if (!bp && !createp) {
		dfree (nname, MDL);
		return DHCP_R_UNKNOWNATTRIBUTE;
	}
	if (!value) {
		dfree (nname, MDL);
		if (!bp)
			return DHCP_R_UNKNOWNATTRIBUTE;
		binding_value_dereference (&bp -> value, MDL);
		return ISC_R_SUCCESS;
	}

	nv = (struct binding_value *)0;
	if (!binding_value_allocate (&nv, MDL)) {
		dfree (nname, MDL);
		return ISC_R_NOMEMORY;
	}
	switch (value -> type) {
	      case omapi_datatype_int:
		nv -> type = binding_numeric;
		nv -> value.intval = value -> u.integer;
		break;

	      case omapi_datatype_string:
	      case omapi_datatype_data:
		if (!buffer_allocate (&nv -> value.data.buffer,
				      value -> u.buffer.len, MDL)) {
			binding_value_dereference (&nv, MDL);
			dfree (nname, MDL);
			return ISC_R_NOMEMORY;
		}
		memcpy (&nv -> value.data.buffer -> data [1],
			value -> u.buffer.value, value -> u.buffer.len);
		nv -> value.data.len = value -> u.buffer.len;
		break;

	      case omapi_datatype_object:
		binding_value_dereference (&nv, MDL);
		dfree (nname, MDL);
		return DHCP_R_INVALIDARG;
	}

	if (!bp) {
		bp = dmalloc (sizeof *bp, MDL);
		if (!bp) {
			binding_value_dereference (&nv, MDL);
			dfree (nname, MDL);
			return ISC_R_NOMEMORY;
		}
		memset (bp, 0, sizeof *bp);
		bp -> name = nname;
		bp -> next = scope -> bindings;
		scope -> bindings = bp;
	} else {
		if (bp -> value)
			binding_value_dereference (&bp -> value, MDL);
		dfree (nname, MDL);
	}
	binding_value_reference (&bp -> value, nv, MDL);
	binding_value_dereference (&nv, MDL);
	return ISC_R_SUCCESS;
}

isc_result_t binding_scope_get_value (omapi_value_t **value,
				      struct binding_scope *scope,
				      omapi_data_string_t *name)
{
	struct binding *bp;
	omapi_typed_data_t *td;
	isc_result_t status;
	char *nname;
	nname = dmalloc (name -> len + 1, MDL);
	if (!nname)
		return ISC_R_NOMEMORY;
	memcpy (nname, name -> value, name -> len);
	nname [name -> len] = 0;
	bp = find_binding (scope, nname);
	dfree (nname, MDL);
	if (!bp)
		return DHCP_R_UNKNOWNATTRIBUTE;
	if (!bp -> value)
		return DHCP_R_UNKNOWNATTRIBUTE;

	switch (bp -> value -> type) {
	      case binding_boolean:
		td = (omapi_typed_data_t *)0;
		status = omapi_typed_data_new (MDL, &td, omapi_datatype_int,
					       bp -> value -> value.boolean);
		break;

	      case binding_numeric:
		td = (omapi_typed_data_t *)0;
		status = omapi_typed_data_new (MDL, &td, omapi_datatype_int,
					       (int)
					       bp -> value -> value.intval);
		break;

	      case binding_data:
		td = (omapi_typed_data_t *)0;
		status = omapi_typed_data_new (MDL, &td, omapi_datatype_data,
					       bp -> value -> value.data.len);
		if (status != ISC_R_SUCCESS)
			return status;
		memcpy (&td -> u.buffer.value [0],
			bp -> value -> value.data.data,
			bp -> value -> value.data.len);
		break;

		/* Can't return values for these two (yet?). */
	      case binding_dns:
	      case binding_function:
		return DHCP_R_INVALIDARG;

	      default:
		log_fatal ("Impossible case at %s:%d.", MDL);
		return ISC_R_FAILURE;
	}

	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_value_new (value, MDL);
	if (status != ISC_R_SUCCESS) {
		omapi_typed_data_dereference (&td, MDL);
		return status;
	}

	omapi_data_string_reference (&(*value) -> name, name, MDL);
	omapi_typed_data_reference (&(*value) -> value, td, MDL);
	omapi_typed_data_dereference (&td, MDL);

	return ISC_R_SUCCESS;
}

isc_result_t binding_scope_stuff_values (omapi_object_t *c,
					 struct binding_scope *scope)
{
	struct binding *bp;
	unsigned len;
	isc_result_t status;

	for (bp = scope -> bindings; bp; bp = bp -> next) {
	    if (bp -> value) {
		if (bp -> value -> type == binding_dns ||
		    bp -> value -> type == binding_function)
			continue;

		/* Stuff the name. */
		len = strlen (bp -> name);
		status = omapi_connection_put_uint16 (c, len);
		if (status != ISC_R_SUCCESS)
		    return status;
		status = omapi_connection_copyin (c,
						  (unsigned char *)bp -> name,
						  len);
		if (status != ISC_R_SUCCESS)
			return status;

		switch (bp -> value -> type) {
		  case binding_boolean:
		    status = omapi_connection_put_uint32 (c,
							  sizeof (u_int32_t));
		    if (status != ISC_R_SUCCESS)
			return status;
		    status = (omapi_connection_put_uint32
			      (c,
			       ((u_int32_t)(bp -> value -> value.boolean))));
		    if (status != ISC_R_SUCCESS)
			    return status;
		    break;

		  case binding_data:
		    status = (omapi_connection_put_uint32
			      (c, bp -> value -> value.data.len));
		    if (status != ISC_R_SUCCESS)
			return status;
		    if (bp -> value -> value.data.len) {
			status = (omapi_connection_copyin
				  (c, bp -> value -> value.data.data,
				   bp -> value -> value.data.len));
			if (status != ISC_R_SUCCESS)
			    return status;
		    }
		    break;

		  case binding_numeric:
		    status = (omapi_connection_put_uint32
			      (c, sizeof (u_int32_t)));
		    if (status != ISC_R_SUCCESS)
			    return status;
		    status = (omapi_connection_put_uint32
			      (c, ((u_int32_t)
				   (bp -> value -> value.intval))));
		    if (status != ISC_R_SUCCESS)
			    return status;
		    break;


		    /* NOTREACHED */
		  case binding_dns:
		  case binding_function:
		    break;
		}
	    }
	}
	return ISC_R_SUCCESS;
}

/* vim: set tabstop=8: */
