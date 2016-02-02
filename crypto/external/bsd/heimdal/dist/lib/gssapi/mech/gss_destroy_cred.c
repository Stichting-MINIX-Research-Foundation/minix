/*	$NetBSD: gss_destroy_cred.c,v 1.1.1.2 2014/04/24 12:45:29 pettai Exp $	*/

/*-
 * Copyright (c) 2005 Doug Rabson
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "mech_locl.h"
#include <heim_threads.h>

OM_uint32 GSSAPI_LIB_FUNCTION
gss_destroy_cred(void *status,
		 gss_cred_id_t *cred_handle)
{
    struct _gss_cred *cred;
    struct _gss_mechanism_cred *mc;
    OM_uint32 junk;

    if (cred_handle == NULL)
	return GSS_S_CALL_INACCESSIBLE_READ;
    if (*cred_handle == GSS_C_NO_CREDENTIAL)
	return GSS_S_COMPLETE;

    cred = (struct _gss_cred *)*cred_handle;

    while (HEIM_SLIST_FIRST(&cred->gc_mc)) {
	mc = HEIM_SLIST_FIRST(&cred->gc_mc);
	HEIM_SLIST_REMOVE_HEAD(&cred->gc_mc, gmc_link);
	if (mc->gmc_mech->gm_destroy_cred)
	    mc->gmc_mech->gm_destroy_cred(&junk, &mc->gmc_cred);
	else
	    mc->gmc_mech->gm_release_cred(&junk, &mc->gmc_cred);
	free(mc);
    }
    free(cred);

    return GSS_S_COMPLETE;
}
