/*	$NetBSD: uvm_pdpolicy_impl.h,v 1.2 2006/09/15 15:51:13 yamt Exp $	*/

/*-
 * Copyright (c)2006 YAMAMOTO Takashi,
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

#ifndef _UVM_PDPOLICY_IMPL_H_
#define _UVM_PDPOLICY_IMPL_H_

/*
 * definitions for page replacement policy implementations.
 */

#include <sys/evcnt.h>

#define	PDPOL_EVCNT_DEFINE(name) \
struct evcnt pdpol_evcnt_##name = EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, \
    "pdpolicy", #name); \
EVCNT_ATTACH_STATIC(pdpol_evcnt_##name);
#define	PDPOL_EVCNT_INCR(ev)		pdpol_evcnt_##ev.ev_count++

#endif /* !_UVM_PDPOLICY_IMPL_H_ */
