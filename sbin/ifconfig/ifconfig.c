/*	$NetBSD: ifconfig.c,v 1.235 2015/07/29 07:42:27 ozaki-r Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * Copyright (c) 1983, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1983, 1993\
 The Regents of the University of California.  All rights reserved.");
__RCSID("$NetBSD: ifconfig.c,v 1.235 2015/07/29 07:42:27 ozaki-r Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>
#include <netinet/in.h>		/* XXX */
#include <netinet/in_var.h>	/* XXX */

#include <netdb.h>

#include <sys/protosw.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <util.h>

#include "extern.h"

#include "media.h"
#include "parse.h"
#include "env.h"
#include "prog_ops.h"

#define WAIT_DAD	10000000 /* nanoseconds between each poll, 10ms */

static bool bflag, dflag, hflag, sflag, uflag, wflag;
bool lflag, Nflag, vflag, zflag;
static long wflag_secs;

static char gflags[10 + 26 * 2 + 1] = "AabCdhlNsuvw:z";
bool gflagset[10 + 26 * 2];

static int carrier(prop_dictionary_t);
static int clone_command(prop_dictionary_t, prop_dictionary_t);
static void do_setifpreference(prop_dictionary_t);
static int flag_index(int);
static void init_afs(void);
static int list_cloners(prop_dictionary_t, prop_dictionary_t);
static int media_status_exec(prop_dictionary_t, prop_dictionary_t);
static int wait_dad_exec(prop_dictionary_t, prop_dictionary_t);
static int no_cmds_exec(prop_dictionary_t, prop_dictionary_t);
static int notrailers(prop_dictionary_t, prop_dictionary_t);
static void printall(const char *, prop_dictionary_t);
static int setifaddr(prop_dictionary_t, prop_dictionary_t);
static int setifbroadaddr(prop_dictionary_t, prop_dictionary_t);
static int setifcaps(prop_dictionary_t, prop_dictionary_t);
static int setifdstormask(prop_dictionary_t, prop_dictionary_t);
static int setifflags(prop_dictionary_t, prop_dictionary_t);
static int setifmetric(prop_dictionary_t, prop_dictionary_t);
static int setifmtu(prop_dictionary_t, prop_dictionary_t);
static int setifnetmask(prop_dictionary_t, prop_dictionary_t);
static int setifprefixlen(prop_dictionary_t, prop_dictionary_t);
static int setlinkstr(prop_dictionary_t, prop_dictionary_t);
static int unsetlinkstr(prop_dictionary_t, prop_dictionary_t);
static void status(const struct sockaddr *, prop_dictionary_t,
    prop_dictionary_t);
__dead static void usage(void);

static const struct kwinst ifflagskw[] = {
	  IFKW("arp", -IFF_NOARP)
	, IFKW("debug", IFF_DEBUG)
	, IFKW("link0", IFF_LINK0)
	, IFKW("link1", IFF_LINK1)
	, IFKW("link2", IFF_LINK2)
	, {.k_word = "down", .k_type = KW_T_INT, .k_int = -IFF_UP}
	, {.k_word = "up", .k_type = KW_T_INT, .k_int = IFF_UP}
};

static const struct kwinst ifcapskw[] = {
	  IFKW("ip4csum-tx",	IFCAP_CSUM_IPv4_Tx)
	, IFKW("ip4csum-rx",	IFCAP_CSUM_IPv4_Rx)
	, IFKW("tcp4csum-tx",	IFCAP_CSUM_TCPv4_Tx)
	, IFKW("tcp4csum-rx",	IFCAP_CSUM_TCPv4_Rx)
	, IFKW("udp4csum-tx",	IFCAP_CSUM_UDPv4_Tx)
	, IFKW("udp4csum-rx",	IFCAP_CSUM_UDPv4_Rx)
	, IFKW("tcp6csum-tx",	IFCAP_CSUM_TCPv6_Tx)
	, IFKW("tcp6csum-rx",	IFCAP_CSUM_TCPv6_Rx)
	, IFKW("udp6csum-tx",	IFCAP_CSUM_UDPv6_Tx)
	, IFKW("udp6csum-rx",	IFCAP_CSUM_UDPv6_Rx)
	, IFKW("ip4csum",	IFCAP_CSUM_IPv4_Tx|IFCAP_CSUM_IPv4_Rx)
	, IFKW("tcp4csum",	IFCAP_CSUM_TCPv4_Tx|IFCAP_CSUM_TCPv4_Rx)
	, IFKW("udp4csum",	IFCAP_CSUM_UDPv4_Tx|IFCAP_CSUM_UDPv4_Rx)
	, IFKW("tcp6csum",	IFCAP_CSUM_TCPv6_Tx|IFCAP_CSUM_TCPv6_Rx)
	, IFKW("udp6csum",	IFCAP_CSUM_UDPv6_Tx|IFCAP_CSUM_UDPv6_Rx)
	, IFKW("tso4",		IFCAP_TSOv4)
	, IFKW("tso6",		IFCAP_TSOv6)
};

extern struct pbranch command_root;
extern struct pbranch opt_command;
extern struct pbranch opt_family, opt_silent_family;
extern struct pkw cloning, silent_family, family, ifcaps, ifflags, misc;
extern struct pstr parse_linkstr;

struct pinteger parse_metric = PINTEGER_INITIALIZER(&parse_metric, "metric", 10,
    setifmetric, "metric", &command_root.pb_parser);

struct pinteger parse_mtu = PINTEGER_INITIALIZER(&parse_mtu, "mtu", 10,
    setifmtu, "mtu", &command_root.pb_parser);

struct pinteger parse_prefixlen = PINTEGER_INITIALIZER(&parse_prefixlen,
    "prefixlen", 10, setifprefixlen, "prefixlen", &command_root.pb_parser);

struct pinteger parse_preference = PINTEGER_INITIALIZER1(&parse_preference,
    "preference", INT16_MIN, INT16_MAX, 10, NULL, "preference",
    &command_root.pb_parser);

struct paddr parse_netmask = PADDR_INITIALIZER(&parse_netmask, "netmask",
    setifnetmask, "dstormask", NULL, NULL, NULL, &command_root.pb_parser);

struct paddr parse_broadcast = PADDR_INITIALIZER(&parse_broadcast,
    "broadcast address",
    setifbroadaddr, "broadcast", NULL, NULL, NULL, &command_root.pb_parser);

static const struct kwinst misckw[] = {
	  {.k_word = "alias", .k_key = "alias", .k_deact = "alias",
	   .k_type = KW_T_BOOL, .k_neg = true,
	   .k_bool = true, .k_negbool = false,
	   .k_nextparser = &command_root.pb_parser}
	, {.k_word = "broadcast", .k_nextparser = &parse_broadcast.pa_parser}
	, {.k_word = "delete", .k_key = "alias", .k_deact = "alias",
	   .k_type = KW_T_BOOL, .k_bool = false,
	   .k_nextparser = &command_root.pb_parser}
	, {.k_word = "metric", .k_nextparser = &parse_metric.pi_parser}
	, {.k_word = "mtu", .k_nextparser = &parse_mtu.pi_parser}
	, {.k_word = "netmask", .k_nextparser = &parse_netmask.pa_parser}
	, {.k_word = "preference", .k_act = "address",
	   .k_nextparser = &parse_preference.pi_parser}
	, {.k_word = "prefixlen", .k_nextparser = &parse_prefixlen.pi_parser}
	, {.k_word = "trailers", .k_neg = true,
	   .k_exec = notrailers, .k_nextparser = &command_root.pb_parser}
	, {.k_word = "linkstr", .k_nextparser = &parse_linkstr.ps_parser }
	, {.k_word = "-linkstr", .k_exec = unsetlinkstr,
	   .k_nextparser = &command_root.pb_parser }
};

/* key: clonecmd */
static const struct kwinst clonekw[] = {
	{.k_word = "create", .k_type = KW_T_INT, .k_int = SIOCIFCREATE,
	 .k_nextparser = &opt_silent_family.pb_parser},
	{.k_word = "destroy", .k_type = KW_T_INT, .k_int = SIOCIFDESTROY}
};

static struct kwinst familykw[24];

struct pterm cloneterm = PTERM_INITIALIZER(&cloneterm, "list cloners",
    list_cloners, "none");

struct pterm wait_dad = PTERM_INITIALIZER(&wait_dad, "wait DAD", wait_dad_exec,
    "none");

struct pterm no_cmds = PTERM_INITIALIZER(&no_cmds, "no commands", no_cmds_exec,
    "none");

struct pkw family_only =
    PKW_INITIALIZER(&family_only, "family-only", NULL, "af", familykw,
	__arraycount(familykw), &no_cmds.pt_parser);

struct paddr address = PADDR_INITIALIZER(&address,
    "local address (address 1)",
    setifaddr, "address", "netmask", NULL, "address", &command_root.pb_parser);

struct paddr dstormask = PADDR_INITIALIZER(&dstormask,
    "destination/netmask (address 2)",
    setifdstormask, "dstormask", NULL, "address", "dstormask",
    &command_root.pb_parser);

struct paddr broadcast = PADDR_INITIALIZER(&broadcast,
    "broadcast address (address 3)",
    setifbroadaddr, "broadcast", NULL, "dstormask", "broadcast",
    &command_root.pb_parser);

struct pstr parse_linkstr = PSTR_INITIALIZER(&parse_linkstr, "linkstr",
    setlinkstr, "linkstr", &command_root.pb_parser);

static SIMPLEQ_HEAD(, afswtch) aflist = SIMPLEQ_HEAD_INITIALIZER(aflist);

static SIMPLEQ_HEAD(, usage_func) usage_funcs =
    SIMPLEQ_HEAD_INITIALIZER(usage_funcs);
static SIMPLEQ_HEAD(, status_func) status_funcs =
    SIMPLEQ_HEAD_INITIALIZER(status_funcs);
static SIMPLEQ_HEAD(, statistics_func) statistics_funcs =
    SIMPLEQ_HEAD_INITIALIZER(statistics_funcs);
static SIMPLEQ_HEAD(, cmdloop_branch) cmdloop_branches =
    SIMPLEQ_HEAD_INITIALIZER(cmdloop_branches);

struct branch opt_clone_brs[] = {
	  {.b_nextparser = &cloning.pk_parser}
	, {.b_nextparser = &opt_family.pb_parser}
}, opt_silent_family_brs[] = {
	  {.b_nextparser = &silent_family.pk_parser}
	, {.b_nextparser = &command_root.pb_parser}
}, opt_family_brs[] = {
	  {.b_nextparser = &family.pk_parser}
	, {.b_nextparser = &opt_command.pb_parser}
}, command_root_brs[] = {
	  {.b_nextparser = &ifflags.pk_parser}
	, {.b_nextparser = &ifcaps.pk_parser}
	, {.b_nextparser = &kwmedia.pk_parser}
	, {.b_nextparser = &misc.pk_parser}
	, {.b_nextparser = &address.pa_parser}
	, {.b_nextparser = &dstormask.pa_parser}
	, {.b_nextparser = &broadcast.pa_parser}
	, {.b_nextparser = NULL}
}, opt_command_brs[] = {
	  {.b_nextparser = &no_cmds.pt_parser}
	, {.b_nextparser = &command_root.pb_parser}
};

struct branch opt_family_only_brs[] = {
	  {.b_nextparser = &no_cmds.pt_parser}
	, {.b_nextparser = &family_only.pk_parser}
};
struct pbranch opt_family_only = PBRANCH_INITIALIZER(&opt_family_only,
    "opt-family-only", opt_family_only_brs,
    __arraycount(opt_family_only_brs), true);
struct pbranch opt_command = PBRANCH_INITIALIZER(&opt_command,
    "optional command",
    opt_command_brs, __arraycount(opt_command_brs), true);

struct pbranch command_root = PBRANCH_INITIALIZER(&command_root,
    "command-root", command_root_brs, __arraycount(command_root_brs), true);

struct piface iface_opt_family_only =
    PIFACE_INITIALIZER(&iface_opt_family_only, "iface-opt-family-only",
    NULL, "if", &opt_family_only.pb_parser);

struct pkw family = PKW_INITIALIZER(&family, "family", NULL, "af",
    familykw, __arraycount(familykw), &opt_command.pb_parser);

struct pkw silent_family = PKW_INITIALIZER(&silent_family, "silent family",
    NULL, "af", familykw, __arraycount(familykw), &command_root.pb_parser);

struct pkw *family_users[] = {&family_only, &family, &silent_family};

struct pkw ifcaps = PKW_INITIALIZER(&ifcaps, "ifcaps", setifcaps,
    "ifcap", ifcapskw, __arraycount(ifcapskw), &command_root.pb_parser);

struct pkw ifflags = PKW_INITIALIZER(&ifflags, "ifflags", setifflags,
    "ifflag", ifflagskw, __arraycount(ifflagskw), &command_root.pb_parser);

struct pkw cloning = PKW_INITIALIZER(&cloning, "cloning", clone_command,
    "clonecmd", clonekw, __arraycount(clonekw), NULL);

struct pkw misc = PKW_INITIALIZER(&misc, "misc", NULL, NULL,
    misckw, __arraycount(misckw), NULL);

struct pbranch opt_clone = PBRANCH_INITIALIZER(&opt_clone,
    "opt-clone", opt_clone_brs, __arraycount(opt_clone_brs), true);

struct pbranch opt_silent_family = PBRANCH_INITIALIZER(&opt_silent_family,
    "optional silent family", opt_silent_family_brs,
    __arraycount(opt_silent_family_brs), true);

struct pbranch opt_family = PBRANCH_INITIALIZER(&opt_family,
    "opt-family", opt_family_brs, __arraycount(opt_family_brs), true);

struct piface iface_start = PIFACE_INITIALIZER(&iface_start,
    "iface-opt-family", NULL, "if", &opt_clone.pb_parser);

struct piface iface_only = PIFACE_INITIALIZER(&iface_only, "iface",
    media_status_exec, "if", NULL);

static bool
flag_is_registered(const char *flags, int flag)
{
	return flags != NULL && strchr(flags, flag) != NULL;
}

static int
check_flag(const char *flags, int flag)
{
	if (flag_is_registered(flags, flag)) {
		errno = EEXIST;
		return -1;
	}

	if (flag >= '0' && flag <= '9')
		return 0;
	if (flag >= 'a' && flag <= 'z')
		return 0;
	if (flag >= 'A' && flag <= 'Z')
		return 0;

	errno = EINVAL;
	return -1;
}

void
cmdloop_branch_init(cmdloop_branch_t *b, struct parser *p)
{
	b->b_parser = p;
}

void
statistics_func_init(statistics_func_t *f, statistics_cb_t func)
{
	f->f_func = func;
}

void
status_func_init(status_func_t *f, status_cb_t func)
{
	f->f_func = func;
}

void
usage_func_init(usage_func_t *f, usage_cb_t func)
{
	f->f_func = func;
}

int
register_cmdloop_branch(cmdloop_branch_t *b)
{
	SIMPLEQ_INSERT_TAIL(&cmdloop_branches, b, b_next);
	return 0;
}

int
register_statistics(statistics_func_t *f)
{
	SIMPLEQ_INSERT_TAIL(&statistics_funcs, f, f_next);
	return 0;
}

int
register_status(status_func_t *f)
{
	SIMPLEQ_INSERT_TAIL(&status_funcs, f, f_next);
	return 0;
}

int
register_usage(usage_func_t *f)
{
	SIMPLEQ_INSERT_TAIL(&usage_funcs, f, f_next);
	return 0;
}

int
register_family(struct afswtch *af)
{
	SIMPLEQ_INSERT_TAIL(&aflist, af, af_next);
	return 0;
}

int
register_flag(int flag)
{
	if (check_flag(gflags, flag) == -1)
		return -1;

	if (strlen(gflags) + 1 >= sizeof(gflags)) {
		errno = ENOMEM;
		return -1;
	}

	gflags[strlen(gflags)] = flag;

	return 0;
}

static int
flag_index(int flag)
{
	if (flag >= '0' && flag <= '9')
		return flag - '0';
	if (flag >= 'a' && flag <= 'z')
		return 10 + flag - 'a';
	if (flag >= 'A' && flag <= 'Z')
		return 10 + 26 + flag - 'a';

	errno = EINVAL;
	return -1;
}

static bool
set_flag(int flag)
{
	int idx;

	if ((idx = flag_index(flag)) == -1)
		return false;

	return gflagset[idx] = true;
}

bool
get_flag(int flag)
{
	int idx;

	if ((idx = flag_index(flag)) == -1)
		return false;

	return gflagset[idx];
}

static struct parser *
init_parser(void)
{
	cmdloop_branch_t *b;

	if (parser_init(&iface_opt_family_only.pif_parser) == -1)
		err(EXIT_FAILURE, "parser_init(iface_opt_family_only)");
	if (parser_init(&iface_only.pif_parser) == -1)
		err(EXIT_FAILURE, "parser_init(iface_only)");
	if (parser_init(&iface_start.pif_parser) == -1)
		err(EXIT_FAILURE, "parser_init(iface_start)");

	SIMPLEQ_FOREACH(b, &cmdloop_branches, b_next)
		pbranch_addbranch(&command_root, b->b_parser);

	return &iface_start.pif_parser;
}

static int
no_cmds_exec(prop_dictionary_t env, prop_dictionary_t oenv)
{
	const char *ifname;
	unsigned short ignore;

	/* ifname == NULL is ok.  It indicates 'ifconfig -a'. */
	if ((ifname = getifname(env)) == NULL)
		;
	else if (getifflags(env, oenv, &ignore) == -1)
		err(EXIT_FAILURE, "SIOCGIFFLAGS %s", ifname);

	printall(ifname, env);
	exit(EXIT_SUCCESS);
}

static int
wait_dad_exec(prop_dictionary_t env, prop_dictionary_t oenv)
{
	bool waiting;
	struct ifaddrs *ifaddrs, *ifa;
	const struct timespec ts = { .tv_sec = 0, .tv_nsec = WAIT_DAD };
	const struct timespec add = { .tv_sec = wflag_secs, .tv_nsec = 0};
	struct timespec now, end = { .tv_sec = wflag_secs, .tv_nsec = 0};
	const struct afswtch *afp;

	if (wflag_secs) {
		if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
			err(EXIT_FAILURE, "clock_gettime");
		timespecadd(&now, &add, &end);
	}

	if (getifaddrs(&ifaddrs) == -1)
		err(EXIT_FAILURE, "getifaddrs");

	for (;;) {
		waiting = false;
		for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL)
				continue;
			afp = lookup_af_bynum(ifa->ifa_addr->sa_family);
			if (afp && afp->af_addr_tentative &&
			    afp->af_addr_tentative(ifa))
			{
				waiting = true;
				break;
			}
		}
		if (!waiting)
			break;
		nanosleep(&ts, NULL);
		if (wflag_secs) {
			if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
				err(EXIT_FAILURE, "clock_gettime");
			if (timespeccmp(&now, &end, >))
				errx(EXIT_FAILURE, "timed out");
		}
	}

	freeifaddrs(ifaddrs);
	exit(EXIT_SUCCESS);
}

static int
media_status_exec(prop_dictionary_t env, prop_dictionary_t oenv)
{
	const char *ifname;
	unsigned short ignore;

	/* ifname == NULL is ok.  It indicates 'ifconfig -a'. */
	if ((ifname = getifname(env)) == NULL)
		;
	else if (getifflags(env, oenv, &ignore) == -1)
		err(EXIT_FAILURE, "SIOCGIFFLAGS %s", ifname);

	exit(carrier(env));
}

static void
do_setifcaps(prop_dictionary_t env)
{
	struct ifcapreq ifcr;
	prop_data_t d;

	d = (prop_data_t )prop_dictionary_get(env, "ifcaps");
	if (d == NULL)
		return;

	assert(sizeof(ifcr) == prop_data_size(d));

	memcpy(&ifcr, prop_data_data_nocopy(d), sizeof(ifcr));
	if (direct_ioctl(env, SIOCSIFCAP, &ifcr) == -1)
		err(EXIT_FAILURE, "SIOCSIFCAP");
}

int
main(int argc, char **argv)
{
	const struct afswtch *afp;
	int af, s;
	bool aflag = false, Cflag = false;
	struct match match[32];
	size_t nmatch;
	struct parser *start;
	int ch, narg = 0, rc;
	prop_dictionary_t env, oenv;
	const char *ifname;
	char *end;

	memset(match, 0, sizeof(match));

	init_afs();

	start = init_parser();

	/* Parse command-line options */
	Nflag = vflag = zflag = false;
	aflag = argc == 1 ? true : false;
	if (aflag)
		start = &opt_family_only.pb_parser;

	while ((ch = getopt(argc, argv, gflags)) != -1) {
		switch (ch) {
		case 'A':
			warnx("-A is deprecated");
			break;

		case 'a':
			aflag = true;
			break;

		case 'b':
			bflag = true;
			break;

		case 'C':
			Cflag = true;
			break;

		case 'd':
			dflag = true;
			break;
		case 'h':
			hflag = true;
			break;
		case 'l':
			lflag = true;
			break;
		case 'N':
			Nflag = true;
			break;

		case 's':
			sflag = true;
			break;

		case 'u':
			uflag = true;
			break;

		case 'v':
			vflag = true;
			break;

		case 'w':
			wflag = true;
			wflag_secs = strtol(optarg, &end, 10);
			if ((end != NULL && *end != '\0') ||
			    wflag_secs < 0 || wflag_secs >= INT32_MAX)
				errx(EXIT_FAILURE, "%s: not a number", optarg);
			break;

		case 'z':
			zflag = true;
			break;

		default:
			if (!set_flag(ch))
				usage();
			break;
		}
		switch (ch) {
		case 'a':
			start = &opt_family_only.pb_parser;
			break;

		case 'L':
		case 'm':
		case 'z':
			if (start != &opt_family_only.pb_parser)
				start = &iface_opt_family_only.pif_parser;
			break;
		case 'C':
			start = &cloneterm.pt_parser;
			break;
		case 'l':
			start = &no_cmds.pt_parser;
			break;
		case 's':
			if (start != &no_cmds.pt_parser &&
			    start != &opt_family_only.pb_parser)
				start = &iface_only.pif_parser;
			break;
		case 'w':
			start = &wait_dad.pt_parser;
			break;
		default:
			break;
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * -l means "list all interfaces", and is mutually exclusive with
	 * all other flags/commands.
	 *
	 * -C means "list all names of cloners", and it mutually exclusive
	 * with all other flags/commands.
	 *
	 * -a means "print status of all interfaces".
	 *
	 * -w means "spin until DAD completes for all addreseses", and is
	 * mutually exclusivewith all other flags/commands.
	 */
	if ((lflag || Cflag || wflag) &&
	    (aflag || get_flag('m') || vflag || zflag))
		usage();
	if ((lflag || Cflag || wflag) && get_flag('L'))
		usage();
	if ((lflag && Cflag) || (lflag & wflag) || (Cflag && wflag))
		usage();

	nmatch = __arraycount(match);

	rc = parse(argc, argv, start, match, &nmatch, &narg);
	if (rc != 0)
		usage();

	if (prog_init && prog_init() == -1)
		err(1, "rump client init");

	if ((oenv = prop_dictionary_create()) == NULL)
		err(EXIT_FAILURE, "%s: prop_dictionary_create", __func__);

	if (matches_exec(match, oenv, nmatch) == -1)
		err(EXIT_FAILURE, "exec_matches");

	argc -= narg;
	argv += narg;

	env = (nmatch > 0) ? match[(int)nmatch - 1].m_env : NULL;
	if (env == NULL)
		env = oenv;
	else {
		env = prop_dictionary_augment(env, oenv);
		if (env == NULL)
			err(EXIT_FAILURE, "%s: prop_dictionary_augment",
			    __func__);
	}

	/* Process any media commands that may have been issued. */
	process_media_commands(env);

	if ((af = getaf(env)) == -1)
		af = AF_INET;

	if ((s = getsock(af)) == -1)
		err(EXIT_FAILURE, "%s: getsock", __func__);

	if ((ifname = getifname(env)) == NULL)
		err(EXIT_FAILURE, "%s: getifname", __func__);

	if ((afp = lookup_af_bynum(af)) == NULL)
		errx(EXIT_FAILURE, "%s: lookup_af_bynum", __func__);

	assert(afp->af_addr_commit != NULL);
	(*afp->af_addr_commit)(env, oenv);

	do_setifpreference(env);
	do_setifcaps(env);

	exit(EXIT_SUCCESS);
}

static void
init_afs(void)
{
	size_t i;
	const struct afswtch *afp;
	struct kwinst kw = {.k_type = KW_T_INT};

	SIMPLEQ_FOREACH(afp, &aflist, af_next) {
		kw.k_word = afp->af_name;
		kw.k_int = afp->af_af;
		for (i = 0; i < __arraycount(familykw); i++) {
			if (familykw[i].k_word == NULL) {
				familykw[i] = kw;
				break;
			}
		}
	}
}

const struct afswtch *
lookup_af_bynum(int afnum)
{
	const struct afswtch *afp;

	SIMPLEQ_FOREACH(afp, &aflist, af_next) {
		if (afp->af_af == afnum)
			break;
	}
	return afp;
}

void
printall(const char *ifname, prop_dictionary_t env0)
{
	struct ifaddrs *ifap, *ifa;
	struct ifreq ifr;
	const struct sockaddr *sdl = NULL;
	prop_dictionary_t env, oenv;
	int idx;
	char *p;

	if (env0 == NULL)
		env = prop_dictionary_create();
	else
		env = prop_dictionary_copy_mutable(env0);

	oenv = prop_dictionary_create();

	if (env == NULL || oenv == NULL)
		errx(EXIT_FAILURE, "%s: prop_dictionary_copy/create", __func__);

	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	p = NULL;
	idx = 0;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		memset(&ifr, 0, sizeof(ifr));
		estrlcpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
		if (sizeof(ifr.ifr_addr) >= ifa->ifa_addr->sa_len) {
			memcpy(&ifr.ifr_addr, ifa->ifa_addr,
			    ifa->ifa_addr->sa_len);
		}

		if (ifname != NULL && strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family == AF_LINK)
			sdl = ifa->ifa_addr;
		if (p && strcmp(p, ifa->ifa_name) == 0)
			continue;
		if (!prop_dictionary_set_cstring(env, "if", ifa->ifa_name))
			continue;
		p = ifa->ifa_name;

		if (bflag && (ifa->ifa_flags & IFF_BROADCAST) == 0)
			continue;
		if (dflag && (ifa->ifa_flags & IFF_UP) != 0)
			continue;
		if (uflag && (ifa->ifa_flags & IFF_UP) == 0)
			continue;

		if (sflag && carrier(env))
			continue;
		idx++;
		/*
		 * Are we just listing the interfaces?
		 */
		if (lflag) {
			if (idx > 1)
				printf(" ");
			fputs(ifa->ifa_name, stdout);
			continue;
		}

		status(sdl, env, oenv);
		sdl = NULL;
	}
	if (lflag)
		printf("\n");
	prop_object_release((prop_object_t)env);
	prop_object_release((prop_object_t)oenv);
	freeifaddrs(ifap);
}

static int
list_cloners(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct if_clonereq ifcr;
	char *cp, *buf;
	int idx, s;

	memset(&ifcr, 0, sizeof(ifcr));

	s = getsock(AF_INET);

	if (prog_ioctl(s, SIOCIFGCLONERS, &ifcr) == -1)
		err(EXIT_FAILURE, "SIOCIFGCLONERS for count");

	buf = malloc(ifcr.ifcr_total * IFNAMSIZ);
	if (buf == NULL)
		err(EXIT_FAILURE, "unable to allocate cloner name buffer");

	ifcr.ifcr_count = ifcr.ifcr_total;
	ifcr.ifcr_buffer = buf;

	if (prog_ioctl(s, SIOCIFGCLONERS, &ifcr) == -1)
		err(EXIT_FAILURE, "SIOCIFGCLONERS for names");

	/*
	 * In case some disappeared in the mean time, clamp it down.
	 */
	if (ifcr.ifcr_count > ifcr.ifcr_total)
		ifcr.ifcr_count = ifcr.ifcr_total;

	for (cp = buf, idx = 0; idx < ifcr.ifcr_count; idx++, cp += IFNAMSIZ) {
		if (idx > 0)
			printf(" ");
		printf("%s", cp);
	}

	printf("\n");
	free(buf);
	exit(EXIT_SUCCESS);
}

static int
clone_command(prop_dictionary_t env, prop_dictionary_t oenv)
{
	int64_t cmd;

	if (!prop_dictionary_get_int64(env, "clonecmd", &cmd)) {
		errno = ENOENT;
		return -1;
	}

	if (indirect_ioctl(env, (unsigned long)cmd, NULL) == -1) {
		warn("%s", __func__);
		return -1;
	}
	return 0;
}

/*ARGSUSED*/
static int
setifaddr(prop_dictionary_t env, prop_dictionary_t oenv)
{
	const struct paddr_prefix *pfx0;
	struct paddr_prefix *pfx;
	prop_data_t d;
	int af;

	if ((af = getaf(env)) == -1)
		af = AF_INET;

	d = (prop_data_t)prop_dictionary_get(env, "address");
	assert(d != NULL);
	pfx0 = prop_data_data_nocopy(d);

	if (pfx0->pfx_len >= 0) {
		pfx = prefixlen_to_mask(af, pfx0->pfx_len);
		if (pfx == NULL)
			err(EXIT_FAILURE, "prefixlen_to_mask");
		free(pfx);
	}

	return 0;
}

static int
setifnetmask(prop_dictionary_t env, prop_dictionary_t oenv)
{
	prop_data_t d;

	d = (prop_data_t)prop_dictionary_get(env, "dstormask");
	assert(d != NULL);

	if (!prop_dictionary_set(oenv, "netmask", (prop_object_t)d))
		return -1;

	return 0;
}

static int
setifbroadaddr(prop_dictionary_t env, prop_dictionary_t oenv)
{
	prop_data_t d;
	unsigned short flags;

	if (getifflags(env, oenv, &flags) == -1)
		err(EXIT_FAILURE, "%s: getifflags", __func__);

	if ((flags & IFF_BROADCAST) == 0)
		errx(EXIT_FAILURE, "not a broadcast interface");

	d = (prop_data_t)prop_dictionary_get(env, "broadcast");
	assert(d != NULL);

	if (!prop_dictionary_set(oenv, "broadcast", (prop_object_t)d))
		return -1;

	return 0;
}

/*ARGSUSED*/
static int
notrailers(prop_dictionary_t env, prop_dictionary_t oenv)
{
	puts("Note: trailers are no longer sent, but always received");
	return 0;
}

/*ARGSUSED*/
static int
setifdstormask(prop_dictionary_t env, prop_dictionary_t oenv)
{
	const char *key;
	prop_data_t d;
	unsigned short flags;

	if (getifflags(env, oenv, &flags) == -1)
		err(EXIT_FAILURE, "%s: getifflags", __func__);

	d = (prop_data_t)prop_dictionary_get(env, "dstormask");
	assert(d != NULL);

	if ((flags & IFF_BROADCAST) == 0) {
		key = "dst";
	} else {
		key = "netmask";
	}

	if (!prop_dictionary_set(oenv, key, (prop_object_t)d))
		return -1;

	return 0;
}

static int
setifflags(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct ifreq ifr;
	int64_t ifflag;
	bool rc;

	rc = prop_dictionary_get_int64(env, "ifflag", &ifflag);
	assert(rc);

 	if (direct_ioctl(env, SIOCGIFFLAGS, &ifr) == -1)
		return -1;

	if (ifflag < 0) {
		ifflag = -ifflag;
		ifr.ifr_flags &= ~ifflag;
	} else
		ifr.ifr_flags |= ifflag;

	if (direct_ioctl(env, SIOCSIFFLAGS, &ifr) == -1)
		return -1;

	return 0;
}

static int
getifcaps(prop_dictionary_t env, prop_dictionary_t oenv, struct ifcapreq *oifcr)
{
	bool rc;
	struct ifcapreq ifcr;
	const struct ifcapreq *tmpifcr;
	prop_data_t capdata;

	capdata = (prop_data_t)prop_dictionary_get(env, "ifcaps");

	if (capdata != NULL) {
		tmpifcr = prop_data_data_nocopy(capdata);
		*oifcr = *tmpifcr;
		return 0;
	}

	(void)direct_ioctl(env, SIOCGIFCAP, &ifcr);
	*oifcr = ifcr;

	capdata = prop_data_create_data(&ifcr, sizeof(ifcr));

	rc = prop_dictionary_set(oenv, "ifcaps", capdata);

	prop_object_release((prop_object_t)capdata);

	return rc ? 0 : -1;
}

static int
setifcaps(prop_dictionary_t env, prop_dictionary_t oenv)
{
	int64_t ifcap;
	bool rc;
	prop_data_t capdata;
	struct ifcapreq ifcr;

	rc = prop_dictionary_get_int64(env, "ifcap", &ifcap);
	assert(rc);

	if (getifcaps(env, oenv, &ifcr) == -1)
		return -1;

	if (ifcap < 0) {
		ifcap = -ifcap;
		ifcr.ifcr_capenable &= ~ifcap;
	} else
		ifcr.ifcr_capenable |= ifcap;

	if ((capdata = prop_data_create_data(&ifcr, sizeof(ifcr))) == NULL)
		return -1;

	rc = prop_dictionary_set(oenv, "ifcaps", capdata);
	prop_object_release((prop_object_t)capdata);

	return rc ? 0 : -1;
}

static int
setifmetric(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct ifreq ifr;
	bool rc;
	int64_t metric;

	rc = prop_dictionary_get_int64(env, "metric", &metric);
	assert(rc);

	ifr.ifr_metric = metric;
	if (direct_ioctl(env, SIOCSIFMETRIC, &ifr) == -1)
		warn("SIOCSIFMETRIC");
	return 0;
}

static void
do_setifpreference(prop_dictionary_t env)
{
	struct if_addrprefreq ifap;
	prop_data_t d;
	const struct paddr_prefix *pfx;

	memset(&ifap, 0, sizeof(ifap));

	if (!prop_dictionary_get_int16(env, "preference",
	    &ifap.ifap_preference))
		return;

	d = (prop_data_t)prop_dictionary_get(env, "address");
	assert(d != NULL);

	pfx = prop_data_data_nocopy(d);

	memcpy(&ifap.ifap_addr, &pfx->pfx_addr,
	    MIN(sizeof(ifap.ifap_addr), pfx->pfx_addr.sa_len));
	if (direct_ioctl(env, SIOCSIFADDRPREF, &ifap) == -1)
		warn("SIOCSIFADDRPREF");
}

static int
setifmtu(prop_dictionary_t env, prop_dictionary_t oenv)
{
	int64_t mtu;
	bool rc;
	struct ifreq ifr;

	rc = prop_dictionary_get_int64(env, "mtu", &mtu);
	assert(rc);

	ifr.ifr_mtu = mtu;
	if (direct_ioctl(env, SIOCSIFMTU, &ifr) == -1)
		warn("SIOCSIFMTU");

	return 0;
}

static int
carrier(prop_dictionary_t env)
{
	struct ifmediareq ifmr;

	memset(&ifmr, 0, sizeof(ifmr));

	if (direct_ioctl(env, SIOCGIFMEDIA, &ifmr) == -1) {
		/*
		 * Interface doesn't support SIOC{G,S}IFMEDIA;
		 * assume ok.
		 */
		return EXIT_SUCCESS;
	}
	if ((ifmr.ifm_status & IFM_AVALID) == 0) {
		/*
		 * Interface doesn't report media-valid status.
		 * assume ok.
		 */
		return EXIT_SUCCESS;
	}
	/* otherwise, return ok for active, not-ok if not active. */
	if (ifmr.ifm_status & IFM_ACTIVE)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

static void
print_plural(const char *prefix, uint64_t n, const char *unit)
{
	printf("%s%" PRIu64 " %s%s", prefix, n, unit, (n == 1) ? "" : "s");
}

static void
print_human_bytes(bool humanize, uint64_t n)
{
	char buf[5];

	if (humanize) {
		(void)humanize_number(buf, sizeof(buf),
		    (int64_t)n, "", HN_AUTOSCALE, HN_NOSPACE | HN_DECIMAL);
		printf(", %s byte%s", buf, (atof(buf) == 1.0) ? "" : "s");
	} else
		print_plural(", ", n, "byte");
}

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */

#define MAX_PRINT_LEN 58	/* XXX need a better way to determine this! */

void
status(const struct sockaddr *sdl, prop_dictionary_t env,
    prop_dictionary_t oenv)
{
	const struct if_data *ifi;
	status_func_t *status_f;
	statistics_func_t *statistics_f;
	struct ifdatareq ifdr;
	struct ifreq ifr;
	struct ifdrv ifdrv;
	char fbuf[BUFSIZ];
	char *bp;
	int af, s;
	const char *ifname;
	struct ifcapreq ifcr;
	unsigned short flags;
	const struct afswtch *afp;

	if ((af = getaf(env)) == -1) {
		afp = NULL;
		af = AF_UNSPEC;
	} else
		afp = lookup_af_bynum(af);

	/* get out early if the family is unsupported by the kernel */
	if ((s = getsock(af)) == -1)
		err(EXIT_FAILURE, "%s: getsock", __func__);

	if ((ifname = getifinfo(env, oenv, &flags)) == NULL)
		err(EXIT_FAILURE, "%s: getifinfo", __func__);

	(void)snprintb(fbuf, sizeof(fbuf), IFFBITS, flags);
	printf("%s: flags=%s", ifname, fbuf);

	estrlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (prog_ioctl(s, SIOCGIFMETRIC, &ifr) == -1)
		warn("SIOCGIFMETRIC %s", ifr.ifr_name);
	else if (ifr.ifr_metric != 0)
		printf(" metric %d", ifr.ifr_metric);

	estrlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (prog_ioctl(s, SIOCGIFMTU, &ifr) != -1 && ifr.ifr_mtu != 0)
		printf(" mtu %d", ifr.ifr_mtu);
	printf("\n");

	if (getifcaps(env, oenv, &ifcr) == -1)
		err(EXIT_FAILURE, "%s: getifcaps", __func__);

	if (ifcr.ifcr_capabilities != 0) {
		(void)snprintb_m(fbuf, sizeof(fbuf), IFCAPBITS,
		    ifcr.ifcr_capabilities, MAX_PRINT_LEN);
		bp = fbuf;
		while (*bp != '\0') {
			printf("\tcapabilities=%s\n", &bp[2]);
			bp += strlen(bp) + 1;
		}
		(void)snprintb_m(fbuf, sizeof(fbuf), IFCAPBITS,
		    ifcr.ifcr_capenable, MAX_PRINT_LEN);
		bp = fbuf;
		while (*bp != '\0') {
			printf("\tenabled=%s\n", &bp[2]);
			bp += strlen(bp) + 1;
		}
	}

	SIMPLEQ_FOREACH(status_f, &status_funcs, f_next)
		(*status_f->f_func)(env, oenv);

	print_link_addresses(env, true);

	estrlcpy(ifdrv.ifd_name, ifname, sizeof(ifdrv.ifd_name));
	ifdrv.ifd_cmd = IFLINKSTR_QUERYLEN;
	ifdrv.ifd_len = 0;
	ifdrv.ifd_data = NULL;
	/* interface supports linkstr? */
	if (prog_ioctl(s, SIOCGLINKSTR, &ifdrv) != -1) {
		char *p;

		p = malloc(ifdrv.ifd_len);
		if (p == NULL)
			err(EXIT_FAILURE, "malloc linkstr buf failed");
		ifdrv.ifd_data = p;
		ifdrv.ifd_cmd = 0;
		if (prog_ioctl(s, SIOCGLINKSTR, &ifdrv) == -1)
			err(EXIT_FAILURE, "failed to query linkstr");
		printf("\tlinkstr: %s\n", (char *)ifdrv.ifd_data);
		free(p);
	}

	media_status(env, oenv);

	if (!vflag && !zflag)
		goto proto_status;

	estrlcpy(ifdr.ifdr_name, ifname, sizeof(ifdr.ifdr_name));

	if (prog_ioctl(s, zflag ? SIOCZIFDATA : SIOCGIFDATA, &ifdr) == -1)
		err(EXIT_FAILURE, zflag ? "SIOCZIFDATA" : "SIOCGIFDATA");

	ifi = &ifdr.ifdr_data;

	print_plural("\tinput: ", ifi->ifi_ipackets, "packet");
	print_human_bytes(hflag, ifi->ifi_ibytes);
	if (ifi->ifi_imcasts)
		print_plural(", ", ifi->ifi_imcasts, "multicast");
	if (ifi->ifi_ierrors)
		print_plural(", ", ifi->ifi_ierrors, "error");
	if (ifi->ifi_iqdrops)
		print_plural(", ", ifi->ifi_iqdrops, "queue drop");
	if (ifi->ifi_noproto)
		printf(", %" PRIu64 " unknown protocol", ifi->ifi_noproto);
	print_plural("\n\toutput: ", ifi->ifi_opackets, "packet");
	print_human_bytes(hflag, ifi->ifi_obytes);
	if (ifi->ifi_omcasts)
		print_plural(", ", ifi->ifi_omcasts, "multicast");
	if (ifi->ifi_oerrors)
		print_plural(", ", ifi->ifi_oerrors, "error");
	if (ifi->ifi_collisions)
		print_plural(", ", ifi->ifi_collisions, "collision");
	printf("\n");

	SIMPLEQ_FOREACH(statistics_f, &statistics_funcs, f_next)
		(*statistics_f->f_func)(env);

 proto_status:

	if (afp != NULL)
		(*afp->af_status)(env, oenv, true);
	else SIMPLEQ_FOREACH(afp, &aflist, af_next)
		(*afp->af_status)(env, oenv, false);
}

static int
setifprefixlen(prop_dictionary_t env, prop_dictionary_t oenv)
{
	bool rc;
	int64_t plen;
	int af;
	struct paddr_prefix *pfx;
	prop_data_t d;

	if ((af = getaf(env)) == -1)
		af = AF_INET;

	rc = prop_dictionary_get_int64(env, "prefixlen", &plen);
	assert(rc);

	pfx = prefixlen_to_mask(af, plen);
	if (pfx == NULL)
		err(EXIT_FAILURE, "prefixlen_to_mask");

	d = prop_data_create_data(pfx, paddr_prefix_size(pfx));
	if (d == NULL)
		err(EXIT_FAILURE, "%s: prop_data_create_data", __func__);

	if (!prop_dictionary_set(oenv, "netmask", (prop_object_t)d))
		err(EXIT_FAILURE, "%s: prop_dictionary_set", __func__);

	free(pfx);
	return 0;
}

static int
setlinkstr(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct ifdrv ifdrv;
	size_t linkstrlen;
	prop_data_t data;
	char *linkstr;

	data = (prop_data_t)prop_dictionary_get(env, "linkstr");
	if (data == NULL) {
		errno = ENOENT;
		return -1;
	}
	linkstrlen = prop_data_size(data)+1;

	linkstr = malloc(linkstrlen);
	if (linkstr == NULL)
		err(EXIT_FAILURE, "malloc linkstr space");
	if (getargstr(env, "linkstr", linkstr, linkstrlen) == -1)
		errx(EXIT_FAILURE, "getargstr linkstr failed");

	ifdrv.ifd_cmd = 0;
	ifdrv.ifd_len = linkstrlen;
	ifdrv.ifd_data = __UNCONST(linkstr);

	if (direct_ioctl(env, SIOCSLINKSTR, &ifdrv) == -1)
		err(EXIT_FAILURE, "SIOCSLINKSTR");
	free(linkstr);

	return 0;
}

static int
unsetlinkstr(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct ifdrv ifdrv;

	memset(&ifdrv, 0, sizeof(ifdrv));
	ifdrv.ifd_cmd = IFLINKSTR_UNSET;

	if (direct_ioctl(env, SIOCSLINKSTR, &ifdrv) == -1)
		err(EXIT_FAILURE, "SIOCSLINKSTR");

	return 0;
}

static void
usage(void)
{
	const char *progname = getprogname();
	usage_func_t *usage_f;
	prop_dictionary_t env;

	if ((env = prop_dictionary_create()) == NULL)
		err(EXIT_FAILURE, "%s: prop_dictionary_create", __func__);

	fprintf(stderr, "usage: %s [-h] %s[-v] [-z] %sinterface\n"
		"\t[ af [ address [ dest_addr ] ] [ netmask mask ] [ prefixlen n ]\n"
		"\t\t[ alias | -alias ] ]\n"
		"\t[ up ] [ down ] [ metric n ] [ mtu n ]\n", progname,
		flag_is_registered(gflags, 'm') ? "[-m] " : "",
		flag_is_registered(gflags, 'L') ? "[-L] " : "");

	SIMPLEQ_FOREACH(usage_f, &usage_funcs, f_next)
		(*usage_f->f_func)(env);

	fprintf(stderr,
		"\t[ arp | -arp ]\n"
		"\t[ preference n ]\n"
		"\t[ link0 | -link0 ] [ link1 | -link1 ] [ link2 | -link2 ]\n"
		"       %s -a [-b] [-d] [-h] %s[-u] [-v] [-z] [ af ]\n"
		"       %s -l [-b] [-d] [-s] [-u]\n"
		"       %s -C\n"
		"       %s -w n\n"
		"       %s interface create\n"
		"       %s interface destroy\n",
		progname, flag_is_registered(gflags, 'm') ? "[-m] " : "",
		progname, progname, progname, progname, progname);

	prop_object_release((prop_object_t)env);
	exit(EXIT_FAILURE);
}
