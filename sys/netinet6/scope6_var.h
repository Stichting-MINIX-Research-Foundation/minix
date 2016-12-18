/*	$FreeBSD: src/sys/netinet6/scope6_var.h,v 1.4 2005/01/07 02:30:35 imp Exp $	*/
/*	$KAME: scope6_var.h,v 1.4 2000/05/18 15:03:27 jinmei Exp $	*/

/*-
 * Copyright (C) 2000 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NETINET6_SCOPE6_VAR_H_
#define _NETINET6_SCOPE6_VAR_H_

#ifdef _KERNEL
struct scope6_id {
	/*
	 * 16 is correspondent to 4bit multicast scope field.
	 * i.e. from interface-local to global with some
	 * reserved/unassigned types.
	 */
	uint32_t s6id_list[16];
};

void	scope6_init(void);
struct scope6_id *scope6_ifattach(struct ifnet *);
void	scope6_ifdetach(struct scope6_id *);
int	scope6_set(struct ifnet *, const struct scope6_id *);
int	scope6_get(const struct ifnet *, struct scope6_id *);
void	scope6_setdefault(struct ifnet *);
int	scope6_get_default(struct scope6_id *);
uint32_t scope6_in6_addrscope(struct in6_addr *);
uint32_t scope6_addr2default(const struct in6_addr *);
int	sa6_embedscope(struct sockaddr_in6 *, int);
int	sa6_recoverscope(struct sockaddr_in6 *);
int	in6_setzoneid(struct in6_addr *, uint32_t);
int	in6_setscope(struct in6_addr *, const struct ifnet *, u_int32_t *);
int	in6_clearscope(struct in6_addr *);
const char *in6_getscopename(const struct in6_addr *);
#endif /* _KERNEL */

#endif /* _NETINET6_SCOPE6_VAR_H_ */
