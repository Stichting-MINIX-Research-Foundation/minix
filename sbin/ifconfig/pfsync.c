/*	$NetBSD: pfsync.c,v 1.1 2009/09/14 10:36:49 degroote Exp $	*/
/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: pfsync.c,v 1.1 2009/09/14 10:36:49 degroote Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/route.h>
#include <net/pfvar.h>
#include <net/if_pfsync.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <util.h>

#include "env.h"
#include "parse.h"
#include "extern.h"

static status_func_t status;
static usage_func_t usage;
static cmdloop_branch_t branch;

static void pfsync_constructor(void) __attribute__((constructor));
static void pfsync_status(prop_dictionary_t, prop_dictionary_t);
static int setpfsync_maxupd(prop_dictionary_t, prop_dictionary_t);
static int setpfsync_peer(prop_dictionary_t, prop_dictionary_t);
static int setpfsyncdev(prop_dictionary_t, prop_dictionary_t);

struct pinteger parse_maxupd = PINTEGER_INITIALIZER1(&parse_maxupd, "maxupd",
    0, 255, 10, setpfsync_maxupd, "maxupd", &command_root.pb_parser);

struct piface pfsyncdev = PIFACE_INITIALIZER(&pfsyncdev, "syncdev", setpfsyncdev,
    "syncdev", &command_root.pb_parser);

struct paddr parse_sync_peer = PADDR_INITIALIZER(&parse_sync_peer, "syncpeer",
		setpfsync_peer, "syncpeer", NULL, NULL, NULL, &command_root.pb_parser);

static const struct kwinst pfsynckw[] = {
	{.k_word = "maxupd", .k_nextparser = &parse_maxupd.pi_parser},
	{.k_word = "syncdev", .k_nextparser = &pfsyncdev.pif_parser},
	{.k_word = "-syncdev", .k_key = "syncdev", .k_type = KW_T_STR,
	    .k_str = "", .k_exec = setpfsyncdev,
	    .k_nextparser = &command_root.pb_parser},
	{.k_word = "syncpeer", .k_nextparser = &parse_sync_peer.pa_parser},
	{.k_word = "-syncpeer", .k_key = "syncpeer", .k_type = KW_T_STR,
	    .k_str = "", .k_exec = setpfsync_peer,
	    .k_nextparser = &command_root.pb_parser}
};

struct pkw pfsync = PKW_INITIALIZER(&pfsync, "pfsync", NULL, NULL,
    pfsynckw, __arraycount(pfsynckw), NULL);

static void
pfsync_set(prop_dictionary_t env, struct pfsyncreq *pfsyncr)
{
	if (indirect_ioctl(env, SIOCSETPFSYNC, pfsyncr) == -1)
		err(EXIT_FAILURE, "SIOCSETPFSYNC");
}

static int
pfsync_get1(prop_dictionary_t env, struct pfsyncreq *pfsyncr)
{
	memset(pfsyncr, 0, sizeof(*pfsyncr));

	return indirect_ioctl(env, SIOCGETPFSYNC, pfsyncr);
}

static void
pfsync_get(prop_dictionary_t env, struct pfsyncreq *pfsyncr)
{
	if (pfsync_get1(env, pfsyncr) == -1)
		err(EXIT_FAILURE, "SIOCGETPFSYNC");
}

static void
pfsync_status(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct pfsyncreq pfsyncr;

	if (pfsync_get1(env, &pfsyncr) == -1)
		return;

	if (pfsyncr.pfsyncr_syncdev[0] != '\0') {
		printf("\tpfsync: syncdev: %s ", pfsyncr.pfsyncr_syncdev);
		if (pfsyncr.pfsyncr_syncpeer.s_addr != INADDR_PFSYNC_GROUP)
			printf("syncpeer: %s ",
			    inet_ntoa(pfsyncr.pfsyncr_syncpeer));
		printf("maxupd: %d\n", pfsyncr.pfsyncr_maxupdates);
	}
}

/* ARGSUSED */
int
setpfsync_maxupd(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct pfsyncreq pfsyncr;
	uint8_t maxupd;

	if (!prop_dictionary_get_uint8(env, "maxupd", &maxupd)) {
		errno = ENOENT;
		return -1;
	}

	pfsync_get(env, &pfsyncr);

	pfsyncr.pfsyncr_maxupdates = maxupd;

	pfsync_set(env, &pfsyncr);
	return 0;
}


/* ARGSUSED */
int
setpfsyncdev(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct pfsyncreq pfsyncr;
	const char *dev;

	if (!prop_dictionary_get_cstring_nocopy(env, "syncdev", &dev)) {
		errno = ENOENT;
		return -1;
	}

	pfsync_get(env, &pfsyncr);

	strlcpy(pfsyncr.pfsyncr_syncdev, dev, sizeof(pfsyncr.pfsyncr_syncdev));

	pfsync_set(env, &pfsyncr);
	return 0;
}

/* ARGSUSED */
int
setpfsync_peer(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct pfsyncreq pfsyncr;
	prop_data_t data;
	const struct paddr_prefix *peerpfx;
	const struct sockaddr_in *s;

	data = (prop_data_t)prop_dictionary_get(env, "syncpeer");
	if (data == NULL) {
		errno = ENOENT;
		return -1;
	}

	pfsync_get(env, &pfsyncr);

	peerpfx = prop_data_data_nocopy(data);

	if (peerpfx != NULL) {
		// Only AF_INET is supported for now
		if (peerpfx->pfx_addr.sa_family != AF_INET) {
			errno = ENOENT;
			return -1;
		}


		s = (const struct sockaddr_in*)&peerpfx->pfx_addr;

		memcpy(&pfsyncr.pfsyncr_syncpeer.s_addr, &s->sin_addr,
		    MIN(sizeof(pfsyncr.pfsyncr_syncpeer.s_addr),
		    peerpfx->pfx_addr.sa_len));	   
	} else {
		memset(&pfsyncr.pfsyncr_syncpeer.s_addr, 0,
		    sizeof(pfsyncr.pfsyncr_syncpeer.s_addr));
	}

	pfsync_set(env, &pfsyncr);

	return 0;
}

static void
pfsync_usage(prop_dictionary_t env)
{
	fprintf(stderr,
	    "\t[ maxupd n ] [ syncdev iface ] [syncpeer peer_addr]\n");
}

static void
pfsync_constructor(void)
{
	cmdloop_branch_init(&branch, &pfsync.pk_parser);
	register_cmdloop_branch(&branch);
	status_func_init(&status, pfsync_status);
	usage_func_init(&usage, pfsync_usage);
	register_status(&status);
	register_usage(&usage);
}
