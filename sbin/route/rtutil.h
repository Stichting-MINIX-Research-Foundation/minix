/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#define RT_AFLAG	__BIT(0)	/* show address field */
#define RT_TFLAG	__BIT(1)	/* show tag field */
#define RT_VFLAG	__BIT(2)	/* show verbose statistics */
#define RT_NFLAG	__BIT(3)	/* numeric output */
#define RT_LFLAG	__BIT(4)	/* don't show LLINFO entries */

void p_rttables(int, int, int, int);
void p_rthdr(int, int);
void p_family(int);
void p_sockaddr(const struct sockaddr *, const struct sockaddr *, int, int, int);
void p_flags(int);
struct rt_metrics;
void p_rtrmx(const struct rt_metrics *);
void p_addr(const struct sockaddr *sa, const struct sockaddr *mask, int, int);
void p_gwaddr(const struct sockaddr *sa, int, int);

char *routename(const struct sockaddr *sa, int);
char *routename4(in_addr_t, int);
#ifdef INET6
char *routename6(const struct sockaddr_in6 *, int);
char *netname6(const struct sockaddr_in6 *, const struct sockaddr_in6 *, int);
#endif
char *netname(const struct sockaddr *, const struct sockaddr *, int);
char *netname4(const struct sockaddr_in *, const struct sockaddr_in *, int);

char *mpls_ntoa(const struct sockaddr *);
char *any_ntoa(const struct sockaddr *);
