/*
 * Copyright (C) 2004-2007, 2010, 2011  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: aclconf.h,v 1.12.72.2 2011-06-17 23:47:12 tbox Exp $ */

#ifndef ISCCFG_ACLCONF_H
#define ISCCFG_ACLCONF_H 1

#include <isc/lang.h>

#include <isccfg/cfg.h>

#include <dns/types.h>

typedef struct cfg_aclconfctx {
	ISC_LIST(dns_acl_t) named_acl_cache;
	isc_mem_t *mctx;
	isc_refcount_t references;
} cfg_aclconfctx_t;

/***
 *** Functions
 ***/

ISC_LANG_BEGINDECLS

isc_result_t
cfg_aclconfctx_create(isc_mem_t *mctx, cfg_aclconfctx_t **ret);
/*
 * Creates and initializes an ACL configuration context.
 */

void
cfg_aclconfctx_detach(cfg_aclconfctx_t **actxp);
/*
 * Removes a reference to an ACL configuration context; when references
 * reaches zero, clears the contents and deallocate the structure.
 */

void
cfg_aclconfctx_attach(cfg_aclconfctx_t *src, cfg_aclconfctx_t **dest);
/*
 * Attaches a pointer to an existing ACL configuration context.
 */

isc_result_t
cfg_acl_fromconfig(const cfg_obj_t *caml,
		   const cfg_obj_t *cctx,
		   isc_log_t *lctx,
		   cfg_aclconfctx_t *ctx,
		   isc_mem_t *mctx,
		   unsigned int nest_level,
		   dns_acl_t **target);
/*
 * Construct a new dns_acl_t from configuration data in 'caml' and
 * 'cctx'.  Memory is allocated through 'mctx'.
 *
 * Any named ACLs referred to within 'caml' will be be converted
 * into nested dns_acl_t objects.  Multiple references to the same
 * named ACLs will be converted into shared references to a single
 * nested dns_acl_t object when the referring objects were created
 * passing the same ACL configuration context 'ctx'.
 *
 * On success, attach '*target' to the new dns_acl_t object.
 */

ISC_LANG_ENDDECLS

#endif /* ISCCFG_ACLCONF_H */
