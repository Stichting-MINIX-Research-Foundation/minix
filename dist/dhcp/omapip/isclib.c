/*
 * Copyright(c) 2009-2010 by Internet Systems Consortium, Inc.("ISC")
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
 *   http://www.isc.org/
 *
 */

/*Trying to figure out what we need to define to get things to work.
  It looks like we want/need the export library but need the fdwatchcommand
  which may be a problem */

#include "dhcpd.h"

#include <sys/time.h>

dhcp_context_t dhcp_gbl_ctx;

void
isclib_cleanup(void)
{
#if defined (NSUPDATE)
	if (dhcp_gbl_ctx.dnsclient != NULL)
		dns_client_destroy((dns_client_t **)&dhcp_gbl_ctx.dnsclient);
#endif

	if (dhcp_gbl_ctx.task != NULL) {
		isc_task_shutdown(dhcp_gbl_ctx.task);
		isc_task_detach(&dhcp_gbl_ctx.task);
	}

	if (dhcp_gbl_ctx.timermgr != NULL)
		isc_timermgr_destroy(&dhcp_gbl_ctx.timermgr);

	if (dhcp_gbl_ctx.socketmgr != NULL)
		isc_socketmgr_destroy(&dhcp_gbl_ctx.socketmgr);

	if (dhcp_gbl_ctx.taskmgr != NULL)
		isc_taskmgr_destroy(&dhcp_gbl_ctx.taskmgr);

	if (dhcp_gbl_ctx.actx_started != ISC_FALSE) {
		isc_app_ctxfinish(dhcp_gbl_ctx.actx);
		dhcp_gbl_ctx.actx_started = ISC_FALSE;
	}

	if (dhcp_gbl_ctx.actx != NULL)
		isc_appctx_destroy(&dhcp_gbl_ctx.actx);

	if (dhcp_gbl_ctx.mctx != NULL)
		isc_mem_detach(&dhcp_gbl_ctx.mctx);

	return;
}

isc_result_t
dhcp_context_create(void) {
	isc_result_t result;

	/*
	 * Set up the error messages, this isn't the right place
	 * for this call but it is convienent for now.
	 */
	result = dhcp_result_register();
	if (result != ISC_R_SUCCESS) {
		log_fatal("register_table() %s: %u", "failed", result);
	}

	memset(&dhcp_gbl_ctx, 0, sizeof (dhcp_gbl_ctx));
	
	isc_lib_register();

	/* get the current time for use as the random seed */
	gettimeofday(&cur_tv, (struct timezone *)0);
	isc_random_seed(cur_tv.tv_sec);

#if defined (NSUPDATE)
	result = dns_lib_init();
	if (result != ISC_R_SUCCESS)
		goto cleanup;
#endif

	result = isc_mem_create(0, 0, &dhcp_gbl_ctx.mctx);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = isc_appctx_create(dhcp_gbl_ctx.mctx, &dhcp_gbl_ctx.actx);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = isc_app_ctxstart(dhcp_gbl_ctx.actx);
	if (result != ISC_R_SUCCESS)
		return (result);
	dhcp_gbl_ctx.actx_started = ISC_TRUE;

	result = isc_taskmgr_createinctx(dhcp_gbl_ctx.mctx,
					 dhcp_gbl_ctx.actx,
					 1, 0,
					 &dhcp_gbl_ctx.taskmgr);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = isc_socketmgr_createinctx(dhcp_gbl_ctx.mctx,
					   dhcp_gbl_ctx.actx,
					   &dhcp_gbl_ctx.socketmgr);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = isc_timermgr_createinctx(dhcp_gbl_ctx.mctx,
					  dhcp_gbl_ctx.actx,
					  &dhcp_gbl_ctx.timermgr);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = isc_task_create(dhcp_gbl_ctx.taskmgr, 0, &dhcp_gbl_ctx.task);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

#if defined (NSUPDATE)
	result = dns_client_createx(dhcp_gbl_ctx.mctx,
				    dhcp_gbl_ctx.actx,
				    dhcp_gbl_ctx.taskmgr,
				    dhcp_gbl_ctx.socketmgr,
				    dhcp_gbl_ctx.timermgr,
				    0,
				    &dhcp_gbl_ctx.dnsclient);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
#else
	/* The dst library is inited as part of dns_lib_init, we don't
	 * need it if NSUPDATE is enabled */
	result = dst_lib_init(dhcp_gbl_ctx.mctx, NULL, 0);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

#endif
	return(ISC_R_SUCCESS);

 cleanup:
	/*
	 * Currently we don't try and cleanup, just return an error
	 * expecting that our caller will log the error and exit.
	 */

	return(result);
}

/*
 * Convert a string name into the proper structure for the isc routines
 *
 * Previously we allowed names without a trailing '.' however the current
 * dns and dst code requires the names to end in a period.  If the
 * name doesn't have a trailing period add one as part of creating
 * the dns name.
 */

isc_result_t
dhcp_isc_name(unsigned char   *namestr,
	      dns_fixedname_t *namefix,
	      dns_name_t     **name)
{
	size_t namelen;
	isc_buffer_t b;
	isc_result_t result;

	namelen = strlen((char *)namestr); 
	isc_buffer_init(&b, namestr, namelen);
	isc_buffer_add(&b, namelen);
	dns_fixedname_init(namefix);
	*name = dns_fixedname_name(namefix);
	result = dns_name_fromtext(*name, &b, dns_rootname, 0, NULL);
	isc_buffer_invalidate(&b);
	return(result);
}

isc_result_t
isclib_make_dst_key(char          *inname,
		    char          *algorithm,
		    unsigned char *secret,
		    int            length,
		    dst_key_t    **dstkey)
{
	isc_result_t result;
	dns_name_t *name;
	dns_fixedname_t name0;
	isc_buffer_t b;

	isc_buffer_init(&b, secret, length);
	isc_buffer_add(&b, length);

	/* We only support HMAC_MD5 currently */
	if (strcasecmp(algorithm, DHCP_HMAC_MD5_NAME) != 0) {
		return(DHCP_R_INVALIDARG);
	}

	result = dhcp_isc_name((unsigned char *)inname, &name0, &name);
	if (result != ISC_R_SUCCESS) {
		return(result);
	}

	return(dst_key_frombuffer(name, DST_ALG_HMACMD5, DNS_KEYOWNER_ENTITY,
				  DNS_KEYPROTO_DNSSEC, dns_rdataclass_in,
				  &b, dhcp_gbl_ctx.mctx, dstkey));
}

