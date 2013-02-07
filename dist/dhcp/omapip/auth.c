/* auth.c

   Subroutines having to do with authentication. */

/*
 * Copyright (c) 2009-2010 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2007 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1998-2003 by Internet Software Consortium
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

#include "dhcpd.h"

#include <omapip/omapip_p.h>

OMAPI_OBJECT_ALLOC (omapi_auth_key, omapi_auth_key_t, omapi_type_auth_key)
typedef struct hash omapi_auth_hash_t;
HASH_FUNCTIONS_DECL (omapi_auth_key, const char *,
		     omapi_auth_key_t, omapi_auth_hash_t)
omapi_auth_hash_t *auth_key_hash;
HASH_FUNCTIONS (omapi_auth_key, const char *, omapi_auth_key_t,
		omapi_auth_hash_t,
		omapi_auth_key_reference, omapi_auth_key_dereference,
		do_case_hash)

isc_result_t omapi_auth_key_new (omapi_auth_key_t **o, const char *file,
				 int line)
{
	return omapi_auth_key_allocate (o, file, line);
}

isc_result_t omapi_auth_key_destroy (omapi_object_t *h,
				     const char *file, int line)
{
	omapi_auth_key_t *a;

	if (h->type != omapi_type_auth_key)
		return DHCP_R_INVALIDARG;
	a = (omapi_auth_key_t *)h;

	if (auth_key_hash != NULL)
		omapi_auth_key_hash_delete(auth_key_hash, a->name, 0, MDL);

	if (a->name != NULL)
		dfree(a->name, MDL);
	if (a->algorithm != NULL)
		dfree(a->algorithm, MDL);
	if (a->key != NULL)
		omapi_data_string_dereference(&a->key, MDL);
	if (a->tsec_key != NULL)
		dns_tsec_destroy(&a->tsec_key);
	
	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_enter (omapi_auth_key_t *a)
{
	omapi_auth_key_t *tk;
	isc_result_t      status;
	dst_key_t        *dstkey;

	if (a -> type != omapi_type_auth_key)
		return DHCP_R_INVALIDARG;

	tk = (omapi_auth_key_t *)0;
	if (auth_key_hash) {
		omapi_auth_key_hash_lookup (&tk, auth_key_hash,
					    a -> name, 0, MDL);
		if (tk == a) {
			omapi_auth_key_dereference (&tk, MDL);
			return ISC_R_SUCCESS;
		}
		if (tk) {
			omapi_auth_key_hash_delete (auth_key_hash,
						    tk -> name, 0, MDL);
			omapi_auth_key_dereference (&tk, MDL);
		}
	} else {
		if (!omapi_auth_key_new_hash(&auth_key_hash,
					     KEY_HASH_SIZE, MDL))
			return ISC_R_NOMEMORY;
	}

	/*
	 * If possible create a tsec structure for this key,
	 * if we can't create the structure we put out a warning 
	 * and continue.
	 */
	status = isclib_make_dst_key(a->name, a->algorithm,
				     a->key->value, a->key->len,
				     &dstkey);
	if (status == ISC_R_SUCCESS) {
		status = dns_tsec_create(dhcp_gbl_ctx.mctx, dns_tsectype_tsig,
					 dstkey, &a->tsec_key);
		dst_key_free(&dstkey);
	}
	if (status != ISC_R_SUCCESS)
		log_error("Unable to create tsec structure for %s", a->name);

	omapi_auth_key_hash_add (auth_key_hash, a -> name, 0, a, MDL);
	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_lookup_name (omapi_auth_key_t **a,
					 const char *name)
{
	if (!auth_key_hash)
		return ISC_R_NOTFOUND;
	if (!omapi_auth_key_hash_lookup (a, auth_key_hash, name, 0, MDL))
		return ISC_R_NOTFOUND;
	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_lookup (omapi_object_t **h,
				    omapi_object_t *id,
				    omapi_object_t *ref)
{
	isc_result_t status;
	omapi_value_t *name = (omapi_value_t *)0;
	omapi_value_t *algorithm = (omapi_value_t *)0;

	if (!auth_key_hash)
		return ISC_R_NOTFOUND;

	if (!ref)
		return DHCP_R_NOKEYS;

	status = omapi_get_value_str (ref, id, "name", &name);
	if (status != ISC_R_SUCCESS)
		return status;

	if ((name -> value -> type != omapi_datatype_string) &&
	    (name -> value -> type != omapi_datatype_data)) {
		omapi_value_dereference (&name, MDL);
		return ISC_R_NOTFOUND;
	}

	status = omapi_get_value_str (ref, id, "algorithm", &algorithm);
	if (status != ISC_R_SUCCESS) {
		omapi_value_dereference (&name, MDL);
		return status;
	}

	if ((algorithm -> value -> type != omapi_datatype_string) &&
	    (algorithm -> value -> type != omapi_datatype_data)) {
		omapi_value_dereference (&name, MDL);
		omapi_value_dereference (&algorithm, MDL);
		return ISC_R_NOTFOUND;
	}


	if (!omapi_auth_key_hash_lookup ((omapi_auth_key_t **)h, auth_key_hash,
					 (const char *)
					 name -> value -> u.buffer.value,
					 name -> value -> u.buffer.len, MDL)) {
		omapi_value_dereference (&name, MDL);
		omapi_value_dereference (&algorithm, MDL);
		return ISC_R_NOTFOUND;
	}

	if (omapi_td_strcasecmp (algorithm -> value,
				 ((omapi_auth_key_t *)*h) -> algorithm) != 0) {
		omapi_value_dereference (&name, MDL);
		omapi_value_dereference (&algorithm, MDL);
		omapi_object_dereference (h, MDL);
		return ISC_R_NOTFOUND;
	}

	omapi_value_dereference (&name, MDL);
	omapi_value_dereference (&algorithm, MDL);

	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_stuff_values (omapi_object_t *c,
					  omapi_object_t *id,
					  omapi_object_t *h)
{
	omapi_auth_key_t *a;
	isc_result_t status;

	if (h -> type != omapi_type_auth_key)
		return DHCP_R_INVALIDARG;
	a = (omapi_auth_key_t *)h;

	/* Write only the name and algorithm -- not the secret! */
	if (a -> name) {
		status = omapi_connection_put_name (c, "name");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_string (c, a -> name);
		if (status != ISC_R_SUCCESS)
			return status;
	}
	if (a -> algorithm) {
		status = omapi_connection_put_name (c, "algorithm");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_string (c, a -> algorithm);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t omapi_auth_key_get_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_value_t **value)
{
	omapi_auth_key_t *a;
	isc_result_t status;

	if (h -> type != omapi_type_auth_key)
		return ISC_R_UNEXPECTED;
	a = (omapi_auth_key_t *)h;

	if (omapi_ds_strcmp (name, "name") == 0) {
		if (a -> name)
			return omapi_make_string_value
				(value, name, a -> name, MDL);
		else
			return ISC_R_NOTFOUND;
	} else if (omapi_ds_strcmp (name, "key") == 0) {
		if (a -> key) {
			status = omapi_value_new (value, MDL);
			if (status != ISC_R_SUCCESS)
				return status;

			status = omapi_data_string_reference
				(&(*value) -> name, name, MDL);
			if (status != ISC_R_SUCCESS) {
				omapi_value_dereference (value, MDL);
				return status;
			}

			status = omapi_typed_data_new (MDL, &(*value) -> value,
						       omapi_datatype_data,
						       a -> key -> len);
			if (status != ISC_R_SUCCESS) {
				omapi_value_dereference (value, MDL);
				return status;
			}

			memcpy ((*value) -> value -> u.buffer.value,
				a -> key -> value, a -> key -> len);
			return ISC_R_SUCCESS;
		} else
			return ISC_R_NOTFOUND;
	} else if (omapi_ds_strcmp (name, "algorithm") == 0) {
		if (a -> algorithm)
			return omapi_make_string_value
				(value, name, a -> algorithm, MDL);
		else
			return ISC_R_NOTFOUND;
	}

	return ISC_R_SUCCESS;
}
