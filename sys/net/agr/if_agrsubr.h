/*	$NetBSD: if_agrsubr.h,v 1.4 2007/02/21 23:00:07 thorpej Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
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

#ifndef _NET_AGR_IF_AGRSUBR_H_
#define	_NET_AGR_IF_AGRSUBR_H_

#include <sys/queue.h>

struct agr_mc_entry;

struct agr_multiaddrs {
	TAILQ_HEAD(, agr_mc_entry) ama_addrs;
};

int agr_mc_init(struct agr_softc *, struct agr_multiaddrs *);
int agr_mc_purgeall(struct agr_softc *, struct agr_multiaddrs *);

int agr_mc_foreach(struct agr_multiaddrs *,
    int (*)(struct agr_mc_entry *, void *), void *);
int agr_port_foreach(struct agr_softc *, int (*)(struct agr_port *, void *),
    void *);

int agr_configmulti_port(struct agr_multiaddrs *, struct agr_port *, bool);
int agr_configmulti_ifreq(struct agr_softc *, struct agr_multiaddrs *,
    struct ifreq *, bool);

int agr_port_getmedia(struct agr_port *, u_int *, u_int *);

#endif /* !_NET_AGR_IF_AGRSUBR_H_ */
