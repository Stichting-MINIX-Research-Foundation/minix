/*	$NetBSD: ieee8023ad_lacp_debug.h,v 1.3 2005/12/10 23:21:39 elad Exp $	*/

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

#ifndef _NET_AGR_IEEE8023AD_LACP_DEBUG_H_
#define	_NET_AGR_IEEE8023AD_LACP_DEBUG_H_

#define	LACP_DEBUG

/* following constants don't include terminating NUL */
#define	LACP_MACSTR_MAX		(2*6 + 5)
#define	LACP_SYSTEMPRIOSTR_MAX	(4)
#define	LACP_SYSTEMIDSTR_MAX	(LACP_SYSTEMPRIOSTR_MAX + 1 + LACP_MACSTR_MAX)
#define	LACP_PORTPRIOSTR_MAX	(4)
#define	LACP_PORTNOSTR_MAX	(4)
#define	LACP_PORTIDSTR_MAX	(LACP_PORTPRIOSTR_MAX + 1 + LACP_PORTNOSTR_MAX)
#define	LACP_KEYSTR_MAX		(4)
#define	LACP_PARTNERSTR_MAX	\
	(1 + LACP_SYSTEMIDSTR_MAX + 1 + LACP_KEYSTR_MAX + 1 \
	+ LACP_PORTIDSTR_MAX + 1)
#define	LACP_LAGIDSTR_MAX	\
	(1 + LACP_PARTNERSTR_MAX + 1 + LACP_PARTNERSTR_MAX + 1)
#define	LACP_STATESTR_MAX	(255) /* XXX */

void lacp_dump_lacpdu(const struct lacpdu *);
const char *lacp_format_partner(const struct lacp_peerinfo *, char *, size_t);
const char *lacp_format_lagid(const struct lacp_peerinfo *,
    const struct lacp_peerinfo *, char *, size_t);
const char *lacp_format_lagid_aggregator(const struct lacp_aggregator *,
    char *, size_t);
const char *lacp_format_state(uint8_t, char *, size_t);
const char *lacp_format_mac(const uint8_t *, char *, size_t);
const char *lacp_format_systemid(const struct lacp_systemid *, char *, size_t);
const char *lacp_format_portid(const struct lacp_portid *, char *, size_t);

#if defined(LACP_DEBUG)
extern int lacpdebug;
void lacp_dprintf(const struct lacp_port *, const char *, ...)
    __attribute__((__format__(__printf__, 2, 3)));
#define	LACP_DPRINTF(a)	if (lacpdebug) lacp_dprintf a
#else
#define LACP_DPRINTF(a) /* nothing */
#endif

#endif /* !_NET_AGR_IEEE8023AD_LACP_DEBUG_H_ */
