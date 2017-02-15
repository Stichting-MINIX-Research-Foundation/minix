/*	$NetBSD: tunnel.c,v 1.20 2013/10/19 15:59:15 christos Exp $	*/

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
__RCSID("$NetBSD: tunnel.c,v 1.20 2013/10/19 15:59:15 christos Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>

#include <net/if.h> 

#ifdef INET6
#include <netinet/in.h>
#endif

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util.h>

#include "env.h"
#include "extern.h"
#include "parse.h"
#include "util.h"

static status_func_t status;
static usage_func_t usage;
static cmdloop_branch_t branch;

static void tunnel_constructor(void) __attribute__((constructor));
static int settunnel(prop_dictionary_t, prop_dictionary_t);
static int deletetunnel(prop_dictionary_t, prop_dictionary_t);
static void tunnel_status(prop_dictionary_t, prop_dictionary_t);

struct paddr tundst = PADDR_INITIALIZER(&tundst, "tundst", settunnel,
    "tundst", NULL, NULL, NULL, &command_root.pb_parser);

struct paddr tunsrc = PADDR_INITIALIZER(&tunsrc, "tunsrc", NULL,
    "tunsrc", NULL, NULL, NULL, &tundst.pa_parser);

static const struct kwinst tunnelkw[] = {
	  {.k_word = "deletetunnel", .k_exec = deletetunnel,
	   .k_nextparser = &command_root.pb_parser}
	, {.k_word = "tunnel", .k_nextparser = &tunsrc.pa_parser}
};

struct pkw tunnel = PKW_INITIALIZER(&tunnel, "tunnel", NULL, NULL,
    tunnelkw, __arraycount(tunnelkw), NULL);

static int
settunnel(prop_dictionary_t env, prop_dictionary_t oenv)
{
	const struct paddr_prefix *srcpfx, *dstpfx;
	struct if_laddrreq req;
	prop_data_t srcdata, dstdata;

	srcdata = (prop_data_t)prop_dictionary_get(env, "tunsrc");
	dstdata = (prop_data_t)prop_dictionary_get(env, "tundst");

	if (srcdata == NULL || dstdata == NULL) {
		warnx("%s.%d", __func__, __LINE__);
		errno = ENOENT;
		return -1;
	}

	srcpfx = prop_data_data_nocopy(srcdata);
	dstpfx = prop_data_data_nocopy(dstdata);

	if (srcpfx->pfx_addr.sa_family != dstpfx->pfx_addr.sa_family)
		errx(EXIT_FAILURE,
		    "source and destination address families do not match");

	memset(&req, 0, sizeof(req));
	memcpy(&req.addr, &srcpfx->pfx_addr,
	    MIN(sizeof(req.addr), srcpfx->pfx_addr.sa_len));
	memcpy(&req.dstaddr, &dstpfx->pfx_addr,
	    MIN(sizeof(req.dstaddr), dstpfx->pfx_addr.sa_len));

#ifdef INET6
	if (req.addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *s6, *d;

		s6 = (struct sockaddr_in6 *)&req.addr;
		d = (struct sockaddr_in6 *)&req.dstaddr;
		if (s6->sin6_scope_id != d->sin6_scope_id) {
			errx(EXIT_FAILURE, "scope mismatch");
			/* NOTREACHED */
		}
		if (IN6_IS_ADDR_MULTICAST(&d->sin6_addr) ||
		    IN6_IS_ADDR_MULTICAST(&s6->sin6_addr))
			errx(EXIT_FAILURE, "tunnel src/dst is multicast");
		/* embed scopeid */
		inet6_putscopeid(s6, INET6_IS_ADDR_LINKLOCAL);
		inet6_putscopeid(d, INET6_IS_ADDR_LINKLOCAL);
	}
#endif /* INET6 */

	if (direct_ioctl(env, SIOCSLIFPHYADDR, &req) == -1)
		warn("SIOCSLIFPHYADDR");
	return 0;
}

static int
deletetunnel(prop_dictionary_t env, prop_dictionary_t oenv)
{
	if (indirect_ioctl(env, SIOCDIFPHYADDR, NULL) == -1)
		err(EXIT_FAILURE, "SIOCDIFPHYADDR");
	return 0;
}

static void
tunnel_status(prop_dictionary_t env, prop_dictionary_t oenv)
{
	char dstserv[sizeof(",65535")];
	char srcserv[sizeof(",65535")];
	char psrcaddr[NI_MAXHOST];
	char pdstaddr[NI_MAXHOST];
	const int niflag = Nflag ? 0 : (NI_NUMERICHOST|NI_NUMERICSERV);
	struct if_laddrreq req;
	const struct afswtch *afp;

	psrcaddr[0] = pdstaddr[0] = '\0';

	memset(&req, 0, sizeof(req));
	if (direct_ioctl(env, SIOCGLIFPHYADDR, &req) == -1)
		return;
	afp = lookup_af_bynum(req.addr.ss_family);
#ifdef INET6
	if (req.addr.ss_family == AF_INET6)
		inet6_getscopeid((struct sockaddr_in6 *)&req.addr,
		    INET6_IS_ADDR_LINKLOCAL);
#endif /* INET6 */
	getnameinfo((struct sockaddr *)&req.addr, req.addr.ss_len,
	    psrcaddr, sizeof(psrcaddr), &srcserv[1], sizeof(srcserv) - 1,
	    niflag);

#ifdef INET6
	if (req.dstaddr.ss_family == AF_INET6)
		inet6_getscopeid((struct sockaddr_in6 *)&req.dstaddr,
		    INET6_IS_ADDR_LINKLOCAL);
#endif
	getnameinfo((struct sockaddr *)&req.dstaddr, req.dstaddr.ss_len,
	    pdstaddr, sizeof(pdstaddr), &dstserv[1], sizeof(dstserv) - 1,
	    niflag);

	srcserv[0] = (strcmp(&srcserv[1], "0") == 0) ? '\0' : ',';
	dstserv[0] = (strcmp(&dstserv[1], "0") == 0) ? '\0' : ',';

	printf("\ttunnel %s %s%s --> %s%s\n", afp ? afp->af_name : "???",
	    psrcaddr, srcserv, pdstaddr, dstserv);
}

static void
tunnel_usage(prop_dictionary_t env)
{
	fprintf(stderr,
	    "\t[ [ af ] tunnel src_addr dest_addr ] [ deletetunnel ]\n");
}

static void
tunnel_constructor(void)
{
	cmdloop_branch_init(&branch, &tunnel.pk_parser);
	register_cmdloop_branch(&branch);
	status_func_init(&status, tunnel_status);
	usage_func_init(&usage, tunnel_usage);
	register_status(&status);
	register_usage(&usage);
}
