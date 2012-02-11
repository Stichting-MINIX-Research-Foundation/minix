/*	$NetBSD: ns.h,v 1.1 2006/08/26 16:07:01 matt Exp $	*/

/*
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ns.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETNS_NS_H_
#define _NETNS_NS_H_

/*
 * NS addressing
 */
union ns_host {
	u_int8_t	c_host[6];
	u_int16_t	s_host[3];
};

union ns_net {
	u_int8_t	c_net[4];
	u_int16_t	s_net[2];
};

union ns_net_u {
	union ns_net	net_e;
	u_int32_t	long_e;
};

struct ns_addr {
	union ns_net	x_net;
	union ns_host	x_host;
	u_int16_t	x_port;
};

__BEGIN_DECLS
extern struct ns_addr ns_addr __P((const char *));
extern char *ns_ntoa __P((struct ns_addr));
__END_DECLS

#endif /* !_NETNS_NS_H_ */
