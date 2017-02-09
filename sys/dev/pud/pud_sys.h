/*	$NetBSD: pud_sys.h,v 1.1 2007/11/20 18:47:06 pooka Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Research Foundation of Helsinki University of Technology
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _DEV_PUD_PUDSYS_H_
#define _DEV_PUD_PUDSYS_H_

#include <dev/putter/putter_sys.h>

#include <dev/pud/pud_msgif.h>

struct pud_touser {
	struct pud_req		*pt_pdr; 

	kcondvar_t		pt_cv;
	TAILQ_ENTRY(pud_touser)	pt_entries; 
}; 

struct pud_dev {
	dev_t			pd_dev;
	struct putter_instance	*pd_pi;

	kmutex_t		pd_mtx;

	TAILQ_HEAD(,pud_touser)	pd_waitq_req;
	TAILQ_HEAD(,pud_touser)	pd_waitq_resp;
	kcondvar_t		pd_waitq_req_cv;
	size_t			pd_waitcount;

	kcondvar_t		pd_draincv;

	LIST_ENTRY(pud_dev)	pd_entries;

	uint64_t		pd_nextreq;
};

int		pud_config(int, int, int);
int		pud_request(dev_t, void *, size_t, int, int);

struct pud_dev	*pud_dev2pud(dev_t);

#endif /* _DEV_PUD_PUDSYS_H_ */
