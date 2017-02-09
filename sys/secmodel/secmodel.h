/* $NetBSD: secmodel.h,v 1.4 2011/12/04 19:24:59 jym Exp $ */
/*-
 * Copyright (c) 2006, 2011 Elad Efrat <elad@NetBSD.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SECMODEL_SECMODEL_H_
#define	_SECMODEL_SECMODEL_H_

#include <prop/proplib.h>

void secmodel_init(void);

/*
 * Functions used for inter-secmodel communication, allowing evaluation
 * or setting information.
 */
typedef int (*secmodel_eval_t)(const char *, void *, void *);
typedef int (*secmodel_setinfo_t)(void *); /* XXX TODO */

/*
 * Secmodel entry.
 */
struct secmodel_descr {
	LIST_ENTRY(secmodel_descr) sm_list;
	const char *sm_id;
	const char *sm_name;
	prop_dictionary_t sm_behavior;
	secmodel_eval_t sm_eval;
	secmodel_setinfo_t sm_setinfo;
};
typedef struct secmodel_descr *secmodel_t;

int secmodel_register(secmodel_t *, const char *, const char *,
    prop_dictionary_t, secmodel_eval_t, secmodel_setinfo_t);
int secmodel_deregister(secmodel_t);
int secmodel_nsecmodels(void);

int secmodel_eval(const char *, const char *, void *, void *);
int secmodel_setinfo(const char *, void *, int *); /* XXX TODO */
#endif /* !_SECMODEL_SECMODEL_H_ */
