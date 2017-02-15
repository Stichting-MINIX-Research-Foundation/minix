/*	$NetBSD: agr.c,v 1.15 2008/07/15 21:27:58 dyoung Exp $	*/

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

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: agr.c,v 1.15 2008/07/15 21:27:58 dyoung Exp $");
#endif /* !defined(lint) */

#include <sys/param.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/agr/if_agrioctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <util.h>

#include "env.h"
#include "extern.h"
#include "parse.h"
#include "util.h"

static int agrsetport(prop_dictionary_t, prop_dictionary_t);
static void agr_constructor(void) __attribute__((constructor));
static int checkifname(prop_dictionary_t);
static void assertifname(prop_dictionary_t);

static struct piface agrif = PIFACE_INITIALIZER(&agrif, "agr interface",
    agrsetport, "agrport", &command_root.pb_parser);

static const struct kwinst agrkw[] = {
	  {.k_word = "agrport", .k_type = KW_T_INT, .k_int = AGRCMD_ADDPORT,
	   .k_nextparser = &agrif.pif_parser}
	, {.k_word = "-agrport", .k_type = KW_T_INT, .k_int = AGRCMD_REMPORT,
	   .k_nextparser = &agrif.pif_parser}
};

struct pkw agr = PKW_INITIALIZER(&agr, "agr", NULL, "agrcmd",
    agrkw, __arraycount(agrkw), NULL);

static int
checkifname(prop_dictionary_t env)
{
	const char *ifname;

	if ((ifname = getifname(env)) == NULL)
		return 1;

	return strncmp(ifname, "agr", 3) != 0 ||
	    !isdigit((unsigned char)ifname[3]);
}

static void
assertifname(prop_dictionary_t env)
{
	if (checkifname(env))
		errx(EXIT_FAILURE, "valid only with agr(4) interfaces");
}

int
agrsetport(prop_dictionary_t env, prop_dictionary_t oenv)
{
	char buf[IFNAMSIZ];
	struct agrreq ar;
	const char *port;
	int64_t cmd;

	if (!prop_dictionary_get_int64(env, "agrcmd", &cmd)) {
		warnx("%s.%d", __func__, __LINE__);
		errno = ENOENT;
		return -1;
	}

	if (!prop_dictionary_get_cstring_nocopy(env, "agrport", &port)) {
		warnx("%s.%d", __func__, __LINE__);
		errno = ENOENT;
		return -1;
	}
	strlcpy(buf, port, sizeof(buf));

	assertifname(env);
	memset(&ar, 0, sizeof(ar));
	ar.ar_version = AGRREQ_VERSION;
	ar.ar_cmd = cmd;
	ar.ar_buf = buf;
	ar.ar_buflen = strlen(buf);

	if (indirect_ioctl(env, SIOCSETAGR, &ar) == -1)
		err(EXIT_FAILURE, "SIOCSETAGR");
	return 0;
}

static void
agr_status(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct agrreq ar;
	void *buf = NULL;
	size_t buflen = 0;
	struct agrportlist *apl;
	struct agrportinfo *api;
	int i;

	if (checkifname(env))
		return;

again:
	memset(&ar, 0, sizeof(ar));
	ar.ar_version = AGRREQ_VERSION;
	ar.ar_cmd = AGRCMD_PORTLIST;
	ar.ar_buf = buf;
	ar.ar_buflen = buflen;

	if (indirect_ioctl(env, SIOCGETAGR, &ar) == -1) {
		if (errno != E2BIG) {
			warn("SIOCGETAGR");
			return;
		}

		free(buf);
		buf = NULL;
		buflen = 0;
		goto again;
	}

	if (buf == NULL) {
		buflen = ar.ar_buflen;
		buf = malloc(buflen);
		if (buf == NULL) {
			err(EXIT_FAILURE, "agr_status");
		}
		goto again;
	}

	apl = buf;
	api = (void *)(apl + 1);

	for (i = 0; i < apl->apl_nports; i++) {
		char tmp[256];

		snprintb(tmp, sizeof(tmp), AGRPORTINFO_BITS, api->api_flags);
		printf("\tagrport: %s, flags=%s\n", api->api_ifname, tmp);
		api++;
	}
}

static status_func_t status;
static usage_func_t usage;
static cmdloop_branch_t branch;

static void
agr_usage(prop_dictionary_t env)
{
	fprintf(stderr, "\t[ agrport i ] [ -agrport i ]\n");
}

static void
agr_constructor(void)
{
	status_func_init(&status, agr_status);
	usage_func_init(&usage, agr_usage);
	register_status(&status);
	register_usage(&usage);
	cmdloop_branch_init(&branch, &agr.pk_parser);
	register_cmdloop_branch(&branch);
}
