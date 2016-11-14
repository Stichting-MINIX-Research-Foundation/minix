/*	$NetBSD: subr_lwp_specificdata.c,v 1.3 2013/10/25 16:17:35 martin Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define _LWP_API_PRIVATE

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_lwp_specificdata.c,v 1.3 2013/10/25 16:17:35 martin Exp $");

#include <sys/param.h>
#include <sys/lwp.h>
#include <sys/specificdata.h>

static specificdata_domain_t lwp_specificdata_domain;

void
lwpinit_specificdata(void)
{

	lwp_specificdata_domain = specificdata_domain_create();
	KASSERT(lwp_specificdata_domain != NULL);
}

/*
 * lwp_specific_key_create --
 *	Create a key for subsystem lwp-specific data.
 */
int
lwp_specific_key_create(specificdata_key_t *keyp, specificdata_dtor_t dtor)
{

	return (specificdata_key_create(lwp_specificdata_domain, keyp, dtor));
}

/*
 * lwp_specific_key_delete --
 *	Delete a key for subsystem lwp-specific data.
 */
void
lwp_specific_key_delete(specificdata_key_t key)
{

	specificdata_key_delete(lwp_specificdata_domain, key);
}

/*
 * lwp_initspecific --
 *	Initialize an LWP's specificdata container.
 */
void
lwp_initspecific(struct lwp *l)
{
	int error __diagused;

	error = specificdata_init(lwp_specificdata_domain, &l->l_specdataref);
	KASSERT(error == 0);
}

/*
 * lwp_finispecific --
 *	Finalize an LWP's specificdata container.
 */
void
lwp_finispecific(struct lwp *l)
{

	specificdata_fini(lwp_specificdata_domain, &l->l_specdataref);
}

/*
 * lwp_getspecific --
 *	Return lwp-specific data corresponding to the specified key.
 *
 *	Note: LWP specific data is NOT INTERLOCKED.  An LWP should access
 *	only its OWN SPECIFIC DATA.  If it is necessary to access another
 *	LWP's specifc data, care must be taken to ensure that doing so
 *	would not cause internal data structure inconsistency (i.e. caller
 *	can guarantee that the target LWP is not inside an lwp_getspecific()
 *	or lwp_setspecific() call).
 */
void *
lwp_getspecific(specificdata_key_t key)
{

	return (specificdata_getspecific_unlocked(lwp_specificdata_domain,
						  &curlwp->l_specdataref, key));
}

void *
_lwp_getspecific_by_lwp(struct lwp *l, specificdata_key_t key)
{

	return (specificdata_getspecific_unlocked(lwp_specificdata_domain,
						  &l->l_specdataref, key));
}

/*
 * lwp_setspecific --
 *	Set lwp-specific data corresponding to the specified key.
 */
void
lwp_setspecific(specificdata_key_t key, void *data)
{

	specificdata_setspecific(lwp_specificdata_domain,
				 &curlwp->l_specdataref, key, data);
}
