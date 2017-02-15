/* $NetBSD: carp.c,v 1.13 2009/09/11 23:22:28 dyoung Exp $ */

/*
 * Copyright (c) 2002 Michael Shalayeff. All rights reserved.
 * Copyright (c) 2003 Ryan McBride. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: carp.c,v 1.13 2009/09/11 23:22:28 dyoung Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <netinet/ip_carp.h>
#include <net/route.h>

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

static void carp_constructor(void) __attribute__((constructor));
static void carp_status(prop_dictionary_t, prop_dictionary_t);
static int setcarp_advbase(prop_dictionary_t, prop_dictionary_t);
static int setcarp_advskew(prop_dictionary_t, prop_dictionary_t);
static int setcarp_passwd(prop_dictionary_t, prop_dictionary_t);
static int setcarp_vhid(prop_dictionary_t, prop_dictionary_t);
static int setcarp_state(prop_dictionary_t, prop_dictionary_t);
static int setcarpdev(prop_dictionary_t, prop_dictionary_t);

static const char *carp_states[] = { CARP_STATES };

struct kwinst carpstatekw[] = {
	  {.k_word = "INIT", .k_nextparser = &command_root.pb_parser}
	, {.k_word = "BACKUP", .k_nextparser = &command_root.pb_parser}
	, {.k_word = "MASTER", .k_nextparser = &command_root.pb_parser}
};

struct pinteger parse_advbase = PINTEGER_INITIALIZER1(&parse_advbase, "advbase",
    0, 255, 10, setcarp_advbase, "advbase", &command_root.pb_parser);

struct pinteger parse_advskew = PINTEGER_INITIALIZER1(&parse_advskew, "advskew",
    0, 254, 10, setcarp_advskew, "advskew", &command_root.pb_parser);

struct piface carpdev = PIFACE_INITIALIZER(&carpdev, "carpdev", setcarpdev,
    "carpdev", &command_root.pb_parser);

struct pkw carpstate = PKW_INITIALIZER(&carpstate, "carp state", setcarp_state,
    "carp_state", carpstatekw, __arraycount(carpstatekw),
    &command_root.pb_parser);

struct pstr pass = PSTR_INITIALIZER(&pass, "pass", setcarp_passwd,
    "pass", &command_root.pb_parser);

struct pinteger parse_vhid = PINTEGER_INITIALIZER1(&vhid, "vhid",
    0, 255, 10, setcarp_vhid, "vhid", &command_root.pb_parser);

static const struct kwinst carpkw[] = {
	  {.k_word = "advbase", .k_nextparser = &parse_advbase.pi_parser}
	, {.k_word = "advskew", .k_nextparser = &parse_advskew.pi_parser}
	, {.k_word = "carpdev", .k_nextparser = &carpdev.pif_parser}
	, {.k_word = "-carpdev", .k_key = "carpdev", .k_type = KW_T_STR,
	   .k_str = "", .k_exec = setcarpdev,
	   .k_nextparser = &command_root.pb_parser}
	, {.k_word = "pass", .k_nextparser = &pass.ps_parser}
	, {.k_word = "state", .k_nextparser = &carpstate.pk_parser}
	, {.k_word = "vhid", .k_nextparser = &parse_vhid.pi_parser}
};

struct pkw carp = PKW_INITIALIZER(&carp, "CARP", NULL, NULL,
    carpkw, __arraycount(carpkw), NULL);

static void
carp_set(prop_dictionary_t env, struct carpreq *carpr)
{
	if (indirect_ioctl(env, SIOCSVH, carpr) == -1)
		err(EXIT_FAILURE, "SIOCSVH");
}

static int
carp_get1(prop_dictionary_t env, struct carpreq *carpr)
{
	memset(carpr, 0, sizeof(*carpr));

	return indirect_ioctl(env, SIOCGVH, carpr);
}

static void
carp_get(prop_dictionary_t env, struct carpreq *carpr)
{
	if (carp_get1(env, carpr) == -1)
		err(EXIT_FAILURE, "SIOCGVH");
}

static void
carp_status(prop_dictionary_t env, prop_dictionary_t oenv)
{
	const char *state;
	struct carpreq carpr;

	if (carp_get1(env, &carpr) == -1)
		return;

	if (carpr.carpr_vhid <= 0)
		return;
	if (carpr.carpr_state > CARP_MAXSTATE)
		state = "<UNKNOWN>";
	else
		state = carp_states[carpr.carpr_state];

	printf("\tcarp: %s carpdev %s vhid %d advbase %d advskew %d\n",
	    state, carpr.carpr_carpdev[0] != '\0' ?
	    carpr.carpr_carpdev : "none", carpr.carpr_vhid,
	    carpr.carpr_advbase, carpr.carpr_advskew);
}

int
setcarp_passwd(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct carpreq carpr;
	prop_data_t data;

	data = (prop_data_t)prop_dictionary_get(env, "pass");
	if (data == NULL) {
		errno = ENOENT;
		return -1;
	}

	carp_get(env, &carpr);

	memset(carpr.carpr_key, 0, sizeof(carpr.carpr_key));
	/* XXX Should hash the password into the key here, perhaps? */
	strlcpy((char *)carpr.carpr_key, prop_data_data_nocopy(data),
	    MIN(CARP_KEY_LEN, prop_data_size(data)));

	carp_set(env, &carpr);
	return 0;
}

int
setcarp_vhid(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct carpreq carpr;
	int64_t vhid;

	if (!prop_dictionary_get_int64(env, "vhid", &vhid)) {
		errno = ENOENT;
		return -1;
	}

	carp_get(env, &carpr);

	carpr.carpr_vhid = vhid;

	carp_set(env, &carpr);
	return 0;
}

int
setcarp_advskew(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct carpreq carpr;
	int64_t advskew;

	if (!prop_dictionary_get_int64(env, "advskew", &advskew)) {
		errno = ENOENT;
		return -1;
	}

	carp_get(env, &carpr);

	carpr.carpr_advskew = advskew;

	carp_set(env, &carpr);
	return 0;
}

/* ARGSUSED */
int
setcarp_advbase(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct carpreq carpr;
	int64_t advbase;

	if (!prop_dictionary_get_int64(env, "advbase", &advbase)) {
		errno = ENOENT;
		return -1;
	}

	carp_get(env, &carpr);

	carpr.carpr_advbase = advbase;

	carp_set(env, &carpr);
	return 0;
}

/* ARGSUSED */
static int
setcarp_state(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct carpreq carpr;
	int64_t carp_state;

	if (!prop_dictionary_get_int64(env, "carp_state", &carp_state)) {
		errno = ENOENT;
		return -1;
	}

	carp_get(env, &carpr);

	carpr.carpr_state = carp_state;

	carp_set(env, &carpr);
	return 0;
}

/* ARGSUSED */
int
setcarpdev(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct carpreq carpr;
	prop_string_t s;

	s = (prop_string_t)prop_dictionary_get(env, "carpdev");
	if (s == NULL) {
		errno = ENOENT;
		return -1;
	}

	carp_get(env, &carpr);

	strlcpy(carpr.carpr_carpdev, prop_string_cstring_nocopy(s),
	    sizeof(carpr.carpr_carpdev));

	carp_set(env, &carpr);
	return 0;
}

static void
carp_usage(prop_dictionary_t env)
{
	fprintf(stderr,
	    "\t[ advbase n ] [ advskew n ] [ carpdev iface ] "
	    "[ pass passphrase ] [ state state ] [ vhid n ]\n");

}

static void
carp_constructor(void)
{
	cmdloop_branch_init(&branch, &carp.pk_parser);
	register_cmdloop_branch(&branch);
	status_func_init(&status, carp_status);
	usage_func_init(&usage, carp_usage);
	register_status(&status);
	register_usage(&usage);
}
