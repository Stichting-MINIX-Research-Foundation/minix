/*	$NetBSD: vlan.c,v 1.14 2014/09/15 06:46:04 ozaki-r Exp $	*/

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
__RCSID("$NetBSD: vlan.c,v 1.14 2014/09/15 06:46:04 ozaki-r Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 

#include <net/if.h> 
#include <net/if_ether.h>
#include <net/if_vlanvar.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util.h>

#include "env.h"
#include "extern.h"
#include "util.h"

static status_func_t status;
static usage_func_t usage;
static cmdloop_branch_t branch;

static void vlan_constructor(void) __attribute__((constructor));
static void vlan_status(prop_dictionary_t, prop_dictionary_t);

static int setvlan(prop_dictionary_t, prop_dictionary_t);
static int setvlanif(prop_dictionary_t, prop_dictionary_t);

struct pinteger vlantag = PINTEGER_INITIALIZER1(&vlantag, "VLAN tag",
    0, USHRT_MAX, 10, setvlan, "vlantag", &command_root.pb_parser);

struct piface vlanif = PIFACE_INITIALIZER(&vlanif, "vlanif", setvlanif,
    "vlanif", &command_root.pb_parser);

static const struct kwinst vlankw[] = {
	  {.k_word = "vlan", .k_nextparser = &vlantag.pi_parser}
	, {.k_word = "vlanif", .k_act = "vlantag",
	   .k_nextparser = &vlanif.pif_parser}
	, {.k_word = "-vlanif", .k_key = "vlanif", .k_type = KW_T_STR,
	   .k_str = "", .k_exec = setvlanif}
};

struct pkw vlan = PKW_INITIALIZER(&vlan, "vlan", NULL, NULL,
    vlankw, __arraycount(vlankw), NULL);

static int
checkifname(prop_dictionary_t env)
{
	const char *ifname;

	if ((ifname = getifname(env)) == NULL)
		return 1;

	return strncmp(ifname, "vlan", 4) != 0 ||
	    !isdigit((unsigned char)ifname[4]);
}

static int
getvlan(prop_dictionary_t env, struct vlanreq *vlr, bool quiet)
{
	memset(vlr, 0, sizeof(*vlr));

	if (checkifname(env)) {
		if (quiet)
			return -1;
		errx(EXIT_FAILURE, "valid only with vlan(4) interfaces");
	}

	if (indirect_ioctl(env, SIOCGETVLAN, vlr) == -1)
		return -1;

	return 0;
}

int
setvlan(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct vlanreq vlr;
	int64_t tag;

	if (getvlan(env, &vlr, false) == -1)
		err(EXIT_FAILURE, "%s: getvlan", __func__);

	if (!prop_dictionary_get_int64(env, "vlantag", &tag)) {
		errno = ENOENT;
		return -1;
	}

	vlr.vlr_tag = tag;

	if (indirect_ioctl(env, SIOCSETVLAN, &vlr) == -1)
		err(EXIT_FAILURE, "SIOCSETVLAN");
	return 0;
}

int
setvlanif(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct vlanreq vlr;
	const char *parent;
	int64_t tag;

	if (getvlan(env, &vlr, false) == -1)
		err(EXIT_FAILURE, "%s: getsock", __func__);

	if (!prop_dictionary_get_cstring_nocopy(env, "vlanif", &parent)) {
		errno = ENOENT;
		return -1;
	}
	strlcpy(vlr.vlr_parent, parent, sizeof(vlr.vlr_parent));
	if (strcmp(parent, "") == 0)
		;
	else if (!prop_dictionary_get_int64(env, "vlantag", &tag)) {
		errno = ENOENT;
		return -1;
	} else
		vlr.vlr_tag = (unsigned short)tag;

	if (indirect_ioctl(env, SIOCSETVLAN, &vlr) == -1)
		err(EXIT_FAILURE, "SIOCSETVLAN");
	return 0;
}

static void
vlan_status(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct vlanreq vlr;

	if (getvlan(env, &vlr, true) == -1)
		return;

	if (vlr.vlr_tag || vlr.vlr_parent[0] != '\0')
		printf("\tvlan: %d parent: %s\n",
		    vlr.vlr_tag, vlr.vlr_parent[0] == '\0' ?
		    "<none>" : vlr.vlr_parent);
}

static void
vlan_usage(prop_dictionary_t env)
{
	fprintf(stderr, "\t[ vlan n vlanif i ] [ -vlanif i ]\n");
}

static void
vlan_constructor(void)
{
	cmdloop_branch_init(&branch, &vlan.pk_parser);
	register_cmdloop_branch(&branch);
	status_func_init(&status, vlan_status);
	usage_func_init(&usage, vlan_usage);
	register_status(&status);
	register_usage(&usage);
}
