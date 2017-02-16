/*	$NetBSD: comapi.c,v 1.1.1.3 2014/07/12 11:57:39 spz Exp $	*/
/* omapi.c

   OMAPI object interfaces for the DHCP server. */

/*
 * Copyright (c) 2012,2014 Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004-2007,2009 by Internet Systems Consortium, Inc. ("ISC")
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
__RCSID("$NetBSD: comapi.c,v 1.1.1.3 2014/07/12 11:57:39 spz Exp $");

/* Many, many thanks to Brian Murrell and BCtel for this code - BCtel
   provided the funding that resulted in this code and the entire
   OMAPI support library being written, and Brian helped brainstorm
   and refine the requirements.  To the extent that this code is
   useful, you have Brian and BCtel to thank.  Any limitations in the
   code are a result of mistakes on my part.  -- Ted Lemon */

#include "dhcpd.h"
#include <omapip/omapip_p.h>

OMAPI_OBJECT_ALLOC (subnet, struct subnet, dhcp_type_subnet)
OMAPI_OBJECT_ALLOC (shared_network, struct shared_network,
		    dhcp_type_shared_network)
OMAPI_OBJECT_ALLOC (group_object, struct group_object, dhcp_type_group)
OMAPI_OBJECT_ALLOC (dhcp_control, dhcp_control_object_t, dhcp_type_control)

omapi_object_type_t *dhcp_type_interface;
omapi_object_type_t *dhcp_type_group;
omapi_object_type_t *dhcp_type_shared_network;
omapi_object_type_t *dhcp_type_subnet;
omapi_object_type_t *dhcp_type_control;
dhcp_control_object_t *dhcp_control_object;

void dhcp_common_objects_setup ()
{
	isc_result_t status;

	status = omapi_object_type_register (&dhcp_type_control,
					     "control",
					     dhcp_control_set_value,
					     dhcp_control_get_value,
					     dhcp_control_destroy,
					     dhcp_control_signal_handler,
					     dhcp_control_stuff_values,
					     dhcp_control_lookup, 
					     dhcp_control_create,
					     dhcp_control_remove, 0, 0, 0,
					     sizeof (dhcp_control_object_t),
					     0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register control object type: %s",
			   isc_result_totext (status));
	status = dhcp_control_allocate (&dhcp_control_object, MDL);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't make initial control object: %s",
			   isc_result_totext (status));
	dhcp_control_object -> state = server_startup;

	status = omapi_object_type_register (&dhcp_type_group,
					     "group",
					     dhcp_group_set_value,
					     dhcp_group_get_value,
					     dhcp_group_destroy,
					     dhcp_group_signal_handler,
					     dhcp_group_stuff_values,
					     dhcp_group_lookup, 
					     dhcp_group_create,
					     dhcp_group_remove, 0, 0, 0,
					     sizeof (struct group_object), 0,
					     RC_MISC);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register group object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register (&dhcp_type_subnet,
					     "subnet",
					     dhcp_subnet_set_value,
					     dhcp_subnet_get_value,
					     dhcp_subnet_destroy,
					     dhcp_subnet_signal_handler,
					     dhcp_subnet_stuff_values,
					     dhcp_subnet_lookup, 
					     dhcp_subnet_create,
					     dhcp_subnet_remove, 0, 0, 0,
					     sizeof (struct subnet), 0,
					     RC_MISC);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register subnet object type: %s",
			   isc_result_totext (status));

	status = omapi_object_type_register
		(&dhcp_type_shared_network,
		 "shared-network",
		 dhcp_shared_network_set_value,
		 dhcp_shared_network_get_value,
		 dhcp_shared_network_destroy,
		 dhcp_shared_network_signal_handler,
		 dhcp_shared_network_stuff_values,
		 dhcp_shared_network_lookup, 
		 dhcp_shared_network_create,
		 dhcp_shared_network_remove, 0, 0, 0,
		 sizeof (struct shared_network), 0, RC_MISC);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register shared network object type: %s",
			   isc_result_totext (status));

	interface_setup ();
}

isc_result_t dhcp_group_set_value  (omapi_object_t *h,
				    omapi_object_t *id,
				    omapi_data_string_t *name,
				    omapi_typed_data_t *value)
{
	struct group_object *group;
	isc_result_t status;

	if (h -> type != dhcp_type_group)
		return DHCP_R_INVALIDARG;
	group = (struct group_object *)h;

	/* XXX For now, we can only set these values on new group objects. 
	   XXX Soon, we need to be able to update group objects. */
	if (!omapi_ds_strcmp (name, "name")) {
		if (group -> name)
			return ISC_R_EXISTS;
		if (value -> type == omapi_datatype_data ||
		    value -> type == omapi_datatype_string) {
			group -> name = dmalloc (value -> u.buffer.len + 1,
						 MDL);
			if (!group -> name)
				return ISC_R_NOMEMORY;
			memcpy (group -> name,
				value -> u.buffer.value,
				value -> u.buffer.len);
			group -> name [value -> u.buffer.len] = 0;
		} else
			return DHCP_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	if (!omapi_ds_strcmp (name, "statements")) {
		if (group -> group && group -> group -> statements)
			return ISC_R_EXISTS;
		if (!group -> group) {
			if (!clone_group (&group -> group, root_group, MDL))
				return ISC_R_NOMEMORY;
		}
		if (value -> type == omapi_datatype_data ||
		    value -> type == omapi_datatype_string) {
			struct parse *parse;
			int lose = 0;
			parse = NULL;
			status = new_parse(&parse, -1,
					    (char *) value->u.buffer.value,
					    value->u.buffer.len,
					    "network client", 0);
			if (status != ISC_R_SUCCESS || parse == NULL)
				return status;
			if (!(parse_executable_statements
			      (&group -> group -> statements, parse, &lose,
			       context_any))) {
				end_parse (&parse);
				return DHCP_R_BADPARSE;
			}
			end_parse (&parse);
			return ISC_R_SUCCESS;
		} else
			return DHCP_R_INVALIDARG;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}
			  
	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_group_get_value (omapi_object_t *h, omapi_object_t *id,
				   omapi_data_string_t *name,
				   omapi_value_t **value)
{
	struct group_object *group;
	isc_result_t status;

	if (h -> type != dhcp_type_group)
		return DHCP_R_INVALIDARG;
	group = (struct group_object *)h;

	if (!omapi_ds_strcmp (name, "name"))
		return omapi_make_string_value (value,
						name, group -> name, MDL);

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_group_destroy (omapi_object_t *h, const char *file, int line)
{
	struct group_object *group, *t;

	if (h -> type != dhcp_type_group)
		return DHCP_R_INVALIDARG;
	group = (struct group_object *)h;

	if (group -> name) {
		if (group_name_hash) {
			t = (struct group_object *)0;
			if (group_hash_lookup (&t, group_name_hash,
					       group -> name,
					       strlen (group -> name), MDL)) {
				group_hash_delete (group_name_hash,
						   group -> name,
						   strlen (group -> name),
						   MDL);
				group_object_dereference (&t, MDL);
			}
		}
		dfree (group -> name, file, line);
		group -> name = (char *)0;
	}
	if (group -> group)
		group_dereference (&group -> group, MDL);

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_group_signal_handler (omapi_object_t *h,
					const char *name, va_list ap)
{
	struct group_object *group;
	isc_result_t status;
	int updatep = 0;

	if (h -> type != dhcp_type_group)
		return DHCP_R_INVALIDARG;
	group = (struct group_object *)h;

	if (!strcmp (name, "updated")) {
		/* A group object isn't valid if a subgroup hasn't yet been
		   associated with it. */
		if (!group -> group)
			return DHCP_R_INVALIDARG;

		/* Group objects always have to have names. */
		if (!group -> name) {
			char hnbuf [64];
			sprintf (hnbuf, "ng%08lx%08lx",
				 (unsigned long)cur_time,
				 (unsigned long)group);
			group -> name = dmalloc (strlen (hnbuf) + 1, MDL);
			if (!group -> name)
				return ISC_R_NOMEMORY;
			strcpy (group -> name, hnbuf);
		}

		supersede_group (group, 1);
		updatep = 1;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	if (updatep)
		return ISC_R_SUCCESS;
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_group_stuff_values (omapi_object_t *c,
				      omapi_object_t *id,
				      omapi_object_t *h)
{
	struct group_object *group;
	isc_result_t status;

	if (h -> type != dhcp_type_group)
		return DHCP_R_INVALIDARG;
	group = (struct group_object *)h;

	/* Write out all the values. */
	if (group -> name) {
		status = omapi_connection_put_name (c, "name");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_string (c, group -> name);
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

isc_result_t dhcp_group_lookup (omapi_object_t **lp,
				omapi_object_t *id, omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	struct group_object *group;

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
		if ((*lp) -> type != dhcp_type_group) {
			omapi_object_dereference (lp, MDL);
			return DHCP_R_INVALIDARG;
		}
	}

	/* Now look for a name. */
	status = omapi_get_value_str (ref, id, "name", &tv);
	if (status == ISC_R_SUCCESS) {
		group = (struct group_object *)0;
		if (group_name_hash &&
		    group_hash_lookup (&group, group_name_hash,
				       (const char *)
				       tv -> value -> u.buffer.value,
				       tv -> value -> u.buffer.len, MDL)) {
			omapi_value_dereference (&tv, MDL);

			if (*lp && *lp != (omapi_object_t *)group) {
			    group_object_dereference (&group, MDL);
			    omapi_object_dereference (lp, MDL);
			    return DHCP_R_KEYCONFLICT;
			} else if (!*lp) {
			    /* XXX fix so that hash lookup itself creates
			       XXX the reference. */
			    omapi_object_reference (lp,
						    (omapi_object_t *)group,
						    MDL);
			    group_object_dereference (&group, MDL);
			}
		} else if (!*lp)
			return ISC_R_NOTFOUND;
	}

	/* If we get to here without finding a group, no valid key was
	   specified. */
	if (!*lp)
		return DHCP_R_NOKEYS;

	if (((struct group_object *)(*lp)) -> flags & GROUP_OBJECT_DELETED) {
		omapi_object_dereference (lp, MDL);
		return ISC_R_NOTFOUND;
	}
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_group_create (omapi_object_t **lp,
			       omapi_object_t *id)
{
	struct group_object *group;
	isc_result_t status;
	group = (struct group_object *)0;

	status = group_object_allocate (&group, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	group -> flags = GROUP_OBJECT_DYNAMIC;
	status = omapi_object_reference (lp, (omapi_object_t *)group, MDL);
	group_object_dereference (&group, MDL);
	return status;
}

isc_result_t dhcp_group_remove (omapi_object_t *lp,
				omapi_object_t *id)
{
	struct group_object *group;
	isc_result_t status;
	if (lp -> type != dhcp_type_group)
		return DHCP_R_INVALIDARG;
	group = (struct group_object *)lp;

	group -> flags |= GROUP_OBJECT_DELETED;
	if (group_write_hook) {
		if (!(*group_write_hook) (group))
			return ISC_R_IOERROR;
	}

	status = dhcp_group_destroy ((omapi_object_t *)group, MDL);

	return status;
}

isc_result_t dhcp_control_set_value  (omapi_object_t *h,
				      omapi_object_t *id,
				      omapi_data_string_t *name,
				      omapi_typed_data_t *value)
{
	dhcp_control_object_t *control;
	isc_result_t status;
	unsigned long newstate;

	if (h -> type != dhcp_type_control)
		return DHCP_R_INVALIDARG;
	control = (dhcp_control_object_t *)h;

	if (!omapi_ds_strcmp (name, "state")) {
		status = omapi_get_int_value (&newstate, value);
		if (status != ISC_R_SUCCESS)
			return status;
		status = dhcp_set_control_state (control -> state, newstate);
		if (status == ISC_R_SUCCESS)
			control -> state = value -> u.integer;
		return status;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}
			  
	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_control_get_value (omapi_object_t *h, omapi_object_t *id,
				   omapi_data_string_t *name,
				   omapi_value_t **value)
{
	dhcp_control_object_t *control;
	isc_result_t status;

	if (h -> type != dhcp_type_control)
		return DHCP_R_INVALIDARG;
	control = (dhcp_control_object_t *)h;

	if (!omapi_ds_strcmp (name, "state"))
		return omapi_make_int_value (value,
					     name, (int)control -> state, MDL);

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_control_destroy (omapi_object_t *h,
				   const char *file, int line)
{
	if (h -> type != dhcp_type_control)
		return DHCP_R_INVALIDARG;

	/* Can't destroy the control object. */
	return ISC_R_NOPERM;
}

isc_result_t dhcp_control_signal_handler (omapi_object_t *h,
					const char *name, va_list ap)
{
	/* In this function h should be a (dhcp_control_object_t *) */

	isc_result_t status;

	if (h -> type != dhcp_type_control)
		return DHCP_R_INVALIDARG;

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_control_stuff_values (omapi_object_t *c,
					omapi_object_t *id,
					omapi_object_t *h)
{
	dhcp_control_object_t *control;
	isc_result_t status;

	if (h -> type != dhcp_type_control)
		return DHCP_R_INVALIDARG;
	control = (dhcp_control_object_t *)h;

	/* Write out all the values. */
	status = omapi_connection_put_name (c, "state");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, control -> state);
	if (status != ISC_R_SUCCESS)
		return status;

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_control_lookup (omapi_object_t **lp,
				  omapi_object_t *id, omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;

	/* First see if we were sent a handle. */
	if (ref) {
		status = omapi_get_value_str (ref, id, "handle", &tv);
		if (status == ISC_R_SUCCESS) {
			status = omapi_handle_td_lookup (lp, tv -> value);

			omapi_value_dereference (&tv, MDL);
			if (status != ISC_R_SUCCESS)
				return status;

			/* Don't return the object if the type is wrong. */
			if ((*lp) -> type != dhcp_type_control) {
				omapi_object_dereference (lp, MDL);
				return DHCP_R_INVALIDARG;
			}
		}
	}

	/* Otherwise, stop playing coy - there's only one control object,
	   so we can just return it. */
	dhcp_control_reference ((dhcp_control_object_t **)lp,
				dhcp_control_object, MDL);
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_control_create (omapi_object_t **lp,
				  omapi_object_t *id)
{
	/* Can't create a control object - there can be only one. */
	return ISC_R_NOPERM;
}

isc_result_t dhcp_control_remove (omapi_object_t *lp,
				omapi_object_t *id)
{
	/* Form is emptiness; emptiness form.   The control object
	   cannot go out of existance. */
	return ISC_R_NOPERM;
}

isc_result_t dhcp_subnet_set_value  (omapi_object_t *h,
				     omapi_object_t *id,
				     omapi_data_string_t *name,
				     omapi_typed_data_t *value)
{
	/* In this function h should be a (struct subnet *) */

	isc_result_t status;

	if (h -> type != dhcp_type_subnet)
		return DHCP_R_INVALIDARG;

	/* No values to set yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}

	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_subnet_get_value (omapi_object_t *h, omapi_object_t *id,
				    omapi_data_string_t *name,
				    omapi_value_t **value)
{
	/* In this function h should be a (struct subnet *) */

	isc_result_t status;

	if (h -> type != dhcp_type_subnet)
		return DHCP_R_INVALIDARG;

	/* No values to get yet. */

	/* Try to find some inner object that can provide the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_subnet_destroy (omapi_object_t *h, const char *file, int line)
{
#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	struct subnet *subnet;
#endif

	if (h -> type != dhcp_type_subnet)
		return DHCP_R_INVALIDARG;

#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	subnet = (struct subnet *)h;
	if (subnet -> next_subnet)
		subnet_dereference (&subnet -> next_subnet, file, line);
	if (subnet -> next_sibling)
		subnet_dereference (&subnet -> next_sibling, file, line);
	if (subnet -> shared_network)
		shared_network_dereference (&subnet -> shared_network,
					    file, line);
	if (subnet -> interface)
		interface_dereference (&subnet -> interface, file, line);
	if (subnet -> group)
		group_dereference (&subnet -> group, file, line);
#endif

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_subnet_signal_handler (omapi_object_t *h,
					 const char *name, va_list ap)
{
	/* In this function h should be a (struct subnet *) */

	isc_result_t status;

	if (h -> type != dhcp_type_subnet)
		return DHCP_R_INVALIDARG;

	/* Can't write subnets yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_subnet_stuff_values (omapi_object_t *c,
				       omapi_object_t *id,
				       omapi_object_t *h)
{
	/* In this function h should be a (struct subnet *) */

	isc_result_t status;

	if (h -> type != dhcp_type_subnet)
		return DHCP_R_INVALIDARG;

	/* Can't stuff subnet values yet. */

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_subnet_lookup (omapi_object_t **lp,
				 omapi_object_t *id,
				 omapi_object_t *ref)
{
	/* Can't look up subnets yet. */

	/* If we get to here without finding a subnet, no valid key was
	   specified. */
	if (!*lp)
		return DHCP_R_NOKEYS;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_subnet_create (omapi_object_t **lp,
				 omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_subnet_remove (omapi_object_t *lp,
			       omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_shared_network_set_value  (omapi_object_t *h,
					     omapi_object_t *id,
					     omapi_data_string_t *name,
					     omapi_typed_data_t *value)
{
	/* In this function h should be a (struct shared_network *) */

	isc_result_t status;

	if (h -> type != dhcp_type_shared_network)
		return DHCP_R_INVALIDARG;

	/* No values to set yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}

	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_shared_network_get_value (omapi_object_t *h,
					    omapi_object_t *id,
					    omapi_data_string_t *name,
					    omapi_value_t **value)
{
	/* In this function h should be a (struct shared_network *) */

	isc_result_t status;

	if (h -> type != dhcp_type_shared_network)
		return DHCP_R_INVALIDARG;

	/* No values to get yet. */

	/* Try to find some inner object that can provide the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> get_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_shared_network_destroy (omapi_object_t *h,
					  const char *file, int line)
{
	/* In this function h should be a (struct shared_network *) */

#if defined (DEBUG_MEMORY_LEAKAGE) || \
    defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	struct shared_network *shared_network;
#endif

	if (h -> type != dhcp_type_shared_network)
		return DHCP_R_INVALIDARG;

#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	shared_network = (struct shared_network *)h;
	if (shared_network -> next)
		shared_network_dereference (&shared_network -> next,
					    file, line);
	if (shared_network -> name) {
		dfree (shared_network -> name, file, line);
		shared_network -> name = 0;
	}
	if (shared_network -> subnets)
		subnet_dereference (&shared_network -> subnets, file, line);
	if (shared_network -> interface)
		interface_dereference (&shared_network -> interface,
				       file, line);
	if (shared_network -> pools)
	    omapi_object_dereference ((omapi_object_t **)
				      &shared_network -> pools, file, line);
	if (shared_network -> group)
		group_dereference (&shared_network -> group, file, line);
#if defined (FAILOVER_PROTOCOL)
	if (shared_network -> failover_peer)
	    omapi_object_dereference ((omapi_object_t **)
				      &shared_network -> failover_peer,
				      file, line);
#endif
#endif /* DEBUG_MEMORY_LEAKAGE */

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_shared_network_signal_handler (omapi_object_t *h,
						 const char *name,
						 va_list ap)
{
	/* In this function h should be a (struct shared_network *) */

	isc_result_t status;

	if (h -> type != dhcp_type_shared_network)
		return DHCP_R_INVALIDARG;

	/* Can't write shared_networks yet. */

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> get_value) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_shared_network_stuff_values (omapi_object_t *c,
					       omapi_object_t *id,
					       omapi_object_t *h)
{
	/* In this function h should be a (struct shared_network *) */

	isc_result_t status;

	if (h -> type != dhcp_type_shared_network)
		return DHCP_R_INVALIDARG;

	/* Can't stuff shared_network values yet. */

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_shared_network_lookup (omapi_object_t **lp,
					 omapi_object_t *id,
					 omapi_object_t *ref)
{
	/* Can't look up shared_networks yet. */

	/* If we get to here without finding a shared_network, no valid key was
	   specified. */
	if (!*lp)
		return DHCP_R_NOKEYS;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_shared_network_create (omapi_object_t **lp,
					 omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_shared_network_remove (omapi_object_t *lp,
					 omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

