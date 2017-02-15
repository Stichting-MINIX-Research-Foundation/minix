/*	$NetBSD: af_link.c,v 1.7 2014/01/19 22:31:13 matt Exp $	*/

/*-
 * Copyright (c) 2008 David Young.  All rights reserved.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: af_link.c,v 1.7 2014/01/19 22:31:13 matt Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>

#include <net/if.h> 
#include <net/if_dl.h> 

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util.h>

#include "env.h"
#include "extern.h"
#include "af_inetany.h"

static void link_status(prop_dictionary_t, prop_dictionary_t, bool);
static void link_commit_address(prop_dictionary_t, prop_dictionary_t);

static const struct kwinst linkkw[] = {
	  {.k_word = "active", .k_key = "active", .k_type = KW_T_BOOL,
	   .k_bool = true, .k_nextparser = &command_root.pb_parser}
};

struct pkw link_pkw = PKW_INITIALIZER(&link_pkw, "link", NULL, NULL,
    linkkw, __arraycount(linkkw), NULL);

static struct afswtch af = {
	.af_name = "link", .af_af = AF_LINK, .af_status = link_status,
	.af_addr_commit = link_commit_address
};

static cmdloop_branch_t branch;

static void link_constructor(void) __attribute__((constructor));

static void
link_status(prop_dictionary_t env, prop_dictionary_t oenv, bool force)
{
	print_link_addresses(env, false);
}

static int
link_pre_aifaddr(prop_dictionary_t env, const struct afparam *param)
{
	bool active;
	struct if_laddrreq *iflr = param->req.buf;

	if (prop_dictionary_get_bool(env, "active", &active) && active)
		iflr->flags |= IFLR_ACTIVE;

	return 0;
}

static void
link_commit_address(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct if_laddrreq dgreq = {
		.addr = {
			.ss_family = AF_LINK,
			.ss_len = sizeof(dgreq.addr),
		},
	};
	struct if_laddrreq req = {
		.addr = {
			.ss_family = AF_LINK,
			.ss_len = sizeof(req.addr),
		}
	};
	struct afparam linkparam = {
		  .req = BUFPARAM(req)
		, .dgreq = BUFPARAM(dgreq)
		, .name = {
			{.buf = dgreq.iflr_name,
			 .buflen = sizeof(dgreq.iflr_name)},
			{.buf = req.iflr_name,
			 .buflen = sizeof(req.iflr_name)}
		  }
		, .dgaddr = BUFPARAM(dgreq.addr)
		, .addr = BUFPARAM(req.addr)
		, .aifaddr = IFADDR_PARAM(SIOCALIFADDR)
		, .difaddr = IFADDR_PARAM(SIOCDLIFADDR)
		, .gifaddr = IFADDR_PARAM(0)
		, .pre_aifaddr = link_pre_aifaddr
	};
	commit_address(env, oenv, &linkparam);
}

static void
link_constructor(void)
{
	register_family(&af);
	cmdloop_branch_init(&branch, &link_pkw.pk_parser);
	register_cmdloop_branch(&branch);
}
