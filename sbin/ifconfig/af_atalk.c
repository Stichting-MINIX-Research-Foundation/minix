/*	$NetBSD: af_atalk.c,v 1.19 2013/10/19 00:35:30 christos Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: af_atalk.c,v 1.19 2013/10/19 00:35:30 christos Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>

#include <net/if.h> 

#include <netatalk/at.h>

#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <util.h>

#include "env.h"
#include "af_inetany.h"
#include "parse.h"
#include "extern.h"
#include "prog_ops.h"

#ifndef satocsat
#define	satocsat(__sa) ((const struct sockaddr_at *)(__sa))
#endif

static void at_status(prop_dictionary_t, prop_dictionary_t, bool);
static void at_commit_address(prop_dictionary_t, prop_dictionary_t);

static void at_constructor(void) __attribute__((constructor));

static struct afswtch ataf = {
	.af_name = "atalk", .af_af = AF_APPLETALK, .af_status = at_status,
	.af_addr_commit = at_commit_address
};
struct pinteger phase = PINTEGER_INITIALIZER1(&phase, "phase",
    1, 2, 10, NULL, "phase", &command_root.pb_parser);

struct pstr parse_range = PSTR_INITIALIZER(&range, "range", NULL, "range",
    &command_root.pb_parser);

static const struct kwinst atalkkw[] = {
	  {.k_word = "phase", .k_nextparser = &phase.pi_parser}
	, {.k_word = "range", .k_nextparser = &parse_range.ps_parser}
};

struct pkw atalk = PKW_INITIALIZER(&atalk, "AppleTalk", NULL, NULL,
    atalkkw, __arraycount(atalkkw), NULL);

static cmdloop_branch_t branch;

static void
setatrange_impl(prop_dictionary_t env, prop_dictionary_t oenv,
    struct netrange *nr)
{
	char range[24];
	u_short	first = 123, last = 123;

	if (getargstr(env, "range", range, sizeof(range)) == -1)
		return;

	if (sscanf(range, "%hu-%hu", &first, &last) != 2 ||
	    first == 0 || last == 0 || first > last)
		errx(EXIT_FAILURE, "%s: illegal net range: %u-%u", range,
		    first, last);
	nr->nr_firstnet = htons(first);
	nr->nr_lastnet = htons(last);
}

static void
at_commit_address(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct ifreq ifr;
	struct ifaliasreq ifra __attribute__((aligned(4)));
	struct afparam atparam = {
		  .req = BUFPARAM(ifra)
		, .dgreq = BUFPARAM(ifr)
		, .name = {
			  {.buf = ifr.ifr_name,
			   .buflen = sizeof(ifr.ifr_name)}
			, {.buf = ifra.ifra_name,
			   .buflen = sizeof(ifra.ifra_name)}
		  }
		, .dgaddr = BUFPARAM(ifr.ifr_addr)
		, .addr = BUFPARAM(ifra.ifra_addr)
		, .dst = BUFPARAM(ifra.ifra_dstaddr)
		, .brd = BUFPARAM(ifra.ifra_broadaddr)
		, .mask = BUFPARAM(ifra.ifra_mask)
		, .aifaddr = IFADDR_PARAM(SIOCAIFADDR)
		, .difaddr = IFADDR_PARAM(SIOCDIFADDR)
		, .gifaddr = IFADDR_PARAM(SIOCGIFADDR)
		, .defmask = {.buf = NULL, .buflen = 0}
	};
	struct netrange nr = {.nr_phase = 2};	/* AppleTalk net range */
	prop_data_t d, d0;
	prop_dictionary_t ienv;
	struct paddr_prefix *addr;
	struct sockaddr_at *sat;

	if ((d0 = (prop_data_t)prop_dictionary_get(env, "address")) == NULL)
		return;

	addr = prop_data_data(d0);

	sat = (struct sockaddr_at *)&addr->pfx_addr;

	(void)prop_dictionary_get_uint8(env, "phase", &nr.nr_phase);
	/* Default range of one */
	nr.nr_firstnet = nr.nr_lastnet = sat->sat_addr.s_net;
	setatrange_impl(env, oenv, &nr);

	if (ntohs(nr.nr_firstnet) > ntohs(sat->sat_addr.s_net) ||
	    ntohs(nr.nr_lastnet) < ntohs(sat->sat_addr.s_net))
		errx(EXIT_FAILURE, "AppleTalk address is not in range");
	memcpy(&sat->sat_zero, &nr, sizeof(nr));

	/* Copy the new address to a temporary input environment */

	d = prop_data_create_data_nocopy(addr, paddr_prefix_size(addr));
	ienv = prop_dictionary_copy_mutable(env);

	if (d == NULL)
		err(EXIT_FAILURE, "%s: prop_data_create_data", __func__);
	if (ienv == NULL)
		err(EXIT_FAILURE, "%s: prop_dictionary_copy_mutable", __func__);

	if (!prop_dictionary_set(ienv, "address", (prop_object_t)d))
		err(EXIT_FAILURE, "%s: prop_dictionary_set", __func__);

	/* copy to output environment for good measure */
	if (!prop_dictionary_set(oenv, "address", (prop_object_t)d))
		err(EXIT_FAILURE, "%s: prop_dictionary_set", __func__);

	prop_object_release((prop_object_t)d);

	memset(&ifr, 0, sizeof(ifr));
	memset(&ifra, 0, sizeof(ifra));
	commit_address(ienv, oenv, &atparam);

	/* release temporary input environment */
	prop_object_release((prop_object_t)ienv);
}

static void
sat_print1(const char *prefix, const struct sockaddr *sa)
{
	char buf[40];

	(void)getnameinfo(sa, sa->sa_len, buf, sizeof(buf), NULL, 0, 0);
	
	printf("%s%s", prefix, buf);
}

static void
at_status(prop_dictionary_t env, prop_dictionary_t oenv, bool force)
{
	struct sockaddr_at *sat;
	struct ifreq ifr;
	int s;
	const char *ifname;
	unsigned short flags;

	if ((s = getsock(AF_APPLETALK)) == -1) {
		if (errno == EAFNOSUPPORT)
			return;
		err(EXIT_FAILURE, "getsock");
	}
	if ((ifname = getifinfo(env, oenv, &flags)) == NULL)
		err(EXIT_FAILURE, "%s: getifinfo", __func__);

	memset(&ifr, 0, sizeof(ifr));
	estrlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_addr.sa_family = AF_APPLETALK;
	if (prog_ioctl(s, SIOCGIFADDR, &ifr) != -1)
		;
	else if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
		if (!force)
			return;
		memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
	} else
		warn("SIOCGIFADDR");
	sat = (struct sockaddr_at *)&ifr.ifr_addr;

	sat_print1("\tatalk ", &ifr.ifr_addr);

	if (flags & IFF_POINTOPOINT) {
		estrlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		if (prog_ioctl(s, SIOCGIFDSTADDR, &ifr) == -1) {
			if (errno == EADDRNOTAVAIL)
				memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
			else
				warn("SIOCGIFDSTADDR");
		}
		sat_print1(" --> ", &ifr.ifr_dstaddr);
	}
	if (flags & IFF_BROADCAST) {
		/* note RTAX_BRD overlap with IFF_POINTOPOINT */
		/* note Appletalk broadcast is fixed. */
		printf(" broadcast %u.%u", ntohs(sat->sat_addr.s_net),
			ATADDR_BCAST);
	}
	printf("\n");
}

static void
at_constructor(void)
{
	register_family(&ataf);
	cmdloop_branch_init(&branch, &atalk.pk_parser);
	register_cmdloop_branch(&branch);
}
